#include <napi.h>
#include "sandbox/sandbox.h"

namespace sandbox {

void Hello(const Napi::CallbackInfo &info) {
    Sandbox::Options options;
    Sandbox sandbox(options);
    sandbox.init();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("hello", Napi::Function::New(env, Hello));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

}  // namespace sandbox
