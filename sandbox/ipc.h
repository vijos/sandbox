#ifndef _SANDBOX_IPC_H
#define _SANDBOX_IPC_H

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace sandbox {
namespace ipc {

enum class Command {
    execute,
};

struct FileInfo {
    int fd = 0;
    bool inherit = false;
    bool readonly = false;
    std::string path;

    template <typename A>
    void serialize(A &a) {
        a(fd, inherit, readonly, path);
    }
};

struct ExecuteRequest {
    std::string path;
    std::vector<std::string> args;
    std::vector<std::string> envs;
    std::vector<FileInfo> files;

    template <typename A>
    void serialize(A &a) {
        a(path, args, envs, files);
    }
};

struct ExecuteResponse {
    int error = 0;
    int result = 0;

    template <typename A>
    void serialize(A &a) {
        a(error, result);
    }
};

}  // namespace ipc
}  // namespace sandbox

#endif  // _SANDBOX_IPC_H
