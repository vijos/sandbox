#include "sandbox/sandbox.h"

#include <iostream>
#include <sstream>
#include <sched.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "sandbox/util.h"

namespace sandbox {

Sandbox::Sandbox(const Options &options)
    : options_(options) {}

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
        do_guest();
        exit(1);
        return false;
    }
    guest_pid_ = pid;
    return true;
}

void Sandbox::do_guest() {
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
        do_guest_init();
        return;
    }
    if (waitpid(pid, nullptr, 0) == -1) {
        perror("waitpid");
        return;
    }
    exit(0);
}

void Sandbox::do_guest_init() {
    // TODO
}

}  // namespace sandbox
