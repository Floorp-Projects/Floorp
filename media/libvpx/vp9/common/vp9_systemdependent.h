/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_SYSTEMDEPENDENT_H_
#define VP9_COMMON_VP9_SYSTEMDEPENDENT_H_

#ifdef _MSC_VER
# include <math.h>  // the ceil() definition must precede intrin.h
# if _MSC_VER > 1310 && (defined(_M_X64) || defined(_M_IX86))
#  include <intrin.h>
#  define USE_MSC_INTRIN
# endif
# define snprintf _snprintf
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "./vpx_config.h"
#if ARCH_X86 || ARCH_X86_64
void vpx_reset_mmx_state(void);
#define vp9_clear_system_state() vpx_reset_mmx_state()
#else
#define vp9_clear_system_state()
#endif

#if defined(_MSC_VER) && _MSC_VER < 1800
// round is not defined in MSVC before VS2013.
static INLINE int round(double x) {
  if (x < 0)
    return (int)ceil(x - 0.5);
  else
    return (int)floor(x + 0.5);
}
#endif

// use GNU builtins where available.
#if defined(__GNUC__) && \
    ((__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || __GNUC__ >= 4)
static INLINE int get_msb(unsigned int n) {
  return 31 ^ __builtin_clz(n);
}
#elif defined(USE_MSC_INTRIN)
#pragma intrinsic(_BitScanReverse)

static INLINE int get_msb(unsigned int n) {
  unsigned long first_set_bit;
  _BitScanReverse(&first_set_bit, n);
  return first_set_bit;
}
#undef USE_MSC_INTRIN
#else
// Returns (int)floor(log2(n)). n must be > 0.
static INLINE int get_msb(unsigned int n) {
  int log = 0;
  unsigned int value = n;
  int i;

  for (i = 4; i >= 0; --i) {
    const int shift = (1 << i);
    const unsigned int x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  return log;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_SYSTEMDEPENDENT_H_
