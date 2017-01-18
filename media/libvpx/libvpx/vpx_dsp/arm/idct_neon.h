/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_DSP_ARM_IDCT_NEON_H_
#define VPX_DSP_ARM_IDCT_NEON_H_

#include <arm_neon.h>

#include "./vpx_config.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/vpx_dsp_common.h"

DECLARE_ALIGNED(16, static const int16_t, kCospi[8]) = {
  16384 /*  cospi_0_64 */,  15137 /* cospi_8_64 */,
  11585 /*  cospi_16_64 */, 6270 /* cospi_24_64 */,
  16069 /*  cospi_4_64 */,  13623 /* cospi_12_64 */,
  -9102 /* -cospi_20_64 */, 3196 /* cospi_28_64 */
};

DECLARE_ALIGNED(16, static const int32_t, kCospi32[8]) = {
  16384 /*  cospi_0_64 */,  15137 /* cospi_8_64 */,
  11585 /*  cospi_16_64 */, 6270 /* cospi_24_64 */,
  16069 /*  cospi_4_64 */,  13623 /* cospi_12_64 */,
  -9102 /* -cospi_20_64 */, 3196 /* cospi_28_64 */
};

//------------------------------------------------------------------------------
// Helper functions used to load tran_low_t into int16, narrowing if necessary.

static INLINE int16x8x2_t load_tran_low_to_s16x2q(const tran_low_t *buf) {
#if CONFIG_VP9_HIGHBITDEPTH
  const int32x4x2_t v0 = vld2q_s32(buf);
  const int32x4x2_t v1 = vld2q_s32(buf + 8);
  const int16x4_t s0 = vmovn_s32(v0.val[0]);
  const int16x4_t s1 = vmovn_s32(v0.val[1]);
  const int16x4_t s2 = vmovn_s32(v1.val[0]);
  const int16x4_t s3 = vmovn_s32(v1.val[1]);
  int16x8x2_t res;
  res.val[0] = vcombine_s16(s0, s2);
  res.val[1] = vcombine_s16(s1, s3);
  return res;
#else
  return vld2q_s16(buf);
#endif
}

static INLINE int16x8_t load_tran_low_to_s16q(const tran_low_t *buf) {
#if CONFIG_VP9_HIGHBITDEPTH
  const int32x4_t v0 = vld1q_s32(buf);
  const int32x4_t v1 = vld1q_s32(buf + 4);
  const int16x4_t s0 = vmovn_s32(v0);
  const int16x4_t s1 = vmovn_s32(v1);
  return vcombine_s16(s0, s1);
#else
  return vld1q_s16(buf);
#endif
}

static INLINE int16x4_t load_tran_low_to_s16d(const tran_low_t *buf) {
#if CONFIG_VP9_HIGHBITDEPTH
  const int32x4_t v0 = vld1q_s32(buf);
  return vmovn_s32(v0);
#else
  return vld1_s16(buf);
#endif
}

//------------------------------------------------------------------------------

// Multiply a by a_const. Saturate, shift and narrow by 14.
static INLINE int16x8_t multiply_shift_and_narrow_s16(const int16x8_t a,
                                                      const int16_t a_const) {
  // Shift by 14 + rounding will be within 16 bits for well formed streams.
  // See WRAPLOW and dct_const_round_shift for details.
  // This instruction doubles the result and returns the high half, essentially
  // resulting in a right shift by 15. By multiplying the constant first that
  // becomes a right shift by 14.
  // The largest possible value used here is
  // vpx_dsp/txfm_common.h:cospi_1_64 = 16364 (* 2 = 32728) a which falls *just*
  // within the range of int16_t (+32767 / -32768) even when negated.
  return vqrdmulhq_n_s16(a, a_const * 2);
}

