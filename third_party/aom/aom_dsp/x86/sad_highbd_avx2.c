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

#include <immintrin.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

// SAD
static INLINE unsigned int get_sad_from_mm256_epi32(const __m256i *v) {
  // input 8 32-bit summation
  __m128i lo128, hi128;
  __m256i u = _mm256_srli_si256(*v, 8);
  u = _mm256_add_epi32(u, *v);

  // 4 32-bit summation
  hi128 = _mm256_extracti128_si256(u, 1);
  lo128 = _mm256_castsi256_si128(u);
  lo128 = _mm_add_epi32(hi128, lo128);

  // 2 32-bit summation
  hi128 = _mm_srli_si128(lo128, 4);
  lo128 = _mm_add_epi32(lo128, hi128);

  return (unsigned int)_mm_cvtsi128_si32(lo128);
}

unsigned int aom_highbd_sad16x8_avx2(const uint8_t *src, int src_stride,
                                     const uint8_t *ref, int ref_stride) {
  const uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);
  const uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(ref);

  // first 4 rows
  __m256i s0 = _mm256_loadu_si256((const __m256i *)src_ptr);
  __m256i s1 = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride));
  __m256i s2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 2 * src_stride));
  __m256i s3 = _mm256_loadu_si256((const __m256i *)(src_ptr + 3 * src_stride));

  __m256i r0 = _mm256_loadu_si256((const __m256i *)ref_ptr);
  __m256i r1 = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride));
  __m256i r2 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 2 * ref_stride));
  __m256i r3 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 3 * ref_stride));

  __m256i u0 = _mm256_sub_epi16(s0, r0);
  __m256i u1 = _mm256_sub_epi16(s1, r1);
  __m256i u2 = _mm256_sub_epi16(s2, r2);
  __m256i u3 = _mm256_sub_epi16(s3, r3);
  __m256i zero = _mm256_setzero_si256();
  __m256i sum0, sum1;

  u0 = _mm256_abs_epi16(u0);
  u1 = _mm256_abs_epi16(u1);
  u2 = _mm256_abs_epi16(u2);
  u3 = _mm256_abs_epi16(u3);

  sum0 = _mm256_add_epi16(u0, u1);
  sum0 = _mm256_add_epi16(sum0, u2);
  sum0 = _mm256_add_epi16(sum0, u3);

  // second 4 rows
  src_ptr += src_stride << 2;
  ref_ptr += ref_stride << 2;
  s0 = _mm256_loadu_si256((const __m256i *)src_ptr);
  s1 = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride));
  s2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 2 * src_stride));
  s3 = _mm256_loadu_si256((const __m256i *)(src_ptr + 3 * src_stride));

  r0 = _mm256_loadu_si256((const __m256i *)ref_ptr);
  r1 = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride));
  r2 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 2 * ref_stride));
  r3 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 3 * ref_stride));

  u0 = _mm256_sub_epi16(s0, r0);
  u1 = _mm256_sub_epi16(s1, r1);
  u2 = _mm256_sub_epi16(s2, r2);
  u3 = _mm256_sub_epi16(s3, r3);

  u0 = _mm256_abs_epi16(u0);
  u1 = _mm256_abs_epi16(u1);
  u2 = _mm256_abs_epi16(u2);
  u3 = _mm256_abs_epi16(u3);

  sum1 = _mm256_add_epi16(u0, u1);
  sum1 = _mm256_add_epi16(sum1, u2);
  sum1 = _mm256_add_epi16(sum1, u3);

  // find out the SAD
  s0 = _mm256_unpacklo_epi16(sum0, zero);
  s1 = _mm256_unpackhi_epi16(sum0, zero);
  r0 = _mm256_unpacklo_epi16(sum1, zero);
  r1 = _mm256_unpackhi_epi16(sum1, zero);
  s0 = _mm256_add_epi32(s0, s1);
  r0 = _mm256_add_epi32(r0, r1);
  sum0 = _mm256_add_epi32(s0, r0);
  // 8 32-bit summation

  return (unsigned int)get_sad_from_mm256_epi32(&sum0);
}

unsigned int aom_highbd_sad16x16_avx2(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride) {
  const uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);
  const uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(ref);
  __m256i s0, s1, s2, s3, r0, r1, r2, r3, u0, u1, u2, u3;
  __m256i sum0;
  __m256i sum = _mm256_setzero_si256();
  const __m256i zero = _mm256_setzero_si256();
  int row = 0;

  // Loop for every 4 rows
  while (row < 16) {
    s0 = _mm256_loadu_si256((const __m256i *)src_ptr);
    s1 = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride));
    s2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 2 * src_stride));
    s3 = _mm256_loadu_si256((const __m256i *)(src_ptr + 3 * src_stride));

    r0 = _mm256_loadu_si256((const __m256i *)ref_ptr);
    r1 = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride));
    r2 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 2 * ref_stride));
    r3 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 3 * ref_stride));

    u0 = _mm256_sub_epi16(s0, r0);
    u1 = _mm256_sub_epi16(s1, r1);
    u2 = _mm256_sub_epi16(s2, r2);
    u3 = _mm256_sub_epi16(s3, r3);

    u0 = _mm256_abs_epi16(u0);
    u1 = _mm256_abs_epi16(u1);
    u2 = _mm256_abs_epi16(u2);
    u3 = _mm256_abs_epi16(u3);

    sum0 = _mm256_add_epi16(u0, u1);
    sum0 = _mm256_add_epi16(sum0, u2);
    sum0 = _mm256_add_epi16(sum0, u3);

    s0 = _mm256_unpacklo_epi16(sum0, zero);
    s1 = _mm256_unpackhi_epi16(sum0, zero);
    sum = _mm256_add_epi32(sum, s0);
    sum = _mm256_add_epi32(sum, s1);
    // 8 32-bit summation

    row += 4;
    src_ptr += src_stride << 2;
    ref_ptr += ref_stride << 2;
  }
  return get_sad_from_mm256_epi32(&sum);
}

