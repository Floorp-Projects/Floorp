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
#include <assert.h>
#include <immintrin.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "av1/common/av1_inv_txfm1d_cfg.h"

// Note:
//  Total 32x4 registers to represent 32x32 block coefficients.
//  For high bit depth, each coefficient is 4-byte.
//  Each __m256i register holds 8 coefficients.
//  So each "row" we needs 4 register. Totally 32 rows
//  Register layout:
//   v0,   v1,   v2,   v3,
//   v4,   v5,   v6,   v7,
//   ... ...
//   v124, v125, v126, v127

static void transpose_32x32_8x8(const __m256i *in, __m256i *out) {
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i x0, x1;

  u0 = _mm256_unpacklo_epi32(in[0], in[4]);
  u1 = _mm256_unpackhi_epi32(in[0], in[4]);

  u2 = _mm256_unpacklo_epi32(in[8], in[12]);
  u3 = _mm256_unpackhi_epi32(in[8], in[12]);

  u4 = _mm256_unpacklo_epi32(in[16], in[20]);
  u5 = _mm256_unpackhi_epi32(in[16], in[20]);

  u6 = _mm256_unpacklo_epi32(in[24], in[28]);
  u7 = _mm256_unpackhi_epi32(in[24], in[28]);

  x0 = _mm256_unpacklo_epi64(u0, u2);
  x1 = _mm256_unpacklo_epi64(u4, u6);
  out[0] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[16] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u0, u2);
  x1 = _mm256_unpackhi_epi64(u4, u6);
  out[4] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[20] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpacklo_epi64(u1, u3);
  x1 = _mm256_unpacklo_epi64(u5, u7);
  out[8] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[24] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u1, u3);
  x1 = _mm256_unpackhi_epi64(u5, u7);
  out[12] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[28] = _mm256_permute2f128_si256(x0, x1, 0x31);
}

static void transpose_32x32_16x16(const __m256i *in, __m256i *out) {
  transpose_32x32_8x8(&in[0], &out[0]);
  transpose_32x32_8x8(&in[1], &out[32]);
  transpose_32x32_8x8(&in[32], &out[1]);
  transpose_32x32_8x8(&in[33], &out[33]);
}

static void transpose_32x32(const __m256i *in, __m256i *out) {
  transpose_32x32_16x16(&in[0], &out[0]);
  transpose_32x32_16x16(&in[2], &out[64]);
  transpose_32x32_16x16(&in[64], &out[2]);
  transpose_32x32_16x16(&in[66], &out[66]);
}

static void load_buffer_32x32(const int32_t *coeff, __m256i *in) {
  int i;
  for (i = 0; i < 128; ++i) {
    in[i] = _mm256_loadu_si256((const __m256i *)coeff);
    coeff += 8;
  }
}

static __m256i highbd_clamp_epi32(__m256i x, int bd) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i max = _mm256_sub_epi16(_mm256_slli_epi16(one, bd), one);
  __m256i clamped, mask;

  mask = _mm256_cmpgt_epi16(x, max);
  clamped = _mm256_andnot_si256(mask, x);
  mask = _mm256_and_si256(mask, max);
  clamped = _mm256_or_si256(mask, clamped);
  mask = _mm256_cmpgt_epi16(clamped, zero);
  clamped = _mm256_and_si256(clamped, mask);

  return clamped;
}

