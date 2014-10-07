/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(_MSC_VER) && _MSC_VER <= 1500
// Need to include math.h before calling tmmintrin.h/intrin.h
// in certain versions of MSVS.
#include <math.h>
#endif
#include <tmmintrin.h>  // SSSE3
#include "vp9/common/x86/vp9_idct_intrin_sse2.h"

static void idct16_8col(__m128i *in, int round) {
  const __m128i k__cospi_p30_m02 = pair_set_epi16(cospi_30_64, -cospi_2_64);
  const __m128i k__cospi_p02_p30 = pair_set_epi16(cospi_2_64, cospi_30_64);
  const __m128i k__cospi_p14_m18 = pair_set_epi16(cospi_14_64, -cospi_18_64);
  const __m128i k__cospi_p18_p14 = pair_set_epi16(cospi_18_64, cospi_14_64);
  const __m128i k__cospi_p22_m10 = pair_set_epi16(cospi_22_64, -cospi_10_64);
  const __m128i k__cospi_p10_p22 = pair_set_epi16(cospi_10_64, cospi_22_64);
  const __m128i k__cospi_p06_m26 = pair_set_epi16(cospi_6_64, -cospi_26_64);
  const __m128i k__cospi_p26_p06 = pair_set_epi16(cospi_26_64, cospi_6_64);
  const __m128i k__cospi_p28_m04 = pair_set_epi16(cospi_28_64, -cospi_4_64);
  const __m128i k__cospi_p04_p28 = pair_set_epi16(cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p12_m20 = pair_set_epi16(cospi_12_64, -cospi_20_64);
  const __m128i k__cospi_p20_p12 = pair_set_epi16(cospi_20_64, cospi_12_64);
  const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  const __m128i k__cospi_p16_p16_x2 = pair_set_epi16(23170, 23170);
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);

  __m128i v[16], u[16], s[16], t[16];

  // stage 1
  s[0] = in[0];
  s[1] = in[8];
  s[2] = in[4];
  s[3] = in[12];
  s[4] = in[2];
  s[5] = in[10];
  s[6] = in[6];
  s[7] = in[14];
  s[8] = in[1];
  s[9] = in[9];
  s[10] = in[5];
  s[11] = in[13];
  s[12] = in[3];
  s[13] = in[11];
  s[14] = in[7];
  s[15] = in[15];

  // stage 2
  u[0] = _mm_unpacklo_epi16(s[8], s[15]);
  u[1] = _mm_unpackhi_epi16(s[8], s[15]);
  u[2] = _mm_unpacklo_epi16(s[9], s[14]);
  u[3] = _mm_unpackhi_epi16(s[9], s[14]);
  u[4] = _mm_unpacklo_epi16(s[10], s[13]);
  u[5] = _mm_unpackhi_epi16(s[10], s[13]);
  u[6] = _mm_unpacklo_epi16(s[11], s[12]);
  u[7] = _mm_unpackhi_epi16(s[11], s[12]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p30_m02);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p30_m02);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p02_p30);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p02_p30);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p14_m18);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p14_m18);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p18_p14);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p18_p14);
  v[8] = _mm_madd_epi16(u[4], k__cospi_p22_m10);
  v[9] = _mm_madd_epi16(u[5], k__cospi_p22_m10);
  v[10] = _mm_madd_epi16(u[4], k__cospi_p10_p22);
  v[11] = _mm_madd_epi16(u[5], k__cospi_p10_p22);
  v[12] = _mm_madd_epi16(u[6], k__cospi_p06_m26);
  v[13] = _mm_madd_epi16(u[7], k__cospi_p06_m26);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p26_p06);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p26_p06);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);
  u[8] = _mm_add_epi32(v[8], k__DCT_CONST_ROUNDING);
  u[9] = _mm_add_epi32(v[9], k__DCT_CONST_ROUNDING);
  u[10] = _mm_add_epi32(v[10], k__DCT_CONST_ROUNDING);
  u[11] = _mm_add_epi32(v[11], k__DCT_CONST_ROUNDING);
  u[12] = _mm_add_epi32(v[12], k__DCT_CONST_ROUNDING);
  u[13] = _mm_add_epi32(v[13], k__DCT_CONST_ROUNDING);
  u[14] = _mm_add_epi32(v[14], k__DCT_CONST_ROUNDING);
  u[15] = _mm_add_epi32(v[15], k__DCT_CONST_ROUNDING);

  u[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  u[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  u[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  u[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  u[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
  u[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
  u[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
  u[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
  u[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
  u[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
  u[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
  u[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
  u[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

  s[8]  = _mm_packs_epi32(u[0], u[1]);
  s[15] = _mm_packs_epi32(u[2], u[3]);
  s[9]  = _mm_packs_epi32(u[4], u[5]);
  s[14] = _mm_packs_epi32(u[6], u[7]);
  s[10] = _mm_packs_epi32(u[8], u[9]);
  s[13] = _mm_packs_epi32(u[10], u[11]);
  s[11] = _mm_packs_epi32(u[12], u[13]);
  s[12] = _mm_packs_epi32(u[14], u[15]);

  // stage 3
  t[0] = s[0];
  t[1] = s[1];
  t[2] = s[2];
  t[3] = s[3];
  u[0] = _mm_unpacklo_epi16(s[4], s[7]);
  u[1] = _mm_unpackhi_epi16(s[4], s[7]);
  u[2] = _mm_unpacklo_epi16(s[5], s[6]);
  u[3] = _mm_unpackhi_epi16(s[5], s[6]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p28_m04);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p28_m04);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p04_p28);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p04_p28);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p12_m20);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p12_m20);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p20_p12);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p20_p12);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

  u[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  u[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  u[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  u[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  u[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

  t[4] = _mm_packs_epi32(u[0], u[1]);
  t[7] = _mm_packs_epi32(u[2], u[3]);
  t[5] = _mm_packs_epi32(u[4], u[5]);
  t[6] = _mm_packs_epi32(u[6], u[7]);
  t[8] = _mm_add_epi16(s[8], s[9]);
  t[9] = _mm_sub_epi16(s[8], s[9]);
  t[10] = _mm_sub_epi16(s[11], s[10]);
  t[11] = _mm_add_epi16(s[10], s[11]);
  t[12] = _mm_add_epi16(s[12], s[13]);
  t[13] = _mm_sub_epi16(s[12], s[13]);
  t[14] = _mm_sub_epi16(s[15], s[14]);
  t[15] = _mm_add_epi16(s[14], s[15]);

  // stage 4
  u[0] = _mm_add_epi16(t[0], t[1]);
  u[1] = _mm_sub_epi16(t[0], t[1]);
  u[2] = _mm_unpacklo_epi16(t[2], t[3]);
  u[3] = _mm_unpackhi_epi16(t[2], t[3]);
  u[4] = _mm_unpacklo_epi16(t[9], t[14]);
  u[5] = _mm_unpackhi_epi16(t[9], t[14]);
  u[6] = _mm_unpacklo_epi16(t[10], t[13]);
  u[7] = _mm_unpackhi_epi16(t[10], t[13]);

  s[0] = _mm_mulhrs_epi16(u[0], k__cospi_p16_p16_x2);
  s[1] = _mm_mulhrs_epi16(u[1], k__cospi_p16_p16_x2);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p24_m08);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p24_m08);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p08_p24);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p08_p24);
  v[8] = _mm_madd_epi16(u[4], k__cospi_m08_p24);
  v[9] = _mm_madd_epi16(u[5], k__cospi_m08_p24);
  v[10] = _mm_madd_epi16(u[4], k__cospi_p24_p08);
  v[11] = _mm_madd_epi16(u[5], k__cospi_p24_p08);
  v[12] = _mm_madd_epi16(u[6], k__cospi_m24_m08);
  v[13] = _mm_madd_epi16(u[7], k__cospi_m24_m08);
  v[14] = _mm_madd_epi16(u[6], k__cospi_m08_p24);
  v[15] = _mm_madd_epi16(u[7], k__cospi_m08_p24);

  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);
  u[8] = _mm_add_epi32(v[8], k__DCT_CONST_ROUNDING);
  u[9] = _mm_add_epi32(v[9], k__DCT_CONST_ROUNDING);
  u[10] = _mm_add_epi32(v[10], k__DCT_CONST_ROUNDING);
  u[11] = _mm_add_epi32(v[11], k__DCT_CONST_ROUNDING);
  u[12] = _mm_add_epi32(v[12], k__DCT_CONST_ROUNDING);
  u[13] = _mm_add_epi32(v[13], k__DCT_CONST_ROUNDING);
  u[14] = _mm_add_epi32(v[14], k__DCT_CONST_ROUNDING);
  u[15] = _mm_add_epi32(v[15], k__DCT_CONST_ROUNDING);

  u[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  u[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  u[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  u[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
  u[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
  u[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
  u[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
  u[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
  u[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
  u[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
  u[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
  u[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

  s[2] = _mm_packs_epi32(u[4], u[5]);
  s[3] = _mm_packs_epi32(u[6], u[7]);
  s[4] = _mm_add_epi16(t[4], t[5]);
  s[5] = _mm_sub_epi16(t[4], t[5]);
  s[6] = _mm_sub_epi16(t[7], t[6]);
  s[7] = _mm_add_epi16(t[6], t[7]);
  s[8] = t[8];
  s[15] = t[15];
  s[9]  = _mm_packs_epi32(u[8], u[9]);
  s[14] = _mm_packs_epi32(u[10], u[11]);
  s[10] = _mm_packs_epi32(u[12], u[13]);
  s[13] = _mm_packs_epi32(u[14], u[15]);
  s[11] = t[11];
  s[12] = t[12];

  // stage 5
  t[0] = _mm_add_epi16(s[0], s[3]);
  t[1] = _mm_add_epi16(s[1], s[2]);
  t[2] = _mm_sub_epi16(s[1], s[2]);
  t[3] = _mm_sub_epi16(s[0], s[3]);
  t[4] = s[4];
  t[7] = s[7];

  u[0] = _mm_sub_epi16(s[6], s[5]);
  u[1] = _mm_add_epi16(s[6], s[5]);
  t[5] = _mm_mulhrs_epi16(u[0], k__cospi_p16_p16_x2);
  t[6] = _mm_mulhrs_epi16(u[1], k__cospi_p16_p16_x2);

  t[8] = _mm_add_epi16(s[8], s[11]);
  t[9] = _mm_add_epi16(s[9], s[10]);
  t[10] = _mm_sub_epi16(s[9], s[10]);
  t[11] = _mm_sub_epi16(s[8], s[11]);
  t[12] = _mm_sub_epi16(s[15], s[12]);
  t[13] = _mm_sub_epi16(s[14], s[13]);
  t[14] = _mm_add_epi16(s[13], s[14]);
  t[15] = _mm_add_epi16(s[12], s[15]);

  // stage 6
  if (round == 1) {
    s[0] = _mm_add_epi16(t[0], t[7]);
    s[1] = _mm_add_epi16(t[1], t[6]);
    s[2] = _mm_add_epi16(t[2], t[5]);
    s[3] = _mm_add_epi16(t[3], t[4]);
    s[4] = _mm_sub_epi16(t[3], t[4]);
    s[5] = _mm_sub_epi16(t[2], t[5]);
    s[6] = _mm_sub_epi16(t[1], t[6]);
    s[7] = _mm_sub_epi16(t[0], t[7]);
    s[8] = t[8];
    s[9] = t[9];

    u[0] = _mm_unpacklo_epi16(t[10], t[13]);
    u[1] = _mm_unpackhi_epi16(t[10], t[13]);
    u[2] = _mm_unpacklo_epi16(t[11], t[12]);
    u[3] = _mm_unpackhi_epi16(t[11], t[12]);

    v[0] = _mm_madd_epi16(u[0], k__cospi_m16_p16);
    v[1] = _mm_madd_epi16(u[1], k__cospi_m16_p16);
    v[2] = _mm_madd_epi16(u[0], k__cospi_p16_p16);
    v[3] = _mm_madd_epi16(u[1], k__cospi_p16_p16);
    v[4] = _mm_madd_epi16(u[2], k__cospi_m16_p16);
    v[5] = _mm_madd_epi16(u[3], k__cospi_m16_p16);
    v[6] = _mm_madd_epi16(u[2], k__cospi_p16_p16);
    v[7] = _mm_madd_epi16(u[3], k__cospi_p16_p16);

    u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
    u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
    u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
    u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
    u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
    u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
    u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
    u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

    u[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
    u[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
    u[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
    u[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
    u[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
    u[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
    u[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
    u[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

    s[10] = _mm_packs_epi32(u[0], u[1]);
    s[13] = _mm_packs_epi32(u[2], u[3]);
    s[11] = _mm_packs_epi32(u[4], u[5]);
    s[12] = _mm_packs_epi32(u[6], u[7]);
    s[14] = t[14];
    s[15] = t[15];
  } else {
    s[0] = _mm_add_epi16(t[0], t[7]);
    s[1] = _mm_add_epi16(t[1], t[6]);
    s[2] = _mm_add_epi16(t[2], t[5]);
    s[3] = _mm_add_epi16(t[3], t[4]);
    s[4] = _mm_sub_epi16(t[3], t[4]);
    s[5] = _mm_sub_epi16(t[2], t[5]);
    s[6] = _mm_sub_epi16(t[1], t[6]);
    s[7] = _mm_sub_epi16(t[0], t[7]);
    s[8] = t[8];
    s[9] = t[9];

    u[0] = _mm_sub_epi16(t[13], t[10]);
    u[1] = _mm_add_epi16(t[13], t[10]);
    u[2] = _mm_sub_epi16(t[12], t[11]);
    u[3] = _mm_add_epi16(t[12], t[11]);

    s[10] = _mm_mulhrs_epi16(u[0], k__cospi_p16_p16_x2);
    s[13] = _mm_mulhrs_epi16(u[1], k__cospi_p16_p16_x2);
    s[11] = _mm_mulhrs_epi16(u[2], k__cospi_p16_p16_x2);
    s[12] = _mm_mulhrs_epi16(u[3], k__cospi_p16_p16_x2);
    s[14] = t[14];
    s[15] = t[15];
  }

  // stage 7
  in[0] = _mm_add_epi16(s[0], s[15]);
  in[1] = _mm_add_epi16(s[1], s[14]);
  in[2] = _mm_add_epi16(s[2], s[13]);
  in[3] = _mm_add_epi16(s[3], s[12]);
  in[4] = _mm_add_epi16(s[4], s[11]);
  in[5] = _mm_add_epi16(s[5], s[10]);
  in[6] = _mm_add_epi16(s[6], s[9]);
  in[7] = _mm_add_epi16(s[7], s[8]);
  in[8] = _mm_sub_epi16(s[7], s[8]);
  in[9] = _mm_sub_epi16(s[6], s[9]);
  in[10] = _mm_sub_epi16(s[5], s[10]);
  in[11] = _mm_sub_epi16(s[4], s[11]);
  in[12] = _mm_sub_epi16(s[3], s[12]);
  in[13] = _mm_sub_epi16(s[2], s[13]);
  in[14] = _mm_sub_epi16(s[1], s[14]);
  in[15] = _mm_sub_epi16(s[0], s[15]);
}

static void idct16_sse2(__m128i *in0, __m128i *in1, int round) {
  array_transpose_16x16(in0, in1);
  idct16_8col(in0, round);
  idct16_8col(in1, round);
}

void vp9_idct16x16_256_add_ssse3(const int16_t *input, uint8_t *dest,
                                int stride) {
  __m128i in0[16], in1[16];

  load_buffer_8x16(input, in0);
  input += 8;
  load_buffer_8x16(input, in1);

  idct16_sse2(in0, in1, 0);
  idct16_sse2(in0, in1, 1);

  write_buffer_8x16(dest, in0, stride);
  dest += 8;
  write_buffer_8x16(dest, in1, stride);
}

static void idct16_10_r1(__m128i *in, __m128i *l) {
  const __m128i rounding = _mm_set1_epi32(DCT_CONST_ROUNDING);
  const __m128i zero = _mm_setzero_si128();

  const __m128i stg2_01 = dual_set_epi16(3212, 32610);
  const __m128i stg2_67 = dual_set_epi16(-9512, 31358);
  const __m128i stg3_01 = dual_set_epi16(6392, 32138);
  const __m128i stg4_01 = dual_set_epi16(23170, 23170);



  const __m128i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
  const __m128i stg4_7 = pair_set_epi16(-cospi_8_64, cospi_24_64);

  __m128i stp1_0, stp1_1, stp1_4, stp1_6,
          stp1_8, stp1_9, stp1_10, stp1_11, stp1_12, stp1_13, stp1_14, stp1_15;
  __m128i stp2_0, stp2_1, stp2_2, stp2_3, stp2_4, stp2_5, stp2_6, stp2_7,
          stp2_8, stp2_9, stp2_10, stp2_11, stp2_12, stp2_13;
  __m128i tmp0, tmp1, tmp2, tmp3, tmp4;

  // Stage2
  {
    const __m128i lo_1_15 = _mm_unpackhi_epi64(in[0], in[0]);
    const __m128i lo_13_3 = _mm_unpackhi_epi64(in[1], in[1]);

    stp2_8  = _mm_mulhrs_epi16(lo_1_15, stg2_01);
    stp2_11 = _mm_mulhrs_epi16(lo_13_3, stg2_67);
  }

  // Stage3
  {
    const __m128i lo_2_14 = _mm_unpacklo_epi64(in[1], in[1]);
    stp1_4 = _mm_mulhrs_epi16(lo_2_14, stg3_01);

    stp1_13 = _mm_unpackhi_epi64(stp2_11, zero);
    stp1_14 = _mm_unpackhi_epi64(stp2_8, zero);
  }

  // Stage4
  {
    const __m128i lo_0_8 = _mm_unpacklo_epi64(in[0], in[0]);
    const __m128i lo_9_14 = _mm_unpacklo_epi16(stp2_8, stp1_14);
    const __m128i lo_10_13 = _mm_unpacklo_epi16(stp2_11, stp1_13);

    tmp0 = _mm_mulhrs_epi16(lo_0_8, stg4_01);
    tmp1 = _mm_madd_epi16(lo_9_14, stg4_4);
    tmp3 = _mm_madd_epi16(lo_9_14, stg4_5);
    tmp2 = _mm_madd_epi16(lo_10_13, stg4_6);
    tmp4 = _mm_madd_epi16(lo_10_13, stg4_7);

    tmp1 = _mm_add_epi32(tmp1, rounding);
    tmp3 = _mm_add_epi32(tmp3, rounding);
    tmp2 = _mm_add_epi32(tmp2, rounding);
    tmp4 = _mm_add_epi32(tmp4, rounding);

    tmp1 = _mm_srai_epi32(tmp1, DCT_CONST_BITS);
    tmp3 = _mm_srai_epi32(tmp3, DCT_CONST_BITS);
    tmp2 = _mm_srai_epi32(tmp2, DCT_CONST_BITS);
    tmp4 = _mm_srai_epi32(tmp4, DCT_CONST_BITS);

    stp1_0 = _mm_unpacklo_epi64(tmp0, tmp0);
    stp1_1 = _mm_unpackhi_epi64(tmp0, tmp0);
    stp2_9 = _mm_packs_epi32(tmp1, tmp3);
    stp2_10 = _mm_packs_epi32(tmp2, tmp4);

    stp2_6 = _mm_unpackhi_epi64(stp1_4, zero);
  }

  // Stage5 and Stage6
  {
    tmp0 = _mm_add_epi16(stp2_8, stp2_11);
    tmp1 = _mm_sub_epi16(stp2_8, stp2_11);
    tmp2 = _mm_add_epi16(stp2_9, stp2_10);
    tmp3 = _mm_sub_epi16(stp2_9, stp2_10);

    stp1_9  = _mm_unpacklo_epi64(tmp2, zero);
    stp1_10 = _mm_unpacklo_epi64(tmp3, zero);
    stp1_8  = _mm_unpacklo_epi64(tmp0, zero);
    stp1_11 = _mm_unpacklo_epi64(tmp1, zero);

    stp1_13 = _mm_unpackhi_epi64(tmp3, zero);
    stp1_14 = _mm_unpackhi_epi64(tmp2, zero);
    stp1_12 = _mm_unpackhi_epi64(tmp1, zero);
    stp1_15 = _mm_unpackhi_epi64(tmp0, zero);
  }

  // Stage6
  {
    const __m128i lo_6_5 = _mm_add_epi16(stp2_6, stp1_4);
    const __m128i lo_6_6 = _mm_sub_epi16(stp2_6, stp1_4);
    const __m128i lo_10_13 = _mm_sub_epi16(stp1_13, stp1_10);
    const __m128i lo_10_14 = _mm_add_epi16(stp1_13, stp1_10);
    const __m128i lo_11_12 = _mm_sub_epi16(stp1_12, stp1_11);
    const __m128i lo_11_13 = _mm_add_epi16(stp1_12, stp1_11);

    tmp1 = _mm_unpacklo_epi64(lo_6_5, lo_6_6);
    tmp0 = _mm_unpacklo_epi64(lo_10_13, lo_10_14);
    tmp4 = _mm_unpacklo_epi64(lo_11_12, lo_11_13);

    stp1_6 = _mm_mulhrs_epi16(tmp1, stg4_01);
    tmp0   = _mm_mulhrs_epi16(tmp0, stg4_01);
    tmp4   = _mm_mulhrs_epi16(tmp4, stg4_01);

    stp2_10 = _mm_unpacklo_epi64(tmp0, zero);
    stp2_13 = _mm_unpackhi_epi64(tmp0, zero);
    stp2_11 = _mm_unpacklo_epi64(tmp4, zero);
    stp2_12 = _mm_unpackhi_epi64(tmp4, zero);

    tmp0 = _mm_add_epi16(stp1_0, stp1_4);
    tmp1 = _mm_sub_epi16(stp1_0, stp1_4);
    tmp2 = _mm_add_epi16(stp1_1, stp1_6);
    tmp3 = _mm_sub_epi16(stp1_1, stp1_6);

    stp2_0 = _mm_unpackhi_epi64(tmp0, zero);
    stp2_1 = _mm_unpacklo_epi64(tmp2, zero);
    stp2_2 = _mm_unpackhi_epi64(tmp2, zero);
    stp2_3 = _mm_unpacklo_epi64(tmp0, zero);
    stp2_4 = _mm_unpacklo_epi64(tmp1, zero);
    stp2_5 = _mm_unpackhi_epi64(tmp3, zero);
    stp2_6 = _mm_unpacklo_epi64(tmp3, zero);
    stp2_7 = _mm_unpackhi_epi64(tmp1, zero);
  }

  // Stage7. Left 8x16 only.
  l[0] = _mm_add_epi16(stp2_0, stp1_15);
  l[1] = _mm_add_epi16(stp2_1, stp1_14);
  l[2] = _mm_add_epi16(stp2_2, stp2_13);
  l[3] = _mm_add_epi16(stp2_3, stp2_12);
  l[4] = _mm_add_epi16(stp2_4, stp2_11);
  l[5] = _mm_add_epi16(stp2_5, stp2_10);
  l[6] = _mm_add_epi16(stp2_6, stp1_9);
  l[7] = _mm_add_epi16(stp2_7, stp1_8);
  l[8] = _mm_sub_epi16(stp2_7, stp1_8);
  l[9] = _mm_sub_epi16(stp2_6, stp1_9);
  l[10] = _mm_sub_epi16(stp2_5, stp2_10);
  l[11] = _mm_sub_epi16(stp2_4, stp2_11);
  l[12] = _mm_sub_epi16(stp2_3, stp2_12);
  l[13] = _mm_sub_epi16(stp2_2, stp2_13);
  l[14] = _mm_sub_epi16(stp2_1, stp1_14);
  l[15] = _mm_sub_epi16(stp2_0, stp1_15);
}

static void idct16_10_r2(__m128i *in) {
  const __m128i rounding = _mm_set1_epi32(DCT_CONST_ROUNDING);

  const __m128i stg2_0 = dual_set_epi16(3212, 3212);
  const __m128i stg2_1 = dual_set_epi16(32610, 32610);
  const __m128i stg2_6 = dual_set_epi16(-9512, -9512);
  const __m128i stg2_7 = dual_set_epi16(31358, 31358);
  const __m128i stg3_0 = dual_set_epi16(6392, 6392);
  const __m128i stg3_1 = dual_set_epi16(32138, 32138);
  const __m128i stg4_01 = dual_set_epi16(23170, 23170);

  const __m128i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
  const __m128i stg4_7 = pair_set_epi16(-cospi_8_64, cospi_24_64);

  __m128i stp1_0, stp1_2, stp1_3, stp1_5, stp1_6,
          stp1_8, stp1_9, stp1_10, stp1_11, stp1_12, stp1_13, stp1_14, stp1_15,
          stp1_8_0, stp1_12_0;
  __m128i stp2_0, stp2_1, stp2_2, stp2_3, stp2_4, stp2_5, stp2_6, stp2_7,
          stp2_9, stp2_10, stp2_11, stp2_12, stp2_13, stp2_14;
  __m128i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;

  /* Stage2 */
  {
    stp1_8_0  = _mm_mulhrs_epi16(in[1], stg2_0);
    stp1_15   = _mm_mulhrs_epi16(in[1], stg2_1);
    stp1_11   = _mm_mulhrs_epi16(in[3], stg2_6);
    stp1_12_0 = _mm_mulhrs_epi16(in[3], stg2_7);
  }

  /* Stage3 */
  {
    stp2_4 = _mm_mulhrs_epi16(in[2], stg3_0);
    stp2_7 = _mm_mulhrs_epi16(in[2], stg3_1);

    stp1_9  =  stp1_8_0;
    stp1_10 =  stp1_11;

    stp1_13 = stp1_12_0;
    stp1_14 = stp1_15;
  }

  /* Stage4 */
  {
    const __m128i lo_9_14 = _mm_unpacklo_epi16(stp1_9, stp1_14);
    const __m128i hi_9_14 = _mm_unpackhi_epi16(stp1_9, stp1_14);
    const __m128i lo_10_13 = _mm_unpacklo_epi16(stp1_10, stp1_13);
    const __m128i hi_10_13 = _mm_unpackhi_epi16(stp1_10, stp1_13);

    stp1_0 = _mm_mulhrs_epi16(in[0], stg4_01);

    stp2_5 = stp2_4;
    stp2_6 = stp2_7;


    tmp0 = _mm_madd_epi16(lo_9_14, stg4_4);
    tmp1 = _mm_madd_epi16(hi_9_14, stg4_4);
    tmp2 = _mm_madd_epi16(lo_9_14, stg4_5);
    tmp3 = _mm_madd_epi16(hi_9_14, stg4_5);
    tmp4 = _mm_madd_epi16(lo_10_13, stg4_6);
    tmp5 = _mm_madd_epi16(hi_10_13, stg4_6);
    tmp6 = _mm_madd_epi16(lo_10_13, stg4_7);
    tmp7 = _mm_madd_epi16(hi_10_13, stg4_7);

    tmp0 = _mm_add_epi32(tmp0, rounding);
    tmp1 = _mm_add_epi32(tmp1, rounding);
    tmp2 = _mm_add_epi32(tmp2, rounding);
    tmp3 = _mm_add_epi32(tmp3, rounding);
    tmp4 = _mm_add_epi32(tmp4, rounding);
    tmp5 = _mm_add_epi32(tmp5, rounding);
    tmp6 = _mm_add_epi32(tmp6, rounding);
    tmp7 = _mm_add_epi32(tmp7, rounding);

    tmp0 = _mm_srai_epi32(tmp0, 14);
    tmp1 = _mm_srai_epi32(tmp1, 14);
    tmp2 = _mm_srai_epi32(tmp2, 14);
    tmp3 = _mm_srai_epi32(tmp3, 14);
    tmp4 = _mm_srai_epi32(tmp4, 14);
    tmp5 = _mm_srai_epi32(tmp5, 14);
    tmp6 = _mm_srai_epi32(tmp6, 14);
    tmp7 = _mm_srai_epi32(tmp7, 14);

    stp2_9 = _mm_packs_epi32(tmp0, tmp1);
    stp2_14 = _mm_packs_epi32(tmp2, tmp3);
    stp2_10 = _mm_packs_epi32(tmp4, tmp5);
    stp2_13 = _mm_packs_epi32(tmp6, tmp7);
  }

  /* Stage5 */
  {
    stp1_2 = stp1_0;
    stp1_3 = stp1_0;

    tmp0 = _mm_sub_epi16(stp2_6, stp2_5);
    tmp1 = _mm_add_epi16(stp2_6, stp2_5);

    stp1_5 = _mm_mulhrs_epi16(tmp0, stg4_01);
    stp1_6 = _mm_mulhrs_epi16(tmp1, stg4_01);

    stp1_8 = _mm_add_epi16(stp1_8_0, stp1_11);
    stp1_9 = _mm_add_epi16(stp2_9, stp2_10);
    stp1_10 = _mm_sub_epi16(stp2_9, stp2_10);
    stp1_11 = _mm_sub_epi16(stp1_8_0, stp1_11);

    stp1_12 = _mm_sub_epi16(stp1_15, stp1_12_0);
    stp1_13 = _mm_sub_epi16(stp2_14, stp2_13);
    stp1_14 = _mm_add_epi16(stp2_14, stp2_13);
    stp1_15 = _mm_add_epi16(stp1_15, stp1_12_0);
  }

  /* Stage6 */
  {
    stp2_0 = _mm_add_epi16(stp1_0, stp2_7);
    stp2_1 = _mm_add_epi16(stp1_0, stp1_6);
    stp2_2 = _mm_add_epi16(stp1_2, stp1_5);
    stp2_3 = _mm_add_epi16(stp1_3, stp2_4);

    tmp0 = _mm_sub_epi16(stp1_13, stp1_10);
    tmp1 = _mm_add_epi16(stp1_13, stp1_10);
    tmp2 = _mm_sub_epi16(stp1_12, stp1_11);
    tmp3 = _mm_add_epi16(stp1_12, stp1_11);

    stp2_4 = _mm_sub_epi16(stp1_3, stp2_4);
    stp2_5 = _mm_sub_epi16(stp1_2, stp1_5);
    stp2_6 = _mm_sub_epi16(stp1_0, stp1_6);
    stp2_7 = _mm_sub_epi16(stp1_0, stp2_7);

    stp2_10 = _mm_mulhrs_epi16(tmp0, stg4_01);
    stp2_13 = _mm_mulhrs_epi16(tmp1, stg4_01);
    stp2_11 = _mm_mulhrs_epi16(tmp2, stg4_01);
    stp2_12 = _mm_mulhrs_epi16(tmp3, stg4_01);
  }

  // Stage7
  in[0] = _mm_add_epi16(stp2_0, stp1_15);
  in[1] = _mm_add_epi16(stp2_1, stp1_14);
  in[2] = _mm_add_epi16(stp2_2, stp2_13);
  in[3] = _mm_add_epi16(stp2_3, stp2_12);
  in[4] = _mm_add_epi16(stp2_4, stp2_11);
  in[5] = _mm_add_epi16(stp2_5, stp2_10);
  in[6] = _mm_add_epi16(stp2_6, stp1_9);
  in[7] = _mm_add_epi16(stp2_7, stp1_8);
  in[8] = _mm_sub_epi16(stp2_7, stp1_8);
  in[9] = _mm_sub_epi16(stp2_6, stp1_9);
  in[10] = _mm_sub_epi16(stp2_5, stp2_10);
  in[11] = _mm_sub_epi16(stp2_4, stp2_11);
  in[12] = _mm_sub_epi16(stp2_3, stp2_12);
  in[13] = _mm_sub_epi16(stp2_2, stp2_13);
  in[14] = _mm_sub_epi16(stp2_1, stp1_14);
  in[15] = _mm_sub_epi16(stp2_0, stp1_15);
}

void vp9_idct16x16_10_add_ssse3(const int16_t *input, uint8_t *dest,
                               int stride) {
  const __m128i final_rounding = _mm_set1_epi16(1<<5);
  const __m128i zero = _mm_setzero_si128();
  __m128i in[16], l[16];

  int i;
  // First 1-D inverse DCT
  // Load input data.
  in[0] = _mm_load_si128((const __m128i *)input);
  in[1] = _mm_load_si128((const __m128i *)(input + 8 * 2));
  in[2] = _mm_load_si128((const __m128i *)(input + 8 * 4));
  in[3] = _mm_load_si128((const __m128i *)(input + 8 * 6));

  TRANSPOSE_8X4(in[0], in[1], in[2], in[3], in[0], in[1]);

  idct16_10_r1(in, l);

  // Second 1-D inverse transform, performed per 8x16 block
  for (i = 0; i < 2; i++) {
    array_transpose_4X8(l + 8*i, in);

    idct16_10_r2(in);

    // Final rounding and shift
    in[0] = _mm_adds_epi16(in[0], final_rounding);
    in[1] = _mm_adds_epi16(in[1], final_rounding);
    in[2] = _mm_adds_epi16(in[2], final_rounding);
    in[3] = _mm_adds_epi16(in[3], final_rounding);
    in[4] = _mm_adds_epi16(in[4], final_rounding);
    in[5] = _mm_adds_epi16(in[5], final_rounding);
    in[6] = _mm_adds_epi16(in[6], final_rounding);
    in[7] = _mm_adds_epi16(in[7], final_rounding);
    in[8] = _mm_adds_epi16(in[8], final_rounding);
    in[9] = _mm_adds_epi16(in[9], final_rounding);
    in[10] = _mm_adds_epi16(in[10], final_rounding);
    in[11] = _mm_adds_epi16(in[11], final_rounding);
    in[12] = _mm_adds_epi16(in[12], final_rounding);
    in[13] = _mm_adds_epi16(in[13], final_rounding);
    in[14] = _mm_adds_epi16(in[14], final_rounding);
    in[15] = _mm_adds_epi16(in[15], final_rounding);

    in[0] = _mm_srai_epi16(in[0], 6);
    in[1] = _mm_srai_epi16(in[1], 6);
    in[2] = _mm_srai_epi16(in[2], 6);
    in[3] = _mm_srai_epi16(in[3], 6);
    in[4] = _mm_srai_epi16(in[4], 6);
    in[5] = _mm_srai_epi16(in[5], 6);
    in[6] = _mm_srai_epi16(in[6], 6);
    in[7] = _mm_srai_epi16(in[7], 6);
    in[8] = _mm_srai_epi16(in[8], 6);
    in[9] = _mm_srai_epi16(in[9], 6);
    in[10] = _mm_srai_epi16(in[10], 6);
    in[11] = _mm_srai_epi16(in[11], 6);
    in[12] = _mm_srai_epi16(in[12], 6);
    in[13] = _mm_srai_epi16(in[13], 6);
    in[14] = _mm_srai_epi16(in[14], 6);
    in[15] = _mm_srai_epi16(in[15], 6);

    RECON_AND_STORE(dest, in[0]);
    RECON_AND_STORE(dest, in[1]);
    RECON_AND_STORE(dest, in[2]);
    RECON_AND_STORE(dest, in[3]);
    RECON_AND_STORE(dest, in[4]);
    RECON_AND_STORE(dest, in[5]);
    RECON_AND_STORE(dest, in[6]);
    RECON_AND_STORE(dest, in[7]);
    RECON_AND_STORE(dest, in[8]);
    RECON_AND_STORE(dest, in[9]);
    RECON_AND_STORE(dest, in[10]);
    RECON_AND_STORE(dest, in[11]);
    RECON_AND_STORE(dest, in[12]);
    RECON_AND_STORE(dest, in[13]);
    RECON_AND_STORE(dest, in[14]);
    RECON_AND_STORE(dest, in[15]);

    dest += 8 - (stride * 16);
  }
}
