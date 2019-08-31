#ifndef _SANDBOX_IPC_H
#define _SANDBOX_IPC_H

namespace sandbox {
namespace ipc {

enum class Command {
    shell,
};

struct ShellRequest {};

struct ShellResponse {
    int wstatus;

    template <typename A>
    void serialize(A &a) {
        a(wstatus);
    }
};

}  // namespace ipc
}  // namespace sandbox

#endif  // _SANDBOX_IPC_H
