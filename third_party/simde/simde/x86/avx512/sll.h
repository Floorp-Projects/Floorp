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
 */

#if !defined(SIMDE_X86_AVX512_SLL_H)
#define SIMDE_X86_AVX512_SLL_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"
#include "setzero.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_sll_epi16 (simde__m512i a, simde__m128i count) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_sll_epi16(a, count);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_sll_epi16(a_.m256i[i], count);
      }
    #else
      simde__m128i_private
        count_ = simde__m128i_to_private(count);

      uint64_t shift = HEDLEY_STATIC_CAST(uint64_t, count_.i64[0]);
      if (shift > 15)
        return simde_mm512_setzero_si512();

      #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
        r_.i16 = a_.i16 << HEDLEY_STATIC_CAST(int16_t, shift);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
          r_.i16[i] = HEDLEY_STATIC_CAST(int16_t, a_.i16[i] << (shift));
        }
      #endif
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_sll_epi16
  #define _mm512_sll_epi16(a, count) simde_mm512_sll_epi16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_sll_epi16 (simde__m512i src, simde__mmask32 k, simde__m512i a, simde__m128i count) {
  #if defined(SIMDE_X86_AVX51BW_NATIVE)
    return _mm512_mask_sll_epi16(src, k, a, count);
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_sll_epi16(a, count));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_sll_epi16
  #define _mm512_mask_sll_epi16(src, k, a, count) simde_mm512_mask_sll_epi16(src, k, a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_sll_epi16 (simde__mmask32 k, simde__m512i a, simde__m128i count) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_sll_epi16(k, a, count);
  #else
    return simde_mm512_maskz_mov_epi16(k, simde_mm512_sll_epi16(a, count));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_sll_epi16
  #define _mm512_maskz_sll_epi16(src, k, a, count) simde_mm512_maskz_sll_epi16(src, k, a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_sll_epi32 (simde__m512i a, simde__m128i count) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_sll_epi32(a, count);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
        for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
          r_.m256i[i] = simde_mm256_sll_epi32(a_.m256i[i], count);
        }
    #else
      simde__m128i_private
        count_ = simde__m128i_to_private(count);

      uint64_t shift = HEDLEY_STATIC_CAST(uint64_t, count_.i64[0]);
      if (shift > 31)
        return simde_mm512_setzero_si512();

      #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
        r_.i32 = a_.i32 << HEDLEY_STATIC_CAST(int32_t, shift);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
          r_.i32[i] = HEDLEY_STATIC_CAST(int32_t, a_.i32[i] << (shift));
        }
      #endif
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_sll_epi32
  #define _mm512_sll_epi32(a, count) simde_mm512_sll_epi32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_sll_epi32(simde__m512i src, simde__mmask16 k, simde__m512i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_sll_epi32(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_sll_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_sll_epi32
  #define _mm512_mask_sll_epi32(src, k, a, b) simde_mm512_mask_sll_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_sll_epi32(simde__mmask16 k, simde__m512i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_sll_epi32(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_sll_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_sll_epi32
  #define _mm512_maskz_sll_epi32(k, a, b) simde_mm512_maskz_sll_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_sll_epi64 (simde__m512i a, simde__m128i count) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_sll_epi64(a, count);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
        for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
          r_.m256i[i] = simde_mm256_sll_epi64(a_.m256i[i], count);
        }
    #else
      simde__m128i_private
        count_ = simde__m128i_to_private(count);

      uint64_t shift = HEDLEY_STATIC_CAST(uint64_t, count_.i64[0]);
      if (shift > 63)
        return simde_mm512_setzero_si512();

      #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
        r_.i64 = a_.i64 << HEDLEY_STATIC_CAST(int64_t, shift);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
          r_.i64[i] = HEDLEY_STATIC_CAST(int64_t, a_.i64[i] << (shift));
        }
      #endif
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_sll_epi64
  #define _mm512_sll_epi64(a, count) simde_mm512_sll_epi64(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_sll_epi64(simde__m512i src, simde__mmask8 k, simde__m512i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_sll_epi64(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_sll_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_sll_epi64
  #define _mm512_mask_sll_epi64(src, k, a, b) simde_mm512_mask_sll_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_sll_epi64(simde__mmask8 k, simde__m512i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_sll_epi64(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_sll_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_sll_epi64
  #define _mm512_maskz_sll_epi64(k, a, b) simde_mm512_maskz_sll_epi64(k, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SLL_H) */
