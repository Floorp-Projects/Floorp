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

// -----------------------------------------------------------------------------
// H_PRED

void aom_highbd_h_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  const __m128i left_u16 = _mm_loadl_epi64((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  (void)above;
  (void)bd;
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
}

void aom_highbd_h_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  aom_highbd_h_predictor_4x4_sse2(dst, stride, above, left, bd);
  dst += stride << 2;
  left += 4;
  aom_highbd_h_predictor_4x4_sse2(dst, stride, above, left, bd);
}

void aom_highbd_h_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  const __m128i left_u16 = _mm_load_si128((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  (void)above;
  (void)bd;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row0, row0));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row1, row1));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row2, row2));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row3, row3));
}

void aom_highbd_h_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  const __m128i left_u16 = _mm_load_si128((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  const __m128i row4 = _mm_shufflehi_epi16(left_u16, 0x0);
  const __m128i row5 = _mm_shufflehi_epi16(left_u16, 0x55);
  const __m128i row6 = _mm_shufflehi_epi16(left_u16, 0xaa);
  const __m128i row7 = _mm_shufflehi_epi16(left_u16, 0xff);
  (void)above;
  (void)bd;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row0, row0));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row1, row1));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row2, row2));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpacklo_epi64(row3, row3));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpackhi_epi64(row4, row4));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpackhi_epi64(row5, row5));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpackhi_epi64(row6, row6));
  dst += stride;
  _mm_store_si128((__m128i *)dst, _mm_unpackhi_epi64(row7, row7));
}

void aom_highbd_h_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  aom_highbd_h_predictor_8x8_sse2(dst, stride, above, left, bd);
  dst += stride << 3;
  left += 8;
  aom_highbd_h_predictor_8x8_sse2(dst, stride, above, left, bd);
}

static INLINE void h_store_16_unpacklo(uint16_t **dst, const ptrdiff_t stride,
                                       const __m128i *row) {
  const __m128i val = _mm_unpacklo_epi64(*row, *row);
  _mm_store_si128((__m128i *)*dst, val);
  _mm_store_si128((__m128i *)(*dst + 8), val);
  *dst += stride;
}

static INLINE void h_store_16_unpackhi(uint16_t **dst, const ptrdiff_t stride,
                                       const __m128i *row) {
  const __m128i val = _mm_unpackhi_epi64(*row, *row);
  _mm_store_si128((__m128i *)(*dst), val);
  _mm_store_si128((__m128i *)(*dst + 8), val);
  *dst += stride;
}

static INLINE void h_predictor_16x8(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *left) {
  const __m128i left_u16 = _mm_load_si128((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  const __m128i row4 = _mm_shufflehi_epi16(left_u16, 0x0);
  const __m128i row5 = _mm_shufflehi_epi16(left_u16, 0x55);
  const __m128i row6 = _mm_shufflehi_epi16(left_u16, 0xaa);
  const __m128i row7 = _mm_shufflehi_epi16(left_u16, 0xff);
  h_store_16_unpacklo(&dst, stride, &row0);
  h_store_16_unpacklo(&dst, stride, &row1);
  h_store_16_unpacklo(&dst, stride, &row2);
  h_store_16_unpacklo(&dst, stride, &row3);
  h_store_16_unpackhi(&dst, stride, &row4);
  h_store_16_unpackhi(&dst, stride, &row5);
  h_store_16_unpackhi(&dst, stride, &row6);
  h_store_16_unpackhi(&dst, stride, &row7);
}

void aom_highbd_h_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  h_predictor_16x8(dst, stride, left);
}

void aom_highbd_h_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i;
  (void)above;
  (void)bd;

  for (i = 0; i < 2; i++, left += 8) {
    h_predictor_16x8(dst, stride, left);
    dst += stride << 3;
  }
}

void aom_highbd_h_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i;
  (void)above;
  (void)bd;

  for (i = 0; i < 4; i++, left += 8) {
    h_predictor_16x8(dst, stride, left);
    dst += stride << 3;
  }
}

static INLINE void h_store_32_unpacklo(uint16_t **dst, const ptrdiff_t stride,
                                       const __m128i *row) {
  const __m128i val = _mm_unpacklo_epi64(*row, *row);
  _mm_store_si128((__m128i *)(*dst), val);
  _mm_store_si128((__m128i *)(*dst + 8), val);
  _mm_store_si128((__m128i *)(*dst + 16), val);
  _mm_store_si128((__m128i *)(*dst + 24), val);
  *dst += stride;
}

static INLINE void h_store_32_unpackhi(uint16_t **dst, const ptrdiff_t stride,
                                       const __m128i *row) {
  const __m128i val = _mm_unpackhi_epi64(*row, *row);
  _mm_store_si128((__m128i *)(*dst), val);
  _mm_store_si128((__m128i *)(*dst + 8), val);
  _mm_store_si128((__m128i *)(*dst + 16), val);
  _mm_store_si128((__m128i *)(*dst + 24), val);
  *dst += stride;
}

