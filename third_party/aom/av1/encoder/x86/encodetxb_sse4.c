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

#include <assert.h>
#include <emmintrin.h>  // SSE2
#include <smmintrin.h>  /* SSE4.1 */

#include "aom/aom_integer.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/txb_common.h"

void av1_txb_init_levels_sse4_1(const tran_low_t *const coeff, const int width,
                                const int height, uint8_t *const levels) {
  const int stride = width + TX_PAD_HOR;
  memset(levels - TX_PAD_TOP * stride, 0,
         sizeof(*levels) * TX_PAD_TOP * stride);
  memset(levels + stride * height, 0,
         sizeof(*levels) * (TX_PAD_BOTTOM * stride + TX_PAD_END));

  const __m128i zeros = _mm_setzero_si128();
  int i = 0;
  uint8_t *ls = levels;
  const tran_low_t *cf = coeff;
  if (width == 4) {
    do {
      const __m128i coeffA = _mm_load_si128((__m128i *)(cf));
      const __m128i coeffB = _mm_load_si128((__m128i *)(cf + width));
      const __m128i coeffAB = _mm_packs_epi32(coeffA, coeffB);
      const __m128i absAB = _mm_abs_epi16(coeffAB);
      const __m128i absAB8 = _mm_packs_epi16(absAB, zeros);
      const __m128i lsAB = _mm_unpacklo_epi32(absAB8, zeros);
      _mm_storeu_si128((__m128i *)ls, lsAB);
      ls += (stride << 1);
      cf += (width << 1);
      i += 2;
    } while (i < height);
  } else if (width == 8) {
    do {
      const __m128i coeffA = _mm_load_si128((__m128i *)(cf));
      const __m128i coeffB = _mm_load_si128((__m128i *)(cf + 4));
      const __m128i coeffAB = _mm_packs_epi32(coeffA, coeffB);
      const __m128i absAB = _mm_abs_epi16(coeffAB);
      const __m128i absAB8 = _mm_packs_epi16(absAB, zeros);
      _mm_storeu_si128((__m128i *)ls, absAB8);
      ls += stride;
      cf += width;
      i += 1;
    } while (i < height);
  } else {
    do {
      int j = 0;
      do {
        const __m128i coeffA = _mm_load_si128((__m128i *)(cf));
        const __m128i coeffB = _mm_load_si128((__m128i *)(cf + 4));
        const __m128i coeffC = _mm_load_si128((__m128i *)(cf + 8));
        const __m128i coeffD = _mm_load_si128((__m128i *)(cf + 12));
        const __m128i coeffAB = _mm_packs_epi32(coeffA, coeffB);
        const __m128i coeffCD = _mm_packs_epi32(coeffC, coeffD);
        const __m128i absAB = _mm_abs_epi16(coeffAB);
        const __m128i absCD = _mm_abs_epi16(coeffCD);
        const __m128i absABCD = _mm_packs_epi16(absAB, absCD);
        _mm_storeu_si128((__m128i *)(ls + j), absABCD);
        j += 16;
        cf += 16;
      } while (j < width);
      *(int32_t *)(ls + width) = 0;
      ls += stride;
      i += 1;
    } while (i < height);
  }
}
