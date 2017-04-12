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

#include <stdlib.h>
#include <emmintrin.h>
#include <tmmintrin.h>

#include "aom_ports/mem.h"
#include "./aom_config.h"
#include "aom/aom_integer.h"

static INLINE __m128i width8_load_2rows(const uint8_t *ptr, int stride) {
  __m128i temp1 = _mm_loadl_epi64((const __m128i *)ptr);
  __m128i temp2 = _mm_loadl_epi64((const __m128i *)(ptr + stride));
  return _mm_unpacklo_epi64(temp1, temp2);
}

static INLINE __m128i width4_load_4rows(const uint8_t *ptr, int stride) {
  __m128i temp1 = _mm_cvtsi32_si128(*(const uint32_t *)ptr);
  __m128i temp2 = _mm_cvtsi32_si128(*(const uint32_t *)(ptr + stride));
  __m128i temp3 = _mm_unpacklo_epi32(temp1, temp2);
  temp1 = _mm_cvtsi32_si128(*(const uint32_t *)(ptr + stride * 2));
  temp2 = _mm_cvtsi32_si128(*(const uint32_t *)(ptr + stride * 3));
  temp1 = _mm_unpacklo_epi32(temp1, temp2);
  return _mm_unpacklo_epi64(temp3, temp1);
}

static INLINE unsigned int masked_sad_ssse3(const uint8_t *a_ptr, int a_stride,
                                            const uint8_t *b_ptr, int b_stride,
                                            const uint8_t *m_ptr, int m_stride,
                                            int width, int height);

static INLINE unsigned int masked_sad8xh_ssse3(
    const uint8_t *a_ptr, int a_stride, const uint8_t *b_ptr, int b_stride,
    const uint8_t *m_ptr, int m_stride, int height);

static INLINE unsigned int masked_sad4xh_ssse3(
    const uint8_t *a_ptr, int a_stride, const uint8_t *b_ptr, int b_stride,
    const uint8_t *m_ptr, int m_stride, int height);

#define MASKSADMXN_SSSE3(m, n)                                                 \
  unsigned int aom_masked_sad##m##x##n##_ssse3(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride,  \
      const uint8_t *msk, int msk_stride) {                                    \
    return masked_sad_ssse3(src, src_stride, ref, ref_stride, msk, msk_stride, \
                            m, n);                                             \
  }

#if CONFIG_EXT_PARTITION
MASKSADMXN_SSSE3(128, 128)
MASKSADMXN_SSSE3(128, 64)
MASKSADMXN_SSSE3(64, 128)
#endif  // CONFIG_EXT_PARTITION
MASKSADMXN_SSSE3(64, 64)
MASKSADMXN_SSSE3(64, 32)
MASKSADMXN_SSSE3(32, 64)
MASKSADMXN_SSSE3(32, 32)
MASKSADMXN_SSSE3(32, 16)
MASKSADMXN_SSSE3(16, 32)
MASKSADMXN_SSSE3(16, 16)
MASKSADMXN_SSSE3(16, 8)

#define MASKSAD8XN_SSSE3(n)                                                   \
  unsigned int aom_masked_sad8x##n##_ssse3(                                   \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *msk, int msk_stride) {                                   \
    return masked_sad8xh_ssse3(src, src_stride, ref, ref_stride, msk,         \
                               msk_stride, n);                                \
  }

MASKSAD8XN_SSSE3(16)
MASKSAD8XN_SSSE3(8)
MASKSAD8XN_SSSE3(4)

#define MASKSAD4XN_SSSE3(n)                                                   \
  unsigned int aom_masked_sad4x##n##_ssse3(                                   \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *msk, int msk_stride) {                                   \
    return masked_sad4xh_ssse3(src, src_stride, ref, ref_stride, msk,         \
                               msk_stride, n);                                \
  }

MASKSAD4XN_SSSE3(8)
MASKSAD4XN_SSSE3(4)

