// Copyright (C) (an anomynous person)
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "par_obl_primitives.h"
#include "obleq_arr.h"
#include "oblsort_dispatch.h"

namespace oht::obl {

using namespace obl::decl;

template <typename T>
inline bool OblLt(const T &a, const T &b) {
  return ObliviousLess(a, b);
}

template <typename T>
inline bool OblGt(const T &a, const T &b) {
  return ObliviousGreater(a, b);
}

template <typename T>
inline T OblChoose(bool pred, const T &t_val, const T &f_val) {
  return ObliviousChoose(pred, t_val, f_val);
}

template <typename T>
inline bool OblEq(const T &a, const T &b) {
  return ObliviousEqual(a, b);
}

template <typename Iter>
inline void OblCompact(Iter begin, Iter end, uint8_t *tags) {
  return ObliviousCompact(begin, end, tags);
}

}  // namespace oht::obl
