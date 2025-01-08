#include <vector>
#include <random>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cassert>
#include "src/pkt.h"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

using namespace boomerang;

constexpr uint64_t kPktNum = 10000;
constexpr double kPktDummyNum = 50;
constexpr uint32_t kSeed = 114514;

int main() {
#ifdef _WIN32
  setmode(fileno(stdout), O_BINARY);
  setmode(fileno(stdin), O_BINARY);
#endif

  std::vector<Pkt> pkts{};
  pkts.reserve(kPktNum);
  for (uint64_t i = 0; i < kPktNum; i++) {
    Pkt pkt{};
    pkt.head.signal = i;
    std::memcpy(pkt.content, &i, sizeof(uint64_t));
    if (i < kPktDummyNum) {
      pkt.head.is_dummy = true;
    }
    pkts.push_back(pkt);
  }

  std::mt19937 rng(kSeed);
  std::shuffle(pkts.begin(), pkts.end(), rng);

  const auto n = fwrite(pkts.data(), sizeof(Pkt), pkts.size(), stdout);
  assert(n == kPktNum);
  return 0;
}
