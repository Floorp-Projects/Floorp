/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/arm/idct_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/inv_txfm.h"

static INLINE void highbd_idct8x8_1_add_kernel(uint16_t **dest,
                                               const int stride,
                                               const int16x8_t res,
                                               const int16x8_t max) {
  const uint16x8_t a = vld1q_u16(*dest);
  const int16x8_t b = vaddq_s16(res, vreinterpretq_s16_u16(a));
  const int16x8_t c = vminq_s16(b, max);
  const uint16x8_t d = vqshluq_n_s16(c, 0);
  vst1q_u16(*dest, d);
  *dest += stride;
}

void vpx_highbd_idct8x8_1_add_neon(const tran_low_t *input, uint8_t *dest8,
                                   int stride, int bd) {
  const int16x8_t max = vdupq_n_s16((1 << bd) - 1);
  const tran_low_t out0 =
      HIGHBD_WRAPLOW(dct_const_round_shift(input[0] * cospi_16_64), bd);
  const tran_low_t out1 =
      HIGHBD_WRAPLOW(dct_const_round_shift(out0 * cospi_16_64), bd);
  const int16_t a1 = ROUND_POWER_OF_TWO(out1, 5);
  const int16x8_t dc = vdupq_n_s16(a1);
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  highbd_idct8x8_1_add_kernel(&dest, stride, dc, max);
  highbd_idct8x8_1_add_kernel(&dest, stride, dc, max);
  highbd_idct8x8_1_add_kernel(&dest, stride, dc, max);
  highbd_idct8x8_1_add_kernel(&dest, stride, dc, max);
  highbd_idct8x8_1_add_kernel(&dest, stride, dc, max);
  highbd_idct8x8_1_add_kernel(&dest, stride, dc, max);
  highbd_idct8x8_1_add_kernel(&dest, stride, dc, max);
  highbd_idct8x8_1_add_kernel(&dest, stride, dc, max);
}

static INLINE void idct8x8_12_half1d_bd10(
    const int32x4_t cospis0, const int32x4_t cospis1, int32x4_t *const io0,
    int32x4_t *const io1, int32x4_t *const io2, int32x4_t *const io3,
    int32x4_t *const io4, int32x4_t *const io5, int32x4_t *const io6,
    int32x4_t *const io7) {
  int32x4_t step1[8], step2[8];

  transpose_s32_4x4(io0, io1, io2, io3);

  // stage 1
  step1[4] = vmulq_lane_s32(*io1, vget_high_s32(cospis1), 1);
  step1[5] = vmulq_lane_s32(*io3, vget_high_s32(cospis1), 0);
  step1[6] = vmulq_lane_s32(*io3, vget_low_s32(cospis1), 1);
  step1[7] = vmulq_lane_s32(*io1, vget_low_s32(cospis1), 0);
  step1[4] = vrshrq_n_s32(step1[4], 14);
  step1[5] = vrshrq_n_s32(step1[5], 14);
  step1[6] = vrshrq_n_s32(step1[6], 14);
  step1[7] = vrshrq_n_s32(step1[7], 14);

  // stage 2
  step2[1] = vmulq_lane_s32(*io0, vget_high_s32(cospis0), 0);
  step2[2] = vmulq_lane_s32(*io2, vget_high_s32(cospis0), 1);
  step2[3] = vmulq_lane_s32(*io2, vget_low_s32(cospis0), 1);
  step2[1] = vrshrq_n_s32(step2[1], 14);
  step2[2] = vrshrq_n_s32(step2[2], 14);
  step2[3] = vrshrq_n_s32(step2[3], 14);

  step2[4] = vaddq_s32(step1[4], step1[5]);
  step2[5] = vsubq_s32(step1[4], step1[5]);
  step2[6] = vsubq_s32(step1[7], step1[6]);
  step2[7] = vaddq_s32(step1[7], step1[6]);

  // stage 3
  step1[0] = vaddq_s32(step2[1], step2[3]);
  step1[1] = vaddq_s32(step2[1], step2[2]);
  step1[2] = vsubq_s32(step2[1], step2[2]);
  step1[3] = vsubq_s32(step2[1], step2[3]);

  step1[6] = vmulq_lane_s32(step2[6], vget_high_s32(cospis0), 0);
  step1[5] = vmlsq_lane_s32(step1[6], step2[5], vget_high_s32(cospis0), 0);
  step1[6] = vmlaq_lane_s32(step1[6], step2[5], vget_high_s32(cospis0), 0);
  step1[5] = vrshrq_n_s32(step1[5], 14);
  step1[6] = vrshrq_n_s32(step1[6], 14);

  // stage 4
  *io0 = vaddq_s32(step1[0], step2[7]);
  *io1 = vaddq_s32(step1[1], step1[6]);
  *io2 = vaddq_s32(step1[2], step1[5]);
  *io3 = vaddq_s32(step1[3], step2[4]);
  *io4 = vsubq_s32(step1[3], step2[4]);
  *io5 = vsubq_s32(step1[2], step1[5]);
  *io6 = vsubq_s32(step1[1], step1[6]);
  *io7 = vsubq_s32(step1[0], step2[7]);
}

