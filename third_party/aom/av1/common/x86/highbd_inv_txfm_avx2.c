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
#include "av1/common/idct.h"
#include "av1/common/x86/av1_inv_txfm_ssse3.h"
#include "av1/common/x86/highbd_txfm_utility_sse4.h"

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

static INLINE __m256i highbd_clamp_epi16_avx2(__m256i u, int bd) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i max = _mm256_sub_epi16(_mm256_slli_epi16(one, bd), one);
  __m256i clamped, mask;

  mask = _mm256_cmpgt_epi16(u, max);
  clamped = _mm256_andnot_si256(mask, u);
  mask = _mm256_and_si256(mask, max);
  clamped = _mm256_or_si256(mask, clamped);
  mask = _mm256_cmpgt_epi16(clamped, zero);
  clamped = _mm256_and_si256(clamped, mask);

  return clamped;
}

static INLINE __m256i highbd_get_recon_16x8_avx2(const __m256i pred,
                                                 __m256i res0, __m256i res1,
                                                 const int bd) {
  __m256i x0 = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(pred));
  __m256i x1 = _mm256_cvtepi16_epi32(_mm256_extractf128_si256(pred, 1));

  x0 = _mm256_add_epi32(res0, x0);
  x1 = _mm256_add_epi32(res1, x1);
  x0 = _mm256_packus_epi32(x0, x1);
  x0 = _mm256_permute4x64_epi64(x0, 0xd8);
  x0 = highbd_clamp_epi16_avx2(x0, bd);
  return x0;
}

static INLINE void highbd_write_buffer_16xn_avx2(__m256i *in, uint16_t *output,
                                                 int stride, int flipud,
                                                 int height, const int bd) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    __m256i v = _mm256_loadu_si256((__m256i const *)(output + i * stride));
    __m256i u = highbd_get_recon_16x8_avx2(v, in[j], in[j + height], bd);

    _mm256_storeu_si256((__m256i *)(output + i * stride), u);
  }
}

static INLINE __m256i av1_round_shift_32_avx2(__m256i vec, int bit) {
  __m256i tmp, round;
  round = _mm256_set1_epi32(1 << (bit - 1));
  tmp = _mm256_add_epi32(vec, round);
  return _mm256_srai_epi32(tmp, bit);
}

static INLINE void av1_round_shift_array_32_avx2(__m256i *input,
                                                 __m256i *output,
                                                 const int size,
                                                 const int bit) {
  if (bit > 0) {
    int i;
    for (i = 0; i < size; i++) {
      output[i] = av1_round_shift_32_avx2(input[i], bit);
    }
  } else {
    int i;
    for (i = 0; i < size; i++) {
      output[i] = _mm256_slli_epi32(input[i], -bit);
    }
  }
}

static void transpose_8x8_avx2(const __m256i *in, __m256i *out) {
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i x0, x1;

  u0 = _mm256_unpacklo_epi32(in[0], in[1]);
  u1 = _mm256_unpackhi_epi32(in[0], in[1]);

  u2 = _mm256_unpacklo_epi32(in[2], in[3]);
  u3 = _mm256_unpackhi_epi32(in[2], in[3]);

  u4 = _mm256_unpacklo_epi32(in[4], in[5]);
  u5 = _mm256_unpackhi_epi32(in[4], in[5]);

  u6 = _mm256_unpacklo_epi32(in[6], in[7]);
  u7 = _mm256_unpackhi_epi32(in[6], in[7]);

  x0 = _mm256_unpacklo_epi64(u0, u2);
  x1 = _mm256_unpacklo_epi64(u4, u6);
  out[0] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[4] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u0, u2);
  x1 = _mm256_unpackhi_epi64(u4, u6);
  out[1] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[5] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpacklo_epi64(u1, u3);
  x1 = _mm256_unpacklo_epi64(u5, u7);
  out[2] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[6] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u1, u3);
  x1 = _mm256_unpackhi_epi64(u5, u7);
  out[3] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[7] = _mm256_permute2f128_si256(x0, x1, 0x31);
}

static void load_buffer_32x32(const int32_t *coeff, __m256i *in,
                              int input_stiride, int size) {
  int i;
  for (i = 0; i < size; ++i) {
    in[i] = _mm256_loadu_si256((const __m256i *)(coeff + i * input_stiride));
  }
}

static INLINE __m256i half_btf_0_avx2(const __m256i *w0, const __m256i *n0,
                                      const __m256i *rounding, int bit) {
  __m256i x;
  x = _mm256_mullo_epi32(*w0, *n0);
  x = _mm256_add_epi32(x, *rounding);
  x = _mm256_srai_epi32(x, bit);
  return x;
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

  a0 = _mm256_sra_epi32(a0, _mm_cvtsi32_si128(shift));
  a1 = _mm256_sra_epi32(a1, _mm_cvtsi32_si128(shift));

  a0 = _mm256_max_epi32(a0, *clamp_lo);
  a0 = _mm256_min_epi32(a0, *clamp_hi);
  a1 = _mm256_max_epi32(a1, *clamp_lo);
  a1 = _mm256_min_epi32(a1, *clamp_hi);

  *out0 = a0;
  *out1 = a1;
}

