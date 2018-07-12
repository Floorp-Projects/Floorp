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
#include <emmintrin.h>  // SSE2

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "aom_dsp/txfm_common.h"
#include "aom_dsp/x86/fwd_txfm_sse2.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/txfm_common_sse2.h"
#include "aom_ports/mem.h"

static INLINE void load_buffer_4x4(const int16_t *input, __m128i *in,
                                   int stride, int flipud, int fliplr) {
  const __m128i k__nonzero_bias_a = _mm_setr_epi16(0, 1, 1, 1, 1, 1, 1, 1);
  const __m128i k__nonzero_bias_b = _mm_setr_epi16(1, 0, 0, 0, 0, 0, 0, 0);
  __m128i mask;

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

  in[0] = _mm_slli_epi16(in[0], 4);
  in[1] = _mm_slli_epi16(in[1], 4);
  in[2] = _mm_slli_epi16(in[2], 4);
  in[3] = _mm_slli_epi16(in[3], 4);

  mask = _mm_cmpeq_epi16(in[0], k__nonzero_bias_a);
  in[0] = _mm_add_epi16(in[0], mask);
  in[0] = _mm_add_epi16(in[0], k__nonzero_bias_b);
}

static INLINE void write_buffer_4x4(tran_low_t *output, __m128i *res) {
  const __m128i kOne = _mm_set1_epi16(1);
  __m128i in01 = _mm_unpacklo_epi64(res[0], res[1]);
  __m128i in23 = _mm_unpacklo_epi64(res[2], res[3]);
  __m128i out01 = _mm_add_epi16(in01, kOne);
  __m128i out23 = _mm_add_epi16(in23, kOne);
  out01 = _mm_srai_epi16(out01, 2);
  out23 = _mm_srai_epi16(out23, 2);
  store_output(&out01, (output + 0 * 8));
  store_output(&out23, (output + 1 * 8));
}

static INLINE void transpose_4x4(__m128i *res) {
  // Combine and transpose
  // 00 01 02 03 20 21 22 23
  // 10 11 12 13 30 31 32 33
  const __m128i tr0_0 = _mm_unpacklo_epi16(res[0], res[1]);
  const __m128i tr0_1 = _mm_unpackhi_epi16(res[0], res[1]);

  // 00 10 01 11 02 12 03 13
  // 20 30 21 31 22 32 23 33
  res[0] = _mm_unpacklo_epi32(tr0_0, tr0_1);
  res[2] = _mm_unpackhi_epi32(tr0_0, tr0_1);

  // 00 10 20 30 01 11 21 31
  // 02 12 22 32 03 13 23 33
  // only use the first 4 16-bit integers
  res[1] = _mm_unpackhi_epi64(res[0], res[0]);
  res[3] = _mm_unpackhi_epi64(res[2], res[2]);
}

static void fdct4_sse2(__m128i *in) {
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  __m128i u[4], v[4];
  u[0] = _mm_unpacklo_epi16(in[0], in[1]);
  u[1] = _mm_unpacklo_epi16(in[3], in[2]);

  v[0] = _mm_add_epi16(u[0], u[1]);
  v[1] = _mm_sub_epi16(u[0], u[1]);

  u[0] = _mm_madd_epi16(v[0], k__cospi_p16_p16);  // 0
  u[1] = _mm_madd_epi16(v[0], k__cospi_p16_m16);  // 2
  u[2] = _mm_madd_epi16(v[1], k__cospi_p08_p24);  // 1
  u[3] = _mm_madd_epi16(v[1], k__cospi_p24_m08);  // 3

  v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
  u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u[0], u[1]);
  in[1] = _mm_packs_epi32(u[2], u[3]);
  transpose_4x4(in);
}

static void fadst4_sse2(__m128i *in) {
  const __m128i k__sinpi_p01_p02 = pair_set_epi16(sinpi_1_9, sinpi_2_9);
  const __m128i k__sinpi_p04_m01 = pair_set_epi16(sinpi_4_9, -sinpi_1_9);
  const __m128i k__sinpi_p03_p04 = pair_set_epi16(sinpi_3_9, sinpi_4_9);
  const __m128i k__sinpi_m03_p02 = pair_set_epi16(-sinpi_3_9, sinpi_2_9);
  const __m128i k__sinpi_p03_p03 = _mm_set1_epi16((int16_t)sinpi_3_9);
  const __m128i kZero = _mm_set1_epi16(0);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  __m128i u[8], v[8];
  __m128i in7 = _mm_add_epi16(in[0], in[1]);

  u[0] = _mm_unpacklo_epi16(in[0], in[1]);
  u[1] = _mm_unpacklo_epi16(in[2], in[3]);
  u[2] = _mm_unpacklo_epi16(in7, kZero);
  u[3] = _mm_unpacklo_epi16(in[2], kZero);
  u[4] = _mm_unpacklo_epi16(in[3], kZero);

  v[0] = _mm_madd_epi16(u[0], k__sinpi_p01_p02);  // s0 + s2
  v[1] = _mm_madd_epi16(u[1], k__sinpi_p03_p04);  // s4 + s5
  v[2] = _mm_madd_epi16(u[2], k__sinpi_p03_p03);  // x1
  v[3] = _mm_madd_epi16(u[0], k__sinpi_p04_m01);  // s1 - s3
  v[4] = _mm_madd_epi16(u[1], k__sinpi_m03_p02);  // -s4 + s6
  v[5] = _mm_madd_epi16(u[3], k__sinpi_p03_p03);  // s4
  v[6] = _mm_madd_epi16(u[4], k__sinpi_p03_p03);

  u[0] = _mm_add_epi32(v[0], v[1]);
  u[1] = _mm_sub_epi32(v[2], v[6]);
  u[2] = _mm_add_epi32(v[3], v[4]);
  u[3] = _mm_sub_epi32(u[2], u[0]);
  u[4] = _mm_slli_epi32(v[5], 2);
  u[5] = _mm_sub_epi32(u[4], v[5]);
  u[6] = _mm_add_epi32(u[3], u[5]);

  v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  v[3] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);

  u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u[0], u[2]);
  in[1] = _mm_packs_epi32(u[1], u[3]);
  transpose_4x4(in);
}

#if CONFIG_EXT_TX
static void fidtx4_sse2(__m128i *in) {
  const __m128i k__zero_epi16 = _mm_set1_epi16((int16_t)0);
  const __m128i k__sqrt2_epi16 = _mm_set1_epi16((int16_t)Sqrt2);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  __m128i v0, v1, v2, v3;
  __m128i u0, u1, u2, u3;

  v0 = _mm_unpacklo_epi16(in[0], k__zero_epi16);
  v1 = _mm_unpacklo_epi16(in[1], k__zero_epi16);
  v2 = _mm_unpacklo_epi16(in[2], k__zero_epi16);
  v3 = _mm_unpacklo_epi16(in[3], k__zero_epi16);

  u0 = _mm_madd_epi16(v0, k__sqrt2_epi16);
  u1 = _mm_madd_epi16(v1, k__sqrt2_epi16);
  u2 = _mm_madd_epi16(v2, k__sqrt2_epi16);
  u3 = _mm_madd_epi16(v3, k__sqrt2_epi16);

  v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
  v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
  v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
  v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);

  u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
  u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
  u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
  u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u0, u2);
  in[1] = _mm_packs_epi32(u1, u3);
  transpose_4x4(in);
}
#endif  // CONFIG_EXT_TX

void av1_fht4x4_sse2(const int16_t *input, tran_low_t *output, int stride,
                     TxfmParam *txfm_param) {
  __m128i in[4];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  switch (tx_type) {
    case DCT_DCT: aom_fdct4x4_sse2(input, output, stride); break;
    case ADST_DCT:
      load_buffer_4x4(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fdct4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case DCT_ADST:
      load_buffer_4x4(input, in, stride, 0, 0);
      fdct4_sse2(in);
      fadst4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case ADST_ADST:
      load_buffer_4x4(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fadst4_sse2(in);
      write_buffer_4x4(output, in);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_4x4(input, in, stride, 1, 0);
      fadst4_sse2(in);
      fdct4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case DCT_FLIPADST:
      load_buffer_4x4(input, in, stride, 0, 1);
      fdct4_sse2(in);
      fadst4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_4x4(input, in, stride, 1, 1);
      fadst4_sse2(in);
      fadst4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case ADST_FLIPADST:
      load_buffer_4x4(input, in, stride, 0, 1);
      fadst4_sse2(in);
      fadst4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case FLIPADST_ADST:
      load_buffer_4x4(input, in, stride, 1, 0);
      fadst4_sse2(in);
      fadst4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case IDTX:
      load_buffer_4x4(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fidtx4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case V_DCT:
      load_buffer_4x4(input, in, stride, 0, 0);
      fdct4_sse2(in);
      fidtx4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case H_DCT:
      load_buffer_4x4(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fdct4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case V_ADST:
      load_buffer_4x4(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fidtx4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case H_ADST:
      load_buffer_4x4(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fadst4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case V_FLIPADST:
      load_buffer_4x4(input, in, stride, 1, 0);
      fadst4_sse2(in);
      fidtx4_sse2(in);
      write_buffer_4x4(output, in);
      break;
    case H_FLIPADST:
      load_buffer_4x4(input, in, stride, 0, 1);
      fidtx4_sse2(in);
      fadst4_sse2(in);
      write_buffer_4x4(output, in);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

// load 8x8 array
static INLINE void load_buffer_8x8(const int16_t *input, __m128i *in,
                                   int stride, int flipud, int fliplr) {
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

  in[0] = _mm_slli_epi16(in[0], 2);
  in[1] = _mm_slli_epi16(in[1], 2);
  in[2] = _mm_slli_epi16(in[2], 2);
  in[3] = _mm_slli_epi16(in[3], 2);
  in[4] = _mm_slli_epi16(in[4], 2);
  in[5] = _mm_slli_epi16(in[5], 2);
  in[6] = _mm_slli_epi16(in[6], 2);
  in[7] = _mm_slli_epi16(in[7], 2);
}

// right shift and rounding
static INLINE void right_shift_8x8(__m128i *res, const int bit) {
  __m128i sign0 = _mm_srai_epi16(res[0], 15);
  __m128i sign1 = _mm_srai_epi16(res[1], 15);
  __m128i sign2 = _mm_srai_epi16(res[2], 15);
  __m128i sign3 = _mm_srai_epi16(res[3], 15);
  __m128i sign4 = _mm_srai_epi16(res[4], 15);
  __m128i sign5 = _mm_srai_epi16(res[5], 15);
  __m128i sign6 = _mm_srai_epi16(res[6], 15);
  __m128i sign7 = _mm_srai_epi16(res[7], 15);

  if (bit == 2) {
    const __m128i const_rounding = _mm_set1_epi16(1);
    res[0] = _mm_adds_epi16(res[0], const_rounding);
    res[1] = _mm_adds_epi16(res[1], const_rounding);
    res[2] = _mm_adds_epi16(res[2], const_rounding);
    res[3] = _mm_adds_epi16(res[3], const_rounding);
    res[4] = _mm_adds_epi16(res[4], const_rounding);
    res[5] = _mm_adds_epi16(res[5], const_rounding);
    res[6] = _mm_adds_epi16(res[6], const_rounding);
    res[7] = _mm_adds_epi16(res[7], const_rounding);
  }

  res[0] = _mm_sub_epi16(res[0], sign0);
  res[1] = _mm_sub_epi16(res[1], sign1);
  res[2] = _mm_sub_epi16(res[2], sign2);
  res[3] = _mm_sub_epi16(res[3], sign3);
  res[4] = _mm_sub_epi16(res[4], sign4);
  res[5] = _mm_sub_epi16(res[5], sign5);
  res[6] = _mm_sub_epi16(res[6], sign6);
  res[7] = _mm_sub_epi16(res[7], sign7);

  if (bit == 1) {
    res[0] = _mm_srai_epi16(res[0], 1);
    res[1] = _mm_srai_epi16(res[1], 1);
    res[2] = _mm_srai_epi16(res[2], 1);
    res[3] = _mm_srai_epi16(res[3], 1);
    res[4] = _mm_srai_epi16(res[4], 1);
    res[5] = _mm_srai_epi16(res[5], 1);
    res[6] = _mm_srai_epi16(res[6], 1);
    res[7] = _mm_srai_epi16(res[7], 1);
  } else {
    res[0] = _mm_srai_epi16(res[0], 2);
    res[1] = _mm_srai_epi16(res[1], 2);
    res[2] = _mm_srai_epi16(res[2], 2);
    res[3] = _mm_srai_epi16(res[3], 2);
    res[4] = _mm_srai_epi16(res[4], 2);
    res[5] = _mm_srai_epi16(res[5], 2);
    res[6] = _mm_srai_epi16(res[6], 2);
    res[7] = _mm_srai_epi16(res[7], 2);
  }
}

// write 8x8 array
static INLINE void write_buffer_8x8(tran_low_t *output, __m128i *res,
                                    int stride) {
  store_output(&res[0], (output + 0 * stride));
  store_output(&res[1], (output + 1 * stride));
  store_output(&res[2], (output + 2 * stride));
  store_output(&res[3], (output + 3 * stride));
  store_output(&res[4], (output + 4 * stride));
  store_output(&res[5], (output + 5 * stride));
  store_output(&res[6], (output + 6 * stride));
  store_output(&res[7], (output + 7 * stride));
}

// perform in-place transpose
static INLINE void array_transpose_8x8(__m128i *in, __m128i *res) {
  const __m128i tr0_0 = _mm_unpacklo_epi16(in[0], in[1]);
  const __m128i tr0_1 = _mm_unpacklo_epi16(in[2], in[3]);
  const __m128i tr0_2 = _mm_unpackhi_epi16(in[0], in[1]);
  const __m128i tr0_3 = _mm_unpackhi_epi16(in[2], in[3]);
  const __m128i tr0_4 = _mm_unpacklo_epi16(in[4], in[5]);
  const __m128i tr0_5 = _mm_unpacklo_epi16(in[6], in[7]);
  const __m128i tr0_6 = _mm_unpackhi_epi16(in[4], in[5]);
  const __m128i tr0_7 = _mm_unpackhi_epi16(in[6], in[7]);
  // 00 10 01 11 02 12 03 13
  // 20 30 21 31 22 32 23 33
  // 04 14 05 15 06 16 07 17
  // 24 34 25 35 26 36 27 37
  // 40 50 41 51 42 52 43 53
  // 60 70 61 71 62 72 63 73
  // 44 54 45 55 46 56 47 57
  // 64 74 65 75 66 76 67 77
  const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
  const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_4, tr0_5);
  const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
  const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_4, tr0_5);
  const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_2, tr0_3);
  const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
  const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_2, tr0_3);
  const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
  // 00 10 20 30 01 11 21 31
  // 40 50 60 70 41 51 61 71
  // 02 12 22 32 03 13 23 33
  // 42 52 62 72 43 53 63 73
  // 04 14 24 34 05 15 25 35
  // 44 54 64 74 45 55 65 75
  // 06 16 26 36 07 17 27 37
  // 46 56 66 76 47 57 67 77
  res[0] = _mm_unpacklo_epi64(tr1_0, tr1_1);
  res[1] = _mm_unpackhi_epi64(tr1_0, tr1_1);
  res[2] = _mm_unpacklo_epi64(tr1_2, tr1_3);
  res[3] = _mm_unpackhi_epi64(tr1_2, tr1_3);
  res[4] = _mm_unpacklo_epi64(tr1_4, tr1_5);
  res[5] = _mm_unpackhi_epi64(tr1_4, tr1_5);
  res[6] = _mm_unpacklo_epi64(tr1_6, tr1_7);
  res[7] = _mm_unpackhi_epi64(tr1_6, tr1_7);
  // 00 10 20 30 40 50 60 70
  // 01 11 21 31 41 51 61 71
  // 02 12 22 32 42 52 62 72
  // 03 13 23 33 43 53 63 73
  // 04 14 24 34 44 54 64 74
  // 05 15 25 35 45 55 65 75
  // 06 16 26 36 46 56 66 76
  // 07 17 27 37 47 57 67 77
}

static void fdct8_sse2(__m128i *in) {
  // constants
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
  const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;
  __m128i s0, s1, s2, s3, s4, s5, s6, s7;

  // stage 1
  s0 = _mm_add_epi16(in[0], in[7]);
  s1 = _mm_add_epi16(in[1], in[6]);
  s2 = _mm_add_epi16(in[2], in[5]);
  s3 = _mm_add_epi16(in[3], in[4]);
  s4 = _mm_sub_epi16(in[3], in[4]);
  s5 = _mm_sub_epi16(in[2], in[5]);
  s6 = _mm_sub_epi16(in[1], in[6]);
  s7 = _mm_sub_epi16(in[0], in[7]);

  u0 = _mm_add_epi16(s0, s3);
  u1 = _mm_add_epi16(s1, s2);
  u2 = _mm_sub_epi16(s1, s2);
  u3 = _mm_sub_epi16(s0, s3);
  // interleave and perform butterfly multiplication/addition
  v0 = _mm_unpacklo_epi16(u0, u1);
  v1 = _mm_unpackhi_epi16(u0, u1);
  v2 = _mm_unpacklo_epi16(u2, u3);
  v3 = _mm_unpackhi_epi16(u2, u3);

  u0 = _mm_madd_epi16(v0, k__cospi_p16_p16);
  u1 = _mm_madd_epi16(v1, k__cospi_p16_p16);
  u2 = _mm_madd_epi16(v0, k__cospi_p16_m16);
  u3 = _mm_madd_epi16(v1, k__cospi_p16_m16);
  u4 = _mm_madd_epi16(v2, k__cospi_p24_p08);
  u5 = _mm_madd_epi16(v3, k__cospi_p24_p08);
  u6 = _mm_madd_epi16(v2, k__cospi_m08_p24);
  u7 = _mm_madd_epi16(v3, k__cospi_m08_p24);

  // shift and rounding
  v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
  v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
  v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
  v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
  v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
  v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
  v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
  v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);

  u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
  u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
  u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
  u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
  u4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
  u5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
  u6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
  u7 = _mm_srai_epi32(v7, DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u0, u1);
  in[2] = _mm_packs_epi32(u4, u5);
  in[4] = _mm_packs_epi32(u2, u3);
  in[6] = _mm_packs_epi32(u6, u7);

  // stage 2
  // interleave and perform butterfly multiplication/addition
  u0 = _mm_unpacklo_epi16(s6, s5);
  u1 = _mm_unpackhi_epi16(s6, s5);
  v0 = _mm_madd_epi16(u0, k__cospi_p16_m16);
  v1 = _mm_madd_epi16(u1, k__cospi_p16_m16);
  v2 = _mm_madd_epi16(u0, k__cospi_p16_p16);
  v3 = _mm_madd_epi16(u1, k__cospi_p16_p16);

  // shift and rounding
  u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
  u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
  u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
  u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);

  v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
  v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
  v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
  v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);

  u0 = _mm_packs_epi32(v0, v1);
  u1 = _mm_packs_epi32(v2, v3);

  // stage 3
  s0 = _mm_add_epi16(s4, u0);
  s1 = _mm_sub_epi16(s4, u0);
  s2 = _mm_sub_epi16(s7, u1);
  s3 = _mm_add_epi16(s7, u1);

  // stage 4
  u0 = _mm_unpacklo_epi16(s0, s3);
  u1 = _mm_unpackhi_epi16(s0, s3);
  u2 = _mm_unpacklo_epi16(s1, s2);
  u3 = _mm_unpackhi_epi16(s1, s2);

  v0 = _mm_madd_epi16(u0, k__cospi_p28_p04);
  v1 = _mm_madd_epi16(u1, k__cospi_p28_p04);
  v2 = _mm_madd_epi16(u2, k__cospi_p12_p20);
  v3 = _mm_madd_epi16(u3, k__cospi_p12_p20);
  v4 = _mm_madd_epi16(u2, k__cospi_m20_p12);
  v5 = _mm_madd_epi16(u3, k__cospi_m20_p12);
  v6 = _mm_madd_epi16(u0, k__cospi_m04_p28);
  v7 = _mm_madd_epi16(u1, k__cospi_m04_p28);

  // shift and rounding
  u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
  u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
  u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
  u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);
  u4 = _mm_add_epi32(v4, k__DCT_CONST_ROUNDING);
  u5 = _mm_add_epi32(v5, k__DCT_CONST_ROUNDING);
  u6 = _mm_add_epi32(v6, k__DCT_CONST_ROUNDING);
  u7 = _mm_add_epi32(v7, k__DCT_CONST_ROUNDING);

  v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
  v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
  v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
  v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);
  v4 = _mm_srai_epi32(u4, DCT_CONST_BITS);
  v5 = _mm_srai_epi32(u5, DCT_CONST_BITS);
  v6 = _mm_srai_epi32(u6, DCT_CONST_BITS);
  v7 = _mm_srai_epi32(u7, DCT_CONST_BITS);

  in[1] = _mm_packs_epi32(v0, v1);
  in[3] = _mm_packs_epi32(v4, v5);
  in[5] = _mm_packs_epi32(v2, v3);
  in[7] = _mm_packs_epi32(v6, v7);

  // transpose
  array_transpose_8x8(in, in);
}