static INLINE void h_predictor_32x8(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *left) {
  const __m128i left_u16 = _mm_load_si128((const __m128i *)left);
  const __m128i row0 = _mm_shufflelo_epi16(left_u16, 0x0);
  const __m128i row1 = _mm_shufflelo_epi16(left_u16, 0x55);
  const __m128i row2 = _mm_shufflelo_epi16(left_u16, 0xaa);
  const __m128i row3 = _mm_shufflelo_epi16(left_u16, 0xff);
  const __m128i row4 = _mm_shufflehi_epi16(left_u16, 0x0);
  const __m128i row5 = _mm_shufflehi_epi16(left_u16, 0x55);
  const __m128i row6 = _mm_shufflehi_epi16(left_u16, 0xaa);
  const __m128i row7 = _mm_shufflehi_epi16(left_u16, 0xff);
  h_store_32_unpacklo(&dst, stride, &row0);
  h_store_32_unpacklo(&dst, stride, &row1);
  h_store_32_unpacklo(&dst, stride, &row2);
  h_store_32_unpacklo(&dst, stride, &row3);
  h_store_32_unpackhi(&dst, stride, &row4);
  h_store_32_unpackhi(&dst, stride, &row5);
  h_store_32_unpackhi(&dst, stride, &row6);
  h_store_32_unpackhi(&dst, stride, &row7);
}

void aom_highbd_h_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i;
  (void)above;
  (void)bd;

  for (i = 0; i < 2; i++, left += 8) {
    h_predictor_32x8(dst, stride, left);
    dst += stride << 3;
  }
}

void aom_highbd_h_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i;
  (void)above;
  (void)bd;

  for (i = 0; i < 4; i++, left += 8) {
    h_predictor_32x8(dst, stride, left);
    dst += stride << 3;
  }
}

// -----------------------------------------------------------------------------
// DC_TOP, DC_LEFT, DC_128

// 4x4

static INLINE __m128i dc_sum_4(const uint16_t *ref) {
  const __m128i _dcba = _mm_loadl_epi64((const __m128i *)ref);
  const __m128i _xxdc = _mm_shufflelo_epi16(_dcba, 0xe);
  const __m128i a = _mm_add_epi16(_dcba, _xxdc);
  return _mm_add_epi16(a, _mm_shufflelo_epi16(a, 0x1));
}

static INLINE void dc_store_4x4(uint16_t *dst, ptrdiff_t stride,
                                const __m128i *dc) {
  const __m128i dc_dup = _mm_shufflelo_epi16(*dc, 0x0);
  int i;
  for (i = 0; i < 4; ++i, dst += stride) {
    _mm_storel_epi64((__m128i *)dst, dc_dup);
  }
}

void aom_highbd_dc_left_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i two = _mm_cvtsi32_si128(2);
  const __m128i sum = dc_sum_4(left);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, two), 2);
  (void)above;
  (void)bd;
  dc_store_4x4(dst, stride, &dc);
}

void aom_highbd_dc_top_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  const __m128i two = _mm_cvtsi32_si128(2);
  const __m128i sum = dc_sum_4(above);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, two), 2);
  (void)left;
  (void)bd;
  dc_store_4x4(dst, stride, &dc);
}

void aom_highbd_dc_128_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  const __m128i dc = _mm_cvtsi32_si128(1 << (bd - 1));
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0x0);
  (void)above;
  (void)left;
  dc_store_4x4(dst, stride, &dc_dup);
}

// -----------------------------------------------------------------------------
// 4x8

static INLINE void dc_store_4x8(uint16_t *dst, ptrdiff_t stride,
                                const __m128i *dc) {
  const __m128i dc_dup = _mm_shufflelo_epi16(*dc, 0x0);
  int i;
  for (i = 0; i < 8; ++i, dst += stride) {
    _mm_storel_epi64((__m128i *)dst, dc_dup);
  }
}

// Shared with DC 8xh
static INLINE __m128i dc_sum_8(const uint16_t *ref) {
  const __m128i ref_u16 = _mm_load_si128((const __m128i *)ref);
  const __m128i _dcba = _mm_add_epi16(ref_u16, _mm_srli_si128(ref_u16, 8));
  const __m128i _xxdc = _mm_shufflelo_epi16(_dcba, 0xe);
  const __m128i a = _mm_add_epi16(_dcba, _xxdc);

  return _mm_add_epi16(a, _mm_shufflelo_epi16(a, 0x1));
}

void aom_highbd_dc_left_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i sum = dc_sum_8(left);
  const __m128i four = _mm_cvtsi32_si128(4);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, four), 3);
  (void)above;
  (void)bd;
  dc_store_4x8(dst, stride, &dc);
}