static INLINE void idct8x8_12_half1d_bd12(
    const int32x4_t cospis0, const int32x4_t cospis1, int32x4_t *const io0,
    int32x4_t *const io1, int32x4_t *const io2, int32x4_t *const io3,
    int32x4_t *const io4, int32x4_t *const io5, int32x4_t *const io6,
    int32x4_t *const io7) {
  int32x2_t input_1l, input_1h, input_3l, input_3h;
  int32x2_t step1l[2], step1h[2];
  int32x4_t step1[8], step2[8];
  int64x2_t t64[8];
  int32x2_t t32[8];

  transpose_s32_4x4(io0, io1, io2, io3);

  // stage 1
  input_1l = vget_low_s32(*io1);
  input_1h = vget_high_s32(*io1);
  input_3l = vget_low_s32(*io3);
  input_3h = vget_high_s32(*io3);
  step1l[0] = vget_low_s32(*io0);
  step1h[0] = vget_high_s32(*io0);
  step1l[1] = vget_low_s32(*io2);
  step1h[1] = vget_high_s32(*io2);

  t64[0] = vmull_lane_s32(input_1l, vget_high_s32(cospis1), 1);
  t64[1] = vmull_lane_s32(input_1h, vget_high_s32(cospis1), 1);
  t64[2] = vmull_lane_s32(input_3l, vget_high_s32(cospis1), 0);
  t64[3] = vmull_lane_s32(input_3h, vget_high_s32(cospis1), 0);
  t64[4] = vmull_lane_s32(input_3l, vget_low_s32(cospis1), 1);
  t64[5] = vmull_lane_s32(input_3h, vget_low_s32(cospis1), 1);
  t64[6] = vmull_lane_s32(input_1l, vget_low_s32(cospis1), 0);
  t64[7] = vmull_lane_s32(input_1h, vget_low_s32(cospis1), 0);
  t32[0] = vrshrn_n_s64(t64[0], 14);
  t32[1] = vrshrn_n_s64(t64[1], 14);
  t32[2] = vrshrn_n_s64(t64[2], 14);
  t32[3] = vrshrn_n_s64(t64[3], 14);
  t32[4] = vrshrn_n_s64(t64[4], 14);
  t32[5] = vrshrn_n_s64(t64[5], 14);
  t32[6] = vrshrn_n_s64(t64[6], 14);
  t32[7] = vrshrn_n_s64(t64[7], 14);
  step1[4] = vcombine_s32(t32[0], t32[1]);
  step1[5] = vcombine_s32(t32[2], t32[3]);
  step1[6] = vcombine_s32(t32[4], t32[5]);
  step1[7] = vcombine_s32(t32[6], t32[7]);

  // stage 2
  t64[2] = vmull_lane_s32(step1l[0], vget_high_s32(cospis0), 0);
  t64[3] = vmull_lane_s32(step1h[0], vget_high_s32(cospis0), 0);
  t64[4] = vmull_lane_s32(step1l[1], vget_high_s32(cospis0), 1);
  t64[5] = vmull_lane_s32(step1h[1], vget_high_s32(cospis0), 1);
  t64[6] = vmull_lane_s32(step1l[1], vget_low_s32(cospis0), 1);
  t64[7] = vmull_lane_s32(step1h[1], vget_low_s32(cospis0), 1);
  t32[2] = vrshrn_n_s64(t64[2], 14);
  t32[3] = vrshrn_n_s64(t64[3], 14);
  t32[4] = vrshrn_n_s64(t64[4], 14);
  t32[5] = vrshrn_n_s64(t64[5], 14);
  t32[6] = vrshrn_n_s64(t64[6], 14);
  t32[7] = vrshrn_n_s64(t64[7], 14);
  step2[1] = vcombine_s32(t32[2], t32[3]);
  step2[2] = vcombine_s32(t32[4], t32[5]);
  step2[3] = vcombine_s32(t32[6], t32[7]);

  step2[4] = vaddq_s32(step1[4], step1[5]);
  step2[5] = vsubq_s32(step1[4], step1[5]);
  step2[6] = vsubq_s32(step1[7], step1[6]);
  step2[7] = vaddq_s32(step1[7], step1[6]);

  // stage 3
  step1[0] = vaddq_s32(step2[1], step2[3]);
  step1[1] = vaddq_s32(step2[1], step2[2]);
  step1[2] = vsubq_s32(step2[1], step2[2]);
  step1[3] = vsubq_s32(step2[1], step2[3]);

  t64[2] = vmull_lane_s32(vget_low_s32(step2[6]), vget_high_s32(cospis0), 0);
  t64[3] = vmull_lane_s32(vget_high_s32(step2[6]), vget_high_s32(cospis0), 0);
  t64[0] =
      vmlsl_lane_s32(t64[2], vget_low_s32(step2[5]), vget_high_s32(cospis0), 0);
  t64[1] = vmlsl_lane_s32(t64[3], vget_high_s32(step2[5]),
                          vget_high_s32(cospis0), 0);
  t64[2] =
      vmlal_lane_s32(t64[2], vget_low_s32(step2[5]), vget_high_s32(cospis0), 0);
  t64[3] = vmlal_lane_s32(t64[3], vget_high_s32(step2[5]),
                          vget_high_s32(cospis0), 0);
  t32[0] = vrshrn_n_s64(t64[0], 14);
  t32[1] = vrshrn_n_s64(t64[1], 14);
  t32[2] = vrshrn_n_s64(t64[2], 14);
  t32[3] = vrshrn_n_s64(t64[3], 14);
  step1[5] = vcombine_s32(t32[0], t32[1]);
  step1[6] = vcombine_s32(t32[2], t32[3]);

  // stage 4
  *io0 = vaddq_s32(step1[0], step2[7]);
  *io1 = vaddq_s32(step1[1], step1[6]);
  *io2 = vaddq_s32(step1[2], step1[5]);
  *io3 = vaddq_s32(step1[3], step2[4]);
  *io4 = vsubq_s32(step1[3], step2[4]);
  *io5 = vsubq_s32(step1[2], step1[5]);
  *io6 = vsubq_s32(step1[1], step1[6]);
  *io7 = vsubq_s32(step1[0], step2[7]);
}

