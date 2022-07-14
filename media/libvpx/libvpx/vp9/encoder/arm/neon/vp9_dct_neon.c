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

#include "./vpx_config.h"
#include "./vp9_rtcd.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx_dsp/txfm_common.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/arm/fdct_neon.h"

static INLINE void load_buffer_4x4(const int16_t *input, int16x8_t *in,
                                   int stride) {
  // { 0, 1, 1, 1, 1, 1, 1, 1 };
  const int16x8_t nonzero_bias_a = vextq_s16(vdupq_n_s16(0), vdupq_n_s16(1), 7);
  // { 1, 0, 0, 0, 0, 0, 0, 0 };
  const int16x8_t nonzero_bias_b = vextq_s16(vdupq_n_s16(1), vdupq_n_s16(0), 7);
  int16x8_t mask;

  int16x4_t input_0 = vshl_n_s16(vld1_s16(input + 0 * stride), 4);
  int16x4_t input_1 = vshl_n_s16(vld1_s16(input + 1 * stride), 4);
  int16x4_t input_2 = vshl_n_s16(vld1_s16(input + 2 * stride), 4);
  int16x4_t input_3 = vshl_n_s16(vld1_s16(input + 3 * stride), 4);

  in[0] = vcombine_s16(input_0, input_1);
  in[1] = vcombine_s16(input_2, input_3);

  // Copy the SSE method, use a mask to avoid an 'if' branch here to increase by
  // one non-zero first elements
  mask = vreinterpretq_s16_u16(vceqq_s16(in[0], nonzero_bias_a));
  in[0] = vaddq_s16(in[0], mask);
  in[0] = vaddq_s16(in[0], nonzero_bias_b);
}

static INLINE void write_buffer_4x4(tran_low_t *output, int16x8_t *res) {
  const int16x8_t one_s16 = vdupq_n_s16(1);
  res[0] = vaddq_s16(res[0], one_s16);
  res[1] = vaddq_s16(res[1], one_s16);
  res[0] = vshrq_n_s16(res[0], 2);
  res[1] = vshrq_n_s16(res[1], 2);
  store_s16q_to_tran_low(output + 0 * 8, res[0]);
  store_s16q_to_tran_low(output + 1 * 8, res[1]);
}

static INLINE void fadst4x4_neon(int16x8_t *in) {
  int32x4_t u0, u1, u2, u3;
  int16x4_t out_0, out_1, out_2, out_3;
  const int32x4_t k__DCT_CONST_ROUNDING = vdupq_n_s32(DCT_CONST_ROUNDING);

  const int16x4_t s0 = vget_low_s16(in[0]);   // | x_00 | x_01 | x_02 | x_03 |
  const int16x4_t s1 = vget_high_s16(in[0]);  // | x_10 | x_11 | x_12 | x_13 |
  const int16x4_t s2 = vget_low_s16(in[1]);   // | x_20 | x_21 | x_22 | x_23 |
  const int16x4_t s3 = vget_high_s16(in[1]);  // | x_30 | x_31 | x_32 | x_33 |

  // s0 * sinpi_1_9, s0 * sinpi_4_9
  // Must expand all elements to s32. See 'needs32' comment in fwd_txfm.c.
  const int32x4_t s0s1_9 = vmull_n_s16(s0, sinpi_1_9);
  const int32x4_t s0s4_9 = vmull_n_s16(s0, sinpi_4_9);
  // s1 * sinpi_1_9, s1 * sinpi_2_9
  const int32x4_t s1s1_9 = vmull_n_s16(s1, sinpi_1_9);
  const int32x4_t s1s2_9 = vmull_n_s16(s1, sinpi_2_9);
  // s2 * sinpi_3_9
  const int32x4_t s2s3_9 = vmull_n_s16(s2, sinpi_3_9);
  // s3 * sinpi_2_9, s3 * sinpi_4_9
  const int32x4_t s3s2_9 = vmull_n_s16(s3, sinpi_2_9);
  const int32x4_t s3s4_9 = vmull_n_s16(s3, sinpi_4_9);

  // (s0 + s1) * sinpi_3_9
  const int32x4_t s0_p_s1 = vaddl_s16(s0, s1);
  const int32x4_t s0_p_s1_m_s3 = vsubw_s16(s0_p_s1, s3);

  // s_0 * sinpi_1_9 + s_1 * sinpi_2_9
  // s_0 * sinpi_4_9 - s_1 * sinpi_1_9
  const int32x4_t s0s1_9_p_s1s2_9 = vaddq_s32(s0s1_9, s1s2_9);
  const int32x4_t s0s4_9_m_s1s1_9 = vsubq_s32(s0s4_9, s1s1_9);
  /*
   * t0 = s0s1_9 + s1s2_9 + s3s4_9
   * t1 = (s0 + s1) * sinpi_3_9 - s3 * sinpi_3_9
   * t2 = s0s4_9 - s1s1_9 + s3s2_9
   * t3 = s2s3_9
   */
  const int32x4_t t0 = vaddq_s32(s0s1_9_p_s1s2_9, s3s4_9);
  const int32x4_t t1 = vmulq_n_s32(s0_p_s1_m_s3, sinpi_3_9);
  const int32x4_t t2 = vaddq_s32(s0s4_9_m_s1s1_9, s3s2_9);
  const int32x4_t t3 = s2s3_9;
  /*
   * u0 = t0 + t3
   * u1 = t1
   * u2 = t2 - t3
   * u3 = t2 - t0 + t3
   */
  u0 = vaddq_s32(t0, t3);
  u1 = t1;
  u2 = vsubq_s32(t2, t3);
  u3 = vaddq_s32(vsubq_s32(t2, t0), t3);

  // fdct_round_shift
  u0 = vaddq_s32(u0, k__DCT_CONST_ROUNDING);
  u1 = vaddq_s32(u1, k__DCT_CONST_ROUNDING);
  u2 = vaddq_s32(u2, k__DCT_CONST_ROUNDING);
  u3 = vaddq_s32(u3, k__DCT_CONST_ROUNDING);

  out_0 = vshrn_n_s32(u0, DCT_CONST_BITS);
  out_1 = vshrn_n_s32(u1, DCT_CONST_BITS);
  out_2 = vshrn_n_s32(u2, DCT_CONST_BITS);
  out_3 = vshrn_n_s32(u3, DCT_CONST_BITS);

  transpose_s16_4x4d(&out_0, &out_1, &out_2, &out_3);

  in[0] = vcombine_s16(out_0, out_1);
  in[1] = vcombine_s16(out_2, out_3);
}

void vp9_fht4x4_neon(const int16_t *input, tran_low_t *output, int stride,
                     int tx_type) {
  int16x8_t in[2];

  switch (tx_type) {
    case DCT_DCT: vpx_fdct4x4_neon(input, output, stride); break;
    case ADST_DCT:
      load_buffer_4x4(input, in, stride);
      fadst4x4_neon(in);
      vpx_fdct4x4_pass1_neon((int16x4_t *)in);
      write_buffer_4x4(output, in);
      break;
    case DCT_ADST:
      load_buffer_4x4(input, in, stride);
      vpx_fdct4x4_pass1_neon((int16x4_t *)in);
      fadst4x4_neon(in);
      write_buffer_4x4(output, in);
      break;
    default:
      assert(tx_type == ADST_ADST);
      load_buffer_4x4(input, in, stride);
      fadst4x4_neon(in);
      fadst4x4_neon(in);
      write_buffer_4x4(output, in);
      break;
  }
}

static INLINE void load_buffer_8x8(const int16_t *input, int16x8_t *in,
                                   int stride) {
  in[0] = vshlq_n_s16(vld1q_s16(input + 0 * stride), 2);
  in[1] = vshlq_n_s16(vld1q_s16(input + 1 * stride), 2);
  in[2] = vshlq_n_s16(vld1q_s16(input + 2 * stride), 2);
  in[3] = vshlq_n_s16(vld1q_s16(input + 3 * stride), 2);
  in[4] = vshlq_n_s16(vld1q_s16(input + 4 * stride), 2);
  in[5] = vshlq_n_s16(vld1q_s16(input + 5 * stride), 2);
  in[6] = vshlq_n_s16(vld1q_s16(input + 6 * stride), 2);
  in[7] = vshlq_n_s16(vld1q_s16(input + 7 * stride), 2);
}

/* right shift and rounding
 * first get the sign bit (bit 15).
 * If bit == 1, it's the simple case of shifting right by one bit.
 * If bit == 2, it essentially computes the expression:
 *
 * out[j * 16 + i] = (temp_out[j] + 1 + (temp_out[j] < 0)) >> 2;
 *
 * for each row.
 */
static INLINE void right_shift_8x8(int16x8_t *res, const int bit) {
  int16x8_t sign0 = vshrq_n_s16(res[0], 15);
  int16x8_t sign1 = vshrq_n_s16(res[1], 15);
  int16x8_t sign2 = vshrq_n_s16(res[2], 15);
  int16x8_t sign3 = vshrq_n_s16(res[3], 15);
  int16x8_t sign4 = vshrq_n_s16(res[4], 15);
  int16x8_t sign5 = vshrq_n_s16(res[5], 15);
  int16x8_t sign6 = vshrq_n_s16(res[6], 15);
  int16x8_t sign7 = vshrq_n_s16(res[7], 15);

  if (bit == 2) {
    const int16x8_t const_rounding = vdupq_n_s16(1);
    res[0] = vaddq_s16(res[0], const_rounding);
    res[1] = vaddq_s16(res[1], const_rounding);
    res[2] = vaddq_s16(res[2], const_rounding);
    res[3] = vaddq_s16(res[3], const_rounding);
    res[4] = vaddq_s16(res[4], const_rounding);
    res[5] = vaddq_s16(res[5], const_rounding);
    res[6] = vaddq_s16(res[6], const_rounding);
    res[7] = vaddq_s16(res[7], const_rounding);
  }

  res[0] = vsubq_s16(res[0], sign0);
  res[1] = vsubq_s16(res[1], sign1);
  res[2] = vsubq_s16(res[2], sign2);
  res[3] = vsubq_s16(res[3], sign3);
  res[4] = vsubq_s16(res[4], sign4);
  res[5] = vsubq_s16(res[5], sign5);
  res[6] = vsubq_s16(res[6], sign6);
  res[7] = vsubq_s16(res[7], sign7);

  if (bit == 1) {
    res[0] = vshrq_n_s16(res[0], 1);
    res[1] = vshrq_n_s16(res[1], 1);
    res[2] = vshrq_n_s16(res[2], 1);
    res[3] = vshrq_n_s16(res[3], 1);
    res[4] = vshrq_n_s16(res[4], 1);
    res[5] = vshrq_n_s16(res[5], 1);
    res[6] = vshrq_n_s16(res[6], 1);
    res[7] = vshrq_n_s16(res[7], 1);
  } else {
    res[0] = vshrq_n_s16(res[0], 2);
    res[1] = vshrq_n_s16(res[1], 2);
    res[2] = vshrq_n_s16(res[2], 2);
    res[3] = vshrq_n_s16(res[3], 2);
    res[4] = vshrq_n_s16(res[4], 2);
    res[5] = vshrq_n_s16(res[5], 2);
    res[6] = vshrq_n_s16(res[6], 2);
    res[7] = vshrq_n_s16(res[7], 2);
  }
}

static INLINE void write_buffer_8x8(tran_low_t *output, int16x8_t *res,
                                    int stride) {
  store_s16q_to_tran_low(output + 0 * stride, res[0]);
  store_s16q_to_tran_low(output + 1 * stride, res[1]);
  store_s16q_to_tran_low(output + 2 * stride, res[2]);
  store_s16q_to_tran_low(output + 3 * stride, res[3]);
  store_s16q_to_tran_low(output + 4 * stride, res[4]);
  store_s16q_to_tran_low(output + 5 * stride, res[5]);
  store_s16q_to_tran_low(output + 6 * stride, res[6]);
  store_s16q_to_tran_low(output + 7 * stride, res[7]);
}