// For width a multiple of 16
// Assumes values in m are <=64
static INLINE unsigned int masked_sad_ssse3(const uint8_t *a_ptr, int a_stride,
                                            const uint8_t *b_ptr, int b_stride,
                                            const uint8_t *m_ptr, int m_stride,
                                            int width, int height) {
  int y, x;
  __m128i a, b, m, temp1, temp2;
  __m128i res = _mm_setzero_si128();
  __m128i one = _mm_set1_epi16(1);
  // For each row
  for (y = 0; y < height; y++) {
    // Covering the full width
    for (x = 0; x < width; x += 16) {
      // Load a, b, m in xmm registers
      a = _mm_loadu_si128((const __m128i *)(a_ptr + x));
      b = _mm_loadu_si128((const __m128i *)(b_ptr + x));
      m = _mm_loadu_si128((const __m128i *)(m_ptr + x));

      // Calculate the difference between a & b
      temp1 = _mm_subs_epu8(a, b);
      temp2 = _mm_subs_epu8(b, a);
      temp1 = _mm_or_si128(temp1, temp2);

      // Multiply by m and add together
      temp2 = _mm_maddubs_epi16(temp1, m);
      // Pad out row result to 32 bit integers & add to running total
      res = _mm_add_epi32(res, _mm_madd_epi16(temp2, one));
    }
    // Move onto the next row
    a_ptr += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  // sad = (sad + 31) >> 6;
  return (_mm_cvtsi128_si32(res) + 31) >> 6;
}

static INLINE unsigned int masked_sad8xh_ssse3(
    const uint8_t *a_ptr, int a_stride, const uint8_t *b_ptr, int b_stride,
    const uint8_t *m_ptr, int m_stride, int height) {
  int y;
  __m128i a, b, m, temp1, temp2, row_res;
  __m128i res = _mm_setzero_si128();
  __m128i one = _mm_set1_epi16(1);
  // Add the masked SAD for 2 rows at a time
  for (y = 0; y < height; y += 2) {
    // Load a, b, m in xmm registers
    a = width8_load_2rows(a_ptr, a_stride);
    b = width8_load_2rows(b_ptr, b_stride);
    m = width8_load_2rows(m_ptr, m_stride);

    // Calculate the difference between a & b
    temp1 = _mm_subs_epu8(a, b);
    temp2 = _mm_subs_epu8(b, a);
    temp1 = _mm_or_si128(temp1, temp2);

    // Multiply by m and add together
    row_res = _mm_maddubs_epi16(temp1, m);

    // Pad out row result to 32 bit integers & add to running total
    res = _mm_add_epi32(res, _mm_madd_epi16(row_res, one));

    // Move onto the next rows
    a_ptr += a_stride * 2;
    b_ptr += b_stride * 2;
    m_ptr += m_stride * 2;
  }
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  // sad = (sad + 31) >> 6;
  return (_mm_cvtsi128_si32(res) + 31) >> 6;
}

static INLINE unsigned int masked_sad4xh_ssse3(
    const uint8_t *a_ptr, int a_stride, const uint8_t *b_ptr, int b_stride,
    const uint8_t *m_ptr, int m_stride, int height) {
  int y;
  __m128i a, b, m, temp1, temp2, row_res;
  __m128i res = _mm_setzero_si128();
  __m128i one = _mm_set1_epi16(1);
  // Add the masked SAD for 4 rows at a time
  for (y = 0; y < height; y += 4) {
    // Load a, b, m in xmm registers
    a = width4_load_4rows(a_ptr, a_stride);
    b = width4_load_4rows(b_ptr, b_stride);
    m = width4_load_4rows(m_ptr, m_stride);

    // Calculate the difference between a & b
    temp1 = _mm_subs_epu8(a, b);
    temp2 = _mm_subs_epu8(b, a);
    temp1 = _mm_or_si128(temp1, temp2);

    // Multiply by m and add together
    row_res = _mm_maddubs_epi16(temp1, m);

    // Pad out row result to 32 bit integers & add to running total
    res = _mm_add_epi32(res, _mm_madd_epi16(row_res, one));

    // Move onto the next rows
    a_ptr += a_stride * 4;
    b_ptr += b_stride * 4;
    m_ptr += m_stride * 4;
  }
  // Pad out row result to 32 bit integers & add to running total
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  // sad = (sad + 31) >> 6;
  return (_mm_cvtsi128_si32(res) + 31) >> 6;
}

#if CONFIG_HIGHBITDEPTH
static INLINE __m128i highbd_width4_load_2rows(const uint16_t *ptr,
                                               int stride) {
  __m128i temp1 = _mm_loadl_epi64((const __m128i *)ptr);
  __m128i temp2 = _mm_loadl_epi64((const __m128i *)(ptr + stride));
  return _mm_unpacklo_epi64(temp1, temp2);
}

static INLINE unsigned int highbd_masked_sad_ssse3(
    const uint8_t *a8_ptr, int a_stride, const uint8_t *b8_ptr, int b_stride,
    const uint8_t *m_ptr, int m_stride, int width, int height);

static INLINE unsigned int highbd_masked_sad4xh_ssse3(
    const uint8_t *a8_ptr, int a_stride, const uint8_t *b8_ptr, int b_stride,
    const uint8_t *m_ptr, int m_stride, int height);

#define HIGHBD_MASKSADMXN_SSSE3(m, n)                                         \
  unsigned int aom_highbd_masked_sad##m##x##n##_ssse3(                        \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *msk, int msk_stride) {                                   \
    return highbd_masked_sad_ssse3(src, src_stride, ref, ref_stride, msk,     \
                                   msk_stride, m, n);                         \
  }

