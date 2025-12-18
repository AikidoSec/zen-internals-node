{
  "targets": [
    {
      "target_name": "codegen_hook",
      "sources": ["src/binding.cc"],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}