// Add a and b, then multiply by ab_const. Shift and narrow by 14.
static INLINE int16x8_t add_multiply_shift_and_narrow_s16(
    const int16x8_t a, const int16x8_t b, const int16_t ab_const) {
  // In both add_ and it's pair, sub_, the input for well-formed streams will be
  // well within 16 bits (input to the idct is the difference between two frames
  // and will be within -255 to 255, or 9 bits)
  // However, for inputs over about 25,000 (valid for int16_t, but not for idct
  // input) this function can not use vaddq_s16.
  // In order to match existing behavior and intentionally out of range tests,
  // expand the addition up to 32 bits to prevent truncation.
  int32x4_t temp_low = vaddl_s16(vget_low_s16(a), vget_low_s16(b));
  int32x4_t temp_high = vaddl_s16(vget_high_s16(a), vget_high_s16(b));
  temp_low = vmulq_n_s32(temp_low, ab_const);
  temp_high = vmulq_n_s32(temp_high, ab_const);
  return vcombine_s16(vrshrn_n_s32(temp_low, 14), vrshrn_n_s32(temp_high, 14));
}

// Subtract b from a, then multiply by ab_const. Shift and narrow by 14.
static INLINE int16x8_t sub_multiply_shift_and_narrow_s16(
    const int16x8_t a, const int16x8_t b, const int16_t ab_const) {
  int32x4_t temp_low = vsubl_s16(vget_low_s16(a), vget_low_s16(b));
  int32x4_t temp_high = vsubl_s16(vget_high_s16(a), vget_high_s16(b));
  temp_low = vmulq_n_s32(temp_low, ab_const);
  temp_high = vmulq_n_s32(temp_high, ab_const);
  return vcombine_s16(vrshrn_n_s32(temp_low, 14), vrshrn_n_s32(temp_high, 14));
}

// Multiply a by a_const and b by b_const, then accumulate. Shift and narrow by
// 14.
static INLINE int16x8_t multiply_accumulate_shift_and_narrow_s16(
    const int16x8_t a, const int16_t a_const, const int16x8_t b,
    const int16_t b_const) {
  int32x4_t temp_low = vmull_n_s16(vget_low_s16(a), a_const);
  int32x4_t temp_high = vmull_n_s16(vget_high_s16(a), a_const);
  temp_low = vmlal_n_s16(temp_low, vget_low_s16(b), b_const);
  temp_high = vmlal_n_s16(temp_high, vget_high_s16(b), b_const);
  return vcombine_s16(vrshrn_n_s32(temp_low, 14), vrshrn_n_s32(temp_high, 14));
}

// Shift the output down by 6 and add it to the destination buffer.
static INLINE void add_and_store_u8_s16(const int16x8_t a0, const int16x8_t a1,
                                        const int16x8_t a2, const int16x8_t a3,
                                        const int16x8_t a4, const int16x8_t a5,
                                        const int16x8_t a6, const int16x8_t a7,
                                        uint8_t *b, const int b_stride) {
  uint8x8_t b0, b1, b2, b3, b4, b5, b6, b7;
  int16x8_t c0, c1, c2, c3, c4, c5, c6, c7;
  b0 = vld1_u8(b);
  b += b_stride;
  b1 = vld1_u8(b);
  b += b_stride;
  b2 = vld1_u8(b);
  b += b_stride;
  b3 = vld1_u8(b);
  b += b_stride;
  b4 = vld1_u8(b);
  b += b_stride;
  b5 = vld1_u8(b);
  b += b_stride;
  b6 = vld1_u8(b);
  b += b_stride;
  b7 = vld1_u8(b);
  b -= (7 * b_stride);

  // c = b + (a >> 6)
  c0 = vrsraq_n_s16(vreinterpretq_s16_u16(vmovl_u8(b0)), a0, 6);
  c1 = vrsraq_n_s16(vreinterpretq_s16_u16(vmovl_u8(b1)), a1, 6);
  c2 = vrsraq_n_s16(vreinterpretq_s16_u16(vmovl_u8(b2)), a2, 6);
  c3 = vrsraq_n_s16(vreinterpretq_s16_u16(vmovl_u8(b3)), a3, 6);
  c4 = vrsraq_n_s16(vreinterpretq_s16_u16(vmovl_u8(b4)), a4, 6);
  c5 = vrsraq_n_s16(vreinterpretq_s16_u16(vmovl_u8(b5)), a5, 6);
  c6 = vrsraq_n_s16(vreinterpretq_s16_u16(vmovl_u8(b6)), a6, 6);
  c7 = vrsraq_n_s16(vreinterpretq_s16_u16(vmovl_u8(b7)), a7, 6);

  b0 = vqmovun_s16(c0);
  b1 = vqmovun_s16(c1);
  b2 = vqmovun_s16(c2);
  b3 = vqmovun_s16(c3);
  b4 = vqmovun_s16(c4);
  b5 = vqmovun_s16(c5);
  b6 = vqmovun_s16(c6);
  b7 = vqmovun_s16(c7);

  vst1_u8(b, b0);
  b += b_stride;
  vst1_u8(b, b1);
  b += b_stride;
  vst1_u8(b, b2);
  b += b_stride;
  vst1_u8(b, b3);
  b += b_stride;
  vst1_u8(b, b4);
  b += b_stride;
  vst1_u8(b, b5);
  b += b_stride;
  vst1_u8(b, b6);
  b += b_stride;
  vst1_u8(b, b7);
}

