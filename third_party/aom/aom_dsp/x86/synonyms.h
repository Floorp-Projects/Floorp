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

#ifndef AOM_DSP_X86_SYNONYMS_H_
#define AOM_DSP_X86_SYNONYMS_H_

#include <immintrin.h>

#include "./aom_config.h"
#include "aom/aom_integer.h"

/**
 * Various reusable shorthands for x86 SIMD intrinsics.
 *
 * Intrinsics prefixed with xx_ operate on or return 128bit XMM registers.
 * Intrinsics prefixed with yy_ operate on or return 256bit YMM registers.
 */

// Loads and stores to do away with the tedium of casting the address
// to the right type.
static INLINE __m128i xx_loadl_32(const void *a) {
  return _mm_cvtsi32_si128(*(const uint32_t *)a);
}

static INLINE __m128i xx_loadl_64(const void *a) {
  return _mm_loadl_epi64((const __m128i *)a);
}

static INLINE __m128i xx_load_128(const void *a) {
  return _mm_load_si128((const __m128i *)a);
}

static INLINE __m128i xx_loadu_128(const void *a) {
  return _mm_loadu_si128((const __m128i *)a);
}

static INLINE void xx_storel_32(void *const a, const __m128i v) {
  *(uint32_t *)a = _mm_cvtsi128_si32(v);
}

static INLINE void xx_storel_64(void *const a, const __m128i v) {
  _mm_storel_epi64((__m128i *)a, v);
}

static INLINE void xx_store_128(void *const a, const __m128i v) {
  _mm_store_si128((__m128i *)a, v);
}

static INLINE void xx_storeu_128(void *const a, const __m128i v) {
  _mm_storeu_si128((__m128i *)a, v);
}

static INLINE __m128i xx_round_epu16(__m128i v_val_w) {
  return _mm_avg_epu16(v_val_w, _mm_setzero_si128());
}

static INLINE __m128i xx_roundn_epu16(__m128i v_val_w, int bits) {
  const __m128i v_s_w = _mm_srli_epi16(v_val_w, bits - 1);
  return _mm_avg_epu16(v_s_w, _mm_setzero_si128());
}

static INLINE __m128i xx_roundn_epu32(__m128i v_val_d, int bits) {
  const __m128i v_bias_d = _mm_set1_epi32((1 << bits) >> 1);
  const __m128i v_tmp_d = _mm_add_epi32(v_val_d, v_bias_d);
  return _mm_srli_epi32(v_tmp_d, bits);
}

// This is equivalent to ROUND_POWER_OF_TWO(v_val_d, bits)
static INLINE __m128i xx_roundn_epi32_unsigned(__m128i v_val_d, int bits) {
  const __m128i v_bias_d = _mm_set1_epi32((1 << bits) >> 1);
  const __m128i v_tmp_d = _mm_add_epi32(v_val_d, v_bias_d);
  return _mm_srai_epi32(v_tmp_d, bits);
}

// This is equivalent to ROUND_POWER_OF_TWO_SIGNED(v_val_d, bits)
static INLINE __m128i xx_roundn_epi32(__m128i v_val_d, int bits) {
  const __m128i v_bias_d = _mm_set1_epi32((1 << bits) >> 1);
  const __m128i v_sign_d = _mm_srai_epi32(v_val_d, 31);
  const __m128i v_tmp_d =
      _mm_add_epi32(_mm_add_epi32(v_val_d, v_bias_d), v_sign_d);
  return _mm_srai_epi32(v_tmp_d, bits);
}

#endif  // AOM_DSP_X86_SYNONYMS_H_
