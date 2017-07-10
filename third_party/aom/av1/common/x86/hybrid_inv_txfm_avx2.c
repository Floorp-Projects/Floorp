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

#include "./aom_config.h"
#include "./av1_rtcd.h"

#include "aom_dsp/x86/inv_txfm_common_avx2.h"

void av1_idct16_avx2(__m256i *in) {
  const __m256i cospi_p30_m02 = pair256_set_epi16(cospi_30_64, -cospi_2_64);
  const __m256i cospi_p02_p30 = pair256_set_epi16(cospi_2_64, cospi_30_64);
  const __m256i cospi_p14_m18 = pair256_set_epi16(cospi_14_64, -cospi_18_64);
  const __m256i cospi_p18_p14 = pair256_set_epi16(cospi_18_64, cospi_14_64);
  const __m256i cospi_p22_m10 = pair256_set_epi16(cospi_22_64, -cospi_10_64);
  const __m256i cospi_p10_p22 = pair256_set_epi16(cospi_10_64, cospi_22_64);
  const __m256i cospi_p06_m26 = pair256_set_epi16(cospi_6_64, -cospi_26_64);
  const __m256i cospi_p26_p06 = pair256_set_epi16(cospi_26_64, cospi_6_64);
  const __m256i cospi_p28_m04 = pair256_set_epi16(cospi_28_64, -cospi_4_64);
  const __m256i cospi_p04_p28 = pair256_set_epi16(cospi_4_64, cospi_28_64);
  const __m256i cospi_p12_m20 = pair256_set_epi16(cospi_12_64, -cospi_20_64);
  const __m256i cospi_p20_p12 = pair256_set_epi16(cospi_20_64, cospi_12_64);
  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  const __m256i cospi_p24_m08 = pair256_set_epi16(cospi_24_64, -cospi_8_64);
  const __m256i cospi_p08_p24 = pair256_set_epi16(cospi_8_64, cospi_24_64);
  const __m256i cospi_m08_p24 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i cospi_p24_p08 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i cospi_m24_m08 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i v0, v1, v2, v3, v4, v5, v6, v7;
  __m256i t0, t1, t2, t3, t4, t5, t6, t7;

  // stage 1, (0-7)
  u0 = in[0];
  u1 = in[8];
  u2 = in[4];
  u3 = in[12];
  u4 = in[2];
  u5 = in[10];
  u6 = in[6];
  u7 = in[14];

  // stage 2, (0-7)
  // stage 3, (0-7)
  t0 = u0;
  t1 = u1;
  t2 = u2;
  t3 = u3;
  unpack_butter_fly(&u4, &u7, &cospi_p28_m04, &cospi_p04_p28, &t4, &t7);
  unpack_butter_fly(&u5, &u6, &cospi_p12_m20, &cospi_p20_p12, &t5, &t6);

  // stage 4, (0-7)
  unpack_butter_fly(&t0, &t1, &cospi_p16_p16, &cospi_p16_m16, &u0, &u1);
  unpack_butter_fly(&t2, &t3, &cospi_p24_m08, &cospi_p08_p24, &u2, &u3);
  u4 = _mm256_add_epi16(t4, t5);
  u5 = _mm256_sub_epi16(t4, t5);
  u6 = _mm256_sub_epi16(t7, t6);
  u7 = _mm256_add_epi16(t7, t6);

  // stage 5, (0-7)
  t0 = _mm256_add_epi16(u0, u3);
  t1 = _mm256_add_epi16(u1, u2);
  t2 = _mm256_sub_epi16(u1, u2);
  t3 = _mm256_sub_epi16(u0, u3);
  t4 = u4;
  t7 = u7;
  unpack_butter_fly(&u6, &u5, &cospi_p16_m16, &cospi_p16_p16, &t5, &t6);

  // stage 6, (0-7)
  u0 = _mm256_add_epi16(t0, t7);
  u1 = _mm256_add_epi16(t1, t6);
  u2 = _mm256_add_epi16(t2, t5);
  u3 = _mm256_add_epi16(t3, t4);
  u4 = _mm256_sub_epi16(t3, t4);
  u5 = _mm256_sub_epi16(t2, t5);
  u6 = _mm256_sub_epi16(t1, t6);
  u7 = _mm256_sub_epi16(t0, t7);

  // stage 1, (8-15)
  v0 = in[1];
  v1 = in[9];
  v2 = in[5];
  v3 = in[13];
  v4 = in[3];
  v5 = in[11];
  v6 = in[7];
  v7 = in[15];

  // stage 2, (8-15)
  unpack_butter_fly(&v0, &v7, &cospi_p30_m02, &cospi_p02_p30, &t0, &t7);
  unpack_butter_fly(&v1, &v6, &cospi_p14_m18, &cospi_p18_p14, &t1, &t6);
  unpack_butter_fly(&v2, &v5, &cospi_p22_m10, &cospi_p10_p22, &t2, &t5);
  unpack_butter_fly(&v3, &v4, &cospi_p06_m26, &cospi_p26_p06, &t3, &t4);

  // stage 3, (8-15)
  v0 = _mm256_add_epi16(t0, t1);
  v1 = _mm256_sub_epi16(t0, t1);
  v2 = _mm256_sub_epi16(t3, t2);
  v3 = _mm256_add_epi16(t2, t3);
  v4 = _mm256_add_epi16(t4, t5);
  v5 = _mm256_sub_epi16(t4, t5);
  v6 = _mm256_sub_epi16(t7, t6);
  v7 = _mm256_add_epi16(t6, t7);

  // stage 4, (8-15)
  t0 = v0;
  t7 = v7;
  t3 = v3;
  t4 = v4;
  unpack_butter_fly(&v1, &v6, &cospi_m08_p24, &cospi_p24_p08, &t1, &t6);
  unpack_butter_fly(&v2, &v5, &cospi_m24_m08, &cospi_m08_p24, &t2, &t5);

  // stage 5, (8-15)
  v0 = _mm256_add_epi16(t0, t3);
  v1 = _mm256_add_epi16(t1, t2);
  v2 = _mm256_sub_epi16(t1, t2);
  v3 = _mm256_sub_epi16(t0, t3);
  v4 = _mm256_sub_epi16(t7, t4);
  v5 = _mm256_sub_epi16(t6, t5);
  v6 = _mm256_add_epi16(t6, t5);
  v7 = _mm256_add_epi16(t7, t4);

  // stage 6, (8-15)
  t0 = v0;
  t1 = v1;
  t6 = v6;
  t7 = v7;
  unpack_butter_fly(&v5, &v2, &cospi_p16_m16, &cospi_p16_p16, &t2, &t5);
  unpack_butter_fly(&v4, &v3, &cospi_p16_m16, &cospi_p16_p16, &t3, &t4);

  // stage 7
  in[0] = _mm256_add_epi16(u0, t7);
  in[1] = _mm256_add_epi16(u1, t6);
  in[2] = _mm256_add_epi16(u2, t5);
  in[3] = _mm256_add_epi16(u3, t4);
  in[4] = _mm256_add_epi16(u4, t3);
  in[5] = _mm256_add_epi16(u5, t2);
  in[6] = _mm256_add_epi16(u6, t1);
  in[7] = _mm256_add_epi16(u7, t0);
  in[8] = _mm256_sub_epi16(u7, t0);
  in[9] = _mm256_sub_epi16(u6, t1);
  in[10] = _mm256_sub_epi16(u5, t2);
  in[11] = _mm256_sub_epi16(u4, t3);
  in[12] = _mm256_sub_epi16(u3, t4);
  in[13] = _mm256_sub_epi16(u2, t5);
  in[14] = _mm256_sub_epi16(u1, t6);
  in[15] = _mm256_sub_epi16(u0, t7);
}