static INLINE uint8x16_t create_dcq(const int16_t dc) {
  // Clip both sides and gcc may compile to assembly 'usat'.
  const int16_t t = (dc < 0) ? 0 : ((dc > 255) ? 255 : dc);
  return vdupq_n_u8((uint8_t)t);
}

static INLINE void idct4x4_16_kernel_bd8(const int16x4_t cospis,
                                         int16x8_t *const a0,
                                         int16x8_t *const a1) {
  int16x4_t b0, b1, b2, b3;
  int32x4_t c0, c1, c2, c3;
  int16x8_t d0, d1;

  transpose_s16_4x4q(a0, a1);
  b0 = vget_low_s16(*a0);
  b1 = vget_high_s16(*a0);
  b2 = vget_low_s16(*a1);
  b3 = vget_high_s16(*a1);
  c0 = vmull_lane_s16(b0, cospis, 2);
  c2 = vmull_lane_s16(b1, cospis, 2);
  c1 = vsubq_s32(c0, c2);
  c0 = vaddq_s32(c0, c2);
  c2 = vmull_lane_s16(b2, cospis, 3);
  c3 = vmull_lane_s16(b2, cospis, 1);
  c2 = vmlsl_lane_s16(c2, b3, cospis, 1);
  c3 = vmlal_lane_s16(c3, b3, cospis, 3);
  b0 = vrshrn_n_s32(c0, 14);
  b1 = vrshrn_n_s32(c1, 14);
  b2 = vrshrn_n_s32(c2, 14);
  b3 = vrshrn_n_s32(c3, 14);
  d0 = vcombine_s16(b0, b1);
  d1 = vcombine_s16(b3, b2);
  *a0 = vaddq_s16(d0, d1);
  *a1 = vsubq_s16(d0, d1);
}

static INLINE void idct8x8_12_pass1_bd8(
    const int16x4_t cospis0, const int16x4_t cospisd0, const int16x4_t cospisd1,
    int16x4_t *const io0, int16x4_t *const io1, int16x4_t *const io2,
    int16x4_t *const io3, int16x4_t *const io4, int16x4_t *const io5,
    int16x4_t *const io6, int16x4_t *const io7) {
  int16x4_t step1[8], step2[8];
  int32x4_t t32[2];

  transpose_s16_4x4d(io0, io1, io2, io3);

  // stage 1
  step1[4] = vqrdmulh_lane_s16(*io1, cospisd1, 3);
  step1[5] = vqrdmulh_lane_s16(*io3, cospisd1, 2);
  step1[6] = vqrdmulh_lane_s16(*io3, cospisd1, 1);
  step1[7] = vqrdmulh_lane_s16(*io1, cospisd1, 0);

  // stage 2
  step2[1] = vqrdmulh_lane_s16(*io0, cospisd0, 2);
  step2[2] = vqrdmulh_lane_s16(*io2, cospisd0, 3);
  step2[3] = vqrdmulh_lane_s16(*io2, cospisd0, 1);

  step2[4] = vadd_s16(step1[4], step1[5]);
  step2[5] = vsub_s16(step1[4], step1[5]);
  step2[6] = vsub_s16(step1[7], step1[6]);
  step2[7] = vadd_s16(step1[7], step1[6]);

  // stage 3
  step1[0] = vadd_s16(step2[1], step2[3]);
  step1[1] = vadd_s16(step2[1], step2[2]);
  step1[2] = vsub_s16(step2[1], step2[2]);
  step1[3] = vsub_s16(step2[1], step2[3]);

  t32[1] = vmull_lane_s16(step2[6], cospis0, 2);
  t32[0] = vmlsl_lane_s16(t32[1], step2[5], cospis0, 2);
  t32[1] = vmlal_lane_s16(t32[1], step2[5], cospis0, 2);
  step1[5] = vrshrn_n_s32(t32[0], 14);
  step1[6] = vrshrn_n_s32(t32[1], 14);

  // stage 4
  *io0 = vadd_s16(step1[0], step2[7]);
  *io1 = vadd_s16(step1[1], step1[6]);
  *io2 = vadd_s16(step1[2], step1[5]);
  *io3 = vadd_s16(step1[3], step2[4]);
  *io4 = vsub_s16(step1[3], step2[4]);
  *io5 = vsub_s16(step1[2], step1[5]);
  *io6 = vsub_s16(step1[1], step1[6]);
  *io7 = vsub_s16(step1[0], step2[7]);
}

