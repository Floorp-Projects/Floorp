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

#ifndef _V256_INTRINSICS_V128_H
#define _V256_INTRINSICS_V128_H

#if HAVE_NEON
#include "./v128_intrinsics_arm.h"
#elif HAVE_SSE2
#include "./v128_intrinsics_x86.h"
#else
#include "./v128_intrinsics.h"
#endif

typedef struct { v128 lo, hi; } v256;

SIMD_INLINE uint32_t v256_low_u32(v256 a) { return v128_low_u32(a.lo); }

SIMD_INLINE v64 v256_low_v64(v256 a) { return v128_low_v64(a.lo); }

SIMD_INLINE v128 v256_low_v128(v256 a) { return a.lo; }

SIMD_INLINE v128 v256_high_v128(v256 a) { return a.hi; }

SIMD_INLINE v256 v256_from_v128(v128 hi, v128 lo) {
  v256 t;
  t.hi = hi;
  t.lo = lo;
  return t;
}

SIMD_INLINE v256 v256_from_64(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
  return v256_from_v128(v128_from_64(a, b), v128_from_64(c, d));
}

SIMD_INLINE v256 v256_from_v64(v64 a, v64 b, v64 c, v64 d) {
  return v256_from_v128(v128_from_v64(a, b), v128_from_v64(c, d));
}

SIMD_INLINE v256 v256_load_unaligned(const void *p) {
  return v256_from_v128(v128_load_unaligned((uint8_t *)p + 16),
                        v128_load_unaligned(p));
}

SIMD_INLINE v256 v256_load_aligned(const void *p) {
  return v256_from_v128(v128_load_aligned((uint8_t *)p + 16),
                        v128_load_aligned(p));
}

SIMD_INLINE void v256_store_unaligned(void *p, v256 a) {
  v128_store_unaligned(p, a.lo);
  v128_store_unaligned((uint8_t *)p + 16, a.hi);
}

SIMD_INLINE void v256_store_aligned(void *p, v256 a) {
  v128_store_aligned(p, a.lo);
  v128_store_aligned((uint8_t *)p + 16, a.hi);
}

SIMD_INLINE v256 v256_zero() {
  return v256_from_v128(v128_zero(), v128_zero());
}

SIMD_INLINE v256 v256_dup_8(uint8_t x) {
  v128 t = v128_dup_8(x);
  return v256_from_v128(t, t);
}

SIMD_INLINE v256 v256_dup_16(uint16_t x) {
  v128 t = v128_dup_16(x);
  return v256_from_v128(t, t);
}

SIMD_INLINE v256 v256_dup_32(uint32_t x) {
  v128 t = v128_dup_32(x);
  return v256_from_v128(t, t);
}

SIMD_INLINE int64_t v256_dotp_s16(v256 a, v256 b) {
  return v128_dotp_s16(a.hi, b.hi) + v128_dotp_s16(a.lo, b.lo);
}

SIMD_INLINE uint64_t v256_hadd_u8(v256 a) {
  return v128_hadd_u8(a.hi) + v128_hadd_u8(a.lo);
}

typedef struct {
  sad128_internal hi;
  sad128_internal lo;
} sad256_internal;

SIMD_INLINE sad256_internal v256_sad_u8_init() {
  sad256_internal t;
  t.hi = v128_sad_u8_init();
  t.lo = v128_sad_u8_init();
  return t;
}

/* Implementation dependent return value.  Result must be finalised with
   v256_sad_u8_sum().
   The result for more than 16 v256_sad_u8() calls is undefined. */
SIMD_INLINE sad256_internal v256_sad_u8(sad256_internal s, v256 a, v256 b) {
  sad256_internal t;
  t.hi = v128_sad_u8(s.hi, a.hi, b.hi);
  t.lo = v128_sad_u8(s.lo, a.lo, b.lo);
  return t;
}