static INLINE void highbd_add8x8(int16x8_t a0, int16x8_t a1, int16x8_t a2,
                                 int16x8_t a3, int16x8_t a4, int16x8_t a5,
                                 int16x8_t a6, int16x8_t a7, uint16_t *dest,
                                 const int stride, const int bd) {
  const int16x8_t max = vdupq_n_s16((1 << bd) - 1);
  const uint16_t *dst = dest;
  uint16x8_t d0, d1, d2, d3, d4, d5, d6, d7;
  uint16x8_t d0_u16, d1_u16, d2_u16, d3_u16, d4_u16, d5_u16, d6_u16, d7_u16;
  int16x8_t d0_s16, d1_s16, d2_s16, d3_s16, d4_s16, d5_s16, d6_s16, d7_s16;

  d0 = vld1q_u16(dst);
  dst += stride;
  d1 = vld1q_u16(dst);
  dst += stride;
  d2 = vld1q_u16(dst);
  dst += stride;
  d3 = vld1q_u16(dst);
  dst += stride;
  d4 = vld1q_u16(dst);
  dst += stride;
  d5 = vld1q_u16(dst);
  dst += stride;
  d6 = vld1q_u16(dst);
  dst += stride;
  d7 = vld1q_u16(dst);

  d0_s16 = vqaddq_s16(a0, vreinterpretq_s16_u16(d0));
  d1_s16 = vqaddq_s16(a1, vreinterpretq_s16_u16(d1));
  d2_s16 = vqaddq_s16(a2, vreinterpretq_s16_u16(d2));
  d3_s16 = vqaddq_s16(a3, vreinterpretq_s16_u16(d3));
  d4_s16 = vqaddq_s16(a4, vreinterpretq_s16_u16(d4));
  d5_s16 = vqaddq_s16(a5, vreinterpretq_s16_u16(d5));
  d6_s16 = vqaddq_s16(a6, vreinterpretq_s16_u16(d6));
  d7_s16 = vqaddq_s16(a7, vreinterpretq_s16_u16(d7));

  d0_s16 = vminq_s16(d0_s16, max);
  d1_s16 = vminq_s16(d1_s16, max);
  d2_s16 = vminq_s16(d2_s16, max);
  d3_s16 = vminq_s16(d3_s16, max);
  d4_s16 = vminq_s16(d4_s16, max);
  d5_s16 = vminq_s16(d5_s16, max);
  d6_s16 = vminq_s16(d6_s16, max);
  d7_s16 = vminq_s16(d7_s16, max);
  d0_u16 = vqshluq_n_s16(d0_s16, 0);
  d1_u16 = vqshluq_n_s16(d1_s16, 0);
  d2_u16 = vqshluq_n_s16(d2_s16, 0);
  d3_u16 = vqshluq_n_s16(d3_s16, 0);
  d4_u16 = vqshluq_n_s16(d4_s16, 0);
  d5_u16 = vqshluq_n_s16(d5_s16, 0);
  d6_u16 = vqshluq_n_s16(d6_s16, 0);
  d7_u16 = vqshluq_n_s16(d7_s16, 0);

  vst1q_u16(dest, d0_u16);
  dest += stride;
  vst1q_u16(dest, d1_u16);
  dest += stride;
  vst1q_u16(dest, d2_u16);
  dest += stride;
  vst1q_u16(dest, d3_u16);
  dest += stride;
  vst1q_u16(dest, d4_u16);
  dest += stride;
  vst1q_u16(dest, d5_u16);
  dest += stride;
  vst1q_u16(dest, d6_u16);
  dest += stride;
  vst1q_u16(dest, d7_u16);
}