void aom_highbd_dc_top_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  const __m128i two = _mm_cvtsi32_si128(2);
  const __m128i sum = dc_sum_4(above);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, two), 2);
  (void)left;
  (void)bd;
  dc_store_4x8(dst, stride, &dc);
}

void aom_highbd_dc_128_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  const __m128i dc = _mm_cvtsi32_si128(1 << (bd - 1));
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0x0);
  (void)above;
  (void)left;
  dc_store_4x8(dst, stride, &dc_dup);
}

// -----------------------------------------------------------------------------
// 8xh

static INLINE void dc_store_8xh(uint16_t *dst, ptrdiff_t stride, int height,
                                const __m128i *dc) {
  const __m128i dc_dup_lo = _mm_shufflelo_epi16(*dc, 0);
  const __m128i dc_dup = _mm_unpacklo_epi64(dc_dup_lo, dc_dup_lo);
  int i;
  for (i = 0; i < height; ++i, dst += stride) {
    _mm_store_si128((__m128i *)dst, dc_dup);
  }
}

// -----------------------------------------------------------------------------
// DC_TOP

static INLINE void dc_top_predictor_8xh(uint16_t *dst, ptrdiff_t stride,
                                        int height, const uint16_t *above) {
  const __m128i four = _mm_cvtsi32_si128(4);
  const __m128i sum = dc_sum_8(above);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, four), 3);
  dc_store_8xh(dst, stride, height, &dc);
}

void aom_highbd_dc_top_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  dc_top_predictor_8xh(dst, stride, 4, above);
}

void aom_highbd_dc_top_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  dc_top_predictor_8xh(dst, stride, 8, above);
}

void aom_highbd_dc_top_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  dc_top_predictor_8xh(dst, stride, 16, above);
}

// -----------------------------------------------------------------------------
// DC_LEFT

void aom_highbd_dc_left_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i two = _mm_cvtsi32_si128(2);
  const __m128i sum = dc_sum_4(left);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, two), 2);
  (void)above;
  (void)bd;
  dc_store_8xh(dst, stride, 4, &dc);
}

void aom_highbd_dc_left_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i four = _mm_cvtsi32_si128(4);
  const __m128i sum = dc_sum_8(left);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, four), 3);
  (void)above;
  (void)bd;
  dc_store_8xh(dst, stride, 8, &dc);
}

// Shared with DC 16xh
static INLINE __m128i dc_sum_16(const uint16_t *ref) {
  const __m128i sum_lo = dc_sum_8(ref);
  const __m128i sum_hi = dc_sum_8(ref + 8);
  return _mm_add_epi16(sum_lo, sum_hi);
}

void aom_highbd_dc_left_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i eight = _mm_cvtsi32_si128(8);
  const __m128i sum = dc_sum_16(left);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, eight), 4);
  (void)above;
  (void)bd;
  dc_store_8xh(dst, stride, 16, &dc);
}

// -----------------------------------------------------------------------------
// DC_128

static INLINE void dc_128_predictor_8xh(uint16_t *dst, ptrdiff_t stride,
                                        int height, int bd) {
  const __m128i dc = _mm_cvtsi32_si128(1 << (bd - 1));
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0x0);
  dc_store_8xh(dst, stride, height, &dc_dup);
}

void aom_highbd_dc_128_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  (void)above;
  (void)left;
  dc_128_predictor_8xh(dst, stride, 4, bd);
}

void aom_highbd_dc_128_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  (void)above;
  (void)left;
  dc_128_predictor_8xh(dst, stride, 8, bd);
}

void aom_highbd_dc_128_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  (void)above;
  (void)left;
  dc_128_predictor_8xh(dst, stride, 16, bd);
}

// -----------------------------------------------------------------------------
// 16xh

static INLINE void dc_store_16xh(uint16_t *dst, ptrdiff_t stride, int height,
                                 const __m128i *dc) {
  const __m128i dc_dup_lo = _mm_shufflelo_epi16(*dc, 0);
  const __m128i dc_dup = _mm_unpacklo_epi64(dc_dup_lo, dc_dup_lo);
  int i;
  for (i = 0; i < height; ++i, dst += stride) {
    _mm_store_si128((__m128i *)dst, dc_dup);
    _mm_store_si128((__m128i *)(dst + 8), dc_dup);
  }
}

// -----------------------------------------------------------------------------
// DC_LEFT

void aom_highbd_dc_left_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i four = _mm_cvtsi32_si128(4);
  const __m128i sum = dc_sum_8(left);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, four), 3);
  (void)above;
  (void)bd;
  dc_store_16xh(dst, stride, 8, &dc);
}

