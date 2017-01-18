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

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/arm/idct_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/txfm_common.h"

// Only for the first pass of the  _34_ variant. Since it only uses values from
// the top left 8x8 it can safely assume all the remaining values are 0 and skip
// an awful lot of calculations. In fact, only the first 6 columns make the cut.
// None of the elements in the 7th or 8th column are used so it skips any calls
// to input[67] too.
// In C this does a single row of 32 for each call. Here it transposes the top
// left 8x8 to allow using SIMD.

// vp9/common/vp9_scan.c:vp9_default_iscan_32x32 arranges the first 34 non-zero
// coefficients as follows:
//    0  1  2  3  4  5  6  7
// 0  0  2  5 10 17 25
// 1  1  4  8 15 22 30
// 2  3  7 12 18 28
// 3  6 11 16 23 31
// 4  9 14 19 29
// 5 13 20 26
// 6 21 27 33
// 7 24 32
static void idct32_6_neon(const tran_low_t *input, int16_t *output) {
  int16x8_t in0, in1, in2, in3, in4, in5, in6, in7;
  int16x8_t s1_0, s1_1, s1_2, s1_3, s1_4, s1_5, s1_6, s1_7, s1_8, s1_9, s1_10,
      s1_11, s1_12, s1_13, s1_14, s1_15, s1_16, s1_17, s1_18, s1_19, s1_20,
      s1_21, s1_22, s1_23, s1_24, s1_25, s1_26, s1_27, s1_28, s1_29, s1_30,
      s1_31;
  int16x8_t s2_0, s2_1, s2_2, s2_3, s2_4, s2_5, s2_6, s2_7, s2_8, s2_9, s2_10,
      s2_11, s2_12, s2_13, s2_14, s2_15, s2_16, s2_17, s2_18, s2_19, s2_20,
      s2_21, s2_22, s2_23, s2_24, s2_25, s2_26, s2_27, s2_28, s2_29, s2_30,
      s2_31;
  int16x8_t s3_24, s3_25, s3_26, s3_27;

  in0 = load_tran_low_to_s16q(input);
  input += 32;
  in1 = load_tran_low_to_s16q(input);
  input += 32;
  in2 = load_tran_low_to_s16q(input);
  input += 32;
  in3 = load_tran_low_to_s16q(input);
  input += 32;
  in4 = load_tran_low_to_s16q(input);
  input += 32;
  in5 = load_tran_low_to_s16q(input);
  input += 32;
  in6 = load_tran_low_to_s16q(input);
  input += 32;
  in7 = load_tran_low_to_s16q(input);
  transpose_s16_8x8(&in0, &in1, &in2, &in3, &in4, &in5, &in6, &in7);

  // stage 1
  // input[1] * cospi_31_64 - input[31] * cospi_1_64 (but input[31] == 0)
  s1_16 = multiply_shift_and_narrow_s16(in1, cospi_31_64);
  // input[1] * cospi_1_64 + input[31] * cospi_31_64 (but input[31] == 0)
  s1_31 = multiply_shift_and_narrow_s16(in1, cospi_1_64);

  s1_20 = multiply_shift_and_narrow_s16(in5, cospi_27_64);
  s1_27 = multiply_shift_and_narrow_s16(in5, cospi_5_64);

  s1_23 = multiply_shift_and_narrow_s16(in3, -cospi_29_64);
  s1_24 = multiply_shift_and_narrow_s16(in3, cospi_3_64);

  // stage 2
  s2_8 = multiply_shift_and_narrow_s16(in2, cospi_30_64);
  s2_15 = multiply_shift_and_narrow_s16(in2, cospi_2_64);

  // stage 3
  s1_4 = multiply_shift_and_narrow_s16(in4, cospi_28_64);
  s1_7 = multiply_shift_and_narrow_s16(in4, cospi_4_64);

  s1_17 = multiply_accumulate_shift_and_narrow_s16(s1_16, -cospi_4_64, s1_31,
                                                   cospi_28_64);
  s1_30 = multiply_accumulate_shift_and_narrow_s16(s1_16, cospi_28_64, s1_31,
                                                   cospi_4_64);

  s1_21 = multiply_accumulate_shift_and_narrow_s16(s1_20, -cospi_20_64, s1_27,
                                                   cospi_12_64);
  s1_26 = multiply_accumulate_shift_and_narrow_s16(s1_20, cospi_12_64, s1_27,
                                                   cospi_20_64);

  s1_22 = multiply_accumulate_shift_and_narrow_s16(s1_23, -cospi_12_64, s1_24,
                                                   -cospi_20_64);
  s1_25 = multiply_accumulate_shift_and_narrow_s16(s1_23, -cospi_20_64, s1_24,
                                                   cospi_12_64);

  // stage 4
  s1_0 = multiply_shift_and_narrow_s16(in0, cospi_16_64);

  s2_9 = multiply_accumulate_shift_and_narrow_s16(s2_8, -cospi_8_64, s2_15,
                                                  cospi_24_64);
  s2_14 = multiply_accumulate_shift_and_narrow_s16(s2_8, cospi_24_64, s2_15,
                                                   cospi_8_64);

  s2_20 = vsubq_s16(s1_23, s1_20);
  s2_21 = vsubq_s16(s1_22, s1_21);
  s2_22 = vaddq_s16(s1_21, s1_22);
  s2_23 = vaddq_s16(s1_20, s1_23);
  s2_24 = vaddq_s16(s1_24, s1_27);
  s2_25 = vaddq_s16(s1_25, s1_26);
  s2_26 = vsubq_s16(s1_25, s1_26);
  s2_27 = vsubq_s16(s1_24, s1_27);

  // stage 5
  s1_5 = sub_multiply_shift_and_narrow_s16(s1_7, s1_4, cospi_16_64);
  s1_6 = add_multiply_shift_and_narrow_s16(s1_4, s1_7, cospi_16_64);

  s1_18 = multiply_accumulate_shift_and_narrow_s16(s1_17, -cospi_8_64, s1_30,
                                                   cospi_24_64);
  s1_29 = multiply_accumulate_shift_and_narrow_s16(s1_17, cospi_24_64, s1_30,
                                                   cospi_8_64);

  s1_19 = multiply_accumulate_shift_and_narrow_s16(s1_16, -cospi_8_64, s1_31,
                                                   cospi_24_64);
  s1_28 = multiply_accumulate_shift_and_narrow_s16(s1_16, cospi_24_64, s1_31,
                                                   cospi_8_64);

  s1_20 = multiply_accumulate_shift_and_narrow_s16(s2_20, -cospi_24_64, s2_27,
                                                   -cospi_8_64);
  s1_27 = multiply_accumulate_shift_and_narrow_s16(s2_20, -cospi_8_64, s2_27,
                                                   cospi_24_64);

  s1_21 = multiply_accumulate_shift_and_narrow_s16(s2_21, -cospi_24_64, s2_26,
                                                   -cospi_8_64);
  s1_26 = multiply_accumulate_shift_and_narrow_s16(s2_21, -cospi_8_64, s2_26,
                                                   cospi_24_64);

  // stage 6
  s2_0 = vaddq_s16(s1_0, s1_7);
  s2_1 = vaddq_s16(s1_0, s1_6);
  s2_2 = vaddq_s16(s1_0, s1_5);
  s2_3 = vaddq_s16(s1_0, s1_4);
  s2_4 = vsubq_s16(s1_0, s1_4);
  s2_5 = vsubq_s16(s1_0, s1_5);
  s2_6 = vsubq_s16(s1_0, s1_6);
  s2_7 = vsubq_s16(s1_0, s1_7);

  s2_10 = sub_multiply_shift_and_narrow_s16(s2_14, s2_9, cospi_16_64);
  s2_13 = add_multiply_shift_and_narrow_s16(s2_9, s2_14, cospi_16_64);

  s2_11 = sub_multiply_shift_and_narrow_s16(s2_15, s2_8, cospi_16_64);
  s2_12 = add_multiply_shift_and_narrow_s16(s2_8, s2_15, cospi_16_64);

  s2_16 = vaddq_s16(s1_16, s2_23);
  s2_17 = vaddq_s16(s1_17, s2_22);
  s2_18 = vaddq_s16(s1_18, s1_21);
  s2_19 = vaddq_s16(s1_19, s1_20);
  s2_20 = vsubq_s16(s1_19, s1_20);
  s2_21 = vsubq_s16(s1_18, s1_21);
  s2_22 = vsubq_s16(s1_17, s2_22);
  s2_23 = vsubq_s16(s1_16, s2_23);

  s3_24 = vsubq_s16(s1_31, s2_24);
  s3_25 = vsubq_s16(s1_30, s2_25);
  s3_26 = vsubq_s16(s1_29, s1_26);
  s3_27 = vsubq_s16(s1_28, s1_27);
  s2_28 = vaddq_s16(s1_27, s1_28);
  s2_29 = vaddq_s16(s1_26, s1_29);
  s2_30 = vaddq_s16(s2_25, s1_30);
  s2_31 = vaddq_s16(s2_24, s1_31);

  // stage 7
  s1_0 = vaddq_s16(s2_0, s2_15);
  s1_1 = vaddq_s16(s2_1, s2_14);
  s1_2 = vaddq_s16(s2_2, s2_13);
  s1_3 = vaddq_s16(s2_3, s2_12);
  s1_4 = vaddq_s16(s2_4, s2_11);
  s1_5 = vaddq_s16(s2_5, s2_10);
  s1_6 = vaddq_s16(s2_6, s2_9);
  s1_7 = vaddq_s16(s2_7, s2_8);
  s1_8 = vsubq_s16(s2_7, s2_8);
  s1_9 = vsubq_s16(s2_6, s2_9);
  s1_10 = vsubq_s16(s2_5, s2_10);
  s1_11 = vsubq_s16(s2_4, s2_11);
  s1_12 = vsubq_s16(s2_3, s2_12);
  s1_13 = vsubq_s16(s2_2, s2_13);
  s1_14 = vsubq_s16(s2_1, s2_14);
  s1_15 = vsubq_s16(s2_0, s2_15);

  s1_20 = sub_multiply_shift_and_narrow_s16(s3_27, s2_20, cospi_16_64);
  s1_27 = add_multiply_shift_and_narrow_s16(s2_20, s3_27, cospi_16_64);

  s1_21 = sub_multiply_shift_and_narrow_s16(s3_26, s2_21, cospi_16_64);
  s1_26 = add_multiply_shift_and_narrow_s16(s2_21, s3_26, cospi_16_64);

  s1_22 = sub_multiply_shift_and_narrow_s16(s3_25, s2_22, cospi_16_64);
  s1_25 = add_multiply_shift_and_narrow_s16(s2_22, s3_25, cospi_16_64);

  s1_23 = sub_multiply_shift_and_narrow_s16(s3_24, s2_23, cospi_16_64);
  s1_24 = add_multiply_shift_and_narrow_s16(s2_23, s3_24, cospi_16_64);

  // final stage
  vst1q_s16(output, vaddq_s16(s1_0, s2_31));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_1, s2_30));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_2, s2_29));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_3, s2_28));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_4, s1_27));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_5, s1_26));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_6, s1_25));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_7, s1_24));
  output += 8;

  vst1q_s16(output, vaddq_s16(s1_8, s1_23));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_9, s1_22));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_10, s1_21));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_11, s1_20));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_12, s2_19));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_13, s2_18));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_14, s2_17));
  output += 8;
  vst1q_s16(output, vaddq_s16(s1_15, s2_16));
  output += 8;

  vst1q_s16(output, vsubq_s16(s1_15, s2_16));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_14, s2_17));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_13, s2_18));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_12, s2_19));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_11, s1_20));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_10, s1_21));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_9, s1_22));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_8, s1_23));
  output += 8;

  vst1q_s16(output, vsubq_s16(s1_7, s1_24));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_6, s1_25));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_5, s1_26));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_4, s1_27));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_3, s2_28));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_2, s2_29));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_1, s2_30));
  output += 8;
  vst1q_s16(output, vsubq_s16(s1_0, s2_31));
}

