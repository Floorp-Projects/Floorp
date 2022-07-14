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
#include <assert.h>

#include "./vpx_dsp_rtcd.h"
#include "./vpx_config.h"

#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/sum_neon.h"
#include "vpx_ports/mem.h"

#if defined(__ARM_FEATURE_DOTPROD) && (__ARM_FEATURE_DOTPROD == 1)

// Process a block of width 4 four rows at a time.
static void variance_neon_w4x4(const uint8_t *src_ptr, int src_stride,
                               const uint8_t *ref_ptr, int ref_stride, int h,
                               uint32_t *sse, int *sum) {
  int i;
  uint32x4_t sum_a = vdupq_n_u32(0);
  uint32x4_t sum_b = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  for (i = 0; i < h; i += 4) {
    const uint8x16_t a = load_unaligned_u8q(src_ptr, src_stride);
    const uint8x16_t b = load_unaligned_u8q(ref_ptr, ref_stride);

    const uint8x16_t abs_diff = vabdq_u8(a, b);
    sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

    sum_a = vdotq_u32(sum_a, a, vdupq_n_u8(1));
    sum_b = vdotq_u32(sum_b, b, vdupq_n_u8(1));

    src_ptr += 4 * src_stride;
    ref_ptr += 4 * ref_stride;
  }

  *sum = horizontal_add_int32x4(vreinterpretq_s32_u32(vsubq_u32(sum_a, sum_b)));
  *sse = horizontal_add_uint32x4(sse_u32);
}

// Process a block of any size where the width is divisible by 16.
static void variance_neon_w16(const uint8_t *src_ptr, int src_stride,
                              const uint8_t *ref_ptr, int ref_stride, int w,
                              int h, uint32_t *sse, int *sum) {
  int i, j;
  uint32x4_t sum_a = vdupq_n_u32(0);
  uint32x4_t sum_b = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 16) {
      const uint8x16_t a = vld1q_u8(src_ptr + j);
      const uint8x16_t b = vld1q_u8(ref_ptr + j);

      const uint8x16_t abs_diff = vabdq_u8(a, b);
      sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

      sum_a = vdotq_u32(sum_a, a, vdupq_n_u8(1));
      sum_b = vdotq_u32(sum_b, b, vdupq_n_u8(1));
    }
    src_ptr += src_stride;
    ref_ptr += ref_stride;
  }

  *sum = horizontal_add_int32x4(vreinterpretq_s32_u32(vsubq_u32(sum_a, sum_b)));
  *sse = horizontal_add_uint32x4(sse_u32);
}

// Process a block of width 8 two rows at a time.
static void variance_neon_w8x2(const uint8_t *src_ptr, int src_stride,
                               const uint8_t *ref_ptr, int ref_stride, int h,
                               uint32_t *sse, int *sum) {
  int i = 0;
  uint32x2_t sum_a = vdup_n_u32(0);
  uint32x2_t sum_b = vdup_n_u32(0);
  uint32x2_t sse_lo_u32 = vdup_n_u32(0);
  uint32x2_t sse_hi_u32 = vdup_n_u32(0);

  do {
    const uint8x8_t a_0 = vld1_u8(src_ptr);
    const uint8x8_t a_1 = vld1_u8(src_ptr + src_stride);
    const uint8x8_t b_0 = vld1_u8(ref_ptr);
    const uint8x8_t b_1 = vld1_u8(ref_ptr + ref_stride);

    const uint8x8_t abs_diff_0 = vabd_u8(a_0, b_0);
    const uint8x8_t abs_diff_1 = vabd_u8(a_1, b_1);
    sse_lo_u32 = vdot_u32(sse_lo_u32, abs_diff_0, abs_diff_0);
    sse_hi_u32 = vdot_u32(sse_hi_u32, abs_diff_1, abs_diff_1);

    sum_a = vdot_u32(sum_a, a_0, vdup_n_u8(1));
    sum_b = vdot_u32(sum_b, b_0, vdup_n_u8(1));
    sum_a = vdot_u32(sum_a, a_1, vdup_n_u8(1));
    sum_b = vdot_u32(sum_b, b_1, vdup_n_u8(1));

    src_ptr += src_stride + src_stride;
    ref_ptr += ref_stride + ref_stride;
    i += 2;
  } while (i < h);

  *sum = horizontal_add_int32x2(vreinterpret_s32_u32(vsub_u32(sum_a, sum_b)));
  *sse = horizontal_add_uint32x2(vadd_u32(sse_lo_u32, sse_hi_u32));
}