void aom_highbd_dc_left_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *above,
                                             const uint16_t *left, int bd) {
  const __m128i eight = _mm_cvtsi32_si128(8);
  const __m128i sum = dc_sum_16(left);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, eight), 4);
  (void)above;
  (void)bd;
  dc_store_16xh(dst, stride, 16, &dc);
}

// Shared with 32xh
static INLINE __m128i dc_sum_32(const uint16_t *ref) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i sum_a = dc_sum_16(ref);
  const __m128i sum_b = dc_sum_16(ref + 16);
  // 12 bit bd will outrange, so expand to 32 bit before adding final total
  return _mm_add_epi32(_mm_unpacklo_epi16(sum_a, zero),
                       _mm_unpacklo_epi16(sum_b, zero));
}

void aom_highbd_dc_left_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *above,
                                             const uint16_t *left, int bd) {
  const __m128i sixteen = _mm_cvtsi32_si128(16);
  const __m128i sum = dc_sum_32(left);
  const __m128i dc = _mm_srli_epi32(_mm_add_epi32(sum, sixteen), 5);
  (void)above;
  (void)bd;
  dc_store_16xh(dst, stride, 32, &dc);
}

// -----------------------------------------------------------------------------
// DC_TOP

void aom_highbd_dc_top_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i eight = _mm_cvtsi32_si128(8);
  const __m128i sum = dc_sum_16(above);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, eight), 4);
  (void)left;
  (void)bd;
  dc_store_16xh(dst, stride, 8, &dc);
}

void aom_highbd_dc_top_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i eight = _mm_cvtsi32_si128(8);
  const __m128i sum = dc_sum_16(above);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, eight), 4);
  (void)left;
  (void)bd;
  dc_store_16xh(dst, stride, 16, &dc);
}

void aom_highbd_dc_top_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i eight = _mm_cvtsi32_si128(8);
  const __m128i sum = dc_sum_16(above);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, eight), 4);
  (void)left;
  (void)bd;
  dc_store_16xh(dst, stride, 32, &dc);
}

// -----------------------------------------------------------------------------
// DC_128

void aom_highbd_dc_128_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i dc = _mm_cvtsi32_si128(1 << (bd - 1));
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0x0);
  (void)above;
  (void)left;
  dc_store_16xh(dst, stride, 8, &dc_dup);
}

void aom_highbd_dc_128_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i dc = _mm_cvtsi32_si128(1 << (bd - 1));
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0x0);
  (void)above;
  (void)left;
  dc_store_16xh(dst, stride, 16, &dc_dup);
}

void aom_highbd_dc_128_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i dc = _mm_cvtsi32_si128(1 << (bd - 1));
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0x0);
  (void)above;
  (void)left;
  dc_store_16xh(dst, stride, 32, &dc_dup);
}

// -----------------------------------------------------------------------------
// 32xh

static INLINE void dc_store_32xh(uint16_t *dst, ptrdiff_t stride, int height,
                                 const __m128i *dc) {
  const __m128i dc_dup_lo = _mm_shufflelo_epi16(*dc, 0);
  const __m128i dc_dup = _mm_unpacklo_epi64(dc_dup_lo, dc_dup_lo);
  int i;
  for (i = 0; i < height; ++i, dst += stride) {
    _mm_store_si128((__m128i *)dst, dc_dup);
    _mm_store_si128((__m128i *)(dst + 8), dc_dup);
    _mm_store_si128((__m128i *)(dst + 16), dc_dup);
    _mm_store_si128((__m128i *)(dst + 24), dc_dup);
  }
}

void aom_highbd_dc_left_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *above,
                                             const uint16_t *left, int bd) {
  const __m128i eight = _mm_cvtsi32_si128(8);
  const __m128i sum = dc_sum_16(left);
  const __m128i dc = _mm_srli_epi16(_mm_add_epi16(sum, eight), 4);
  (void)above;
  (void)bd;
  dc_store_32xh(dst, stride, 16, &dc);
}

void aom_highbd_dc_left_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *above,
                                             const uint16_t *left, int bd) {
  const __m128i sixteen = _mm_cvtsi32_si128(16);
  const __m128i sum = dc_sum_32(left);
  const __m128i dc = _mm_srli_epi32(_mm_add_epi32(sum, sixteen), 5);
  (void)above;
  (void)bd;
  dc_store_32xh(dst, stride, 32, &dc);
}

void aom_highbd_dc_top_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i sixteen = _mm_cvtsi32_si128(16);
  const __m128i sum = dc_sum_32(above);
  const __m128i dc = _mm_srli_epi32(_mm_add_epi32(sum, sixteen), 5);
  (void)left;
  (void)bd;
  dc_store_32xh(dst, stride, 16, &dc);
}

