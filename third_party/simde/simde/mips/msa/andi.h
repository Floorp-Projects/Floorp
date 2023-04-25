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

#if !defined(SIMDE_MIPS_MSA_ANDI_H)
#define SIMDE_MIPS_MSA_ANDI_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_v16u8
simde_msa_andi_b(simde_v16u8 a, const int imm0_255)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm0_255, 0, 255) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vandq_u8(a, vdupq_n_u8(HEDLEY_STATIC_CAST(uint8_t, imm0_255)));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_and(a, vec_splats(HEDLEY_STATIC_CAST(unsigned char, imm0_255)));
  #else
    simde_v16u8_private
      a_ = simde_v16u8_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_and_si128(a_.m128i, _mm_set1_epi8(HEDLEY_STATIC_CAST(int8_t, imm0_255)));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_v128_and(a_.v128, wasm_i8x16_splat(HEDLEY_STATIC_CAST(int8_t, imm0_255)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = a_.values & HEDLEY_STATIC_CAST(uint8_t, imm0_255);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] & HEDLEY_STATIC_CAST(int8_t, imm0_255);
      }
    #endif

    return simde_v16u8_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_NATIVE)
  #define simde_msa_andi_b(a, imm0_255) __msa_andi_b((a), (imm0_255))
#endif
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_andi_b
  #define __msa_andi_b(a, imm0_255) simde_msa_andi_b((a), (imm0_255))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_MIPS_MSA_ANDI_H) */
