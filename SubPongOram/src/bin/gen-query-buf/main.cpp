#include <vector>
#include <random>
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

int main() {
#ifdef _WIN32
  setmode(fileno(stdout), O_BINARY);
  setmode(fileno(stdin), O_BINARY);
#endif

  std::vector<Pkt> pkts(kPktNum, Pkt{});
  const auto n_in = fread(pkts.data(), sizeof(Pkt), pkts.size(), stdin);
  assert(n_in == kPktNum);

  for (auto &pkt : pkts) {
    if (!pkt.head.is_dummy) {
      std::memset(pkt.content, 0, sizeof(uint64_t));
    }
  }

  const auto n_out = fwrite(pkts.data(), sizeof(Pkt), pkts.size(), stdout);
  assert(n_out == kPktNum);
  return 0;
}
