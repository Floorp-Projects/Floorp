/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#ifndef AV1_COMMON_CDEF_SIMD_H_
#define AV1_COMMON_CDEF_SIMD_H_

#include "aom_dsp/aom_simd.h"

// sign(a-b) * min(abs(a-b), max(0, threshold - (abs(a-b) >> adjdamp)))
SIMD_INLINE v128 constrain16(v128 a, v128 b, unsigned int threshold,
                             unsigned int adjdamp) {
  v128 diff = v128_sub_16(a, b);
  const v128 sign = v128_shr_n_s16(diff, 15);
  diff = v128_abs_s16(diff);
  const v128 s =
      v128_ssub_u16(v128_dup_16(threshold), v128_shr_u16(diff, adjdamp));
  return v128_xor(v128_add_16(sign, v128_min_s16(diff, s)), sign);
}

#endif  // AV1_COMMON_CDEF_SIMD_H_
