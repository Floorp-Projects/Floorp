/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/inv_txfm.h"
#include "aom_dsp/x86/inv_txfm_common_avx2.h"
#include "aom_dsp/x86/txfm_common_avx2.h"

void aom_idct16x16_256_add_avx2(const tran_low_t *input, uint8_t *dest,
                                int stride) {
  __m256i in[16];
  load_buffer_16x16(input, in);
  mm256_transpose_16x16(in, in);
  av1_idct16_avx2(in);
  mm256_transpose_16x16(in, in);
  av1_idct16_avx2(in);
  store_buffer_16xN(in, stride, dest, 16);
}

static INLINE void transpose_col_to_row_nz4x4(__m256i *in /*in[4]*/) {
  const __m256i u0 = _mm256_unpacklo_epi16(in[0], in[1]);
  const __m256i u1 = _mm256_unpacklo_epi16(in[2], in[3]);
  const __m256i v0 = _mm256_unpacklo_epi32(u0, u1);
  const __m256i v1 = _mm256_unpackhi_epi32(u0, u1);
  in[0] = _mm256_permute4x64_epi64(v0, 0xA8);
  in[1] = _mm256_permute4x64_epi64(v0, 0xA9);
  in[2] = _mm256_permute4x64_epi64(v1, 0xA8);
  in[3] = _mm256_permute4x64_epi64(v1, 0xA9);
}

#define MM256_SHUFFLE_EPI64(x0, x1, imm8)                        \
  _mm256_castpd_si256(_mm256_shuffle_pd(_mm256_castsi256_pd(x0), \
                                        _mm256_castsi256_pd(x1), imm8))

static INLINE void transpose_col_to_row_nz4x16(__m256i *in /*in[16]*/) {
  int i;
  for (i = 0; i < 16; i += 4) {
    transpose_col_to_row_nz4x4(&in[i]);
  }

  for (i = 0; i < 4; ++i) {
    in[i] = MM256_SHUFFLE_EPI64(in[i], in[i + 4], 0);
    in[i + 8] = MM256_SHUFFLE_EPI64(in[i + 8], in[i + 12], 0);
  }

  for (i = 0; i < 4; ++i) {
    in[i] = _mm256_permute2x128_si256(in[i], in[i + 8], 0x20);
  }
}

// Coefficients 0-7 before the final butterfly
static INLINE void idct16_10_first_half(const __m256i *in, __m256i *out) {
  const __m256i c2p28 = pair256_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
  const __m256i c2p04 = pair256_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);
  const __m256i v4 = _mm256_mulhrs_epi16(in[2], c2p28);
  const __m256i v7 = _mm256_mulhrs_epi16(in[2], c2p04);

  const __m256i c2p16 = pair256_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
  const __m256i v0 = _mm256_mulhrs_epi16(in[0], c2p16);
  const __m256i v1 = v0;

  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  __m256i v5, v6;
  unpack_butter_fly(&v7, &v4, &cospi_p16_m16, &cospi_p16_p16, &v5, &v6);

  out[0] = _mm256_add_epi16(v0, v7);
  out[1] = _mm256_add_epi16(v1, v6);
  out[2] = _mm256_add_epi16(v1, v5);
  out[3] = _mm256_add_epi16(v0, v4);
  out[4] = _mm256_sub_epi16(v0, v4);
  out[5] = _mm256_sub_epi16(v1, v5);
  out[6] = _mm256_sub_epi16(v1, v6);
  out[7] = _mm256_sub_epi16(v0, v7);
}

// Coefficients 8-15 before the final butterfly
static INLINE void idct16_10_second_half(const __m256i *in, __m256i *out) {
  const __m256i c2p30 = pair256_set_epi16(2 * cospi_30_64, 2 * cospi_30_64);
  const __m256i c2p02 = pair256_set_epi16(2 * cospi_2_64, 2 * cospi_2_64);
  const __m256i t0 = _mm256_mulhrs_epi16(in[1], c2p30);
  const __m256i t7 = _mm256_mulhrs_epi16(in[1], c2p02);

  const __m256i c2m26 = pair256_set_epi16(-2 * cospi_26_64, -2 * cospi_26_64);
  const __m256i c2p06 = pair256_set_epi16(2 * cospi_6_64, 2 * cospi_6_64);
  const __m256i t3 = _mm256_mulhrs_epi16(in[3], c2m26);
  const __m256i t4 = _mm256_mulhrs_epi16(in[3], c2p06);

  const __m256i cospi_m08_p24 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i cospi_p24_p08 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i cospi_m24_m08 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);

  __m256i t1, t2, t5, t6;
  unpack_butter_fly(&t0, &t7, &cospi_m08_p24, &cospi_p24_p08, &t1, &t6);
  unpack_butter_fly(&t3, &t4, &cospi_m24_m08, &cospi_m08_p24, &t2, &t5);

  out[0] = _mm256_add_epi16(t0, t3);
  out[1] = _mm256_add_epi16(t1, t2);
  out[6] = _mm256_add_epi16(t6, t5);
  out[7] = _mm256_add_epi16(t7, t4);

  const __m256i v2 = _mm256_sub_epi16(t1, t2);
  const __m256i v3 = _mm256_sub_epi16(t0, t3);
  const __m256i v4 = _mm256_sub_epi16(t7, t4);
  const __m256i v5 = _mm256_sub_epi16(t6, t5);
  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  unpack_butter_fly(&v5, &v2, &cospi_p16_m16, &cospi_p16_p16, &out[2], &out[5]);
  unpack_butter_fly(&v4, &v3, &cospi_p16_m16, &cospi_p16_p16, &out[3], &out[4]);
}

static INLINE void add_sub_butterfly(const __m256i *in, __m256i *out,
                                     int size) {
  int i = 0;
  const int num = size >> 1;
  const int bound = size - 1;
  while (i < num) {
    out[i] = _mm256_add_epi16(in[i], in[bound - i]);
    out[bound - i] = _mm256_sub_epi16(in[i], in[bound - i]);
    i++;
  }
}

static INLINE void idct16_10(__m256i *in /*in[16]*/) {
  __m256i out[16];
  idct16_10_first_half(in, out);
  idct16_10_second_half(in, &out[8]);
  add_sub_butterfly(out, in, 16);
}

void aom_idct16x16_10_add_avx2(const tran_low_t *input, uint8_t *dest,
                               int stride) {
  __m256i in[16];

  load_coeff(input, &in[0]);
  load_coeff(input + 16, &in[1]);
  load_coeff(input + 32, &in[2]);
  load_coeff(input + 48, &in[3]);

  transpose_col_to_row_nz4x4(in);
  idct16_10(in);

  transpose_col_to_row_nz4x16(in);
  idct16_10(in);

  store_buffer_16xN(in, stride, dest, 16);
}

// Note:
//  For 16x16 int16_t matrix
//  transpose first 8 columns into first 8 rows.
//  Since only upper-left 8x8 are non-zero, the input are first 8 rows (in[8]).
//  After transposing, the 8 row vectors are in in[8].
void transpose_col_to_row_nz8x8(__m256i *in /*in[8]*/) {
  __m256i u0 = _mm256_unpacklo_epi16(in[0], in[1]);
  __m256i u1 = _mm256_unpackhi_epi16(in[0], in[1]);
  __m256i u2 = _mm256_unpacklo_epi16(in[2], in[3]);
  __m256i u3 = _mm256_unpackhi_epi16(in[2], in[3]);

  const __m256i v0 = _mm256_unpacklo_epi32(u0, u2);
  const __m256i v1 = _mm256_unpackhi_epi32(u0, u2);
  const __m256i v2 = _mm256_unpacklo_epi32(u1, u3);
  const __m256i v3 = _mm256_unpackhi_epi32(u1, u3);

  u0 = _mm256_unpacklo_epi16(in[4], in[5]);
  u1 = _mm256_unpackhi_epi16(in[4], in[5]);
  u2 = _mm256_unpacklo_epi16(in[6], in[7]);
  u3 = _mm256_unpackhi_epi16(in[6], in[7]);

  const __m256i v4 = _mm256_unpacklo_epi32(u0, u2);
  const __m256i v5 = _mm256_unpackhi_epi32(u0, u2);
  const __m256i v6 = _mm256_unpacklo_epi32(u1, u3);
  const __m256i v7 = _mm256_unpackhi_epi32(u1, u3);

  in[0] = MM256_SHUFFLE_EPI64(v0, v4, 0);
  in[1] = MM256_SHUFFLE_EPI64(v0, v4, 3);
  in[2] = MM256_SHUFFLE_EPI64(v1, v5, 0);
  in[3] = MM256_SHUFFLE_EPI64(v1, v5, 3);
  in[4] = MM256_SHUFFLE_EPI64(v2, v6, 0);
  in[5] = MM256_SHUFFLE_EPI64(v2, v6, 3);
  in[6] = MM256_SHUFFLE_EPI64(v3, v7, 0);
  in[7] = MM256_SHUFFLE_EPI64(v3, v7, 3);
}

