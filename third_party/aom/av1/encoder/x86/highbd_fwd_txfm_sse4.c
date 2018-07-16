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
#include <smmintrin.h> /* SSE4.1 */

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "av1/common/av1_txfm.h"
#include "av1/common/x86/highbd_txfm_utility_sse4.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "aom_dsp/txfm_common.h"
#include "aom_dsp/x86/txfm_common_sse2.h"
#include "aom_ports/mem.h"

static INLINE void load_buffer_4x4(const int16_t *input, __m128i *in,
                                   int stride, int flipud, int fliplr,
                                   int shift) {
  if (!flipud) {
    in[0] = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
    in[1] = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
    in[2] = _mm_loadl_epi64((const __m128i *)(input + 2 * stride));
    in[3] = _mm_loadl_epi64((const __m128i *)(input + 3 * stride));
  } else {
    in[0] = _mm_loadl_epi64((const __m128i *)(input + 3 * stride));
    in[1] = _mm_loadl_epi64((const __m128i *)(input + 2 * stride));
    in[2] = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
    in[3] = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
  }

  if (fliplr) {
    in[0] = _mm_shufflelo_epi16(in[0], 0x1b);
    in[1] = _mm_shufflelo_epi16(in[1], 0x1b);
    in[2] = _mm_shufflelo_epi16(in[2], 0x1b);
    in[3] = _mm_shufflelo_epi16(in[3], 0x1b);
  }

  in[0] = _mm_cvtepi16_epi32(in[0]);
  in[1] = _mm_cvtepi16_epi32(in[1]);
  in[2] = _mm_cvtepi16_epi32(in[2]);
  in[3] = _mm_cvtepi16_epi32(in[3]);

  in[0] = _mm_slli_epi32(in[0], shift);
  in[1] = _mm_slli_epi32(in[1], shift);
  in[2] = _mm_slli_epi32(in[2], shift);
  in[3] = _mm_slli_epi32(in[3], shift);
}

// We only use stage-2 bit;
// shift[0] is used in load_buffer_4x4()
// shift[1] is used in txfm_func_col()
// shift[2] is used in txfm_func_row()
static void fdct4x4_sse4_1(__m128i *in, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
  const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  __m128i s0, s1, s2, s3;
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3;

  s0 = _mm_add_epi32(in[0], in[3]);
  s1 = _mm_add_epi32(in[1], in[2]);
  s2 = _mm_sub_epi32(in[1], in[2]);
  s3 = _mm_sub_epi32(in[0], in[3]);

  // btf_32_sse4_1_type0(cospi32, cospi32, s[01], u[02], bit);
  u0 = _mm_mullo_epi32(s0, cospi32);
  u1 = _mm_mullo_epi32(s1, cospi32);
  u2 = _mm_add_epi32(u0, u1);
  v0 = _mm_sub_epi32(u0, u1);

  u3 = _mm_add_epi32(u2, rnding);
  v1 = _mm_add_epi32(v0, rnding);

  u0 = _mm_srai_epi32(u3, bit);
  u2 = _mm_srai_epi32(v1, bit);

  // btf_32_sse4_1_type1(cospi48, cospi16, s[23], u[13], bit);
  v0 = _mm_mullo_epi32(s2, cospi48);
  v1 = _mm_mullo_epi32(s3, cospi16);
  v2 = _mm_add_epi32(v0, v1);

  v3 = _mm_add_epi32(v2, rnding);
  u1 = _mm_srai_epi32(v3, bit);

  v0 = _mm_mullo_epi32(s2, cospi16);
  v1 = _mm_mullo_epi32(s3, cospi48);
  v2 = _mm_sub_epi32(v1, v0);

  v3 = _mm_add_epi32(v2, rnding);
  u3 = _mm_srai_epi32(v3, bit);

  // Note: shift[1] and shift[2] are zeros

  // Transpose 4x4 32-bit
  v0 = _mm_unpacklo_epi32(u0, u1);
  v1 = _mm_unpackhi_epi32(u0, u1);
  v2 = _mm_unpacklo_epi32(u2, u3);
  v3 = _mm_unpackhi_epi32(u2, u3);

  in[0] = _mm_unpacklo_epi64(v0, v2);
  in[1] = _mm_unpackhi_epi64(v0, v2);
  in[2] = _mm_unpacklo_epi64(v1, v3);
  in[3] = _mm_unpackhi_epi64(v1, v3);
}

static INLINE void write_buffer_4x4(__m128i *res, int32_t *output) {
  _mm_store_si128((__m128i *)(output + 0 * 4), res[0]);
  _mm_store_si128((__m128i *)(output + 1 * 4), res[1]);
  _mm_store_si128((__m128i *)(output + 2 * 4), res[2]);
  _mm_store_si128((__m128i *)(output + 3 * 4), res[3]);
}

static void fadst4x4_sse4_1(__m128i *in, int bit) {
  const int32_t *sinpi = sinpi_arr(bit);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  const __m128i sinpi1 = _mm_set1_epi32((int)sinpi[1]);
  const __m128i sinpi2 = _mm_set1_epi32((int)sinpi[2]);
  const __m128i sinpi3 = _mm_set1_epi32((int)sinpi[3]);
  const __m128i sinpi4 = _mm_set1_epi32((int)sinpi[4]);
  __m128i t;
  __m128i s0, s1, s2, s3, s4, s5, s6, s7;
  __m128i x0, x1, x2, x3;
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3;

  s0 = _mm_mullo_epi32(in[0], sinpi1);
  s1 = _mm_mullo_epi32(in[0], sinpi4);
  s2 = _mm_mullo_epi32(in[1], sinpi2);
  s3 = _mm_mullo_epi32(in[1], sinpi1);
  s4 = _mm_mullo_epi32(in[2], sinpi3);
  s5 = _mm_mullo_epi32(in[3], sinpi4);
  s6 = _mm_mullo_epi32(in[3], sinpi2);
  t = _mm_add_epi32(in[0], in[1]);
  s7 = _mm_sub_epi32(t, in[3]);

  t = _mm_add_epi32(s0, s2);
  x0 = _mm_add_epi32(t, s5);
  x1 = _mm_mullo_epi32(s7, sinpi3);
  t = _mm_sub_epi32(s1, s3);
  x2 = _mm_add_epi32(t, s6);
  x3 = s4;

  s0 = _mm_add_epi32(x0, x3);
  s1 = x1;
  s2 = _mm_sub_epi32(x2, x3);
  t = _mm_sub_epi32(x2, x0);
  s3 = _mm_add_epi32(t, x3);

  u0 = _mm_add_epi32(s0, rnding);
  u0 = _mm_srai_epi32(u0, bit);

  u1 = _mm_add_epi32(s1, rnding);
  u1 = _mm_srai_epi32(u1, bit);

  u2 = _mm_add_epi32(s2, rnding);
  u2 = _mm_srai_epi32(u2, bit);

  u3 = _mm_add_epi32(s3, rnding);
  u3 = _mm_srai_epi32(u3, bit);

  v0 = _mm_unpacklo_epi32(u0, u1);
  v1 = _mm_unpackhi_epi32(u0, u1);
  v2 = _mm_unpacklo_epi32(u2, u3);
  v3 = _mm_unpackhi_epi32(u2, u3);

  in[0] = _mm_unpacklo_epi64(v0, v2);
  in[1] = _mm_unpackhi_epi64(v0, v2);
  in[2] = _mm_unpacklo_epi64(v1, v3);
  in[3] = _mm_unpackhi_epi64(v1, v3);
}

void av1_fwd_txfm2d_4x4_sse4_1(const int16_t *input, int32_t *coeff,
                               int input_stride, TX_TYPE tx_type, int bd) {
  __m128i in[4];
  const int8_t *shift = fwd_txfm_shift_ls[TX_4X4];
  const int txw_idx = get_txw_idx(TX_4X4);
  const int txh_idx = get_txh_idx(TX_4X4);

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_4x4(input, in, input_stride, 0, 0, shift[0]);
      fdct4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fdct4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    case ADST_DCT:
      load_buffer_4x4(input, in, input_stride, 0, 0, shift[0]);
      fadst4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fdct4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    case DCT_ADST:
      load_buffer_4x4(input, in, input_stride, 0, 0, shift[0]);
      fdct4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fadst4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    case ADST_ADST:
      load_buffer_4x4(input, in, input_stride, 0, 0, shift[0]);
      fadst4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fadst4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    case FLIPADST_DCT:
      load_buffer_4x4(input, in, input_stride, 1, 0, shift[0]);
      fadst4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fdct4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    case DCT_FLIPADST:
      load_buffer_4x4(input, in, input_stride, 0, 1, shift[0]);
      fdct4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fadst4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_4x4(input, in, input_stride, 1, 1, shift[0]);
      fadst4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fadst4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    case ADST_FLIPADST:
      load_buffer_4x4(input, in, input_stride, 0, 1, shift[0]);
      fadst4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fadst4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    case FLIPADST_ADST:
      load_buffer_4x4(input, in, input_stride, 1, 0, shift[0]);
      fadst4x4_sse4_1(in, fwd_cos_bit_col[txw_idx][txh_idx]);
      fadst4x4_sse4_1(in, fwd_cos_bit_row[txw_idx][txh_idx]);
      write_buffer_4x4(in, coeff);
      break;
    default: assert(0);
  }
  (void)bd;
}