static void sad32x4(const uint16_t *src_ptr, int src_stride,
                    const uint16_t *ref_ptr, int ref_stride,
                    const uint16_t *sec_ptr, __m256i *sad_acc) {
  __m256i s0, s1, s2, s3, r0, r1, r2, r3;
  const __m256i zero = _mm256_setzero_si256();
  int row_sections = 0;

  while (row_sections < 2) {
    s0 = _mm256_loadu_si256((const __m256i *)src_ptr);
    s1 = _mm256_loadu_si256((const __m256i *)(src_ptr + 16));
    s2 = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride));
    s3 = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride + 16));

    r0 = _mm256_loadu_si256((const __m256i *)ref_ptr);
    r1 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 16));
    r2 = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride));
    r3 = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride + 16));

    if (sec_ptr) {
      r0 = _mm256_avg_epu16(r0, _mm256_loadu_si256((const __m256i *)sec_ptr));
      r1 = _mm256_avg_epu16(
          r1, _mm256_loadu_si256((const __m256i *)(sec_ptr + 16)));
      r2 = _mm256_avg_epu16(
          r2, _mm256_loadu_si256((const __m256i *)(sec_ptr + 32)));
      r3 = _mm256_avg_epu16(
          r3, _mm256_loadu_si256((const __m256i *)(sec_ptr + 48)));
    }
    s0 = _mm256_sub_epi16(s0, r0);
    s1 = _mm256_sub_epi16(s1, r1);
    s2 = _mm256_sub_epi16(s2, r2);
    s3 = _mm256_sub_epi16(s3, r3);

    s0 = _mm256_abs_epi16(s0);
    s1 = _mm256_abs_epi16(s1);
    s2 = _mm256_abs_epi16(s2);
    s3 = _mm256_abs_epi16(s3);

    s0 = _mm256_add_epi16(s0, s1);
    s0 = _mm256_add_epi16(s0, s2);
    s0 = _mm256_add_epi16(s0, s3);

    r0 = _mm256_unpacklo_epi16(s0, zero);
    r1 = _mm256_unpackhi_epi16(s0, zero);

    r0 = _mm256_add_epi32(r0, r1);
    *sad_acc = _mm256_add_epi32(*sad_acc, r0);

    row_sections += 1;
    src_ptr += src_stride << 1;
    ref_ptr += ref_stride << 1;
    if (sec_ptr) sec_ptr += 32 << 1;
  }
}

