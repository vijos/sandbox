#ifndef _SANDBOX_SANDBOX_H
#define _SANDBOX_SANDBOX_H

#include <string>
#include <vector>
#include <unistd.h>

namespace sandbox {

class Sandbox {
public:
    struct Options {
        std::string temp_dir = "/tmp";
        uid_t guest_uid = 1000;
        gid_t guest_gid = 1000;
        std::string guest_hostname = "sandbox";
    };

    explicit Sandbox(const Options &options);
    ~Sandbox();

    Sandbox(const Sandbox &) = delete;
    Sandbox &operator=(const Sandbox &) = delete;

    bool init();

    struct ExecuteOptions {
        std::string file;
        std::vector<std::string> args;
    };
    void execute(const ExecuteOptions &options);

private:
    bool init_dirs();
    bool init_sockets();
    bool init_guest();
    void do_guest();
    void do_guest_init();

    Options options_;
    std::string root_dir_;
    int host_socket_ = -1;
    int guest_socket_ = -1;
    pid_t guest_pid_ = 0;
};

}  // namespace sandbox

#endif  // _SANDBOX_SANDBOX_H
