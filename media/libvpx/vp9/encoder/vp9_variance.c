/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vp9_rtcd.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx_ports/mem.h"
#include "vpx/vpx_integer.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_filter.h"

#include "vp9/encoder/vp9_variance.h"

static uint8_t bilinear_filters[8][2] = {
  { 128,   0, },
  { 112,  16, },
  {  96,  32, },
  {  80,  48, },
  {  64,  64, },
  {  48,  80, },
  {  32,  96, },
  {  16, 112, },
};

// Applies a 1-D 2-tap bi-linear filter to the source block in either horizontal
// or vertical direction to produce the filtered output block. Used to implement
// first-pass of 2-D separable filter.
//
// Produces int32_t output to retain precision for next pass. Two filter taps
// should sum to VP9_FILTER_WEIGHT. pixel_step defines whether the filter is
// applied horizontally (pixel_step=1) or vertically (pixel_step=stride). It
// defines the offset required to move from one input to the next.
static void var_filter_block2d_bil_first_pass(const uint8_t *src_ptr,
                                              uint16_t *output_ptr,
                                              unsigned int src_pixels_per_line,
                                              int pixel_step,
                                              unsigned int output_height,
                                              unsigned int output_width,
                                              const uint8_t *vp9_filter) {
  unsigned int i, j;

  for (i = 0; i < output_height; i++) {
    for (j = 0; j < output_width; j++) {
      output_ptr[j] = ROUND_POWER_OF_TWO((int)src_ptr[0] * vp9_filter[0] +
                          (int)src_ptr[pixel_step] * vp9_filter[1],
                          FILTER_BITS);

      src_ptr++;
    }

    // Next row...
    src_ptr    += src_pixels_per_line - output_width;
    output_ptr += output_width;
  }
}

// Applies a 1-D 2-tap bi-linear filter to the source block in either horizontal
// or vertical direction to produce the filtered output block. Used to implement
// second-pass of 2-D separable filter.
//
// Requires 32-bit input as produced by filter_block2d_bil_first_pass. Two
// filter taps should sum to VP9_FILTER_WEIGHT. pixel_step defines whether the
// filter is applied horizontally (pixel_step=1) or vertically (pixel_step=
// stride). It defines the offset required to move from one input to the next.
static void var_filter_block2d_bil_second_pass(const uint16_t *src_ptr,
                                               uint8_t *output_ptr,
                                               unsigned int src_pixels_per_line,
                                               unsigned int pixel_step,
                                               unsigned int output_height,
                                               unsigned int output_width,
                                               const uint8_t *vp9_filter) {
  unsigned int  i, j;

  for (i = 0; i < output_height; i++) {
    for (j = 0; j < output_width; j++) {
      output_ptr[j] = ROUND_POWER_OF_TWO((int)src_ptr[0] * vp9_filter[0] +
                          (int)src_ptr[pixel_step] * vp9_filter[1],
                          FILTER_BITS);
      src_ptr++;
    }

    src_ptr += src_pixels_per_line - output_width;
    output_ptr += output_width;
  }
}

#define SUBPIX_VAR(W, H) \
unsigned int vp9_sub_pixel_variance##W##x##H##_c( \
  const uint8_t *src, int  src_stride, \
  int xoffset, int  yoffset, \
  const uint8_t *dst, int dst_stride, \
  unsigned int *sse) { \
  uint16_t fdata3[(H + 1) * W]; \
  uint8_t temp2[H * W]; \
\
  var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, W, \
                                    bilinear_filters[xoffset]); \
  var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                     bilinear_filters[yoffset]); \
\
  return vpx_variance##W##x##H##_c(temp2, W, dst, dst_stride, sse); \
}

#define SUBPIX_AVG_VAR(W, H) \
unsigned int vp9_sub_pixel_avg_variance##W##x##H##_c( \
  const uint8_t *src, int  src_stride, \
  int xoffset, int  yoffset, \
  const uint8_t *dst, int dst_stride, \
  unsigned int *sse, \
  const uint8_t *second_pred) { \
  uint16_t fdata3[(H + 1) * W]; \
  uint8_t temp2[H * W]; \
  DECLARE_ALIGNED(16, uint8_t, temp3[H * W]); \
\
  var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, W, \
                                    bilinear_filters[xoffset]); \
  var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                     bilinear_filters[yoffset]); \
\
  vpx_comp_avg_pred(temp3, second_pred, W, H, temp2, W); \
\
  return vpx_variance##W##x##H##_c(temp3, W, dst, dst_stride, sse); \
}

SUBPIX_VAR(4, 4)
SUBPIX_AVG_VAR(4, 4)