#else

// The variance helper functions use int16_t for sum. 8 values are accumulated
// and then added (at which point they expand up to int32_t). To avoid overflow,
// there can be no more than 32767 / 255 ~= 128 values accumulated in each
// column. For a 32x32 buffer, this results in 32 / 8 = 4 values per row * 32
// rows = 128. Asserts have been added to each function to warn against reaching
// this limit.

// Process a block of width 4 four rows at a time.
static void variance_neon_w4x4(const uint8_t *src_ptr, int src_stride,
                               const uint8_t *ref_ptr, int ref_stride, int h,
                               uint32_t *sse, int *sum) {
  int i;
  int16x8_t sum_s16 = vdupq_n_s16(0);
  int32x4_t sse_lo_s32 = vdupq_n_s32(0);
  int32x4_t sse_hi_s32 = vdupq_n_s32(0);

  // Since width is only 4, sum_s16 only loads a half row per loop.
  assert(h <= 256);

  for (i = 0; i < h; i += 4) {
    const uint8x16_t a_u8 = load_unaligned_u8q(src_ptr, src_stride);
    const uint8x16_t b_u8 = load_unaligned_u8q(ref_ptr, ref_stride);
    const uint16x8_t diff_lo_u16 =
        vsubl_u8(vget_low_u8(a_u8), vget_low_u8(b_u8));
    const uint16x8_t diff_hi_u16 =
        vsubl_u8(vget_high_u8(a_u8), vget_high_u8(b_u8));

    const int16x8_t diff_lo_s16 = vreinterpretq_s16_u16(diff_lo_u16);
    const int16x8_t diff_hi_s16 = vreinterpretq_s16_u16(diff_hi_u16);

    sum_s16 = vaddq_s16(sum_s16, diff_lo_s16);
    sum_s16 = vaddq_s16(sum_s16, diff_hi_s16);

    sse_lo_s32 = vmlal_s16(sse_lo_s32, vget_low_s16(diff_lo_s16),
                           vget_low_s16(diff_lo_s16));
    sse_lo_s32 = vmlal_s16(sse_lo_s32, vget_high_s16(diff_lo_s16),
                           vget_high_s16(diff_lo_s16));

    sse_hi_s32 = vmlal_s16(sse_hi_s32, vget_low_s16(diff_hi_s16),
                           vget_low_s16(diff_hi_s16));
    sse_hi_s32 = vmlal_s16(sse_hi_s32, vget_high_s16(diff_hi_s16),
                           vget_high_s16(diff_hi_s16));

    src_ptr += 4 * src_stride;
    ref_ptr += 4 * ref_stride;
  }

  *sum = horizontal_add_int16x8(sum_s16);
  *sse = horizontal_add_uint32x4(
      vreinterpretq_u32_s32(vaddq_s32(sse_lo_s32, sse_hi_s32)));
}