// Note:
//  For 16x16 int16_t matrix
//  transpose first 8 columns into first 8 rows.
//  Since only matrix left 8x16 are non-zero, the input are total 16 rows
//  (in[16]).
//  After transposing, the 8 row vectors are in in[8]. All else are zero.
static INLINE void transpose_col_to_row_nz8x16(__m256i *in /*in[16]*/) {
  transpose_col_to_row_nz8x8(in);
  transpose_col_to_row_nz8x8(&in[8]);

  int i;
  for (i = 0; i < 8; ++i) {
    in[i] = _mm256_permute2x128_si256(in[i], in[i + 8], 0x20);
  }
}

static INLINE void idct16_38_first_half(const __m256i *in, __m256i *out) {
  const __m256i c2p28 = pair256_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
  const __m256i c2p04 = pair256_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);
  __m256i t4 = _mm256_mulhrs_epi16(in[2], c2p28);
  __m256i t7 = _mm256_mulhrs_epi16(in[2], c2p04);

  const __m256i c2m20 = pair256_set_epi16(-2 * cospi_20_64, -2 * cospi_20_64);
  const __m256i c2p12 = pair256_set_epi16(2 * cospi_12_64, 2 * cospi_12_64);
  __m256i t5 = _mm256_mulhrs_epi16(in[6], c2m20);
  __m256i t6 = _mm256_mulhrs_epi16(in[6], c2p12);

  const __m256i c2p16 = pair256_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
  const __m256i c2p24 = pair256_set_epi16(2 * cospi_24_64, 2 * cospi_24_64);
  const __m256i c2p08 = pair256_set_epi16(2 * cospi_8_64, 2 * cospi_8_64);
  const __m256i u0 = _mm256_mulhrs_epi16(in[0], c2p16);
  const __m256i u1 = _mm256_mulhrs_epi16(in[0], c2p16);
  const __m256i u2 = _mm256_mulhrs_epi16(in[4], c2p24);
  const __m256i u3 = _mm256_mulhrs_epi16(in[4], c2p08);

  const __m256i u4 = _mm256_add_epi16(t4, t5);
  const __m256i u5 = _mm256_sub_epi16(t4, t5);
  const __m256i u6 = _mm256_sub_epi16(t7, t6);
  const __m256i u7 = _mm256_add_epi16(t7, t6);

  const __m256i t0 = _mm256_add_epi16(u0, u3);
  const __m256i t1 = _mm256_add_epi16(u1, u2);
  const __m256i t2 = _mm256_sub_epi16(u1, u2);
  const __m256i t3 = _mm256_sub_epi16(u0, u3);

  t4 = u4;
  t7 = u7;

  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  unpack_butter_fly(&u6, &u5, &cospi_p16_m16, &cospi_p16_p16, &t5, &t6);

  out[0] = _mm256_add_epi16(t0, t7);
  out[1] = _mm256_add_epi16(t1, t6);
  out[2] = _mm256_add_epi16(t2, t5);
  out[3] = _mm256_add_epi16(t3, t4);
  out[4] = _mm256_sub_epi16(t3, t4);
  out[5] = _mm256_sub_epi16(t2, t5);
  out[6] = _mm256_sub_epi16(t1, t6);
  out[7] = _mm256_sub_epi16(t0, t7);
}

static INLINE void idct16_38_second_half(const __m256i *in, __m256i *out) {
  const __m256i c2p30 = pair256_set_epi16(2 * cospi_30_64, 2 * cospi_30_64);
  const __m256i c2p02 = pair256_set_epi16(2 * cospi_2_64, 2 * cospi_2_64);
  __m256i t0 = _mm256_mulhrs_epi16(in[1], c2p30);
  __m256i t7 = _mm256_mulhrs_epi16(in[1], c2p02);

  const __m256i c2m18 = pair256_set_epi16(-2 * cospi_18_64, -2 * cospi_18_64);
  const __m256i c2p14 = pair256_set_epi16(2 * cospi_14_64, 2 * cospi_14_64);
  __m256i t1 = _mm256_mulhrs_epi16(in[7], c2m18);
  __m256i t6 = _mm256_mulhrs_epi16(in[7], c2p14);

  const __m256i c2p22 = pair256_set_epi16(2 * cospi_22_64, 2 * cospi_22_64);
  const __m256i c2p10 = pair256_set_epi16(2 * cospi_10_64, 2 * cospi_10_64);
  __m256i t2 = _mm256_mulhrs_epi16(in[5], c2p22);
  __m256i t5 = _mm256_mulhrs_epi16(in[5], c2p10);

  const __m256i c2m26 = pair256_set_epi16(-2 * cospi_26_64, -2 * cospi_26_64);
  const __m256i c2p06 = pair256_set_epi16(2 * cospi_6_64, 2 * cospi_6_64);
  __m256i t3 = _mm256_mulhrs_epi16(in[3], c2m26);
  __m256i t4 = _mm256_mulhrs_epi16(in[3], c2p06);

  __m256i v0, v1, v2, v3, v4, v5, v6, v7;
  v0 = _mm256_add_epi16(t0, t1);
  v1 = _mm256_sub_epi16(t0, t1);
  v2 = _mm256_sub_epi16(t3, t2);
  v3 = _mm256_add_epi16(t2, t3);
  v4 = _mm256_add_epi16(t4, t5);
  v5 = _mm256_sub_epi16(t4, t5);
  v6 = _mm256_sub_epi16(t7, t6);
  v7 = _mm256_add_epi16(t6, t7);

  t0 = v0;
  t7 = v7;
  t3 = v3;
  t4 = v4;
  const __m256i cospi_m08_p24 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i cospi_p24_p08 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i cospi_m24_m08 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
  unpack_butter_fly(&v1, &v6, &cospi_m08_p24, &cospi_p24_p08, &t1, &t6);
  unpack_butter_fly(&v2, &v5, &cospi_m24_m08, &cospi_m08_p24, &t2, &t5);

  v0 = _mm256_add_epi16(t0, t3);
  v1 = _mm256_add_epi16(t1, t2);
  v2 = _mm256_sub_epi16(t1, t2);
  v3 = _mm256_sub_epi16(t0, t3);
  v4 = _mm256_sub_epi16(t7, t4);
  v5 = _mm256_sub_epi16(t6, t5);
  v6 = _mm256_add_epi16(t6, t5);
  v7 = _mm256_add_epi16(t7, t4);

  // stage 6, (8-15)
  out[0] = v0;
  out[1] = v1;
  out[6] = v6;
  out[7] = v7;
  const __m256i cospi_p16_p16 = _mm256_set1_epi16((int16_t)cospi_16_64);
  const __m256i cospi_p16_m16 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  unpack_butter_fly(&v5, &v2, &cospi_p16_m16, &cospi_p16_p16, &out[2], &out[5]);
  unpack_butter_fly(&v4, &v3, &cospi_p16_m16, &cospi_p16_p16, &out[3], &out[4]);
}

static INLINE void idct16_38(__m256i *in /*in[16]*/) {
  __m256i out[16];
  idct16_38_first_half(in, out);
  idct16_38_second_half(in, &out[8]);
  add_sub_butterfly(out, in, 16);
}