void vpx_highbd_idct8x8_12_add_neon(const tran_low_t *input, uint8_t *dest8,
                                    int stride, int bd) {
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);
  int32x4_t a0 = vld1q_s32(input);
  int32x4_t a1 = vld1q_s32(input + 8);
  int32x4_t a2 = vld1q_s32(input + 16);
  int32x4_t a3 = vld1q_s32(input + 24);
  int16x8_t c0, c1, c2, c3, c4, c5, c6, c7;

  if (bd == 8) {
    const int16x8_t cospis = vld1q_s16(kCospi);
    const int16x8_t cospisd = vaddq_s16(cospis, cospis);
    const int16x4_t cospis0 = vget_low_s16(cospis);     // cospi 0, 8, 16, 24
    const int16x4_t cospisd0 = vget_low_s16(cospisd);   // doubled 0, 8, 16, 24
    const int16x4_t cospisd1 = vget_high_s16(cospisd);  // doubled 4, 12, 20, 28
    int16x4_t b0 = vmovn_s32(a0);
    int16x4_t b1 = vmovn_s32(a1);
    int16x4_t b2 = vmovn_s32(a2);
    int16x4_t b3 = vmovn_s32(a3);
    int16x4_t b4, b5, b6, b7;

    idct8x8_12_pass1_bd8(cospis0, cospisd0, cospisd1, &b0, &b1, &b2, &b3, &b4,
                         &b5, &b6, &b7);
    idct8x8_12_pass2_bd8(cospis0, cospisd0, cospisd1, b0, b1, b2, b3, b4, b5,
                         b6, b7, &c0, &c1, &c2, &c3, &c4, &c5, &c6, &c7);
    c0 = vrshrq_n_s16(c0, 5);
    c1 = vrshrq_n_s16(c1, 5);
    c2 = vrshrq_n_s16(c2, 5);
    c3 = vrshrq_n_s16(c3, 5);
    c4 = vrshrq_n_s16(c4, 5);
    c5 = vrshrq_n_s16(c5, 5);
    c6 = vrshrq_n_s16(c6, 5);
    c7 = vrshrq_n_s16(c7, 5);
  } else {
    const int32x4_t cospis0 = vld1q_s32(kCospi32);      // cospi 0, 8, 16, 24
    const int32x4_t cospis1 = vld1q_s32(kCospi32 + 4);  // cospi 4, 12, 20, 28
    int32x4_t a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15;

    if (bd == 10) {
      idct8x8_12_half1d_bd10(cospis0, cospis1, &a0, &a1, &a2, &a3, &a4, &a5,
                             &a6, &a7);
      idct8x8_12_half1d_bd10(cospis0, cospis1, &a0, &a1, &a2, &a3, &a8, &a9,
                             &a10, &a11);
      idct8x8_12_half1d_bd10(cospis0, cospis1, &a4, &a5, &a6, &a7, &a12, &a13,
                             &a14, &a15);
    } else {
      idct8x8_12_half1d_bd12(cospis0, cospis1, &a0, &a1, &a2, &a3, &a4, &a5,
                             &a6, &a7);
      idct8x8_12_half1d_bd12(cospis0, cospis1, &a0, &a1, &a2, &a3, &a8, &a9,
                             &a10, &a11);
      idct8x8_12_half1d_bd12(cospis0, cospis1, &a4, &a5, &a6, &a7, &a12, &a13,
                             &a14, &a15);
    }
    c0 = vcombine_s16(vrshrn_n_s32(a0, 5), vrshrn_n_s32(a4, 5));
    c1 = vcombine_s16(vrshrn_n_s32(a1, 5), vrshrn_n_s32(a5, 5));
    c2 = vcombine_s16(vrshrn_n_s32(a2, 5), vrshrn_n_s32(a6, 5));
    c3 = vcombine_s16(vrshrn_n_s32(a3, 5), vrshrn_n_s32(a7, 5));
    c4 = vcombine_s16(vrshrn_n_s32(a8, 5), vrshrn_n_s32(a12, 5));
    c5 = vcombine_s16(vrshrn_n_s32(a9, 5), vrshrn_n_s32(a13, 5));
    c6 = vcombine_s16(vrshrn_n_s32(a10, 5), vrshrn_n_s32(a14, 5));
    c7 = vcombine_s16(vrshrn_n_s32(a11, 5), vrshrn_n_s32(a15, 5));
  }
  highbd_add8x8(c0, c1, c2, c3, c4, c5, c6, c7, dest, stride, bd);
}