static INLINE void load_buffer_8x8(const int16_t *input, __m128i *in,
                                   int stride, int flipud, int fliplr,
                                   int shift) {
  __m128i u;
  if (!flipud) {
    in[0] = _mm_load_si128((const __m128i *)(input + 0 * stride));
    in[1] = _mm_load_si128((const __m128i *)(input + 1 * stride));
    in[2] = _mm_load_si128((const __m128i *)(input + 2 * stride));
    in[3] = _mm_load_si128((const __m128i *)(input + 3 * stride));
    in[4] = _mm_load_si128((const __m128i *)(input + 4 * stride));
    in[5] = _mm_load_si128((const __m128i *)(input + 5 * stride));
    in[6] = _mm_load_si128((const __m128i *)(input + 6 * stride));
    in[7] = _mm_load_si128((const __m128i *)(input + 7 * stride));
  } else {
    in[0] = _mm_load_si128((const __m128i *)(input + 7 * stride));
    in[1] = _mm_load_si128((const __m128i *)(input + 6 * stride));
    in[2] = _mm_load_si128((const __m128i *)(input + 5 * stride));
    in[3] = _mm_load_si128((const __m128i *)(input + 4 * stride));
    in[4] = _mm_load_si128((const __m128i *)(input + 3 * stride));
    in[5] = _mm_load_si128((const __m128i *)(input + 2 * stride));
    in[6] = _mm_load_si128((const __m128i *)(input + 1 * stride));
    in[7] = _mm_load_si128((const __m128i *)(input + 0 * stride));
  }

  if (fliplr) {
    in[0] = mm_reverse_epi16(in[0]);
    in[1] = mm_reverse_epi16(in[1]);
    in[2] = mm_reverse_epi16(in[2]);
    in[3] = mm_reverse_epi16(in[3]);
    in[4] = mm_reverse_epi16(in[4]);
    in[5] = mm_reverse_epi16(in[5]);
    in[6] = mm_reverse_epi16(in[6]);
    in[7] = mm_reverse_epi16(in[7]);
  }

  u = _mm_unpackhi_epi64(in[4], in[4]);
  in[8] = _mm_cvtepi16_epi32(in[4]);
  in[9] = _mm_cvtepi16_epi32(u);

  u = _mm_unpackhi_epi64(in[5], in[5]);
  in[10] = _mm_cvtepi16_epi32(in[5]);
  in[11] = _mm_cvtepi16_epi32(u);

  u = _mm_unpackhi_epi64(in[6], in[6]);
  in[12] = _mm_cvtepi16_epi32(in[6]);
  in[13] = _mm_cvtepi16_epi32(u);

  u = _mm_unpackhi_epi64(in[7], in[7]);
  in[14] = _mm_cvtepi16_epi32(in[7]);
  in[15] = _mm_cvtepi16_epi32(u);

  u = _mm_unpackhi_epi64(in[3], in[3]);
  in[6] = _mm_cvtepi16_epi32(in[3]);
  in[7] = _mm_cvtepi16_epi32(u);

  u = _mm_unpackhi_epi64(in[2], in[2]);
  in[4] = _mm_cvtepi16_epi32(in[2]);
  in[5] = _mm_cvtepi16_epi32(u);

  u = _mm_unpackhi_epi64(in[1], in[1]);
  in[2] = _mm_cvtepi16_epi32(in[1]);
  in[3] = _mm_cvtepi16_epi32(u);

  u = _mm_unpackhi_epi64(in[0], in[0]);
  in[0] = _mm_cvtepi16_epi32(in[0]);
  in[1] = _mm_cvtepi16_epi32(u);

  in[0] = _mm_slli_epi32(in[0], shift);
  in[1] = _mm_slli_epi32(in[1], shift);
  in[2] = _mm_slli_epi32(in[2], shift);
  in[3] = _mm_slli_epi32(in[3], shift);
  in[4] = _mm_slli_epi32(in[4], shift);
  in[5] = _mm_slli_epi32(in[5], shift);
  in[6] = _mm_slli_epi32(in[6], shift);
  in[7] = _mm_slli_epi32(in[7], shift);

  in[8] = _mm_slli_epi32(in[8], shift);
  in[9] = _mm_slli_epi32(in[9], shift);
  in[10] = _mm_slli_epi32(in[10], shift);
  in[11] = _mm_slli_epi32(in[11], shift);
  in[12] = _mm_slli_epi32(in[12], shift);
  in[13] = _mm_slli_epi32(in[13], shift);
  in[14] = _mm_slli_epi32(in[14], shift);
  in[15] = _mm_slli_epi32(in[15], shift);
}

static INLINE void col_txfm_8x8_rounding(__m128i *in, int shift) {
  const __m128i rounding = _mm_set1_epi32(1 << (shift - 1));

  in[0] = _mm_add_epi32(in[0], rounding);
  in[1] = _mm_add_epi32(in[1], rounding);
  in[2] = _mm_add_epi32(in[2], rounding);
  in[3] = _mm_add_epi32(in[3], rounding);
  in[4] = _mm_add_epi32(in[4], rounding);
  in[5] = _mm_add_epi32(in[5], rounding);
  in[6] = _mm_add_epi32(in[6], rounding);
  in[7] = _mm_add_epi32(in[7], rounding);
  in[8] = _mm_add_epi32(in[8], rounding);
  in[9] = _mm_add_epi32(in[9], rounding);
  in[10] = _mm_add_epi32(in[10], rounding);
  in[11] = _mm_add_epi32(in[11], rounding);
  in[12] = _mm_add_epi32(in[12], rounding);
  in[13] = _mm_add_epi32(in[13], rounding);
  in[14] = _mm_add_epi32(in[14], rounding);
  in[15] = _mm_add_epi32(in[15], rounding);

  in[0] = _mm_srai_epi32(in[0], shift);
  in[1] = _mm_srai_epi32(in[1], shift);
  in[2] = _mm_srai_epi32(in[2], shift);
  in[3] = _mm_srai_epi32(in[3], shift);
  in[4] = _mm_srai_epi32(in[4], shift);
  in[5] = _mm_srai_epi32(in[5], shift);
  in[6] = _mm_srai_epi32(in[6], shift);
  in[7] = _mm_srai_epi32(in[7], shift);
  in[8] = _mm_srai_epi32(in[8], shift);
  in[9] = _mm_srai_epi32(in[9], shift);
  in[10] = _mm_srai_epi32(in[10], shift);
  in[11] = _mm_srai_epi32(in[11], shift);
  in[12] = _mm_srai_epi32(in[12], shift);
  in[13] = _mm_srai_epi32(in[13], shift);
  in[14] = _mm_srai_epi32(in[14], shift);
  in[15] = _mm_srai_epi32(in[15], shift);
}

static INLINE void write_buffer_8x8(const __m128i *res, int32_t *output) {
  _mm_store_si128((__m128i *)(output + 0 * 4), res[0]);
  _mm_store_si128((__m128i *)(output + 1 * 4), res[1]);
  _mm_store_si128((__m128i *)(output + 2 * 4), res[2]);
  _mm_store_si128((__m128i *)(output + 3 * 4), res[3]);

  _mm_store_si128((__m128i *)(output + 4 * 4), res[4]);
  _mm_store_si128((__m128i *)(output + 5 * 4), res[5]);
  _mm_store_si128((__m128i *)(output + 6 * 4), res[6]);
  _mm_store_si128((__m128i *)(output + 7 * 4), res[7]);

  _mm_store_si128((__m128i *)(output + 8 * 4), res[8]);
  _mm_store_si128((__m128i *)(output + 9 * 4), res[9]);
  _mm_store_si128((__m128i *)(output + 10 * 4), res[10]);
  _mm_store_si128((__m128i *)(output + 11 * 4), res[11]);

  _mm_store_si128((__m128i *)(output + 12 * 4), res[12]);
  _mm_store_si128((__m128i *)(output + 13 * 4), res[13]);
  _mm_store_si128((__m128i *)(output + 14 * 4), res[14]);
  _mm_store_si128((__m128i *)(output + 15 * 4), res[15]);
}

