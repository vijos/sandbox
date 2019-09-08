{
    "targets": [
        {
            "target_name": "sandbox",
            "sources": [
                "sandbox/sandbox.cc",
                "sandbox/util.cc",
                "sandbox/nodejs/module.cc",
                "sandbox/nodejs/sandbox-wrap.cc"
            ],
            "include_dirs": [
                "<!@(node -p \"require('node-addon-api').include\")",
                ".",
                "node_modules/cereal/include"
            ],
            "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
            "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
            "cflags_cc!": ["-fno-exceptions", "-fno-rtti"]
        }
    ]
}
