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

#if !defined(SIMDE_X86_AVX512_SRLV_H)
#define SIMDE_X86_AVX512_SRLV_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_srlv_epi16 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_srlv_epi16(a, b);
  #else
    simde__m128i_private
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), (b_.u16 < 16)) & (a_.u16 >> b_.u16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        r_.u16[i] = (b_.u16[i] < 16) ? (a_.u16[i] >> b_.u16[i]) : 0;
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_srlv_epi16
  #define _mm_srlv_epi16(a, b) simde_mm_srlv_epi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_srlv_epi16(simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_srlv_epi16(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi16(src, k, simde_mm_srlv_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_srlv_epi16
  #define _mm_mask_srlv_epi16(src, k, a, b) simde_mm_mask_srlv_epi16(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_srlv_epi16(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_maskz_srlv_epi16(k, a, b);
  #else
    return simde_mm_maskz_mov_epi16(k, simde_mm_srlv_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_srlv_epi16
  #define _mm_maskz_srlv_epi16(k, a, b) simde_mm_maskz_srlv_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_srlv_epi32(simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_srlv_epi32(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi32(src, k, simde_mm_srlv_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_srlv_epi32
  #define _mm_mask_srlv_epi32(src, k, a, b) simde_mm_mask_srlv_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_srlv_epi32(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_srlv_epi32(k, a, b);
  #else
    return simde_mm_maskz_mov_epi32(k, simde_mm_srlv_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_srlv_epi32
  #define _mm_maskz_srlv_epi32(k, a, b) simde_mm_maskz_srlv_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_srlv_epi64(simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_srlv_epi64(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi64(src, k, simde_mm_srlv_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_srlv_epi64
  #define _mm_mask_srlv_epi64(src, k, a, b) simde_mm_mask_srlv_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_srlv_epi64(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_srlv_epi64(k, a, b);
  #else
    return simde_mm_maskz_mov_epi64(k, simde_mm_srlv_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_srlv_epi64
  #define _mm_maskz_srlv_epi64(k, a, b) simde_mm_maskz_srlv_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_srlv_epi16 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_srlv_epi16(a, b);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b),
      r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_srlv_epi16(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), (b_.u16 < 16)) & (a_.u16 >> b_.u16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        r_.u16[i] = (b_.u16[i] < 16) ? (a_.u16[i] >> b_.u16[i]) : 0;
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_srlv_epi16
  #define _mm256_srlv_epi16(a, b) simde_mm256_srlv_epi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_srlv_epi16 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_srlv_epi16(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b),
      r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_srlv_epi16(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), (b_.u16 < 16)) & (a_.u16 >> b_.u16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        r_.u16[i] = (b_.u16[i] < 16) ? (a_.u16[i] >> b_.u16[i]) : 0;
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_srlv_epi16
  #define _mm512_srlv_epi16(a, b) simde_mm512_srlv_epi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_srlv_epi32 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_srlv_epi32(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b),
      r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_srlv_epi32(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.u32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u32), (b_.u32 < 32)) & (a_.u32 >> b_.u32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
        r_.u32[i] = (b_.u32[i] < 32) ? (a_.u32[i] >> b_.u32[i]) : 0;
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_srlv_epi32
  #define _mm512_srlv_epi32(a, b) simde_mm512_srlv_epi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_srlv_epi64 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_srlv_epi64(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b),
      r_;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_srlv_epi64(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.u64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u64), (b_.u64 < 64)) & (a_.u64 >> b_.u64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
        r_.u64[i] = (b_.u64[i] < 64) ? (a_.u64[i] >> b_.u64[i]) : 0;
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_srlv_epi64
  #define _mm512_srlv_epi64(a, b) simde_mm512_srlv_epi64(a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SRLV_H) */