static void fadst8_sse2(__m128i *in) {
  // Constants
  const __m128i k__cospi_p02_p30 = pair_set_epi16(cospi_2_64, cospi_30_64);
  const __m128i k__cospi_p30_m02 = pair_set_epi16(cospi_30_64, -cospi_2_64);
  const __m128i k__cospi_p10_p22 = pair_set_epi16(cospi_10_64, cospi_22_64);
  const __m128i k__cospi_p22_m10 = pair_set_epi16(cospi_22_64, -cospi_10_64);
  const __m128i k__cospi_p18_p14 = pair_set_epi16(cospi_18_64, cospi_14_64);
  const __m128i k__cospi_p14_m18 = pair_set_epi16(cospi_14_64, -cospi_18_64);
  const __m128i k__cospi_p26_p06 = pair_set_epi16(cospi_26_64, cospi_6_64);
  const __m128i k__cospi_p06_m26 = pair_set_epi16(cospi_6_64, -cospi_26_64);
  const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_m24_p08 = pair_set_epi16(-cospi_24_64, cospi_8_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
  const __m128i k__const_0 = _mm_set1_epi16(0);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  __m128i u0, u1, u2, u3, u4, u5, u6, u7, u8, u9, u10, u11, u12, u13, u14, u15;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;
  __m128i w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15;
  __m128i s0, s1, s2, s3, s4, s5, s6, s7;
  __m128i in0, in1, in2, in3, in4, in5, in6, in7;

  // properly aligned for butterfly input
  in0 = in[7];
  in1 = in[0];
  in2 = in[5];
  in3 = in[2];
  in4 = in[3];
  in5 = in[4];
  in6 = in[1];
  in7 = in[6];

  // column transformation
  // stage 1
  // interleave and multiply/add into 32-bit integer
  s0 = _mm_unpacklo_epi16(in0, in1);
  s1 = _mm_unpackhi_epi16(in0, in1);
  s2 = _mm_unpacklo_epi16(in2, in3);
  s3 = _mm_unpackhi_epi16(in2, in3);
  s4 = _mm_unpacklo_epi16(in4, in5);
  s5 = _mm_unpackhi_epi16(in4, in5);
  s6 = _mm_unpacklo_epi16(in6, in7);
  s7 = _mm_unpackhi_epi16(in6, in7);

  u0 = _mm_madd_epi16(s0, k__cospi_p02_p30);
  u1 = _mm_madd_epi16(s1, k__cospi_p02_p30);
  u2 = _mm_madd_epi16(s0, k__cospi_p30_m02);
  u3 = _mm_madd_epi16(s1, k__cospi_p30_m02);
  u4 = _mm_madd_epi16(s2, k__cospi_p10_p22);
  u5 = _mm_madd_epi16(s3, k__cospi_p10_p22);
  u6 = _mm_madd_epi16(s2, k__cospi_p22_m10);
  u7 = _mm_madd_epi16(s3, k__cospi_p22_m10);
  u8 = _mm_madd_epi16(s4, k__cospi_p18_p14);
  u9 = _mm_madd_epi16(s5, k__cospi_p18_p14);
  u10 = _mm_madd_epi16(s4, k__cospi_p14_m18);
  u11 = _mm_madd_epi16(s5, k__cospi_p14_m18);
  u12 = _mm_madd_epi16(s6, k__cospi_p26_p06);
  u13 = _mm_madd_epi16(s7, k__cospi_p26_p06);
  u14 = _mm_madd_epi16(s6, k__cospi_p06_m26);
  u15 = _mm_madd_epi16(s7, k__cospi_p06_m26);

  // addition
  w0 = _mm_add_epi32(u0, u8);
  w1 = _mm_add_epi32(u1, u9);
  w2 = _mm_add_epi32(u2, u10);
  w3 = _mm_add_epi32(u3, u11);
  w4 = _mm_add_epi32(u4, u12);
  w5 = _mm_add_epi32(u5, u13);
  w6 = _mm_add_epi32(u6, u14);
  w7 = _mm_add_epi32(u7, u15);
  w8 = _mm_sub_epi32(u0, u8);
  w9 = _mm_sub_epi32(u1, u9);
  w10 = _mm_sub_epi32(u2, u10);
  w11 = _mm_sub_epi32(u3, u11);
  w12 = _mm_sub_epi32(u4, u12);
  w13 = _mm_sub_epi32(u5, u13);
  w14 = _mm_sub_epi32(u6, u14);
  w15 = _mm_sub_epi32(u7, u15);

  // shift and rounding
  v8 = _mm_add_epi32(w8, k__DCT_CONST_ROUNDING);
  v9 = _mm_add_epi32(w9, k__DCT_CONST_ROUNDING);
  v10 = _mm_add_epi32(w10, k__DCT_CONST_ROUNDING);
  v11 = _mm_add_epi32(w11, k__DCT_CONST_ROUNDING);
  v12 = _mm_add_epi32(w12, k__DCT_CONST_ROUNDING);
  v13 = _mm_add_epi32(w13, k__DCT_CONST_ROUNDING);
  v14 = _mm_add_epi32(w14, k__DCT_CONST_ROUNDING);
  v15 = _mm_add_epi32(w15, k__DCT_CONST_ROUNDING);

  u8 = _mm_srai_epi32(v8, DCT_CONST_BITS);
  u9 = _mm_srai_epi32(v9, DCT_CONST_BITS);
  u10 = _mm_srai_epi32(v10, DCT_CONST_BITS);
  u11 = _mm_srai_epi32(v11, DCT_CONST_BITS);
  u12 = _mm_srai_epi32(v12, DCT_CONST_BITS);
  u13 = _mm_srai_epi32(v13, DCT_CONST_BITS);
  u14 = _mm_srai_epi32(v14, DCT_CONST_BITS);
  u15 = _mm_srai_epi32(v15, DCT_CONST_BITS);

  // back to 16-bit and pack 8 integers into __m128i
  v0 = _mm_add_epi32(w0, w4);
  v1 = _mm_add_epi32(w1, w5);
  v2 = _mm_add_epi32(w2, w6);
  v3 = _mm_add_epi32(w3, w7);
  v4 = _mm_sub_epi32(w0, w4);
  v5 = _mm_sub_epi32(w1, w5);
  v6 = _mm_sub_epi32(w2, w6);
  v7 = _mm_sub_epi32(w3, w7);

  w0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
  w1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
  w2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
  w3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);
  w4 = _mm_add_epi32(v4, k__DCT_CONST_ROUNDING);
  w5 = _mm_add_epi32(v5, k__DCT_CONST_ROUNDING);
  w6 = _mm_add_epi32(v6, k__DCT_CONST_ROUNDING);
  w7 = _mm_add_epi32(v7, k__DCT_CONST_ROUNDING);

  v0 = _mm_srai_epi32(w0, DCT_CONST_BITS);
  v1 = _mm_srai_epi32(w1, DCT_CONST_BITS);
  v2 = _mm_srai_epi32(w2, DCT_CONST_BITS);
  v3 = _mm_srai_epi32(w3, DCT_CONST_BITS);
  v4 = _mm_srai_epi32(w4, DCT_CONST_BITS);
  v5 = _mm_srai_epi32(w5, DCT_CONST_BITS);
  v6 = _mm_srai_epi32(w6, DCT_CONST_BITS);
  v7 = _mm_srai_epi32(w7, DCT_CONST_BITS);

  in[4] = _mm_packs_epi32(u8, u9);
  in[5] = _mm_packs_epi32(u10, u11);
  in[6] = _mm_packs_epi32(u12, u13);
  in[7] = _mm_packs_epi32(u14, u15);

  // stage 2
  s0 = _mm_packs_epi32(v0, v1);
  s1 = _mm_packs_epi32(v2, v3);
  s2 = _mm_packs_epi32(v4, v5);
  s3 = _mm_packs_epi32(v6, v7);

  u0 = _mm_unpacklo_epi16(in[4], in[5]);
  u1 = _mm_unpackhi_epi16(in[4], in[5]);
  u2 = _mm_unpacklo_epi16(in[6], in[7]);
  u3 = _mm_unpackhi_epi16(in[6], in[7]);

  v0 = _mm_madd_epi16(u0, k__cospi_p08_p24);
  v1 = _mm_madd_epi16(u1, k__cospi_p08_p24);
  v2 = _mm_madd_epi16(u0, k__cospi_p24_m08);
  v3 = _mm_madd_epi16(u1, k__cospi_p24_m08);
  v4 = _mm_madd_epi16(u2, k__cospi_m24_p08);
  v5 = _mm_madd_epi16(u3, k__cospi_m24_p08);
  v6 = _mm_madd_epi16(u2, k__cospi_p08_p24);
  v7 = _mm_madd_epi16(u3, k__cospi_p08_p24);

  w0 = _mm_add_epi32(v0, v4);
  w1 = _mm_add_epi32(v1, v5);
  w2 = _mm_add_epi32(v2, v6);
  w3 = _mm_add_epi32(v3, v7);
  w4 = _mm_sub_epi32(v0, v4);
  w5 = _mm_sub_epi32(v1, v5);
  w6 = _mm_sub_epi32(v2, v6);
  w7 = _mm_sub_epi32(v3, v7);

  v0 = _mm_add_epi32(w0, k__DCT_CONST_ROUNDING);
  v1 = _mm_add_epi32(w1, k__DCT_CONST_ROUNDING);
  v2 = _mm_add_epi32(w2, k__DCT_CONST_ROUNDING);
  v3 = _mm_add_epi32(w3, k__DCT_CONST_ROUNDING);
  v4 = _mm_add_epi32(w4, k__DCT_CONST_ROUNDING);
  v5 = _mm_add_epi32(w5, k__DCT_CONST_ROUNDING);
  v6 = _mm_add_epi32(w6, k__DCT_CONST_ROUNDING);
  v7 = _mm_add_epi32(w7, k__DCT_CONST_ROUNDING);

  u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
  u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
  u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
  u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
  u4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
  u5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
  u6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
  u7 = _mm_srai_epi32(v7, DCT_CONST_BITS);

  // back to 16-bit intergers
  s4 = _mm_packs_epi32(u0, u1);
  s5 = _mm_packs_epi32(u2, u3);
  s6 = _mm_packs_epi32(u4, u5);
  s7 = _mm_packs_epi32(u6, u7);

  // stage 3
  u0 = _mm_unpacklo_epi16(s2, s3);
  u1 = _mm_unpackhi_epi16(s2, s3);
  u2 = _mm_unpacklo_epi16(s6, s7);
  u3 = _mm_unpackhi_epi16(s6, s7);

  v0 = _mm_madd_epi16(u0, k__cospi_p16_p16);
  v1 = _mm_madd_epi16(u1, k__cospi_p16_p16);
  v2 = _mm_madd_epi16(u0, k__cospi_p16_m16);
  v3 = _mm_madd_epi16(u1, k__cospi_p16_m16);
  v4 = _mm_madd_epi16(u2, k__cospi_p16_p16);
  v5 = _mm_madd_epi16(u3, k__cospi_p16_p16);
  v6 = _mm_madd_epi16(u2, k__cospi_p16_m16);
  v7 = _mm_madd_epi16(u3, k__cospi_p16_m16);

  u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
  u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
  u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
  u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);
  u4 = _mm_add_epi32(v4, k__DCT_CONST_ROUNDING);
  u5 = _mm_add_epi32(v5, k__DCT_CONST_ROUNDING);
  u6 = _mm_add_epi32(v6, k__DCT_CONST_ROUNDING);
  u7 = _mm_add_epi32(v7, k__DCT_CONST_ROUNDING);

  v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
  v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
  v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
  v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);
  v4 = _mm_srai_epi32(u4, DCT_CONST_BITS);
  v5 = _mm_srai_epi32(u5, DCT_CONST_BITS);
  v6 = _mm_srai_epi32(u6, DCT_CONST_BITS);
  v7 = _mm_srai_epi32(u7, DCT_CONST_BITS);

  s2 = _mm_packs_epi32(v0, v1);
  s3 = _mm_packs_epi32(v2, v3);
  s6 = _mm_packs_epi32(v4, v5);
  s7 = _mm_packs_epi32(v6, v7);

  // FIXME(jingning): do subtract using bit inversion?
  in[0] = s0;
  in[1] = _mm_sub_epi16(k__const_0, s4);
  in[2] = s6;
  in[3] = _mm_sub_epi16(k__const_0, s2);
  in[4] = s3;
  in[5] = _mm_sub_epi16(k__const_0, s7);
  in[6] = s5;
  in[7] = _mm_sub_epi16(k__const_0, s1);

  // transpose
  array_transpose_8x8(in, in);
}

