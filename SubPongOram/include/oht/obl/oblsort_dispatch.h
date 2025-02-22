// Copyright (C) (an anomynous person)
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef OHT_OBL_OBLSORT_STD_MULTITHREAD
#include <thread>
#include <cassert>
#endif
#include "par_obl_primitives.h"

namespace oht::obl::decl {

template <typename Iter, typename Cmp>
void OblSort(Iter begin, Iter end, Cmp cmp, int jobs);

#ifdef OHT_OBL_OBLSORT_SINGLETHREAD
template <typename Iter, typename Cmp>
inline void OblSort(Iter begin, Iter end, Cmp cmp, int jobs) {
  return ObliviousSortParallel(begin, end, cmp, jobs, 0);
}
#endif

#ifdef OHT_OBL_OBLSORT_STD_MULTITHREAD
template <typename Iter, typename Cmp>
inline void OblSort(Iter begin, Iter end, Cmp cmp, int jobs) {
  assert((void("jobs can not be 0"), jobs != 0));
  if (jobs == 1) {
    ObliviousSortParallel(begin, end, cmp, jobs, 0);
  } else {
    std::thread workers[jobs - 1];
    for (size_t i = 1; i < jobs; i++) {
      workers[i - 1] = std::thread(ObliviousSortParallel<Iter, Cmp>, begin, end, cmp, jobs, i);
    }
    ObliviousSortParallel(begin, end, cmp, jobs, 0);
    for (size_t i = 1; i < jobs; i++) {
      workers[i - 1].join();
    }
  }
}
#endif

}  // namespace oht::obl::decl

#ifdef OHT_OBL_OBLSORT_INCLUDEIMPL
#include "oht_obl_oblsort_impl.h"
#endif
