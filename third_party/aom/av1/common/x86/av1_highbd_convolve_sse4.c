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
#include <smmintrin.h>

#include "config/av1_rtcd.h"

#include "av1/common/filter.h"

typedef void (*TransposeSave)(int width, int pixelsNum, uint32_t *src,
                              int src_stride, uint16_t *dst, int dst_stride,
                              int bd);

// pixelsNum 0: write all 4 pixels
//           1/2/3: residual pixels 1/2/3
static void writePixel(__m128i *u, int width, int pixelsNum, uint16_t *dst,
                       int dst_stride) {
  if (2 == width) {
    if (0 == pixelsNum) {
      *(int *)dst = _mm_cvtsi128_si32(u[0]);
      *(int *)(dst + dst_stride) = _mm_cvtsi128_si32(u[1]);
      *(int *)(dst + 2 * dst_stride) = _mm_cvtsi128_si32(u[2]);
      *(int *)(dst + 3 * dst_stride) = _mm_cvtsi128_si32(u[3]);
    } else if (1 == pixelsNum) {
      *(int *)dst = _mm_cvtsi128_si32(u[0]);
    } else if (2 == pixelsNum) {
      *(int *)dst = _mm_cvtsi128_si32(u[0]);
      *(int *)(dst + dst_stride) = _mm_cvtsi128_si32(u[1]);
    } else if (3 == pixelsNum) {
      *(int *)dst = _mm_cvtsi128_si32(u[0]);
      *(int *)(dst + dst_stride) = _mm_cvtsi128_si32(u[1]);
      *(int *)(dst + 2 * dst_stride) = _mm_cvtsi128_si32(u[2]);
    }
  } else {
    if (0 == pixelsNum) {
      _mm_storel_epi64((__m128i *)dst, u[0]);
      _mm_storel_epi64((__m128i *)(dst + dst_stride), u[1]);
      _mm_storel_epi64((__m128i *)(dst + 2 * dst_stride), u[2]);
      _mm_storel_epi64((__m128i *)(dst + 3 * dst_stride), u[3]);
    } else if (1 == pixelsNum) {
      _mm_storel_epi64((__m128i *)dst, u[0]);
    } else if (2 == pixelsNum) {
      _mm_storel_epi64((__m128i *)dst, u[0]);
      _mm_storel_epi64((__m128i *)(dst + dst_stride), u[1]);
    } else if (3 == pixelsNum) {
      _mm_storel_epi64((__m128i *)dst, u[0]);
      _mm_storel_epi64((__m128i *)(dst + dst_stride), u[1]);
      _mm_storel_epi64((__m128i *)(dst + 2 * dst_stride), u[2]);
    }
  }
}

// 16-bit pixels clip with bd (10/12)
static void highbd_clip(__m128i *p, int numVecs, int bd) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  const __m128i max = _mm_sub_epi16(_mm_slli_epi16(one, bd), one);
  __m128i clamped, mask;
  int i;

  for (i = 0; i < numVecs; i++) {
    mask = _mm_cmpgt_epi16(p[i], max);
    clamped = _mm_andnot_si128(mask, p[i]);
    mask = _mm_and_si128(mask, max);
    clamped = _mm_or_si128(mask, clamped);
    mask = _mm_cmpgt_epi16(clamped, zero);
    p[i] = _mm_and_si128(clamped, mask);
  }
}

static void transClipPixel(uint32_t *src, int src_stride, __m128i *u, int bd) {
  __m128i v0, v1;
  __m128i rnd = _mm_set1_epi32(1 << (FILTER_BITS - 1));

  u[0] = _mm_loadu_si128((__m128i const *)src);
  u[1] = _mm_loadu_si128((__m128i const *)(src + src_stride));
  u[2] = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u[3] = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));

  u[0] = _mm_add_epi32(u[0], rnd);
  u[1] = _mm_add_epi32(u[1], rnd);
  u[2] = _mm_add_epi32(u[2], rnd);
  u[3] = _mm_add_epi32(u[3], rnd);

  u[0] = _mm_srai_epi32(u[0], FILTER_BITS);
  u[1] = _mm_srai_epi32(u[1], FILTER_BITS);
  u[2] = _mm_srai_epi32(u[2], FILTER_BITS);
  u[3] = _mm_srai_epi32(u[3], FILTER_BITS);

  u[0] = _mm_packus_epi32(u[0], u[1]);
  u[1] = _mm_packus_epi32(u[2], u[3]);

  highbd_clip(u, 2, bd);

  v0 = _mm_unpacklo_epi16(u[0], u[1]);
  v1 = _mm_unpackhi_epi16(u[0], u[1]);

  u[0] = _mm_unpacklo_epi16(v0, v1);
  u[2] = _mm_unpackhi_epi16(v0, v1);

  u[1] = _mm_srli_si128(u[0], 8);
  u[3] = _mm_srli_si128(u[2], 8);
}

