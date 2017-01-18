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

static INLINE void load_8x8_s16(const tran_low_t *input, int16x8_t *const in0,
                                int16x8_t *const in1, int16x8_t *const in2,
                                int16x8_t *const in3, int16x8_t *const in4,
                                int16x8_t *const in5, int16x8_t *const in6,
                                int16x8_t *const in7) {
  *in0 = load_tran_low_to_s16q(input);
  input += 32;
  *in1 = load_tran_low_to_s16q(input);
  input += 32;
  *in2 = load_tran_low_to_s16q(input);
  input += 32;
  *in3 = load_tran_low_to_s16q(input);
  input += 32;
  *in4 = load_tran_low_to_s16q(input);
  input += 32;
  *in5 = load_tran_low_to_s16q(input);
  input += 32;
  *in6 = load_tran_low_to_s16q(input);
  input += 32;
  *in7 = load_tran_low_to_s16q(input);
}

static INLINE void load_4x8_s16(const tran_low_t *input, int16x4_t *const in0,
                                int16x4_t *const in1, int16x4_t *const in2,
                                int16x4_t *const in3, int16x4_t *const in4,
                                int16x4_t *const in5, int16x4_t *const in6,
                                int16x4_t *const in7) {
  *in0 = load_tran_low_to_s16d(input);
  input += 32;
  *in1 = load_tran_low_to_s16d(input);
  input += 32;
  *in2 = load_tran_low_to_s16d(input);
  input += 32;
  *in3 = load_tran_low_to_s16d(input);
  input += 32;
  *in4 = load_tran_low_to_s16d(input);
  input += 32;
  *in5 = load_tran_low_to_s16d(input);
  input += 32;
  *in6 = load_tran_low_to_s16d(input);
  input += 32;
  *in7 = load_tran_low_to_s16d(input);
}

// Only for the first pass of the  _135_ variant. Since it only uses values from
// the top left 16x16 it can safely assume all the remaining values are 0 and
// skip an awful lot of calculations. In fact, only the first 12 columns make
// the cut. None of the elements in the 13th, 14th, 15th or 16th columns are
// used so it skips any calls to input[12|13|14|15] too.
// In C this does a single row of 32 for each call. Here it transposes the top
// left 12x8 to allow using SIMD.

