/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>  // avx2

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"

#include "aom_dsp/x86/fwd_txfm_avx2.h"
#include "aom_dsp/txfm_common.h"
#include "aom_dsp/x86/txfm_common_avx2.h"

static INLINE void load_buffer_16x16(const int16_t *input, int stride,
                                     int flipud, int fliplr, __m256i *in) {
  if (!flipud) {
    in[0] = _mm256_loadu_si256((const __m256i *)(input + 0 * stride));
    in[1] = _mm256_loadu_si256((const __m256i *)(input + 1 * stride));
    in[2] = _mm256_loadu_si256((const __m256i *)(input + 2 * stride));
    in[3] = _mm256_loadu_si256((const __m256i *)(input + 3 * stride));
    in[4] = _mm256_loadu_si256((const __m256i *)(input + 4 * stride));
    in[5] = _mm256_loadu_si256((const __m256i *)(input + 5 * stride));
    in[6] = _mm256_loadu_si256((const __m256i *)(input + 6 * stride));
    in[7] = _mm256_loadu_si256((const __m256i *)(input + 7 * stride));
    in[8] = _mm256_loadu_si256((const __m256i *)(input + 8 * stride));
    in[9] = _mm256_loadu_si256((const __m256i *)(input + 9 * stride));
    in[10] = _mm256_loadu_si256((const __m256i *)(input + 10 * stride));
    in[11] = _mm256_loadu_si256((const __m256i *)(input + 11 * stride));
    in[12] = _mm256_loadu_si256((const __m256i *)(input + 12 * stride));
    in[13] = _mm256_loadu_si256((const __m256i *)(input + 13 * stride));
    in[14] = _mm256_loadu_si256((const __m256i *)(input + 14 * stride));
    in[15] = _mm256_loadu_si256((const __m256i *)(input + 15 * stride));
  } else {
    in[0] = _mm256_loadu_si256((const __m256i *)(input + 15 * stride));
    in[1] = _mm256_loadu_si256((const __m256i *)(input + 14 * stride));
    in[2] = _mm256_loadu_si256((const __m256i *)(input + 13 * stride));
    in[3] = _mm256_loadu_si256((const __m256i *)(input + 12 * stride));
    in[4] = _mm256_loadu_si256((const __m256i *)(input + 11 * stride));
    in[5] = _mm256_loadu_si256((const __m256i *)(input + 10 * stride));
    in[6] = _mm256_loadu_si256((const __m256i *)(input + 9 * stride));
    in[7] = _mm256_loadu_si256((const __m256i *)(input + 8 * stride));
    in[8] = _mm256_loadu_si256((const __m256i *)(input + 7 * stride));
    in[9] = _mm256_loadu_si256((const __m256i *)(input + 6 * stride));
    in[10] = _mm256_loadu_si256((const __m256i *)(input + 5 * stride));
    in[11] = _mm256_loadu_si256((const __m256i *)(input + 4 * stride));
    in[12] = _mm256_loadu_si256((const __m256i *)(input + 3 * stride));
    in[13] = _mm256_loadu_si256((const __m256i *)(input + 2 * stride));
    in[14] = _mm256_loadu_si256((const __m256i *)(input + 1 * stride));
    in[15] = _mm256_loadu_si256((const __m256i *)(input + 0 * stride));
  }

  if (fliplr) {
    mm256_reverse_epi16(&in[0]);
    mm256_reverse_epi16(&in[1]);
    mm256_reverse_epi16(&in[2]);
    mm256_reverse_epi16(&in[3]);
    mm256_reverse_epi16(&in[4]);
    mm256_reverse_epi16(&in[5]);
    mm256_reverse_epi16(&in[6]);
    mm256_reverse_epi16(&in[7]);
    mm256_reverse_epi16(&in[8]);
    mm256_reverse_epi16(&in[9]);
    mm256_reverse_epi16(&in[10]);
    mm256_reverse_epi16(&in[11]);
    mm256_reverse_epi16(&in[12]);
    mm256_reverse_epi16(&in[13]);
    mm256_reverse_epi16(&in[14]);
    mm256_reverse_epi16(&in[15]);
  }

  in[0] = _mm256_slli_epi16(in[0], 2);
  in[1] = _mm256_slli_epi16(in[1], 2);
  in[2] = _mm256_slli_epi16(in[2], 2);
  in[3] = _mm256_slli_epi16(in[3], 2);
  in[4] = _mm256_slli_epi16(in[4], 2);
  in[5] = _mm256_slli_epi16(in[5], 2);
  in[6] = _mm256_slli_epi16(in[6], 2);
  in[7] = _mm256_slli_epi16(in[7], 2);
  in[8] = _mm256_slli_epi16(in[8], 2);
  in[9] = _mm256_slli_epi16(in[9], 2);
  in[10] = _mm256_slli_epi16(in[10], 2);
  in[11] = _mm256_slli_epi16(in[11], 2);
  in[12] = _mm256_slli_epi16(in[12], 2);
  in[13] = _mm256_slli_epi16(in[13], 2);
  in[14] = _mm256_slli_epi16(in[14], 2);
  in[15] = _mm256_slli_epi16(in[15], 2);
}

static INLINE void write_buffer_16x16(const __m256i *in, tran_low_t *output) {
  int i;
  for (i = 0; i < 16; ++i) {
    storeu_output_avx2(&in[i], output + (i << 4));
  }
}

static void right_shift_16x16(__m256i *in) {
  const __m256i one = _mm256_set1_epi16(1);
  __m256i s0 = _mm256_srai_epi16(in[0], 15);
  __m256i s1 = _mm256_srai_epi16(in[1], 15);
  __m256i s2 = _mm256_srai_epi16(in[2], 15);
  __m256i s3 = _mm256_srai_epi16(in[3], 15);
  __m256i s4 = _mm256_srai_epi16(in[4], 15);
  __m256i s5 = _mm256_srai_epi16(in[5], 15);
  __m256i s6 = _mm256_srai_epi16(in[6], 15);
  __m256i s7 = _mm256_srai_epi16(in[7], 15);
  __m256i s8 = _mm256_srai_epi16(in[8], 15);
  __m256i s9 = _mm256_srai_epi16(in[9], 15);
  __m256i s10 = _mm256_srai_epi16(in[10], 15);
  __m256i s11 = _mm256_srai_epi16(in[11], 15);
  __m256i s12 = _mm256_srai_epi16(in[12], 15);
  __m256i s13 = _mm256_srai_epi16(in[13], 15);
  __m256i s14 = _mm256_srai_epi16(in[14], 15);
  __m256i s15 = _mm256_srai_epi16(in[15], 15);

  in[0] = _mm256_add_epi16(in[0], one);
  in[1] = _mm256_add_epi16(in[1], one);
  in[2] = _mm256_add_epi16(in[2], one);
  in[3] = _mm256_add_epi16(in[3], one);
  in[4] = _mm256_add_epi16(in[4], one);
  in[5] = _mm256_add_epi16(in[5], one);
  in[6] = _mm256_add_epi16(in[6], one);
  in[7] = _mm256_add_epi16(in[7], one);
  in[8] = _mm256_add_epi16(in[8], one);
  in[9] = _mm256_add_epi16(in[9], one);
  in[10] = _mm256_add_epi16(in[10], one);
  in[11] = _mm256_add_epi16(in[11], one);
  in[12] = _mm256_add_epi16(in[12], one);
  in[13] = _mm256_add_epi16(in[13], one);
  in[14] = _mm256_add_epi16(in[14], one);
  in[15] = _mm256_add_epi16(in[15], one);

  in[0] = _mm256_sub_epi16(in[0], s0);
  in[1] = _mm256_sub_epi16(in[1], s1);
  in[2] = _mm256_sub_epi16(in[2], s2);
  in[3] = _mm256_sub_epi16(in[3], s3);
  in[4] = _mm256_sub_epi16(in[4], s4);
  in[5] = _mm256_sub_epi16(in[5], s5);
  in[6] = _mm256_sub_epi16(in[6], s6);
  in[7] = _mm256_sub_epi16(in[7], s7);
  in[8] = _mm256_sub_epi16(in[8], s8);
  in[9] = _mm256_sub_epi16(in[9], s9);
  in[10] = _mm256_sub_epi16(in[10], s10);
  in[11] = _mm256_sub_epi16(in[11], s11);
  in[12] = _mm256_sub_epi16(in[12], s12);
  in[13] = _mm256_sub_epi16(in[13], s13);
  in[14] = _mm256_sub_epi16(in[14], s14);
  in[15] = _mm256_sub_epi16(in[15], s15);

  in[0] = _mm256_srai_epi16(in[0], 2);
  in[1] = _mm256_srai_epi16(in[1], 2);
  in[2] = _mm256_srai_epi16(in[2], 2);
  in[3] = _mm256_srai_epi16(in[3], 2);
  in[4] = _mm256_srai_epi16(in[4], 2);
  in[5] = _mm256_srai_epi16(in[5], 2);
  in[6] = _mm256_srai_epi16(in[6], 2);
  in[7] = _mm256_srai_epi16(in[7], 2);
  in[8] = _mm256_srai_epi16(in[8], 2);
  in[9] = _mm256_srai_epi16(in[9], 2);
  in[10] = _mm256_srai_epi16(in[10], 2);
  in[11] = _mm256_srai_epi16(in[11], 2);
  in[12] = _mm256_srai_epi16(in[12], 2);
  in[13] = _mm256_srai_epi16(in[13], 2);
  in[14] = _mm256_srai_epi16(in[14], 2);
  in[15] = _mm256_srai_epi16(in[15], 2);
}

