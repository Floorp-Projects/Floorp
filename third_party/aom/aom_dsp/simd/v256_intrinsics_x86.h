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

#ifndef _V256_INTRINSICS_H
#define _V256_INTRINSICS_H

#if !defined(__AVX2__)

#include "./v256_intrinsics_v128.h"

#else

// The _m256i type seems to cause problems for g++'s mangling prior to
// version 5, but adding -fabi-version=0 fixes this.
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5 && \
    defined(__AVX2__) && defined(__cplusplus)
#pragma GCC optimize "-fabi-version=0"
#endif

#include <immintrin.h>
#include "./v128_intrinsics_x86.h"

typedef __m256i v256;

SIMD_INLINE uint32_t v256_low_u32(v256 a) {
  return (uint32_t)_mm_cvtsi128_si32(_mm256_extracti128_si256(a, 0));
}

SIMD_INLINE v64 v256_low_v64(v256 a) {
  return _mm_unpacklo_epi64(_mm256_extracti128_si256(a, 0), v64_zero());
}

SIMD_INLINE v128 v256_low_v128(v256 a) {
  return _mm256_extracti128_si256(a, 0);
}

SIMD_INLINE v128 v256_high_v128(v256 a) {
  return _mm256_extracti128_si256(a, 1);
}

SIMD_INLINE v256 v256_from_v128(v128 a, v128 b) {
  // gcc seems to be missing _mm256_set_m128i()
  return _mm256_insertf128_si256(
      _mm256_insertf128_si256(_mm256_setzero_si256(), b, 0), a, 1);
}

SIMD_INLINE v256 v256_from_v64(v64 a, v64 b, v64 c, v64 d) {
  return v256_from_v128(v128_from_v64(a, b), v128_from_v64(c, d));
}

SIMD_INLINE v256 v256_from_64(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
  return v256_from_v128(v128_from_64(a, b), v128_from_64(c, d));
}

SIMD_INLINE v256 v256_load_aligned(const void *p) {
  return _mm256_load_si256((const __m256i *)p);
}

SIMD_INLINE v256 v256_load_unaligned(const void *p) {
  return _mm256_loadu_si256((const __m256i *)p);
}

SIMD_INLINE void v256_store_aligned(void *p, v256 a) {
  _mm256_store_si256((__m256i *)p, a);
}

SIMD_INLINE void v256_store_unaligned(void *p, v256 a) {
  _mm256_storeu_si256((__m256i *)p, a);
}

SIMD_INLINE v256 v256_zero() { return _mm256_setzero_si256(); }

SIMD_INLINE v256 v256_dup_8(uint8_t x) { return _mm256_set1_epi8(x); }

SIMD_INLINE v256 v256_dup_16(uint16_t x) { return _mm256_set1_epi16(x); }

SIMD_INLINE v256 v256_dup_32(uint32_t x) { return _mm256_set1_epi32(x); }

SIMD_INLINE v256 v256_add_8(v256 a, v256 b) { return _mm256_add_epi8(a, b); }

SIMD_INLINE v256 v256_add_16(v256 a, v256 b) { return _mm256_add_epi16(a, b); }

SIMD_INLINE v256 v256_sadd_s16(v256 a, v256 b) {
  return _mm256_adds_epi16(a, b);
}

SIMD_INLINE v256 v256_add_32(v256 a, v256 b) { return _mm256_add_epi32(a, b); }

SIMD_INLINE v256 v256_padd_s16(v256 a) {
  return _mm256_madd_epi16(a, _mm256_set1_epi16(1));
}

SIMD_INLINE v256 v256_sub_8(v256 a, v256 b) { return _mm256_sub_epi8(a, b); }

SIMD_INLINE v256 v256_ssub_u8(v256 a, v256 b) { return _mm256_subs_epu8(a, b); }

SIMD_INLINE v256 v256_ssub_s8(v256 a, v256 b) { return _mm256_subs_epi8(a, b); }

SIMD_INLINE v256 v256_sub_16(v256 a, v256 b) { return _mm256_sub_epi16(a, b); }

SIMD_INLINE v256 v256_ssub_s16(v256 a, v256 b) {
  return _mm256_subs_epi16(a, b);
}