// Process a block of any size where the width is divisible by 16.
static void variance_neon_w16(const uint8_t *src_ptr, int src_stride,
                              const uint8_t *ref_ptr, int ref_stride, int w,
                              int h, uint32_t *sse, int *sum) {
  int i, j;
  int16x8_t sum_s16 = vdupq_n_s16(0);
  int32x4_t sse_lo_s32 = vdupq_n_s32(0);
  int32x4_t sse_hi_s32 = vdupq_n_s32(0);

  // The loop loads 16 values at a time but doubles them up when accumulating
  // into sum_s16.
  assert(w / 8 * h <= 128);

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 16) {
      const uint8x16_t a_u8 = vld1q_u8(src_ptr + j);
      const uint8x16_t b_u8 = vld1q_u8(ref_ptr + j);

      const uint16x8_t diff_lo_u16 =
          vsubl_u8(vget_low_u8(a_u8), vget_low_u8(b_u8));
      const uint16x8_t diff_hi_u16 =
          vsubl_u8(vget_high_u8(a_u8), vget_high_u8(b_u8));

      const int16x8_t diff_lo_s16 = vreinterpretq_s16_u16(diff_lo_u16);
      const int16x8_t diff_hi_s16 = vreinterpretq_s16_u16(diff_hi_u16);

      sum_s16 = vaddq_s16(sum_s16, diff_lo_s16);
      sum_s16 = vaddq_s16(sum_s16, diff_hi_s16);

      sse_lo_s32 = vmlal_s16(sse_lo_s32, vget_low_s16(diff_lo_s16),
                             vget_low_s16(diff_lo_s16));
      sse_lo_s32 = vmlal_s16(sse_lo_s32, vget_high_s16(diff_lo_s16),
                             vget_high_s16(diff_lo_s16));

      sse_hi_s32 = vmlal_s16(sse_hi_s32, vget_low_s16(diff_hi_s16),
                             vget_low_s16(diff_hi_s16));
      sse_hi_s32 = vmlal_s16(sse_hi_s32, vget_high_s16(diff_hi_s16),
                             vget_high_s16(diff_hi_s16));
    }
    src_ptr += src_stride;
    ref_ptr += ref_stride;
  }

  *sum = horizontal_add_int16x8(sum_s16);
  *sse = horizontal_add_uint32x4(
      vreinterpretq_u32_s32(vaddq_s32(sse_lo_s32, sse_hi_s32)));
}

// Process a block of width 8 two rows at a time.
static void variance_neon_w8x2(const uint8_t *src_ptr, int src_stride,
                               const uint8_t *ref_ptr, int ref_stride, int h,
                               uint32_t *sse, int *sum) {
  int i = 0;
  int16x8_t sum_s16 = vdupq_n_s16(0);
  int32x4_t sse_lo_s32 = vdupq_n_s32(0);
  int32x4_t sse_hi_s32 = vdupq_n_s32(0);

  // Each column has it's own accumulator entry in sum_s16.
  assert(h <= 128);

  do {
    const uint8x8_t a_0_u8 = vld1_u8(src_ptr);
    const uint8x8_t a_1_u8 = vld1_u8(src_ptr + src_stride);
    const uint8x8_t b_0_u8 = vld1_u8(ref_ptr);
    const uint8x8_t b_1_u8 = vld1_u8(ref_ptr + ref_stride);
    const uint16x8_t diff_0_u16 = vsubl_u8(a_0_u8, b_0_u8);
    const uint16x8_t diff_1_u16 = vsubl_u8(a_1_u8, b_1_u8);
    const int16x8_t diff_0_s16 = vreinterpretq_s16_u16(diff_0_u16);
    const int16x8_t diff_1_s16 = vreinterpretq_s16_u16(diff_1_u16);
    sum_s16 = vaddq_s16(sum_s16, diff_0_s16);
    sum_s16 = vaddq_s16(sum_s16, diff_1_s16);
    sse_lo_s32 = vmlal_s16(sse_lo_s32, vget_low_s16(diff_0_s16),
                           vget_low_s16(diff_0_s16));
    sse_lo_s32 = vmlal_s16(sse_lo_s32, vget_low_s16(diff_1_s16),
                           vget_low_s16(diff_1_s16));
    sse_hi_s32 = vmlal_s16(sse_hi_s32, vget_high_s16(diff_0_s16),
                           vget_high_s16(diff_0_s16));
    sse_hi_s32 = vmlal_s16(sse_hi_s32, vget_high_s16(diff_1_s16),
                           vget_high_s16(diff_1_s16));
    src_ptr += src_stride + src_stride;
    ref_ptr += ref_stride + ref_stride;
    i += 2;
  } while (i < h);

  *sum = horizontal_add_int16x8(sum_s16);
  *sse = horizontal_add_uint32x4(
      vreinterpretq_u32_s32(vaddq_s32(sse_lo_s32, sse_hi_s32)));
}

#endif