static void fdct16_avx2(__m256i *in) {
  // sequence: cospi_L_H = pairs(L, H) and L first
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  const __m256i cospi_p16_p16 = pair256_set_epi16(cospi_16_64, cospi_16_64);
  const __m256i cospi_p24_p08 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i cospi_m08_p24 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i cospi_m24_m08 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);

  const __m256i cospi_p28_p04 = pair256_set_epi16(cospi_28_64, cospi_4_64);
  const __m256i cospi_m04_p28 = pair256_set_epi16(-cospi_4_64, cospi_28_64);
  const __m256i cospi_p12_p20 = pair256_set_epi16(cospi_12_64, cospi_20_64);
  const __m256i cospi_m20_p12 = pair256_set_epi16(-cospi_20_64, cospi_12_64);

  const __m256i cospi_p30_p02 = pair256_set_epi16(cospi_30_64, cospi_2_64);
  const __m256i cospi_m02_p30 = pair256_set_epi16(-cospi_2_64, cospi_30_64);

  const __m256i cospi_p14_p18 = pair256_set_epi16(cospi_14_64, cospi_18_64);
  const __m256i cospi_m18_p14 = pair256_set_epi16(-cospi_18_64, cospi_14_64);

  const __m256i cospi_p22_p10 = pair256_set_epi16(cospi_22_64, cospi_10_64);
  const __m256i cospi_m10_p22 = pair256_set_epi16(-cospi_10_64, cospi_22_64);

  const __m256i cospi_p06_p26 = pair256_set_epi16(cospi_6_64, cospi_26_64);
  const __m256i cospi_m26_p06 = pair256_set_epi16(-cospi_26_64, cospi_6_64);

  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i s0, s1, s2, s3, s4, s5, s6, s7;
  __m256i t0, t1, t2, t3, t4, t5, t6, t7;
  __m256i v0, v1, v2, v3;
  __m256i x0, x1;

  // 0, 4, 8, 12
  u0 = _mm256_add_epi16(in[0], in[15]);
  u1 = _mm256_add_epi16(in[1], in[14]);
  u2 = _mm256_add_epi16(in[2], in[13]);
  u3 = _mm256_add_epi16(in[3], in[12]);
  u4 = _mm256_add_epi16(in[4], in[11]);
  u5 = _mm256_add_epi16(in[5], in[10]);
  u6 = _mm256_add_epi16(in[6], in[9]);
  u7 = _mm256_add_epi16(in[7], in[8]);

  s0 = _mm256_add_epi16(u0, u7);
  s1 = _mm256_add_epi16(u1, u6);
  s2 = _mm256_add_epi16(u2, u5);
  s3 = _mm256_add_epi16(u3, u4);

  // 0, 8
  v0 = _mm256_add_epi16(s0, s3);
  v1 = _mm256_add_epi16(s1, s2);

  x0 = _mm256_unpacklo_epi16(v0, v1);
  x1 = _mm256_unpackhi_epi16(v0, v1);

  t0 = butter_fly(&x0, &x1, &cospi_p16_p16);
  t1 = butter_fly(&x0, &x1, &cospi_p16_m16);

  // 4, 12
  v0 = _mm256_sub_epi16(s1, s2);
  v1 = _mm256_sub_epi16(s0, s3);

  x0 = _mm256_unpacklo_epi16(v0, v1);
  x1 = _mm256_unpackhi_epi16(v0, v1);

  t2 = butter_fly(&x0, &x1, &cospi_p24_p08);
  t3 = butter_fly(&x0, &x1, &cospi_m08_p24);

  // 2, 6, 10, 14
  s0 = _mm256_sub_epi16(u3, u4);
  s1 = _mm256_sub_epi16(u2, u5);
  s2 = _mm256_sub_epi16(u1, u6);
  s3 = _mm256_sub_epi16(u0, u7);

  v0 = s0;  // output[4]
  v3 = s3;  // output[7]

  x0 = _mm256_unpacklo_epi16(s2, s1);
  x1 = _mm256_unpackhi_epi16(s2, s1);

  v2 = butter_fly(&x0, &x1, &cospi_p16_p16);  // output[5]
  v1 = butter_fly(&x0, &x1, &cospi_p16_m16);  // output[6]

  s0 = _mm256_add_epi16(v0, v1);  // step[4]
  s1 = _mm256_sub_epi16(v0, v1);  // step[5]
  s2 = _mm256_sub_epi16(v3, v2);  // step[6]
  s3 = _mm256_add_epi16(v3, v2);  // step[7]

  // 2, 14
  x0 = _mm256_unpacklo_epi16(s0, s3);
  x1 = _mm256_unpackhi_epi16(s0, s3);

  t4 = butter_fly(&x0, &x1, &cospi_p28_p04);
  t5 = butter_fly(&x0, &x1, &cospi_m04_p28);

  // 10, 6
  x0 = _mm256_unpacklo_epi16(s1, s2);
  x1 = _mm256_unpackhi_epi16(s1, s2);
  t6 = butter_fly(&x0, &x1, &cospi_p12_p20);
  t7 = butter_fly(&x0, &x1, &cospi_m20_p12);

  // 1, 3, 5, 7, 9, 11, 13, 15
  s0 = _mm256_sub_epi16(in[7], in[8]);  // step[8]
  s1 = _mm256_sub_epi16(in[6], in[9]);  // step[9]
  u2 = _mm256_sub_epi16(in[5], in[10]);
  u3 = _mm256_sub_epi16(in[4], in[11]);
  u4 = _mm256_sub_epi16(in[3], in[12]);
  u5 = _mm256_sub_epi16(in[2], in[13]);
  s6 = _mm256_sub_epi16(in[1], in[14]);  // step[14]
  s7 = _mm256_sub_epi16(in[0], in[15]);  // step[15]

  in[0] = t0;
  in[8] = t1;
  in[4] = t2;
  in[12] = t3;
  in[2] = t4;
  in[14] = t5;
  in[10] = t6;
  in[6] = t7;

  x0 = _mm256_unpacklo_epi16(u5, u2);
  x1 = _mm256_unpackhi_epi16(u5, u2);

  s2 = butter_fly(&x0, &x1, &cospi_p16_p16);  // step[13]
  s5 = butter_fly(&x0, &x1, &cospi_p16_m16);  // step[10]

  x0 = _mm256_unpacklo_epi16(u4, u3);
  x1 = _mm256_unpackhi_epi16(u4, u3);

  s3 = butter_fly(&x0, &x1, &cospi_p16_p16);  // step[12]
  s4 = butter_fly(&x0, &x1, &cospi_p16_m16);  // step[11]

  u0 = _mm256_add_epi16(s0, s4);  // output[8]
  u1 = _mm256_add_epi16(s1, s5);
  u2 = _mm256_sub_epi16(s1, s5);
  u3 = _mm256_sub_epi16(s0, s4);
  u4 = _mm256_sub_epi16(s7, s3);
  u5 = _mm256_sub_epi16(s6, s2);
  u6 = _mm256_add_epi16(s6, s2);
  u7 = _mm256_add_epi16(s7, s3);

  // stage 4
  s0 = u0;
  s3 = u3;
  s4 = u4;
  s7 = u7;

  x0 = _mm256_unpacklo_epi16(u1, u6);
  x1 = _mm256_unpackhi_epi16(u1, u6);

  s1 = butter_fly(&x0, &x1, &cospi_m08_p24);
  s6 = butter_fly(&x0, &x1, &cospi_p24_p08);

  x0 = _mm256_unpacklo_epi16(u2, u5);
  x1 = _mm256_unpackhi_epi16(u2, u5);

  s2 = butter_fly(&x0, &x1, &cospi_m24_m08);
  s5 = butter_fly(&x0, &x1, &cospi_m08_p24);

  // stage 5
  u0 = _mm256_add_epi16(s0, s1);
  u1 = _mm256_sub_epi16(s0, s1);
  u2 = _mm256_sub_epi16(s3, s2);
  u3 = _mm256_add_epi16(s3, s2);
  u4 = _mm256_add_epi16(s4, s5);
  u5 = _mm256_sub_epi16(s4, s5);
  u6 = _mm256_sub_epi16(s7, s6);
  u7 = _mm256_add_epi16(s7, s6);

  // stage 6
  x0 = _mm256_unpacklo_epi16(u0, u7);
  x1 = _mm256_unpackhi_epi16(u0, u7);
  in[1] = butter_fly(&x0, &x1, &cospi_p30_p02);
  in[15] = butter_fly(&x0, &x1, &cospi_m02_p30);

  x0 = _mm256_unpacklo_epi16(u1, u6);
  x1 = _mm256_unpackhi_epi16(u1, u6);
  in[9] = butter_fly(&x0, &x1, &cospi_p14_p18);
  in[7] = butter_fly(&x0, &x1, &cospi_m18_p14);

  x0 = _mm256_unpacklo_epi16(u2, u5);
  x1 = _mm256_unpackhi_epi16(u2, u5);
  in[5] = butter_fly(&x0, &x1, &cospi_p22_p10);
  in[11] = butter_fly(&x0, &x1, &cospi_m10_p22);

  x0 = _mm256_unpacklo_epi16(u3, u4);
  x1 = _mm256_unpackhi_epi16(u3, u4);
  in[13] = butter_fly(&x0, &x1, &cospi_p06_p26);
  in[3] = butter_fly(&x0, &x1, &cospi_m26_p06);
}

