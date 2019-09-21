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
    bool execute(
        const ipc::ExecuteRequest &request, ipc::ExecuteResponse &response);

private:
    void guest_entry();
    void guest_init();
    void guest_execute(
        const ipc::ExecuteRequest &request, ipc::ExecuteResponse &response);
    void guest_execute_child(const ipc::ExecuteRequest &request);

    bool host_cgroup_sync();
    void guest_cgroup_sync();

    Options options_;
    int host_socket_ = -1;
    int guest_socket_ = -1;
    FdStreamBuf host_streambuf_;
    std::iostream host_stream_;
    int host_cgroup_socket_ = -1;
    int guest_cgroup_socket_ = -1;
};

}  // namespace sandbox

#endif  // _SANDBOX_SANDBOX_H