unsigned int aom_highbd_sad32x16_avx2(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  const int left_shift = 2;
  int row_section = 0;

  while (row_section < 4) {
    sad32x4(srcp, src_stride, refp, ref_stride, NULL, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
    row_section += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad16x32_avx2(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride) {
  uint32_t sum = aom_highbd_sad16x16_avx2(src, src_stride, ref, ref_stride);
  src += src_stride << 4;
  ref += ref_stride << 4;
  sum += aom_highbd_sad16x16_avx2(src, src_stride, ref, ref_stride);
  return sum;
}

unsigned int aom_highbd_sad32x32_avx2(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride) {
  uint32_t sum = aom_highbd_sad32x16_avx2(src, src_stride, ref, ref_stride);
  src += src_stride << 4;
  ref += ref_stride << 4;
  sum += aom_highbd_sad32x16_avx2(src, src_stride, ref, ref_stride);
  return sum;
}

unsigned int aom_highbd_sad32x64_avx2(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride) {
  uint32_t sum = aom_highbd_sad32x32_avx2(src, src_stride, ref, ref_stride);
  src += src_stride << 5;
  ref += ref_stride << 5;
  sum += aom_highbd_sad32x32_avx2(src, src_stride, ref, ref_stride);
  return sum;
}

static void sad64x2(const uint16_t *src_ptr, int src_stride,
                    const uint16_t *ref_ptr, int ref_stride,
                    const uint16_t *sec_ptr, __m256i *sad_acc) {
  __m256i s[8], r[8];
  const __m256i zero = _mm256_setzero_si256();

  s[0] = _mm256_loadu_si256((const __m256i *)src_ptr);
  s[1] = _mm256_loadu_si256((const __m256i *)(src_ptr + 16));
  s[2] = _mm256_loadu_si256((const __m256i *)(src_ptr + 32));
  s[3] = _mm256_loadu_si256((const __m256i *)(src_ptr + 48));
  s[4] = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride));
  s[5] = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride + 16));
  s[6] = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride + 32));
  s[7] = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride + 48));

  r[0] = _mm256_loadu_si256((const __m256i *)ref_ptr);
  r[1] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 16));
  r[2] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 32));
  r[3] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 48));
  r[4] = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride));
  r[5] = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride + 16));
  r[6] = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride + 32));
  r[7] = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride + 48));

  if (sec_ptr) {
    r[0] = _mm256_avg_epu16(r[0], _mm256_loadu_si256((const __m256i *)sec_ptr));
    r[1] = _mm256_avg_epu16(
        r[1], _mm256_loadu_si256((const __m256i *)(sec_ptr + 16)));
    r[2] = _mm256_avg_epu16(
        r[2], _mm256_loadu_si256((const __m256i *)(sec_ptr + 32)));
    r[3] = _mm256_avg_epu16(
        r[3], _mm256_loadu_si256((const __m256i *)(sec_ptr + 48)));
    r[4] = _mm256_avg_epu16(
        r[4], _mm256_loadu_si256((const __m256i *)(sec_ptr + 64)));
    r[5] = _mm256_avg_epu16(
        r[5], _mm256_loadu_si256((const __m256i *)(sec_ptr + 80)));
    r[6] = _mm256_avg_epu16(
        r[6], _mm256_loadu_si256((const __m256i *)(sec_ptr + 96)));
    r[7] = _mm256_avg_epu16(
        r[7], _mm256_loadu_si256((const __m256i *)(sec_ptr + 112)));
  }

  s[0] = _mm256_sub_epi16(s[0], r[0]);
  s[1] = _mm256_sub_epi16(s[1], r[1]);
  s[2] = _mm256_sub_epi16(s[2], r[2]);
  s[3] = _mm256_sub_epi16(s[3], r[3]);
  s[4] = _mm256_sub_epi16(s[4], r[4]);
  s[5] = _mm256_sub_epi16(s[5], r[5]);
  s[6] = _mm256_sub_epi16(s[6], r[6]);
  s[7] = _mm256_sub_epi16(s[7], r[7]);

  s[0] = _mm256_abs_epi16(s[0]);
  s[1] = _mm256_abs_epi16(s[1]);
  s[2] = _mm256_abs_epi16(s[2]);
  s[3] = _mm256_abs_epi16(s[3]);
  s[4] = _mm256_abs_epi16(s[4]);
  s[5] = _mm256_abs_epi16(s[5]);
  s[6] = _mm256_abs_epi16(s[6]);
  s[7] = _mm256_abs_epi16(s[7]);

  s[0] = _mm256_add_epi16(s[0], s[1]);
  s[0] = _mm256_add_epi16(s[0], s[2]);
  s[0] = _mm256_add_epi16(s[0], s[3]);

  s[4] = _mm256_add_epi16(s[4], s[5]);
  s[4] = _mm256_add_epi16(s[4], s[6]);
  s[4] = _mm256_add_epi16(s[4], s[7]);

  r[0] = _mm256_unpacklo_epi16(s[0], zero);
  r[1] = _mm256_unpackhi_epi16(s[0], zero);
  r[2] = _mm256_unpacklo_epi16(s[4], zero);
  r[3] = _mm256_unpackhi_epi16(s[4], zero);

  r[0] = _mm256_add_epi32(r[0], r[1]);
  r[0] = _mm256_add_epi32(r[0], r[2]);
  r[0] = _mm256_add_epi32(r[0], r[3]);
  *sad_acc = _mm256_add_epi32(*sad_acc, r[0]);
}