#if CONFIG_EXT_TX
static void fidtx8_sse2(__m128i *in) {
  in[0] = _mm_slli_epi16(in[0], 1);
  in[1] = _mm_slli_epi16(in[1], 1);
  in[2] = _mm_slli_epi16(in[2], 1);
  in[3] = _mm_slli_epi16(in[3], 1);
  in[4] = _mm_slli_epi16(in[4], 1);
  in[5] = _mm_slli_epi16(in[5], 1);
  in[6] = _mm_slli_epi16(in[6], 1);
  in[7] = _mm_slli_epi16(in[7], 1);

  array_transpose_8x8(in, in);
}
#endif  // CONFIG_EXT_TX

void av1_fht8x8_sse2(const int16_t *input, tran_low_t *output, int stride,
                     TxfmParam *txfm_param) {
  __m128i in[8];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  switch (tx_type) {
    case DCT_DCT: aom_fdct8x8_sse2(input, output, stride); break;
    case ADST_DCT:
      load_buffer_8x8(input, in, stride, 0, 0);
      fadst8_sse2(in);
      fdct8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case DCT_ADST:
      load_buffer_8x8(input, in, stride, 0, 0);
      fdct8_sse2(in);
      fadst8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case ADST_ADST:
      load_buffer_8x8(input, in, stride, 0, 0);
      fadst8_sse2(in);
      fadst8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_8x8(input, in, stride, 1, 0);
      fadst8_sse2(in);
      fdct8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case DCT_FLIPADST:
      load_buffer_8x8(input, in, stride, 0, 1);
      fdct8_sse2(in);
      fadst8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_8x8(input, in, stride, 1, 1);
      fadst8_sse2(in);
      fadst8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case ADST_FLIPADST:
      load_buffer_8x8(input, in, stride, 0, 1);
      fadst8_sse2(in);
      fadst8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case FLIPADST_ADST:
      load_buffer_8x8(input, in, stride, 1, 0);
      fadst8_sse2(in);
      fadst8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case IDTX:
      load_buffer_8x8(input, in, stride, 0, 0);
      fidtx8_sse2(in);
      fidtx8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case V_DCT:
      load_buffer_8x8(input, in, stride, 0, 0);
      fdct8_sse2(in);
      fidtx8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case H_DCT:
      load_buffer_8x8(input, in, stride, 0, 0);
      fidtx8_sse2(in);
      fdct8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case V_ADST:
      load_buffer_8x8(input, in, stride, 0, 0);
      fadst8_sse2(in);
      fidtx8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case H_ADST:
      load_buffer_8x8(input, in, stride, 0, 0);
      fidtx8_sse2(in);
      fadst8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case V_FLIPADST:
      load_buffer_8x8(input, in, stride, 1, 0);
      fadst8_sse2(in);
      fidtx8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
    case H_FLIPADST:
      load_buffer_8x8(input, in, stride, 0, 1);
      fidtx8_sse2(in);
      fadst8_sse2(in);
      right_shift_8x8(in, 1);
      write_buffer_8x8(output, in, 8);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static INLINE void load_buffer_16x16(const int16_t *input, __m128i *in0,
                                     __m128i *in1, int stride, int flipud,
                                     int fliplr) {
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
  load_buffer_8x8(topL, in0, stride, flipud, fliplr);
  load_buffer_8x8(botL, in0 + 8, stride, flipud, fliplr);

  // load second 8 columns
  load_buffer_8x8(topR, in1, stride, flipud, fliplr);
  load_buffer_8x8(botR, in1 + 8, stride, flipud, fliplr);
}

static INLINE void write_buffer_16x16(tran_low_t *output, __m128i *in0,
                                      __m128i *in1, int stride) {
  // write first 8 columns
  write_buffer_8x8(output, in0, stride);
  write_buffer_8x8(output + 8 * stride, in0 + 8, stride);
  // write second 8 columns
  output += 8;
  write_buffer_8x8(output, in1, stride);
  write_buffer_8x8(output + 8 * stride, in1 + 8, stride);
}

static INLINE void array_transpose_16x16(__m128i *res0, __m128i *res1) {
  __m128i tbuf[8];
  array_transpose_8x8(res0, res0);
  array_transpose_8x8(res1, tbuf);
  array_transpose_8x8(res0 + 8, res1);
  array_transpose_8x8(res1 + 8, res1 + 8);

  res0[8] = tbuf[0];
  res0[9] = tbuf[1];
  res0[10] = tbuf[2];
  res0[11] = tbuf[3];
  res0[12] = tbuf[4];
  res0[13] = tbuf[5];
  res0[14] = tbuf[6];
  res0[15] = tbuf[7];
}

static INLINE void right_shift_16x16(__m128i *res0, __m128i *res1) {
  // perform rounding operations
  right_shift_8x8(res0, 2);
  right_shift_8x8(res0 + 8, 2);
  right_shift_8x8(res1, 2);
  right_shift_8x8(res1 + 8, 2);
}

static void fdct16_8col(__m128i *in) {
  // perform 16x16 1-D DCT for 8 columns
  __m128i i[8], s[8], p[8], t[8], u[16], v[16];
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
  const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
  const __m128i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
  const __m128i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
  const __m128i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
  const __m128i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
  const __m128i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
  const __m128i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
  const __m128i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  // stage 1
  i[0] = _mm_add_epi16(in[0], in[15]);
  i[1] = _mm_add_epi16(in[1], in[14]);
  i[2] = _mm_add_epi16(in[2], in[13]);
  i[3] = _mm_add_epi16(in[3], in[12]);
  i[4] = _mm_add_epi16(in[4], in[11]);
  i[5] = _mm_add_epi16(in[5], in[10]);
  i[6] = _mm_add_epi16(in[6], in[9]);
  i[7] = _mm_add_epi16(in[7], in[8]);

  s[0] = _mm_sub_epi16(in[7], in[8]);
  s[1] = _mm_sub_epi16(in[6], in[9]);
  s[2] = _mm_sub_epi16(in[5], in[10]);
  s[3] = _mm_sub_epi16(in[4], in[11]);
  s[4] = _mm_sub_epi16(in[3], in[12]);
  s[5] = _mm_sub_epi16(in[2], in[13]);
  s[6] = _mm_sub_epi16(in[1], in[14]);
  s[7] = _mm_sub_epi16(in[0], in[15]);

  p[0] = _mm_add_epi16(i[0], i[7]);
  p[1] = _mm_add_epi16(i[1], i[6]);
  p[2] = _mm_add_epi16(i[2], i[5]);
  p[3] = _mm_add_epi16(i[3], i[4]);
  p[4] = _mm_sub_epi16(i[3], i[4]);
  p[5] = _mm_sub_epi16(i[2], i[5]);
  p[6] = _mm_sub_epi16(i[1], i[6]);
  p[7] = _mm_sub_epi16(i[0], i[7]);

  u[0] = _mm_add_epi16(p[0], p[3]);
  u[1] = _mm_add_epi16(p[1], p[2]);
  u[2] = _mm_sub_epi16(p[1], p[2]);
  u[3] = _mm_sub_epi16(p[0], p[3]);

  v[0] = _mm_unpacklo_epi16(u[0], u[1]);
  v[1] = _mm_unpackhi_epi16(u[0], u[1]);
  v[2] = _mm_unpacklo_epi16(u[2], u[3]);
  v[3] = _mm_unpackhi_epi16(u[2], u[3]);

  u[0] = _mm_madd_epi16(v[0], k__cospi_p16_p16);
  u[1] = _mm_madd_epi16(v[1], k__cospi_p16_p16);
  u[2] = _mm_madd_epi16(v[0], k__cospi_p16_m16);
  u[3] = _mm_madd_epi16(v[1], k__cospi_p16_m16);
  u[4] = _mm_madd_epi16(v[2], k__cospi_p24_p08);
  u[5] = _mm_madd_epi16(v[3], k__cospi_p24_p08);
  u[6] = _mm_madd_epi16(v[2], k__cospi_m08_p24);
  u[7] = _mm_madd_epi16(v[3], k__cospi_m08_p24);

  v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
  v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
  v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
  v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
  v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);

  u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
  u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
  u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
  u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
  u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u[0], u[1]);
  in[4] = _mm_packs_epi32(u[4], u[5]);
  in[8] = _mm_packs_epi32(u[2], u[3]);
  in[12] = _mm_packs_epi32(u[6], u[7]);

  u[0] = _mm_unpacklo_epi16(p[5], p[6]);
  u[1] = _mm_unpackhi_epi16(p[5], p[6]);
  v[0] = _mm_madd_epi16(u[0], k__cospi_m16_p16);
  v[1] = _mm_madd_epi16(u[1], k__cospi_m16_p16);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p16_p16);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p16_p16);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);

  u[0] = _mm_packs_epi32(v[0], v[1]);
  u[1] = _mm_packs_epi32(v[2], v[3]);

  t[0] = _mm_add_epi16(p[4], u[0]);
  t[1] = _mm_sub_epi16(p[4], u[0]);
  t[2] = _mm_sub_epi16(p[7], u[1]);
  t[3] = _mm_add_epi16(p[7], u[1]);

  u[0] = _mm_unpacklo_epi16(t[0], t[3]);
  u[1] = _mm_unpackhi_epi16(t[0], t[3]);
  u[2] = _mm_unpacklo_epi16(t[1], t[2]);
  u[3] = _mm_unpackhi_epi16(t[1], t[2]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p28_p04);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p28_p04);
  v[2] = _mm_madd_epi16(u[2], k__cospi_p12_p20);
  v[3] = _mm_madd_epi16(u[3], k__cospi_p12_p20);
  v[4] = _mm_madd_epi16(u[2], k__cospi_m20_p12);
  v[5] = _mm_madd_epi16(u[3], k__cospi_m20_p12);
  v[6] = _mm_madd_epi16(u[0], k__cospi_m04_p28);
  v[7] = _mm_madd_epi16(u[1], k__cospi_m04_p28);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

  in[2] = _mm_packs_epi32(v[0], v[1]);
  in[6] = _mm_packs_epi32(v[4], v[5]);
  in[10] = _mm_packs_epi32(v[2], v[3]);
  in[14] = _mm_packs_epi32(v[6], v[7]);

  // stage 2
  u[0] = _mm_unpacklo_epi16(s[2], s[5]);
  u[1] = _mm_unpackhi_epi16(s[2], s[5]);
  u[2] = _mm_unpacklo_epi16(s[3], s[4]);
  u[3] = _mm_unpackhi_epi16(s[3], s[4]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_m16_p16);
  v[1] = _mm_madd_epi16(u[1], k__cospi_m16_p16);
  v[2] = _mm_madd_epi16(u[2], k__cospi_m16_p16);
  v[3] = _mm_madd_epi16(u[3], k__cospi_m16_p16);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p16_p16);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p16_p16);
  v[6] = _mm_madd_epi16(u[0], k__cospi_p16_p16);
  v[7] = _mm_madd_epi16(u[1], k__cospi_p16_p16);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

  t[2] = _mm_packs_epi32(v[0], v[1]);
  t[3] = _mm_packs_epi32(v[2], v[3]);
  t[4] = _mm_packs_epi32(v[4], v[5]);
  t[5] = _mm_packs_epi32(v[6], v[7]);

  // stage 3
  p[0] = _mm_add_epi16(s[0], t[3]);
  p[1] = _mm_add_epi16(s[1], t[2]);
  p[2] = _mm_sub_epi16(s[1], t[2]);
  p[3] = _mm_sub_epi16(s[0], t[3]);
  p[4] = _mm_sub_epi16(s[7], t[4]);
  p[5] = _mm_sub_epi16(s[6], t[5]);
  p[6] = _mm_add_epi16(s[6], t[5]);
  p[7] = _mm_add_epi16(s[7], t[4]);

  // stage 4
  u[0] = _mm_unpacklo_epi16(p[1], p[6]);
  u[1] = _mm_unpackhi_epi16(p[1], p[6]);
  u[2] = _mm_unpacklo_epi16(p[2], p[5]);
  u[3] = _mm_unpackhi_epi16(p[2], p[5]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_m08_p24);
  v[1] = _mm_madd_epi16(u[1], k__cospi_m08_p24);
  v[2] = _mm_madd_epi16(u[2], k__cospi_m24_m08);
  v[3] = _mm_madd_epi16(u[3], k__cospi_m24_m08);
  v[4] = _mm_madd_epi16(u[2], k__cospi_m08_p24);
  v[5] = _mm_madd_epi16(u[3], k__cospi_m08_p24);
  v[6] = _mm_madd_epi16(u[0], k__cospi_p24_p08);
  v[7] = _mm_madd_epi16(u[1], k__cospi_p24_p08);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

  t[1] = _mm_packs_epi32(v[0], v[1]);
  t[2] = _mm_packs_epi32(v[2], v[3]);
  t[5] = _mm_packs_epi32(v[4], v[5]);
  t[6] = _mm_packs_epi32(v[6], v[7]);

  // stage 5
  s[0] = _mm_add_epi16(p[0], t[1]);
  s[1] = _mm_sub_epi16(p[0], t[1]);
  s[2] = _mm_sub_epi16(p[3], t[2]);
  s[3] = _mm_add_epi16(p[3], t[2]);
  s[4] = _mm_add_epi16(p[4], t[5]);
  s[5] = _mm_sub_epi16(p[4], t[5]);
  s[6] = _mm_sub_epi16(p[7], t[6]);
  s[7] = _mm_add_epi16(p[7], t[6]);

  // stage 6
  u[0] = _mm_unpacklo_epi16(s[0], s[7]);
  u[1] = _mm_unpackhi_epi16(s[0], s[7]);
  u[2] = _mm_unpacklo_epi16(s[1], s[6]);
  u[3] = _mm_unpackhi_epi16(s[1], s[6]);
  u[4] = _mm_unpacklo_epi16(s[2], s[5]);
  u[5] = _mm_unpackhi_epi16(s[2], s[5]);
  u[6] = _mm_unpacklo_epi16(s[3], s[4]);
  u[7] = _mm_unpackhi_epi16(s[3], s[4]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p30_p02);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p30_p02);
  v[2] = _mm_madd_epi16(u[2], k__cospi_p14_p18);
  v[3] = _mm_madd_epi16(u[3], k__cospi_p14_p18);
  v[4] = _mm_madd_epi16(u[4], k__cospi_p22_p10);
  v[5] = _mm_madd_epi16(u[5], k__cospi_p22_p10);
  v[6] = _mm_madd_epi16(u[6], k__cospi_p06_p26);
  v[7] = _mm_madd_epi16(u[7], k__cospi_p06_p26);
  v[8] = _mm_madd_epi16(u[6], k__cospi_m26_p06);
  v[9] = _mm_madd_epi16(u[7], k__cospi_m26_p06);
  v[10] = _mm_madd_epi16(u[4], k__cospi_m10_p22);
  v[11] = _mm_madd_epi16(u[5], k__cospi_m10_p22);
  v[12] = _mm_madd_epi16(u[2], k__cospi_m18_p14);
  v[13] = _mm_madd_epi16(u[3], k__cospi_m18_p14);
  v[14] = _mm_madd_epi16(u[0], k__cospi_m02_p30);
  v[15] = _mm_madd_epi16(u[1], k__cospi_m02_p30);

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

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
  v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
  v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
  v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
  v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
  v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
  v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
  v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
  v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

  in[1] = _mm_packs_epi32(v[0], v[1]);
  in[9] = _mm_packs_epi32(v[2], v[3]);
  in[5] = _mm_packs_epi32(v[4], v[5]);
  in[13] = _mm_packs_epi32(v[6], v[7]);
  in[3] = _mm_packs_epi32(v[8], v[9]);
  in[11] = _mm_packs_epi32(v[10], v[11]);
  in[7] = _mm_packs_epi32(v[12], v[13]);
  in[15] = _mm_packs_epi32(v[14], v[15]);
}

