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
 *   2020      Himanshi Mathur <himanshi18037@iiitd.ac.in>
 */

#if !defined(SIMDE_X86_AVX512_ADDS_H)
#define SIMDE_X86_AVX512_ADDS_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_adds_epi8(simde__m128i src, simde__mmask16 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_adds_epi8(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi8(src, k, simde_mm_adds_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_adds_epi8
  #define _mm_mask_adds_epi8(src, k, a, b) simde_mm_mask_adds_epi8(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_adds_epi8(simde__mmask16 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_maskz_adds_epi8(k, a, b);
  #else
    return simde_mm_maskz_mov_epi8(k, simde_mm_adds_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_adds_epi8
  #define _mm_maskz_adds_epi8(k, a, b) simde_mm_maskz_adds_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_adds_epi16(simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_adds_epi16(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi16(src, k, simde_mm_adds_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_adds_epi16
  #define _mm_mask_adds_epi16(src, k, a, b) simde_mm_mask_adds_epi16(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_adds_epi16(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_maskz_adds_epi16(k, a, b);
  #else
    return simde_mm_maskz_mov_epi16(k, simde_mm_adds_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_adds_epi16
  #define _mm_maskz_adds_epi16(k, a, b) simde_mm_maskz_adds_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_adds_epi8(simde__m256i src, simde__mmask32 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_adds_epi8(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi8(src, k, simde_mm256_adds_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_adds_epi8
  #define _mm256_mask_adds_epi8(src, k, a, b) simde_mm256_mask_adds_epi8(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_adds_epi8(simde__mmask32 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_maskz_adds_epi8(k, a, b);
  #else
    return simde_mm256_maskz_mov_epi8(k, simde_mm256_adds_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_adds_epi8
  #define _mm256_maskz_adds_epi8(k, a, b) simde_mm256_maskz_adds_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_adds_epi16(simde__m256i src, simde__mmask16 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_adds_epi16(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi16(src, k, simde_mm256_adds_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_adds_epi16
  #define _mm256_mask_adds_epi16(src, k, a, b) simde_mm256_mask_adds_epi16(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_adds_epi16(simde__mmask16 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_maskz_adds_epi16(k, a, b);
  #else
    return simde_mm256_maskz_mov_epi16(k, simde_mm256_adds_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_adds_epi16
  #define _mm256_maskz_adds_epi16(k, a, b) simde_mm256_maskz_adds_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_adds_epi8 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_adds_epi8(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if !defined(HEDLEY_INTEL_VERSION)
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_adds_epi8(a_.m256i[i], b_.m256i[i]);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = simde_math_adds_i8(a_.i8[i], b_.i8[i]);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_adds_epi8
  #define _mm512_adds_epi8(a, b) simde_mm512_adds_epi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_adds_epi8 (simde__m512i src, simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_adds_epi8(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_adds_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_adds_epi8
  #define _mm512_mask_adds_epi8(src, k, a, b) simde_mm512_mask_adds_epi8(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_adds_epi8 (simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_adds_epi8(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_adds_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_adds_epi8
  #define _mm512_maskz_adds_epi8(k, a, b) simde_mm512_maskz_adds_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_adds_epi16 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_adds_epi16(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

      #if !defined(HEDLEY_INTEL_VERSION)
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
          r_.m256i[i] = simde_mm256_adds_epi16(a_.m256i[i], b_.m256i[i]);
        }
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
          r_.i16[i] = simde_math_adds_i16(a_.i16[i], b_.i16[i]);
        }
      #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_adds_epi16
  #define _mm512_adds_epi16(a, b) simde_mm512_adds_epi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_adds_epi16 (simde__m512i src, simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_adds_epi16(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_adds_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_adds_epi16
  #define _mm512_mask_adds_epi16(src, k, a, b) simde_mm512_mask_adds_epi16(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_adds_epi16 (simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_adds_epi16(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi16(k, simde_mm512_adds_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_adds_epi16
  #define _mm512_maskz_adds_epi16(k, a, b) simde_mm512_maskz_adds_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_adds_epu8 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_adds_epu8(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if !defined(HEDLEY_INTEL_VERSION)
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_adds_epu8(a_.m128i[i], b_.m128i[i]);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u8) / sizeof(r_.u8[0])) ; i++) {
        r_.u8[i] = simde_math_adds_u8(a_.u8[i], b_.u8[i]);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_adds_epu8
  #define _mm512_adds_epu8(a, b) simde_mm512_adds_epu8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_adds_epu8 (simde__m512i src, simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_adds_epu8(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_adds_epu8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_adds_epu8
  #define _mm512_mask_adds_epu8(src, k, a, b) simde_mm512_mask_adds_epu8(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_adds_epu8 (simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_adds_epu8(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_adds_epu8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_adds_epu8
  #define _mm512_maskz_adds_epu8(k, a, b) simde_mm512_maskz_adds_epu8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_adds_epu16 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_adds_epu16(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if !defined(HEDLEY_INTEL_VERSION)
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_adds_epu16(a_.m256i[i], b_.m256i[i]);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        r_.u16[i] = simde_math_adds_u16(a_.u16[i], b_.u16[i]);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_adds_epu16
  #define _mm512_adds_epu16(a, b) simde_mm512_adds_epu16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_adds_epu16 (simde__m512i src, simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_adds_epu16(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_adds_epu16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_adds_epu16
  #define _mm512_mask_adds_epu16(src, k, a, b) simde_mm512_mask_adds_epu16(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_adds_epu16 (simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_adds_epu16(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi16(k, simde_mm512_adds_epu16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_adds_epu16
  #define _mm512_maskz_adds_epu16(k, a, b) simde_mm512_maskz_adds_epu16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_adds_epi32(simde__m128i a, simde__m128i b) {
  simde__m128i_private
    r_,
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b);

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    r_.neon_i32 = vqaddq_s32(a_.neon_i32, b_.neon_i32);
  #elif defined(SIMDE_POWER_ALTIVEC_P6)
    r_.altivec_i32 = vec_adds(a_.altivec_i32, b_.altivec_i32);
  #else
    #if defined(SIMDE_X86_SSE2_NATIVE)
      /* https://stackoverflow.com/a/56544654/501126 */
      const __m128i int_max = _mm_set1_epi32(INT32_MAX);

      /* normal result (possibly wraps around) */
      const __m128i sum = _mm_add_epi32(a_.n, b_.n);

      /* If result saturates, it has the same sign as both a and b */
      const __m128i sign_bit = _mm_srli_epi32(a_.n, 31); /* shift sign to lowest bit */

      #if defined(SIMDE_X86_AVX512VL_NATIVE)
        const __m128i overflow = _mm_ternarylogic_epi32(a_.n, b_.n, sum, 0x42);
      #else
        const __m128i sign_xor = _mm_xor_si128(a_.n, b_.n);
        const __m128i overflow = _mm_andnot_si128(sign_xor, _mm_xor_si128(a_.n, sum));
      #endif

      #if defined(SIMDE_X86_AVX512DQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
        r_.n = _mm_mask_add_epi32(sum, _mm_movepi32_mask(overflow), int_max, sign_bit);
      #else
        const __m128i saturated = _mm_add_epi32(int_max, sign_bit);

        #if defined(SIMDE_X86_SSE4_1_NATIVE)
          r_.n =
            _mm_castps_si128(
              _mm_blendv_ps(
                _mm_castsi128_ps(sum),
                _mm_castsi128_ps(saturated),
                _mm_castsi128_ps(overflow)
              )
            );
        #else
          const __m128i overflow_mask = _mm_srai_epi32(overflow, 31);
          r_.n =
            _mm_or_si128(
              _mm_and_si128(overflow_mask, saturated),
              _mm_andnot_si128(overflow_mask, sum)
            );
        #endif
      #endif
    #elif defined(SIMDE_VECTOR_SCALAR)
      uint32_t au SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.i32);
      uint32_t bu SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.i32);
      uint32_t ru SIMDE_VECTOR(16) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = simde_math_adds_i32(a_.i32[i], b_.i32[i]);
      }
    #endif
  #endif

  return simde__m128i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_adds_epi32(simde__m256i a, simde__m256i b) {
  simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

  #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
    for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
      r_.m128i[i] = simde_x_mm_adds_epi32(a_.m128i[i], b_.m128i[i]);
    }
  #elif defined(SIMDE_VECTOR_SCALAR)
    uint32_t au SIMDE_VECTOR(32) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.i32);
    uint32_t bu SIMDE_VECTOR(32) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.i32);
    uint32_t ru SIMDE_VECTOR(32) = au + bu;

    au = (au >> 31) + INT32_MAX;

    uint32_t m SIMDE_VECTOR(32) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
    r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (au & ~m) | (ru & m));
  #else
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
      r_.i32[i] = simde_math_adds_i32(a_.i32[i], b_.i32[i]);
    }
  #endif

  return simde__m256i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_adds_epi32(simde__m512i a, simde__m512i b) {
  simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

  #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
    for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
      r_.m128i[i] = simde_x_mm_adds_epi32(a_.m128i[i], b_.m128i[i]);
    }
  #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
    for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
      r_.m256i[i] = simde_x_mm256_adds_epi32(a_.m256i[i], b_.m256i[i]);
    }
  #elif defined(SIMDE_VECTOR_SCALAR)
    uint32_t au SIMDE_VECTOR(64) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.i32);
    uint32_t bu SIMDE_VECTOR(64) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.i32);
    uint32_t ru SIMDE_VECTOR(64) = au + bu;

    au = (au >> 31) + INT32_MAX;

    uint32_t m SIMDE_VECTOR(64) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
    r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (au & ~m) | (ru & m));
  #else
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
      r_.i32[i] = simde_math_adds_i32(a_.i32[i], b_.i32[i]);
    }
  #endif

  return simde__m512i_from_private(r_);
}

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_ADDS_H) */
