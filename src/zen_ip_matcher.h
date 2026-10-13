// Must match ip_matcher/src/ffi.rs in zen-internals. A panic in the Rust code aborts the process.

#ifndef ZEN_INTERNALS_NODE_ZEN_IP_MATCHER_H_
#define ZEN_INTERNALS_NODE_ZEN_IP_MATCHER_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IpMatcherHandle IpMatcherHandle;

typedef struct IpMatcherByteSlice {
  const uint8_t* ptr;
  size_t len;
} IpMatcherByteSlice;

#define IP_MATCHER_NO_MATCH 0
#define IP_MATCHER_MATCH 1
#define IP_MATCHER_ERROR 2

// Returns NULL for invalid input, more than 1,000,000 networks or more than 64 MiB of input.
IpMatcherHandle* ip_matcher_create(const IpMatcherByteSlice* networks, size_t network_count);

// Returns IP_MATCHER_ERROR for lookups over 128 bytes or invalid UTF-8.
int ip_matcher_has(const IpMatcherHandle* handle, const uint8_t* network, size_t network_len);

size_t ip_matcher_memory_size(const IpMatcherHandle* handle);

void ip_matcher_free(IpMatcherHandle* handle);

#ifdef __cplusplus
}
#endif

#endif
