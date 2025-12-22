/**
 * Uses V8's SetModifyCodeGenerationFromStringsCallback to intercept eval/Function calls.
 * SetErrorMessageForCodeGenerationFromStrings sets the custom error message.
 *
 * Based on Node's --disallow-code-generation-from-strings flag behavior.
 */

#include <node_api.h>
#include <v8.h>
#include <cstring>

static v8::Persistent<v8::Function> g_callback;
static v8::Isolate* g_isolate = nullptr;

v8::ModifyCodeGenerationFromStringsResult ModifyCodeGenCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> source,
    bool is_code_like) {

  if (g_isolate == nullptr || g_callback.IsEmpty()) {
    return {true, {}};
  }

  v8::Isolate* isolate = context->GetIsolate();

  // Ensure we're on the same isolate where the callback was registered
  if (isolate != g_isolate) {
    return {true, {}};
  }

  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Function> callback = g_callback.Get(isolate);
  if (callback.IsEmpty()) {
    return {true, {}};
  }

  v8::Local<v8::Value> argv[1] = { source };

  v8::TryCatch try_catch(isolate);
  v8::MaybeLocal<v8::Value> maybe_result = callback->Call(context, context->Global(), 1, argv);

  if (try_catch.HasCaught()) {
    // The callback threw an exception, allow code generation
    return {true, {}};
  }

  if (maybe_result.IsEmpty()) {
    return {true, {}};
  }

  v8::Local<v8::Value> result = maybe_result.ToLocalChecked();

  // String result = block with custom error message
  if (result->IsString()) {
    v8::Local<v8::String> error_msg = result.As<v8::String>();
    context->SetErrorMessageForCodeGenerationFromStrings(error_msg);
    return {false, {}};
  }

  // Any other result, allow code generation
  return {true, {}};
}

napi_value SetCodeGenerationCallback(napi_env env, napi_callback_info info) {
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

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (isolate == nullptr) {
    napi_throw_error(env, nullptr, "Failed to get V8 isolate");
    return nullptr;
  }

  // Convert napi_value to v8::Local<v8::Value> using memcpy to avoid strict-aliasing violations
  v8::Local<v8::Value> v8_value;
  std::memcpy(&v8_value, &argv[0], sizeof(v8_value));
  v8::Local<v8::Function> v8_func = v8_value.As<v8::Function>();

  g_isolate = isolate;
  g_callback.Reset(isolate, v8_func);

  // Register the V8 callback (safe to call multiple times, only needs to be set once)
  isolate->SetModifyCodeGenerationFromStringsCallback(ModifyCodeGenCallback);

  return nullptr;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc = {
    "setCodeGenerationCallback",  // name
    nullptr,                      // symbol
    SetCodeGenerationCallback,    // method
    nullptr,                      // getter
    nullptr,                      // setter
    nullptr,                      // value
    napi_default,                 // attributes
    nullptr                       // data
  };
  napi_define_properties(env, exports, 1, &desc);
  return exports;
}

NAPI_MODULE(codegen_hook, Init)
