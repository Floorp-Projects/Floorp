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

#include "aom_ports/msvc.h"
#include "./aom_dsp_rtcd.h"

// -----------------------------------------------------------------------------
// D45E_PRED
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
static INLINE __m256i avg3_epu16(const __m256i *x, const __m256i *y,
                                 const __m256i *z) {
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i a = _mm256_avg_epu16(*x, *z);
  const __m256i b =
      _mm256_subs_epu16(a, _mm256_and_si256(_mm256_xor_si256(*x, *z), one));
  return _mm256_avg_epu16(b, *y);
}

static INLINE void d45e_w16(const __m256i *a0, const __m256i *a1,
                            const __m256i *a2, uint16_t **dst,
                            ptrdiff_t stride) {
  const __m256i y = avg3_epu16(a0, a1, a2);
  _mm256_storeu_si256((__m256i *)*dst, y);
  *dst += stride;
}

void aom_highbd_d45e_predictor_16x8_avx2(uint16_t *dst, ptrdiff_t stride,
                                         const uint16_t *above,
                                         const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m256i x0 = _mm256_loadu_si256((const __m256i *)above);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)(above + 1));
  __m256i x2 = _mm256_loadu_si256((const __m256i *)(above + 2));

  d45e_w16(&x0, &x1, &x2, &dst, stride);

  int i = 3;
  do {
    x0 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x1, &x2, &x0, &dst, stride);

    x1 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x2, &x0, &x1, &dst, stride);

    x2 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x0, &x1, &x2, &dst, stride);
  } while (i < 9);

  x0 = _mm256_loadu_si256((const __m256i *)(above + 9));
  x0 = _mm256_insert_epi16(x0, above[23], 15);
  const __m256i y = avg3_epu16(&x1, &x2, &x0);
  _mm256_storeu_si256((__m256i *)dst, y);
}

void aom_highbd_d45e_predictor_16x16_avx2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m256i x0 = _mm256_loadu_si256((const __m256i *)above);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)(above + 1));
  __m256i x2 = _mm256_loadu_si256((const __m256i *)(above + 2));

  d45e_w16(&x0, &x1, &x2, &dst, stride);

  int i = 3;
  do {
    x0 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x1, &x2, &x0, &dst, stride);

    x1 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x2, &x0, &x1, &dst, stride);

    x2 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x0, &x1, &x2, &dst, stride);
  } while (i < 15);

  x0 = _mm256_loadu_si256((const __m256i *)(above + 15));
  d45e_w16(&x1, &x2, &x0, &dst, stride);

  x1 = _mm256_loadu_si256((const __m256i *)(above + 16));
  d45e_w16(&x2, &x0, &x1, &dst, stride);

  x2 = _mm256_loadu_si256((const __m256i *)(above + 17));
  x2 = _mm256_insert_epi16(x2, above[31], 15);
  const __m256i y = avg3_epu16(&x0, &x1, &x2);
  _mm256_storeu_si256((__m256i *)dst, y);
}

void aom_highbd_d45e_predictor_16x32_avx2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m256i x0 = _mm256_loadu_si256((const __m256i *)above);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)(above + 1));
  __m256i x2 = _mm256_loadu_si256((const __m256i *)(above + 2));

  d45e_w16(&x0, &x1, &x2, &dst, stride);

  int i = 3;
  do {
    x0 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x1, &x2, &x0, &dst, stride);

    x1 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x2, &x0, &x1, &dst, stride);

    x2 = _mm256_loadu_si256((const __m256i *)(above + i++));
    d45e_w16(&x0, &x1, &x2, &dst, stride);
  } while (i < 33);

  x0 = _mm256_loadu_si256((const __m256i *)(above + 33));
  x0 = _mm256_insert_epi16(x0, above[47], 15);
  const __m256i y = avg3_epu16(&x1, &x2, &x0);
  _mm256_storeu_si256((__m256i *)dst, y);
}

