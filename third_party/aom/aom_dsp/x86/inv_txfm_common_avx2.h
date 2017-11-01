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

#ifndef AOM_DSP_X86_INV_TXFM_COMMON_AVX2_H
#define AOM_DSP_X86_INV_TXFM_COMMON_AVX2_H

#include <immintrin.h>

#include "aom_dsp/txfm_common.h"
#include "aom_dsp/x86/txfm_common_avx2.h"

static INLINE void load_coeff(const tran_low_t *coeff, __m256i *in) {
  if (sizeof(tran_low_t) == 4) {
    *in = _mm256_setr_epi16(
        (int16_t)coeff[0], (int16_t)coeff[1], (int16_t)coeff[2],
        (int16_t)coeff[3], (int16_t)coeff[4], (int16_t)coeff[5],
        (int16_t)coeff[6], (int16_t)coeff[7], (int16_t)coeff[8],
        (int16_t)coeff[9], (int16_t)coeff[10], (int16_t)coeff[11],
        (int16_t)coeff[12], (int16_t)coeff[13], (int16_t)coeff[14],
        (int16_t)coeff[15]);
  } else {
    *in = _mm256_loadu_si256((const __m256i *)coeff);
  }
}

static INLINE void load_buffer_16x16(const tran_low_t *coeff, __m256i *in) {
  int i = 0;
  while (i < 16) {
    load_coeff(coeff + (i << 4), &in[i]);
    i += 1;
  }
}

static INLINE void recon_and_store(const __m256i *res, uint8_t *output) {
  const __m128i zero = _mm_setzero_si128();
  __m128i x = _mm_loadu_si128((__m128i const *)output);
  __m128i p0 = _mm_unpacklo_epi8(x, zero);
  __m128i p1 = _mm_unpackhi_epi8(x, zero);

  p0 = _mm_add_epi16(p0, _mm256_castsi256_si128(*res));
  p1 = _mm_add_epi16(p1, _mm256_extractf128_si256(*res, 1));
  x = _mm_packus_epi16(p0, p1);
  _mm_storeu_si128((__m128i *)output, x);
}

#define IDCT_ROUNDING_POS (6)
static INLINE void store_buffer_16xN(__m256i *in, const int stride,
                                     uint8_t *output, int num) {
  const __m256i rounding = _mm256_set1_epi16(1 << (IDCT_ROUNDING_POS - 1));
  int i = 0;

  while (i < num) {
    in[i] = _mm256_adds_epi16(in[i], rounding);
    in[i] = _mm256_srai_epi16(in[i], IDCT_ROUNDING_POS);
    recon_and_store(&in[i], output + i * stride);
    i += 1;
  }
}

static INLINE void unpack_butter_fly(const __m256i *a0, const __m256i *a1,
                                     const __m256i *c0, const __m256i *c1,
                                     __m256i *b0, __m256i *b1) {
  __m256i x0, x1;
  x0 = _mm256_unpacklo_epi16(*a0, *a1);
  x1 = _mm256_unpackhi_epi16(*a0, *a1);
  *b0 = butter_fly(&x0, &x1, c0);
  *b1 = butter_fly(&x0, &x1, c1);
}

void av1_idct16_avx2(__m256i *in);

#endif  // AOM_DSP_X86_INV_TXFM_COMMON_AVX2_H