static INLINE void idct8x8_64_half1d_bd10(
    const int32x4_t cospis0, const int32x4_t cospis1, int32x4_t *const io0,
    int32x4_t *const io1, int32x4_t *const io2, int32x4_t *const io3,
    int32x4_t *const io4, int32x4_t *const io5, int32x4_t *const io6,
    int32x4_t *const io7) {
  int32x4_t step1[8], step2[8];

  transpose_s32_8x4(io0, io1, io2, io3, io4, io5, io6, io7);

  // stage 1
  step1[4] = vmulq_lane_s32(*io1, vget_high_s32(cospis1), 1);
  step1[5] = vmulq_lane_s32(*io3, vget_high_s32(cospis1), 0);
  step1[6] = vmulq_lane_s32(*io3, vget_low_s32(cospis1), 1);
  step1[7] = vmulq_lane_s32(*io1, vget_low_s32(cospis1), 0);

  step1[4] = vmlsq_lane_s32(step1[4], *io7, vget_low_s32(cospis1), 0);
  step1[5] = vmlaq_lane_s32(step1[5], *io5, vget_low_s32(cospis1), 1);
  step1[6] = vmlsq_lane_s32(step1[6], *io5, vget_high_s32(cospis1), 0);
  step1[7] = vmlaq_lane_s32(step1[7], *io7, vget_high_s32(cospis1), 1);

  step1[4] = vrshrq_n_s32(step1[4], 14);
  step1[5] = vrshrq_n_s32(step1[5], 14);
  step1[6] = vrshrq_n_s32(step1[6], 14);
  step1[7] = vrshrq_n_s32(step1[7], 14);

  // stage 2
  step2[1] = vmulq_lane_s32(*io0, vget_high_s32(cospis0), 0);
  step2[2] = vmulq_lane_s32(*io2, vget_high_s32(cospis0), 1);
  step2[3] = vmulq_lane_s32(*io2, vget_low_s32(cospis0), 1);

  step2[0] = vmlaq_lane_s32(step2[1], *io4, vget_high_s32(cospis0), 0);
  step2[1] = vmlsq_lane_s32(step2[1], *io4, vget_high_s32(cospis0), 0);
  step2[2] = vmlsq_lane_s32(step2[2], *io6, vget_low_s32(cospis0), 1);
  step2[3] = vmlaq_lane_s32(step2[3], *io6, vget_high_s32(cospis0), 1);

  step2[0] = vrshrq_n_s32(step2[0], 14);
  step2[1] = vrshrq_n_s32(step2[1], 14);
  step2[2] = vrshrq_n_s32(step2[2], 14);
  step2[3] = vrshrq_n_s32(step2[3], 14);

  step2[4] = vaddq_s32(step1[4], step1[5]);
  step2[5] = vsubq_s32(step1[4], step1[5]);
  step2[6] = vsubq_s32(step1[7], step1[6]);
  step2[7] = vaddq_s32(step1[7], step1[6]);

  // stage 3
  step1[0] = vaddq_s32(step2[0], step2[3]);
  step1[1] = vaddq_s32(step2[1], step2[2]);
  step1[2] = vsubq_s32(step2[1], step2[2]);
  step1[3] = vsubq_s32(step2[0], step2[3]);

  step1[6] = vmulq_lane_s32(step2[6], vget_high_s32(cospis0), 0);
  step1[5] = vmlsq_lane_s32(step1[6], step2[5], vget_high_s32(cospis0), 0);
  step1[6] = vmlaq_lane_s32(step1[6], step2[5], vget_high_s32(cospis0), 0);
  step1[5] = vrshrq_n_s32(step1[5], 14);
  step1[6] = vrshrq_n_s32(step1[6], 14);

  // stage 4
  *io0 = vaddq_s32(step1[0], step2[7]);
  *io1 = vaddq_s32(step1[1], step1[6]);
  *io2 = vaddq_s32(step1[2], step1[5]);
  *io3 = vaddq_s32(step1[3], step2[4]);
  *io4 = vsubq_s32(step1[3], step2[4]);
  *io5 = vsubq_s32(step1[2], step1[5]);
  *io6 = vsubq_s32(step1[1], step1[6]);
  *io7 = vsubq_s32(step1[0], step2[7]);
}

