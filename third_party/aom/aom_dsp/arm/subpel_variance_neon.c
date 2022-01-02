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

#include <arm_neon.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"

#include "aom_ports/mem.h"
#include "aom/aom_integer.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/variance.h"

static void var_filter_block2d_bil_w8(const uint8_t *src_ptr,
                                      uint8_t *output_ptr,
                                      unsigned int src_pixels_per_line,
                                      int pixel_step,
                                      unsigned int output_height,
                                      unsigned int output_width,
                                      const uint8_t *filter) {
  const uint8x8_t f0 = vmov_n_u8(filter[0]);
  const uint8x8_t f1 = vmov_n_u8(filter[1]);
  unsigned int i;
  for (i = 0; i < output_height; ++i) {
    const uint8x8_t src_0 = vld1_u8(&src_ptr[0]);
    const uint8x8_t src_1 = vld1_u8(&src_ptr[pixel_step]);
    const uint16x8_t a = vmull_u8(src_0, f0);
    const uint16x8_t b = vmlal_u8(a, src_1, f1);
    const uint8x8_t out = vrshrn_n_u16(b, FILTER_BITS);
    vst1_u8(&output_ptr[0], out);
    // Next row...
    src_ptr += src_pixels_per_line;
    output_ptr += output_width;
  }
}

static void var_filter_block2d_bil_w16(const uint8_t *src_ptr,
                                       uint8_t *output_ptr,
                                       unsigned int src_pixels_per_line,
                                       int pixel_step,
                                       unsigned int output_height,
                                       unsigned int output_width,
                                       const uint8_t *filter) {
  const uint8x8_t f0 = vmov_n_u8(filter[0]);
  const uint8x8_t f1 = vmov_n_u8(filter[1]);
  unsigned int i, j;
  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; j += 16) {
      const uint8x16_t src_0 = vld1q_u8(&src_ptr[j]);
      const uint8x16_t src_1 = vld1q_u8(&src_ptr[j + pixel_step]);
      const uint16x8_t a = vmull_u8(vget_low_u8(src_0), f0);
      const uint16x8_t b = vmlal_u8(a, vget_low_u8(src_1), f1);
      const uint8x8_t out_lo = vrshrn_n_u16(b, FILTER_BITS);
      const uint16x8_t c = vmull_u8(vget_high_u8(src_0), f0);
      const uint16x8_t d = vmlal_u8(c, vget_high_u8(src_1), f1);
      const uint8x8_t out_hi = vrshrn_n_u16(d, FILTER_BITS);
      vst1q_u8(&output_ptr[j], vcombine_u8(out_lo, out_hi));
    }
    // Next row...
    src_ptr += src_pixels_per_line;
    output_ptr += output_width;
  }
}

unsigned int aom_sub_pixel_variance8x8_neon(const uint8_t *src, int src_stride,
                                            int xoffset, int yoffset,
                                            const uint8_t *dst, int dst_stride,
                                            unsigned int *sse) {
  DECLARE_ALIGNED(16, uint8_t, temp2[8 * 8]);
  DECLARE_ALIGNED(16, uint8_t, fdata3[9 * 8]);

  var_filter_block2d_bil_w8(src, fdata3, src_stride, 1, 9, 8,
                            bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w8(fdata3, temp2, 8, 8, 8, 8,
                            bilinear_filters_2t[yoffset]);
  return aom_variance8x8_neon(temp2, 8, dst, dst_stride, sse);
}

unsigned int aom_sub_pixel_variance16x16_neon(const uint8_t *src,
                                              int src_stride, int xoffset,
                                              int yoffset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  DECLARE_ALIGNED(16, uint8_t, temp2[16 * 16]);
  DECLARE_ALIGNED(16, uint8_t, fdata3[17 * 16]);

  var_filter_block2d_bil_w16(src, fdata3, src_stride, 1, 17, 16,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(fdata3, temp2, 16, 16, 16, 16,
                             bilinear_filters_2t[yoffset]);
  return aom_variance16x16_neon(temp2, 16, dst, dst_stride, sse);
}

unsigned int aom_sub_pixel_variance32x32_neon(const uint8_t *src,
                                              int src_stride, int xoffset,
                                              int yoffset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  DECLARE_ALIGNED(16, uint8_t, temp2[32 * 32]);
  DECLARE_ALIGNED(16, uint8_t, fdata3[33 * 32]);

  var_filter_block2d_bil_w16(src, fdata3, src_stride, 1, 33, 32,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(fdata3, temp2, 32, 32, 32, 32,
                             bilinear_filters_2t[yoffset]);
  return aom_variance32x32_neon(temp2, 32, dst, dst_stride, sse);
}

unsigned int aom_sub_pixel_variance64x64_neon(const uint8_t *src,
                                              int src_stride, int xoffset,
                                              int yoffset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  DECLARE_ALIGNED(16, uint8_t, temp2[64 * 64]);
  DECLARE_ALIGNED(16, uint8_t, fdata3[65 * 64]);

  var_filter_block2d_bil_w16(src, fdata3, src_stride, 1, 65, 64,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(fdata3, temp2, 64, 64, 64, 64,
                             bilinear_filters_2t[yoffset]);
  return aom_variance64x64_neon(temp2, 64, dst, dst_stride, sse);
}