void aom_idct16x16_38_add_avx2(const tran_low_t *input, uint8_t *dest,
                               int stride) {
  __m256i in[16];

  int i;
  for (i = 0; i < 8; ++i) {
    load_coeff(input + (i << 4), &in[i]);
  }

  transpose_col_to_row_nz8x8(in);
  idct16_38(in);

  transpose_col_to_row_nz8x16(in);
  idct16_38(in);

  store_buffer_16xN(in, stride, dest, 16);
}

static INLINE int calculate_dc(const tran_low_t *input) {
  int dc = (int)dct_const_round_shift(input[0] * cospi_16_64);
  dc = (int)dct_const_round_shift(dc * cospi_16_64);
  dc = ROUND_POWER_OF_TWO(dc, IDCT_ROUNDING_POS);
  return dc;
}

void aom_idct16x16_1_add_avx2(const tran_low_t *input, uint8_t *dest,
                              int stride) {
  const int dc = calculate_dc(input);
  if (dc == 0) return;

  const __m256i dc_value = _mm256_set1_epi16(dc);

  int i;
  for (i = 0; i < 16; ++i) {
    recon_and_store(&dc_value, dest);
    dest += stride;
  }
}

// -----------------------------------------------------------------------------
// 32x32 partial IDCT

void aom_idct32x32_1_add_avx2(const tran_low_t *input, uint8_t *dest,
                              int stride) {
  const int dc = calculate_dc(input);
  if (dc == 0) return;

  const __m256i dc_value = _mm256_set1_epi16(dc);

  int i;
  for (i = 0; i < 32; ++i) {
    recon_and_store(&dc_value, dest);
    recon_and_store(&dc_value, dest + 16);
    dest += stride;
  }
}

static void load_buffer_32x16(const tran_low_t *input, __m256i *in /*in[32]*/) {
  int i;
  for (i = 0; i < 16; ++i) {
    load_coeff(input, &in[i]);
    load_coeff(input + 16, &in[i + 16]);
    input += 32;
  }
}

// Note:
//  We extend SSSE3 operations to AVX2. Instead of operating on __m128i, we
// operate coefficients on __m256i. Our operation capacity doubles for each
// instruction.
#define BUTTERFLY_PAIR(x0, x1, co0, co1)            \
  do {                                              \
    tmp0 = _mm256_madd_epi16(x0, co0);              \
    tmp1 = _mm256_madd_epi16(x1, co0);              \
    tmp2 = _mm256_madd_epi16(x0, co1);              \
    tmp3 = _mm256_madd_epi16(x1, co1);              \
    tmp0 = _mm256_add_epi32(tmp0, rounding);        \
    tmp1 = _mm256_add_epi32(tmp1, rounding);        \
    tmp2 = _mm256_add_epi32(tmp2, rounding);        \
    tmp3 = _mm256_add_epi32(tmp3, rounding);        \
    tmp0 = _mm256_srai_epi32(tmp0, DCT_CONST_BITS); \
    tmp1 = _mm256_srai_epi32(tmp1, DCT_CONST_BITS); \
    tmp2 = _mm256_srai_epi32(tmp2, DCT_CONST_BITS); \
    tmp3 = _mm256_srai_epi32(tmp3, DCT_CONST_BITS); \
  } while (0)

static INLINE void butterfly(const __m256i *x0, const __m256i *x1,
                             const __m256i *c0, const __m256i *c1, __m256i *y0,
                             __m256i *y1) {
  __m256i tmp0, tmp1, tmp2, tmp3, u0, u1;
  const __m256i rounding = _mm256_set1_epi32(DCT_CONST_ROUNDING);

  u0 = _mm256_unpacklo_epi16(*x0, *x1);
  u1 = _mm256_unpackhi_epi16(*x0, *x1);
  BUTTERFLY_PAIR(u0, u1, *c0, *c1);
  *y0 = _mm256_packs_epi32(tmp0, tmp1);
  *y1 = _mm256_packs_epi32(tmp2, tmp3);
}

static INLINE void butterfly_self(__m256i *x0, __m256i *x1, const __m256i *c0,
                                  const __m256i *c1) {
  __m256i tmp0, tmp1, tmp2, tmp3, u0, u1;
  const __m256i rounding = _mm256_set1_epi32(DCT_CONST_ROUNDING);

  u0 = _mm256_unpacklo_epi16(*x0, *x1);
  u1 = _mm256_unpackhi_epi16(*x0, *x1);
  BUTTERFLY_PAIR(u0, u1, *c0, *c1);
  *x0 = _mm256_packs_epi32(tmp0, tmp1);
  *x1 = _mm256_packs_epi32(tmp2, tmp3);
}

// For each 16x32 block __m256i in[32],
// Input with index, 2, 6, 10, 14, 18, 22, 26, 30
// output pixels: 8-15 in __m256i in[32]
static void idct32_full_16x32_quarter_2(const __m256i *in /*in[32]*/,
                                        __m256i *out /*out[16]*/) {
  __m256i u8, u9, u10, u11, u12, u13, u14, u15;  // stp2_
  __m256i v8, v9, v10, v11, v12, v13, v14, v15;  // stp1_

  {
    const __m256i stg2_0 = pair256_set_epi16(cospi_30_64, -cospi_2_64);
    const __m256i stg2_1 = pair256_set_epi16(cospi_2_64, cospi_30_64);
    const __m256i stg2_2 = pair256_set_epi16(cospi_14_64, -cospi_18_64);
    const __m256i stg2_3 = pair256_set_epi16(cospi_18_64, cospi_14_64);
    butterfly(&in[2], &in[30], &stg2_0, &stg2_1, &u8, &u15);
    butterfly(&in[18], &in[14], &stg2_2, &stg2_3, &u9, &u14);
  }

  v8 = _mm256_add_epi16(u8, u9);
  v9 = _mm256_sub_epi16(u8, u9);
  v14 = _mm256_sub_epi16(u15, u14);
  v15 = _mm256_add_epi16(u15, u14);

  {
    const __m256i stg2_4 = pair256_set_epi16(cospi_22_64, -cospi_10_64);
    const __m256i stg2_5 = pair256_set_epi16(cospi_10_64, cospi_22_64);
    const __m256i stg2_6 = pair256_set_epi16(cospi_6_64, -cospi_26_64);
    const __m256i stg2_7 = pair256_set_epi16(cospi_26_64, cospi_6_64);
    butterfly(&in[10], &in[22], &stg2_4, &stg2_5, &u10, &u13);
    butterfly(&in[26], &in[6], &stg2_6, &stg2_7, &u11, &u12);
  }

  v10 = _mm256_sub_epi16(u11, u10);
  v11 = _mm256_add_epi16(u11, u10);
  v12 = _mm256_add_epi16(u12, u13);
  v13 = _mm256_sub_epi16(u12, u13);

  {
    const __m256i stg4_4 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
    const __m256i stg4_5 = pair256_set_epi16(cospi_24_64, cospi_8_64);
    const __m256i stg4_6 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
    butterfly_self(&v9, &v14, &stg4_4, &stg4_5);
    butterfly_self(&v10, &v13, &stg4_6, &stg4_4);
  }

  out[0] = _mm256_add_epi16(v8, v11);
  out[1] = _mm256_add_epi16(v9, v10);
  out[6] = _mm256_add_epi16(v14, v13);
  out[7] = _mm256_add_epi16(v15, v12);

  out[2] = _mm256_sub_epi16(v9, v10);
  out[3] = _mm256_sub_epi16(v8, v11);
  out[4] = _mm256_sub_epi16(v15, v12);
  out[5] = _mm256_sub_epi16(v14, v13);

  {
    const __m256i stg4_0 = pair256_set_epi16(cospi_16_64, cospi_16_64);
    const __m256i stg6_0 = pair256_set_epi16(-cospi_16_64, cospi_16_64);
    butterfly_self(&out[2], &out[5], &stg6_0, &stg4_0);
    butterfly_self(&out[3], &out[4], &stg6_0, &stg4_0);
  }
}