static INLINE void idct8x8_64_half1d_bd12(
    const int32x4_t cospis0, const int32x4_t cospis1, int32x4_t *const io0,
    int32x4_t *const io1, int32x4_t *const io2, int32x4_t *const io3,
    int32x4_t *const io4, int32x4_t *const io5, int32x4_t *const io6,
    int32x4_t *const io7) {
  int32x2_t input_1l, input_1h, input_3l, input_3h, input_5l, input_5h,
      input_7l, input_7h;
  int32x2_t step1l[4], step1h[4];
  int32x4_t step1[8], step2[8];
  int64x2_t t64[8];
  int32x2_t t32[8];

  transpose_s32_8x4(io0, io1, io2, io3, io4, io5, io6, io7);

  // stage 1
  input_1l = vget_low_s32(*io1);
  input_1h = vget_high_s32(*io1);
  input_3l = vget_low_s32(*io3);
  input_3h = vget_high_s32(*io3);
  input_5l = vget_low_s32(*io5);
  input_5h = vget_high_s32(*io5);
  input_7l = vget_low_s32(*io7);
  input_7h = vget_high_s32(*io7);
  step1l[0] = vget_low_s32(*io0);
  step1h[0] = vget_high_s32(*io0);
  step1l[1] = vget_low_s32(*io2);
  step1h[1] = vget_high_s32(*io2);
  step1l[2] = vget_low_s32(*io4);
  step1h[2] = vget_high_s32(*io4);
  step1l[3] = vget_low_s32(*io6);
  step1h[3] = vget_high_s32(*io6);

  t64[0] = vmull_lane_s32(input_1l, vget_high_s32(cospis1), 1);
  t64[1] = vmull_lane_s32(input_1h, vget_high_s32(cospis1), 1);
  t64[2] = vmull_lane_s32(input_3l, vget_high_s32(cospis1), 0);
  t64[3] = vmull_lane_s32(input_3h, vget_high_s32(cospis1), 0);
  t64[4] = vmull_lane_s32(input_3l, vget_low_s32(cospis1), 1);
  t64[5] = vmull_lane_s32(input_3h, vget_low_s32(cospis1), 1);
  t64[6] = vmull_lane_s32(input_1l, vget_low_s32(cospis1), 0);
  t64[7] = vmull_lane_s32(input_1h, vget_low_s32(cospis1), 0);
  t64[0] = vmlsl_lane_s32(t64[0], input_7l, vget_low_s32(cospis1), 0);
  t64[1] = vmlsl_lane_s32(t64[1], input_7h, vget_low_s32(cospis1), 0);
  t64[2] = vmlal_lane_s32(t64[2], input_5l, vget_low_s32(cospis1), 1);
  t64[3] = vmlal_lane_s32(t64[3], input_5h, vget_low_s32(cospis1), 1);
  t64[4] = vmlsl_lane_s32(t64[4], input_5l, vget_high_s32(cospis1), 0);
  t64[5] = vmlsl_lane_s32(t64[5], input_5h, vget_high_s32(cospis1), 0);
  t64[6] = vmlal_lane_s32(t64[6], input_7l, vget_high_s32(cospis1), 1);
  t64[7] = vmlal_lane_s32(t64[7], input_7h, vget_high_s32(cospis1), 1);
  t32[0] = vrshrn_n_s64(t64[0], 14);
  t32[1] = vrshrn_n_s64(t64[1], 14);
  t32[2] = vrshrn_n_s64(t64[2], 14);
  t32[3] = vrshrn_n_s64(t64[3], 14);
  t32[4] = vrshrn_n_s64(t64[4], 14);
  t32[5] = vrshrn_n_s64(t64[5], 14);
  t32[6] = vrshrn_n_s64(t64[6], 14);
  t32[7] = vrshrn_n_s64(t64[7], 14);
  step1[4] = vcombine_s32(t32[0], t32[1]);
  step1[5] = vcombine_s32(t32[2], t32[3]);
  step1[6] = vcombine_s32(t32[4], t32[5]);
  step1[7] = vcombine_s32(t32[6], t32[7]);

  // stage 2
  t64[2] = vmull_lane_s32(step1l[0], vget_high_s32(cospis0), 0);
  t64[3] = vmull_lane_s32(step1h[0], vget_high_s32(cospis0), 0);
  t64[4] = vmull_lane_s32(step1l[1], vget_high_s32(cospis0), 1);
  t64[5] = vmull_lane_s32(step1h[1], vget_high_s32(cospis0), 1);
  t64[6] = vmull_lane_s32(step1l[1], vget_low_s32(cospis0), 1);
  t64[7] = vmull_lane_s32(step1h[1], vget_low_s32(cospis0), 1);
  t64[0] = vmlal_lane_s32(t64[2], step1l[2], vget_high_s32(cospis0), 0);
  t64[1] = vmlal_lane_s32(t64[3], step1h[2], vget_high_s32(cospis0), 0);
  t64[2] = vmlsl_lane_s32(t64[2], step1l[2], vget_high_s32(cospis0), 0);
  t64[3] = vmlsl_lane_s32(t64[3], step1h[2], vget_high_s32(cospis0), 0);
  t64[4] = vmlsl_lane_s32(t64[4], step1l[3], vget_low_s32(cospis0), 1);
  t64[5] = vmlsl_lane_s32(t64[5], step1h[3], vget_low_s32(cospis0), 1);
  t64[6] = vmlal_lane_s32(t64[6], step1l[3], vget_high_s32(cospis0), 1);
  t64[7] = vmlal_lane_s32(t64[7], step1h[3], vget_high_s32(cospis0), 1);
  t32[0] = vrshrn_n_s64(t64[0], 14);
  t32[1] = vrshrn_n_s64(t64[1], 14);
  t32[2] = vrshrn_n_s64(t64[2], 14);
  t32[3] = vrshrn_n_s64(t64[3], 14);
  t32[4] = vrshrn_n_s64(t64[4], 14);
  t32[5] = vrshrn_n_s64(t64[5], 14);
  t32[6] = vrshrn_n_s64(t64[6], 14);
  t32[7] = vrshrn_n_s64(t64[7], 14);
  step2[0] = vcombine_s32(t32[0], t32[1]);
  step2[1] = vcombine_s32(t32[2], t32[3]);
  step2[2] = vcombine_s32(t32[4], t32[5]);
  step2[3] = vcombine_s32(t32[6], t32[7]);

  step2[4] = vaddq_s32(step1[4], step1[5]);
  step2[5] = vsubq_s32(step1[4], step1[5]);
  step2[6] = vsubq_s32(step1[7], step1[6]);
  step2[7] = vaddq_s32(step1[7], step1[6]);

  // stage 3
  step1[0] = vaddq_s32(step2[0], step2[3]);
  step1[1] = vaddq_s32(step2[1], step2[2]);
  step1[2] = vsubq_s32(step2[1], step2[2]);
  step1[3] = vsubq_s32(step2[0], step2[3]);

  t64[2] = vmull_lane_s32(vget_low_s32(step2[6]), vget_high_s32(cospis0), 0);
  t64[3] = vmull_lane_s32(vget_high_s32(step2[6]), vget_high_s32(cospis0), 0);
  t64[0] =
      vmlsl_lane_s32(t64[2], vget_low_s32(step2[5]), vget_high_s32(cospis0), 0);
  t64[1] = vmlsl_lane_s32(t64[3], vget_high_s32(step2[5]),
                          vget_high_s32(cospis0), 0);
  t64[2] =
      vmlal_lane_s32(t64[2], vget_low_s32(step2[5]), vget_high_s32(cospis0), 0);
  t64[3] = vmlal_lane_s32(t64[3], vget_high_s32(step2[5]),
                          vget_high_s32(cospis0), 0);
  t32[0] = vrshrn_n_s64(t64[0], 14);
  t32[1] = vrshrn_n_s64(t64[1], 14);
  t32[2] = vrshrn_n_s64(t64[2], 14);
  t32[3] = vrshrn_n_s64(t64[3], 14);
  step1[5] = vcombine_s32(t32[0], t32[1]);
  step1[6] = vcombine_s32(t32[2], t32[3]);

  // stage 4
  *io0 = vaddq_s32(step1[0], step2[7]);
  *io1 = vaddq_s32(step1[1], step1[6]);
  *io2 = vaddq_s32(step1[2], step1[5]);
  *io3 = vaddq_s32(step1[3], step2[4]);
  *io4 = vsubq_s32(step1[3], step2[4]);
  *io5 = vsubq_s32(step1[2], step1[5]);
  *io6 = vsubq_s32(step1[1], step1[6]);
  *io7 = vsubq_s32(step1[0], step2[7]);
}

