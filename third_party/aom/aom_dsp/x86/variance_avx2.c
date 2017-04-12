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

#include <immintrin.h>
#include "./aom_dsp_rtcd.h"

typedef void (*get_var_avx2)(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride,
                             unsigned int *sse, int *sum);

void aom_get32x32var_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *ref, int ref_stride, unsigned int *sse,
                          int *sum);

static void variance_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *ref, int ref_stride, int w, int h,
                          unsigned int *sse, int *sum, get_var_avx2 var_fn,
                          int block_size) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += 16) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(&src[src_stride * i + j], src_stride, &ref[ref_stride * i + j],
             ref_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}

unsigned int aom_variance16x16_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  unsigned int variance;
  variance_avx2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                aom_get16x16var_avx2, 16);

  variance = *sse - (((uint32_t)((int64_t)sum * sum)) >> 8);
  _mm256_zeroupper();
  return variance;
}

unsigned int aom_mse16x16_avx2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  int sum;
  aom_get16x16var_avx2(src, src_stride, ref, ref_stride, sse, &sum);
  _mm256_zeroupper();
  return *sse;
}

unsigned int aom_variance32x16_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  unsigned int variance;
  variance_avx2(src, src_stride, ref, ref_stride, 32, 16, sse, &sum,
                aom_get32x32var_avx2, 32);

  variance = *sse - (uint32_t)(((int64_t)sum * sum) >> 9);
  _mm256_zeroupper();
  return variance;
}

unsigned int aom_variance32x32_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  unsigned int variance;
  variance_avx2(src, src_stride, ref, ref_stride, 32, 32, sse, &sum,
                aom_get32x32var_avx2, 32);

  variance = *sse - (uint32_t)(((int64_t)sum * sum) >> 10);
  _mm256_zeroupper();
  return variance;
}

unsigned int aom_variance64x64_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  unsigned int variance;
  variance_avx2(src, src_stride, ref, ref_stride, 64, 64, sse, &sum,
                aom_get32x32var_avx2, 32);

  variance = *sse - (uint32_t)(((int64_t)sum * sum) >> 12);
  _mm256_zeroupper();
  return variance;
}

unsigned int aom_variance64x32_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  unsigned int variance;
  variance_avx2(src, src_stride, ref, ref_stride, 64, 32, sse, &sum,
                aom_get32x32var_avx2, 32);

  variance = *sse - (uint32_t)(((int64_t)sum * sum) >> 11);
  _mm256_zeroupper();
  return variance;
}

unsigned int aom_sub_pixel_variance32xh_avx2(const uint8_t *src, int src_stride,
                                             int x_offset, int y_offset,
                                             const uint8_t *dst, int dst_stride,
                                             int height, unsigned int *sse);

unsigned int aom_sub_pixel_avg_variance32xh_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, const uint8_t *sec, int sec_stride,
    int height, unsigned int *sseptr);

unsigned int aom_sub_pixel_variance64x64_avx2(const uint8_t *src,
                                              int src_stride, int x_offset,
                                              int y_offset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  unsigned int sse1;
  const int se1 = aom_sub_pixel_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, 64, &sse1);
  unsigned int sse2;
  const int se2 =
      aom_sub_pixel_variance32xh_avx2(src + 32, src_stride, x_offset, y_offset,
                                      dst + 32, dst_stride, 64, &sse2);
  const int se = se1 + se2;
  unsigned int variance;
  *sse = sse1 + sse2;

  variance = *sse - (uint32_t)(((int64_t)se * se) >> 12);
  _mm256_zeroupper();
  return variance;
}

unsigned int aom_sub_pixel_variance32x32_avx2(const uint8_t *src,
                                              int src_stride, int x_offset,
                                              int y_offset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  const int se = aom_sub_pixel_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, 32, sse);

  const unsigned int variance = *sse - (uint32_t)(((int64_t)se * se) >> 10);
  _mm256_zeroupper();
  return variance;
}

unsigned int aom_sub_pixel_avg_variance64x64_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, unsigned int *sse, const uint8_t *sec) {
  unsigned int sse1;
  const int se1 = aom_sub_pixel_avg_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, sec, 64, 64, &sse1);
  unsigned int sse2;
  const int se2 = aom_sub_pixel_avg_variance32xh_avx2(
      src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride, sec + 32,
      64, 64, &sse2);
  const int se = se1 + se2;
  unsigned int variance;

  *sse = sse1 + sse2;

  variance = *sse - (uint32_t)(((int64_t)se * se) >> 12);
  _mm256_zeroupper();
  return variance;
}

unsigned int aom_sub_pixel_avg_variance32x32_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, unsigned int *sse, const uint8_t *sec) {
  // Process 32 elements in parallel.
  const int se = aom_sub_pixel_avg_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, sec, 32, 32, sse);

  const unsigned int variance = *sse - (uint32_t)(((int64_t)se * se) >> 10);
  _mm256_zeroupper();
  return variance;
}