static INLINE void idct32_stage4_avx2(
    __m256i *bf1, const __m256i *cospim8, const __m256i *cospi56,
    const __m256i *cospi8, const __m256i *cospim56, const __m256i *cospim40,
    const __m256i *cospi24, const __m256i *cospi40, const __m256i *cospim24,
    const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  temp1 = half_btf_avx2(cospim8, &bf1[17], cospi56, &bf1[30], rounding, bit);
  bf1[30] = half_btf_avx2(cospi56, &bf1[17], cospi8, &bf1[30], rounding, bit);
  bf1[17] = temp1;

  temp2 = half_btf_avx2(cospim56, &bf1[18], cospim8, &bf1[29], rounding, bit);
  bf1[29] = half_btf_avx2(cospim8, &bf1[18], cospi56, &bf1[29], rounding, bit);
  bf1[18] = temp2;

  temp1 = half_btf_avx2(cospim40, &bf1[21], cospi24, &bf1[26], rounding, bit);
  bf1[26] = half_btf_avx2(cospi24, &bf1[21], cospi40, &bf1[26], rounding, bit);
  bf1[21] = temp1;

  temp2 = half_btf_avx2(cospim24, &bf1[22], cospim40, &bf1[25], rounding, bit);
  bf1[25] = half_btf_avx2(cospim40, &bf1[22], cospi24, &bf1[25], rounding, bit);
  bf1[22] = temp2;
}

static INLINE void idct32_stage5_avx2(
    __m256i *bf1, const __m256i *cospim16, const __m256i *cospi48,
    const __m256i *cospi16, const __m256i *cospim48, const __m256i *clamp_lo,
    const __m256i *clamp_hi, const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  temp1 = half_btf_avx2(cospim16, &bf1[9], cospi48, &bf1[14], rounding, bit);
  bf1[14] = half_btf_avx2(cospi48, &bf1[9], cospi16, &bf1[14], rounding, bit);
  bf1[9] = temp1;

  temp2 = half_btf_avx2(cospim48, &bf1[10], cospim16, &bf1[13], rounding, bit);
  bf1[13] = half_btf_avx2(cospim16, &bf1[10], cospi48, &bf1[13], rounding, bit);
  bf1[10] = temp2;

  addsub_avx2(bf1[16], bf1[19], bf1 + 16, bf1 + 19, clamp_lo, clamp_hi);
  addsub_avx2(bf1[17], bf1[18], bf1 + 17, bf1 + 18, clamp_lo, clamp_hi);
  addsub_avx2(bf1[23], bf1[20], bf1 + 23, bf1 + 20, clamp_lo, clamp_hi);
  addsub_avx2(bf1[22], bf1[21], bf1 + 22, bf1 + 21, clamp_lo, clamp_hi);
  addsub_avx2(bf1[24], bf1[27], bf1 + 24, bf1 + 27, clamp_lo, clamp_hi);
  addsub_avx2(bf1[25], bf1[26], bf1 + 25, bf1 + 26, clamp_lo, clamp_hi);
  addsub_avx2(bf1[31], bf1[28], bf1 + 31, bf1 + 28, clamp_lo, clamp_hi);
  addsub_avx2(bf1[30], bf1[29], bf1 + 30, bf1 + 29, clamp_lo, clamp_hi);
}

static INLINE void idct32_stage6_avx2(
    __m256i *bf1, const __m256i *cospim32, const __m256i *cospi32,
    const __m256i *cospim16, const __m256i *cospi48, const __m256i *cospi16,
    const __m256i *cospim48, const __m256i *clamp_lo, const __m256i *clamp_hi,
    const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  temp1 = half_btf_avx2(cospim32, &bf1[5], cospi32, &bf1[6], rounding, bit);
  bf1[6] = half_btf_avx2(cospi32, &bf1[5], cospi32, &bf1[6], rounding, bit);
  bf1[5] = temp1;

  addsub_avx2(bf1[8], bf1[11], bf1 + 8, bf1 + 11, clamp_lo, clamp_hi);
  addsub_avx2(bf1[9], bf1[10], bf1 + 9, bf1 + 10, clamp_lo, clamp_hi);
  addsub_avx2(bf1[15], bf1[12], bf1 + 15, bf1 + 12, clamp_lo, clamp_hi);
  addsub_avx2(bf1[14], bf1[13], bf1 + 14, bf1 + 13, clamp_lo, clamp_hi);

  temp1 = half_btf_avx2(cospim16, &bf1[18], cospi48, &bf1[29], rounding, bit);
  bf1[29] = half_btf_avx2(cospi48, &bf1[18], cospi16, &bf1[29], rounding, bit);
  bf1[18] = temp1;
  temp2 = half_btf_avx2(cospim16, &bf1[19], cospi48, &bf1[28], rounding, bit);
  bf1[28] = half_btf_avx2(cospi48, &bf1[19], cospi16, &bf1[28], rounding, bit);
  bf1[19] = temp2;
  temp1 = half_btf_avx2(cospim48, &bf1[20], cospim16, &bf1[27], rounding, bit);
  bf1[27] = half_btf_avx2(cospim16, &bf1[20], cospi48, &bf1[27], rounding, bit);
  bf1[20] = temp1;
  temp2 = half_btf_avx2(cospim48, &bf1[21], cospim16, &bf1[26], rounding, bit);
  bf1[26] = half_btf_avx2(cospim16, &bf1[21], cospi48, &bf1[26], rounding, bit);
  bf1[21] = temp2;
}

