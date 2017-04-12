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

#ifndef AOM_DSP_X86_TXFM_COMMON_SSE2_H_
#define AOM_DSP_X86_TXFM_COMMON_SSE2_H_

#include <emmintrin.h>
#include "aom/aom_integer.h"
#include "aom_dsp/x86/synonyms.h"

#define pair_set_epi16(a, b)                                            \
  _mm_set_epi16((int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
                (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a))

#define dual_set_epi16(a, b)                                            \
  _mm_set_epi16((int16_t)(b), (int16_t)(b), (int16_t)(b), (int16_t)(b), \
                (int16_t)(a), (int16_t)(a), (int16_t)(a), (int16_t)(a))

#define octa_set_epi16(a, b, c, d, e, f, g, h)                           \
  _mm_setr_epi16((int16_t)(a), (int16_t)(b), (int16_t)(c), (int16_t)(d), \
                 (int16_t)(e), (int16_t)(f), (int16_t)(g), (int16_t)(h))

// Reverse the 8 16 bit words in __m128i
static INLINE __m128i mm_reverse_epi16(const __m128i x) {
  const __m128i a = _mm_shufflelo_epi16(x, 0x1b);
  const __m128i b = _mm_shufflehi_epi16(a, 0x1b);
  return _mm_shuffle_epi32(b, 0x4e);
}

#if CONFIG_EXT_TX
// Identity transform (both forward and inverse).
static INLINE void idtx16_8col(__m128i *in) {
  const __m128i k__zero_epi16 = _mm_set1_epi16((int16_t)0);
  const __m128i k__sqrt2_epi16 = _mm_set1_epi16((int16_t)Sqrt2);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  __m128i v0, v1, v2, v3, v4, v5, v6, v7;
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i x0, x1, x2, x3, x4, x5, x6, x7;
  __m128i y0, y1, y2, y3, y4, y5, y6, y7;

  in[0] = _mm_slli_epi16(in[0], 1);
  in[1] = _mm_slli_epi16(in[1], 1);
  in[2] = _mm_slli_epi16(in[2], 1);
  in[3] = _mm_slli_epi16(in[3], 1);
  in[4] = _mm_slli_epi16(in[4], 1);
  in[5] = _mm_slli_epi16(in[5], 1);
  in[6] = _mm_slli_epi16(in[6], 1);
  in[7] = _mm_slli_epi16(in[7], 1);
  in[8] = _mm_slli_epi16(in[8], 1);
  in[9] = _mm_slli_epi16(in[9], 1);
  in[10] = _mm_slli_epi16(in[10], 1);
  in[11] = _mm_slli_epi16(in[11], 1);
  in[12] = _mm_slli_epi16(in[12], 1);
  in[13] = _mm_slli_epi16(in[13], 1);
  in[14] = _mm_slli_epi16(in[14], 1);
  in[15] = _mm_slli_epi16(in[15], 1);

  v0 = _mm_unpacklo_epi16(in[0], k__zero_epi16);
  v1 = _mm_unpacklo_epi16(in[1], k__zero_epi16);
  v2 = _mm_unpacklo_epi16(in[2], k__zero_epi16);
  v3 = _mm_unpacklo_epi16(in[3], k__zero_epi16);
  v4 = _mm_unpacklo_epi16(in[4], k__zero_epi16);
  v5 = _mm_unpacklo_epi16(in[5], k__zero_epi16);
  v6 = _mm_unpacklo_epi16(in[6], k__zero_epi16);
  v7 = _mm_unpacklo_epi16(in[7], k__zero_epi16);

  u0 = _mm_unpacklo_epi16(in[8], k__zero_epi16);
  u1 = _mm_unpacklo_epi16(in[9], k__zero_epi16);
  u2 = _mm_unpacklo_epi16(in[10], k__zero_epi16);
  u3 = _mm_unpacklo_epi16(in[11], k__zero_epi16);
  u4 = _mm_unpacklo_epi16(in[12], k__zero_epi16);
  u5 = _mm_unpacklo_epi16(in[13], k__zero_epi16);
  u6 = _mm_unpacklo_epi16(in[14], k__zero_epi16);
  u7 = _mm_unpacklo_epi16(in[15], k__zero_epi16);

  x0 = _mm_unpackhi_epi16(in[0], k__zero_epi16);
  x1 = _mm_unpackhi_epi16(in[1], k__zero_epi16);
  x2 = _mm_unpackhi_epi16(in[2], k__zero_epi16);
  x3 = _mm_unpackhi_epi16(in[3], k__zero_epi16);
  x4 = _mm_unpackhi_epi16(in[4], k__zero_epi16);
  x5 = _mm_unpackhi_epi16(in[5], k__zero_epi16);
  x6 = _mm_unpackhi_epi16(in[6], k__zero_epi16);
  x7 = _mm_unpackhi_epi16(in[7], k__zero_epi16);

  y0 = _mm_unpackhi_epi16(in[8], k__zero_epi16);
  y1 = _mm_unpackhi_epi16(in[9], k__zero_epi16);
  y2 = _mm_unpackhi_epi16(in[10], k__zero_epi16);
  y3 = _mm_unpackhi_epi16(in[11], k__zero_epi16);
  y4 = _mm_unpackhi_epi16(in[12], k__zero_epi16);
  y5 = _mm_unpackhi_epi16(in[13], k__zero_epi16);
  y6 = _mm_unpackhi_epi16(in[14], k__zero_epi16);
  y7 = _mm_unpackhi_epi16(in[15], k__zero_epi16);

  v0 = _mm_madd_epi16(v0, k__sqrt2_epi16);
  v1 = _mm_madd_epi16(v1, k__sqrt2_epi16);
  v2 = _mm_madd_epi16(v2, k__sqrt2_epi16);
  v3 = _mm_madd_epi16(v3, k__sqrt2_epi16);
  v4 = _mm_madd_epi16(v4, k__sqrt2_epi16);
  v5 = _mm_madd_epi16(v5, k__sqrt2_epi16);
  v6 = _mm_madd_epi16(v6, k__sqrt2_epi16);
  v7 = _mm_madd_epi16(v7, k__sqrt2_epi16);

  x0 = _mm_madd_epi16(x0, k__sqrt2_epi16);
  x1 = _mm_madd_epi16(x1, k__sqrt2_epi16);
  x2 = _mm_madd_epi16(x2, k__sqrt2_epi16);
  x3 = _mm_madd_epi16(x3, k__sqrt2_epi16);
  x4 = _mm_madd_epi16(x4, k__sqrt2_epi16);
  x5 = _mm_madd_epi16(x5, k__sqrt2_epi16);
  x6 = _mm_madd_epi16(x6, k__sqrt2_epi16);
  x7 = _mm_madd_epi16(x7, k__sqrt2_epi16);

  u0 = _mm_madd_epi16(u0, k__sqrt2_epi16);
  u1 = _mm_madd_epi16(u1, k__sqrt2_epi16);
  u2 = _mm_madd_epi16(u2, k__sqrt2_epi16);
  u3 = _mm_madd_epi16(u3, k__sqrt2_epi16);
  u4 = _mm_madd_epi16(u4, k__sqrt2_epi16);
  u5 = _mm_madd_epi16(u5, k__sqrt2_epi16);
  u6 = _mm_madd_epi16(u6, k__sqrt2_epi16);
  u7 = _mm_madd_epi16(u7, k__sqrt2_epi16);

  y0 = _mm_madd_epi16(y0, k__sqrt2_epi16);
  y1 = _mm_madd_epi16(y1, k__sqrt2_epi16);
  y2 = _mm_madd_epi16(y2, k__sqrt2_epi16);
  y3 = _mm_madd_epi16(y3, k__sqrt2_epi16);
  y4 = _mm_madd_epi16(y4, k__sqrt2_epi16);
  y5 = _mm_madd_epi16(y5, k__sqrt2_epi16);
  y6 = _mm_madd_epi16(y6, k__sqrt2_epi16);
  y7 = _mm_madd_epi16(y7, k__sqrt2_epi16);

  v0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
  v1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
  v2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
  v3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);
  v4 = _mm_add_epi32(v4, k__DCT_CONST_ROUNDING);
  v5 = _mm_add_epi32(v5, k__DCT_CONST_ROUNDING);
  v6 = _mm_add_epi32(v6, k__DCT_CONST_ROUNDING);
  v7 = _mm_add_epi32(v7, k__DCT_CONST_ROUNDING);

  x0 = _mm_add_epi32(x0, k__DCT_CONST_ROUNDING);
  x1 = _mm_add_epi32(x1, k__DCT_CONST_ROUNDING);
  x2 = _mm_add_epi32(x2, k__DCT_CONST_ROUNDING);
  x3 = _mm_add_epi32(x3, k__DCT_CONST_ROUNDING);
  x4 = _mm_add_epi32(x4, k__DCT_CONST_ROUNDING);
  x5 = _mm_add_epi32(x5, k__DCT_CONST_ROUNDING);
  x6 = _mm_add_epi32(x6, k__DCT_CONST_ROUNDING);
  x7 = _mm_add_epi32(x7, k__DCT_CONST_ROUNDING);

  u0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
  u1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
  u2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
  u3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
  u4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
  u5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
  u6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
  u7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);

  y0 = _mm_add_epi32(y0, k__DCT_CONST_ROUNDING);
  y1 = _mm_add_epi32(y1, k__DCT_CONST_ROUNDING);
  y2 = _mm_add_epi32(y2, k__DCT_CONST_ROUNDING);
  y3 = _mm_add_epi32(y3, k__DCT_CONST_ROUNDING);
  y4 = _mm_add_epi32(y4, k__DCT_CONST_ROUNDING);
  y5 = _mm_add_epi32(y5, k__DCT_CONST_ROUNDING);
  y6 = _mm_add_epi32(y6, k__DCT_CONST_ROUNDING);
  y7 = _mm_add_epi32(y7, k__DCT_CONST_ROUNDING);

  v0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
  v1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
  v2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
  v3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
  v4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
  v5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
  v6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
  v7 = _mm_srai_epi32(v7, DCT_CONST_BITS);

  x0 = _mm_srai_epi32(x0, DCT_CONST_BITS);
  x1 = _mm_srai_epi32(x1, DCT_CONST_BITS);
  x2 = _mm_srai_epi32(x2, DCT_CONST_BITS);
  x3 = _mm_srai_epi32(x3, DCT_CONST_BITS);
  x4 = _mm_srai_epi32(x4, DCT_CONST_BITS);
  x5 = _mm_srai_epi32(x5, DCT_CONST_BITS);
  x6 = _mm_srai_epi32(x6, DCT_CONST_BITS);
  x7 = _mm_srai_epi32(x7, DCT_CONST_BITS);

  u0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
  u1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
  u2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
  u3 = _mm_srai_epi32(u3, DCT_CONST_BITS);
  u4 = _mm_srai_epi32(u4, DCT_CONST_BITS);
  u5 = _mm_srai_epi32(u5, DCT_CONST_BITS);
  u6 = _mm_srai_epi32(u6, DCT_CONST_BITS);
  u7 = _mm_srai_epi32(u7, DCT_CONST_BITS);

  y0 = _mm_srai_epi32(y0, DCT_CONST_BITS);
  y1 = _mm_srai_epi32(y1, DCT_CONST_BITS);
  y2 = _mm_srai_epi32(y2, DCT_CONST_BITS);
  y3 = _mm_srai_epi32(y3, DCT_CONST_BITS);
  y4 = _mm_srai_epi32(y4, DCT_CONST_BITS);
  y5 = _mm_srai_epi32(y5, DCT_CONST_BITS);
  y6 = _mm_srai_epi32(y6, DCT_CONST_BITS);
  y7 = _mm_srai_epi32(y7, DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(v0, x0);
  in[1] = _mm_packs_epi32(v1, x1);
  in[2] = _mm_packs_epi32(v2, x2);
  in[3] = _mm_packs_epi32(v3, x3);
  in[4] = _mm_packs_epi32(v4, x4);
  in[5] = _mm_packs_epi32(v5, x5);
  in[6] = _mm_packs_epi32(v6, x6);
  in[7] = _mm_packs_epi32(v7, x7);

  in[8] = _mm_packs_epi32(u0, y0);
  in[9] = _mm_packs_epi32(u1, y1);
  in[10] = _mm_packs_epi32(u2, y2);
  in[11] = _mm_packs_epi32(u3, y3);
  in[12] = _mm_packs_epi32(u4, y4);
  in[13] = _mm_packs_epi32(u5, y5);
  in[14] = _mm_packs_epi32(u6, y6);
  in[15] = _mm_packs_epi32(u7, y7);
}
#endif  // CONFIG_EXT_TX

