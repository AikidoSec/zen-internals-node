{
  "targets": [
    {
      "target_name": "zen-internals-node",
      "sources": ["src/binding.cc"],
      "defines": ["NAPI_VERSION=8"],
      # Run `npm run build:rust` first to build the Rust library linked below.
      "conditions": [
        ["OS=='win'", {
          "libraries": ["<(module_root_dir)/rust/target/release/zen_ip_matcher_node.lib"],
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
        ["OS=='linux'", {
          "libraries": ["<(module_root_dir)/rust/target/release/libzen_ip_matcher_node.a"],
          # Remove unused code and don't export the Rust library's symbols from the .node.
          "ldflags": ["-Wl,--gc-sections", "-Wl,--exclude-libs,ALL"]
        }],
        ["OS=='mac'", {
          "libraries": ["<(module_root_dir)/rust/target/release/libzen_ip_matcher_node.a"],
          "xcode_settings": {"DEAD_CODE_STRIPPING": "YES"}
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
