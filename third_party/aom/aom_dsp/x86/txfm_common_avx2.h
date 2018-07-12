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

#ifndef AOM_DSP_X86_TXFM_COMMON_AVX2_H
#define AOM_DSP_X86_TXFM_COMMON_AVX2_H

#include <immintrin.h>

#include "aom_dsp/txfm_common.h"
#include "aom_dsp/x86/common_avx2.h"

#define pair256_set_epi16(a, b)                                            \
  _mm256_set_epi16((int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
                   (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
                   (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
                   (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a))

#define pair256_set_epi32(a, b)                                                \
  _mm256_set_epi32((int)(b), (int)(a), (int)(b), (int)(a), (int)(b), (int)(a), \
                   (int)(b), (int)(a))

static INLINE void mm256_reverse_epi16(__m256i *u) {
  const __m256i control = _mm256_set_epi16(
      0x0100, 0x0302, 0x0504, 0x0706, 0x0908, 0x0B0A, 0x0D0C, 0x0F0E, 0x0100,
      0x0302, 0x0504, 0x0706, 0x0908, 0x0B0A, 0x0D0C, 0x0F0E);
  __m256i v = _mm256_shuffle_epi8(*u, control);
  *u = _mm256_permute2x128_si256(v, v, 1);
}

static INLINE __m256i butter_fly(const __m256i *a0, const __m256i *a1,
                                 const __m256i *cospi) {
  const __m256i dct_rounding = _mm256_set1_epi32(DCT_CONST_ROUNDING);
  __m256i y0 = _mm256_madd_epi16(*a0, *cospi);
  __m256i y1 = _mm256_madd_epi16(*a1, *cospi);

  y0 = _mm256_add_epi32(y0, dct_rounding);
  y1 = _mm256_add_epi32(y1, dct_rounding);
  y0 = _mm256_srai_epi32(y0, DCT_CONST_BITS);
  y1 = _mm256_srai_epi32(y1, DCT_CONST_BITS);

  return _mm256_packs_epi32(y0, y1);
}

static INLINE void txfm_scaling16_avx2(const int16_t c, __m256i *in) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i sqrt2_epi16 = _mm256_set1_epi16(c);
  const __m256i dct_const_rounding = _mm256_set1_epi32(DCT_CONST_ROUNDING);
  __m256i u0, u1;
  int i = 0;

  while (i < 16) {
    in[i] = _mm256_slli_epi16(in[i], 1);

    u0 = _mm256_unpacklo_epi16(zero, in[i]);
    u1 = _mm256_unpackhi_epi16(zero, in[i]);

    u0 = _mm256_madd_epi16(u0, sqrt2_epi16);
    u1 = _mm256_madd_epi16(u1, sqrt2_epi16);

    u0 = _mm256_add_epi32(u0, dct_const_rounding);
    u1 = _mm256_add_epi32(u1, dct_const_rounding);

    u0 = _mm256_srai_epi32(u0, DCT_CONST_BITS);
    u1 = _mm256_srai_epi32(u1, DCT_CONST_BITS);
    in[i] = _mm256_packs_epi32(u0, u1);
    i++;
  }
}

#endif  // AOM_DSP_X86_TXFM_COMMON_AVX2_H
