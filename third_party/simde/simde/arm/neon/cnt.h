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
 */

#if !defined(SIMDE_ARM_NEON_CNT_H)
#define SIMDE_ARM_NEON_CNT_H

#include "types.h"
#include "reinterpret.h"
#include <limits.h>

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
uint8_t
simde_x_arm_neon_cntb(uint8_t v) {
  v = v - ((v >> 1) & (85));
  v = (v & (51)) + ((v >> (2)) & (51));
  v = (v + (v >> (4))) & (15);
  return HEDLEY_STATIC_CAST(uint8_t, v) >> (sizeof(uint8_t) - 1) * CHAR_BIT;
}

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vcnt_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcnt_s8(a);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = HEDLEY_STATIC_CAST(int8_t, simde_x_arm_neon_cntb(HEDLEY_STATIC_CAST(uint8_t, a_.values[i])));
    }

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcnt_s8
  #define vcnt_s8(a) simde_vcnt_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vcnt_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcnt_u8(a);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_x_arm_neon_cntb(a_.values[i]);
    }

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcnt_u8
  #define vcnt_u8(a) simde_vcnt_u8((a))
#endif

/* The x86 implementations are stolen from
 * https://github.com/WebAssembly/simd/pull/379. They could be cleaned
 * up a bit if someone is bored; they're mostly just direct
 * translations from the assembly. */

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vcntq_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcntq_s8(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), vec_popcnt(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), a)));
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a);

    #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BITALG_NATIVE)
      r_.m128i = _mm_popcnt_epi8(a_.m128i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      __m128i tmp0 = _mm_set1_epi8(0x0f);
      __m128i tmp1 = _mm_andnot_si128(tmp0, a_.m128i);
      __m128i y = _mm_and_si128(tmp0, a_.m128i);
      tmp0 = _mm_set_epi8(4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0);
      tmp1 = _mm_srli_epi16(tmp1, 4);
      y = _mm_shuffle_epi8(tmp0, y);
      tmp1 = _mm_shuffle_epi8(tmp0, tmp1);
      r_.m128i = _mm_add_epi8(y, tmp1);
    #elif defined(SIMDE_X86_SSSE3_NATIVE)
      __m128i tmp0 = _mm_set1_epi8(0x0f);
      __m128i tmp1 = a_.m128i;
      tmp1 = _mm_and_si128(tmp1, tmp0);
      tmp0 = _mm_andnot_si128(tmp0, a_.m128i);
      __m128i y = _mm_set_epi8(4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0);
      tmp0 = _mm_srli_epi16(tmp0, 4);
      y = _mm_shuffle_epi8(y, tmp1);
      tmp1 = _mm_set_epi8(4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0);
      tmp1 = _mm_shuffle_epi8(tmp1, tmp0);
      r_.m128i = _mm_add_epi8(y, tmp1);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      __m128i tmp = _mm_and_si128(_mm_srli_epi16(a_.m128i, 1), _mm_set1_epi8(0x55));
      a_.m128i = _mm_sub_epi8(a_.m128i, tmp);
      tmp = a_.m128i;
      a_.m128i = _mm_and_si128(a_.m128i, _mm_set1_epi8(0x33));
      tmp = _mm_and_si128(_mm_srli_epi16(tmp, 2), _mm_set1_epi8(0x33));
      a_.m128i = _mm_add_epi8(a_.m128i, tmp);
      tmp = _mm_srli_epi16(a_.m128i, 4);
      a_.m128i = _mm_add_epi8(a_.m128i, tmp);
      r_.m128i = _mm_and_si128(a_.m128i, _mm_set1_epi8(0x0f));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int8_t, simde_x_arm_neon_cntb(HEDLEY_STATIC_CAST(uint8_t, a_.values[i])));
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcntq_s8
  #define vcntq_s8(a) simde_vcntq_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vcntq_u8(simde_uint8x16_t a) {
  return simde_vreinterpretq_u8_s8(simde_vcntq_s8(simde_vreinterpretq_s8_u8(a)));
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcntq_u8
  #define vcntq_u8(a) simde_vcntq_u8((a))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_CNT_H) */