static void fadst16_8col(__m128i *in) {
  // perform 16x16 1-D ADST for 8 columns
  __m128i s[16], x[16], u[32], v[32];
  const __m128i k__cospi_p01_p31 = pair_set_epi16(cospi_1_64, cospi_31_64);
  const __m128i k__cospi_p31_m01 = pair_set_epi16(cospi_31_64, -cospi_1_64);
  const __m128i k__cospi_p05_p27 = pair_set_epi16(cospi_5_64, cospi_27_64);
  const __m128i k__cospi_p27_m05 = pair_set_epi16(cospi_27_64, -cospi_5_64);
  const __m128i k__cospi_p09_p23 = pair_set_epi16(cospi_9_64, cospi_23_64);
  const __m128i k__cospi_p23_m09 = pair_set_epi16(cospi_23_64, -cospi_9_64);
  const __m128i k__cospi_p13_p19 = pair_set_epi16(cospi_13_64, cospi_19_64);
  const __m128i k__cospi_p19_m13 = pair_set_epi16(cospi_19_64, -cospi_13_64);
  const __m128i k__cospi_p17_p15 = pair_set_epi16(cospi_17_64, cospi_15_64);
  const __m128i k__cospi_p15_m17 = pair_set_epi16(cospi_15_64, -cospi_17_64);
  const __m128i k__cospi_p21_p11 = pair_set_epi16(cospi_21_64, cospi_11_64);
  const __m128i k__cospi_p11_m21 = pair_set_epi16(cospi_11_64, -cospi_21_64);
  const __m128i k__cospi_p25_p07 = pair_set_epi16(cospi_25_64, cospi_7_64);
  const __m128i k__cospi_p07_m25 = pair_set_epi16(cospi_7_64, -cospi_25_64);
  const __m128i k__cospi_p29_p03 = pair_set_epi16(cospi_29_64, cospi_3_64);
  const __m128i k__cospi_p03_m29 = pair_set_epi16(cospi_3_64, -cospi_29_64);
  const __m128i k__cospi_p04_p28 = pair_set_epi16(cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p28_m04 = pair_set_epi16(cospi_28_64, -cospi_4_64);
  const __m128i k__cospi_p20_p12 = pair_set_epi16(cospi_20_64, cospi_12_64);
  const __m128i k__cospi_p12_m20 = pair_set_epi16(cospi_12_64, -cospi_20_64);
  const __m128i k__cospi_m28_p04 = pair_set_epi16(-cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m12_p20 = pair_set_epi16(-cospi_12_64, cospi_20_64);
  const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_m24_p08 = pair_set_epi16(-cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m16_m16 = _mm_set1_epi16((int16_t)-cospi_16_64);
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  const __m128i kZero = _mm_set1_epi16(0);

  u[0] = _mm_unpacklo_epi16(in[15], in[0]);
  u[1] = _mm_unpackhi_epi16(in[15], in[0]);
  u[2] = _mm_unpacklo_epi16(in[13], in[2]);
  u[3] = _mm_unpackhi_epi16(in[13], in[2]);
  u[4] = _mm_unpacklo_epi16(in[11], in[4]);
  u[5] = _mm_unpackhi_epi16(in[11], in[4]);
  u[6] = _mm_unpacklo_epi16(in[9], in[6]);
  u[7] = _mm_unpackhi_epi16(in[9], in[6]);
  u[8] = _mm_unpacklo_epi16(in[7], in[8]);
  u[9] = _mm_unpackhi_epi16(in[7], in[8]);
  u[10] = _mm_unpacklo_epi16(in[5], in[10]);
  u[11] = _mm_unpackhi_epi16(in[5], in[10]);
  u[12] = _mm_unpacklo_epi16(in[3], in[12]);
  u[13] = _mm_unpackhi_epi16(in[3], in[12]);
  u[14] = _mm_unpacklo_epi16(in[1], in[14]);
  u[15] = _mm_unpackhi_epi16(in[1], in[14]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p01_p31);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p01_p31);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p31_m01);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p31_m01);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p05_p27);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p05_p27);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p27_m05);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p27_m05);
  v[8] = _mm_madd_epi16(u[4], k__cospi_p09_p23);
  v[9] = _mm_madd_epi16(u[5], k__cospi_p09_p23);
  v[10] = _mm_madd_epi16(u[4], k__cospi_p23_m09);
  v[11] = _mm_madd_epi16(u[5], k__cospi_p23_m09);
  v[12] = _mm_madd_epi16(u[6], k__cospi_p13_p19);
  v[13] = _mm_madd_epi16(u[7], k__cospi_p13_p19);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p19_m13);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p19_m13);
  v[16] = _mm_madd_epi16(u[8], k__cospi_p17_p15);
  v[17] = _mm_madd_epi16(u[9], k__cospi_p17_p15);
  v[18] = _mm_madd_epi16(u[8], k__cospi_p15_m17);
  v[19] = _mm_madd_epi16(u[9], k__cospi_p15_m17);
  v[20] = _mm_madd_epi16(u[10], k__cospi_p21_p11);
  v[21] = _mm_madd_epi16(u[11], k__cospi_p21_p11);
  v[22] = _mm_madd_epi16(u[10], k__cospi_p11_m21);
  v[23] = _mm_madd_epi16(u[11], k__cospi_p11_m21);
  v[24] = _mm_madd_epi16(u[12], k__cospi_p25_p07);
  v[25] = _mm_madd_epi16(u[13], k__cospi_p25_p07);
  v[26] = _mm_madd_epi16(u[12], k__cospi_p07_m25);
  v[27] = _mm_madd_epi16(u[13], k__cospi_p07_m25);
  v[28] = _mm_madd_epi16(u[14], k__cospi_p29_p03);
  v[29] = _mm_madd_epi16(u[15], k__cospi_p29_p03);
  v[30] = _mm_madd_epi16(u[14], k__cospi_p03_m29);
  v[31] = _mm_madd_epi16(u[15], k__cospi_p03_m29);

  u[0] = _mm_add_epi32(v[0], v[16]);
  u[1] = _mm_add_epi32(v[1], v[17]);
  u[2] = _mm_add_epi32(v[2], v[18]);
  u[3] = _mm_add_epi32(v[3], v[19]);
  u[4] = _mm_add_epi32(v[4], v[20]);
  u[5] = _mm_add_epi32(v[5], v[21]);
  u[6] = _mm_add_epi32(v[6], v[22]);
  u[7] = _mm_add_epi32(v[7], v[23]);
  u[8] = _mm_add_epi32(v[8], v[24]);
  u[9] = _mm_add_epi32(v[9], v[25]);
  u[10] = _mm_add_epi32(v[10], v[26]);
  u[11] = _mm_add_epi32(v[11], v[27]);
  u[12] = _mm_add_epi32(v[12], v[28]);
  u[13] = _mm_add_epi32(v[13], v[29]);
  u[14] = _mm_add_epi32(v[14], v[30]);
  u[15] = _mm_add_epi32(v[15], v[31]);
  u[16] = _mm_sub_epi32(v[0], v[16]);
  u[17] = _mm_sub_epi32(v[1], v[17]);
  u[18] = _mm_sub_epi32(v[2], v[18]);
  u[19] = _mm_sub_epi32(v[3], v[19]);
  u[20] = _mm_sub_epi32(v[4], v[20]);
  u[21] = _mm_sub_epi32(v[5], v[21]);
  u[22] = _mm_sub_epi32(v[6], v[22]);
  u[23] = _mm_sub_epi32(v[7], v[23]);
  u[24] = _mm_sub_epi32(v[8], v[24]);
  u[25] = _mm_sub_epi32(v[9], v[25]);
  u[26] = _mm_sub_epi32(v[10], v[26]);
  u[27] = _mm_sub_epi32(v[11], v[27]);
  u[28] = _mm_sub_epi32(v[12], v[28]);
  u[29] = _mm_sub_epi32(v[13], v[29]);
  u[30] = _mm_sub_epi32(v[14], v[30]);
  u[31] = _mm_sub_epi32(v[15], v[31]);

  v[16] = _mm_add_epi32(u[16], k__DCT_CONST_ROUNDING);
  v[17] = _mm_add_epi32(u[17], k__DCT_CONST_ROUNDING);
  v[18] = _mm_add_epi32(u[18], k__DCT_CONST_ROUNDING);
  v[19] = _mm_add_epi32(u[19], k__DCT_CONST_ROUNDING);
  v[20] = _mm_add_epi32(u[20], k__DCT_CONST_ROUNDING);
  v[21] = _mm_add_epi32(u[21], k__DCT_CONST_ROUNDING);
  v[22] = _mm_add_epi32(u[22], k__DCT_CONST_ROUNDING);
  v[23] = _mm_add_epi32(u[23], k__DCT_CONST_ROUNDING);
  v[24] = _mm_add_epi32(u[24], k__DCT_CONST_ROUNDING);
  v[25] = _mm_add_epi32(u[25], k__DCT_CONST_ROUNDING);
  v[26] = _mm_add_epi32(u[26], k__DCT_CONST_ROUNDING);
  v[27] = _mm_add_epi32(u[27], k__DCT_CONST_ROUNDING);
  v[28] = _mm_add_epi32(u[28], k__DCT_CONST_ROUNDING);
  v[29] = _mm_add_epi32(u[29], k__DCT_CONST_ROUNDING);
  v[30] = _mm_add_epi32(u[30], k__DCT_CONST_ROUNDING);
  v[31] = _mm_add_epi32(u[31], k__DCT_CONST_ROUNDING);

  u[16] = _mm_srai_epi32(v[16], DCT_CONST_BITS);
  u[17] = _mm_srai_epi32(v[17], DCT_CONST_BITS);
  u[18] = _mm_srai_epi32(v[18], DCT_CONST_BITS);
  u[19] = _mm_srai_epi32(v[19], DCT_CONST_BITS);
  u[20] = _mm_srai_epi32(v[20], DCT_CONST_BITS);
  u[21] = _mm_srai_epi32(v[21], DCT_CONST_BITS);
  u[22] = _mm_srai_epi32(v[22], DCT_CONST_BITS);
  u[23] = _mm_srai_epi32(v[23], DCT_CONST_BITS);
  u[24] = _mm_srai_epi32(v[24], DCT_CONST_BITS);
  u[25] = _mm_srai_epi32(v[25], DCT_CONST_BITS);
  u[26] = _mm_srai_epi32(v[26], DCT_CONST_BITS);
  u[27] = _mm_srai_epi32(v[27], DCT_CONST_BITS);
  u[28] = _mm_srai_epi32(v[28], DCT_CONST_BITS);
  u[29] = _mm_srai_epi32(v[29], DCT_CONST_BITS);
  u[30] = _mm_srai_epi32(v[30], DCT_CONST_BITS);
  u[31] = _mm_srai_epi32(v[31], DCT_CONST_BITS);

  v[0] = _mm_add_epi32(u[0], u[8]);
  v[1] = _mm_add_epi32(u[1], u[9]);
  v[2] = _mm_add_epi32(u[2], u[10]);
  v[3] = _mm_add_epi32(u[3], u[11]);
  v[4] = _mm_add_epi32(u[4], u[12]);
  v[5] = _mm_add_epi32(u[5], u[13]);
  v[6] = _mm_add_epi32(u[6], u[14]);
  v[7] = _mm_add_epi32(u[7], u[15]);

  v[16] = _mm_add_epi32(v[0], v[4]);
  v[17] = _mm_add_epi32(v[1], v[5]);
  v[18] = _mm_add_epi32(v[2], v[6]);
  v[19] = _mm_add_epi32(v[3], v[7]);
  v[20] = _mm_sub_epi32(v[0], v[4]);
  v[21] = _mm_sub_epi32(v[1], v[5]);
  v[22] = _mm_sub_epi32(v[2], v[6]);
  v[23] = _mm_sub_epi32(v[3], v[7]);
  v[16] = _mm_add_epi32(v[16], k__DCT_CONST_ROUNDING);
  v[17] = _mm_add_epi32(v[17], k__DCT_CONST_ROUNDING);
  v[18] = _mm_add_epi32(v[18], k__DCT_CONST_ROUNDING);
  v[19] = _mm_add_epi32(v[19], k__DCT_CONST_ROUNDING);
  v[20] = _mm_add_epi32(v[20], k__DCT_CONST_ROUNDING);
  v[21] = _mm_add_epi32(v[21], k__DCT_CONST_ROUNDING);
  v[22] = _mm_add_epi32(v[22], k__DCT_CONST_ROUNDING);
  v[23] = _mm_add_epi32(v[23], k__DCT_CONST_ROUNDING);
  v[16] = _mm_srai_epi32(v[16], DCT_CONST_BITS);
  v[17] = _mm_srai_epi32(v[17], DCT_CONST_BITS);
  v[18] = _mm_srai_epi32(v[18], DCT_CONST_BITS);
  v[19] = _mm_srai_epi32(v[19], DCT_CONST_BITS);
  v[20] = _mm_srai_epi32(v[20], DCT_CONST_BITS);
  v[21] = _mm_srai_epi32(v[21], DCT_CONST_BITS);
  v[22] = _mm_srai_epi32(v[22], DCT_CONST_BITS);
  v[23] = _mm_srai_epi32(v[23], DCT_CONST_BITS);
  s[0] = _mm_packs_epi32(v[16], v[17]);
  s[1] = _mm_packs_epi32(v[18], v[19]);
  s[2] = _mm_packs_epi32(v[20], v[21]);
  s[3] = _mm_packs_epi32(v[22], v[23]);

  v[8] = _mm_sub_epi32(u[0], u[8]);
  v[9] = _mm_sub_epi32(u[1], u[9]);
  v[10] = _mm_sub_epi32(u[2], u[10]);
  v[11] = _mm_sub_epi32(u[3], u[11]);
  v[12] = _mm_sub_epi32(u[4], u[12]);
  v[13] = _mm_sub_epi32(u[5], u[13]);
  v[14] = _mm_sub_epi32(u[6], u[14]);
  v[15] = _mm_sub_epi32(u[7], u[15]);

  v[8] = _mm_add_epi32(v[8], k__DCT_CONST_ROUNDING);
  v[9] = _mm_add_epi32(v[9], k__DCT_CONST_ROUNDING);
  v[10] = _mm_add_epi32(v[10], k__DCT_CONST_ROUNDING);
  v[11] = _mm_add_epi32(v[11], k__DCT_CONST_ROUNDING);
  v[12] = _mm_add_epi32(v[12], k__DCT_CONST_ROUNDING);
  v[13] = _mm_add_epi32(v[13], k__DCT_CONST_ROUNDING);
  v[14] = _mm_add_epi32(v[14], k__DCT_CONST_ROUNDING);
  v[15] = _mm_add_epi32(v[15], k__DCT_CONST_ROUNDING);

  v[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
  v[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
  v[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
  v[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
  v[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
  v[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
  v[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
  v[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);

  s[4] = _mm_packs_epi32(v[8], v[9]);
  s[5] = _mm_packs_epi32(v[10], v[11]);
  s[6] = _mm_packs_epi32(v[12], v[13]);
  s[7] = _mm_packs_epi32(v[14], v[15]);
  //

  s[8] = _mm_packs_epi32(u[16], u[17]);
  s[9] = _mm_packs_epi32(u[18], u[19]);
  s[10] = _mm_packs_epi32(u[20], u[21]);
  s[11] = _mm_packs_epi32(u[22], u[23]);
  s[12] = _mm_packs_epi32(u[24], u[25]);
  s[13] = _mm_packs_epi32(u[26], u[27]);
  s[14] = _mm_packs_epi32(u[28], u[29]);
  s[15] = _mm_packs_epi32(u[30], u[31]);

  // stage 2
  u[0] = _mm_unpacklo_epi16(s[8], s[9]);
  u[1] = _mm_unpackhi_epi16(s[8], s[9]);
  u[2] = _mm_unpacklo_epi16(s[10], s[11]);
  u[3] = _mm_unpackhi_epi16(s[10], s[11]);
  u[4] = _mm_unpacklo_epi16(s[12], s[13]);
  u[5] = _mm_unpackhi_epi16(s[12], s[13]);
  u[6] = _mm_unpacklo_epi16(s[14], s[15]);
  u[7] = _mm_unpackhi_epi16(s[14], s[15]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p04_p28);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p04_p28);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p28_m04);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p28_m04);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p20_p12);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p20_p12);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p12_m20);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p12_m20);
  v[8] = _mm_madd_epi16(u[4], k__cospi_m28_p04);
  v[9] = _mm_madd_epi16(u[5], k__cospi_m28_p04);
  v[10] = _mm_madd_epi16(u[4], k__cospi_p04_p28);
  v[11] = _mm_madd_epi16(u[5], k__cospi_p04_p28);
  v[12] = _mm_madd_epi16(u[6], k__cospi_m12_p20);
  v[13] = _mm_madd_epi16(u[7], k__cospi_m12_p20);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p20_p12);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p20_p12);

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

  v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
  v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
  v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
  v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
  v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
  v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
  v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
  v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

  u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
  u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
  u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
  u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
  u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
  u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
  u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
  u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);

  v[8] = _mm_add_epi32(u[0], u[4]);
  v[9] = _mm_add_epi32(u[1], u[5]);
  v[10] = _mm_add_epi32(u[2], u[6]);
  v[11] = _mm_add_epi32(u[3], u[7]);
  v[12] = _mm_sub_epi32(u[0], u[4]);
  v[13] = _mm_sub_epi32(u[1], u[5]);
  v[14] = _mm_sub_epi32(u[2], u[6]);
  v[15] = _mm_sub_epi32(u[3], u[7]);

  v[8] = _mm_add_epi32(v[8], k__DCT_CONST_ROUNDING);
  v[9] = _mm_add_epi32(v[9], k__DCT_CONST_ROUNDING);
  v[10] = _mm_add_epi32(v[10], k__DCT_CONST_ROUNDING);
  v[11] = _mm_add_epi32(v[11], k__DCT_CONST_ROUNDING);
  v[12] = _mm_add_epi32(v[12], k__DCT_CONST_ROUNDING);
  v[13] = _mm_add_epi32(v[13], k__DCT_CONST_ROUNDING);
  v[14] = _mm_add_epi32(v[14], k__DCT_CONST_ROUNDING);
  v[15] = _mm_add_epi32(v[15], k__DCT_CONST_ROUNDING);
  v[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
  v[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
  v[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
  v[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
  v[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
  v[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
  v[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
  v[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
  s[8] = _mm_packs_epi32(v[8], v[9]);
  s[9] = _mm_packs_epi32(v[10], v[11]);
  s[10] = _mm_packs_epi32(v[12], v[13]);
  s[11] = _mm_packs_epi32(v[14], v[15]);

  x[12] = _mm_packs_epi32(u[8], u[9]);
  x[13] = _mm_packs_epi32(u[10], u[11]);
  x[14] = _mm_packs_epi32(u[12], u[13]);
  x[15] = _mm_packs_epi32(u[14], u[15]);

  // stage 3
  u[0] = _mm_unpacklo_epi16(s[4], s[5]);
  u[1] = _mm_unpackhi_epi16(s[4], s[5]);
  u[2] = _mm_unpacklo_epi16(s[6], s[7]);
  u[3] = _mm_unpackhi_epi16(s[6], s[7]);
  u[4] = _mm_unpacklo_epi16(x[12], x[13]);
  u[5] = _mm_unpackhi_epi16(x[12], x[13]);
  u[6] = _mm_unpacklo_epi16(x[14], x[15]);
  u[7] = _mm_unpackhi_epi16(x[14], x[15]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p08_p24);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p08_p24);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p24_m08);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p24_m08);
  v[4] = _mm_madd_epi16(u[2], k__cospi_m24_p08);
  v[5] = _mm_madd_epi16(u[3], k__cospi_m24_p08);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p08_p24);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p08_p24);
  v[8] = _mm_madd_epi16(u[4], k__cospi_p08_p24);
  v[9] = _mm_madd_epi16(u[5], k__cospi_p08_p24);
  v[10] = _mm_madd_epi16(u[4], k__cospi_p24_m08);
  v[11] = _mm_madd_epi16(u[5], k__cospi_p24_m08);
  v[12] = _mm_madd_epi16(u[6], k__cospi_m24_p08);
  v[13] = _mm_madd_epi16(u[7], k__cospi_m24_p08);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p08_p24);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p08_p24);

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

  u[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
  u[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
  u[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
  u[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
  u[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
  u[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
  u[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
  u[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
  u[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
  v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
  v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
  v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
  v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
  v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
  v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
  v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
  v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

  s[4] = _mm_packs_epi32(v[0], v[1]);
  s[5] = _mm_packs_epi32(v[2], v[3]);
  s[6] = _mm_packs_epi32(v[4], v[5]);
  s[7] = _mm_packs_epi32(v[6], v[7]);

  s[12] = _mm_packs_epi32(v[8], v[9]);
  s[13] = _mm_packs_epi32(v[10], v[11]);
  s[14] = _mm_packs_epi32(v[12], v[13]);
  s[15] = _mm_packs_epi32(v[14], v[15]);

  // stage 4
  u[0] = _mm_unpacklo_epi16(s[2], s[3]);
  u[1] = _mm_unpackhi_epi16(s[2], s[3]);
  u[2] = _mm_unpacklo_epi16(s[6], s[7]);
  u[3] = _mm_unpackhi_epi16(s[6], s[7]);
  u[4] = _mm_unpacklo_epi16(s[10], s[11]);
  u[5] = _mm_unpackhi_epi16(s[10], s[11]);
  u[6] = _mm_unpacklo_epi16(s[14], s[15]);
  u[7] = _mm_unpackhi_epi16(s[14], s[15]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_m16_m16);
  v[1] = _mm_madd_epi16(u[1], k__cospi_m16_m16);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p16_m16);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p16_m16);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p16_p16);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p16_p16);
  v[6] = _mm_madd_epi16(u[2], k__cospi_m16_p16);
  v[7] = _mm_madd_epi16(u[3], k__cospi_m16_p16);
  v[8] = _mm_madd_epi16(u[4], k__cospi_p16_p16);
  v[9] = _mm_madd_epi16(u[5], k__cospi_p16_p16);
  v[10] = _mm_madd_epi16(u[4], k__cospi_m16_p16);
  v[11] = _mm_madd_epi16(u[5], k__cospi_m16_p16);
  v[12] = _mm_madd_epi16(u[6], k__cospi_m16_m16);
  v[13] = _mm_madd_epi16(u[7], k__cospi_m16_m16);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p16_m16);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p16_m16);

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

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
  v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
  v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
  v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
  v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
  v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
  v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
  v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
  v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

  in[0] = s[0];
  in[1] = _mm_sub_epi16(kZero, s[8]);
  in[2] = s[12];
  in[3] = _mm_sub_epi16(kZero, s[4]);
  in[4] = _mm_packs_epi32(v[4], v[5]);
  in[5] = _mm_packs_epi32(v[12], v[13]);
  in[6] = _mm_packs_epi32(v[8], v[9]);
  in[7] = _mm_packs_epi32(v[0], v[1]);
  in[8] = _mm_packs_epi32(v[2], v[3]);
  in[9] = _mm_packs_epi32(v[10], v[11]);
  in[10] = _mm_packs_epi32(v[14], v[15]);
  in[11] = _mm_packs_epi32(v[6], v[7]);
  in[12] = s[5];
  in[13] = _mm_sub_epi16(kZero, s[13]);
  in[14] = s[9];
  in[15] = _mm_sub_epi16(kZero, s[1]);
}

static void fdct16_sse2(__m128i *in0, __m128i *in1) {
  fdct16_8col(in0);
  fdct16_8col(in1);
  array_transpose_16x16(in0, in1);
}

static void fadst16_sse2(__m128i *in0, __m128i *in1) {
  fadst16_8col(in0);
  fadst16_8col(in1);
  array_transpose_16x16(in0, in1);
}

#if CONFIG_EXT_TX
static void fidtx16_sse2(__m128i *in0, __m128i *in1) {
  idtx16_8col(in0);
  idtx16_8col(in1);
  array_transpose_16x16(in0, in1);
}
#endif  // CONFIG_EXT_TX

void av1_fht16x16_sse2(const int16_t *input, tran_low_t *output, int stride,
                       TxfmParam *txfm_param) {
  __m128i in0[16], in1[16];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fdct16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fdct16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case ADST_DCT:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fadst16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fdct16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case DCT_ADST:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fdct16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case ADST_ADST:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fadst16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_16x16(input, in0, in1, stride, 1, 0);
      fadst16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fdct16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case DCT_FLIPADST:
      load_buffer_16x16(input, in0, in1, stride, 0, 1);
      fdct16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_16x16(input, in0, in1, stride, 1, 1);
      fadst16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case ADST_FLIPADST:
      load_buffer_16x16(input, in0, in1, stride, 0, 1);
      fadst16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case FLIPADST_ADST:
      load_buffer_16x16(input, in0, in1, stride, 1, 0);
      fadst16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case IDTX:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fidtx16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fidtx16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case V_DCT:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fdct16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fidtx16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case H_DCT:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fidtx16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fdct16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case V_ADST:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fadst16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fidtx16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case H_ADST:
      load_buffer_16x16(input, in0, in1, stride, 0, 0);
      fidtx16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case V_FLIPADST:
      load_buffer_16x16(input, in0, in1, stride, 1, 0);
      fadst16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fidtx16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
    case H_FLIPADST:
      load_buffer_16x16(input, in0, in1, stride, 0, 1);
      fidtx16_sse2(in0, in1);
      right_shift_16x16(in0, in1);
      fadst16_sse2(in0, in1);
      write_buffer_16x16(output, in0, in1, 16);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

static INLINE void prepare_4x8_row_first(__m128i *in) {
  in[0] = _mm_unpacklo_epi64(in[0], in[2]);
  in[1] = _mm_unpacklo_epi64(in[1], in[3]);
  transpose_4x4(in);
  in[4] = _mm_unpacklo_epi64(in[4], in[6]);
  in[5] = _mm_unpacklo_epi64(in[5], in[7]);
  transpose_4x4(in + 4);
}

// Load input into the left-hand half of in (ie, into lanes 0..3 of
// each element of in). The right hand half (lanes 4..7) should be
// treated as being filled with "don't care" values.
static INLINE void load_buffer_4x8(const int16_t *input, __m128i *in,
                                   int stride, int flipud, int fliplr) {
  const int shift = 2;
  if (!flipud) {
    in[0] = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
    in[1] = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
    in[2] = _mm_loadl_epi64((const __m128i *)(input + 2 * stride));
    in[3] = _mm_loadl_epi64((const __m128i *)(input + 3 * stride));
    in[4] = _mm_loadl_epi64((const __m128i *)(input + 4 * stride));
    in[5] = _mm_loadl_epi64((const __m128i *)(input + 5 * stride));
    in[6] = _mm_loadl_epi64((const __m128i *)(input + 6 * stride));
    in[7] = _mm_loadl_epi64((const __m128i *)(input + 7 * stride));
  } else {
    in[0] = _mm_loadl_epi64((const __m128i *)(input + 7 * stride));
    in[1] = _mm_loadl_epi64((const __m128i *)(input + 6 * stride));
    in[2] = _mm_loadl_epi64((const __m128i *)(input + 5 * stride));
    in[3] = _mm_loadl_epi64((const __m128i *)(input + 4 * stride));
    in[4] = _mm_loadl_epi64((const __m128i *)(input + 3 * stride));
    in[5] = _mm_loadl_epi64((const __m128i *)(input + 2 * stride));
    in[6] = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
    in[7] = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
  }

  if (fliplr) {
    in[0] = _mm_shufflelo_epi16(in[0], 0x1b);
    in[1] = _mm_shufflelo_epi16(in[1], 0x1b);
    in[2] = _mm_shufflelo_epi16(in[2], 0x1b);
    in[3] = _mm_shufflelo_epi16(in[3], 0x1b);
    in[4] = _mm_shufflelo_epi16(in[4], 0x1b);
    in[5] = _mm_shufflelo_epi16(in[5], 0x1b);
    in[6] = _mm_shufflelo_epi16(in[6], 0x1b);
    in[7] = _mm_shufflelo_epi16(in[7], 0x1b);
  }

  in[0] = _mm_slli_epi16(in[0], shift);
  in[1] = _mm_slli_epi16(in[1], shift);
  in[2] = _mm_slli_epi16(in[2], shift);
  in[3] = _mm_slli_epi16(in[3], shift);
  in[4] = _mm_slli_epi16(in[4], shift);
  in[5] = _mm_slli_epi16(in[5], shift);
  in[6] = _mm_slli_epi16(in[6], shift);
  in[7] = _mm_slli_epi16(in[7], shift);

  scale_sqrt2_8x4(in);
  scale_sqrt2_8x4(in + 4);
  prepare_4x8_row_first(in);
}

static INLINE void write_buffer_4x8(tran_low_t *output, __m128i *res) {
  __m128i in01, in23, in45, in67, sign01, sign23, sign45, sign67;
  const int shift = 1;

  // revert the 8x8 txfm's transpose
  array_transpose_8x8(res, res);

  in01 = _mm_unpacklo_epi64(res[0], res[1]);
  in23 = _mm_unpacklo_epi64(res[2], res[3]);
  in45 = _mm_unpacklo_epi64(res[4], res[5]);
  in67 = _mm_unpacklo_epi64(res[6], res[7]);

  sign01 = _mm_srai_epi16(in01, 15);
  sign23 = _mm_srai_epi16(in23, 15);
  sign45 = _mm_srai_epi16(in45, 15);
  sign67 = _mm_srai_epi16(in67, 15);

  in01 = _mm_sub_epi16(in01, sign01);
  in23 = _mm_sub_epi16(in23, sign23);
  in45 = _mm_sub_epi16(in45, sign45);
  in67 = _mm_sub_epi16(in67, sign67);

  in01 = _mm_srai_epi16(in01, shift);
  in23 = _mm_srai_epi16(in23, shift);
  in45 = _mm_srai_epi16(in45, shift);
  in67 = _mm_srai_epi16(in67, shift);

  store_output(&in01, (output + 0 * 8));
  store_output(&in23, (output + 1 * 8));
  store_output(&in45, (output + 2 * 8));
  store_output(&in67, (output + 3 * 8));
}

void av1_fht4x8_sse2(const int16_t *input, tran_low_t *output, int stride,
                     TxfmParam *txfm_param) {
  __m128i in[8];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_4x8(input, in, stride, 0, 0);
      fdct4_sse2(in);
      fdct4_sse2(in + 4);
      fdct8_sse2(in);
      break;
    case ADST_DCT:
      load_buffer_4x8(input, in, stride, 0, 0);
      fdct4_sse2(in);
      fdct4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case DCT_ADST:
      load_buffer_4x8(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fdct8_sse2(in);
      break;
    case ADST_ADST:
      load_buffer_4x8(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fadst8_sse2(in);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_4x8(input, in, stride, 1, 0);
      fdct4_sse2(in);
      fdct4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case DCT_FLIPADST:
      load_buffer_4x8(input, in, stride, 0, 1);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fdct8_sse2(in);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_4x8(input, in, stride, 1, 1);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case ADST_FLIPADST:
      load_buffer_4x8(input, in, stride, 0, 1);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case FLIPADST_ADST:
      load_buffer_4x8(input, in, stride, 1, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case IDTX:
      load_buffer_4x8(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fidtx4_sse2(in + 4);
      fidtx8_sse2(in);
      break;
    case V_DCT:
      load_buffer_4x8(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fidtx4_sse2(in + 4);
      fdct8_sse2(in);
      break;
    case H_DCT:
      load_buffer_4x8(input, in, stride, 0, 0);
      fdct4_sse2(in);
      fdct4_sse2(in + 4);
      fidtx8_sse2(in);
      break;
    case V_ADST:
      load_buffer_4x8(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fidtx4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case H_ADST:
      load_buffer_4x8(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fidtx8_sse2(in);
      break;
    case V_FLIPADST:
      load_buffer_4x8(input, in, stride, 1, 0);
      fidtx4_sse2(in);
      fidtx4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case H_FLIPADST:
      load_buffer_4x8(input, in, stride, 0, 1);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fidtx8_sse2(in);
      break;
#endif
    default: assert(0); break;
  }
  write_buffer_4x8(output, in);
}

// Load input into the left-hand half of in (ie, into lanes 0..3 of
// each element of in). The right hand half (lanes 4..7) should be
// treated as being filled with "don't care" values.
// The input is split horizontally into two 4x4
// chunks 'l' and 'r'. Then 'l' is stored in the top-left 4x4
// block of 'in' and 'r' is stored in the bottom-left block.
// This is to allow us to reuse 4x4 transforms.
static INLINE void load_buffer_8x4(const int16_t *input, __m128i *in,
                                   int stride, int flipud, int fliplr) {
  const int shift = 2;
  if (!flipud) {
    in[0] = _mm_loadu_si128((const __m128i *)(input + 0 * stride));
    in[1] = _mm_loadu_si128((const __m128i *)(input + 1 * stride));
    in[2] = _mm_loadu_si128((const __m128i *)(input + 2 * stride));
    in[3] = _mm_loadu_si128((const __m128i *)(input + 3 * stride));
  } else {
    in[0] = _mm_loadu_si128((const __m128i *)(input + 3 * stride));
    in[1] = _mm_loadu_si128((const __m128i *)(input + 2 * stride));
    in[2] = _mm_loadu_si128((const __m128i *)(input + 1 * stride));
    in[3] = _mm_loadu_si128((const __m128i *)(input + 0 * stride));
  }

  if (fliplr) {
    in[0] = mm_reverse_epi16(in[0]);
    in[1] = mm_reverse_epi16(in[1]);
    in[2] = mm_reverse_epi16(in[2]);
    in[3] = mm_reverse_epi16(in[3]);
  }

  in[0] = _mm_slli_epi16(in[0], shift);
  in[1] = _mm_slli_epi16(in[1], shift);
  in[2] = _mm_slli_epi16(in[2], shift);
  in[3] = _mm_slli_epi16(in[3], shift);

  scale_sqrt2_8x4(in);

  in[4] = _mm_shuffle_epi32(in[0], 0xe);
  in[5] = _mm_shuffle_epi32(in[1], 0xe);
  in[6] = _mm_shuffle_epi32(in[2], 0xe);
  in[7] = _mm_shuffle_epi32(in[3], 0xe);
}

static INLINE void write_buffer_8x4(tran_low_t *output, __m128i *res) {
  __m128i out0, out1, out2, out3, sign0, sign1, sign2, sign3;
  const int shift = 1;
  sign0 = _mm_srai_epi16(res[0], 15);
  sign1 = _mm_srai_epi16(res[1], 15);
  sign2 = _mm_srai_epi16(res[2], 15);
  sign3 = _mm_srai_epi16(res[3], 15);

  out0 = _mm_sub_epi16(res[0], sign0);
  out1 = _mm_sub_epi16(res[1], sign1);
  out2 = _mm_sub_epi16(res[2], sign2);
  out3 = _mm_sub_epi16(res[3], sign3);

  out0 = _mm_srai_epi16(out0, shift);
  out1 = _mm_srai_epi16(out1, shift);
  out2 = _mm_srai_epi16(out2, shift);
  out3 = _mm_srai_epi16(out3, shift);

  store_output(&out0, (output + 0 * 8));
  store_output(&out1, (output + 1 * 8));
  store_output(&out2, (output + 2 * 8));
  store_output(&out3, (output + 3 * 8));
}

void av1_fht8x4_sse2(const int16_t *input, tran_low_t *output, int stride,
                     TxfmParam *txfm_param) {
  __m128i in[8];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_8x4(input, in, stride, 0, 0);
      fdct4_sse2(in);
      fdct4_sse2(in + 4);
      fdct8_sse2(in);
      break;
    case ADST_DCT:
      load_buffer_8x4(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fdct8_sse2(in);
      break;
    case DCT_ADST:
      load_buffer_8x4(input, in, stride, 0, 0);
      fdct4_sse2(in);
      fdct4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case ADST_ADST:
      load_buffer_8x4(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fadst8_sse2(in);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_8x4(input, in, stride, 1, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fdct8_sse2(in);
      break;
    case DCT_FLIPADST:
      load_buffer_8x4(input, in, stride, 0, 1);
      fdct4_sse2(in);
      fdct4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_8x4(input, in, stride, 1, 1);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case ADST_FLIPADST:
      load_buffer_8x4(input, in, stride, 0, 1);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case FLIPADST_ADST:
      load_buffer_8x4(input, in, stride, 1, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case IDTX:
      load_buffer_8x4(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fidtx4_sse2(in + 4);
      fidtx8_sse2(in);
      break;
    case V_DCT:
      load_buffer_8x4(input, in, stride, 0, 0);
      fdct4_sse2(in);
      fdct4_sse2(in + 4);
      fidtx8_sse2(in);
      break;
    case H_DCT:
      load_buffer_8x4(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fidtx4_sse2(in + 4);
      fdct8_sse2(in);
      break;
    case V_ADST:
      load_buffer_8x4(input, in, stride, 0, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fidtx8_sse2(in);
      break;
    case H_ADST:
      load_buffer_8x4(input, in, stride, 0, 0);
      fidtx4_sse2(in);
      fidtx4_sse2(in + 4);
      fadst8_sse2(in);
      break;
    case V_FLIPADST:
      load_buffer_8x4(input, in, stride, 1, 0);
      fadst4_sse2(in);
      fadst4_sse2(in + 4);
      fidtx8_sse2(in);
      break;
    case H_FLIPADST:
      load_buffer_8x4(input, in, stride, 0, 1);
      fidtx4_sse2(in);
      fidtx4_sse2(in + 4);
      fadst8_sse2(in);
      break;
#endif
    default: assert(0); break;
  }
  write_buffer_8x4(output, in);
}

static INLINE void load_buffer_8x16(const int16_t *input, __m128i *in,
                                    int stride, int flipud, int fliplr) {
  // Load 2 8x8 blocks
  const int16_t *t = input;
  const int16_t *b = input + 8 * stride;

  if (flipud) {
    const int16_t *const tmp = t;
    t = b;
    b = tmp;
  }

  load_buffer_8x8(t, in, stride, flipud, fliplr);
  scale_sqrt2_8x8(in);
  load_buffer_8x8(b, in + 8, stride, flipud, fliplr);
  scale_sqrt2_8x8(in + 8);
}

static INLINE void round_power_of_two_signed(__m128i *x, int n) {
  const __m128i rounding = _mm_set1_epi16((1 << n) >> 1);
  const __m128i sign = _mm_srai_epi16(*x, 15);
  const __m128i res = _mm_add_epi16(_mm_add_epi16(*x, rounding), sign);
  *x = _mm_srai_epi16(res, n);
}

static void row_8x16_rounding(__m128i *in, int bits) {
  int i;
  for (i = 0; i < 16; i++) {
    round_power_of_two_signed(&in[i], bits);
  }
}

void av1_fht8x16_sse2(const int16_t *input, tran_low_t *output, int stride,
                      TxfmParam *txfm_param) {
  __m128i in[16];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  __m128i *const t = in;      // Alias to top 8x8 sub block
  __m128i *const b = in + 8;  // Alias to bottom 8x8 sub block

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fdct8_sse2(t);
      fdct8_sse2(b);
      row_8x16_rounding(in, 2);
      fdct16_8col(in);
      break;
    case ADST_DCT:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fdct8_sse2(t);
      fdct8_sse2(b);
      row_8x16_rounding(in, 2);
      fadst16_8col(in);
      break;
    case DCT_ADST:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fadst8_sse2(t);
      fadst8_sse2(b);
      row_8x16_rounding(in, 2);
      fdct16_8col(in);
      break;
    case ADST_ADST:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fadst8_sse2(t);
      fadst8_sse2(b);
      row_8x16_rounding(in, 2);
      fadst16_8col(in);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_8x16(input, in, stride, 1, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fdct8_sse2(t);
      fdct8_sse2(b);
      row_8x16_rounding(in, 2);
      fadst16_8col(in);
      break;
    case DCT_FLIPADST:
      load_buffer_8x16(input, in, stride, 0, 1);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fadst8_sse2(t);
      fadst8_sse2(b);
      row_8x16_rounding(in, 2);
      fdct16_8col(in);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_8x16(input, in, stride, 1, 1);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fadst8_sse2(t);
      fadst8_sse2(b);
      row_8x16_rounding(in, 2);
      fadst16_8col(in);
      break;
    case ADST_FLIPADST:
      load_buffer_8x16(input, in, stride, 0, 1);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fadst8_sse2(t);
      fadst8_sse2(b);
      row_8x16_rounding(in, 2);
      fadst16_8col(in);
      break;
    case FLIPADST_ADST:
      load_buffer_8x16(input, in, stride, 1, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fadst8_sse2(t);
      fadst8_sse2(b);
      row_8x16_rounding(in, 2);
      fadst16_8col(in);
      break;
    case IDTX:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fidtx8_sse2(t);
      fidtx8_sse2(b);
      row_8x16_rounding(in, 2);
      idtx16_8col(in);
      break;
    case V_DCT:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fidtx8_sse2(t);
      fidtx8_sse2(b);
      row_8x16_rounding(in, 2);
      fdct16_8col(in);
      break;
    case H_DCT:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fdct8_sse2(t);
      fdct8_sse2(b);
      row_8x16_rounding(in, 2);
      idtx16_8col(in);
      break;
    case V_ADST:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fidtx8_sse2(t);
      fidtx8_sse2(b);
      row_8x16_rounding(in, 2);
      fadst16_8col(in);
      break;
    case H_ADST:
      load_buffer_8x16(input, in, stride, 0, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fadst8_sse2(t);
      fadst8_sse2(b);
      row_8x16_rounding(in, 2);
      idtx16_8col(in);
      break;
    case V_FLIPADST:
      load_buffer_8x16(input, in, stride, 1, 0);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fidtx8_sse2(t);
      fidtx8_sse2(b);
      row_8x16_rounding(in, 2);
      fadst16_8col(in);
      break;
    case H_FLIPADST:
      load_buffer_8x16(input, in, stride, 0, 1);
      array_transpose_8x8(t, t);
      array_transpose_8x8(b, b);
      fadst8_sse2(t);
      fadst8_sse2(b);
      row_8x16_rounding(in, 2);
      idtx16_8col(in);
      break;
#endif
    default: assert(0); break;
  }
  write_buffer_8x8(output, t, 8);
  write_buffer_8x8(output + 64, b, 8);
}

static INLINE void load_buffer_16x8(const int16_t *input, __m128i *in,
                                    int stride, int flipud, int fliplr) {
  // Load 2 8x8 blocks
  const int16_t *l = input;
  const int16_t *r = input + 8;

  if (fliplr) {
    const int16_t *const tmp = l;
    l = r;
    r = tmp;
  }

  // load first 8 columns
  load_buffer_8x8(l, in, stride, flipud, fliplr);
  scale_sqrt2_8x8(in);
  load_buffer_8x8(r, in + 8, stride, flipud, fliplr);
  scale_sqrt2_8x8(in + 8);
}

#define col_16x8_rounding row_8x16_rounding

void av1_fht16x8_sse2(const int16_t *input, tran_low_t *output, int stride,
                      TxfmParam *txfm_param) {
  __m128i in[16];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  __m128i *const l = in;      // Alias to left 8x8 sub block
  __m128i *const r = in + 8;  // Alias to right 8x8 sub block, which we store
                              // in the second half of the array

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_16x8(input, in, stride, 0, 0);
      fdct8_sse2(l);
      fdct8_sse2(r);
      col_16x8_rounding(in, 2);
      fdct16_8col(in);
      break;
    case ADST_DCT:
      load_buffer_16x8(input, in, stride, 0, 0);
      fadst8_sse2(l);
      fadst8_sse2(r);
      col_16x8_rounding(in, 2);
      fdct16_8col(in);
      break;
    case DCT_ADST:
      load_buffer_16x8(input, in, stride, 0, 0);
      fdct8_sse2(l);
      fdct8_sse2(r);
      col_16x8_rounding(in, 2);
      fadst16_8col(in);
      break;
    case ADST_ADST:
      load_buffer_16x8(input, in, stride, 0, 0);
      fadst8_sse2(l);
      fadst8_sse2(r);
      col_16x8_rounding(in, 2);
      fadst16_8col(in);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_16x8(input, in, stride, 1, 0);
      fadst8_sse2(l);
      fadst8_sse2(r);
      col_16x8_rounding(in, 2);
      fdct16_8col(in);
      break;
    case DCT_FLIPADST:
      load_buffer_16x8(input, in, stride, 0, 1);
      fdct8_sse2(l);
      fdct8_sse2(r);
      col_16x8_rounding(in, 2);
      fadst16_8col(in);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_16x8(input, in, stride, 1, 1);
      fadst8_sse2(l);
      fadst8_sse2(r);
      col_16x8_rounding(in, 2);
      fadst16_8col(in);
      break;
    case ADST_FLIPADST:
      load_buffer_16x8(input, in, stride, 0, 1);
      fadst8_sse2(l);
      fadst8_sse2(r);
      col_16x8_rounding(in, 2);
      fadst16_8col(in);
      break;
    case FLIPADST_ADST:
      load_buffer_16x8(input, in, stride, 1, 0);
      fadst8_sse2(l);
      fadst8_sse2(r);
      col_16x8_rounding(in, 2);
      fadst16_8col(in);
      break;
    case IDTX:
      load_buffer_16x8(input, in, stride, 0, 0);
      fidtx8_sse2(l);
      fidtx8_sse2(r);
      col_16x8_rounding(in, 2);
      idtx16_8col(in);
      break;
    case V_DCT:
      load_buffer_16x8(input, in, stride, 0, 0);
      fdct8_sse2(l);
      fdct8_sse2(r);
      col_16x8_rounding(in, 2);
      idtx16_8col(in);
      break;
    case H_DCT:
      load_buffer_16x8(input, in, stride, 0, 0);
      fidtx8_sse2(l);
      fidtx8_sse2(r);
      col_16x8_rounding(in, 2);
      fdct16_8col(in);
      break;
    case V_ADST:
      load_buffer_16x8(input, in, stride, 0, 0);
      fadst8_sse2(l);
      fadst8_sse2(r);
      col_16x8_rounding(in, 2);
      idtx16_8col(in);
      break;
    case H_ADST:
      load_buffer_16x8(input, in, stride, 0, 0);
      fidtx8_sse2(l);
      fidtx8_sse2(r);
      col_16x8_rounding(in, 2);
      fadst16_8col(in);
      break;
    case V_FLIPADST:
      load_buffer_16x8(input, in, stride, 1, 0);
      fadst8_sse2(l);
      fadst8_sse2(r);
      col_16x8_rounding(in, 2);
      idtx16_8col(in);
      break;
    case H_FLIPADST:
      load_buffer_16x8(input, in, stride, 0, 1);
      fidtx8_sse2(l);
      fidtx8_sse2(r);
      col_16x8_rounding(in, 2);
      fadst16_8col(in);
      break;
#endif
    default: assert(0); break;
  }
  array_transpose_8x8(l, l);
  array_transpose_8x8(r, r);
  write_buffer_8x8(output, l, 16);
  write_buffer_8x8(output + 8, r, 16);
}

// Note: The 16-column 32-element transforms expect their input to be
// split up into a 2x2 grid of 8x16 blocks
static INLINE void fdct32_16col(__m128i *tl, __m128i *tr, __m128i *bl,
                                __m128i *br) {
  fdct32_8col(tl, bl);
  fdct32_8col(tr, br);
  array_transpose_16x16(tl, tr);
  array_transpose_16x16(bl, br);
}

#if CONFIG_EXT_TX
static INLINE void fidtx32_16col(__m128i *tl, __m128i *tr, __m128i *bl,
                                 __m128i *br) {
  int i;
  for (i = 0; i < 16; ++i) {
    tl[i] = _mm_slli_epi16(tl[i], 2);
    tr[i] = _mm_slli_epi16(tr[i], 2);
    bl[i] = _mm_slli_epi16(bl[i], 2);
    br[i] = _mm_slli_epi16(br[i], 2);
  }
  array_transpose_16x16(tl, tr);
  array_transpose_16x16(bl, br);
}
#endif

static INLINE void load_buffer_16x32(const int16_t *input, __m128i *intl,
                                     __m128i *intr, __m128i *inbl,
                                     __m128i *inbr, int stride, int flipud,
                                     int fliplr) {
  int i;
  if (flipud) {
    input = input + 31 * stride;
    stride = -stride;
  }

  for (i = 0; i < 16; ++i) {
    intl[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 0)), 2);
    intr[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 8)), 2);
    inbl[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + (i + 16) * stride + 0)), 2);
    inbr[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + (i + 16) * stride + 8)), 2);
  }

  if (fliplr) {
    __m128i tmp;
    for (i = 0; i < 16; ++i) {
      tmp = intl[i];
      intl[i] = mm_reverse_epi16(intr[i]);
      intr[i] = mm_reverse_epi16(tmp);
      tmp = inbl[i];
      inbl[i] = mm_reverse_epi16(inbr[i]);
      inbr[i] = mm_reverse_epi16(tmp);
    }
  }

  scale_sqrt2_8x16(intl);
  scale_sqrt2_8x16(intr);
  scale_sqrt2_8x16(inbl);
  scale_sqrt2_8x16(inbr);
}

static INLINE void write_buffer_16x32(tran_low_t *output, __m128i *restl,
                                      __m128i *restr, __m128i *resbl,
                                      __m128i *resbr) {
  int i;
  for (i = 0; i < 16; ++i) {
    store_output(&restl[i], output + i * 16 + 0);
    store_output(&restr[i], output + i * 16 + 8);
    store_output(&resbl[i], output + (i + 16) * 16 + 0);
    store_output(&resbr[i], output + (i + 16) * 16 + 8);
  }
}

static INLINE void round_signed_8x8(__m128i *in, const int bit) {
  const __m128i rounding = _mm_set1_epi16((1 << bit) >> 1);
  __m128i sign0 = _mm_srai_epi16(in[0], 15);
  __m128i sign1 = _mm_srai_epi16(in[1], 15);
  __m128i sign2 = _mm_srai_epi16(in[2], 15);
  __m128i sign3 = _mm_srai_epi16(in[3], 15);
  __m128i sign4 = _mm_srai_epi16(in[4], 15);
  __m128i sign5 = _mm_srai_epi16(in[5], 15);
  __m128i sign6 = _mm_srai_epi16(in[6], 15);
  __m128i sign7 = _mm_srai_epi16(in[7], 15);

  in[0] = _mm_add_epi16(_mm_add_epi16(in[0], rounding), sign0);
  in[1] = _mm_add_epi16(_mm_add_epi16(in[1], rounding), sign1);
  in[2] = _mm_add_epi16(_mm_add_epi16(in[2], rounding), sign2);
  in[3] = _mm_add_epi16(_mm_add_epi16(in[3], rounding), sign3);
  in[4] = _mm_add_epi16(_mm_add_epi16(in[4], rounding), sign4);
  in[5] = _mm_add_epi16(_mm_add_epi16(in[5], rounding), sign5);
  in[6] = _mm_add_epi16(_mm_add_epi16(in[6], rounding), sign6);
  in[7] = _mm_add_epi16(_mm_add_epi16(in[7], rounding), sign7);

  in[0] = _mm_srai_epi16(in[0], bit);
  in[1] = _mm_srai_epi16(in[1], bit);
  in[2] = _mm_srai_epi16(in[2], bit);
  in[3] = _mm_srai_epi16(in[3], bit);
  in[4] = _mm_srai_epi16(in[4], bit);
  in[5] = _mm_srai_epi16(in[5], bit);
  in[6] = _mm_srai_epi16(in[6], bit);
  in[7] = _mm_srai_epi16(in[7], bit);
}

static INLINE void round_signed_16x16(__m128i *in0, __m128i *in1) {
  const int bit = 4;
  round_signed_8x8(in0, bit);
  round_signed_8x8(in0 + 8, bit);
  round_signed_8x8(in1, bit);
  round_signed_8x8(in1 + 8, bit);
}

// Note:
//  suffix "t" indicates the transpose operation comes first
static void fdct16t_sse2(__m128i *in0, __m128i *in1) {
  array_transpose_16x16(in0, in1);
  fdct16_8col(in0);
  fdct16_8col(in1);
}

static void fadst16t_sse2(__m128i *in0, __m128i *in1) {
  array_transpose_16x16(in0, in1);
  fadst16_8col(in0);
  fadst16_8col(in1);
}

static INLINE void fdct32t_16col(__m128i *tl, __m128i *tr, __m128i *bl,
                                 __m128i *br) {
  array_transpose_16x16(tl, tr);
  array_transpose_16x16(bl, br);
  fdct32_8col(tl, bl);
  fdct32_8col(tr, br);
}

typedef enum transpose_indicator_ {
  transpose,
  no_transpose,
} transpose_indicator;

static INLINE void fhalfright32_16col(__m128i *tl, __m128i *tr, __m128i *bl,
                                      __m128i *br, transpose_indicator t) {
  __m128i tmpl[16], tmpr[16];
  int i;

  // Copy the bottom half of the input to temporary storage
  for (i = 0; i < 16; ++i) {
    tmpl[i] = bl[i];
    tmpr[i] = br[i];
  }

  // Generate the bottom half of the output
  for (i = 0; i < 16; ++i) {
    bl[i] = _mm_slli_epi16(tl[i], 2);
    br[i] = _mm_slli_epi16(tr[i], 2);
  }
  array_transpose_16x16(bl, br);

  // Copy the temporary storage back to the top half of the input
  for (i = 0; i < 16; ++i) {
    tl[i] = tmpl[i];
    tr[i] = tmpr[i];
  }

  // Generate the top half of the output
  scale_sqrt2_8x16(tl);
  scale_sqrt2_8x16(tr);
  if (t == transpose)
    fdct16t_sse2(tl, tr);
  else
    fdct16_sse2(tl, tr);
}

// Note on data layout, for both this and the 32x16 transforms:
// So that we can reuse the 16-element transforms easily,
// we want to split the input into 8x16 blocks.
// For 16x32, this means the input is a 2x2 grid of such blocks.
// For 32x16, it means the input is a 4x1 grid.
void av1_fht16x32_sse2(const int16_t *input, tran_low_t *output, int stride,
                       TxfmParam *txfm_param) {
  __m128i intl[16], intr[16], inbl[16], inbr[16];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fdct16t_sse2(intl, intr);
      fdct16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fdct32t_16col(intl, intr, inbl, inbr);
      break;
    case ADST_DCT:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fdct16t_sse2(intl, intr);
      fdct16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fhalfright32_16col(intl, intr, inbl, inbr, transpose);
      break;
    case DCT_ADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fadst16t_sse2(intl, intr);
      fadst16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fdct32t_16col(intl, intr, inbl, inbr);
      break;
    case ADST_ADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fadst16t_sse2(intl, intr);
      fadst16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fhalfright32_16col(intl, intr, inbl, inbr, transpose);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 1, 0);
      fdct16t_sse2(intl, intr);
      fdct16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fhalfright32_16col(intl, intr, inbl, inbr, transpose);
      break;
    case DCT_FLIPADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 1);
      fadst16t_sse2(intl, intr);
      fadst16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fdct32t_16col(intl, intr, inbl, inbr);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 1, 1);
      fadst16t_sse2(intl, intr);
      fadst16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fhalfright32_16col(intl, intr, inbl, inbr, transpose);
      break;
    case ADST_FLIPADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 1);
      fadst16t_sse2(intl, intr);
      fadst16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fhalfright32_16col(intl, intr, inbl, inbr, transpose);
      break;
    case FLIPADST_ADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 1, 0);
      fadst16t_sse2(intl, intr);
      fadst16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fhalfright32_16col(intl, intr, inbl, inbr, transpose);
      break;
    case IDTX:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fidtx16_sse2(intl, intr);
      fidtx16_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fidtx32_16col(intl, intr, inbl, inbr);
      break;
    case V_DCT:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fidtx16_sse2(intl, intr);
      fidtx16_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fdct32t_16col(intl, intr, inbl, inbr);
      break;
    case H_DCT:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fdct16t_sse2(intl, intr);
      fdct16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fidtx32_16col(intl, intr, inbl, inbr);
      break;
    case V_ADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fidtx16_sse2(intl, intr);
      fidtx16_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fhalfright32_16col(intl, intr, inbl, inbr, transpose);
      break;
    case H_ADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 0);
      fadst16t_sse2(intl, intr);
      fadst16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fidtx32_16col(intl, intr, inbl, inbr);
      break;
    case V_FLIPADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 1, 0);
      fidtx16_sse2(intl, intr);
      fidtx16_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fhalfright32_16col(intl, intr, inbl, inbr, transpose);
      break;
    case H_FLIPADST:
      load_buffer_16x32(input, intl, intr, inbl, inbr, stride, 0, 1);
      fadst16t_sse2(intl, intr);
      fadst16t_sse2(inbl, inbr);
      round_signed_16x16(intl, intr);
      round_signed_16x16(inbl, inbr);
      fidtx32_16col(intl, intr, inbl, inbr);
      break;
#endif
    default: assert(0); break;
  }
  write_buffer_16x32(output, intl, intr, inbl, inbr);
}

static INLINE void load_buffer_32x16(const int16_t *input, __m128i *in0,
                                     __m128i *in1, __m128i *in2, __m128i *in3,
                                     int stride, int flipud, int fliplr) {
  int i;
  if (flipud) {
    input += 15 * stride;
    stride = -stride;
  }

  for (i = 0; i < 16; ++i) {
    in0[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 0)), 2);
    in1[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 8)), 2);
    in2[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 16)), 2);
    in3[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 24)), 2);
  }

  if (fliplr) {
    for (i = 0; i < 16; ++i) {
      __m128i tmp1 = in0[i];
      __m128i tmp2 = in1[i];
      in0[i] = mm_reverse_epi16(in3[i]);
      in1[i] = mm_reverse_epi16(in2[i]);
      in2[i] = mm_reverse_epi16(tmp2);
      in3[i] = mm_reverse_epi16(tmp1);
    }
  }

  scale_sqrt2_8x16(in0);
  scale_sqrt2_8x16(in1);
  scale_sqrt2_8x16(in2);
  scale_sqrt2_8x16(in3);
}

