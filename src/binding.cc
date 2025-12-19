/**
 * This addon uses pure V8 API for the code generation callback:
 *
 * - N-API is used only for module initialization and exporting functions.
 *
 * - V8 is used directly for SetModifyCodeGenerationFromStringsCallback and
 *   for storing/calling the JS callback. This is necessary because the callback
 *   can be invoked during V8 internal operations (microtasks, module parsing)
 *   where N-API functions cannot be safely called due to locking requirements.
 *
 * V8-specific code is marked with [V8 API] comments below.
 */

#include <node_api.h>
#include <v8.h>  // [V8 API] Required for SetModifyCodeGenerationFromStringsCallback
#include <cstring>

// [V8 API] Store callback as V8 Persistent to avoid N-API locking issues
static v8::Persistent<v8::Function> g_callback;
static v8::Isolate* g_isolate = nullptr;

// [V8 API] This callback signature is defined by V8, not N-API.
// It receives V8 types directly from the engine when eval/Function is called.
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

  // [V8 API] Create a HandleScope for our local handles
  v8::HandleScope handle_scope(isolate);

  // Get the callback function from the persistent handle
  v8::Local<v8::Function> callback = g_callback.Get(isolate);
  if (callback.IsEmpty()) {
    return {true, {}};
  }

  // Prepare arguments - source is already a V8 value
  v8::Local<v8::Value> argv[1] = { source };

  // Call the JS callback
  v8::TryCatch try_catch(isolate);
  v8::MaybeLocal<v8::Value> maybe_result = callback->Call(context, context->Global(), 1, argv);

  if (try_catch.HasCaught()) {
    // The callback threw an exception - default to allow
    return {true, {}};
  }

  if (maybe_result.IsEmpty()) {
    return {true, {}};
  }

  v8::Local<v8::Value> result = maybe_result.ToLocalChecked();

  // Check if result is a string (means block with custom message)
  if (result->IsString()) {
    v8::Local<v8::String> error_msg = result.As<v8::String>();
    context->SetErrorMessageForCodeGenerationFromStrings(error_msg);
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

  // [V8 API] Get the V8 isolate and function from N-API values
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  // Clean up existing callback if any
  if (!g_callback.IsEmpty()) {
    g_callback.Reset();
  }

  // [V8 API] Convert N-API value to V8 Local and store as Persistent
  // N-API values are wrappers around V8 values, so we can cast them
  v8::Local<v8::Value> v8_value = reinterpret_cast<v8::Local<v8::Value>*>(&argv[0])[0];
  v8::Local<v8::Function> v8_func = v8_value.As<v8::Function>();

  g_isolate = isolate;
  g_callback.Reset(isolate, v8_func);

  // [V8 API] Register the V8 callback
  // This is the core V8-specific API - N-API has no equivalent for intercepting
  // code generation from strings (eval, new Function, etc.)
  isolate->SetModifyCodeGenerationFromStringsCallback(ModifyCodeGenCallback);

  return nullptr;
}

napi_value Init(napi_env env, napi_value exports) {
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
