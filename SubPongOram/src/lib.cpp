extern "C" {
#include <openenclave/enclave.h>
#include "api/rworam_t.h"
}
#include <memory>
#include <cstring>
#include <cmath>
#include <cassert>
#include <cstdio>
#include "pkt.h"
#include "oht/lib.h"
#include "rworam/lib.h"
#include "crypto.h"
#ifdef RWORAM_ENCLAVE_EXPERIMENT
#include "experiment/lib.h"
#endif

using namespace rworam;
using namespace oht;
using namespace boomerang;

constexpr unsigned kJobs = 8;

template <size_t Ksize, size_t Vsize>
Elem<Ksize, Vsize> pkt_to_elem(const Pkt *pkt) {
  Elem<Ksize, Vsize> elem{};
  elem.tag = oht::obl::OblChoose(pkt->head.is_dummy, kTagDummy, uint32_t(0));
  std::memcpy(elem.key, &pkt->head.signal, Ksize);
  std::memcpy(elem.val, pkt->content, Vsize);
  return elem;
}

static std::vector<uint8_t> kSymmKey(16, 'a');
static std::vector<uint8_t> kSymmIv(12, 0);

template <size_t Ksize, size_t Vsize>
Elem<Ksize, Vsize> pkt_buf_to_elem(uint8_t *pkt_start) {
  assert(aes128_dec(pkt_start, kPktSize - kMacSize, nullptr, kSymmKey.data(), kSymmIv.data()) == 0);
  return pkt_to_elem<Ksize, Vsize>(reinterpret_cast<const Pkt *>(pkt_start));
}

template <size_t Ksize, size_t Vsize>
Elem<Ksize, Vsize> pkt_const_buf_to_elem(const uint8_t *pkt_start, std::vector<uint8_t> &buf) {
  std::memcpy(buf.data(), pkt_start, kPktSize);
  return pkt_buf_to_elem<Ksize, Vsize>(buf.data());
}

template <size_t Vsize>
void val_elem_to_pkt(uint8_t *pkt_start, const uint8_t *val) {
  auto pkt = reinterpret_cast<Pkt *>(pkt_start);
  std::memcpy(pkt->content, val, Vsize);
  assert(aes128_enc(pkt_start, kPktSize - kMacSize, pkt->mac, kSymmKey.data(), kSymmIv.data()) == 0);
}

static size_t kBatchNum;
static std::shared_ptr<Rworam<kKsize, kVsize>> Oram;

static std::vector<uint8_t> kPrfKey{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

void rworam_write(const uint8_t *buf, size_t buf_size) {
#ifdef RWORAM_ENCLAVE_LOG
  printf("write: start\n");
#endif

  std::vector<Elem<kKsize, kVsize>> elems{};
  elems.reserve(kBatchNum);
  std::vector<uint8_t> pkt_dec_buf{};
  pkt_dec_buf.reserve(kPktSize);
  for (auto pkt_start = buf; pkt_start + kPktSize <= buf + buf_size; pkt_start += kPktSize) {
    elems.push_back(pkt_const_buf_to_elem<kKsize, kVsize>(pkt_start, pkt_dec_buf));
  }

#ifdef RWORAM_ENCLAVE_EXPERIMENT
  {
    const auto timestamp = experiment::unix_timestamp();
    for (auto &elem : elems) {
      experiment::put_u64(elem.val + experiment::kTimestampStartOffset, timestamp);
    }
    printf("write: before oram write, timestamp: %lu\n", timestamp);
  }
#endif

  Oram->write(std::move(elems), kPrfKey, kJobs);
  // {
  //   std::lock_guard lock(pos::m);
  //   pos::state.fn = pos::ThreadFn::exit;
  // }
  // pos::cv.notify_all();

#ifdef RWORAM_ENCLAVE_EXPERIMENT
  {
    const auto timestamp = experiment::unix_timestamp();
    for (auto &elem : elems) {
      experiment::put_u64(elem.val + experiment::kTimestampStartOffset, timestamp);
    }
    printf("write: done, timestamp: %lu\n", timestamp);
  }
#endif
}

void rworam_read(uint8_t *buf, size_t buf_size) {
#ifdef RWORAM_ENCLAVE_LOG
  printf("read: start\n");
#endif

  std::vector<Elem<kKsize, kVsize>> elems{};
  elems.reserve(kBatchNum);
  for (auto pkt_start = buf; pkt_start + kPktSize <= buf + buf_size; pkt_start += kPktSize) {
    elems.push_back(pkt_buf_to_elem<kKsize, kVsize>(pkt_start));
  }

#ifdef RWORAM_ENCLAVE_EXPERIMENT
  {
    const auto timestamp = experiment::unix_timestamp();
    printf("read: before oram lookup, timestamp: %lu\n", timestamp);
  }
#endif

  for (auto &elem : elems) {
    Oram->lookup(elem);
  }

#ifdef RWORAM_ENCLAVE_EXPERIMENT
  {
    const auto timestamp = experiment::unix_timestamp();
    printf("read: done, timestamp: %lu\n", timestamp);
  }
#endif

  for (auto i = 0; i < elems.size(); i++) {
    auto pkt_start = buf + kPktSize * i;
    val_elem_to_pkt<kVsize>(pkt_start, elems[i].val);
  }
}

constexpr double kLambda = 128;
constexpr double kEpsilon = 0.75;
static double kLogLambdaPowEpsilon = std::pow(std::log(kLambda), kEpsilon);

void rworam_init(size_t layer_num, size_t batch_num) {
  std::vector<uint32_t> kBs{};
  std::vector<uint32_t> kZs{};
  const auto kZ = uint32_t(std::ceil(5.0 * kLogLambdaPowEpsilon));
  for (auto i = 0; i < layer_num; i++) {
    kZs.push_back(kZ);
    const double n = double(batch_num) * std::pow(2.0, double(i));
    const auto b = uint32_t(std::ceil(n / kLogLambdaPowEpsilon));
    kBs.push_back(b);
  }
  Oram = std::make_shared<Rworam<kKsize, kVsize>>(layer_num, batch_num, kBs, kZs);
  kBatchNum = batch_num;
  assert(kPktSize == 288);
}

void rworam_clear() {
  Oram->clear();
}

// No sure if a seperate reset is still required,
// since it is added to solve a thread sync problem but we finally found out it is related to another misc problem.
// Maybe just reset the version in the main thread can work,
// but we do use the function in our experiment.
void rworam_sync_reset() {
  OblSortResetVersion();
  // {
  //   std::lock_guard lock(pos::m);
  //   pos::state.fn = pos::ThreadFn::cmp;
  // }
}
