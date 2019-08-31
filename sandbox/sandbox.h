#ifndef _SANDBOX_SANDBOX_H
#define _SANDBOX_SANDBOX_H

#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include "sandbox/ipc.h"
#include "sandbox/util.h"

namespace sandbox {

class Sandbox {
public:
    struct Options {
        std::string temp_dir = "/tmp";
        uid_t guest_uid = 1000;
        gid_t guest_gid = 1000;
        std::string guest_hostname = "sandbox";
        std::string guest_username = "user";
    };

    explicit Sandbox(const Options &options);
    ~Sandbox();

    Sandbox(const Sandbox &) = delete;
    Sandbox &operator=(const Sandbox &) = delete;

    bool init();
    bool shell(
        const ipc::ShellRequest &request, ipc::ShellResponse &response);

private:
    bool init_dirs();
    bool init_sockets();
    bool init_guest();
    void guest_entry();
    void guest_init();
    void guest_shell(
        const ipc::ShellRequest &request, ipc::ShellResponse &response);

    Options options_;
    std::string root_dir_;
    int host_socket_ = -1;
    int guest_socket_ = -1;
    pid_t guest_pid_ = 0;
    FdStreamBuf host_streambuf_;
    std::iostream host_stream_;
};

}  // namespace sandbox

#endif  // _SANDBOX_SANDBOX_H
