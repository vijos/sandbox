#include "sandbox/sandbox.h"

#include <mutex>
#include <sstream>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cereal/archives/binary.hpp>
#include "sandbox/ipc.h"

extern "C" int pivot_root(const char *new_root, const char *put_old);

namespace sandbox {

Sandbox::Sandbox(const Options &options)
    : options_(options), host_stream_(&host_streambuf_) {}

Sandbox::~Sandbox() {
    if (host_socket_ != -1) {
        close(host_socket_);
    }
    if (host_cgroup_socket_ != -1) {
        close(host_cgroup_socket_);
    }
}

bool Sandbox::init() {
    static std::mutex init_mutex;
    std::lock_guard<std::mutex> lock(init_mutex);
    static std::vector<int> host_sockets;

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds)) {
        perror("socketpair");
        return false;
    }
    host_socket_ = fds[0];
    guest_socket_ = fds[1];
    host_streambuf_.fd(host_socket_);
    host_sockets.push_back(host_socket_);
    if (socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, fds)) {
        perror("socketpair");
        return false;
    }
    host_cgroup_socket_ = fds[0];
    guest_cgroup_socket_ = fds[1];
    host_sockets.push_back(host_cgroup_socket_);
    int optval = 1;
    if (setsockopt(host_cgroup_socket_, SOL_SOCKET, SO_PASSCRED,
                   &optval, sizeof(optval))) {
        perror("setsockopt");
        return false;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return false;
    }
    if (pid == 0) {
        for (int host_socket : host_sockets) {
            close(host_socket);
        }
        guest_entry();
        exit(1);
        return false;
    }
    close(guest_socket_);
    close(guest_cgroup_socket_);
    // TODO(iceboy): pid became zombie.
    return true;
}

bool Sandbox::execute(
    const ipc::ExecuteRequest &request, ipc::ExecuteResponse &response) {
    try {
        cereal::BinaryOutputArchive output(host_stream_);
        output(ipc::Command::execute);
        output(request);
        host_stream_.flush();
        if (!host_cgroup_sync()) {
            return false;
        }
        cereal::BinaryInputArchive input(host_stream_);
        input(response);
        return true;
    } catch (const cereal::Exception &) {
        return false;
    }
}

void Sandbox::guest_entry() {
    uid_t host_euid = geteuid();
    gid_t host_egid = getegid();
    if (unshare(CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC |
                CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET)) {
        perror("unshare");
        return;
    }
    std::ostringstream uid_map;
    uid_map << options_.uid << ' ' << host_euid << " 1";
    if (!write_file("/proc/self/uid_map", uid_map.str())) {
        std::cerr << "write uid_map failed" << std::endl;
        return;
    }
    // setgroups is not available on some systems, ignoring errors.
    write_file("/proc/self/setgroups", "deny");
    std::ostringstream gid_map;
    gid_map << options_.gid << ' ' << host_egid << " 1";
    if (!write_file("/proc/self/gid_map", gid_map.str())) {
        std::cerr << "write gid_map failed" << std::endl;
        return;
    }
    if (setresuid(options_.uid, options_.uid, options_.uid)) {
        perror("setresuid");
        return;
    }
    if (setresgid(options_.gid, options_.gid, options_.gid)) {
        perror("setresgid");
        return;
    }
    if (sethostname(options_.hostname.data(), options_.hostname.size())) {
        perror("sethostname");
        return;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        guest_init();
        return;
    }
    if (waitpid(pid, nullptr, 0) == -1) {
        perror("waitpid");
        return;
    }
    exit(0);
}

