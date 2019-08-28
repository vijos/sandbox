{
    "targets": [
        {
            "target_name": "sandbox",
            "sources": [
                "sandbox/sandbox.cc",
                "sandbox/nodejs/module.cc",
                "sandbox/nodejs/sandbox-wrap.cc",
            ],
            "include_dirs": [
                "<!@(node -p \"require('node-addon-api').include\")",
                "."
            ],
            "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
            "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"]
        }
    ]
}