unsigned int aom_highbd_sad64x32_avx2(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  const int left_shift = 1;
  int row_section = 0;

  while (row_section < 16) {
    sad64x2(srcp, src_stride, refp, ref_stride, NULL, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
    row_section += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad64x64_avx2(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride) {
  uint32_t sum = aom_highbd_sad64x32_avx2(src, src_stride, ref, ref_stride);
  src += src_stride << 5;
  ref += ref_stride << 5;
  sum += aom_highbd_sad64x32_avx2(src, src_stride, ref, ref_stride);
  return sum;
}

#if CONFIG_EXT_PARTITION
static void sad128x1(const uint16_t *src_ptr, const uint16_t *ref_ptr,
                     const uint16_t *sec_ptr, __m256i *sad_acc) {
  __m256i s[8], r[8];
  const __m256i zero = _mm256_setzero_si256();

  s[0] = _mm256_loadu_si256((const __m256i *)src_ptr);
  s[1] = _mm256_loadu_si256((const __m256i *)(src_ptr + 16));
  s[2] = _mm256_loadu_si256((const __m256i *)(src_ptr + 32));
  s[3] = _mm256_loadu_si256((const __m256i *)(src_ptr + 48));
  s[4] = _mm256_loadu_si256((const __m256i *)(src_ptr + 64));
  s[5] = _mm256_loadu_si256((const __m256i *)(src_ptr + 80));
  s[6] = _mm256_loadu_si256((const __m256i *)(src_ptr + 96));
  s[7] = _mm256_loadu_si256((const __m256i *)(src_ptr + 112));

  r[0] = _mm256_loadu_si256((const __m256i *)ref_ptr);
  r[1] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 16));
  r[2] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 32));
  r[3] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 48));
  r[4] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 64));
  r[5] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 80));
  r[6] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 96));
  r[7] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 112));

  if (sec_ptr) {
    r[0] = _mm256_avg_epu16(r[0], _mm256_loadu_si256((const __m256i *)sec_ptr));
    r[1] = _mm256_avg_epu16(
        r[1], _mm256_loadu_si256((const __m256i *)(sec_ptr + 16)));
    r[2] = _mm256_avg_epu16(
        r[2], _mm256_loadu_si256((const __m256i *)(sec_ptr + 32)));
    r[3] = _mm256_avg_epu16(
        r[3], _mm256_loadu_si256((const __m256i *)(sec_ptr + 48)));
    r[4] = _mm256_avg_epu16(
        r[4], _mm256_loadu_si256((const __m256i *)(sec_ptr + 64)));
    r[5] = _mm256_avg_epu16(
        r[5], _mm256_loadu_si256((const __m256i *)(sec_ptr + 80)));
    r[6] = _mm256_avg_epu16(
        r[6], _mm256_loadu_si256((const __m256i *)(sec_ptr + 96)));
    r[7] = _mm256_avg_epu16(
        r[7], _mm256_loadu_si256((const __m256i *)(sec_ptr + 112)));
  }

  s[0] = _mm256_sub_epi16(s[0], r[0]);
  s[1] = _mm256_sub_epi16(s[1], r[1]);
  s[2] = _mm256_sub_epi16(s[2], r[2]);
  s[3] = _mm256_sub_epi16(s[3], r[3]);
  s[4] = _mm256_sub_epi16(s[4], r[4]);
  s[5] = _mm256_sub_epi16(s[5], r[5]);
  s[6] = _mm256_sub_epi16(s[6], r[6]);
  s[7] = _mm256_sub_epi16(s[7], r[7]);

  s[0] = _mm256_abs_epi16(s[0]);
  s[1] = _mm256_abs_epi16(s[1]);
  s[2] = _mm256_abs_epi16(s[2]);
  s[3] = _mm256_abs_epi16(s[3]);
  s[4] = _mm256_abs_epi16(s[4]);
  s[5] = _mm256_abs_epi16(s[5]);
  s[6] = _mm256_abs_epi16(s[6]);
  s[7] = _mm256_abs_epi16(s[7]);

  s[0] = _mm256_add_epi16(s[0], s[1]);
  s[0] = _mm256_add_epi16(s[0], s[2]);
  s[0] = _mm256_add_epi16(s[0], s[3]);

  s[4] = _mm256_add_epi16(s[4], s[5]);
  s[4] = _mm256_add_epi16(s[4], s[6]);
  s[4] = _mm256_add_epi16(s[4], s[7]);

  r[0] = _mm256_unpacklo_epi16(s[0], zero);
  r[1] = _mm256_unpackhi_epi16(s[0], zero);
  r[2] = _mm256_unpacklo_epi16(s[4], zero);
  r[3] = _mm256_unpackhi_epi16(s[4], zero);

  r[0] = _mm256_add_epi32(r[0], r[1]);
  r[0] = _mm256_add_epi32(r[0], r[2]);
  r[0] = _mm256_add_epi32(r[0], r[3]);
  *sad_acc = _mm256_add_epi32(*sad_acc, r[0]);
}