// For each 8x32 block __m256i in[32],
// Input with index, 0, 4, 8, 12, 16, 20, 24, 28
// output pixels: 0-7 in __m256i in[32]
static void idct32_full_16x32_quarter_1(const __m256i *in /*in[32]*/,
                                        __m256i *out /*out[8]*/) {
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;  // stp1_
  __m256i v0, v1, v2, v3, v4, v5, v6, v7;  // stp2_

  {
    const __m256i stg3_0 = pair256_set_epi16(cospi_28_64, -cospi_4_64);
    const __m256i stg3_1 = pair256_set_epi16(cospi_4_64, cospi_28_64);
    const __m256i stg3_2 = pair256_set_epi16(cospi_12_64, -cospi_20_64);
    const __m256i stg3_3 = pair256_set_epi16(cospi_20_64, cospi_12_64);
    butterfly(&in[4], &in[28], &stg3_0, &stg3_1, &u4, &u7);
    butterfly(&in[20], &in[12], &stg3_2, &stg3_3, &u5, &u6);
  }

  v4 = _mm256_add_epi16(u4, u5);
  v5 = _mm256_sub_epi16(u4, u5);
  v6 = _mm256_sub_epi16(u7, u6);
  v7 = _mm256_add_epi16(u7, u6);

  {
    const __m256i stg4_0 = pair256_set_epi16(cospi_16_64, cospi_16_64);
    const __m256i stg4_1 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
    const __m256i stg4_2 = pair256_set_epi16(cospi_24_64, -cospi_8_64);
    const __m256i stg4_3 = pair256_set_epi16(cospi_8_64, cospi_24_64);
    butterfly(&v6, &v5, &stg4_1, &stg4_0, &v5, &v6);

    butterfly(&in[0], &in[16], &stg4_0, &stg4_1, &u0, &u1);
    butterfly(&in[8], &in[24], &stg4_2, &stg4_3, &u2, &u3);
  }

  v0 = _mm256_add_epi16(u0, u3);
  v1 = _mm256_add_epi16(u1, u2);
  v2 = _mm256_sub_epi16(u1, u2);
  v3 = _mm256_sub_epi16(u0, u3);

  out[0] = _mm256_add_epi16(v0, v7);
  out[1] = _mm256_add_epi16(v1, v6);
  out[2] = _mm256_add_epi16(v2, v5);
  out[3] = _mm256_add_epi16(v3, v4);
  out[4] = _mm256_sub_epi16(v3, v4);
  out[5] = _mm256_sub_epi16(v2, v5);
  out[6] = _mm256_sub_epi16(v1, v6);
  out[7] = _mm256_sub_epi16(v0, v7);
}

// For each 8x32 block __m256i in[32],
// Input with odd index,
// 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
// output pixels: 16-23, 24-31 in __m256i in[32]
// We avoid hide an offset, 16, inside this function. So we output 0-15 into
// array out[16]
static void idct32_full_16x32_quarter_3_4(const __m256i *in /*in[32]*/,
                                          __m256i *out /*out[16]*/) {
  __m256i v16, v17, v18, v19, v20, v21, v22, v23;
  __m256i v24, v25, v26, v27, v28, v29, v30, v31;
  __m256i u16, u17, u18, u19, u20, u21, u22, u23;
  __m256i u24, u25, u26, u27, u28, u29, u30, u31;

  {
    const __m256i stg1_0 = pair256_set_epi16(cospi_31_64, -cospi_1_64);
    const __m256i stg1_1 = pair256_set_epi16(cospi_1_64, cospi_31_64);
    const __m256i stg1_2 = pair256_set_epi16(cospi_15_64, -cospi_17_64);
    const __m256i stg1_3 = pair256_set_epi16(cospi_17_64, cospi_15_64);
    const __m256i stg1_4 = pair256_set_epi16(cospi_23_64, -cospi_9_64);
    const __m256i stg1_5 = pair256_set_epi16(cospi_9_64, cospi_23_64);
    const __m256i stg1_6 = pair256_set_epi16(cospi_7_64, -cospi_25_64);
    const __m256i stg1_7 = pair256_set_epi16(cospi_25_64, cospi_7_64);
    const __m256i stg1_8 = pair256_set_epi16(cospi_27_64, -cospi_5_64);
    const __m256i stg1_9 = pair256_set_epi16(cospi_5_64, cospi_27_64);
    const __m256i stg1_10 = pair256_set_epi16(cospi_11_64, -cospi_21_64);
    const __m256i stg1_11 = pair256_set_epi16(cospi_21_64, cospi_11_64);
    const __m256i stg1_12 = pair256_set_epi16(cospi_19_64, -cospi_13_64);
    const __m256i stg1_13 = pair256_set_epi16(cospi_13_64, cospi_19_64);
    const __m256i stg1_14 = pair256_set_epi16(cospi_3_64, -cospi_29_64);
    const __m256i stg1_15 = pair256_set_epi16(cospi_29_64, cospi_3_64);
    butterfly(&in[1], &in[31], &stg1_0, &stg1_1, &u16, &u31);
    butterfly(&in[17], &in[15], &stg1_2, &stg1_3, &u17, &u30);
    butterfly(&in[9], &in[23], &stg1_4, &stg1_5, &u18, &u29);
    butterfly(&in[25], &in[7], &stg1_6, &stg1_7, &u19, &u28);

    butterfly(&in[5], &in[27], &stg1_8, &stg1_9, &u20, &u27);
    butterfly(&in[21], &in[11], &stg1_10, &stg1_11, &u21, &u26);

    butterfly(&in[13], &in[19], &stg1_12, &stg1_13, &u22, &u25);
    butterfly(&in[29], &in[3], &stg1_14, &stg1_15, &u23, &u24);
  }

  v16 = _mm256_add_epi16(u16, u17);
  v17 = _mm256_sub_epi16(u16, u17);
  v18 = _mm256_sub_epi16(u19, u18);
  v19 = _mm256_add_epi16(u19, u18);

  v20 = _mm256_add_epi16(u20, u21);
  v21 = _mm256_sub_epi16(u20, u21);
  v22 = _mm256_sub_epi16(u23, u22);
  v23 = _mm256_add_epi16(u23, u22);

  v24 = _mm256_add_epi16(u24, u25);
  v25 = _mm256_sub_epi16(u24, u25);
  v26 = _mm256_sub_epi16(u27, u26);
  v27 = _mm256_add_epi16(u27, u26);

  v28 = _mm256_add_epi16(u28, u29);
  v29 = _mm256_sub_epi16(u28, u29);
  v30 = _mm256_sub_epi16(u31, u30);
  v31 = _mm256_add_epi16(u31, u30);

  {
    const __m256i stg3_4 = pair256_set_epi16(-cospi_4_64, cospi_28_64);
    const __m256i stg3_5 = pair256_set_epi16(cospi_28_64, cospi_4_64);
    const __m256i stg3_6 = pair256_set_epi16(-cospi_28_64, -cospi_4_64);
    const __m256i stg3_8 = pair256_set_epi16(-cospi_20_64, cospi_12_64);
    const __m256i stg3_9 = pair256_set_epi16(cospi_12_64, cospi_20_64);
    const __m256i stg3_10 = pair256_set_epi16(-cospi_12_64, -cospi_20_64);
    butterfly_self(&v17, &v30, &stg3_4, &stg3_5);
    butterfly_self(&v18, &v29, &stg3_6, &stg3_4);
    butterfly_self(&v21, &v26, &stg3_8, &stg3_9);
    butterfly_self(&v22, &v25, &stg3_10, &stg3_8);
  }

  u16 = _mm256_add_epi16(v16, v19);
  u17 = _mm256_add_epi16(v17, v18);
  u18 = _mm256_sub_epi16(v17, v18);
  u19 = _mm256_sub_epi16(v16, v19);
  u20 = _mm256_sub_epi16(v23, v20);
  u21 = _mm256_sub_epi16(v22, v21);
  u22 = _mm256_add_epi16(v22, v21);
  u23 = _mm256_add_epi16(v23, v20);

  u24 = _mm256_add_epi16(v24, v27);
  u25 = _mm256_add_epi16(v25, v26);
  u26 = _mm256_sub_epi16(v25, v26);
  u27 = _mm256_sub_epi16(v24, v27);

  u28 = _mm256_sub_epi16(v31, v28);
  u29 = _mm256_sub_epi16(v30, v29);
  u30 = _mm256_add_epi16(v29, v30);
  u31 = _mm256_add_epi16(v28, v31);

  {
    const __m256i stg4_4 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
    const __m256i stg4_5 = pair256_set_epi16(cospi_24_64, cospi_8_64);
    const __m256i stg4_6 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
    butterfly_self(&u18, &u29, &stg4_4, &stg4_5);
    butterfly_self(&u19, &u28, &stg4_4, &stg4_5);
    butterfly_self(&u20, &u27, &stg4_6, &stg4_4);
    butterfly_self(&u21, &u26, &stg4_6, &stg4_4);
  }

  out[0] = _mm256_add_epi16(u16, u23);
  out[1] = _mm256_add_epi16(u17, u22);
  out[2] = _mm256_add_epi16(u18, u21);
  out[3] = _mm256_add_epi16(u19, u20);
  out[4] = _mm256_sub_epi16(u19, u20);
  out[5] = _mm256_sub_epi16(u18, u21);
  out[6] = _mm256_sub_epi16(u17, u22);
  out[7] = _mm256_sub_epi16(u16, u23);

  out[8] = _mm256_sub_epi16(u31, u24);
  out[9] = _mm256_sub_epi16(u30, u25);
  out[10] = _mm256_sub_epi16(u29, u26);
  out[11] = _mm256_sub_epi16(u28, u27);
  out[12] = _mm256_add_epi16(u27, u28);
  out[13] = _mm256_add_epi16(u26, u29);
  out[14] = _mm256_add_epi16(u25, u30);
  out[15] = _mm256_add_epi16(u24, u31);

  {
    const __m256i stg4_0 = pair256_set_epi16(cospi_16_64, cospi_16_64);
    const __m256i stg6_0 = pair256_set_epi16(-cospi_16_64, cospi_16_64);
    butterfly_self(&out[4], &out[11], &stg6_0, &stg4_0);
    butterfly_self(&out[5], &out[10], &stg6_0, &stg4_0);
    butterfly_self(&out[6], &out[9], &stg6_0, &stg4_0);
    butterfly_self(&out[7], &out[8], &stg6_0, &stg4_0);
  }
}

