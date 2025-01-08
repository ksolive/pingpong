#pragma once

#include <cstdint>
#include <cstddef>

namespace boomerang {

constexpr size_t kMacSize = 16;
constexpr size_t kContentSize = 256;

struct PktHead {
  uint64_t signal;
  bool ___read_write;
  bool is_dummy;
  bool ___is_exceed;
  uint8_t ___enode_id;
};

struct Pkt {
  PktHead head;
  uint8_t content[kContentSize]{};
  uint8_t mac[kMacSize]{};
};

constexpr size_t kPktSize = sizeof(Pkt);

}  // namespace boomerang

constexpr size_t kKsize = sizeof(uint64_t);
constexpr size_t kVsize = boomerang::kContentSize;