static INLINE void idct32_stage7_avx2(__m256i *bf1, const __m256i *cospim32,
                                      const __m256i *cospi32,
                                      const __m256i *clamp_lo,
                                      const __m256i *clamp_hi,
                                      const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  addsub_avx2(bf1[0], bf1[7], bf1 + 0, bf1 + 7, clamp_lo, clamp_hi);
  addsub_avx2(bf1[1], bf1[6], bf1 + 1, bf1 + 6, clamp_lo, clamp_hi);
  addsub_avx2(bf1[2], bf1[5], bf1 + 2, bf1 + 5, clamp_lo, clamp_hi);
  addsub_avx2(bf1[3], bf1[4], bf1 + 3, bf1 + 4, clamp_lo, clamp_hi);

  temp1 = half_btf_avx2(cospim32, &bf1[10], cospi32, &bf1[13], rounding, bit);
  bf1[13] = half_btf_avx2(cospi32, &bf1[10], cospi32, &bf1[13], rounding, bit);
  bf1[10] = temp1;
  temp2 = half_btf_avx2(cospim32, &bf1[11], cospi32, &bf1[12], rounding, bit);
  bf1[12] = half_btf_avx2(cospi32, &bf1[11], cospi32, &bf1[12], rounding, bit);
  bf1[11] = temp2;

  addsub_avx2(bf1[16], bf1[23], bf1 + 16, bf1 + 23, clamp_lo, clamp_hi);
  addsub_avx2(bf1[17], bf1[22], bf1 + 17, bf1 + 22, clamp_lo, clamp_hi);
  addsub_avx2(bf1[18], bf1[21], bf1 + 18, bf1 + 21, clamp_lo, clamp_hi);
  addsub_avx2(bf1[19], bf1[20], bf1 + 19, bf1 + 20, clamp_lo, clamp_hi);
  addsub_avx2(bf1[31], bf1[24], bf1 + 31, bf1 + 24, clamp_lo, clamp_hi);
  addsub_avx2(bf1[30], bf1[25], bf1 + 30, bf1 + 25, clamp_lo, clamp_hi);
  addsub_avx2(bf1[29], bf1[26], bf1 + 29, bf1 + 26, clamp_lo, clamp_hi);
  addsub_avx2(bf1[28], bf1[27], bf1 + 28, bf1 + 27, clamp_lo, clamp_hi);
}

static INLINE void idct32_stage8_avx2(__m256i *bf1, const __m256i *cospim32,
                                      const __m256i *cospi32,
                                      const __m256i *clamp_lo,
                                      const __m256i *clamp_hi,
                                      const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  addsub_avx2(bf1[0], bf1[15], bf1 + 0, bf1 + 15, clamp_lo, clamp_hi);
  addsub_avx2(bf1[1], bf1[14], bf1 + 1, bf1 + 14, clamp_lo, clamp_hi);
  addsub_avx2(bf1[2], bf1[13], bf1 + 2, bf1 + 13, clamp_lo, clamp_hi);
  addsub_avx2(bf1[3], bf1[12], bf1 + 3, bf1 + 12, clamp_lo, clamp_hi);
  addsub_avx2(bf1[4], bf1[11], bf1 + 4, bf1 + 11, clamp_lo, clamp_hi);
  addsub_avx2(bf1[5], bf1[10], bf1 + 5, bf1 + 10, clamp_lo, clamp_hi);
  addsub_avx2(bf1[6], bf1[9], bf1 + 6, bf1 + 9, clamp_lo, clamp_hi);
  addsub_avx2(bf1[7], bf1[8], bf1 + 7, bf1 + 8, clamp_lo, clamp_hi);

  temp1 = half_btf_avx2(cospim32, &bf1[20], cospi32, &bf1[27], rounding, bit);
  bf1[27] = half_btf_avx2(cospi32, &bf1[20], cospi32, &bf1[27], rounding, bit);
  bf1[20] = temp1;
  temp2 = half_btf_avx2(cospim32, &bf1[21], cospi32, &bf1[26], rounding, bit);
  bf1[26] = half_btf_avx2(cospi32, &bf1[21], cospi32, &bf1[26], rounding, bit);
  bf1[21] = temp2;
  temp1 = half_btf_avx2(cospim32, &bf1[22], cospi32, &bf1[25], rounding, bit);
  bf1[25] = half_btf_avx2(cospi32, &bf1[22], cospi32, &bf1[25], rounding, bit);
  bf1[22] = temp1;
  temp2 = half_btf_avx2(cospim32, &bf1[23], cospi32, &bf1[24], rounding, bit);
  bf1[24] = half_btf_avx2(cospi32, &bf1[23], cospi32, &bf1[24], rounding, bit);
  bf1[23] = temp2;
}

