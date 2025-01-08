extern "C" {
#include <openenclave/enclave.h>
#include "api/rworam_t.h"
}
#include "oht_obl_oblsort_impl.h"
#include "oht/lib.h"
#include "pkt.h"
#ifdef RWORAM_ENCLAVE_LOG
#include <cstdio>
#endif

using namespace oht;

std::mutex gOblSortParamsMutex;
std::condition_variable gOblSortParamsCv;
OblSortParams<void *, void *> gOblSortParams;
#ifdef RWORAM_ENCLAVE_LOG
std::mutex gOblSortLogMutex;
#endif

constexpr size_t kOblSortTimesPerCall = 4;

void rworam_oblsort_worker(size_t tid) {
  for (auto i = 1; i <= kOblSortTimesPerCall; i++) {
    std::vector<Elem<kKsize, kVsize>>::iterator begin, end;
    bool (*cmp)(const Elem<kKsize, kVsize> &a, const Elem<kKsize, kVsize> &b);
    unsigned jobs;

    {
      std::unique_lock lock(gOblSortParamsMutex);
      gOblSortParamsCv.wait(lock, [i] { return gOblSortParams.version == i; });
      begin = *reinterpret_cast<std::vector<Elem<kKsize, kVsize>>::iterator *>(gOblSortParams.begin);
      end = *reinterpret_cast<std::vector<Elem<kKsize, kVsize>>::iterator *>(gOblSortParams.end);
      cmp =
        reinterpret_cast<bool (*)(const Elem<kKsize, kVsize> &a, const Elem<kKsize, kVsize> &b)>(gOblSortParams.cmp);
      jobs = gOblSortParams.jobs;
    }

#ifdef RWORAM_ENCLAVE_LOG
    {
      std::lock_guard lock(gOblSortLogMutex);
      fprintf(stderr, "oblsort worker %zu: %d started\n", tid, i);
    }
#endif

    ObliviousSortParallelNonAdaptive(begin, end, cmp, jobs, int(tid));

#ifdef RWORAM_ENCLAVE_LOG
    {
      std::lock_guard lock(gOblSortLogMutex);
      fprintf(stderr, "oblsort worker %zu: %d done\n", tid, i);
    }
#endif
  }
}