static INLINE void scale_sqrt2_8x4(__m128i *in) {
  // Implements ROUND_POWER_OF_TWO(input * Sqrt2, DCT_CONST_BITS), for 32
  // consecutive elements.
  const __m128i v_scale_w = _mm_set1_epi16((int16_t)Sqrt2);

  const __m128i v_p0l_w = _mm_mullo_epi16(in[0], v_scale_w);
  const __m128i v_p0h_w = _mm_mulhi_epi16(in[0], v_scale_w);
  const __m128i v_p1l_w = _mm_mullo_epi16(in[1], v_scale_w);
  const __m128i v_p1h_w = _mm_mulhi_epi16(in[1], v_scale_w);
  const __m128i v_p2l_w = _mm_mullo_epi16(in[2], v_scale_w);
  const __m128i v_p2h_w = _mm_mulhi_epi16(in[2], v_scale_w);
  const __m128i v_p3l_w = _mm_mullo_epi16(in[3], v_scale_w);
  const __m128i v_p3h_w = _mm_mulhi_epi16(in[3], v_scale_w);

  const __m128i v_p0a_d = _mm_unpacklo_epi16(v_p0l_w, v_p0h_w);
  const __m128i v_p0b_d = _mm_unpackhi_epi16(v_p0l_w, v_p0h_w);
  const __m128i v_p1a_d = _mm_unpacklo_epi16(v_p1l_w, v_p1h_w);
  const __m128i v_p1b_d = _mm_unpackhi_epi16(v_p1l_w, v_p1h_w);
  const __m128i v_p2a_d = _mm_unpacklo_epi16(v_p2l_w, v_p2h_w);
  const __m128i v_p2b_d = _mm_unpackhi_epi16(v_p2l_w, v_p2h_w);
  const __m128i v_p3a_d = _mm_unpacklo_epi16(v_p3l_w, v_p3h_w);
  const __m128i v_p3b_d = _mm_unpackhi_epi16(v_p3l_w, v_p3h_w);

  in[0] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p0a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p0b_d, DCT_CONST_BITS));
  in[1] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p1a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p1b_d, DCT_CONST_BITS));
  in[2] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p2a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p2b_d, DCT_CONST_BITS));
  in[3] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p3a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p3b_d, DCT_CONST_BITS));
}

