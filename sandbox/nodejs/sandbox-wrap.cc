#include "sandbox/nodejs/sandbox-wrap.h"

namespace sandbox {
namespace nodejs {
namespace {

Sandbox::Options get_sandbox_options(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) {
        return Sandbox::Options();
    }
    if (!info[0].IsObject()) {
        NAPI_THROW(
            Napi::TypeError::New(env, "invalid argument"),
            Sandbox::Options());
    }
    Napi::Object js_options = info[0].ToObject();
    Sandbox::Options options;
    if (js_options.Has("tempDir")) {
        options.temp_dir = js_options.Get("tempDir").ToString().Utf8Value();
    }
    if (js_options.Has("guestUid")) {
        options.guest_uid = js_options.Get("guestUid").ToNumber().Uint32Value();
    }
    if (js_options.Has("guestGid")) {
        options.guest_gid = js_options.Get("guestGid").ToNumber().Uint32Value();
    }
    if (js_options.Has("guestHostname")) {
        options.guest_hostname =
            js_options.Get("guestHostname").ToString().Utf8Value();
    }
    return options;
}

}  // namespace

void SandboxWrap::init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Sandbox", {
        InstanceMethod("execute", &SandboxWrap::execute),
    });
    exports.Set("Sandbox", func);
}

SandboxWrap::SandboxWrap(const Napi::CallbackInfo &info)
    : ObjectWrap(info),
      Sandbox(get_sandbox_options(info)) {
    Napi::Env env = info.Env();
    if (!Sandbox::init()) {
        NAPI_THROW_VOID(Napi::Error::New(env, "init failed"));
    }
}

void SandboxWrap::execute(const Napi::CallbackInfo &info) {
    // TODO(iceboy): async
    ExecuteOptions options;
    Sandbox::execute(options);
}

}  // namespace nodejs
}  // namespace sandbox
