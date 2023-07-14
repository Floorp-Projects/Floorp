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
 *   2020      Sean Maher <seanptmaher@gmail.com> (Copyright owned by Google, LLC)
 */

#if !defined(SIMDE_ARM_NEON_QRDMULH_H)
#define SIMDE_ARM_NEON_QRDMULH_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int16_t
simde_vqrdmulhh_s16(int16_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqrdmulhh_s16(a, b);
  #else
    return HEDLEY_STATIC_CAST(int16_t, (((1 << 15) + ((HEDLEY_STATIC_CAST(int32_t, (HEDLEY_STATIC_CAST(int32_t, a) * HEDLEY_STATIC_CAST(int32_t, b)))) << 1)) >> 16) & 0xffff);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhh_s16
  #define vqrdmulhh_s16(a, b) simde_vqrdmulhh_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vqrdmulhs_s32(int32_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqrdmulhs_s32(a, b);
  #else
    return HEDLEY_STATIC_CAST(int32_t, (((HEDLEY_STATIC_CAST(int64_t, 1) << 31) + ((HEDLEY_STATIC_CAST(int64_t, (HEDLEY_STATIC_CAST(int64_t, a) * HEDLEY_STATIC_CAST(int64_t, b)))) << 1)) >> 32) & 0xffffffff);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhs_s32
  #define vqrdmulhs_s32(a, b) simde_vqrdmulhs_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vqrdmulh_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqrdmulh_s16(a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);


    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqrdmulhh_s16(a_.values[i], b_.values[i]);
    }

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulh_s16
  #define vqrdmulh_s16(a, b) simde_vqrdmulh_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vqrdmulh_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqrdmulh_s32(a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqrdmulhs_s32(a_.values[i], b_.values[i]);
    }

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulh_s32
  #define vqrdmulh_s32(a, b) simde_vqrdmulh_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vqrdmulhq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqrdmulhq_s16(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    /* https://github.com/WebAssembly/simd/pull/365 */
    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_i16 = vqrdmulhq_s16(a_.neon_i16, b_.neon_i16);
    #elif defined(SIMDE_X86_SSSE3_NATIVE)
      __m128i y = _mm_mulhrs_epi16(a_.m128i, b_.m128i);
      __m128i tmp = _mm_cmpeq_epi16(y, _mm_set1_epi16(INT16_MAX));
      r_.m128i = _mm_xor_si128(y, tmp);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i prod_lo = _mm_mullo_epi16(a_.m128i, b_.m128i);
      const __m128i prod_hi = _mm_mulhi_epi16(a_.m128i, b_.m128i);
      const __m128i tmp =
        _mm_add_epi16(
          _mm_avg_epu16(
            _mm_srli_epi16(prod_lo, 14),
            _mm_setzero_si128()
          ),
          _mm_add_epi16(prod_hi, prod_hi)
        );
      r_.m128i =
        _mm_xor_si128(
          tmp,
          _mm_cmpeq_epi16(_mm_set1_epi16(INT16_MAX), tmp)
        );
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqrdmulhh_s16(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhq_s16
  #define vqrdmulhq_s16(a, b) simde_vqrdmulhq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vqrdmulhq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqrdmulhq_s32(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqrdmulhs_s32(a_.values[i], b_.values[i]);
    }

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhq_s32
  #define vqrdmulhq_s32(a, b) simde_vqrdmulhq_s32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QRDMULH_H) */
