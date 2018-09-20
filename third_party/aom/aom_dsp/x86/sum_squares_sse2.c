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

#include <assert.h>
#include <emmintrin.h>
#include <stdio.h>

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/sum_squares_sse2.h"
#include "config/aom_dsp_rtcd.h"

static INLINE __m128i xx_loadh_64(__m128i a, const void *b) {
  const __m128d ad = _mm_castsi128_pd(a);
  return _mm_castpd_si128(_mm_loadh_pd(ad, (double *)b));
}

static INLINE uint64_t xx_cvtsi128_si64(__m128i a) {
#if ARCH_X86_64
  return (uint64_t)_mm_cvtsi128_si64(a);
#else
  {
    uint64_t tmp;
    _mm_storel_epi64((__m128i *)&tmp, a);
    return tmp;
  }
#endif
}

static INLINE __m128i sum_squares_i16_4x4_sse2(const int16_t *src, int stride) {
  const __m128i v_val_0_w = xx_loadl_64(src + 0 * stride);
  const __m128i v_val_2_w = xx_loadl_64(src + 2 * stride);
  const __m128i v_val_01_w = xx_loadh_64(v_val_0_w, src + 1 * stride);
  const __m128i v_val_23_w = xx_loadh_64(v_val_2_w, src + 3 * stride);
  const __m128i v_sq_01_d = _mm_madd_epi16(v_val_01_w, v_val_01_w);
  const __m128i v_sq_23_d = _mm_madd_epi16(v_val_23_w, v_val_23_w);

  return _mm_add_epi32(v_sq_01_d, v_sq_23_d);
}

uint64_t aom_sum_squares_2d_i16_4x4_sse2(const int16_t *src, int stride) {
  const __m128i v_sum_0123_d = sum_squares_i16_4x4_sse2(src, stride);
  __m128i v_sum_d =
      _mm_add_epi32(v_sum_0123_d, _mm_srli_epi64(v_sum_0123_d, 32));
  v_sum_d = _mm_add_epi32(v_sum_d, _mm_srli_si128(v_sum_d, 8));
  return (uint64_t)_mm_cvtsi128_si32(v_sum_d);
}

uint64_t aom_sum_squares_2d_i16_4xn_sse2(const int16_t *src, int stride,
                                         int height) {
  int r = 0;
  __m128i v_acc_q = _mm_setzero_si128();
  do {
    const __m128i v_acc_d = sum_squares_i16_4x4_sse2(src, stride);
    v_acc_q = _mm_add_epi32(v_acc_q, v_acc_d);
    src += stride << 2;
    r += 4;
  } while (r < height);
  const __m128i v_zext_mask_q = xx_set1_64_from_32i(0xffffffff);
  __m128i v_acc_64 = _mm_add_epi64(_mm_srli_epi64(v_acc_q, 32),
                                   _mm_and_si128(v_acc_q, v_zext_mask_q));
  v_acc_64 = _mm_add_epi64(v_acc_64, _mm_srli_si128(v_acc_64, 8));
  return xx_cvtsi128_si64(v_acc_64);
}

#ifdef __GNUC__
// This prevents GCC/Clang from inlining this function into
// aom_sum_squares_2d_i16_sse2, which in turn saves some stack
// maintenance instructions in the common case of 4x4.
__attribute__((noinline))
#endif
uint64_t
aom_sum_squares_2d_i16_nxn_sse2(const int16_t *src, int stride, int width,
                                int height) {
  int r = 0;

  const __m128i v_zext_mask_q = xx_set1_64_from_32i(0xffffffff);
  __m128i v_acc_q = _mm_setzero_si128();

  do {
    __m128i v_acc_d = _mm_setzero_si128();
    int c = 0;
    do {
      const int16_t *b = src + c;

      const __m128i v_val_0_w = xx_load_128(b + 0 * stride);
      const __m128i v_val_1_w = xx_load_128(b + 1 * stride);
      const __m128i v_val_2_w = xx_load_128(b + 2 * stride);
      const __m128i v_val_3_w = xx_load_128(b + 3 * stride);

      const __m128i v_sq_0_d = _mm_madd_epi16(v_val_0_w, v_val_0_w);
      const __m128i v_sq_1_d = _mm_madd_epi16(v_val_1_w, v_val_1_w);
      const __m128i v_sq_2_d = _mm_madd_epi16(v_val_2_w, v_val_2_w);
      const __m128i v_sq_3_d = _mm_madd_epi16(v_val_3_w, v_val_3_w);

      const __m128i v_sum_01_d = _mm_add_epi32(v_sq_0_d, v_sq_1_d);
      const __m128i v_sum_23_d = _mm_add_epi32(v_sq_2_d, v_sq_3_d);

      const __m128i v_sum_0123_d = _mm_add_epi32(v_sum_01_d, v_sum_23_d);

      v_acc_d = _mm_add_epi32(v_acc_d, v_sum_0123_d);
      c += 8;
    } while (c < width);

    v_acc_q = _mm_add_epi64(v_acc_q, _mm_and_si128(v_acc_d, v_zext_mask_q));
    v_acc_q = _mm_add_epi64(v_acc_q, _mm_srli_epi64(v_acc_d, 32));

    src += 4 * stride;
    r += 4;
  } while (r < height);

  v_acc_q = _mm_add_epi64(v_acc_q, _mm_srli_si128(v_acc_q, 8));
  return xx_cvtsi128_si64(v_acc_q);
}

