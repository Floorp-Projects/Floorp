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
 *   2020      Hidayat Khan <huk2209@gmail.com>
 *   2020      Christopher Moore <moore@free.fr>
 *   2021      Andrew Rodriguez <anrodriguez@linkedin.com>
 */

#if !defined(SIMDE_X86_AVX512_TEST_H)
#define SIMDE_X86_AVX512_TEST_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_test_epi32_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_test_epi32_mask(a, b);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);
    simde__mmask8 r = 0;

    SIMDE_VECTORIZE_REDUCTION(|:r)
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r |= HEDLEY_STATIC_CAST(simde__mmask16, !!(a_.i32[i] & b_.i32[i]) << i);
    }

    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_test_epi32_mask
#define _mm256_test_epi32_mask(a, b) simde_mm256_test_epi32_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_test_epi32_mask (simde__mmask8 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_test_epi32_mask(k1, a, b);
  #else
    return simde_mm256_test_epi32_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_test_epi32_mask
  #define _mm256_mask_test_epi32_mask(k1, a, b) simde_mm256_mask_test_epi32_mask(k1, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm512_test_epi16_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_test_epi16_mask(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);
    simde__mmask32 r = 0;

    SIMDE_VECTORIZE_REDUCTION(|:r)
    for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0])) ; i++) {
      r |= HEDLEY_STATIC_CAST(simde__mmask32, !!(a_.i16[i] & b_.i16[i]) << i);
    }

    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_test_epi16_mask
  #define _mm512_test_epi16_mask(a, b) simde_mm512_test_epi16_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_test_epi32_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_test_epi32_mask(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);
    simde__mmask16 r = 0;

    SIMDE_VECTORIZE_REDUCTION(|:r)
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r |= HEDLEY_STATIC_CAST(simde__mmask16, !!(a_.i32[i] & b_.i32[i]) << i);
    }

    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_test_epi32_mask
#define _mm512_test_epi32_mask(a, b) simde_mm512_test_epi32_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_test_epi64_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_test_epi64_mask(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);
    simde__mmask8 r = 0;

    SIMDE_VECTORIZE_REDUCTION(|:r)
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r |= HEDLEY_STATIC_CAST(simde__mmask8, !!(a_.i64[i] & b_.i64[i]) << i);
    }

    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_test_epi64_mask
  #define _mm512_test_epi64_mask(a, b) simde_mm512_test_epi64_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_mm512_test_epi8_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_test_epi8_mask(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);
    simde__mmask64 r = 0;

    SIMDE_VECTORIZE_REDUCTION(|:r)
    for (size_t i = 0 ; i < (sizeof(a_.i8) / sizeof(a_.i8[0])) ; i++) {
      r |= HEDLEY_STATIC_CAST(simde__mmask64, HEDLEY_STATIC_CAST(uint64_t, !!(a_.i8[i] & b_.i8[i])) << i);
    }

    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_test_epi8_mask
  #define _mm512_test_epi8_mask(a, b) simde_mm512_test_epi8_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm512_mask_test_epi16_mask (simde__mmask32 k1, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_test_epi16_mask(k1, a, b);
  #else
    return simde_mm512_test_epi16_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_test_epi16_mask
  #define _mm512_mask_test_epi16_mask(k1, a, b) simde_mm512_mask_test_epi16_mask(k1, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_mask_test_epi32_mask (simde__mmask16 k1, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_test_epi32_mask(k1, a, b);
  #else
    return simde_mm512_test_epi32_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_test_epi32_mask
  #define _mm512_mask_test_epi32_mask(k1, a, b) simde_mm512_mask_test_epi32_mask(k1, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_mask_test_epi64_mask (simde__mmask8 k1, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_test_epi64_mask(k1, a, b);
  #else
    return simde_mm512_test_epi64_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_test_epi64_mask
  #define _mm512_mask_test_epi64_mask(k1, a, b) simde_mm512_mask_test_epi64_mask(k1, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_mm512_mask_test_epi8_mask (simde__mmask64 k1, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_test_epi8_mask(k1, a, b);
  #else
    return simde_mm512_test_epi8_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_test_epi8_mask
  #define _mm512_mask_test_epi8_mask(k1, a, b) simde_mm512_mask_test_epi8_mask(k1, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_TEST_H) */
