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
#include <immintrin.h>  // AVX2
#include "./aom_dsp_rtcd.h"
#include "aom/aom_integer.h"

void aom_sad32x32x4d_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t res[4]) {
  __m256i src_reg, ref0_reg, ref1_reg, ref2_reg, ref3_reg;
  __m256i sum_ref0, sum_ref1, sum_ref2, sum_ref3;
  __m256i sum_mlow, sum_mhigh;
  int i;
  const uint8_t *ref0, *ref1, *ref2, *ref3;

  ref0 = ref[0];
  ref1 = ref[1];
  ref2 = ref[2];
  ref3 = ref[3];
  sum_ref0 = _mm256_set1_epi16(0);
  sum_ref1 = _mm256_set1_epi16(0);
  sum_ref2 = _mm256_set1_epi16(0);
  sum_ref3 = _mm256_set1_epi16(0);
  for (i = 0; i < 32; i++) {
    // load src and all refs
    src_reg = _mm256_loadu_si256((const __m256i *)src);
    ref0_reg = _mm256_loadu_si256((const __m256i *)ref0);
    ref1_reg = _mm256_loadu_si256((const __m256i *)ref1);
    ref2_reg = _mm256_loadu_si256((const __m256i *)ref2);
    ref3_reg = _mm256_loadu_si256((const __m256i *)ref3);
    // sum of the absolute differences between every ref-i to src
    ref0_reg = _mm256_sad_epu8(ref0_reg, src_reg);
    ref1_reg = _mm256_sad_epu8(ref1_reg, src_reg);
    ref2_reg = _mm256_sad_epu8(ref2_reg, src_reg);
    ref3_reg = _mm256_sad_epu8(ref3_reg, src_reg);
    // sum every ref-i
    sum_ref0 = _mm256_add_epi32(sum_ref0, ref0_reg);
    sum_ref1 = _mm256_add_epi32(sum_ref1, ref1_reg);
    sum_ref2 = _mm256_add_epi32(sum_ref2, ref2_reg);
    sum_ref3 = _mm256_add_epi32(sum_ref3, ref3_reg);

    src += src_stride;
    ref0 += ref_stride;
    ref1 += ref_stride;
    ref2 += ref_stride;
    ref3 += ref_stride;
  }
  {
    __m128i sum;
    // in sum_ref-i the result is saved in the first 4 bytes
    // the other 4 bytes are zeroed.
    // sum_ref1 and sum_ref3 are shifted left by 4 bytes
    sum_ref1 = _mm256_slli_si256(sum_ref1, 4);
    sum_ref3 = _mm256_slli_si256(sum_ref3, 4);

    // merge sum_ref0 and sum_ref1 also sum_ref2 and sum_ref3
    sum_ref0 = _mm256_or_si256(sum_ref0, sum_ref1);
    sum_ref2 = _mm256_or_si256(sum_ref2, sum_ref3);

    // merge every 64 bit from each sum_ref-i
    sum_mlow = _mm256_unpacklo_epi64(sum_ref0, sum_ref2);
    sum_mhigh = _mm256_unpackhi_epi64(sum_ref0, sum_ref2);

    // add the low 64 bit to the high 64 bit
    sum_mlow = _mm256_add_epi32(sum_mlow, sum_mhigh);

    // add the low 128 bit to the high 128 bit
    sum = _mm_add_epi32(_mm256_castsi256_si128(sum_mlow),
                        _mm256_extractf128_si256(sum_mlow, 1));

    _mm_storeu_si128((__m128i *)(res), sum);
  }
  _mm256_zeroupper();
}

