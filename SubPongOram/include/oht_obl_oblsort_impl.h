#pragma once

#include <cassert>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include "oht/obl/par_obl_primitives.h"

template <typename Iter, typename Cmp>
struct OblSortParams {
  Iter begin;
  Iter end;
  Cmp cmp;
  unsigned jobs;
  unsigned version = 0;
};
extern std::mutex gOblSortParamsMutex;
extern std::condition_variable gOblSortParamsCv;
extern OblSortParams<void *, void *> gOblSortParams;

namespace oht::obl::decl {

template <typename Iter, typename Cmp>
void OblSort(Iter begin, Iter end, Cmp cmp, int jobs) {
  assert((void("jobs can not be 0"), jobs > 0));
  if (jobs == 1) {
    ObliviousSortParallel(begin, end, cmp, jobs, 0);
  } else {
    {
      std::lock_guard lock(gOblSortParamsMutex);
      gOblSortParams.begin = &begin;
      gOblSortParams.end = &end;
      gOblSortParams.cmp = reinterpret_cast<void *>(cmp);
      gOblSortParams.jobs = unsigned(jobs);
      gOblSortParams.version++;
    }
    gOblSortParamsCv.notify_all();

    ObliviousSortParallelNonAdaptive(begin, end, cmp, jobs, 0);
  }
}

}  // namespace oht::obl::decl

static inline void OblSortResetVersion() {
  {
    std::lock_guard lock(gOblSortParamsMutex);
    gOblSortParams.version = 0;
  }
}
