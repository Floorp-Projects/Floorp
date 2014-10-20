/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include "./vp9_rtcd.h"

#include "vpx_ports/mem.h"
#include "vpx/vpx_integer.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_filter.h"

#include "vp9/encoder/vp9_variance.h"

enum { kWidth8 = 8 };
enum { kHeight8 = 8 };
enum { kHeight8PlusOne = 9 };
enum { kWidth16 = 16 };
enum { kHeight16 = 16 };
enum { kHeight16PlusOne = 17 };
enum { kWidth32 = 32 };
enum { kHeight32 = 32 };
enum { kHeight32PlusOne = 33 };
enum { kPixelStepOne = 1 };
enum { kAlign16 = 16 };

static INLINE int horizontal_add_s16x8(const int16x8_t v_16x8) {
  const int32x4_t a = vpaddlq_s16(v_16x8);
  const int64x2_t b = vpaddlq_s32(a);
  const int32x2_t c = vadd_s32(vreinterpret_s32_s64(vget_low_s64(b)),
                               vreinterpret_s32_s64(vget_high_s64(b)));
  return vget_lane_s32(c, 0);
}

static INLINE int horizontal_add_s32x4(const int32x4_t v_32x4) {
  const int64x2_t b = vpaddlq_s32(v_32x4);
  const int32x2_t c = vadd_s32(vreinterpret_s32_s64(vget_low_s64(b)),
                               vreinterpret_s32_s64(vget_high_s64(b)));
  return vget_lane_s32(c, 0);
}

static void variance_neon_w8(const uint8_t *a, int a_stride,
                             const uint8_t *b, int b_stride,
                             int w, int h, unsigned int *sse, int *sum) {
  int i, j;
  int16x8_t v_sum = vdupq_n_s16(0);
  int32x4_t v_sse_lo = vdupq_n_s32(0);
  int32x4_t v_sse_hi = vdupq_n_s32(0);

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 8) {
      const uint8x8_t v_a = vld1_u8(&a[j]);
      const uint8x8_t v_b = vld1_u8(&b[j]);
      const uint16x8_t v_diff = vsubl_u8(v_a, v_b);
      const int16x8_t sv_diff = vreinterpretq_s16_u16(v_diff);
      v_sum = vaddq_s16(v_sum, sv_diff);
      v_sse_lo = vmlal_s16(v_sse_lo,
                           vget_low_s16(sv_diff),
                           vget_low_s16(sv_diff));
      v_sse_hi = vmlal_s16(v_sse_hi,
                           vget_high_s16(sv_diff),
                           vget_high_s16(sv_diff));
    }
    a += a_stride;
    b += b_stride;
  }

  *sum = horizontal_add_s16x8(v_sum);
  *sse = (unsigned int)horizontal_add_s32x4(vaddq_s32(v_sse_lo, v_sse_hi));
}

void vp9_get8x8var_neon(const uint8_t *src_ptr, int source_stride,
                        const uint8_t *ref_ptr, int ref_stride,
                        unsigned int *sse, int *sum) {
  variance_neon_w8(src_ptr, source_stride, ref_ptr, ref_stride, kWidth8,
                   kHeight8, sse, sum);
}

unsigned int vp9_variance8x8_neon(const uint8_t *a, int a_stride,
                                  const uint8_t *b, int b_stride,
                                  unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, kWidth8, kHeight8, sse, &sum);
  return *sse - (((int64_t)sum * sum) / (kWidth8 * kHeight8));
}

void vp9_get16x16var_neon(const uint8_t *src_ptr, int source_stride,
                          const uint8_t *ref_ptr, int ref_stride,
                          unsigned int *sse, int *sum) {
  variance_neon_w8(src_ptr, source_stride, ref_ptr, ref_stride, kWidth16,
                   kHeight16, sse, sum);
}

unsigned int vp9_variance16x16_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, kWidth16, kHeight16, sse, &sum);
  return *sse - (((int64_t)sum * sum) / (kWidth16 * kHeight16));
}

