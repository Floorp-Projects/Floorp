/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vpx_dsp_rtcd.h"

typedef void (*get_var_avx2)(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride,
                             unsigned int *sse, int *sum);

void vpx_get32x32var_avx2(const uint8_t *src, int src_stride,
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

unsigned int vpx_variance16x16_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                vpx_get16x16var_avx2, 16);
  return *sse - (((uint32_t)((int64_t)sum * sum)) >> 8);
}

unsigned int vpx_mse16x16_avx2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  int sum;
  vpx_get16x16var_avx2(src, src_stride, ref, ref_stride, sse, &sum);
  return *sse;
}

unsigned int vpx_variance32x16_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 32, 16, sse, &sum,
                vpx_get32x32var_avx2, 32);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 9);
}

unsigned int vpx_variance32x32_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 32, 32, sse, &sum,
                vpx_get32x32var_avx2, 32);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 10);
}

unsigned int vpx_variance64x64_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 64, 64, sse, &sum,
                vpx_get32x32var_avx2, 32);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 12);
}

unsigned int vpx_variance64x32_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 64, 32, sse, &sum,
                vpx_get32x32var_avx2, 32);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 11);
}

unsigned int vpx_sub_pixel_variance32xh_avx2(const uint8_t *src, int src_stride,
                                             int x_offset, int y_offset,
                                             const uint8_t *dst, int dst_stride,
                                             int height, unsigned int *sse);

unsigned int vpx_sub_pixel_avg_variance32xh_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, const uint8_t *sec, int sec_stride,
    int height, unsigned int *sseptr);

unsigned int vpx_sub_pixel_variance64x64_avx2(const uint8_t *src,
                                              int src_stride, int x_offset,
                                              int y_offset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  unsigned int sse1;
  const int se1 = vpx_sub_pixel_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, 64, &sse1);
  unsigned int sse2;
  const int se2 =
      vpx_sub_pixel_variance32xh_avx2(src + 32, src_stride, x_offset, y_offset,
                                      dst + 32, dst_stride, 64, &sse2);
  const int se = se1 + se2;
  *sse = sse1 + sse2;
  return *sse - (uint32_t)(((int64_t)se * se) >> 12);
}

unsigned int vpx_sub_pixel_variance32x32_avx2(const uint8_t *src,
                                              int src_stride, int x_offset,
                                              int y_offset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  const int se = vpx_sub_pixel_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, 32, sse);
  return *sse - (uint32_t)(((int64_t)se * se) >> 10);
}

unsigned int vpx_sub_pixel_avg_variance64x64_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, unsigned int *sse, const uint8_t *sec) {
  unsigned int sse1;
  const int se1 = vpx_sub_pixel_avg_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, sec, 64, 64, &sse1);
  unsigned int sse2;
  const int se2 = vpx_sub_pixel_avg_variance32xh_avx2(
      src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride, sec + 32,
      64, 64, &sse2);
  const int se = se1 + se2;

  *sse = sse1 + sse2;

  return *sse - (uint32_t)(((int64_t)se * se) >> 12);
}

unsigned int vpx_sub_pixel_avg_variance32x32_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, unsigned int *sse, const uint8_t *sec) {
  // Process 32 elements in parallel.
  const int se = vpx_sub_pixel_avg_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, sec, 32, 32, sse);
  return *sse - (uint32_t)(((int64_t)se * se) >> 10);
}