void fadst16_avx2(__m256i *in) {
  const __m256i cospi_p01_p31 = pair256_set_epi16(cospi_1_64, cospi_31_64);
  const __m256i cospi_p31_m01 = pair256_set_epi16(cospi_31_64, -cospi_1_64);
  const __m256i cospi_p05_p27 = pair256_set_epi16(cospi_5_64, cospi_27_64);
  const __m256i cospi_p27_m05 = pair256_set_epi16(cospi_27_64, -cospi_5_64);
  const __m256i cospi_p09_p23 = pair256_set_epi16(cospi_9_64, cospi_23_64);
  const __m256i cospi_p23_m09 = pair256_set_epi16(cospi_23_64, -cospi_9_64);
  const __m256i cospi_p13_p19 = pair256_set_epi16(cospi_13_64, cospi_19_64);
  const __m256i cospi_p19_m13 = pair256_set_epi16(cospi_19_64, -cospi_13_64);
  const __m256i cospi_p17_p15 = pair256_set_epi16(cospi_17_64, cospi_15_64);
  const __m256i cospi_p15_m17 = pair256_set_epi16(cospi_15_64, -cospi_17_64);
  const __m256i cospi_p21_p11 = pair256_set_epi16(cospi_21_64, cospi_11_64);
  const __m256i cospi_p11_m21 = pair256_set_epi16(cospi_11_64, -cospi_21_64);
  const __m256i cospi_p25_p07 = pair256_set_epi16(cospi_25_64, cospi_7_64);
  const __m256i cospi_p07_m25 = pair256_set_epi16(cospi_7_64, -cospi_25_64);
  const __m256i cospi_p29_p03 = pair256_set_epi16(cospi_29_64, cospi_3_64);
  const __m256i cospi_p03_m29 = pair256_set_epi16(cospi_3_64, -cospi_29_64);
  const __m256i cospi_p04_p28 = pair256_set_epi16(cospi_4_64, cospi_28_64);
  const __m256i cospi_p28_m04 = pair256_set_epi16(cospi_28_64, -cospi_4_64);
  const __m256i cospi_p20_p12 = pair256_set_epi16(cospi_20_64, cospi_12_64);
  const __m256i cospi_p12_m20 = pair256_set_epi16(cospi_12_64, -cospi_20_64);
  const __m256i cospi_m28_p04 = pair256_set_epi16(-cospi_28_64, cospi_4_64);
  const __m256i cospi_m12_p20 = pair256_set_epi16(-cospi_12_64, cospi_20_64);
  const __m256i cospi_p08_p24 = pair256_set_epi16(cospi_8_64, cospi_24_64);
  const __m256i cospi_p24_m08 = pair256_set_epi16(cospi_24_64, -cospi_8_64);
  const __m256i cospi_m24_p08 = pair256_set_epi16(-cospi_24_64, cospi_8_64);
  const __m256i cospi_m16_m16 = _mm256_set1_epi16((int16_t)-cospi_16_64);
  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  const __m256i cospi_m16_p16 = pair256_set_epi16(-cospi_16_64, cospi_16_64);
  const __m256i zero = _mm256_setzero_si256();
  const __m256i dct_rounding = _mm256_set1_epi32(DCT_CONST_ROUNDING);
  __m256i s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15;
  __m256i x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
  __m256i u0, u1, u2, u3, u4, u5, u6, u7, u8, u9, u10, u11, u12, u13, u14, u15;
  __m256i v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;
  __m256i y0, y1;

  // stage 1, s takes low 256 bits; x takes high 256 bits
  y0 = _mm256_unpacklo_epi16(in[15], in[0]);
  y1 = _mm256_unpackhi_epi16(in[15], in[0]);
  s0 = _mm256_madd_epi16(y0, cospi_p01_p31);
  x0 = _mm256_madd_epi16(y1, cospi_p01_p31);
  s1 = _mm256_madd_epi16(y0, cospi_p31_m01);
  x1 = _mm256_madd_epi16(y1, cospi_p31_m01);

  y0 = _mm256_unpacklo_epi16(in[13], in[2]);
  y1 = _mm256_unpackhi_epi16(in[13], in[2]);
  s2 = _mm256_madd_epi16(y0, cospi_p05_p27);
  x2 = _mm256_madd_epi16(y1, cospi_p05_p27);
  s3 = _mm256_madd_epi16(y0, cospi_p27_m05);
  x3 = _mm256_madd_epi16(y1, cospi_p27_m05);

  y0 = _mm256_unpacklo_epi16(in[11], in[4]);
  y1 = _mm256_unpackhi_epi16(in[11], in[4]);
  s4 = _mm256_madd_epi16(y0, cospi_p09_p23);
  x4 = _mm256_madd_epi16(y1, cospi_p09_p23);
  s5 = _mm256_madd_epi16(y0, cospi_p23_m09);
  x5 = _mm256_madd_epi16(y1, cospi_p23_m09);

  y0 = _mm256_unpacklo_epi16(in[9], in[6]);
  y1 = _mm256_unpackhi_epi16(in[9], in[6]);
  s6 = _mm256_madd_epi16(y0, cospi_p13_p19);
  x6 = _mm256_madd_epi16(y1, cospi_p13_p19);
  s7 = _mm256_madd_epi16(y0, cospi_p19_m13);
  x7 = _mm256_madd_epi16(y1, cospi_p19_m13);

  y0 = _mm256_unpacklo_epi16(in[7], in[8]);
  y1 = _mm256_unpackhi_epi16(in[7], in[8]);
  s8 = _mm256_madd_epi16(y0, cospi_p17_p15);
  x8 = _mm256_madd_epi16(y1, cospi_p17_p15);
  s9 = _mm256_madd_epi16(y0, cospi_p15_m17);
  x9 = _mm256_madd_epi16(y1, cospi_p15_m17);

  y0 = _mm256_unpacklo_epi16(in[5], in[10]);
  y1 = _mm256_unpackhi_epi16(in[5], in[10]);
  s10 = _mm256_madd_epi16(y0, cospi_p21_p11);
  x10 = _mm256_madd_epi16(y1, cospi_p21_p11);
  s11 = _mm256_madd_epi16(y0, cospi_p11_m21);
  x11 = _mm256_madd_epi16(y1, cospi_p11_m21);

  y0 = _mm256_unpacklo_epi16(in[3], in[12]);
  y1 = _mm256_unpackhi_epi16(in[3], in[12]);
  s12 = _mm256_madd_epi16(y0, cospi_p25_p07);
  x12 = _mm256_madd_epi16(y1, cospi_p25_p07);
  s13 = _mm256_madd_epi16(y0, cospi_p07_m25);
  x13 = _mm256_madd_epi16(y1, cospi_p07_m25);

  y0 = _mm256_unpacklo_epi16(in[1], in[14]);
  y1 = _mm256_unpackhi_epi16(in[1], in[14]);
  s14 = _mm256_madd_epi16(y0, cospi_p29_p03);
  x14 = _mm256_madd_epi16(y1, cospi_p29_p03);
  s15 = _mm256_madd_epi16(y0, cospi_p03_m29);
  x15 = _mm256_madd_epi16(y1, cospi_p03_m29);

  // u takes low 256 bits; v takes high 256 bits
  u0 = _mm256_add_epi32(s0, s8);
  u1 = _mm256_add_epi32(s1, s9);
  u2 = _mm256_add_epi32(s2, s10);
  u3 = _mm256_add_epi32(s3, s11);
  u4 = _mm256_add_epi32(s4, s12);
  u5 = _mm256_add_epi32(s5, s13);
  u6 = _mm256_add_epi32(s6, s14);
  u7 = _mm256_add_epi32(s7, s15);

  u8 = _mm256_sub_epi32(s0, s8);
  u9 = _mm256_sub_epi32(s1, s9);
  u10 = _mm256_sub_epi32(s2, s10);
  u11 = _mm256_sub_epi32(s3, s11);
  u12 = _mm256_sub_epi32(s4, s12);
  u13 = _mm256_sub_epi32(s5, s13);
  u14 = _mm256_sub_epi32(s6, s14);
  u15 = _mm256_sub_epi32(s7, s15);

  v0 = _mm256_add_epi32(x0, x8);
  v1 = _mm256_add_epi32(x1, x9);
  v2 = _mm256_add_epi32(x2, x10);
  v3 = _mm256_add_epi32(x3, x11);
  v4 = _mm256_add_epi32(x4, x12);
  v5 = _mm256_add_epi32(x5, x13);
  v6 = _mm256_add_epi32(x6, x14);
  v7 = _mm256_add_epi32(x7, x15);

  v8 = _mm256_sub_epi32(x0, x8);
  v9 = _mm256_sub_epi32(x1, x9);
  v10 = _mm256_sub_epi32(x2, x10);
  v11 = _mm256_sub_epi32(x3, x11);
  v12 = _mm256_sub_epi32(x4, x12);
  v13 = _mm256_sub_epi32(x5, x13);
  v14 = _mm256_sub_epi32(x6, x14);
  v15 = _mm256_sub_epi32(x7, x15);

  // low 256 bits rounding
  u8 = _mm256_add_epi32(u8, dct_rounding);
  u9 = _mm256_add_epi32(u9, dct_rounding);
  u10 = _mm256_add_epi32(u10, dct_rounding);
  u11 = _mm256_add_epi32(u11, dct_rounding);
  u12 = _mm256_add_epi32(u12, dct_rounding);
  u13 = _mm256_add_epi32(u13, dct_rounding);
  u14 = _mm256_add_epi32(u14, dct_rounding);
  u15 = _mm256_add_epi32(u15, dct_rounding);

  u8 = _mm256_srai_epi32(u8, DCT_CONST_BITS);
  u9 = _mm256_srai_epi32(u9, DCT_CONST_BITS);
  u10 = _mm256_srai_epi32(u10, DCT_CONST_BITS);
  u11 = _mm256_srai_epi32(u11, DCT_CONST_BITS);
  u12 = _mm256_srai_epi32(u12, DCT_CONST_BITS);
  u13 = _mm256_srai_epi32(u13, DCT_CONST_BITS);
  u14 = _mm256_srai_epi32(u14, DCT_CONST_BITS);
  u15 = _mm256_srai_epi32(u15, DCT_CONST_BITS);

  // high 256 bits rounding
  v8 = _mm256_add_epi32(v8, dct_rounding);
  v9 = _mm256_add_epi32(v9, dct_rounding);
  v10 = _mm256_add_epi32(v10, dct_rounding);
  v11 = _mm256_add_epi32(v11, dct_rounding);
  v12 = _mm256_add_epi32(v12, dct_rounding);
  v13 = _mm256_add_epi32(v13, dct_rounding);
  v14 = _mm256_add_epi32(v14, dct_rounding);
  v15 = _mm256_add_epi32(v15, dct_rounding);

  v8 = _mm256_srai_epi32(v8, DCT_CONST_BITS);
  v9 = _mm256_srai_epi32(v9, DCT_CONST_BITS);
  v10 = _mm256_srai_epi32(v10, DCT_CONST_BITS);
  v11 = _mm256_srai_epi32(v11, DCT_CONST_BITS);
  v12 = _mm256_srai_epi32(v12, DCT_CONST_BITS);
  v13 = _mm256_srai_epi32(v13, DCT_CONST_BITS);
  v14 = _mm256_srai_epi32(v14, DCT_CONST_BITS);
  v15 = _mm256_srai_epi32(v15, DCT_CONST_BITS);

  // Saturation pack 32-bit to 16-bit
  x8 = _mm256_packs_epi32(u8, v8);
  x9 = _mm256_packs_epi32(u9, v9);
  x10 = _mm256_packs_epi32(u10, v10);
  x11 = _mm256_packs_epi32(u11, v11);
  x12 = _mm256_packs_epi32(u12, v12);
  x13 = _mm256_packs_epi32(u13, v13);
  x14 = _mm256_packs_epi32(u14, v14);
  x15 = _mm256_packs_epi32(u15, v15);

  // stage 2
  y0 = _mm256_unpacklo_epi16(x8, x9);
  y1 = _mm256_unpackhi_epi16(x8, x9);
  s8 = _mm256_madd_epi16(y0, cospi_p04_p28);
  x8 = _mm256_madd_epi16(y1, cospi_p04_p28);
  s9 = _mm256_madd_epi16(y0, cospi_p28_m04);
  x9 = _mm256_madd_epi16(y1, cospi_p28_m04);

  y0 = _mm256_unpacklo_epi16(x10, x11);
  y1 = _mm256_unpackhi_epi16(x10, x11);
  s10 = _mm256_madd_epi16(y0, cospi_p20_p12);
  x10 = _mm256_madd_epi16(y1, cospi_p20_p12);
  s11 = _mm256_madd_epi16(y0, cospi_p12_m20);
  x11 = _mm256_madd_epi16(y1, cospi_p12_m20);

  y0 = _mm256_unpacklo_epi16(x12, x13);
  y1 = _mm256_unpackhi_epi16(x12, x13);
  s12 = _mm256_madd_epi16(y0, cospi_m28_p04);
  x12 = _mm256_madd_epi16(y1, cospi_m28_p04);
  s13 = _mm256_madd_epi16(y0, cospi_p04_p28);
  x13 = _mm256_madd_epi16(y1, cospi_p04_p28);

  y0 = _mm256_unpacklo_epi16(x14, x15);
  y1 = _mm256_unpackhi_epi16(x14, x15);
  s14 = _mm256_madd_epi16(y0, cospi_m12_p20);
  x14 = _mm256_madd_epi16(y1, cospi_m12_p20);
  s15 = _mm256_madd_epi16(y0, cospi_p20_p12);
  x15 = _mm256_madd_epi16(y1, cospi_p20_p12);

  x0 = _mm256_add_epi32(u0, u4);
  s0 = _mm256_add_epi32(v0, v4);
  x1 = _mm256_add_epi32(u1, u5);
  s1 = _mm256_add_epi32(v1, v5);
  x2 = _mm256_add_epi32(u2, u6);
  s2 = _mm256_add_epi32(v2, v6);
  x3 = _mm256_add_epi32(u3, u7);
  s3 = _mm256_add_epi32(v3, v7);

  v8 = _mm256_sub_epi32(u0, u4);
  v9 = _mm256_sub_epi32(v0, v4);
  v10 = _mm256_sub_epi32(u1, u5);
  v11 = _mm256_sub_epi32(v1, v5);
  v12 = _mm256_sub_epi32(u2, u6);
  v13 = _mm256_sub_epi32(v2, v6);
  v14 = _mm256_sub_epi32(u3, u7);
  v15 = _mm256_sub_epi32(v3, v7);

  v8 = _mm256_add_epi32(v8, dct_rounding);
  v9 = _mm256_add_epi32(v9, dct_rounding);
  v10 = _mm256_add_epi32(v10, dct_rounding);
  v11 = _mm256_add_epi32(v11, dct_rounding);
  v12 = _mm256_add_epi32(v12, dct_rounding);
  v13 = _mm256_add_epi32(v13, dct_rounding);
  v14 = _mm256_add_epi32(v14, dct_rounding);
  v15 = _mm256_add_epi32(v15, dct_rounding);

  v8 = _mm256_srai_epi32(v8, DCT_CONST_BITS);
  v9 = _mm256_srai_epi32(v9, DCT_CONST_BITS);
  v10 = _mm256_srai_epi32(v10, DCT_CONST_BITS);
  v11 = _mm256_srai_epi32(v11, DCT_CONST_BITS);
  v12 = _mm256_srai_epi32(v12, DCT_CONST_BITS);
  v13 = _mm256_srai_epi32(v13, DCT_CONST_BITS);
  v14 = _mm256_srai_epi32(v14, DCT_CONST_BITS);
  v15 = _mm256_srai_epi32(v15, DCT_CONST_BITS);

  x4 = _mm256_packs_epi32(v8, v9);
  x5 = _mm256_packs_epi32(v10, v11);
  x6 = _mm256_packs_epi32(v12, v13);
  x7 = _mm256_packs_epi32(v14, v15);

  u8 = _mm256_add_epi32(s8, s12);
  u9 = _mm256_add_epi32(s9, s13);
  u10 = _mm256_add_epi32(s10, s14);
  u11 = _mm256_add_epi32(s11, s15);
  u12 = _mm256_sub_epi32(s8, s12);
  u13 = _mm256_sub_epi32(s9, s13);
  u14 = _mm256_sub_epi32(s10, s14);
  u15 = _mm256_sub_epi32(s11, s15);

  v8 = _mm256_add_epi32(x8, x12);
  v9 = _mm256_add_epi32(x9, x13);
  v10 = _mm256_add_epi32(x10, x14);
  v11 = _mm256_add_epi32(x11, x15);
  v12 = _mm256_sub_epi32(x8, x12);
  v13 = _mm256_sub_epi32(x9, x13);
  v14 = _mm256_sub_epi32(x10, x14);
  v15 = _mm256_sub_epi32(x11, x15);

  u12 = _mm256_add_epi32(u12, dct_rounding);
  u13 = _mm256_add_epi32(u13, dct_rounding);
  u14 = _mm256_add_epi32(u14, dct_rounding);
  u15 = _mm256_add_epi32(u15, dct_rounding);

  u12 = _mm256_srai_epi32(u12, DCT_CONST_BITS);
  u13 = _mm256_srai_epi32(u13, DCT_CONST_BITS);
  u14 = _mm256_srai_epi32(u14, DCT_CONST_BITS);
  u15 = _mm256_srai_epi32(u15, DCT_CONST_BITS);

  v12 = _mm256_add_epi32(v12, dct_rounding);
  v13 = _mm256_add_epi32(v13, dct_rounding);
  v14 = _mm256_add_epi32(v14, dct_rounding);
  v15 = _mm256_add_epi32(v15, dct_rounding);

  v12 = _mm256_srai_epi32(v12, DCT_CONST_BITS);
  v13 = _mm256_srai_epi32(v13, DCT_CONST_BITS);
  v14 = _mm256_srai_epi32(v14, DCT_CONST_BITS);
  v15 = _mm256_srai_epi32(v15, DCT_CONST_BITS);

  x12 = _mm256_packs_epi32(u12, v12);
  x13 = _mm256_packs_epi32(u13, v13);
  x14 = _mm256_packs_epi32(u14, v14);
  x15 = _mm256_packs_epi32(u15, v15);

  // stage 3
  y0 = _mm256_unpacklo_epi16(x4, x5);
  y1 = _mm256_unpackhi_epi16(x4, x5);
  s4 = _mm256_madd_epi16(y0, cospi_p08_p24);
  x4 = _mm256_madd_epi16(y1, cospi_p08_p24);
  s5 = _mm256_madd_epi16(y0, cospi_p24_m08);
  x5 = _mm256_madd_epi16(y1, cospi_p24_m08);

  y0 = _mm256_unpacklo_epi16(x6, x7);
  y1 = _mm256_unpackhi_epi16(x6, x7);
  s6 = _mm256_madd_epi16(y0, cospi_m24_p08);
  x6 = _mm256_madd_epi16(y1, cospi_m24_p08);
  s7 = _mm256_madd_epi16(y0, cospi_p08_p24);
  x7 = _mm256_madd_epi16(y1, cospi_p08_p24);

  y0 = _mm256_unpacklo_epi16(x12, x13);
  y1 = _mm256_unpackhi_epi16(x12, x13);
  s12 = _mm256_madd_epi16(y0, cospi_p08_p24);
  x12 = _mm256_madd_epi16(y1, cospi_p08_p24);
  s13 = _mm256_madd_epi16(y0, cospi_p24_m08);
  x13 = _mm256_madd_epi16(y1, cospi_p24_m08);

  y0 = _mm256_unpacklo_epi16(x14, x15);
  y1 = _mm256_unpackhi_epi16(x14, x15);
  s14 = _mm256_madd_epi16(y0, cospi_m24_p08);
  x14 = _mm256_madd_epi16(y1, cospi_m24_p08);
  s15 = _mm256_madd_epi16(y0, cospi_p08_p24);
  x15 = _mm256_madd_epi16(y1, cospi_p08_p24);

  u0 = _mm256_add_epi32(x0, x2);
  v0 = _mm256_add_epi32(s0, s2);
  u1 = _mm256_add_epi32(x1, x3);
  v1 = _mm256_add_epi32(s1, s3);
  u2 = _mm256_sub_epi32(x0, x2);
  v2 = _mm256_sub_epi32(s0, s2);
  u3 = _mm256_sub_epi32(x1, x3);
  v3 = _mm256_sub_epi32(s1, s3);

  u0 = _mm256_add_epi32(u0, dct_rounding);
  v0 = _mm256_add_epi32(v0, dct_rounding);
  u1 = _mm256_add_epi32(u1, dct_rounding);
  v1 = _mm256_add_epi32(v1, dct_rounding);
  u2 = _mm256_add_epi32(u2, dct_rounding);
  v2 = _mm256_add_epi32(v2, dct_rounding);
  u3 = _mm256_add_epi32(u3, dct_rounding);
  v3 = _mm256_add_epi32(v3, dct_rounding);

  u0 = _mm256_srai_epi32(u0, DCT_CONST_BITS);
  v0 = _mm256_srai_epi32(v0, DCT_CONST_BITS);
  u1 = _mm256_srai_epi32(u1, DCT_CONST_BITS);
  v1 = _mm256_srai_epi32(v1, DCT_CONST_BITS);
  u2 = _mm256_srai_epi32(u2, DCT_CONST_BITS);
  v2 = _mm256_srai_epi32(v2, DCT_CONST_BITS);
  u3 = _mm256_srai_epi32(u3, DCT_CONST_BITS);
  v3 = _mm256_srai_epi32(v3, DCT_CONST_BITS);

  in[0] = _mm256_packs_epi32(u0, v0);
  x1 = _mm256_packs_epi32(u1, v1);
  x2 = _mm256_packs_epi32(u2, v2);
  x3 = _mm256_packs_epi32(u3, v3);

  // Rounding on s4 + s6, s5 + s7, s4 - s6, s5 - s7
  u4 = _mm256_add_epi32(s4, s6);
  u5 = _mm256_add_epi32(s5, s7);
  u6 = _mm256_sub_epi32(s4, s6);
  u7 = _mm256_sub_epi32(s5, s7);

  v4 = _mm256_add_epi32(x4, x6);
  v5 = _mm256_add_epi32(x5, x7);
  v6 = _mm256_sub_epi32(x4, x6);
  v7 = _mm256_sub_epi32(x5, x7);

  u4 = _mm256_add_epi32(u4, dct_rounding);
  u5 = _mm256_add_epi32(u5, dct_rounding);
  u6 = _mm256_add_epi32(u6, dct_rounding);
  u7 = _mm256_add_epi32(u7, dct_rounding);

  u4 = _mm256_srai_epi32(u4, DCT_CONST_BITS);
  u5 = _mm256_srai_epi32(u5, DCT_CONST_BITS);
  u6 = _mm256_srai_epi32(u6, DCT_CONST_BITS);
  u7 = _mm256_srai_epi32(u7, DCT_CONST_BITS);

  v4 = _mm256_add_epi32(v4, dct_rounding);
  v5 = _mm256_add_epi32(v5, dct_rounding);
  v6 = _mm256_add_epi32(v6, dct_rounding);
  v7 = _mm256_add_epi32(v7, dct_rounding);

  v4 = _mm256_srai_epi32(v4, DCT_CONST_BITS);
  v5 = _mm256_srai_epi32(v5, DCT_CONST_BITS);
  v6 = _mm256_srai_epi32(v6, DCT_CONST_BITS);
  v7 = _mm256_srai_epi32(v7, DCT_CONST_BITS);

  x4 = _mm256_packs_epi32(u4, v4);
  in[12] = _mm256_packs_epi32(u5, v5);
  x6 = _mm256_packs_epi32(u6, v6);
  x7 = _mm256_packs_epi32(u7, v7);

  u0 = _mm256_add_epi32(u8, u10);
  v0 = _mm256_add_epi32(v8, v10);
  u1 = _mm256_add_epi32(u9, u11);
  v1 = _mm256_add_epi32(v9, v11);
  u2 = _mm256_sub_epi32(u8, u10);
  v2 = _mm256_sub_epi32(v8, v10);
  u3 = _mm256_sub_epi32(u9, u11);
  v3 = _mm256_sub_epi32(v9, v11);

  u0 = _mm256_add_epi32(u0, dct_rounding);
  v0 = _mm256_add_epi32(v0, dct_rounding);
  u1 = _mm256_add_epi32(u1, dct_rounding);
  v1 = _mm256_add_epi32(v1, dct_rounding);
  u2 = _mm256_add_epi32(u2, dct_rounding);
  v2 = _mm256_add_epi32(v2, dct_rounding);
  u3 = _mm256_add_epi32(u3, dct_rounding);
  v3 = _mm256_add_epi32(v3, dct_rounding);

  u0 = _mm256_srai_epi32(u0, DCT_CONST_BITS);
  v0 = _mm256_srai_epi32(v0, DCT_CONST_BITS);
  u1 = _mm256_srai_epi32(u1, DCT_CONST_BITS);
  v1 = _mm256_srai_epi32(v1, DCT_CONST_BITS);
  u2 = _mm256_srai_epi32(u2, DCT_CONST_BITS);
  v2 = _mm256_srai_epi32(v2, DCT_CONST_BITS);
  u3 = _mm256_srai_epi32(u3, DCT_CONST_BITS);
  v3 = _mm256_srai_epi32(v3, DCT_CONST_BITS);

  x8 = _mm256_packs_epi32(u0, v0);
  in[14] = _mm256_packs_epi32(u1, v1);
  x10 = _mm256_packs_epi32(u2, v2);
  x11 = _mm256_packs_epi32(u3, v3);

  // Rounding on s12 + s14, s13 + s15, s12 - s14, s13 - s15
  u12 = _mm256_add_epi32(s12, s14);
  u13 = _mm256_add_epi32(s13, s15);
  u14 = _mm256_sub_epi32(s12, s14);
  u15 = _mm256_sub_epi32(s13, s15);

  v12 = _mm256_add_epi32(x12, x14);
  v13 = _mm256_add_epi32(x13, x15);
  v14 = _mm256_sub_epi32(x12, x14);
  v15 = _mm256_sub_epi32(x13, x15);

  u12 = _mm256_add_epi32(u12, dct_rounding);
  u13 = _mm256_add_epi32(u13, dct_rounding);
  u14 = _mm256_add_epi32(u14, dct_rounding);
  u15 = _mm256_add_epi32(u15, dct_rounding);

  u12 = _mm256_srai_epi32(u12, DCT_CONST_BITS);
  u13 = _mm256_srai_epi32(u13, DCT_CONST_BITS);
  u14 = _mm256_srai_epi32(u14, DCT_CONST_BITS);
  u15 = _mm256_srai_epi32(u15, DCT_CONST_BITS);

  v12 = _mm256_add_epi32(v12, dct_rounding);
  v13 = _mm256_add_epi32(v13, dct_rounding);
  v14 = _mm256_add_epi32(v14, dct_rounding);
  v15 = _mm256_add_epi32(v15, dct_rounding);

  v12 = _mm256_srai_epi32(v12, DCT_CONST_BITS);
  v13 = _mm256_srai_epi32(v13, DCT_CONST_BITS);
  v14 = _mm256_srai_epi32(v14, DCT_CONST_BITS);
  v15 = _mm256_srai_epi32(v15, DCT_CONST_BITS);

  x12 = _mm256_packs_epi32(u12, v12);
  x13 = _mm256_packs_epi32(u13, v13);
  x14 = _mm256_packs_epi32(u14, v14);
  x15 = _mm256_packs_epi32(u15, v15);
  in[2] = x12;

  // stage 4
  y0 = _mm256_unpacklo_epi16(x2, x3);
  y1 = _mm256_unpackhi_epi16(x2, x3);
  s2 = _mm256_madd_epi16(y0, cospi_m16_m16);
  x2 = _mm256_madd_epi16(y1, cospi_m16_m16);
  s3 = _mm256_madd_epi16(y0, cospi_p16_m16);
  x3 = _mm256_madd_epi16(y1, cospi_p16_m16);

  y0 = _mm256_unpacklo_epi16(x6, x7);
  y1 = _mm256_unpackhi_epi16(x6, x7);
  s6 = _mm256_madd_epi16(y0, cospi_p16_p16);
  x6 = _mm256_madd_epi16(y1, cospi_p16_p16);
  s7 = _mm256_madd_epi16(y0, cospi_m16_p16);
  x7 = _mm256_madd_epi16(y1, cospi_m16_p16);

  y0 = _mm256_unpacklo_epi16(x10, x11);
  y1 = _mm256_unpackhi_epi16(x10, x11);
  s10 = _mm256_madd_epi16(y0, cospi_p16_p16);
  x10 = _mm256_madd_epi16(y1, cospi_p16_p16);
  s11 = _mm256_madd_epi16(y0, cospi_m16_p16);
  x11 = _mm256_madd_epi16(y1, cospi_m16_p16);

  y0 = _mm256_unpacklo_epi16(x14, x15);
  y1 = _mm256_unpackhi_epi16(x14, x15);
  s14 = _mm256_madd_epi16(y0, cospi_m16_m16);
  x14 = _mm256_madd_epi16(y1, cospi_m16_m16);
  s15 = _mm256_madd_epi16(y0, cospi_p16_m16);
  x15 = _mm256_madd_epi16(y1, cospi_p16_m16);

  // Rounding
  u2 = _mm256_add_epi32(s2, dct_rounding);
  u3 = _mm256_add_epi32(s3, dct_rounding);
  u6 = _mm256_add_epi32(s6, dct_rounding);
  u7 = _mm256_add_epi32(s7, dct_rounding);

  u10 = _mm256_add_epi32(s10, dct_rounding);
  u11 = _mm256_add_epi32(s11, dct_rounding);
  u14 = _mm256_add_epi32(s14, dct_rounding);
  u15 = _mm256_add_epi32(s15, dct_rounding);

  u2 = _mm256_srai_epi32(u2, DCT_CONST_BITS);
  u3 = _mm256_srai_epi32(u3, DCT_CONST_BITS);
  u6 = _mm256_srai_epi32(u6, DCT_CONST_BITS);
  u7 = _mm256_srai_epi32(u7, DCT_CONST_BITS);

  u10 = _mm256_srai_epi32(u10, DCT_CONST_BITS);
  u11 = _mm256_srai_epi32(u11, DCT_CONST_BITS);
  u14 = _mm256_srai_epi32(u14, DCT_CONST_BITS);
  u15 = _mm256_srai_epi32(u15, DCT_CONST_BITS);

  v2 = _mm256_add_epi32(x2, dct_rounding);
  v3 = _mm256_add_epi32(x3, dct_rounding);
  v6 = _mm256_add_epi32(x6, dct_rounding);
  v7 = _mm256_add_epi32(x7, dct_rounding);

  v10 = _mm256_add_epi32(x10, dct_rounding);
  v11 = _mm256_add_epi32(x11, dct_rounding);
  v14 = _mm256_add_epi32(x14, dct_rounding);
  v15 = _mm256_add_epi32(x15, dct_rounding);

  v2 = _mm256_srai_epi32(v2, DCT_CONST_BITS);
  v3 = _mm256_srai_epi32(v3, DCT_CONST_BITS);
  v6 = _mm256_srai_epi32(v6, DCT_CONST_BITS);
  v7 = _mm256_srai_epi32(v7, DCT_CONST_BITS);

  v10 = _mm256_srai_epi32(v10, DCT_CONST_BITS);
  v11 = _mm256_srai_epi32(v11, DCT_CONST_BITS);
  v14 = _mm256_srai_epi32(v14, DCT_CONST_BITS);
  v15 = _mm256_srai_epi32(v15, DCT_CONST_BITS);

  in[7] = _mm256_packs_epi32(u2, v2);
  in[8] = _mm256_packs_epi32(u3, v3);

  in[4] = _mm256_packs_epi32(u6, v6);
  in[11] = _mm256_packs_epi32(u7, v7);

  in[6] = _mm256_packs_epi32(u10, v10);
  in[9] = _mm256_packs_epi32(u11, v11);

  in[5] = _mm256_packs_epi32(u14, v14);
  in[10] = _mm256_packs_epi32(u15, v15);

  in[1] = _mm256_sub_epi16(zero, x8);
  in[3] = _mm256_sub_epi16(zero, x4);
  in[13] = _mm256_sub_epi16(zero, x13);
  in[15] = _mm256_sub_epi16(zero, x1);
}