static void idct16(__m256i *in) {
  mm256_transpose_16x16(in, in);
  av1_idct16_avx2(in);
}

static INLINE void butterfly_32b(const __m256i *a0, const __m256i *a1,
                                 const __m256i *c0, const __m256i *c1,
                                 __m256i *b) {
  __m256i x0, x1;
  x0 = _mm256_unpacklo_epi16(*a0, *a1);
  x1 = _mm256_unpackhi_epi16(*a0, *a1);
  b[0] = _mm256_madd_epi16(x0, *c0);
  b[1] = _mm256_madd_epi16(x1, *c0);
  b[2] = _mm256_madd_epi16(x0, *c1);
  b[3] = _mm256_madd_epi16(x1, *c1);
}

static INLINE void group_rounding(__m256i *a, int num) {
  const __m256i dct_rounding = _mm256_set1_epi32(DCT_CONST_ROUNDING);
  int i;
  for (i = 0; i < num; ++i) {
    a[i] = _mm256_add_epi32(a[i], dct_rounding);
    a[i] = _mm256_srai_epi32(a[i], DCT_CONST_BITS);
  }
}

static INLINE void add_rnd(const __m256i *a, const __m256i *b, __m256i *out) {
  __m256i x[4];
  x[0] = _mm256_add_epi32(a[0], b[0]);
  x[1] = _mm256_add_epi32(a[1], b[1]);
  x[2] = _mm256_add_epi32(a[2], b[2]);
  x[3] = _mm256_add_epi32(a[3], b[3]);

  group_rounding(x, 4);

  out[0] = _mm256_packs_epi32(x[0], x[1]);
  out[1] = _mm256_packs_epi32(x[2], x[3]);
}