static void idct32_full_16x32_quarter_1_2(const __m256i *in /*in[32]*/,
                                          __m256i *out /*out[32]*/) {
  __m256i temp[16];
  idct32_full_16x32_quarter_1(in, temp);
  idct32_full_16x32_quarter_2(in, &temp[8]);
  add_sub_butterfly(temp, out, 16);
}

static void idct32_16x32(const __m256i *in /*in[32]*/,
                         __m256i *out /*out[32]*/) {
  __m256i temp[32];
  idct32_full_16x32_quarter_1_2(in, temp);
  idct32_full_16x32_quarter_3_4(in, &temp[16]);
  add_sub_butterfly(temp, out, 32);
}

void aom_idct32x32_1024_add_avx2(const tran_low_t *input, uint8_t *dest,
                                 int stride) {
  __m256i col[64], in[32];
  int i;

  for (i = 0; i < 2; ++i) {
    load_buffer_32x16(input, in);
    input += 32 << 4;

    mm256_transpose_16x16(in, in);
    mm256_transpose_16x16(&in[16], &in[16]);
    idct32_16x32(in, col + (i << 5));
  }

  for (i = 0; i < 2; ++i) {
    int j = i << 4;
    mm256_transpose_16x16(col + j, in);
    mm256_transpose_16x16(col + j + 32, &in[16]);
    idct32_16x32(in, in);
    store_buffer_16xN(in, stride, dest, 32);
    dest += 16;
  }
}

// Group the coefficient calculation into smaller functions
// to prevent stack spillover:
// quarter_1: 0-7
// quarter_2: 8-15
// quarter_3_4: 16-23, 24-31
static void idct32_16x32_135_quarter_1(const __m256i *in /*in[16]*/,
                                       __m256i *out /*out[8]*/) {
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i v0, v1, v2, v3, v4, v5, v6, v7;

  {
    const __m256i stk4_0 = pair256_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
    const __m256i stk4_2 = pair256_set_epi16(2 * cospi_24_64, 2 * cospi_24_64);
    const __m256i stk4_3 = pair256_set_epi16(2 * cospi_8_64, 2 * cospi_8_64);
    u0 = _mm256_mulhrs_epi16(in[0], stk4_0);
    u2 = _mm256_mulhrs_epi16(in[8], stk4_2);
    u3 = _mm256_mulhrs_epi16(in[8], stk4_3);
    u1 = u0;
  }

  v0 = _mm256_add_epi16(u0, u3);
  v1 = _mm256_add_epi16(u1, u2);
  v2 = _mm256_sub_epi16(u1, u2);
  v3 = _mm256_sub_epi16(u0, u3);

  {
    const __m256i stk3_0 = pair256_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
    const __m256i stk3_1 = pair256_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);
    const __m256i stk3_2 =
        pair256_set_epi16(-2 * cospi_20_64, -2 * cospi_20_64);
    const __m256i stk3_3 = pair256_set_epi16(2 * cospi_12_64, 2 * cospi_12_64);
    u4 = _mm256_mulhrs_epi16(in[4], stk3_0);
    u7 = _mm256_mulhrs_epi16(in[4], stk3_1);
    u5 = _mm256_mulhrs_epi16(in[12], stk3_2);
    u6 = _mm256_mulhrs_epi16(in[12], stk3_3);
  }

  v4 = _mm256_add_epi16(u4, u5);
  v5 = _mm256_sub_epi16(u4, u5);
  v6 = _mm256_sub_epi16(u7, u6);
  v7 = _mm256_add_epi16(u7, u6);

  {
    const __m256i stg4_0 = pair256_set_epi16(cospi_16_64, cospi_16_64);
    const __m256i stg4_1 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
    butterfly(&v6, &v5, &stg4_1, &stg4_0, &v5, &v6);
  }

  out[0] = _mm256_add_epi16(v0, v7);
  out[1] = _mm256_add_epi16(v1, v6);
  out[2] = _mm256_add_epi16(v2, v5);
  out[3] = _mm256_add_epi16(v3, v4);
  out[4] = _mm256_sub_epi16(v3, v4);
  out[5] = _mm256_sub_epi16(v2, v5);
  out[6] = _mm256_sub_epi16(v1, v6);
  out[7] = _mm256_sub_epi16(v0, v7);
}

