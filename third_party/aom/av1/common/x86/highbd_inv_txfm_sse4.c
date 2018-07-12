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

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/x86/highbd_txfm_utility_sse4.h"

static INLINE void load_buffer_4x4(const int32_t *coeff, __m128i *in) {
  in[0] = _mm_load_si128((const __m128i *)(coeff + 0));
  in[1] = _mm_load_si128((const __m128i *)(coeff + 4));
  in[2] = _mm_load_si128((const __m128i *)(coeff + 8));
  in[3] = _mm_load_si128((const __m128i *)(coeff + 12));
}

static void idct4x4_sse4_1(__m128i *in, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
  const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
  const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3, x, y;

  v0 = _mm_unpacklo_epi32(in[0], in[1]);
  v1 = _mm_unpackhi_epi32(in[0], in[1]);
  v2 = _mm_unpacklo_epi32(in[2], in[3]);
  v3 = _mm_unpackhi_epi32(in[2], in[3]);

  u0 = _mm_unpacklo_epi64(v0, v2);
  u1 = _mm_unpackhi_epi64(v0, v2);
  u2 = _mm_unpacklo_epi64(v1, v3);
  u3 = _mm_unpackhi_epi64(v1, v3);

  x = _mm_mullo_epi32(u0, cospi32);
  y = _mm_mullo_epi32(u2, cospi32);
  v0 = _mm_add_epi32(x, y);
  v0 = _mm_add_epi32(v0, rnding);
  v0 = _mm_srai_epi32(v0, bit);

  v1 = _mm_sub_epi32(x, y);
  v1 = _mm_add_epi32(v1, rnding);
  v1 = _mm_srai_epi32(v1, bit);

  x = _mm_mullo_epi32(u1, cospi48);
  y = _mm_mullo_epi32(u3, cospim16);
  v2 = _mm_add_epi32(x, y);
  v2 = _mm_add_epi32(v2, rnding);
  v2 = _mm_srai_epi32(v2, bit);

  x = _mm_mullo_epi32(u1, cospi16);
  y = _mm_mullo_epi32(u3, cospi48);
  v3 = _mm_add_epi32(x, y);
  v3 = _mm_add_epi32(v3, rnding);
  v3 = _mm_srai_epi32(v3, bit);

  in[0] = _mm_add_epi32(v0, v3);
  in[1] = _mm_add_epi32(v1, v2);
  in[2] = _mm_sub_epi32(v1, v2);
  in[3] = _mm_sub_epi32(v0, v3);
}

static void iadst4x4_sse4_1(__m128i *in, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
  const __m128i cospim8 = _mm_set1_epi32(-cospi[8]);
  const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
  const __m128i cospim40 = _mm_set1_epi32(-cospi[40]);
  const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
  const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  const __m128i zero = _mm_setzero_si128();
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3, x, y;

  v0 = _mm_unpacklo_epi32(in[0], in[1]);
  v1 = _mm_unpackhi_epi32(in[0], in[1]);
  v2 = _mm_unpacklo_epi32(in[2], in[3]);
  v3 = _mm_unpackhi_epi32(in[2], in[3]);

  u0 = _mm_unpacklo_epi64(v0, v2);
  u1 = _mm_unpackhi_epi64(v0, v2);
  u2 = _mm_unpacklo_epi64(v1, v3);
  u3 = _mm_unpackhi_epi64(v1, v3);

  // stage 0
  // stage 1
  u1 = _mm_sub_epi32(zero, u1);
  u3 = _mm_sub_epi32(zero, u3);

  // stage 2
  v0 = u0;
  v1 = u3;
  x = _mm_mullo_epi32(u1, cospi32);
  y = _mm_mullo_epi32(u2, cospi32);
  v2 = _mm_add_epi32(x, y);
  v2 = _mm_add_epi32(v2, rnding);
  v2 = _mm_srai_epi32(v2, bit);

  v3 = _mm_sub_epi32(x, y);
  v3 = _mm_add_epi32(v3, rnding);
  v3 = _mm_srai_epi32(v3, bit);

  // stage 3
  u0 = _mm_add_epi32(v0, v2);
  u1 = _mm_add_epi32(v1, v3);
  u2 = _mm_sub_epi32(v0, v2);
  u3 = _mm_sub_epi32(v1, v3);

  // stage 4
  x = _mm_mullo_epi32(u0, cospi8);
  y = _mm_mullo_epi32(u1, cospi56);
  in[3] = _mm_add_epi32(x, y);
  in[3] = _mm_add_epi32(in[3], rnding);
  in[3] = _mm_srai_epi32(in[3], bit);

  x = _mm_mullo_epi32(u0, cospi56);
  y = _mm_mullo_epi32(u1, cospim8);
  in[0] = _mm_add_epi32(x, y);
  in[0] = _mm_add_epi32(in[0], rnding);
  in[0] = _mm_srai_epi32(in[0], bit);

  x = _mm_mullo_epi32(u2, cospi40);
  y = _mm_mullo_epi32(u3, cospi24);
  in[1] = _mm_add_epi32(x, y);
  in[1] = _mm_add_epi32(in[1], rnding);
  in[1] = _mm_srai_epi32(in[1], bit);

  x = _mm_mullo_epi32(u2, cospi24);
  y = _mm_mullo_epi32(u3, cospim40);
  in[2] = _mm_add_epi32(x, y);
  in[2] = _mm_add_epi32(in[2], rnding);
  in[2] = _mm_srai_epi32(in[2], bit);
}