#if CONFIG_EXT_TX
static void fidtx16_avx2(__m256i *in) {
  txfm_scaling16_avx2((int16_t)Sqrt2, in);
}
#endif

void av1_fht16x16_avx2(const int16_t *input, tran_low_t *output, int stride,
                       TxfmParam *txfm_param) {
  __m256i in[16];
  int tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_16x16(input, stride, 0, 0, in);
      fdct16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fdct16_avx2(in);
      break;
    case ADST_DCT:
      load_buffer_16x16(input, stride, 0, 0, in);
      fadst16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fdct16_avx2(in);
      break;
    case DCT_ADST:
      load_buffer_16x16(input, stride, 0, 0, in);
      fdct16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fadst16_avx2(in);
      break;
    case ADST_ADST:
      load_buffer_16x16(input, stride, 0, 0, in);
      fadst16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fadst16_avx2(in);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_16x16(input, stride, 1, 0, in);
      fadst16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fdct16_avx2(in);
      break;
    case DCT_FLIPADST:
      load_buffer_16x16(input, stride, 0, 1, in);
      fdct16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fadst16_avx2(in);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_16x16(input, stride, 1, 1, in);
      fadst16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fadst16_avx2(in);
      break;
    case ADST_FLIPADST:
      load_buffer_16x16(input, stride, 0, 1, in);
      fadst16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fadst16_avx2(in);
      break;
    case FLIPADST_ADST:
      load_buffer_16x16(input, stride, 1, 0, in);
      fadst16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fadst16_avx2(in);
      break;
    case IDTX:
      load_buffer_16x16(input, stride, 0, 0, in);
      fidtx16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fidtx16_avx2(in);
      break;
    case V_DCT:
      load_buffer_16x16(input, stride, 0, 0, in);
      fdct16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fidtx16_avx2(in);
      break;
    case H_DCT:
      load_buffer_16x16(input, stride, 0, 0, in);
      fidtx16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fdct16_avx2(in);
      break;
    case V_ADST:
      load_buffer_16x16(input, stride, 0, 0, in);
      fadst16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fidtx16_avx2(in);
      break;
    case H_ADST:
      load_buffer_16x16(input, stride, 0, 0, in);
      fidtx16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fadst16_avx2(in);
      break;
    case V_FLIPADST:
      load_buffer_16x16(input, stride, 1, 0, in);
      fadst16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fidtx16_avx2(in);
      break;
    case H_FLIPADST:
      load_buffer_16x16(input, stride, 0, 1, in);
      fidtx16_avx2(in);
      mm256_transpose_16x16(in, in);
      right_shift_16x16(in);
      fadst16_avx2(in);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
  mm256_transpose_16x16(in, in);
  write_buffer_16x16(in, output);
  _mm256_zeroupper();
}