static INLINE void fadst8x8_neon(int16x8_t *in) {
  int16x4_t x0_lo, x0_hi, x1_lo, x1_hi, x2_lo, x2_hi, x3_lo, x3_hi, x4_lo,
      x4_hi, x5_lo, x5_hi, x6_lo, x6_hi, x7_lo, x7_hi;
  int32x4_t s0_lo, s0_hi, s1_lo, s1_hi, s2_lo, s2_hi, s3_lo, s3_hi, s4_lo,
      s4_hi, s5_lo, s5_hi, s6_lo, s6_hi, s7_lo, s7_hi;
  int32x4_t t0_lo, t0_hi, t1_lo, t1_hi, t2_lo, t2_hi, t3_lo, t3_hi, t4_lo,
      t4_hi, t5_lo, t5_hi, t6_lo, t6_hi, t7_lo, t7_hi;
  const int32x4_t k__DCT_CONST_ROUNDING = vdupq_n_s32(DCT_CONST_ROUNDING);

  x0_lo = vget_low_s16(in[7]);
  x0_hi = vget_high_s16(in[7]);
  x1_lo = vget_low_s16(in[0]);
  x1_hi = vget_high_s16(in[0]);
  x2_lo = vget_low_s16(in[5]);
  x2_hi = vget_high_s16(in[5]);
  x3_lo = vget_low_s16(in[2]);
  x3_hi = vget_high_s16(in[2]);
  x4_lo = vget_low_s16(in[3]);
  x4_hi = vget_high_s16(in[3]);
  x5_lo = vget_low_s16(in[4]);
  x5_hi = vget_high_s16(in[4]);
  x6_lo = vget_low_s16(in[1]);
  x6_hi = vget_high_s16(in[1]);
  x7_lo = vget_low_s16(in[6]);
  x7_hi = vget_high_s16(in[6]);

  // stage 1
  // s0 = cospi_2_64 * x0 + cospi_30_64 * x1;
  s0_lo = vaddq_s32(vmull_n_s16(x0_lo, cospi_2_64),
                    vmull_n_s16(x1_lo, cospi_30_64));
  s0_hi = vaddq_s32(vmull_n_s16(x0_hi, cospi_2_64),
                    vmull_n_s16(x1_hi, cospi_30_64));
  // s1 = cospi_30_64 * x0 - cospi_2_64 * x1;
  s1_lo = vsubq_s32(vmull_n_s16(x0_lo, cospi_30_64),
                    vmull_n_s16(x1_lo, cospi_2_64));
  s1_hi = vsubq_s32(vmull_n_s16(x0_hi, cospi_30_64),
                    vmull_n_s16(x1_hi, cospi_2_64));
  // s2 = cospi_10_64 * x2 + cospi_22_64 * x3;
  s2_lo = vaddq_s32(vmull_n_s16(x2_lo, cospi_10_64),
                    vmull_n_s16(x3_lo, cospi_22_64));
  s2_hi = vaddq_s32(vmull_n_s16(x2_hi, cospi_10_64),
                    vmull_n_s16(x3_hi, cospi_22_64));
  // s3 = cospi_22_64 * x2 - cospi_10_64 * x3;
  s3_lo = vsubq_s32(vmull_n_s16(x2_lo, cospi_22_64),
                    vmull_n_s16(x3_lo, cospi_10_64));
  s3_hi = vsubq_s32(vmull_n_s16(x2_hi, cospi_22_64),
                    vmull_n_s16(x3_hi, cospi_10_64));
  // s4 = cospi_18_64 * x4 + cospi_14_64 * x5;
  s4_lo = vaddq_s32(vmull_n_s16(x4_lo, cospi_18_64),
                    vmull_n_s16(x5_lo, cospi_14_64));
  s4_hi = vaddq_s32(vmull_n_s16(x4_hi, cospi_18_64),
                    vmull_n_s16(x5_hi, cospi_14_64));
  // s5 = cospi_14_64 * x4 - cospi_18_64 * x5;
  s5_lo = vsubq_s32(vmull_n_s16(x4_lo, cospi_14_64),
                    vmull_n_s16(x5_lo, cospi_18_64));
  s5_hi = vsubq_s32(vmull_n_s16(x4_hi, cospi_14_64),
                    vmull_n_s16(x5_hi, cospi_18_64));
  // s6 = cospi_26_64 * x6 + cospi_6_64 * x7;
  s6_lo = vaddq_s32(vmull_n_s16(x6_lo, cospi_26_64),
                    vmull_n_s16(x7_lo, cospi_6_64));
  s6_hi = vaddq_s32(vmull_n_s16(x6_hi, cospi_26_64),
                    vmull_n_s16(x7_hi, cospi_6_64));
  // s7 = cospi_6_64 * x6 - cospi_26_64 * x7;
  s7_lo = vsubq_s32(vmull_n_s16(x6_lo, cospi_6_64),
                    vmull_n_s16(x7_lo, cospi_26_64));
  s7_hi = vsubq_s32(vmull_n_s16(x6_hi, cospi_6_64),
                    vmull_n_s16(x7_hi, cospi_26_64));

  // fdct_round_shift
  t0_lo = vaddq_s32(s0_lo, s4_lo);
  t0_hi = vaddq_s32(s0_hi, s4_hi);
  t1_lo = vaddq_s32(s1_lo, s5_lo);
  t1_hi = vaddq_s32(s1_hi, s5_hi);
  t2_lo = vaddq_s32(s2_lo, s6_lo);
  t2_hi = vaddq_s32(s2_hi, s6_hi);
  t3_lo = vaddq_s32(s3_lo, s7_lo);
  t3_hi = vaddq_s32(s3_hi, s7_hi);
  t4_lo = vsubq_s32(s0_lo, s4_lo);
  t4_hi = vsubq_s32(s0_hi, s4_hi);
  t5_lo = vsubq_s32(s1_lo, s5_lo);
  t5_hi = vsubq_s32(s1_hi, s5_hi);
  t6_lo = vsubq_s32(s2_lo, s6_lo);
  t6_hi = vsubq_s32(s2_hi, s6_hi);
  t7_lo = vsubq_s32(s3_lo, s7_lo);
  t7_hi = vsubq_s32(s3_hi, s7_hi);

  t0_lo = vaddq_s32(t0_lo, k__DCT_CONST_ROUNDING);
  t0_hi = vaddq_s32(t0_hi, k__DCT_CONST_ROUNDING);
  t1_lo = vaddq_s32(t1_lo, k__DCT_CONST_ROUNDING);
  t1_hi = vaddq_s32(t1_hi, k__DCT_CONST_ROUNDING);
  t2_lo = vaddq_s32(t2_lo, k__DCT_CONST_ROUNDING);
  t2_hi = vaddq_s32(t2_hi, k__DCT_CONST_ROUNDING);
  t3_lo = vaddq_s32(t3_lo, k__DCT_CONST_ROUNDING);
  t3_hi = vaddq_s32(t3_hi, k__DCT_CONST_ROUNDING);
  t4_lo = vaddq_s32(t4_lo, k__DCT_CONST_ROUNDING);
  t4_hi = vaddq_s32(t4_hi, k__DCT_CONST_ROUNDING);
  t5_lo = vaddq_s32(t5_lo, k__DCT_CONST_ROUNDING);
  t5_hi = vaddq_s32(t5_hi, k__DCT_CONST_ROUNDING);
  t6_lo = vaddq_s32(t6_lo, k__DCT_CONST_ROUNDING);
  t6_hi = vaddq_s32(t6_hi, k__DCT_CONST_ROUNDING);
  t7_lo = vaddq_s32(t7_lo, k__DCT_CONST_ROUNDING);
  t7_hi = vaddq_s32(t7_hi, k__DCT_CONST_ROUNDING);

  t0_lo = vshrq_n_s32(t0_lo, DCT_CONST_BITS);
  t0_hi = vshrq_n_s32(t0_hi, DCT_CONST_BITS);
  t1_lo = vshrq_n_s32(t1_lo, DCT_CONST_BITS);
  t1_hi = vshrq_n_s32(t1_hi, DCT_CONST_BITS);
  t2_lo = vshrq_n_s32(t2_lo, DCT_CONST_BITS);
  t2_hi = vshrq_n_s32(t2_hi, DCT_CONST_BITS);
  t3_lo = vshrq_n_s32(t3_lo, DCT_CONST_BITS);
  t3_hi = vshrq_n_s32(t3_hi, DCT_CONST_BITS);
  t4_lo = vshrq_n_s32(t4_lo, DCT_CONST_BITS);
  t4_hi = vshrq_n_s32(t4_hi, DCT_CONST_BITS);
  t5_lo = vshrq_n_s32(t5_lo, DCT_CONST_BITS);
  t5_hi = vshrq_n_s32(t5_hi, DCT_CONST_BITS);
  t6_lo = vshrq_n_s32(t6_lo, DCT_CONST_BITS);
  t6_hi = vshrq_n_s32(t6_hi, DCT_CONST_BITS);
  t7_lo = vshrq_n_s32(t7_lo, DCT_CONST_BITS);
  t7_hi = vshrq_n_s32(t7_hi, DCT_CONST_BITS);

  // stage 2
  s0_lo = t0_lo;
  s0_hi = t0_hi;
  s1_lo = t1_lo;
  s1_hi = t1_hi;
  s2_lo = t2_lo;
  s2_hi = t2_hi;
  s3_lo = t3_lo;
  s3_hi = t3_hi;
  s4_lo = vaddq_s32(vmulq_n_s32(t4_lo, cospi_8_64),
                    vmulq_n_s32(t5_lo, cospi_24_64));
  s4_hi = vaddq_s32(vmulq_n_s32(t4_hi, cospi_8_64),
                    vmulq_n_s32(t5_hi, cospi_24_64));
  s5_lo = vsubq_s32(vmulq_n_s32(t4_lo, cospi_24_64),
                    vmulq_n_s32(t5_lo, cospi_8_64));
  s5_hi = vsubq_s32(vmulq_n_s32(t4_hi, cospi_24_64),
                    vmulq_n_s32(t5_hi, cospi_8_64));
  s6_lo = vaddq_s32(vmulq_n_s32(t6_lo, -cospi_24_64),
                    vmulq_n_s32(t7_lo, cospi_8_64));
  s6_hi = vaddq_s32(vmulq_n_s32(t6_hi, -cospi_24_64),
                    vmulq_n_s32(t7_hi, cospi_8_64));
  s7_lo = vaddq_s32(vmulq_n_s32(t6_lo, cospi_8_64),
                    vmulq_n_s32(t7_lo, cospi_24_64));
  s7_hi = vaddq_s32(vmulq_n_s32(t6_hi, cospi_8_64),
                    vmulq_n_s32(t7_hi, cospi_24_64));

  // s0 + s2
  t0_lo = vaddq_s32(s0_lo, s2_lo);
  t0_hi = vaddq_s32(s0_hi, s2_hi);
  // s1 + s3
  t1_lo = vaddq_s32(s1_lo, s3_lo);
  t1_hi = vaddq_s32(s1_hi, s3_hi);
  // s0 - s2
  t2_lo = vsubq_s32(s0_lo, s2_lo);
  t2_hi = vsubq_s32(s0_hi, s2_hi);
  // s1 - s3
  t3_lo = vsubq_s32(s1_lo, s3_lo);
  t3_hi = vsubq_s32(s1_hi, s3_hi);
  // s4 + s6
  t4_lo = vaddq_s32(s4_lo, s6_lo);
  t4_hi = vaddq_s32(s4_hi, s6_hi);
  // s5 + s7
  t5_lo = vaddq_s32(s5_lo, s7_lo);
  t5_hi = vaddq_s32(s5_hi, s7_hi);
  // s4 - s6
  t6_lo = vsubq_s32(s4_lo, s6_lo);
  t6_hi = vsubq_s32(s4_hi, s6_hi);
  // s5 - s7
  t7_lo = vsubq_s32(s5_lo, s7_lo);
  t7_hi = vsubq_s32(s5_hi, s7_hi);

  // fdct_round_shift
  t4_lo = vaddq_s32(t4_lo, k__DCT_CONST_ROUNDING);
  t4_hi = vaddq_s32(t4_hi, k__DCT_CONST_ROUNDING);
  t5_lo = vaddq_s32(t5_lo, k__DCT_CONST_ROUNDING);
  t5_hi = vaddq_s32(t5_hi, k__DCT_CONST_ROUNDING);
  t6_lo = vaddq_s32(t6_lo, k__DCT_CONST_ROUNDING);
  t6_hi = vaddq_s32(t6_hi, k__DCT_CONST_ROUNDING);
  t7_lo = vaddq_s32(t7_lo, k__DCT_CONST_ROUNDING);
  t7_hi = vaddq_s32(t7_hi, k__DCT_CONST_ROUNDING);
  t4_lo = vshrq_n_s32(t4_lo, DCT_CONST_BITS);
  t4_hi = vshrq_n_s32(t4_hi, DCT_CONST_BITS);
  t5_lo = vshrq_n_s32(t5_lo, DCT_CONST_BITS);
  t5_hi = vshrq_n_s32(t5_hi, DCT_CONST_BITS);
  t6_lo = vshrq_n_s32(t6_lo, DCT_CONST_BITS);
  t6_hi = vshrq_n_s32(t6_hi, DCT_CONST_BITS);
  t7_lo = vshrq_n_s32(t7_lo, DCT_CONST_BITS);
  t7_hi = vshrq_n_s32(t7_hi, DCT_CONST_BITS);

  // stage 3
  // cospi_16_64 * (x2 + x3)
  s2_lo = vmulq_n_s32(vaddq_s32(t2_lo, t3_lo), cospi_16_64);
  s2_hi = vmulq_n_s32(vaddq_s32(t2_hi, t3_hi), cospi_16_64);
  // cospi_16_64 * (x2 - x3)
  s3_lo = vmulq_n_s32(vsubq_s32(t2_lo, t3_lo), cospi_16_64);
  s3_hi = vmulq_n_s32(vsubq_s32(t2_hi, t3_hi), cospi_16_64);
  // cospi_16_64 * (x6 + x7)
  s6_lo = vmulq_n_s32(vaddq_s32(t6_lo, t7_lo), cospi_16_64);
  s6_hi = vmulq_n_s32(vaddq_s32(t6_hi, t7_hi), cospi_16_64);
  // cospi_16_64 * (x2 - x3)
  s7_lo = vmulq_n_s32(vsubq_s32(t6_lo, t7_lo), cospi_16_64);
  s7_hi = vmulq_n_s32(vsubq_s32(t6_hi, t7_hi), cospi_16_64);

  // final fdct_round_shift
  t2_lo = vaddq_s32(s2_lo, k__DCT_CONST_ROUNDING);
  t2_hi = vaddq_s32(s2_hi, k__DCT_CONST_ROUNDING);
  t3_lo = vaddq_s32(s3_lo, k__DCT_CONST_ROUNDING);
  t3_hi = vaddq_s32(s3_hi, k__DCT_CONST_ROUNDING);
  t6_lo = vaddq_s32(s6_lo, k__DCT_CONST_ROUNDING);
  t6_hi = vaddq_s32(s6_hi, k__DCT_CONST_ROUNDING);
  t7_lo = vaddq_s32(s7_lo, k__DCT_CONST_ROUNDING);
  t7_hi = vaddq_s32(s7_hi, k__DCT_CONST_ROUNDING);

  x2_lo = vshrn_n_s32(t2_lo, DCT_CONST_BITS);
  x2_hi = vshrn_n_s32(t2_hi, DCT_CONST_BITS);
  x3_lo = vshrn_n_s32(t3_lo, DCT_CONST_BITS);
  x3_hi = vshrn_n_s32(t3_hi, DCT_CONST_BITS);
  x6_lo = vshrn_n_s32(t6_lo, DCT_CONST_BITS);
  x6_hi = vshrn_n_s32(t6_hi, DCT_CONST_BITS);
  x7_lo = vshrn_n_s32(t7_lo, DCT_CONST_BITS);
  x7_hi = vshrn_n_s32(t7_hi, DCT_CONST_BITS);

  // x0, x1, x4, x5 narrow down to 16-bits directly
  x0_lo = vmovn_s32(t0_lo);
  x0_hi = vmovn_s32(t0_hi);
  x1_lo = vmovn_s32(t1_lo);
  x1_hi = vmovn_s32(t1_hi);
  x4_lo = vmovn_s32(t4_lo);
  x4_hi = vmovn_s32(t4_hi);
  x5_lo = vmovn_s32(t5_lo);
  x5_hi = vmovn_s32(t5_hi);

  in[0] = vcombine_s16(x0_lo, x0_hi);
  in[1] = vnegq_s16(vcombine_s16(x4_lo, x4_hi));
  in[2] = vcombine_s16(x6_lo, x6_hi);
  in[3] = vnegq_s16(vcombine_s16(x2_lo, x2_hi));
  in[4] = vcombine_s16(x3_lo, x3_hi);
  in[5] = vnegq_s16(vcombine_s16(x7_lo, x7_hi));
  in[6] = vcombine_s16(x5_lo, x5_hi);
  in[7] = vnegq_s16(vcombine_s16(x1_lo, x1_hi));

  transpose_s16_8x8(&in[0], &in[1], &in[2], &in[3], &in[4], &in[5], &in[6],
                    &in[7]);
}