static INLINE void round_shift_4x4(__m128i *in, int shift) {
  __m128i rnding = _mm_set1_epi32(1 << (shift - 1));

  in[0] = _mm_add_epi32(in[0], rnding);
  in[1] = _mm_add_epi32(in[1], rnding);
  in[2] = _mm_add_epi32(in[2], rnding);
  in[3] = _mm_add_epi32(in[3], rnding);

  in[0] = _mm_srai_epi32(in[0], shift);
  in[1] = _mm_srai_epi32(in[1], shift);
  in[2] = _mm_srai_epi32(in[2], shift);
  in[3] = _mm_srai_epi32(in[3], shift);
}

static INLINE __m128i highbd_clamp_epi16(__m128i u, int bd) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  const __m128i max = _mm_sub_epi16(_mm_slli_epi16(one, bd), one);
  __m128i clamped, mask;

  mask = _mm_cmpgt_epi16(u, max);
  clamped = _mm_andnot_si128(mask, u);
  mask = _mm_and_si128(mask, max);
  clamped = _mm_or_si128(mask, clamped);
  mask = _mm_cmpgt_epi16(clamped, zero);
  clamped = _mm_and_si128(clamped, mask);

  return clamped;
}

static void write_buffer_4x4(__m128i *in, uint16_t *output, int stride,
                             int fliplr, int flipud, int shift, int bd) {
  const __m128i zero = _mm_setzero_si128();
  __m128i u0, u1, u2, u3;
  __m128i v0, v1, v2, v3;

  round_shift_4x4(in, shift);

  v0 = _mm_loadl_epi64((__m128i const *)(output + 0 * stride));
  v1 = _mm_loadl_epi64((__m128i const *)(output + 1 * stride));
  v2 = _mm_loadl_epi64((__m128i const *)(output + 2 * stride));
  v3 = _mm_loadl_epi64((__m128i const *)(output + 3 * stride));

  v0 = _mm_unpacklo_epi16(v0, zero);
  v1 = _mm_unpacklo_epi16(v1, zero);
  v2 = _mm_unpacklo_epi16(v2, zero);
  v3 = _mm_unpacklo_epi16(v3, zero);

  if (fliplr) {
    in[0] = _mm_shuffle_epi32(in[0], 0x1B);
    in[1] = _mm_shuffle_epi32(in[1], 0x1B);
    in[2] = _mm_shuffle_epi32(in[2], 0x1B);
    in[3] = _mm_shuffle_epi32(in[3], 0x1B);
  }

  if (flipud) {
    u0 = _mm_add_epi32(in[3], v0);
    u1 = _mm_add_epi32(in[2], v1);
    u2 = _mm_add_epi32(in[1], v2);
    u3 = _mm_add_epi32(in[0], v3);
  } else {
    u0 = _mm_add_epi32(in[0], v0);
    u1 = _mm_add_epi32(in[1], v1);
    u2 = _mm_add_epi32(in[2], v2);
    u3 = _mm_add_epi32(in[3], v3);
  }

  v0 = _mm_packus_epi32(u0, u1);
  v2 = _mm_packus_epi32(u2, u3);

  u0 = highbd_clamp_epi16(v0, bd);
  u2 = highbd_clamp_epi16(v2, bd);

  v0 = _mm_unpacklo_epi64(u0, u0);
  v1 = _mm_unpackhi_epi64(u0, u0);
  v2 = _mm_unpacklo_epi64(u2, u2);
  v3 = _mm_unpackhi_epi64(u2, u2);

  _mm_storel_epi64((__m128i *)(output + 0 * stride), v0);
  _mm_storel_epi64((__m128i *)(output + 1 * stride), v1);
  _mm_storel_epi64((__m128i *)(output + 2 * stride), v2);
  _mm_storel_epi64((__m128i *)(output + 3 * stride), v3);
}

