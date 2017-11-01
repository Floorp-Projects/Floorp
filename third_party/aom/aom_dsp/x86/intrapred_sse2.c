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

#include <emmintrin.h>

#include "./aom_dsp_rtcd.h"

static INLINE void dc_store_4x8(uint32_t dc, uint8_t *dst, ptrdiff_t stride) {
  int i;
  for (i = 0; i < 4; ++i) {
    *(uint32_t *)dst = dc;
    dst += stride;
    *(uint32_t *)dst = dc;
    dst += stride;
  }
}

static INLINE void dc_store_8xh(const __m128i *row, int height, uint8_t *dst,
                                ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_storel_epi64((__m128i *)dst, *row);
    dst += stride;
  }
}

static INLINE void dc_store_16xh(const __m128i *row, int height, uint8_t *dst,
                                 ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, *row);
    dst += stride;
  }
}

static INLINE void dc_store_32xh(const __m128i *row, int height, uint8_t *dst,
                                 ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, *row);
    _mm_store_si128((__m128i *)(dst + 16), *row);
    dst += stride;
  }
}

static INLINE __m128i dc_sum_4(const uint8_t *ref) {
  __m128i x = _mm_loadl_epi64((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  x = _mm_unpacklo_epi8(x, zero);
  return _mm_sad_epu8(x, zero);
}

static INLINE __m128i dc_sum_8(const uint8_t *ref) {
  __m128i x = _mm_loadl_epi64((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  return _mm_sad_epu8(x, zero);
}

static INLINE __m128i dc_sum_16(const uint8_t *ref) {
  __m128i x = _mm_load_si128((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  x = _mm_sad_epu8(x, zero);
  const __m128i high = _mm_unpackhi_epi64(x, x);
  return _mm_add_epi16(x, high);
}

static INLINE __m128i dc_sum_32(const uint8_t *ref) {
  __m128i x0 = _mm_load_si128((__m128i const *)ref);
  __m128i x1 = _mm_load_si128((__m128i const *)(ref + 16));
  const __m128i zero = _mm_setzero_si128();
  x0 = _mm_sad_epu8(x0, zero);
  x1 = _mm_sad_epu8(x1, zero);
  x0 = _mm_add_epi16(x0, x1);
  const __m128i high = _mm_unpackhi_epi64(x0, x0);
  return _mm_add_epi16(x0, high);
}

// -----------------------------------------------------------------------------
// DC_PRED

void aom_dc_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_8(left);
  __m128i sum_above = dc_sum_4(above);
  sum_above = _mm_add_epi16(sum_left, sum_above);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 6;
  sum /= 12;

  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  const uint32_t pred = _mm_cvtsi128_si32(row);
  dc_store_4x8(pred, dst, stride);
}

void aom_dc_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_4(left);
  __m128i sum_above = dc_sum_8(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 6;
  sum /= 12;

  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_16(left);
  __m128i sum_above = dc_sum_8(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 12;
  sum /= 24;
  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_dc_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_8(left);
  __m128i sum_above = dc_sum_16(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 12;
  sum /= 24;
  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_32(left);
  __m128i sum_above = dc_sum_16(above);
  sum_above = _mm_add_epi16(sum_left, sum_above);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 24;
  sum /= 48;
  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_dc_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  __m128i sum_above = dc_sum_32(above);
  const __m128i sum_left = dc_sum_16(left);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 24;
  sum /= 48;
  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_32xh(&row, 16, dst, stride);
}

// -----------------------------------------------------------------------------
// DC_TOP

void aom_dc_top_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_4(above);
  const __m128i two = _mm_set1_epi16((int16_t)2);
  sum_above = _mm_add_epi16(sum_above, two);
  sum_above = _mm_srai_epi16(sum_above, 2);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  sum_above = _mm_packus_epi16(sum_above, sum_above);

  const uint32_t pred = _mm_cvtsi128_si32(sum_above);
  dc_store_4x8(pred, dst, stride);
}

void aom_dc_top_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_8(above);
  const __m128i four = _mm_set1_epi16((uint16_t)4);
  sum_above = _mm_add_epi16(sum_above, four);
  sum_above = _mm_srai_epi16(sum_above, 3);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  const __m128i row = _mm_shufflelo_epi16(sum_above, 0);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_top_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_8(above);
  const __m128i four = _mm_set1_epi16((uint16_t)4);
  sum_above = _mm_add_epi16(sum_above, four);
  sum_above = _mm_srai_epi16(sum_above, 3);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  const __m128i row = _mm_shufflelo_epi16(sum_above, 0);
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_dc_top_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_16(above);
  const __m128i eight = _mm_set1_epi16((uint16_t)8);
  sum_above = _mm_add_epi16(sum_above, eight);
  sum_above = _mm_srai_epi16(sum_above, 4);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_top_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_16(above);
  const __m128i eight = _mm_set1_epi16((uint16_t)8);
  sum_above = _mm_add_epi16(sum_above, eight);
  sum_above = _mm_srai_epi16(sum_above, 4);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_dc_top_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_32(above);
  const __m128i sixteen = _mm_set1_epi16((uint16_t)16);
  sum_above = _mm_add_epi16(sum_above, sixteen);
  sum_above = _mm_srai_epi16(sum_above, 5);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_32xh(&row, 16, dst, stride);
}

// -----------------------------------------------------------------------------
// DC_LEFT

void aom_dc_left_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_8(left);
  const __m128i four = _mm_set1_epi16((uint16_t)4);
  sum_left = _mm_add_epi16(sum_left, four);
  sum_left = _mm_srai_epi16(sum_left, 3);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  sum_left = _mm_packus_epi16(sum_left, sum_left);

  const uint32_t pred = _mm_cvtsi128_si32(sum_left);
  dc_store_4x8(pred, dst, stride);
}

void aom_dc_left_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_4(left);
  const __m128i two = _mm_set1_epi16((uint16_t)2);
  sum_left = _mm_add_epi16(sum_left, two);
  sum_left = _mm_srai_epi16(sum_left, 2);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  const __m128i row = _mm_shufflelo_epi16(sum_left, 0);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_left_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_16(left);
  const __m128i eight = _mm_set1_epi16((uint16_t)8);
  sum_left = _mm_add_epi16(sum_left, eight);
  sum_left = _mm_srai_epi16(sum_left, 4);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  const __m128i row = _mm_shufflelo_epi16(sum_left, 0);
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_dc_left_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_8(left);
  const __m128i four = _mm_set1_epi16((uint16_t)4);
  sum_left = _mm_add_epi16(sum_left, four);
  sum_left = _mm_srai_epi16(sum_left, 3);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_left_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_32(left);
  const __m128i sixteen = _mm_set1_epi16((uint16_t)16);
  sum_left = _mm_add_epi16(sum_left, sixteen);
  sum_left = _mm_srai_epi16(sum_left, 5);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_dc_left_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_16(left);
  const __m128i eight = _mm_set1_epi16((uint16_t)8);
  sum_left = _mm_add_epi16(sum_left, eight);
  sum_left = _mm_srai_epi16(sum_left, 4);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_32xh(&row, 16, dst, stride);
}

// -----------------------------------------------------------------------------
// DC_128

void aom_dc_128_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const uint32_t pred = 0x80808080;
  dc_store_4x8(pred, dst, stride);
}

void aom_dc_128_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_128_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_dc_128_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_128_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_dc_128_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_32xh(&row, 16, dst, stride);
}

// -----------------------------------------------------------------------------
// V_PRED

void aom_v_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const uint32_t pred = *(uint32_t *)above;
  (void)left;
  dc_store_4x8(pred, dst, stride);
}