void aom_highbd_dc_128_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i dc = _mm_cvtsi32_si128(1 << (bd - 1));
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0x0);
  (void)above;
  (void)left;
  dc_store_32xh(dst, stride, 16, &dc_dup);
}

void aom_highbd_dc_top_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i sixteen = _mm_cvtsi32_si128(16);
  const __m128i sum = dc_sum_32(above);
  const __m128i dc = _mm_srli_epi32(_mm_add_epi32(sum, sixteen), 5);
  (void)left;
  (void)bd;
  dc_store_32xh(dst, stride, 32, &dc);
}

void aom_highbd_dc_128_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  const __m128i dc = _mm_cvtsi32_si128(1 << (bd - 1));
  const __m128i dc_dup = _mm_shufflelo_epi16(dc, 0x0);
  (void)above;
  (void)left;
  dc_store_32xh(dst, stride, 32, &dc_dup);
}

// -----------------------------------------------------------------------------
// V_PRED

void aom_highbd_v_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  const __m128i above_u16 = _mm_loadl_epi64((const __m128i *)above);
  int i;
  for (i = 0; i < 2; ++i) {
    _mm_storel_epi64((__m128i *)dst, above_u16);
    _mm_storel_epi64((__m128i *)(dst + stride), above_u16);
    _mm_storel_epi64((__m128i *)(dst + 2 * stride), above_u16);
    _mm_storel_epi64((__m128i *)(dst + 3 * stride), above_u16);
    dst += stride << 2;
  }
}

void aom_highbd_v_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  const __m128i above_u16 = _mm_load_si128((const __m128i *)above);
  _mm_store_si128((__m128i *)dst, above_u16);
  _mm_store_si128((__m128i *)(dst + stride), above_u16);
  _mm_store_si128((__m128i *)(dst + 2 * stride), above_u16);
  _mm_store_si128((__m128i *)(dst + 3 * stride), above_u16);
}

void aom_highbd_v_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  const __m128i above_u16 = _mm_load_si128((const __m128i *)above);
  int i;
  for (i = 0; i < 4; ++i) {
    _mm_store_si128((__m128i *)dst, above_u16);
    _mm_store_si128((__m128i *)(dst + stride), above_u16);
    _mm_store_si128((__m128i *)(dst + 2 * stride), above_u16);
    _mm_store_si128((__m128i *)(dst + 3 * stride), above_u16);
    dst += stride << 2;
  }
}

void aom_highbd_v_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  const __m128i above0_u16 = _mm_load_si128((const __m128i *)above);
  const __m128i above1_u16 = _mm_load_si128((const __m128i *)(above + 8));
  int i;
  for (i = 0; i < 2; ++i) {
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    dst += stride;
  }
}

void aom_highbd_v_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  const __m128i above0_u16 = _mm_load_si128((const __m128i *)above);
  const __m128i above1_u16 = _mm_load_si128((const __m128i *)(above + 8));
  int i;
  for (i = 0; i < 8; ++i) {
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    dst += stride;
  }
}

void aom_highbd_v_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  const __m128i above0_u16 = _mm_load_si128((const __m128i *)above);
  const __m128i above1_u16 = _mm_load_si128((const __m128i *)(above + 8));
  const __m128i above2_u16 = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i above3_u16 = _mm_load_si128((const __m128i *)(above + 24));
  int i;
  for (i = 0; i < 4; ++i) {
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    _mm_store_si128((__m128i *)(dst + 16), above2_u16);
    _mm_store_si128((__m128i *)(dst + 24), above3_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    _mm_store_si128((__m128i *)(dst + 16), above2_u16);
    _mm_store_si128((__m128i *)(dst + 24), above3_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    _mm_store_si128((__m128i *)(dst + 16), above2_u16);
    _mm_store_si128((__m128i *)(dst + 24), above3_u16);
    dst += stride;
    _mm_store_si128((__m128i *)dst, above0_u16);
    _mm_store_si128((__m128i *)(dst + 8), above1_u16);
    _mm_store_si128((__m128i *)(dst + 16), above2_u16);
    _mm_store_si128((__m128i *)(dst + 24), above3_u16);
    dst += stride;
  }
}

// -----------------------------------------------------------------------------
// DC_PRED

void aom_highbd_dc_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)bd;
  const __m128i sum_above = dc_sum_4(above);
  const __m128i sum_left = dc_sum_8(left);
  const __m128i sum = _mm_add_epi16(sum_above, sum_left);
  uint32_t sum32 = _mm_cvtsi128_si32(sum);
  sum32 >>= 16;
  sum32 += 6;
  sum32 /= 12;
  const __m128i row = _mm_set1_epi16((uint16_t)sum32);
  int i;
  for (i = 0; i < 4; ++i) {
    _mm_storel_epi64((__m128i *)dst, row);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row);
    dst += stride;
  }
}

