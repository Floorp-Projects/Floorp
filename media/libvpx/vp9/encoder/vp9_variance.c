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

#include "vpx_ports/mem.h"
#include "vpx/vpx_integer.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_filter.h"

#include "vp9/encoder/vp9_variance.h"

void variance(const uint8_t *a, int  a_stride,
              const uint8_t *b, int  b_stride,
              int  w, int  h, unsigned int *sse, int *sum) {
  int i, j;

  *sum = 0;
  *sse = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      const int diff = a[j] - b[j];
      *sum += diff;
      *sse += diff * diff;
    }

    a += a_stride;
    b += b_stride;
  }
}

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
                                              const int16_t *vp9_filter) {
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
                                               const int16_t *vp9_filter) {
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

unsigned int vp9_get_mb_ss_c(const int16_t *src_ptr) {
  unsigned int i, sum = 0;

  for (i = 0; i < 256; ++i) {
    sum += src_ptr[i] * src_ptr[i];
  }

  return sum;
}

#define VAR(W, H) \
unsigned int vp9_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                       const uint8_t *b, int b_stride, \
                                       unsigned int *sse) { \
  int sum; \
  variance(a, a_stride, b, b_stride, W, H, sse, &sum); \
  return *sse - (((int64_t)sum * sum) / (W * H)); \
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
                                    BILINEAR_FILTERS_2TAP(xoffset)); \
  var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                     BILINEAR_FILTERS_2TAP(yoffset)); \
\
  return vp9_variance##W##x##H##_c(temp2, W, dst, dst_stride, sse); \
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
  DECLARE_ALIGNED_ARRAY(16, uint8_t, temp3, H * W); \
\
  var_filter_block2d_bil_first_pass(src, fdata3, src_stride, 1, H + 1, W, \
                                    BILINEAR_FILTERS_2TAP(xoffset)); \
  var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W, \
                                     BILINEAR_FILTERS_2TAP(yoffset)); \
\
  vp9_comp_avg_pred(temp3, second_pred, W, H, temp2, W); \
\
  return vp9_variance##W##x##H##_c(temp3, W, dst, dst_stride, sse); \
}

void vp9_get16x16var_c(const uint8_t *src_ptr, int source_stride,
                       const uint8_t *ref_ptr, int ref_stride,
                       unsigned int *sse, int *sum) {
  variance(src_ptr, source_stride, ref_ptr, ref_stride, 16, 16, sse, sum);
}

void vp9_get8x8var_c(const uint8_t *src_ptr, int source_stride,
                     const uint8_t *ref_ptr, int ref_stride,
                     unsigned int *sse, int *sum) {
  variance(src_ptr, source_stride, ref_ptr, ref_stride, 8, 8, sse, sum);
}

unsigned int vp9_mse16x16_c(const uint8_t *src, int src_stride,
                            const uint8_t *ref, int ref_stride,
                            unsigned int *sse) {
  int sum;
  variance(src, src_stride, ref, ref_stride, 16, 16, sse, &sum);
  return *sse;
}

unsigned int vp9_mse16x8_c(const uint8_t *src, int src_stride,
                           const uint8_t *ref, int ref_stride,
                           unsigned int *sse) {
  int sum;
  variance(src, src_stride, ref, ref_stride, 16, 8, sse, &sum);
  return *sse;
}

unsigned int vp9_mse8x16_c(const uint8_t *src, int src_stride,
                           const uint8_t *ref, int ref_stride,
                           unsigned int *sse) {
  int sum;
  variance(src, src_stride, ref, ref_stride, 8, 16, sse, &sum);
  return *sse;
}

unsigned int vp9_mse8x8_c(const uint8_t *src, int src_stride,
                          const uint8_t *ref, int ref_stride,
                          unsigned int *sse) {
  int sum;
  variance(src, src_stride, ref, ref_stride, 8, 8, sse, &sum);
  return *sse;
}

VAR(4, 4)
SUBPIX_VAR(4, 4)
SUBPIX_AVG_VAR(4, 4)

VAR(4, 8)
SUBPIX_VAR(4, 8)
SUBPIX_AVG_VAR(4, 8)

VAR(8, 4)
SUBPIX_VAR(8, 4)
SUBPIX_AVG_VAR(8, 4)

VAR(8, 8)
SUBPIX_VAR(8, 8)
SUBPIX_AVG_VAR(8, 8)

VAR(8, 16)
SUBPIX_VAR(8, 16)
SUBPIX_AVG_VAR(8, 16)

VAR(16, 8)
SUBPIX_VAR(16, 8)
SUBPIX_AVG_VAR(16, 8)

VAR(16, 16)
SUBPIX_VAR(16, 16)
SUBPIX_AVG_VAR(16, 16)

VAR(16, 32)
SUBPIX_VAR(16, 32)
SUBPIX_AVG_VAR(16, 32)

VAR(32, 16)
SUBPIX_VAR(32, 16)
SUBPIX_AVG_VAR(32, 16)

VAR(32, 32)
SUBPIX_VAR(32, 32)
SUBPIX_AVG_VAR(32, 32)

VAR(32, 64)
SUBPIX_VAR(32, 64)
SUBPIX_AVG_VAR(32, 64)

VAR(64, 32)
SUBPIX_VAR(64, 32)
SUBPIX_AVG_VAR(64, 32)

VAR(64, 64)
SUBPIX_VAR(64, 64)
SUBPIX_AVG_VAR(64, 64)

void vp9_comp_avg_pred(uint8_t *comp_pred, const uint8_t *pred, int width,
                       int height, const uint8_t *ref, int ref_stride) {
  int i, j;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      const int tmp = pred[j] + ref[j];
      comp_pred[j] = ROUND_POWER_OF_TWO(tmp, 1);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}