SIMD_INLINE uint32_t v256_sad_u8_sum(sad256_internal s) {
  return v128_sad_u8_sum(s.hi) + v128_sad_u8_sum(s.lo);
}

typedef struct {
  ssd128_internal hi;
  ssd128_internal lo;
} ssd256_internal;

SIMD_INLINE ssd256_internal v256_ssd_u8_init() {
  ssd256_internal t;
  t.hi = v128_ssd_u8_init();
  t.lo = v128_ssd_u8_init();
  return t;
}

/* Implementation dependent return value.  Result must be finalised with
 * v256_ssd_u8_sum(). */
SIMD_INLINE ssd256_internal v256_ssd_u8(ssd256_internal s, v256 a, v256 b) {
  ssd256_internal t;
  t.hi = v128_ssd_u8(s.hi, a.hi, b.hi);
  t.lo = v128_ssd_u8(s.lo, a.lo, b.lo);
  return t;
}

SIMD_INLINE uint32_t v256_ssd_u8_sum(ssd256_internal s) {
  return v128_ssd_u8_sum(s.hi) + v128_ssd_u8_sum(s.lo);
}

SIMD_INLINE v256 v256_or(v256 a, v256 b) {
  return v256_from_v128(v128_or(a.hi, b.hi), v128_or(a.lo, b.lo));
}

SIMD_INLINE v256 v256_xor(v256 a, v256 b) {
  return v256_from_v128(v128_xor(a.hi, b.hi), v128_xor(a.lo, b.lo));
}

SIMD_INLINE v256 v256_and(v256 a, v256 b) {
  return v256_from_v128(v128_and(a.hi, b.hi), v128_and(a.lo, b.lo));
}

SIMD_INLINE v256 v256_andn(v256 a, v256 b) {
  return v256_from_v128(v128_andn(a.hi, b.hi), v128_andn(a.lo, b.lo));
}