uint64_t aom_sum_squares_2d_i16_sse2(const int16_t *src, int stride, int width,
                                     int height) {
  // 4 elements per row only requires half an XMM register, so this
  // must be a special case, but also note that over 75% of all calls
  // are with size == 4, so it is also the common case.
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_squares_2d_i16_4x4_sse2(src, stride);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    return aom_sum_squares_2d_i16_4xn_sse2(src, stride, height);
  } else if (LIKELY((width & 7) == 0 && (height & 3) == 0)) {
    // Generic case
    return aom_sum_squares_2d_i16_nxn_sse2(src, stride, width, height);
  } else {
    return aom_sum_squares_2d_i16_c(src, stride, width, height);
  }
}

//////////////////////////////////////////////////////////////////////////////
// 1D version
//////////////////////////////////////////////////////////////////////////////

static uint64_t aom_sum_squares_i16_64n_sse2(const int16_t *src, uint32_t n) {
  const __m128i v_zext_mask_q = xx_set1_64_from_32i(0xffffffff);
  __m128i v_acc0_q = _mm_setzero_si128();
  __m128i v_acc1_q = _mm_setzero_si128();

  const int16_t *const end = src + n;

  assert(n % 64 == 0);

  while (src < end) {
    const __m128i v_val_0_w = xx_load_128(src);
    const __m128i v_val_1_w = xx_load_128(src + 8);
    const __m128i v_val_2_w = xx_load_128(src + 16);
    const __m128i v_val_3_w = xx_load_128(src + 24);
    const __m128i v_val_4_w = xx_load_128(src + 32);
    const __m128i v_val_5_w = xx_load_128(src + 40);
    const __m128i v_val_6_w = xx_load_128(src + 48);
    const __m128i v_val_7_w = xx_load_128(src + 56);

    const __m128i v_sq_0_d = _mm_madd_epi16(v_val_0_w, v_val_0_w);
    const __m128i v_sq_1_d = _mm_madd_epi16(v_val_1_w, v_val_1_w);
    const __m128i v_sq_2_d = _mm_madd_epi16(v_val_2_w, v_val_2_w);
    const __m128i v_sq_3_d = _mm_madd_epi16(v_val_3_w, v_val_3_w);
    const __m128i v_sq_4_d = _mm_madd_epi16(v_val_4_w, v_val_4_w);
    const __m128i v_sq_5_d = _mm_madd_epi16(v_val_5_w, v_val_5_w);
    const __m128i v_sq_6_d = _mm_madd_epi16(v_val_6_w, v_val_6_w);
    const __m128i v_sq_7_d = _mm_madd_epi16(v_val_7_w, v_val_7_w);

    const __m128i v_sum_01_d = _mm_add_epi32(v_sq_0_d, v_sq_1_d);
    const __m128i v_sum_23_d = _mm_add_epi32(v_sq_2_d, v_sq_3_d);
    const __m128i v_sum_45_d = _mm_add_epi32(v_sq_4_d, v_sq_5_d);
    const __m128i v_sum_67_d = _mm_add_epi32(v_sq_6_d, v_sq_7_d);

    const __m128i v_sum_0123_d = _mm_add_epi32(v_sum_01_d, v_sum_23_d);
    const __m128i v_sum_4567_d = _mm_add_epi32(v_sum_45_d, v_sum_67_d);

    const __m128i v_sum_d = _mm_add_epi32(v_sum_0123_d, v_sum_4567_d);

    v_acc0_q = _mm_add_epi64(v_acc0_q, _mm_and_si128(v_sum_d, v_zext_mask_q));
    v_acc1_q = _mm_add_epi64(v_acc1_q, _mm_srli_epi64(v_sum_d, 32));

    src += 64;
  }

  v_acc0_q = _mm_add_epi64(v_acc0_q, v_acc1_q);
  v_acc0_q = _mm_add_epi64(v_acc0_q, _mm_srli_si128(v_acc0_q, 8));
  return xx_cvtsi128_si64(v_acc0_q);
}

uint64_t aom_sum_squares_i16_sse2(const int16_t *src, uint32_t n) {
  if (n % 64 == 0) {
    return aom_sum_squares_i16_64n_sse2(src, n);
  } else if (n > 64) {
    int k = n & ~(64 - 1);
    return aom_sum_squares_i16_64n_sse2(src, k) +
           aom_sum_squares_i16_c(src + k, n - k);
  } else {
    return aom_sum_squares_i16_c(src, n);
  }
}
