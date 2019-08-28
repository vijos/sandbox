#include "sandbox/sandbox.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

namespace sandbox {

Sandbox::Sandbox(const Options &options)
    : options_(options) {}

Sandbox::~Sandbox() {
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
}

bool Sandbox::init() {
    return init_dirs() && init_sockets() && init_guest();
}

bool Sandbox::init_dirs() {
    // TODO
    return true;
}

bool Sandbox::init_sockets() {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds)) {
        return false;
    }
    host_socket_ = fds[0];
    guest_socket_ = fds[1];
    return true;
}

bool Sandbox::init_guest() {
    pid_t pid = fork();
    if (pid == -1) {
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
    printf("hello from guest\n");
}

}  // namespace sandbox
