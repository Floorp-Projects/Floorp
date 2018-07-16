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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"

void aom_half_horiz_vert_variance16x_h_sse2(const unsigned char *ref,
                                            int ref_stride,
                                            const unsigned char *src,
                                            int src_stride, unsigned int height,
                                            int *sum, unsigned int *sumsquared);
void aom_half_horiz_variance16x_h_sse2(const unsigned char *ref, int ref_stride,
                                       const unsigned char *src, int src_stride,
                                       unsigned int height, int *sum,
                                       unsigned int *sumsquared);
void aom_half_vert_variance16x_h_sse2(const unsigned char *ref, int ref_stride,
                                      const unsigned char *src, int src_stride,
                                      unsigned int height, int *sum,
                                      unsigned int *sumsquared);

uint32_t aom_variance_halfpixvar16x16_h_sse2(const unsigned char *src,
                                             int src_stride,
                                             const unsigned char *dst,
                                             int dst_stride, uint32_t *sse) {
  int xsum0;
  unsigned int xxsum0;

  aom_half_horiz_variance16x_h_sse2(src, src_stride, dst, dst_stride, 16,
                                    &xsum0, &xxsum0);

  *sse = xxsum0;
  assert(xsum0 <= 255 * 16 * 16);
  assert(xsum0 >= -255 * 16 * 16);
  return (xxsum0 - ((uint32_t)((int64_t)xsum0 * xsum0) >> 8));
}

uint32_t aom_variance_halfpixvar16x16_v_sse2(const unsigned char *src,
                                             int src_stride,
                                             const unsigned char *dst,
                                             int dst_stride, uint32_t *sse) {
  int xsum0;
  unsigned int xxsum0;
  aom_half_vert_variance16x_h_sse2(src, src_stride, dst, dst_stride, 16, &xsum0,
                                   &xxsum0);

  *sse = xxsum0;
  assert(xsum0 <= 255 * 16 * 16);
  assert(xsum0 >= -255 * 16 * 16);
  return (xxsum0 - ((uint32_t)((int64_t)xsum0 * xsum0) >> 8));
}

uint32_t aom_variance_halfpixvar16x16_hv_sse2(const unsigned char *src,
                                              int src_stride,
                                              const unsigned char *dst,
                                              int dst_stride, uint32_t *sse) {
  int xsum0;
  unsigned int xxsum0;

  aom_half_horiz_vert_variance16x_h_sse2(src, src_stride, dst, dst_stride, 16,
                                         &xsum0, &xxsum0);

  *sse = xxsum0;
  assert(xsum0 <= 255 * 16 * 16);
  assert(xsum0 >= -255 * 16 * 16);
  return (xxsum0 - ((uint32_t)((int64_t)xsum0 * xsum0) >> 8));
}