static void mm256_vectors_swap(__m256i *a0, __m256i *a1, const int size) {
  int i = 0;
  __m256i temp;
  while (i < size) {
    temp = a0[i];
    a0[i] = a1[i];
    a1[i] = temp;
    i++;
  }
}

static void mm256_transpose_32x32(__m256i *in0, __m256i *in1) {
  mm256_transpose_16x16(in0, in0);
  mm256_transpose_16x16(&in0[16], &in0[16]);
  mm256_transpose_16x16(in1, in1);
  mm256_transpose_16x16(&in1[16], &in1[16]);
  mm256_vectors_swap(&in0[16], in1, 16);
}

static void prepare_16x16_even(const __m256i *in, __m256i *even) {
  even[0] = _mm256_add_epi16(in[0], in[31]);
  even[1] = _mm256_add_epi16(in[1], in[30]);
  even[2] = _mm256_add_epi16(in[2], in[29]);
  even[3] = _mm256_add_epi16(in[3], in[28]);
  even[4] = _mm256_add_epi16(in[4], in[27]);
  even[5] = _mm256_add_epi16(in[5], in[26]);
  even[6] = _mm256_add_epi16(in[6], in[25]);
  even[7] = _mm256_add_epi16(in[7], in[24]);
  even[8] = _mm256_add_epi16(in[8], in[23]);
  even[9] = _mm256_add_epi16(in[9], in[22]);
  even[10] = _mm256_add_epi16(in[10], in[21]);
  even[11] = _mm256_add_epi16(in[11], in[20]);
  even[12] = _mm256_add_epi16(in[12], in[19]);
  even[13] = _mm256_add_epi16(in[13], in[18]);
  even[14] = _mm256_add_epi16(in[14], in[17]);
  even[15] = _mm256_add_epi16(in[15], in[16]);
}