SIMD_INLINE v256 v256_ssub_u16(v256 a, v256 b) {
  return _mm256_subs_epu16(a, b);
}

SIMD_INLINE v256 v256_sub_32(v256 a, v256 b) { return _mm256_sub_epi32(a, b); }

SIMD_INLINE v256 v256_abs_s16(v256 a) { return _mm256_abs_epi16(a); }

SIMD_INLINE v256 v256_abs_s8(v256 a) { return _mm256_abs_epi8(a); }

// AVX doesn't have the direct intrinsics to zip/unzip 8, 16, 32 bit
// lanes of lower or upper halves of a 256bit vector because the
// unpack/pack intrinsics operate on the 256 bit input vector as 2
// independent 128 bit vectors.
SIMD_INLINE v256 v256_ziplo_8(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_8(v256_low_v128(a), v256_low_v128(b)),
                        v128_ziplo_8(v256_low_v128(a), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_ziphi_8(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_8(v256_high_v128(a), v256_high_v128(b)),
                        v128_ziplo_8(v256_high_v128(a), v256_high_v128(b)));
}

SIMD_INLINE v256 v256_ziplo_16(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_16(v256_low_v128(a), v256_low_v128(b)),
                        v128_ziplo_16(v256_low_v128(a), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_ziphi_16(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_16(v256_high_v128(a), v256_high_v128(b)),
                        v128_ziplo_16(v256_high_v128(a), v256_high_v128(b)));
}

SIMD_INLINE v256 v256_ziplo_32(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_32(v256_low_v128(a), v256_low_v128(b)),
                        v128_ziplo_32(v256_low_v128(a), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_ziphi_32(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_32(v256_high_v128(a), v256_high_v128(b)),
                        v128_ziplo_32(v256_high_v128(a), v256_high_v128(b)));
}

SIMD_INLINE v256 v256_ziplo_64(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_64(v256_low_v128(a), v256_low_v128(b)),
                        v128_ziplo_64(v256_low_v128(a), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_ziphi_64(v256 a, v256 b) {
  return v256_from_v128(v128_ziphi_64(v256_high_v128(a), v256_high_v128(b)),
                        v128_ziplo_64(v256_high_v128(a), v256_high_v128(b)));
}

SIMD_INLINE v256 v256_ziplo_128(v256 a, v256 b) {
  return v256_from_v128(v256_low_v128(a), v256_low_v128(b));
}

SIMD_INLINE v256 v256_ziphi_128(v256 a, v256 b) {
  return v256_from_v128(v256_high_v128(a), v256_high_v128(b));
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
  return v256_from_v128(v128_unziplo_8(v256_high_v128(a), v256_low_v128(a)),
                        v128_unziplo_8(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_unziphi_8(v256 a, v256 b) {
  return v256_from_v128(v128_unziphi_8(v256_high_v128(a), v256_low_v128(a)),
                        v128_unziphi_8(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_unziplo_16(v256 a, v256 b) {
  return v256_from_v128(v128_unziplo_16(v256_high_v128(a), v256_low_v128(a)),
                        v128_unziplo_16(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_unziphi_16(v256 a, v256 b) {
  return v256_from_v128(v128_unziphi_16(v256_high_v128(a), v256_low_v128(a)),
                        v128_unziphi_16(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_unziplo_32(v256 a, v256 b) {
  return v256_from_v128(v128_unziplo_32(v256_high_v128(a), v256_low_v128(a)),
                        v128_unziplo_32(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_unziphi_32(v256 a, v256 b) {
  return v256_from_v128(v128_unziphi_32(v256_high_v128(a), v256_low_v128(a)),
                        v128_unziphi_32(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_unpack_u8_s16(v128 a) {
  return v256_from_v128(v128_unpackhi_u8_s16(a), v128_unpacklo_u8_s16(a));
}

SIMD_INLINE v256 v256_unpacklo_u8_s16(v256 a) {
  return v256_from_v128(v128_unpackhi_u8_s16(v256_low_v128(a)),
                        v128_unpacklo_u8_s16(v256_low_v128(a)));
}

SIMD_INLINE v256 v256_unpackhi_u8_s16(v256 a) {
  return v256_from_v128(v128_unpackhi_u8_s16(v256_high_v128(a)),
                        v128_unpacklo_u8_s16(v256_high_v128(a)));
}

SIMD_INLINE v256 v256_unpack_s8_s16(v128 a) {
  return v256_from_v128(v128_unpackhi_s8_s16(a), v128_unpacklo_s8_s16(a));
}

SIMD_INLINE v256 v256_unpacklo_s8_s16(v256 a) {
  return v256_from_v128(v128_unpackhi_s8_s16(v256_low_v128(a)),
                        v128_unpacklo_s8_s16(v256_low_v128(a)));
}

SIMD_INLINE v256 v256_unpackhi_s8_s16(v256 a) {
  return v256_from_v128(v128_unpackhi_s8_s16(v256_high_v128(a)),
                        v128_unpacklo_s8_s16(v256_high_v128(a)));
}

SIMD_INLINE v256 v256_pack_s32_s16(v256 a, v256 b) {
  return v256_from_v128(v128_pack_s32_s16(v256_high_v128(a), v256_low_v128(a)),
                        v128_pack_s32_s16(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_pack_s16_u8(v256 a, v256 b) {
  return v256_from_v128(v128_pack_s16_u8(v256_high_v128(a), v256_low_v128(a)),
                        v128_pack_s16_u8(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_pack_s16_s8(v256 a, v256 b) {
  return v256_from_v128(v128_pack_s16_s8(v256_high_v128(a), v256_low_v128(a)),
                        v128_pack_s16_s8(v256_high_v128(b), v256_low_v128(b)));
}

SIMD_INLINE v256 v256_unpack_u16_s32(v128 a) {
  return v256_from_v128(v128_unpackhi_u16_s32(a), v128_unpacklo_u16_s32(a));
}

SIMD_INLINE v256 v256_unpack_s16_s32(v128 a) {
  return v256_from_v128(v128_unpackhi_s16_s32(a), v128_unpacklo_s16_s32(a));
}

SIMD_INLINE v256 v256_unpacklo_u16_s32(v256 a) {
  return v256_from_v128(v128_unpackhi_u16_s32(v256_low_v128(a)),
                        v128_unpacklo_u16_s32(v256_low_v128(a)));
}

SIMD_INLINE v256 v256_unpacklo_s16_s32(v256 a) {
  return v256_from_v128(v128_unpackhi_s16_s32(v256_low_v128(a)),
                        v128_unpacklo_s16_s32(v256_low_v128(a)));
}

SIMD_INLINE v256 v256_unpackhi_u16_s32(v256 a) {
  return v256_from_v128(v128_unpackhi_u16_s32(v256_high_v128(a)),
                        v128_unpacklo_u16_s32(v256_high_v128(a)));
}

SIMD_INLINE v256 v256_unpackhi_s16_s32(v256 a) {
  return v256_from_v128(v128_unpackhi_s16_s32(v256_high_v128(a)),
                        v128_unpacklo_s16_s32(v256_high_v128(a)));
}
SIMD_INLINE v256 v256_shuffle_8(v256 a, v256 pattern) {
  v128 c16 = v128_dup_8(16);
  v128 hi = v256_high_v128(pattern);
  v128 lo = v256_low_v128(pattern);
  v128 maskhi = v128_cmplt_s8(hi, c16);
  v128 masklo = v128_cmplt_s8(lo, c16);
  return v256_from_v128(
      v128_or(v128_and(v128_shuffle_8(v256_low_v128(a), hi), maskhi),
              v128_andn(v128_shuffle_8(v256_high_v128(a), v128_sub_8(hi, c16)),
                        maskhi)),
      v128_or(v128_and(v128_shuffle_8(v256_low_v128(a), lo), masklo),
              v128_andn(v128_shuffle_8(v256_high_v128(a), v128_sub_8(lo, c16)),
                        masklo)));
}

SIMD_INLINE v256 v256_pshuffle_8(v256 a, v256 pattern) {
  return _mm256_shuffle_epi8(a, pattern);
}

SIMD_INLINE int64_t v256_dotp_s16(v256 a, v256 b) {
  v256 r = _mm256_madd_epi16(a, b);
#if defined(__x86_64__)
  v128 t;
  r = _mm256_add_epi64(_mm256_cvtepi32_epi64(v256_high_v128(r)),
                       _mm256_cvtepi32_epi64(v256_low_v128(r)));
  t = v256_low_v128(_mm256_add_epi64(
      r, _mm256_permute2x128_si256(r, r, _MM_SHUFFLE(2, 0, 0, 1))));
  return _mm_cvtsi128_si64(_mm_add_epi64(t, _mm_srli_si128(t, 8)));
#else
  v128 l = v256_low_v128(r);
  v128 h = v256_high_v128(r);
  return (int64_t)_mm_cvtsi128_si32(l) +
         (int64_t)_mm_cvtsi128_si32(_mm_srli_si128(l, 4)) +
         (int64_t)_mm_cvtsi128_si32(_mm_srli_si128(l, 8)) +
         (int64_t)_mm_cvtsi128_si32(_mm_srli_si128(l, 12)) +
         (int64_t)_mm_cvtsi128_si32(h) +
         (int64_t)_mm_cvtsi128_si32(_mm_srli_si128(h, 4)) +
         (int64_t)_mm_cvtsi128_si32(_mm_srli_si128(h, 8)) +
         (int64_t)_mm_cvtsi128_si32(_mm_srli_si128(h, 12));
#endif
}

SIMD_INLINE uint64_t v256_hadd_u8(v256 a) {
  v256 t = _mm256_sad_epu8(a, _mm256_setzero_si256());
  v128 lo = v256_low_v128(t);
  v128 hi = v256_high_v128(t);
  lo = v128_add_32(lo, hi);
  return v64_low_u32(v128_low_v64(lo)) + v128_low_u32(v128_high_v64(lo));
}

typedef v256 sad256_internal;

SIMD_INLINE sad256_internal v256_sad_u8_init() {
  return _mm256_setzero_si256();
}

/* Implementation dependent return value.  Result must be finalised with
   v256_sad_sum().
   The result for more than 32 v256_sad_u8() calls is undefined. */
SIMD_INLINE sad256_internal v256_sad_u8(sad256_internal s, v256 a, v256 b) {
  return _mm256_add_epi64(s, _mm256_sad_epu8(a, b));
}

SIMD_INLINE uint32_t v256_sad_u8_sum(sad256_internal s) {
  v256 t = _mm256_add_epi32(s, _mm256_unpackhi_epi64(s, s));
  return v128_low_u32(_mm_add_epi32(v256_high_v128(t), v256_low_v128(t)));
}

typedef v256 ssd256_internal;

SIMD_INLINE ssd256_internal v256_ssd_u8_init() {
  return _mm256_setzero_si256();
}

/* Implementation dependent return value.  Result must be finalised with
 * v256_ssd_sum(). */
SIMD_INLINE ssd256_internal v256_ssd_u8(ssd256_internal s, v256 a, v256 b) {
  v256 l = _mm256_sub_epi16(_mm256_unpacklo_epi8(a, _mm256_setzero_si256()),
                            _mm256_unpacklo_epi8(b, _mm256_setzero_si256()));
  v256 h = _mm256_sub_epi16(_mm256_unpackhi_epi8(a, _mm256_setzero_si256()),
                            _mm256_unpackhi_epi8(b, _mm256_setzero_si256()));
  v256 rl = _mm256_madd_epi16(l, l);
  v256 rh = _mm256_madd_epi16(h, h);
  v128 c = _mm_cvtsi32_si128(32);
  rl = _mm256_add_epi32(rl, _mm256_srli_si256(rl, 8));
  rl = _mm256_add_epi32(rl, _mm256_srli_si256(rl, 4));
  rh = _mm256_add_epi32(rh, _mm256_srli_si256(rh, 8));
  rh = _mm256_add_epi32(rh, _mm256_srli_si256(rh, 4));
  return _mm256_add_epi64(
      s,
      _mm256_srl_epi64(_mm256_sll_epi64(_mm256_unpacklo_epi64(rl, rh), c), c));
}

SIMD_INLINE uint32_t v256_ssd_u8_sum(ssd256_internal s) {
  v256 t = _mm256_add_epi32(s, _mm256_unpackhi_epi64(s, s));
  return v128_low_u32(_mm_add_epi32(v256_high_v128(t), v256_low_v128(t)));
}

SIMD_INLINE v256 v256_or(v256 a, v256 b) { return _mm256_or_si256(a, b); }

SIMD_INLINE v256 v256_xor(v256 a, v256 b) { return _mm256_xor_si256(a, b); }

SIMD_INLINE v256 v256_and(v256 a, v256 b) { return _mm256_and_si256(a, b); }

SIMD_INLINE v256 v256_andn(v256 a, v256 b) { return _mm256_andnot_si256(b, a); }

SIMD_INLINE v256 v256_mul_s16(v64 a, v64 b) {
  v128 lo_bits = v128_mullo_s16(a, b);
  v128 hi_bits = v128_mulhi_s16(a, b);
  return v256_from_v128(v128_ziphi_16(hi_bits, lo_bits),
                        v128_ziplo_16(hi_bits, lo_bits));
}

SIMD_INLINE v256 v256_mullo_s16(v256 a, v256 b) {
  return _mm256_mullo_epi16(a, b);
}

SIMD_INLINE v256 v256_mulhi_s16(v256 a, v256 b) {
  return _mm256_mulhi_epi16(a, b);
}

SIMD_INLINE v256 v256_mullo_s32(v256 a, v256 b) {
  return _mm256_mullo_epi32(a, b);
}

SIMD_INLINE v256 v256_madd_s16(v256 a, v256 b) {
  return _mm256_madd_epi16(a, b);
}

SIMD_INLINE v256 v256_madd_us8(v256 a, v256 b) {
  return _mm256_maddubs_epi16(a, b);
}

SIMD_INLINE v256 v256_avg_u8(v256 a, v256 b) { return _mm256_avg_epu8(a, b); }

SIMD_INLINE v256 v256_rdavg_u8(v256 a, v256 b) {
  return _mm256_sub_epi8(
      _mm256_avg_epu8(a, b),
      _mm256_and_si256(_mm256_xor_si256(a, b), v256_dup_8(1)));
}

SIMD_INLINE v256 v256_avg_u16(v256 a, v256 b) { return _mm256_avg_epu16(a, b); }

SIMD_INLINE v256 v256_min_u8(v256 a, v256 b) { return _mm256_min_epu8(a, b); }

SIMD_INLINE v256 v256_max_u8(v256 a, v256 b) { return _mm256_max_epu8(a, b); }

SIMD_INLINE v256 v256_min_s8(v256 a, v256 b) { return _mm256_min_epi8(a, b); }

SIMD_INLINE v256 v256_max_s8(v256 a, v256 b) { return _mm256_max_epi8(a, b); }

SIMD_INLINE v256 v256_min_s16(v256 a, v256 b) { return _mm256_min_epi16(a, b); }

SIMD_INLINE v256 v256_max_s16(v256 a, v256 b) { return _mm256_max_epi16(a, b); }

SIMD_INLINE v256 v256_cmpgt_s8(v256 a, v256 b) {
  return _mm256_cmpgt_epi8(a, b);
}

SIMD_INLINE v256 v256_cmplt_s8(v256 a, v256 b) {
  return v256_andn(_mm256_cmpgt_epi8(b, a), _mm256_cmpeq_epi8(b, a));
}

SIMD_INLINE v256 v256_cmpeq_8(v256 a, v256 b) {
  return _mm256_cmpeq_epi8(a, b);
}

SIMD_INLINE v256 v256_cmpgt_s16(v256 a, v256 b) {
  return _mm256_cmpgt_epi16(a, b);
}

SIMD_INLINE v256 v256_cmplt_s16(v256 a, v256 b) {
  return v256_andn(_mm256_cmpgt_epi16(b, a), _mm256_cmpeq_epi16(b, a));
}

SIMD_INLINE v256 v256_cmpeq_16(v256 a, v256 b) {
  return _mm256_cmpeq_epi16(a, b);
}

SIMD_INLINE v256 v256_shl_8(v256 a, unsigned int c) {
  return _mm256_and_si256(_mm256_set1_epi8((uint8_t)(0xff << c)),
                          _mm256_sll_epi16(a, _mm_cvtsi32_si128(c)));
}

SIMD_INLINE v256 v256_shr_u8(v256 a, unsigned int c) {
  return _mm256_and_si256(_mm256_set1_epi8(0xff >> c),
                          _mm256_srl_epi16(a, _mm_cvtsi32_si128(c)));
}

SIMD_INLINE v256 v256_shr_s8(v256 a, unsigned int c) {
  __m128i x = _mm_cvtsi32_si128(c + 8);
  return _mm256_packs_epi16(_mm256_sra_epi16(_mm256_unpacklo_epi8(a, a), x),
                            _mm256_sra_epi16(_mm256_unpackhi_epi8(a, a), x));
}

SIMD_INLINE v256 v256_shl_16(v256 a, unsigned int c) {
  return _mm256_sll_epi16(a, _mm_cvtsi32_si128(c));
}

SIMD_INLINE v256 v256_shr_u16(v256 a, unsigned int c) {
  return _mm256_srl_epi16(a, _mm_cvtsi32_si128(c));
}

SIMD_INLINE v256 v256_shr_s16(v256 a, unsigned int c) {
  return _mm256_sra_epi16(a, _mm_cvtsi32_si128(c));
}

SIMD_INLINE v256 v256_shl_32(v256 a, unsigned int c) {
  return _mm256_sll_epi32(a, _mm_cvtsi32_si128(c));
}

SIMD_INLINE v256 v256_shr_u32(v256 a, unsigned int c) {
  return _mm256_srl_epi32(a, _mm_cvtsi32_si128(c));
}

SIMD_INLINE v256 v256_shr_s32(v256 a, unsigned int c) {
  return _mm256_sra_epi32(a, _mm_cvtsi32_si128(c));
}

/* These intrinsics require immediate values, so we must use #defines
   to enforce that. */
// _mm256_slli_si256 works on 128 bit lanes and can't be used
#define v256_shl_n_byte(a, n)                                                 \
  ((n) < 16                                                                   \
       ? v256_from_v128(v128_or(v128_shl_n_byte(v256_high_v128(a), n),        \
                                v128_shr_n_byte(v256_low_v128(a), 16 - (n))), \
                        v128_shl_n_byte(v256_low_v128(a), n))                 \
       : v256_from_v128(v128_shl_n_byte(v256_low_v128(a), (n)-16),            \
                        v128_zero()))

// _mm256_srli_si256 works on 128 bit lanes and can't be used
#define v256_shr_n_byte(a, n)                                                 \
  ((n) < 16                                                                   \
       ? _mm256_alignr_epi8(                                                  \
             _mm256_permute2x128_si256(a, a, _MM_SHUFFLE(2, 0, 0, 1)), a, n)  \
       : ((n) > 16                                                            \
              ? _mm256_srli_si256(                                            \
                    _mm256_permute2x128_si256(a, a, _MM_SHUFFLE(2, 0, 0, 1)), \
                    (n)-16)                                                   \
              : _mm256_permute2x128_si256(a, a, _MM_SHUFFLE(2, 0, 0, 1))))

// _mm256_alignr_epi8 works on two 128 bit lanes and can't be used
#define v256_align(a, b, c) \
  ((c) ? v256_or(v256_shr_n_byte(b, c), v256_shl_n_byte(a, 32 - c)) : b)

#define v256_shl_n_8(a, c)                                   \
  _mm256_and_si256(_mm256_set1_epi8((uint8_t)(0xff << (c))), \
                   _mm256_slli_epi16(a, c))
#define v256_shr_n_u8(a, c) \
  _mm256_and_si256(_mm256_set1_epi8(0xff >> (c)), _mm256_srli_epi16(a, c))
#define v256_shr_n_s8(a, c)                                                  \
  _mm256_packs_epi16(_mm256_srai_epi16(_mm256_unpacklo_epi8(a, a), (c) + 8), \
                     _mm256_srai_epi16(_mm256_unpackhi_epi8(a, a), (c) + 8))
#define v256_shl_n_16(a, c) _mm256_slli_epi16(a, c)
#define v256_shr_n_u16(a, c) _mm256_srli_epi16(a, c)
#define v256_shr_n_s16(a, c) _mm256_srai_epi16(a, c)
#define v256_shl_n_32(a, c) _mm256_slli_epi32(a, c)
#define v256_shr_n_u32(a, c) _mm256_srli_epi32(a, c)
#define v256_shr_n_s32(a, c) _mm256_srai_epi32(a, c)
#endif

#endif /* _V256_INTRINSICS_H */
