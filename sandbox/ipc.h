#ifndef _SANDBOX_IPC_H
#define _SANDBOX_IPC_H

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace sandbox {
namespace ipc {

enum class Command {
    shell,
};

struct ShellRequest {
    std::string path;
    std::vector<std::string> args;
    std::vector<std::string> envs;

    template <typename A>
    void serialize(A &a) {
        a(path, args, envs);
    }
};

struct ShellResponse {
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