static void prepare_16x16_odd(const __m256i *in, __m256i *odd) {
  odd[0] = _mm256_sub_epi16(in[15], in[16]);
  odd[1] = _mm256_sub_epi16(in[14], in[17]);
  odd[2] = _mm256_sub_epi16(in[13], in[18]);
  odd[3] = _mm256_sub_epi16(in[12], in[19]);
  odd[4] = _mm256_sub_epi16(in[11], in[20]);
  odd[5] = _mm256_sub_epi16(in[10], in[21]);
  odd[6] = _mm256_sub_epi16(in[9], in[22]);
  odd[7] = _mm256_sub_epi16(in[8], in[23]);
  odd[8] = _mm256_sub_epi16(in[7], in[24]);
  odd[9] = _mm256_sub_epi16(in[6], in[25]);
  odd[10] = _mm256_sub_epi16(in[5], in[26]);
  odd[11] = _mm256_sub_epi16(in[4], in[27]);
  odd[12] = _mm256_sub_epi16(in[3], in[28]);
  odd[13] = _mm256_sub_epi16(in[2], in[29]);
  odd[14] = _mm256_sub_epi16(in[1], in[30]);
  odd[15] = _mm256_sub_epi16(in[0], in[31]);
}

static void collect_16col(const __m256i *even, const __m256i *odd,
                          __m256i *out) {
  // fdct16_avx2() already maps the output
  out[0] = even[0];
  out[2] = even[1];
  out[4] = even[2];
  out[6] = even[3];
  out[8] = even[4];
  out[10] = even[5];
  out[12] = even[6];
  out[14] = even[7];
  out[16] = even[8];
  out[18] = even[9];
  out[20] = even[10];
  out[22] = even[11];
  out[24] = even[12];
  out[26] = even[13];
  out[28] = even[14];
  out[30] = even[15];

  out[1] = odd[0];
  out[17] = odd[1];
  out[9] = odd[2];
  out[25] = odd[3];
  out[5] = odd[4];
  out[21] = odd[5];
  out[13] = odd[6];
  out[29] = odd[7];
  out[3] = odd[8];
  out[19] = odd[9];
  out[11] = odd[10];
  out[27] = odd[11];
  out[7] = odd[12];
  out[23] = odd[13];
  out[15] = odd[14];
  out[31] = odd[15];
}

static void collect_coeffs(const __m256i *first_16col_even,
                           const __m256i *first_16col_odd,
                           const __m256i *second_16col_even,
                           const __m256i *second_16col_odd, __m256i *in0,
                           __m256i *in1) {
  collect_16col(first_16col_even, first_16col_odd, in0);
  collect_16col(second_16col_even, second_16col_odd, in1);
}