void aom_highbd_dc_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)bd;
  const __m128i sum_left = dc_sum_4(left);
  const __m128i sum_above = dc_sum_8(above);
  const __m128i sum = _mm_add_epi16(sum_above, sum_left);
  uint32_t sum32 = _mm_cvtsi128_si32(sum);
  sum32 >>= 16;
  sum32 += 6;
  sum32 /= 12;
  const __m128i row = _mm_set1_epi16((uint16_t)sum32);

  _mm_store_si128((__m128i *)dst, row);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row);
}

void aom_highbd_dc_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  (void)bd;
  __m128i sum_left = dc_sum_16(left);
  __m128i sum_above = dc_sum_8(above);
  const __m128i zero = _mm_setzero_si128();
  sum_left = _mm_unpacklo_epi16(sum_left, zero);
  sum_above = _mm_unpacklo_epi16(sum_above, zero);
  const __m128i sum = _mm_add_epi32(sum_left, sum_above);
  uint32_t sum32 = _mm_cvtsi128_si32(sum);
  sum32 += 12;
  sum32 /= 24;
  const __m128i row = _mm_set1_epi16((uint16_t)sum32);
  int i;
  for (i = 0; i < 4; ++i) {
    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
  }
}

void aom_highbd_dc_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  (void)bd;
  __m128i sum_left = dc_sum_8(left);
  __m128i sum_above = dc_sum_16(above);
  const __m128i zero = _mm_setzero_si128();
  sum_left = _mm_unpacklo_epi16(sum_left, zero);
  sum_above = _mm_unpacklo_epi16(sum_above, zero);
  const __m128i sum = _mm_add_epi32(sum_left, sum_above);
  uint32_t sum32 = _mm_cvtsi128_si32(sum);
  sum32 += 12;
  sum32 /= 24;
  const __m128i row = _mm_set1_epi16((uint16_t)sum32);
  int i;
  for (i = 0; i < 2; ++i) {
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    dst += stride;
  }
}

void aom_highbd_dc_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  (void)bd;
  __m128i sum_left = dc_sum_32(left);
  __m128i sum_above = dc_sum_16(above);
  const __m128i zero = _mm_setzero_si128();
  sum_above = _mm_unpacklo_epi16(sum_above, zero);
  const __m128i sum = _mm_add_epi32(sum_left, sum_above);
  uint32_t sum32 = _mm_cvtsi128_si32(sum);
  sum32 += 24;
  sum32 /= 48;
  const __m128i row = _mm_set1_epi16((uint16_t)sum32);
  int i;
  for (i = 0; i < 8; ++i) {
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    dst += stride;
  }
}

void aom_highbd_dc_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  (void)bd;
  __m128i sum_left = dc_sum_16(left);
  __m128i sum_above = dc_sum_32(above);
  const __m128i zero = _mm_setzero_si128();
  sum_left = _mm_unpacklo_epi16(sum_left, zero);
  const __m128i sum = _mm_add_epi32(sum_left, sum_above);
  uint32_t sum32 = _mm_cvtsi128_si32(sum);
  sum32 += 24;
  sum32 /= 48;
  const __m128i row = _mm_set1_epi16((uint16_t)sum32);
  int i;
  for (i = 0; i < 4; ++i) {
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    _mm_store_si128((__m128i *)(dst + 16), row);
    _mm_store_si128((__m128i *)(dst + 24), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    _mm_store_si128((__m128i *)(dst + 16), row);
    _mm_store_si128((__m128i *)(dst + 24), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    _mm_store_si128((__m128i *)(dst + 16), row);
    _mm_store_si128((__m128i *)(dst + 24), row);
    dst += stride;
    _mm_store_si128((__m128i *)dst, row);
    _mm_store_si128((__m128i *)(dst + 8), row);
    _mm_store_si128((__m128i *)(dst + 16), row);
    _mm_store_si128((__m128i *)(dst + 24), row);
    dst += stride;
  }
}

// -----------------------------------------------------------------------------
/*
; ------------------------------------------
; input: x, y, z, result
;
; trick from pascal
; (x+2y+z+2)>>2 can be calculated as:
; result = avg(x,z)
; result -= xor(x,z) & 1
; result = avg(result,y)
; ------------------------------------------
*/
static INLINE __m128i avg3_epu16(const __m128i *x, const __m128i *y,
                                 const __m128i *z) {
  const __m128i one = _mm_set1_epi16(1);
  const __m128i a = _mm_avg_epu16(*x, *z);
  const __m128i b =
      _mm_subs_epu16(a, _mm_and_si128(_mm_xor_si128(*x, *z), one));
  return _mm_avg_epu16(b, *y);
}

void aom_highbd_d117_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const __m128i XXXXABCD = _mm_loadu_si128((const __m128i *)(above - 4));
  const __m128i KXXXABCD = _mm_insert_epi16(XXXXABCD, K, 0);
  const __m128i KJXXABCD = _mm_insert_epi16(KXXXABCD, J, 1);
  const __m128i KJIXABCD = _mm_insert_epi16(KJXXABCD, I, 2);
  const __m128i JIXABCD0 = _mm_srli_si128(KJIXABCD, 2);
  const __m128i IXABCD00 = _mm_srli_si128(KJIXABCD, 4);
  const __m128i avg2 = _mm_avg_epu16(KJIXABCD, JIXABCD0);
  const __m128i avg3 = avg3_epu16(&KJIXABCD, &JIXABCD0, &IXABCD00);
  const __m128i row0 = _mm_srli_si128(avg2, 6);
  const __m128i row1 = _mm_srli_si128(avg3, 4);
  const __m128i row2 = _mm_srli_si128(avg2, 4);
  const __m128i row3 = _mm_srli_si128(avg3, 2);
  (void)bd;
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);

  dst -= stride;
  dst[0] = _mm_extract_epi16(avg3, 1);
  dst[stride] = _mm_extract_epi16(avg3, 0);
}