static void idct32_16x32_135_quarter_2(const __m256i *in /*in[16]*/,
                                       __m256i *out /*out[8]*/) {
  __m256i u8, u9, u10, u11, u12, u13, u14, u15;
  __m256i v8, v9, v10, v11, v12, v13, v14, v15;

  {
    const __m256i stk2_0 = pair256_set_epi16(2 * cospi_30_64, 2 * cospi_30_64);
    const __m256i stk2_1 = pair256_set_epi16(2 * cospi_2_64, 2 * cospi_2_64);
    const __m256i stk2_2 =
        pair256_set_epi16(-2 * cospi_18_64, -2 * cospi_18_64);
    const __m256i stk2_3 = pair256_set_epi16(2 * cospi_14_64, 2 * cospi_14_64);
    const __m256i stk2_4 = pair256_set_epi16(2 * cospi_22_64, 2 * cospi_22_64);
    const __m256i stk2_5 = pair256_set_epi16(2 * cospi_10_64, 2 * cospi_10_64);
    const __m256i stk2_6 =
        pair256_set_epi16(-2 * cospi_26_64, -2 * cospi_26_64);
    const __m256i stk2_7 = pair256_set_epi16(2 * cospi_6_64, 2 * cospi_6_64);
    u8 = _mm256_mulhrs_epi16(in[2], stk2_0);
    u15 = _mm256_mulhrs_epi16(in[2], stk2_1);
    u9 = _mm256_mulhrs_epi16(in[14], stk2_2);
    u14 = _mm256_mulhrs_epi16(in[14], stk2_3);
    u10 = _mm256_mulhrs_epi16(in[10], stk2_4);
    u13 = _mm256_mulhrs_epi16(in[10], stk2_5);
    u11 = _mm256_mulhrs_epi16(in[6], stk2_6);
    u12 = _mm256_mulhrs_epi16(in[6], stk2_7);
  }

  v8 = _mm256_add_epi16(u8, u9);
  v9 = _mm256_sub_epi16(u8, u9);
  v10 = _mm256_sub_epi16(u11, u10);
  v11 = _mm256_add_epi16(u11, u10);
  v12 = _mm256_add_epi16(u12, u13);
  v13 = _mm256_sub_epi16(u12, u13);
  v14 = _mm256_sub_epi16(u15, u14);
  v15 = _mm256_add_epi16(u15, u14);

  {
    const __m256i stg4_4 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
    const __m256i stg4_5 = pair256_set_epi16(cospi_24_64, cospi_8_64);
    const __m256i stg4_6 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
    butterfly_self(&v9, &v14, &stg4_4, &stg4_5);
    butterfly_self(&v10, &v13, &stg4_6, &stg4_4);
  }

  out[0] = _mm256_add_epi16(v8, v11);
  out[1] = _mm256_add_epi16(v9, v10);
  out[2] = _mm256_sub_epi16(v9, v10);
  out[3] = _mm256_sub_epi16(v8, v11);
  out[4] = _mm256_sub_epi16(v15, v12);
  out[5] = _mm256_sub_epi16(v14, v13);
  out[6] = _mm256_add_epi16(v14, v13);
  out[7] = _mm256_add_epi16(v15, v12);

  {
    const __m256i stg4_0 = pair256_set_epi16(cospi_16_64, cospi_16_64);
    const __m256i stg6_0 = pair256_set_epi16(-cospi_16_64, cospi_16_64);
    butterfly_self(&out[2], &out[5], &stg6_0, &stg4_0);
    butterfly_self(&out[3], &out[4], &stg6_0, &stg4_0);
  }
}

// 8x32 block even indexed 8 inputs of in[16],
// output first half 16 to out[32]
static void idct32_16x32_quarter_1_2(const __m256i *in /*in[16]*/,
                                     __m256i *out /*out[32]*/) {
  __m256i temp[16];
  idct32_16x32_135_quarter_1(in, temp);
  idct32_16x32_135_quarter_2(in, &temp[8]);
  add_sub_butterfly(temp, out, 16);
}

// 8x32 block odd indexed 8 inputs of in[16],
// output second half 16 to out[32]
static void idct32_16x32_quarter_3_4(const __m256i *in /*in[16]*/,
                                     __m256i *out /*out[32]*/) {
  __m256i v16, v17, v18, v19, v20, v21, v22, v23;
  __m256i v24, v25, v26, v27, v28, v29, v30, v31;
  __m256i u16, u17, u18, u19, u20, u21, u22, u23;
  __m256i u24, u25, u26, u27, u28, u29, u30, u31;

  {
    const __m256i stk1_0 = pair256_set_epi16(2 * cospi_31_64, 2 * cospi_31_64);
    const __m256i stk1_1 = pair256_set_epi16(2 * cospi_1_64, 2 * cospi_1_64);
    const __m256i stk1_2 =
        pair256_set_epi16(-2 * cospi_17_64, -2 * cospi_17_64);
    const __m256i stk1_3 = pair256_set_epi16(2 * cospi_15_64, 2 * cospi_15_64);

    const __m256i stk1_4 = pair256_set_epi16(2 * cospi_23_64, 2 * cospi_23_64);
    const __m256i stk1_5 = pair256_set_epi16(2 * cospi_9_64, 2 * cospi_9_64);
    const __m256i stk1_6 =
        pair256_set_epi16(-2 * cospi_25_64, -2 * cospi_25_64);
    const __m256i stk1_7 = pair256_set_epi16(2 * cospi_7_64, 2 * cospi_7_64);
    const __m256i stk1_8 = pair256_set_epi16(2 * cospi_27_64, 2 * cospi_27_64);
    const __m256i stk1_9 = pair256_set_epi16(2 * cospi_5_64, 2 * cospi_5_64);
    const __m256i stk1_10 =
        pair256_set_epi16(-2 * cospi_21_64, -2 * cospi_21_64);
    const __m256i stk1_11 = pair256_set_epi16(2 * cospi_11_64, 2 * cospi_11_64);

    const __m256i stk1_12 = pair256_set_epi16(2 * cospi_19_64, 2 * cospi_19_64);
    const __m256i stk1_13 = pair256_set_epi16(2 * cospi_13_64, 2 * cospi_13_64);
    const __m256i stk1_14 =
        pair256_set_epi16(-2 * cospi_29_64, -2 * cospi_29_64);
    const __m256i stk1_15 = pair256_set_epi16(2 * cospi_3_64, 2 * cospi_3_64);
    u16 = _mm256_mulhrs_epi16(in[1], stk1_0);
    u31 = _mm256_mulhrs_epi16(in[1], stk1_1);
    u17 = _mm256_mulhrs_epi16(in[15], stk1_2);
    u30 = _mm256_mulhrs_epi16(in[15], stk1_3);

    u18 = _mm256_mulhrs_epi16(in[9], stk1_4);
    u29 = _mm256_mulhrs_epi16(in[9], stk1_5);
    u19 = _mm256_mulhrs_epi16(in[7], stk1_6);
    u28 = _mm256_mulhrs_epi16(in[7], stk1_7);

    u20 = _mm256_mulhrs_epi16(in[5], stk1_8);
    u27 = _mm256_mulhrs_epi16(in[5], stk1_9);
    u21 = _mm256_mulhrs_epi16(in[11], stk1_10);
    u26 = _mm256_mulhrs_epi16(in[11], stk1_11);

    u22 = _mm256_mulhrs_epi16(in[13], stk1_12);
    u25 = _mm256_mulhrs_epi16(in[13], stk1_13);
    u23 = _mm256_mulhrs_epi16(in[3], stk1_14);
    u24 = _mm256_mulhrs_epi16(in[3], stk1_15);
  }

  v16 = _mm256_add_epi16(u16, u17);
  v17 = _mm256_sub_epi16(u16, u17);
  v18 = _mm256_sub_epi16(u19, u18);
  v19 = _mm256_add_epi16(u19, u18);

  v20 = _mm256_add_epi16(u20, u21);
  v21 = _mm256_sub_epi16(u20, u21);
  v22 = _mm256_sub_epi16(u23, u22);
  v23 = _mm256_add_epi16(u23, u22);

  v24 = _mm256_add_epi16(u24, u25);
  v25 = _mm256_sub_epi16(u24, u25);
  v26 = _mm256_sub_epi16(u27, u26);
  v27 = _mm256_add_epi16(u27, u26);

  v28 = _mm256_add_epi16(u28, u29);
  v29 = _mm256_sub_epi16(u28, u29);
  v30 = _mm256_sub_epi16(u31, u30);
  v31 = _mm256_add_epi16(u31, u30);

  {
    const __m256i stg3_4 = pair256_set_epi16(-cospi_4_64, cospi_28_64);
    const __m256i stg3_5 = pair256_set_epi16(cospi_28_64, cospi_4_64);
    const __m256i stg3_6 = pair256_set_epi16(-cospi_28_64, -cospi_4_64);
    const __m256i stg3_8 = pair256_set_epi16(-cospi_20_64, cospi_12_64);
    const __m256i stg3_9 = pair256_set_epi16(cospi_12_64, cospi_20_64);
    const __m256i stg3_10 = pair256_set_epi16(-cospi_12_64, -cospi_20_64);

    butterfly_self(&v17, &v30, &stg3_4, &stg3_5);
    butterfly_self(&v18, &v29, &stg3_6, &stg3_4);
    butterfly_self(&v21, &v26, &stg3_8, &stg3_9);
    butterfly_self(&v22, &v25, &stg3_10, &stg3_8);
  }

  u16 = _mm256_add_epi16(v16, v19);
  u17 = _mm256_add_epi16(v17, v18);
  u18 = _mm256_sub_epi16(v17, v18);
  u19 = _mm256_sub_epi16(v16, v19);
  u20 = _mm256_sub_epi16(v23, v20);
  u21 = _mm256_sub_epi16(v22, v21);
  u22 = _mm256_add_epi16(v22, v21);
  u23 = _mm256_add_epi16(v23, v20);

  u24 = _mm256_add_epi16(v24, v27);
  u25 = _mm256_add_epi16(v25, v26);
  u26 = _mm256_sub_epi16(v25, v26);
  u27 = _mm256_sub_epi16(v24, v27);
  u28 = _mm256_sub_epi16(v31, v28);
  u29 = _mm256_sub_epi16(v30, v29);
  u30 = _mm256_add_epi16(v29, v30);
  u31 = _mm256_add_epi16(v28, v31);

  {
    const __m256i stg4_4 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
    const __m256i stg4_5 = pair256_set_epi16(cospi_24_64, cospi_8_64);
    const __m256i stg4_6 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);
    butterfly_self(&u18, &u29, &stg4_4, &stg4_5);
    butterfly_self(&u19, &u28, &stg4_4, &stg4_5);
    butterfly_self(&u20, &u27, &stg4_6, &stg4_4);
    butterfly_self(&u21, &u26, &stg4_6, &stg4_4);
  }

  out[0] = _mm256_add_epi16(u16, u23);
  out[1] = _mm256_add_epi16(u17, u22);
  out[2] = _mm256_add_epi16(u18, u21);
  out[3] = _mm256_add_epi16(u19, u20);
  v20 = _mm256_sub_epi16(u19, u20);
  v21 = _mm256_sub_epi16(u18, u21);
  v22 = _mm256_sub_epi16(u17, u22);
  v23 = _mm256_sub_epi16(u16, u23);

  v24 = _mm256_sub_epi16(u31, u24);
  v25 = _mm256_sub_epi16(u30, u25);
  v26 = _mm256_sub_epi16(u29, u26);
  v27 = _mm256_sub_epi16(u28, u27);
  out[12] = _mm256_add_epi16(u27, u28);
  out[13] = _mm256_add_epi16(u26, u29);
  out[14] = _mm256_add_epi16(u25, u30);
  out[15] = _mm256_add_epi16(u24, u31);

  {
    const __m256i stg4_0 = pair256_set_epi16(cospi_16_64, cospi_16_64);
    const __m256i stg6_0 = pair256_set_epi16(-cospi_16_64, cospi_16_64);
    butterfly(&v20, &v27, &stg6_0, &stg4_0, &out[4], &out[11]);
    butterfly(&v21, &v26, &stg6_0, &stg4_0, &out[5], &out[10]);
    butterfly(&v22, &v25, &stg6_0, &stg4_0, &out[6], &out[9]);
    butterfly(&v23, &v24, &stg6_0, &stg4_0, &out[7], &out[8]);
  }
}