// pixelsNum = 0     : all 4 rows of pixels will be saved.
// pixelsNum = 1/2/3 : residual 1/2/4 rows of pixels will be saved.
void trans_save_4x4(int width, int pixelsNum, uint32_t *src, int src_stride,
                    uint16_t *dst, int dst_stride, int bd) {
  __m128i u[4];
  transClipPixel(src, src_stride, u, bd);
  writePixel(u, width, pixelsNum, dst, dst_stride);
}

void trans_accum_save_4x4(int width, int pixelsNum, uint32_t *src,
                          int src_stride, uint16_t *dst, int dst_stride,
                          int bd) {
  __m128i u[4], v[4];
  const __m128i ones = _mm_set1_epi16(1);

  transClipPixel(src, src_stride, u, bd);

  v[0] = _mm_loadl_epi64((__m128i const *)dst);
  v[1] = _mm_loadl_epi64((__m128i const *)(dst + dst_stride));
  v[2] = _mm_loadl_epi64((__m128i const *)(dst + 2 * dst_stride));
  v[3] = _mm_loadl_epi64((__m128i const *)(dst + 3 * dst_stride));

  u[0] = _mm_add_epi16(u[0], v[0]);
  u[1] = _mm_add_epi16(u[1], v[1]);
  u[2] = _mm_add_epi16(u[2], v[2]);
  u[3] = _mm_add_epi16(u[3], v[3]);

  u[0] = _mm_add_epi16(u[0], ones);
  u[1] = _mm_add_epi16(u[1], ones);
  u[2] = _mm_add_epi16(u[2], ones);
  u[3] = _mm_add_epi16(u[3], ones);

  u[0] = _mm_srai_epi16(u[0], 1);
  u[1] = _mm_srai_epi16(u[1], 1);
  u[2] = _mm_srai_epi16(u[2], 1);
  u[3] = _mm_srai_epi16(u[3], 1);

  writePixel(u, width, pixelsNum, dst, dst_stride);
}

// Vertical convolutional filter

typedef void (*WritePixels)(__m128i *u, int bd, uint16_t *dst);

static void highbdRndingPacks(__m128i *u) {
  __m128i rnd = _mm_set1_epi32(1 << (FILTER_BITS - 1));
  u[0] = _mm_add_epi32(u[0], rnd);
  u[0] = _mm_srai_epi32(u[0], FILTER_BITS);
  u[0] = _mm_packus_epi32(u[0], u[0]);
}

static void write2pixelsOnly(__m128i *u, int bd, uint16_t *dst) {
  highbdRndingPacks(u);
  highbd_clip(u, 1, bd);
  *(uint32_t *)dst = _mm_cvtsi128_si32(u[0]);
}

static void write2pixelsAccum(__m128i *u, int bd, uint16_t *dst) {
  __m128i v = _mm_loadl_epi64((__m128i const *)dst);
  const __m128i ones = _mm_set1_epi16(1);

  highbdRndingPacks(u);
  highbd_clip(u, 1, bd);

  v = _mm_add_epi16(v, u[0]);
  v = _mm_add_epi16(v, ones);
  v = _mm_srai_epi16(v, 1);
  *(uint32_t *)dst = _mm_cvtsi128_si32(v);
}

WritePixels write2pixelsTab[2] = { write2pixelsOnly, write2pixelsAccum };

static void write4pixelsOnly(__m128i *u, int bd, uint16_t *dst) {
  highbdRndingPacks(u);
  highbd_clip(u, 1, bd);
  _mm_storel_epi64((__m128i *)dst, u[0]);
}

static void write4pixelsAccum(__m128i *u, int bd, uint16_t *dst) {
  __m128i v = _mm_loadl_epi64((__m128i const *)dst);
  const __m128i ones = _mm_set1_epi16(1);

  highbdRndingPacks(u);
  highbd_clip(u, 1, bd);

  v = _mm_add_epi16(v, u[0]);
  v = _mm_add_epi16(v, ones);
  v = _mm_srai_epi16(v, 1);
  _mm_storel_epi64((__m128i *)dst, v);
}

WritePixels write4pixelsTab[2] = { write4pixelsOnly, write4pixelsAccum };
