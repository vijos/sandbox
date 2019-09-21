#include "sandbox/nodejs/sandbox-wrap.h"

#include <sstream>
#include <string>
#include <utility>
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

class SandboxWrap::ExecuteWorker : public Napi::AsyncWorker {
public:
    explicit ExecuteWorker(
        SandboxWrap &wrap,
        const Napi::CallbackInfo &info,
        Napi::Promise::Deferred deferred);

    void Execute() override;
    void OnOK() override;
    void OnError(const Napi::Error &e) override;

private:
    void populate_files(Napi::Object files);

    SandboxWrap &wrap_;
    Napi::Promise::Deferred deferred_;
    ipc::ExecuteRequest request_;
    ipc::ExecuteResponse response_;
};

Napi::Value SandboxWrap::execute(const Napi::CallbackInfo &info) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(info.Env());
    (new ExecuteWorker(*this, info, deferred))->Queue();
    return deferred.Promise();
}

SandboxWrap::ExecuteWorker::ExecuteWorker(
    SandboxWrap &wrap,
    const Napi::CallbackInfo &info,
    Napi::Promise::Deferred deferred)
    : AsyncWorker(info.Env()), wrap_(wrap), deferred_(deferred) {
    Napi::Object params = info[0].ToObject();
    if (params.Has("path")) {
        request_.path = params.Get("path").ToString().Utf8Value();
    }
    if (params.Has("args")) {
        js_to_native(params.Get("args").As<Napi::Array>(), request_.args);
    }
    if (params.Has("envs")) {
        js_to_native(params.Get("envs").As<Napi::Array>(), request_.envs);
    }
    if (params.Has("files")) {
        populate_files(params.Get("files").ToObject());
    }
}

void SandboxWrap::ExecuteWorker::Execute() {
    if (!wrap_.Sandbox::execute(request_, response_)) {
        SetError("execute failed: ipc");
    }
    if (response_.error) {
        std::ostringstream error;
        error << "execute failed: " << response_.error;
        SetError(error.str());
    }
}

void SandboxWrap::ExecuteWorker::OnOK() {
    deferred_.Resolve(Napi::Number::From(Env(), response_.result));
}

void SandboxWrap::ExecuteWorker::OnError(const Napi::Error &e) {
    deferred_.Reject(e.Value());
}

void SandboxWrap::ExecuteWorker::populate_files(Napi::Object files) {
    Napi::Array names = files.GetPropertyNames();
    for (uint32_t i = 0; i < names.Length(); ++i) {
        Napi::Value name = names.Get(i);
        if (!name.IsNumber()) {
            continue;
        }
        Napi::Object value = files.Get(name).ToObject();
        ipc::FileInfo file;
        file.fd = static_cast<int>(name.ToNumber().Int32Value());
        file.inherit = value.Get("inherit").ToBoolean().Value();
        file.readonly = value.Get("readonly").ToBoolean().Value();
        file.path = value.Get("path").ToString().Utf8Value();
        request_.files.push_back(std::move(file));
    }
}

}  // namespace nodejs
}  // namespace sandbox