unsigned int aom_highbd_sad128x64_avx2(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  int row = 0;
  while (row < 64) {
    sad128x1(srcp, refp, NULL, &sad);
    srcp += src_stride;
    refp += ref_stride;
    row += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad64x128_avx2(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride) {
  uint32_t sum = aom_highbd_sad64x64_avx2(src, src_stride, ref, ref_stride);
  src += src_stride << 6;
  ref += ref_stride << 6;
  sum += aom_highbd_sad64x64_avx2(src, src_stride, ref, ref_stride);
  return sum;
}

unsigned int aom_highbd_sad128x128_avx2(const uint8_t *src, int src_stride,
                                        const uint8_t *ref, int ref_stride) {
  uint32_t sum = aom_highbd_sad128x64_avx2(src, src_stride, ref, ref_stride);
  src += src_stride << 6;
  ref += ref_stride << 6;
  sum += aom_highbd_sad128x64_avx2(src, src_stride, ref, ref_stride);
  return sum;
}
#endif  // CONFIG_EXT_PARTITION

// If sec_ptr = 0, calculate regular SAD. Otherwise, calculate average SAD.
static INLINE void sad16x4(const uint16_t *src_ptr, int src_stride,
                           const uint16_t *ref_ptr, int ref_stride,
                           const uint16_t *sec_ptr, __m256i *sad_acc) {
  __m256i s0, s1, s2, s3, r0, r1, r2, r3;
  const __m256i zero = _mm256_setzero_si256();

  s0 = _mm256_loadu_si256((const __m256i *)src_ptr);
  s1 = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride));
  s2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 2 * src_stride));
  s3 = _mm256_loadu_si256((const __m256i *)(src_ptr + 3 * src_stride));

  r0 = _mm256_loadu_si256((const __m256i *)ref_ptr);
  r1 = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride));
  r2 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 2 * ref_stride));
  r3 = _mm256_loadu_si256((const __m256i *)(ref_ptr + 3 * ref_stride));

  if (sec_ptr) {
    r0 = _mm256_avg_epu16(r0, _mm256_loadu_si256((const __m256i *)sec_ptr));
    r1 = _mm256_avg_epu16(r1,
                          _mm256_loadu_si256((const __m256i *)(sec_ptr + 16)));
    r2 = _mm256_avg_epu16(r2,
                          _mm256_loadu_si256((const __m256i *)(sec_ptr + 32)));
    r3 = _mm256_avg_epu16(r3,
                          _mm256_loadu_si256((const __m256i *)(sec_ptr + 48)));
  }

  s0 = _mm256_sub_epi16(s0, r0);
  s1 = _mm256_sub_epi16(s1, r1);
  s2 = _mm256_sub_epi16(s2, r2);
  s3 = _mm256_sub_epi16(s3, r3);

  s0 = _mm256_abs_epi16(s0);
  s1 = _mm256_abs_epi16(s1);
  s2 = _mm256_abs_epi16(s2);
  s3 = _mm256_abs_epi16(s3);

  s0 = _mm256_add_epi16(s0, s1);
  s0 = _mm256_add_epi16(s0, s2);
  s0 = _mm256_add_epi16(s0, s3);

  r0 = _mm256_unpacklo_epi16(s0, zero);
  r1 = _mm256_unpackhi_epi16(s0, zero);

  r0 = _mm256_add_epi32(r0, r1);
  *sad_acc = _mm256_add_epi32(*sad_acc, r0);
}

unsigned int aom_highbd_sad16x8_avg_avx2(const uint8_t *src, int src_stride,
                                         const uint8_t *ref, int ref_stride,
                                         const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);

  sad16x4(srcp, src_stride, refp, ref_stride, secp, &sad);

  // Next 4 rows
  srcp += src_stride << 2;
  refp += ref_stride << 2;
  secp += 64;
  sad16x4(srcp, src_stride, refp, ref_stride, secp, &sad);
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad16x16_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 3;
  uint32_t sum = aom_highbd_sad16x8_avg_avx2(src, src_stride, ref, ref_stride,
                                             second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 16 << left_shift;
  sum += aom_highbd_sad16x8_avg_avx2(src, src_stride, ref, ref_stride,
                                     second_pred);
  return sum;
}

unsigned int aom_highbd_sad16x32_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 4;
  uint32_t sum = aom_highbd_sad16x16_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 16 << left_shift;
  sum += aom_highbd_sad16x16_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