void aom_highbd_d135_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  const __m128i XXXXABCD = _mm_loadu_si128((const __m128i *)(above - 4));
  const __m128i KXXXABCD = _mm_insert_epi16(XXXXABCD, K, 0);
  const __m128i KJXXABCD = _mm_insert_epi16(KXXXABCD, J, 1);
  const __m128i KJIXABCD = _mm_insert_epi16(KJXXABCD, I, 2);
  const __m128i JIXABCD0 = _mm_srli_si128(KJIXABCD, 2);
  const __m128i LKJIXABC = _mm_insert_epi16(_mm_slli_si128(KJIXABCD, 2), L, 0);
  const __m128i avg3 = avg3_epu16(&JIXABCD0, &KJIXABCD, &LKJIXABC);
  const __m128i row0 = _mm_srli_si128(avg3, 6);
  const __m128i row1 = _mm_srli_si128(avg3, 4);
  const __m128i row2 = _mm_srli_si128(avg3, 2);
  const __m128i row3 = avg3;
  (void)bd;
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
}

void aom_highbd_d153_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  const __m128i XXXXXABC = _mm_loadu_si128((const __m128i *)(above - 5));
  const __m128i LXXXXABC = _mm_insert_epi16(XXXXXABC, L, 0);
  const __m128i LKXXXABC = _mm_insert_epi16(LXXXXABC, K, 1);
  const __m128i LKJXXABC = _mm_insert_epi16(LKXXXABC, J, 2);
  const __m128i LKJIXABC = _mm_insert_epi16(LKJXXABC, I, 3);
  const __m128i KJIXABC0 = _mm_srli_si128(LKJIXABC, 2);
  const __m128i JIXABC00 = _mm_srli_si128(LKJIXABC, 4);
  const __m128i avg3 = avg3_epu16(&LKJIXABC, &KJIXABC0, &JIXABC00);
  const __m128i avg2 = _mm_avg_epu16(LKJIXABC, KJIXABC0);
  const __m128i row3 = _mm_unpacklo_epi16(avg2, avg3);
  const __m128i row2 = _mm_srli_si128(row3, 4);
  const __m128i row1 = _mm_srli_si128(row3, 8);
  const __m128i row0 = _mm_srli_si128(avg3, 4);
  (void)bd;
  _mm_storel_epi64((__m128i *)dst, row0);
  dst[0] = _mm_extract_epi16(avg2, 3);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
}

void aom_highbd_d45e_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  const __m128i ABCDEFGH = _mm_loadu_si128((const __m128i *)above);
  const __m128i BCDEFGH0 = _mm_srli_si128(ABCDEFGH, 2);
  __m128i CDEFGH00 = _mm_srli_si128(ABCDEFGH, 4);
  CDEFGH00 = _mm_insert_epi16(CDEFGH00, above[7], 6);
  const __m128i avg3 = avg3_epu16(&ABCDEFGH, &BCDEFGH0, &CDEFGH00);
  (void)left;
  (void)bd;
  _mm_storel_epi64((__m128i *)dst, avg3);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 2));
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 4));
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 6));
}

