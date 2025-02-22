/* Copyright (C) 2008-2017 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _IMMINTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED

#include <intrinsics/mmintrin.h>

#include <intrinsics/xmmintrin.h>

#include <intrinsics/emmintrin.h>

#include <intrinsics/pmmintrin.h>

#include <intrinsics/tmmintrin.h>

#include <intrinsics/smmintrin.h>

#include <intrinsics/wmmintrin.h>

#include <intrinsics/avxintrin.h>

#include <intrinsics/avx2intrin.h>

// Modification by (an anomynous person).
// Either g++ or clang++ can not handle these AVX512 headers.
// Including them gives compilation errors.
/*
#include <intrinsics/avx512fintrin.h>

#include <intrinsics/avx512erintrin.h>

#include <intrinsics/avx512pfintrin.h>

#include <intrinsics/avx512cdintrin.h>

#include <intrinsics/avx512vlintrin.h>

#include <intrinsics/avx512bwintrin.h>

#include <intrinsics/avx512dqintrin.h>

#include <intrinsics/avx512vlbwintrin.h>

#include <intrinsics/avx512vldqintrin.h>

#include <intrinsics/avx512ifmaintrin.h>

#include <intrinsics/avx512ifmavlintrin.h>

#include <intrinsics/avx512vbmiintrin.h>

#include <intrinsics/avx512vbmivlintrin.h>

#include <intrinsics/avx5124fmapsintrin.h>

#include <intrinsics/avx5124vnniwintrin.h>

#include <intrinsics/avx512vpopcntdqintrin.h>
*/

#include <intrinsics/shaintrin.h>

#include <intrinsics/lzcntintrin.h>

#include <intrinsics/bmiintrin.h>

#include <intrinsics/bmi2intrin.h>

#include <intrinsics/fmaintrin.h>

#include <intrinsics/f16cintrin.h>

#include <intrinsics/rtmintrin.h>

#include <intrinsics/xtestintrin.h>

#ifndef __RDRND__
#pragma GCC push_options
#pragma GCC target("rdrnd")
#define __DISABLE_RDRND__
#endif /* __RDRND__ */
extern __inline int
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_rdrand16_step (unsigned short *__P)
{
  return __builtin_ia32_rdrand16_step (__P);
}

extern __inline int
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_rdrand32_step (unsigned int *__P)
{
  return __builtin_ia32_rdrand32_step (__P);
}
#ifdef __DISABLE_RDRND__
#undef __DISABLE_RDRND__
#pragma GCC pop_options
#endif /* __DISABLE_RDRND__ */

#ifndef __RDPID__
#pragma GCC push_options
#pragma GCC target("rdpid")
#define __DISABLE_RDPID__
#endif /* __RDPID__ */
extern __inline unsigned int
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_rdpid_u32 (void)
{
  return __builtin_ia32_rdpid ();
}
#ifdef __DISABLE_RDPID__
#undef __DISABLE_RDPID__
#pragma GCC pop_options
#endif /* __DISABLE_RDPID__ */

#ifdef  __x86_64__

#ifndef __FSGSBASE__
#pragma GCC push_options
#pragma GCC target("fsgsbase")
#define __DISABLE_FSGSBASE__
#endif /* __FSGSBASE__ */
extern __inline unsigned int
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_readfsbase_u32 (void)
{
  return __builtin_ia32_rdfsbase32 ();
}

extern __inline unsigned long long
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_readfsbase_u64 (void)
{
  return __builtin_ia32_rdfsbase64 ();
}

extern __inline unsigned int
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_readgsbase_u32 (void)
{
  return __builtin_ia32_rdgsbase32 ();
}

extern __inline unsigned long long
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_readgsbase_u64 (void)
{
  return __builtin_ia32_rdgsbase64 ();
}

extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_writefsbase_u32 (unsigned int __B)
{
  __builtin_ia32_wrfsbase32 (__B);
}

extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_writefsbase_u64 (unsigned long long __B)
{
  __builtin_ia32_wrfsbase64 (__B);
}

extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_writegsbase_u32 (unsigned int __B)
{
  __builtin_ia32_wrgsbase32 (__B);
}

extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_writegsbase_u64 (unsigned long long __B)
{
  __builtin_ia32_wrgsbase64 (__B);
}
#ifdef __DISABLE_FSGSBASE__
#undef __DISABLE_FSGSBASE__
#pragma GCC pop_options
#endif /* __DISABLE_FSGSBASE__ */

#ifndef __RDRND__
#pragma GCC push_options
#pragma GCC target("rdrnd")
#define __DISABLE_RDRND__
#endif /* __RDRND__ */
extern __inline int
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_rdrand64_step (unsigned long long *__P)
{
  return __builtin_ia32_rdrand64_step (__P);
}
#ifdef __DISABLE_RDRND__
#undef __DISABLE_RDRND__
#pragma GCC pop_options
#endif /* __DISABLE_RDRND__ */

#endif /* __x86_64__  */

#endif /* _IMMINTRIN_H_INCLUDED */