unsigned int aom_highbd_sad32x16_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  const int left_shift = 2;
  int row_section = 0;

  while (row_section < 4) {
    sad32x4(srcp, src_stride, refp, ref_stride, secp, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
    secp += 32 << left_shift;
    row_section += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad32x32_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 4;
  uint32_t sum = aom_highbd_sad32x16_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 32 << left_shift;
  sum += aom_highbd_sad32x16_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

unsigned int aom_highbd_sad32x64_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 5;
  uint32_t sum = aom_highbd_sad32x32_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 32 << left_shift;
  sum += aom_highbd_sad32x32_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

unsigned int aom_highbd_sad64x32_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  const int left_shift = 1;
  int row_section = 0;

  while (row_section < 16) {
    sad64x2(srcp, src_stride, refp, ref_stride, secp, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
    secp += 64 << left_shift;
    row_section += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad64x64_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 5;
  uint32_t sum = aom_highbd_sad64x32_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 64 << left_shift;
  sum += aom_highbd_sad64x32_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

#if CONFIG_EXT_PARTITION
unsigned int aom_highbd_sad64x128_avg_avx2(const uint8_t *src, int src_stride,
                                           const uint8_t *ref, int ref_stride,
                                           const uint8_t *second_pred) {
  const int left_shift = 6;
  uint32_t sum = aom_highbd_sad64x64_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 64 << left_shift;
  sum += aom_highbd_sad64x64_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

unsigned int aom_highbd_sad128x64_avg_avx2(const uint8_t *src, int src_stride,
                                           const uint8_t *ref, int ref_stride,
                                           const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  int row = 0;
  while (row < 64) {
    sad128x1(srcp, refp, secp, &sad);
    srcp += src_stride;
    refp += ref_stride;
    secp += 16 << 3;
    row += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad128x128_avg_avx2(const uint8_t *src, int src_stride,
                                            const uint8_t *ref, int ref_stride,
                                            const uint8_t *second_pred) {
  unsigned int sum;
  const int left_shift = 6;

  sum = aom_highbd_sad128x64_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 128 << left_shift;
  sum += aom_highbd_sad128x64_avg_avx2(src, src_stride, ref, ref_stride,
                                       second_pred);
  return sum;
}
#endif  // CONFIG_EXT_PARTITION

// SAD 4D
// Combine 4 __m256i vectors to uint32_t result[4]
static INLINE void get_4d_sad_from_mm256_epi32(const __m256i *v,
                                               uint32_t *res) {
  __m256i u0, u1, u2, u3;
#if defined(_MSC_VER) && defined(_M_IX86) && _MSC_VER < 1900
  const __m256i mask = _mm256_setr_epi32(UINT32_MAX, 0, UINT32_MAX, 0,
                                         UINT32_MAX, 0, UINT32_MAX, 0);
#else
  const __m256i mask = _mm256_set1_epi64x(UINT32_MAX);
#endif
  __m128i sad;

  // 8 32-bit summation
  u0 = _mm256_srli_si256(v[0], 4);
  u1 = _mm256_srli_si256(v[1], 4);
  u2 = _mm256_srli_si256(v[2], 4);
  u3 = _mm256_srli_si256(v[3], 4);

  u0 = _mm256_add_epi32(u0, v[0]);
  u1 = _mm256_add_epi32(u1, v[1]);
  u2 = _mm256_add_epi32(u2, v[2]);
  u3 = _mm256_add_epi32(u3, v[3]);

  u0 = _mm256_and_si256(u0, mask);
  u1 = _mm256_and_si256(u1, mask);
  u2 = _mm256_and_si256(u2, mask);
  u3 = _mm256_and_si256(u3, mask);
  // 4 32-bit summation, evenly positioned

  u1 = _mm256_slli_si256(u1, 4);
  u3 = _mm256_slli_si256(u3, 4);

  u0 = _mm256_or_si256(u0, u1);
  u2 = _mm256_or_si256(u2, u3);
  // 8 32-bit summation, interleaved

  u1 = _mm256_unpacklo_epi64(u0, u2);
  u3 = _mm256_unpackhi_epi64(u0, u2);

  u0 = _mm256_add_epi32(u1, u3);
  sad = _mm_add_epi32(_mm256_extractf128_si256(u0, 1),
                      _mm256_castsi256_si128(u0));
  _mm_storeu_si128((__m128i *)res, sad);
}

static void convert_pointers(const uint8_t *const ref8[],
                             const uint16_t *ref[]) {
  ref[0] = CONVERT_TO_SHORTPTR(ref8[0]);
  ref[1] = CONVERT_TO_SHORTPTR(ref8[1]);
  ref[2] = CONVERT_TO_SHORTPTR(ref8[2]);
  ref[3] = CONVERT_TO_SHORTPTR(ref8[3]);
}

static void init_sad(__m256i *s) {
  s[0] = _mm256_setzero_si256();
  s[1] = _mm256_setzero_si256();
  s[2] = _mm256_setzero_si256();
  s[3] = _mm256_setzero_si256();
}

void aom_highbd_sad16x8x4d_avx2(const uint8_t *src, int src_stride,
                                const uint8_t *const ref_array[],
                                int ref_stride, uint32_t *sad_array) {
  __m256i sad_vec[4];
  const uint16_t *refp[4];
  const uint16_t *keep = CONVERT_TO_SHORTPTR(src);
  const uint16_t *srcp;
  const int shift_for_4_rows = 2;
  int i;

  init_sad(sad_vec);
  convert_pointers(ref_array, refp);

  for (i = 0; i < 4; ++i) {
    srcp = keep;
    sad16x4(srcp, src_stride, refp[i], ref_stride, 0, &sad_vec[i]);
    srcp += src_stride << shift_for_4_rows;
    refp[i] += ref_stride << shift_for_4_rows;
    sad16x4(srcp, src_stride, refp[i], ref_stride, 0, &sad_vec[i]);
  }
  get_4d_sad_from_mm256_epi32(sad_vec, sad_array);
}

void aom_highbd_sad16x16x4d_avx2(const uint8_t *src, int src_stride,
                                 const uint8_t *const ref_array[],
                                 int ref_stride, uint32_t *sad_array) {
  uint32_t first8rows[4];
  uint32_t second8rows[4];
  const uint8_t *ref[4];
  const int shift_for_8_rows = 3;

  ref[0] = ref_array[0];
  ref[1] = ref_array[1];
  ref[2] = ref_array[2];
  ref[3] = ref_array[3];

  aom_highbd_sad16x8x4d_avx2(src, src_stride, ref, ref_stride, first8rows);
  src += src_stride << shift_for_8_rows;
  ref[0] += ref_stride << shift_for_8_rows;
  ref[1] += ref_stride << shift_for_8_rows;
  ref[2] += ref_stride << shift_for_8_rows;
  ref[3] += ref_stride << shift_for_8_rows;
  aom_highbd_sad16x8x4d_avx2(src, src_stride, ref, ref_stride, second8rows);
  sad_array[0] = first8rows[0] + second8rows[0];
  sad_array[1] = first8rows[1] + second8rows[1];
  sad_array[2] = first8rows[2] + second8rows[2];
  sad_array[3] = first8rows[3] + second8rows[3];
}

void aom_highbd_sad16x32x4d_avx2(const uint8_t *src, int src_stride,
                                 const uint8_t *const ref_array[],
                                 int ref_stride, uint32_t *sad_array) {
  uint32_t first_half[4];
  uint32_t second_half[4];
  const uint8_t *ref[4];
  const int shift_for_rows = 4;

  ref[0] = ref_array[0];
  ref[1] = ref_array[1];
  ref[2] = ref_array[2];
  ref[3] = ref_array[3];

  aom_highbd_sad16x16x4d_avx2(src, src_stride, ref, ref_stride, first_half);
  src += src_stride << shift_for_rows;
  ref[0] += ref_stride << shift_for_rows;
  ref[1] += ref_stride << shift_for_rows;
  ref[2] += ref_stride << shift_for_rows;
  ref[3] += ref_stride << shift_for_rows;
  aom_highbd_sad16x16x4d_avx2(src, src_stride, ref, ref_stride, second_half);
  sad_array[0] = first_half[0] + second_half[0];
  sad_array[1] = first_half[1] + second_half[1];
  sad_array[2] = first_half[2] + second_half[2];
  sad_array[3] = first_half[3] + second_half[3];
}

void aom_highbd_sad32x16x4d_avx2(const uint8_t *src, int src_stride,
                                 const uint8_t *const ref_array[],
                                 int ref_stride, uint32_t *sad_array) {
  __m256i sad_vec[4];
  const uint16_t *refp[4];
  const uint16_t *keep = CONVERT_TO_SHORTPTR(src);
  const uint16_t *srcp;
  const int shift_for_4_rows = 2;
  int i;
  int rows_section;

  init_sad(sad_vec);
  convert_pointers(ref_array, refp);

  for (i = 0; i < 4; ++i) {
    srcp = keep;
    rows_section = 0;
    while (rows_section < 4) {
      sad32x4(srcp, src_stride, refp[i], ref_stride, 0, &sad_vec[i]);
      srcp += src_stride << shift_for_4_rows;
      refp[i] += ref_stride << shift_for_4_rows;
      rows_section++;
    }
  }
  get_4d_sad_from_mm256_epi32(sad_vec, sad_array);
}

void aom_highbd_sad32x32x4d_avx2(const uint8_t *src, int src_stride,
                                 const uint8_t *const ref_array[],
                                 int ref_stride, uint32_t *sad_array) {
  uint32_t first_half[4];
  uint32_t second_half[4];
  const uint8_t *ref[4];
  const int shift_for_rows = 4;

  ref[0] = ref_array[0];
  ref[1] = ref_array[1];
  ref[2] = ref_array[2];
  ref[3] = ref_array[3];

  aom_highbd_sad32x16x4d_avx2(src, src_stride, ref, ref_stride, first_half);
  src += src_stride << shift_for_rows;
  ref[0] += ref_stride << shift_for_rows;
  ref[1] += ref_stride << shift_for_rows;
  ref[2] += ref_stride << shift_for_rows;
  ref[3] += ref_stride << shift_for_rows;
  aom_highbd_sad32x16x4d_avx2(src, src_stride, ref, ref_stride, second_half);
  sad_array[0] = first_half[0] + second_half[0];
  sad_array[1] = first_half[1] + second_half[1];
  sad_array[2] = first_half[2] + second_half[2];
  sad_array[3] = first_half[3] + second_half[3];
}

void aom_highbd_sad32x64x4d_avx2(const uint8_t *src, int src_stride,
                                 const uint8_t *const ref_array[],
                                 int ref_stride, uint32_t *sad_array) {
  uint32_t first_half[4];
  uint32_t second_half[4];
  const uint8_t *ref[4];
  const int shift_for_rows = 5;

  ref[0] = ref_array[0];
  ref[1] = ref_array[1];
  ref[2] = ref_array[2];
  ref[3] = ref_array[3];

  aom_highbd_sad32x32x4d_avx2(src, src_stride, ref, ref_stride, first_half);
  src += src_stride << shift_for_rows;
  ref[0] += ref_stride << shift_for_rows;
  ref[1] += ref_stride << shift_for_rows;
  ref[2] += ref_stride << shift_for_rows;
  ref[3] += ref_stride << shift_for_rows;
  aom_highbd_sad32x32x4d_avx2(src, src_stride, ref, ref_stride, second_half);
  sad_array[0] = first_half[0] + second_half[0];
  sad_array[1] = first_half[1] + second_half[1];
  sad_array[2] = first_half[2] + second_half[2];
  sad_array[3] = first_half[3] + second_half[3];
}

void aom_highbd_sad64x32x4d_avx2(const uint8_t *src, int src_stride,
                                 const uint8_t *const ref_array[],
                                 int ref_stride, uint32_t *sad_array) {
  __m256i sad_vec[4];
  const uint16_t *refp[4];
  const uint16_t *keep = CONVERT_TO_SHORTPTR(src);
  const uint16_t *srcp;
  const int shift_for_rows = 1;
  int i;
  int rows_section;

  init_sad(sad_vec);
  convert_pointers(ref_array, refp);

  for (i = 0; i < 4; ++i) {
    srcp = keep;
    rows_section = 0;
    while (rows_section < 16) {
      sad64x2(srcp, src_stride, refp[i], ref_stride, NULL, &sad_vec[i]);
      srcp += src_stride << shift_for_rows;
      refp[i] += ref_stride << shift_for_rows;
      rows_section++;
    }
  }
  get_4d_sad_from_mm256_epi32(sad_vec, sad_array);
}

void aom_highbd_sad64x64x4d_avx2(const uint8_t *src, int src_stride,
                                 const uint8_t *const ref_array[],
                                 int ref_stride, uint32_t *sad_array) {
  uint32_t first_half[4];
  uint32_t second_half[4];
  const uint8_t *ref[4];
  const int shift_for_rows = 5;

  ref[0] = ref_array[0];
  ref[1] = ref_array[1];
  ref[2] = ref_array[2];
  ref[3] = ref_array[3];

  aom_highbd_sad64x32x4d_avx2(src, src_stride, ref, ref_stride, first_half);
  src += src_stride << shift_for_rows;
  ref[0] += ref_stride << shift_for_rows;
  ref[1] += ref_stride << shift_for_rows;
  ref[2] += ref_stride << shift_for_rows;
  ref[3] += ref_stride << shift_for_rows;
  aom_highbd_sad64x32x4d_avx2(src, src_stride, ref, ref_stride, second_half);
  sad_array[0] = first_half[0] + second_half[0];
  sad_array[1] = first_half[1] + second_half[1];
  sad_array[2] = first_half[2] + second_half[2];
  sad_array[3] = first_half[3] + second_half[3];
}

#if CONFIG_EXT_PARTITION
void aom_highbd_sad64x128x4d_avx2(const uint8_t *src, int src_stride,
                                  const uint8_t *const ref_array[],
                                  int ref_stride, uint32_t *sad_array) {
  uint32_t first_half[4];
  uint32_t second_half[4];
  const uint8_t *ref[4];
  const int shift_for_rows = 6;

  ref[0] = ref_array[0];
  ref[1] = ref_array[1];
  ref[2] = ref_array[2];
  ref[3] = ref_array[3];

  aom_highbd_sad64x64x4d_avx2(src, src_stride, ref, ref_stride, first_half);
  src += src_stride << shift_for_rows;
  ref[0] += ref_stride << shift_for_rows;
  ref[1] += ref_stride << shift_for_rows;
  ref[2] += ref_stride << shift_for_rows;
  ref[3] += ref_stride << shift_for_rows;
  aom_highbd_sad64x64x4d_avx2(src, src_stride, ref, ref_stride, second_half);
  sad_array[0] = first_half[0] + second_half[0];
  sad_array[1] = first_half[1] + second_half[1];
  sad_array[2] = first_half[2] + second_half[2];
  sad_array[3] = first_half[3] + second_half[3];
}

void aom_highbd_sad128x64x4d_avx2(const uint8_t *src, int src_stride,
                                  const uint8_t *const ref_array[],
                                  int ref_stride, uint32_t *sad_array) {
  __m256i sad_vec[4];
  const uint16_t *refp[4];
  const uint16_t *keep = CONVERT_TO_SHORTPTR(src);
  const uint16_t *srcp;
  int i;
  int rows_section;

  init_sad(sad_vec);
  convert_pointers(ref_array, refp);

  for (i = 0; i < 4; ++i) {
    srcp = keep;
    rows_section = 0;
    while (rows_section < 64) {
      sad128x1(srcp, refp[i], NULL, &sad_vec[i]);
      srcp += src_stride;
      refp[i] += ref_stride;
      rows_section++;
    }
  }
  get_4d_sad_from_mm256_epi32(sad_vec, sad_array);
}

void aom_highbd_sad128x128x4d_avx2(const uint8_t *src, int src_stride,
                                   const uint8_t *const ref_array[],
                                   int ref_stride, uint32_t *sad_array) {
  uint32_t first_half[4];
  uint32_t second_half[4];
  const uint8_t *ref[4];
  const int shift_for_rows = 6;

  ref[0] = ref_array[0];
  ref[1] = ref_array[1];
  ref[2] = ref_array[2];
  ref[3] = ref_array[3];

  aom_highbd_sad128x64x4d_avx2(src, src_stride, ref, ref_stride, first_half);
  src += src_stride << shift_for_rows;
  ref[0] += ref_stride << shift_for_rows;
  ref[1] += ref_stride << shift_for_rows;
  ref[2] += ref_stride << shift_for_rows;
  ref[3] += ref_stride << shift_for_rows;
  aom_highbd_sad128x64x4d_avx2(src, src_stride, ref, ref_stride, second_half);
  sad_array[0] = first_half[0] + second_half[0];
  sad_array[1] = first_half[1] + second_half[1];
  sad_array[2] = first_half[2] + second_half[2];
  sad_array[3] = first_half[3] + second_half[3];
}
#endif  // CONFIG_EXT_PARTITION
