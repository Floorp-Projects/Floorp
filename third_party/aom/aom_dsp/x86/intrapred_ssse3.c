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

#include <tmmintrin.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/intrapred_common.h"

// -----------------------------------------------------------------------------
// TM_PRED

// Return 8 16-bit pixels in one row
static INLINE __m128i paeth_8x1_pred(const __m128i *left, const __m128i *top,
                                     const __m128i *topleft) {
  const __m128i base = _mm_sub_epi16(_mm_add_epi16(*top, *left), *topleft);

  __m128i pl = _mm_abs_epi16(_mm_sub_epi16(base, *left));
  __m128i pt = _mm_abs_epi16(_mm_sub_epi16(base, *top));
  __m128i ptl = _mm_abs_epi16(_mm_sub_epi16(base, *topleft));

  __m128i mask1 = _mm_cmpgt_epi16(pl, pt);
  mask1 = _mm_or_si128(mask1, _mm_cmpgt_epi16(pl, ptl));
  __m128i mask2 = _mm_cmpgt_epi16(pt, ptl);

  pl = _mm_andnot_si128(mask1, *left);

  ptl = _mm_and_si128(mask2, *topleft);
  pt = _mm_andnot_si128(mask2, *top);
  pt = _mm_or_si128(pt, ptl);
  pt = _mm_and_si128(mask1, pt);

  return _mm_or_si128(pl, pt);
}