static void fdct16_odd_avx2(__m256i *in) {
  // sequence: cospi_L_H = pairs(L, H) and L first
  const __m256i cospi_p16_p16 = pair256_set_epi16(cospi_16_64, cospi_16_64);
  const __m256i cospi_m16_p16 = pair256_set_epi16(-cospi_16_64, cospi_16_64);
  const __m256i cospi_m08_p24 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i cospi_p24_p08 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i cospi_m24_m08 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
  const __m256i cospi_m04_p28 = pair256_set_epi16(-cospi_4_64, cospi_28_64);
  const __m256i cospi_p28_p04 = pair256_set_epi16(cospi_28_64, cospi_4_64);
  const __m256i cospi_m28_m04 = pair256_set_epi16(-cospi_28_64, -cospi_4_64);
  const __m256i cospi_m20_p12 = pair256_set_epi16(-cospi_20_64, cospi_12_64);
  const __m256i cospi_p12_p20 = pair256_set_epi16(cospi_12_64, cospi_20_64);
  const __m256i cospi_m12_m20 = pair256_set_epi16(-cospi_12_64, -cospi_20_64);

  const __m256i cospi_p31_p01 = pair256_set_epi16(cospi_31_64, cospi_1_64);
  const __m256i cospi_m01_p31 = pair256_set_epi16(-cospi_1_64, cospi_31_64);
  const __m256i cospi_p15_p17 = pair256_set_epi16(cospi_15_64, cospi_17_64);
  const __m256i cospi_m17_p15 = pair256_set_epi16(-cospi_17_64, cospi_15_64);
  const __m256i cospi_p23_p09 = pair256_set_epi16(cospi_23_64, cospi_9_64);
  const __m256i cospi_m09_p23 = pair256_set_epi16(-cospi_9_64, cospi_23_64);
  const __m256i cospi_p07_p25 = pair256_set_epi16(cospi_7_64, cospi_25_64);
  const __m256i cospi_m25_p07 = pair256_set_epi16(-cospi_25_64, cospi_7_64);
  const __m256i cospi_p27_p05 = pair256_set_epi16(cospi_27_64, cospi_5_64);
  const __m256i cospi_m05_p27 = pair256_set_epi16(-cospi_5_64, cospi_27_64);
  const __m256i cospi_p11_p21 = pair256_set_epi16(cospi_11_64, cospi_21_64);
  const __m256i cospi_m21_p11 = pair256_set_epi16(-cospi_21_64, cospi_11_64);
  const __m256i cospi_p19_p13 = pair256_set_epi16(cospi_19_64, cospi_13_64);
  const __m256i cospi_m13_p19 = pair256_set_epi16(-cospi_13_64, cospi_19_64);
  const __m256i cospi_p03_p29 = pair256_set_epi16(cospi_3_64, cospi_29_64);
  const __m256i cospi_m29_p03 = pair256_set_epi16(-cospi_29_64, cospi_3_64);

  __m256i x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
  __m256i y0, y1, y2, y3, y4, y5, y6, y7, y8, y9, y10, y11, y12, y13, y14, y15;
  __m256i u0, u1;

  // stage 1 is in prepare_16x16_odd()

  // stage 2
  y0 = in[0];
  y1 = in[1];
  y2 = in[2];
  y3 = in[3];

  u0 = _mm256_unpacklo_epi16(in[4], in[11]);
  u1 = _mm256_unpackhi_epi16(in[4], in[11]);
  y4 = butter_fly(&u0, &u1, &cospi_m16_p16);
  y11 = butter_fly(&u0, &u1, &cospi_p16_p16);

  u0 = _mm256_unpacklo_epi16(in[5], in[10]);
  u1 = _mm256_unpackhi_epi16(in[5], in[10]);
  y5 = butter_fly(&u0, &u1, &cospi_m16_p16);
  y10 = butter_fly(&u0, &u1, &cospi_p16_p16);

  u0 = _mm256_unpacklo_epi16(in[6], in[9]);
  u1 = _mm256_unpackhi_epi16(in[6], in[9]);
  y6 = butter_fly(&u0, &u1, &cospi_m16_p16);
  y9 = butter_fly(&u0, &u1, &cospi_p16_p16);

  u0 = _mm256_unpacklo_epi16(in[7], in[8]);
  u1 = _mm256_unpackhi_epi16(in[7], in[8]);
  y7 = butter_fly(&u0, &u1, &cospi_m16_p16);
  y8 = butter_fly(&u0, &u1, &cospi_p16_p16);

  y12 = in[12];
  y13 = in[13];
  y14 = in[14];
  y15 = in[15];

  // stage 3
  x0 = _mm256_add_epi16(y0, y7);
  x1 = _mm256_add_epi16(y1, y6);
  x2 = _mm256_add_epi16(y2, y5);
  x3 = _mm256_add_epi16(y3, y4);
  x4 = _mm256_sub_epi16(y3, y4);
  x5 = _mm256_sub_epi16(y2, y5);
  x6 = _mm256_sub_epi16(y1, y6);
  x7 = _mm256_sub_epi16(y0, y7);
  x8 = _mm256_sub_epi16(y15, y8);
  x9 = _mm256_sub_epi16(y14, y9);
  x10 = _mm256_sub_epi16(y13, y10);
  x11 = _mm256_sub_epi16(y12, y11);
  x12 = _mm256_add_epi16(y12, y11);
  x13 = _mm256_add_epi16(y13, y10);
  x14 = _mm256_add_epi16(y14, y9);
  x15 = _mm256_add_epi16(y15, y8);

  // stage 4
  y0 = x0;
  y1 = x1;
  y6 = x6;
  y7 = x7;
  y8 = x8;
  y9 = x9;
  y14 = x14;
  y15 = x15;

  u0 = _mm256_unpacklo_epi16(x2, x13);
  u1 = _mm256_unpackhi_epi16(x2, x13);
  y2 = butter_fly(&u0, &u1, &cospi_m08_p24);
  y13 = butter_fly(&u0, &u1, &cospi_p24_p08);

  u0 = _mm256_unpacklo_epi16(x3, x12);
  u1 = _mm256_unpackhi_epi16(x3, x12);
  y3 = butter_fly(&u0, &u1, &cospi_m08_p24);
  y12 = butter_fly(&u0, &u1, &cospi_p24_p08);

  u0 = _mm256_unpacklo_epi16(x4, x11);
  u1 = _mm256_unpackhi_epi16(x4, x11);
  y4 = butter_fly(&u0, &u1, &cospi_m24_m08);
  y11 = butter_fly(&u0, &u1, &cospi_m08_p24);

  u0 = _mm256_unpacklo_epi16(x5, x10);
  u1 = _mm256_unpackhi_epi16(x5, x10);
  y5 = butter_fly(&u0, &u1, &cospi_m24_m08);
  y10 = butter_fly(&u0, &u1, &cospi_m08_p24);

  // stage 5
  x0 = _mm256_add_epi16(y0, y3);
  x1 = _mm256_add_epi16(y1, y2);
  x2 = _mm256_sub_epi16(y1, y2);
  x3 = _mm256_sub_epi16(y0, y3);
  x4 = _mm256_sub_epi16(y7, y4);
  x5 = _mm256_sub_epi16(y6, y5);
  x6 = _mm256_add_epi16(y6, y5);
  x7 = _mm256_add_epi16(y7, y4);

  x8 = _mm256_add_epi16(y8, y11);
  x9 = _mm256_add_epi16(y9, y10);
  x10 = _mm256_sub_epi16(y9, y10);
  x11 = _mm256_sub_epi16(y8, y11);
  x12 = _mm256_sub_epi16(y15, y12);
  x13 = _mm256_sub_epi16(y14, y13);
  x14 = _mm256_add_epi16(y14, y13);
  x15 = _mm256_add_epi16(y15, y12);

  // stage 6
  y0 = x0;
  y3 = x3;
  y4 = x4;
  y7 = x7;
  y8 = x8;
  y11 = x11;
  y12 = x12;
  y15 = x15;

  u0 = _mm256_unpacklo_epi16(x1, x14);
  u1 = _mm256_unpackhi_epi16(x1, x14);
  y1 = butter_fly(&u0, &u1, &cospi_m04_p28);
  y14 = butter_fly(&u0, &u1, &cospi_p28_p04);

  u0 = _mm256_unpacklo_epi16(x2, x13);
  u1 = _mm256_unpackhi_epi16(x2, x13);
  y2 = butter_fly(&u0, &u1, &cospi_m28_m04);
  y13 = butter_fly(&u0, &u1, &cospi_m04_p28);

  u0 = _mm256_unpacklo_epi16(x5, x10);
  u1 = _mm256_unpackhi_epi16(x5, x10);
  y5 = butter_fly(&u0, &u1, &cospi_m20_p12);
  y10 = butter_fly(&u0, &u1, &cospi_p12_p20);

  u0 = _mm256_unpacklo_epi16(x6, x9);
  u1 = _mm256_unpackhi_epi16(x6, x9);
  y6 = butter_fly(&u0, &u1, &cospi_m12_m20);
  y9 = butter_fly(&u0, &u1, &cospi_m20_p12);

  // stage 7
  x0 = _mm256_add_epi16(y0, y1);
  x1 = _mm256_sub_epi16(y0, y1);
  x2 = _mm256_sub_epi16(y3, y2);
  x3 = _mm256_add_epi16(y3, y2);
  x4 = _mm256_add_epi16(y4, y5);
  x5 = _mm256_sub_epi16(y4, y5);
  x6 = _mm256_sub_epi16(y7, y6);
  x7 = _mm256_add_epi16(y7, y6);

  x8 = _mm256_add_epi16(y8, y9);
  x9 = _mm256_sub_epi16(y8, y9);
  x10 = _mm256_sub_epi16(y11, y10);
  x11 = _mm256_add_epi16(y11, y10);
  x12 = _mm256_add_epi16(y12, y13);
  x13 = _mm256_sub_epi16(y12, y13);
  x14 = _mm256_sub_epi16(y15, y14);
  x15 = _mm256_add_epi16(y15, y14);

  // stage 8
  u0 = _mm256_unpacklo_epi16(x0, x15);
  u1 = _mm256_unpackhi_epi16(x0, x15);
  in[0] = butter_fly(&u0, &u1, &cospi_p31_p01);
  in[15] = butter_fly(&u0, &u1, &cospi_m01_p31);

  u0 = _mm256_unpacklo_epi16(x1, x14);
  u1 = _mm256_unpackhi_epi16(x1, x14);
  in[1] = butter_fly(&u0, &u1, &cospi_p15_p17);
  in[14] = butter_fly(&u0, &u1, &cospi_m17_p15);

  u0 = _mm256_unpacklo_epi16(x2, x13);
  u1 = _mm256_unpackhi_epi16(x2, x13);
  in[2] = butter_fly(&u0, &u1, &cospi_p23_p09);
  in[13] = butter_fly(&u0, &u1, &cospi_m09_p23);

  u0 = _mm256_unpacklo_epi16(x3, x12);
  u1 = _mm256_unpackhi_epi16(x3, x12);
  in[3] = butter_fly(&u0, &u1, &cospi_p07_p25);
  in[12] = butter_fly(&u0, &u1, &cospi_m25_p07);

  u0 = _mm256_unpacklo_epi16(x4, x11);
  u1 = _mm256_unpackhi_epi16(x4, x11);
  in[4] = butter_fly(&u0, &u1, &cospi_p27_p05);
  in[11] = butter_fly(&u0, &u1, &cospi_m05_p27);

  u0 = _mm256_unpacklo_epi16(x5, x10);
  u1 = _mm256_unpackhi_epi16(x5, x10);
  in[5] = butter_fly(&u0, &u1, &cospi_p11_p21);
  in[10] = butter_fly(&u0, &u1, &cospi_m21_p11);

  u0 = _mm256_unpacklo_epi16(x6, x9);
  u1 = _mm256_unpackhi_epi16(x6, x9);
  in[6] = butter_fly(&u0, &u1, &cospi_p19_p13);
  in[9] = butter_fly(&u0, &u1, &cospi_m13_p19);

  u0 = _mm256_unpacklo_epi16(x7, x8);
  u1 = _mm256_unpackhi_epi16(x7, x8);
  in[7] = butter_fly(&u0, &u1, &cospi_p03_p29);
  in[8] = butter_fly(&u0, &u1, &cospi_m29_p03);
}

