/**
 * Uses V8's SetModifyCodeGenerationFromStringsCallback to intercept eval/Function calls.
 * SetErrorMessageForCodeGenerationFromStrings sets the custom error message.
 *
 * Based on Node's --disallow-code-generation-from-strings flag behavior.
 */

#include <node_api.h>
#include <v8.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ip_matcher.h"

static v8::Persistent<v8::Function> g_callback;
static v8::Isolate* g_isolate = nullptr;

constexpr uint32_t kMaxIPMatcherNetworks = 1'000'000;
constexpr size_t kMaxIPMatcherInputBytes = 64 * 1024 * 1024;
constexpr uint32_t kInitialIPMatcherCapacity = 1024;
constexpr size_t kIPMatcherStackNetworkBytes = 128;

v8::ModifyCodeGenerationFromStringsResult ModifyCodeGenCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> source,
    bool is_code_like) {

  if (g_isolate == nullptr || g_callback.IsEmpty()) {
    return {true, {}};
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();

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

struct BuildIPMatcherData {
  napi_async_work work;
  napi_deferred deferred;
  std::vector<std::string> networks;
  std::unique_ptr<ip_matcher::IPMatcher> matcher;
};

struct WrappedIPMatcher {
  std::unique_ptr<ip_matcher::IPMatcher> matcher;
  int64_t external_memory = 0;
};

bool GetStringLength(napi_env env, napi_value value, size_t* length) {
  napi_valuetype type;
  return napi_typeof(env, value, &type) == napi_ok &&
      type == napi_string &&
      napi_get_value_string_utf8(env, value, nullptr, 0, length) == napi_ok;
}

bool CopyString(napi_env env, napi_value value, size_t length, std::string* result) {
  result->resize(length + 1);
  size_t copied;
  if (napi_get_value_string_utf8(env, value, result->data(), result->size(), &copied) != napi_ok) {
    return false;
  }
  result->resize(copied);
  return true;
}

bool GetString(napi_env env, napi_value value, std::string* result) {
  size_t length;
  return GetStringLength(env, value, &length) && CopyString(env, value, length, result);
}

napi_value CreateError(napi_env env, const char* message) {
  napi_value error_message;
  napi_value error;
  napi_create_string_utf8(env, message, NAPI_AUTO_LENGTH, &error_message);
  napi_create_error(env, nullptr, error_message, &error);
  return error;
}

void DeleteIPMatcher(napi_env env, void* data, void*) {
  auto* wrapped_matcher = static_cast<WrappedIPMatcher*>(data);
  if (env != nullptr && wrapped_matcher->external_memory > 0) {
    int64_t adjusted_external_memory;
    napi_adjust_external_memory(env, -wrapped_matcher->external_memory, &adjusted_external_memory);
  }
  delete wrapped_matcher;
}

napi_value IPMatcherHas(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_value receiver;
  napi_get_cb_info(env, info, &argc, argv, &receiver, nullptr);

  WrappedIPMatcher* wrapped_matcher = nullptr;
  bool matches = false;
  try {
    if (argc == 1 &&
        napi_unwrap(env, receiver, reinterpret_cast<void**>(&wrapped_matcher)) == napi_ok &&
        wrapped_matcher != nullptr) {
      std::array<char, kIPMatcherStackNetworkBytes> network_buffer;
      size_t copied;
      if (napi_get_value_string_utf8(
              env, argv[0], network_buffer.data(), network_buffer.size(), &copied) == napi_ok) {
        if (copied < network_buffer.size() - 1) {
          matches = wrapped_matcher->matcher->Has(std::string_view(network_buffer.data(), copied));
        } else {
          std::string network;
          if (GetString(env, argv[0], &network)) {
            matches = wrapped_matcher->matcher->Has(network);
          }
        }
      }
    }
  } catch (...) {
    matches = false;
  }

  napi_value result;
  napi_get_boolean(env, matches, &result);
  return result;
}

void BuildIPMatcher(napi_env, void* data) noexcept {
  auto* build_data = static_cast<BuildIPMatcherData*>(data);
  try {
    build_data->matcher = std::make_unique<ip_matcher::IPMatcher>(build_data->networks);
  } catch (...) {
    build_data->matcher.reset();
  }
}

void CompleteIPMatcherBuild(napi_env env, napi_status status, void* data) {
  auto* build_data = static_cast<BuildIPMatcherData*>(data);
  if (status != napi_ok || !build_data->matcher) {
    napi_reject_deferred(env, build_data->deferred, CreateError(env, "Failed to build IP matcher"));
  } else {
    std::unique_ptr<WrappedIPMatcher> wrapped_matcher;
    try {
      wrapped_matcher = std::make_unique<WrappedIPMatcher>();
      wrapped_matcher->matcher = std::move(build_data->matcher);
    } catch (...) {
      napi_reject_deferred(env, build_data->deferred, CreateError(env, "Failed to create IP matcher"));
    }

    if (wrapped_matcher) {
      napi_value matcher;
      napi_value has;
      WrappedIPMatcher* const matcher_data = wrapped_matcher.get();
      napi_status matcher_status = napi_create_object(env, &matcher);

      if (matcher_status == napi_ok) {
        matcher_status = napi_wrap(env, matcher, matcher_data, DeleteIPMatcher, nullptr, nullptr);
        if (matcher_status == napi_ok) {
          wrapped_matcher.release();
        }
      }

      if (matcher_status == napi_ok) {
        const int64_t external_memory = static_cast<int64_t>(
            sizeof(*matcher_data) + matcher_data->matcher->MemorySize());
        int64_t adjusted_external_memory;
        matcher_status = napi_adjust_external_memory(env, external_memory, &adjusted_external_memory);
        if (matcher_status == napi_ok) {
          matcher_data->external_memory = external_memory;
        }
      }

      if (matcher_status == napi_ok) {
        matcher_status = napi_create_function(
            env, "has", NAPI_AUTO_LENGTH, IPMatcherHas, nullptr, &has);
      }

      if (matcher_status == napi_ok) {
        matcher_status = napi_set_named_property(env, matcher, "has", has);
      }

      if (matcher_status == napi_ok) {
        napi_resolve_deferred(env, build_data->deferred, matcher);
      } else {
        napi_reject_deferred(env, build_data->deferred, CreateError(env, "Failed to create IP matcher"));
      }
    }
  }

  napi_delete_async_work(env, build_data->work);
  delete build_data;
}

napi_value CreateIPMatcher(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  bool is_array = false;
  if (argc != 1 || napi_is_array(env, argv[0], &is_array) != napi_ok || !is_array) {
    napi_throw_type_error(env, nullptr, "Expected an array of IP networks");
    return nullptr;
  }

  uint32_t length;
  if (napi_get_array_length(env, argv[0], &length) != napi_ok) {
    napi_throw_error(env, nullptr, "Failed to read IP networks");
    return nullptr;
  }
  if (length > kMaxIPMatcherNetworks) {
    napi_throw_range_error(env, nullptr, "IP matcher supports at most 1000000 networks");
    return nullptr;
  }

  std::unique_ptr<BuildIPMatcherData> build_data;
  try {
    build_data = std::make_unique<BuildIPMatcherData>();
    build_data->networks.reserve(
        length < kInitialIPMatcherCapacity ? length : kInitialIPMatcherCapacity);

    size_t total_input_bytes = 0;
    for (uint32_t index = 0; index < length; index++) {
      const std::string index_name = std::to_string(index);
      napi_value index_value;
      bool has_own_property;
      if (napi_create_string_utf8(env, index_name.c_str(), NAPI_AUTO_LENGTH, &index_value) != napi_ok ||
          napi_has_own_property(env, argv[0], index_value, &has_own_property) != napi_ok) {
        return nullptr;
      }
      if (!has_own_property) {
        napi_throw_type_error(env, nullptr, "Expected IP networks to be a dense array");
        return nullptr;
      }

      napi_value network;
      if (napi_get_element(env, argv[0], index, &network) != napi_ok) {
        return nullptr;
      }

      size_t network_length;
      if (!GetStringLength(env, network, &network_length)) {
        napi_throw_type_error(env, nullptr, "Expected IP networks to be strings");
        return nullptr;
      }
      if (network_length > kMaxIPMatcherInputBytes - total_input_bytes) {
        napi_throw_range_error(env, nullptr, "IP matcher supports at most 64 MiB of network strings");
        return nullptr;
      }
      total_input_bytes += network_length;

      std::string value;
      if (!CopyString(env, network, network_length, &value)) {
        return nullptr;
      }
      build_data->networks.push_back(std::move(value));
    }
  } catch (...) {
    napi_throw_error(env, nullptr, "Failed to prepare IP matcher");
    return nullptr;
  }

  napi_value promise;
  if (napi_create_promise(env, &build_data->deferred, &promise) != napi_ok) {
    napi_throw_error(env, nullptr, "Failed to create IP matcher promise");
    return nullptr;
  }

  napi_value resource_name;
  if (napi_create_string_utf8(env, "createIPMatcher", NAPI_AUTO_LENGTH, &resource_name) != napi_ok) {
    napi_throw_error(env, nullptr, "Failed to create IP matcher resource name");
    return nullptr;
  }
  if (napi_create_async_work(
          env,
          nullptr,
          resource_name,
          BuildIPMatcher,
          CompleteIPMatcherBuild,
          build_data.get(),
          &build_data->work) != napi_ok) {
    napi_throw_error(env, nullptr, "Failed to queue IP matcher build");
    return nullptr;
  }

  if (napi_queue_async_work(env, build_data->work) != napi_ok) {
    napi_delete_async_work(env, build_data->work);
    napi_throw_error(env, nullptr, "Failed to queue IP matcher build");
    return nullptr;
  }
  build_data.release();
  return promise;
}

napi_value Init(napi_env env, napi_value exports) {
  const napi_property_descriptor descriptors[] = {
      {
          .utf8name = "setCodeGenerationCallback",
          .method = SetCodeGenerationCallback,
          .attributes = napi_default,
      },
      {
          .utf8name = "createIPMatcher",
          .method = CreateIPMatcher,
          .attributes = napi_default,
      },
  };
  napi_define_properties(env, exports, 2, descriptors);
  return exports;
}

NAPI_MODULE(codegen_hook, Init)