static INLINE void write_buffer_32x16(tran_low_t *output, __m128i *res0,
                                      __m128i *res1, __m128i *res2,
                                      __m128i *res3) {
  int i;
  for (i = 0; i < 16; ++i) {
    store_output(&res0[i], output + i * 32 + 0);
    store_output(&res1[i], output + i * 32 + 8);
    store_output(&res2[i], output + i * 32 + 16);
    store_output(&res3[i], output + i * 32 + 24);
  }
}

void av1_fht32x16_sse2(const int16_t *input, tran_low_t *output, int stride,
                       TxfmParam *txfm_param) {
  __m128i in0[16], in1[16], in2[16], in3[16];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "Invalid tx type for tx size");
#endif

  load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 0);
  switch (tx_type) {
    case DCT_DCT:
      fdct16_sse2(in0, in1);
      fdct16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fdct32_16col(in0, in1, in2, in3);
      break;
    case ADST_DCT:
      fadst16_sse2(in0, in1);
      fadst16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fdct32_16col(in0, in1, in2, in3);
      break;
    case DCT_ADST:
      fdct16_sse2(in0, in1);
      fdct16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fhalfright32_16col(in0, in1, in2, in3, no_transpose);
      break;
    case ADST_ADST:
      fadst16_sse2(in0, in1);
      fadst16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fhalfright32_16col(in0, in1, in2, in3, no_transpose);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 1, 0);
      fadst16_sse2(in0, in1);
      fadst16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fdct32_16col(in0, in1, in2, in3);
      break;
    case DCT_FLIPADST:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 1);
      fdct16_sse2(in0, in1);
      fdct16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fhalfright32_16col(in0, in1, in2, in3, no_transpose);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 1, 1);
      fadst16_sse2(in0, in1);
      fadst16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fhalfright32_16col(in0, in1, in2, in3, no_transpose);
      break;
    case ADST_FLIPADST:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 1);
      fadst16_sse2(in0, in1);
      fadst16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fhalfright32_16col(in0, in1, in2, in3, no_transpose);
      break;
    case FLIPADST_ADST:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 1, 0);
      fadst16_sse2(in0, in1);
      fadst16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fhalfright32_16col(in0, in1, in2, in3, no_transpose);
      break;
    case IDTX:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 0);
      fidtx16_sse2(in0, in1);
      fidtx16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fidtx32_16col(in0, in1, in2, in3);
      break;
    case V_DCT:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 0);
      fdct16_sse2(in0, in1);
      fdct16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fidtx32_16col(in0, in1, in2, in3);
      break;
    case H_DCT:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 0);
      fidtx16_sse2(in0, in1);
      fidtx16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fdct32_16col(in0, in1, in2, in3);
      break;
    case V_ADST:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 0);
      fadst16_sse2(in0, in1);
      fadst16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fidtx32_16col(in0, in1, in2, in3);
      break;
    case H_ADST:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 0);
      fidtx16_sse2(in0, in1);
      fidtx16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fhalfright32_16col(in0, in1, in2, in3, no_transpose);
      break;
    case V_FLIPADST:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 1, 0);
      fadst16_sse2(in0, in1);
      fadst16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fidtx32_16col(in0, in1, in2, in3);
      break;
    case H_FLIPADST:
      load_buffer_32x16(input, in0, in1, in2, in3, stride, 0, 1);
      fidtx16_sse2(in0, in1);
      fidtx16_sse2(in2, in3);
      round_signed_16x16(in0, in1);
      round_signed_16x16(in2, in3);
      fhalfright32_16col(in0, in1, in2, in3, no_transpose);
      break;