static INLINE void sub_rnd(const __m256i *a, const __m256i *b, __m256i *out) {
  __m256i x[4];
  x[0] = _mm256_sub_epi32(a[0], b[0]);
  x[1] = _mm256_sub_epi32(a[1], b[1]);
  x[2] = _mm256_sub_epi32(a[2], b[2]);
  x[3] = _mm256_sub_epi32(a[3], b[3]);

  group_rounding(x, 4);

  out[0] = _mm256_packs_epi32(x[0], x[1]);
  out[1] = _mm256_packs_epi32(x[2], x[3]);
}

static INLINE void butterfly_rnd(__m256i *a, __m256i *out) {
  group_rounding(a, 4);
  out[0] = _mm256_packs_epi32(a[0], a[1]);
  out[1] = _mm256_packs_epi32(a[2], a[3]);
}

static void iadst16_avx2(__m256i *in) {
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
  __m256i x[16], s[16];
  __m256i u[4], v[4];

  // stage 1
  butterfly_32b(&in[15], &in[0], &cospi_p01_p31, &cospi_p31_m01, u);
  butterfly_32b(&in[7], &in[8], &cospi_p17_p15, &cospi_p15_m17, v);
  add_rnd(u, v, &x[0]);
  sub_rnd(u, v, &x[8]);

  butterfly_32b(&in[13], &in[2], &cospi_p05_p27, &cospi_p27_m05, u);
  butterfly_32b(&in[5], &in[10], &cospi_p21_p11, &cospi_p11_m21, v);
  add_rnd(u, v, &x[2]);
  sub_rnd(u, v, &x[10]);

  butterfly_32b(&in[11], &in[4], &cospi_p09_p23, &cospi_p23_m09, u);
  butterfly_32b(&in[3], &in[12], &cospi_p25_p07, &cospi_p07_m25, v);
  add_rnd(u, v, &x[4]);
  sub_rnd(u, v, &x[12]);

  butterfly_32b(&in[9], &in[6], &cospi_p13_p19, &cospi_p19_m13, u);
  butterfly_32b(&in[1], &in[14], &cospi_p29_p03, &cospi_p03_m29, v);
  add_rnd(u, v, &x[6]);
  sub_rnd(u, v, &x[14]);

  // stage 2
  s[0] = _mm256_add_epi16(x[0], x[4]);
  s[1] = _mm256_add_epi16(x[1], x[5]);
  s[2] = _mm256_add_epi16(x[2], x[6]);
  s[3] = _mm256_add_epi16(x[3], x[7]);
  s[4] = _mm256_sub_epi16(x[0], x[4]);
  s[5] = _mm256_sub_epi16(x[1], x[5]);
  s[6] = _mm256_sub_epi16(x[2], x[6]);
  s[7] = _mm256_sub_epi16(x[3], x[7]);
  butterfly_32b(&x[8], &x[9], &cospi_p04_p28, &cospi_p28_m04, u);
  butterfly_32b(&x[12], &x[13], &cospi_m28_p04, &cospi_p04_p28, v);
  add_rnd(u, v, &s[8]);
  sub_rnd(u, v, &s[12]);

  butterfly_32b(&x[10], &x[11], &cospi_p20_p12, &cospi_p12_m20, u);
  butterfly_32b(&x[14], &x[15], &cospi_m12_p20, &cospi_p20_p12, v);
  add_rnd(u, v, &s[10]);
  sub_rnd(u, v, &s[14]);

  // stage 3
  x[0] = _mm256_add_epi16(s[0], s[2]);
  x[1] = _mm256_add_epi16(s[1], s[3]);
  x[2] = _mm256_sub_epi16(s[0], s[2]);
  x[3] = _mm256_sub_epi16(s[1], s[3]);

  x[8] = _mm256_add_epi16(s[8], s[10]);
  x[9] = _mm256_add_epi16(s[9], s[11]);
  x[10] = _mm256_sub_epi16(s[8], s[10]);
  x[11] = _mm256_sub_epi16(s[9], s[11]);

  butterfly_32b(&s[4], &s[5], &cospi_p08_p24, &cospi_p24_m08, u);
  butterfly_32b(&s[6], &s[7], &cospi_m24_p08, &cospi_p08_p24, v);
  add_rnd(u, v, &x[4]);
  sub_rnd(u, v, &x[6]);

  butterfly_32b(&s[12], &s[13], &cospi_p08_p24, &cospi_p24_m08, u);
  butterfly_32b(&s[14], &s[15], &cospi_m24_p08, &cospi_p08_p24, v);
  add_rnd(u, v, &x[12]);
  sub_rnd(u, v, &x[14]);

  // stage 4
  butterfly_32b(&x[2], &x[3], &cospi_m16_m16, &cospi_p16_m16, u);
  butterfly_32b(&x[6], &x[7], &cospi_p16_p16, &cospi_m16_p16, v);
  butterfly_rnd(u, &x[2]);
  butterfly_rnd(v, &x[6]);

  butterfly_32b(&x[10], &x[11], &cospi_p16_p16, &cospi_m16_p16, u);
  butterfly_32b(&x[14], &x[15], &cospi_m16_m16, &cospi_p16_m16, v);
  butterfly_rnd(u, &x[10]);
  butterfly_rnd(v, &x[14]);

  in[0] = x[0];
  in[1] = _mm256_sub_epi16(zero, x[8]);
  in[2] = x[12];
  in[3] = _mm256_sub_epi16(zero, x[4]);
  in[4] = x[6];
  in[5] = x[14];
  in[6] = x[10];
  in[7] = x[2];
  in[8] = x[3];
  in[9] = x[11];
  in[10] = x[15];
  in[11] = x[7];
  in[12] = x[5];
  in[13] = _mm256_sub_epi16(zero, x[13]);
  in[14] = x[9];
  in[15] = _mm256_sub_epi16(zero, x[1]);
}

