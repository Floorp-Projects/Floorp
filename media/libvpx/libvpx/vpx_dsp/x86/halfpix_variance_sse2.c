/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"

void vpx_half_horiz_vert_variance16x_h_sse2(const unsigned char *ref,
                                            int ref_stride,
                                            const unsigned char *src,
                                            int src_stride,
                                            unsigned int height,
                                            int *sum,
                                            unsigned int *sumsquared);
void vpx_half_horiz_variance16x_h_sse2(const unsigned char *ref, int ref_stride,
                                       const unsigned char *src, int src_stride,
                                       unsigned int height, int *sum,
                                       unsigned int *sumsquared);
void vpx_half_vert_variance16x_h_sse2(const unsigned char *ref, int ref_stride,
                                      const unsigned char *src, int src_stride,
                                      unsigned int height, int *sum,
                                      unsigned int *sumsquared);

uint32_t vpx_variance_halfpixvar16x16_h_sse2(const unsigned char *src,
                                             int src_stride,
                                             const unsigned char *dst,
                                             int dst_stride,
                                             uint32_t *sse) {
  int xsum0;
  unsigned int xxsum0;

  vpx_half_horiz_variance16x_h_sse2(src, src_stride, dst, dst_stride, 16,
                                    &xsum0, &xxsum0);

  *sse = xxsum0;
  assert(xsum0 <= 255 * 16 * 16);
  assert(xsum0 >= -255 * 16 * 16);
  return (xxsum0 - ((uint32_t)((int64_t)xsum0 * xsum0) >> 8));
}

uint32_t vpx_variance_halfpixvar16x16_v_sse2(const unsigned char *src,
                                             int src_stride,
                                             const unsigned char *dst,
                                             int dst_stride,
                                             uint32_t *sse) {
  int xsum0;
  unsigned int xxsum0;
  vpx_half_vert_variance16x_h_sse2(src, src_stride, dst, dst_stride, 16,
                                   &xsum0, &xxsum0);

  *sse = xxsum0;
  assert(xsum0 <= 255 * 16 * 16);
  assert(xsum0 >= -255 * 16 * 16);
  return (xxsum0 - ((uint32_t)((int64_t)xsum0 * xsum0) >> 8));
}


uint32_t vpx_variance_halfpixvar16x16_hv_sse2(const unsigned char *src,
                                              int src_stride,
                                              const unsigned char *dst,
                                              int dst_stride,
                                              uint32_t *sse) {
  int xsum0;
  unsigned int xxsum0;

  vpx_half_horiz_vert_variance16x_h_sse2(src, src_stride, dst, dst_stride, 16,
                                         &xsum0, &xxsum0);

  *sse = xxsum0;
  assert(xsum0 <= 255 * 16 * 16);
  assert(xsum0 >= -255 * 16 * 16);
  return (xxsum0 - ((uint32_t)((int64_t)xsum0 * xsum0) >> 8));
}