static void fdct8x8_sse4_1(__m128i *in, __m128i *out, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospim32 = _mm_set1_epi32(-cospi[32]);
  const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
  const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
  const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
  const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
  const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
  const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  __m128i u[8], v[8];

  // Even 8 points 0, 2, ..., 14
  // stage 0
  // stage 1
  u[0] = _mm_add_epi32(in[0], in[14]);
  v[7] = _mm_sub_epi32(in[0], in[14]);  // v[7]
  u[1] = _mm_add_epi32(in[2], in[12]);
  u[6] = _mm_sub_epi32(in[2], in[12]);
  u[2] = _mm_add_epi32(in[4], in[10]);
  u[5] = _mm_sub_epi32(in[4], in[10]);
  u[3] = _mm_add_epi32(in[6], in[8]);
  v[4] = _mm_sub_epi32(in[6], in[8]);  // v[4]

  // stage 2
  v[0] = _mm_add_epi32(u[0], u[3]);
  v[3] = _mm_sub_epi32(u[0], u[3]);
  v[1] = _mm_add_epi32(u[1], u[2]);
  v[2] = _mm_sub_epi32(u[1], u[2]);

  v[5] = _mm_mullo_epi32(u[5], cospim32);
  v[6] = _mm_mullo_epi32(u[6], cospi32);
  v[5] = _mm_add_epi32(v[5], v[6]);
  v[5] = _mm_add_epi32(v[5], rnding);
  v[5] = _mm_srai_epi32(v[5], bit);

  u[0] = _mm_mullo_epi32(u[5], cospi32);
  v[6] = _mm_mullo_epi32(u[6], cospim32);
  v[6] = _mm_sub_epi32(u[0], v[6]);
  v[6] = _mm_add_epi32(v[6], rnding);
  v[6] = _mm_srai_epi32(v[6], bit);

  // stage 3
  // type 0
  v[0] = _mm_mullo_epi32(v[0], cospi32);
  v[1] = _mm_mullo_epi32(v[1], cospi32);
  u[0] = _mm_add_epi32(v[0], v[1]);
  u[0] = _mm_add_epi32(u[0], rnding);
  u[0] = _mm_srai_epi32(u[0], bit);

  u[1] = _mm_sub_epi32(v[0], v[1]);
  u[1] = _mm_add_epi32(u[1], rnding);
  u[1] = _mm_srai_epi32(u[1], bit);

  // type 1
  v[0] = _mm_mullo_epi32(v[2], cospi48);
  v[1] = _mm_mullo_epi32(v[3], cospi16);
  u[2] = _mm_add_epi32(v[0], v[1]);
  u[2] = _mm_add_epi32(u[2], rnding);
  u[2] = _mm_srai_epi32(u[2], bit);

  v[0] = _mm_mullo_epi32(v[2], cospi16);
  v[1] = _mm_mullo_epi32(v[3], cospi48);
  u[3] = _mm_sub_epi32(v[1], v[0]);
  u[3] = _mm_add_epi32(u[3], rnding);
  u[3] = _mm_srai_epi32(u[3], bit);

  u[4] = _mm_add_epi32(v[4], v[5]);
  u[5] = _mm_sub_epi32(v[4], v[5]);
  u[6] = _mm_sub_epi32(v[7], v[6]);
  u[7] = _mm_add_epi32(v[7], v[6]);

  // stage 4
  // stage 5
  v[0] = _mm_mullo_epi32(u[4], cospi56);
  v[1] = _mm_mullo_epi32(u[7], cospi8);
  v[0] = _mm_add_epi32(v[0], v[1]);
  v[0] = _mm_add_epi32(v[0], rnding);
  out[2] = _mm_srai_epi32(v[0], bit);  // buf0[4]

  v[0] = _mm_mullo_epi32(u[4], cospi8);
  v[1] = _mm_mullo_epi32(u[7], cospi56);
  v[0] = _mm_sub_epi32(v[1], v[0]);
  v[0] = _mm_add_epi32(v[0], rnding);
  out[14] = _mm_srai_epi32(v[0], bit);  // buf0[7]

  v[0] = _mm_mullo_epi32(u[5], cospi24);
  v[1] = _mm_mullo_epi32(u[6], cospi40);
  v[0] = _mm_add_epi32(v[0], v[1]);
  v[0] = _mm_add_epi32(v[0], rnding);
  out[10] = _mm_srai_epi32(v[0], bit);  // buf0[5]

  v[0] = _mm_mullo_epi32(u[5], cospi40);
  v[1] = _mm_mullo_epi32(u[6], cospi24);
  v[0] = _mm_sub_epi32(v[1], v[0]);
  v[0] = _mm_add_epi32(v[0], rnding);
  out[6] = _mm_srai_epi32(v[0], bit);  // buf0[6]

  out[0] = u[0];   // buf0[0]
  out[8] = u[1];   // buf0[1]
  out[4] = u[2];   // buf0[2]
  out[12] = u[3];  // buf0[3]

  // Odd 8 points: 1, 3, ..., 15
  // stage 0
  // stage 1
  u[0] = _mm_add_epi32(in[1], in[15]);
  v[7] = _mm_sub_epi32(in[1], in[15]);  // v[7]
  u[1] = _mm_add_epi32(in[3], in[13]);
  u[6] = _mm_sub_epi32(in[3], in[13]);
  u[2] = _mm_add_epi32(in[5], in[11]);
  u[5] = _mm_sub_epi32(in[5], in[11]);
  u[3] = _mm_add_epi32(in[7], in[9]);
  v[4] = _mm_sub_epi32(in[7], in[9]);  // v[4]

  // stage 2
  v[0] = _mm_add_epi32(u[0], u[3]);
  v[3] = _mm_sub_epi32(u[0], u[3]);
  v[1] = _mm_add_epi32(u[1], u[2]);
  v[2] = _mm_sub_epi32(u[1], u[2]);

  v[5] = _mm_mullo_epi32(u[5], cospim32);
  v[6] = _mm_mullo_epi32(u[6], cospi32);
  v[5] = _mm_add_epi32(v[5], v[6]);
  v[5] = _mm_add_epi32(v[5], rnding);
  v[5] = _mm_srai_epi32(v[5], bit);

  u[0] = _mm_mullo_epi32(u[5], cospi32);
  v[6] = _mm_mullo_epi32(u[6], cospim32);
  v[6] = _mm_sub_epi32(u[0], v[6]);
  v[6] = _mm_add_epi32(v[6], rnding);
  v[6] = _mm_srai_epi32(v[6], bit);

  // stage 3
  // type 0
  v[0] = _mm_mullo_epi32(v[0], cospi32);
  v[1] = _mm_mullo_epi32(v[1], cospi32);
  u[0] = _mm_add_epi32(v[0], v[1]);
  u[0] = _mm_add_epi32(u[0], rnding);
  u[0] = _mm_srai_epi32(u[0], bit);

  u[1] = _mm_sub_epi32(v[0], v[1]);
  u[1] = _mm_add_epi32(u[1], rnding);
  u[1] = _mm_srai_epi32(u[1], bit);

  // type 1
  v[0] = _mm_mullo_epi32(v[2], cospi48);
  v[1] = _mm_mullo_epi32(v[3], cospi16);
  u[2] = _mm_add_epi32(v[0], v[1]);
  u[2] = _mm_add_epi32(u[2], rnding);
  u[2] = _mm_srai_epi32(u[2], bit);

  v[0] = _mm_mullo_epi32(v[2], cospi16);
  v[1] = _mm_mullo_epi32(v[3], cospi48);
  u[3] = _mm_sub_epi32(v[1], v[0]);
  u[3] = _mm_add_epi32(u[3], rnding);
  u[3] = _mm_srai_epi32(u[3], bit);

  u[4] = _mm_add_epi32(v[4], v[5]);
  u[5] = _mm_sub_epi32(v[4], v[5]);
  u[6] = _mm_sub_epi32(v[7], v[6]);
  u[7] = _mm_add_epi32(v[7], v[6]);

  // stage 4
  // stage 5
  v[0] = _mm_mullo_epi32(u[4], cospi56);
  v[1] = _mm_mullo_epi32(u[7], cospi8);
  v[0] = _mm_add_epi32(v[0], v[1]);
  v[0] = _mm_add_epi32(v[0], rnding);
  out[3] = _mm_srai_epi32(v[0], bit);  // buf0[4]

  v[0] = _mm_mullo_epi32(u[4], cospi8);
  v[1] = _mm_mullo_epi32(u[7], cospi56);
  v[0] = _mm_sub_epi32(v[1], v[0]);
  v[0] = _mm_add_epi32(v[0], rnding);
  out[15] = _mm_srai_epi32(v[0], bit);  // buf0[7]

  v[0] = _mm_mullo_epi32(u[5], cospi24);
  v[1] = _mm_mullo_epi32(u[6], cospi40);
  v[0] = _mm_add_epi32(v[0], v[1]);
  v[0] = _mm_add_epi32(v[0], rnding);
  out[11] = _mm_srai_epi32(v[0], bit);  // buf0[5]

  v[0] = _mm_mullo_epi32(u[5], cospi40);
  v[1] = _mm_mullo_epi32(u[6], cospi24);
  v[0] = _mm_sub_epi32(v[1], v[0]);
  v[0] = _mm_add_epi32(v[0], rnding);
  out[7] = _mm_srai_epi32(v[0], bit);  // buf0[6]

  out[1] = u[0];   // buf0[0]
  out[9] = u[1];   // buf0[1]
  out[5] = u[2];   // buf0[2]
  out[13] = u[3];  // buf0[3]
}

