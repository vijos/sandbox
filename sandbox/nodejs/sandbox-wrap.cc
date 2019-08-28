#include "sandbox/nodejs/sandbox-wrap.h"

namespace sandbox {
namespace nodejs {

void SandboxWrap::init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Sandbox", {
        InstanceMethod("execute", &SandboxWrap::execute),
    });
    exports.Set("Sandbox", func);
}

void SandboxWrap::execute(const Napi::CallbackInfo &info) {
    printf("SandboxWrap::execute()\n");
}

}  // namespace nodejs
}  // namespace sandbox