static void idct32_8_neon(const int16_t *input, uint8_t *output, int stride) {
  int16x8_t in0, in1, in2, in3, in4, in5, in6, in7;
  int16x8_t out0, out1, out2, out3, out4, out5, out6, out7;
  int16x8_t s1_0, s1_1, s1_2, s1_3, s1_4, s1_5, s1_6, s1_7, s1_8, s1_9, s1_10,
      s1_11, s1_12, s1_13, s1_14, s1_15, s1_16, s1_17, s1_18, s1_19, s1_20,
      s1_21, s1_22, s1_23, s1_24, s1_25, s1_26, s1_27, s1_28, s1_29, s1_30,
      s1_31;
  int16x8_t s2_0, s2_1, s2_2, s2_3, s2_4, s2_5, s2_6, s2_7, s2_8, s2_9, s2_10,
      s2_11, s2_12, s2_13, s2_14, s2_15, s2_16, s2_17, s2_18, s2_19, s2_20,
      s2_21, s2_22, s2_23, s2_24, s2_25, s2_26, s2_27, s2_28, s2_29, s2_30,
      s2_31;
  int16x8_t s3_24, s3_25, s3_26, s3_27;

  load_and_transpose_s16_8x8(input, 8, &in0, &in1, &in2, &in3, &in4, &in5, &in6,
                             &in7);

  // stage 1
  s1_16 = multiply_shift_and_narrow_s16(in1, cospi_31_64);
  s1_31 = multiply_shift_and_narrow_s16(in1, cospi_1_64);

  // Different for _8_
  s1_19 = multiply_shift_and_narrow_s16(in7, -cospi_25_64);
  s1_28 = multiply_shift_and_narrow_s16(in7, cospi_7_64);

  s1_20 = multiply_shift_and_narrow_s16(in5, cospi_27_64);
  s1_27 = multiply_shift_and_narrow_s16(in5, cospi_5_64);

  s1_23 = multiply_shift_and_narrow_s16(in3, -cospi_29_64);
  s1_24 = multiply_shift_and_narrow_s16(in3, cospi_3_64);

  // stage 2
  s2_8 = multiply_shift_and_narrow_s16(in2, cospi_30_64);
  s2_15 = multiply_shift_and_narrow_s16(in2, cospi_2_64);

  s2_11 = multiply_shift_and_narrow_s16(in6, -cospi_26_64);
  s2_12 = multiply_shift_and_narrow_s16(in6, cospi_6_64);

  // stage 3
  s1_4 = multiply_shift_and_narrow_s16(in4, cospi_28_64);
  s1_7 = multiply_shift_and_narrow_s16(in4, cospi_4_64);

  s1_17 = multiply_accumulate_shift_and_narrow_s16(s1_16, -cospi_4_64, s1_31,
                                                   cospi_28_64);
  s1_30 = multiply_accumulate_shift_and_narrow_s16(s1_16, cospi_28_64, s1_31,
                                                   cospi_4_64);

  // Different for _8_
  s1_18 = multiply_accumulate_shift_and_narrow_s16(s1_19, -cospi_28_64, s1_28,
                                                   -cospi_4_64);
  s1_29 = multiply_accumulate_shift_and_narrow_s16(s1_19, -cospi_4_64, s1_28,
                                                   cospi_28_64);

  s1_21 = multiply_accumulate_shift_and_narrow_s16(s1_20, -cospi_20_64, s1_27,
                                                   cospi_12_64);
  s1_26 = multiply_accumulate_shift_and_narrow_s16(s1_20, cospi_12_64, s1_27,
                                                   cospi_20_64);

  s1_22 = multiply_accumulate_shift_and_narrow_s16(s1_23, -cospi_12_64, s1_24,
                                                   -cospi_20_64);
  s1_25 = multiply_accumulate_shift_and_narrow_s16(s1_23, -cospi_20_64, s1_24,
                                                   cospi_12_64);

  // stage 4
  s1_0 = multiply_shift_and_narrow_s16(in0, cospi_16_64);

  s2_9 = multiply_accumulate_shift_and_narrow_s16(s2_8, -cospi_8_64, s2_15,
                                                  cospi_24_64);
  s2_14 = multiply_accumulate_shift_and_narrow_s16(s2_8, cospi_24_64, s2_15,
                                                   cospi_8_64);

  s2_10 = multiply_accumulate_shift_and_narrow_s16(s2_11, -cospi_24_64, s2_12,
                                                   -cospi_8_64);
  s2_13 = multiply_accumulate_shift_and_narrow_s16(s2_11, -cospi_8_64, s2_12,
                                                   cospi_24_64);

  s2_16 = vaddq_s16(s1_16, s1_19);

  s2_17 = vaddq_s16(s1_17, s1_18);
  s2_18 = vsubq_s16(s1_17, s1_18);

  s2_19 = vsubq_s16(s1_16, s1_19);

  s2_20 = vsubq_s16(s1_23, s1_20);
  s2_21 = vsubq_s16(s1_22, s1_21);

  s2_22 = vaddq_s16(s1_21, s1_22);
  s2_23 = vaddq_s16(s1_20, s1_23);

  s2_24 = vaddq_s16(s1_24, s1_27);
  s2_25 = vaddq_s16(s1_25, s1_26);
  s2_26 = vsubq_s16(s1_25, s1_26);
  s2_27 = vsubq_s16(s1_24, s1_27);

  s2_28 = vsubq_s16(s1_31, s1_28);
  s2_29 = vsubq_s16(s1_30, s1_29);
  s2_30 = vaddq_s16(s1_29, s1_30);
  s2_31 = vaddq_s16(s1_28, s1_31);

  // stage 5
  s1_5 = sub_multiply_shift_and_narrow_s16(s1_7, s1_4, cospi_16_64);
  s1_6 = add_multiply_shift_and_narrow_s16(s1_4, s1_7, cospi_16_64);

  s1_8 = vaddq_s16(s2_8, s2_11);
  s1_9 = vaddq_s16(s2_9, s2_10);
  s1_10 = vsubq_s16(s2_9, s2_10);
  s1_11 = vsubq_s16(s2_8, s2_11);
  s1_12 = vsubq_s16(s2_15, s2_12);
  s1_13 = vsubq_s16(s2_14, s2_13);
  s1_14 = vaddq_s16(s2_13, s2_14);
  s1_15 = vaddq_s16(s2_12, s2_15);

  s1_18 = multiply_accumulate_shift_and_narrow_s16(s2_18, -cospi_8_64, s2_29,
                                                   cospi_24_64);
  s1_29 = multiply_accumulate_shift_and_narrow_s16(s2_18, cospi_24_64, s2_29,
                                                   cospi_8_64);

  s1_19 = multiply_accumulate_shift_and_narrow_s16(s2_19, -cospi_8_64, s2_28,
                                                   cospi_24_64);
  s1_28 = multiply_accumulate_shift_and_narrow_s16(s2_19, cospi_24_64, s2_28,
                                                   cospi_8_64);

  s1_20 = multiply_accumulate_shift_and_narrow_s16(s2_20, -cospi_24_64, s2_27,
                                                   -cospi_8_64);
  s1_27 = multiply_accumulate_shift_and_narrow_s16(s2_20, -cospi_8_64, s2_27,
                                                   cospi_24_64);

  s1_21 = multiply_accumulate_shift_and_narrow_s16(s2_21, -cospi_24_64, s2_26,
                                                   -cospi_8_64);
  s1_26 = multiply_accumulate_shift_and_narrow_s16(s2_21, -cospi_8_64, s2_26,
                                                   cospi_24_64);

  // stage 6
  s2_0 = vaddq_s16(s1_0, s1_7);
  s2_1 = vaddq_s16(s1_0, s1_6);
  s2_2 = vaddq_s16(s1_0, s1_5);
  s2_3 = vaddq_s16(s1_0, s1_4);
  s2_4 = vsubq_s16(s1_0, s1_4);
  s2_5 = vsubq_s16(s1_0, s1_5);
  s2_6 = vsubq_s16(s1_0, s1_6);
  s2_7 = vsubq_s16(s1_0, s1_7);

  s2_10 = sub_multiply_shift_and_narrow_s16(s1_13, s1_10, cospi_16_64);
  s2_13 = add_multiply_shift_and_narrow_s16(s1_10, s1_13, cospi_16_64);

  s2_11 = sub_multiply_shift_and_narrow_s16(s1_12, s1_11, cospi_16_64);
  s2_12 = add_multiply_shift_and_narrow_s16(s1_11, s1_12, cospi_16_64);

  s1_16 = vaddq_s16(s2_16, s2_23);
  s1_17 = vaddq_s16(s2_17, s2_22);
  s2_18 = vaddq_s16(s1_18, s1_21);
  s2_19 = vaddq_s16(s1_19, s1_20);
  s2_20 = vsubq_s16(s1_19, s1_20);
  s2_21 = vsubq_s16(s1_18, s1_21);
  s1_22 = vsubq_s16(s2_17, s2_22);
  s1_23 = vsubq_s16(s2_16, s2_23);

  s3_24 = vsubq_s16(s2_31, s2_24);
  s3_25 = vsubq_s16(s2_30, s2_25);
  s3_26 = vsubq_s16(s1_29, s1_26);
  s3_27 = vsubq_s16(s1_28, s1_27);
  s2_28 = vaddq_s16(s1_27, s1_28);
  s2_29 = vaddq_s16(s1_26, s1_29);
  s2_30 = vaddq_s16(s2_25, s2_30);
  s2_31 = vaddq_s16(s2_24, s2_31);

  // stage 7
  s1_0 = vaddq_s16(s2_0, s1_15);
  s1_1 = vaddq_s16(s2_1, s1_14);
  s1_2 = vaddq_s16(s2_2, s2_13);
  s1_3 = vaddq_s16(s2_3, s2_12);
  s1_4 = vaddq_s16(s2_4, s2_11);
  s1_5 = vaddq_s16(s2_5, s2_10);
  s1_6 = vaddq_s16(s2_6, s1_9);
  s1_7 = vaddq_s16(s2_7, s1_8);
  s1_8 = vsubq_s16(s2_7, s1_8);
  s1_9 = vsubq_s16(s2_6, s1_9);
  s1_10 = vsubq_s16(s2_5, s2_10);
  s1_11 = vsubq_s16(s2_4, s2_11);
  s1_12 = vsubq_s16(s2_3, s2_12);
  s1_13 = vsubq_s16(s2_2, s2_13);
  s1_14 = vsubq_s16(s2_1, s1_14);
  s1_15 = vsubq_s16(s2_0, s1_15);

  s1_20 = sub_multiply_shift_and_narrow_s16(s3_27, s2_20, cospi_16_64);
  s1_27 = add_multiply_shift_and_narrow_s16(s2_20, s3_27, cospi_16_64);

  s1_21 = sub_multiply_shift_and_narrow_s16(s3_26, s2_21, cospi_16_64);
  s1_26 = add_multiply_shift_and_narrow_s16(s2_21, s3_26, cospi_16_64);

  s2_22 = sub_multiply_shift_and_narrow_s16(s3_25, s1_22, cospi_16_64);
  s1_25 = add_multiply_shift_and_narrow_s16(s1_22, s3_25, cospi_16_64);

  s2_23 = sub_multiply_shift_and_narrow_s16(s3_24, s1_23, cospi_16_64);
  s1_24 = add_multiply_shift_and_narrow_s16(s1_23, s3_24, cospi_16_64);

  // final stage
  out0 = vaddq_s16(s1_0, s2_31);
  out1 = vaddq_s16(s1_1, s2_30);
  out2 = vaddq_s16(s1_2, s2_29);
  out3 = vaddq_s16(s1_3, s2_28);
  out4 = vaddq_s16(s1_4, s1_27);
  out5 = vaddq_s16(s1_5, s1_26);
  out6 = vaddq_s16(s1_6, s1_25);
  out7 = vaddq_s16(s1_7, s1_24);

  add_and_store_u8_s16(out0, out1, out2, out3, out4, out5, out6, out7, output,
                       stride);

  out0 = vaddq_s16(s1_8, s2_23);
  out1 = vaddq_s16(s1_9, s2_22);
  out2 = vaddq_s16(s1_10, s1_21);
  out3 = vaddq_s16(s1_11, s1_20);
  out4 = vaddq_s16(s1_12, s2_19);
  out5 = vaddq_s16(s1_13, s2_18);
  out6 = vaddq_s16(s1_14, s1_17);
  out7 = vaddq_s16(s1_15, s1_16);

  add_and_store_u8_s16(out0, out1, out2, out3, out4, out5, out6, out7,
                       output + (8 * stride), stride);

  out0 = vsubq_s16(s1_15, s1_16);
  out1 = vsubq_s16(s1_14, s1_17);
  out2 = vsubq_s16(s1_13, s2_18);
  out3 = vsubq_s16(s1_12, s2_19);
  out4 = vsubq_s16(s1_11, s1_20);
  out5 = vsubq_s16(s1_10, s1_21);
  out6 = vsubq_s16(s1_9, s2_22);
  out7 = vsubq_s16(s1_8, s2_23);

  add_and_store_u8_s16(out0, out1, out2, out3, out4, out5, out6, out7,
                       output + (16 * stride), stride);

  out0 = vsubq_s16(s1_7, s1_24);
  out1 = vsubq_s16(s1_6, s1_25);
  out2 = vsubq_s16(s1_5, s1_26);
  out3 = vsubq_s16(s1_4, s1_27);
  out4 = vsubq_s16(s1_3, s2_28);
  out5 = vsubq_s16(s1_2, s2_29);
  out6 = vsubq_s16(s1_1, s2_30);
  out7 = vsubq_s16(s1_0, s2_31);

  add_and_store_u8_s16(out0, out1, out2, out3, out4, out5, out6, out7,
                       output + (24 * stride), stride);
}

void vpx_idct32x32_34_add_neon(const tran_low_t *input, uint8_t *dest,
                               int stride) {
  int i;
  int16_t temp[32 * 8];
  int16_t *t = temp;

  idct32_6_neon(input, t);

  for (i = 0; i < 32; i += 8) {
    idct32_8_neon(t, dest, stride);
    t += (8 * 8);
    dest += 8;
  }
}
