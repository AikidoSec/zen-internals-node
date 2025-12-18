{
  "targets": [
    {
      "target_name": "zen-internals-node",
      "sources": ["src/binding.cc"],
      "defines": ["NAPI_VERSION=8"],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "AdditionalOptions": ["/std:c++20"]
            }
          }
        }],
        ["OS!='win'", {
          "cflags_cc": ["-std=c++20"],
          "xcode_settings": {
            "CLANG_CXX_LANGUAGE_STANDARD": "c++20"
          }
        }]
      ]
    }
  ],
  "variables": {
    "openssl_fips": ""
  }
}
