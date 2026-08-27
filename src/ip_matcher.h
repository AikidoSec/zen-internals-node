// Ported from firewall-node/library/helpers/ip-matcher.
// Based on https://github.com/demskie/netparser (MIT Copyright 2019 alex).

#ifndef ZEN_INTERNALS_NODE_IP_MATCHER_H_
#define ZEN_INTERNALS_NODE_IP_MATCHER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ip_matcher {

class Address {
 public:
  Address() = default;

  bool IsValid() const;
  int byte_length() const;
  const std::array<uint8_t, 16>& bytes() const;
  void SetBytes(const std::array<uint8_t, 16>& bytes, int byte_length);
  void Destroy();
  Address Duplicate() const;
  int Compare(const Address& address) const;
  bool Equals(const Address& address) const;
  void ApplySubnetMask(int cidr);
  bool IsBaseAddress(int cidr) const;
  void Increase(int cidr);

 private:
  void OffsetAddress(int cidr, bool forwards);

  std::array<uint8_t, 16> bytes_{};
  int byte_length_ = 0;
};

class Network {
 public:
  Network() = default;
  explicit Network(std::string_view network);

  bool IsValid() const;
  int cidr() const;
  void SetCIDR(int cidr);
  Network Duplicate() const;
  void Next();
  int Compare(const Network& network) const;
  bool Contains(const Network& network) const;
  bool Adjacent(const Network& network) const;

  Address addr;

 private:
  int netbits_ = -1;
};

std::optional<Network> ParseBaseNetwork(std::string_view network, bool strict);
void SortNetworks(std::vector<Network>* networks);
std::vector<Network> SummarizeSortedNetworks(const std::vector<Network>& sorted_networks);
size_t BinarySearchForInsertionIndex(const Network& network, const std::vector<Network>& sorted_networks);

class IPMatcher {
 public:
  explicit IPMatcher(const std::vector<std::string>& networks);

  size_t MemorySize() const;
  bool Has(std::string_view network) const;

 private:
  std::vector<Network> sorted_;
};

}

#endif
