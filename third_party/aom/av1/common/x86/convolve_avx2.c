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

#include <immintrin.h>

#include "aom_dsp/aom_dsp_common.h"
#include "./av1_rtcd.h"

#if CONFIG_CONVOLVE_ROUND
static const uint32_t sindex[8] = { 0, 4, 1, 5, 2, 6, 3, 7 };

// 16 epi16 pixels
static INLINE void pixel_clamp_avx2(__m256i *u, int bd) {
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i max = _mm256_sub_epi16(_mm256_slli_epi16(one, bd), one);
  __m256i clamped, mask;

  mask = _mm256_cmpgt_epi16(*u, max);
  clamped = _mm256_andnot_si256(mask, *u);
  mask = _mm256_and_si256(mask, max);
  clamped = _mm256_or_si256(mask, clamped);

  const __m256i zero = _mm256_setzero_si256();
  mask = _mm256_cmpgt_epi16(clamped, zero);
  *u = _mm256_and_si256(clamped, mask);
}

// 8 epi16 pixels
static INLINE void pixel_clamp_sse2(__m128i *u, int bd) {
  const __m128i one = _mm_set1_epi16(1);
  const __m128i max = _mm_sub_epi16(_mm_slli_epi16(one, bd), one);
  __m128i clamped, mask;

  mask = _mm_cmpgt_epi16(*u, max);
  clamped = _mm_andnot_si128(mask, *u);
  mask = _mm_and_si128(mask, max);
  clamped = _mm_or_si128(mask, clamped);

  const __m128i zero = _mm_setzero_si128();
  mask = _mm_cmpgt_epi16(clamped, zero);
  *u = _mm_and_si128(clamped, mask);
}

// Work on multiple of 32 pixels
static INLINE void cal_rounding_32xn_avx2(const int32_t *src, uint8_t *dst,
                                          const __m256i *rnd, int shift,
                                          int num) {
  do {
    __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
    __m256i x1 = _mm256_loadu_si256((const __m256i *)src + 1);
    __m256i x2 = _mm256_loadu_si256((const __m256i *)src + 2);
    __m256i x3 = _mm256_loadu_si256((const __m256i *)src + 3);

    x0 = _mm256_add_epi32(x0, *rnd);
    x1 = _mm256_add_epi32(x1, *rnd);
    x2 = _mm256_add_epi32(x2, *rnd);
    x3 = _mm256_add_epi32(x3, *rnd);

    x0 = _mm256_srai_epi32(x0, shift);
    x1 = _mm256_srai_epi32(x1, shift);
    x2 = _mm256_srai_epi32(x2, shift);
    x3 = _mm256_srai_epi32(x3, shift);

    x0 = _mm256_packs_epi32(x0, x1);
    x2 = _mm256_packs_epi32(x2, x3);

    pixel_clamp_avx2(&x0, 8);
    pixel_clamp_avx2(&x2, 8);

    x0 = _mm256_packus_epi16(x0, x2);
    x1 = _mm256_loadu_si256((const __m256i *)sindex);
    x2 = _mm256_permutevar8x32_epi32(x0, x1);

    _mm256_storeu_si256((__m256i *)dst, x2);
    src += 32;
    dst += 32;
    num--;
  } while (num > 0);
}

static INLINE void cal_rounding_16_avx2(const int32_t *src, uint8_t *dst,
                                        const __m256i *rnd, int shift) {
  __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)src + 1);

  x0 = _mm256_add_epi32(x0, *rnd);
  x1 = _mm256_add_epi32(x1, *rnd);

  x0 = _mm256_srai_epi32(x0, shift);
  x1 = _mm256_srai_epi32(x1, shift);

  x0 = _mm256_packs_epi32(x0, x1);
  pixel_clamp_avx2(&x0, 8);

  const __m256i x2 = _mm256_packus_epi16(x0, x0);
  x1 = _mm256_loadu_si256((const __m256i *)sindex);
  x0 = _mm256_permutevar8x32_epi32(x2, x1);

  _mm_storeu_si128((__m128i *)dst, _mm256_castsi256_si128(x0));
}

