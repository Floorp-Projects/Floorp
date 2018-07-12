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

#include <emmintrin.h>  // SSE2

#include "aom_dsp/fwd_txfm.h"
#include "aom_dsp/txfm_common.h"
#include "aom_dsp/x86/txfm_common_sse2.h"

// Apply a 32-element IDCT to 8 columns. This does not do any transposition
// of its output - the caller is expected to do that.
// The input buffers are the top and bottom halves of an 8x32 block.
void fdct32_8col(__m128i *in0, __m128i *in1) {
  // Constants
  //    When we use them, in one case, they are all the same. In all others
  //    it's a pair of them that we need to repeat four times. This is done
  //    by constructing the 32 bit constant corresponding to that pair.
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(+cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(+cospi_24_64, cospi_8_64);
  const __m128i k__cospi_p12_p20 = pair_set_epi16(+cospi_12_64, cospi_20_64);
  const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p28_p04 = pair_set_epi16(+cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m28_m04 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
  const __m128i k__cospi_m12_m20 = pair_set_epi16(-cospi_12_64, -cospi_20_64);
  const __m128i k__cospi_p30_p02 = pair_set_epi16(+cospi_30_64, cospi_2_64);
  const __m128i k__cospi_p14_p18 = pair_set_epi16(+cospi_14_64, cospi_18_64);
  const __m128i k__cospi_p22_p10 = pair_set_epi16(+cospi_22_64, cospi_10_64);
  const __m128i k__cospi_p06_p26 = pair_set_epi16(+cospi_6_64, cospi_26_64);
  const __m128i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
  const __m128i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
  const __m128i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
  const __m128i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
  const __m128i k__cospi_p31_p01 = pair_set_epi16(+cospi_31_64, cospi_1_64);
  const __m128i k__cospi_p15_p17 = pair_set_epi16(+cospi_15_64, cospi_17_64);
  const __m128i k__cospi_p23_p09 = pair_set_epi16(+cospi_23_64, cospi_9_64);
  const __m128i k__cospi_p07_p25 = pair_set_epi16(+cospi_7_64, cospi_25_64);
  const __m128i k__cospi_m25_p07 = pair_set_epi16(-cospi_25_64, cospi_7_64);
  const __m128i k__cospi_m09_p23 = pair_set_epi16(-cospi_9_64, cospi_23_64);
  const __m128i k__cospi_m17_p15 = pair_set_epi16(-cospi_17_64, cospi_15_64);
  const __m128i k__cospi_m01_p31 = pair_set_epi16(-cospi_1_64, cospi_31_64);
  const __m128i k__cospi_p27_p05 = pair_set_epi16(+cospi_27_64, cospi_5_64);
  const __m128i k__cospi_p11_p21 = pair_set_epi16(+cospi_11_64, cospi_21_64);
  const __m128i k__cospi_p19_p13 = pair_set_epi16(+cospi_19_64, cospi_13_64);
  const __m128i k__cospi_p03_p29 = pair_set_epi16(+cospi_3_64, cospi_29_64);
  const __m128i k__cospi_m29_p03 = pair_set_epi16(-cospi_29_64, cospi_3_64);
  const __m128i k__cospi_m13_p19 = pair_set_epi16(-cospi_13_64, cospi_19_64);
  const __m128i k__cospi_m21_p11 = pair_set_epi16(-cospi_21_64, cospi_11_64);
  const __m128i k__cospi_m05_p27 = pair_set_epi16(-cospi_5_64, cospi_27_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  __m128i step1[32];
  __m128i step2[32];
  __m128i step3[32];
  __m128i out[32];
  // Stage 1
  {
    const __m128i *ina = in0;
    const __m128i *inb = in1 + 15;
    __m128i *step1a = &step1[0];
    __m128i *step1b = &step1[31];
    const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
    const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + 1));
    const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + 2));
    const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + 3));
    const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - 3));
    const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - 2));
    const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - 1));
    const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
    step1a[0] = _mm_add_epi16(ina0, inb0);
    step1a[1] = _mm_add_epi16(ina1, inb1);
    step1a[2] = _mm_add_epi16(ina2, inb2);
    step1a[3] = _mm_add_epi16(ina3, inb3);
    step1b[-3] = _mm_sub_epi16(ina3, inb3);
    step1b[-2] = _mm_sub_epi16(ina2, inb2);
    step1b[-1] = _mm_sub_epi16(ina1, inb1);
    step1b[-0] = _mm_sub_epi16(ina0, inb0);
  }
  {
    const __m128i *ina = in0 + 4;
    const __m128i *inb = in1 + 11;
    __m128i *step1a = &step1[4];
    __m128i *step1b = &step1[27];
    const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
    const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + 1));
    const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + 2));
    const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + 3));
    const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - 3));
    const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - 2));
    const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - 1));
    const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
    step1a[0] = _mm_add_epi16(ina0, inb0);
    step1a[1] = _mm_add_epi16(ina1, inb1);
    step1a[2] = _mm_add_epi16(ina2, inb2);
    step1a[3] = _mm_add_epi16(ina3, inb3);
    step1b[-3] = _mm_sub_epi16(ina3, inb3);
    step1b[-2] = _mm_sub_epi16(ina2, inb2);
    step1b[-1] = _mm_sub_epi16(ina1, inb1);
    step1b[-0] = _mm_sub_epi16(ina0, inb0);
  }
  {
    const __m128i *ina = in0 + 8;
    const __m128i *inb = in1 + 7;
    __m128i *step1a = &step1[8];
    __m128i *step1b = &step1[23];
    const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
    const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + 1));
    const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + 2));
    const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + 3));
    const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - 3));
    const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - 2));
    const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - 1));
    const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
    step1a[0] = _mm_add_epi16(ina0, inb0);
    step1a[1] = _mm_add_epi16(ina1, inb1);
    step1a[2] = _mm_add_epi16(ina2, inb2);
    step1a[3] = _mm_add_epi16(ina3, inb3);
    step1b[-3] = _mm_sub_epi16(ina3, inb3);
    step1b[-2] = _mm_sub_epi16(ina2, inb2);
    step1b[-1] = _mm_sub_epi16(ina1, inb1);
    step1b[-0] = _mm_sub_epi16(ina0, inb0);
  }
  {
    const __m128i *ina = in0 + 12;
    const __m128i *inb = in1 + 3;
    __m128i *step1a = &step1[12];
    __m128i *step1b = &step1[19];
    const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
    const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + 1));
    const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + 2));
    const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + 3));
    const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - 3));
    const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - 2));
    const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - 1));
    const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
    step1a[0] = _mm_add_epi16(ina0, inb0);
    step1a[1] = _mm_add_epi16(ina1, inb1);
    step1a[2] = _mm_add_epi16(ina2, inb2);
    step1a[3] = _mm_add_epi16(ina3, inb3);
    step1b[-3] = _mm_sub_epi16(ina3, inb3);
    step1b[-2] = _mm_sub_epi16(ina2, inb2);
    step1b[-1] = _mm_sub_epi16(ina1, inb1);
    step1b[-0] = _mm_sub_epi16(ina0, inb0);
  }
  // Stage 2
  {
    step2[0] = _mm_add_epi16(step1[0], step1[15]);
    step2[1] = _mm_add_epi16(step1[1], step1[14]);
    step2[2] = _mm_add_epi16(step1[2], step1[13]);
    step2[3] = _mm_add_epi16(step1[3], step1[12]);
    step2[4] = _mm_add_epi16(step1[4], step1[11]);
    step2[5] = _mm_add_epi16(step1[5], step1[10]);
    step2[6] = _mm_add_epi16(step1[6], step1[9]);
    step2[7] = _mm_add_epi16(step1[7], step1[8]);
    step2[8] = _mm_sub_epi16(step1[7], step1[8]);
    step2[9] = _mm_sub_epi16(step1[6], step1[9]);
    step2[10] = _mm_sub_epi16(step1[5], step1[10]);
    step2[11] = _mm_sub_epi16(step1[4], step1[11]);
    step2[12] = _mm_sub_epi16(step1[3], step1[12]);
    step2[13] = _mm_sub_epi16(step1[2], step1[13]);
    step2[14] = _mm_sub_epi16(step1[1], step1[14]);
    step2[15] = _mm_sub_epi16(step1[0], step1[15]);
  }
  {
    const __m128i s2_20_0 = _mm_unpacklo_epi16(step1[27], step1[20]);
    const __m128i s2_20_1 = _mm_unpackhi_epi16(step1[27], step1[20]);
    const __m128i s2_21_0 = _mm_unpacklo_epi16(step1[26], step1[21]);
    const __m128i s2_21_1 = _mm_unpackhi_epi16(step1[26], step1[21]);
    const __m128i s2_22_0 = _mm_unpacklo_epi16(step1[25], step1[22]);
    const __m128i s2_22_1 = _mm_unpackhi_epi16(step1[25], step1[22]);
    const __m128i s2_23_0 = _mm_unpacklo_epi16(step1[24], step1[23]);
    const __m128i s2_23_1 = _mm_unpackhi_epi16(step1[24], step1[23]);
    const __m128i s2_20_2 = _mm_madd_epi16(s2_20_0, k__cospi_p16_m16);
    const __m128i s2_20_3 = _mm_madd_epi16(s2_20_1, k__cospi_p16_m16);
    const __m128i s2_21_2 = _mm_madd_epi16(s2_21_0, k__cospi_p16_m16);
    const __m128i s2_21_3 = _mm_madd_epi16(s2_21_1, k__cospi_p16_m16);
    const __m128i s2_22_2 = _mm_madd_epi16(s2_22_0, k__cospi_p16_m16);
    const __m128i s2_22_3 = _mm_madd_epi16(s2_22_1, k__cospi_p16_m16);
    const __m128i s2_23_2 = _mm_madd_epi16(s2_23_0, k__cospi_p16_m16);
    const __m128i s2_23_3 = _mm_madd_epi16(s2_23_1, k__cospi_p16_m16);
    const __m128i s2_24_2 = _mm_madd_epi16(s2_23_0, k__cospi_p16_p16);
    const __m128i s2_24_3 = _mm_madd_epi16(s2_23_1, k__cospi_p16_p16);
    const __m128i s2_25_2 = _mm_madd_epi16(s2_22_0, k__cospi_p16_p16);
    const __m128i s2_25_3 = _mm_madd_epi16(s2_22_1, k__cospi_p16_p16);
    const __m128i s2_26_2 = _mm_madd_epi16(s2_21_0, k__cospi_p16_p16);
    const __m128i s2_26_3 = _mm_madd_epi16(s2_21_1, k__cospi_p16_p16);
    const __m128i s2_27_2 = _mm_madd_epi16(s2_20_0, k__cospi_p16_p16);
    const __m128i s2_27_3 = _mm_madd_epi16(s2_20_1, k__cospi_p16_p16);
    // dct_const_round_shift
    const __m128i s2_20_4 = _mm_add_epi32(s2_20_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_20_5 = _mm_add_epi32(s2_20_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_21_4 = _mm_add_epi32(s2_21_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_21_5 = _mm_add_epi32(s2_21_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_22_4 = _mm_add_epi32(s2_22_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_22_5 = _mm_add_epi32(s2_22_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_23_4 = _mm_add_epi32(s2_23_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_23_5 = _mm_add_epi32(s2_23_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_24_4 = _mm_add_epi32(s2_24_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_24_5 = _mm_add_epi32(s2_24_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_25_4 = _mm_add_epi32(s2_25_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_25_5 = _mm_add_epi32(s2_25_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_26_4 = _mm_add_epi32(s2_26_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_26_5 = _mm_add_epi32(s2_26_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_27_4 = _mm_add_epi32(s2_27_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_27_5 = _mm_add_epi32(s2_27_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_20_6 = _mm_srai_epi32(s2_20_4, DCT_CONST_BITS);
    const __m128i s2_20_7 = _mm_srai_epi32(s2_20_5, DCT_CONST_BITS);
    const __m128i s2_21_6 = _mm_srai_epi32(s2_21_4, DCT_CONST_BITS);
    const __m128i s2_21_7 = _mm_srai_epi32(s2_21_5, DCT_CONST_BITS);
    const __m128i s2_22_6 = _mm_srai_epi32(s2_22_4, DCT_CONST_BITS);
    const __m128i s2_22_7 = _mm_srai_epi32(s2_22_5, DCT_CONST_BITS);
    const __m128i s2_23_6 = _mm_srai_epi32(s2_23_4, DCT_CONST_BITS);
    const __m128i s2_23_7 = _mm_srai_epi32(s2_23_5, DCT_CONST_BITS);
    const __m128i s2_24_6 = _mm_srai_epi32(s2_24_4, DCT_CONST_BITS);
    const __m128i s2_24_7 = _mm_srai_epi32(s2_24_5, DCT_CONST_BITS);
    const __m128i s2_25_6 = _mm_srai_epi32(s2_25_4, DCT_CONST_BITS);
    const __m128i s2_25_7 = _mm_srai_epi32(s2_25_5, DCT_CONST_BITS);
    const __m128i s2_26_6 = _mm_srai_epi32(s2_26_4, DCT_CONST_BITS);
    const __m128i s2_26_7 = _mm_srai_epi32(s2_26_5, DCT_CONST_BITS);
    const __m128i s2_27_6 = _mm_srai_epi32(s2_27_4, DCT_CONST_BITS);
    const __m128i s2_27_7 = _mm_srai_epi32(s2_27_5, DCT_CONST_BITS);
    // Combine
    step2[20] = _mm_packs_epi32(s2_20_6, s2_20_7);
    step2[21] = _mm_packs_epi32(s2_21_6, s2_21_7);
    step2[22] = _mm_packs_epi32(s2_22_6, s2_22_7);
    step2[23] = _mm_packs_epi32(s2_23_6, s2_23_7);
    step2[24] = _mm_packs_epi32(s2_24_6, s2_24_7);
    step2[25] = _mm_packs_epi32(s2_25_6, s2_25_7);
    step2[26] = _mm_packs_epi32(s2_26_6, s2_26_7);
    step2[27] = _mm_packs_epi32(s2_27_6, s2_27_7);
  }
  // Stage 3
  {
    step3[0] = _mm_add_epi16(step2[(8 - 1)], step2[0]);
    step3[1] = _mm_add_epi16(step2[(8 - 2)], step2[1]);
    step3[2] = _mm_add_epi16(step2[(8 - 3)], step2[2]);
    step3[3] = _mm_add_epi16(step2[(8 - 4)], step2[3]);
    step3[4] = _mm_sub_epi16(step2[(8 - 5)], step2[4]);
    step3[5] = _mm_sub_epi16(step2[(8 - 6)], step2[5]);
    step3[6] = _mm_sub_epi16(step2[(8 - 7)], step2[6]);
    step3[7] = _mm_sub_epi16(step2[(8 - 8)], step2[7]);
  }
  {
    const __m128i s3_10_0 = _mm_unpacklo_epi16(step2[13], step2[10]);
    const __m128i s3_10_1 = _mm_unpackhi_epi16(step2[13], step2[10]);
    const __m128i s3_11_0 = _mm_unpacklo_epi16(step2[12], step2[11]);
    const __m128i s3_11_1 = _mm_unpackhi_epi16(step2[12], step2[11]);
    const __m128i s3_10_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_m16);
    const __m128i s3_10_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_m16);
    const __m128i s3_11_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_m16);
    const __m128i s3_11_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_m16);
    const __m128i s3_12_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_p16);
    const __m128i s3_12_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_p16);
    const __m128i s3_13_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_p16);
    const __m128i s3_13_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_p16);
    // dct_const_round_shift
    const __m128i s3_10_4 = _mm_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_10_5 = _mm_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_11_4 = _mm_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_11_5 = _mm_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_12_4 = _mm_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_12_5 = _mm_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_13_4 = _mm_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_13_5 = _mm_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_10_6 = _mm_srai_epi32(s3_10_4, DCT_CONST_BITS);
    const __m128i s3_10_7 = _mm_srai_epi32(s3_10_5, DCT_CONST_BITS);
    const __m128i s3_11_6 = _mm_srai_epi32(s3_11_4, DCT_CONST_BITS);
    const __m128i s3_11_7 = _mm_srai_epi32(s3_11_5, DCT_CONST_BITS);
    const __m128i s3_12_6 = _mm_srai_epi32(s3_12_4, DCT_CONST_BITS);
    const __m128i s3_12_7 = _mm_srai_epi32(s3_12_5, DCT_CONST_BITS);
    const __m128i s3_13_6 = _mm_srai_epi32(s3_13_4, DCT_CONST_BITS);
    const __m128i s3_13_7 = _mm_srai_epi32(s3_13_5, DCT_CONST_BITS);
    // Combine
    step3[10] = _mm_packs_epi32(s3_10_6, s3_10_7);
    step3[11] = _mm_packs_epi32(s3_11_6, s3_11_7);
    step3[12] = _mm_packs_epi32(s3_12_6, s3_12_7);
    step3[13] = _mm_packs_epi32(s3_13_6, s3_13_7);
  }
  {
    step3[16] = _mm_add_epi16(step2[23], step1[16]);
    step3[17] = _mm_add_epi16(step2[22], step1[17]);
    step3[18] = _mm_add_epi16(step2[21], step1[18]);
    step3[19] = _mm_add_epi16(step2[20], step1[19]);
    step3[20] = _mm_sub_epi16(step1[19], step2[20]);
    step3[21] = _mm_sub_epi16(step1[18], step2[21]);
    step3[22] = _mm_sub_epi16(step1[17], step2[22]);
    step3[23] = _mm_sub_epi16(step1[16], step2[23]);
    step3[24] = _mm_sub_epi16(step1[31], step2[24]);
    step3[25] = _mm_sub_epi16(step1[30], step2[25]);
    step3[26] = _mm_sub_epi16(step1[29], step2[26]);
    step3[27] = _mm_sub_epi16(step1[28], step2[27]);
    step3[28] = _mm_add_epi16(step2[27], step1[28]);
    step3[29] = _mm_add_epi16(step2[26], step1[29]);
    step3[30] = _mm_add_epi16(step2[25], step1[30]);
    step3[31] = _mm_add_epi16(step2[24], step1[31]);
  }

  // Stage 4
  {
    step1[0] = _mm_add_epi16(step3[3], step3[0]);
    step1[1] = _mm_add_epi16(step3[2], step3[1]);
    step1[2] = _mm_sub_epi16(step3[1], step3[2]);
    step1[3] = _mm_sub_epi16(step3[0], step3[3]);
    step1[8] = _mm_add_epi16(step3[11], step2[8]);
    step1[9] = _mm_add_epi16(step3[10], step2[9]);
    step1[10] = _mm_sub_epi16(step2[9], step3[10]);
    step1[11] = _mm_sub_epi16(step2[8], step3[11]);
    step1[12] = _mm_sub_epi16(step2[15], step3[12]);
    step1[13] = _mm_sub_epi16(step2[14], step3[13]);
    step1[14] = _mm_add_epi16(step3[13], step2[14]);
    step1[15] = _mm_add_epi16(step3[12], step2[15]);
  }
  {
    const __m128i s1_05_0 = _mm_unpacklo_epi16(step3[6], step3[5]);
    const __m128i s1_05_1 = _mm_unpackhi_epi16(step3[6], step3[5]);
    const __m128i s1_05_2 = _mm_madd_epi16(s1_05_0, k__cospi_p16_m16);
    const __m128i s1_05_3 = _mm_madd_epi16(s1_05_1, k__cospi_p16_m16);
    const __m128i s1_06_2 = _mm_madd_epi16(s1_05_0, k__cospi_p16_p16);
    const __m128i s1_06_3 = _mm_madd_epi16(s1_05_1, k__cospi_p16_p16);
    // dct_const_round_shift
    const __m128i s1_05_4 = _mm_add_epi32(s1_05_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_05_5 = _mm_add_epi32(s1_05_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_06_4 = _mm_add_epi32(s1_06_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_06_5 = _mm_add_epi32(s1_06_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_05_6 = _mm_srai_epi32(s1_05_4, DCT_CONST_BITS);
    const __m128i s1_05_7 = _mm_srai_epi32(s1_05_5, DCT_CONST_BITS);
    const __m128i s1_06_6 = _mm_srai_epi32(s1_06_4, DCT_CONST_BITS);
    const __m128i s1_06_7 = _mm_srai_epi32(s1_06_5, DCT_CONST_BITS);
    // Combine
    step1[5] = _mm_packs_epi32(s1_05_6, s1_05_7);
    step1[6] = _mm_packs_epi32(s1_06_6, s1_06_7);
  }
  {
    const __m128i s1_18_0 = _mm_unpacklo_epi16(step3[18], step3[29]);
    const __m128i s1_18_1 = _mm_unpackhi_epi16(step3[18], step3[29]);
    const __m128i s1_19_0 = _mm_unpacklo_epi16(step3[19], step3[28]);
    const __m128i s1_19_1 = _mm_unpackhi_epi16(step3[19], step3[28]);
    const __m128i s1_20_0 = _mm_unpacklo_epi16(step3[20], step3[27]);
    const __m128i s1_20_1 = _mm_unpackhi_epi16(step3[20], step3[27]);
    const __m128i s1_21_0 = _mm_unpacklo_epi16(step3[21], step3[26]);
    const __m128i s1_21_1 = _mm_unpackhi_epi16(step3[21], step3[26]);
    const __m128i s1_18_2 = _mm_madd_epi16(s1_18_0, k__cospi_m08_p24);
    const __m128i s1_18_3 = _mm_madd_epi16(s1_18_1, k__cospi_m08_p24);
    const __m128i s1_19_2 = _mm_madd_epi16(s1_19_0, k__cospi_m08_p24);
    const __m128i s1_19_3 = _mm_madd_epi16(s1_19_1, k__cospi_m08_p24);
    const __m128i s1_20_2 = _mm_madd_epi16(s1_20_0, k__cospi_m24_m08);
    const __m128i s1_20_3 = _mm_madd_epi16(s1_20_1, k__cospi_m24_m08);
    const __m128i s1_21_2 = _mm_madd_epi16(s1_21_0, k__cospi_m24_m08);
    const __m128i s1_21_3 = _mm_madd_epi16(s1_21_1, k__cospi_m24_m08);
    const __m128i s1_26_2 = _mm_madd_epi16(s1_21_0, k__cospi_m08_p24);
    const __m128i s1_26_3 = _mm_madd_epi16(s1_21_1, k__cospi_m08_p24);
    const __m128i s1_27_2 = _mm_madd_epi16(s1_20_0, k__cospi_m08_p24);
    const __m128i s1_27_3 = _mm_madd_epi16(s1_20_1, k__cospi_m08_p24);
    const __m128i s1_28_2 = _mm_madd_epi16(s1_19_0, k__cospi_p24_p08);
    const __m128i s1_28_3 = _mm_madd_epi16(s1_19_1, k__cospi_p24_p08);
    const __m128i s1_29_2 = _mm_madd_epi16(s1_18_0, k__cospi_p24_p08);
    const __m128i s1_29_3 = _mm_madd_epi16(s1_18_1, k__cospi_p24_p08);
    // dct_const_round_shift
    const __m128i s1_18_4 = _mm_add_epi32(s1_18_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_18_5 = _mm_add_epi32(s1_18_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_19_4 = _mm_add_epi32(s1_19_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_19_5 = _mm_add_epi32(s1_19_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_20_4 = _mm_add_epi32(s1_20_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_20_5 = _mm_add_epi32(s1_20_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_21_4 = _mm_add_epi32(s1_21_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_21_5 = _mm_add_epi32(s1_21_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_26_4 = _mm_add_epi32(s1_26_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_26_5 = _mm_add_epi32(s1_26_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_27_4 = _mm_add_epi32(s1_27_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_27_5 = _mm_add_epi32(s1_27_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_28_4 = _mm_add_epi32(s1_28_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_28_5 = _mm_add_epi32(s1_28_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_29_4 = _mm_add_epi32(s1_29_2, k__DCT_CONST_ROUNDING);
    const __m128i s1_29_5 = _mm_add_epi32(s1_29_3, k__DCT_CONST_ROUNDING);
    const __m128i s1_18_6 = _mm_srai_epi32(s1_18_4, DCT_CONST_BITS);
    const __m128i s1_18_7 = _mm_srai_epi32(s1_18_5, DCT_CONST_BITS);
    const __m128i s1_19_6 = _mm_srai_epi32(s1_19_4, DCT_CONST_BITS);
    const __m128i s1_19_7 = _mm_srai_epi32(s1_19_5, DCT_CONST_BITS);
    const __m128i s1_20_6 = _mm_srai_epi32(s1_20_4, DCT_CONST_BITS);
    const __m128i s1_20_7 = _mm_srai_epi32(s1_20_5, DCT_CONST_BITS);
    const __m128i s1_21_6 = _mm_srai_epi32(s1_21_4, DCT_CONST_BITS);
    const __m128i s1_21_7 = _mm_srai_epi32(s1_21_5, DCT_CONST_BITS);
    const __m128i s1_26_6 = _mm_srai_epi32(s1_26_4, DCT_CONST_BITS);
    const __m128i s1_26_7 = _mm_srai_epi32(s1_26_5, DCT_CONST_BITS);
    const __m128i s1_27_6 = _mm_srai_epi32(s1_27_4, DCT_CONST_BITS);
    const __m128i s1_27_7 = _mm_srai_epi32(s1_27_5, DCT_CONST_BITS);
    const __m128i s1_28_6 = _mm_srai_epi32(s1_28_4, DCT_CONST_BITS);
    const __m128i s1_28_7 = _mm_srai_epi32(s1_28_5, DCT_CONST_BITS);
    const __m128i s1_29_6 = _mm_srai_epi32(s1_29_4, DCT_CONST_BITS);
    const __m128i s1_29_7 = _mm_srai_epi32(s1_29_5, DCT_CONST_BITS);
    // Combine
    step1[18] = _mm_packs_epi32(s1_18_6, s1_18_7);
    step1[19] = _mm_packs_epi32(s1_19_6, s1_19_7);
    step1[20] = _mm_packs_epi32(s1_20_6, s1_20_7);
    step1[21] = _mm_packs_epi32(s1_21_6, s1_21_7);
    step1[26] = _mm_packs_epi32(s1_26_6, s1_26_7);
    step1[27] = _mm_packs_epi32(s1_27_6, s1_27_7);
    step1[28] = _mm_packs_epi32(s1_28_6, s1_28_7);
    step1[29] = _mm_packs_epi32(s1_29_6, s1_29_7);
  }
  // Stage 5
  {
    step2[4] = _mm_add_epi16(step1[5], step3[4]);
    step2[5] = _mm_sub_epi16(step3[4], step1[5]);
    step2[6] = _mm_sub_epi16(step3[7], step1[6]);
    step2[7] = _mm_add_epi16(step1[6], step3[7]);
  }
  {
    const __m128i out_00_0 = _mm_unpacklo_epi16(step1[0], step1[1]);
    const __m128i out_00_1 = _mm_unpackhi_epi16(step1[0], step1[1]);
    const __m128i out_08_0 = _mm_unpacklo_epi16(step1[2], step1[3]);
    const __m128i out_08_1 = _mm_unpackhi_epi16(step1[2], step1[3]);
    const __m128i out_00_2 = _mm_madd_epi16(out_00_0, k__cospi_p16_p16);
    const __m128i out_00_3 = _mm_madd_epi16(out_00_1, k__cospi_p16_p16);
    const __m128i out_16_2 = _mm_madd_epi16(out_00_0, k__cospi_p16_m16);
    const __m128i out_16_3 = _mm_madd_epi16(out_00_1, k__cospi_p16_m16);
    const __m128i out_08_2 = _mm_madd_epi16(out_08_0, k__cospi_p24_p08);
    const __m128i out_08_3 = _mm_madd_epi16(out_08_1, k__cospi_p24_p08);
    const __m128i out_24_2 = _mm_madd_epi16(out_08_0, k__cospi_m08_p24);
    const __m128i out_24_3 = _mm_madd_epi16(out_08_1, k__cospi_m08_p24);
    // dct_const_round_shift
    const __m128i out_00_4 = _mm_add_epi32(out_00_2, k__DCT_CONST_ROUNDING);
    const __m128i out_00_5 = _mm_add_epi32(out_00_3, k__DCT_CONST_ROUNDING);
    const __m128i out_16_4 = _mm_add_epi32(out_16_2, k__DCT_CONST_ROUNDING);
    const __m128i out_16_5 = _mm_add_epi32(out_16_3, k__DCT_CONST_ROUNDING);
    const __m128i out_08_4 = _mm_add_epi32(out_08_2, k__DCT_CONST_ROUNDING);
    const __m128i out_08_5 = _mm_add_epi32(out_08_3, k__DCT_CONST_ROUNDING);
    const __m128i out_24_4 = _mm_add_epi32(out_24_2, k__DCT_CONST_ROUNDING);
    const __m128i out_24_5 = _mm_add_epi32(out_24_3, k__DCT_CONST_ROUNDING);
    const __m128i out_00_6 = _mm_srai_epi32(out_00_4, DCT_CONST_BITS);
    const __m128i out_00_7 = _mm_srai_epi32(out_00_5, DCT_CONST_BITS);
    const __m128i out_16_6 = _mm_srai_epi32(out_16_4, DCT_CONST_BITS);
    const __m128i out_16_7 = _mm_srai_epi32(out_16_5, DCT_CONST_BITS);
    const __m128i out_08_6 = _mm_srai_epi32(out_08_4, DCT_CONST_BITS);
    const __m128i out_08_7 = _mm_srai_epi32(out_08_5, DCT_CONST_BITS);
    const __m128i out_24_6 = _mm_srai_epi32(out_24_4, DCT_CONST_BITS);
    const __m128i out_24_7 = _mm_srai_epi32(out_24_5, DCT_CONST_BITS);
    // Combine
    out[0] = _mm_packs_epi32(out_00_6, out_00_7);
    out[16] = _mm_packs_epi32(out_16_6, out_16_7);
    out[8] = _mm_packs_epi32(out_08_6, out_08_7);
    out[24] = _mm_packs_epi32(out_24_6, out_24_7);
  }
  {
    const __m128i s2_09_0 = _mm_unpacklo_epi16(step1[9], step1[14]);
    const __m128i s2_09_1 = _mm_unpackhi_epi16(step1[9], step1[14]);
    const __m128i s2_10_0 = _mm_unpacklo_epi16(step1[10], step1[13]);
    const __m128i s2_10_1 = _mm_unpackhi_epi16(step1[10], step1[13]);
    const __m128i s2_09_2 = _mm_madd_epi16(s2_09_0, k__cospi_m08_p24);
    const __m128i s2_09_3 = _mm_madd_epi16(s2_09_1, k__cospi_m08_p24);
    const __m128i s2_10_2 = _mm_madd_epi16(s2_10_0, k__cospi_m24_m08);
    const __m128i s2_10_3 = _mm_madd_epi16(s2_10_1, k__cospi_m24_m08);
    const __m128i s2_13_2 = _mm_madd_epi16(s2_10_0, k__cospi_m08_p24);
    const __m128i s2_13_3 = _mm_madd_epi16(s2_10_1, k__cospi_m08_p24);
    const __m128i s2_14_2 = _mm_madd_epi16(s2_09_0, k__cospi_p24_p08);
    const __m128i s2_14_3 = _mm_madd_epi16(s2_09_1, k__cospi_p24_p08);
    // dct_const_round_shift
    const __m128i s2_09_4 = _mm_add_epi32(s2_09_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_09_5 = _mm_add_epi32(s2_09_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_10_4 = _mm_add_epi32(s2_10_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_10_5 = _mm_add_epi32(s2_10_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_13_4 = _mm_add_epi32(s2_13_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_13_5 = _mm_add_epi32(s2_13_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_14_4 = _mm_add_epi32(s2_14_2, k__DCT_CONST_ROUNDING);
    const __m128i s2_14_5 = _mm_add_epi32(s2_14_3, k__DCT_CONST_ROUNDING);
    const __m128i s2_09_6 = _mm_srai_epi32(s2_09_4, DCT_CONST_BITS);
    const __m128i s2_09_7 = _mm_srai_epi32(s2_09_5, DCT_CONST_BITS);
    const __m128i s2_10_6 = _mm_srai_epi32(s2_10_4, DCT_CONST_BITS);
    const __m128i s2_10_7 = _mm_srai_epi32(s2_10_5, DCT_CONST_BITS);
    const __m128i s2_13_6 = _mm_srai_epi32(s2_13_4, DCT_CONST_BITS);
    const __m128i s2_13_7 = _mm_srai_epi32(s2_13_5, DCT_CONST_BITS);
    const __m128i s2_14_6 = _mm_srai_epi32(s2_14_4, DCT_CONST_BITS);
    const __m128i s2_14_7 = _mm_srai_epi32(s2_14_5, DCT_CONST_BITS);
    // Combine
    step2[9] = _mm_packs_epi32(s2_09_6, s2_09_7);
    step2[10] = _mm_packs_epi32(s2_10_6, s2_10_7);
    step2[13] = _mm_packs_epi32(s2_13_6, s2_13_7);
    step2[14] = _mm_packs_epi32(s2_14_6, s2_14_7);
  }
  {
    step2[16] = _mm_add_epi16(step1[19], step3[16]);
    step2[17] = _mm_add_epi16(step1[18], step3[17]);
    step2[18] = _mm_sub_epi16(step3[17], step1[18]);
    step2[19] = _mm_sub_epi16(step3[16], step1[19]);
    step2[20] = _mm_sub_epi16(step3[23], step1[20]);
    step2[21] = _mm_sub_epi16(step3[22], step1[21]);
    step2[22] = _mm_add_epi16(step1[21], step3[22]);
    step2[23] = _mm_add_epi16(step1[20], step3[23]);
    step2[24] = _mm_add_epi16(step1[27], step3[24]);
    step2[25] = _mm_add_epi16(step1[26], step3[25]);
    step2[26] = _mm_sub_epi16(step3[25], step1[26]);
    step2[27] = _mm_sub_epi16(step3[24], step1[27]);
    step2[28] = _mm_sub_epi16(step3[31], step1[28]);
    step2[29] = _mm_sub_epi16(step3[30], step1[29]);
    step2[30] = _mm_add_epi16(step1[29], step3[30]);
    step2[31] = _mm_add_epi16(step1[28], step3[31]);
  }
  // Stage 6
  {
    const __m128i out_04_0 = _mm_unpacklo_epi16(step2[4], step2[7]);
    const __m128i out_04_1 = _mm_unpackhi_epi16(step2[4], step2[7]);
    const __m128i out_20_0 = _mm_unpacklo_epi16(step2[5], step2[6]);
    const __m128i out_20_1 = _mm_unpackhi_epi16(step2[5], step2[6]);
    const __m128i out_12_0 = _mm_unpacklo_epi16(step2[5], step2[6]);
    const __m128i out_12_1 = _mm_unpackhi_epi16(step2[5], step2[6]);
    const __m128i out_28_0 = _mm_unpacklo_epi16(step2[4], step2[7]);
    const __m128i out_28_1 = _mm_unpackhi_epi16(step2[4], step2[7]);
    const __m128i out_04_2 = _mm_madd_epi16(out_04_0, k__cospi_p28_p04);
    const __m128i out_04_3 = _mm_madd_epi16(out_04_1, k__cospi_p28_p04);
    const __m128i out_20_2 = _mm_madd_epi16(out_20_0, k__cospi_p12_p20);
    const __m128i out_20_3 = _mm_madd_epi16(out_20_1, k__cospi_p12_p20);
    const __m128i out_12_2 = _mm_madd_epi16(out_12_0, k__cospi_m20_p12);
    const __m128i out_12_3 = _mm_madd_epi16(out_12_1, k__cospi_m20_p12);
    const __m128i out_28_2 = _mm_madd_epi16(out_28_0, k__cospi_m04_p28);
    const __m128i out_28_3 = _mm_madd_epi16(out_28_1, k__cospi_m04_p28);
    // dct_const_round_shift
    const __m128i out_04_4 = _mm_add_epi32(out_04_2, k__DCT_CONST_ROUNDING);
    const __m128i out_04_5 = _mm_add_epi32(out_04_3, k__DCT_CONST_ROUNDING);
    const __m128i out_20_4 = _mm_add_epi32(out_20_2, k__DCT_CONST_ROUNDING);
    const __m128i out_20_5 = _mm_add_epi32(out_20_3, k__DCT_CONST_ROUNDING);
    const __m128i out_12_4 = _mm_add_epi32(out_12_2, k__DCT_CONST_ROUNDING);
    const __m128i out_12_5 = _mm_add_epi32(out_12_3, k__DCT_CONST_ROUNDING);
    const __m128i out_28_4 = _mm_add_epi32(out_28_2, k__DCT_CONST_ROUNDING);
    const __m128i out_28_5 = _mm_add_epi32(out_28_3, k__DCT_CONST_ROUNDING);
    const __m128i out_04_6 = _mm_srai_epi32(out_04_4, DCT_CONST_BITS);
    const __m128i out_04_7 = _mm_srai_epi32(out_04_5, DCT_CONST_BITS);
    const __m128i out_20_6 = _mm_srai_epi32(out_20_4, DCT_CONST_BITS);
    const __m128i out_20_7 = _mm_srai_epi32(out_20_5, DCT_CONST_BITS);
    const __m128i out_12_6 = _mm_srai_epi32(out_12_4, DCT_CONST_BITS);
    const __m128i out_12_7 = _mm_srai_epi32(out_12_5, DCT_CONST_BITS);
    const __m128i out_28_6 = _mm_srai_epi32(out_28_4, DCT_CONST_BITS);
    const __m128i out_28_7 = _mm_srai_epi32(out_28_5, DCT_CONST_BITS);
    // Combine
    out[4] = _mm_packs_epi32(out_04_6, out_04_7);
    out[20] = _mm_packs_epi32(out_20_6, out_20_7);
    out[12] = _mm_packs_epi32(out_12_6, out_12_7);
    out[28] = _mm_packs_epi32(out_28_6, out_28_7);
  }
  {
    step3[8] = _mm_add_epi16(step2[9], step1[8]);
    step3[9] = _mm_sub_epi16(step1[8], step2[9]);
    step3[10] = _mm_sub_epi16(step1[11], step2[10]);
    step3[11] = _mm_add_epi16(step2[10], step1[11]);
    step3[12] = _mm_add_epi16(step2[13], step1[12]);
    step3[13] = _mm_sub_epi16(step1[12], step2[13]);
    step3[14] = _mm_sub_epi16(step1[15], step2[14]);
    step3[15] = _mm_add_epi16(step2[14], step1[15]);
  }
  {
    const __m128i s3_17_0 = _mm_unpacklo_epi16(step2[17], step2[30]);
    const __m128i s3_17_1 = _mm_unpackhi_epi16(step2[17], step2[30]);
    const __m128i s3_18_0 = _mm_unpacklo_epi16(step2[18], step2[29]);
    const __m128i s3_18_1 = _mm_unpackhi_epi16(step2[18], step2[29]);
    const __m128i s3_21_0 = _mm_unpacklo_epi16(step2[21], step2[26]);
    const __m128i s3_21_1 = _mm_unpackhi_epi16(step2[21], step2[26]);
    const __m128i s3_22_0 = _mm_unpacklo_epi16(step2[22], step2[25]);
    const __m128i s3_22_1 = _mm_unpackhi_epi16(step2[22], step2[25]);
    const __m128i s3_17_2 = _mm_madd_epi16(s3_17_0, k__cospi_m04_p28);
    const __m128i s3_17_3 = _mm_madd_epi16(s3_17_1, k__cospi_m04_p28);
    const __m128i s3_18_2 = _mm_madd_epi16(s3_18_0, k__cospi_m28_m04);
    const __m128i s3_18_3 = _mm_madd_epi16(s3_18_1, k__cospi_m28_m04);
    const __m128i s3_21_2 = _mm_madd_epi16(s3_21_0, k__cospi_m20_p12);
    const __m128i s3_21_3 = _mm_madd_epi16(s3_21_1, k__cospi_m20_p12);
    const __m128i s3_22_2 = _mm_madd_epi16(s3_22_0, k__cospi_m12_m20);
    const __m128i s3_22_3 = _mm_madd_epi16(s3_22_1, k__cospi_m12_m20);
    const __m128i s3_25_2 = _mm_madd_epi16(s3_22_0, k__cospi_m20_p12);
    const __m128i s3_25_3 = _mm_madd_epi16(s3_22_1, k__cospi_m20_p12);
    const __m128i s3_26_2 = _mm_madd_epi16(s3_21_0, k__cospi_p12_p20);
    const __m128i s3_26_3 = _mm_madd_epi16(s3_21_1, k__cospi_p12_p20);
    const __m128i s3_29_2 = _mm_madd_epi16(s3_18_0, k__cospi_m04_p28);
    const __m128i s3_29_3 = _mm_madd_epi16(s3_18_1, k__cospi_m04_p28);
    const __m128i s3_30_2 = _mm_madd_epi16(s3_17_0, k__cospi_p28_p04);
    const __m128i s3_30_3 = _mm_madd_epi16(s3_17_1, k__cospi_p28_p04);
    // dct_const_round_shift
    const __m128i s3_17_4 = _mm_add_epi32(s3_17_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_17_5 = _mm_add_epi32(s3_17_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_18_4 = _mm_add_epi32(s3_18_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_18_5 = _mm_add_epi32(s3_18_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_21_4 = _mm_add_epi32(s3_21_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_21_5 = _mm_add_epi32(s3_21_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_22_4 = _mm_add_epi32(s3_22_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_22_5 = _mm_add_epi32(s3_22_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_17_6 = _mm_srai_epi32(s3_17_4, DCT_CONST_BITS);
    const __m128i s3_17_7 = _mm_srai_epi32(s3_17_5, DCT_CONST_BITS);
    const __m128i s3_18_6 = _mm_srai_epi32(s3_18_4, DCT_CONST_BITS);
    const __m128i s3_18_7 = _mm_srai_epi32(s3_18_5, DCT_CONST_BITS);
    const __m128i s3_21_6 = _mm_srai_epi32(s3_21_4, DCT_CONST_BITS);
    const __m128i s3_21_7 = _mm_srai_epi32(s3_21_5, DCT_CONST_BITS);
    const __m128i s3_22_6 = _mm_srai_epi32(s3_22_4, DCT_CONST_BITS);
    const __m128i s3_22_7 = _mm_srai_epi32(s3_22_5, DCT_CONST_BITS);
    const __m128i s3_25_4 = _mm_add_epi32(s3_25_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_25_5 = _mm_add_epi32(s3_25_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_26_4 = _mm_add_epi32(s3_26_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_26_5 = _mm_add_epi32(s3_26_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_29_4 = _mm_add_epi32(s3_29_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_29_5 = _mm_add_epi32(s3_29_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_30_4 = _mm_add_epi32(s3_30_2, k__DCT_CONST_ROUNDING);
    const __m128i s3_30_5 = _mm_add_epi32(s3_30_3, k__DCT_CONST_ROUNDING);
    const __m128i s3_25_6 = _mm_srai_epi32(s3_25_4, DCT_CONST_BITS);
    const __m128i s3_25_7 = _mm_srai_epi32(s3_25_5, DCT_CONST_BITS);
    const __m128i s3_26_6 = _mm_srai_epi32(s3_26_4, DCT_CONST_BITS);
    const __m128i s3_26_7 = _mm_srai_epi32(s3_26_5, DCT_CONST_BITS);
    const __m128i s3_29_6 = _mm_srai_epi32(s3_29_4, DCT_CONST_BITS);
    const __m128i s3_29_7 = _mm_srai_epi32(s3_29_5, DCT_CONST_BITS);
    const __m128i s3_30_6 = _mm_srai_epi32(s3_30_4, DCT_CONST_BITS);
    const __m128i s3_30_7 = _mm_srai_epi32(s3_30_5, DCT_CONST_BITS);
    // Combine
    step3[17] = _mm_packs_epi32(s3_17_6, s3_17_7);
    step3[18] = _mm_packs_epi32(s3_18_6, s3_18_7);
    step3[21] = _mm_packs_epi32(s3_21_6, s3_21_7);
    step3[22] = _mm_packs_epi32(s3_22_6, s3_22_7);
    // Combine
    step3[25] = _mm_packs_epi32(s3_25_6, s3_25_7);
    step3[26] = _mm_packs_epi32(s3_26_6, s3_26_7);
    step3[29] = _mm_packs_epi32(s3_29_6, s3_29_7);
    step3[30] = _mm_packs_epi32(s3_30_6, s3_30_7);
  }
  // Stage 7
  {
    const __m128i out_02_0 = _mm_unpacklo_epi16(step3[8], step3[15]);
    const __m128i out_02_1 = _mm_unpackhi_epi16(step3[8], step3[15]);
    const __m128i out_18_0 = _mm_unpacklo_epi16(step3[9], step3[14]);
    const __m128i out_18_1 = _mm_unpackhi_epi16(step3[9], step3[14]);
    const __m128i out_10_0 = _mm_unpacklo_epi16(step3[10], step3[13]);
    const __m128i out_10_1 = _mm_unpackhi_epi16(step3[10], step3[13]);
    const __m128i out_26_0 = _mm_unpacklo_epi16(step3[11], step3[12]);
    const __m128i out_26_1 = _mm_unpackhi_epi16(step3[11], step3[12]);
    const __m128i out_02_2 = _mm_madd_epi16(out_02_0, k__cospi_p30_p02);
    const __m128i out_02_3 = _mm_madd_epi16(out_02_1, k__cospi_p30_p02);
    const __m128i out_18_2 = _mm_madd_epi16(out_18_0, k__cospi_p14_p18);
    const __m128i out_18_3 = _mm_madd_epi16(out_18_1, k__cospi_p14_p18);
    const __m128i out_10_2 = _mm_madd_epi16(out_10_0, k__cospi_p22_p10);
    const __m128i out_10_3 = _mm_madd_epi16(out_10_1, k__cospi_p22_p10);
    const __m128i out_26_2 = _mm_madd_epi16(out_26_0, k__cospi_p06_p26);
    const __m128i out_26_3 = _mm_madd_epi16(out_26_1, k__cospi_p06_p26);
    const __m128i out_06_2 = _mm_madd_epi16(out_26_0, k__cospi_m26_p06);
    const __m128i out_06_3 = _mm_madd_epi16(out_26_1, k__cospi_m26_p06);
    const __m128i out_22_2 = _mm_madd_epi16(out_10_0, k__cospi_m10_p22);
    const __m128i out_22_3 = _mm_madd_epi16(out_10_1, k__cospi_m10_p22);
    const __m128i out_14_2 = _mm_madd_epi16(out_18_0, k__cospi_m18_p14);
    const __m128i out_14_3 = _mm_madd_epi16(out_18_1, k__cospi_m18_p14);
    const __m128i out_30_2 = _mm_madd_epi16(out_02_0, k__cospi_m02_p30);
    const __m128i out_30_3 = _mm_madd_epi16(out_02_1, k__cospi_m02_p30);
    // dct_const_round_shift
    const __m128i out_02_4 = _mm_add_epi32(out_02_2, k__DCT_CONST_ROUNDING);
    const __m128i out_02_5 = _mm_add_epi32(out_02_3, k__DCT_CONST_ROUNDING);
    const __m128i out_18_4 = _mm_add_epi32(out_18_2, k__DCT_CONST_ROUNDING);
    const __m128i out_18_5 = _mm_add_epi32(out_18_3, k__DCT_CONST_ROUNDING);
    const __m128i out_10_4 = _mm_add_epi32(out_10_2, k__DCT_CONST_ROUNDING);
    const __m128i out_10_5 = _mm_add_epi32(out_10_3, k__DCT_CONST_ROUNDING);
    const __m128i out_26_4 = _mm_add_epi32(out_26_2, k__DCT_CONST_ROUNDING);
    const __m128i out_26_5 = _mm_add_epi32(out_26_3, k__DCT_CONST_ROUNDING);
    const __m128i out_06_4 = _mm_add_epi32(out_06_2, k__DCT_CONST_ROUNDING);
    const __m128i out_06_5 = _mm_add_epi32(out_06_3, k__DCT_CONST_ROUNDING);
    const __m128i out_22_4 = _mm_add_epi32(out_22_2, k__DCT_CONST_ROUNDING);
    const __m128i out_22_5 = _mm_add_epi32(out_22_3, k__DCT_CONST_ROUNDING);
    const __m128i out_14_4 = _mm_add_epi32(out_14_2, k__DCT_CONST_ROUNDING);
    const __m128i out_14_5 = _mm_add_epi32(out_14_3, k__DCT_CONST_ROUNDING);
    const __m128i out_30_4 = _mm_add_epi32(out_30_2, k__DCT_CONST_ROUNDING);
    const __m128i out_30_5 = _mm_add_epi32(out_30_3, k__DCT_CONST_ROUNDING);
    const __m128i out_02_6 = _mm_srai_epi32(out_02_4, DCT_CONST_BITS);
    const __m128i out_02_7 = _mm_srai_epi32(out_02_5, DCT_CONST_BITS);
    const __m128i out_18_6 = _mm_srai_epi32(out_18_4, DCT_CONST_BITS);
    const __m128i out_18_7 = _mm_srai_epi32(out_18_5, DCT_CONST_BITS);
    const __m128i out_10_6 = _mm_srai_epi32(out_10_4, DCT_CONST_BITS);
    const __m128i out_10_7 = _mm_srai_epi32(out_10_5, DCT_CONST_BITS);
    const __m128i out_26_6 = _mm_srai_epi32(out_26_4, DCT_CONST_BITS);
    const __m128i out_26_7 = _mm_srai_epi32(out_26_5, DCT_CONST_BITS);
    const __m128i out_06_6 = _mm_srai_epi32(out_06_4, DCT_CONST_BITS);
    const __m128i out_06_7 = _mm_srai_epi32(out_06_5, DCT_CONST_BITS);
    const __m128i out_22_6 = _mm_srai_epi32(out_22_4, DCT_CONST_BITS);
    const __m128i out_22_7 = _mm_srai_epi32(out_22_5, DCT_CONST_BITS);
    const __m128i out_14_6 = _mm_srai_epi32(out_14_4, DCT_CONST_BITS);
    const __m128i out_14_7 = _mm_srai_epi32(out_14_5, DCT_CONST_BITS);
    const __m128i out_30_6 = _mm_srai_epi32(out_30_4, DCT_CONST_BITS);
    const __m128i out_30_7 = _mm_srai_epi32(out_30_5, DCT_CONST_BITS);
    // Combine
    out[2] = _mm_packs_epi32(out_02_6, out_02_7);
    out[18] = _mm_packs_epi32(out_18_6, out_18_7);
    out[10] = _mm_packs_epi32(out_10_6, out_10_7);
    out[26] = _mm_packs_epi32(out_26_6, out_26_7);
    out[6] = _mm_packs_epi32(out_06_6, out_06_7);
    out[22] = _mm_packs_epi32(out_22_6, out_22_7);
    out[14] = _mm_packs_epi32(out_14_6, out_14_7);
    out[30] = _mm_packs_epi32(out_30_6, out_30_7);
  }
  {
    step1[16] = _mm_add_epi16(step3[17], step2[16]);
    step1[17] = _mm_sub_epi16(step2[16], step3[17]);
    step1[18] = _mm_sub_epi16(step2[19], step3[18]);
    step1[19] = _mm_add_epi16(step3[18], step2[19]);
    step1[20] = _mm_add_epi16(step3[21], step2[20]);
    step1[21] = _mm_sub_epi16(step2[20], step3[21]);
    step1[22] = _mm_sub_epi16(step2[23], step3[22]);
    step1[23] = _mm_add_epi16(step3[22], step2[23]);
    step1[24] = _mm_add_epi16(step3[25], step2[24]);
    step1[25] = _mm_sub_epi16(step2[24], step3[25]);
    step1[26] = _mm_sub_epi16(step2[27], step3[26]);
    step1[27] = _mm_add_epi16(step3[26], step2[27]);
    step1[28] = _mm_add_epi16(step3[29], step2[28]);
    step1[29] = _mm_sub_epi16(step2[28], step3[29]);
    step1[30] = _mm_sub_epi16(step2[31], step3[30]);
    step1[31] = _mm_add_epi16(step3[30], step2[31]);
  }
  // Final stage --- outputs indices are bit-reversed.
  {
    const __m128i out_01_0 = _mm_unpacklo_epi16(step1[16], step1[31]);
    const __m128i out_01_1 = _mm_unpackhi_epi16(step1[16], step1[31]);
    const __m128i out_17_0 = _mm_unpacklo_epi16(step1[17], step1[30]);
    const __m128i out_17_1 = _mm_unpackhi_epi16(step1[17], step1[30]);
    const __m128i out_09_0 = _mm_unpacklo_epi16(step1[18], step1[29]);
    const __m128i out_09_1 = _mm_unpackhi_epi16(step1[18], step1[29]);
    const __m128i out_25_0 = _mm_unpacklo_epi16(step1[19], step1[28]);
    const __m128i out_25_1 = _mm_unpackhi_epi16(step1[19], step1[28]);
    const __m128i out_01_2 = _mm_madd_epi16(out_01_0, k__cospi_p31_p01);
    const __m128i out_01_3 = _mm_madd_epi16(out_01_1, k__cospi_p31_p01);
    const __m128i out_17_2 = _mm_madd_epi16(out_17_0, k__cospi_p15_p17);
    const __m128i out_17_3 = _mm_madd_epi16(out_17_1, k__cospi_p15_p17);
    const __m128i out_09_2 = _mm_madd_epi16(out_09_0, k__cospi_p23_p09);
    const __m128i out_09_3 = _mm_madd_epi16(out_09_1, k__cospi_p23_p09);
    const __m128i out_25_2 = _mm_madd_epi16(out_25_0, k__cospi_p07_p25);
    const __m128i out_25_3 = _mm_madd_epi16(out_25_1, k__cospi_p07_p25);
    const __m128i out_07_2 = _mm_madd_epi16(out_25_0, k__cospi_m25_p07);
    const __m128i out_07_3 = _mm_madd_epi16(out_25_1, k__cospi_m25_p07);
    const __m128i out_23_2 = _mm_madd_epi16(out_09_0, k__cospi_m09_p23);
    const __m128i out_23_3 = _mm_madd_epi16(out_09_1, k__cospi_m09_p23);
    const __m128i out_15_2 = _mm_madd_epi16(out_17_0, k__cospi_m17_p15);
    const __m128i out_15_3 = _mm_madd_epi16(out_17_1, k__cospi_m17_p15);
    const __m128i out_31_2 = _mm_madd_epi16(out_01_0, k__cospi_m01_p31);
    const __m128i out_31_3 = _mm_madd_epi16(out_01_1, k__cospi_m01_p31);
    // dct_const_round_shift
    const __m128i out_01_4 = _mm_add_epi32(out_01_2, k__DCT_CONST_ROUNDING);
    const __m128i out_01_5 = _mm_add_epi32(out_01_3, k__DCT_CONST_ROUNDING);
    const __m128i out_17_4 = _mm_add_epi32(out_17_2, k__DCT_CONST_ROUNDING);
    const __m128i out_17_5 = _mm_add_epi32(out_17_3, k__DCT_CONST_ROUNDING);
    const __m128i out_09_4 = _mm_add_epi32(out_09_2, k__DCT_CONST_ROUNDING);
    const __m128i out_09_5 = _mm_add_epi32(out_09_3, k__DCT_CONST_ROUNDING);
    const __m128i out_25_4 = _mm_add_epi32(out_25_2, k__DCT_CONST_ROUNDING);
    const __m128i out_25_5 = _mm_add_epi32(out_25_3, k__DCT_CONST_ROUNDING);
    const __m128i out_07_4 = _mm_add_epi32(out_07_2, k__DCT_CONST_ROUNDING);
    const __m128i out_07_5 = _mm_add_epi32(out_07_3, k__DCT_CONST_ROUNDING);
    const __m128i out_23_4 = _mm_add_epi32(out_23_2, k__DCT_CONST_ROUNDING);
    const __m128i out_23_5 = _mm_add_epi32(out_23_3, k__DCT_CONST_ROUNDING);
    const __m128i out_15_4 = _mm_add_epi32(out_15_2, k__DCT_CONST_ROUNDING);
    const __m128i out_15_5 = _mm_add_epi32(out_15_3, k__DCT_CONST_ROUNDING);
    const __m128i out_31_4 = _mm_add_epi32(out_31_2, k__DCT_CONST_ROUNDING);
    const __m128i out_31_5 = _mm_add_epi32(out_31_3, k__DCT_CONST_ROUNDING);
    const __m128i out_01_6 = _mm_srai_epi32(out_01_4, DCT_CONST_BITS);
    const __m128i out_01_7 = _mm_srai_epi32(out_01_5, DCT_CONST_BITS);
    const __m128i out_17_6 = _mm_srai_epi32(out_17_4, DCT_CONST_BITS);
    const __m128i out_17_7 = _mm_srai_epi32(out_17_5, DCT_CONST_BITS);
    const __m128i out_09_6 = _mm_srai_epi32(out_09_4, DCT_CONST_BITS);
    const __m128i out_09_7 = _mm_srai_epi32(out_09_5, DCT_CONST_BITS);
    const __m128i out_25_6 = _mm_srai_epi32(out_25_4, DCT_CONST_BITS);
    const __m128i out_25_7 = _mm_srai_epi32(out_25_5, DCT_CONST_BITS);
    const __m128i out_07_6 = _mm_srai_epi32(out_07_4, DCT_CONST_BITS);
    const __m128i out_07_7 = _mm_srai_epi32(out_07_5, DCT_CONST_BITS);
    const __m128i out_23_6 = _mm_srai_epi32(out_23_4, DCT_CONST_BITS);
    const __m128i out_23_7 = _mm_srai_epi32(out_23_5, DCT_CONST_BITS);
    const __m128i out_15_6 = _mm_srai_epi32(out_15_4, DCT_CONST_BITS);
    const __m128i out_15_7 = _mm_srai_epi32(out_15_5, DCT_CONST_BITS);
    const __m128i out_31_6 = _mm_srai_epi32(out_31_4, DCT_CONST_BITS);
    const __m128i out_31_7 = _mm_srai_epi32(out_31_5, DCT_CONST_BITS);
    // Combine
    out[1] = _mm_packs_epi32(out_01_6, out_01_7);
    out[17] = _mm_packs_epi32(out_17_6, out_17_7);
    out[9] = _mm_packs_epi32(out_09_6, out_09_7);
    out[25] = _mm_packs_epi32(out_25_6, out_25_7);
    out[7] = _mm_packs_epi32(out_07_6, out_07_7);
    out[23] = _mm_packs_epi32(out_23_6, out_23_7);
    out[15] = _mm_packs_epi32(out_15_6, out_15_7);
    out[31] = _mm_packs_epi32(out_31_6, out_31_7);
  }
  {
    const __m128i out_05_0 = _mm_unpacklo_epi16(step1[20], step1[27]);
    const __m128i out_05_1 = _mm_unpackhi_epi16(step1[20], step1[27]);
    const __m128i out_21_0 = _mm_unpacklo_epi16(step1[21], step1[26]);
    const __m128i out_21_1 = _mm_unpackhi_epi16(step1[21], step1[26]);
    const __m128i out_13_0 = _mm_unpacklo_epi16(step1[22], step1[25]);
    const __m128i out_13_1 = _mm_unpackhi_epi16(step1[22], step1[25]);
    const __m128i out_29_0 = _mm_unpacklo_epi16(step1[23], step1[24]);
    const __m128i out_29_1 = _mm_unpackhi_epi16(step1[23], step1[24]);
    const __m128i out_05_2 = _mm_madd_epi16(out_05_0, k__cospi_p27_p05);
    const __m128i out_05_3 = _mm_madd_epi16(out_05_1, k__cospi_p27_p05);
    const __m128i out_21_2 = _mm_madd_epi16(out_21_0, k__cospi_p11_p21);
    const __m128i out_21_3 = _mm_madd_epi16(out_21_1, k__cospi_p11_p21);
    const __m128i out_13_2 = _mm_madd_epi16(out_13_0, k__cospi_p19_p13);
    const __m128i out_13_3 = _mm_madd_epi16(out_13_1, k__cospi_p19_p13);
    const __m128i out_29_2 = _mm_madd_epi16(out_29_0, k__cospi_p03_p29);
    const __m128i out_29_3 = _mm_madd_epi16(out_29_1, k__cospi_p03_p29);
    const __m128i out_03_2 = _mm_madd_epi16(out_29_0, k__cospi_m29_p03);
    const __m128i out_03_3 = _mm_madd_epi16(out_29_1, k__cospi_m29_p03);
    const __m128i out_19_2 = _mm_madd_epi16(out_13_0, k__cospi_m13_p19);
    const __m128i out_19_3 = _mm_madd_epi16(out_13_1, k__cospi_m13_p19);
    const __m128i out_11_2 = _mm_madd_epi16(out_21_0, k__cospi_m21_p11);
    const __m128i out_11_3 = _mm_madd_epi16(out_21_1, k__cospi_m21_p11);
    const __m128i out_27_2 = _mm_madd_epi16(out_05_0, k__cospi_m05_p27);
    const __m128i out_27_3 = _mm_madd_epi16(out_05_1, k__cospi_m05_p27);
    // dct_const_round_shift
    const __m128i out_05_4 = _mm_add_epi32(out_05_2, k__DCT_CONST_ROUNDING);
    const __m128i out_05_5 = _mm_add_epi32(out_05_3, k__DCT_CONST_ROUNDING);
    const __m128i out_21_4 = _mm_add_epi32(out_21_2, k__DCT_CONST_ROUNDING);
    const __m128i out_21_5 = _mm_add_epi32(out_21_3, k__DCT_CONST_ROUNDING);
    const __m128i out_13_4 = _mm_add_epi32(out_13_2, k__DCT_CONST_ROUNDING);
    const __m128i out_13_5 = _mm_add_epi32(out_13_3, k__DCT_CONST_ROUNDING);
    const __m128i out_29_4 = _mm_add_epi32(out_29_2, k__DCT_CONST_ROUNDING);
    const __m128i out_29_5 = _mm_add_epi32(out_29_3, k__DCT_CONST_ROUNDING);
    const __m128i out_03_4 = _mm_add_epi32(out_03_2, k__DCT_CONST_ROUNDING);
    const __m128i out_03_5 = _mm_add_epi32(out_03_3, k__DCT_CONST_ROUNDING);
    const __m128i out_19_4 = _mm_add_epi32(out_19_2, k__DCT_CONST_ROUNDING);
    const __m128i out_19_5 = _mm_add_epi32(out_19_3, k__DCT_CONST_ROUNDING);
    const __m128i out_11_4 = _mm_add_epi32(out_11_2, k__DCT_CONST_ROUNDING);
    const __m128i out_11_5 = _mm_add_epi32(out_11_3, k__DCT_CONST_ROUNDING);
    const __m128i out_27_4 = _mm_add_epi32(out_27_2, k__DCT_CONST_ROUNDING);
    const __m128i out_27_5 = _mm_add_epi32(out_27_3, k__DCT_CONST_ROUNDING);
    const __m128i out_05_6 = _mm_srai_epi32(out_05_4, DCT_CONST_BITS);
    const __m128i out_05_7 = _mm_srai_epi32(out_05_5, DCT_CONST_BITS);
    const __m128i out_21_6 = _mm_srai_epi32(out_21_4, DCT_CONST_BITS);
    const __m128i out_21_7 = _mm_srai_epi32(out_21_5, DCT_CONST_BITS);
    const __m128i out_13_6 = _mm_srai_epi32(out_13_4, DCT_CONST_BITS);
    const __m128i out_13_7 = _mm_srai_epi32(out_13_5, DCT_CONST_BITS);
    const __m128i out_29_6 = _mm_srai_epi32(out_29_4, DCT_CONST_BITS);
    const __m128i out_29_7 = _mm_srai_epi32(out_29_5, DCT_CONST_BITS);
    const __m128i out_03_6 = _mm_srai_epi32(out_03_4, DCT_CONST_BITS);
    const __m128i out_03_7 = _mm_srai_epi32(out_03_5, DCT_CONST_BITS);
    const __m128i out_19_6 = _mm_srai_epi32(out_19_4, DCT_CONST_BITS);
    const __m128i out_19_7 = _mm_srai_epi32(out_19_5, DCT_CONST_BITS);
    const __m128i out_11_6 = _mm_srai_epi32(out_11_4, DCT_CONST_BITS);
    const __m128i out_11_7 = _mm_srai_epi32(out_11_5, DCT_CONST_BITS);
    const __m128i out_27_6 = _mm_srai_epi32(out_27_4, DCT_CONST_BITS);
    const __m128i out_27_7 = _mm_srai_epi32(out_27_5, DCT_CONST_BITS);
    // Combine
    out[5] = _mm_packs_epi32(out_05_6, out_05_7);
    out[21] = _mm_packs_epi32(out_21_6, out_21_7);
    out[13] = _mm_packs_epi32(out_13_6, out_13_7);
    out[29] = _mm_packs_epi32(out_29_6, out_29_7);
    out[3] = _mm_packs_epi32(out_03_6, out_03_7);
    out[19] = _mm_packs_epi32(out_19_6, out_19_7);
    out[11] = _mm_packs_epi32(out_11_6, out_11_7);
    out[27] = _mm_packs_epi32(out_27_6, out_27_7);
  }

  // Output results
  {
    int j;
    for (j = 0; j < 16; ++j) {
      _mm_storeu_si128((__m128i *)(in0 + j), out[j]);
      _mm_storeu_si128((__m128i *)(in1 + j), out[j + 16]);
    }
  }
}  // NOLINT
