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
 *   2020      Christopher Moore <moore@free.fr>
 */

/* N.B. CM: vrev64_f16 and vrev64q_f16 are omitted as
 * SIMDe has no 16-bit floating point support. */

#if !defined(SIMDE_ARM_NEON_REV64_H)
#define SIMDE_ARM_NEON_REV64_H

#include "reinterpret.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vrev64_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64_s8(a);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_shuffle_pi8(a_.m64, _mm_set_pi8(0, 1, 2, 3, 4, 5, 6, 7));
    #elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = SIMDE_SHUFFLE_VECTOR_(8, 8, a_.values, a_.values, 7, 6, 5, 4, 3, 2, 1, 0);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i ^ 7];
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64_s8
  #define vrev64_s8(a) simde_vrev64_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vrev64_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64_s16(a);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a);

    #if defined(SIMDE_X86_SSE_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_shuffle_pi16(a_.m64, (0 << 6) | (1 << 4) | (2 << 2) | (3 << 0));
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(16, 8, a_.values, a_.values, 3, 2, 1, 0);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i ^ 3];
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64_s16
  #define vrev64_s16(a) simde_vrev64_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vrev64_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64_s32(a);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a);

    #if defined(SIMDE_X86_SSE_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 =  _mm_shuffle_pi16(a_.m64, (1 << 6) | (0 << 4) | (3 << 2) | (2 << 0));
    #elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = SIMDE_SHUFFLE_VECTOR_(32, 8, a_.values, a_.values, 1, 0);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i ^ 1];
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64_s32
  #define vrev64_s32(a) simde_vrev64_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vrev64_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64_u8(a);
  #else
    return simde_vreinterpret_u8_s8(simde_vrev64_s8(simde_vreinterpret_s8_u8(a)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64_u8
  #define vrev64_u8(a) simde_vrev64_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vrev64_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64_u16(a);
  #else
    return simde_vreinterpret_u16_s16(simde_vrev64_s16(simde_vreinterpret_s16_u16(a)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64_u16
  #define vrev64_u16(a) simde_vrev64_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vrev64_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64_u32(a);
  #else
    return simde_vreinterpret_u32_s32(simde_vrev64_s32(simde_vreinterpret_s32_u32(a)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64_u32
  #define vrev64_u32(a) simde_vrev64_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vrev64_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64_f32(a);
  #else
    return simde_vreinterpret_f32_s32(simde_vrev64_s32(simde_vreinterpret_s32_f32(a)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64_f32
  #define vrev64_f32(a) simde_vrev64_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vrev64q_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64q_s8(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char),
                                   vec_revb(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed long long), a)));
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char),
                                   vec_reve(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed long long), vec_reve(a))));
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_shuffle_epi8(a_.m128i, _mm_set_epi8(8, 9, 10, 11, 12, 13, 14, 15,
                                                         0, 1,  2,  3,  4,  5,  6,  7));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_shuffle(a_.v128, a_.v128, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(8, 16, a_.values, a_.values, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i ^ 7];
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64q_s8
  #define vrev64q_s8(a) simde_vrev64q_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vrev64q_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64q_s16(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed short),
                                   vec_reve(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed long long), vec_reve(a))));
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_shuffle_epi8(a_.m128i, _mm_set_epi8(9, 8, 11, 10, 13, 12, 15, 14,
                                                         1, 0,  3,  2,  5,  4,  7,  6));
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_shufflehi_epi16(_mm_shufflelo_epi16(a_.m128i,
                                                        (0 << 6) | (1 << 4) | (2 << 2) | (3 << 0)),
                                                        (0 << 6) | (1 << 4) | (2 << 2) | (3 << 0));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_shuffle(a_.v128, a_.v128, 6, 7, 4, 5, 2, 3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(16, 16, a_.values, a_.values, 3, 2, 1, 0, 7, 6, 5, 4);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i ^ 3];
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64q_s16
  #define vrev64q_s16(a) simde_vrev64q_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vrev64q_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64q_s32(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed int),
                                   vec_reve(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed long long), vec_reve(a))));
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_shuffle_epi32(a_.m128i, (2 << 6) | (3 << 4) | (0 << 2) | (1 << 0));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_shuffle(a_.v128, a_.v128, 4, 5, 6, 7, 0, 1, 2, 3, 12, 13, 14, 15, 8, 9, 10, 11);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(32, 16, a_.values, a_.values, 1, 0, 3, 2);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i ^ 1];
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64q_s32
  #define vrev64q_s32(a) simde_vrev64q_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vrev64q_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64q_u8(a);
  #else
    return simde_vreinterpretq_u8_s8(simde_vrev64q_s8(simde_vreinterpretq_s8_u8(a)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64q_u8
  #define vrev64q_u8(a) simde_vrev64q_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vrev64q_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64q_u16(a);
  #else
    return simde_vreinterpretq_u16_s16(simde_vrev64q_s16(simde_vreinterpretq_s16_u16(a)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64q_u16
  #define vrev64q_u16(a) simde_vrev64q_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vrev64q_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64q_u32(a);
  #else
    return simde_vreinterpretq_u32_s32(simde_vrev64q_s32(simde_vreinterpretq_s32_u32(a)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64q_u32
  #define vrev64q_u32(a) simde_vrev64q_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vrev64q_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrev64q_f32(a);
  #else
    return simde_vreinterpretq_f32_s32(simde_vrev64q_s32(simde_vreinterpretq_s32_f32(a)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrev64q_f32
  #define vrev64q_f32(a) simde_vrev64q_f32(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_REV64_H) */
