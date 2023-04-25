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
 *   2020-2021 Evan Nemerson <evan@nemerson.com>
 *   2020      Christopher Moore <moore@free.fr>
 *   2021      Andrew Rodriguez <anrodriguez@linkedin.com>
 */

#if !defined(SIMDE_X86_AVX512_CMPGE_H)
#define SIMDE_X86_AVX512_CMPGE_H

#include "types.h"
#include "mov.h"
#include "mov_mask.h"
#include "movm.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_cmpge_epi8 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_movm_epi8(_mm_cmpge_epi8_mask(a, b));
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_u8 = vcgeq_s8(a_.neon_i8, b_.neon_i8);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_i8x16_ge(a_.wasm_v128, b_.wasm_v128);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_i8 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), vec_cmpge(a_.altivec_i8, b_.altivec_i8));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i8 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i8), a_.i8 >= b_.i8);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i8) / sizeof(a_.i8[0])) ; i++) {
        r_.i8[i] = (a_.i8[i] >= b_.i8[i]) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm_cmpge_epi8_mask (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cmpge_epi8_mask(a, b);
  #else
    return simde_mm_movepi8_mask(simde_x_mm_cmpge_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi8_mask
  #define _mm512_cmpge_epi8_mask(a, b) simde_mm512_cmpge_epi8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm_mask_cmpge_epi8_mask(simde__mmask16 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_cmpge_epi8_mask(k, a, b);
  #else
    return k & simde_mm_cmpge_epi8_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VBW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpge_epi8_mask
  #define _mm_mask_cmpge_epi8_mask(src, k, a, b) simde_mm_mask_cmpge_epi8_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_cmpge_epi8 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return simde_mm256_movm_epi8(_mm256_cmpge_epi8_mask(a, b));
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epi8(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i8 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i8), a_.i8 >= b_.i8);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i8) / sizeof(a_.i8[0])) ; i++) {
        r_.i8[i] = (a_.i8[i] >= b_.i8[i]) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm256_cmpge_epi8_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cmpge_epi8_mask(a, b);
  #else
    return simde_mm256_movepi8_mask(simde_x_mm256_cmpge_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VBW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi8_mask
  #define _mm512_cmpge_epi8_mask(a, b) simde_mm512_cmpge_epi8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm256_mask_cmpge_epi8_mask(simde__mmask32 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_cmpge_epi8_mask(k, a, b);
  #else
    return k & simde_mm256_cmpge_epi8_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpge_epi8_mask
  #define _mm256_mask_cmpge_epi8_mask(src, k, a, b) simde_mm256_mask_cmpge_epi8_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_cmpge_epi8 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return simde_mm512_movm_epi8(_mm512_cmpge_epi8_mask(a, b));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epi8(a_.m128i[i], b_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_x_mm256_cmpge_epi8(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i8 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i8), a_.i8 >= b_.i8);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i8) / sizeof(a_.i8[0])) ; i++) {
        r_.i8[i] = (a_.i8[i] >= b_.i8[i]) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_mm512_cmpge_epi8_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cmpge_epi8_mask(a, b);
  #else
    return simde_mm512_movepi8_mask(simde_x_mm512_cmpge_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi8_mask
  #define _mm512_cmpge_epi8_mask(a, b) simde_mm512_cmpge_epi8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_mm512_mask_cmpge_epi8_mask(simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_cmpge_epi8_mask(k, a, b);
  #else
    return k & simde_mm512_cmpge_epi8_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpge_epi8_mask
  #define _mm512_mask_cmpge_epi8_mask(src, k, a, b) simde_mm512_mask_cmpge_epi8_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_cmpge_epu8 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_movm_epi8(_mm_cmpge_epu8_mask(a, b));
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_u8 = vcgeq_u8(a_.neon_u8, b_.neon_u8);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_u8x16_ge(a_.wasm_v128, b_.wasm_v128);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_u8 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), vec_cmpge(a_.altivec_u8, b_.altivec_u8));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u8 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u8), a_.u8 >= b_.u8);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0])) ; i++) {
        r_.u8[i] = (a_.u8[i] >= b_.u8[i]) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm_cmpge_epu8_mask (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cmpge_epu8_mask(a, b);
  #else
    return simde_mm_movepi8_mask(simde_x_mm_cmpge_epu8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu8_mask
  #define _mm512_cmpge_epu8_mask(a, b) simde_mm512_cmpge_epu8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm_mask_cmpge_epu8_mask(simde__mmask16 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_cmpge_epu8_mask(k, a, b);
  #else
    return k & simde_mm_cmpge_epu8_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpge_epu8_mask
  #define _mm_mask_cmpge_epu8_mask(src, k, a, b) simde_mm_mask_cmpge_epu8_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_cmpge_epu8 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return simde_mm256_movm_epi8(_mm256_cmpge_epu8_mask(a, b));
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epu8(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u8 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u8), a_.u8 >= b_.u8);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0])) ; i++) {
        r_.u8[i] = (a_.u8[i] >= b_.u8[i]) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm256_cmpge_epu8_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cmpge_epu8_mask(a, b);
  #else
    return simde_mm256_movepi8_mask(simde_x_mm256_cmpge_epu8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu8_mask
  #define _mm512_cmpge_epu8_mask(a, b) simde_mm512_cmpge_epu8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm256_mask_cmpge_epu8_mask(simde__mmask32 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_cmpge_epu8_mask(k, a, b);
  #else
    return k & simde_mm256_cmpge_epu8_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpge_epu8_mask
  #define _mm256_mask_cmpge_epu8_mask(src, k, a, b) simde_mm256_mask_cmpge_epu8_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_cmpge_epu8 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return simde_mm512_movm_epi8(_mm512_cmpge_epu8_mask(a, b));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epu8(a_.m128i[i], b_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_x_mm256_cmpge_epu8(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u8 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u8), a_.u8 >= b_.u8);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0])) ; i++) {
        r_.u8[i] = (a_.u8[i] >= b_.u8[i]) ? ~INT8_C(0) : INT8_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_mm512_cmpge_epu8_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cmpge_epu8_mask(a, b);
  #else
    return simde_mm512_movepi8_mask(simde_x_mm512_cmpge_epu8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu8_mask
  #define _mm512_cmpge_epu8_mask(a, b) simde_mm512_cmpge_epu8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_mm512_mask_cmpge_epu8_mask(simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_cmpge_epu8_mask(k, a, b);
  #else
    return k & simde_mm512_cmpge_epu8_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpge_epu8_mask
  #define _mm512_mask_cmpge_epu8_mask(src, k, a, b) simde_mm512_mask_cmpge_epu8_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_cmpge_epi16 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_movm_epi16(_mm_cmpge_epi16_mask(a, b));
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_u16 = vcgeq_s16(a_.neon_i16, b_.neon_i16);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_i16x8_ge(a_.wasm_v128, b_.wasm_v128);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_i16 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed short), vec_cmpge(a_.altivec_i16, b_.altivec_i16));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i16), a_.i16 >= b_.i16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0])) ; i++) {
        r_.i16[i] = (a_.i16[i] >= b_.i16[i]) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpge_epi16_mask (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cmpge_epi16_mask(a, b);
  #else
    return simde_mm_movepi16_mask(simde_x_mm_cmpge_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi16_mask
  #define _mm512_cmpge_epi16_mask(a, b) simde_mm512_cmpge_epi16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpge_epi16_mask(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_cmpge_epi16_mask(k, a, b);
  #else
    return k & simde_mm_cmpge_epi16_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpge_epi16_mask
  #define _mm_mask_cmpge_epi16_mask(src, k, a, b) simde_mm_mask_cmpge_epi16_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_cmpge_epi16 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return simde_mm256_movm_epi16(_mm256_cmpge_epi16_mask(a, b));
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epi16(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i16), a_.i16 >= b_.i16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0])) ; i++) {
        r_.i16[i] = (a_.i16[i] >= b_.i16[i]) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm256_cmpge_epi16_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cmpge_epi16_mask(a, b);
  #else
    return simde_mm256_movepi16_mask(simde_x_mm256_cmpge_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi16_mask
  #define _mm512_cmpge_epi16_mask(a, b) simde_mm512_cmpge_epi16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm256_mask_cmpge_epi16_mask(simde__mmask16 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_cmpge_epi16_mask(k, a, b);
  #else
    return k & simde_mm256_cmpge_epi16_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpge_epi16_mask
  #define _mm256_mask_cmpge_epi16_mask(src, k, a, b) simde_mm256_mask_cmpge_epi16_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_cmpge_epi16 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return simde_mm512_movm_epi16(_mm512_cmpge_epi16_mask(a, b));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epi16(a_.m128i[i], b_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_x_mm256_cmpge_epi16(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i16), a_.i16 >= b_.i16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0])) ; i++) {
        r_.i16[i] = (a_.i16[i] >= b_.i16[i]) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm512_cmpge_epi16_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cmpge_epi16_mask(a, b);
  #else
    return simde_mm512_movepi16_mask(simde_x_mm512_cmpge_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi16_mask
  #define _mm512_cmpge_epi16_mask(a, b) simde_mm512_cmpge_epi16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm512_mask_cmpge_epi16_mask(simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_cmpge_epi16_mask(k, a, b);
  #else
    return k & simde_mm512_cmpge_epi16_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpge_epi16_mask
  #define _mm512_mask_cmpge_epi16_mask(src, k, a, b) simde_mm512_mask_cmpge_epi16_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_cmpge_epu16 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_movm_epi16(_mm_cmpge_epu16_mask(a, b));
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_u16 = vcgeq_u16(a_.neon_u16, b_.neon_u16);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_u16x8_ge(a_.wasm_v128, b_.wasm_v128);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_u16 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short), vec_cmpge(a_.altivec_u16, b_.altivec_u16));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), a_.u16 >= b_.u16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u16) / sizeof(a_.u16[0])) ; i++) {
        r_.u16[i] = (a_.u16[i] >= b_.u16[i]) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpge_epu16_mask (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cmpge_epu16_mask(a, b);
  #else
    return simde_mm_movepi16_mask(simde_x_mm_cmpge_epu16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu16_mask
  #define _mm512_cmpge_epu16_mask(a, b) simde_mm512_cmpge_epu16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpge_epu16_mask(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_cmpge_epu16_mask(k, a, b);
  #else
    return k & simde_mm_cmpge_epu16_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpge_epu16_mask
  #define _mm_mask_cmpge_epu16_mask(src, k, a, b) simde_mm_mask_cmpge_epu16_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_cmpge_epu16 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return simde_mm256_movm_epi16(_mm256_cmpge_epu16_mask(a, b));
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epu16(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), a_.u16 >= b_.u16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u16) / sizeof(a_.u16[0])) ; i++) {
        r_.u16[i] = (a_.u16[i] >= b_.u16[i]) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm256_cmpge_epu16_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cmpge_epu16_mask(a, b);
  #else
    return simde_mm256_movepi16_mask(simde_x_mm256_cmpge_epu16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu16_mask
  #define _mm512_cmpge_epu16_mask(a, b) simde_mm512_cmpge_epu16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm256_mask_cmpge_epu16_mask(simde__mmask16 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_cmpge_epu16_mask(k, a, b);
  #else
    return k & simde_mm256_cmpge_epu16_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpge_epu16_mask
  #define _mm256_mask_cmpge_epu16_mask(src, k, a, b) simde_mm256_mask_cmpge_epu16_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_cmpge_epu16 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return simde_mm512_movm_epi16(_mm512_cmpge_epu16_mask(a, b));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epu16(a_.m128i[i], b_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_x_mm256_cmpge_epu16(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), a_.u16 >= b_.u16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u16) / sizeof(a_.u16[0])) ; i++) {
        r_.u16[i] = (a_.u16[i] >= b_.u16[i]) ? ~INT16_C(0) : INT16_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm512_cmpge_epu16_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cmpge_epu16_mask(a, b);
  #else
    return simde_mm512_movepi16_mask(simde_x_mm512_cmpge_epu16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu16_mask
  #define _mm512_cmpge_epu16_mask(a, b) simde_mm512_cmpge_epu16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm512_mask_cmpge_epu16_mask(simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_cmpge_epu16_mask(k, a, b);
  #else
    return k & simde_mm512_cmpge_epu16_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpge_epu16_mask
  #define _mm512_mask_cmpge_epu16_mask(src, k, a, b) simde_mm512_mask_cmpge_epu16_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_cmpge_epi32 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm_movm_epi32(_mm_cmpge_epi32_mask(a, b));
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_u32 = vcgeq_s32(a_.neon_i32, b_.neon_i32);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_i32x4_ge(a_.wasm_v128, b_.wasm_v128);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_i32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed int), vec_cmpge(a_.altivec_i32, b_.altivec_i32));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), a_.i32 >= b_.i32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
        r_.i32[i] = (a_.i32[i] >= b_.i32[i]) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpge_epi32_mask (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_cmpge_epi32_mask(a, b);
  #else
    return simde_mm_movepi32_mask(simde_x_mm_cmpge_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi32_mask
  #define _mm512_cmpge_epi32_mask(a, b) simde_mm512_cmpge_epi32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpge_epi32_mask(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_cmpge_epi32_mask(k, a, b);
  #else
    return k & simde_mm_cmpge_epi32_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpge_epi32_mask
  #define _mm_mask_cmpge_epi32_mask(src, k, a, b) simde_mm_mask_cmpge_epi32_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_cmpge_epi32 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm256_movm_epi32(_mm256_cmpge_epi32_mask(a, b));
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epi32(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), a_.i32 >= b_.i32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
        r_.i32[i] = (a_.i32[i] >= b_.i32[i]) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_cmpge_epi32_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_cmpge_epi32_mask(a, b);
  #else
    return simde_mm256_movepi32_mask(simde_x_mm256_cmpge_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi32_mask
  #define _mm512_cmpge_epi32_mask(a, b) simde_mm512_cmpge_epi32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_cmpge_epi32_mask(simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_cmpge_epi32_mask(k, a, b);
  #else
    return k & simde_mm256_cmpge_epi32_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpge_epi32_mask
  #define _mm256_mask_cmpge_epi32_mask(src, k, a, b) simde_mm256_mask_cmpge_epi32_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_cmpge_epi32 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return simde_mm512_movm_epi32(_mm512_cmpge_epi32_mask(a, b));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epi32(a_.m128i[i], b_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_x_mm256_cmpge_epi32(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), a_.i32 >= b_.i32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
        r_.i32[i] = (a_.i32[i] >= b_.i32[i]) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_cmpge_epi32_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cmpge_epi32_mask(a, b);
  #else
    return simde_mm512_movepi32_mask(simde_x_mm512_cmpge_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi32_mask
  #define _mm512_cmpge_epi32_mask(a, b) simde_mm512_cmpge_epi32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_mask_cmpge_epi32_mask(simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cmpge_epi32_mask(k, a, b);
  #else
    return k & simde_mm512_cmpge_epi32_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpge_epi32_mask
  #define _mm512_mask_cmpge_epi32_mask(src, k, a, b) simde_mm512_mask_cmpge_epi32_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_cmpge_epu32 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm_movm_epi32(_mm_cmpge_epu32_mask(a, b));
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_u32 = vcgeq_u32(a_.neon_u32, b_.neon_u32);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_u32x4_ge(a_.wasm_v128, b_.wasm_v128);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_u32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), vec_cmpge(a_.altivec_u32, b_.altivec_u32));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u32), a_.u32 >= b_.u32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u32) / sizeof(a_.u32[0])) ; i++) {
        r_.u32[i] = (a_.u32[i] >= b_.u32[i]) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpge_epu32_mask (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_cmpge_epu32_mask(a, b);
  #else
    return simde_mm_movepi32_mask(simde_x_mm_cmpge_epu32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu32_mask
  #define _mm512_cmpge_epu32_mask(a, b) simde_mm512_cmpge_epu32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpge_epu32_mask(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_cmpge_epu32_mask(k, a, b);
  #else
    return k & simde_mm_cmpge_epu32_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpge_epu32_mask
  #define _mm_mask_cmpge_epu32_mask(src, k, a, b) simde_mm_mask_cmpge_epu32_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_cmpge_epu32 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm256_movm_epi32(_mm256_cmpge_epu32_mask(a, b));
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epu32(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u32), a_.u32 >= b_.u32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u32) / sizeof(a_.u32[0])) ; i++) {
        r_.u32[i] = (a_.u32[i] >= b_.u32[i]) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_cmpge_epu32_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_cmpge_epu32_mask(a, b);
  #else
    return simde_mm256_movepi32_mask(simde_x_mm256_cmpge_epu32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu32_mask
  #define _mm512_cmpge_epu32_mask(a, b) simde_mm512_cmpge_epu32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_cmpge_epu32_mask(simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_cmpge_epu32_mask(k, a, b);
  #else
    return k & simde_mm256_cmpge_epu32_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpge_epu32_mask
  #define _mm256_mask_cmpge_epu32_mask(src, k, a, b) simde_mm256_mask_cmpge_epu32_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_cmpge_epu32 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return simde_mm512_movm_epi32(_mm512_cmpge_epu32_mask(a, b));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epu32(a_.m128i[i], b_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_x_mm256_cmpge_epu32(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u32), a_.u32 >= b_.u32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u32) / sizeof(a_.u32[0])) ; i++) {
        r_.u32[i] = (a_.u32[i] >= b_.u32[i]) ? ~INT32_C(0) : INT32_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_cmpge_epu32_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cmpge_epu32_mask(a, b);
  #else
    return simde_mm512_movepi32_mask(simde_x_mm512_cmpge_epu32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu32_mask
  #define _mm512_cmpge_epu32_mask(a, b) simde_mm512_cmpge_epu32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_mask_cmpge_epu32_mask(simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cmpge_epu32_mask(k, a, b);
  #else
    return k & simde_mm512_cmpge_epu32_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpge_epu32_mask
  #define _mm512_mask_cmpge_epu32_mask(src, k, a, b) simde_mm512_mask_cmpge_epu32_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_cmpge_epi64 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm_movm_epi64(_mm_cmpge_epi64_mask(a, b));
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r_.neon_u64 = vcgeq_s64(a_.neon_i64, b_.neon_i64);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_i64x2_ge(a_.wasm_v128, b_.wasm_v128);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_i64 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed long long), vec_cmpge(a_.altivec_i64, b_.altivec_i64));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), a_.i64 >= b_.i64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
        r_.i64[i] = (a_.i64[i] >= b_.i64[i]) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpge_epi64_mask (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_cmpge_epi64_mask(a, b);
  #else
    return simde_mm_movepi64_mask(simde_x_mm_cmpge_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpge_epi64_mask
  #define _mm_cmpge_epi64_mask(a, b) simde_mm_cmpge_epi64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpge_epi64_mask(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_cmpge_epi64_mask(k, a, b);
  #else
    return k & simde_mm_cmpge_epi64_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpge_epi64_mask
  #define _mm_mask_cmpge_epi64_mask(src, k, a, b) simde_mm_mask_cmpge_epi64_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_cmpge_epi64 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm256_movm_epi64(_mm256_cmpge_epi64_mask(a, b));
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epi64(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), a_.i64 >= b_.i64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
        r_.i64[i] = (a_.i64[i] >= b_.i64[i]) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_cmpge_epi64_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_cmpge_epi64_mask(a, b);
  #else
    return simde_mm256_movepi64_mask(simde_x_mm256_cmpge_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpge_epi64_mask
  #define _mm256_cmpge_epi64_mask(a, b) simde_mm256_cmpge_epi64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_cmpge_epi64_mask(simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_cmpge_epi64_mask(k, a, b);
  #else
    return k & simde_mm256_cmpge_epi64_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpge_epi64_mask
  #define _mm256_mask_cmpge_epi64_mask(src, k, a, b) simde_mm256_mask_cmpge_epi64_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_cmpge_epi64 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return simde_mm512_movm_epi64(_mm512_cmpge_epi64_mask(a, b));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epi64(a_.m128i[i], b_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_x_mm256_cmpge_epi64(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), a_.i64 >= b_.i64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
        r_.i64[i] = (a_.i64[i] >= b_.i64[i]) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_cmpge_epi64_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cmpge_epi64_mask(a, b);
  #else
    return simde_mm512_movepi64_mask(simde_x_mm512_cmpge_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epi64_mask
  #define _mm512_cmpge_epi64_mask(a, b) simde_mm512_cmpge_epi64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_mask_cmpge_epi64_mask(simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cmpge_epi64_mask(k, a, b);
  #else
    return k & simde_mm512_cmpge_epi64_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpge_epi64_mask
  #define _mm512_mask_cmpge_epi64_mask(src, k, a, b) simde_mm512_mask_cmpge_epi64_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_x_mm_cmpge_epu64 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm_movm_epi64(_mm_cmpge_epu64_mask(a, b));
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r_.neon_u64 = vcgeq_u64(a_.neon_u64, b_.neon_u64);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_u64 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), vec_cmpge(a_.altivec_u64, b_.altivec_u64));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u64), a_.u64 >= b_.u64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u64) / sizeof(a_.u64[0])) ; i++) {
        r_.u64[i] = (a_.u64[i] >= b_.u64[i]) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpge_epu64_mask (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_cmpge_epu64_mask(a, b);
  #else
    return simde_mm_movepi64_mask(simde_x_mm_cmpge_epu64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu64_mask
  #define _mm512_cmpge_epu64_mask(a, b) simde_mm512_cmpge_epu64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpge_epu64_mask(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_cmpge_epu64_mask(k, a, b);
  #else
    return k & simde_mm_cmpge_epu64_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpge_epu64_mask
  #define _mm_mask_cmpge_epu64_mask(src, k, a, b) simde_mm_mask_cmpge_epu64_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_x_mm256_cmpge_epu64 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm256_movm_epi64(_mm256_cmpge_epu64_mask(a, b));
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epu64(a_.m128i[i], b_.m128i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u64), a_.u64 >= b_.u64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u64) / sizeof(a_.u64[0])) ; i++) {
        r_.u64[i] = (a_.u64[i] >= b_.u64[i]) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_cmpge_epu64_mask (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_cmpge_epu64_mask(a, b);
  #else
    return simde_mm256_movepi64_mask(simde_x_mm256_cmpge_epu64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu64_mask
  #define _mm512_cmpge_epu64_mask(a, b) simde_mm512_cmpge_epu64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_cmpge_epu64_mask(simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_cmpge_epu64_mask(k, a, b);
  #else
    return k & simde_mm256_cmpge_epu64_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpge_epu64_mask
  #define _mm256_mask_cmpge_epu64_mask(src, k, a, b) simde_mm256_mask_cmpge_epu64_mask((src), (k), (a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_cmpge_epu64 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return simde_mm512_movm_epi64(_mm512_cmpge_epu64_mask(a, b));
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_x_mm_cmpge_epu64(a_.m128i[i], b_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_x_mm256_cmpge_epu64(a_.m256i[i], b_.m256i[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.u64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u64), a_.u64 >= b_.u64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u64) / sizeof(a_.u64[0])) ; i++) {
        r_.u64[i] = (a_.u64[i] >= b_.u64[i]) ? ~INT64_C(0) : INT64_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_cmpge_epu64_mask (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cmpge_epu64_mask(a, b);
  #else
    return simde_mm512_movepi64_mask(simde_x_mm512_cmpge_epu64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmpge_epu64_mask
  #define _mm512_cmpge_epu64_mask(a, b) simde_mm512_cmpge_epu64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_mask_cmpge_epu64_mask(simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cmpge_epu64_mask(k, a, b);
  #else
    return k & simde_mm512_cmpge_epu64_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmpge_epu64_mask
  #define _mm512_mask_cmpge_epu64_mask(src, k, a, b) simde_mm512_mask_cmpge_epu64_mask((src), (k), (a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_CMPGE_H) */