static void write_buffer_32x32(__m256i *in, uint16_t *output, int stride,
                               int fliplr, int flipud, int shift, int bd) {
  __m256i u0, u1, x0, x1, x2, x3, v0, v1, v2, v3;
  const __m256i zero = _mm256_setzero_si256();
  int i = 0;
  (void)fliplr;
  (void)flipud;

  __m256i round = _mm256_set1_epi32((1 << shift) >> 1);

  while (i < 128) {
    u0 = _mm256_loadu_si256((const __m256i *)output);
    u1 = _mm256_loadu_si256((const __m256i *)(output + 16));

    x0 = _mm256_unpacklo_epi16(u0, zero);
    x1 = _mm256_unpackhi_epi16(u0, zero);
    x2 = _mm256_unpacklo_epi16(u1, zero);
    x3 = _mm256_unpackhi_epi16(u1, zero);

    v0 = _mm256_permute2f128_si256(in[i], in[i + 1], 0x20);
    v1 = _mm256_permute2f128_si256(in[i], in[i + 1], 0x31);
    v2 = _mm256_permute2f128_si256(in[i + 2], in[i + 3], 0x20);
    v3 = _mm256_permute2f128_si256(in[i + 2], in[i + 3], 0x31);

    v0 = _mm256_add_epi32(v0, round);
    v1 = _mm256_add_epi32(v1, round);
    v2 = _mm256_add_epi32(v2, round);
    v3 = _mm256_add_epi32(v3, round);

    v0 = _mm256_sra_epi32(v0, _mm_cvtsi32_si128(shift));
    v1 = _mm256_sra_epi32(v1, _mm_cvtsi32_si128(shift));
    v2 = _mm256_sra_epi32(v2, _mm_cvtsi32_si128(shift));
    v3 = _mm256_sra_epi32(v3, _mm_cvtsi32_si128(shift));

    v0 = _mm256_add_epi32(v0, x0);
    v1 = _mm256_add_epi32(v1, x1);
    v2 = _mm256_add_epi32(v2, x2);
    v3 = _mm256_add_epi32(v3, x3);

    v0 = _mm256_packus_epi32(v0, v1);
    v2 = _mm256_packus_epi32(v2, v3);

    v0 = highbd_clamp_epi32(v0, bd);
    v2 = highbd_clamp_epi32(v2, bd);

    _mm256_storeu_si256((__m256i *)output, v0);
    _mm256_storeu_si256((__m256i *)(output + 16), v2);
    output += stride;
    i += 4;
  }
}

static INLINE __m256i half_btf_avx2(const __m256i *w0, const __m256i *n0,
                                    const __m256i *w1, const __m256i *n1,
                                    const __m256i *rounding, int bit) {
  __m256i x, y;

  x = _mm256_mullo_epi32(*w0, *n0);
  y = _mm256_mullo_epi32(*w1, *n1);
  x = _mm256_add_epi32(x, y);
  x = _mm256_add_epi32(x, *rounding);
  x = _mm256_srai_epi32(x, bit);
  return x;
}

static void addsub_avx2(const __m256i in0, const __m256i in1, __m256i *out0,
                        __m256i *out1, const __m256i *clamp_lo,
                        const __m256i *clamp_hi) {
  __m256i a0 = _mm256_add_epi32(in0, in1);
  __m256i a1 = _mm256_sub_epi32(in0, in1);

  a0 = _mm256_max_epi32(a0, *clamp_lo);
  a0 = _mm256_min_epi32(a0, *clamp_hi);
  a1 = _mm256_max_epi32(a1, *clamp_lo);
  a1 = _mm256_min_epi32(a1, *clamp_hi);

  *out0 = a0;
  *out1 = a1;
}

static void addsub_no_clamp_avx2(const __m256i in0, const __m256i in1,
                                 __m256i *out0, __m256i *out1) {
  __m256i a0 = _mm256_add_epi32(in0, in1);
  __m256i a1 = _mm256_sub_epi32(in0, in1);

  *out0 = a0;
  *out1 = a1;
}

static void addsub_shift_avx2(const __m256i in0, const __m256i in1,
                              __m256i *out0, __m256i *out1,
                              const __m256i *clamp_lo, const __m256i *clamp_hi,
                              int shift) {
  __m256i offset = _mm256_set1_epi32((1 << shift) >> 1);
  __m256i in0_w_offset = _mm256_add_epi32(in0, offset);
  __m256i a0 = _mm256_add_epi32(in0_w_offset, in1);
  __m256i a1 = _mm256_sub_epi32(in0_w_offset, in1);

  a0 = _mm256_max_epi32(a0, *clamp_lo);
  a0 = _mm256_min_epi32(a0, *clamp_hi);
  a1 = _mm256_max_epi32(a1, *clamp_lo);
  a1 = _mm256_min_epi32(a1, *clamp_hi);

  a0 = _mm256_sra_epi32(a0, _mm_cvtsi32_si128(shift));
  a1 = _mm256_sra_epi32(a1, _mm_cvtsi32_si128(shift));

  *out0 = a0;
  *out1 = a1;
}