void aom_highbd_d45e_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m128i h76543210 = _mm_load_si128((const __m128i *)above);
  __m128i hx7654321 = _mm_srli_si128(h76543210, 2);
  __m128i h87654321 = _mm_insert_epi16(hx7654321, above[8], 7);
  __m128i hx8765432 = _mm_srli_si128(h87654321, 2);
  __m128i h98765432 = _mm_insert_epi16(hx8765432, above[9], 7);
  __m128i avg3 = avg3_epu16(&h76543210, &h87654321, &h98765432);
  _mm_storel_epi64((__m128i *)dst, avg3);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 2));
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 4));
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 6));
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 8));
  dst += stride;

  // hcba98765
  h76543210 = _mm_loadu_si128((const __m128i *)((above + 5)));
  h76543210 = _mm_insert_epi16(h76543210, above[11], 7);
  // hxcba9876
  hx7654321 = _mm_srli_si128(h76543210, 2);
  // hxxcba987
  hx8765432 = _mm_srli_si128(h76543210, 4);
  avg3 = avg3_epu16(&h76543210, &hx7654321, &hx8765432);
  _mm_storel_epi64((__m128i *)dst, avg3);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 2));
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, _mm_srli_si128(avg3, 4));
}

void aom_highbd_d45e_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m128i x0 = _mm_load_si128((const __m128i *)above);
  __m128i x1 = _mm_loadu_si128((const __m128i *)(above + 1));
  __m128i x2 = _mm_loadu_si128((const __m128i *)(above + 2));
  __m128i y = avg3_epu16(&x0, &x1, &x2);
  _mm_store_si128((__m128i *)dst, y);
  dst += stride;

  x0 = _mm_loadu_si128((const __m128i *)(above + 3));
  y = avg3_epu16(&x1, &x2, &x0);
  _mm_store_si128((__m128i *)dst, y);
  dst += stride;

  x1 = _mm_loadu_si128((const __m128i *)(above + 4));
  y = avg3_epu16(&x2, &x0, &x1);
  _mm_store_si128((__m128i *)dst, y);
  dst += stride;

  x2 = _mm_loadu_si128((const __m128i *)(above + 5));
  x2 = _mm_insert_epi16(x2, above[11], 7);
  y = avg3_epu16(&x0, &x1, &x2);
  _mm_store_si128((__m128i *)dst, y);
}

static INLINE void d45e_w8(const __m128i *a0, const __m128i *a1,
                           const __m128i *a2, uint16_t **dst,
                           ptrdiff_t stride) {
  const __m128i y = avg3_epu16(a0, a1, a2);
  _mm_storeu_si128((__m128i *)*dst, y);
  *dst += stride;
}

void aom_highbd_d45e_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t stride,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m128i x0 = _mm_load_si128((const __m128i *)above);
  __m128i x1 = _mm_loadu_si128((const __m128i *)(above + 1));
  __m128i x2 = _mm_loadu_si128((const __m128i *)(above + 2));

  d45e_w8(&x0, &x1, &x2, &dst, stride);

  int i = 3;
  do {
    x0 = _mm_loadu_si128((const __m128i *)(above + i++));
    d45e_w8(&x1, &x2, &x0, &dst, stride);

    x1 = _mm_loadu_si128((const __m128i *)(above + i++));
    d45e_w8(&x2, &x0, &x1, &dst, stride);

    x2 = _mm_loadu_si128((const __m128i *)(above + i++));
    d45e_w8(&x0, &x1, &x2, &dst, stride);
  } while (i < 9);

  x0 = _mm_loadu_si128((const __m128i *)(above + 9));
  x0 = _mm_insert_epi16(x0, above[15], 7);
  const __m128i y = avg3_epu16(&x1, &x2, &x0);
  _mm_store_si128((__m128i *)dst, y);
}

void aom_highbd_d45e_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t stride,
                                         const uint16_t *above,
                                         const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m128i x0 = _mm_load_si128((const __m128i *)above);
  __m128i x1 = _mm_loadu_si128((const __m128i *)(above + 1));
  __m128i x2 = _mm_loadu_si128((const __m128i *)(above + 2));

  d45e_w8(&x0, &x1, &x2, &dst, stride);

  int i = 3;
  do {
    x0 = _mm_loadu_si128((const __m128i *)(above + i++));
    d45e_w8(&x1, &x2, &x0, &dst, stride);

    x1 = _mm_loadu_si128((const __m128i *)(above + i++));
    d45e_w8(&x2, &x0, &x1, &dst, stride);

    x2 = _mm_loadu_si128((const __m128i *)(above + i++));
    d45e_w8(&x0, &x1, &x2, &dst, stride);
  } while (i < 15);

  x0 = _mm_loadu_si128((const __m128i *)(above + 15));
  __m128i y = avg3_epu16(&x1, &x2, &x0);
  _mm_store_si128((__m128i *)dst, y);
  dst += stride;

  x1 = _mm_loadu_si128((const __m128i *)(above + 16));
  y = avg3_epu16(&x2, &x0, &x1);
  _mm_store_si128((__m128i *)dst, y);
  dst += stride;

  x2 = _mm_loadu_si128((const __m128i *)(above + 17));
  x2 = _mm_insert_epi16(x2, above[23], 7);
  y = avg3_epu16(&x0, &x1, &x2);
  _mm_store_si128((__m128i *)dst, y);
}