void vpx_get8x8var_neon(const uint8_t *src_ptr, int src_stride,
                        const uint8_t *ref_ptr, int ref_stride,
                        unsigned int *sse, int *sum) {
  variance_neon_w8x2(src_ptr, src_stride, ref_ptr, ref_stride, 8, sse, sum);
}

void vpx_get16x16var_neon(const uint8_t *src_ptr, int src_stride,
                          const uint8_t *ref_ptr, int ref_stride,
                          unsigned int *sse, int *sum) {
  variance_neon_w16(src_ptr, src_stride, ref_ptr, ref_stride, 16, 16, sse, sum);
}

#define VARIANCENXM(n, m, shift)                                             \
  unsigned int vpx_variance##n##x##m##_neon(                                 \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,        \
      int ref_stride, unsigned int *sse) {                                   \
    int sum;                                                                 \
    if (n == 4)                                                              \
      variance_neon_w4x4(src_ptr, src_stride, ref_ptr, ref_stride, m, sse,   \
                         &sum);                                              \
    else if (n == 8)                                                         \
      variance_neon_w8x2(src_ptr, src_stride, ref_ptr, ref_stride, m, sse,   \
                         &sum);                                              \
    else                                                                     \
      variance_neon_w16(src_ptr, src_stride, ref_ptr, ref_stride, n, m, sse, \
                        &sum);                                               \
    if (n * m < 16 * 16)                                                     \
      return *sse - ((sum * sum) >> shift);                                  \
    else                                                                     \
      return *sse - (uint32_t)(((int64_t)sum * sum) >> shift);               \
  }

VARIANCENXM(4, 4, 4)
VARIANCENXM(4, 8, 5)
VARIANCENXM(8, 4, 5)
VARIANCENXM(8, 8, 6)
VARIANCENXM(8, 16, 7)
VARIANCENXM(16, 8, 7)
VARIANCENXM(16, 16, 8)
VARIANCENXM(16, 32, 9)
VARIANCENXM(32, 16, 9)
VARIANCENXM(32, 32, 10)