static INLINE void cal_rounding_8_avx2(const int32_t *src, uint8_t *dst,
                                       const __m256i *rnd, int shift) {
  __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
  x0 = _mm256_add_epi32(x0, *rnd);
  x0 = _mm256_srai_epi32(x0, shift);

  x0 = _mm256_packs_epi32(x0, x0);
  pixel_clamp_avx2(&x0, 8);

  x0 = _mm256_packus_epi16(x0, x0);
  const __m256i x1 = _mm256_loadu_si256((const __m256i *)sindex);
  x0 = _mm256_permutevar8x32_epi32(x0, x1);

  _mm_storel_epi64((__m128i *)dst, _mm256_castsi256_si128(x0));
}

static INLINE void cal_rounding_4_sse2(const int32_t *src, uint8_t *dst,
                                       const __m128i *rnd, int shift) {
  __m128i x = _mm_loadu_si128((const __m128i *)src);
  x = _mm_add_epi32(x, *rnd);
  x = _mm_srai_epi32(x, shift);

  x = _mm_packs_epi32(x, x);
  pixel_clamp_sse2(&x, 8);

  x = _mm_packus_epi16(x, x);
  *(uint32_t *)dst = _mm_cvtsi128_si32(x);
}

void av1_convolve_rounding_avx2(const int32_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                int bits) {
  const __m256i rnd_num = _mm256_set1_epi32((int32_t)(1 << (bits - 1)));
  const __m128i rnd_num_sse2 = _mm256_castsi256_si128(rnd_num);

  if (w > 64) {  // width = 128
    do {
      cal_rounding_32xn_avx2(src, dst, &rnd_num, bits, 4);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 32) {  // width = 64
    do {
      cal_rounding_32xn_avx2(src, dst, &rnd_num, bits, 2);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 16) {  // width = 32
    do {
      cal_rounding_32xn_avx2(src, dst, &rnd_num, bits, 1);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 8) {  // width = 16
    do {
      cal_rounding_16_avx2(src, dst, &rnd_num, bits);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 4) {  // width = 8
    do {
      cal_rounding_8_avx2(src, dst, &rnd_num, bits);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 2) {  // width = 4
    do {
      cal_rounding_4_sse2(src, dst, &rnd_num_sse2, bits);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else {  // width = 2
    do {
      dst[0] = clip_pixel(ROUND_POWER_OF_TWO(src[0], bits));
      dst[1] = clip_pixel(ROUND_POWER_OF_TWO(src[1], bits));
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  }
}

#if CONFIG_HIGHBITDEPTH
static INLINE void cal_highbd_rounding_32xn_avx2(const int32_t *src,
                                                 uint16_t *dst,
                                                 const __m256i *rnd, int shift,
                                                 int num, int bd) {
  do {
    __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
    __m256i x1 = _mm256_loadu_si256((const __m256i *)src + 1);
    __m256i x2 = _mm256_loadu_si256((const __m256i *)src + 2);
    __m256i x3 = _mm256_loadu_si256((const __m256i *)src + 3);

    x0 = _mm256_add_epi32(x0, *rnd);
    x1 = _mm256_add_epi32(x1, *rnd);
    x2 = _mm256_add_epi32(x2, *rnd);
    x3 = _mm256_add_epi32(x3, *rnd);

    x0 = _mm256_srai_epi32(x0, shift);
    x1 = _mm256_srai_epi32(x1, shift);
    x2 = _mm256_srai_epi32(x2, shift);
    x3 = _mm256_srai_epi32(x3, shift);

    x0 = _mm256_packs_epi32(x0, x1);
    x2 = _mm256_packs_epi32(x2, x3);

    pixel_clamp_avx2(&x0, bd);
    pixel_clamp_avx2(&x2, bd);

    x0 = _mm256_permute4x64_epi64(x0, 0xD8);
    x2 = _mm256_permute4x64_epi64(x2, 0xD8);

    _mm256_storeu_si256((__m256i *)dst, x0);
    _mm256_storeu_si256((__m256i *)(dst + 16), x2);
    src += 32;
    dst += 32;
    num--;
  } while (num > 0);
}

static INLINE void cal_highbd_rounding_16_avx2(const int32_t *src,
                                               uint16_t *dst,
                                               const __m256i *rnd, int shift,
                                               int bd) {
  __m256i x0 = _mm256_loadu_si256((const __m256i *)src);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)src + 1);

  x0 = _mm256_add_epi32(x0, *rnd);
  x1 = _mm256_add_epi32(x1, *rnd);

  x0 = _mm256_srai_epi32(x0, shift);
  x1 = _mm256_srai_epi32(x1, shift);

  x0 = _mm256_packs_epi32(x0, x1);
  pixel_clamp_avx2(&x0, bd);

  x0 = _mm256_permute4x64_epi64(x0, 0xD8);
  _mm256_storeu_si256((__m256i *)dst, x0);
}

static INLINE void cal_highbd_rounding_8_avx2(const int32_t *src, uint16_t *dst,
                                              const __m256i *rnd, int shift,
                                              int bd) {
  __m256i x = _mm256_loadu_si256((const __m256i *)src);
  x = _mm256_add_epi32(x, *rnd);
  x = _mm256_srai_epi32(x, shift);

  x = _mm256_packs_epi32(x, x);
  pixel_clamp_avx2(&x, bd);

  x = _mm256_permute4x64_epi64(x, 0xD8);
  _mm_storeu_si128((__m128i *)dst, _mm256_castsi256_si128(x));
}

static INLINE void cal_highbd_rounding_4_sse2(const int32_t *src, uint16_t *dst,
                                              const __m128i *rnd, int shift,
                                              int bd) {
  __m128i x = _mm_loadu_si128((const __m128i *)src);
  x = _mm_add_epi32(x, *rnd);
  x = _mm_srai_epi32(x, shift);

  x = _mm_packs_epi32(x, x);
  pixel_clamp_sse2(&x, bd);
  _mm_storel_epi64((__m128i *)dst, x);
}

void av1_highbd_convolve_rounding_avx2(const int32_t *src, int src_stride,
                                       uint8_t *dst8, int dst_stride, int w,
                                       int h, int bits, int bd) {
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const __m256i rnd_num = _mm256_set1_epi32((int32_t)(1 << (bits - 1)));
  const __m128i rnd_num_sse2 = _mm256_castsi256_si128(rnd_num);

  if (w > 64) {  // width = 128
    do {
      cal_highbd_rounding_32xn_avx2(src, dst, &rnd_num, bits, 4, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 32) {  // width = 64
    do {
      cal_highbd_rounding_32xn_avx2(src, dst, &rnd_num, bits, 2, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 16) {  // width = 32
    do {
      cal_highbd_rounding_32xn_avx2(src, dst, &rnd_num, bits, 1, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 8) {  // width = 16
    do {
      cal_highbd_rounding_16_avx2(src, dst, &rnd_num, bits, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 4) {  // width = 8
    do {
      cal_highbd_rounding_8_avx2(src, dst, &rnd_num, bits, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else if (w > 2) {  // width = 4
    do {
      cal_highbd_rounding_4_sse2(src, dst, &rnd_num_sse2, bits, bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  } else {  // width = 2
    do {
      dst[0] = clip_pixel_highbd(ROUND_POWER_OF_TWO(src[0], bits), bd);
      dst[1] = clip_pixel_highbd(ROUND_POWER_OF_TWO(src[1], bits), bd);
      src += src_stride;
      dst += dst_stride;
      h--;
    } while (h > 0);
  }
}
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_CONVOLVE_ROUND
