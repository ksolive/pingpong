#pragma once

extern "C" {
#include <openenclave/enclave.h>
}
#include <chrono>

namespace experiment {

static inline uint64_t unix_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto duration = now.time_since_epoch();
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  return static_cast<uint64_t>(milliseconds.count());
}

constexpr size_t kTimestampStartOffset = 8;
constexpr size_t kTimestampDoneOffset = 16;

static inline uint64_t get_u64(const uint8_t *pos) {
  return *reinterpret_cast<const uint64_t *>(pos);
}
static inline void put_u64(uint8_t *pos, uint64_t x) {
  *reinterpret_cast<uint64_t *>(pos) = x;
}

}  // namespace experiment