SUBPIX_VAR(4, 8)
SUBPIX_AVG_VAR(4, 8)

SUBPIX_VAR(8, 4)
SUBPIX_AVG_VAR(8, 4)

SUBPIX_VAR(8, 8)
SUBPIX_AVG_VAR(8, 8)

SUBPIX_VAR(8, 16)
SUBPIX_AVG_VAR(8, 16)

SUBPIX_VAR(16, 8)
SUBPIX_AVG_VAR(16, 8)

SUBPIX_VAR(16, 16)
SUBPIX_AVG_VAR(16, 16)

SUBPIX_VAR(16, 32)
SUBPIX_AVG_VAR(16, 32)

SUBPIX_VAR(32, 16)
SUBPIX_AVG_VAR(32, 16)

SUBPIX_VAR(32, 32)
SUBPIX_AVG_VAR(32, 32)

SUBPIX_VAR(32, 64)
SUBPIX_AVG_VAR(32, 64)

SUBPIX_VAR(64, 32)
SUBPIX_AVG_VAR(64, 32)

SUBPIX_VAR(64, 64)
SUBPIX_AVG_VAR(64, 64)

#if CONFIG_VP9_HIGHBITDEPTH
static void highbd_var_filter_block2d_bil_first_pass(
    const uint8_t *src_ptr8,
    uint16_t *output_ptr,
    unsigned int src_pixels_per_line,
    int pixel_step,
    unsigned int output_height,
    unsigned int output_width,
    const uint8_t *vp9_filter) {
  unsigned int i, j;
  uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src_ptr8);
  for (i = 0; i < output_height; i++) {
    for (j = 0; j < output_width; j++) {
      output_ptr[j] =
          ROUND_POWER_OF_TWO((int)src_ptr[0] * vp9_filter[0] +
                             (int)src_ptr[pixel_step] * vp9_filter[1],
                             FILTER_BITS);

      src_ptr++;
    }

    // Next row...
    src_ptr += src_pixels_per_line - output_width;
    output_ptr += output_width;
  }
}

static void highbd_var_filter_block2d_bil_second_pass(
    const uint16_t *src_ptr,
    uint16_t *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int pixel_step,
    unsigned int output_height,
    unsigned int output_width,
    const uint8_t *vp9_filter) {
  unsigned int  i, j;

  for (i = 0; i < output_height; i++) {
    for (j = 0; j < output_width; j++) {
      output_ptr[j] =
          ROUND_POWER_OF_TWO((int)src_ptr[0] * vp9_filter[0] +
                             (int)src_ptr[pixel_step] * vp9_filter[1],
                             FILTER_BITS);
      src_ptr++;
    }

    src_ptr += src_pixels_per_line - output_width;
    output_ptr += output_width;
  }
}

#define HIGHBD_SUBPIX_VAR(W, H) \
unsigned int vp9_highbd_sub_pixel_variance##W##x##H##_c( \
  const uint8_t *src, int  src_stride, \
  int xoffset, int  yoffset, \
  const uint8_t *dst, int dst_stride, \
  unsigned int *sse) { \
  uint16_t fdata3[(H + 1) * W]; \
  uint16_t temp2[H * W]; \
\
  highbd_var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, \
                                           W, bilinear_filters[xoffset]); \
  highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                            bilinear_filters[yoffset]); \
\
  return vpx_highbd_8_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), W, dst, \
                                          dst_stride, sse); \
} \
\
unsigned int vp9_highbd_10_sub_pixel_variance##W##x##H##_c( \
  const uint8_t *src, int  src_stride, \
  int xoffset, int  yoffset, \
  const uint8_t *dst, int dst_stride, \
  unsigned int *sse) { \
  uint16_t fdata3[(H + 1) * W]; \
  uint16_t temp2[H * W]; \
\
  highbd_var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, \
                                           W, bilinear_filters[xoffset]); \
  highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                            bilinear_filters[yoffset]); \
\
  return vpx_highbd_10_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), \
                                             W, dst, dst_stride, sse); \
} \
\
unsigned int vp9_highbd_12_sub_pixel_variance##W##x##H##_c( \
  const uint8_t *src, int  src_stride, \
  int xoffset, int  yoffset, \
  const uint8_t *dst, int dst_stride, \
  unsigned int *sse) { \
  uint16_t fdata3[(H + 1) * W]; \
  uint16_t temp2[H * W]; \
\
  highbd_var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, \
                                           W, bilinear_filters[xoffset]); \
  highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                            bilinear_filters[yoffset]); \