void vpx_highbd_idct8x8_64_add_neon(const tran_low_t *input, uint8_t *dest8,
                                    int stride, int bd) {
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);
  int32x4_t a0 = vld1q_s32(input);
  int32x4_t a1 = vld1q_s32(input + 4);
  int32x4_t a2 = vld1q_s32(input + 8);
  int32x4_t a3 = vld1q_s32(input + 12);
  int32x4_t a4 = vld1q_s32(input + 16);
  int32x4_t a5 = vld1q_s32(input + 20);
  int32x4_t a6 = vld1q_s32(input + 24);
  int32x4_t a7 = vld1q_s32(input + 28);
  int32x4_t a8 = vld1q_s32(input + 32);
  int32x4_t a9 = vld1q_s32(input + 36);
  int32x4_t a10 = vld1q_s32(input + 40);
  int32x4_t a11 = vld1q_s32(input + 44);
  int32x4_t a12 = vld1q_s32(input + 48);
  int32x4_t a13 = vld1q_s32(input + 52);
  int32x4_t a14 = vld1q_s32(input + 56);
  int32x4_t a15 = vld1q_s32(input + 60);
  int16x8_t c0, c1, c2, c3, c4, c5, c6, c7;

  if (bd == 8) {
    const int16x8_t cospis = vld1q_s16(kCospi);
    const int16x4_t cospis0 = vget_low_s16(cospis);   // cospi 0, 8, 16, 24
    const int16x4_t cospis1 = vget_high_s16(cospis);  // cospi 4, 12, 20, 28
    int16x8_t b0 = vcombine_s16(vmovn_s32(a0), vmovn_s32(a1));
    int16x8_t b1 = vcombine_s16(vmovn_s32(a2), vmovn_s32(a3));
    int16x8_t b2 = vcombine_s16(vmovn_s32(a4), vmovn_s32(a5));
    int16x8_t b3 = vcombine_s16(vmovn_s32(a6), vmovn_s32(a7));
    int16x8_t b4 = vcombine_s16(vmovn_s32(a8), vmovn_s32(a9));
    int16x8_t b5 = vcombine_s16(vmovn_s32(a10), vmovn_s32(a11));
    int16x8_t b6 = vcombine_s16(vmovn_s32(a12), vmovn_s32(a13));
    int16x8_t b7 = vcombine_s16(vmovn_s32(a14), vmovn_s32(a15));

    idct8x8_64_1d_bd8(cospis0, cospis1, &b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);
    idct8x8_64_1d_bd8(cospis0, cospis1, &b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);

    c0 = vrshrq_n_s16(b0, 5);
    c1 = vrshrq_n_s16(b1, 5);
    c2 = vrshrq_n_s16(b2, 5);
    c3 = vrshrq_n_s16(b3, 5);
    c4 = vrshrq_n_s16(b4, 5);
    c5 = vrshrq_n_s16(b5, 5);
    c6 = vrshrq_n_s16(b6, 5);
    c7 = vrshrq_n_s16(b7, 5);
  } else {
    const int32x4_t cospis0 = vld1q_s32(kCospi32);      // cospi 0, 8, 16, 24
    const int32x4_t cospis1 = vld1q_s32(kCospi32 + 4);  // cospi 4, 12, 20, 28

    if (bd == 10) {
      idct8x8_64_half1d_bd10(cospis0, cospis1, &a0, &a1, &a2, &a3, &a4, &a5,
                             &a6, &a7);
      idct8x8_64_half1d_bd10(cospis0, cospis1, &a8, &a9, &a10, &a11, &a12, &a13,
                             &a14, &a15);
      idct8x8_64_half1d_bd10(cospis0, cospis1, &a0, &a8, &a1, &a9, &a2, &a10,
                             &a3, &a11);
      idct8x8_64_half1d_bd10(cospis0, cospis1, &a4, &a12, &a5, &a13, &a6, &a14,
                             &a7, &a15);
    } else {
      idct8x8_64_half1d_bd12(cospis0, cospis1, &a0, &a1, &a2, &a3, &a4, &a5,
                             &a6, &a7);
      idct8x8_64_half1d_bd12(cospis0, cospis1, &a8, &a9, &a10, &a11, &a12, &a13,
                             &a14, &a15);
      idct8x8_64_half1d_bd12(cospis0, cospis1, &a0, &a8, &a1, &a9, &a2, &a10,
                             &a3, &a11);
      idct8x8_64_half1d_bd12(cospis0, cospis1, &a4, &a12, &a5, &a13, &a6, &a14,
                             &a7, &a15);
    }
    c0 = vcombine_s16(vrshrn_n_s32(a0, 5), vrshrn_n_s32(a4, 5));
    c1 = vcombine_s16(vrshrn_n_s32(a8, 5), vrshrn_n_s32(a12, 5));
    c2 = vcombine_s16(vrshrn_n_s32(a1, 5), vrshrn_n_s32(a5, 5));
    c3 = vcombine_s16(vrshrn_n_s32(a9, 5), vrshrn_n_s32(a13, 5));
    c4 = vcombine_s16(vrshrn_n_s32(a2, 5), vrshrn_n_s32(a6, 5));
    c5 = vcombine_s16(vrshrn_n_s32(a10, 5), vrshrn_n_s32(a14, 5));
    c6 = vcombine_s16(vrshrn_n_s32(a3, 5), vrshrn_n_s32(a7, 5));
    c7 = vcombine_s16(vrshrn_n_s32(a11, 5), vrshrn_n_s32(a15, 5));
  }
  highbd_add8x8(c0, c1, c2, c3, c4, c5, c6, c7, dest, stride, bd);
}