void Sandbox::guest_init() {
    // TODO(iceboy): Error handling.
    // TODO(iceboy): Handle zombies.
    mount("root", options_.temp_dir.c_str(), "tmpfs", MS_NOSUID, nullptr);
    chdir(options_.temp_dir.c_str());
    mkdir("proc", 0755);
    mount("proc", "proc", "proc", MS_NOSUID, nullptr);
    mkdir("dev", 0755);
    bind_node("/dev/null", "dev/null");
    bind_node("/dev/urandom", "dev/urandom");
    mkdir("tmp", 0755);
    mount("tmp", "tmp", "tmpfs", MS_NOSUID, "size=16m,nr_inodes=4k");
    bind_or_link("/bin", "bin");
    mkdir("etc", 0755);
    bind_or_link("/etc/alternatives", "etc/alternatives");
    bind_or_link("/lib", "lib");
    bind_or_link("/lib64", "lib64");
    mkdir("usr", 0755);
    bind_or_link("/usr/bin", "usr/bin");
    bind_or_link("/usr/include", "usr/include");
    bind_or_link("/usr/lib", "usr/lib");
    bind_or_link("/usr/lib64", "usr/lib64");
    bind_or_link("/usr/libexec", "usr/libexec");
    bind_or_link("/usr/share", "usr/share");
    mkdir("var", 0755);
    mkdir("var/lib", 0755);
    bind_or_link("/var/lib/ghc", "var/lib/ghc");
    for (const Mount &mount : options_.mounts) {
        bind_dir(mount.from, mount.to, mount.readonly);
    }
    std::ostringstream passwd;
    passwd << options_.username << ":x:" << options_.uid << ":" << options_.gid
           << ":" << options_.username << ":/:/bin/bash\n";
    write_file("etc/passwd", passwd.str());
    mkdir("old_root", 0755);
    pivot_root(".", "old_root");
    umount2("old_root", MNT_DETACH);
    rmdir("old_root");
    mount("/", "/", "", MS_BIND | MS_REMOUNT | MS_RDONLY | MS_NOSUID, nullptr);

    FdStreamBuf streambuf(guest_socket_);
    std::iostream stream(&streambuf);
    cereal::BinaryInputArchive input(stream);
    cereal::BinaryOutputArchive output(stream);
    try {
        while (stream) {
            ipc::Command command;
            input(command);
            if (command == ipc::Command::execute) {
                ipc::ExecuteRequest request;
                input(request);
                ipc::ExecuteResponse response;
                guest_execute(request, response);
                output(response);
            }
            stream.flush();
        }
    } catch (const cereal::Exception &e) {
    }
}

void Sandbox::guest_execute(
    const ipc::ExecuteRequest &request, ipc::ExecuteResponse &response) {
    pid_t pid = fork();
    if (pid == -1) {
        response.error = errno;
        return;
    }
    if (pid == 0) {
        guest_execute_child(request);
        return;
    }
    int wstatus;
    if (waitpid(pid, &wstatus, 0) == -1) {
        response.error = errno;
        return;
    }
    if (WIFSIGNALED(wstatus)) {
        response.result = -WTERMSIG(wstatus);
    } else {
        response.result = WEXITSTATUS(wstatus);
    }
}

void Sandbox::guest_execute_child(const ipc::ExecuteRequest &request) {
    for (const ipc::FileInfo &file : request.files) {
        if (file.inherit) {
            int flags = fcntl(file.fd, F_GETFD);
            if (flags == -1) {
                continue;
            }
            fcntl(file.fd, F_SETFD, flags & ~FD_CLOEXEC);
        } else {
            int fd = open(file.path.c_str(),
                          file.readonly ? O_RDONLY : O_RDWR | O_CREAT,
                          0644);
            if (fd == -1) {
                continue;
            }
            dup2(fd, file.fd);
            close(fd);
        }
    }
    std::vector<const char *> argv;
    for (const std::string &arg : request.args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    std::vector<const char *> envp;
    for (const std::string &env : request.envs) {
        envp.push_back(env.c_str());
    }
    envp.push_back(nullptr);
    guest_cgroup_sync();
    exit(execve(request.path.c_str(),
                const_cast<char **>(argv.data()),
                const_cast<char **>(envp.data())));
}

bool Sandbox::host_cgroup_sync() {
    char buffer = 0;
    iovec iov;
    iov.iov_base = &buffer;
    iov.iov_len = 1;
    char control[CMSG_SPACE(sizeof(ucred))];
    msghdr msgh;
    msgh.msg_name = nullptr;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = control;
    msgh.msg_controllen = sizeof(control);
    if (recvmsg(host_cgroup_socket_, &msgh, 0) < 0) {
        perror("recvmsg");
        return false;
    }
    cmsghdr *cmsgp = CMSG_FIRSTHDR(&msgh);
    if (!cmsgp ||
        cmsgp->cmsg_len != CMSG_LEN(sizeof(ucred)) ||
        cmsgp->cmsg_level != SOL_SOCKET ||
        cmsgp->cmsg_type != SCM_CREDENTIALS) {
        std::cerr << "invalid cmsg" << std::endl;
        return false;
    }
    ucred *ucredp = reinterpret_cast<ucred *>(CMSG_DATA(cmsgp));
    // TODO(iceboy): Add to cgroup.
    printf("guest pid = %d\n", ucredp->pid);
    if (send(host_cgroup_socket_, &buffer, 1, 0) < 0) {
        perror("send");
        return false;
    }
    return true;
}

void Sandbox::guest_cgroup_sync() {
    char buffer = 0;
    if (send(guest_cgroup_socket_, &buffer, 1, 0) < 0) {
        int err = errno;
        perror("send");
        exit(err);
    }
    if (recv(guest_cgroup_socket_, &buffer, 1, 0) < 0) {
        int err = errno;
        perror("recv");
        exit(err);
    }
}

}  // namespace sandbox