#endif
    default: assert(0); break;
  }
  write_buffer_32x16(output, in0, in1, in2, in3);
}

// Note:
// 32x32 hybrid fwd txfm
//  4x2 grids of 8x16 block. Each block is represented by __m128i in[16]
static INLINE void load_buffer_32x32(const int16_t *input,
                                     __m128i *in0 /*in0[32]*/,
                                     __m128i *in1 /*in1[32]*/,
                                     __m128i *in2 /*in2[32]*/,
                                     __m128i *in3 /*in3[32]*/, int stride,
                                     int flipud, int fliplr) {
  if (flipud) {
    input += 31 * stride;
    stride = -stride;
  }

  int i;
  for (i = 0; i < 32; ++i) {
    in0[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 0)), 2);
    in1[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 8)), 2);
    in2[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 16)), 2);
    in3[i] = _mm_slli_epi16(
        _mm_load_si128((const __m128i *)(input + i * stride + 24)), 2);
  }

  if (fliplr) {
    for (i = 0; i < 32; ++i) {
      __m128i tmp1 = in0[i];
      __m128i tmp2 = in1[i];
      in0[i] = mm_reverse_epi16(in3[i]);
      in1[i] = mm_reverse_epi16(in2[i]);
      in2[i] = mm_reverse_epi16(tmp2);
      in3[i] = mm_reverse_epi16(tmp1);
    }
  }
}