static INLINE void idct32_stage9_avx2(__m256i *bf1, __m256i *out,
                                      const int do_cols, const int bd,
                                      const int out_shift,
                                      const int log_range) {
  if (do_cols) {
    addsub_no_clamp_avx2(bf1[0], bf1[31], out + 0, out + 31);
    addsub_no_clamp_avx2(bf1[1], bf1[30], out + 1, out + 30);
    addsub_no_clamp_avx2(bf1[2], bf1[29], out + 2, out + 29);
    addsub_no_clamp_avx2(bf1[3], bf1[28], out + 3, out + 28);
    addsub_no_clamp_avx2(bf1[4], bf1[27], out + 4, out + 27);
    addsub_no_clamp_avx2(bf1[5], bf1[26], out + 5, out + 26);
    addsub_no_clamp_avx2(bf1[6], bf1[25], out + 6, out + 25);
    addsub_no_clamp_avx2(bf1[7], bf1[24], out + 7, out + 24);
    addsub_no_clamp_avx2(bf1[8], bf1[23], out + 8, out + 23);
    addsub_no_clamp_avx2(bf1[9], bf1[22], out + 9, out + 22);
    addsub_no_clamp_avx2(bf1[10], bf1[21], out + 10, out + 21);
    addsub_no_clamp_avx2(bf1[11], bf1[20], out + 11, out + 20);
    addsub_no_clamp_avx2(bf1[12], bf1[19], out + 12, out + 19);
    addsub_no_clamp_avx2(bf1[13], bf1[18], out + 13, out + 18);
    addsub_no_clamp_avx2(bf1[14], bf1[17], out + 14, out + 17);
    addsub_no_clamp_avx2(bf1[15], bf1[16], out + 15, out + 16);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const __m256i clamp_lo_out = _mm256_set1_epi32(AOMMAX(
        -(1 << (log_range_out - 1)), -(1 << (log_range - 1 - out_shift))));
    const __m256i clamp_hi_out = _mm256_set1_epi32(AOMMIN(
        (1 << (log_range_out - 1)) - 1, (1 << (log_range - 1 - out_shift))));

    addsub_shift_avx2(bf1[0], bf1[31], out + 0, out + 31, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[1], bf1[30], out + 1, out + 30, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[2], bf1[29], out + 2, out + 29, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[3], bf1[28], out + 3, out + 28, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[4], bf1[27], out + 4, out + 27, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[5], bf1[26], out + 5, out + 26, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[6], bf1[25], out + 6, out + 25, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[7], bf1[24], out + 7, out + 24, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[8], bf1[23], out + 8, out + 23, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[9], bf1[22], out + 9, out + 22, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[10], bf1[21], out + 10, out + 21, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[11], bf1[20], out + 11, out + 20, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[12], bf1[19], out + 12, out + 19, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[13], bf1[18], out + 13, out + 18, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[14], bf1[17], out + 14, out + 17, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
    addsub_shift_avx2(bf1[15], bf1[16], out + 15, out + 16, &clamp_lo_out,
                      &clamp_hi_out, out_shift);
  }
}

static void idct32_low1_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                             int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rounding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i x;
  // stage 0
  // stage 1
  // stage 2
  // stage 3
  // stage 4
  // stage 5
  x = _mm256_mullo_epi32(in[0], cospi32);
  x = _mm256_add_epi32(x, rounding);
  x = _mm256_srai_epi32(x, bit);

  // stage 6
  // stage 7
  // stage 8
  // stage 9
  if (do_cols) {
    x = _mm256_max_epi32(x, clamp_lo);
    x = _mm256_min_epi32(x, clamp_hi);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const __m256i clamp_lo_out = _mm256_set1_epi32(AOMMAX(
        -(1 << (log_range_out - 1)), -(1 << (log_range - 1 - out_shift))));
    const __m256i clamp_hi_out = _mm256_set1_epi32(AOMMIN(
        (1 << (log_range_out - 1)) - 1, (1 << (log_range - 1 - out_shift))));
    __m256i offset = _mm256_set1_epi32((1 << out_shift) >> 1);
    x = _mm256_add_epi32(offset, x);
    x = _mm256_sra_epi32(x, _mm_cvtsi32_si128(out_shift));
    x = _mm256_max_epi32(x, clamp_lo_out);
    x = _mm256_min_epi32(x, clamp_hi_out);
  }

  out[0] = x;
  out[1] = x;
  out[2] = x;
  out[3] = x;
  out[4] = x;
  out[5] = x;
  out[6] = x;
  out[7] = x;
  out[8] = x;
  out[9] = x;
  out[10] = x;
  out[11] = x;
  out[12] = x;
  out[13] = x;
  out[14] = x;
  out[15] = x;
  out[16] = x;
  out[17] = x;
  out[18] = x;
  out[19] = x;
  out[20] = x;
  out[21] = x;
  out[22] = x;
  out[23] = x;
  out[24] = x;
  out[25] = x;
  out[26] = x;
  out[27] = x;
  out[28] = x;
  out[29] = x;
  out[30] = x;
  out[31] = x;
}