void aom_highbd_d45e_predictor_32x16_avx2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m256i x0 = _mm256_loadu_si256((const __m256i *)above);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)(above + 1));
  __m256i x2 = _mm256_loadu_si256((const __m256i *)(above + 2));
  __m256i y0 = _mm256_loadu_si256((const __m256i *)(above + 16));
  __m256i y1 = _mm256_loadu_si256((const __m256i *)(above + 17));
  __m256i y2 = _mm256_loadu_si256((const __m256i *)(above + 18));

  uint16_t *dst1 = dst;
  uint16_t *dst2 = dst + 16;

  d45e_w16(&x0, &x1, &x2, &dst1, stride);
  d45e_w16(&y0, &y1, &y2, &dst2, stride);

  int i = 3;
  do {
    x0 = _mm256_loadu_si256((const __m256i *)(above + i));
    d45e_w16(&x1, &x2, &x0, &dst1, stride);
    y0 = _mm256_loadu_si256((const __m256i *)(above + 16 + i++));
    d45e_w16(&y1, &y2, &y0, &dst2, stride);

    x1 = _mm256_loadu_si256((const __m256i *)(above + i));
    d45e_w16(&x2, &x0, &x1, &dst1, stride);
    y1 = _mm256_loadu_si256((const __m256i *)(above + 16 + i++));
    d45e_w16(&y2, &y0, &y1, &dst2, stride);

    x2 = _mm256_loadu_si256((const __m256i *)(above + i));
    d45e_w16(&x0, &x1, &x2, &dst1, stride);
    y2 = _mm256_loadu_si256((const __m256i *)(above + 16 + i++));
    d45e_w16(&y0, &y1, &y2, &dst2, stride);
  } while (i < 15);

  x0 = _mm256_loadu_si256((const __m256i *)(above + 15));
  d45e_w16(&x1, &x2, &x0, &dst1, stride);
  y0 = _mm256_loadu_si256((const __m256i *)(above + 16 + 15));
  d45e_w16(&y1, &y2, &y0, &dst2, stride);

  x1 = _mm256_loadu_si256((const __m256i *)(above + 16));
  d45e_w16(&x2, &x0, &x1, &dst1, stride);
  y1 = _mm256_loadu_si256((const __m256i *)(above + 16 + 16));
  d45e_w16(&y2, &y0, &y1, &dst2, stride);

  x2 = _mm256_loadu_si256((const __m256i *)(above + 17));
  __m256i u = avg3_epu16(&x0, &x1, &x2);
  _mm256_storeu_si256((__m256i *)dst1, u);

  y2 = _mm256_loadu_si256((const __m256i *)(above + 16 + 17));
  y2 = _mm256_insert_epi16(y2, above[47], 15);
  u = avg3_epu16(&y0, &y1, &y2);
  _mm256_storeu_si256((__m256i *)dst2, u);
}

void aom_highbd_d45e_predictor_32x32_avx2(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *above,
                                          const uint16_t *left, int bd) {
  (void)left;
  (void)bd;
  __m256i x0 = _mm256_loadu_si256((const __m256i *)above);
  __m256i x1 = _mm256_loadu_si256((const __m256i *)(above + 1));
  __m256i x2 = _mm256_loadu_si256((const __m256i *)(above + 2));
  __m256i y0 = _mm256_loadu_si256((const __m256i *)(above + 16));
  __m256i y1 = _mm256_loadu_si256((const __m256i *)(above + 17));
  __m256i y2 = _mm256_loadu_si256((const __m256i *)(above + 18));

  uint16_t *dst1 = dst;
  uint16_t *dst2 = dst + 16;

  d45e_w16(&x0, &x1, &x2, &dst1, stride);
  d45e_w16(&y0, &y1, &y2, &dst2, stride);

  int i = 3;
  do {
    x0 = _mm256_loadu_si256((const __m256i *)(above + i));
    d45e_w16(&x1, &x2, &x0, &dst1, stride);
    y0 = _mm256_loadu_si256((const __m256i *)(above + 16 + i++));
    d45e_w16(&y1, &y2, &y0, &dst2, stride);

    x1 = _mm256_loadu_si256((const __m256i *)(above + i));
    d45e_w16(&x2, &x0, &x1, &dst1, stride);
    y1 = _mm256_loadu_si256((const __m256i *)(above + 16 + i++));
    d45e_w16(&y2, &y0, &y1, &dst2, stride);

    x2 = _mm256_loadu_si256((const __m256i *)(above + i));
    d45e_w16(&x0, &x1, &x2, &dst1, stride);
    y2 = _mm256_loadu_si256((const __m256i *)(above + 16 + i++));
    d45e_w16(&y0, &y1, &y2, &dst2, stride);
  } while (i < 33);

  x0 = _mm256_loadu_si256((const __m256i *)(above + 33));
  __m256i u = avg3_epu16(&x1, &x2, &x0);
  _mm256_storeu_si256((__m256i *)dst1, u);

  y0 = _mm256_loadu_si256((const __m256i *)(above + 16 + 33));
  y0 = _mm256_insert_epi16(y0, above[63], 15);
  u = avg3_epu16(&y1, &y2, &y0);
  _mm256_storeu_si256((__m256i *)dst2, u);
}