void aom_v_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_loadl_epi64((__m128i const *)above);
  (void)left;
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_v_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_loadl_epi64((__m128i const *)above);
  (void)left;
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_v_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_load_si128((__m128i const *)above);
  (void)left;
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_v_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_load_si128((__m128i const *)above);
  (void)left;
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_v_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i row0 = _mm_load_si128((__m128i const *)above);
  const __m128i row1 = _mm_load_si128((__m128i const *)(above + 16));
  (void)left;
  int i;
  for (i = 0; i < 16; ++i) {
    _mm_store_si128((__m128i *)dst, row0);
    _mm_store_si128((__m128i *)(dst + 16), row1);
    dst += stride;
  }
}

// -----------------------------------------------------------------------------
// H_PRED

void aom_h_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i left_col = _mm_loadl_epi64((__m128i const *)left);
  left_col = _mm_unpacklo_epi8(left_col, left_col);
  __m128i row0 = _mm_shufflelo_epi16(left_col, 0);
  __m128i row1 = _mm_shufflelo_epi16(left_col, 0x55);
  __m128i row2 = _mm_shufflelo_epi16(left_col, 0xaa);
  __m128i row3 = _mm_shufflelo_epi16(left_col, 0xff);
  *(uint32_t *)dst = _mm_cvtsi128_si32(row0);
  dst += stride;
  *(uint32_t *)dst = _mm_cvtsi128_si32(row1);
  dst += stride;
  *(uint32_t *)dst = _mm_cvtsi128_si32(row2);
  dst += stride;
  *(uint32_t *)dst = _mm_cvtsi128_si32(row3);
  dst += stride;
  left_col = _mm_unpackhi_epi64(left_col, left_col);
  row0 = _mm_shufflelo_epi16(left_col, 0);
  row1 = _mm_shufflelo_epi16(left_col, 0x55);
  row2 = _mm_shufflelo_epi16(left_col, 0xaa);
  row3 = _mm_shufflelo_epi16(left_col, 0xff);
  *(uint32_t *)dst = _mm_cvtsi128_si32(row0);
  dst += stride;
  *(uint32_t *)dst = _mm_cvtsi128_si32(row1);
  dst += stride;
  *(uint32_t *)dst = _mm_cvtsi128_si32(row2);
  dst += stride;
  *(uint32_t *)dst = _mm_cvtsi128_si32(row3);
}