static void idct32_low8_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                             int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
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
  __m256i bf1[32];

  {
    // stage 0
    // stage 1
    bf1[0] = in[0];
    bf1[4] = in[4];
    bf1[8] = in[2];
    bf1[12] = in[6];
    bf1[16] = in[1];
    bf1[20] = in[5];
    bf1[24] = in[3];
    bf1[28] = in[7];

    // stage 2
    bf1[31] = half_btf_0_avx2(&cospi2, &bf1[16], &rounding, bit);
    bf1[16] = half_btf_0_avx2(&cospi62, &bf1[16], &rounding, bit);
    bf1[19] = half_btf_0_avx2(&cospim50, &bf1[28], &rounding, bit);
    bf1[28] = half_btf_0_avx2(&cospi14, &bf1[28], &rounding, bit);
    bf1[27] = half_btf_0_avx2(&cospi10, &bf1[20], &rounding, bit);
    bf1[20] = half_btf_0_avx2(&cospi54, &bf1[20], &rounding, bit);
    bf1[23] = half_btf_0_avx2(&cospim58, &bf1[24], &rounding, bit);
    bf1[24] = half_btf_0_avx2(&cospi6, &bf1[24], &rounding, bit);

    // stage 3
    bf1[15] = half_btf_0_avx2(&cospi4, &bf1[8], &rounding, bit);
    bf1[8] = half_btf_0_avx2(&cospi60, &bf1[8], &rounding, bit);

    bf1[11] = half_btf_0_avx2(&cospim52, &bf1[12], &rounding, bit);
    bf1[12] = half_btf_0_avx2(&cospi12, &bf1[12], &rounding, bit);
    bf1[17] = bf1[16];
    bf1[18] = bf1[19];
    bf1[21] = bf1[20];
    bf1[22] = bf1[23];
    bf1[25] = bf1[24];
    bf1[26] = bf1[27];
    bf1[29] = bf1[28];
    bf1[30] = bf1[31];

    // stage 4
    bf1[7] = half_btf_0_avx2(&cospi8, &bf1[4], &rounding, bit);
    bf1[4] = half_btf_0_avx2(&cospi56, &bf1[4], &rounding, bit);

    bf1[9] = bf1[8];
    bf1[10] = bf1[11];
    bf1[13] = bf1[12];
    bf1[14] = bf1[15];

    idct32_stage4_avx2(bf1, &cospim8, &cospi56, &cospi8, &cospim56, &cospim40,
                       &cospi24, &cospi40, &cospim24, &rounding, bit);

    // stage 5
    bf1[0] = half_btf_0_avx2(&cospi32, &bf1[0], &rounding, bit);
    bf1[1] = bf1[0];
    bf1[5] = bf1[4];
    bf1[6] = bf1[7];

    idct32_stage5_avx2(bf1, &cospim16, &cospi48, &cospi16, &cospim48, &clamp_lo,
                       &clamp_hi, &rounding, bit);

    // stage 6
    bf1[3] = bf1[0];
    bf1[2] = bf1[1];

    idct32_stage6_avx2(bf1, &cospim32, &cospi32, &cospim16, &cospi48, &cospi16,
                       &cospim48, &clamp_lo, &clamp_hi, &rounding, bit);

    // stage 7
    idct32_stage7_avx2(bf1, &cospim32, &cospi32, &clamp_lo, &clamp_hi,
                       &rounding, bit);

    // stage 8
    idct32_stage8_avx2(bf1, &cospim32, &cospi32, &clamp_lo, &clamp_hi,
                       &rounding, bit);

    // stage 9
    idct32_stage9_avx2(bf1, out, do_cols, bd, out_shift, log_range);
  }
}

