/**
 * This addon uses a hybrid N-API/V8 approach:
 *
 * - N-API is used for module initialization, JS callback storage, and general
 *   JS interop. This provides ABI stability across Node.js versions.
 *
 * - V8 is used directly for SetModifyCodeGenerationFromStringsCallback because
 *   N-API has no equivalent API for intercepting eval/new Function() calls.
 *   This V8 API has been stable since Node.js 12, but changes could break
 *   prebuilt binaries. A runtime version check guards against this.
 *
 * V8-specific code is marked with [V8 API] comments below.
 */

#include <node_api.h>
#include <v8.h>  // [V8 API] Required for SetModifyCodeGenerationFromStringsCallback
#include <cstring>

#define MIN_NODE_VERSION 16
#define MAX_NODE_VERSION 25

static napi_env g_env = nullptr;
static napi_ref g_callback_ref = nullptr;

// [V8 API] This callback signature is defined by V8, not N-API.
// It receives V8 types directly from the engine when eval/Function is called.
v8::ModifyCodeGenerationFromStringsResult ModifyCodeGenCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> source,
    bool is_code_like) {

  if (g_callback_ref == nullptr || g_env == nullptr) {
    return {true, {}};
  }

  napi_value callback;
  napi_status status = napi_get_reference_value(g_env, g_callback_ref, &callback);
  if (status != napi_ok || callback == nullptr) {
    return {true, {}};
  }

  // [V8 API] Convert V8 source to N-API string
  // We must use V8 to extract the string since 'source' is a V8 type
  v8::Isolate* isolate = context->GetIsolate();
  v8::String::Utf8Value utf8(isolate, source);

  napi_value source_napi;
  status = napi_create_string_utf8(g_env, *utf8, utf8.length(), &source_napi);
  if (status != napi_ok) {
    return {true, {}};
  }

  // Call the JS callback
  napi_value global, result;
  napi_get_global(g_env, &global);

  napi_value argv[1] = { source_napi };
  status = napi_call_function(g_env, global, callback, 1, argv, &result);

  if (status != napi_ok) {
    // Exception was thrown - block code generation
    napi_value exception;
    bool is_pending;
    napi_is_exception_pending(g_env, &is_pending);
    if (is_pending) {
      napi_get_and_clear_last_exception(g_env, &exception);
    }
    return {false, {}};
  }

  // Check if result is a string (means block with custom message)
  napi_valuetype result_type;
  napi_typeof(g_env, result, &result_type);

  if (result_type == napi_string) {
    // Get string length and content
    size_t str_len;
    napi_get_value_string_utf8(g_env, result, nullptr, 0, &str_len);

    char* buf = new char[str_len + 1];
    napi_get_value_string_utf8(g_env, result, buf, str_len + 1, &str_len);

    // [V8 API] Set error message using V8 API
    // N-API has no equivalent for SetErrorMessageForCodeGenerationFromStrings
    v8::Local<v8::String> error_msg = v8::String::NewFromUtf8(
        isolate, buf, v8::NewStringType::kNormal, static_cast<int>(str_len))
        .ToLocalChecked();
    context->SetErrorMessageForCodeGenerationFromStrings(error_msg);

    delete[] buf;
    return {false, {}};
  }

  // Undefined or other = allow
  return {true, {}};
}

napi_value SetCallback(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  if (argc < 1) {
    napi_throw_type_error(env, nullptr, "Expected a callback function");
    return nullptr;
  }

  napi_valuetype arg_type;
  napi_typeof(env, argv[0], &arg_type);
  if (arg_type != napi_function) {
    napi_throw_type_error(env, nullptr, "Expected a callback function");
    return nullptr;
  }

  // Clean up existing reference if any
  if (g_callback_ref != nullptr) {
    napi_delete_reference(g_env, g_callback_ref);
    g_callback_ref = nullptr;
  }

  // Store env and create reference to callback
  g_env = env;
  napi_create_reference(env, argv[0], 1, &g_callback_ref);

  // [V8 API] Register the V8 callback
  // This is the core V8-specific API - N-API has no equivalent for intercepting
  // code generation from strings (eval, new Function, etc.)
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  isolate->SetModifyCodeGenerationFromStringsCallback(ModifyCodeGenCallback);

  return nullptr;
}

napi_value Init(napi_env env, napi_value exports) {
  // Check Node.js version at runtime
  const napi_node_version* node_version;
  napi_get_node_version(env, &node_version);

  if (node_version->major < MIN_NODE_VERSION || node_version->major > MAX_NODE_VERSION) {
    napi_throw_error(env, nullptr,
      "zen-internals-node: Unsupported Node.js version. "
      "This prebuilt binary supports Node.js 16-25. "
      "Please rebuild from source or use a supported version.");
    return nullptr;
  }

  napi_property_descriptor desc = {
    "setCodeGenerationCallback",  // name
    nullptr,                       // symbol
    SetCallback,                   // method
    nullptr,                       // getter
    nullptr,                       // setter
    nullptr,                       // value
    napi_default,                  // attributes
    nullptr                        // data
  };
  napi_define_properties(env, exports, 1, &desc);
  return exports;
}

NAPI_MODULE(codegen_hook, Init)