// 16x16 block input __m256i in[32], output 16x32 __m256i in[32]
static void idct32_16x32_135(__m256i *in /*in[32]*/) {
  __m256i out[32];
  idct32_16x32_quarter_1_2(in, out);
  idct32_16x32_quarter_3_4(in, &out[16]);
  add_sub_butterfly(out, in, 32);
}

static INLINE void load_buffer_from_32x32(const tran_low_t *coeff, __m256i *in,
                                          int size) {
  int i = 0;
  while (i < size) {
    load_coeff(coeff + (i << 5), &in[i]);
    i += 1;
  }
}

static INLINE void zero_buffer(__m256i *in, int num) {
  int i;
  for (i = 0; i < num; ++i) {
    in[i] = _mm256_setzero_si256();
  }
}

// Only upper-left 16x16 has non-zero coeff
void aom_idct32x32_135_add_avx2(const tran_low_t *input, uint8_t *dest,
                                int stride) {
  __m256i in[32];
  zero_buffer(in, 32);
  load_buffer_from_32x32(input, in, 16);
  mm256_transpose_16x16(in, in);
  idct32_16x32_135(in);

  __m256i out[32];
  mm256_transpose_16x16(in, out);
  idct32_16x32_135(out);
  store_buffer_16xN(out, stride, dest, 32);
  mm256_transpose_16x16(&in[16], in);
  idct32_16x32_135(in);
  store_buffer_16xN(in, stride, dest + 16, 32);
}

static void idct32_34_first_half(const __m256i *in, __m256i *stp1) {
  const __m256i stk2_0 = pair256_set_epi16(2 * cospi_30_64, 2 * cospi_30_64);
  const __m256i stk2_1 = pair256_set_epi16(2 * cospi_2_64, 2 * cospi_2_64);
  const __m256i stk2_6 = pair256_set_epi16(-2 * cospi_26_64, -2 * cospi_26_64);
  const __m256i stk2_7 = pair256_set_epi16(2 * cospi_6_64, 2 * cospi_6_64);

  const __m256i stk3_0 = pair256_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
  const __m256i stk3_1 = pair256_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);

  const __m256i stg4_0 = pair256_set_epi16(cospi_16_64, cospi_16_64);
  const __m256i stk4_0 = pair256_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
  const __m256i stg4_1 = pair256_set_epi16(cospi_16_64, -cospi_16_64);
  const __m256i stg4_4 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i stg4_5 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i stg4_6 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);

  const __m256i stg6_0 = pair256_set_epi16(-cospi_16_64, cospi_16_64);
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i x0, x1, x4, x5, x6, x7;
  __m256i v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;

  // phase 1

  // 0, 15
  u2 = _mm256_mulhrs_epi16(in[2], stk2_1);  // stp2_15
  u3 = _mm256_mulhrs_epi16(in[6], stk2_7);  // stp2_12
  v15 = _mm256_add_epi16(u2, u3);
  // in[0], in[4]
  x0 = _mm256_mulhrs_epi16(in[0], stk4_0);  // stp1[0]
  x7 = _mm256_mulhrs_epi16(in[4], stk3_1);  // stp1[7]
  v0 = _mm256_add_epi16(x0, x7);            // stp2_0
  stp1[0] = _mm256_add_epi16(v0, v15);
  stp1[15] = _mm256_sub_epi16(v0, v15);

  // in[2], in[6]
  u0 = _mm256_mulhrs_epi16(in[2], stk2_0);          // stp2_8
  u1 = _mm256_mulhrs_epi16(in[6], stk2_6);          // stp2_11
  butterfly(&u0, &u2, &stg4_4, &stg4_5, &u4, &u5);  // stp2_9, stp2_14
  butterfly(&u1, &u3, &stg4_6, &stg4_4, &u6, &u7);  // stp2_10, stp2_13

  v8 = _mm256_add_epi16(u0, u1);
  v9 = _mm256_add_epi16(u4, u6);
  v10 = _mm256_sub_epi16(u4, u6);
  v11 = _mm256_sub_epi16(u0, u1);
  v12 = _mm256_sub_epi16(u2, u3);
  v13 = _mm256_sub_epi16(u5, u7);
  v14 = _mm256_add_epi16(u5, u7);

  butterfly_self(&v10, &v13, &stg6_0, &stg4_0);
  butterfly_self(&v11, &v12, &stg6_0, &stg4_0);

  // 1, 14
  x1 = _mm256_mulhrs_epi16(in[0], stk4_0);  // stp1[1], stk4_1 = stk4_0
  // stp1[2] = stp1[0], stp1[3] = stp1[1]
  x4 = _mm256_mulhrs_epi16(in[4], stk3_0);  // stp1[4]
  butterfly(&x7, &x4, &stg4_1, &stg4_0, &x5, &x6);
  v1 = _mm256_add_epi16(x1, x6);  // stp2_1
  v2 = _mm256_add_epi16(x0, x5);  // stp2_2
  stp1[1] = _mm256_add_epi16(v1, v14);
  stp1[14] = _mm256_sub_epi16(v1, v14);

  stp1[2] = _mm256_add_epi16(v2, v13);
  stp1[13] = _mm256_sub_epi16(v2, v13);

  v3 = _mm256_add_epi16(x1, x4);  // stp2_3
  v4 = _mm256_sub_epi16(x1, x4);  // stp2_4

  v5 = _mm256_sub_epi16(x0, x5);  // stp2_5

  v6 = _mm256_sub_epi16(x1, x6);  // stp2_6
  v7 = _mm256_sub_epi16(x0, x7);  // stp2_7
  stp1[3] = _mm256_add_epi16(v3, v12);
  stp1[12] = _mm256_sub_epi16(v3, v12);

  stp1[6] = _mm256_add_epi16(v6, v9);
  stp1[9] = _mm256_sub_epi16(v6, v9);

  stp1[7] = _mm256_add_epi16(v7, v8);
  stp1[8] = _mm256_sub_epi16(v7, v8);

  stp1[4] = _mm256_add_epi16(v4, v11);
  stp1[11] = _mm256_sub_epi16(v4, v11);

  stp1[5] = _mm256_add_epi16(v5, v10);
  stp1[10] = _mm256_sub_epi16(v5, v10);
}