static void idct32_low16_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                              int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
  const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
  const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospim42 = _mm256_set1_epi32(-cospi[42]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospim34 = _mm256_set1_epi32(-cospi[34]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
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
  __m256i bf1[32];

  {
    // stage 0
    // stage 1
    bf1[0] = in[0];
    bf1[2] = in[8];
    bf1[4] = in[4];
    bf1[6] = in[12];
    bf1[8] = in[2];
    bf1[10] = in[10];
    bf1[12] = in[6];
    bf1[14] = in[14];
    bf1[16] = in[1];
    bf1[18] = in[9];
    bf1[20] = in[5];
    bf1[22] = in[13];
    bf1[24] = in[3];
    bf1[26] = in[11];
    bf1[28] = in[7];
    bf1[30] = in[15];

    // stage 2
    bf1[31] = half_btf_0_avx2(&cospi2, &bf1[16], &rounding, bit);
    bf1[16] = half_btf_0_avx2(&cospi62, &bf1[16], &rounding, bit);
    bf1[17] = half_btf_0_avx2(&cospim34, &bf1[30], &rounding, bit);
    bf1[30] = half_btf_0_avx2(&cospi30, &bf1[30], &rounding, bit);
    bf1[29] = half_btf_0_avx2(&cospi18, &bf1[18], &rounding, bit);
    bf1[18] = half_btf_0_avx2(&cospi46, &bf1[18], &rounding, bit);
    bf1[19] = half_btf_0_avx2(&cospim50, &bf1[28], &rounding, bit);
    bf1[28] = half_btf_0_avx2(&cospi14, &bf1[28], &rounding, bit);
    bf1[27] = half_btf_0_avx2(&cospi10, &bf1[20], &rounding, bit);
    bf1[20] = half_btf_0_avx2(&cospi54, &bf1[20], &rounding, bit);
    bf1[21] = half_btf_0_avx2(&cospim42, &bf1[26], &rounding, bit);
    bf1[26] = half_btf_0_avx2(&cospi22, &bf1[26], &rounding, bit);
    bf1[25] = half_btf_0_avx2(&cospi26, &bf1[22], &rounding, bit);
    bf1[22] = half_btf_0_avx2(&cospi38, &bf1[22], &rounding, bit);
    bf1[23] = half_btf_0_avx2(&cospim58, &bf1[24], &rounding, bit);
    bf1[24] = half_btf_0_avx2(&cospi6, &bf1[24], &rounding, bit);

    // stage 3
    bf1[15] = half_btf_0_avx2(&cospi4, &bf1[8], &rounding, bit);
    bf1[8] = half_btf_0_avx2(&cospi60, &bf1[8], &rounding, bit);
    bf1[9] = half_btf_0_avx2(&cospim36, &bf1[14], &rounding, bit);
    bf1[14] = half_btf_0_avx2(&cospi28, &bf1[14], &rounding, bit);
    bf1[13] = half_btf_0_avx2(&cospi20, &bf1[10], &rounding, bit);
    bf1[10] = half_btf_0_avx2(&cospi44, &bf1[10], &rounding, bit);
    bf1[11] = half_btf_0_avx2(&cospim52, &bf1[12], &rounding, bit);
    bf1[12] = half_btf_0_avx2(&cospi12, &bf1[12], &rounding, bit);

    addsub_avx2(bf1[16], bf1[17], bf1 + 16, bf1 + 17, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[19], bf1[18], bf1 + 19, bf1 + 18, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[20], bf1[21], bf1 + 20, bf1 + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[23], bf1[22], bf1 + 23, bf1 + 22, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[24], bf1[25], bf1 + 24, bf1 + 25, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[27], bf1[26], bf1 + 27, bf1 + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[28], bf1[29], bf1 + 28, bf1 + 29, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[31], bf1[30], bf1 + 31, bf1 + 30, &clamp_lo, &clamp_hi);

    // stage 4
    bf1[7] = half_btf_0_avx2(&cospi8, &bf1[4], &rounding, bit);
    bf1[4] = half_btf_0_avx2(&cospi56, &bf1[4], &rounding, bit);
    bf1[5] = half_btf_0_avx2(&cospim40, &bf1[6], &rounding, bit);
    bf1[6] = half_btf_0_avx2(&cospi24, &bf1[6], &rounding, bit);

    addsub_avx2(bf1[8], bf1[9], bf1 + 8, bf1 + 9, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[11], bf1[10], bf1 + 11, bf1 + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[12], bf1[13], bf1 + 12, bf1 + 13, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[15], bf1[14], bf1 + 15, bf1 + 14, &clamp_lo, &clamp_hi);

    idct32_stage4_avx2(bf1, &cospim8, &cospi56, &cospi8, &cospim56, &cospim40,
                       &cospi24, &cospi40, &cospim24, &rounding, bit);

    // stage 5
    bf1[0] = half_btf_0_avx2(&cospi32, &bf1[0], &rounding, bit);
    bf1[1] = bf1[0];
    bf1[3] = half_btf_0_avx2(&cospi16, &bf1[2], &rounding, bit);
    bf1[2] = half_btf_0_avx2(&cospi48, &bf1[2], &rounding, bit);

    addsub_avx2(bf1[4], bf1[5], bf1 + 4, bf1 + 5, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[7], bf1[6], bf1 + 7, bf1 + 6, &clamp_lo, &clamp_hi);

    idct32_stage5_avx2(bf1, &cospim16, &cospi48, &cospi16, &cospim48, &clamp_lo,
                       &clamp_hi, &rounding, bit);

    // stage 6
    addsub_avx2(bf1[0], bf1[3], bf1 + 0, bf1 + 3, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[1], bf1[2], bf1 + 1, bf1 + 2, &clamp_lo, &clamp_hi);

    idct32_stage6_avx2(bf1, &cospim32, &cospi32, &cospim16, &cospi48, &cospi16,
                       &cospim48, &clamp_lo, &clamp_hi, &rounding, bit);

    // stage 7
    idct32_stage7_avx2(bf1, &cospim32, &cospi32, &clamp_lo, &clamp_hi,
                       &rounding, bit);

    // stage 8
    idct32_stage8_avx2(bf1, &cospim32, &cospi32, &clamp_lo, &clamp_hi,
                       &rounding, bit);

    // stage 9
    idct32_stage9_avx2(bf1, out, do_cols, bd, out_shift, log_range);
  }
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

  {
    // stage 0
    // stage 1
    bf1[0] = in[0];
    bf1[1] = in[16];
    bf1[2] = in[8];
    bf1[3] = in[24];
    bf1[4] = in[4];
    bf1[5] = in[20];
    bf1[6] = in[12];
    bf1[7] = in[28];
    bf1[8] = in[2];
    bf1[9] = in[18];
    bf1[10] = in[10];
    bf1[11] = in[26];
    bf1[12] = in[6];
    bf1[13] = in[22];
    bf1[14] = in[14];
    bf1[15] = in[30];
    bf1[16] = in[1];
    bf1[17] = in[17];
    bf1[18] = in[9];
    bf1[19] = in[25];
    bf1[20] = in[5];
    bf1[21] = in[21];
    bf1[22] = in[13];
    bf1[23] = in[29];
    bf1[24] = in[3];
    bf1[25] = in[19];
    bf1[26] = in[11];
    bf1[27] = in[27];
    bf1[28] = in[7];
    bf1[29] = in[23];
    bf1[30] = in[15];
    bf1[31] = in[31];

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
      addsub_no_clamp_avx2(bf0[0], bf0[31], out + 0, out + 31);
      addsub_no_clamp_avx2(bf0[1], bf0[30], out + 1, out + 30);
      addsub_no_clamp_avx2(bf0[2], bf0[29], out + 2, out + 29);
      addsub_no_clamp_avx2(bf0[3], bf0[28], out + 3, out + 28);
      addsub_no_clamp_avx2(bf0[4], bf0[27], out + 4, out + 27);
      addsub_no_clamp_avx2(bf0[5], bf0[26], out + 5, out + 26);
      addsub_no_clamp_avx2(bf0[6], bf0[25], out + 6, out + 25);
      addsub_no_clamp_avx2(bf0[7], bf0[24], out + 7, out + 24);
      addsub_no_clamp_avx2(bf0[8], bf0[23], out + 8, out + 23);
      addsub_no_clamp_avx2(bf0[9], bf0[22], out + 9, out + 22);
      addsub_no_clamp_avx2(bf0[10], bf0[21], out + 10, out + 21);
      addsub_no_clamp_avx2(bf0[11], bf0[20], out + 11, out + 20);
      addsub_no_clamp_avx2(bf0[12], bf0[19], out + 12, out + 19);
      addsub_no_clamp_avx2(bf0[13], bf0[18], out + 13, out + 18);
      addsub_no_clamp_avx2(bf0[14], bf0[17], out + 14, out + 17);
      addsub_no_clamp_avx2(bf0[15], bf0[16], out + 15, out + 16);
    } else {
      const int log_range_out = AOMMAX(16, bd + 6);
      const __m256i clamp_lo_out = _mm256_set1_epi32(AOMMAX(
          -(1 << (log_range_out - 1)), -(1 << (log_range - 1 - out_shift))));
      const __m256i clamp_hi_out = _mm256_set1_epi32(AOMMIN(
          (1 << (log_range_out - 1)) - 1, (1 << (log_range - 1 - out_shift))));

      addsub_shift_avx2(bf0[0], bf0[31], out + 0, out + 31, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[1], bf0[30], out + 1, out + 30, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[2], bf0[29], out + 2, out + 29, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[3], bf0[28], out + 3, out + 28, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[4], bf0[27], out + 4, out + 27, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[5], bf0[26], out + 5, out + 26, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[6], bf0[25], out + 6, out + 25, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[7], bf0[24], out + 7, out + 24, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[8], bf0[23], out + 8, out + 23, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[9], bf0[22], out + 9, out + 22, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[10], bf0[21], out + 10, out + 21, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[11], bf0[20], out + 11, out + 20, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[12], bf0[19], out + 12, out + 19, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[13], bf0[18], out + 13, out + 18, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[14], bf0[17], out + 14, out + 17, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
      addsub_shift_avx2(bf0[15], bf0[16], out + 15, out + 16, &clamp_lo_out,
                        &clamp_hi_out, out_shift);
    }
  }
}

typedef void (*transform_1d_avx2)(__m256i *in, __m256i *out, int bit,
                                  int do_cols, int bd, int out_shift);

static const transform_1d_avx2
    highbd_txfm_all_1d_zeros_w8_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },
      {
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { idct32_low1_avx2, idct32_low8_avx2, idct32_low16_avx2, idct32_avx2 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },

      { { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };

static void highbd_inv_txfm2d_add_no_identity_avx2(const int32_t *input,
                                                   uint16_t *output, int stride,
                                                   TX_TYPE tx_type,
                                                   TX_SIZE tx_size, int eob,
                                                   const int bd) {
  __m256i buf1[64 * 2];
  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_nonzero_w_div8 = (eobx + 8) >> 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int input_stride = AOMMIN(32, txfm_size_col);

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_avx2 row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_avx2 col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // 1st stage: column transform
  for (int i = 0; i < buf_size_nonzero_h_div8; i++) {
    __m256i buf0[32];
    const int32_t *input_row = input + i * input_stride * 8;
    for (int j = 0; j < buf_size_nonzero_w_div8; ++j) {
      __m256i *buf0_cur = buf0 + j * 8;
      load_buffer_32x32(input_row + j * 8, buf0_cur, input_stride, 8);

      transpose_8x8_avx2(&buf0_cur[0], &buf0_cur[0]);
    }

    row_txfm(buf0, buf0, inv_cos_bit_row[txw_idx][txh_idx], 0, bd, -shift[0]);

    __m256i *_buf1 = buf1 + i * 8;
    for (int j = 0; j < buf_size_w_div8; ++j) {
      transpose_8x8_avx2(&buf0[j * 8], &_buf1[j * txfm_size_row]);
    }
  }
  // 2nd stage: column transform
  for (int i = 0; i < buf_size_w_div8; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row,
             inv_cos_bit_col[txw_idx][txh_idx], 1, bd, 0);

    av1_round_shift_array_32_avx2(buf1 + i * txfm_size_row,
                                  buf1 + i * txfm_size_row, txfm_size_row,
                                  -shift[1]);
  }

  // write to buffer
  {
    for (int i = 0; i < (txfm_size_col >> 4); i++) {
      highbd_write_buffer_16xn_avx2(buf1 + i * txfm_size_row * 2,
                                    output + 16 * i, stride, ud_flip,
                                    txfm_size_row, bd);
    }
  }
}

void av1_highbd_inv_txfm2d_add_universe_avx2(const int32_t *input,
                                             uint8_t *output, int stride,
                                             TX_TYPE tx_type, TX_SIZE tx_size,
                                             int eob, const int bd) {
  switch (tx_type) {
    case DCT_DCT:
      highbd_inv_txfm2d_add_no_identity_avx2(input, CONVERT_TO_SHORTPTR(output),
                                             stride, tx_type, tx_size, eob, bd);
      break;
    default: assert(0); break;
  }
}

void av1_highbd_inv_txfm_add_32x32_avx2(const tran_low_t *input, uint8_t *dest,
                                        int stride,
                                        const TxfmParam *txfm_param) {
  const int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
      av1_highbd_inv_txfm2d_add_universe_avx2(input, dest, stride, tx_type,
                                              txfm_param->tx_size,
                                              txfm_param->eob, bd);
      break;
      // Assembly version doesn't support IDTX, so use C version for it.
    case IDTX:
      av1_inv_txfm2d_add_32x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 tx_type, bd);
      break;

    default: assert(0);
  }
}