void aom_h_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i left_col = _mm_loadl_epi64((__m128i const *)left);
  left_col = _mm_unpacklo_epi8(left_col, left_col);
  __m128i row0 = _mm_shufflelo_epi16(left_col, 0);
  __m128i row1 = _mm_shufflelo_epi16(left_col, 0x55);
  __m128i row2 = _mm_shufflelo_epi16(left_col, 0xaa);
  __m128i row3 = _mm_shufflelo_epi16(left_col, 0xff);
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
}

void aom_h_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)above;
  const __m128i left_col = _mm_load_si128((__m128i const *)left);
  __m128i left_col_low = _mm_unpacklo_epi8(left_col, left_col);
  __m128i left_col_high = _mm_unpackhi_epi8(left_col, left_col);

  __m128i row0 = _mm_shufflelo_epi16(left_col_low, 0);
  __m128i row1 = _mm_shufflelo_epi16(left_col_low, 0x55);
  __m128i row2 = _mm_shufflelo_epi16(left_col_low, 0xaa);
  __m128i row3 = _mm_shufflelo_epi16(left_col_low, 0xff);
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
  dst += stride;

  left_col_low = _mm_unpackhi_epi64(left_col_low, left_col_low);
  row0 = _mm_shufflelo_epi16(left_col_low, 0);
  row1 = _mm_shufflelo_epi16(left_col_low, 0x55);
  row2 = _mm_shufflelo_epi16(left_col_low, 0xaa);
  row3 = _mm_shufflelo_epi16(left_col_low, 0xff);
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
  dst += stride;

  row0 = _mm_shufflelo_epi16(left_col_high, 0);
  row1 = _mm_shufflelo_epi16(left_col_high, 0x55);
  row2 = _mm_shufflelo_epi16(left_col_high, 0xaa);
  row3 = _mm_shufflelo_epi16(left_col_high, 0xff);
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
  dst += stride;

  left_col_high = _mm_unpackhi_epi64(left_col_high, left_col_high);
  row0 = _mm_shufflelo_epi16(left_col_high, 0);
  row1 = _mm_shufflelo_epi16(left_col_high, 0x55);
  row2 = _mm_shufflelo_epi16(left_col_high, 0xaa);
  row3 = _mm_shufflelo_epi16(left_col_high, 0xff);
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
}

static INLINE void h_pred_store_16xh(const __m128i *row, int h, uint8_t *dst,
                                     ptrdiff_t stride) {
  int i;
  for (i = 0; i < h; ++i) {
    _mm_store_si128((__m128i *)dst, row[i]);
    dst += stride;
  }
}

static INLINE void repeat_low_4pixels(const __m128i *x, __m128i *row) {
  const __m128i u0 = _mm_shufflelo_epi16(*x, 0);
  const __m128i u1 = _mm_shufflelo_epi16(*x, 0x55);
  const __m128i u2 = _mm_shufflelo_epi16(*x, 0xaa);
  const __m128i u3 = _mm_shufflelo_epi16(*x, 0xff);

  row[0] = _mm_unpacklo_epi64(u0, u0);
  row[1] = _mm_unpacklo_epi64(u1, u1);
  row[2] = _mm_unpacklo_epi64(u2, u2);
  row[3] = _mm_unpacklo_epi64(u3, u3);
}

