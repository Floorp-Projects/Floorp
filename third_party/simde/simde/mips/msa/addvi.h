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

#if !defined(SIMDE_MIPS_MSA_ADDVI_H)
#define SIMDE_MIPS_MSA_ADDVI_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_v16i8
simde_msa_addvi_b(simde_v16i8 a, const int imm0_31)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm0_31, 0, 31) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaddq_s8(a, vdupq_n_s8(HEDLEY_STATIC_CAST(int8_t, imm0_31)));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_add(a, vec_splats(HEDLEY_STATIC_CAST(signed char, imm0_31)));
  #else
    simde_v16i8_private
      a_ = simde_v16i8_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_add_epi8(a_.m128i, _mm_set1_epi8(HEDLEY_STATIC_CAST(int8_t, imm0_31)));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_add(a_.v128, wasm_i8x16_splat(HEDLEY_STATIC_CAST(int8_t, imm0_31)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = a_.values + HEDLEY_STATIC_CAST(int8_t, imm0_31);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] + HEDLEY_STATIC_CAST(int8_t, imm0_31);
      }
    #endif

    return simde_v16i8_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_NATIVE)
  #define simde_msa_addvi_b(a, imm0_31) __msa_addvi_b((a), (imm0_31))
#endif
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_addvi_b
  #define __msa_addvi_b(a, imm0_31) simde_msa_addvi_b((a), (imm0_31))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v8i16
simde_msa_addvi_h(simde_v8i16 a, const int imm0_31)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm0_31, 0, 31) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaddq_s16(a, vdupq_n_s16(HEDLEY_STATIC_CAST(int16_t, imm0_31)));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_add(a, vec_splats(HEDLEY_STATIC_CAST(signed short, imm0_31)));
  #else
    simde_v8i16_private
      a_ = simde_v8i16_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_add_epi16(a_.m128i, _mm_set1_epi16(HEDLEY_STATIC_CAST(int16_t, imm0_31)));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_add(a_.v128, wasm_i16x8_splat(HEDLEY_STATIC_CAST(int16_t, imm0_31)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = a_.values + HEDLEY_STATIC_CAST(int16_t, imm0_31);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] + HEDLEY_STATIC_CAST(int16_t, imm0_31);
      }
    #endif

    return simde_v8i16_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_NATIVE)
  #define simde_msa_addvi_h(a, imm0_31) __msa_addvi_h((a), (imm0_31))
#endif
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_addvi_h
  #define __msa_addvi_h(a, imm0_31) simde_msa_addvi_h((a), (imm0_31))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v4i32
simde_msa_addvi_w(simde_v4i32 a, const int imm0_31)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm0_31, 0, 31) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaddq_s32(a, vdupq_n_s32(HEDLEY_STATIC_CAST(int32_t, imm0_31)));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_add(a, vec_splats(HEDLEY_STATIC_CAST(signed int, imm0_31)));
  #else
    simde_v4i32_private
      a_ = simde_v4i32_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_add_epi32(a_.m128i, _mm_set1_epi32(HEDLEY_STATIC_CAST(int32_t, imm0_31)));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_add(a_.v128, wasm_i32x4_splat(HEDLEY_STATIC_CAST(int32_t, imm0_31)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = a_.values + HEDLEY_STATIC_CAST(int32_t, imm0_31);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] + HEDLEY_STATIC_CAST(int32_t, imm0_31);
      }
    #endif

    return simde_v4i32_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_NATIVE)
  #define simde_msa_addvi_w(a, imm0_31) __msa_addvi_w((a), (imm0_31))
#endif
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_addvi_w
  #define __msa_addvi_w(a, imm0_31) simde_msa_addvi_w((a), (imm0_31))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v2i64
simde_msa_addvi_d(simde_v2i64 a, const int imm0_31)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm0_31, 0, 31) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaddq_s64(a, vdupq_n_s64(HEDLEY_STATIC_CAST(int64_t, imm0_31)));
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_add(a, vec_splats(HEDLEY_STATIC_CAST(signed long long, imm0_31)));
  #else
    simde_v2i64_private
      a_ = simde_v2i64_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_add_epi64(a_.m128i, _mm_set1_epi64x(HEDLEY_STATIC_CAST(int64_t, imm0_31)));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_add(a_.v128, wasm_i64x2_splat(HEDLEY_STATIC_CAST(int64_t, imm0_31)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = a_.values + HEDLEY_STATIC_CAST(int64_t, HEDLEY_STATIC_CAST(int64_t, imm0_31));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] + imm0_31;
      }
    #endif

    return simde_v2i64_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_NATIVE)
  #define simde_msa_addvi_d(a, imm0_31) __msa_addvi_d((a), (imm0_31))
#endif
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_addvi_d
  #define __msa_addvi_d(a, imm0_31) simde_msa_addvi_d((a), (imm0_31))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_MIPS_MSA_ADDVI_H) */