void vp9_fht8x8_neon(const int16_t *input, tran_low_t *output, int stride,
                     int tx_type) {
  int16x8_t in[8];

  switch (tx_type) {
    case DCT_DCT: vpx_fdct8x8_neon(input, output, stride); break;
    case ADST_DCT:
      load_buffer_8x8(input, in, stride);
      fadst8x8_neon(in);
      vpx_fdct8x8_pass1_neon(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case DCT_ADST:
      load_buffer_8x8(input, in, stride);
      vpx_fdct8x8_pass1_neon(in);
      fadst8x8_neon(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    default:
      assert(tx_type == ADST_ADST);
      load_buffer_8x8(input, in, stride);
      fadst8x8_neon(in);
      fadst8x8_neon(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
  }
}

static INLINE void load_buffer_16x16(const int16_t *input, int16x8_t *in0,
                                     int16x8_t *in1, int stride) {
  // load first 8 columns
  load_buffer_8x8(input, in0, stride);
  load_buffer_8x8(input + 8 * stride, in0 + 8, stride);

  input += 8;
  // load second 8 columns
  load_buffer_8x8(input, in1, stride);
  load_buffer_8x8(input + 8 * stride, in1 + 8, stride);
}

static INLINE void write_buffer_16x16(tran_low_t *output, int16x8_t *in0,
                                      int16x8_t *in1, int stride) {
  // write first 8 columns
  write_buffer_8x8(output, in0, stride);
  write_buffer_8x8(output + 8 * stride, in0 + 8, stride);

  // write second 8 columns
  output += 8;
  write_buffer_8x8(output, in1, stride);
  write_buffer_8x8(output + 8 * stride, in1 + 8, stride);
}

static INLINE void right_shift_16x16(int16x8_t *res0, int16x8_t *res1) {
  // perform rounding operations
  right_shift_8x8(res0, 2);
  right_shift_8x8(res0 + 8, 2);
  right_shift_8x8(res1, 2);
  right_shift_8x8(res1 + 8, 2);
}

static void fdct16_8col(int16x8_t *in) {
  // perform 16x16 1-D DCT for 8 columns
  int16x8_t i[8], s1[8], s2[8], s3[8], t[8];
  int16x4_t t_lo[8], t_hi[8];
  int32x4_t u_lo[8], u_hi[8];
  const int32x4_t k__DCT_CONST_ROUNDING = vdupq_n_s32(DCT_CONST_ROUNDING);

  // stage 1
  i[0] = vaddq_s16(in[0], in[15]);
  i[1] = vaddq_s16(in[1], in[14]);
  i[2] = vaddq_s16(in[2], in[13]);
  i[3] = vaddq_s16(in[3], in[12]);
  i[4] = vaddq_s16(in[4], in[11]);
  i[5] = vaddq_s16(in[5], in[10]);
  i[6] = vaddq_s16(in[6], in[9]);
  i[7] = vaddq_s16(in[7], in[8]);

  vpx_fdct8x8_pass1_neon(i);
  transpose_s16_8x8(&i[0], &i[1], &i[2], &i[3], &i[4], &i[5], &i[6], &i[7]);

  // step 2
  s1[0] = vsubq_s16(in[7], in[8]);
  s1[1] = vsubq_s16(in[6], in[9]);
  s1[2] = vsubq_s16(in[5], in[10]);
  s1[3] = vsubq_s16(in[4], in[11]);
  s1[4] = vsubq_s16(in[3], in[12]);
  s1[5] = vsubq_s16(in[2], in[13]);
  s1[6] = vsubq_s16(in[1], in[14]);
  s1[7] = vsubq_s16(in[0], in[15]);

  t[2] = vsubq_s16(s1[5], s1[2]);
  t[3] = vsubq_s16(s1[4], s1[3]);
  t[4] = vaddq_s16(s1[4], s1[3]);
  t[5] = vaddq_s16(s1[5], s1[2]);

  t_lo[2] = vget_low_s16(t[2]);
  t_hi[2] = vget_high_s16(t[2]);
  t_lo[3] = vget_low_s16(t[3]);
  t_hi[3] = vget_high_s16(t[3]);
  t_lo[4] = vget_low_s16(t[4]);
  t_hi[4] = vget_high_s16(t[4]);
  t_lo[5] = vget_low_s16(t[5]);
  t_hi[5] = vget_high_s16(t[5]);

  u_lo[2] = vmull_n_s16(t_lo[2], cospi_16_64);
  u_hi[2] = vmull_n_s16(t_hi[2], cospi_16_64);
  u_lo[3] = vmull_n_s16(t_lo[3], cospi_16_64);
  u_hi[3] = vmull_n_s16(t_hi[3], cospi_16_64);
  u_lo[4] = vmull_n_s16(t_lo[4], cospi_16_64);
  u_hi[4] = vmull_n_s16(t_hi[4], cospi_16_64);
  u_lo[5] = vmull_n_s16(t_lo[5], cospi_16_64);
  u_hi[5] = vmull_n_s16(t_hi[5], cospi_16_64);

  u_lo[2] = vaddq_s32(u_lo[2], k__DCT_CONST_ROUNDING);
  u_hi[2] = vaddq_s32(u_hi[2], k__DCT_CONST_ROUNDING);
  u_lo[3] = vaddq_s32(u_lo[3], k__DCT_CONST_ROUNDING);
  u_hi[3] = vaddq_s32(u_hi[3], k__DCT_CONST_ROUNDING);
  u_lo[4] = vaddq_s32(u_lo[4], k__DCT_CONST_ROUNDING);
  u_hi[4] = vaddq_s32(u_hi[4], k__DCT_CONST_ROUNDING);
  u_lo[5] = vaddq_s32(u_lo[5], k__DCT_CONST_ROUNDING);
  u_hi[5] = vaddq_s32(u_hi[5], k__DCT_CONST_ROUNDING);

  t_lo[2] = vshrn_n_s32(u_lo[2], DCT_CONST_BITS);
  t_hi[2] = vshrn_n_s32(u_hi[2], DCT_CONST_BITS);
  t_lo[3] = vshrn_n_s32(u_lo[3], DCT_CONST_BITS);
  t_hi[3] = vshrn_n_s32(u_hi[3], DCT_CONST_BITS);
  t_lo[4] = vshrn_n_s32(u_lo[4], DCT_CONST_BITS);
  t_hi[4] = vshrn_n_s32(u_hi[4], DCT_CONST_BITS);
  t_lo[5] = vshrn_n_s32(u_lo[5], DCT_CONST_BITS);
  t_hi[5] = vshrn_n_s32(u_hi[5], DCT_CONST_BITS);

  s2[2] = vcombine_s16(t_lo[2], t_hi[2]);
  s2[3] = vcombine_s16(t_lo[3], t_hi[3]);
  s2[4] = vcombine_s16(t_lo[4], t_hi[4]);
  s2[5] = vcombine_s16(t_lo[5], t_hi[5]);

  // step 3
  s3[0] = vaddq_s16(s1[0], s2[3]);
  s3[1] = vaddq_s16(s1[1], s2[2]);
  s3[2] = vsubq_s16(s1[1], s2[2]);
  s3[3] = vsubq_s16(s1[0], s2[3]);
  s3[4] = vsubq_s16(s1[7], s2[4]);
  s3[5] = vsubq_s16(s1[6], s2[5]);
  s3[6] = vaddq_s16(s1[6], s2[5]);
  s3[7] = vaddq_s16(s1[7], s2[4]);

  // step 4
  t_lo[0] = vget_low_s16(s3[0]);
  t_hi[0] = vget_high_s16(s3[0]);
  t_lo[1] = vget_low_s16(s3[1]);
  t_hi[1] = vget_high_s16(s3[1]);
  t_lo[2] = vget_low_s16(s3[2]);
  t_hi[2] = vget_high_s16(s3[2]);
  t_lo[3] = vget_low_s16(s3[3]);
  t_hi[3] = vget_high_s16(s3[3]);
  t_lo[4] = vget_low_s16(s3[4]);
  t_hi[4] = vget_high_s16(s3[4]);
  t_lo[5] = vget_low_s16(s3[5]);
  t_hi[5] = vget_high_s16(s3[5]);
  t_lo[6] = vget_low_s16(s3[6]);
  t_hi[6] = vget_high_s16(s3[6]);
  t_lo[7] = vget_low_s16(s3[7]);
  t_hi[7] = vget_high_s16(s3[7]);

  u_lo[1] = vaddq_s32(vmull_n_s16(t_lo[1], -cospi_8_64),
                      vmull_n_s16(t_lo[6], cospi_24_64));
  u_hi[1] = vaddq_s32(vmull_n_s16(t_hi[1], -cospi_8_64),
                      vmull_n_s16(t_hi[6], cospi_24_64));
  u_lo[2] = vaddq_s32(vmull_n_s16(t_lo[2], cospi_24_64),
                      vmull_n_s16(t_lo[5], cospi_8_64));
  u_hi[2] = vaddq_s32(vmull_n_s16(t_hi[2], cospi_24_64),
                      vmull_n_s16(t_hi[5], cospi_8_64));
  u_lo[5] = vaddq_s32(vmull_n_s16(t_lo[2], cospi_8_64),
                      vmull_n_s16(t_lo[5], -cospi_24_64));
  u_hi[5] = vaddq_s32(vmull_n_s16(t_hi[2], cospi_8_64),
                      vmull_n_s16(t_hi[5], -cospi_24_64));
  u_lo[6] = vaddq_s32(vmull_n_s16(t_lo[1], cospi_24_64),
                      vmull_n_s16(t_lo[6], cospi_8_64));
  u_hi[6] = vaddq_s32(vmull_n_s16(t_hi[1], cospi_24_64),
                      vmull_n_s16(t_hi[6], cospi_8_64));

  u_lo[1] = vaddq_s32(u_lo[1], k__DCT_CONST_ROUNDING);
  u_hi[1] = vaddq_s32(u_hi[1], k__DCT_CONST_ROUNDING);
  u_lo[2] = vaddq_s32(u_lo[2], k__DCT_CONST_ROUNDING);
  u_hi[2] = vaddq_s32(u_hi[2], k__DCT_CONST_ROUNDING);
  u_lo[5] = vaddq_s32(u_lo[5], k__DCT_CONST_ROUNDING);
  u_hi[5] = vaddq_s32(u_hi[5], k__DCT_CONST_ROUNDING);
  u_lo[6] = vaddq_s32(u_lo[6], k__DCT_CONST_ROUNDING);
  u_hi[6] = vaddq_s32(u_hi[6], k__DCT_CONST_ROUNDING);

  t_lo[1] = vshrn_n_s32(u_lo[1], DCT_CONST_BITS);
  t_hi[1] = vshrn_n_s32(u_hi[1], DCT_CONST_BITS);
  t_lo[2] = vshrn_n_s32(u_lo[2], DCT_CONST_BITS);
  t_hi[2] = vshrn_n_s32(u_hi[2], DCT_CONST_BITS);
  t_lo[5] = vshrn_n_s32(u_lo[5], DCT_CONST_BITS);
  t_hi[5] = vshrn_n_s32(u_hi[5], DCT_CONST_BITS);
  t_lo[6] = vshrn_n_s32(u_lo[6], DCT_CONST_BITS);
  t_hi[6] = vshrn_n_s32(u_hi[6], DCT_CONST_BITS);

  s2[1] = vcombine_s16(t_lo[1], t_hi[1]);
  s2[2] = vcombine_s16(t_lo[2], t_hi[2]);
  s2[5] = vcombine_s16(t_lo[5], t_hi[5]);
  s2[6] = vcombine_s16(t_lo[6], t_hi[6]);

  // step 5
  s1[0] = vaddq_s16(s3[0], s2[1]);
  s1[1] = vsubq_s16(s3[0], s2[1]);
  s1[2] = vaddq_s16(s3[3], s2[2]);
  s1[3] = vsubq_s16(s3[3], s2[2]);
  s1[4] = vsubq_s16(s3[4], s2[5]);
  s1[5] = vaddq_s16(s3[4], s2[5]);
  s1[6] = vsubq_s16(s3[7], s2[6]);
  s1[7] = vaddq_s16(s3[7], s2[6]);

  // step 6
  t_lo[0] = vget_low_s16(s1[0]);
  t_hi[0] = vget_high_s16(s1[0]);
  t_lo[1] = vget_low_s16(s1[1]);
  t_hi[1] = vget_high_s16(s1[1]);
  t_lo[2] = vget_low_s16(s1[2]);
  t_hi[2] = vget_high_s16(s1[2]);
  t_lo[3] = vget_low_s16(s1[3]);
  t_hi[3] = vget_high_s16(s1[3]);
  t_lo[4] = vget_low_s16(s1[4]);
  t_hi[4] = vget_high_s16(s1[4]);
  t_lo[5] = vget_low_s16(s1[5]);
  t_hi[5] = vget_high_s16(s1[5]);
  t_lo[6] = vget_low_s16(s1[6]);
  t_hi[6] = vget_high_s16(s1[6]);
  t_lo[7] = vget_low_s16(s1[7]);
  t_hi[7] = vget_high_s16(s1[7]);

  // step1[0] * cospi_30_64 + step1[7] * cospi_2_64;
  u_lo[0] = vaddq_s32(vmull_n_s16(t_lo[0], cospi_30_64),
                      vmull_n_s16(t_lo[7], cospi_2_64));
  u_hi[0] = vaddq_s32(vmull_n_s16(t_hi[0], cospi_30_64),
                      vmull_n_s16(t_hi[7], cospi_2_64));

  // step1[1] * cospi_14_64 + step1[6] * cospi_18_64;
  u_lo[1] = vaddq_s32(vmull_n_s16(t_lo[1], cospi_14_64),
                      vmull_n_s16(t_lo[6], cospi_18_64));
  u_hi[1] = vaddq_s32(vmull_n_s16(t_hi[1], cospi_14_64),
                      vmull_n_s16(t_hi[6], cospi_18_64));

  // step1[2] * cospi_22_64 + step1[5] * cospi_10_64;
  u_lo[2] = vaddq_s32(vmull_n_s16(t_lo[2], cospi_22_64),
                      vmull_n_s16(t_lo[5], cospi_10_64));
  u_hi[2] = vaddq_s32(vmull_n_s16(t_hi[2], cospi_22_64),
                      vmull_n_s16(t_hi[5], cospi_10_64));

  // step1[3] * cospi_6_64 + step1[4] * cospi_26_64;
  u_lo[3] = vaddq_s32(vmull_n_s16(t_lo[3], cospi_6_64),
                      vmull_n_s16(t_lo[4], cospi_26_64));
  u_hi[3] = vaddq_s32(vmull_n_s16(t_hi[3], cospi_6_64),
                      vmull_n_s16(t_hi[4], cospi_26_64));

  // step1[3] * -cospi_26_64 + step1[4] * cospi_6_64;
  u_lo[4] = vaddq_s32(vmull_n_s16(t_lo[3], -cospi_26_64),
                      vmull_n_s16(t_lo[4], cospi_6_64));
  u_hi[4] = vaddq_s32(vmull_n_s16(t_hi[3], -cospi_26_64),
                      vmull_n_s16(t_hi[4], cospi_6_64));

  // step1[2] * -cospi_10_64 + step1[5] * cospi_22_64;
  u_lo[5] = vaddq_s32(vmull_n_s16(t_lo[2], -cospi_10_64),
                      vmull_n_s16(t_lo[5], cospi_22_64));
  u_hi[5] = vaddq_s32(vmull_n_s16(t_hi[2], -cospi_10_64),
                      vmull_n_s16(t_hi[5], cospi_22_64));

  // step1[1] * -cospi_18_64 + step1[6] * cospi_14_64;
  u_lo[6] = vaddq_s32(vmull_n_s16(t_lo[1], -cospi_18_64),
                      vmull_n_s16(t_lo[6], cospi_14_64));
  u_hi[6] = vaddq_s32(vmull_n_s16(t_hi[1], -cospi_18_64),
                      vmull_n_s16(t_hi[6], cospi_14_64));

  // step1[0] * -cospi_2_64 + step1[7] * cospi_30_64;
  u_lo[7] = vaddq_s32(vmull_n_s16(t_lo[0], -cospi_2_64),
                      vmull_n_s16(t_lo[7], cospi_30_64));
  u_hi[7] = vaddq_s32(vmull_n_s16(t_hi[0], -cospi_2_64),
                      vmull_n_s16(t_hi[7], cospi_30_64));

  // final fdct_round_shift
  u_lo[0] = vaddq_s32(u_lo[0], k__DCT_CONST_ROUNDING);
  u_hi[0] = vaddq_s32(u_hi[0], k__DCT_CONST_ROUNDING);
  u_lo[1] = vaddq_s32(u_lo[1], k__DCT_CONST_ROUNDING);
  u_hi[1] = vaddq_s32(u_hi[1], k__DCT_CONST_ROUNDING);
  u_lo[2] = vaddq_s32(u_lo[2], k__DCT_CONST_ROUNDING);
  u_hi[2] = vaddq_s32(u_hi[2], k__DCT_CONST_ROUNDING);
  u_lo[3] = vaddq_s32(u_lo[3], k__DCT_CONST_ROUNDING);
  u_hi[3] = vaddq_s32(u_hi[3], k__DCT_CONST_ROUNDING);
  u_lo[4] = vaddq_s32(u_lo[4], k__DCT_CONST_ROUNDING);
  u_hi[4] = vaddq_s32(u_hi[4], k__DCT_CONST_ROUNDING);
  u_lo[5] = vaddq_s32(u_lo[5], k__DCT_CONST_ROUNDING);
  u_hi[5] = vaddq_s32(u_hi[5], k__DCT_CONST_ROUNDING);
  u_lo[6] = vaddq_s32(u_lo[6], k__DCT_CONST_ROUNDING);
  u_hi[6] = vaddq_s32(u_hi[6], k__DCT_CONST_ROUNDING);
  u_lo[7] = vaddq_s32(u_lo[7], k__DCT_CONST_ROUNDING);
  u_hi[7] = vaddq_s32(u_hi[7], k__DCT_CONST_ROUNDING);

  t_lo[0] = vshrn_n_s32(u_lo[0], DCT_CONST_BITS);
  t_hi[0] = vshrn_n_s32(u_hi[0], DCT_CONST_BITS);
  t_lo[1] = vshrn_n_s32(u_lo[1], DCT_CONST_BITS);
  t_hi[1] = vshrn_n_s32(u_hi[1], DCT_CONST_BITS);
  t_lo[2] = vshrn_n_s32(u_lo[2], DCT_CONST_BITS);
  t_hi[2] = vshrn_n_s32(u_hi[2], DCT_CONST_BITS);
  t_lo[3] = vshrn_n_s32(u_lo[3], DCT_CONST_BITS);
  t_hi[3] = vshrn_n_s32(u_hi[3], DCT_CONST_BITS);
  t_lo[4] = vshrn_n_s32(u_lo[4], DCT_CONST_BITS);
  t_hi[4] = vshrn_n_s32(u_hi[4], DCT_CONST_BITS);
  t_lo[5] = vshrn_n_s32(u_lo[5], DCT_CONST_BITS);
  t_hi[5] = vshrn_n_s32(u_hi[5], DCT_CONST_BITS);
  t_lo[6] = vshrn_n_s32(u_lo[6], DCT_CONST_BITS);
  t_hi[6] = vshrn_n_s32(u_hi[6], DCT_CONST_BITS);
  t_lo[7] = vshrn_n_s32(u_lo[7], DCT_CONST_BITS);
  t_hi[7] = vshrn_n_s32(u_hi[7], DCT_CONST_BITS);

  in[0] = i[0];
  in[2] = i[1];
  in[4] = i[2];
  in[6] = i[3];
  in[8] = i[4];
  in[10] = i[5];
  in[12] = i[6];
  in[14] = i[7];
  in[1] = vcombine_s16(t_lo[0], t_hi[0]);
  in[3] = vcombine_s16(t_lo[4], t_hi[4]);
  in[5] = vcombine_s16(t_lo[2], t_hi[2]);
  in[7] = vcombine_s16(t_lo[6], t_hi[6]);
  in[9] = vcombine_s16(t_lo[1], t_hi[1]);
  in[11] = vcombine_s16(t_lo[5], t_hi[5]);
  in[13] = vcombine_s16(t_lo[3], t_hi[3]);
  in[15] = vcombine_s16(t_lo[7], t_hi[7]);
}

static void fadst16_8col(int16x8_t *in) {
  // perform 16x16 1-D ADST for 8 columns
  int16x4_t x_lo[16], x_hi[16];
  int32x4_t s_lo[16], s_hi[16];
  int32x4_t t_lo[16], t_hi[16];
  const int32x4_t k__DCT_CONST_ROUNDING = vdupq_n_s32(DCT_CONST_ROUNDING);

  x_lo[0] = vget_low_s16(in[15]);
  x_hi[0] = vget_high_s16(in[15]);
  x_lo[1] = vget_low_s16(in[0]);
  x_hi[1] = vget_high_s16(in[0]);
  x_lo[2] = vget_low_s16(in[13]);
  x_hi[2] = vget_high_s16(in[13]);
  x_lo[3] = vget_low_s16(in[2]);
  x_hi[3] = vget_high_s16(in[2]);
  x_lo[4] = vget_low_s16(in[11]);
  x_hi[4] = vget_high_s16(in[11]);
  x_lo[5] = vget_low_s16(in[4]);
  x_hi[5] = vget_high_s16(in[4]);
  x_lo[6] = vget_low_s16(in[9]);
  x_hi[6] = vget_high_s16(in[9]);
  x_lo[7] = vget_low_s16(in[6]);
  x_hi[7] = vget_high_s16(in[6]);
  x_lo[8] = vget_low_s16(in[7]);
  x_hi[8] = vget_high_s16(in[7]);
  x_lo[9] = vget_low_s16(in[8]);
  x_hi[9] = vget_high_s16(in[8]);
  x_lo[10] = vget_low_s16(in[5]);
  x_hi[10] = vget_high_s16(in[5]);
  x_lo[11] = vget_low_s16(in[10]);
  x_hi[11] = vget_high_s16(in[10]);
  x_lo[12] = vget_low_s16(in[3]);
  x_hi[12] = vget_high_s16(in[3]);
  x_lo[13] = vget_low_s16(in[12]);
  x_hi[13] = vget_high_s16(in[12]);
  x_lo[14] = vget_low_s16(in[1]);
  x_hi[14] = vget_high_s16(in[1]);
  x_lo[15] = vget_low_s16(in[14]);
  x_hi[15] = vget_high_s16(in[14]);

  // stage 1
  // s0 = cospi_1_64 * x0 + cospi_31_64 * x1;
  s_lo[0] = vaddq_s32(vmull_n_s16(x_lo[0], cospi_1_64),
                      vmull_n_s16(x_lo[1], cospi_31_64));
  s_hi[0] = vaddq_s32(vmull_n_s16(x_hi[0], cospi_1_64),
                      vmull_n_s16(x_hi[1], cospi_31_64));
  // s1 = cospi_31_64 * x0 - cospi_1_64 * x1;
  s_lo[1] = vsubq_s32(vmull_n_s16(x_lo[0], cospi_31_64),
                      vmull_n_s16(x_lo[1], cospi_1_64));
  s_hi[1] = vsubq_s32(vmull_n_s16(x_hi[0], cospi_31_64),
                      vmull_n_s16(x_hi[1], cospi_1_64));
  // s2 = cospi_5_64 * x2 + cospi_27_64 * x3;
  s_lo[2] = vaddq_s32(vmull_n_s16(x_lo[2], cospi_5_64),
                      vmull_n_s16(x_lo[3], cospi_27_64));
  s_hi[2] = vaddq_s32(vmull_n_s16(x_hi[2], cospi_5_64),
                      vmull_n_s16(x_hi[3], cospi_27_64));
  // s3 = cospi_27_64 * x2 - cospi_5_64 * x3;
  s_lo[3] = vsubq_s32(vmull_n_s16(x_lo[2], cospi_27_64),
                      vmull_n_s16(x_lo[3], cospi_5_64));
  s_hi[3] = vsubq_s32(vmull_n_s16(x_hi[2], cospi_27_64),
                      vmull_n_s16(x_hi[3], cospi_5_64));
  // s4 = cospi_9_64 * x4 + cospi_23_64 * x5;
  s_lo[4] = vaddq_s32(vmull_n_s16(x_lo[4], cospi_9_64),
                      vmull_n_s16(x_lo[5], cospi_23_64));
  s_hi[4] = vaddq_s32(vmull_n_s16(x_hi[4], cospi_9_64),
                      vmull_n_s16(x_hi[5], cospi_23_64));
  // s5 = cospi_23_64 * x4 - cospi_9_64 * x5;
  s_lo[5] = vsubq_s32(vmull_n_s16(x_lo[4], cospi_23_64),
                      vmull_n_s16(x_lo[5], cospi_9_64));
  s_hi[5] = vsubq_s32(vmull_n_s16(x_hi[4], cospi_23_64),
                      vmull_n_s16(x_hi[5], cospi_9_64));
  // s6 = cospi_13_64 * x6 + cospi_19_64 * x7;
  s_lo[6] = vaddq_s32(vmull_n_s16(x_lo[6], cospi_13_64),
                      vmull_n_s16(x_lo[7], cospi_19_64));
  s_hi[6] = vaddq_s32(vmull_n_s16(x_hi[6], cospi_13_64),
                      vmull_n_s16(x_hi[7], cospi_19_64));
  // s7 = cospi_19_64 * x6 - cospi_13_64 * x7;
  s_lo[7] = vsubq_s32(vmull_n_s16(x_lo[6], cospi_19_64),
                      vmull_n_s16(x_lo[7], cospi_13_64));
  s_hi[7] = vsubq_s32(vmull_n_s16(x_hi[6], cospi_19_64),
                      vmull_n_s16(x_hi[7], cospi_13_64));
  // s8 = cospi_17_64 * x8 + cospi_15_64 * x9;
  s_lo[8] = vaddq_s32(vmull_n_s16(x_lo[8], cospi_17_64),
                      vmull_n_s16(x_lo[9], cospi_15_64));
  s_hi[8] = vaddq_s32(vmull_n_s16(x_hi[8], cospi_17_64),
                      vmull_n_s16(x_hi[9], cospi_15_64));
  // s9 = cospi_15_64 * x8 - cospi_17_64 * x9;
  s_lo[9] = vsubq_s32(vmull_n_s16(x_lo[8], cospi_15_64),
                      vmull_n_s16(x_lo[9], cospi_17_64));
  s_hi[9] = vsubq_s32(vmull_n_s16(x_hi[8], cospi_15_64),
                      vmull_n_s16(x_hi[9], cospi_17_64));
  // s10 = cospi_21_64 * x10 + cospi_11_64 * x11;
  s_lo[10] = vaddq_s32(vmull_n_s16(x_lo[10], cospi_21_64),
                       vmull_n_s16(x_lo[11], cospi_11_64));
  s_hi[10] = vaddq_s32(vmull_n_s16(x_hi[10], cospi_21_64),
                       vmull_n_s16(x_hi[11], cospi_11_64));
  // s11 = cospi_11_64 * x10 - cospi_21_64 * x11;
  s_lo[11] = vsubq_s32(vmull_n_s16(x_lo[10], cospi_11_64),
                       vmull_n_s16(x_lo[11], cospi_21_64));
  s_hi[11] = vsubq_s32(vmull_n_s16(x_hi[10], cospi_11_64),
                       vmull_n_s16(x_hi[11], cospi_21_64));
  // s12 = cospi_25_64 * x12 + cospi_7_64 * x13;
  s_lo[12] = vaddq_s32(vmull_n_s16(x_lo[12], cospi_25_64),
                       vmull_n_s16(x_lo[13], cospi_7_64));
  s_hi[12] = vaddq_s32(vmull_n_s16(x_hi[12], cospi_25_64),
                       vmull_n_s16(x_hi[13], cospi_7_64));
  // s13 = cospi_7_64 * x12 - cospi_25_64 * x13;
  s_lo[13] = vsubq_s32(vmull_n_s16(x_lo[12], cospi_7_64),
                       vmull_n_s16(x_lo[13], cospi_25_64));
  s_hi[13] = vsubq_s32(vmull_n_s16(x_hi[12], cospi_7_64),
                       vmull_n_s16(x_hi[13], cospi_25_64));
  // s14 = cospi_29_64 * x14 + cospi_3_64 * x15;
  s_lo[14] = vaddq_s32(vmull_n_s16(x_lo[14], cospi_29_64),
                       vmull_n_s16(x_lo[15], cospi_3_64));
  s_hi[14] = vaddq_s32(vmull_n_s16(x_hi[14], cospi_29_64),
                       vmull_n_s16(x_hi[15], cospi_3_64));
  // s15 = cospi_3_64 * x14 - cospi_29_64 * x15;
  s_lo[15] = vsubq_s32(vmull_n_s16(x_lo[14], cospi_3_64),
                       vmull_n_s16(x_lo[15], cospi_29_64));
  s_hi[15] = vsubq_s32(vmull_n_s16(x_hi[14], cospi_3_64),
                       vmull_n_s16(x_hi[15], cospi_29_64));

  // fdct_round_shift
  t_lo[0] = vaddq_s32(s_lo[0], s_lo[8]);
  t_hi[0] = vaddq_s32(s_hi[0], s_hi[8]);
  t_lo[1] = vaddq_s32(s_lo[1], s_lo[9]);
  t_hi[1] = vaddq_s32(s_hi[1], s_hi[9]);
  t_lo[2] = vaddq_s32(s_lo[2], s_lo[10]);
  t_hi[2] = vaddq_s32(s_hi[2], s_hi[10]);
  t_lo[3] = vaddq_s32(s_lo[3], s_lo[11]);
  t_hi[3] = vaddq_s32(s_hi[3], s_hi[11]);
  t_lo[4] = vaddq_s32(s_lo[4], s_lo[12]);
  t_hi[4] = vaddq_s32(s_hi[4], s_hi[12]);
  t_lo[5] = vaddq_s32(s_lo[5], s_lo[13]);
  t_hi[5] = vaddq_s32(s_hi[5], s_hi[13]);
  t_lo[6] = vaddq_s32(s_lo[6], s_lo[14]);
  t_hi[6] = vaddq_s32(s_hi[6], s_hi[14]);
  t_lo[7] = vaddq_s32(s_lo[7], s_lo[15]);
  t_hi[7] = vaddq_s32(s_hi[7], s_hi[15]);
  t_lo[8] = vsubq_s32(s_lo[0], s_lo[8]);
  t_hi[8] = vsubq_s32(s_hi[0], s_hi[8]);
  t_lo[9] = vsubq_s32(s_lo[1], s_lo[9]);
  t_hi[9] = vsubq_s32(s_hi[1], s_hi[9]);
  t_lo[10] = vsubq_s32(s_lo[2], s_lo[10]);
  t_hi[10] = vsubq_s32(s_hi[2], s_hi[10]);
  t_lo[11] = vsubq_s32(s_lo[3], s_lo[11]);
  t_hi[11] = vsubq_s32(s_hi[3], s_hi[11]);
  t_lo[12] = vsubq_s32(s_lo[4], s_lo[12]);
  t_hi[12] = vsubq_s32(s_hi[4], s_hi[12]);
  t_lo[13] = vsubq_s32(s_lo[5], s_lo[13]);
  t_hi[13] = vsubq_s32(s_hi[5], s_hi[13]);
  t_lo[14] = vsubq_s32(s_lo[6], s_lo[14]);
  t_hi[14] = vsubq_s32(s_hi[6], s_hi[14]);
  t_lo[15] = vsubq_s32(s_lo[7], s_lo[15]);
  t_hi[15] = vsubq_s32(s_hi[7], s_hi[15]);

  t_lo[0] = vaddq_s32(t_lo[0], k__DCT_CONST_ROUNDING);
  t_hi[0] = vaddq_s32(t_hi[0], k__DCT_CONST_ROUNDING);
  t_lo[1] = vaddq_s32(t_lo[1], k__DCT_CONST_ROUNDING);
  t_hi[1] = vaddq_s32(t_hi[1], k__DCT_CONST_ROUNDING);
  t_lo[2] = vaddq_s32(t_lo[2], k__DCT_CONST_ROUNDING);
  t_hi[2] = vaddq_s32(t_hi[2], k__DCT_CONST_ROUNDING);
  t_lo[3] = vaddq_s32(t_lo[3], k__DCT_CONST_ROUNDING);
  t_hi[3] = vaddq_s32(t_hi[3], k__DCT_CONST_ROUNDING);
  t_lo[4] = vaddq_s32(t_lo[4], k__DCT_CONST_ROUNDING);
  t_hi[4] = vaddq_s32(t_hi[4], k__DCT_CONST_ROUNDING);
  t_lo[5] = vaddq_s32(t_lo[5], k__DCT_CONST_ROUNDING);
  t_hi[5] = vaddq_s32(t_hi[5], k__DCT_CONST_ROUNDING);
  t_lo[6] = vaddq_s32(t_lo[6], k__DCT_CONST_ROUNDING);
  t_hi[6] = vaddq_s32(t_hi[6], k__DCT_CONST_ROUNDING);
  t_lo[7] = vaddq_s32(t_lo[7], k__DCT_CONST_ROUNDING);
  t_hi[7] = vaddq_s32(t_hi[7], k__DCT_CONST_ROUNDING);
  t_lo[8] = vaddq_s32(t_lo[8], k__DCT_CONST_ROUNDING);
  t_hi[8] = vaddq_s32(t_hi[8], k__DCT_CONST_ROUNDING);
  t_lo[9] = vaddq_s32(t_lo[9], k__DCT_CONST_ROUNDING);
  t_hi[9] = vaddq_s32(t_hi[9], k__DCT_CONST_ROUNDING);
  t_lo[10] = vaddq_s32(t_lo[10], k__DCT_CONST_ROUNDING);
  t_hi[10] = vaddq_s32(t_hi[10], k__DCT_CONST_ROUNDING);
  t_lo[11] = vaddq_s32(t_lo[11], k__DCT_CONST_ROUNDING);
  t_hi[11] = vaddq_s32(t_hi[11], k__DCT_CONST_ROUNDING);
  t_lo[12] = vaddq_s32(t_lo[12], k__DCT_CONST_ROUNDING);
  t_hi[12] = vaddq_s32(t_hi[12], k__DCT_CONST_ROUNDING);
  t_lo[13] = vaddq_s32(t_lo[13], k__DCT_CONST_ROUNDING);
  t_hi[13] = vaddq_s32(t_hi[13], k__DCT_CONST_ROUNDING);
  t_lo[14] = vaddq_s32(t_lo[14], k__DCT_CONST_ROUNDING);
  t_hi[14] = vaddq_s32(t_hi[14], k__DCT_CONST_ROUNDING);
  t_lo[15] = vaddq_s32(t_lo[15], k__DCT_CONST_ROUNDING);
  t_hi[15] = vaddq_s32(t_hi[15], k__DCT_CONST_ROUNDING);

  t_lo[0] = vshrq_n_s32(t_lo[0], DCT_CONST_BITS);
  t_hi[0] = vshrq_n_s32(t_hi[0], DCT_CONST_BITS);
  t_lo[1] = vshrq_n_s32(t_lo[1], DCT_CONST_BITS);
  t_hi[1] = vshrq_n_s32(t_hi[1], DCT_CONST_BITS);
  t_lo[2] = vshrq_n_s32(t_lo[2], DCT_CONST_BITS);
  t_hi[2] = vshrq_n_s32(t_hi[2], DCT_CONST_BITS);
  t_lo[3] = vshrq_n_s32(t_lo[3], DCT_CONST_BITS);
  t_hi[3] = vshrq_n_s32(t_hi[3], DCT_CONST_BITS);
  t_lo[4] = vshrq_n_s32(t_lo[4], DCT_CONST_BITS);
  t_hi[4] = vshrq_n_s32(t_hi[4], DCT_CONST_BITS);
  t_lo[5] = vshrq_n_s32(t_lo[5], DCT_CONST_BITS);
  t_hi[5] = vshrq_n_s32(t_hi[5], DCT_CONST_BITS);
  t_lo[6] = vshrq_n_s32(t_lo[6], DCT_CONST_BITS);
  t_hi[6] = vshrq_n_s32(t_hi[6], DCT_CONST_BITS);
  t_lo[7] = vshrq_n_s32(t_lo[7], DCT_CONST_BITS);
  t_hi[7] = vshrq_n_s32(t_hi[7], DCT_CONST_BITS);
  t_lo[8] = vshrq_n_s32(t_lo[8], DCT_CONST_BITS);
  t_hi[8] = vshrq_n_s32(t_hi[8], DCT_CONST_BITS);
  t_lo[9] = vshrq_n_s32(t_lo[9], DCT_CONST_BITS);
  t_hi[9] = vshrq_n_s32(t_hi[9], DCT_CONST_BITS);
  t_lo[10] = vshrq_n_s32(t_lo[10], DCT_CONST_BITS);
  t_hi[10] = vshrq_n_s32(t_hi[10], DCT_CONST_BITS);
  t_lo[11] = vshrq_n_s32(t_lo[11], DCT_CONST_BITS);
  t_hi[11] = vshrq_n_s32(t_hi[11], DCT_CONST_BITS);
  t_lo[12] = vshrq_n_s32(t_lo[12], DCT_CONST_BITS);
  t_hi[12] = vshrq_n_s32(t_hi[12], DCT_CONST_BITS);
  t_lo[13] = vshrq_n_s32(t_lo[13], DCT_CONST_BITS);
  t_hi[13] = vshrq_n_s32(t_hi[13], DCT_CONST_BITS);
  t_lo[14] = vshrq_n_s32(t_lo[14], DCT_CONST_BITS);
  t_hi[14] = vshrq_n_s32(t_hi[14], DCT_CONST_BITS);
  t_lo[15] = vshrq_n_s32(t_lo[15], DCT_CONST_BITS);
  t_hi[15] = vshrq_n_s32(t_hi[15], DCT_CONST_BITS);

  // stage 2
  s_lo[0] = t_lo[0];
  s_hi[0] = t_hi[0];
  s_lo[1] = t_lo[1];
  s_hi[1] = t_hi[1];
  s_lo[2] = t_lo[2];
  s_hi[2] = t_hi[2];
  s_lo[3] = t_lo[3];
  s_hi[3] = t_hi[3];
  s_lo[4] = t_lo[4];
  s_hi[4] = t_hi[4];
  s_lo[5] = t_lo[5];
  s_hi[5] = t_hi[5];
  s_lo[6] = t_lo[6];
  s_hi[6] = t_hi[6];
  s_lo[7] = t_lo[7];
  s_hi[7] = t_hi[7];
  // s8 = x8 * cospi_4_64 + x9 * cospi_28_64;
  s_lo[8] = vaddq_s32(vmulq_n_s32(t_lo[8], cospi_4_64),
                      vmulq_n_s32(t_lo[9], cospi_28_64));
  s_hi[8] = vaddq_s32(vmulq_n_s32(t_hi[8], cospi_4_64),
                      vmulq_n_s32(t_hi[9], cospi_28_64));
  // s9 = x8 * cospi_28_64 - x9 * cospi_4_64;
  s_lo[9] = vsubq_s32(vmulq_n_s32(t_lo[8], cospi_28_64),
                      vmulq_n_s32(t_lo[9], cospi_4_64));
  s_hi[9] = vsubq_s32(vmulq_n_s32(t_hi[8], cospi_28_64),
                      vmulq_n_s32(t_hi[9], cospi_4_64));
  // s10 = x10 * cospi_20_64 + x11 * cospi_12_64;
  s_lo[10] = vaddq_s32(vmulq_n_s32(t_lo[10], cospi_20_64),
                       vmulq_n_s32(t_lo[11], cospi_12_64));
  s_hi[10] = vaddq_s32(vmulq_n_s32(t_hi[10], cospi_20_64),
                       vmulq_n_s32(t_hi[11], cospi_12_64));
  // s11 = x10 * cospi_12_64 - x11 * cospi_20_64;
  s_lo[11] = vsubq_s32(vmulq_n_s32(t_lo[10], cospi_12_64),
                       vmulq_n_s32(t_lo[11], cospi_20_64));
  s_hi[11] = vsubq_s32(vmulq_n_s32(t_hi[10], cospi_12_64),
                       vmulq_n_s32(t_hi[11], cospi_20_64));
  // s12 = -x12 * cospi_28_64 + x13 * cospi_4_64;
  s_lo[12] = vaddq_s32(vmulq_n_s32(t_lo[12], -cospi_28_64),
                       vmulq_n_s32(t_lo[13], cospi_4_64));
  s_hi[12] = vaddq_s32(vmulq_n_s32(t_hi[12], -cospi_28_64),
                       vmulq_n_s32(t_hi[13], cospi_4_64));
  // s13 = x12 * cospi_4_64 + x13 * cospi_28_64;
  s_lo[13] = vaddq_s32(vmulq_n_s32(t_lo[12], cospi_4_64),
                       vmulq_n_s32(t_lo[13], cospi_28_64));
  s_hi[13] = vaddq_s32(vmulq_n_s32(t_hi[12], cospi_4_64),
                       vmulq_n_s32(t_hi[13], cospi_28_64));
  // s14 = -x14 * cospi_12_64 + x15 * cospi_20_64;
  s_lo[14] = vaddq_s32(vmulq_n_s32(t_lo[14], -cospi_12_64),
                       vmulq_n_s32(t_lo[15], cospi_20_64));
  s_hi[14] = vaddq_s32(vmulq_n_s32(t_hi[14], -cospi_12_64),
                       vmulq_n_s32(t_hi[15], cospi_20_64));
  // s15 = x14 * cospi_20_64 + x15 * cospi_12_64;
  s_lo[15] = vaddq_s32(vmulq_n_s32(t_lo[14], cospi_20_64),
                       vmulq_n_s32(t_lo[15], cospi_12_64));
  s_hi[15] = vaddq_s32(vmulq_n_s32(t_hi[14], cospi_20_64),
                       vmulq_n_s32(t_hi[15], cospi_12_64));

  // s0 + s4
  t_lo[0] = vaddq_s32(s_lo[0], s_lo[4]);
  t_hi[0] = vaddq_s32(s_hi[0], s_hi[4]);
  // s1 + s5
  t_lo[1] = vaddq_s32(s_lo[1], s_lo[5]);
  t_hi[1] = vaddq_s32(s_hi[1], s_hi[5]);
  // s2 + s6
  t_lo[2] = vaddq_s32(s_lo[2], s_lo[6]);
  t_hi[2] = vaddq_s32(s_hi[2], s_hi[6]);
  // s3 + s7
  t_lo[3] = vaddq_s32(s_lo[3], s_lo[7]);
  t_hi[3] = vaddq_s32(s_hi[3], s_hi[7]);
  // s0 - s4
  t_lo[4] = vsubq_s32(s_lo[0], s_lo[4]);
  t_hi[4] = vsubq_s32(s_hi[0], s_hi[4]);
  // s1 - s7
  t_lo[5] = vsubq_s32(s_lo[1], s_lo[5]);
  t_hi[5] = vsubq_s32(s_hi[1], s_hi[5]);
  // s2 - s6
  t_lo[6] = vsubq_s32(s_lo[2], s_lo[6]);
  t_hi[6] = vsubq_s32(s_hi[2], s_hi[6]);
  // s3 - s7
  t_lo[7] = vsubq_s32(s_lo[3], s_lo[7]);
  t_hi[7] = vsubq_s32(s_hi[3], s_hi[7]);
  // s8 + s12
  t_lo[8] = vaddq_s32(s_lo[8], s_lo[12]);
  t_hi[8] = vaddq_s32(s_hi[8], s_hi[12]);
  // s9 + s13
  t_lo[9] = vaddq_s32(s_lo[9], s_lo[13]);
  t_hi[9] = vaddq_s32(s_hi[9], s_hi[13]);
  // s10 + s14
  t_lo[10] = vaddq_s32(s_lo[10], s_lo[14]);
  t_hi[10] = vaddq_s32(s_hi[10], s_hi[14]);
  // s11 + s15
  t_lo[11] = vaddq_s32(s_lo[11], s_lo[15]);
  t_hi[11] = vaddq_s32(s_hi[11], s_hi[15]);
  // s8 + s12
  t_lo[12] = vsubq_s32(s_lo[8], s_lo[12]);
  t_hi[12] = vsubq_s32(s_hi[8], s_hi[12]);
  // s9 + s13
  t_lo[13] = vsubq_s32(s_lo[9], s_lo[13]);
  t_hi[13] = vsubq_s32(s_hi[9], s_hi[13]);
  // s10 + s14
  t_lo[14] = vsubq_s32(s_lo[10], s_lo[14]);
  t_hi[14] = vsubq_s32(s_hi[10], s_hi[14]);
  // s11 + s15
  t_lo[15] = vsubq_s32(s_lo[11], s_lo[15]);
  t_hi[15] = vsubq_s32(s_hi[11], s_hi[15]);

  t_lo[8] = vaddq_s32(t_lo[8], k__DCT_CONST_ROUNDING);
  t_hi[8] = vaddq_s32(t_hi[8], k__DCT_CONST_ROUNDING);
  t_lo[9] = vaddq_s32(t_lo[9], k__DCT_CONST_ROUNDING);
  t_hi[9] = vaddq_s32(t_hi[9], k__DCT_CONST_ROUNDING);
  t_lo[10] = vaddq_s32(t_lo[10], k__DCT_CONST_ROUNDING);
  t_hi[10] = vaddq_s32(t_hi[10], k__DCT_CONST_ROUNDING);
  t_lo[11] = vaddq_s32(t_lo[11], k__DCT_CONST_ROUNDING);
  t_hi[11] = vaddq_s32(t_hi[11], k__DCT_CONST_ROUNDING);
  t_lo[12] = vaddq_s32(t_lo[12], k__DCT_CONST_ROUNDING);
  t_hi[12] = vaddq_s32(t_hi[12], k__DCT_CONST_ROUNDING);
  t_lo[13] = vaddq_s32(t_lo[13], k__DCT_CONST_ROUNDING);
  t_hi[13] = vaddq_s32(t_hi[13], k__DCT_CONST_ROUNDING);
  t_lo[14] = vaddq_s32(t_lo[14], k__DCT_CONST_ROUNDING);
  t_hi[14] = vaddq_s32(t_hi[14], k__DCT_CONST_ROUNDING);
  t_lo[15] = vaddq_s32(t_lo[15], k__DCT_CONST_ROUNDING);
  t_hi[15] = vaddq_s32(t_hi[15], k__DCT_CONST_ROUNDING);
  t_lo[8] = vshrq_n_s32(t_lo[8], DCT_CONST_BITS);
  t_hi[8] = vshrq_n_s32(t_hi[8], DCT_CONST_BITS);
  t_lo[9] = vshrq_n_s32(t_lo[9], DCT_CONST_BITS);
  t_hi[9] = vshrq_n_s32(t_hi[9], DCT_CONST_BITS);
  t_lo[10] = vshrq_n_s32(t_lo[10], DCT_CONST_BITS);
  t_hi[10] = vshrq_n_s32(t_hi[10], DCT_CONST_BITS);
  t_lo[11] = vshrq_n_s32(t_lo[11], DCT_CONST_BITS);
  t_hi[11] = vshrq_n_s32(t_hi[11], DCT_CONST_BITS);
  t_lo[12] = vshrq_n_s32(t_lo[12], DCT_CONST_BITS);
  t_hi[12] = vshrq_n_s32(t_hi[12], DCT_CONST_BITS);
  t_lo[13] = vshrq_n_s32(t_lo[13], DCT_CONST_BITS);
  t_hi[13] = vshrq_n_s32(t_hi[13], DCT_CONST_BITS);
  t_lo[14] = vshrq_n_s32(t_lo[14], DCT_CONST_BITS);
  t_hi[14] = vshrq_n_s32(t_hi[14], DCT_CONST_BITS);
  t_lo[15] = vshrq_n_s32(t_lo[15], DCT_CONST_BITS);
  t_hi[15] = vshrq_n_s32(t_hi[15], DCT_CONST_BITS);

  // stage 3
  s_lo[0] = t_lo[0];
  s_hi[0] = t_hi[0];
  s_lo[1] = t_lo[1];
  s_hi[1] = t_hi[1];
  s_lo[2] = t_lo[2];
  s_hi[2] = t_hi[2];
  s_lo[3] = t_lo[3];
  s_hi[3] = t_hi[3];
  // s4 = x4 * cospi_8_64 + x5 * cospi_24_64;
  s_lo[4] = vaddq_s32(vmulq_n_s32(t_lo[4], cospi_8_64),
                      vmulq_n_s32(t_lo[5], cospi_24_64));
  s_hi[4] = vaddq_s32(vmulq_n_s32(t_hi[4], cospi_8_64),
                      vmulq_n_s32(t_hi[5], cospi_24_64));
  // s5 = x4 * cospi_24_64 - x5 * cospi_8_64;
  s_lo[5] = vaddq_s32(vmulq_n_s32(t_lo[4], cospi_24_64),
                      vmulq_n_s32(t_lo[5], -cospi_8_64));
  s_hi[5] = vaddq_s32(vmulq_n_s32(t_hi[4], cospi_24_64),
                      vmulq_n_s32(t_hi[5], -cospi_8_64));
  // s6 = -x6 * cospi_24_64 + x7 * cospi_8_64;
  s_lo[6] = vaddq_s32(vmulq_n_s32(t_lo[6], -cospi_24_64),
                      vmulq_n_s32(t_lo[7], cospi_8_64));
  s_hi[6] = vaddq_s32(vmulq_n_s32(t_hi[6], -cospi_24_64),
                      vmulq_n_s32(t_hi[7], cospi_8_64));
  // s7 = x6 * cospi_8_64 + x7 * cospi_24_64;
  s_lo[7] = vaddq_s32(vmulq_n_s32(t_lo[6], cospi_8_64),
                      vmulq_n_s32(t_lo[7], cospi_24_64));
  s_hi[7] = vaddq_s32(vmulq_n_s32(t_hi[6], cospi_8_64),
                      vmulq_n_s32(t_hi[7], cospi_24_64));
  s_lo[8] = t_lo[8];
  s_hi[8] = t_hi[8];
  s_lo[9] = t_lo[9];
  s_hi[9] = t_hi[9];
  s_lo[10] = t_lo[10];
  s_hi[10] = t_hi[10];
  s_lo[11] = t_lo[11];
  s_hi[11] = t_hi[11];
  // s12 = x12 * cospi_8_64 + x13 * cospi_24_64;
  s_lo[12] = vaddq_s32(vmulq_n_s32(t_lo[12], cospi_8_64),
                       vmulq_n_s32(t_lo[13], cospi_24_64));
  s_hi[12] = vaddq_s32(vmulq_n_s32(t_hi[12], cospi_8_64),
                       vmulq_n_s32(t_hi[13], cospi_24_64));
  // s13 = x12 * cospi_24_64 - x13 * cospi_8_64;
  s_lo[13] = vaddq_s32(vmulq_n_s32(t_lo[12], cospi_24_64),
                       vmulq_n_s32(t_lo[13], -cospi_8_64));
  s_hi[13] = vaddq_s32(vmulq_n_s32(t_hi[12], cospi_24_64),
                       vmulq_n_s32(t_hi[13], -cospi_8_64));
  // s14 = -x14 * cospi_24_64 + x15 * cospi_8_64;
  s_lo[14] = vaddq_s32(vmulq_n_s32(t_lo[14], -cospi_24_64),
                       vmulq_n_s32(t_lo[15], cospi_8_64));
  s_hi[14] = vaddq_s32(vmulq_n_s32(t_hi[14], -cospi_24_64),
                       vmulq_n_s32(t_hi[15], cospi_8_64));
  // s15 = x14 * cospi_8_64 + x15 * cospi_24_64;
  s_lo[15] = vaddq_s32(vmulq_n_s32(t_lo[14], cospi_8_64),
                       vmulq_n_s32(t_lo[15], cospi_24_64));
  s_hi[15] = vaddq_s32(vmulq_n_s32(t_hi[14], cospi_8_64),
                       vmulq_n_s32(t_hi[15], cospi_24_64));

  // s0 + s4
  t_lo[0] = vaddq_s32(s_lo[0], s_lo[2]);
  t_hi[0] = vaddq_s32(s_hi[0], s_hi[2]);
  // s1 + s3
  t_lo[1] = vaddq_s32(s_lo[1], s_lo[3]);
  t_hi[1] = vaddq_s32(s_hi[1], s_hi[3]);
  // s0 - s4
  t_lo[2] = vsubq_s32(s_lo[0], s_lo[2]);
  t_hi[2] = vsubq_s32(s_hi[0], s_hi[2]);
  // s1 - s3
  t_lo[3] = vsubq_s32(s_lo[1], s_lo[3]);
  t_hi[3] = vsubq_s32(s_hi[1], s_hi[3]);
  // s4 + s6
  t_lo[4] = vaddq_s32(s_lo[4], s_lo[6]);
  t_hi[4] = vaddq_s32(s_hi[4], s_hi[6]);
  // s5 + s7
  t_lo[5] = vaddq_s32(s_lo[5], s_lo[7]);
  t_hi[5] = vaddq_s32(s_hi[5], s_hi[7]);
  // s4 - s6
  t_lo[6] = vsubq_s32(s_lo[4], s_lo[6]);
  t_hi[6] = vsubq_s32(s_hi[4], s_hi[6]);
  // s5 - s7
  t_lo[7] = vsubq_s32(s_lo[5], s_lo[7]);
  t_hi[7] = vsubq_s32(s_hi[5], s_hi[7]);
  // s8 + s10
  t_lo[8] = vaddq_s32(s_lo[8], s_lo[10]);
  t_hi[8] = vaddq_s32(s_hi[8], s_hi[10]);
  // s9 + s11
  t_lo[9] = vaddq_s32(s_lo[9], s_lo[11]);
  t_hi[9] = vaddq_s32(s_hi[9], s_hi[11]);
  // s8 - s10
  t_lo[10] = vsubq_s32(s_lo[8], s_lo[10]);
  t_hi[10] = vsubq_s32(s_hi[8], s_hi[10]);
  // s9 - s11
  t_lo[11] = vsubq_s32(s_lo[9], s_lo[11]);
  t_hi[11] = vsubq_s32(s_hi[9], s_hi[11]);
  // s12 + s14
  t_lo[12] = vaddq_s32(s_lo[12], s_lo[14]);
  t_hi[12] = vaddq_s32(s_hi[12], s_hi[14]);
  // s13 + s15
  t_lo[13] = vaddq_s32(s_lo[13], s_lo[15]);
  t_hi[13] = vaddq_s32(s_hi[13], s_hi[15]);
  // s12 - s14
  t_lo[14] = vsubq_s32(s_lo[12], s_lo[14]);
  t_hi[14] = vsubq_s32(s_hi[12], s_hi[14]);
  // s13 - s15
  t_lo[15] = vsubq_s32(s_lo[13], s_lo[15]);
  t_hi[15] = vsubq_s32(s_hi[13], s_hi[15]);

  t_lo[4] = vaddq_s32(t_lo[4], k__DCT_CONST_ROUNDING);
  t_hi[4] = vaddq_s32(t_hi[4], k__DCT_CONST_ROUNDING);
  t_lo[5] = vaddq_s32(t_lo[5], k__DCT_CONST_ROUNDING);
  t_hi[5] = vaddq_s32(t_hi[5], k__DCT_CONST_ROUNDING);
  t_lo[6] = vaddq_s32(t_lo[6], k__DCT_CONST_ROUNDING);
  t_hi[6] = vaddq_s32(t_hi[6], k__DCT_CONST_ROUNDING);
  t_lo[7] = vaddq_s32(t_lo[7], k__DCT_CONST_ROUNDING);
  t_hi[7] = vaddq_s32(t_hi[7], k__DCT_CONST_ROUNDING);
  t_lo[12] = vaddq_s32(t_lo[12], k__DCT_CONST_ROUNDING);
  t_hi[12] = vaddq_s32(t_hi[12], k__DCT_CONST_ROUNDING);
  t_lo[13] = vaddq_s32(t_lo[13], k__DCT_CONST_ROUNDING);
  t_hi[13] = vaddq_s32(t_hi[13], k__DCT_CONST_ROUNDING);
  t_lo[14] = vaddq_s32(t_lo[14], k__DCT_CONST_ROUNDING);
  t_hi[14] = vaddq_s32(t_hi[14], k__DCT_CONST_ROUNDING);
  t_lo[15] = vaddq_s32(t_lo[15], k__DCT_CONST_ROUNDING);
  t_hi[15] = vaddq_s32(t_hi[15], k__DCT_CONST_ROUNDING);
  t_lo[4] = vshrq_n_s32(t_lo[4], DCT_CONST_BITS);
  t_hi[4] = vshrq_n_s32(t_hi[4], DCT_CONST_BITS);
  t_lo[5] = vshrq_n_s32(t_lo[5], DCT_CONST_BITS);
  t_hi[5] = vshrq_n_s32(t_hi[5], DCT_CONST_BITS);
  t_lo[6] = vshrq_n_s32(t_lo[6], DCT_CONST_BITS);
  t_hi[6] = vshrq_n_s32(t_hi[6], DCT_CONST_BITS);
  t_lo[7] = vshrq_n_s32(t_lo[7], DCT_CONST_BITS);
  t_hi[7] = vshrq_n_s32(t_hi[7], DCT_CONST_BITS);
  t_lo[12] = vshrq_n_s32(t_lo[12], DCT_CONST_BITS);
  t_hi[12] = vshrq_n_s32(t_hi[12], DCT_CONST_BITS);
  t_lo[13] = vshrq_n_s32(t_lo[13], DCT_CONST_BITS);
  t_hi[13] = vshrq_n_s32(t_hi[13], DCT_CONST_BITS);
  t_lo[14] = vshrq_n_s32(t_lo[14], DCT_CONST_BITS);
  t_hi[14] = vshrq_n_s32(t_hi[14], DCT_CONST_BITS);
  t_lo[15] = vshrq_n_s32(t_lo[15], DCT_CONST_BITS);
  t_hi[15] = vshrq_n_s32(t_hi[15], DCT_CONST_BITS);

  // stage 4
  // s2 = (-cospi_16_64) * (x2 + x3);
  s_lo[2] = vmulq_n_s32(vaddq_s32(t_lo[2], t_lo[3]), -cospi_16_64);
  s_hi[2] = vmulq_n_s32(vaddq_s32(t_hi[2], t_hi[3]), -cospi_16_64);
  // s3 = cospi_16_64 * (x2 - x3);
  s_lo[3] = vmulq_n_s32(vsubq_s32(t_lo[2], t_lo[3]), cospi_16_64);
  s_hi[3] = vmulq_n_s32(vsubq_s32(t_hi[2], t_hi[3]), cospi_16_64);
  // s6 = cospi_16_64 * (x6 + x7);
  s_lo[6] = vmulq_n_s32(vaddq_s32(t_lo[6], t_lo[7]), cospi_16_64);
  s_hi[6] = vmulq_n_s32(vaddq_s32(t_hi[6], t_hi[7]), cospi_16_64);
  // s7 = cospi_16_64 * (-x6 + x7);
  s_lo[7] = vmulq_n_s32(vsubq_s32(t_lo[7], t_lo[6]), cospi_16_64);
  s_hi[7] = vmulq_n_s32(vsubq_s32(t_hi[7], t_hi[6]), cospi_16_64);
  // s10 = cospi_16_64 * (x10 + x11);
  s_lo[10] = vmulq_n_s32(vaddq_s32(t_lo[10], t_lo[11]), cospi_16_64);
  s_hi[10] = vmulq_n_s32(vaddq_s32(t_hi[10], t_hi[11]), cospi_16_64);
  // s11 = cospi_16_64 * (-x10 + x11);
  s_lo[11] = vmulq_n_s32(vsubq_s32(t_lo[11], t_lo[10]), cospi_16_64);
  s_hi[11] = vmulq_n_s32(vsubq_s32(t_hi[11], t_hi[10]), cospi_16_64);
  // s14 = (-cospi_16_64) * (x14 + x15);
  s_lo[14] = vmulq_n_s32(vaddq_s32(t_lo[14], t_lo[15]), -cospi_16_64);
  s_hi[14] = vmulq_n_s32(vaddq_s32(t_hi[14], t_hi[15]), -cospi_16_64);
  // s15 = cospi_16_64 * (x14 - x15);
  s_lo[15] = vmulq_n_s32(vsubq_s32(t_lo[14], t_lo[15]), cospi_16_64);
  s_hi[15] = vmulq_n_s32(vsubq_s32(t_hi[14], t_hi[15]), cospi_16_64);

  // final fdct_round_shift
  t_lo[2] = vaddq_s32(s_lo[2], k__DCT_CONST_ROUNDING);
  t_hi[2] = vaddq_s32(s_hi[2], k__DCT_CONST_ROUNDING);
  t_lo[3] = vaddq_s32(s_lo[3], k__DCT_CONST_ROUNDING);
  t_hi[3] = vaddq_s32(s_hi[3], k__DCT_CONST_ROUNDING);
  t_lo[6] = vaddq_s32(s_lo[6], k__DCT_CONST_ROUNDING);
  t_hi[6] = vaddq_s32(s_hi[6], k__DCT_CONST_ROUNDING);
  t_lo[7] = vaddq_s32(s_lo[7], k__DCT_CONST_ROUNDING);
  t_hi[7] = vaddq_s32(s_hi[7], k__DCT_CONST_ROUNDING);
  t_lo[10] = vaddq_s32(s_lo[10], k__DCT_CONST_ROUNDING);
  t_hi[10] = vaddq_s32(s_hi[10], k__DCT_CONST_ROUNDING);
  t_lo[11] = vaddq_s32(s_lo[11], k__DCT_CONST_ROUNDING);
  t_hi[11] = vaddq_s32(s_hi[11], k__DCT_CONST_ROUNDING);
  t_lo[14] = vaddq_s32(s_lo[14], k__DCT_CONST_ROUNDING);
  t_hi[14] = vaddq_s32(s_hi[14], k__DCT_CONST_ROUNDING);
  t_lo[15] = vaddq_s32(s_lo[15], k__DCT_CONST_ROUNDING);
  t_hi[15] = vaddq_s32(s_hi[15], k__DCT_CONST_ROUNDING);

  x_lo[2] = vshrn_n_s32(t_lo[2], DCT_CONST_BITS);
  x_hi[2] = vshrn_n_s32(t_hi[2], DCT_CONST_BITS);
  x_lo[3] = vshrn_n_s32(t_lo[3], DCT_CONST_BITS);
  x_hi[3] = vshrn_n_s32(t_hi[3], DCT_CONST_BITS);
  x_lo[6] = vshrn_n_s32(t_lo[6], DCT_CONST_BITS);
  x_hi[6] = vshrn_n_s32(t_hi[6], DCT_CONST_BITS);
  x_lo[7] = vshrn_n_s32(t_lo[7], DCT_CONST_BITS);
  x_hi[7] = vshrn_n_s32(t_hi[7], DCT_CONST_BITS);
  x_lo[10] = vshrn_n_s32(t_lo[10], DCT_CONST_BITS);
  x_hi[10] = vshrn_n_s32(t_hi[10], DCT_CONST_BITS);
  x_lo[11] = vshrn_n_s32(t_lo[11], DCT_CONST_BITS);
  x_hi[11] = vshrn_n_s32(t_hi[11], DCT_CONST_BITS);
  x_lo[14] = vshrn_n_s32(t_lo[14], DCT_CONST_BITS);
  x_hi[14] = vshrn_n_s32(t_hi[14], DCT_CONST_BITS);
  x_lo[15] = vshrn_n_s32(t_lo[15], DCT_CONST_BITS);
  x_hi[15] = vshrn_n_s32(t_hi[15], DCT_CONST_BITS);

  // x0, x1, x4, x5, x8, x9, x12, x13 narrow down to 16-bits directly
  x_lo[0] = vmovn_s32(t_lo[0]);
  x_hi[0] = vmovn_s32(t_hi[0]);
  x_lo[1] = vmovn_s32(t_lo[1]);
  x_hi[1] = vmovn_s32(t_hi[1]);
  x_lo[4] = vmovn_s32(t_lo[4]);
  x_hi[4] = vmovn_s32(t_hi[4]);
  x_lo[5] = vmovn_s32(t_lo[5]);
  x_hi[5] = vmovn_s32(t_hi[5]);
  x_lo[8] = vmovn_s32(t_lo[8]);
  x_hi[8] = vmovn_s32(t_hi[8]);
  x_lo[9] = vmovn_s32(t_lo[9]);
  x_hi[9] = vmovn_s32(t_hi[9]);
  x_lo[12] = vmovn_s32(t_lo[12]);
  x_hi[12] = vmovn_s32(t_hi[12]);
  x_lo[13] = vmovn_s32(t_lo[13]);
  x_hi[13] = vmovn_s32(t_hi[13]);

  in[0] = vcombine_s16(x_lo[0], x_hi[0]);
  in[1] = vnegq_s16(vcombine_s16(x_lo[8], x_hi[8]));
  in[2] = vcombine_s16(x_lo[12], x_hi[12]);
  in[3] = vnegq_s16(vcombine_s16(x_lo[4], x_hi[4]));
  in[4] = vcombine_s16(x_lo[6], x_hi[6]);
  in[5] = vcombine_s16(x_lo[14], x_hi[14]);
  in[6] = vcombine_s16(x_lo[10], x_hi[10]);
  in[7] = vcombine_s16(x_lo[2], x_hi[2]);
  in[8] = vcombine_s16(x_lo[3], x_hi[3]);
  in[9] = vcombine_s16(x_lo[11], x_hi[11]);
  in[10] = vcombine_s16(x_lo[15], x_hi[15]);
  in[11] = vcombine_s16(x_lo[7], x_hi[7]);
  in[12] = vcombine_s16(x_lo[5], x_hi[5]);
  in[13] = vnegq_s16(vcombine_s16(x_lo[13], x_hi[13]));
  in[14] = vcombine_s16(x_lo[9], x_hi[9]);
  in[15] = vnegq_s16(vcombine_s16(x_lo[1], x_hi[1]));
}

static void fdct16x16_neon(int16x8_t *in0, int16x8_t *in1) {
  // Left half.
  fdct16_8col(in0);
  // Right half.
  fdct16_8col(in1);
  transpose_s16_16x16(in0, in1);
}

static void fadst16x16_neon(int16x8_t *in0, int16x8_t *in1) {
  fadst16_8col(in0);
  fadst16_8col(in1);
  transpose_s16_16x16(in0, in1);
}

void vp9_fht16x16_neon(const int16_t *input, tran_low_t *output, int stride,
                       int tx_type) {
  int16x8_t in0[16], in1[16];

  switch (tx_type) {
    case DCT_DCT: vpx_fdct16x16_neon(input, output, stride); break;
    case ADST_DCT:
      load_buffer_16x16(input, in0, in1, stride);
      fadst16x16_neon(in0, in1);
      right_shift_16x16(in0, in1);
      fdct16x16_neon(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case DCT_ADST:
      load_buffer_16x16(input, in0, in1, stride);
      fdct16x16_neon(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16x16_neon(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    default:
      assert(tx_type == ADST_ADST);
      load_buffer_16x16(input, in0, in1, stride);
      fadst16x16_neon(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16x16_neon(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
  }
}