SIMD_INLINE v256 v256_add_8(v256 a, v256 b) {
  return v256_from_v128(v128_add_8(a.hi, b.hi), v128_add_8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_add_16(v256 a, v256 b) {
  return v256_from_v128(v128_add_16(a.hi, b.hi), v128_add_16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_sadd_s16(v256 a, v256 b) {
  return v256_from_v128(v128_sadd_s16(a.hi, b.hi), v128_sadd_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_add_32(v256 a, v256 b) {
  return v256_from_v128(v128_add_32(a.hi, b.hi), v128_add_32(a.lo, b.lo));
}

SIMD_INLINE v256 v256_padd_s16(v256 a) {
  return v256_from_v128(v128_padd_s16(a.hi), v128_padd_s16(a.lo));
}

SIMD_INLINE v256 v256_sub_8(v256 a, v256 b) {
  return v256_from_v128(v128_sub_8(a.hi, b.hi), v128_sub_8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ssub_u8(v256 a, v256 b) {
  return v256_from_v128(v128_ssub_u8(a.hi, b.hi), v128_ssub_u8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ssub_s8(v256 a, v256 b) {
  return v256_from_v128(v128_ssub_s8(a.hi, b.hi), v128_ssub_s8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_sub_16(v256 a, v256 b) {
  return v256_from_v128(v128_sub_16(a.hi, b.hi), v128_sub_16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ssub_s16(v256 a, v256 b) {
  return v256_from_v128(v128_ssub_s16(a.hi, b.hi), v128_ssub_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ssub_u16(v256 a, v256 b) {
  return v256_from_v128(v128_ssub_u16(a.hi, b.hi), v128_ssub_u16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_sub_32(v256 a, v256 b) {
  return v256_from_v128(v128_sub_32(a.hi, b.hi), v128_sub_32(a.lo, b.lo));
}

SIMD_INLINE v256 v256_abs_s16(v256 a) {
  return v256_from_v128(v128_abs_s16(a.hi), v128_abs_s16(a.lo));
}

SIMD_INLINE v256 v256_abs_s8(v256 a) {
  return v256_from_v128(v128_abs_s8(a.hi), v128_abs_s8(a.lo));
}

SIMD_INLINE v256 v256_mul_s16(v128 a, v128 b) {
  v128 lo_bits = v128_mullo_s16(a, b);
  v128 hi_bits = v128_mulhi_s16(a, b);
  return v256_from_v128(v128_ziphi_16(hi_bits, lo_bits),
                        v128_ziplo_16(hi_bits, lo_bits));
}

SIMD_INLINE v256 v256_mullo_s16(v256 a, v256 b) {
  return v256_from_v128(v128_mullo_s16(a.hi, b.hi), v128_mullo_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_mulhi_s16(v256 a, v256 b) {
  return v256_from_v128(v128_mulhi_s16(a.hi, b.hi), v128_mulhi_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_mullo_s32(v256 a, v256 b) {
  return v256_from_v128(v128_mullo_s32(a.hi, b.hi), v128_mullo_s32(a.lo, b.lo));
}

SIMD_INLINE v256 v256_madd_s16(v256 a, v256 b) {
  return v256_from_v128(v128_madd_s16(a.hi, b.hi), v128_madd_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_madd_us8(v256 a, v256 b) {
  return v256_from_v128(v128_madd_us8(a.hi, b.hi), v128_madd_us8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_avg_u8(v256 a, v256 b) {
  return v256_from_v128(v128_avg_u8(a.hi, b.hi), v128_avg_u8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_rdavg_u8(v256 a, v256 b) {
  return v256_from_v128(v128_rdavg_u8(a.hi, b.hi), v128_rdavg_u8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_avg_u16(v256 a, v256 b) {
  return v256_from_v128(v128_avg_u16(a.hi, b.hi), v128_avg_u16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_min_u8(v256 a, v256 b) {
  return v256_from_v128(v128_min_u8(a.hi, b.hi), v128_min_u8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_max_u8(v256 a, v256 b) {
  return v256_from_v128(v128_max_u8(a.hi, b.hi), v128_max_u8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_min_s8(v256 a, v256 b) {
  return v256_from_v128(v128_min_s8(a.hi, b.hi), v128_min_s8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_max_s8(v256 a, v256 b) {
  return v256_from_v128(v128_max_s8(a.hi, b.hi), v128_max_s8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_min_s16(v256 a, v256 b) {
  return v256_from_v128(v128_min_s16(a.hi, b.hi), v128_min_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_max_s16(v256 a, v256 b) {
  return v256_from_v128(v128_max_s16(a.hi, b.hi), v128_max_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ziplo_8(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_8(a.lo, b.lo), v128_ziplo_8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ziphi_8(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_8(a.hi, b.hi), v128_ziplo_8(a.hi, b.hi));
}

SIMD_INLINE v256 v256_ziplo_16(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_16(a.lo, b.lo), v128_ziplo_16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ziphi_16(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_16(a.hi, b.hi), v128_ziplo_16(a.hi, b.hi));
}

SIMD_INLINE v256 v256_ziplo_32(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_32(a.lo, b.lo), v128_ziplo_32(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ziphi_32(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_32(a.hi, b.hi), v128_ziplo_32(a.hi, b.hi));
}

SIMD_INLINE v256 v256_ziplo_64(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_64(a.lo, b.lo), v128_ziplo_64(a.lo, b.lo));
}

SIMD_INLINE v256 v256_ziphi_64(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_64(a.hi, b.hi), v128_ziplo_64(a.hi, b.hi));
}

SIMD_INLINE v256 v256_ziplo_128(v256 a, v256 b) {
  return v256_from_v128(a.lo, b.lo);
}

SIMD_INLINE v256 v256_ziphi_128(v256 a, v256 b) {
  return v256_from_v128(a.hi, b.hi);
}

SIMD_INLINE v256 v256_zip_8(v128 a, v128 b) {
  return v256_from_v128(v128_ziphi_8(a, b), v128_ziplo_8(a, b));
}

SIMD_INLINE v256 v256_zip_16(v128 a, v128 b) {
  return v256_from_v128(v128_ziphi_16(a, b), v128_ziplo_16(a, b));
}

SIMD_INLINE v256 v256_zip_32(v128 a, v128 b) {
  return v256_from_v128(v128_ziphi_32(a, b), v128_ziplo_32(a, b));
}

SIMD_INLINE v256 v256_unziplo_8(v256 a, v256 b) {
  return v256_from_v128(v128_unziplo_8(a.hi, a.lo), v128_unziplo_8(b.hi, b.lo));
}

SIMD_INLINE v256 v256_unziphi_8(v256 a, v256 b) {
  return v256_from_v128(v128_unziphi_8(a.hi, a.lo), v128_unziphi_8(b.hi, b.lo));
}

SIMD_INLINE v256 v256_unziplo_16(v256 a, v256 b) {
  return v256_from_v128(v128_unziplo_16(a.hi, a.lo),
                        v128_unziplo_16(b.hi, b.lo));
}

SIMD_INLINE v256 v256_unziphi_16(v256 a, v256 b) {
  return v256_from_v128(v128_unziphi_16(a.hi, a.lo),
                        v128_unziphi_16(b.hi, b.lo));
}

SIMD_INLINE v256 v256_unziplo_32(v256 a, v256 b) {
  return v256_from_v128(v128_unziplo_32(a.hi, a.lo),
                        v128_unziplo_32(b.hi, b.lo));
}

SIMD_INLINE v256 v256_unziphi_32(v256 a, v256 b) {
  return v256_from_v128(v128_unziphi_32(a.hi, a.lo),
                        v128_unziphi_32(b.hi, b.lo));
}

SIMD_INLINE v256 v256_unpack_u8_s16(v128 a) {
  return v256_from_v128(v128_unpackhi_u8_s16(a), v128_unpacklo_u8_s16(a));
}

SIMD_INLINE v256 v256_unpacklo_u8_s16(v256 a) {
  return v256_from_v128(v128_unpackhi_u8_s16(a.lo), v128_unpacklo_u8_s16(a.lo));
}

SIMD_INLINE v256 v256_unpackhi_u8_s16(v256 a) {
  return v256_from_v128(v128_unpackhi_u8_s16(a.hi), v128_unpacklo_u8_s16(a.hi));
}

SIMD_INLINE v256 v256_unpack_s8_s16(v128 a) {
  return v256_from_v128(v128_unpackhi_s8_s16(a), v128_unpacklo_s8_s16(a));
}

SIMD_INLINE v256 v256_unpacklo_s8_s16(v256 a) {
  return v256_from_v128(v128_unpackhi_s8_s16(a.lo), v128_unpacklo_s8_s16(a.lo));
}

SIMD_INLINE v256 v256_unpackhi_s8_s16(v256 a) {
  return v256_from_v128(v128_unpackhi_s8_s16(a.hi), v128_unpacklo_s8_s16(a.hi));
}

SIMD_INLINE v256 v256_pack_s32_s16(v256 a, v256 b) {
  return v256_from_v128(v128_pack_s32_s16(a.hi, a.lo),
                        v128_pack_s32_s16(b.hi, b.lo));
}

SIMD_INLINE v256 v256_pack_s16_u8(v256 a, v256 b) {
  return v256_from_v128(v128_pack_s16_u8(a.hi, a.lo),
                        v128_pack_s16_u8(b.hi, b.lo));
}

SIMD_INLINE v256 v256_pack_s16_s8(v256 a, v256 b) {
  return v256_from_v128(v128_pack_s16_s8(a.hi, a.lo),
                        v128_pack_s16_s8(b.hi, b.lo));
}

SIMD_INLINE v256 v256_unpack_u16_s32(v128 a) {
  return v256_from_v128(v128_unpackhi_u16_s32(a), v128_unpacklo_u16_s32(a));
}

SIMD_INLINE v256 v256_unpack_s16_s32(v128 a) {
  return v256_from_v128(v128_unpackhi_s16_s32(a), v128_unpacklo_s16_s32(a));
}

SIMD_INLINE v256 v256_unpacklo_u16_s32(v256 a) {
  return v256_from_v128(v128_unpackhi_u16_s32(a.lo),
                        v128_unpacklo_u16_s32(a.lo));
}

SIMD_INLINE v256 v256_unpacklo_s16_s32(v256 a) {
  return v256_from_v128(v128_unpackhi_s16_s32(a.lo),
                        v128_unpacklo_s16_s32(a.lo));
}

SIMD_INLINE v256 v256_unpackhi_u16_s32(v256 a) {
  return v256_from_v128(v128_unpackhi_u16_s32(a.hi),
                        v128_unpacklo_u16_s32(a.hi));
}

SIMD_INLINE v256 v256_unpackhi_s16_s32(v256 a) {
  return v256_from_v128(v128_unpackhi_s16_s32(a.hi),
                        v128_unpacklo_s16_s32(a.hi));
}

SIMD_INLINE v256 v256_shuffle_8(v256 a, v256 pattern) {
  v128 c16 = v128_dup_8(16);
  v128 maskhi = v128_cmplt_s8(pattern.hi, c16);
  v128 masklo = v128_cmplt_s8(pattern.lo, c16);
  return v256_from_v128(
      v128_or(
          v128_and(v128_shuffle_8(a.lo, pattern.hi), maskhi),
          v128_andn(v128_shuffle_8(a.hi, v128_sub_8(pattern.hi, c16)), maskhi)),
      v128_or(v128_and(v128_shuffle_8(a.lo, pattern.lo), masklo),
              v128_andn(v128_shuffle_8(a.hi, v128_sub_8(pattern.lo, c16)),
                        masklo)));
}

SIMD_INLINE v256 v256_pshuffle_8(v256 a, v256 pattern) {
  return v256_from_v128(
      v128_shuffle_8(v256_high_v128(a), v256_high_v128(pattern)),
      v128_shuffle_8(v256_low_v128(a), v256_low_v128(pattern)));
}

SIMD_INLINE v256 v256_cmpgt_s8(v256 a, v256 b) {
  return v256_from_v128(v128_cmpgt_s8(a.hi, b.hi), v128_cmpgt_s8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_cmplt_s8(v256 a, v256 b) {
  return v256_from_v128(v128_cmplt_s8(a.hi, b.hi), v128_cmplt_s8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_cmpeq_8(v256 a, v256 b) {
  return v256_from_v128(v128_cmpeq_8(a.hi, b.hi), v128_cmpeq_8(a.lo, b.lo));
}

SIMD_INLINE v256 v256_cmpgt_s16(v256 a, v256 b) {
  return v256_from_v128(v128_cmpgt_s16(a.hi, b.hi), v128_cmpgt_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_cmplt_s16(v256 a, v256 b) {
  return v256_from_v128(v128_cmplt_s16(a.hi, b.hi), v128_cmplt_s16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_cmpeq_16(v256 a, v256 b) {
  return v256_from_v128(v128_cmpeq_16(a.hi, b.hi), v128_cmpeq_16(a.lo, b.lo));
}

SIMD_INLINE v256 v256_shl_8(v256 a, unsigned int c) {
  return v256_from_v128(v128_shl_8(a.hi, c), v128_shl_8(a.lo, c));
}

SIMD_INLINE v256 v256_shr_u8(v256 a, unsigned int c) {
  return v256_from_v128(v128_shr_u8(a.hi, c), v128_shr_u8(a.lo, c));
}

SIMD_INLINE v256 v256_shr_s8(v256 a, unsigned int c) {
  return v256_from_v128(v128_shr_s8(a.hi, c), v128_shr_s8(a.lo, c));
}

SIMD_INLINE v256 v256_shl_16(v256 a, unsigned int c) {
  return v256_from_v128(v128_shl_16(a.hi, c), v128_shl_16(a.lo, c));
}

SIMD_INLINE v256 v256_shr_u16(v256 a, unsigned int c) {
  return v256_from_v128(v128_shr_u16(a.hi, c), v128_shr_u16(a.lo, c));
}

SIMD_INLINE v256 v256_shr_s16(v256 a, unsigned int c) {
  return v256_from_v128(v128_shr_s16(a.hi, c), v128_shr_s16(a.lo, c));
}

SIMD_INLINE v256 v256_shl_32(v256 a, unsigned int c) {
  return v256_from_v128(v128_shl_32(a.hi, c), v128_shl_32(a.lo, c));
}

SIMD_INLINE v256 v256_shr_u32(v256 a, unsigned int c) {
  return v256_from_v128(v128_shr_u32(a.hi, c), v128_shr_u32(a.lo, c));
}

SIMD_INLINE v256 v256_shr_s32(v256 a, unsigned int c) {
  return v256_from_v128(v128_shr_s32(a.hi, c), v128_shr_s32(a.lo, c));
}

/* These intrinsics require immediate values, so we must use #defines
   to enforce that. */
#define v256_shl_n_byte(a, n)                                                 \
  ((n) < 16 ? v256_from_v128(v128_or(v128_shl_n_byte(a.hi, n),                \
                                     v128_shr_n_byte(a.lo, (16 - (n)) & 31)), \
                             v128_shl_n_byte(a.lo, (n)))                      \
            : v256_from_v128(                                                 \
                  (n) > 16 ? v128_shl_n_byte(a.lo, ((n)-16) & 31) : a.lo,     \
                  v128_zero()))

#define v256_shr_n_byte(a, n)                                                 \
  ((n) < 16 ? v256_from_v128(v128_shr_n_byte(a.hi, n),                        \
                             v128_or(v128_shr_n_byte(a.lo, n),                \
                                     v128_shl_n_byte(a.hi, (16 - (n)) & 31))) \
            : v256_from_v128(                                                 \
                  v128_zero(),                                                \
                  (n) > 16 ? v128_shr_n_byte(a.hi, ((n)-16) & 31) : a.hi))

#define v256_align(a, b, c) \
  ((c) ? v256_or(v256_shr_n_byte(b, c), v256_shl_n_byte(a, 32 - (c))) : b)

#define v256_shl_n_8(a, n) \
  v256_from_v128(v128_shl_n_8(a.hi, n), v128_shl_n_8(a.lo, n))
#define v256_shl_n_16(a, n) \
  v256_from_v128(v128_shl_n_16(a.hi, n), v128_shl_n_16(a.lo, n))
#define v256_shl_n_32(a, n) \
  v256_from_v128(v128_shl_n_32(a.hi, n), v128_shl_n_32(a.lo, n))
#define v256_shr_n_u8(a, n) \
  v256_from_v128(v128_shr_n_u8(a.hi, n), v128_shr_n_u8(a.lo, n))
#define v256_shr_n_u16(a, n) \
  v256_from_v128(v128_shr_n_u16(a.hi, n), v128_shr_n_u16(a.lo, n))
#define v256_shr_n_u32(a, n) \
  v256_from_v128(v128_shr_n_u32(a.hi, n), v128_shr_n_u32(a.lo, n))
#define v256_shr_n_s8(a, n) \
  v256_from_v128(v128_shr_n_s8(a.hi, n), v128_shr_n_s8(a.lo, n))
#define v256_shr_n_s16(a, n) \
  v256_from_v128(v128_shr_n_s16(a.hi, n), v128_shr_n_s16(a.lo, n))
#define v256_shr_n_s32(a, n) \
  v256_from_v128(v128_shr_n_s32(a.hi, n), v128_shr_n_s32(a.lo, n))

#endif /* _V256_INTRINSICS_V128_H */