static void idct32_34_second_half(const __m256i *in, __m256i *stp1) {
  const __m256i stk1_0 = pair256_set_epi16(2 * cospi_31_64, 2 * cospi_31_64);
  const __m256i stk1_1 = pair256_set_epi16(2 * cospi_1_64, 2 * cospi_1_64);
  const __m256i stk1_6 = pair256_set_epi16(-2 * cospi_25_64, -2 * cospi_25_64);
  const __m256i stk1_7 = pair256_set_epi16(2 * cospi_7_64, 2 * cospi_7_64);
  const __m256i stk1_8 = pair256_set_epi16(2 * cospi_27_64, 2 * cospi_27_64);
  const __m256i stk1_9 = pair256_set_epi16(2 * cospi_5_64, 2 * cospi_5_64);
  const __m256i stk1_14 = pair256_set_epi16(-2 * cospi_29_64, -2 * cospi_29_64);
  const __m256i stk1_15 = pair256_set_epi16(2 * cospi_3_64, 2 * cospi_3_64);
  const __m256i stg3_4 = pair256_set_epi16(-cospi_4_64, cospi_28_64);
  const __m256i stg3_5 = pair256_set_epi16(cospi_28_64, cospi_4_64);
  const __m256i stg3_6 = pair256_set_epi16(-cospi_28_64, -cospi_4_64);
  const __m256i stg3_8 = pair256_set_epi16(-cospi_20_64, cospi_12_64);
  const __m256i stg3_9 = pair256_set_epi16(cospi_12_64, cospi_20_64);
  const __m256i stg3_10 = pair256_set_epi16(-cospi_12_64, -cospi_20_64);

  const __m256i stg4_0 = pair256_set_epi16(cospi_16_64, cospi_16_64);
  const __m256i stg4_4 = pair256_set_epi16(-cospi_8_64, cospi_24_64);
  const __m256i stg4_5 = pair256_set_epi16(cospi_24_64, cospi_8_64);
  const __m256i stg4_6 = pair256_set_epi16(-cospi_24_64, -cospi_8_64);

  const __m256i stg6_0 = pair256_set_epi16(-cospi_16_64, cospi_16_64);
  __m256i v16, v17, v18, v19, v20, v21, v22, v23;
  __m256i v24, v25, v26, v27, v28, v29, v30, v31;
  __m256i u16, u17, u18, u19, u20, u21, u22, u23;
  __m256i u24, u25, u26, u27, u28, u29, u30, u31;

  v16 = _mm256_mulhrs_epi16(in[1], stk1_0);
  v31 = _mm256_mulhrs_epi16(in[1], stk1_1);

  v19 = _mm256_mulhrs_epi16(in[7], stk1_6);
  v28 = _mm256_mulhrs_epi16(in[7], stk1_7);

  v20 = _mm256_mulhrs_epi16(in[5], stk1_8);
  v27 = _mm256_mulhrs_epi16(in[5], stk1_9);

  v23 = _mm256_mulhrs_epi16(in[3], stk1_14);
  v24 = _mm256_mulhrs_epi16(in[3], stk1_15);

  butterfly(&v16, &v31, &stg3_4, &stg3_5, &v17, &v30);
  butterfly(&v19, &v28, &stg3_6, &stg3_4, &v18, &v29);
  butterfly(&v20, &v27, &stg3_8, &stg3_9, &v21, &v26);
  butterfly(&v23, &v24, &stg3_10, &stg3_8, &v22, &v25);

  u16 = _mm256_add_epi16(v16, v19);
  u17 = _mm256_add_epi16(v17, v18);
  u18 = _mm256_sub_epi16(v17, v18);
  u19 = _mm256_sub_epi16(v16, v19);
  u20 = _mm256_sub_epi16(v23, v20);
  u21 = _mm256_sub_epi16(v22, v21);
  u22 = _mm256_add_epi16(v22, v21);
  u23 = _mm256_add_epi16(v23, v20);
  u24 = _mm256_add_epi16(v24, v27);
  u27 = _mm256_sub_epi16(v24, v27);
  u25 = _mm256_add_epi16(v25, v26);
  u26 = _mm256_sub_epi16(v25, v26);
  u28 = _mm256_sub_epi16(v31, v28);
  u31 = _mm256_add_epi16(v28, v31);
  u29 = _mm256_sub_epi16(v30, v29);
  u30 = _mm256_add_epi16(v29, v30);

  butterfly_self(&u18, &u29, &stg4_4, &stg4_5);
  butterfly_self(&u19, &u28, &stg4_4, &stg4_5);
  butterfly_self(&u20, &u27, &stg4_6, &stg4_4);
  butterfly_self(&u21, &u26, &stg4_6, &stg4_4);

  stp1[0] = _mm256_add_epi16(u16, u23);
  stp1[7] = _mm256_sub_epi16(u16, u23);

  stp1[1] = _mm256_add_epi16(u17, u22);
  stp1[6] = _mm256_sub_epi16(u17, u22);

  stp1[2] = _mm256_add_epi16(u18, u21);
  stp1[5] = _mm256_sub_epi16(u18, u21);

  stp1[3] = _mm256_add_epi16(u19, u20);
  stp1[4] = _mm256_sub_epi16(u19, u20);

  stp1[8] = _mm256_sub_epi16(u31, u24);
  stp1[15] = _mm256_add_epi16(u24, u31);

  stp1[9] = _mm256_sub_epi16(u30, u25);
  stp1[14] = _mm256_add_epi16(u25, u30);

  stp1[10] = _mm256_sub_epi16(u29, u26);
  stp1[13] = _mm256_add_epi16(u26, u29);

  stp1[11] = _mm256_sub_epi16(u28, u27);
  stp1[12] = _mm256_add_epi16(u27, u28);

  butterfly_self(&stp1[4], &stp1[11], &stg6_0, &stg4_0);
  butterfly_self(&stp1[5], &stp1[10], &stg6_0, &stg4_0);
  butterfly_self(&stp1[6], &stp1[9], &stg6_0, &stg4_0);
  butterfly_self(&stp1[7], &stp1[8], &stg6_0, &stg4_0);
}

// 16x16 block input __m256i in[32], output 16x32 __m256i in[32]
static void idct32_16x32_34(__m256i *in /*in[32]*/) {
  __m256i out[32];
  idct32_34_first_half(in, out);
  idct32_34_second_half(in, &out[16]);
  add_sub_butterfly(out, in, 32);
}

// Only upper-left 8x8 has non-zero coeff
void aom_idct32x32_34_add_avx2(const tran_low_t *input, uint8_t *dest,
                               int stride) {
  __m256i in[32];
  zero_buffer(in, 32);
  load_buffer_from_32x32(input, in, 8);
  mm256_transpose_16x16(in, in);
  idct32_16x32_34(in);

  __m256i out[32];
  mm256_transpose_16x16(in, out);
  idct32_16x32_34(out);
  store_buffer_16xN(out, stride, dest, 32);
  mm256_transpose_16x16(&in[16], in);
  idct32_16x32_34(in);
  store_buffer_16xN(in, stride, dest + 16, 32);
}