void aom_sad64x64x4d_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t res[4]) {
  __m256i src_reg, srcnext_reg, ref0_reg, ref0next_reg;
  __m256i ref1_reg, ref1next_reg, ref2_reg, ref2next_reg;
  __m256i ref3_reg, ref3next_reg;
  __m256i sum_ref0, sum_ref1, sum_ref2, sum_ref3;
  __m256i sum_mlow, sum_mhigh;
  int i;
  const uint8_t *ref0, *ref1, *ref2, *ref3;

  ref0 = ref[0];
  ref1 = ref[1];
  ref2 = ref[2];
  ref3 = ref[3];
  sum_ref0 = _mm256_set1_epi16(0);
  sum_ref1 = _mm256_set1_epi16(0);
  sum_ref2 = _mm256_set1_epi16(0);
  sum_ref3 = _mm256_set1_epi16(0);
  for (i = 0; i < 64; i++) {
    // load 64 bytes from src and all refs
    src_reg = _mm256_loadu_si256((const __m256i *)src);
    srcnext_reg = _mm256_loadu_si256((const __m256i *)(src + 32));
    ref0_reg = _mm256_loadu_si256((const __m256i *)ref0);
    ref0next_reg = _mm256_loadu_si256((const __m256i *)(ref0 + 32));
    ref1_reg = _mm256_loadu_si256((const __m256i *)ref1);
    ref1next_reg = _mm256_loadu_si256((const __m256i *)(ref1 + 32));
    ref2_reg = _mm256_loadu_si256((const __m256i *)ref2);
    ref2next_reg = _mm256_loadu_si256((const __m256i *)(ref2 + 32));
    ref3_reg = _mm256_loadu_si256((const __m256i *)ref3);
    ref3next_reg = _mm256_loadu_si256((const __m256i *)(ref3 + 32));
    // sum of the absolute differences between every ref-i to src
    ref0_reg = _mm256_sad_epu8(ref0_reg, src_reg);
    ref1_reg = _mm256_sad_epu8(ref1_reg, src_reg);
    ref2_reg = _mm256_sad_epu8(ref2_reg, src_reg);
    ref3_reg = _mm256_sad_epu8(ref3_reg, src_reg);
    ref0next_reg = _mm256_sad_epu8(ref0next_reg, srcnext_reg);
    ref1next_reg = _mm256_sad_epu8(ref1next_reg, srcnext_reg);
    ref2next_reg = _mm256_sad_epu8(ref2next_reg, srcnext_reg);
    ref3next_reg = _mm256_sad_epu8(ref3next_reg, srcnext_reg);

    // sum every ref-i
    sum_ref0 = _mm256_add_epi32(sum_ref0, ref0_reg);
    sum_ref1 = _mm256_add_epi32(sum_ref1, ref1_reg);
    sum_ref2 = _mm256_add_epi32(sum_ref2, ref2_reg);
    sum_ref3 = _mm256_add_epi32(sum_ref3, ref3_reg);
    sum_ref0 = _mm256_add_epi32(sum_ref0, ref0next_reg);
    sum_ref1 = _mm256_add_epi32(sum_ref1, ref1next_reg);
    sum_ref2 = _mm256_add_epi32(sum_ref2, ref2next_reg);
    sum_ref3 = _mm256_add_epi32(sum_ref3, ref3next_reg);
    src += src_stride;
    ref0 += ref_stride;
    ref1 += ref_stride;
    ref2 += ref_stride;
    ref3 += ref_stride;
  }
  {
    __m128i sum;

    // in sum_ref-i the result is saved in the first 4 bytes
    // the other 4 bytes are zeroed.
    // sum_ref1 and sum_ref3 are shifted left by 4 bytes
    sum_ref1 = _mm256_slli_si256(sum_ref1, 4);
    sum_ref3 = _mm256_slli_si256(sum_ref3, 4);

    // merge sum_ref0 and sum_ref1 also sum_ref2 and sum_ref3
    sum_ref0 = _mm256_or_si256(sum_ref0, sum_ref1);
    sum_ref2 = _mm256_or_si256(sum_ref2, sum_ref3);

    // merge every 64 bit from each sum_ref-i
    sum_mlow = _mm256_unpacklo_epi64(sum_ref0, sum_ref2);
    sum_mhigh = _mm256_unpackhi_epi64(sum_ref0, sum_ref2);

    // add the low 64 bit to the high 64 bit
    sum_mlow = _mm256_add_epi32(sum_mlow, sum_mhigh);

    // add the low 128 bit to the high 128 bit
    sum = _mm_add_epi32(_mm256_castsi256_si128(sum_mlow),
                        _mm256_extractf128_si256(sum_mlow, 1));

    _mm_storeu_si128((__m128i *)(res), sum);
  }
  _mm256_zeroupper();
}

void aom_sad32x64x4d_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t res[4]) {
  const uint8_t *rf[4];
  uint32_t sum0[4];
  uint32_t sum1[4];

  rf[0] = ref[0];
  rf[1] = ref[1];
  rf[2] = ref[2];
  rf[3] = ref[3];
  aom_sad32x32x4d_avx2(src, src_stride, rf, ref_stride, sum0);
  src += src_stride << 5;
  rf[0] += ref_stride << 5;
  rf[1] += ref_stride << 5;
  rf[2] += ref_stride << 5;
  rf[3] += ref_stride << 5;
  aom_sad32x32x4d_avx2(src, src_stride, rf, ref_stride, sum1);
  res[0] = sum0[0] + sum1[0];
  res[1] = sum0[1] + sum1[1];
  res[2] = sum0[2] + sum1[2];
  res[3] = sum0[3] + sum1[3];
}

void aom_sad64x32x4d_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t res[4]) {
  const uint8_t *rf[4];
  uint32_t sum0[4];
  uint32_t sum1[4];
  unsigned int half_width = 32;

  rf[0] = ref[0];
  rf[1] = ref[1];
  rf[2] = ref[2];
  rf[3] = ref[3];
  aom_sad32x32x4d_avx2(src, src_stride, rf, ref_stride, sum0);
  src += half_width;
  rf[0] += half_width;
  rf[1] += half_width;
  rf[2] += half_width;
  rf[3] += half_width;
  aom_sad32x32x4d_avx2(src, src_stride, rf, ref_stride, sum1);
  res[0] = sum0[0] + sum1[0];
  res[1] = sum0[1] + sum1[1];
  res[2] = sum0[2] + sum1[2];
  res[3] = sum0[3] + sum1[3];
}