static void fadst8x8_sse4_1(__m128i *in, __m128i *out, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
  const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
  const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
  const __m128i cospim48 = _mm_set1_epi32(-cospi[48]);
  const __m128i cospi4 = _mm_set1_epi32(cospi[4]);
  const __m128i cospim4 = _mm_set1_epi32(-cospi[4]);
  const __m128i cospi60 = _mm_set1_epi32(cospi[60]);
  const __m128i cospi20 = _mm_set1_epi32(cospi[20]);
  const __m128i cospim20 = _mm_set1_epi32(-cospi[20]);
  const __m128i cospi44 = _mm_set1_epi32(cospi[44]);
  const __m128i cospi28 = _mm_set1_epi32(cospi[28]);
  const __m128i cospi36 = _mm_set1_epi32(cospi[36]);
  const __m128i cospim36 = _mm_set1_epi32(-cospi[36]);
  const __m128i cospi52 = _mm_set1_epi32(cospi[52]);
  const __m128i cospim52 = _mm_set1_epi32(-cospi[52]);
  const __m128i cospi12 = _mm_set1_epi32(cospi[12]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  const __m128i zero = _mm_setzero_si128();
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;
  __m128i x, y;
  int col;

  // Note:
  //  Even column: 0, 2, ..., 14
  //  Odd column: 1, 3, ..., 15
  //  one even column plus one odd column constructs one row (8 coeffs)
  //  total we have 8 rows (8x8).
  for (col = 0; col < 2; ++col) {
    // stage 0
    // stage 1
    u0 = in[2 * 0 + col];
    u1 = _mm_sub_epi32(zero, in[2 * 7 + col]);
    u2 = _mm_sub_epi32(zero, in[2 * 3 + col]);
    u3 = in[2 * 4 + col];
    u4 = _mm_sub_epi32(zero, in[2 * 1 + col]);
    u5 = in[2 * 6 + col];
    u6 = in[2 * 2 + col];
    u7 = _mm_sub_epi32(zero, in[2 * 5 + col]);

    // stage 2
    v0 = u0;
    v1 = u1;

    x = _mm_mullo_epi32(u2, cospi32);
    y = _mm_mullo_epi32(u3, cospi32);
    v2 = _mm_add_epi32(x, y);
    v2 = _mm_add_epi32(v2, rnding);
    v2 = _mm_srai_epi32(v2, bit);

    v3 = _mm_sub_epi32(x, y);
    v3 = _mm_add_epi32(v3, rnding);
    v3 = _mm_srai_epi32(v3, bit);

    v4 = u4;
    v5 = u5;

    x = _mm_mullo_epi32(u6, cospi32);
    y = _mm_mullo_epi32(u7, cospi32);
    v6 = _mm_add_epi32(x, y);
    v6 = _mm_add_epi32(v6, rnding);
    v6 = _mm_srai_epi32(v6, bit);

    v7 = _mm_sub_epi32(x, y);
    v7 = _mm_add_epi32(v7, rnding);
    v7 = _mm_srai_epi32(v7, bit);

    // stage 3
    u0 = _mm_add_epi32(v0, v2);
    u1 = _mm_add_epi32(v1, v3);
    u2 = _mm_sub_epi32(v0, v2);
    u3 = _mm_sub_epi32(v1, v3);
    u4 = _mm_add_epi32(v4, v6);
    u5 = _mm_add_epi32(v5, v7);
    u6 = _mm_sub_epi32(v4, v6);
    u7 = _mm_sub_epi32(v5, v7);

    // stage 4
    v0 = u0;
    v1 = u1;
    v2 = u2;
    v3 = u3;

    x = _mm_mullo_epi32(u4, cospi16);
    y = _mm_mullo_epi32(u5, cospi48);
    v4 = _mm_add_epi32(x, y);
    v4 = _mm_add_epi32(v4, rnding);
    v4 = _mm_srai_epi32(v4, bit);

    x = _mm_mullo_epi32(u4, cospi48);
    y = _mm_mullo_epi32(u5, cospim16);
    v5 = _mm_add_epi32(x, y);
    v5 = _mm_add_epi32(v5, rnding);
    v5 = _mm_srai_epi32(v5, bit);

    x = _mm_mullo_epi32(u6, cospim48);
    y = _mm_mullo_epi32(u7, cospi16);
    v6 = _mm_add_epi32(x, y);
    v6 = _mm_add_epi32(v6, rnding);
    v6 = _mm_srai_epi32(v6, bit);

    x = _mm_mullo_epi32(u6, cospi16);
    y = _mm_mullo_epi32(u7, cospi48);
    v7 = _mm_add_epi32(x, y);
    v7 = _mm_add_epi32(v7, rnding);
    v7 = _mm_srai_epi32(v7, bit);

    // stage 5
    u0 = _mm_add_epi32(v0, v4);
    u1 = _mm_add_epi32(v1, v5);
    u2 = _mm_add_epi32(v2, v6);
    u3 = _mm_add_epi32(v3, v7);
    u4 = _mm_sub_epi32(v0, v4);
    u5 = _mm_sub_epi32(v1, v5);
    u6 = _mm_sub_epi32(v2, v6);
    u7 = _mm_sub_epi32(v3, v7);

    // stage 6
    x = _mm_mullo_epi32(u0, cospi4);
    y = _mm_mullo_epi32(u1, cospi60);
    v0 = _mm_add_epi32(x, y);
    v0 = _mm_add_epi32(v0, rnding);
    v0 = _mm_srai_epi32(v0, bit);

    x = _mm_mullo_epi32(u0, cospi60);
    y = _mm_mullo_epi32(u1, cospim4);
    v1 = _mm_add_epi32(x, y);
    v1 = _mm_add_epi32(v1, rnding);
    v1 = _mm_srai_epi32(v1, bit);

    x = _mm_mullo_epi32(u2, cospi20);
    y = _mm_mullo_epi32(u3, cospi44);
    v2 = _mm_add_epi32(x, y);
    v2 = _mm_add_epi32(v2, rnding);
    v2 = _mm_srai_epi32(v2, bit);

    x = _mm_mullo_epi32(u2, cospi44);
    y = _mm_mullo_epi32(u3, cospim20);
    v3 = _mm_add_epi32(x, y);
    v3 = _mm_add_epi32(v3, rnding);
    v3 = _mm_srai_epi32(v3, bit);

    x = _mm_mullo_epi32(u4, cospi36);
    y = _mm_mullo_epi32(u5, cospi28);
    v4 = _mm_add_epi32(x, y);
    v4 = _mm_add_epi32(v4, rnding);
    v4 = _mm_srai_epi32(v4, bit);

    x = _mm_mullo_epi32(u4, cospi28);
    y = _mm_mullo_epi32(u5, cospim36);
    v5 = _mm_add_epi32(x, y);
    v5 = _mm_add_epi32(v5, rnding);
    v5 = _mm_srai_epi32(v5, bit);

    x = _mm_mullo_epi32(u6, cospi52);
    y = _mm_mullo_epi32(u7, cospi12);
    v6 = _mm_add_epi32(x, y);
    v6 = _mm_add_epi32(v6, rnding);
    v6 = _mm_srai_epi32(v6, bit);

    x = _mm_mullo_epi32(u6, cospi12);
    y = _mm_mullo_epi32(u7, cospim52);
    v7 = _mm_add_epi32(x, y);
    v7 = _mm_add_epi32(v7, rnding);
    v7 = _mm_srai_epi32(v7, bit);

    // stage 7
    out[2 * 0 + col] = v1;
    out[2 * 1 + col] = v6;
    out[2 * 2 + col] = v3;
    out[2 * 3 + col] = v4;
    out[2 * 4 + col] = v5;
    out[2 * 5 + col] = v2;
    out[2 * 6 + col] = v7;
    out[2 * 7 + col] = v0;
  }
}

void av1_fwd_txfm2d_8x8_sse4_1(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  __m128i in[16], out[16];
  const int8_t *shift = fwd_txfm_shift_ls[TX_8X8];
  const int txw_idx = get_txw_idx(TX_8X8);
  const int txh_idx = get_txh_idx(TX_8X8);

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fdct8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fdct8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    case ADST_DCT:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fdct8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    case DCT_ADST:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fdct8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    case ADST_ADST:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    case FLIPADST_DCT:
      load_buffer_8x8(input, in, stride, 1, 0, shift[0]);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fdct8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    case DCT_FLIPADST:
      load_buffer_8x8(input, in, stride, 0, 1, shift[0]);
      fdct8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_8x8(input, in, stride, 1, 1, shift[0]);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    case ADST_FLIPADST:
      load_buffer_8x8(input, in, stride, 0, 1, shift[0]);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    case FLIPADST_ADST:
      load_buffer_8x8(input, in, stride, 1, 0, shift[0]);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_8x8_rounding(out, -shift[1]);
      transpose_8x8(out, in);
      fadst8x8_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_8x8(out, in);
      write_buffer_8x8(in, coeff);
      break;
    default: assert(0);
  }
  (void)bd;
}

