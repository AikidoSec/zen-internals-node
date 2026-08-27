{
  "targets": [
    {
      "target_name": "zen-internals-node",
      "sources": ["src/binding.cc", "src/ip_matcher.cc"],
      "defines": ["NAPI_VERSION=8"],
      "conditions": [
        ["OS=='win'", {
          # Node 26 leaks Clang ThinLTO options into MSVC addon builds.
          "msvs_settings": {
            "VCCLCompilerTool": {
              "AdditionalOptions": ["/std:c++20"],
              "AdditionalOptions/": [["exclude", "flto"], ["exclude", "lldltojobs"]],
              "ExceptionHandling": 1
            },
            "VCLibrarianTool": {
              "AdditionalOptions/": [["exclude", "flto"], ["exclude", "lldltojobs"]]
            },
            "VCLinkerTool": {
              "AdditionalOptions/": [["exclude", "flto"], ["exclude", "lldltojobs"]]
            }
          }
        }],
        ["OS!='win'", {
          "cflags!": ["-fno-exceptions"],
          "cflags_cc!": ["-fno-exceptions"],
          "cflags_cc": ["-std=c++20", "-fexceptions"],
          "xcode_settings": {
            "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
          }
        }]
      ]
    }
  ],
  "variables": {
    "openssl_fips": ""
  }
}
