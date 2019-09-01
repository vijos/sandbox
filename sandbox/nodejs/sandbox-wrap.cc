#include "sandbox/nodejs/sandbox-wrap.h"

#include <sstream>
#include <string>
#include <vector>

namespace sandbox {
namespace nodejs {
namespace {

void js_to_native(Napi::Array input, std::vector<std::string> &output) {
    output.clear();
    for (uint32_t i = 0; i < input.Length(); ++i) {
        output.push_back(input.Get(i).ToString().Utf8Value());
    }
}

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
    if (js_options.Has("uid")) {
        options.uid = js_options.Get("uid").ToNumber().Uint32Value();
    }
    if (js_options.Has("gid")) {
        options.gid = js_options.Get("gid").ToNumber().Uint32Value();
    }
    if (js_options.Has("mounts")) {
        Napi::Array js_mounts = js_options.Get("mounts").As<Napi::Array>();
        for (uint32_t i = 0; i < js_mounts.Length(); ++i) {
            Napi::Object js_mount = js_mounts.Get(i).ToObject();
            options.mounts.emplace_back();
            auto &mount = options.mounts.back();
            mount.from = js_mount.Get("from").ToString();
            mount.to = js_mount.Get("to").ToString();
            if (js_mount.Has("readonly")) {
                mount.readonly = js_mount.Get("readonly").ToBoolean().Value();
            }
        }
    }
    if (js_options.Has("hostname")) {
        options.hostname = js_options.Get("hostname").ToString().Utf8Value();
    }
    if (js_options.Has("username")) {
        options.username = js_options.Get("username").ToString().Utf8Value();
    }
    return options;
}

}  // namespace

void SandboxWrap::init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Sandbox", {
        InstanceMethod("shell", &SandboxWrap::shell),
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

class SandboxWrap::ShellWorker : public Napi::AsyncWorker {
public:
    explicit ShellWorker(
        SandboxWrap &wrap,
        const Napi::CallbackInfo &info,
        Napi::Promise::Deferred deferred)
        : AsyncWorker(info.Env()), wrap_(wrap), deferred_(deferred) {
        if (info.Length() >= 1) {
            request_.path = info[0].ToString().Utf8Value();
        }
        if (info.Length() >= 2) {
            js_to_native(info[1].As<Napi::Array>(), request_.args);
        }
        if (info.Length() >= 3) {
            js_to_native(info[2].As<Napi::Array>(), request_.envs);
        }
    }

    void Execute() override {
        if (!wrap_.Sandbox::shell(request_, response_)) {
            SetError("shell failed: ipc");
        }
        if (response_.error) {
            std::ostringstream error;
            error << "shell failed: " << response_.error;
            SetError(error.str());
        }
    }

    void OnOK() override {
        deferred_.Resolve(Napi::Number::From(Env(), response_.result));
    }

    void OnError(const Napi::Error &e) override {
        deferred_.Reject(e.Value());
    }

private:
    SandboxWrap &wrap_;
    Napi::Promise::Deferred deferred_;
    ipc::ShellRequest request_;
    ipc::ShellResponse response_;
};

Napi::Value SandboxWrap::shell(const Napi::CallbackInfo &info) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(info.Env());
    (new ShellWorker(*this, info, deferred))->Queue();
    return deferred.Promise();
}

}  // namespace nodejs
}  // namespace sandbox