static INLINE void scale_sqrt2_8x8(__m128i *in) {
  // Implements 'ROUND_POWER_OF_TWO_SIGNED(input * Sqrt2, DCT_CONST_BITS)'
  // for each element.
  const __m128i v_scale_w = _mm_set1_epi16((int16_t)Sqrt2);

  const __m128i v_p0l_w = _mm_mullo_epi16(in[0], v_scale_w);
  const __m128i v_p0h_w = _mm_mulhi_epi16(in[0], v_scale_w);
  const __m128i v_p1l_w = _mm_mullo_epi16(in[1], v_scale_w);
  const __m128i v_p1h_w = _mm_mulhi_epi16(in[1], v_scale_w);
  const __m128i v_p2l_w = _mm_mullo_epi16(in[2], v_scale_w);
  const __m128i v_p2h_w = _mm_mulhi_epi16(in[2], v_scale_w);
  const __m128i v_p3l_w = _mm_mullo_epi16(in[3], v_scale_w);
  const __m128i v_p3h_w = _mm_mulhi_epi16(in[3], v_scale_w);
  const __m128i v_p4l_w = _mm_mullo_epi16(in[4], v_scale_w);
  const __m128i v_p4h_w = _mm_mulhi_epi16(in[4], v_scale_w);
  const __m128i v_p5l_w = _mm_mullo_epi16(in[5], v_scale_w);
  const __m128i v_p5h_w = _mm_mulhi_epi16(in[5], v_scale_w);
  const __m128i v_p6l_w = _mm_mullo_epi16(in[6], v_scale_w);
  const __m128i v_p6h_w = _mm_mulhi_epi16(in[6], v_scale_w);
  const __m128i v_p7l_w = _mm_mullo_epi16(in[7], v_scale_w);
  const __m128i v_p7h_w = _mm_mulhi_epi16(in[7], v_scale_w);

  const __m128i v_p0a_d = _mm_unpacklo_epi16(v_p0l_w, v_p0h_w);
  const __m128i v_p0b_d = _mm_unpackhi_epi16(v_p0l_w, v_p0h_w);
  const __m128i v_p1a_d = _mm_unpacklo_epi16(v_p1l_w, v_p1h_w);
  const __m128i v_p1b_d = _mm_unpackhi_epi16(v_p1l_w, v_p1h_w);
  const __m128i v_p2a_d = _mm_unpacklo_epi16(v_p2l_w, v_p2h_w);
  const __m128i v_p2b_d = _mm_unpackhi_epi16(v_p2l_w, v_p2h_w);
  const __m128i v_p3a_d = _mm_unpacklo_epi16(v_p3l_w, v_p3h_w);
  const __m128i v_p3b_d = _mm_unpackhi_epi16(v_p3l_w, v_p3h_w);
  const __m128i v_p4a_d = _mm_unpacklo_epi16(v_p4l_w, v_p4h_w);
  const __m128i v_p4b_d = _mm_unpackhi_epi16(v_p4l_w, v_p4h_w);
  const __m128i v_p5a_d = _mm_unpacklo_epi16(v_p5l_w, v_p5h_w);
  const __m128i v_p5b_d = _mm_unpackhi_epi16(v_p5l_w, v_p5h_w);
  const __m128i v_p6a_d = _mm_unpacklo_epi16(v_p6l_w, v_p6h_w);
  const __m128i v_p6b_d = _mm_unpackhi_epi16(v_p6l_w, v_p6h_w);
  const __m128i v_p7a_d = _mm_unpacklo_epi16(v_p7l_w, v_p7h_w);
  const __m128i v_p7b_d = _mm_unpackhi_epi16(v_p7l_w, v_p7h_w);

  in[0] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p0a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p0b_d, DCT_CONST_BITS));
  in[1] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p1a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p1b_d, DCT_CONST_BITS));
  in[2] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p2a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p2b_d, DCT_CONST_BITS));
  in[3] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p3a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p3b_d, DCT_CONST_BITS));
  in[4] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p4a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p4b_d, DCT_CONST_BITS));
  in[5] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p5a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p5b_d, DCT_CONST_BITS));
  in[6] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p6a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p6b_d, DCT_CONST_BITS));
  in[7] = _mm_packs_epi32(xx_roundn_epi32_unsigned(v_p7a_d, DCT_CONST_BITS),
                          xx_roundn_epi32_unsigned(v_p7b_d, DCT_CONST_BITS));
}

static INLINE void scale_sqrt2_8x16(__m128i *in) {
  scale_sqrt2_8x8(in);
  scale_sqrt2_8x8(in + 8);
}

#endif  // AOM_DSP_X86_TXFM_COMMON_SSE2_H_