static void fdct32_avx2(__m256i *in0, __m256i *in1) {
  __m256i even0[16], even1[16], odd0[16], odd1[16];
  prepare_16x16_even(in0, even0);
  fdct16_avx2(even0);

  prepare_16x16_odd(in0, odd0);
  fdct16_odd_avx2(odd0);

  prepare_16x16_even(in1, even1);
  fdct16_avx2(even1);

  prepare_16x16_odd(in1, odd1);
  fdct16_odd_avx2(odd1);

  collect_coeffs(even0, odd0, even1, odd1, in0, in1);

  mm256_transpose_32x32(in0, in1);
}

static INLINE void write_buffer_32x32(const __m256i *in0, const __m256i *in1,
                                      tran_low_t *output) {
  int i = 0;
  const int stride = 32;
  tran_low_t *coeff = output;
  while (i < 32) {
    storeu_output_avx2(&in0[i], coeff);
    storeu_output_avx2(&in1[i], coeff + 16);
    coeff += stride;
    i += 1;
  }
}

#if CONFIG_EXT_TX
static void fhalfright32_16col_avx2(__m256i *in) {
  int i = 0;
  const __m256i zero = _mm256_setzero_si256();
  const __m256i sqrt2 = _mm256_set1_epi16((int16_t)Sqrt2);
  const __m256i dct_rounding = _mm256_set1_epi32(DCT_CONST_ROUNDING);
  __m256i x0, x1;

  while (i < 16) {
    in[i] = _mm256_slli_epi16(in[i], 2);
    x0 = _mm256_unpacklo_epi16(in[i + 16], zero);
    x1 = _mm256_unpackhi_epi16(in[i + 16], zero);
    x0 = _mm256_madd_epi16(x0, sqrt2);
    x1 = _mm256_madd_epi16(x1, sqrt2);
    x0 = _mm256_add_epi32(x0, dct_rounding);
    x1 = _mm256_add_epi32(x1, dct_rounding);
    x0 = _mm256_srai_epi32(x0, DCT_CONST_BITS);
    x1 = _mm256_srai_epi32(x1, DCT_CONST_BITS);
    in[i + 16] = _mm256_packs_epi32(x0, x1);
    i += 1;
  }
  fdct16_avx2(&in[16]);
}

static void fhalfright32_avx2(__m256i *in0, __m256i *in1) {
  fhalfright32_16col_avx2(in0);
  fhalfright32_16col_avx2(in1);
  mm256_vectors_swap(in0, &in0[16], 16);
  mm256_vectors_swap(in1, &in1[16], 16);
  mm256_transpose_32x32(in0, in1);
}
#endif  // CONFIG_EXT_TX

static INLINE void load_buffer_32x32(const int16_t *input, int stride,
                                     int flipud, int fliplr, __m256i *in0,
                                     __m256i *in1) {
  // Load 4 16x16 blocks
  const int16_t *topL = input;
  const int16_t *topR = input + 16;
  const int16_t *botL = input + 16 * stride;
  const int16_t *botR = input + 16 * stride + 16;

  const int16_t *tmp;

  if (flipud) {
    // Swap left columns
    tmp = topL;
    topL = botL;
    botL = tmp;
    // Swap right columns
    tmp = topR;
    topR = botR;
    botR = tmp;
  }

  if (fliplr) {
    // Swap top rows
    tmp = topL;
    topL = topR;
    topR = tmp;
    // Swap bottom rows
    tmp = botL;
    botL = botR;
    botR = tmp;
  }

  // load first 16 columns
  load_buffer_16x16(topL, stride, flipud, fliplr, in0);
  load_buffer_16x16(botL, stride, flipud, fliplr, in0 + 16);

  // load second 16 columns
  load_buffer_16x16(topR, stride, flipud, fliplr, in1);
  load_buffer_16x16(botR, stride, flipud, fliplr, in1 + 16);
}

static INLINE void right_shift_32x32_16col(int bit, __m256i *in) {
  int i = 0;
  const __m256i rounding = _mm256_set1_epi16((1 << bit) >> 1);
  __m256i sign;
  while (i < 32) {
    sign = _mm256_srai_epi16(in[i], 15);
    in[i] = _mm256_add_epi16(in[i], rounding);
    in[i] = _mm256_add_epi16(in[i], sign);
    in[i] = _mm256_srai_epi16(in[i], bit);
    i += 1;
  }
}

// Positive rounding
static INLINE void right_shift_32x32(__m256i *in0, __m256i *in1) {
  const int bit = 4;
  right_shift_32x32_16col(bit, in0);
  right_shift_32x32_16col(bit, in1);
}

#if CONFIG_EXT_TX
static void fidtx32_avx2(__m256i *in0, __m256i *in1) {
  int i = 0;
  while (i < 32) {
    in0[i] = _mm256_slli_epi16(in0[i], 2);
    in1[i] = _mm256_slli_epi16(in1[i], 2);
    i += 1;
  }
  mm256_transpose_32x32(in0, in1);
}
#endif

void av1_fht32x32_avx2(const int16_t *input, tran_low_t *output, int stride,
                       TxfmParam *txfm_param) {
  __m256i in0[32];  // left 32 columns
  __m256i in1[32];  // right 32 columns
  int tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "No avx2 32x32 implementation of MRC_DCT");
#endif

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fdct32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fdct32_avx2(in0, in1);
      break;
#if CONFIG_EXT_TX
    case ADST_DCT:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fhalfright32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fdct32_avx2(in0, in1);
      break;
    case DCT_ADST:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fdct32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fhalfright32_avx2(in0, in1);
      break;
    case ADST_ADST:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fhalfright32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fhalfright32_avx2(in0, in1);
      break;
    case FLIPADST_DCT:
      load_buffer_32x32(input, stride, 1, 0, in0, in1);
      fhalfright32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fdct32_avx2(in0, in1);
      break;
    case DCT_FLIPADST:
      load_buffer_32x32(input, stride, 0, 1, in0, in1);
      fdct32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fhalfright32_avx2(in0, in1);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_32x32(input, stride, 1, 1, in0, in1);
      fhalfright32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fhalfright32_avx2(in0, in1);
      break;
    case ADST_FLIPADST:
      load_buffer_32x32(input, stride, 0, 1, in0, in1);
      fhalfright32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fhalfright32_avx2(in0, in1);
      break;
    case FLIPADST_ADST:
      load_buffer_32x32(input, stride, 1, 0, in0, in1);
      fhalfright32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fhalfright32_avx2(in0, in1);
      break;
    case IDTX:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fidtx32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fidtx32_avx2(in0, in1);
      break;
    case V_DCT:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fdct32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fidtx32_avx2(in0, in1);
      break;
    case H_DCT:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fidtx32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fdct32_avx2(in0, in1);
      break;
    case V_ADST:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fhalfright32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fidtx32_avx2(in0, in1);
      break;
    case H_ADST:
      load_buffer_32x32(input, stride, 0, 0, in0, in1);
      fidtx32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fhalfright32_avx2(in0, in1);
      break;
    case V_FLIPADST:
      load_buffer_32x32(input, stride, 1, 0, in0, in1);
      fhalfright32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fidtx32_avx2(in0, in1);
      break;
    case H_FLIPADST:
      load_buffer_32x32(input, stride, 0, 1, in0, in1);
      fidtx32_avx2(in0, in1);
      right_shift_32x32(in0, in1);
      fhalfright32_avx2(in0, in1);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
  write_buffer_32x32(in0, in1, output);
  _mm256_zeroupper();
}
