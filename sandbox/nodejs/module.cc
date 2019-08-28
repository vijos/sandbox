#include <napi.h>
#include "sandbox/nodejs/sandbox-wrap.h"

namespace sandbox {
namespace nodejs {

Napi::Object init(Napi::Env env, Napi::Object exports) {
    SandboxWrap::init(env, exports);
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, init)

}  // namespace nodejs
}  // namespace sandbox
