#include <nan.h>

using v8::Context;
using v8::Function;
using v8::Local;
using v8::MaybeLocal;
using v8::ModifyCodeGenerationFromStringsResult;
using v8::String;
using v8::Value;

static Nan::Persistent<Function> g_js_callback;

ModifyCodeGenerationFromStringsResult ModifyCodeGenCallback(
    Local<Context> context,
    Local<Value> source,
    bool is_code_like) {

  Nan::HandleScope scope;

  if (g_js_callback.IsEmpty()) {
    return {true, {}};
  }

  Local<Function> callback = Nan::New(g_js_callback);
  Local<Value> argv[1] = { source };

  MaybeLocal<Value> maybe_result = Nan::Call(callback, context->Global(), 1, argv);

  if (maybe_result.IsEmpty()) {
    return {false, {}};
  }

  Local<Value> result = maybe_result.ToLocalChecked();

  // String = block with custom message
  if (result->IsString()) {
    context->SetErrorMessageForCodeGenerationFromStrings(result.As<String>());
    return {false, {}};
  }

  // Undefined = allow
  return {true, {}};
}

NAN_METHOD(SetCallback) {
  if (info.Length() < 1 || !info[0]->IsFunction()) {
    Nan::ThrowTypeError("Expected a callback function");
    return;
  }

  g_js_callback.Reset(info[0].As<Function>());
  info.GetIsolate()->SetModifyCodeGenerationFromStringsCallback(ModifyCodeGenCallback);
}

NAN_MODULE_INIT(Init) {
  Nan::SetMethod(target, "setCodeGenerationCallback", SetCallback);
}

NODE_MODULE(codegen_hook, Init)
