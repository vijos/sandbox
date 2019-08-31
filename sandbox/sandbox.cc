#include "sandbox/sandbox.h"

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
    // TODO(iceboy): when does nodejs destruct objects?
    // TODO(iceboy): send terminate signal.
    if (guest_pid_) {
        waitpid(guest_pid_, nullptr, 0);
    }
    if (guest_socket_ != -1) {
        close(guest_socket_);
    }
    if (host_socket_ != -1) {
        close(host_socket_);
    }
    if (!root_dir_.empty()) {
        rmdir(root_dir_.c_str());
    }
}

bool Sandbox::init() {
    return init_dirs() && init_sockets() && init_guest();
}

bool Sandbox::shell(
    const ipc::ShellRequest &request, ipc::ShellResponse &response) {
    try {
        cereal::BinaryOutputArchive output(host_stream_);
        output(ipc::Command::shell);
        output(request);
        host_stream_.flush();
        cereal::BinaryInputArchive input(host_stream_);
        input(response);
        return true;
    } catch (const cereal::Exception &) {
        return false;
    }
}

bool Sandbox::init_dirs() {
    std::string root_dir = options_.temp_dir + "/sandbox.XXXXXX";
    if (!mkdtemp(&root_dir[0])) {
        perror("mkdtemp");
        return false;
    }
    root_dir_ = root_dir;
    return true;
}

bool Sandbox::init_sockets() {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds)) {
        perror("socketpair");
        return false;
    }
    host_socket_ = fds[0];
    guest_socket_ = fds[1];
    host_streambuf_.fd(host_socket_);
    return true;
}

bool Sandbox::init_guest() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return false;
    }
    // We don't close either host or guest sockets after fork and we don't rely
    // on the close semantics, as there can be open sockets from other sandbox
    // instances.
    if (pid == 0) {
        guest_entry();
        exit(1);
        return false;
    }
    guest_pid_ = pid;
    return true;
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
    uid_map << options_.guest_uid << ' ' << host_euid << " 1";
    if (!write_file("/proc/self/uid_map", uid_map.str())) {
        std::cerr << "write uid_map failed" << std::endl;
        return;
    }
    // setgroups is not available on some systems, ignoring errors.
    write_file("/proc/self/setgroups", "deny");
    std::ostringstream gid_map;
    gid_map << options_.guest_gid << ' ' << host_egid << " 1";
    if (!write_file("/proc/self/gid_map", gid_map.str())) {
        std::cerr << "write gid_map failed" << std::endl;
        return;
    }
    if (setresuid(options_.guest_uid, options_.guest_uid, options_.guest_uid)) {
        perror("setresuid");
        return;
    }
    if (setresgid(options_.guest_gid, options_.guest_gid, options_.guest_gid)) {
        perror("setresgid");
        return;
    }
    if (sethostname(options_.guest_hostname.data(),
                    options_.guest_hostname.size())) {
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
    mount("root", root_dir_.c_str(), "tmpfs", MS_NOSUID, nullptr);
    chdir(root_dir_.c_str());
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
    // TODO(iceboy): bind user directories.
    std::ostringstream passwd;
    passwd << options_.guest_username << ":x:" << options_.guest_uid << ":"
           << options_.guest_gid << ":" << options_.guest_username
           << ":/:/bin/bash\n";
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
    ipc::Command command;
    while (stream) {
        try {
            input(command);
            if (command == ipc::Command::shell) {
                ipc::ShellRequest request;
                input(request);
                ipc::ShellResponse response;
                guest_shell(request, response);
                output(response);
            }
        } catch (const cereal::Exception &) {
            break;
        }
        stream.flush();
    }
}

void Sandbox::guest_shell(
    const ipc::ShellRequest &request, ipc::ShellResponse &response) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        fcntl(0, F_SETFD, fcntl(0, F_GETFD) & ~FD_CLOEXEC);
        fcntl(1, F_SETFD, fcntl(1, F_GETFD) & ~FD_CLOEXEC);
        fcntl(2, F_SETFD, fcntl(2, F_GETFD) & ~FD_CLOEXEC);
        execl("/bin/bash", "shell", nullptr);
        return;
    }
    if (waitpid(pid, &response.wstatus, 0) == -1) {
        perror("waitpid");
        return;
    }
}

}  // namespace sandbox