void av1_highbd_inv_txfm_add_avx2(const tran_low_t *input, uint8_t *dest,
                                  int stride, const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const TX_SIZE tx_size = txfm_param->tx_size;
  switch (tx_size) {
    case TX_32X32:
      av1_highbd_inv_txfm_add_32x32_avx2(input, dest, stride, txfm_param);
      break;
    case TX_16X16:
      av1_highbd_inv_txfm_add_16x16_sse4_1(input, dest, stride, txfm_param);
      break;
    case TX_8X8:
      av1_highbd_inv_txfm_add_8x8_sse4_1(input, dest, stride, txfm_param);
      break;
    case TX_4X8:
      av1_highbd_inv_txfm_add_4x8(input, dest, stride, txfm_param);
      break;
    case TX_8X4:
      av1_highbd_inv_txfm_add_8x4(input, dest, stride, txfm_param);
      break;
    case TX_8X16:
      av1_highbd_inv_txfm_add_8x16_sse4_1(input, dest, stride, txfm_param);
      break;
    case TX_16X8:
      av1_highbd_inv_txfm_add_16x8_sse4_1(input, dest, stride, txfm_param);
      break;
    case TX_16X32:
      av1_highbd_inv_txfm_add_16x32(input, dest, stride, txfm_param);
      break;
    case TX_32X16:
      av1_highbd_inv_txfm_add_32x16(input, dest, stride, txfm_param);
      break;
    case TX_32X64:
      av1_highbd_inv_txfm_add_32x64(input, dest, stride, txfm_param);
      break;
    case TX_64X32:
      av1_highbd_inv_txfm_add_64x32(input, dest, stride, txfm_param);
      break;
    case TX_4X4:
      av1_highbd_inv_txfm_add_4x4_sse4_1(input, dest, stride, txfm_param);
      break;
    case TX_16X4:
      av1_highbd_inv_txfm_add_16x4(input, dest, stride, txfm_param);
      break;
    case TX_4X16:
      av1_highbd_inv_txfm_add_4x16(input, dest, stride, txfm_param);
      break;
    case TX_8X32:
      av1_highbd_inv_txfm_add_8x32(input, dest, stride, txfm_param);
      break;
    case TX_32X8:
      av1_highbd_inv_txfm_add_32x8(input, dest, stride, txfm_param);
      break;
    case TX_64X64:
    case TX_16X64:
    case TX_64X16:
      av1_highbd_inv_txfm2d_add_universe_sse4_1(
          input, dest, stride, txfm_param->tx_type, txfm_param->tx_size,
          txfm_param->eob, txfm_param->bd);
      break;
    default: assert(0 && "Invalid transform size"); break;
  }
}