// Hybrid Transform 16x16

static INLINE void convert_8x8_to_16x16(const __m128i *in, __m128i *out) {
  int row_index = 0;
  int dst_index = 0;
  int src_index = 0;

  // row 0, 1, .., 7
  do {
    out[dst_index] = in[src_index];
    out[dst_index + 1] = in[src_index + 1];
    out[dst_index + 2] = in[src_index + 16];
    out[dst_index + 3] = in[src_index + 17];
    dst_index += 4;
    src_index += 2;
    row_index += 1;
  } while (row_index < 8);

  // row 8, 9, ..., 15
  src_index += 16;
  do {
    out[dst_index] = in[src_index];
    out[dst_index + 1] = in[src_index + 1];
    out[dst_index + 2] = in[src_index + 16];
    out[dst_index + 3] = in[src_index + 17];
    dst_index += 4;
    src_index += 2;
    row_index += 1;
  } while (row_index < 16);
}

static INLINE void load_buffer_16x16(const int16_t *input, __m128i *out,
                                     int stride, int flipud, int fliplr,
                                     int shift) {
  __m128i in[64];
  // Load 4 8x8 blocks
  const int16_t *topL = input;
  const int16_t *topR = input + 8;
  const int16_t *botL = input + 8 * stride;
  const int16_t *botR = input + 8 * stride + 8;

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

  // load first 8 columns
  load_buffer_8x8(topL, &in[0], stride, flipud, fliplr, shift);
  load_buffer_8x8(botL, &in[32], stride, flipud, fliplr, shift);

  // load second 8 columns
  load_buffer_8x8(topR, &in[16], stride, flipud, fliplr, shift);
  load_buffer_8x8(botR, &in[48], stride, flipud, fliplr, shift);

  convert_8x8_to_16x16(in, out);
}

