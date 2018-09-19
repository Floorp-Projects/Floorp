/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_X86_MEM_SSE2_H_
#define AOM_AOM_DSP_X86_MEM_SSE2_H_

#include <emmintrin.h>  // SSE2

#include "config/aom_config.h"

#include "aom/aom_integer.h"

static INLINE __m128i loadh_epi64(const void *const src, const __m128i s) {
  return _mm_castps_si128(
      _mm_loadh_pi(_mm_castsi128_ps(s), (const __m64 *)src));
}

static INLINE __m128i load_8bit_4x4_to_1_reg_sse2(const void *const src,
                                                  const int byte_stride) {
  return _mm_setr_epi32(*(const int32_t *)((int8_t *)src + 0 * byte_stride),
                        *(const int32_t *)((int8_t *)src + 1 * byte_stride),
                        *(const int32_t *)((int8_t *)src + 2 * byte_stride),
                        *(const int32_t *)((int8_t *)src + 3 * byte_stride));
}

static INLINE __m128i load_8bit_8x2_to_1_reg_sse2(const void *const src,
                                                  const int byte_stride) {
  __m128i dst;
  dst = _mm_loadl_epi64((__m128i *)((int8_t *)src + 0 * byte_stride));
  dst = loadh_epi64((int8_t *)src + 1 * byte_stride, dst);
  return dst;
}

#endif  // AOM_AOM_DSP_X86_MEM_SSE2_H_