static void iadst16(__m256i *in) {
  mm256_transpose_16x16(in, in);
  iadst16_avx2(in);
}

#if CONFIG_EXT_TX
static void flip_row(__m256i *in, int rows) {
  int i;
  for (i = 0; i < rows; ++i) {
    mm256_reverse_epi16(&in[i]);
  }
}

static void flip_col(uint8_t **dest, int *stride, int rows) {
  *dest = *dest + (rows - 1) * (*stride);
  *stride = -*stride;
}

static void iidtx16(__m256i *in) {
  mm256_transpose_16x16(in, in);
  txfm_scaling16_avx2((int16_t)Sqrt2, in);
}
#endif

void av1_iht16x16_256_add_avx2(const tran_low_t *input, uint8_t *dest,
                               int stride, const TxfmParam *txfm_param) {
  __m256i in[16];
  int tx_type = txfm_param->tx_type;

  load_buffer_16x16(input, in);
  switch (tx_type) {
    case DCT_DCT:
      idct16(in);
      idct16(in);
      break;
    case ADST_DCT:
      idct16(in);
      iadst16(in);
      break;
    case DCT_ADST:
      iadst16(in);
      idct16(in);
      break;
    case ADST_ADST:
      iadst16(in);
      iadst16(in);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      idct16(in);
      iadst16(in);
      flip_col(&dest, &stride, 16);
      break;
    case DCT_FLIPADST:
      iadst16(in);
      idct16(in);
      flip_row(in, 16);
      break;
    case FLIPADST_FLIPADST:
      iadst16(in);
      iadst16(in);
      flip_row(in, 16);
      flip_col(&dest, &stride, 16);
      break;
    case ADST_FLIPADST:
      iadst16(in);
      iadst16(in);
      flip_row(in, 16);
      break;
    case FLIPADST_ADST:
      iadst16(in);
      iadst16(in);
      flip_col(&dest, &stride, 16);
      break;
    case IDTX:
      iidtx16(in);
      iidtx16(in);
      break;
    case V_DCT:
      iidtx16(in);
      idct16(in);
      break;
    case H_DCT:
      idct16(in);
      iidtx16(in);
      break;
    case V_ADST:
      iidtx16(in);
      iadst16(in);
      break;
    case H_ADST:
      iadst16(in);
      iidtx16(in);
      break;
    case V_FLIPADST:
      iidtx16(in);
      iadst16(in);
      flip_col(&dest, &stride, 16);
      break;
    case H_FLIPADST:
      iadst16(in);
      iidtx16(in);
      flip_row(in, 16);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
  store_buffer_16xN(in, stride, dest, 16);
}