#if CONFIG_EXT_PARTITION
HIGHBD_MASKSADMXN_SSSE3(128, 128)
HIGHBD_MASKSADMXN_SSSE3(128, 64)
HIGHBD_MASKSADMXN_SSSE3(64, 128)
#endif  // CONFIG_EXT_PARTITION
HIGHBD_MASKSADMXN_SSSE3(64, 64)
HIGHBD_MASKSADMXN_SSSE3(64, 32)
HIGHBD_MASKSADMXN_SSSE3(32, 64)
HIGHBD_MASKSADMXN_SSSE3(32, 32)
HIGHBD_MASKSADMXN_SSSE3(32, 16)
HIGHBD_MASKSADMXN_SSSE3(16, 32)
HIGHBD_MASKSADMXN_SSSE3(16, 16)
HIGHBD_MASKSADMXN_SSSE3(16, 8)
HIGHBD_MASKSADMXN_SSSE3(8, 16)
HIGHBD_MASKSADMXN_SSSE3(8, 8)
HIGHBD_MASKSADMXN_SSSE3(8, 4)

#define HIGHBD_MASKSAD4XN_SSSE3(n)                                            \
  unsigned int aom_highbd_masked_sad4x##n##_ssse3(                            \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *msk, int msk_stride) {                                   \
    return highbd_masked_sad4xh_ssse3(src, src_stride, ref, ref_stride, msk,  \
                                      msk_stride, n);                         \
  }

HIGHBD_MASKSAD4XN_SSSE3(8)
HIGHBD_MASKSAD4XN_SSSE3(4)

// For width a multiple of 8
// Assumes values in m are <=64
static INLINE unsigned int highbd_masked_sad_ssse3(
    const uint8_t *a8_ptr, int a_stride, const uint8_t *b8_ptr, int b_stride,
    const uint8_t *m_ptr, int m_stride, int width, int height) {
  int y, x;
  __m128i a, b, m, temp1, temp2;
  const uint16_t *a_ptr = CONVERT_TO_SHORTPTR(a8_ptr);
  const uint16_t *b_ptr = CONVERT_TO_SHORTPTR(b8_ptr);
  __m128i res = _mm_setzero_si128();
  // For each row
  for (y = 0; y < height; y++) {
    // Covering the full width
    for (x = 0; x < width; x += 8) {
      // Load a, b, m in xmm registers
      a = _mm_loadu_si128((const __m128i *)(a_ptr + x));
      b = _mm_loadu_si128((const __m128i *)(b_ptr + x));
      m = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(m_ptr + x)),
                            _mm_setzero_si128());

      // Calculate the difference between a & b
      temp1 = _mm_subs_epu16(a, b);
      temp2 = _mm_subs_epu16(b, a);
      temp1 = _mm_or_si128(temp1, temp2);

      // Add result of multiplying by m and add pairs together to running total
      res = _mm_add_epi32(res, _mm_madd_epi16(temp1, m));
    }
    // Move onto the next row
    a_ptr += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  // sad = (sad + 31) >> 6;
  return (_mm_cvtsi128_si32(res) + 31) >> 6;
}

static INLINE unsigned int highbd_masked_sad4xh_ssse3(
    const uint8_t *a8_ptr, int a_stride, const uint8_t *b8_ptr, int b_stride,
    const uint8_t *m_ptr, int m_stride, int height) {
  int y;
  __m128i a, b, m, temp1, temp2;
  const uint16_t *a_ptr = CONVERT_TO_SHORTPTR(a8_ptr);
  const uint16_t *b_ptr = CONVERT_TO_SHORTPTR(b8_ptr);
  __m128i res = _mm_setzero_si128();
  // Add the masked SAD for 2 rows at a time
  for (y = 0; y < height; y += 2) {
    // Load a, b, m in xmm registers
    a = highbd_width4_load_2rows(a_ptr, a_stride);
    b = highbd_width4_load_2rows(b_ptr, b_stride);
    temp1 = _mm_loadl_epi64((const __m128i *)m_ptr);
    temp2 = _mm_loadl_epi64((const __m128i *)(m_ptr + m_stride));
    m = _mm_unpacklo_epi8(_mm_unpacklo_epi32(temp1, temp2),
                          _mm_setzero_si128());

    // Calculate the difference between a & b
    temp1 = _mm_subs_epu16(a, b);
    temp2 = _mm_subs_epu16(b, a);
    temp1 = _mm_or_si128(temp1, temp2);

    // Multiply by m and add together
    res = _mm_add_epi32(res, _mm_madd_epi16(temp1, m));

    // Move onto the next rows
    a_ptr += a_stride * 2;
    b_ptr += b_stride * 2;
    m_ptr += m_stride * 2;
  }
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  res = _mm_hadd_epi32(res, _mm_setzero_si128());
  // sad = (sad + 31) >> 6;
  return (_mm_cvtsi128_si32(res) + 31) >> 6;
}
#endif  // CONFIG_HIGHBITDEPTH
