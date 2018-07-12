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

#include "./aom_dsp_rtcd.h"

static uint64_t aom_sum_squares_2d_i16_4x4_sse2(const int16_t *src,
                                                int stride) {
  const __m128i v_val_0_w =
      _mm_loadl_epi64((const __m128i *)(src + 0 * stride));
  const __m128i v_val_1_w =
      _mm_loadl_epi64((const __m128i *)(src + 1 * stride));
  const __m128i v_val_2_w =
      _mm_loadl_epi64((const __m128i *)(src + 2 * stride));
  const __m128i v_val_3_w =
      _mm_loadl_epi64((const __m128i *)(src + 3 * stride));

  const __m128i v_sq_0_d = _mm_madd_epi16(v_val_0_w, v_val_0_w);
  const __m128i v_sq_1_d = _mm_madd_epi16(v_val_1_w, v_val_1_w);
  const __m128i v_sq_2_d = _mm_madd_epi16(v_val_2_w, v_val_2_w);
  const __m128i v_sq_3_d = _mm_madd_epi16(v_val_3_w, v_val_3_w);

  const __m128i v_sum_01_d = _mm_add_epi32(v_sq_0_d, v_sq_1_d);
  const __m128i v_sum_23_d = _mm_add_epi32(v_sq_2_d, v_sq_3_d);
  const __m128i v_sum_0123_d = _mm_add_epi32(v_sum_01_d, v_sum_23_d);

  const __m128i v_sum_d =
      _mm_add_epi32(v_sum_0123_d, _mm_srli_epi64(v_sum_0123_d, 32));

  return (uint64_t)_mm_cvtsi128_si32(v_sum_d);
}

#ifdef __GNUC__
// This prevents GCC/Clang from inlining this function into
// aom_sum_squares_2d_i16_sse2, which in turn saves some stack
// maintenance instructions in the common case of 4x4.
__attribute__((noinline))
#endif
static uint64_t
aom_sum_squares_2d_i16_nxn_sse2(const int16_t *src, int stride, int width,
                                int height) {
  int r, c;

  const __m128i v_zext_mask_q = _mm_set_epi32(0, 0xffffffff, 0, 0xffffffff);
  __m128i v_acc_q = _mm_setzero_si128();

  for (r = 0; r < height; r += 8) {
    __m128i v_acc_d = _mm_setzero_si128();

    for (c = 0; c < width; c += 8) {
      const int16_t *b = src + c;

      const __m128i v_val_0_w =
          _mm_load_si128((const __m128i *)(b + 0 * stride));
      const __m128i v_val_1_w =
          _mm_load_si128((const __m128i *)(b + 1 * stride));
      const __m128i v_val_2_w =
          _mm_load_si128((const __m128i *)(b + 2 * stride));
      const __m128i v_val_3_w =
          _mm_load_si128((const __m128i *)(b + 3 * stride));
      const __m128i v_val_4_w =
          _mm_load_si128((const __m128i *)(b + 4 * stride));
      const __m128i v_val_5_w =
          _mm_load_si128((const __m128i *)(b + 5 * stride));
      const __m128i v_val_6_w =
          _mm_load_si128((const __m128i *)(b + 6 * stride));
      const __m128i v_val_7_w =
          _mm_load_si128((const __m128i *)(b + 7 * stride));

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

      v_acc_d = _mm_add_epi32(v_acc_d, v_sum_0123_d);
      v_acc_d = _mm_add_epi32(v_acc_d, v_sum_4567_d);
    }

    v_acc_q = _mm_add_epi64(v_acc_q, _mm_and_si128(v_acc_d, v_zext_mask_q));
    v_acc_q = _mm_add_epi64(v_acc_q, _mm_srli_epi64(v_acc_d, 32));

    src += 8 * stride;
  }

  v_acc_q = _mm_add_epi64(v_acc_q, _mm_srli_si128(v_acc_q, 8));

#if ARCH_X86_64
  return (uint64_t)_mm_cvtsi128_si64(v_acc_q);
#else
  {
    uint64_t tmp;
    _mm_storel_epi64((__m128i *)&tmp, v_acc_q);
    return tmp;
  }
#endif
}

uint64_t aom_sum_squares_2d_i16_sse2(const int16_t *src, int stride, int width,
                                     int height) {
  // 4 elements per row only requires half an XMM register, so this
  // must be a special case, but also note that over 75% of all calls
  // are with size == 4, so it is also the common case.
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_squares_2d_i16_4x4_sse2(src, stride);
  } else if (LIKELY(width % 8 == 0 && height % 8 == 0)) {
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
  const __m128i v_zext_mask_q = _mm_set_epi32(0, 0xffffffff, 0, 0xffffffff);
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

#if ARCH_X86_64
  return (uint64_t)_mm_cvtsi128_si64(v_acc0_q);
#else
  {
    uint64_t tmp;
    _mm_storel_epi64((__m128i *)&tmp, v_acc0_q);
    return tmp;
  }
#endif
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
