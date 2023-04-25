/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2020      Evan Nemerson <evan@nemerson.com>
 *   2020      Christopher Moore <moore@free.fr>
 */

#if !defined(SIMDE_X86_AVX512_MOVM_H)
#define SIMDE_X86_AVX512_MOVM_H

#include "types.h"
#include "../avx2.h"
#include "set.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_movm_epi8 (simde__mmask16 k) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_movm_epi8(k);
  #elif defined(SIMDE_X86_SSSE3_NATIVE)
    const simde__m128i zero = simde_mm_setzero_si128();
    const simde__m128i bits = simde_mm_set_epi16(0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80);
    const simde__m128i shuffle = simde_mm_set_epi8(15, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 0);
    simde__m128i r;

    r = simde_mm_set1_epi16(HEDLEY_STATIC_CAST(short, k));
    r = simde_mm_mullo_epi16(r, bits);
    r = simde_mm_shuffle_epi8(r, shuffle);
    r = simde_mm_cmpgt_epi8(zero, r);

    return r;
  #else
    simde__m128i_private r_;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      static const int8_t pos_data[] = { 7, 6, 5, 4, 3, 2, 1, 0 };
      int8x8_t pos = vld1_s8(pos_data);
      r_.neon_i8 = vcombine_s8(
        vshr_n_s8(vshl_s8(vdup_n_s8(HEDLEY_STATIC_CAST(int8_t, k)), pos), 7),
        vshr_n_s8(vshl_s8(vdup_n_s8(HEDLEY_STATIC_CAST(int8_t, k >> 8)), pos), 7));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = ((k >> i) & 1) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_movm_epi8
  #define _mm_movm_epi8(k) simde_mm_movm_epi8(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_movm_epi8 (simde__mmask32 k) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_movm_epi8(k);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    const simde__m256i zero = simde_mm256_setzero_si256();
    const simde__m256i bits = simde_mm256_broadcastsi128_si256(simde_mm_set_epi16(0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80));
    const simde__m256i shuffle = simde_mm256_broadcastsi128_si256(simde_mm_set_epi8(15, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 0));
    simde__m256i r;

    r = simde_mm256_set_m128i(_mm_set1_epi16(HEDLEY_STATIC_CAST(short, k >> 16)), _mm_set1_epi16(HEDLEY_STATIC_CAST(short, k)));
    r = simde_mm256_mullo_epi16(r, bits);
    r = simde_mm256_shuffle_epi8(r, shuffle);
    r = simde_mm256_cmpgt_epi8(zero, r);

    return r;
  #else
    simde__m256i_private r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      r_.m128i[0] = simde_mm_movm_epi8(HEDLEY_STATIC_CAST(simde__mmask16, k));
      r_.m128i[1] = simde_mm_movm_epi8(HEDLEY_STATIC_CAST(simde__mmask16, k >> 16));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = ((k >> i) & 1) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_movm_epi8
  #define _mm256_movm_epi8(k) simde_mm256_movm_epi8(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_movm_epi8 (simde__mmask64 k) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_movm_epi8(k);
  #else
    simde__m512i_private r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_movm_epi8(HEDLEY_STATIC_CAST(simde__mmask32, k));
      r_.m256i[1] = simde_mm256_movm_epi8(HEDLEY_STATIC_CAST(simde__mmask32, k >> 32));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = ((k >> i) & 1) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_movm_epi8
  #define _mm512_movm_epi8(k) simde_mm512_movm_epi8(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_movm_epi16 (simde__mmask8 k) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_movm_epi16(k);
  #elif defined(SIMDE_X86_SSE2_NATIVE)
    const simde__m128i bits = simde_mm_set_epi16(0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, INT16_MIN /* 0x8000 */);
    simde__m128i r;

    r = simde_mm_set1_epi16(HEDLEY_STATIC_CAST(short, k));
    r = simde_mm_mullo_epi16(r, bits);
    r = simde_mm_srai_epi16(r, 15);

    return r;
  #else
    simde__m128i_private r_;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      static const int16_t pos_data[] = { 15, 14, 13, 12, 11, 10, 9, 8 };
      const int16x8_t pos = vld1q_s16(pos_data);
      r_.neon_i16 = vshrq_n_s16(vshlq_s16(vdupq_n_s16(HEDLEY_STATIC_CAST(int16_t, k)), pos), 15);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = ((k >> i) & 1) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_movm_epi16
  #define _mm_movm_epi16(k) simde_mm_movm_epi16(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_movm_epi16 (simde__mmask16 k) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_movm_epi16(k);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    const __m256i bits = _mm256_set_epi16(0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
                                          0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, INT16_MIN /* 0x8000 */);
    __m256i r;

    r = _mm256_set1_epi16(HEDLEY_STATIC_CAST(short, k));
    r = _mm256_mullo_epi16(r, bits);
    r = _mm256_srai_epi16(r, 15);

    return r;
  #else
    simde__m256i_private r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      r_.m128i[0] = simde_mm_movm_epi16(HEDLEY_STATIC_CAST(simde__mmask8, k));
      r_.m128i[1] = simde_mm_movm_epi16(HEDLEY_STATIC_CAST(simde__mmask8, k >> 8));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = ((k >> i) & 1) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_movm_epi16
  #define _mm256_movm_epi16(k) simde_mm256_movm_epi16(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_movm_epi16 (simde__mmask32 k) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm512_movm_epi16(k);
  #else
    simde__m512i_private r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_movm_epi16(HEDLEY_STATIC_CAST(simde__mmask16, k));
      r_.m256i[1] = simde_mm256_movm_epi16(HEDLEY_STATIC_CAST(simde__mmask16, k >> 16));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = ((k >> i) & 1) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_movm_epi16
  #define _mm512_movm_epi16(k) simde_mm512_movm_epi16(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_movm_epi32 (simde__mmask8 k) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm_movm_epi32(k);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    const __m128i shifts = _mm_set_epi32(28, 29, 30, 31);
    __m128i r;

    r = _mm_set1_epi32(HEDLEY_STATIC_CAST(int, k));
    r = _mm_sllv_epi32(r, shifts);
    r = _mm_srai_epi32(r, 31);

    return r;
  #elif defined(SIMDE_X86_SSE2_NATIVE)
    const simde__m128i bits = simde_mm_set_epi32(0x10000000, 0x20000000, 0x40000000, INT32_MIN /* 0x80000000 */);
    simde__m128i r;

    r = simde_mm_set1_epi16(HEDLEY_STATIC_CAST(short, k));
    r = simde_mm_mullo_epi16(r, bits);
    r = simde_mm_srai_epi32(r, 31);

    return r;
  #else
    simde__m128i_private r_;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      static const int32_t pos_data[] = { 31, 30, 29, 28 };
      const int32x4_t pos = vld1q_s32(pos_data);
      r_.neon_i32 = vshrq_n_s32(vshlq_s32(vdupq_n_s32(HEDLEY_STATIC_CAST(int32_t, k)), pos), 31);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = ((k >> i) & 1) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_movm_epi32
  #define _mm_movm_epi32(k) simde_mm_movm_epi32(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_movm_epi32 (simde__mmask8 k) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm256_movm_epi32(k);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    const __m256i shifts = _mm256_set_epi32(24, 25, 26, 27, 28, 29, 30, 31);
    __m256i r;

    r = _mm256_set1_epi32(HEDLEY_STATIC_CAST(int, k));
    r = _mm256_sllv_epi32(r, shifts);
    r = _mm256_srai_epi32(r, 31);

    return r;
  #else
    simde__m256i_private r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      r_.m128i[0] = simde_mm_movm_epi32(k     );
      r_.m128i[1] = simde_mm_movm_epi32(k >> 4);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = ((k >> i) & 1) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_movm_epi32
  #define _mm256_movm_epi32(k) simde_mm256_movm_epi32(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_movm_epi32 (simde__mmask16 k) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_movm_epi32(k);
  #else
    simde__m512i_private r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_movm_epi32(HEDLEY_STATIC_CAST(simde__mmask8, k     ));
      r_.m256i[1] = simde_mm256_movm_epi32(HEDLEY_STATIC_CAST(simde__mmask8, k >> 8));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = ((k >> i) & 1) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_movm_epi32
  #define _mm512_movm_epi32(k) simde_mm512_movm_epi32(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_movm_epi64 (simde__mmask8 k) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_movm_epi64(k);
    /* N.B. CM: These fallbacks may not be faster as there are only two elements */
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    const __m128i shifts = _mm_set_epi32(30, 30, 31, 31);
    __m128i r;

    r = _mm_set1_epi32(HEDLEY_STATIC_CAST(int, k));
    r = _mm_sllv_epi32(r, shifts);
    r = _mm_srai_epi32(r, 31);

    return r;
  #elif defined(SIMDE_X86_SSE2_NATIVE)
    const simde__m128i bits = simde_mm_set_epi32(0x40000000, 0x40000000, INT32_MIN /* 0x80000000 */, INT32_MIN /* 0x80000000 */);
    simde__m128i r;

    r = simde_mm_set1_epi16(HEDLEY_STATIC_CAST(short, k));
    r = simde_mm_mullo_epi16(r, bits);
    r = simde_mm_srai_epi32(r, 31);

    return r;
  #else
    simde__m128i_private r_;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      static const int64_t pos_data[] = { 63, 62 };
      const int64x2_t pos = vld1q_s64(pos_data);
      r_.neon_i64 = vshrq_n_s64(vshlq_s64(vdupq_n_s64(HEDLEY_STATIC_CAST(int64_t, k)), pos), 63);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
        r_.i64[i] = ((k >> i) & 1) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_movm_epi64
  #define _mm_movm_epi64(k) simde_mm_movm_epi64(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_movm_epi64 (simde__mmask8 k) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_movm_epi64(k);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    const __m256i shifts = _mm256_set_epi32(28, 28, 29, 29, 30, 30, 31, 31);
    __m256i r;

    r = _mm256_set1_epi32(HEDLEY_STATIC_CAST(int, k));
    r = _mm256_sllv_epi32(r, shifts);
    r = _mm256_srai_epi32(r, 31);

    return r;
  #else
    simde__m256i_private r_;

    /* N.B. CM: This fallback may not be faster as there are only four elements */
    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      r_.m128i[0] = simde_mm_movm_epi64(k     );
      r_.m128i[1] = simde_mm_movm_epi64(k >> 2);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
        r_.i64[i] = ((k >> i) & 1) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_movm_epi64
  #define _mm256_movm_epi64(k) simde_mm256_movm_epi64(k)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_movm_epi64 (simde__mmask8 k) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_movm_epi64(k);
  #else
    simde__m512i_private r_;

    /* N.B. CM: Without AVX2 this fallback may not be faster as there are only eight elements */
    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_movm_epi64(k     );
      r_.m256i[1] = simde_mm256_movm_epi64(k >> 4);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
        r_.i64[i] = ((k >> i) & 1) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_movm_epi64
  #define _mm512_movm_epi64(k) simde_mm512_movm_epi64(k)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_MOVM_H) */