\
  return vpx_highbd_12_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), \
                                             W, dst, dst_stride, sse); \
}

#define HIGHBD_SUBPIX_AVG_VAR(W, H) \
unsigned int vp9_highbd_sub_pixel_avg_variance##W##x##H##_c( \
  const uint8_t *src, int  src_stride, \
  int xoffset, int  yoffset, \
  const uint8_t *dst, int dst_stride, \
  unsigned int *sse, \
  const uint8_t *second_pred) { \
  uint16_t fdata3[(H + 1) * W]; \
  uint16_t temp2[H * W]; \
  DECLARE_ALIGNED(16, uint16_t, temp3[H * W]); \
\
  highbd_var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, \
                                           W, bilinear_filters[xoffset]); \
  highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                            bilinear_filters[yoffset]); \
\
  vpx_highbd_comp_avg_pred(temp3, second_pred, W, H, \
                           CONVERT_TO_BYTEPTR(temp2), W); \
\
  return vpx_highbd_8_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W, dst, \
                                          dst_stride, sse); \
} \
\
unsigned int vp9_highbd_10_sub_pixel_avg_variance##W##x##H##_c( \
  const uint8_t *src, int  src_stride, \
  int xoffset, int  yoffset, \
  const uint8_t *dst, int dst_stride, \
  unsigned int *sse, \
  const uint8_t *second_pred) { \
  uint16_t fdata3[(H + 1) * W]; \
  uint16_t temp2[H * W]; \
  DECLARE_ALIGNED(16, uint16_t, temp3[H * W]); \
\
  highbd_var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, \
                                           W, bilinear_filters[xoffset]); \
  highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                            bilinear_filters[yoffset]); \
\
  vpx_highbd_comp_avg_pred(temp3, second_pred, W, H, \
                           CONVERT_TO_BYTEPTR(temp2), W); \
\
  return vpx_highbd_10_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), \
                                             W, dst, dst_stride, sse); \
} \
\
unsigned int vp9_highbd_12_sub_pixel_avg_variance##W##x##H##_c( \
  const uint8_t *src, int  src_stride, \
  int xoffset, int  yoffset, \
  const uint8_t *dst, int dst_stride, \
  unsigned int *sse, \
  const uint8_t *second_pred) { \
  uint16_t fdata3[(H + 1) * W]; \
  uint16_t temp2[H * W]; \
  DECLARE_ALIGNED(16, uint16_t, temp3[H * W]); \
\
  highbd_var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, \
                                           W, bilinear_filters[xoffset]); \
  highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                            bilinear_filters[yoffset]); \
\
  vpx_highbd_comp_avg_pred(temp3, second_pred, W, H, \
                           CONVERT_TO_BYTEPTR(temp2), W); \
\
  return vpx_highbd_12_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), \
                                             W, dst, dst_stride, sse); \
}

HIGHBD_SUBPIX_VAR(4, 4)
HIGHBD_SUBPIX_AVG_VAR(4, 4)

HIGHBD_SUBPIX_VAR(4, 8)
HIGHBD_SUBPIX_AVG_VAR(4, 8)

HIGHBD_SUBPIX_VAR(8, 4)
HIGHBD_SUBPIX_AVG_VAR(8, 4)

HIGHBD_SUBPIX_VAR(8, 8)
HIGHBD_SUBPIX_AVG_VAR(8, 8)

HIGHBD_SUBPIX_VAR(8, 16)
HIGHBD_SUBPIX_AVG_VAR(8, 16)

HIGHBD_SUBPIX_VAR(16, 8)
HIGHBD_SUBPIX_AVG_VAR(16, 8)

HIGHBD_SUBPIX_VAR(16, 16)
HIGHBD_SUBPIX_AVG_VAR(16, 16)

HIGHBD_SUBPIX_VAR(16, 32)
HIGHBD_SUBPIX_AVG_VAR(16, 32)

HIGHBD_SUBPIX_VAR(32, 16)
HIGHBD_SUBPIX_AVG_VAR(32, 16)

HIGHBD_SUBPIX_VAR(32, 32)
HIGHBD_SUBPIX_AVG_VAR(32, 32)

HIGHBD_SUBPIX_VAR(32, 64)
HIGHBD_SUBPIX_AVG_VAR(32, 64)

HIGHBD_SUBPIX_VAR(64, 32)
HIGHBD_SUBPIX_AVG_VAR(64, 32)

HIGHBD_SUBPIX_VAR(64, 64)
HIGHBD_SUBPIX_AVG_VAR(64, 64)
#endif  // CONFIG_VP9_HIGHBITDEPTH
