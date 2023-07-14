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

#if !defined(SIMDE_ARM_NEON_ORN_H)
#define SIMDE_ARM_NEON_ORN_H

#include "orr.h"
#include "mvn.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vorn_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vorn_s8(a, b);
  #else
    simde_int8x8_private
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vorn_s8
  #define vorn_s8(a, b) simde_vorn_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vorn_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vorn_s16(a, b);
  #else
    simde_int16x4_private
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vorn_s16
  #define vorn_s16(a, b) simde_vorn_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vorn_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vorn_s32(a, b);
  #else
    simde_int32x2_private
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vorn_s32
  #define vorn_s32(a, b) simde_vorn_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vorn_s64(simde_int64x1_t a, simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vorn_s64(a, b);
  #else
    simde_int64x1_private
      a_ = simde_int64x1_to_private(a),
      b_ = simde_int64x1_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vorn_s64
  #define vorn_s64(a, b) simde_vorn_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vorn_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vorn_u8(a, b);
  #else
    simde_uint8x8_private
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vorn_u8
  #define vorn_u8(a, b) simde_vorn_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vorn_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vorn_u16(a, b);
  #else
    simde_uint16x4_private
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vorn_u16
  #define vorn_u16(a, b) simde_vorn_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vorn_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vorn_u32(a, b);
  #else
    simde_uint32x2_private
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vorn_u32
  #define vorn_u32(a, b) simde_vorn_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vorn_u64(simde_uint64x1_t a, simde_uint64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vorn_u64(a, b);
  #else
    simde_uint64x1_private
      a_ = simde_uint64x1_to_private(a),
      b_ = simde_uint64x1_to_private(b),
      r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vorn_u64
  #define vorn_u64(a, b) simde_vorn_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vornq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vornq_s8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_orc(a, b);
  #else
    simde_int8x16_private
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_ternarylogic_epi32(a_.m128i, b_.m128i, a_.m128i, 0xf3);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vornq_s8
  #define vornq_s8(a, b) simde_vornq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vornq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vornq_s16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_orc(a, b);
  #else
    simde_int16x8_private
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_ternarylogic_epi32(a_.m128i, b_.m128i, a_.m128i, 0xf3);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vornq_s16
  #define vornq_s16(a, b) simde_vornq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vornq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vornq_s32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_orc(a, b);
  #else
    simde_int32x4_private
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_ternarylogic_epi32(a_.m128i, b_.m128i, a_.m128i, 0xf3);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vornq_s32
  #define vornq_s32(a, b) simde_vornq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vornq_s64(simde_int64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vornq_s64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_orc(a, b);
  #else
    simde_int64x2_private
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_ternarylogic_epi64(a_.m128i, b_.m128i, a_.m128i, 0xf3);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vornq_s64
  #define vornq_s64(a, b) simde_vornq_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vornq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vornq_u8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_orc(a, b);
  #else
    simde_uint8x16_private
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_ternarylogic_epi32(a_.m128i, b_.m128i, a_.m128i, 0xf3);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vornq_u8
  #define vornq_u8(a, b) simde_vornq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vornq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vornq_u16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_orc(a, b);
  #else
    simde_uint16x8_private
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_ternarylogic_epi32(a_.m128i, b_.m128i, a_.m128i, 0xf3);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vornq_u16
  #define vornq_u16(a, b) simde_vornq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vornq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vornq_u32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_orc(a, b);
  #else
    simde_uint32x4_private
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_ternarylogic_epi32(a_.m128i, b_.m128i, a_.m128i, 0xf3);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vornq_u32
  #define vornq_u32(a, b) simde_vornq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vornq_u64(simde_uint64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vornq_u64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return vec_orc(a, b);
  #else
    simde_uint64x2_private
      a_ = simde_uint64x2_to_private(a),
      b_ = simde_uint64x2_to_private(b),
      r_;

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_ternarylogic_epi64(a_.m128i, b_.m128i, a_.m128i, 0xf3);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = a_.values | ~(b_.values);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] | ~b_.values[i];
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vornq_u64
  #define vornq_u64(a, b) simde_vornq_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_ORN_H) */