static INLINE void repeat_high_4pixels(const __m128i *x, __m128i *row) {
  const __m128i u0 = _mm_shufflehi_epi16(*x, 0);
  const __m128i u1 = _mm_shufflehi_epi16(*x, 0x55);
  const __m128i u2 = _mm_shufflehi_epi16(*x, 0xaa);
  const __m128i u3 = _mm_shufflehi_epi16(*x, 0xff);

  row[0] = _mm_unpackhi_epi64(u0, u0);
  row[1] = _mm_unpackhi_epi64(u1, u1);
  row[2] = _mm_unpackhi_epi64(u2, u2);
  row[3] = _mm_unpackhi_epi64(u3, u3);
}

// Process 16x8, first 4 rows
// Use first 8 bytes of left register: xxxxxxxx33221100
static INLINE void h_prediction_16x8_1(const __m128i *left, uint8_t *dst,
                                       ptrdiff_t stride) {
  __m128i row[4];
  repeat_low_4pixels(left, row);
  h_pred_store_16xh(row, 4, dst, stride);
}

// Process 16x8, second 4 rows
// Use second 8 bytes of left register: 77665544xxxxxxxx
static INLINE void h_prediction_16x8_2(const __m128i *left, uint8_t *dst,
                                       ptrdiff_t stride) {
  __m128i row[4];
  repeat_high_4pixels(left, row);
  h_pred_store_16xh(row, 4, dst, stride);
}

void aom_h_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)above;
  const __m128i left_col = _mm_loadl_epi64((const __m128i *)left);
  const __m128i left_col_8p = _mm_unpacklo_epi8(left_col, left_col);
  h_prediction_16x8_1(&left_col_8p, dst, stride);
  dst += stride << 2;
  h_prediction_16x8_2(&left_col_8p, dst, stride);
}

void aom_h_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  __m128i left_col, left_col_8p;
  (void)above;
  int i = 0;

  do {
    left_col = _mm_load_si128((const __m128i *)left);
    left_col_8p = _mm_unpacklo_epi8(left_col, left_col);
    h_prediction_16x8_1(&left_col_8p, dst, stride);
    dst += stride << 2;
    h_prediction_16x8_2(&left_col_8p, dst, stride);
    dst += stride << 2;

    left_col_8p = _mm_unpackhi_epi8(left_col, left_col);
    h_prediction_16x8_1(&left_col_8p, dst, stride);
    dst += stride << 2;
    h_prediction_16x8_2(&left_col_8p, dst, stride);
    dst += stride << 2;

    left += 16;
    i++;
  } while (i < 2);
}

static INLINE void h_pred_store_32xh(const __m128i *row, int h, uint8_t *dst,
                                     ptrdiff_t stride) {
  int i;
  for (i = 0; i < h; ++i) {
    _mm_store_si128((__m128i *)dst, row[i]);
    _mm_store_si128((__m128i *)(dst + 16), row[i]);
    dst += stride;
  }
}

// Process 32x8, first 4 rows
// Use first 8 bytes of left register: xxxxxxxx33221100
static INLINE void h_prediction_32x8_1(const __m128i *left, uint8_t *dst,
                                       ptrdiff_t stride) {
  __m128i row[4];
  repeat_low_4pixels(left, row);
  h_pred_store_32xh(row, 4, dst, stride);
}

// Process 32x8, second 4 rows
// Use second 8 bytes of left register: 77665544xxxxxxxx
static INLINE void h_prediction_32x8_2(const __m128i *left, uint8_t *dst,
                                       ptrdiff_t stride) {
  __m128i row[4];
  repeat_high_4pixels(left, row);
  h_pred_store_32xh(row, 4, dst, stride);
}

void aom_h_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  __m128i left_col, left_col_8p;
  (void)above;

  left_col = _mm_load_si128((const __m128i *)left);

  left_col_8p = _mm_unpacklo_epi8(left_col, left_col);
  h_prediction_32x8_1(&left_col_8p, dst, stride);
  dst += stride << 2;
  h_prediction_32x8_2(&left_col_8p, dst, stride);
  dst += stride << 2;

  left_col_8p = _mm_unpackhi_epi8(left_col, left_col);
  h_prediction_32x8_1(&left_col_8p, dst, stride);
  dst += stride << 2;
  h_prediction_32x8_2(&left_col_8p, dst, stride);
}