// vp9/common/vp9_scan.c:vp9_default_iscan_32x32 arranges the first 135 non-zero
// coefficients as follows:
//      0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
//  0   0   2   5  10  17  25  38  47  62  83 101 121
//  1   1   4   8  15  22  30  45  58  74  92 112 133
//  2   3   7  12  18  28  36  52  64  82 102 118
//  3   6  11  16  23  31  43  60  73  90 109 126
//  4   9  14  19  29  37  50  65  78  98 116 134
//  5  13  20  26  35  44  54  72  85 105 123
//  6  21  27  33  42  53  63  80  94 113 132
//  7  24  32  39  48  57  71  88 104 120
//  8  34  40  46  56  68  81  96 111 130
//  9  41  49  55  67  77  91 107 124
// 10  51  59  66  76  89  99 119 131
// 11  61  69  75  87 100 114 129
// 12  70  79  86  97 108 122
// 13  84  93 103 110 125
// 14  98 106 115 127
// 15 117 128
static void idct32_12_neon(const tran_low_t *input, int16_t *output) {
  int16x8_t in0, in1, in2, in3, in4, in5, in6, in7;
  int16x4_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int16x8_t in8, in9, in10, in11;
  int16x8_t s1_16, s1_18, s1_19, s1_20, s1_21, s1_23, s1_24, s1_26, s1_27,
      s1_28, s1_29, s1_31;
  int16x8_t s2_8, s2_10, s2_11, s2_12, s2_13, s2_15, s2_18, s2_19, s2_20, s2_21,
      s2_26, s2_27, s2_28, s2_29;
  int16x8_t s3_4, s3_7, s3_10, s3_11, s3_12, s3_13, s3_17, s3_18, s3_21, s3_22,
      s3_25, s3_26, s3_29, s3_30;
  int16x8_t s4_0, s4_2, s4_3, s4_9, s4_10, s4_13, s4_14, s4_16, s4_17, s4_18,
      s4_19, s4_20, s4_21, s4_22, s4_23, s4_24, s4_25, s4_26, s4_27, s4_28,
      s4_29, s4_30, s4_31;
  int16x8_t s5_0, s5_1, s5_2, s5_3, s5_5, s5_6, s5_8, s5_9, s5_10, s5_11, s5_12,
      s5_13, s5_14, s5_15, s5_18, s5_19, s5_20, s5_21, s5_26, s5_27, s5_28,
      s5_29;
  int16x8_t s6_0, s6_1, s6_2, s6_3, s6_4, s6_5, s6_6, s6_7, s6_10, s6_11, s6_12,
      s6_13, s6_16, s6_17, s6_18, s6_19, s6_20, s6_21, s6_22, s6_23, s6_24,
      s6_25, s6_26, s6_27, s6_28, s6_29, s6_30, s6_31;
  int16x8_t s7_0, s7_1, s7_2, s7_3, s7_4, s7_5, s7_6, s7_7, s7_8, s7_9, s7_10,
      s7_11, s7_12, s7_13, s7_14, s7_15, s7_20, s7_21, s7_22, s7_23, s7_24,
      s7_25, s7_26, s7_27;

  load_8x8_s16(input, &in0, &in1, &in2, &in3, &in4, &in5, &in6, &in7);
  transpose_s16_8x8(&in0, &in1, &in2, &in3, &in4, &in5, &in6, &in7);

  load_4x8_s16(input + 8, &tmp0, &tmp1, &tmp2, &tmp3, &tmp4, &tmp5, &tmp6,
               &tmp7);
  transpose_s16_4x8(tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, &in8, &in9,
                    &in10, &in11);

  // stage 1
  s1_16 = multiply_shift_and_narrow_s16(in1, cospi_31_64);
  s1_31 = multiply_shift_and_narrow_s16(in1, cospi_1_64);

  s1_18 = multiply_shift_and_narrow_s16(in9, cospi_23_64);
  s1_29 = multiply_shift_and_narrow_s16(in9, cospi_9_64);

  s1_19 = multiply_shift_and_narrow_s16(in7, -cospi_25_64);
  s1_28 = multiply_shift_and_narrow_s16(in7, cospi_7_64);

  s1_20 = multiply_shift_and_narrow_s16(in5, cospi_27_64);
  s1_27 = multiply_shift_and_narrow_s16(in5, cospi_5_64);

  s1_21 = multiply_shift_and_narrow_s16(in11, -cospi_21_64);
  s1_26 = multiply_shift_and_narrow_s16(in11, cospi_11_64);

  s1_23 = multiply_shift_and_narrow_s16(in3, -cospi_29_64);
  s1_24 = multiply_shift_and_narrow_s16(in3, cospi_3_64);

  // stage 2
  s2_8 = multiply_shift_and_narrow_s16(in2, cospi_30_64);
  s2_15 = multiply_shift_and_narrow_s16(in2, cospi_2_64);

  s2_10 = multiply_shift_and_narrow_s16(in10, cospi_22_64);
  s2_13 = multiply_shift_and_narrow_s16(in10, cospi_10_64);

  s2_11 = multiply_shift_and_narrow_s16(in6, -cospi_26_64);
  s2_12 = multiply_shift_and_narrow_s16(in6, cospi_6_64);

  s2_18 = vsubq_s16(s1_19, s1_18);
  s2_19 = vaddq_s16(s1_18, s1_19);
  s2_20 = vaddq_s16(s1_20, s1_21);
  s2_21 = vsubq_s16(s1_20, s1_21);
  s2_26 = vsubq_s16(s1_27, s1_26);
  s2_27 = vaddq_s16(s1_26, s1_27);
  s2_28 = vaddq_s16(s1_28, s1_29);
  s2_29 = vsubq_s16(s1_28, s1_29);

  // stage 3
  s3_4 = multiply_shift_and_narrow_s16(in4, cospi_28_64);
  s3_7 = multiply_shift_and_narrow_s16(in4, cospi_4_64);

  s3_10 = vsubq_s16(s2_11, s2_10);
  s3_11 = vaddq_s16(s2_10, s2_11);
  s3_12 = vaddq_s16(s2_12, s2_13);
  s3_13 = vsubq_s16(s2_12, s2_13);

  s3_17 = multiply_accumulate_shift_and_narrow_s16(s1_16, -cospi_4_64, s1_31,
                                                   cospi_28_64);
  s3_30 = multiply_accumulate_shift_and_narrow_s16(s1_16, cospi_28_64, s1_31,
                                                   cospi_4_64);

  s3_18 = multiply_accumulate_shift_and_narrow_s16(s2_18, -cospi_28_64, s2_29,
                                                   -cospi_4_64);
  s3_29 = multiply_accumulate_shift_and_narrow_s16(s2_18, -cospi_4_64, s2_29,
                                                   cospi_28_64);

  s3_21 = multiply_accumulate_shift_and_narrow_s16(s2_21, -cospi_20_64, s2_26,
                                                   cospi_12_64);
  s3_26 = multiply_accumulate_shift_and_narrow_s16(s2_21, cospi_12_64, s2_26,
                                                   cospi_20_64);

  s3_22 = multiply_accumulate_shift_and_narrow_s16(s1_23, -cospi_12_64, s1_24,
                                                   -cospi_20_64);
  s3_25 = multiply_accumulate_shift_and_narrow_s16(s1_23, -cospi_20_64, s1_24,
                                                   cospi_12_64);

  // stage 4
  s4_0 = multiply_shift_and_narrow_s16(in0, cospi_16_64);
  s4_2 = multiply_shift_and_narrow_s16(in8, cospi_24_64);
  s4_3 = multiply_shift_and_narrow_s16(in8, cospi_8_64);

  s4_9 = multiply_accumulate_shift_and_narrow_s16(s2_8, -cospi_8_64, s2_15,
                                                  cospi_24_64);
  s4_14 = multiply_accumulate_shift_and_narrow_s16(s2_8, cospi_24_64, s2_15,
                                                   cospi_8_64);

  s4_10 = multiply_accumulate_shift_and_narrow_s16(s3_10, -cospi_24_64, s3_13,
                                                   -cospi_8_64);
  s4_13 = multiply_accumulate_shift_and_narrow_s16(s3_10, -cospi_8_64, s3_13,
                                                   cospi_24_64);

  s4_16 = vaddq_s16(s1_16, s2_19);
  s4_17 = vaddq_s16(s3_17, s3_18);
  s4_18 = vsubq_s16(s3_17, s3_18);
  s4_19 = vsubq_s16(s1_16, s2_19);
  s4_20 = vsubq_s16(s1_23, s2_20);
  s4_21 = vsubq_s16(s3_22, s3_21);
  s4_22 = vaddq_s16(s3_21, s3_22);
  s4_23 = vaddq_s16(s2_20, s1_23);
  s4_24 = vaddq_s16(s1_24, s2_27);
  s4_25 = vaddq_s16(s3_25, s3_26);
  s4_26 = vsubq_s16(s3_25, s3_26);
  s4_27 = vsubq_s16(s1_24, s2_27);
  s4_28 = vsubq_s16(s1_31, s2_28);
  s4_29 = vsubq_s16(s3_30, s3_29);
  s4_30 = vaddq_s16(s3_29, s3_30);
  s4_31 = vaddq_s16(s2_28, s1_31);

  // stage 5
  s5_0 = vaddq_s16(s4_0, s4_3);
  s5_1 = vaddq_s16(s4_0, s4_2);
  s5_2 = vsubq_s16(s4_0, s4_2);
  s5_3 = vsubq_s16(s4_0, s4_3);

  s5_5 = sub_multiply_shift_and_narrow_s16(s3_7, s3_4, cospi_16_64);
  s5_6 = add_multiply_shift_and_narrow_s16(s3_4, s3_7, cospi_16_64);

  s5_8 = vaddq_s16(s2_8, s3_11);
  s5_9 = vaddq_s16(s4_9, s4_10);
  s5_10 = vsubq_s16(s4_9, s4_10);
  s5_11 = vsubq_s16(s2_8, s3_11);
  s5_12 = vsubq_s16(s2_15, s3_12);
  s5_13 = vsubq_s16(s4_14, s4_13);
  s5_14 = vaddq_s16(s4_13, s4_14);
  s5_15 = vaddq_s16(s2_15, s3_12);

  s5_18 = multiply_accumulate_shift_and_narrow_s16(s4_18, -cospi_8_64, s4_29,
                                                   cospi_24_64);
  s5_29 = multiply_accumulate_shift_and_narrow_s16(s4_18, cospi_24_64, s4_29,
                                                   cospi_8_64);

  s5_19 = multiply_accumulate_shift_and_narrow_s16(s4_19, -cospi_8_64, s4_28,
                                                   cospi_24_64);
  s5_28 = multiply_accumulate_shift_and_narrow_s16(s4_19, cospi_24_64, s4_28,
                                                   cospi_8_64);

  s5_20 = multiply_accumulate_shift_and_narrow_s16(s4_20, -cospi_24_64, s4_27,
                                                   -cospi_8_64);
  s5_27 = multiply_accumulate_shift_and_narrow_s16(s4_20, -cospi_8_64, s4_27,
                                                   cospi_24_64);

  s5_21 = multiply_accumulate_shift_and_narrow_s16(s4_21, -cospi_24_64, s4_26,
                                                   -cospi_8_64);
  s5_26 = multiply_accumulate_shift_and_narrow_s16(s4_21, -cospi_8_64, s4_26,
                                                   cospi_24_64);

  // stage 6
  s6_0 = vaddq_s16(s5_0, s3_7);
  s6_1 = vaddq_s16(s5_1, s5_6);
  s6_2 = vaddq_s16(s5_2, s5_5);
  s6_3 = vaddq_s16(s5_3, s3_4);
  s6_4 = vsubq_s16(s5_3, s3_4);
  s6_5 = vsubq_s16(s5_2, s5_5);
  s6_6 = vsubq_s16(s5_1, s5_6);
  s6_7 = vsubq_s16(s5_0, s3_7);

  s6_10 = sub_multiply_shift_and_narrow_s16(s5_13, s5_10, cospi_16_64);
  s6_13 = add_multiply_shift_and_narrow_s16(s5_10, s5_13, cospi_16_64);

  s6_11 = sub_multiply_shift_and_narrow_s16(s5_12, s5_11, cospi_16_64);
  s6_12 = add_multiply_shift_and_narrow_s16(s5_11, s5_12, cospi_16_64);

  s6_16 = vaddq_s16(s4_16, s4_23);
  s6_17 = vaddq_s16(s4_17, s4_22);
  s6_18 = vaddq_s16(s5_18, s5_21);
  s6_19 = vaddq_s16(s5_19, s5_20);
  s6_20 = vsubq_s16(s5_19, s5_20);
  s6_21 = vsubq_s16(s5_18, s5_21);
  s6_22 = vsubq_s16(s4_17, s4_22);
  s6_23 = vsubq_s16(s4_16, s4_23);

  s6_24 = vsubq_s16(s4_31, s4_24);
  s6_25 = vsubq_s16(s4_30, s4_25);
  s6_26 = vsubq_s16(s5_29, s5_26);
  s6_27 = vsubq_s16(s5_28, s5_27);
  s6_28 = vaddq_s16(s5_27, s5_28);
  s6_29 = vaddq_s16(s5_26, s5_29);
  s6_30 = vaddq_s16(s4_25, s4_30);
  s6_31 = vaddq_s16(s4_24, s4_31);

  // stage 7
  s7_0 = vaddq_s16(s6_0, s5_15);
  s7_1 = vaddq_s16(s6_1, s5_14);
  s7_2 = vaddq_s16(s6_2, s6_13);
  s7_3 = vaddq_s16(s6_3, s6_12);
  s7_4 = vaddq_s16(s6_4, s6_11);
  s7_5 = vaddq_s16(s6_5, s6_10);
  s7_6 = vaddq_s16(s6_6, s5_9);
  s7_7 = vaddq_s16(s6_7, s5_8);
  s7_8 = vsubq_s16(s6_7, s5_8);
  s7_9 = vsubq_s16(s6_6, s5_9);
  s7_10 = vsubq_s16(s6_5, s6_10);
  s7_11 = vsubq_s16(s6_4, s6_11);
  s7_12 = vsubq_s16(s6_3, s6_12);
  s7_13 = vsubq_s16(s6_2, s6_13);
  s7_14 = vsubq_s16(s6_1, s5_14);
  s7_15 = vsubq_s16(s6_0, s5_15);

  s7_20 = sub_multiply_shift_and_narrow_s16(s6_27, s6_20, cospi_16_64);
  s7_27 = add_multiply_shift_and_narrow_s16(s6_20, s6_27, cospi_16_64);

  s7_21 = sub_multiply_shift_and_narrow_s16(s6_26, s6_21, cospi_16_64);
  s7_26 = add_multiply_shift_and_narrow_s16(s6_21, s6_26, cospi_16_64);

  s7_22 = sub_multiply_shift_and_narrow_s16(s6_25, s6_22, cospi_16_64);
  s7_25 = add_multiply_shift_and_narrow_s16(s6_22, s6_25, cospi_16_64);

  s7_23 = sub_multiply_shift_and_narrow_s16(s6_24, s6_23, cospi_16_64);
  s7_24 = add_multiply_shift_and_narrow_s16(s6_23, s6_24, cospi_16_64);

  // final stage
  vst1q_s16(output, vaddq_s16(s7_0, s6_31));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_1, s6_30));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_2, s6_29));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_3, s6_28));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_4, s7_27));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_5, s7_26));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_6, s7_25));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_7, s7_24));
  output += 16;

  vst1q_s16(output, vaddq_s16(s7_8, s7_23));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_9, s7_22));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_10, s7_21));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_11, s7_20));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_12, s6_19));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_13, s6_18));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_14, s6_17));
  output += 16;
  vst1q_s16(output, vaddq_s16(s7_15, s6_16));
  output += 16;

  vst1q_s16(output, vsubq_s16(s7_15, s6_16));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_14, s6_17));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_13, s6_18));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_12, s6_19));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_11, s7_20));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_10, s7_21));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_9, s7_22));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_8, s7_23));
  output += 16;

  vst1q_s16(output, vsubq_s16(s7_7, s7_24));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_6, s7_25));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_5, s7_26));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_4, s7_27));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_3, s6_28));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_2, s6_29));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_1, s6_30));
  output += 16;
  vst1q_s16(output, vsubq_s16(s7_0, s6_31));
}

