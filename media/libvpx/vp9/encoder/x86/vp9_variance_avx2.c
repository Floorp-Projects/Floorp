/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vp9_rtcd.h"
#include "./vpx_config.h"

#include "vp9/encoder/vp9_variance.h"
#include "vpx_ports/mem.h"

unsigned int vp9_sub_pixel_variance32xh_avx2(const uint8_t *src, int src_stride,
                                             int x_offset, int y_offset,
                                             const uint8_t *dst, int dst_stride,
                                             int height,
                                             unsigned int *sse);

unsigned int vp9_sub_pixel_avg_variance32xh_avx2(const uint8_t *src,
                                                 int src_stride,
                                                 int x_offset,
                                                 int y_offset,
                                                 const uint8_t *dst,
                                                 int dst_stride,
                                                 const uint8_t *sec,
                                                 int sec_stride,
                                                 int height,
                                                 unsigned int *sseptr);

unsigned int vp9_sub_pixel_variance64x64_avx2(const uint8_t *src,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  unsigned int sse1;
  const int se1 = vp9_sub_pixel_variance32xh_avx2(src, src_stride, x_offset,
                                                  y_offset, dst, dst_stride,
                                                  64, &sse1);
  unsigned int sse2;
  const int se2 = vp9_sub_pixel_variance32xh_avx2(src + 32, src_stride,
                                                  x_offset, y_offset,
                                                  dst + 32, dst_stride,
                                                  64, &sse2);
  const int se = se1 + se2;
  *sse = sse1 + sse2;
  return *sse - (((int64_t)se * se) >> 12);
}

unsigned int vp9_sub_pixel_variance32x32_avx2(const uint8_t *src,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  const int se = vp9_sub_pixel_variance32xh_avx2(src, src_stride, x_offset,
                                                 y_offset, dst, dst_stride,
                                                 32, sse);
  return *sse - (((int64_t)se * se) >> 10);
}

unsigned int vp9_sub_pixel_avg_variance64x64_avx2(const uint8_t *src,
                                                  int src_stride,
                                                  int x_offset,
                                                  int y_offset,
                                                  const uint8_t *dst,
                                                  int dst_stride,
                                                  unsigned int *sse,
                                                  const uint8_t *sec) {
  unsigned int sse1;
  const int se1 = vp9_sub_pixel_avg_variance32xh_avx2(src, src_stride, x_offset,
                                                      y_offset, dst, dst_stride,
                                                      sec, 64, 64, &sse1);
  unsigned int sse2;
  const int se2 =
      vp9_sub_pixel_avg_variance32xh_avx2(src + 32, src_stride, x_offset,
                                          y_offset, dst + 32, dst_stride,
                                          sec + 32, 64, 64, &sse2);
  const int se = se1 + se2;

  *sse = sse1 + sse2;

  return *sse - (((int64_t)se * se) >> 12);
}

unsigned int vp9_sub_pixel_avg_variance32x32_avx2(const uint8_t *src,
                                                  int src_stride,
                                                  int x_offset,
                                                  int y_offset,
                                                  const uint8_t *dst,
                                                  int dst_stride,
                                                  unsigned int *sse,
                                                  const uint8_t *sec) {
  // processing 32 element in parallel
  const int se = vp9_sub_pixel_avg_variance32xh_avx2(src, src_stride, x_offset,
                                                     y_offset, dst, dst_stride,
                                                     sec, 32, 32, sse);
  return *sse - (((int64_t)se * se) >> 10);
}