void av1_inv_txfm2d_add_4x4_sse4_1(const int32_t *coeff, uint16_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  __m128i in[4];
  const TXFM_1D_CFG *row_cfg = NULL;
  const TXFM_1D_CFG *col_cfg = NULL;

  switch (tx_type) {
    case DCT_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_4;
      col_cfg = &inv_txfm_1d_col_cfg_dct_4;
      load_buffer_4x4(coeff, in);
      idct4x4_sse4_1(in, row_cfg->cos_bit[2]);
      idct4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case ADST_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_4;
      col_cfg = &inv_txfm_1d_col_cfg_adst_4;
      load_buffer_4x4(coeff, in);
      idct4x4_sse4_1(in, row_cfg->cos_bit[2]);
      iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case DCT_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_4;
      col_cfg = &inv_txfm_1d_col_cfg_dct_4;
      load_buffer_4x4(coeff, in);
      iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
      idct4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case ADST_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_4;
      col_cfg = &inv_txfm_1d_col_cfg_adst_4;
      load_buffer_4x4(coeff, in);
      iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
      iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_4;
      col_cfg = &inv_txfm_1d_col_cfg_adst_4;
      load_buffer_4x4(coeff, in);
      idct4x4_sse4_1(in, row_cfg->cos_bit[2]);
      iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
      break;
    case DCT_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_4;
      col_cfg = &inv_txfm_1d_col_cfg_dct_4;
      load_buffer_4x4(coeff, in);
      iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
      idct4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
      break;
    case FLIPADST_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_4;
      col_cfg = &inv_txfm_1d_col_cfg_adst_4;
      load_buffer_4x4(coeff, in);
      iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
      iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 1, 1, -row_cfg->shift[1], bd);
      break;
    case ADST_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_4;
      col_cfg = &inv_txfm_1d_col_cfg_adst_4;
      load_buffer_4x4(coeff, in);
      iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
      iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
      break;
    case FLIPADST_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_4;
      col_cfg = &inv_txfm_1d_col_cfg_adst_4;
      load_buffer_4x4(coeff, in);
      iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
      iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
      write_buffer_4x4(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

// 8x8
static void load_buffer_8x8(const int32_t *coeff, __m128i *in) {
  in[0] = _mm_load_si128((const __m128i *)(coeff + 0));
  in[1] = _mm_load_si128((const __m128i *)(coeff + 4));
  in[2] = _mm_load_si128((const __m128i *)(coeff + 8));
  in[3] = _mm_load_si128((const __m128i *)(coeff + 12));
  in[4] = _mm_load_si128((const __m128i *)(coeff + 16));
  in[5] = _mm_load_si128((const __m128i *)(coeff + 20));
  in[6] = _mm_load_si128((const __m128i *)(coeff + 24));
  in[7] = _mm_load_si128((const __m128i *)(coeff + 28));
  in[8] = _mm_load_si128((const __m128i *)(coeff + 32));
  in[9] = _mm_load_si128((const __m128i *)(coeff + 36));
  in[10] = _mm_load_si128((const __m128i *)(coeff + 40));
  in[11] = _mm_load_si128((const __m128i *)(coeff + 44));
  in[12] = _mm_load_si128((const __m128i *)(coeff + 48));
  in[13] = _mm_load_si128((const __m128i *)(coeff + 52));
  in[14] = _mm_load_si128((const __m128i *)(coeff + 56));
  in[15] = _mm_load_si128((const __m128i *)(coeff + 60));
}

static void idct8x8_sse4_1(__m128i *in, __m128i *out, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
  const __m128i cospim8 = _mm_set1_epi32(-cospi[8]);
  const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
  const __m128i cospim40 = _mm_set1_epi32(-cospi[40]);
  const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
  const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
  const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
  const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
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
    // stage 2
    u0 = in[0 * 2 + col];
    u1 = in[4 * 2 + col];
    u2 = in[2 * 2 + col];
    u3 = in[6 * 2 + col];

    x = _mm_mullo_epi32(in[1 * 2 + col], cospi56);
    y = _mm_mullo_epi32(in[7 * 2 + col], cospim8);
    u4 = _mm_add_epi32(x, y);
    u4 = _mm_add_epi32(u4, rnding);
    u4 = _mm_srai_epi32(u4, bit);

    x = _mm_mullo_epi32(in[1 * 2 + col], cospi8);
    y = _mm_mullo_epi32(in[7 * 2 + col], cospi56);
    u7 = _mm_add_epi32(x, y);
    u7 = _mm_add_epi32(u7, rnding);
    u7 = _mm_srai_epi32(u7, bit);

    x = _mm_mullo_epi32(in[5 * 2 + col], cospi24);
    y = _mm_mullo_epi32(in[3 * 2 + col], cospim40);
    u5 = _mm_add_epi32(x, y);
    u5 = _mm_add_epi32(u5, rnding);
    u5 = _mm_srai_epi32(u5, bit);

    x = _mm_mullo_epi32(in[5 * 2 + col], cospi40);
    y = _mm_mullo_epi32(in[3 * 2 + col], cospi24);
    u6 = _mm_add_epi32(x, y);
    u6 = _mm_add_epi32(u6, rnding);
    u6 = _mm_srai_epi32(u6, bit);

    // stage 3
    x = _mm_mullo_epi32(u0, cospi32);
    y = _mm_mullo_epi32(u1, cospi32);
    v0 = _mm_add_epi32(x, y);
    v0 = _mm_add_epi32(v0, rnding);
    v0 = _mm_srai_epi32(v0, bit);

    v1 = _mm_sub_epi32(x, y);
    v1 = _mm_add_epi32(v1, rnding);
    v1 = _mm_srai_epi32(v1, bit);

    x = _mm_mullo_epi32(u2, cospi48);
    y = _mm_mullo_epi32(u3, cospim16);
    v2 = _mm_add_epi32(x, y);
    v2 = _mm_add_epi32(v2, rnding);
    v2 = _mm_srai_epi32(v2, bit);

    x = _mm_mullo_epi32(u2, cospi16);
    y = _mm_mullo_epi32(u3, cospi48);
    v3 = _mm_add_epi32(x, y);
    v3 = _mm_add_epi32(v3, rnding);
    v3 = _mm_srai_epi32(v3, bit);

    v4 = _mm_add_epi32(u4, u5);
    v5 = _mm_sub_epi32(u4, u5);
    v6 = _mm_sub_epi32(u7, u6);
    v7 = _mm_add_epi32(u6, u7);

    // stage 4
    u0 = _mm_add_epi32(v0, v3);
    u1 = _mm_add_epi32(v1, v2);
    u2 = _mm_sub_epi32(v1, v2);
    u3 = _mm_sub_epi32(v0, v3);
    u4 = v4;
    u7 = v7;

    x = _mm_mullo_epi32(v5, cospi32);
    y = _mm_mullo_epi32(v6, cospi32);
    u6 = _mm_add_epi32(y, x);
    u6 = _mm_add_epi32(u6, rnding);
    u6 = _mm_srai_epi32(u6, bit);

    u5 = _mm_sub_epi32(y, x);
    u5 = _mm_add_epi32(u5, rnding);
    u5 = _mm_srai_epi32(u5, bit);

    // stage 5
    out[0 * 2 + col] = _mm_add_epi32(u0, u7);
    out[1 * 2 + col] = _mm_add_epi32(u1, u6);
    out[2 * 2 + col] = _mm_add_epi32(u2, u5);
    out[3 * 2 + col] = _mm_add_epi32(u3, u4);
    out[4 * 2 + col] = _mm_sub_epi32(u3, u4);
    out[5 * 2 + col] = _mm_sub_epi32(u2, u5);
    out[6 * 2 + col] = _mm_sub_epi32(u1, u6);
    out[7 * 2 + col] = _mm_sub_epi32(u0, u7);
  }
}

static void iadst8x8_sse4_1(__m128i *in, __m128i *out, int bit) {
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

static void round_shift_8x8(__m128i *in, int shift) {
  round_shift_4x4(&in[0], shift);
  round_shift_4x4(&in[4], shift);
  round_shift_4x4(&in[8], shift);
  round_shift_4x4(&in[12], shift);
}

static __m128i get_recon_8x8(const __m128i pred, __m128i res_lo, __m128i res_hi,
                             int fliplr, int bd) {
  __m128i x0, x1;
  const __m128i zero = _mm_setzero_si128();

  x0 = _mm_unpacklo_epi16(pred, zero);
  x1 = _mm_unpackhi_epi16(pred, zero);

  if (fliplr) {
    res_lo = _mm_shuffle_epi32(res_lo, 0x1B);
    res_hi = _mm_shuffle_epi32(res_hi, 0x1B);
    x0 = _mm_add_epi32(res_hi, x0);
    x1 = _mm_add_epi32(res_lo, x1);

  } else {
    x0 = _mm_add_epi32(res_lo, x0);
    x1 = _mm_add_epi32(res_hi, x1);
  }

  x0 = _mm_packus_epi32(x0, x1);
  return highbd_clamp_epi16(x0, bd);
}

static void write_buffer_8x8(__m128i *in, uint16_t *output, int stride,
                             int fliplr, int flipud, int shift, int bd) {
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;

  round_shift_8x8(in, shift);

  v0 = _mm_load_si128((__m128i const *)(output + 0 * stride));
  v1 = _mm_load_si128((__m128i const *)(output + 1 * stride));
  v2 = _mm_load_si128((__m128i const *)(output + 2 * stride));
  v3 = _mm_load_si128((__m128i const *)(output + 3 * stride));
  v4 = _mm_load_si128((__m128i const *)(output + 4 * stride));
  v5 = _mm_load_si128((__m128i const *)(output + 5 * stride));
  v6 = _mm_load_si128((__m128i const *)(output + 6 * stride));
  v7 = _mm_load_si128((__m128i const *)(output + 7 * stride));

  if (flipud) {
    u0 = get_recon_8x8(v0, in[14], in[15], fliplr, bd);
    u1 = get_recon_8x8(v1, in[12], in[13], fliplr, bd);
    u2 = get_recon_8x8(v2, in[10], in[11], fliplr, bd);
    u3 = get_recon_8x8(v3, in[8], in[9], fliplr, bd);
    u4 = get_recon_8x8(v4, in[6], in[7], fliplr, bd);
    u5 = get_recon_8x8(v5, in[4], in[5], fliplr, bd);
    u6 = get_recon_8x8(v6, in[2], in[3], fliplr, bd);
    u7 = get_recon_8x8(v7, in[0], in[1], fliplr, bd);
  } else {
    u0 = get_recon_8x8(v0, in[0], in[1], fliplr, bd);
    u1 = get_recon_8x8(v1, in[2], in[3], fliplr, bd);
    u2 = get_recon_8x8(v2, in[4], in[5], fliplr, bd);
    u3 = get_recon_8x8(v3, in[6], in[7], fliplr, bd);
    u4 = get_recon_8x8(v4, in[8], in[9], fliplr, bd);
    u5 = get_recon_8x8(v5, in[10], in[11], fliplr, bd);
    u6 = get_recon_8x8(v6, in[12], in[13], fliplr, bd);
    u7 = get_recon_8x8(v7, in[14], in[15], fliplr, bd);
  }

  _mm_store_si128((__m128i *)(output + 0 * stride), u0);
  _mm_store_si128((__m128i *)(output + 1 * stride), u1);
  _mm_store_si128((__m128i *)(output + 2 * stride), u2);
  _mm_store_si128((__m128i *)(output + 3 * stride), u3);
  _mm_store_si128((__m128i *)(output + 4 * stride), u4);
  _mm_store_si128((__m128i *)(output + 5 * stride), u5);
  _mm_store_si128((__m128i *)(output + 6 * stride), u6);
  _mm_store_si128((__m128i *)(output + 7 * stride), u7);
}

void av1_inv_txfm2d_add_8x8_sse4_1(const int32_t *coeff, uint16_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  __m128i in[16], out[16];
  const TXFM_1D_CFG *row_cfg = NULL;
  const TXFM_1D_CFG *col_cfg = NULL;

  switch (tx_type) {
    case DCT_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_8;
      col_cfg = &inv_txfm_1d_col_cfg_dct_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      idct8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      idct8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case DCT_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_8;
      col_cfg = &inv_txfm_1d_col_cfg_dct_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      idct8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case ADST_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_8;
      col_cfg = &inv_txfm_1d_col_cfg_adst_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      idct8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case ADST_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_8;
      col_cfg = &inv_txfm_1d_col_cfg_adst_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_8;
      col_cfg = &inv_txfm_1d_col_cfg_adst_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      idct8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
      break;
    case DCT_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_8;
      col_cfg = &inv_txfm_1d_col_cfg_dct_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      idct8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
      break;
    case ADST_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_8;
      col_cfg = &inv_txfm_1d_col_cfg_adst_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
      break;
    case FLIPADST_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_8;
      col_cfg = &inv_txfm_1d_col_cfg_adst_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 1, 1, -row_cfg->shift[1], bd);
      break;
    case FLIPADST_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_8;
      col_cfg = &inv_txfm_1d_col_cfg_adst_8;
      load_buffer_8x8(coeff, in);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
      transpose_8x8(in, out);
      iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_8x8(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

// 16x16
static void load_buffer_16x16(const int32_t *coeff, __m128i *in) {
  int i;
  for (i = 0; i < 64; ++i) {
    in[i] = _mm_load_si128((const __m128i *)(coeff + (i << 2)));
  }
}

static void assign_8x8_input_from_16x16(const __m128i *in, __m128i *in8x8,
                                        int col) {
  int i;
  for (i = 0; i < 16; i += 2) {
    in8x8[i] = in[col];
    in8x8[i + 1] = in[col + 1];
    col += 4;
  }
}

static void swap_addr(uint16_t **output1, uint16_t **output2) {
  uint16_t *tmp;
  tmp = *output1;
  *output1 = *output2;
  *output2 = tmp;
}

static void write_buffer_16x16(__m128i *in, uint16_t *output, int stride,
                               int fliplr, int flipud, int shift, int bd) {
  __m128i in8x8[16];
  uint16_t *leftUp = &output[0];
  uint16_t *rightUp = &output[8];
  uint16_t *leftDown = &output[8 * stride];
  uint16_t *rightDown = &output[8 * stride + 8];

  if (fliplr) {
    swap_addr(&leftUp, &rightUp);
    swap_addr(&leftDown, &rightDown);
  }

  if (flipud) {
    swap_addr(&leftUp, &leftDown);
    swap_addr(&rightUp, &rightDown);
  }

  // Left-up quarter
  assign_8x8_input_from_16x16(in, in8x8, 0);
  write_buffer_8x8(in8x8, leftUp, stride, fliplr, flipud, shift, bd);

  // Right-up quarter
  assign_8x8_input_from_16x16(in, in8x8, 2);
  write_buffer_8x8(in8x8, rightUp, stride, fliplr, flipud, shift, bd);

  // Left-down quarter
  assign_8x8_input_from_16x16(in, in8x8, 32);
  write_buffer_8x8(in8x8, leftDown, stride, fliplr, flipud, shift, bd);

  // Right-down quarter
  assign_8x8_input_from_16x16(in, in8x8, 34);
  write_buffer_8x8(in8x8, rightDown, stride, fliplr, flipud, shift, bd);
}

static void idct16x16_sse4_1(__m128i *in, __m128i *out, int bit) {
  const int32_t *cospi = cospi_arr(bit);
  const __m128i cospi60 = _mm_set1_epi32(cospi[60]);
  const __m128i cospim4 = _mm_set1_epi32(-cospi[4]);
  const __m128i cospi28 = _mm_set1_epi32(cospi[28]);
  const __m128i cospim36 = _mm_set1_epi32(-cospi[36]);
  const __m128i cospi44 = _mm_set1_epi32(cospi[44]);
  const __m128i cospi20 = _mm_set1_epi32(cospi[20]);
  const __m128i cospim20 = _mm_set1_epi32(-cospi[20]);
  const __m128i cospi12 = _mm_set1_epi32(cospi[12]);
  const __m128i cospim52 = _mm_set1_epi32(-cospi[52]);
  const __m128i cospi52 = _mm_set1_epi32(cospi[52]);
  const __m128i cospi36 = _mm_set1_epi32(cospi[36]);
  const __m128i cospi4 = _mm_set1_epi32(cospi[4]);
  const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
  const __m128i cospim8 = _mm_set1_epi32(-cospi[8]);
  const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
  const __m128i cospim40 = _mm_set1_epi32(-cospi[40]);
  const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
  const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
  const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
  const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
  const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
  const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
  const __m128i cospim48 = _mm_set1_epi32(-cospi[48]);
  const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
  __m128i u[16], v[16], x, y;
  int col;

  for (col = 0; col < 4; ++col) {
    // stage 0
    // stage 1
    u[0] = in[0 * 4 + col];
    u[1] = in[8 * 4 + col];
    u[2] = in[4 * 4 + col];
    u[3] = in[12 * 4 + col];
    u[4] = in[2 * 4 + col];
    u[5] = in[10 * 4 + col];
    u[6] = in[6 * 4 + col];
    u[7] = in[14 * 4 + col];
    u[8] = in[1 * 4 + col];
    u[9] = in[9 * 4 + col];
    u[10] = in[5 * 4 + col];
    u[11] = in[13 * 4 + col];
    u[12] = in[3 * 4 + col];
    u[13] = in[11 * 4 + col];
    u[14] = in[7 * 4 + col];
    u[15] = in[15 * 4 + col];

    // stage 2
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = u[4];
    v[5] = u[5];
    v[6] = u[6];
    v[7] = u[7];

    v[8] = half_btf_sse4_1(&cospi60, &u[8], &cospim4, &u[15], &rnding, bit);
    v[9] = half_btf_sse4_1(&cospi28, &u[9], &cospim36, &u[14], &rnding, bit);
    v[10] = half_btf_sse4_1(&cospi44, &u[10], &cospim20, &u[13], &rnding, bit);
    v[11] = half_btf_sse4_1(&cospi12, &u[11], &cospim52, &u[12], &rnding, bit);
    v[12] = half_btf_sse4_1(&cospi52, &u[11], &cospi12, &u[12], &rnding, bit);
    v[13] = half_btf_sse4_1(&cospi20, &u[10], &cospi44, &u[13], &rnding, bit);
    v[14] = half_btf_sse4_1(&cospi36, &u[9], &cospi28, &u[14], &rnding, bit);
    v[15] = half_btf_sse4_1(&cospi4, &u[8], &cospi60, &u[15], &rnding, bit);

    // stage 3
    u[0] = v[0];
    u[1] = v[1];
    u[2] = v[2];
    u[3] = v[3];
    u[4] = half_btf_sse4_1(&cospi56, &v[4], &cospim8, &v[7], &rnding, bit);
    u[5] = half_btf_sse4_1(&cospi24, &v[5], &cospim40, &v[6], &rnding, bit);
    u[6] = half_btf_sse4_1(&cospi40, &v[5], &cospi24, &v[6], &rnding, bit);
    u[7] = half_btf_sse4_1(&cospi8, &v[4], &cospi56, &v[7], &rnding, bit);
    u[8] = _mm_add_epi32(v[8], v[9]);
    u[9] = _mm_sub_epi32(v[8], v[9]);
    u[10] = _mm_sub_epi32(v[11], v[10]);
    u[11] = _mm_add_epi32(v[10], v[11]);
    u[12] = _mm_add_epi32(v[12], v[13]);
    u[13] = _mm_sub_epi32(v[12], v[13]);
    u[14] = _mm_sub_epi32(v[15], v[14]);
    u[15] = _mm_add_epi32(v[14], v[15]);

    // stage 4
    x = _mm_mullo_epi32(u[0], cospi32);
    y = _mm_mullo_epi32(u[1], cospi32);
    v[0] = _mm_add_epi32(x, y);
    v[0] = _mm_add_epi32(v[0], rnding);
    v[0] = _mm_srai_epi32(v[0], bit);

    v[1] = _mm_sub_epi32(x, y);
    v[1] = _mm_add_epi32(v[1], rnding);
    v[1] = _mm_srai_epi32(v[1], bit);

    v[2] = half_btf_sse4_1(&cospi48, &u[2], &cospim16, &u[3], &rnding, bit);
    v[3] = half_btf_sse4_1(&cospi16, &u[2], &cospi48, &u[3], &rnding, bit);
    v[4] = _mm_add_epi32(u[4], u[5]);
    v[5] = _mm_sub_epi32(u[4], u[5]);
    v[6] = _mm_sub_epi32(u[7], u[6]);
    v[7] = _mm_add_epi32(u[6], u[7]);
    v[8] = u[8];
    v[9] = half_btf_sse4_1(&cospim16, &u[9], &cospi48, &u[14], &rnding, bit);
    v[10] = half_btf_sse4_1(&cospim48, &u[10], &cospim16, &u[13], &rnding, bit);
    v[11] = u[11];
    v[12] = u[12];
    v[13] = half_btf_sse4_1(&cospim16, &u[10], &cospi48, &u[13], &rnding, bit);
    v[14] = half_btf_sse4_1(&cospi48, &u[9], &cospi16, &u[14], &rnding, bit);
    v[15] = u[15];

    // stage 5
    u[0] = _mm_add_epi32(v[0], v[3]);
    u[1] = _mm_add_epi32(v[1], v[2]);
    u[2] = _mm_sub_epi32(v[1], v[2]);
    u[3] = _mm_sub_epi32(v[0], v[3]);
    u[4] = v[4];

    x = _mm_mullo_epi32(v[5], cospi32);
    y = _mm_mullo_epi32(v[6], cospi32);
    u[5] = _mm_sub_epi32(y, x);
    u[5] = _mm_add_epi32(u[5], rnding);
    u[5] = _mm_srai_epi32(u[5], bit);

    u[6] = _mm_add_epi32(y, x);
    u[6] = _mm_add_epi32(u[6], rnding);
    u[6] = _mm_srai_epi32(u[6], bit);

    u[7] = v[7];
    u[8] = _mm_add_epi32(v[8], v[11]);
    u[9] = _mm_add_epi32(v[9], v[10]);
    u[10] = _mm_sub_epi32(v[9], v[10]);
    u[11] = _mm_sub_epi32(v[8], v[11]);
    u[12] = _mm_sub_epi32(v[15], v[12]);
    u[13] = _mm_sub_epi32(v[14], v[13]);
    u[14] = _mm_add_epi32(v[13], v[14]);
    u[15] = _mm_add_epi32(v[12], v[15]);

    // stage 6
    v[0] = _mm_add_epi32(u[0], u[7]);
    v[1] = _mm_add_epi32(u[1], u[6]);
    v[2] = _mm_add_epi32(u[2], u[5]);
    v[3] = _mm_add_epi32(u[3], u[4]);
    v[4] = _mm_sub_epi32(u[3], u[4]);
    v[5] = _mm_sub_epi32(u[2], u[5]);
    v[6] = _mm_sub_epi32(u[1], u[6]);
    v[7] = _mm_sub_epi32(u[0], u[7]);
    v[8] = u[8];
    v[9] = u[9];

    x = _mm_mullo_epi32(u[10], cospi32);
    y = _mm_mullo_epi32(u[13], cospi32);
    v[10] = _mm_sub_epi32(y, x);
    v[10] = _mm_add_epi32(v[10], rnding);
    v[10] = _mm_srai_epi32(v[10], bit);

    v[13] = _mm_add_epi32(x, y);
    v[13] = _mm_add_epi32(v[13], rnding);
    v[13] = _mm_srai_epi32(v[13], bit);

    x = _mm_mullo_epi32(u[11], cospi32);
    y = _mm_mullo_epi32(u[12], cospi32);
    v[11] = _mm_sub_epi32(y, x);
    v[11] = _mm_add_epi32(v[11], rnding);
    v[11] = _mm_srai_epi32(v[11], bit);

    v[12] = _mm_add_epi32(x, y);
    v[12] = _mm_add_epi32(v[12], rnding);
    v[12] = _mm_srai_epi32(v[12], bit);

    v[14] = u[14];
    v[15] = u[15];

    // stage 7
    out[0 * 4 + col] = _mm_add_epi32(v[0], v[15]);
    out[1 * 4 + col] = _mm_add_epi32(v[1], v[14]);
    out[2 * 4 + col] = _mm_add_epi32(v[2], v[13]);
    out[3 * 4 + col] = _mm_add_epi32(v[3], v[12]);
    out[4 * 4 + col] = _mm_add_epi32(v[4], v[11]);
    out[5 * 4 + col] = _mm_add_epi32(v[5], v[10]);
    out[6 * 4 + col] = _mm_add_epi32(v[6], v[9]);
    out[7 * 4 + col] = _mm_add_epi32(v[7], v[8]);
    out[8 * 4 + col] = _mm_sub_epi32(v[7], v[8]);
    out[9 * 4 + col] = _mm_sub_epi32(v[6], v[9]);
    out[10 * 4 + col] = _mm_sub_epi32(v[5], v[10]);
    out[11 * 4 + col] = _mm_sub_epi32(v[4], v[11]);
    out[12 * 4 + col] = _mm_sub_epi32(v[3], v[12]);
    out[13 * 4 + col] = _mm_sub_epi32(v[2], v[13]);
    out[14 * 4 + col] = _mm_sub_epi32(v[1], v[14]);
    out[15 * 4 + col] = _mm_sub_epi32(v[0], v[15]);
  }
}

static void iadst16x16_sse4_1(__m128i *in, __m128i *out, int bit) {
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

static void round_shift_16x16(__m128i *in, int shift) {
  round_shift_8x8(&in[0], shift);
  round_shift_8x8(&in[16], shift);
  round_shift_8x8(&in[32], shift);
  round_shift_8x8(&in[48], shift);
}

void av1_inv_txfm2d_add_16x16_sse4_1(const int32_t *coeff, uint16_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  __m128i in[64], out[64];
  const TXFM_1D_CFG *row_cfg = NULL;
  const TXFM_1D_CFG *col_cfg = NULL;

  switch (tx_type) {
    case DCT_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_16;
      col_cfg = &inv_txfm_1d_col_cfg_dct_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      idct16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      idct16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case DCT_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_16;
      col_cfg = &inv_txfm_1d_col_cfg_dct_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      idct16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case ADST_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_16;
      col_cfg = &inv_txfm_1d_col_cfg_adst_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      idct16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
    case ADST_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_16;
      col_cfg = &inv_txfm_1d_col_cfg_adst_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 0, 0, -row_cfg->shift[1], bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
      row_cfg = &inv_txfm_1d_row_cfg_dct_16;
      col_cfg = &inv_txfm_1d_col_cfg_adst_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      idct16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
      break;
    case DCT_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_16;
      col_cfg = &inv_txfm_1d_col_cfg_dct_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      idct16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
      break;
    case ADST_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_16;
      col_cfg = &inv_txfm_1d_col_cfg_adst_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
      break;
    case FLIPADST_FLIPADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_16;
      col_cfg = &inv_txfm_1d_col_cfg_adst_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 1, 1, -row_cfg->shift[1], bd);
      break;
    case FLIPADST_ADST:
      row_cfg = &inv_txfm_1d_row_cfg_adst_16;
      col_cfg = &inv_txfm_1d_col_cfg_adst_16;
      load_buffer_16x16(coeff, in);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
      round_shift_16x16(in, -row_cfg->shift[0]);
      transpose_16x16(in, out);
      iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
      write_buffer_16x16(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
      break;
#endif
    default: assert(0);
  }
}