static void idct32_avx2(__m256i *in, __m256i *out, int bit, int do_cols, int bd,
                        int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
  const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
  const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi58 = _mm256_set1_epi32(cospi[58]);
  const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
  const __m256i cospi42 = _mm256_set1_epi32(cospi[42]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi50 = _mm256_set1_epi32(cospi[50]);
  const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
  const __m256i cospi34 = _mm256_set1_epi32(cospi[34]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospim26 = _mm256_set1_epi32(-cospi[26]);
  const __m256i cospim42 = _mm256_set1_epi32(-cospi[42]);
  const __m256i cospim10 = _mm256_set1_epi32(-cospi[10]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospim18 = _mm256_set1_epi32(-cospi[18]);
  const __m256i cospim34 = _mm256_set1_epi32(-cospi[34]);
  const __m256i cospim2 = _mm256_set1_epi32(-cospi[2]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospim20 = _mm256_set1_epi32(-cospi[20]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospim4 = _mm256_set1_epi32(-cospi[4]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i rounding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i bf1[32], bf0[32];
  int col;

  for (col = 0; col < 4; ++col) {
    // stage 0
    // stage 1
    bf1[0] = in[0 * 4 + col];
    bf1[1] = in[16 * 4 + col];
    bf1[2] = in[8 * 4 + col];
    bf1[3] = in[24 * 4 + col];
    bf1[4] = in[4 * 4 + col];
    bf1[5] = in[20 * 4 + col];
    bf1[6] = in[12 * 4 + col];
    bf1[7] = in[28 * 4 + col];
    bf1[8] = in[2 * 4 + col];
    bf1[9] = in[18 * 4 + col];
    bf1[10] = in[10 * 4 + col];
    bf1[11] = in[26 * 4 + col];
    bf1[12] = in[6 * 4 + col];
    bf1[13] = in[22 * 4 + col];
    bf1[14] = in[14 * 4 + col];
    bf1[15] = in[30 * 4 + col];
    bf1[16] = in[1 * 4 + col];
    bf1[17] = in[17 * 4 + col];
    bf1[18] = in[9 * 4 + col];
    bf1[19] = in[25 * 4 + col];
    bf1[20] = in[5 * 4 + col];
    bf1[21] = in[21 * 4 + col];
    bf1[22] = in[13 * 4 + col];
    bf1[23] = in[29 * 4 + col];
    bf1[24] = in[3 * 4 + col];
    bf1[25] = in[19 * 4 + col];
    bf1[26] = in[11 * 4 + col];
    bf1[27] = in[27 * 4 + col];
    bf1[28] = in[7 * 4 + col];
    bf1[29] = in[23 * 4 + col];
    bf1[30] = in[15 * 4 + col];
    bf1[31] = in[31 * 4 + col];

    // stage 2
    bf0[0] = bf1[0];
    bf0[1] = bf1[1];
    bf0[2] = bf1[2];
    bf0[3] = bf1[3];
    bf0[4] = bf1[4];
    bf0[5] = bf1[5];
    bf0[6] = bf1[6];
    bf0[7] = bf1[7];
    bf0[8] = bf1[8];
    bf0[9] = bf1[9];
    bf0[10] = bf1[10];
    bf0[11] = bf1[11];
    bf0[12] = bf1[12];
    bf0[13] = bf1[13];
    bf0[14] = bf1[14];
    bf0[15] = bf1[15];
    bf0[16] =
        half_btf_avx2(&cospi62, &bf1[16], &cospim2, &bf1[31], &rounding, bit);
    bf0[17] =
        half_btf_avx2(&cospi30, &bf1[17], &cospim34, &bf1[30], &rounding, bit);
    bf0[18] =
        half_btf_avx2(&cospi46, &bf1[18], &cospim18, &bf1[29], &rounding, bit);
    bf0[19] =
        half_btf_avx2(&cospi14, &bf1[19], &cospim50, &bf1[28], &rounding, bit);
    bf0[20] =
        half_btf_avx2(&cospi54, &bf1[20], &cospim10, &bf1[27], &rounding, bit);
    bf0[21] =
        half_btf_avx2(&cospi22, &bf1[21], &cospim42, &bf1[26], &rounding, bit);
    bf0[22] =
        half_btf_avx2(&cospi38, &bf1[22], &cospim26, &bf1[25], &rounding, bit);
    bf0[23] =
        half_btf_avx2(&cospi6, &bf1[23], &cospim58, &bf1[24], &rounding, bit);
    bf0[24] =
        half_btf_avx2(&cospi58, &bf1[23], &cospi6, &bf1[24], &rounding, bit);
    bf0[25] =
        half_btf_avx2(&cospi26, &bf1[22], &cospi38, &bf1[25], &rounding, bit);
    bf0[26] =
        half_btf_avx2(&cospi42, &bf1[21], &cospi22, &bf1[26], &rounding, bit);
    bf0[27] =
        half_btf_avx2(&cospi10, &bf1[20], &cospi54, &bf1[27], &rounding, bit);
    bf0[28] =
        half_btf_avx2(&cospi50, &bf1[19], &cospi14, &bf1[28], &rounding, bit);
    bf0[29] =
        half_btf_avx2(&cospi18, &bf1[18], &cospi46, &bf1[29], &rounding, bit);
    bf0[30] =
        half_btf_avx2(&cospi34, &bf1[17], &cospi30, &bf1[30], &rounding, bit);
    bf0[31] =
        half_btf_avx2(&cospi2, &bf1[16], &cospi62, &bf1[31], &rounding, bit);

    // stage 3
    bf1[0] = bf0[0];
    bf1[1] = bf0[1];
    bf1[2] = bf0[2];
    bf1[3] = bf0[3];
    bf1[4] = bf0[4];
    bf1[5] = bf0[5];
    bf1[6] = bf0[6];
    bf1[7] = bf0[7];
    bf1[8] =
        half_btf_avx2(&cospi60, &bf0[8], &cospim4, &bf0[15], &rounding, bit);
    bf1[9] =
        half_btf_avx2(&cospi28, &bf0[9], &cospim36, &bf0[14], &rounding, bit);
    bf1[10] =
        half_btf_avx2(&cospi44, &bf0[10], &cospim20, &bf0[13], &rounding, bit);
    bf1[11] =
        half_btf_avx2(&cospi12, &bf0[11], &cospim52, &bf0[12], &rounding, bit);
    bf1[12] =
        half_btf_avx2(&cospi52, &bf0[11], &cospi12, &bf0[12], &rounding, bit);
    bf1[13] =
        half_btf_avx2(&cospi20, &bf0[10], &cospi44, &bf0[13], &rounding, bit);
    bf1[14] =
        half_btf_avx2(&cospi36, &bf0[9], &cospi28, &bf0[14], &rounding, bit);
    bf1[15] =
        half_btf_avx2(&cospi4, &bf0[8], &cospi60, &bf0[15], &rounding, bit);

    addsub_avx2(bf0[16], bf0[17], bf1 + 16, bf1 + 17, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[19], bf0[18], bf1 + 19, bf1 + 18, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[20], bf0[21], bf1 + 20, bf1 + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[23], bf0[22], bf1 + 23, bf1 + 22, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[24], bf0[25], bf1 + 24, bf1 + 25, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[27], bf0[26], bf1 + 27, bf1 + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[28], bf0[29], bf1 + 28, bf1 + 29, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[31], bf0[30], bf1 + 31, bf1 + 30, &clamp_lo, &clamp_hi);

    // stage 4
    bf0[0] = bf1[0];
    bf0[1] = bf1[1];
    bf0[2] = bf1[2];
    bf0[3] = bf1[3];
    bf0[4] =
        half_btf_avx2(&cospi56, &bf1[4], &cospim8, &bf1[7], &rounding, bit);
    bf0[5] =
        half_btf_avx2(&cospi24, &bf1[5], &cospim40, &bf1[6], &rounding, bit);
    bf0[6] =
        half_btf_avx2(&cospi40, &bf1[5], &cospi24, &bf1[6], &rounding, bit);
    bf0[7] = half_btf_avx2(&cospi8, &bf1[4], &cospi56, &bf1[7], &rounding, bit);

    addsub_avx2(bf1[8], bf1[9], bf0 + 8, bf0 + 9, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[11], bf1[10], bf0 + 11, bf0 + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[12], bf1[13], bf0 + 12, bf0 + 13, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[15], bf1[14], bf0 + 15, bf0 + 14, &clamp_lo, &clamp_hi);

    bf0[16] = bf1[16];
    bf0[17] =
        half_btf_avx2(&cospim8, &bf1[17], &cospi56, &bf1[30], &rounding, bit);
    bf0[18] =
        half_btf_avx2(&cospim56, &bf1[18], &cospim8, &bf1[29], &rounding, bit);
    bf0[19] = bf1[19];
    bf0[20] = bf1[20];
    bf0[21] =
        half_btf_avx2(&cospim40, &bf1[21], &cospi24, &bf1[26], &rounding, bit);
    bf0[22] =
        half_btf_avx2(&cospim24, &bf1[22], &cospim40, &bf1[25], &rounding, bit);
    bf0[23] = bf1[23];
    bf0[24] = bf1[24];
    bf0[25] =
        half_btf_avx2(&cospim40, &bf1[22], &cospi24, &bf1[25], &rounding, bit);
    bf0[26] =
        half_btf_avx2(&cospi24, &bf1[21], &cospi40, &bf1[26], &rounding, bit);
    bf0[27] = bf1[27];
    bf0[28] = bf1[28];
    bf0[29] =
        half_btf_avx2(&cospim8, &bf1[18], &cospi56, &bf1[29], &rounding, bit);
    bf0[30] =
        half_btf_avx2(&cospi56, &bf1[17], &cospi8, &bf1[30], &rounding, bit);
    bf0[31] = bf1[31];

    // stage 5
    bf1[0] =
        half_btf_avx2(&cospi32, &bf0[0], &cospi32, &bf0[1], &rounding, bit);
    bf1[1] =
        half_btf_avx2(&cospi32, &bf0[0], &cospim32, &bf0[1], &rounding, bit);
    bf1[2] =
        half_btf_avx2(&cospi48, &bf0[2], &cospim16, &bf0[3], &rounding, bit);
    bf1[3] =
        half_btf_avx2(&cospi16, &bf0[2], &cospi48, &bf0[3], &rounding, bit);
    addsub_avx2(bf0[4], bf0[5], bf1 + 4, bf1 + 5, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[7], bf0[6], bf1 + 7, bf1 + 6, &clamp_lo, &clamp_hi);
    bf1[8] = bf0[8];
    bf1[9] =
        half_btf_avx2(&cospim16, &bf0[9], &cospi48, &bf0[14], &rounding, bit);
    bf1[10] =
        half_btf_avx2(&cospim48, &bf0[10], &cospim16, &bf0[13], &rounding, bit);
    bf1[11] = bf0[11];
    bf1[12] = bf0[12];
    bf1[13] =
        half_btf_avx2(&cospim16, &bf0[10], &cospi48, &bf0[13], &rounding, bit);
    bf1[14] =
        half_btf_avx2(&cospi48, &bf0[9], &cospi16, &bf0[14], &rounding, bit);
    bf1[15] = bf0[15];
    addsub_avx2(bf0[16], bf0[19], bf1 + 16, bf1 + 19, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[17], bf0[18], bf1 + 17, bf1 + 18, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[23], bf0[20], bf1 + 23, bf1 + 20, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[22], bf0[21], bf1 + 22, bf1 + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[24], bf0[27], bf1 + 24, bf1 + 27, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[25], bf0[26], bf1 + 25, bf1 + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[31], bf0[28], bf1 + 31, bf1 + 28, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[30], bf0[29], bf1 + 30, bf1 + 29, &clamp_lo, &clamp_hi);

    // stage 6
    addsub_avx2(bf1[0], bf1[3], bf0 + 0, bf0 + 3, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[1], bf1[2], bf0 + 1, bf0 + 2, &clamp_lo, &clamp_hi);
    bf0[4] = bf1[4];
    bf0[5] =
        half_btf_avx2(&cospim32, &bf1[5], &cospi32, &bf1[6], &rounding, bit);
    bf0[6] =
        half_btf_avx2(&cospi32, &bf1[5], &cospi32, &bf1[6], &rounding, bit);
    bf0[7] = bf1[7];
    addsub_avx2(bf1[8], bf1[11], bf0 + 8, bf0 + 11, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[9], bf1[10], bf0 + 9, bf0 + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[15], bf1[12], bf0 + 15, bf0 + 12, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[14], bf1[13], bf0 + 14, bf0 + 13, &clamp_lo, &clamp_hi);
    bf0[16] = bf1[16];
    bf0[17] = bf1[17];
    bf0[18] =
        half_btf_avx2(&cospim16, &bf1[18], &cospi48, &bf1[29], &rounding, bit);
    bf0[19] =
        half_btf_avx2(&cospim16, &bf1[19], &cospi48, &bf1[28], &rounding, bit);
    bf0[20] =
        half_btf_avx2(&cospim48, &bf1[20], &cospim16, &bf1[27], &rounding, bit);
    bf0[21] =
        half_btf_avx2(&cospim48, &bf1[21], &cospim16, &bf1[26], &rounding, bit);
    bf0[22] = bf1[22];
    bf0[23] = bf1[23];
    bf0[24] = bf1[24];
    bf0[25] = bf1[25];
    bf0[26] =
        half_btf_avx2(&cospim16, &bf1[21], &cospi48, &bf1[26], &rounding, bit);
    bf0[27] =
        half_btf_avx2(&cospim16, &bf1[20], &cospi48, &bf1[27], &rounding, bit);
    bf0[28] =
        half_btf_avx2(&cospi48, &bf1[19], &cospi16, &bf1[28], &rounding, bit);
    bf0[29] =
        half_btf_avx2(&cospi48, &bf1[18], &cospi16, &bf1[29], &rounding, bit);
    bf0[30] = bf1[30];
    bf0[31] = bf1[31];

    // stage 7
    addsub_avx2(bf0[0], bf0[7], bf1 + 0, bf1 + 7, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[1], bf0[6], bf1 + 1, bf1 + 6, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[2], bf0[5], bf1 + 2, bf1 + 5, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[3], bf0[4], bf1 + 3, bf1 + 4, &clamp_lo, &clamp_hi);
    bf1[8] = bf0[8];
    bf1[9] = bf0[9];
    bf1[10] =
        half_btf_avx2(&cospim32, &bf0[10], &cospi32, &bf0[13], &rounding, bit);
    bf1[11] =
        half_btf_avx2(&cospim32, &bf0[11], &cospi32, &bf0[12], &rounding, bit);
    bf1[12] =
        half_btf_avx2(&cospi32, &bf0[11], &cospi32, &bf0[12], &rounding, bit);
    bf1[13] =
        half_btf_avx2(&cospi32, &bf0[10], &cospi32, &bf0[13], &rounding, bit);
    bf1[14] = bf0[14];
    bf1[15] = bf0[15];
    addsub_avx2(bf0[16], bf0[23], bf1 + 16, bf1 + 23, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[17], bf0[22], bf1 + 17, bf1 + 22, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[18], bf0[21], bf1 + 18, bf1 + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[19], bf0[20], bf1 + 19, bf1 + 20, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[31], bf0[24], bf1 + 31, bf1 + 24, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[30], bf0[25], bf1 + 30, bf1 + 25, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[29], bf0[26], bf1 + 29, bf1 + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[28], bf0[27], bf1 + 28, bf1 + 27, &clamp_lo, &clamp_hi);

    // stage 8
    addsub_avx2(bf1[0], bf1[15], bf0 + 0, bf0 + 15, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[1], bf1[14], bf0 + 1, bf0 + 14, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[2], bf1[13], bf0 + 2, bf0 + 13, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[3], bf1[12], bf0 + 3, bf0 + 12, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[4], bf1[11], bf0 + 4, bf0 + 11, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[5], bf1[10], bf0 + 5, bf0 + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[6], bf1[9], bf0 + 6, bf0 + 9, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[7], bf1[8], bf0 + 7, bf0 + 8, &clamp_lo, &clamp_hi);
    bf0[16] = bf1[16];
    bf0[17] = bf1[17];
    bf0[18] = bf1[18];
    bf0[19] = bf1[19];
    bf0[20] =
        half_btf_avx2(&cospim32, &bf1[20], &cospi32, &bf1[27], &rounding, bit);
    bf0[21] =
        half_btf_avx2(&cospim32, &bf1[21], &cospi32, &bf1[26], &rounding, bit);
    bf0[22] =
        half_btf_avx2(&cospim32, &bf1[22], &cospi32, &bf1[25], &rounding, bit);
    bf0[23] =
        half_btf_avx2(&cospim32, &bf1[23], &cospi32, &bf1[24], &rounding, bit);
    bf0[24] =
        half_btf_avx2(&cospi32, &bf1[23], &cospi32, &bf1[24], &rounding, bit);
    bf0[25] =
        half_btf_avx2(&cospi32, &bf1[22], &cospi32, &bf1[25], &rounding, bit);
    bf0[26] =
        half_btf_avx2(&cospi32, &bf1[21], &cospi32, &bf1[26], &rounding, bit);
    bf0[27] =
        half_btf_avx2(&cospi32, &bf1[20], &cospi32, &bf1[27], &rounding, bit);
    bf0[28] = bf1[28];
    bf0[29] = bf1[29];
    bf0[30] = bf1[30];
    bf0[31] = bf1[31];

    // stage 9
    if (do_cols) {
      addsub_no_clamp_avx2(bf0[0], bf0[31], out + 0 * 4 + col,
                           out + 31 * 4 + col);
      addsub_no_clamp_avx2(bf0[1], bf0[30], out + 1 * 4 + col,
                           out + 30 * 4 + col);
      addsub_no_clamp_avx2(bf0[2], bf0[29], out + 2 * 4 + col,
                           out + 29 * 4 + col);
      addsub_no_clamp_avx2(bf0[3], bf0[28], out + 3 * 4 + col,
                           out + 28 * 4 + col);
      addsub_no_clamp_avx2(bf0[4], bf0[27], out + 4 * 4 + col,
                           out + 27 * 4 + col);
      addsub_no_clamp_avx2(bf0[5], bf0[26], out + 5 * 4 + col,
                           out + 26 * 4 + col);
      addsub_no_clamp_avx2(bf0[6], bf0[25], out + 6 * 4 + col,
                           out + 25 * 4 + col);
      addsub_no_clamp_avx2(bf0[7], bf0[24], out + 7 * 4 + col,
                           out + 24 * 4 + col);
      addsub_no_clamp_avx2(bf0[8], bf0[23], out + 8 * 4 + col,
                           out + 23 * 4 + col);
      addsub_no_clamp_avx2(bf0[9], bf0[22], out + 9 * 4 + col,
                           out + 22 * 4 + col);
      addsub_no_clamp_avx2(bf0[10], bf0[21], out + 10 * 4 + col,
                           out + 21 * 4 + col);
      addsub_no_clamp_avx2(bf0[11], bf0[20], out + 11 * 4 + col,
                           out + 20 * 4 + col);
      addsub_no_clamp_avx2(bf0[12], bf0[19], out + 12 * 4 + col,
                           out + 19 * 4 + col);
      addsub_no_clamp_avx2(bf0[13], bf0[18], out + 13 * 4 + col,
                           out + 18 * 4 + col);
      addsub_no_clamp_avx2(bf0[14], bf0[17], out + 14 * 4 + col,
                           out + 17 * 4 + col);
      addsub_no_clamp_avx2(bf0[15], bf0[16], out + 15 * 4 + col,
                           out + 16 * 4 + col);
    } else {
      addsub_shift_avx2(bf0[0], bf0[31], out + 0 * 4 + col, out + 31 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[1], bf0[30], out + 1 * 4 + col, out + 30 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[2], bf0[29], out + 2 * 4 + col, out + 29 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[3], bf0[28], out + 3 * 4 + col, out + 28 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[4], bf0[27], out + 4 * 4 + col, out + 27 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[5], bf0[26], out + 5 * 4 + col, out + 26 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[6], bf0[25], out + 6 * 4 + col, out + 25 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[7], bf0[24], out + 7 * 4 + col, out + 24 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[8], bf0[23], out + 8 * 4 + col, out + 23 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[9], bf0[22], out + 9 * 4 + col, out + 22 * 4 + col,
                        &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[10], bf0[21], out + 10 * 4 + col,
                        out + 21 * 4 + col, &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[11], bf0[20], out + 11 * 4 + col,
                        out + 20 * 4 + col, &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[12], bf0[19], out + 12 * 4 + col,
                        out + 19 * 4 + col, &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[13], bf0[18], out + 13 * 4 + col,
                        out + 18 * 4 + col, &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[14], bf0[17], out + 14 * 4 + col,
                        out + 17 * 4 + col, &clamp_lo, &clamp_hi, out_shift);
      addsub_shift_avx2(bf0[15], bf0[16], out + 15 * 4 + col,
                        out + 16 * 4 + col, &clamp_lo, &clamp_hi, out_shift);
    }
  }
}

void av1_inv_txfm2d_add_32x32_avx2(const int32_t *coeff, uint16_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  __m256i in[128], out[128];
  const int8_t *shift = inv_txfm_shift_ls[TX_32X32];
  const int txw_idx = get_txw_idx(TX_32X32);
  const int txh_idx = get_txh_idx(TX_32X32);

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_32x32(coeff, in);
      transpose_32x32(in, out);
      idct32_avx2(out, in, inv_cos_bit_row[txw_idx][txh_idx], 0, bd, -shift[0]);
      transpose_32x32(in, out);
      idct32_avx2(out, in, inv_cos_bit_col[txw_idx][txh_idx], 1, bd, 0);
      write_buffer_32x32(in, output, stride, 0, 0, -shift[1], bd);
      break;
    default: assert(0);
  }
}