static void var_filter_block2d_bil_w8(const uint8_t *src_ptr,
                                      uint8_t *output_ptr,
                                      unsigned int src_pixels_per_line,
                                      int pixel_step,
                                      unsigned int output_height,
                                      unsigned int output_width,
                                      const int16_t *vp9_filter) {
  const uint8x8_t f0 = vmov_n_u8((uint8_t)vp9_filter[0]);
  const uint8x8_t f1 = vmov_n_u8((uint8_t)vp9_filter[1]);
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
                                       const int16_t *vp9_filter) {
  const uint8x8_t f0 = vmov_n_u8((uint8_t)vp9_filter[0]);
  const uint8x8_t f1 = vmov_n_u8((uint8_t)vp9_filter[1]);
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

unsigned int vp9_sub_pixel_variance8x8_neon(const uint8_t *src,
                                            int src_stride,
                                            int xoffset,
                                            int yoffset,
                                            const uint8_t *dst,
                                            int dst_stride,
                                            unsigned int *sse) {
  DECLARE_ALIGNED_ARRAY(kAlign16, uint8_t, temp2, kHeight8 * kWidth8);
  DECLARE_ALIGNED_ARRAY(kAlign16, uint8_t, fdata3, kHeight8PlusOne * kWidth8);

  var_filter_block2d_bil_w8(src, fdata3, src_stride, kPixelStepOne,
                            kHeight8PlusOne, kWidth8,
                            BILINEAR_FILTERS_2TAP(xoffset));
  var_filter_block2d_bil_w8(fdata3, temp2, kWidth8, kWidth8, kHeight8,
                            kWidth8, BILINEAR_FILTERS_2TAP(yoffset));
  return vp9_variance8x8_neon(temp2, kWidth8, dst, dst_stride, sse);
}

unsigned int vp9_sub_pixel_variance16x16_neon(const uint8_t *src,
                                              int src_stride,
                                              int xoffset,
                                              int yoffset,
                                              const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  DECLARE_ALIGNED_ARRAY(kAlign16, uint8_t, temp2, kHeight16 * kWidth16);
  DECLARE_ALIGNED_ARRAY(kAlign16, uint8_t, fdata3, kHeight16PlusOne * kWidth16);

  var_filter_block2d_bil_w16(src, fdata3, src_stride, kPixelStepOne,
                             kHeight16PlusOne, kWidth16,
                             BILINEAR_FILTERS_2TAP(xoffset));
  var_filter_block2d_bil_w16(fdata3, temp2, kWidth16, kWidth16, kHeight16,
                             kWidth16, BILINEAR_FILTERS_2TAP(yoffset));
  return vp9_variance16x16_neon(temp2, kWidth16, dst, dst_stride, sse);
}

void vp9_get32x32var_neon(const uint8_t *src_ptr, int source_stride,
                          const uint8_t *ref_ptr, int ref_stride,
                          unsigned int *sse, int *sum) {
  variance_neon_w8(src_ptr, source_stride, ref_ptr, ref_stride, kWidth32,
                   kHeight32, sse, sum);
}

unsigned int vp9_variance32x32_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, kWidth32, kHeight32, sse, &sum);
  return *sse - (((int64_t)sum * sum) / (kWidth32 * kHeight32));
}

unsigned int vp9_sub_pixel_variance32x32_neon(const uint8_t *src,
                                              int src_stride,
                                              int xoffset,
                                              int yoffset,
                                              const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  DECLARE_ALIGNED_ARRAY(kAlign16, uint8_t, temp2, kHeight32 * kWidth32);
  DECLARE_ALIGNED_ARRAY(kAlign16, uint8_t, fdata3, kHeight32PlusOne * kWidth32);

  var_filter_block2d_bil_w16(src, fdata3, src_stride, kPixelStepOne,
                             kHeight32PlusOne, kWidth32,
                             BILINEAR_FILTERS_2TAP(xoffset));
  var_filter_block2d_bil_w16(fdata3, temp2, kWidth32, kWidth32, kHeight32,
                             kWidth32, BILINEAR_FILTERS_2TAP(yoffset));
  return vp9_variance32x32_neon(temp2, kWidth32, dst, dst_stride, sse);
}
