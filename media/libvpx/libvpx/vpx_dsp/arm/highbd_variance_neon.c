/*
 *  Copyright (c) 2022 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "./vpx_dsp_rtcd.h"
#include "./vpx_config.h"

#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/sum_neon.h"
#include "vpx_ports/mem.h"

static const uint8_t bilinear_filters[8][2] = {
  { 128, 0 }, { 112, 16 }, { 96, 32 }, { 80, 48 },
  { 64, 64 }, { 48, 80 },  { 32, 96 }, { 16, 112 },
};

static INLINE void highbd_variance16(const uint16_t *src_ptr, int src_stride,
                                     const uint16_t *ref_ptr, int ref_stride,
                                     int w, int h, uint64_t *sse,
                                     int64_t *sum) {
  int i, j;

  if (w >= 8) {
    int32x4_t sum_s32 = vdupq_n_s32(0);
    uint32x4_t sse_u32 = vdupq_n_u32(0);
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        const int16x8_t src_s16 = vreinterpretq_s16_u16(vld1q_u16(&src_ptr[j]));
        const int16x8_t ref_s16 = vreinterpretq_s16_u16(vld1q_u16(&ref_ptr[j]));
        const int32x4_t diff1_s32 =
            vsubl_s16(vget_low_s16(src_s16), vget_low_s16(ref_s16));
        const int32x4_t diff2_s32 =
            vsubl_s16(vget_high_s16(src_s16), vget_high_s16(ref_s16));
        const uint32x4_t diff1_u32 = vreinterpretq_u32_s32(diff1_s32);
        const uint32x4_t diff2_u32 = vreinterpretq_u32_s32(diff2_s32);
        sum_s32 = vaddq_s32(sum_s32, diff1_s32);
        sum_s32 = vaddq_s32(sum_s32, diff2_s32);
        sse_u32 = vmlaq_u32(sse_u32, diff1_u32, diff1_u32);
        sse_u32 = vmlaq_u32(sse_u32, diff2_u32, diff2_u32);
      }
      src_ptr += src_stride;
      ref_ptr += ref_stride;
    }
    *sum = horizontal_add_int32x4(sum_s32);
    *sse = horizontal_add_uint32x4(sse_u32);
  } else {
    int32x4_t sum_s32 = vdupq_n_s32(0);
    uint32x4_t sse_u32 = vdupq_n_u32(0);
    assert(w >= 4);
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 4) {
        const int16x4_t src_s16 = vreinterpret_s16_u16(vld1_u16(&src_ptr[j]));
        const int16x4_t ref_s16 = vreinterpret_s16_u16(vld1_u16(&ref_ptr[j]));
        const int32x4_t diff_s32 = vsubl_s16(src_s16, ref_s16);
        const uint32x4_t diff_u32 = vreinterpretq_u32_s32(diff_s32);
        sum_s32 = vaddq_s32(sum_s32, diff_s32);
        sse_u32 = vmlaq_u32(sse_u32, diff_u32, diff_u32);
      }
      src_ptr += src_stride;
      ref_ptr += ref_stride;
    }
    *sum = horizontal_add_int32x4(sum_s32);
    *sse = horizontal_add_uint32x4(sse_u32);
  }
}

static INLINE void highbd_variance64(const uint8_t *src8_ptr, int src_stride,
                                     const uint8_t *ref8_ptr, int ref_stride,
                                     int w, int h, uint64_t *sse,
                                     int64_t *sum) {
  uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src8_ptr);
  uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(ref8_ptr);

  if (w < 32 && h < 32) {
    highbd_variance16(src_ptr, src_stride, ref_ptr, ref_stride, w, h, sse, sum);
  } else {
    uint64_t sse_long = 0;
    int64_t sum_long = 0;
    int k, l;
    for (k = 0; k + 16 <= h; k += 16) {
      for (l = 0; l + 16 <= w; l += 16) {
        uint64_t sse_tmp = 0;
        int64_t sum_tmp = 0;
        highbd_variance16(src_ptr + l, src_stride, ref_ptr + l, ref_stride, 16,
                          16, &sse_tmp, &sum_tmp);
        sum_long += sum_tmp;
        sse_long += sse_tmp;
      }
      src_ptr += 16 * src_stride;
      ref_ptr += 16 * ref_stride;
    }
    *sum = sum_long;
    *sse = sse_long;
  }
}

static INLINE void highbd_8_variance(const uint8_t *src8_ptr, int src_stride,
                                     const uint8_t *ref8_ptr, int ref_stride,
                                     int w, int h, uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(src8_ptr, src_stride, ref8_ptr, ref_stride, w, h, &sse_long,
                    &sum_long);
  *sse = (uint32_t)sse_long;
  *sum = (int)sum_long;
}

static INLINE void highbd_10_variance(const uint8_t *src8_ptr, int src_stride,
                                      const uint8_t *ref8_ptr, int ref_stride,
                                      int w, int h, uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(src8_ptr, src_stride, ref8_ptr, ref_stride, w, h, &sse_long,
                    &sum_long);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
  *sum = (int)ROUND_POWER_OF_TWO(sum_long, 2);
}

static INLINE void highbd_12_variance(const uint8_t *src8_ptr, int src_stride,
                                      const uint8_t *ref8_ptr, int ref_stride,
                                      int w, int h, uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(src8_ptr, src_stride, ref8_ptr, ref_stride, w, h, &sse_long,
                    &sum_long);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 8);
  *sum = (int)ROUND_POWER_OF_TWO(sum_long, 4);
}

#define HIGHBD_VAR(W, H)                                                    \
  uint32_t vpx_highbd_8_variance##W##x##H##_neon(                           \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse) {                                      \
    int sum;                                                                \
    highbd_8_variance(src_ptr, src_stride, ref_ptr, ref_stride, W, H, sse,  \
                      &sum);                                                \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));               \
  }                                                                         \
                                                                            \
  uint32_t vpx_highbd_10_variance##W##x##H##_neon(                          \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse) {                                      \
    int sum;                                                                \
    int64_t var;                                                            \
    highbd_10_variance(src_ptr, src_stride, ref_ptr, ref_stride, W, H, sse, \
                       &sum);                                               \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));               \
    return (var >= 0) ? (uint32_t)var : 0;                                  \
  }                                                                         \
                                                                            \
  uint32_t vpx_highbd_12_variance##W##x##H##_neon(                          \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse) {                                      \
    int sum;                                                                \
    int64_t var;                                                            \
    highbd_12_variance(src_ptr, src_stride, ref_ptr, ref_stride, W, H, sse, \
                       &sum);                                               \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));               \
    return (var >= 0) ? (uint32_t)var : 0;                                  \
  }

#define HIGHBD_GET_VAR(S)                                                   \
  void vpx_highbd_8_get##S##x##S##var_neon(                                 \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse, int *sum) {                            \
    highbd_8_variance(src_ptr, src_stride, ref_ptr, ref_stride, S, S, sse,  \
                      sum);                                                 \
  }                                                                         \
                                                                            \
  void vpx_highbd_10_get##S##x##S##var_neon(                                \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse, int *sum) {                            \
    highbd_10_variance(src_ptr, src_stride, ref_ptr, ref_stride, S, S, sse, \
                       sum);                                                \
  }                                                                         \
                                                                            \
  void vpx_highbd_12_get##S##x##S##var_neon(                                \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse, int *sum) {                            \
    highbd_12_variance(src_ptr, src_stride, ref_ptr, ref_stride, S, S, sse, \
                       sum);                                                \
  }

#define HIGHBD_MSE(W, H)                                                    \
  uint32_t vpx_highbd_8_mse##W##x##H##_neon(                                \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse) {                                      \
    int sum;                                                                \
    highbd_8_variance(src_ptr, src_stride, ref_ptr, ref_stride, W, H, sse,  \
                      &sum);                                                \
    return *sse;                                                            \
  }                                                                         \
                                                                            \
  uint32_t vpx_highbd_10_mse##W##x##H##_neon(                               \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse) {                                      \
    int sum;                                                                \
    highbd_10_variance(src_ptr, src_stride, ref_ptr, ref_stride, W, H, sse, \
                       &sum);                                               \
    return *sse;                                                            \
  }                                                                         \
                                                                            \
  uint32_t vpx_highbd_12_mse##W##x##H##_neon(                               \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,       \
      int ref_stride, uint32_t *sse) {                                      \
    int sum;                                                                \
    highbd_12_variance(src_ptr, src_stride, ref_ptr, ref_stride, W, H, sse, \
                       &sum);                                               \
    return *sse;                                                            \
  }

static INLINE void highbd_var_filter_block2d_bil_first_pass(
    const uint8_t *src_ptr8, uint16_t *output_ptr,
    unsigned int src_pixels_per_line, int pixel_step,
    unsigned int output_height, unsigned int output_width,
    const uint8_t *filter) {
  uint32_t i, j;
  uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src_ptr8);

  uint32x4_t round_u32 = vshlq_n_u32(vdupq_n_u32(1), FILTER_BITS - 1);
  uint16x4_t filter1_u16 = vdup_n_u16(filter[0]);
  uint16x4_t filter2_u16 = vdup_n_u16(filter[1]);

  if (output_width >= 8) {
    for (i = 0; i < output_height; ++i) {
      for (j = 0; j < output_width; j += 8) {
        const uint16x8_t src1_u16 = vld1q_u16(&src_ptr[j]);
        const uint16x8_t src2_u16 = vld1q_u16(&src_ptr[j + pixel_step]);
        uint32x4_t sum1_u32 = vmull_u16(filter1_u16, vget_low_u16(src1_u16));
        uint32x4_t sum2_u32 = vmull_u16(filter1_u16, vget_high_u16(src1_u16));
        uint16x4_t out1_u16;
        uint16x4_t out2_u16;
        sum1_u32 = vmlal_u16(sum1_u32, filter2_u16, vget_low_u16(src2_u16));
        sum2_u32 = vmlal_u16(sum2_u32, filter2_u16, vget_high_u16(src2_u16));
        out1_u16 = vshrn_n_u32(vaddq_u32(sum1_u32, round_u32), FILTER_BITS);
        out2_u16 = vshrn_n_u32(vaddq_u32(sum2_u32, round_u32), FILTER_BITS);
        vst1q_u16(&output_ptr[j], vcombine_u16(out1_u16, out2_u16));
      }
      // Next row...
      src_ptr += src_pixels_per_line;
      output_ptr += output_width;
    }
  } else {
    assert(output_width >= 4);
    for (i = 0; i < output_height; ++i) {
      for (j = 0; j < output_width; j += 4) {
        const uint16x4_t src1_u16 = vld1_u16(&src_ptr[j]);
        const uint16x4_t src2_u16 = vld1_u16(&src_ptr[j + pixel_step]);
        uint32x4_t sum_u32 = vmull_u16(filter1_u16, src1_u16);
        uint16x4_t out_u16;
        sum_u32 = vmlal_u16(sum_u32, filter2_u16, src2_u16);
        out_u16 = vshrn_n_u32(vaddq_u32(sum_u32, round_u32), FILTER_BITS);
        vst1_u16(&output_ptr[j], out_u16);
      }
      // Next row...
      src_ptr += src_pixels_per_line;
      output_ptr += output_width;
    }
  }
}

static INLINE void highbd_var_filter_block2d_bil_second_pass(
    const uint16_t *src_ptr, uint16_t *output_ptr,
    unsigned int src_pixels_per_line, unsigned int pixel_step,
    unsigned int output_height, unsigned int output_width,
    const uint8_t *filter) {
  uint32_t i, j;

  uint32x4_t round_u32 = vshlq_n_u32(vdupq_n_u32(1), FILTER_BITS - 1);
  uint16x4_t filter1_u16 = vdup_n_u16(filter[0]);
  uint16x4_t filter2_u16 = vdup_n_u16(filter[1]);

  if (output_width >= 8) {
    for (i = 0; i < output_height; ++i) {
      for (j = 0; j < output_width; j += 8) {
        const uint16x8_t src1_u16 = vld1q_u16(&src_ptr[j]);
        const uint16x8_t src2_u16 = vld1q_u16(&src_ptr[j + pixel_step]);
        uint32x4_t sum1_u32 = vmull_u16(filter1_u16, vget_low_u16(src1_u16));
        uint32x4_t sum2_u32 = vmull_u16(filter1_u16, vget_high_u16(src1_u16));
        uint16x4_t out1_u16;
        uint16x4_t out2_u16;
        sum1_u32 = vmlal_u16(sum1_u32, filter2_u16, vget_low_u16(src2_u16));
        sum2_u32 = vmlal_u16(sum2_u32, filter2_u16, vget_high_u16(src2_u16));
        out1_u16 = vshrn_n_u32(vaddq_u32(sum1_u32, round_u32), FILTER_BITS);
        out2_u16 = vshrn_n_u32(vaddq_u32(sum2_u32, round_u32), FILTER_BITS);
        vst1q_u16(&output_ptr[j], vcombine_u16(out1_u16, out2_u16));
      }
      // Next row...
      src_ptr += src_pixels_per_line;
      output_ptr += output_width;
    }
  } else {
    assert(output_width >= 4);
    for (i = 0; i < output_height; ++i) {
      for (j = 0; j < output_width; j += 4) {
        const uint16x4_t src1_u16 = vld1_u16(&src_ptr[j]);
        const uint16x4_t src2_u16 = vld1_u16(&src_ptr[j + pixel_step]);
        uint32x4_t sum_u32 = vmull_u16(filter1_u16, src1_u16);
        uint16x4_t out_u16;
        sum_u32 = vmlal_u16(sum_u32, filter2_u16, src2_u16);
        out_u16 = vshrn_n_u32(vaddq_u32(sum_u32, round_u32), FILTER_BITS);
        vst1_u16(&output_ptr[j], out_u16);
      }
      // Next row...
      src_ptr += src_pixels_per_line;
      output_ptr += output_width;
    }
  }
}

#define HIGHBD_SUBPIX_VAR(W, H)                                                \
  uint32_t vpx_highbd_8_sub_pixel_variance##W##x##H##_neon(                    \
      const uint8_t *src_ptr, int src_stride, int x_offset, int y_offset,      \
      const uint8_t *ref_ptr, int ref_stride, uint32_t *sse) {                 \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    highbd_var_filter_block2d_bil_first_pass(                                  \
        src_ptr, fdata3, src_stride, 1, H + 1, W, bilinear_filters[x_offset]); \
    highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W,       \
                                              bilinear_filters[y_offset]);     \
                                                                               \
    return vpx_highbd_8_variance##W##x##H##_neon(CONVERT_TO_BYTEPTR(temp2), W, \
                                                 ref_ptr, ref_stride, sse);    \
  }                                                                            \
                                                                               \
  uint32_t vpx_highbd_10_sub_pixel_variance##W##x##H##_neon(                   \
      const uint8_t *src_ptr, int src_stride, int x_offset, int y_offset,      \
      const uint8_t *ref_ptr, int ref_stride, uint32_t *sse) {                 \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    highbd_var_filter_block2d_bil_first_pass(                                  \
        src_ptr, fdata3, src_stride, 1, H + 1, W, bilinear_filters[x_offset]); \
    highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W,       \
                                              bilinear_filters[y_offset]);     \
                                                                               \
    return vpx_highbd_10_variance##W##x##H##_neon(                             \
        CONVERT_TO_BYTEPTR(temp2), W, ref_ptr, ref_stride, sse);               \
  }                                                                            \
                                                                               \
  uint32_t vpx_highbd_12_sub_pixel_variance##W##x##H##_neon(                   \
      const uint8_t *src_ptr, int src_stride, int x_offset, int y_offset,      \
      const uint8_t *ref_ptr, int ref_stride, uint32_t *sse) {                 \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    highbd_var_filter_block2d_bil_first_pass(                                  \
        src_ptr, fdata3, src_stride, 1, H + 1, W, bilinear_filters[x_offset]); \
    highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W,       \
                                              bilinear_filters[y_offset]);     \
                                                                               \
    return vpx_highbd_12_variance##W##x##H##_neon(                             \
        CONVERT_TO_BYTEPTR(temp2), W, ref_ptr, ref_stride, sse);               \
  }

#define HIGHBD_SUBPIX_AVG_VAR(W, H)                                            \
  uint32_t vpx_highbd_8_sub_pixel_avg_variance##W##x##H##_neon(                \
      const uint8_t *src_ptr, int src_stride, int x_offset, int y_offset,      \
      const uint8_t *ref_ptr, int ref_stride, uint32_t *sse,                   \
      const uint8_t *second_pred) {                                            \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    highbd_var_filter_block2d_bil_first_pass(                                  \
        src_ptr, fdata3, src_stride, 1, H + 1, W, bilinear_filters[x_offset]); \
    highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W,       \
                                              bilinear_filters[y_offset]);     \
                                                                               \
    vpx_highbd_comp_avg_pred_neon(temp3, CONVERT_TO_SHORTPTR(second_pred), W,  \
                                  H, temp2, W);                                \
                                                                               \
    return vpx_highbd_8_variance##W##x##H##_neon(CONVERT_TO_BYTEPTR(temp3), W, \
                                                 ref_ptr, ref_stride, sse);    \
  }                                                                            \
                                                                               \
  uint32_t vpx_highbd_10_sub_pixel_avg_variance##W##x##H##_neon(               \
      const uint8_t *src_ptr, int src_stride, int x_offset, int y_offset,      \
      const uint8_t *ref_ptr, int ref_stride, uint32_t *sse,                   \
      const uint8_t *second_pred) {                                            \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    highbd_var_filter_block2d_bil_first_pass(                                  \
        src_ptr, fdata3, src_stride, 1, H + 1, W, bilinear_filters[x_offset]); \
    highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W,       \
                                              bilinear_filters[y_offset]);     \
                                                                               \
    vpx_highbd_comp_avg_pred_neon(temp3, CONVERT_TO_SHORTPTR(second_pred), W,  \
                                  H, temp2, W);                                \
                                                                               \
    return vpx_highbd_10_variance##W##x##H##_neon(                             \
        CONVERT_TO_BYTEPTR(temp3), W, ref_ptr, ref_stride, sse);               \
  }                                                                            \
                                                                               \
  uint32_t vpx_highbd_12_sub_pixel_avg_variance##W##x##H##_neon(               \
      const uint8_t *src_ptr, int src_stride, int x_offset, int y_offset,      \
      const uint8_t *ref_ptr, int ref_stride, uint32_t *sse,                   \
      const uint8_t *second_pred) {                                            \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    highbd_var_filter_block2d_bil_first_pass(                                  \
        src_ptr, fdata3, src_stride, 1, H + 1, W, bilinear_filters[x_offset]); \
    highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, W, W, H, W,       \
                                              bilinear_filters[y_offset]);     \
                                                                               \
    vpx_highbd_comp_avg_pred_neon(temp3, CONVERT_TO_SHORTPTR(second_pred), W,  \
                                  H, temp2, W);                                \
                                                                               \
    return vpx_highbd_12_variance##W##x##H##_neon(                             \
        CONVERT_TO_BYTEPTR(temp3), W, ref_ptr, ref_stride, sse);               \
  }

void vpx_highbd_comp_avg_pred_neon(uint16_t *comp_pred, const uint16_t *pred,
                                   int width, int height, const uint16_t *ref,
                                   int ref_stride) {
  int i, j;
  uint32x4_t one_u32 = vdupq_n_u32(1);
  if (width >= 8) {
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; j += 8) {
        const uint16x8_t pred_u16 = vld1q_u16(&pred[j]);
        const uint16x8_t ref_u16 = vld1q_u16(&ref[j]);
        const uint32x4_t sum1_u32 =
            vaddl_u16(vget_low_u16(pred_u16), vget_low_u16(ref_u16));
        const uint32x4_t sum2_u32 =
            vaddl_u16(vget_high_u16(pred_u16), vget_high_u16(ref_u16));
        const uint16x4_t sum1_u16 =
            vshrn_n_u32(vaddq_u32(sum1_u32, one_u32), 1);
        const uint16x4_t sum2_u16 =
            vshrn_n_u32(vaddq_u32(sum2_u32, one_u32), 1);
        const uint16x8_t vcomp_pred = vcombine_u16(sum1_u16, sum2_u16);
        vst1q_u16(&comp_pred[j], vcomp_pred);
      }
      comp_pred += width;
      pred += width;
      ref += ref_stride;
    }
  } else {
    assert(width >= 4);
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; j += 4) {
        const uint16x4_t pred_u16 = vld1_u16(&pred[j]);
        const uint16x4_t ref_u16 = vld1_u16(&ref[j]);
        const uint32x4_t sum_u32 = vaddl_u16(pred_u16, ref_u16);
        const uint16x4_t vcomp_pred =
            vshrn_n_u32(vaddq_u32(sum_u32, one_u32), 1);
        vst1_u16(&comp_pred[j], vcomp_pred);
      }
      comp_pred += width;
      pred += width;
      ref += ref_stride;
    }
  }
}

/* All three forms of the variance are available in the same sizes. */
#define HIGHBD_VARIANCES(W, H) \
  HIGHBD_VAR(W, H)             \
  HIGHBD_SUBPIX_VAR(W, H)      \
  HIGHBD_SUBPIX_AVG_VAR(W, H)

HIGHBD_VARIANCES(64, 64)
HIGHBD_VARIANCES(64, 32)
HIGHBD_VARIANCES(32, 64)
HIGHBD_VARIANCES(32, 32)
HIGHBD_VARIANCES(32, 16)
HIGHBD_VARIANCES(16, 32)
HIGHBD_VARIANCES(16, 16)
HIGHBD_VARIANCES(16, 8)
HIGHBD_VARIANCES(8, 16)
HIGHBD_VARIANCES(8, 8)
HIGHBD_VARIANCES(8, 4)
HIGHBD_VARIANCES(4, 8)
HIGHBD_VARIANCES(4, 4)

HIGHBD_GET_VAR(8)
HIGHBD_GET_VAR(16)

HIGHBD_MSE(16, 16)
HIGHBD_MSE(16, 8)
HIGHBD_MSE(8, 16)
HIGHBD_MSE(8, 8)
