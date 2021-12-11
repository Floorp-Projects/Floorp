/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>
#include <smmintrin.h>

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"
#include "aom_dsp/x86/sum_squares_sse2.h"
#include "config/aom_dsp_rtcd.h"

static uint64_t aom_sum_squares_2d_i16_nxn_avx2(const int16_t *src, int stride,
                                                int width, int height) {
  uint64_t result;
  __m256i v_acc_q = _mm256_setzero_si256();
  const __m256i v_zext_mask_q = yy_set1_64_from_32i(0xffffffff);
  for (int col = 0; col < height; col += 4) {
    __m256i v_acc_d = _mm256_setzero_si256();
    for (int row = 0; row < width; row += 16) {
      const int16_t *tempsrc = src + row;
      const __m256i v_val_0_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 0 * stride));
      const __m256i v_val_1_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 1 * stride));
      const __m256i v_val_2_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 2 * stride));
      const __m256i v_val_3_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 3 * stride));

      const __m256i v_sq_0_d = _mm256_madd_epi16(v_val_0_w, v_val_0_w);
      const __m256i v_sq_1_d = _mm256_madd_epi16(v_val_1_w, v_val_1_w);
      const __m256i v_sq_2_d = _mm256_madd_epi16(v_val_2_w, v_val_2_w);
      const __m256i v_sq_3_d = _mm256_madd_epi16(v_val_3_w, v_val_3_w);

      const __m256i v_sum_01_d = _mm256_add_epi32(v_sq_0_d, v_sq_1_d);
      const __m256i v_sum_23_d = _mm256_add_epi32(v_sq_2_d, v_sq_3_d);
      const __m256i v_sum_0123_d = _mm256_add_epi32(v_sum_01_d, v_sum_23_d);

      v_acc_d = _mm256_add_epi32(v_acc_d, v_sum_0123_d);
    }
    v_acc_q =
        _mm256_add_epi64(v_acc_q, _mm256_and_si256(v_acc_d, v_zext_mask_q));
    v_acc_q = _mm256_add_epi64(v_acc_q, _mm256_srli_epi64(v_acc_d, 32));
    src += 4 * stride;
  }
  __m128i lower_64_2_Value = _mm256_castsi256_si128(v_acc_q);
  __m128i higher_64_2_Value = _mm256_extracti128_si256(v_acc_q, 1);
  __m128i result_64_2_int = _mm_add_epi64(lower_64_2_Value, higher_64_2_Value);

  result_64_2_int = _mm_add_epi64(
      result_64_2_int, _mm_unpackhi_epi64(result_64_2_int, result_64_2_int));

  xx_storel_64(&result, result_64_2_int);

  return result;
}

uint64_t aom_sum_squares_2d_i16_avx2(const int16_t *src, int stride, int width,
                                     int height) {
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_squares_2d_i16_4x4_sse2(src, stride);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    return aom_sum_squares_2d_i16_4xn_sse2(src, stride, height);
  } else if (LIKELY(width == 8 && (height & 3) == 0)) {
    return aom_sum_squares_2d_i16_nxn_sse2(src, stride, width, height);
  } else if (LIKELY(((width & 15) == 0) && ((height & 3) == 0))) {
    return aom_sum_squares_2d_i16_nxn_avx2(src, stride, width, height);
  } else {
    return aom_sum_squares_2d_i16_c(src, stride, width, height);
  }
}
