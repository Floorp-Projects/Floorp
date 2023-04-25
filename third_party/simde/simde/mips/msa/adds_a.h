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

#if !defined(SIMDE_MIPS_MSA_ADDS_A_H)
#define SIMDE_MIPS_MSA_ADDS_A_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_v16i8
simde_msa_adds_a_b(simde_v16i8 a, simde_v16i8 b) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_adds_a_b(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_s8(vabsq_s8(a), vabsq_s8(b));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_adds(vec_abs(a), vec_abs(b));
  #else
    simde_v16i8_private
      a_ = simde_v16i8_to_private(a),
      b_ = simde_v16i8_to_private(b),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_adds_epi8(_mm_abs_epi8(a_.m128i), _mm_abs_epi8(b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_add_sat(wasm_i8x16_abs(a_.v128), wasm_i8x16_abs(b_.v128));
    #elif defined(SIMDE_VECTOR_SCALAR)
      __typeof__(a_.values) amask = HEDLEY_REINTERPRET_CAST(__typeof__(a_.values), a_.values < 0);
      __typeof__(b_.values) bmask = HEDLEY_REINTERPRET_CAST(__typeof__(b_.values), b_.values < 0);
      __typeof__(a_.values) aabs = (-a_.values & amask) | (a_.values & ~amask);
      __typeof__(b_.values) babs = (-b_.values & bmask) | (b_.values & ~bmask);
      __typeof__(r_.values) sum = aabs + babs;
      __typeof__(r_.values) max = { INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX, INT8_MAX };
      __typeof__(r_.values) smask = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), aabs > (max - babs));
      r_.values = (max & smask) | (sum & ~smask);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] =
          simde_math_adds_i8(
            ((a_.values[i] < 0) ? -a_.values[i] : a_.values[i]),
            ((b_.values[i] < 0) ? -b_.values[i] : b_.values[i])
          );
      }
    #endif

    return simde_v16i8_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_adds_a_b
  #define __msa_adds_a_b(a, b) simde_msa_adds_a_b((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v8i16
simde_msa_adds_a_h(simde_v8i16 a, simde_v8i16 b) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_adds_a_h(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_s16(vabsq_s16(a), vabsq_s16(b));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_adds(vec_abs(a), vec_abs(b));
  #else
    simde_v8i16_private
      a_ = simde_v8i16_to_private(a),
      b_ = simde_v8i16_to_private(b),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_adds_epi16(_mm_abs_epi16(a_.m128i), _mm_abs_epi16(b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_add_sat(wasm_i16x8_abs(a_.v128), wasm_i16x8_abs(b_.v128));
    #elif defined(SIMDE_VECTOR_SCALAR)
      __typeof__(a_.values) amask = HEDLEY_REINTERPRET_CAST(__typeof__(a_.values), a_.values < 0);
      __typeof__(b_.values) bmask = HEDLEY_REINTERPRET_CAST(__typeof__(b_.values), b_.values < 0);
      __typeof__(a_.values) aabs = (-a_.values & amask) | (a_.values & ~amask);
      __typeof__(b_.values) babs = (-b_.values & bmask) | (b_.values & ~bmask);
      __typeof__(r_.values) sum = aabs + babs;
      __typeof__(r_.values) max = { INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX };
      __typeof__(r_.values) smask = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), aabs > (max - babs));
      r_.values = (max & smask) | (sum & ~smask);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] =
          simde_math_adds_i16(
            ((a_.values[i] < 0) ? -a_.values[i] : a_.values[i]),
            ((b_.values[i] < 0) ? -b_.values[i] : b_.values[i])
          );
      }
    #endif

    return simde_v8i16_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_adds_a_h
  #define __msa_adds_a_h(a, b) simde_msa_adds_a_h((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v4i32
simde_msa_adds_a_w(simde_v4i32 a, simde_v4i32 b) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_adds_a_w(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_s32(vabsq_s32(a), vabsq_s32(b));
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_adds(vec_abs(a), vec_abs(b));
  #else
    simde_v4i32_private
      a_ = simde_v4i32_to_private(a),
      b_ = simde_v4i32_to_private(b),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      __m128i aabs = _mm_abs_epi32(a_.m128i);
      __m128i babs = _mm_abs_epi32(b_.m128i);
      __m128i sum = _mm_add_epi32(aabs, babs);
      __m128i max = _mm_set1_epi32(INT32_MAX);
      __m128i smask =
        _mm_cmplt_epi32(
          _mm_sub_epi32(max, babs),
          aabs
        );

      #if defined(SIMDE_X86_SSE4_1_NATIVE)
        r_.m128i = _mm_blendv_epi8(sum, max, smask);
      #else
        r_.m128i =
          _mm_or_si128(
            _mm_and_si128(smask, max),
            _mm_andnot_si128(smask, sum)
          );
      #endif
    #elif defined(SIMDE_VECTOR_SCALAR)
      __typeof__(a_.values) amask = HEDLEY_REINTERPRET_CAST(__typeof__(a_.values), a_.values < 0);
      __typeof__(b_.values) bmask = HEDLEY_REINTERPRET_CAST(__typeof__(b_.values), b_.values < 0);
      __typeof__(a_.values) aabs = (-a_.values & amask) | (a_.values & ~amask);
      __typeof__(b_.values) babs = (-b_.values & bmask) | (b_.values & ~bmask);
      __typeof__(r_.values) sum = aabs + babs;
      __typeof__(r_.values) max = { INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX };
      __typeof__(r_.values) smask = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), aabs > (max - babs));
      r_.values = (max & smask) | (sum & ~smask);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] =
          simde_math_adds_i32(
            ((a_.values[i] < 0) ? -a_.values[i] : a_.values[i]),
            ((b_.values[i] < 0) ? -b_.values[i] : b_.values[i])
          );
      }
    #endif

    return simde_v4i32_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_adds_a_w
  #define __msa_adds_a_w(a, b) simde_msa_adds_a_w((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v2i64
simde_msa_adds_a_d(simde_v2i64 a, simde_v2i64 b) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_adds_a_d(a, b);
  #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqaddq_s64(vabsq_s64(a), vabsq_s64(b));
  #else
    simde_v2i64_private
      a_ = simde_v2i64_to_private(a),
      b_ = simde_v2i64_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SCALAR)
      __typeof__(a_.values) amask = HEDLEY_REINTERPRET_CAST(__typeof__(a_.values), a_.values < 0);
      __typeof__(b_.values) bmask = HEDLEY_REINTERPRET_CAST(__typeof__(b_.values), b_.values < 0);
      __typeof__(a_.values) aabs = (-a_.values & amask) | (a_.values & ~amask);
      __typeof__(b_.values) babs = (-b_.values & bmask) | (b_.values & ~bmask);
      __typeof__(r_.values) sum = aabs + babs;
      __typeof__(r_.values) max = { INT64_MAX, INT64_MAX };
      __typeof__(r_.values) smask = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), aabs > (max - babs));
      r_.values = (max & smask) | (sum & ~smask);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] =
          simde_math_adds_i64(
            ((a_.values[i] < 0) ? -a_.values[i] : a_.values[i]),
            ((b_.values[i] < 0) ? -b_.values[i] : b_.values[i])
          );
      }
    #endif

    return simde_v2i64_from_private(r_);
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_adds_a_d
  #define __msa_adds_a_d(a, b) simde_msa_adds_a_d((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_MIPS_MSA_ADDS_A_H) */