static void fdct16x16_sse4_1(__m128i *in, __m128i *out, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospim32 = _mm_set1_epi32(-cospi[32]);
  const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
  const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
  const __m128i cospim48 = _mm_set1_epi32(-cospi[48]);
  const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
  const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
  const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
  const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
  const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
  const __m128i cospi60 = _mm_set1_epi32(cospi[60]);
  const __m128i cospi4 = _mm_set1_epi32(cospi[4]);
  const __m128i cospi28 = _mm_set1_epi32(cospi[28]);
  const __m128i cospi36 = _mm_set1_epi32(cospi[36]);
  const __m128i cospi44 = _mm_set1_epi32(cospi[44]);
  const __m128i cospi20 = _mm_set1_epi32(cospi[20]);
  const __m128i cospi12 = _mm_set1_epi32(cospi[12]);
  const __m128i cospi52 = _mm_set1_epi32(cospi[52]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  __m128i u[16], v[16], x;
  const int col_num = 4;
  int col;

  // Calculate the column 0, 1, 2, 3
  for (col = 0; col < col_num; ++col) {
    // stage 0
    // stage 1
    u[0] = _mm_add_epi32(in[0 * col_num + col], in[15 * col_num + col]);
    u[15] = _mm_sub_epi32(in[0 * col_num + col], in[15 * col_num + col]);
    u[1] = _mm_add_epi32(in[1 * col_num + col], in[14 * col_num + col]);
    u[14] = _mm_sub_epi32(in[1 * col_num + col], in[14 * col_num + col]);
    u[2] = _mm_add_epi32(in[2 * col_num + col], in[13 * col_num + col]);
    u[13] = _mm_sub_epi32(in[2 * col_num + col], in[13 * col_num + col]);
    u[3] = _mm_add_epi32(in[3 * col_num + col], in[12 * col_num + col]);
    u[12] = _mm_sub_epi32(in[3 * col_num + col], in[12 * col_num + col]);
    u[4] = _mm_add_epi32(in[4 * col_num + col], in[11 * col_num + col]);
    u[11] = _mm_sub_epi32(in[4 * col_num + col], in[11 * col_num + col]);
    u[5] = _mm_add_epi32(in[5 * col_num + col], in[10 * col_num + col]);
    u[10] = _mm_sub_epi32(in[5 * col_num + col], in[10 * col_num + col]);
    u[6] = _mm_add_epi32(in[6 * col_num + col], in[9 * col_num + col]);
    u[9] = _mm_sub_epi32(in[6 * col_num + col], in[9 * col_num + col]);
    u[7] = _mm_add_epi32(in[7 * col_num + col], in[8 * col_num + col]);
    u[8] = _mm_sub_epi32(in[7 * col_num + col], in[8 * col_num + col]);

    // stage 2
    v[0] = _mm_add_epi32(u[0], u[7]);
    v[7] = _mm_sub_epi32(u[0], u[7]);
    v[1] = _mm_add_epi32(u[1], u[6]);
    v[6] = _mm_sub_epi32(u[1], u[6]);
    v[2] = _mm_add_epi32(u[2], u[5]);
    v[5] = _mm_sub_epi32(u[2], u[5]);
    v[3] = _mm_add_epi32(u[3], u[4]);
    v[4] = _mm_sub_epi32(u[3], u[4]);
    v[8] = u[8];
    v[9] = u[9];

    v[10] = _mm_mullo_epi32(u[10], cospim32);
    x = _mm_mullo_epi32(u[13], cospi32);
    v[10] = _mm_add_epi32(v[10], x);
    v[10] = _mm_add_epi32(v[10], rnding);
    v[10] = _mm_srai_epi32(v[10], bit);

    v[13] = _mm_mullo_epi32(u[10], cospi32);
    x = _mm_mullo_epi32(u[13], cospim32);
    v[13] = _mm_sub_epi32(v[13], x);
    v[13] = _mm_add_epi32(v[13], rnding);
    v[13] = _mm_srai_epi32(v[13], bit);

    v[11] = _mm_mullo_epi32(u[11], cospim32);
    x = _mm_mullo_epi32(u[12], cospi32);
    v[11] = _mm_add_epi32(v[11], x);
    v[11] = _mm_add_epi32(v[11], rnding);
    v[11] = _mm_srai_epi32(v[11], bit);

    v[12] = _mm_mullo_epi32(u[11], cospi32);
    x = _mm_mullo_epi32(u[12], cospim32);
    v[12] = _mm_sub_epi32(v[12], x);
    v[12] = _mm_add_epi32(v[12], rnding);
    v[12] = _mm_srai_epi32(v[12], bit);
    v[14] = u[14];
    v[15] = u[15];

    // stage 3
    u[0] = _mm_add_epi32(v[0], v[3]);
    u[3] = _mm_sub_epi32(v[0], v[3]);
    u[1] = _mm_add_epi32(v[1], v[2]);
    u[2] = _mm_sub_epi32(v[1], v[2]);
    u[4] = v[4];

    u[5] = _mm_mullo_epi32(v[5], cospim32);
    x = _mm_mullo_epi32(v[6], cospi32);
    u[5] = _mm_add_epi32(u[5], x);
    u[5] = _mm_add_epi32(u[5], rnding);
    u[5] = _mm_srai_epi32(u[5], bit);

    u[6] = _mm_mullo_epi32(v[5], cospi32);
    x = _mm_mullo_epi32(v[6], cospim32);
    u[6] = _mm_sub_epi32(u[6], x);
    u[6] = _mm_add_epi32(u[6], rnding);
    u[6] = _mm_srai_epi32(u[6], bit);

    u[7] = v[7];
    u[8] = _mm_add_epi32(v[8], v[11]);
    u[11] = _mm_sub_epi32(v[8], v[11]);
    u[9] = _mm_add_epi32(v[9], v[10]);
    u[10] = _mm_sub_epi32(v[9], v[10]);
    u[12] = _mm_sub_epi32(v[15], v[12]);
    u[15] = _mm_add_epi32(v[15], v[12]);
    u[13] = _mm_sub_epi32(v[14], v[13]);
    u[14] = _mm_add_epi32(v[14], v[13]);

    // stage 4
    u[0] = _mm_mullo_epi32(u[0], cospi32);
    u[1] = _mm_mullo_epi32(u[1], cospi32);
    v[0] = _mm_add_epi32(u[0], u[1]);
    v[0] = _mm_add_epi32(v[0], rnding);
    v[0] = _mm_srai_epi32(v[0], bit);

    v[1] = _mm_sub_epi32(u[0], u[1]);
    v[1] = _mm_add_epi32(v[1], rnding);
    v[1] = _mm_srai_epi32(v[1], bit);

    v[2] = _mm_mullo_epi32(u[2], cospi48);
    x = _mm_mullo_epi32(u[3], cospi16);
    v[2] = _mm_add_epi32(v[2], x);
    v[2] = _mm_add_epi32(v[2], rnding);
    v[2] = _mm_srai_epi32(v[2], bit);

    v[3] = _mm_mullo_epi32(u[2], cospi16);
    x = _mm_mullo_epi32(u[3], cospi48);
    v[3] = _mm_sub_epi32(x, v[3]);
    v[3] = _mm_add_epi32(v[3], rnding);
    v[3] = _mm_srai_epi32(v[3], bit);

    v[4] = _mm_add_epi32(u[4], u[5]);
    v[5] = _mm_sub_epi32(u[4], u[5]);
    v[6] = _mm_sub_epi32(u[7], u[6]);
    v[7] = _mm_add_epi32(u[7], u[6]);
    v[8] = u[8];

    v[9] = _mm_mullo_epi32(u[9], cospim16);
    x = _mm_mullo_epi32(u[14], cospi48);
    v[9] = _mm_add_epi32(v[9], x);
    v[9] = _mm_add_epi32(v[9], rnding);
    v[9] = _mm_srai_epi32(v[9], bit);

    v[14] = _mm_mullo_epi32(u[9], cospi48);
    x = _mm_mullo_epi32(u[14], cospim16);
    v[14] = _mm_sub_epi32(v[14], x);
    v[14] = _mm_add_epi32(v[14], rnding);
    v[14] = _mm_srai_epi32(v[14], bit);

    v[10] = _mm_mullo_epi32(u[10], cospim48);
    x = _mm_mullo_epi32(u[13], cospim16);
    v[10] = _mm_add_epi32(v[10], x);
    v[10] = _mm_add_epi32(v[10], rnding);
    v[10] = _mm_srai_epi32(v[10], bit);

    v[13] = _mm_mullo_epi32(u[10], cospim16);
    x = _mm_mullo_epi32(u[13], cospim48);
    v[13] = _mm_sub_epi32(v[13], x);
    v[13] = _mm_add_epi32(v[13], rnding);
    v[13] = _mm_srai_epi32(v[13], bit);

    v[11] = u[11];
    v[12] = u[12];
    v[15] = u[15];

    // stage 5
    u[0] = v[0];
    u[1] = v[1];
    u[2] = v[2];
    u[3] = v[3];

    u[4] = _mm_mullo_epi32(v[4], cospi56);
    x = _mm_mullo_epi32(v[7], cospi8);
    u[4] = _mm_add_epi32(u[4], x);
    u[4] = _mm_add_epi32(u[4], rnding);
    u[4] = _mm_srai_epi32(u[4], bit);

    u[7] = _mm_mullo_epi32(v[4], cospi8);
    x = _mm_mullo_epi32(v[7], cospi56);
    u[7] = _mm_sub_epi32(x, u[7]);
    u[7] = _mm_add_epi32(u[7], rnding);
    u[7] = _mm_srai_epi32(u[7], bit);

    u[5] = _mm_mullo_epi32(v[5], cospi24);
    x = _mm_mullo_epi32(v[6], cospi40);
    u[5] = _mm_add_epi32(u[5], x);
    u[5] = _mm_add_epi32(u[5], rnding);
    u[5] = _mm_srai_epi32(u[5], bit);

    u[6] = _mm_mullo_epi32(v[5], cospi40);
    x = _mm_mullo_epi32(v[6], cospi24);
    u[6] = _mm_sub_epi32(x, u[6]);
    u[6] = _mm_add_epi32(u[6], rnding);
    u[6] = _mm_srai_epi32(u[6], bit);

    u[8] = _mm_add_epi32(v[8], v[9]);
    u[9] = _mm_sub_epi32(v[8], v[9]);
    u[10] = _mm_sub_epi32(v[11], v[10]);
    u[11] = _mm_add_epi32(v[11], v[10]);
    u[12] = _mm_add_epi32(v[12], v[13]);
    u[13] = _mm_sub_epi32(v[12], v[13]);
    u[14] = _mm_sub_epi32(v[15], v[14]);
    u[15] = _mm_add_epi32(v[15], v[14]);

    // stage 6
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = u[4];
    v[5] = u[5];
    v[6] = u[6];
    v[7] = u[7];

    v[8] = _mm_mullo_epi32(u[8], cospi60);
    x = _mm_mullo_epi32(u[15], cospi4);
    v[8] = _mm_add_epi32(v[8], x);
    v[8] = _mm_add_epi32(v[8], rnding);
    v[8] = _mm_srai_epi32(v[8], bit);

    v[15] = _mm_mullo_epi32(u[8], cospi4);
    x = _mm_mullo_epi32(u[15], cospi60);
    v[15] = _mm_sub_epi32(x, v[15]);
    v[15] = _mm_add_epi32(v[15], rnding);
    v[15] = _mm_srai_epi32(v[15], bit);

    v[9] = _mm_mullo_epi32(u[9], cospi28);
    x = _mm_mullo_epi32(u[14], cospi36);
    v[9] = _mm_add_epi32(v[9], x);
    v[9] = _mm_add_epi32(v[9], rnding);
    v[9] = _mm_srai_epi32(v[9], bit);

    v[14] = _mm_mullo_epi32(u[9], cospi36);
    x = _mm_mullo_epi32(u[14], cospi28);
    v[14] = _mm_sub_epi32(x, v[14]);
    v[14] = _mm_add_epi32(v[14], rnding);
    v[14] = _mm_srai_epi32(v[14], bit);

    v[10] = _mm_mullo_epi32(u[10], cospi44);
    x = _mm_mullo_epi32(u[13], cospi20);
    v[10] = _mm_add_epi32(v[10], x);
    v[10] = _mm_add_epi32(v[10], rnding);
    v[10] = _mm_srai_epi32(v[10], bit);

    v[13] = _mm_mullo_epi32(u[10], cospi20);
    x = _mm_mullo_epi32(u[13], cospi44);
    v[13] = _mm_sub_epi32(x, v[13]);
    v[13] = _mm_add_epi32(v[13], rnding);
    v[13] = _mm_srai_epi32(v[13], bit);

    v[11] = _mm_mullo_epi32(u[11], cospi12);
    x = _mm_mullo_epi32(u[12], cospi52);
    v[11] = _mm_add_epi32(v[11], x);
    v[11] = _mm_add_epi32(v[11], rnding);
    v[11] = _mm_srai_epi32(v[11], bit);

    v[12] = _mm_mullo_epi32(u[11], cospi52);
    x = _mm_mullo_epi32(u[12], cospi12);
    v[12] = _mm_sub_epi32(x, v[12]);
    v[12] = _mm_add_epi32(v[12], rnding);
    v[12] = _mm_srai_epi32(v[12], bit);

    out[0 * col_num + col] = v[0];
    out[1 * col_num + col] = v[8];
    out[2 * col_num + col] = v[4];
    out[3 * col_num + col] = v[12];
    out[4 * col_num + col] = v[2];
    out[5 * col_num + col] = v[10];
    out[6 * col_num + col] = v[6];
    out[7 * col_num + col] = v[14];
    out[8 * col_num + col] = v[1];
    out[9 * col_num + col] = v[9];
    out[10 * col_num + col] = v[5];
    out[11 * col_num + col] = v[13];
    out[12 * col_num + col] = v[3];
    out[13 * col_num + col] = v[11];
    out[14 * col_num + col] = v[7];
    out[15 * col_num + col] = v[15];
  }
}

static void fadst16x16_sse4_1(__m128i *in, __m128i *out, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
  const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
  const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
  const __m128i cospim48 = _mm_set1_epi32(-cospi[48]);
  const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
  const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
  const __m128i cospim56 = _mm_set1_epi32(-cospi[56]);
  const __m128i cospim8 = _mm_set1_epi32(-cospi[8]);
  const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
  const __m128i cospim24 = _mm_set1_epi32(-cospi[24]);
  const __m128i cospim40 = _mm_set1_epi32(-cospi[40]);
  const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
  const __m128i cospi2 = _mm_set1_epi32(cospi[2]);
  const __m128i cospi62 = _mm_set1_epi32(cospi[62]);
  const __m128i cospim2 = _mm_set1_epi32(-cospi[2]);
  const __m128i cospi10 = _mm_set1_epi32(cospi[10]);
  const __m128i cospi54 = _mm_set1_epi32(cospi[54]);
  const __m128i cospim10 = _mm_set1_epi32(-cospi[10]);
  const __m128i cospi18 = _mm_set1_epi32(cospi[18]);
  const __m128i cospi46 = _mm_set1_epi32(cospi[46]);
  const __m128i cospim18 = _mm_set1_epi32(-cospi[18]);
  const __m128i cospi26 = _mm_set1_epi32(cospi[26]);
  const __m128i cospi38 = _mm_set1_epi32(cospi[38]);
  const __m128i cospim26 = _mm_set1_epi32(-cospi[26]);
  const __m128i cospi34 = _mm_set1_epi32(cospi[34]);
  const __m128i cospi30 = _mm_set1_epi32(cospi[30]);
  const __m128i cospim34 = _mm_set1_epi32(-cospi[34]);
  const __m128i cospi42 = _mm_set1_epi32(cospi[42]);
  const __m128i cospi22 = _mm_set1_epi32(cospi[22]);
  const __m128i cospim42 = _mm_set1_epi32(-cospi[42]);
  const __m128i cospi50 = _mm_set1_epi32(cospi[50]);
  const __m128i cospi14 = _mm_set1_epi32(cospi[14]);
  const __m128i cospim50 = _mm_set1_epi32(-cospi[50]);
  const __m128i cospi58 = _mm_set1_epi32(cospi[58]);
  const __m128i cospi6 = _mm_set1_epi32(cospi[6]);
  const __m128i cospim58 = _mm_set1_epi32(-cospi[58]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  const __m128i zero = _mm_setzero_si128();

  __m128i u[16], v[16], x, y;
  int col;

  for (col = 0; col < 4; ++col) {
    // stage 0
    // stage 1
    u[0] = in[0 * 4 + col];
    u[1] = _mm_sub_epi32(zero, in[15 * 4 + col]);
    u[2] = _mm_sub_epi32(zero, in[7 * 4 + col]);
    u[3] = in[8 * 4 + col];
    u[4] = _mm_sub_epi32(zero, in[3 * 4 + col]);
    u[5] = in[12 * 4 + col];
    u[6] = in[4 * 4 + col];
    u[7] = _mm_sub_epi32(zero, in[11 * 4 + col]);
    u[8] = _mm_sub_epi32(zero, in[1 * 4 + col]);
    u[9] = in[14 * 4 + col];
    u[10] = in[6 * 4 + col];
    u[11] = _mm_sub_epi32(zero, in[9 * 4 + col]);
    u[12] = in[2 * 4 + col];
    u[13] = _mm_sub_epi32(zero, in[13 * 4 + col]);
    u[14] = _mm_sub_epi32(zero, in[5 * 4 + col]);
    u[15] = in[10 * 4 + col];

    // stage 2
    v[0] = u[0];
    v[1] = u[1];

    x = _mm_mullo_epi32(u[2], cospi32);
    y = _mm_mullo_epi32(u[3], cospi32);
    v[2] = _mm_add_epi32(x, y);
    v[2] = _mm_add_epi32(v[2], rnding);
    v[2] = _mm_srai_epi32(v[2], bit);

    v[3] = _mm_sub_epi32(x, y);
    v[3] = _mm_add_epi32(v[3], rnding);
    v[3] = _mm_srai_epi32(v[3], bit);

    v[4] = u[4];
    v[5] = u[5];

    x = _mm_mullo_epi32(u[6], cospi32);
    y = _mm_mullo_epi32(u[7], cospi32);
    v[6] = _mm_add_epi32(x, y);
    v[6] = _mm_add_epi32(v[6], rnding);
    v[6] = _mm_srai_epi32(v[6], bit);

    v[7] = _mm_sub_epi32(x, y);
    v[7] = _mm_add_epi32(v[7], rnding);
    v[7] = _mm_srai_epi32(v[7], bit);

    v[8] = u[8];
    v[9] = u[9];

    x = _mm_mullo_epi32(u[10], cospi32);
    y = _mm_mullo_epi32(u[11], cospi32);
    v[10] = _mm_add_epi32(x, y);
    v[10] = _mm_add_epi32(v[10], rnding);
    v[10] = _mm_srai_epi32(v[10], bit);

    v[11] = _mm_sub_epi32(x, y);
    v[11] = _mm_add_epi32(v[11], rnding);
    v[11] = _mm_srai_epi32(v[11], bit);

    v[12] = u[12];
    v[13] = u[13];

    x = _mm_mullo_epi32(u[14], cospi32);
    y = _mm_mullo_epi32(u[15], cospi32);
    v[14] = _mm_add_epi32(x, y);
    v[14] = _mm_add_epi32(v[14], rnding);
    v[14] = _mm_srai_epi32(v[14], bit);

    v[15] = _mm_sub_epi32(x, y);
    v[15] = _mm_add_epi32(v[15], rnding);
    v[15] = _mm_srai_epi32(v[15], bit);

    // stage 3
    u[0] = _mm_add_epi32(v[0], v[2]);
    u[1] = _mm_add_epi32(v[1], v[3]);
    u[2] = _mm_sub_epi32(v[0], v[2]);
    u[3] = _mm_sub_epi32(v[1], v[3]);
    u[4] = _mm_add_epi32(v[4], v[6]);
    u[5] = _mm_add_epi32(v[5], v[7]);
    u[6] = _mm_sub_epi32(v[4], v[6]);
    u[7] = _mm_sub_epi32(v[5], v[7]);
    u[8] = _mm_add_epi32(v[8], v[10]);
    u[9] = _mm_add_epi32(v[9], v[11]);
    u[10] = _mm_sub_epi32(v[8], v[10]);
    u[11] = _mm_sub_epi32(v[9], v[11]);
    u[12] = _mm_add_epi32(v[12], v[14]);
    u[13] = _mm_add_epi32(v[13], v[15]);
    u[14] = _mm_sub_epi32(v[12], v[14]);
    u[15] = _mm_sub_epi32(v[13], v[15]);

    // stage 4
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = half_btf_sse4_1(&cospi16, &u[4], &cospi48, &u[5], &rnding, bit);
    v[5] = half_btf_sse4_1(&cospi48, &u[4], &cospim16, &u[5], &rnding, bit);
    v[6] = half_btf_sse4_1(&cospim48, &u[6], &cospi16, &u[7], &rnding, bit);
    v[7] = half_btf_sse4_1(&cospi16, &u[6], &cospi48, &u[7], &rnding, bit);
    v[8] = u[8];
    v[9] = u[9];
    v[10] = u[10];
    v[11] = u[11];
    v[12] = half_btf_sse4_1(&cospi16, &u[12], &cospi48, &u[13], &rnding, bit);
    v[13] = half_btf_sse4_1(&cospi48, &u[12], &cospim16, &u[13], &rnding, bit);
    v[14] = half_btf_sse4_1(&cospim48, &u[14], &cospi16, &u[15], &rnding, bit);
    v[15] = half_btf_sse4_1(&cospi16, &u[14], &cospi48, &u[15], &rnding, bit);

    // stage 5
    u[0] = _mm_add_epi32(v[0], v[4]);
    u[1] = _mm_add_epi32(v[1], v[5]);
    u[2] = _mm_add_epi32(v[2], v[6]);
    u[3] = _mm_add_epi32(v[3], v[7]);
    u[4] = _mm_sub_epi32(v[0], v[4]);
    u[5] = _mm_sub_epi32(v[1], v[5]);
    u[6] = _mm_sub_epi32(v[2], v[6]);
    u[7] = _mm_sub_epi32(v[3], v[7]);
    u[8] = _mm_add_epi32(v[8], v[12]);
    u[9] = _mm_add_epi32(v[9], v[13]);
    u[10] = _mm_add_epi32(v[10], v[14]);
    u[11] = _mm_add_epi32(v[11], v[15]);
    u[12] = _mm_sub_epi32(v[8], v[12]);
    u[13] = _mm_sub_epi32(v[9], v[13]);
    u[14] = _mm_sub_epi32(v[10], v[14]);
    u[15] = _mm_sub_epi32(v[11], v[15]);

    // stage 6
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = u[4];
    v[5] = u[5];
    v[6] = u[6];
    v[7] = u[7];
    v[8] = half_btf_sse4_1(&cospi8, &u[8], &cospi56, &u[9], &rnding, bit);
    v[9] = half_btf_sse4_1(&cospi56, &u[8], &cospim8, &u[9], &rnding, bit);
    v[10] = half_btf_sse4_1(&cospi40, &u[10], &cospi24, &u[11], &rnding, bit);
    v[11] = half_btf_sse4_1(&cospi24, &u[10], &cospim40, &u[11], &rnding, bit);
    v[12] = half_btf_sse4_1(&cospim56, &u[12], &cospi8, &u[13], &rnding, bit);
    v[13] = half_btf_sse4_1(&cospi8, &u[12], &cospi56, &u[13], &rnding, bit);
    v[14] = half_btf_sse4_1(&cospim24, &u[14], &cospi40, &u[15], &rnding, bit);
    v[15] = half_btf_sse4_1(&cospi40, &u[14], &cospi24, &u[15], &rnding, bit);

    // stage 7
    u[0] = _mm_add_epi32(v[0], v[8]);
    u[1] = _mm_add_epi32(v[1], v[9]);
    u[2] = _mm_add_epi32(v[2], v[10]);
    u[3] = _mm_add_epi32(v[3], v[11]);
    u[4] = _mm_add_epi32(v[4], v[12]);
    u[5] = _mm_add_epi32(v[5], v[13]);
    u[6] = _mm_add_epi32(v[6], v[14]);
    u[7] = _mm_add_epi32(v[7], v[15]);
    u[8] = _mm_sub_epi32(v[0], v[8]);
    u[9] = _mm_sub_epi32(v[1], v[9]);
    u[10] = _mm_sub_epi32(v[2], v[10]);
    u[11] = _mm_sub_epi32(v[3], v[11]);
    u[12] = _mm_sub_epi32(v[4], v[12]);
    u[13] = _mm_sub_epi32(v[5], v[13]);
    u[14] = _mm_sub_epi32(v[6], v[14]);
    u[15] = _mm_sub_epi32(v[7], v[15]);

    // stage 8
    v[0] = half_btf_sse4_1(&cospi2, &u[0], &cospi62, &u[1], &rnding, bit);
    v[1] = half_btf_sse4_1(&cospi62, &u[0], &cospim2, &u[1], &rnding, bit);
    v[2] = half_btf_sse4_1(&cospi10, &u[2], &cospi54, &u[3], &rnding, bit);
    v[3] = half_btf_sse4_1(&cospi54, &u[2], &cospim10, &u[3], &rnding, bit);
    v[4] = half_btf_sse4_1(&cospi18, &u[4], &cospi46, &u[5], &rnding, bit);
    v[5] = half_btf_sse4_1(&cospi46, &u[4], &cospim18, &u[5], &rnding, bit);
    v[6] = half_btf_sse4_1(&cospi26, &u[6], &cospi38, &u[7], &rnding, bit);
    v[7] = half_btf_sse4_1(&cospi38, &u[6], &cospim26, &u[7], &rnding, bit);
    v[8] = half_btf_sse4_1(&cospi34, &u[8], &cospi30, &u[9], &rnding, bit);
    v[9] = half_btf_sse4_1(&cospi30, &u[8], &cospim34, &u[9], &rnding, bit);
    v[10] = half_btf_sse4_1(&cospi42, &u[10], &cospi22, &u[11], &rnding, bit);
    v[11] = half_btf_sse4_1(&cospi22, &u[10], &cospim42, &u[11], &rnding, bit);
    v[12] = half_btf_sse4_1(&cospi50, &u[12], &cospi14, &u[13], &rnding, bit);
    v[13] = half_btf_sse4_1(&cospi14, &u[12], &cospim50, &u[13], &rnding, bit);
    v[14] = half_btf_sse4_1(&cospi58, &u[14], &cospi6, &u[15], &rnding, bit);
    v[15] = half_btf_sse4_1(&cospi6, &u[14], &cospim58, &u[15], &rnding, bit);

    // stage 9
    out[0 * 4 + col] = v[1];
    out[1 * 4 + col] = v[14];
    out[2 * 4 + col] = v[3];
    out[3 * 4 + col] = v[12];
    out[4 * 4 + col] = v[5];
    out[5 * 4 + col] = v[10];
    out[6 * 4 + col] = v[7];
    out[7 * 4 + col] = v[8];
    out[8 * 4 + col] = v[9];
    out[9 * 4 + col] = v[6];
    out[10 * 4 + col] = v[11];
    out[11 * 4 + col] = v[4];
    out[12 * 4 + col] = v[13];
    out[13 * 4 + col] = v[2];
    out[14 * 4 + col] = v[15];
    out[15 * 4 + col] = v[0];
  }
}

static void col_txfm_16x16_rounding(__m128i *in, int shift) {
  // Note:
  //  We split 16x16 rounding into 4 sections of 8x8 rounding,
  //  instead of 4 columns
  col_txfm_8x8_rounding(&in[0], shift);
  col_txfm_8x8_rounding(&in[16], shift);
  col_txfm_8x8_rounding(&in[32], shift);
  col_txfm_8x8_rounding(&in[48], shift);
}

static void write_buffer_16x16(const __m128i *in, int32_t *output) {
  const int size_8x8 = 16 * 4;
  write_buffer_8x8(&in[0], output);
  output += size_8x8;
  write_buffer_8x8(&in[16], output);
  output += size_8x8;
  write_buffer_8x8(&in[32], output);
  output += size_8x8;
  write_buffer_8x8(&in[48], output);
}

void av1_fwd_txfm2d_16x16_sse4_1(const int16_t *input, int32_t *coeff,
                                 int stride, TX_TYPE tx_type, int bd) {
  __m128i in[64], out[64];
  const int8_t *shift = fwd_txfm_shift_ls[TX_16X16];
  const int txw_idx = get_txw_idx(TX_16X16);
  const int txh_idx = get_txh_idx(TX_16X16);
  switch (tx_type) {
    case DCT_DCT:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fdct16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fdct16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    case ADST_DCT:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fdct16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    case DCT_ADST:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fdct16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    case ADST_ADST:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    case FLIPADST_DCT:
      load_buffer_16x16(input, in, stride, 1, 0, shift[0]);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fdct16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    case DCT_FLIPADST:
      load_buffer_16x16(input, in, stride, 0, 1, shift[0]);
      fdct16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_16x16(input, in, stride, 1, 1, shift[0]);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    case ADST_FLIPADST:
      load_buffer_16x16(input, in, stride, 0, 1, shift[0]);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    case FLIPADST_ADST:
      load_buffer_16x16(input, in, stride, 1, 0, shift[0]);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_col[txw_idx][txh_idx]);
      col_txfm_16x16_rounding(out, -shift[1]);
      transpose_16x16(out, in);
      fadst16x16_sse4_1(in, out, fwd_cos_bit_row[txw_idx][txh_idx]);
      transpose_16x16(out, in);
      write_buffer_16x16(in, coeff);
      break;
    default: assert(0);
  }
  (void)bd;
}
