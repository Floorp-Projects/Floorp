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
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_MIPS_MSA_ADD_A_H)
#define SIMDE_MIPS_MSA_ADD_A_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_v16i8
simde_msa_add_a_b(simde_v16i8 a, simde_v16i8 b) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_add_a_b(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaddq_s8(vabsq_s8(a), vabsq_s8(b));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_add(vec_abs(a), vec_abs(b));
  #else
    simde_v16i8_private
      a_ = simde_v16i8_to_private(a),
      b_ = simde_v16i8_to_private(b),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_add_epi8(_mm_abs_epi8(a_.m128i), _mm_abs_epi8(b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_add(wasm_i8x16_abs(a_.v128), wasm_i8x16_abs(b_.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      const __typeof__(a_.values) amask = HEDLEY_REINTERPRET_CAST(__typeof__(a_.values), a_.values < 0);
      const __typeof__(b_.values) bmask = HEDLEY_REINTERPRET_CAST(__typeof__(b_.values), b_.values < 0);
      r_.values =
        ((-a_.values & amask) | (a_.values & ~amask)) +
        ((-b_.values & bmask) | (b_.values & ~bmask));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] =
          ((a_.values[i] < 0) ? -a_.values[i] : a_.values[i]) +
          ((b_.values[i] < 0) ? -b_.values[i] : b_.values[i]);
      }
    #endif

    return simde_v16i8_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_add_a_b
  #define __msa_add_a_b(a, b) simde_msa_add_a_b((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v8i16
simde_msa_add_a_h(simde_v8i16 a, simde_v8i16 b) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_add_a_h(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaddq_s16(vabsq_s16(a), vabsq_s16(b));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_add(vec_abs(a), vec_abs(b));
  #else
    simde_v8i16_private
      a_ = simde_v8i16_to_private(a),
      b_ = simde_v8i16_to_private(b),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_add_epi16(_mm_abs_epi16(a_.m128i), _mm_abs_epi16(b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_add(wasm_i16x8_abs(a_.v128), wasm_i16x8_abs(b_.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      const __typeof__(a_.values) amask = HEDLEY_REINTERPRET_CAST(__typeof__(a_.values), a_.values < 0);
      const __typeof__(b_.values) bmask = HEDLEY_REINTERPRET_CAST(__typeof__(b_.values), b_.values < 0);
      r_.values =
        ((-a_.values & amask) | (a_.values & ~amask)) +
        ((-b_.values & bmask) | (b_.values & ~bmask));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] =
          ((a_.values[i] < 0) ? -a_.values[i] : a_.values[i]) +
          ((b_.values[i] < 0) ? -b_.values[i] : b_.values[i]);
      }
    #endif

    return simde_v8i16_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_add_a_h
  #define __msa_add_a_h(a, b) simde_msa_add_a_h((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v4i32
simde_msa_add_a_w(simde_v4i32 a, simde_v4i32 b) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_add_a_w(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaddq_s32(vabsq_s32(a), vabsq_s32(b));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_add(vec_abs(a), vec_abs(b));
  #else
    simde_v4i32_private
      a_ = simde_v4i32_to_private(a),
      b_ = simde_v4i32_to_private(b),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_add_epi32(_mm_abs_epi32(a_.m128i), _mm_abs_epi32(b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_add(wasm_i32x4_abs(a_.v128), wasm_i32x4_abs(b_.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      const __typeof__(a_.values) amask = HEDLEY_REINTERPRET_CAST(__typeof__(a_.values), a_.values < 0);
      const __typeof__(b_.values) bmask = HEDLEY_REINTERPRET_CAST(__typeof__(b_.values), b_.values < 0);
      r_.values =
        ((-a_.values & amask) | (a_.values & ~amask)) +
        ((-b_.values & bmask) | (b_.values & ~bmask));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] =
          ((a_.values[i] < 0) ? -a_.values[i] : a_.values[i]) +
          ((b_.values[i] < 0) ? -b_.values[i] : b_.values[i]);
      }
    #endif

    return simde_v4i32_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_add_a_w
  #define __msa_add_a_w(a, b) simde_msa_add_a_w((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v2i64
simde_msa_add_a_d(simde_v2i64 a, simde_v2i64 b) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_add_a_d(a, b);
  #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddq_s64(vabsq_s64(a), vabsq_s64(b));
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_add(vec_abs(a), vec_abs(b));
  #else
    simde_v2i64_private
      a_ = simde_v2i64_to_private(a),
      b_ = simde_v2i64_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_add_epi64(_mm_abs_epi64(a_.m128i), _mm_abs_epi64(b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_add(wasm_i64x2_abs(a_.v128), wasm_i64x2_abs(b_.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      const __typeof__(a_.values) amask = HEDLEY_REINTERPRET_CAST(__typeof__(a_.values), a_.values < 0);
      const __typeof__(b_.values) bmask = HEDLEY_REINTERPRET_CAST(__typeof__(b_.values), b_.values < 0);
      r_.values =
        ((-a_.values & amask) | (a_.values & ~amask)) +
        ((-b_.values & bmask) | (b_.values & ~bmask));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] =
          ((a_.values[i] < 0) ? -a_.values[i] : a_.values[i]) +
          ((b_.values[i] < 0) ? -b_.values[i] : b_.values[i]);
      }
    #endif

    return simde_v2i64_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_add_a_d
  #define __msa_add_a_d(a, b) simde_msa_add_a_d((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_MIPS_MSA_ADD_A_H) */
