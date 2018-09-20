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

#include <assert.h>
#include <immintrin.h>  // AVX2

#include "config/aom_dsp_rtcd.h"

typedef void (*high_variance_fn_t)(const uint16_t *src, int src_stride,
                                   const uint16_t *ref, int ref_stride,
                                   uint32_t *sse, int *sum);

void aom_highbd_calc8x8var_avx2(const uint16_t *src, int src_stride,
                                const uint16_t *ref, int ref_stride,
                                uint32_t *sse, int *sum) {
  __m256i v_sum_d = _mm256_setzero_si256();
  __m256i v_sse_d = _mm256_setzero_si256();
  for (int i = 0; i < 8; i += 2) {
    const __m128i v_p_a0 = _mm_loadu_si128((const __m128i *)src);
    const __m128i v_p_a1 = _mm_loadu_si128((const __m128i *)(src + src_stride));
    const __m128i v_p_b0 = _mm_loadu_si128((const __m128i *)ref);
    const __m128i v_p_b1 = _mm_loadu_si128((const __m128i *)(ref + ref_stride));
    __m256i v_p_a = _mm256_castsi128_si256(v_p_a0);
    __m256i v_p_b = _mm256_castsi128_si256(v_p_b0);
    v_p_a = _mm256_inserti128_si256(v_p_a, v_p_a1, 1);
    v_p_b = _mm256_inserti128_si256(v_p_b, v_p_b1, 1);
    const __m256i v_diff = _mm256_sub_epi16(v_p_a, v_p_b);
    const __m256i v_sqrdiff = _mm256_madd_epi16(v_diff, v_diff);
    v_sum_d = _mm256_add_epi16(v_sum_d, v_diff);
    v_sse_d = _mm256_add_epi32(v_sse_d, v_sqrdiff);
    src += src_stride * 2;
    ref += ref_stride * 2;
  }
  __m256i v_sum00 = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(v_sum_d));
  __m256i v_sum01 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v_sum_d, 1));
  __m256i v_sum0 = _mm256_add_epi32(v_sum00, v_sum01);
  __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, v_sse_d);
  __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, v_sse_d);
  __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
  const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
  const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
  __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
  v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
  *sum = _mm_extract_epi32(v_d, 0);
  *sse = _mm_extract_epi32(v_d, 1);
}

void aom_highbd_calc16x16var_avx2(const uint16_t *src, int src_stride,
                                  const uint16_t *ref, int ref_stride,
                                  uint32_t *sse, int *sum) {
  __m256i v_sum_d = _mm256_setzero_si256();
  __m256i v_sse_d = _mm256_setzero_si256();
  const __m256i one = _mm256_set1_epi16(1);
  for (int i = 0; i < 16; ++i) {
    const __m256i v_p_a = _mm256_loadu_si256((const __m256i *)src);
    const __m256i v_p_b = _mm256_loadu_si256((const __m256i *)ref);
    const __m256i v_diff = _mm256_sub_epi16(v_p_a, v_p_b);
    const __m256i v_sqrdiff = _mm256_madd_epi16(v_diff, v_diff);
    v_sum_d = _mm256_add_epi16(v_sum_d, v_diff);
    v_sse_d = _mm256_add_epi32(v_sse_d, v_sqrdiff);
    src += src_stride;
    ref += ref_stride;
  }
  __m256i v_sum0 = _mm256_madd_epi16(v_sum_d, one);
  __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, v_sse_d);
  __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, v_sse_d);
  __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
  const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
  const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
  __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
  v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
  *sum = _mm_extract_epi32(v_d, 0);
  *sse = _mm_extract_epi32(v_d, 1);
}

static void highbd_10_variance_avx2(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride, int w,
                                    int h, uint32_t *sse, int *sum,
                                    high_variance_fn_t var_fn, int block_size) {
  int i, j;
  uint64_t sse_long = 0;
  int32_t sum_long = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      sse_long += sse0;
      sum_long += sum0;
    }
  }
  *sum = ROUND_POWER_OF_TWO(sum_long, 2);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
}

#define VAR_FN(w, h, block_size, shift)                                    \
  uint32_t aom_highbd_10_variance##w##x##h##_avx2(                         \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,            \
      int ref_stride, uint32_t *sse) {                                     \
    int sum;                                                               \
    int64_t var;                                                           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                             \
    highbd_10_variance_avx2(                                               \
        src, src_stride, ref, ref_stride, w, h, sse, &sum,                 \
        aom_highbd_calc##block_size##x##block_size##var_avx2, block_size); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) >> shift);               \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }

VAR_FN(128, 128, 16, 14);
VAR_FN(128, 64, 16, 13);
VAR_FN(64, 128, 16, 13);
VAR_FN(64, 64, 16, 12);
VAR_FN(64, 32, 16, 11);
VAR_FN(32, 64, 16, 11);
VAR_FN(32, 32, 16, 10);
VAR_FN(32, 16, 16, 9);
VAR_FN(16, 32, 16, 9);
VAR_FN(16, 16, 16, 8);
VAR_FN(16, 8, 8, 7);
VAR_FN(8, 16, 8, 7);
VAR_FN(8, 8, 8, 6);
VAR_FN(16, 4, 16, 6);
VAR_FN(8, 32, 8, 8);
VAR_FN(32, 8, 8, 8);
VAR_FN(16, 64, 16, 10);
VAR_FN(64, 16, 16, 10);

#undef VAR_FN