static INLINE void swap_16x16(__m128i *b0l /*b0l[16]*/,
                              __m128i *b0r /*b0r[16]*/,
                              __m128i *b1l /*b1l[16]*/,
                              __m128i *b1r /*b1r[16]*/) {
  int i;
  for (i = 0; i < 16; ++i) {
    __m128i tmp0 = b1l[i];
    __m128i tmp1 = b1r[i];
    b1l[i] = b0l[i];
    b1r[i] = b0r[i];
    b0l[i] = tmp0;
    b0r[i] = tmp1;
  }
}

static INLINE void fdct32(__m128i *in0, __m128i *in1, __m128i *in2,
                          __m128i *in3) {
  fdct32_8col(in0, &in0[16]);
  fdct32_8col(in1, &in1[16]);
  fdct32_8col(in2, &in2[16]);
  fdct32_8col(in3, &in3[16]);

  array_transpose_16x16(in0, in1);
  array_transpose_16x16(&in0[16], &in1[16]);
  array_transpose_16x16(in2, in3);
  array_transpose_16x16(&in2[16], &in3[16]);

  swap_16x16(&in0[16], &in1[16], in2, in3);
}

static INLINE void fhalfright32(__m128i *in0, __m128i *in1, __m128i *in2,
                                __m128i *in3) {
  fhalfright32_16col(in0, in1, &in0[16], &in1[16], no_transpose);
  fhalfright32_16col(in2, in3, &in2[16], &in3[16], no_transpose);
  swap_16x16(&in0[16], &in1[16], in2, in3);
}