static void idct32_16_neon(const int16_t *input, uint8_t *output, int stride) {
  int16x8_t in0, in1, in2, in3, in4, in5, in6, in7, in8, in9, in10, in11, in12,
      in13, in14, in15;
  int16x8_t s1_16, s1_17, s1_18, s1_19, s1_20, s1_21, s1_22, s1_23, s1_24,
      s1_25, s1_26, s1_27, s1_28, s1_29, s1_30, s1_31;
  int16x8_t s2_8, s2_9, s2_10, s2_11, s2_12, s2_13, s2_14, s2_15, s2_16, s2_17,
      s2_18, s2_19, s2_20, s2_21, s2_22, s2_23, s2_24, s2_25, s2_26, s2_27,
      s2_28, s2_29, s2_30, s2_31;
  int16x8_t s3_4, s3_5, s3_6, s3_7, s3_8, s3_9, s3_10, s3_11, s3_12, s3_13,
      s3_14, s3_15, s3_17, s3_18, s3_21, s3_22, s3_25, s3_26, s3_29, s3_30;
  int16x8_t s4_0, s4_2, s4_3, s4_4, s4_5, s4_6, s4_7, s4_9, s4_10, s4_13, s4_14,
      s4_16, s4_17, s4_18, s4_19, s4_20, s4_21, s4_22, s4_23, s4_24, s4_25,
      s4_26, s4_27, s4_28, s4_29, s4_30, s4_31;
  int16x8_t s5_0, s5_1, s5_2, s5_3, s5_5, s5_6, s5_8, s5_9, s5_10, s5_11, s5_12,
      s5_13, s5_14, s5_15, s5_18, s5_19, s5_20, s5_21, s5_26, s5_27, s5_28,
      s5_29;
  int16x8_t s6_0, s6_1, s6_2, s6_3, s6_4, s6_5, s6_6, s6_7, s6_10, s6_11, s6_12,
      s6_13, s6_16, s6_17, s6_18, s6_19, s6_20, s6_21, s6_22, s6_23, s6_24,
      s6_25, s6_26, s6_27, s6_28, s6_29, s6_30, s6_31;
  int16x8_t s7_0, s7_1, s7_2, s7_3, s7_4, s7_5, s7_6, s7_7, s7_8, s7_9, s7_10,
      s7_11, s7_12, s7_13, s7_14, s7_15, s7_20, s7_21, s7_22, s7_23, s7_24,
      s7_25, s7_26, s7_27;
  int16x8_t out0, out1, out2, out3, out4, out5, out6, out7;

  load_and_transpose_s16_8x8(input, 16, &in0, &in1, &in2, &in3, &in4, &in5,
                             &in6, &in7);

  load_and_transpose_s16_8x8(input + 8, 16, &in8, &in9, &in10, &in11, &in12,
                             &in13, &in14, &in15);

  // stage 1
  s1_16 = multiply_shift_and_narrow_s16(in1, cospi_31_64);
  s1_31 = multiply_shift_and_narrow_s16(in1, cospi_1_64);

  s1_17 = multiply_shift_and_narrow_s16(in15, -cospi_17_64);
  s1_30 = multiply_shift_and_narrow_s16(in15, cospi_15_64);

  s1_18 = multiply_shift_and_narrow_s16(in9, cospi_23_64);
  s1_29 = multiply_shift_and_narrow_s16(in9, cospi_9_64);

  s1_19 = multiply_shift_and_narrow_s16(in7, -cospi_25_64);
  s1_28 = multiply_shift_and_narrow_s16(in7, cospi_7_64);

  s1_20 = multiply_shift_and_narrow_s16(in5, cospi_27_64);
  s1_27 = multiply_shift_and_narrow_s16(in5, cospi_5_64);

  s1_21 = multiply_shift_and_narrow_s16(in11, -cospi_21_64);
  s1_26 = multiply_shift_and_narrow_s16(in11, cospi_11_64);

  s1_22 = multiply_shift_and_narrow_s16(in13, cospi_19_64);
  s1_25 = multiply_shift_and_narrow_s16(in13, cospi_13_64);

  s1_23 = multiply_shift_and_narrow_s16(in3, -cospi_29_64);
  s1_24 = multiply_shift_and_narrow_s16(in3, cospi_3_64);

  // stage 2
  s2_8 = multiply_shift_and_narrow_s16(in2, cospi_30_64);
  s2_15 = multiply_shift_and_narrow_s16(in2, cospi_2_64);

  s2_9 = multiply_shift_and_narrow_s16(in14, -cospi_18_64);
  s2_14 = multiply_shift_and_narrow_s16(in14, cospi_14_64);

  s2_10 = multiply_shift_and_narrow_s16(in10, cospi_22_64);
  s2_13 = multiply_shift_and_narrow_s16(in10, cospi_10_64);

  s2_11 = multiply_shift_and_narrow_s16(in6, -cospi_26_64);
  s2_12 = multiply_shift_and_narrow_s16(in6, cospi_6_64);

  s2_16 = vaddq_s16(s1_16, s1_17);
  s2_17 = vsubq_s16(s1_16, s1_17);
  s2_18 = vsubq_s16(s1_19, s1_18);
  s2_19 = vaddq_s16(s1_18, s1_19);
  s2_20 = vaddq_s16(s1_20, s1_21);
  s2_21 = vsubq_s16(s1_20, s1_21);
  s2_22 = vsubq_s16(s1_23, s1_22);
  s2_23 = vaddq_s16(s1_22, s1_23);
  s2_24 = vaddq_s16(s1_24, s1_25);
  s2_25 = vsubq_s16(s1_24, s1_25);
  s2_26 = vsubq_s16(s1_27, s1_26);
  s2_27 = vaddq_s16(s1_26, s1_27);
  s2_28 = vaddq_s16(s1_28, s1_29);
  s2_29 = vsubq_s16(s1_28, s1_29);
  s2_30 = vsubq_s16(s1_31, s1_30);
  s2_31 = vaddq_s16(s1_30, s1_31);

  // stage 3
  s3_4 = multiply_shift_and_narrow_s16(in4, cospi_28_64);
  s3_7 = multiply_shift_and_narrow_s16(in4, cospi_4_64);

  s3_5 = multiply_shift_and_narrow_s16(in12, -cospi_20_64);
  s3_6 = multiply_shift_and_narrow_s16(in12, cospi_12_64);

  s3_8 = vaddq_s16(s2_8, s2_9);
  s3_9 = vsubq_s16(s2_8, s2_9);
  s3_10 = vsubq_s16(s2_11, s2_10);
  s3_11 = vaddq_s16(s2_10, s2_11);
  s3_12 = vaddq_s16(s2_12, s2_13);
  s3_13 = vsubq_s16(s2_12, s2_13);
  s3_14 = vsubq_s16(s2_15, s2_14);
  s3_15 = vaddq_s16(s2_14, s2_15);

  s3_17 = multiply_accumulate_shift_and_narrow_s16(s2_17, -cospi_4_64, s2_30,
                                                   cospi_28_64);
  s3_30 = multiply_accumulate_shift_and_narrow_s16(s2_17, cospi_28_64, s2_30,
                                                   cospi_4_64);

  s3_18 = multiply_accumulate_shift_and_narrow_s16(s2_18, -cospi_28_64, s2_29,
                                                   -cospi_4_64);
  s3_29 = multiply_accumulate_shift_and_narrow_s16(s2_18, -cospi_4_64, s2_29,
                                                   cospi_28_64);

  s3_21 = multiply_accumulate_shift_and_narrow_s16(s2_21, -cospi_20_64, s2_26,
                                                   cospi_12_64);
  s3_26 = multiply_accumulate_shift_and_narrow_s16(s2_21, cospi_12_64, s2_26,
                                                   cospi_20_64);

  s3_22 = multiply_accumulate_shift_and_narrow_s16(s2_22, -cospi_12_64, s2_25,
                                                   -cospi_20_64);
  s3_25 = multiply_accumulate_shift_and_narrow_s16(s2_22, -cospi_20_64, s2_25,
                                                   cospi_12_64);

  // stage 4
  s4_0 = multiply_shift_and_narrow_s16(in0, cospi_16_64);
  s4_2 = multiply_shift_and_narrow_s16(in8, cospi_24_64);
  s4_3 = multiply_shift_and_narrow_s16(in8, cospi_8_64);

  s4_4 = vaddq_s16(s3_4, s3_5);
  s4_5 = vsubq_s16(s3_4, s3_5);
  s4_6 = vsubq_s16(s3_7, s3_6);
  s4_7 = vaddq_s16(s3_6, s3_7);

  s4_9 = multiply_accumulate_shift_and_narrow_s16(s3_9, -cospi_8_64, s3_14,
                                                  cospi_24_64);
  s4_14 = multiply_accumulate_shift_and_narrow_s16(s3_9, cospi_24_64, s3_14,
                                                   cospi_8_64);

  s4_10 = multiply_accumulate_shift_and_narrow_s16(s3_10, -cospi_24_64, s3_13,
                                                   -cospi_8_64);
  s4_13 = multiply_accumulate_shift_and_narrow_s16(s3_10, -cospi_8_64, s3_13,
                                                   cospi_24_64);

  s4_16 = vaddq_s16(s2_16, s2_19);
  s4_17 = vaddq_s16(s3_17, s3_18);
  s4_18 = vsubq_s16(s3_17, s3_18);
  s4_19 = vsubq_s16(s2_16, s2_19);
  s4_20 = vsubq_s16(s2_23, s2_20);
  s4_21 = vsubq_s16(s3_22, s3_21);
  s4_22 = vaddq_s16(s3_21, s3_22);
  s4_23 = vaddq_s16(s2_20, s2_23);
  s4_24 = vaddq_s16(s2_24, s2_27);
  s4_25 = vaddq_s16(s3_25, s3_26);
  s4_26 = vsubq_s16(s3_25, s3_26);
  s4_27 = vsubq_s16(s2_24, s2_27);
  s4_28 = vsubq_s16(s2_31, s2_28);
  s4_29 = vsubq_s16(s3_30, s3_29);
  s4_30 = vaddq_s16(s3_29, s3_30);
  s4_31 = vaddq_s16(s2_28, s2_31);

  // stage 5
  s5_0 = vaddq_s16(s4_0, s4_3);
  s5_1 = vaddq_s16(s4_0, s4_2);
  s5_2 = vsubq_s16(s4_0, s4_2);
  s5_3 = vsubq_s16(s4_0, s4_3);

  s5_5 = sub_multiply_shift_and_narrow_s16(s4_6, s4_5, cospi_16_64);
  s5_6 = add_multiply_shift_and_narrow_s16(s4_5, s4_6, cospi_16_64);

  s5_8 = vaddq_s16(s3_8, s3_11);
  s5_9 = vaddq_s16(s4_9, s4_10);
  s5_10 = vsubq_s16(s4_9, s4_10);
  s5_11 = vsubq_s16(s3_8, s3_11);
  s5_12 = vsubq_s16(s3_15, s3_12);
  s5_13 = vsubq_s16(s4_14, s4_13);
  s5_14 = vaddq_s16(s4_13, s4_14);
  s5_15 = vaddq_s16(s3_15, s3_12);

  s5_18 = multiply_accumulate_shift_and_narrow_s16(s4_18, -cospi_8_64, s4_29,
                                                   cospi_24_64);
  s5_29 = multiply_accumulate_shift_and_narrow_s16(s4_18, cospi_24_64, s4_29,
                                                   cospi_8_64);

  s5_19 = multiply_accumulate_shift_and_narrow_s16(s4_19, -cospi_8_64, s4_28,
                                                   cospi_24_64);
  s5_28 = multiply_accumulate_shift_and_narrow_s16(s4_19, cospi_24_64, s4_28,
                                                   cospi_8_64);

  s5_20 = multiply_accumulate_shift_and_narrow_s16(s4_20, -cospi_24_64, s4_27,
                                                   -cospi_8_64);
  s5_27 = multiply_accumulate_shift_and_narrow_s16(s4_20, -cospi_8_64, s4_27,
                                                   cospi_24_64);

  s5_21 = multiply_accumulate_shift_and_narrow_s16(s4_21, -cospi_24_64, s4_26,
                                                   -cospi_8_64);
  s5_26 = multiply_accumulate_shift_and_narrow_s16(s4_21, -cospi_8_64, s4_26,
                                                   cospi_24_64);

  // stage 6
  s6_0 = vaddq_s16(s5_0, s4_7);
  s6_1 = vaddq_s16(s5_1, s5_6);
  s6_2 = vaddq_s16(s5_2, s5_5);
  s6_3 = vaddq_s16(s5_3, s4_4);
  s6_4 = vsubq_s16(s5_3, s4_4);
  s6_5 = vsubq_s16(s5_2, s5_5);
  s6_6 = vsubq_s16(s5_1, s5_6);
  s6_7 = vsubq_s16(s5_0, s4_7);

  s6_10 = sub_multiply_shift_and_narrow_s16(s5_13, s5_10, cospi_16_64);
  s6_13 = add_multiply_shift_and_narrow_s16(s5_10, s5_13, cospi_16_64);

  s6_11 = sub_multiply_shift_and_narrow_s16(s5_12, s5_11, cospi_16_64);
  s6_12 = add_multiply_shift_and_narrow_s16(s5_11, s5_12, cospi_16_64);

  s6_16 = vaddq_s16(s4_16, s4_23);
  s6_17 = vaddq_s16(s4_17, s4_22);
  s6_18 = vaddq_s16(s5_18, s5_21);
  s6_19 = vaddq_s16(s5_19, s5_20);
  s6_20 = vsubq_s16(s5_19, s5_20);
  s6_21 = vsubq_s16(s5_18, s5_21);
  s6_22 = vsubq_s16(s4_17, s4_22);
  s6_23 = vsubq_s16(s4_16, s4_23);
  s6_24 = vsubq_s16(s4_31, s4_24);
  s6_25 = vsubq_s16(s4_30, s4_25);
  s6_26 = vsubq_s16(s5_29, s5_26);
  s6_27 = vsubq_s16(s5_28, s5_27);
  s6_28 = vaddq_s16(s5_27, s5_28);
  s6_29 = vaddq_s16(s5_26, s5_29);
  s6_30 = vaddq_s16(s4_25, s4_30);
  s6_31 = vaddq_s16(s4_24, s4_31);

  // stage 7
  s7_0 = vaddq_s16(s6_0, s5_15);
  s7_1 = vaddq_s16(s6_1, s5_14);
  s7_2 = vaddq_s16(s6_2, s6_13);
  s7_3 = vaddq_s16(s6_3, s6_12);
  s7_4 = vaddq_s16(s6_4, s6_11);
  s7_5 = vaddq_s16(s6_5, s6_10);
  s7_6 = vaddq_s16(s6_6, s5_9);
  s7_7 = vaddq_s16(s6_7, s5_8);
  s7_8 = vsubq_s16(s6_7, s5_8);
  s7_9 = vsubq_s16(s6_6, s5_9);
  s7_10 = vsubq_s16(s6_5, s6_10);
  s7_11 = vsubq_s16(s6_4, s6_11);
  s7_12 = vsubq_s16(s6_3, s6_12);
  s7_13 = vsubq_s16(s6_2, s6_13);
  s7_14 = vsubq_s16(s6_1, s5_14);
  s7_15 = vsubq_s16(s6_0, s5_15);

  s7_20 = sub_multiply_shift_and_narrow_s16(s6_27, s6_20, cospi_16_64);
  s7_27 = add_multiply_shift_and_narrow_s16(s6_20, s6_27, cospi_16_64);

  s7_21 = sub_multiply_shift_and_narrow_s16(s6_26, s6_21, cospi_16_64);
  s7_26 = add_multiply_shift_and_narrow_s16(s6_21, s6_26, cospi_16_64);

  s7_22 = sub_multiply_shift_and_narrow_s16(s6_25, s6_22, cospi_16_64);
  s7_25 = add_multiply_shift_and_narrow_s16(s6_22, s6_25, cospi_16_64);

  s7_23 = sub_multiply_shift_and_narrow_s16(s6_24, s6_23, cospi_16_64);
  s7_24 = add_multiply_shift_and_narrow_s16(s6_23, s6_24, cospi_16_64);

  // final stage
  out0 = vaddq_s16(s7_0, s6_31);
  out1 = vaddq_s16(s7_1, s6_30);
  out2 = vaddq_s16(s7_2, s6_29);
  out3 = vaddq_s16(s7_3, s6_28);
  out4 = vaddq_s16(s7_4, s7_27);
  out5 = vaddq_s16(s7_5, s7_26);
  out6 = vaddq_s16(s7_6, s7_25);
  out7 = vaddq_s16(s7_7, s7_24);

  add_and_store_u8_s16(out0, out1, out2, out3, out4, out5, out6, out7, output,
                       stride);

  out0 = vaddq_s16(s7_8, s7_23);
  out1 = vaddq_s16(s7_9, s7_22);
  out2 = vaddq_s16(s7_10, s7_21);
  out3 = vaddq_s16(s7_11, s7_20);
  out4 = vaddq_s16(s7_12, s6_19);
  out5 = vaddq_s16(s7_13, s6_18);
  out6 = vaddq_s16(s7_14, s6_17);
  out7 = vaddq_s16(s7_15, s6_16);

  add_and_store_u8_s16(out0, out1, out2, out3, out4, out5, out6, out7,
                       output + (8 * stride), stride);

  out0 = vsubq_s16(s7_15, s6_16);
  out1 = vsubq_s16(s7_14, s6_17);
  out2 = vsubq_s16(s7_13, s6_18);
  out3 = vsubq_s16(s7_12, s6_19);
  out4 = vsubq_s16(s7_11, s7_20);
  out5 = vsubq_s16(s7_10, s7_21);
  out6 = vsubq_s16(s7_9, s7_22);
  out7 = vsubq_s16(s7_8, s7_23);

  add_and_store_u8_s16(out0, out1, out2, out3, out4, out5, out6, out7,
                       output + (16 * stride), stride);

  out0 = vsubq_s16(s7_7, s7_24);
  out1 = vsubq_s16(s7_6, s7_25);
  out2 = vsubq_s16(s7_5, s7_26);
  out3 = vsubq_s16(s7_4, s7_27);
  out4 = vsubq_s16(s7_3, s6_28);
  out5 = vsubq_s16(s7_2, s6_29);
  out6 = vsubq_s16(s7_1, s6_30);
  out7 = vsubq_s16(s7_0, s6_31);

  add_and_store_u8_s16(out0, out1, out2, out3, out4, out5, out6, out7,
                       output + (24 * stride), stride);
}

void vpx_idct32x32_135_add_neon(const tran_low_t *input, uint8_t *dest,
                                int stride) {
  int i;
  int16_t temp[32 * 16];
  int16_t *t = temp;

  idct32_12_neon(input, temp);
  idct32_12_neon(input + 32 * 8, temp + 8);

  for (i = 0; i < 32; i += 8) {
    idct32_16_neon(t, dest, stride);
    t += (16 * 8);
    dest += 8;
  }
}
