#ifndef _SANDBOX_SANDBOX_H
#define _SANDBOX_SANDBOX_H

#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include "sandbox/ipc.h"
#include "sandbox/util.h"

namespace sandbox {

struct Mount {
    std::string from;
    std::string to;
    bool readonly = true;
};

class Sandbox {
public:
    struct Options {
        std::string temp_dir = "/tmp";
        uid_t uid = 1000;
        gid_t gid = 1000;
        std::vector<Mount> mounts;
        std::string hostname = "host";
        std::string username = "user";
    };

    explicit Sandbox(const Options &options);
    ~Sandbox();

    Sandbox(const Sandbox &) = delete;
    Sandbox &operator=(const Sandbox &) = delete;

    bool init();
    bool shell(
        const ipc::ShellRequest &request, ipc::ShellResponse &response);

private:
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
