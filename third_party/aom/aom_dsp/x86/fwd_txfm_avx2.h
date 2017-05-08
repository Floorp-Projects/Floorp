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

#ifndef AOM_DSP_X86_FWD_TXFM_AVX2_H
#define AOM_DSP_X86_FWD_TXFM_AVX2_H

#include "./aom_config.h"

static INLINE void storeu_output_avx2(const __m256i *coeff, tran_low_t *out) {
#if CONFIG_HIGHBITDEPTH
  const __m256i zero = _mm256_setzero_si256();
  const __m256i sign = _mm256_cmpgt_epi16(zero, *coeff);

  __m256i x0 = _mm256_unpacklo_epi16(*coeff, sign);
  __m256i x1 = _mm256_unpackhi_epi16(*coeff, sign);

  __m256i y0 = _mm256_permute2x128_si256(x0, x1, 0x20);
  __m256i y1 = _mm256_permute2x128_si256(x0, x1, 0x31);

  _mm256_storeu_si256((__m256i *)out, y0);
  _mm256_storeu_si256((__m256i *)(out + 8), y1);
#else
  _mm256_storeu_si256((__m256i *)out, *coeff);
#endif
}

#endif  // AOM_DSP_X86_FWD_TXFM_AVX2_H
