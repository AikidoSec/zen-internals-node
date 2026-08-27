// Ported from firewall-node/library/helpers/ip-matcher.
// Based on https://github.com/demskie/netparser (MIT Copyright 2019 alex).

#include "ip_matcher.h"

#include <algorithm>
#include <limits>

namespace ip_matcher {
namespace {

constexpr int kBefore = -1;
constexpr int kEquals = 0;
constexpr int kAfter = 1;

struct ParsedNetwork {
  std::array<uint8_t, 16> bytes{};
  int byte_length;
  int cidr;
};

bool IsWhitespace(char character) {
  return character == ' ' ||
      character == '\t' ||
      character == '\n' ||
      character == '\r' ||
      character == '\f' ||
      character == '\v';
}

std::string_view Trim(std::string_view value) {
  size_t start = 0;
  while (start < value.size() && IsWhitespace(value[start])) {
    start++;
  }

  size_t end = value.size();
  while (end > start && IsWhitespace(value[end - 1])) {
    end--;
  }

  return value.substr(start, end - start);
}

std::vector<std::string_view> Split(std::string_view value, char delimiter, size_t max_parts) {
  std::vector<std::string_view> parts;
  size_t start = 0;
  for (size_t index = 0; index <= value.size(); index++) {
    if (index == value.size() || value[index] == delimiter) {
      if (parts.size() == max_parts) {
        return parts;
      }
      parts.push_back(value.substr(start, index - start));
      start = index + 1;
    }
  }
  return parts;
}

std::optional<int64_t> ParseInt10(std::string_view value) {
  size_t index = 0;
  while (index < value.size() && IsWhitespace(value[index])) {
    index++;
  }

  bool negative = false;
  if (index < value.size() && (value[index] == '+' || value[index] == '-')) {
    negative = value[index] == '-';
    index++;
  }

  const size_t start = index;
  uint64_t number = 0;
  while (index < value.size() && value[index] >= '0' && value[index] <= '9') {
    const uint64_t digit = static_cast<uint64_t>(value[index] - '0');
    if (number > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
      return std::nullopt;
    }
    number = number * 10 + digit;
    index++;
  }

  if (index == start || number > static_cast<uint64_t>(INT64_MAX)) {
    return std::nullopt;
  }

  const int64_t result = static_cast<int64_t>(number);
  return negative ? -result : result;
}

std::optional<int> ParseIntRange(std::string_view value, int min, int max) {
  uint64_t number = 0;
  size_t index = 0;
  while (index < value.size() && value[index] >= '0' && value[index] <= '9') {
    const uint64_t digit = static_cast<uint64_t>(value[index] - '0');
    if (number > static_cast<uint64_t>(max)) {
      return std::nullopt;
    }
    number = number * 10 + digit;
    index++;
  }

  if (index == 0 || number < static_cast<uint64_t>(min) || number > static_cast<uint64_t>(max)) {
    return std::nullopt;
  }

  return static_cast<int>(number);
}

std::optional<bool> LooksLikeIPv4(std::string_view value) {
  for (const char character : value) {
    if (character == '.') {
      return true;
    }
    if (character == ':') {
      return false;
    }
  }
  return std::nullopt;
}

bool ParseIPv4(std::string_view value, std::array<uint8_t, 16>* bytes) {
  const auto parts = Split(value, '.', 5);
  if (parts.size() != 4) {
    return false;
  }

  for (size_t index = 0; index < parts.size(); index++) {
    const auto parsed = ParseInt10(parts[index]);
    if (!parsed || *parsed < 0 || *parsed > 255) {
      return false;
    }
    (*bytes)[index] = static_cast<uint8_t>(*parsed);
  }

  return true;
}

int ParseHexDigit(char character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

std::optional<uint16_t> ParseHextet(std::string_view value) {
  const std::string_view trimmed = Trim(value);
  if (trimmed.empty() || trimmed.size() > 4) {
    return std::nullopt;
  }

  uint16_t parsed = 0;
  for (const char character : value) {
    const int digit = ParseHexDigit(character);
    if (digit < 0) {
      return std::nullopt;
    }
    parsed = static_cast<uint16_t>(parsed * 16 + digit);
  }
  return parsed;
}

std::string_view RemoveBrackets(std::string_view value) {
  if (value.empty() || value.front() != '[') {
    return value;
  }

  const size_t closing_bracket = value.rfind(']');
  if (closing_bracket == std::string_view::npos) {
    return value;
  }
  return value.substr(1, closing_bracket - 1);
}

std::string_view RemovePortInfo(std::string_view value) {
  for (size_t index = 0; index < value.size(); index++) {
    if (value[index] == '#' || value[index] == 'p' || value[index] == '.') {
      return Trim(value.substr(0, index));
    }
  }
  return value;
}

bool ParseIPv4Part(std::string_view value, std::array<uint8_t, 16>* bytes, int* byte_index, bool reverse) {
  const auto parts = Split(value, '.', 5);
  if (parts.size() != 4) {
    return false;
  }

  for (size_t offset = 0; offset < parts.size(); offset++) {
    const size_t index = reverse ? parts.size() - offset - 1 : offset;
    const auto parsed = ParseInt10(parts[index]);
    if (!parsed || *parsed < 0 || *parsed > 255 || *byte_index < 0 || *byte_index >= 16) {
      return false;
    }
    (*bytes)[*byte_index] = static_cast<uint8_t>(*parsed);
    *byte_index += reverse ? -1 : 1;
  }

  return true;
}

bool ParseIPv6LeftHalf(std::array<uint8_t, 16>* bytes, std::string_view value, int* byte_index) {
  if (value.empty()) {
    return true;
  }

  for (const std::string_view part : Split(value, ':', 9)) {
    if (*byte_index >= 16) {
      return false;
    }

    if (Split(part, '.', 5).size() == 4) {
      if (!ParseIPv4Part(part, bytes, byte_index, false)) {
        return false;
      }
      continue;
    }

    const auto parsed = ParseHextet(part);
    if (!parsed || *byte_index + 1 >= 16) {
      return false;
    }
    (*bytes)[(*byte_index)++] = static_cast<uint8_t>(*parsed / 256);
    (*bytes)[(*byte_index)++] = static_cast<uint8_t>(*parsed % 256);
  }

  return true;
}

bool ParseIPv6RightHalf(std::array<uint8_t, 16>* bytes, std::string_view value, int left_byte_index) {
  if (value.empty()) {
    return true;
  }

  int right_byte_index = 15;
  const auto parts = Split(value, ':', 9);
  for (size_t offset = 0; offset < parts.size(); offset++) {
    const size_t index = parts.size() - offset - 1;
    std::string_view part = parts[index];
    if (Trim(part).empty() || left_byte_index > right_byte_index) {
      return false;
    }

    if (Split(part, '.', 5).size() == 4) {
      if (!ParseIPv4Part(part, bytes, &right_byte_index, true)) {
        return false;
      }
      continue;
    }

    if (index == parts.size() - 1) {
      part = RemovePortInfo(part);
    }

    const auto parsed = ParseHextet(part);
    if (!parsed || right_byte_index - 1 < 0) {
      return false;
    }
    (*bytes)[right_byte_index--] = static_cast<uint8_t>(*parsed % 256);
    (*bytes)[right_byte_index--] = static_cast<uint8_t>(*parsed / 256);
  }

  return true;
}

bool ParseIPv6(std::string_view value, std::array<uint8_t, 16>* bytes) {
  value = RemoveBrackets(value);
  if (value.empty()) {
    return false;
  }
  if (value == "::") {
    return true;
  }

  const size_t split_index = value.find("::");
  if (split_index != std::string_view::npos) {
    const size_t second_split_index = value.find("::", split_index + 2);
    if (second_split_index != std::string_view::npos) {
      return false;
    }
  }

  const size_t left_length = split_index == std::string_view::npos ? value.size() : split_index;
  const std::string_view left = value.substr(0, left_length);
  int left_byte_index = 0;
  if (!ParseIPv6LeftHalf(bytes, left, &left_byte_index)) {
    return false;
  }

  if (split_index == std::string_view::npos) {
    return true;
  }

  return ParseIPv6RightHalf(bytes, value.substr(split_index + 2), left_byte_index);
}

namespace parse {

std::optional<ParsedNetwork> Network(std::string_view value) {
  value = Trim(value);
  const auto parts = Split(value, '/', 3);
  if (parts.empty() || parts.size() > 2) {
    return std::nullopt;
  }

  const auto is_ipv4 = LooksLikeIPv4(value);
  if (!is_ipv4) {
    return std::nullopt;
  }

  ParsedNetwork network{};
  network.byte_length = *is_ipv4 ? 4 : 16;
  network.cidr = *is_ipv4 ? 32 : 128;
  if (parts.size() == 2) {
    const auto cidr = ParseIntRange(parts[1], 0, network.cidr);
    if (!cidr) {
      return std::nullopt;
    }
    network.cidr = *cidr;
  }

  const bool parsed = *is_ipv4 ? ParseIPv4(parts[0], &network.bytes)
                                : ParseIPv6(parts[0], &network.bytes);
  if (!parsed) {
    return std::nullopt;
  }

  return network;
}

}

void IncreaseSizeByOneBit(Network* network) {
  network->SetCIDR(network->cidr() - 1);
  network->addr.ApplySubnetMask(network->cidr());
}

}

bool Address::IsValid() const {
  return byte_length_ > 0;
}

int Address::byte_length() const {
  return byte_length_;
}

const std::array<uint8_t, 16>& Address::bytes() const {
  return bytes_;
}

void Address::SetBytes(const std::array<uint8_t, 16>& bytes, int byte_length) {
  if (byte_length != 4 && byte_length != 16) {
    Destroy();
    return;
  }
  bytes_ = bytes;
  byte_length_ = byte_length;
}

void Address::Destroy() {
  bytes_.fill(0);
  byte_length_ = 0;
}

Address Address::Duplicate() const {
  return *this;
}

int Address::Compare(const Address& address) const {
  if (!IsValid() || !address.IsValid()) {
    return kEquals;
  }
  if (byte_length_ < address.byte_length_) {
    return kBefore;
  }
  if (byte_length_ > address.byte_length_) {
    return kAfter;
  }
  for (int index = 0; index < byte_length_; index++) {
    if (bytes_[index] < address.bytes_[index]) {
      return kBefore;
    }
    if (bytes_[index] > address.bytes_[index]) {
      return kAfter;
    }
  }
  return kEquals;
}

bool Address::Equals(const Address& address) const {
  return Compare(address) == kEquals;
}

void Address::ApplySubnetMask(int cidr) {
  if (!IsValid()) {
    return;
  }

  int mask_bits = byte_length_ * 8 - cidr;
  for (int index = byte_length_ - 1; index >= 0; index--) {
    const int bits = std::max(0, std::min(mask_bits, 8));
    if (bits == 0) {
      return;
    }
    bytes_[index] &= static_cast<uint8_t>(~((1 << bits) - 1));
    mask_bits -= 8;
  }
}

bool Address::IsBaseAddress(int cidr) const {
  if (!IsValid() || cidr < 0 || cidr > byte_length_ * 8) {
    return false;
  }
  if (cidr == byte_length_ * 8) {
    return true;
  }

  int mask_bits = byte_length_ * 8 - cidr;
  for (int index = byte_length_ - 1; index >= 0; index--) {
    const int bits = std::max(0, std::min(mask_bits, 8));
    if (bits == 0) {
      return true;
    }
    if ((bytes_[index] & ((1 << bits) - 1)) != 0) {
      return false;
    }
    mask_bits -= 8;
  }
  return true;
}

void Address::Increase(int cidr) {
  if (!IsValid()) {
    Destroy();
    return;
  }
  OffsetAddress(cidr, true);
}

void Address::OffsetAddress(int cidr, bool forwards) {
  if (!IsValid() || cidr <= 0) {
    Destroy();
    return;
  }

  const int target_byte = (cidr - 1) / 8;
  if (target_byte < 0 || target_byte >= byte_length_) {
    Destroy();
    return;
  }

  const int increment = 1 << (8 - (cidr - target_byte * 8));
  int value = bytes_[target_byte] + (forwards ? increment : -increment);
  bytes_[target_byte] = static_cast<uint8_t>(value);
  if (value < 0 || value > 255) {
    OffsetAddress(target_byte * 8, forwards);
  }
}

Network::Network(std::string_view network) {
  const auto parsed = parse::Network(network);
  if (!parsed) {
    return;
  }
  addr.SetBytes(parsed->bytes, parsed->byte_length);
  netbits_ = parsed->cidr;
}

bool Network::IsValid() const {
  return addr.IsValid() && netbits_ != -1;
}

int Network::cidr() const {
  return IsValid() ? netbits_ : -1;
}

void Network::SetCIDR(int cidr) {
  if (!addr.IsValid() || cidr < 0 || cidr > addr.byte_length() * 8) {
    addr.Destroy();
    netbits_ = -1;
    return;
  }
  netbits_ = cidr;
}

Network Network::Duplicate() const {
  return *this;
}

void Network::Next() {
  addr.Increase(netbits_);
}

int Network::Compare(const Network& network) const {
  if (!IsValid() || !network.IsValid()) {
    return kEquals;
  }

  const int address_compare = addr.Compare(network.addr);
  if (address_compare != kEquals) {
    return address_compare;
  }
  if (netbits_ < network.netbits_) {
    return kBefore;
  }
  if (netbits_ > network.netbits_) {
    return kAfter;
  }
  return kEquals;
}

bool Network::Contains(const Network& network) const {
  if (!IsValid() || !network.IsValid() || addr.byte_length() != network.addr.byte_length()) {
    return false;
  }
  if (netbits_ == 0) {
    return true;
  }
  if (network.netbits_ == 0 || addr.Compare(network.addr) == kAfter) {
    return false;
  }

  Network next = Duplicate();
  next.Next();
  Network other_next = network.Duplicate();
  other_next.Next();

  if (!next.IsValid()) {
    return true;
  }
  if (!other_next.IsValid()) {
    return false;
  }
  if (next.addr.Compare(other_next.addr) == kBefore) {
    return false;
  }
  return true;
}

bool Network::Adjacent(const Network& network) const {
  if (!IsValid() || !network.IsValid() || addr.byte_length() != network.addr.byte_length()) {
    return false;
  }
  if (netbits_ == 0 || network.netbits_ == 0) {
    return true;
  }

  const int compare = addr.Compare(network.addr);
  if (compare == kEquals) {
    return false;
  }

  Network alpha;
  Network bravo;
  if (compare == kBefore) {
    alpha = Duplicate();
    alpha.Next();
    bravo = network;
  } else {
    alpha = network.Duplicate();
    alpha.Next();
    bravo = *this;
  }

  return alpha.IsValid() && alpha.addr.Compare(bravo.addr) == kEquals;
}

std::optional<Network> ParseBaseNetwork(std::string_view network, bool strict) {
  Network parsed(network);
  if (!parsed.IsValid()) {
    return std::nullopt;
  }

  if (!strict) {
    parsed.addr.ApplySubnetMask(parsed.cidr());
    return parsed;
  }

  const Address original = parsed.addr.Duplicate();
  parsed.addr.ApplySubnetMask(parsed.cidr());
  if (!parsed.addr.Equals(original)) {
    return std::nullopt;
  }
  return parsed;
}

void SortNetworks(std::vector<Network>* networks) {
  std::sort(networks->begin(), networks->end(), [](const Network& a, const Network& b) {
    return a.Compare(b) == kBefore;
  });
}

std::vector<Network> SummarizeSortedNetworks(const std::vector<Network>& sorted_networks) {
  if (sorted_networks.empty()) {
    return {};
  }

  std::vector<Network> summarized{sorted_networks.front()};
  for (size_t index = 1; index < sorted_networks.size(); index++) {
    if (summarized.back().Contains(sorted_networks[index])) {
      continue;
    }

    summarized.push_back(sorted_networks[index]);
    while (summarized.size() >= 2) {
      Network& a = summarized[summarized.size() - 2];
      const Network& b = summarized.back();
      if (a.cidr() != b.cidr() || !a.addr.IsBaseAddress(a.cidr() - 1) || !a.Adjacent(b)) {
        break;
      }
      IncreaseSizeByOneBit(&a);
      summarized.pop_back();
    }
  }
  return summarized;
}

size_t BinarySearchForInsertionIndex(const Network& network, const std::vector<Network>& sorted_networks) {
  if (sorted_networks.empty()) {
    return 0;
  }

  size_t left = 0;
  size_t right = sorted_networks.size() - 1;
  while (left < right) {
    const size_t middle = left + (right - left) / 2;
    switch (sorted_networks[middle].Compare(network)) {
      case kEquals:
        return middle + 1;
      case kBefore:
        left = middle + 1;
        break;
      case kAfter:
        right = middle == 0 ? 0 : middle - 1;
        break;
    }
  }

  return sorted_networks[left].Compare(network) == kBefore ? left + 1 : left;
}

IPMatcher::IPMatcher(const std::vector<std::string>& networks) {
  std::vector<Network> subnets;
  subnets.reserve(networks.size());
  for (const std::string& network : networks) {
    const auto parsed = ParseBaseNetwork(network, false);
    if (parsed && parsed->IsValid()) {
      subnets.push_back(*parsed);
    }
  }
  SortNetworks(&subnets);
  sorted_ = SummarizeSortedNetworks(subnets);
}

size_t IPMatcher::MemorySize() const {
  return sizeof(*this) + sorted_.capacity() * sizeof(Network);
}

bool IPMatcher::Has(std::string_view network) const {
  const auto parsed = ParseBaseNetwork(network, false);
  if (!parsed || !parsed->IsValid()) {
    return false;
  }

  const size_t index = BinarySearchForInsertionIndex(*parsed, sorted_);
  if (index < sorted_.size() && sorted_[index].Contains(*parsed)) {
    return true;
  }
  return index > 0 && sorted_[index - 1].Contains(*parsed);
}

}
