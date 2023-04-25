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

#if !defined(SIMDE_X86_AVX512_CMPGT_H)
#define SIMDE_X86_AVX512_CMPGT_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"
#include "mov_mask.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_mm512_cmpgt_epi8_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cmpgt_epi8_mask(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);
    simde__mmask64 r;

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256) && !defined(HEDLEY_INTEL_VERSION)
      r = 0;

      SIMDE_VECTORIZE_REDUCTION(|:r)
      for (size_t i = 0 ; i < (sizeof(a_.m256i) / sizeof(a_.m256i[0])) ; i++) {
        const uint32_t t = HEDLEY_STATIC_CAST(uint32_t, simde_mm256_movemask_epi8(simde_mm256_cmpgt_epi8(a_.m256i[i], b_.m256i[i])));
        r |= HEDLEY_STATIC_CAST(uint64_t, t) << HEDLEY_STATIC_CAST(uint64_t, i * 32);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      simde__m512i_private tmp;

      tmp.i8 = HEDLEY_REINTERPRET_CAST(__typeof__(tmp.i8), a_.i8 > b_.i8);
      r = simde_mm512_movepi8_mask(simde__m512i_from_private(tmp));
    #else
      r = 0;

      SIMDE_VECTORIZE_REDUCTION(|:r)
      for (size_t i = 0 ; i < (sizeof(a_.i8) / sizeof(a_.i8[0])) ; i++) {
        r |= (a_.i8[i] > b_.i8[i]) ? (UINT64_C(1) << i) : 0;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpgt_epi8_mask
  #define _mm512_cmpgt_epi8_mask(a, b) simde_mm512_cmpgt_epi8_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_mm512_cmpgt_epu8_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cmpgt_epu8_mask(a, b);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);
    simde__mmask64 r = 0;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      simde__m512i_private tmp;

      tmp.i8 = HEDLEY_REINTERPRET_CAST(__typeof__(tmp.i8), a_.u8 > b_.u8);
      r = simde_mm512_movepi8_mask(simde__m512i_from_private(tmp));
    #else
      SIMDE_VECTORIZE_REDUCTION(|:r)
      for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0])) ; i++) {
        r |= (a_.u8[i] > b_.u8[i]) ? (UINT64_C(1) << i) : 0;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpgt_epu8_mask
  #define _mm512_cmpgt_epu8_mask(a, b) simde_mm512_cmpgt_epu8_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_cmpgt_epi32_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cmpgt_epi32_mask(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
      r_.m256i[i] = simde_mm256_cmpgt_epi32(a_.m256i[i], b_.m256i[i]);
    }

    return simde_mm512_movepi32_mask(simde__m512i_from_private(r_));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpgt_epi32_mask
  #define _mm512_cmpgt_epi32_mask(a, b) simde_mm512_cmpgt_epi32_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_mask_cmpgt_epi32_mask (simde__mmask16 k1, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cmpgt_epi32_mask(k1, a, b);
  #else
    return simde_mm512_cmpgt_epi32_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpgt_epi32_mask
  #define _mm512_mask_cmpgt_epi32_mask(k1, a, b) simde_mm512_mask_cmpgt_epi32_mask(k1, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_cmpgt_epi64_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cmpgt_epi64_mask(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
      r_.m256i[i] = simde_mm256_cmpgt_epi64(a_.m256i[i], b_.m256i[i]);
    }

    return simde_mm512_movepi64_mask(simde__m512i_from_private(r_));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpgt_epi64_mask
  #define _mm512_cmpgt_epi64_mask(a, b) simde_mm512_cmpgt_epi64_mask(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_mask_cmpgt_epi64_mask (simde__mmask8 k1, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cmpgt_epi64_mask(k1, a, b);
  #else
    return simde_mm512_cmpgt_epi64_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpgt_epi64_mask
  #define _mm512_mask_cmpgt_epi64_mask(k1, a, b) simde_mm512_mask_cmpgt_epi64_mask(k1, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_CMPGT_H) */
