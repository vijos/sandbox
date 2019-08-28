#include <napi.h>

namespace sandbox {

Napi::String Hello(const Napi::CallbackInfo &info) {
    return Napi::String::New(info.Env(), "world");
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("hello", Napi::Function::New(env, Hello));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

}  // namespace sandbox