unsigned int vpx_variance32x64_neon(const uint8_t *src_ptr, int src_stride,
                                    const uint8_t *ref_ptr, int ref_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;
  variance_neon_w16(src_ptr, src_stride, ref_ptr, ref_stride, 32, 32, &sse1,
                    &sum1);
  variance_neon_w16(src_ptr + (32 * src_stride), src_stride,
                    ref_ptr + (32 * ref_stride), ref_stride, 32, 32, &sse2,
                    &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (unsigned int)(((int64_t)sum1 * sum1) >> 11);
}

unsigned int vpx_variance64x32_neon(const uint8_t *src_ptr, int src_stride,
                                    const uint8_t *ref_ptr, int ref_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;
  variance_neon_w16(src_ptr, src_stride, ref_ptr, ref_stride, 64, 16, &sse1,
                    &sum1);
  variance_neon_w16(src_ptr + (16 * src_stride), src_stride,
                    ref_ptr + (16 * ref_stride), ref_stride, 64, 16, &sse2,
                    &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (unsigned int)(((int64_t)sum1 * sum1) >> 11);
}

unsigned int vpx_variance64x64_neon(const uint8_t *src_ptr, int src_stride,
                                    const uint8_t *ref_ptr, int ref_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;

  variance_neon_w16(src_ptr, src_stride, ref_ptr, ref_stride, 64, 16, &sse1,
                    &sum1);
  variance_neon_w16(src_ptr + (16 * src_stride), src_stride,
                    ref_ptr + (16 * ref_stride), ref_stride, 64, 16, &sse2,
                    &sum2);
  sse1 += sse2;
  sum1 += sum2;

  variance_neon_w16(src_ptr + (16 * 2 * src_stride), src_stride,
                    ref_ptr + (16 * 2 * ref_stride), ref_stride, 64, 16, &sse2,
                    &sum2);
  sse1 += sse2;
  sum1 += sum2;

  variance_neon_w16(src_ptr + (16 * 3 * src_stride), src_stride,
                    ref_ptr + (16 * 3 * ref_stride), ref_stride, 64, 16, &sse2,
                    &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (unsigned int)(((int64_t)sum1 * sum1) >> 12);
}

#if defined(__ARM_FEATURE_DOTPROD) && (__ARM_FEATURE_DOTPROD == 1)

unsigned int vpx_mse16x16_neon(const unsigned char *src_ptr, int src_stride,
                               const unsigned char *ref_ptr, int ref_stride,
                               unsigned int *sse) {
  int i;
  uint8x16_t a[2], b[2], abs_diff[2];
  uint32x4_t sse_vec[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  for (i = 0; i < 8; i++) {
    a[0] = vld1q_u8(src_ptr);
    src_ptr += src_stride;
    a[1] = vld1q_u8(src_ptr);
    src_ptr += src_stride;
    b[0] = vld1q_u8(ref_ptr);
    ref_ptr += ref_stride;
    b[1] = vld1q_u8(ref_ptr);
    ref_ptr += ref_stride;

    abs_diff[0] = vabdq_u8(a[0], b[0]);
    abs_diff[1] = vabdq_u8(a[1], b[1]);

    sse_vec[0] = vdotq_u32(sse_vec[0], abs_diff[0], abs_diff[0]);
    sse_vec[1] = vdotq_u32(sse_vec[1], abs_diff[1], abs_diff[1]);
  }

  *sse = horizontal_add_uint32x4(vaddq_u32(sse_vec[0], sse_vec[1]));
  return horizontal_add_uint32x4(vaddq_u32(sse_vec[0], sse_vec[1]));
}

unsigned int vpx_get4x4sse_cs_neon(const unsigned char *src_ptr, int src_stride,
                                   const unsigned char *ref_ptr,
                                   int ref_stride) {
  uint8x8_t a[4], b[4], abs_diff[4];
  uint32x2_t sse = vdup_n_u32(0);

  a[0] = vld1_u8(src_ptr);
  src_ptr += src_stride;
  b[0] = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  a[1] = vld1_u8(src_ptr);
  src_ptr += src_stride;
  b[1] = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  a[2] = vld1_u8(src_ptr);
  src_ptr += src_stride;
  b[2] = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  a[3] = vld1_u8(src_ptr);
  b[3] = vld1_u8(ref_ptr);

  abs_diff[0] = vabd_u8(a[0], b[0]);
  abs_diff[1] = vabd_u8(a[1], b[1]);
  abs_diff[2] = vabd_u8(a[2], b[2]);
  abs_diff[3] = vabd_u8(a[3], b[3]);

  sse = vdot_u32(sse, abs_diff[0], abs_diff[0]);
  sse = vdot_u32(sse, abs_diff[1], abs_diff[1]);
  sse = vdot_u32(sse, abs_diff[2], abs_diff[2]);
  sse = vdot_u32(sse, abs_diff[3], abs_diff[3]);

  return vget_lane_u32(sse, 0);
}

#else

unsigned int vpx_mse16x16_neon(const unsigned char *src_ptr, int src_stride,
                               const unsigned char *ref_ptr, int ref_stride,
                               unsigned int *sse) {
  int i;
  uint8x16_t a[2], b[2];
  int16x4_t diff_lo[4], diff_hi[4];
  uint16x8_t diff[4];
  int32x4_t sse_vec[4] = { vdupq_n_s32(0), vdupq_n_s32(0), vdupq_n_s32(0),
                           vdupq_n_s32(0) };

  for (i = 0; i < 8; i++) {
    a[0] = vld1q_u8(src_ptr);
    src_ptr += src_stride;
    a[1] = vld1q_u8(src_ptr);
    src_ptr += src_stride;
    b[0] = vld1q_u8(ref_ptr);
    ref_ptr += ref_stride;
    b[1] = vld1q_u8(ref_ptr);
    ref_ptr += ref_stride;

    diff[0] = vsubl_u8(vget_low_u8(a[0]), vget_low_u8(b[0]));
    diff[1] = vsubl_u8(vget_high_u8(a[0]), vget_high_u8(b[0]));
    diff[2] = vsubl_u8(vget_low_u8(a[1]), vget_low_u8(b[1]));
    diff[3] = vsubl_u8(vget_high_u8(a[1]), vget_high_u8(b[1]));

    diff_lo[0] = vreinterpret_s16_u16(vget_low_u16(diff[0]));
    diff_lo[1] = vreinterpret_s16_u16(vget_low_u16(diff[1]));
    sse_vec[0] = vmlal_s16(sse_vec[0], diff_lo[0], diff_lo[0]);
    sse_vec[1] = vmlal_s16(sse_vec[1], diff_lo[1], diff_lo[1]);

    diff_lo[2] = vreinterpret_s16_u16(vget_low_u16(diff[2]));
    diff_lo[3] = vreinterpret_s16_u16(vget_low_u16(diff[3]));
    sse_vec[2] = vmlal_s16(sse_vec[2], diff_lo[2], diff_lo[2]);
    sse_vec[3] = vmlal_s16(sse_vec[3], diff_lo[3], diff_lo[3]);

    diff_hi[0] = vreinterpret_s16_u16(vget_high_u16(diff[0]));
    diff_hi[1] = vreinterpret_s16_u16(vget_high_u16(diff[1]));
    sse_vec[0] = vmlal_s16(sse_vec[0], diff_hi[0], diff_hi[0]);
    sse_vec[1] = vmlal_s16(sse_vec[1], diff_hi[1], diff_hi[1]);

    diff_hi[2] = vreinterpret_s16_u16(vget_high_u16(diff[2]));
    diff_hi[3] = vreinterpret_s16_u16(vget_high_u16(diff[3]));
    sse_vec[2] = vmlal_s16(sse_vec[2], diff_hi[2], diff_hi[2]);
    sse_vec[3] = vmlal_s16(sse_vec[3], diff_hi[3], diff_hi[3]);
  }

  sse_vec[0] = vaddq_s32(sse_vec[0], sse_vec[1]);
  sse_vec[2] = vaddq_s32(sse_vec[2], sse_vec[3]);
  sse_vec[0] = vaddq_s32(sse_vec[0], sse_vec[2]);

  *sse = horizontal_add_uint32x4(vreinterpretq_u32_s32(sse_vec[0]));
  return horizontal_add_uint32x4(vreinterpretq_u32_s32(sse_vec[0]));
}

unsigned int vpx_get4x4sse_cs_neon(const unsigned char *src_ptr, int src_stride,
                                   const unsigned char *ref_ptr,
                                   int ref_stride) {
  uint8x8_t a[4], b[4];
  int16x4_t diff_lo[4];
  uint16x8_t diff[4];
  int32x4_t sse;

  a[0] = vld1_u8(src_ptr);
  src_ptr += src_stride;
  b[0] = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  a[1] = vld1_u8(src_ptr);
  src_ptr += src_stride;
  b[1] = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  a[2] = vld1_u8(src_ptr);
  src_ptr += src_stride;
  b[2] = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  a[3] = vld1_u8(src_ptr);
  b[3] = vld1_u8(ref_ptr);

  diff[0] = vsubl_u8(a[0], b[0]);
  diff[1] = vsubl_u8(a[1], b[1]);
  diff[2] = vsubl_u8(a[2], b[2]);
  diff[3] = vsubl_u8(a[3], b[3]);

  diff_lo[0] = vget_low_s16(vreinterpretq_s16_u16(diff[0]));
  diff_lo[1] = vget_low_s16(vreinterpretq_s16_u16(diff[1]));
  diff_lo[2] = vget_low_s16(vreinterpretq_s16_u16(diff[2]));
  diff_lo[3] = vget_low_s16(vreinterpretq_s16_u16(diff[3]));

  sse = vmull_s16(diff_lo[0], diff_lo[0]);
  sse = vmlal_s16(sse, diff_lo[1], diff_lo[1]);
  sse = vmlal_s16(sse, diff_lo[2], diff_lo[2]);
  sse = vmlal_s16(sse, diff_lo[3], diff_lo[3]);

  return horizontal_add_uint32x4(vreinterpretq_u32_s32(sse));
}

#endif
