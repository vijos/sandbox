#ifndef _SANDBOX_NODEJS_SANDBOX_WRAP_H
#define _SANDBOX_NODEJS_SANDBOX_WRAP_H

#include <napi.h>
#include "sandbox/sandbox.h"

namespace sandbox {
namespace nodejs {

class SandboxWrap : public Napi::ObjectWrap<SandboxWrap>, private Sandbox {
public:
    static void init(Napi::Env env, Napi::Object exports);

    explicit SandboxWrap(const Napi::CallbackInfo &info);

    void execute(const Napi::CallbackInfo &info);
};

}  // namespace nodejs
}  // namespace sandbox

#endif  // _SANDBOX_NODEJS_SANDBOX_WRAP_H