static INLINE void idct8x8_12_pass2_bd8(
    const int16x4_t cospis0, const int16x4_t cospisd0, const int16x4_t cospisd1,
    const int16x4_t input0, const int16x4_t input1, const int16x4_t input2,
    const int16x4_t input3, const int16x4_t input4, const int16x4_t input5,
    const int16x4_t input6, const int16x4_t input7, int16x8_t *const output0,
    int16x8_t *const output1, int16x8_t *const output2,
    int16x8_t *const output3, int16x8_t *const output4,
    int16x8_t *const output5, int16x8_t *const output6,
    int16x8_t *const output7) {
  int16x8_t in[4];
  int16x8_t step1[8], step2[8];
  int32x4_t t32[8];
  int16x4_t t16[8];

  transpose_s16_4x8(input0, input1, input2, input3, input4, input5, input6,
                    input7, &in[0], &in[1], &in[2], &in[3]);

  // stage 1
  step1[4] = vqrdmulhq_lane_s16(in[1], cospisd1, 3);
  step1[5] = vqrdmulhq_lane_s16(in[3], cospisd1, 2);
  step1[6] = vqrdmulhq_lane_s16(in[3], cospisd1, 1);
  step1[7] = vqrdmulhq_lane_s16(in[1], cospisd1, 0);

  // stage 2
  step2[1] = vqrdmulhq_lane_s16(in[0], cospisd0, 2);
  step2[2] = vqrdmulhq_lane_s16(in[2], cospisd0, 3);
  step2[3] = vqrdmulhq_lane_s16(in[2], cospisd0, 1);

  step2[4] = vaddq_s16(step1[4], step1[5]);
  step2[5] = vsubq_s16(step1[4], step1[5]);
  step2[6] = vsubq_s16(step1[7], step1[6]);
  step2[7] = vaddq_s16(step1[7], step1[6]);

  // stage 3
  step1[0] = vaddq_s16(step2[1], step2[3]);
  step1[1] = vaddq_s16(step2[1], step2[2]);
  step1[2] = vsubq_s16(step2[1], step2[2]);
  step1[3] = vsubq_s16(step2[1], step2[3]);

  t32[2] = vmull_lane_s16(vget_low_s16(step2[6]), cospis0, 2);
  t32[3] = vmull_lane_s16(vget_high_s16(step2[6]), cospis0, 2);
  t32[0] = vmlsl_lane_s16(t32[2], vget_low_s16(step2[5]), cospis0, 2);
  t32[1] = vmlsl_lane_s16(t32[3], vget_high_s16(step2[5]), cospis0, 2);
  t32[2] = vmlal_lane_s16(t32[2], vget_low_s16(step2[5]), cospis0, 2);
  t32[3] = vmlal_lane_s16(t32[3], vget_high_s16(step2[5]), cospis0, 2);
  t16[0] = vrshrn_n_s32(t32[0], 14);
  t16[1] = vrshrn_n_s32(t32[1], 14);
  t16[2] = vrshrn_n_s32(t32[2], 14);
  t16[3] = vrshrn_n_s32(t32[3], 14);
  step1[5] = vcombine_s16(t16[0], t16[1]);
  step1[6] = vcombine_s16(t16[2], t16[3]);

  // stage 4
  *output0 = vaddq_s16(step1[0], step2[7]);
  *output1 = vaddq_s16(step1[1], step1[6]);
  *output2 = vaddq_s16(step1[2], step1[5]);
  *output3 = vaddq_s16(step1[3], step2[4]);
  *output4 = vsubq_s16(step1[3], step2[4]);
  *output5 = vsubq_s16(step1[2], step1[5]);
  *output6 = vsubq_s16(step1[1], step1[6]);
  *output7 = vsubq_s16(step1[0], step2[7]);
}

