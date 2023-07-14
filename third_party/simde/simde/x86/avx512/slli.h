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
 */

#if !defined(SIMDE_X86_AVX512_SLLI_H)
#define SIMDE_X86_AVX512_SLLI_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"
#include "setzero.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_slli_epi16 (simde__m512i a, const unsigned int imm8)
    SIMDE_REQUIRE_RANGE(imm8, 0, 255) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && (defined(HEDLEY_GCC_VERSION) && ((__GNUC__ == 5 && __GNUC_MINOR__ == 5) || (__GNUC__ == 6 && __GNUC_MINOR__ >= 4)))
    simde__m512i r;

    SIMDE_CONSTIFY_16_(_mm512_slli_epi16, r, simde_mm512_setzero_si512(), imm8, a);

    return r;
  #elif defined(SIMDE_X86_AVX512BW_NATIVE)
    return SIMDE_BUG_IGNORE_SIGN_CONVERSION(_mm512_slli_epi16(a, imm8));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      if(imm8 < 16)
        r_.i16 = HEDLEY_STATIC_CAST(__typeof__(r_.i16), (a_.i16 << HEDLEY_STATIC_CAST(int16_t, imm8)));
      else
        return simde_mm512_setzero_si512();
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = (imm8 < 16) ? HEDLEY_STATIC_CAST(int16_t, a_.i16[i] << (imm8 & 0xff)) : 0;
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_slli_epi16
  #define _mm512_slli_epi16(a, imm8) simde_mm512_slli_epi16(a, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_slli_epi32 (simde__m512i a, unsigned int imm8) {
  /* I guess the restriction was added in 6.4, back-ported to 5.5, then
   * removed (fixed) in 7? */
  #if defined(SIMDE_X86_AVX512F_NATIVE) && (defined(HEDLEY_GCC_VERSION) && ((__GNUC__ == 5 && __GNUC_MINOR__ == 5) || (__GNUC__ == 6 && __GNUC_MINOR__ >= 4)))
    simde__m512i r;

    SIMDE_CONSTIFY_32_(_mm512_slli_epi32, r, simde_mm512_setzero_si512(), imm8, a);

    return r;
  #elif defined(SIMDE_X86_AVX512F_NATIVE)
    return SIMDE_BUG_IGNORE_SIGN_CONVERSION(_mm512_slli_epi32(a, imm8));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    /* The Intel Intrinsics Guide says that only the 8 LSBits of imm8 are
    * used.  In this case we should do "imm8 &= 0xff".  However in
    * practice all bits are used. */
    if (imm8 > 31) {
      simde_memset(&r_, 0, sizeof(r_));
    } else {
      #if defined(SIMDE_X86_AVX2_NATIVE)
        r_.m256i[0] = simde_mm256_slli_epi32(a_.m256i[0], HEDLEY_STATIC_CAST(int, imm8));
        r_.m256i[1] = simde_mm256_slli_epi32(a_.m256i[1], HEDLEY_STATIC_CAST(int, imm8));
      #elif defined(SIMDE_X86_SSE2_NATIVE)
        r_.m128i[0] = simde_mm_slli_epi32(a_.m128i[0], HEDLEY_STATIC_CAST(int, imm8));
        r_.m128i[1] = simde_mm_slli_epi32(a_.m128i[1], HEDLEY_STATIC_CAST(int, imm8));
        r_.m128i[2] = simde_mm_slli_epi32(a_.m128i[2], HEDLEY_STATIC_CAST(int, imm8));
        r_.m128i[3] = simde_mm_slli_epi32(a_.m128i[3], HEDLEY_STATIC_CAST(int, imm8));
      #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
        r_.u32 = a_.u32 << imm8;
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
          r_.u32[i] = a_.u32[i] << imm8;
        }
      #endif
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_slli_epi32
  #define _mm512_slli_epi32(a, imm8) simde_mm512_slli_epi32(a, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_slli_epi64 (simde__m512i a, unsigned int imm8) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && (defined(HEDLEY_GCC_VERSION) && ((__GNUC__ == 5 && __GNUC_MINOR__ == 5) || (__GNUC__ == 6 && __GNUC_MINOR__ >= 4)))
    simde__m512i r;

    SIMDE_CONSTIFY_64_(_mm512_slli_epi64, r, simde_mm512_setzero_si512(), imm8, a);

    return r;
  #elif defined(SIMDE_X86_AVX512F_NATIVE)
    return SIMDE_BUG_IGNORE_SIGN_CONVERSION(_mm512_slli_epi64(a, imm8));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    /* The Intel Intrinsics Guide says that only the 8 LSBits of imm8 are
    * used.  In this case we should do "imm8 &= 0xff".  However in
    * practice all bits are used. */
    if (imm8 > 63) {
      simde_memset(&r_, 0, sizeof(r_));
    } else {
      #if defined(SIMDE_X86_AVX2_NATIVE)
        r_.m256i[0] = simde_mm256_slli_epi64(a_.m256i[0], HEDLEY_STATIC_CAST(int, imm8));
        r_.m256i[1] = simde_mm256_slli_epi64(a_.m256i[1], HEDLEY_STATIC_CAST(int, imm8));
      #elif defined(SIMDE_X86_SSE2_NATIVE)
        r_.m128i[0] = simde_mm_slli_epi64(a_.m128i[0], HEDLEY_STATIC_CAST(int, imm8));
        r_.m128i[1] = simde_mm_slli_epi64(a_.m128i[1], HEDLEY_STATIC_CAST(int, imm8));
        r_.m128i[2] = simde_mm_slli_epi64(a_.m128i[2], HEDLEY_STATIC_CAST(int, imm8));
        r_.m128i[3] = simde_mm_slli_epi64(a_.m128i[3], HEDLEY_STATIC_CAST(int, imm8));
      #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_97248)
        r_.u64 = a_.u64 << imm8;
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
          r_.u64[i] = a_.u64[i] << imm8;
        }
      #endif
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_slli_epi64
  #define _mm512_slli_epi64(a, imm8) simde_mm512_slli_epi64(a, imm8)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SLLI_H) */
