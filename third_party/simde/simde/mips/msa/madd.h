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
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TOa THE WARRANTIES OF
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

#if !defined(SIMDE_MIPS_MSA_MADD_H)
#define SIMDE_MIPS_MSA_MADD_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_v4f32
simde_msa_fmadd_w(simde_v4f32 a, simde_v4f32 b, simde_v4f32 c) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_fmadd_w(a, b, c);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(__ARM_FEATURE_FMA)
    return vfmaq_f32(a, c, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlaq_f32(a, b, c);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return vec_madd(c, b, a);
  #else
    simde_v4f32_private
      a_ = simde_v4f32_to_private(a),
      b_ = simde_v4f32_to_private(b),
      c_ = simde_v4f32_to_private(c),
      r_;

    #if defined(SIMDE_X86_FMA_NATIVE)
      r_.m128 = _mm_fmadd_ps(c_.m128, b_.m128, a_.m128);
    #elif defined(SIMDE_X86_SSE_NATIVE)
      r_.m128 = _mm_add_ps(a_.m128, _mm_mul_ps(b_.m128, c_.m128));
    #elif defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
      return wasm_f32x4_fma(a_.v128, b_.v128, c_.v128);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f32x4_add(a_.v128, wasm_f32x4_mul(b_.v128, c_.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.values = a_.values + (b_.values * c_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_math_fmaf(c_.values[i], b_.values[i], a_.values[i]);
      }
    #endif

    return simde_v4f32_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_fmadd_w
  #define __msa_fmadd_w(a, b) simde_msa_fmadd_w((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v2f64
simde_msa_fmadd_d(simde_v2f64 a, simde_v2f64 b, simde_v2f64 c) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_fmadd_d(a, b, c);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_madd(c, b, a);
  #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vfmaq_f64(a, c, b);
  #else
    simde_v2f64_private
      a_ = simde_v2f64_to_private(a),
      b_ = simde_v2f64_to_private(b),
      c_ = simde_v2f64_to_private(c),
      r_;

    #if defined(SIMDE_X86_FMA_NATIVE)
      r_.m128d = _mm_fmadd_pd(c_.m128d, b_.m128d, a_.m128d);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128d = _mm_add_pd(a_.m128d, _mm_mul_pd(b_.m128d, c_.m128d));
    #elif defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
      r_.v128 = wasm_f64x2_fma(a_.v128, b_.v128, c_.v128);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f64x2_add(a_.v128, wasm_f64x2_mul(b_.v128, c_.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values + (b_.values * c_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_math_fma(c_.values[i], b_.values[i], a_.values[i]);
      }
    #endif

    return simde_v2f64_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_fmadd_d
  #define __msa_fmadd_d(a, b) simde_msa_fmadd_d((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_MIPS_MSA_MADD_H) */