static INLINE void idct8x8_64_1d_bd8(const int16x4_t cospis0,
                                     const int16x4_t cospis1,
                                     int16x8_t *const io0, int16x8_t *const io1,
                                     int16x8_t *const io2, int16x8_t *const io3,
                                     int16x8_t *const io4, int16x8_t *const io5,
                                     int16x8_t *const io6,
                                     int16x8_t *const io7) {
  int16x4_t input_1l, input_1h, input_3l, input_3h, input_5l, input_5h,
      input_7l, input_7h;
  int16x4_t step1l[4], step1h[4];
  int16x8_t step1[8], step2[8];
  int32x4_t t32[8];
  int16x4_t t16[8];

  transpose_s16_8x8(io0, io1, io2, io3, io4, io5, io6, io7);

  // stage 1
  input_1l = vget_low_s16(*io1);
  input_1h = vget_high_s16(*io1);
  input_3l = vget_low_s16(*io3);
  input_3h = vget_high_s16(*io3);
  input_5l = vget_low_s16(*io5);
  input_5h = vget_high_s16(*io5);
  input_7l = vget_low_s16(*io7);
  input_7h = vget_high_s16(*io7);
  step1l[0] = vget_low_s16(*io0);
  step1h[0] = vget_high_s16(*io0);
  step1l[1] = vget_low_s16(*io2);
  step1h[1] = vget_high_s16(*io2);
  step1l[2] = vget_low_s16(*io4);
  step1h[2] = vget_high_s16(*io4);
  step1l[3] = vget_low_s16(*io6);
  step1h[3] = vget_high_s16(*io6);

  t32[0] = vmull_lane_s16(input_1l, cospis1, 3);
  t32[1] = vmull_lane_s16(input_1h, cospis1, 3);
  t32[2] = vmull_lane_s16(input_3l, cospis1, 2);
  t32[3] = vmull_lane_s16(input_3h, cospis1, 2);
  t32[4] = vmull_lane_s16(input_3l, cospis1, 1);
  t32[5] = vmull_lane_s16(input_3h, cospis1, 1);
  t32[6] = vmull_lane_s16(input_1l, cospis1, 0);
  t32[7] = vmull_lane_s16(input_1h, cospis1, 0);
  t32[0] = vmlsl_lane_s16(t32[0], input_7l, cospis1, 0);
  t32[1] = vmlsl_lane_s16(t32[1], input_7h, cospis1, 0);
  t32[2] = vmlal_lane_s16(t32[2], input_5l, cospis1, 1);
  t32[3] = vmlal_lane_s16(t32[3], input_5h, cospis1, 1);
  t32[4] = vmlsl_lane_s16(t32[4], input_5l, cospis1, 2);
  t32[5] = vmlsl_lane_s16(t32[5], input_5h, cospis1, 2);
  t32[6] = vmlal_lane_s16(t32[6], input_7l, cospis1, 3);
  t32[7] = vmlal_lane_s16(t32[7], input_7h, cospis1, 3);
  t16[0] = vrshrn_n_s32(t32[0], 14);
  t16[1] = vrshrn_n_s32(t32[1], 14);
  t16[2] = vrshrn_n_s32(t32[2], 14);
  t16[3] = vrshrn_n_s32(t32[3], 14);
  t16[4] = vrshrn_n_s32(t32[4], 14);
  t16[5] = vrshrn_n_s32(t32[5], 14);
  t16[6] = vrshrn_n_s32(t32[6], 14);
  t16[7] = vrshrn_n_s32(t32[7], 14);
  step1[4] = vcombine_s16(t16[0], t16[1]);
  step1[5] = vcombine_s16(t16[2], t16[3]);
  step1[6] = vcombine_s16(t16[4], t16[5]);
  step1[7] = vcombine_s16(t16[6], t16[7]);

  // stage 2
  t32[2] = vmull_lane_s16(step1l[0], cospis0, 2);
  t32[3] = vmull_lane_s16(step1h[0], cospis0, 2);
  t32[4] = vmull_lane_s16(step1l[1], cospis0, 3);
  t32[5] = vmull_lane_s16(step1h[1], cospis0, 3);
  t32[6] = vmull_lane_s16(step1l[1], cospis0, 1);
  t32[7] = vmull_lane_s16(step1h[1], cospis0, 1);
  t32[0] = vmlal_lane_s16(t32[2], step1l[2], cospis0, 2);
  t32[1] = vmlal_lane_s16(t32[3], step1h[2], cospis0, 2);
  t32[2] = vmlsl_lane_s16(t32[2], step1l[2], cospis0, 2);
  t32[3] = vmlsl_lane_s16(t32[3], step1h[2], cospis0, 2);
  t32[4] = vmlsl_lane_s16(t32[4], step1l[3], cospis0, 1);
  t32[5] = vmlsl_lane_s16(t32[5], step1h[3], cospis0, 1);
  t32[6] = vmlal_lane_s16(t32[6], step1l[3], cospis0, 3);
  t32[7] = vmlal_lane_s16(t32[7], step1h[3], cospis0, 3);
  t16[0] = vrshrn_n_s32(t32[0], 14);
  t16[1] = vrshrn_n_s32(t32[1], 14);
  t16[2] = vrshrn_n_s32(t32[2], 14);
  t16[3] = vrshrn_n_s32(t32[3], 14);
  t16[4] = vrshrn_n_s32(t32[4], 14);
  t16[5] = vrshrn_n_s32(t32[5], 14);
  t16[6] = vrshrn_n_s32(t32[6], 14);
  t16[7] = vrshrn_n_s32(t32[7], 14);
  step2[0] = vcombine_s16(t16[0], t16[1]);
  step2[1] = vcombine_s16(t16[2], t16[3]);
  step2[2] = vcombine_s16(t16[4], t16[5]);
  step2[3] = vcombine_s16(t16[6], t16[7]);

  step2[4] = vaddq_s16(step1[4], step1[5]);
  step2[5] = vsubq_s16(step1[4], step1[5]);
  step2[6] = vsubq_s16(step1[7], step1[6]);
  step2[7] = vaddq_s16(step1[7], step1[6]);

  // stage 3
  step1[0] = vaddq_s16(step2[0], step2[3]);
  step1[1] = vaddq_s16(step2[1], step2[2]);
  step1[2] = vsubq_s16(step2[1], step2[2]);
  step1[3] = vsubq_s16(step2[0], step2[3]);

  t32[2] = vmull_lane_s16(vget_low_s16(step2[6]), cospis0, 2);
  t32[3] = vmull_lane_s16(vget_high_s16(step2[6]), cospis0, 2);
  t32[0] = vmlsl_lane_s16(t32[2], vget_low_s16(step2[5]), cospis0, 2);
  t32[1] = vmlsl_lane_s16(t32[3], vget_high_s16(step2[5]), cospis0, 2);
  t32[2] = vmlal_lane_s16(t32[2], vget_low_s16(step2[5]), cospis0, 2);
  t32[3] = vmlal_lane_s16(t32[3], vget_high_s16(step2[5]), cospis0, 2);
  t16[0] = vrshrn_n_s32(t32[0], 14);
  t16[1] = vrshrn_n_s32(t32[1], 14);
  t16[2] = vrshrn_n_s32(t32[2], 14);
  t16[3] = vrshrn_n_s32(t32[3], 14);
  step1[5] = vcombine_s16(t16[0], t16[1]);
  step1[6] = vcombine_s16(t16[2], t16[3]);

  // stage 4
  *io0 = vaddq_s16(step1[0], step2[7]);
  *io1 = vaddq_s16(step1[1], step1[6]);
  *io2 = vaddq_s16(step1[2], step1[5]);
  *io3 = vaddq_s16(step1[3], step2[4]);
  *io4 = vsubq_s16(step1[3], step2[4]);
  *io5 = vsubq_s16(step1[2], step1[5]);
  *io6 = vsubq_s16(step1[1], step1[6]);
  *io7 = vsubq_s16(step1[0], step2[7]);
}

#endif  // VPX_DSP_ARM_IDCT_NEON_H_