#if CONFIG_EXT_TX
static INLINE void fidtx32(__m128i *in0, __m128i *in1, __m128i *in2,
                           __m128i *in3) {
  fidtx32_16col(in0, in1, &in0[16], &in1[16]);
  fidtx32_16col(in2, in3, &in2[16], &in3[16]);
  swap_16x16(&in0[16], &in1[16], in2, in3);
}
#endif

static INLINE void round_signed_32x32(__m128i *in0, __m128i *in1, __m128i *in2,
                                      __m128i *in3) {
  round_signed_16x16(in0, in1);
  round_signed_16x16(&in0[16], &in1[16]);
  round_signed_16x16(in2, in3);
  round_signed_16x16(&in2[16], &in3[16]);
}

static INLINE void write_buffer_32x32(__m128i *in0, __m128i *in1, __m128i *in2,
                                      __m128i *in3, tran_low_t *output) {
  int i;
  for (i = 0; i < 32; ++i) {
    store_output(&in0[i], output + i * 32 + 0);
    store_output(&in1[i], output + i * 32 + 8);
    store_output(&in2[i], output + i * 32 + 16);
    store_output(&in3[i], output + i * 32 + 24);
  }
}

void av1_fht32x32_sse2(const int16_t *input, tran_low_t *output, int stride,
                       TxfmParam *txfm_param) {
  __m128i in0[32], in1[32], in2[32], in3[32];
  const TX_TYPE tx_type = txfm_param->tx_type;
#if CONFIG_MRC_TX
  assert(tx_type != MRC_DCT && "No 32x32 sse2 MRC_DCT implementation");
#endif

  load_buffer_32x32(input, in0, in1, in2, in3, stride, 0, 0);
  switch (tx_type) {
    case DCT_DCT:
      fdct32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fdct32(in0, in1, in2, in3);
      break;
    case ADST_DCT:
      fhalfright32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fdct32(in0, in1, in2, in3);
      break;
    case DCT_ADST:
      fdct32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fhalfright32(in0, in1, in2, in3);
      break;
    case ADST_ADST:
      fhalfright32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fhalfright32(in0, in1, in2, in3);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      load_buffer_32x32(input, in0, in1, in2, in3, stride, 1, 0);
      fhalfright32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fdct32(in0, in1, in2, in3);
      break;
    case DCT_FLIPADST:
      load_buffer_32x32(input, in0, in1, in2, in3, stride, 0, 1);
      fdct32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fhalfright32(in0, in1, in2, in3);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_32x32(input, in0, in1, in2, in3, stride, 1, 1);
      fhalfright32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fhalfright32(in0, in1, in2, in3);
      break;
    case ADST_FLIPADST:
      load_buffer_32x32(input, in0, in1, in2, in3, stride, 0, 1);
      fhalfright32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fhalfright32(in0, in1, in2, in3);
      break;
    case FLIPADST_ADST:
      load_buffer_32x32(input, in0, in1, in2, in3, stride, 1, 0);
      fhalfright32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fhalfright32(in0, in1, in2, in3);
      break;
    case IDTX:
      fidtx32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fidtx32(in0, in1, in2, in3);
      break;
    case V_DCT:
      fdct32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fidtx32(in0, in1, in2, in3);
      break;
    case H_DCT:
      fidtx32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fdct32(in0, in1, in2, in3);
      break;
    case V_ADST:
      fhalfright32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fidtx32(in0, in1, in2, in3);
      break;
    case H_ADST:
      fidtx32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fhalfright32(in0, in1, in2, in3);
      break;
    case V_FLIPADST:
      load_buffer_32x32(input, in0, in1, in2, in3, stride, 1, 0);
      fhalfright32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fidtx32(in0, in1, in2, in3);
      break;
    case H_FLIPADST:
      load_buffer_32x32(input, in0, in1, in2, in3, stride, 0, 1);
      fidtx32(in0, in1, in2, in3);
      round_signed_32x32(in0, in1, in2, in3);
      fhalfright32(in0, in1, in2, in3);
      break;
#endif
    default: assert(0);
  }
  write_buffer_32x32(in0, in1, in2, in3, output);
}
