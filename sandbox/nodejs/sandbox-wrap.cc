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
        // TODO(iceboy): Populate request_ from info .
    }

    void Execute() override {
        // TODO(iceboy): Error handling.
        wrap_.Sandbox::shell(request_, response_);
    }

    void OnOK() override {
        // TODO(iceboy): Populate value from response_.
        printf("response_.wstatus = %d\n", response_.wstatus);
        deferred_.Resolve(Env().Undefined());
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