void aom_paeth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(uint32_t *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(uint32_t *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

// Return 16 8-bit pixels in one row
static INLINE __m128i paeth_16x1_pred(const __m128i *left, const __m128i *top0,
                                      const __m128i *top1,
                                      const __m128i *topleft) {
  const __m128i p0 = paeth_8x1_pred(left, top0, topleft);
  const __m128i p1 = paeth_8x1_pred(left, top1, topleft);
  return _mm_packus_epi16(p0, p1);
}

void aom_paeth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  l = _mm_load_si128((const __m128i *)(left + 16));
  rep = _mm_set1_epi16(0x8000);
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16(0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  rep = _mm_set1_epi16(0x8000);
  l = _mm_load_si128((const __m128i *)(left + 16));
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

// -----------------------------------------------------------------------------
// SMOOTH_PRED

// pixels[0]: above and below_pred interleave vector
// pixels[1]: left vector
// pixels[2]: right_pred vector
static INLINE void load_pixel_w4(const uint8_t *above, const uint8_t *left,
                                 int height, __m128i *pixels) {
  __m128i d = _mm_loadl_epi64((const __m128i *)above);
  pixels[2] = _mm_set1_epi16((uint16_t)above[3]);
  pixels[1] = _mm_loadl_epi64((const __m128i *)left);

  const __m128i bp = _mm_set1_epi16((uint16_t)left[height - 1]);
  const __m128i zero = _mm_setzero_si128();
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
}

// weights[0]: weights_h vector
// weights[1]: scale - weights_h vecotr
// weights[2]: weights_w and scale - weights_w interleave vector
static INLINE void load_weight_w4(const uint8_t *weight_array, int height,
                                  __m128i *weights) {
  __m128i t = _mm_loadu_si128((const __m128i *)&weight_array[4]);
  const __m128i zero = _mm_setzero_si128();

  weights[0] = _mm_unpacklo_epi8(t, zero);
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << sm_weight_log2_scale));
  weights[1] = _mm_sub_epi16(d, weights[0]);
  weights[2] = _mm_unpacklo_epi16(weights[0], weights[1]);

  if (height == 8) {
    t = _mm_srli_si128(t, 4);
    weights[0] = _mm_unpacklo_epi8(t, zero);
    weights[1] = _mm_sub_epi16(d, weights[0]);
  }
}

static INLINE void smooth_pred_4xh(const __m128i *pixel, const __m128i *weight,
                                   int h, uint8_t *dst, ptrdiff_t stride) {
  const __m128i round = _mm_set1_epi32((1 << sm_weight_log2_scale));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set1_epi32(0xc080400);
  __m128i rep = _mm_set1_epi16(0x8000);
  __m128i d = _mm_set1_epi16(0x100);

  int i;
  for (i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(weight[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(weight[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s = _mm_madd_epi16(pixel[0], wh_sc);

    __m128i b = _mm_shuffle_epi8(pixel[1], rep);
    b = _mm_unpacklo_epi16(b, pixel[2]);
    __m128i sum = _mm_madd_epi16(b, weight[2]);

    sum = _mm_add_epi32(s, sum);
    sum = _mm_add_epi32(sum, round);
    sum = _mm_srai_epi32(sum, 1 + sm_weight_log2_scale);

    sum = _mm_shuffle_epi8(sum, gat);
    *(uint32_t *)dst = _mm_cvtsi128_si32(sum);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[3];
  load_pixel_w4(above, left, 4, pixels);

  __m128i weights[3];
  load_weight_w4(sm_weight_arrays, 4, weights);

  smooth_pred_4xh(pixels, weights, 4, dst, stride);
}

void aom_smooth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[3];
  load_pixel_w4(above, left, 8, pixels);

  __m128i weights[3];
  load_weight_w4(sm_weight_arrays, 8, weights);

  smooth_pred_4xh(pixels, weights, 8, dst, stride);
}

// pixels[0]: above and below_pred interleave vector, first half
// pixels[1]: above and below_pred interleave vector, second half
// pixels[2]: left vector
// pixels[3]: right_pred vector
static INLINE void load_pixel_w8(const uint8_t *above, const uint8_t *left,
                                 int height, __m128i *pixels) {
  __m128i d = _mm_loadl_epi64((const __m128i *)above);
  pixels[3] = _mm_set1_epi16((uint16_t)above[7]);
  pixels[2] = _mm_load_si128((const __m128i *)left);
  const __m128i bp = _mm_set1_epi16((uint16_t)left[height - 1]);
  const __m128i zero = _mm_setzero_si128();

  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
  pixels[1] = _mm_unpackhi_epi16(d, bp);
}

// weight_h[0]: weight_h vector
// weight_h[1]: scale - weight_h vector
// weight_h[2]: same as [0], second half for height = 16 only
// weight_h[3]: same as [1], second half for height = 16 only
// weight_w[0]: weights_w and scale - weights_w interleave vector, first half
// weight_w[1]: weights_w and scale - weights_w interleave vector, second half
static INLINE void load_weight_w8(const uint8_t *weight_array, int height,
                                  __m128i *weight_h, __m128i *weight_w) {
  const __m128i zero = _mm_setzero_si128();
  const int we_offset = height < 8 ? 4 : 8;
  __m128i we = _mm_loadu_si128((const __m128i *)&weight_array[we_offset]);
  weight_h[0] = _mm_unpacklo_epi8(we, zero);

  const __m128i d = _mm_set1_epi16((uint16_t)(1 << sm_weight_log2_scale));
  weight_h[1] = _mm_sub_epi16(d, weight_h[0]);

  if (height == 4) {
    we = _mm_srli_si128(we, 4);
    __m128i tmp1 = _mm_unpacklo_epi8(we, zero);
    __m128i tmp2 = _mm_sub_epi16(d, tmp1);
    weight_w[0] = _mm_unpacklo_epi16(tmp1, tmp2);
    weight_w[1] = _mm_unpackhi_epi16(tmp1, tmp2);
  } else {
    weight_w[0] = _mm_unpacklo_epi16(weight_h[0], weight_h[1]);
    weight_w[1] = _mm_unpackhi_epi16(weight_h[0], weight_h[1]);
  }

  if (height == 16) {
    we = _mm_loadu_si128((const __m128i *)&weight_array[16]);
    weight_h[0] = _mm_unpacklo_epi8(we, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(we, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
  }
}

static INLINE void smooth_pred_8xh(const __m128i *pixels, const __m128i *wh,
                                   const __m128i *ww, int h, uint8_t *dst,
                                   ptrdiff_t stride, int second_half) {
  const __m128i round = _mm_set1_epi32((1 << sm_weight_log2_scale));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);

  __m128i rep = second_half ? _mm_set1_epi16(0x8008) : _mm_set1_epi16(0x8000);
  __m128i d = _mm_set1_epi16(0x100);

  int i;
  for (i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s0 = _mm_madd_epi16(pixels[0], wh_sc);
    __m128i s1 = _mm_madd_epi16(pixels[1], wh_sc);

    __m128i b = _mm_shuffle_epi8(pixels[2], rep);
    b = _mm_unpacklo_epi16(b, pixels[3]);
    __m128i sum0 = _mm_madd_epi16(b, ww[0]);
    __m128i sum1 = _mm_madd_epi16(b, ww[1]);

    s0 = _mm_add_epi32(s0, sum0);
    s0 = _mm_add_epi32(s0, round);
    s0 = _mm_srai_epi32(s0, 1 + sm_weight_log2_scale);

    s1 = _mm_add_epi32(s1, sum1);
    s1 = _mm_add_epi32(s1, round);
    s1 = _mm_srai_epi32(s1, 1 + sm_weight_log2_scale);

    sum0 = _mm_packus_epi16(s0, s1);
    sum0 = _mm_shuffle_epi8(sum0, gat);
    _mm_storel_epi64((__m128i *)dst, sum0);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 4, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(sm_weight_arrays, 4, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 4, dst, stride, 0);
}

void aom_smooth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 8, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(sm_weight_arrays, 8, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 8, dst, stride, 0);
}

void aom_smooth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 16, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(sm_weight_arrays, 16, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_8xh(pixels, &wh[2], ww, 8, dst, stride, 1);
}

// pixels[0]: above and below_pred interleave vector, 1/4
// pixels[1]: above and below_pred interleave vector, 2/4
// pixels[2]: above and below_pred interleave vector, 3/4
// pixels[3]: above and below_pred interleave vector, 3/4
// pixels[4]: left vector
// pixels[5]: left vector, h = 32 only
// pixels[6]: right_pred vector
static INLINE void load_pixel_w16(const uint8_t *above, const uint8_t *left,
                                  int height, __m128i *pixels) {
  __m128i ab = _mm_load_si128((const __m128i *)above);
  pixels[6] = _mm_set1_epi16((uint16_t)above[15]);
  pixels[4] = _mm_load_si128((const __m128i *)left);
  pixels[5] = _mm_load_si128((const __m128i *)(left + 16));
  const __m128i bp = _mm_set1_epi16((uint16_t)left[height - 1]);
  const __m128i zero = _mm_setzero_si128();

  __m128i x = _mm_unpacklo_epi8(ab, zero);
  pixels[0] = _mm_unpacklo_epi16(x, bp);
  pixels[1] = _mm_unpackhi_epi16(x, bp);

  x = _mm_unpackhi_epi8(ab, zero);
  pixels[2] = _mm_unpacklo_epi16(x, bp);
  pixels[3] = _mm_unpackhi_epi16(x, bp);
}

// weight_h[0]: weight_h vector
// weight_h[1]: scale - weight_h vector
// weight_h[2]: same as [0], second half for height = 16 only
// weight_h[3]: same as [1], second half for height = 16 only
// ... ...
// weight_w[0]: weights_w and scale - weights_w interleave vector, first half
// weight_w[1]: weights_w and scale - weights_w interleave vector, second half
// ... ...
static INLINE void load_weight_w16(const uint8_t *weight_array, int height,
                                   __m128i *weight_h, __m128i *weight_w) {
  const __m128i zero = _mm_setzero_si128();
  __m128i w8 = _mm_loadu_si128((const __m128i *)&weight_array[8]);
  __m128i w16 = _mm_loadu_si128((const __m128i *)&weight_array[16]);
  __m128i w32_0 = _mm_loadu_si128((const __m128i *)&weight_array[32]);
  __m128i w32_1 = _mm_loadu_si128((const __m128i *)&weight_array[32 + 16]);
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << sm_weight_log2_scale));

  if (height == 8) {
    weight_h[0] = _mm_unpacklo_epi8(w8, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);  // scale - weight_h

    __m128i x = _mm_unpacklo_epi8(w16, zero);
    __m128i y = _mm_sub_epi16(d, x);
    weight_w[0] = _mm_unpacklo_epi16(x, y);
    weight_w[1] = _mm_unpackhi_epi16(x, y);
    x = _mm_unpackhi_epi8(w16, zero);
    y = _mm_sub_epi16(d, x);
    weight_w[2] = _mm_unpacklo_epi16(x, y);
    weight_w[3] = _mm_unpackhi_epi16(x, y);
  }

  if (height == 16) {
    weight_h[0] = _mm_unpacklo_epi8(w16, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(w16, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);

    weight_w[0] = _mm_unpacklo_epi16(weight_h[0], weight_h[1]);
    weight_w[1] = _mm_unpackhi_epi16(weight_h[0], weight_h[1]);
    weight_w[2] = _mm_unpacklo_epi16(weight_h[2], weight_h[3]);
    weight_w[3] = _mm_unpackhi_epi16(weight_h[2], weight_h[3]);
  }

  if (height == 32) {
    weight_h[0] = _mm_unpacklo_epi8(w32_0, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(w32_0, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);

    __m128i x = _mm_unpacklo_epi8(w16, zero);
    __m128i y = _mm_sub_epi16(d, x);
    weight_w[0] = _mm_unpacklo_epi16(x, y);
    weight_w[1] = _mm_unpackhi_epi16(x, y);
    x = _mm_unpackhi_epi8(w16, zero);
    y = _mm_sub_epi16(d, x);
    weight_w[2] = _mm_unpacklo_epi16(x, y);
    weight_w[3] = _mm_unpackhi_epi16(x, y);

    weight_h[4] = _mm_unpacklo_epi8(w32_1, zero);
    weight_h[5] = _mm_sub_epi16(d, weight_h[4]);
    weight_h[6] = _mm_unpackhi_epi8(w32_1, zero);
    weight_h[7] = _mm_sub_epi16(d, weight_h[6]);
  }
}

static INLINE void smooth_pred_16x8(const __m128i *pixels, const __m128i *wh,
                                    const __m128i *ww, uint8_t *dst,
                                    ptrdiff_t stride, int quarter) {
  __m128i d = _mm_set1_epi16(0x100);
  const __m128i one = _mm_set1_epi16(1);
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  const __m128i round = _mm_set1_epi32((1 << sm_weight_log2_scale));
  __m128i rep =
      (quarter % 2 == 0) ? _mm_set1_epi16(0x8000) : _mm_set1_epi16(0x8008);
  const __m128i left = (quarter < 2) ? pixels[4] : pixels[5];

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s0 = _mm_madd_epi16(pixels[0], wh_sc);
    __m128i s1 = _mm_madd_epi16(pixels[1], wh_sc);
    __m128i s2 = _mm_madd_epi16(pixels[2], wh_sc);
    __m128i s3 = _mm_madd_epi16(pixels[3], wh_sc);

    __m128i b = _mm_shuffle_epi8(left, rep);
    b = _mm_unpacklo_epi16(b, pixels[6]);
    __m128i sum0 = _mm_madd_epi16(b, ww[0]);
    __m128i sum1 = _mm_madd_epi16(b, ww[1]);
    __m128i sum2 = _mm_madd_epi16(b, ww[2]);
    __m128i sum3 = _mm_madd_epi16(b, ww[3]);

    s0 = _mm_add_epi32(s0, sum0);
    s0 = _mm_add_epi32(s0, round);
    s0 = _mm_srai_epi32(s0, 1 + sm_weight_log2_scale);

    s1 = _mm_add_epi32(s1, sum1);
    s1 = _mm_add_epi32(s1, round);
    s1 = _mm_srai_epi32(s1, 1 + sm_weight_log2_scale);

    s2 = _mm_add_epi32(s2, sum2);
    s2 = _mm_add_epi32(s2, round);
    s2 = _mm_srai_epi32(s2, 1 + sm_weight_log2_scale);

    s3 = _mm_add_epi32(s3, sum3);
    s3 = _mm_add_epi32(s3, round);
    s3 = _mm_srai_epi32(s3, 1 + sm_weight_log2_scale);

    sum0 = _mm_packus_epi16(s0, s1);
    sum0 = _mm_shuffle_epi8(sum0, gat);
    sum1 = _mm_packus_epi16(s2, s3);
    sum1 = _mm_shuffle_epi8(sum1, gat);

    _mm_storel_epi64((__m128i *)dst, sum0);
    _mm_storel_epi64((__m128i *)(dst + 8), sum1);

    dst += stride;
    rep = _mm_add_epi16(rep, one);
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i pixels[7];
  load_pixel_w16(above, left, 8, pixels);

  __m128i wh[2], ww[4];
  load_weight_w16(sm_weight_arrays, 8, wh, ww);

  smooth_pred_16x8(pixels, wh, ww, dst, stride, 0);
}

void aom_smooth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[7];
  load_pixel_w16(above, left, 16, pixels);

  __m128i wh[4], ww[4];
  load_weight_w16(sm_weight_arrays, 16, wh, ww);

  smooth_pred_16x8(pixels, wh, ww, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_16x8(pixels, &wh[2], ww, dst, stride, 1);
}

void aom_smooth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[7];
  load_pixel_w16(above, left, 32, pixels);

  __m128i wh[8], ww[4];
  load_weight_w16(sm_weight_arrays, 32, wh, ww);

  smooth_pred_16x8(pixels, wh, ww, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_16x8(pixels, &wh[2], ww, dst, stride, 1);
  dst += stride << 3;
  smooth_pred_16x8(pixels, &wh[4], ww, dst, stride, 2);
  dst += stride << 3;
  smooth_pred_16x8(pixels, &wh[6], ww, dst, stride, 3);
}

static INLINE void load_pixel_w32(const uint8_t *above, const uint8_t *left,
                                  int height, __m128i *pixels) {
  __m128i ab0 = _mm_load_si128((const __m128i *)above);
  __m128i ab1 = _mm_load_si128((const __m128i *)(above + 16));

  pixels[10] = _mm_set1_epi16((uint16_t)above[31]);
  pixels[8] = _mm_load_si128((const __m128i *)left);
  pixels[9] = _mm_load_si128((const __m128i *)(left + 16));

  const __m128i bp = _mm_set1_epi16((uint16_t)left[height - 1]);
  const __m128i zero = _mm_setzero_si128();

  __m128i x = _mm_unpacklo_epi8(ab0, zero);
  pixels[0] = _mm_unpacklo_epi16(x, bp);
  pixels[1] = _mm_unpackhi_epi16(x, bp);

  x = _mm_unpackhi_epi8(ab0, zero);
  pixels[2] = _mm_unpacklo_epi16(x, bp);
  pixels[3] = _mm_unpackhi_epi16(x, bp);

  x = _mm_unpacklo_epi8(ab1, zero);
  pixels[4] = _mm_unpacklo_epi16(x, bp);
  pixels[5] = _mm_unpackhi_epi16(x, bp);

  x = _mm_unpackhi_epi8(ab1, zero);
  pixels[6] = _mm_unpacklo_epi16(x, bp);
  pixels[7] = _mm_unpackhi_epi16(x, bp);
}

static INLINE void load_weight_w32(const uint8_t *weight_array, int height,
                                   __m128i *weight_h, __m128i *weight_w) {
  const __m128i zero = _mm_setzero_si128();
  __m128i w16 = _mm_loadu_si128((const __m128i *)&weight_array[16]);
  __m128i w32_0 = _mm_loadu_si128((const __m128i *)&weight_array[32]);
  __m128i w32_1 = _mm_loadu_si128((const __m128i *)&weight_array[32 + 16]);
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << sm_weight_log2_scale));

  if (height == 16) {
    weight_h[0] = _mm_unpacklo_epi8(w16, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(w16, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);

    __m128i x = _mm_unpacklo_epi8(w32_0, zero);
    __m128i y = _mm_sub_epi16(d, x);
    weight_w[0] = _mm_unpacklo_epi16(x, y);
    weight_w[1] = _mm_unpackhi_epi16(x, y);

    x = _mm_unpackhi_epi8(w32_0, zero);
    y = _mm_sub_epi16(d, x);
    weight_w[2] = _mm_unpacklo_epi16(x, y);
    weight_w[3] = _mm_unpackhi_epi16(x, y);

    x = _mm_unpacklo_epi8(w32_1, zero);
    y = _mm_sub_epi16(d, x);
    weight_w[4] = _mm_unpacklo_epi16(x, y);
    weight_w[5] = _mm_unpackhi_epi16(x, y);

    x = _mm_unpackhi_epi8(w32_1, zero);
    y = _mm_sub_epi16(d, x);
    weight_w[6] = _mm_unpacklo_epi16(x, y);
    weight_w[7] = _mm_unpackhi_epi16(x, y);
  }

  if (height == 32) {
    weight_h[0] = _mm_unpacklo_epi8(w32_0, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(w32_0, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);

    weight_h[4] = _mm_unpacklo_epi8(w32_1, zero);
    weight_h[5] = _mm_sub_epi16(d, weight_h[4]);
    weight_h[6] = _mm_unpackhi_epi8(w32_1, zero);
    weight_h[7] = _mm_sub_epi16(d, weight_h[6]);

    weight_w[0] = _mm_unpacklo_epi16(weight_h[0], weight_h[1]);
    weight_w[1] = _mm_unpackhi_epi16(weight_h[0], weight_h[1]);
    weight_w[2] = _mm_unpacklo_epi16(weight_h[2], weight_h[3]);
    weight_w[3] = _mm_unpackhi_epi16(weight_h[2], weight_h[3]);

    weight_w[4] = _mm_unpacklo_epi16(weight_h[4], weight_h[5]);
    weight_w[5] = _mm_unpackhi_epi16(weight_h[4], weight_h[5]);
    weight_w[6] = _mm_unpacklo_epi16(weight_h[6], weight_h[7]);
    weight_w[7] = _mm_unpackhi_epi16(weight_h[6], weight_h[7]);
  }
}

static INLINE void smooth_pred_32x8(const __m128i *pixels, const __m128i *wh,
                                    const __m128i *ww, uint8_t *dst,
                                    ptrdiff_t stride, int quarter) {
  __m128i d = _mm_set1_epi16(0x100);
  const __m128i one = _mm_set1_epi16(1);
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  const __m128i round = _mm_set1_epi32((1 << sm_weight_log2_scale));
  __m128i rep =
      (quarter % 2 == 0) ? _mm_set1_epi16(0x8000) : _mm_set1_epi16(0x8008);
  const __m128i left = (quarter < 2) ? pixels[8] : pixels[9];

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);

    int j;
    __m128i s[8];
    __m128i b = _mm_shuffle_epi8(left, rep);
    b = _mm_unpacklo_epi16(b, pixels[10]);

    for (j = 0; j < 8; ++j) {
      s[j] = _mm_madd_epi16(pixels[j], wh_sc);
      s[j] = _mm_add_epi32(s[j], _mm_madd_epi16(b, ww[j]));
      s[j] = _mm_add_epi32(s[j], round);
      s[j] = _mm_srai_epi32(s[j], 1 + sm_weight_log2_scale);
    }

    for (j = 0; j < 8; j += 2) {
      __m128i sum = _mm_packus_epi16(s[j], s[j + 1]);
      sum = _mm_shuffle_epi8(sum, gat);
      _mm_storel_epi64((__m128i *)(dst + (j << 2)), sum);
    }
    dst += stride;
    rep = _mm_add_epi16(rep, one);
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[11];
  load_pixel_w32(above, left, 16, pixels);

  __m128i wh[4], ww[8];
  load_weight_w32(sm_weight_arrays, 16, wh, ww);

  smooth_pred_32x8(pixels, wh, ww, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_32x8(pixels, &wh[2], ww, dst, stride, 1);
}

void aom_smooth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[11];
  load_pixel_w32(above, left, 32, pixels);

  __m128i wh[8], ww[8];
  load_weight_w32(sm_weight_arrays, 32, wh, ww);

  smooth_pred_32x8(pixels, &wh[0], ww, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_32x8(pixels, &wh[2], ww, dst, stride, 1);
  dst += stride << 3;
  smooth_pred_32x8(pixels, &wh[4], ww, dst, stride, 2);
  dst += stride << 3;
  smooth_pred_32x8(pixels, &wh[6], ww, dst, stride, 3);
}
