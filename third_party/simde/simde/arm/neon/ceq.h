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

#if !defined(SIMDE_ARM_NEON_CEQ_H)
#define SIMDE_ARM_NEON_CEQ_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vceqh_f16(simde_float16_t a, simde_float16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vceqh_f16(a, b);
  #else
    return (simde_float16_to_float32(a) == simde_float16_to_float32(b)) ? UINT16_MAX : UINT16_C(0);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vceqh_f16
  #define vceqh_f16(a, b) simde_vceqh_f16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vceqs_f32(simde_float32_t a, simde_float32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceqs_f32(a, b);
  #else
    return (a == b) ? ~UINT32_C(0) : UINT32_C(0);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vceqs_f32
  #define vceqs_f32(a, b) simde_vceqs_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vceqd_f64(simde_float64_t a, simde_float64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceqd_f64(a, b);
  #else
    return (a == b) ? ~UINT64_C(0) : UINT64_C(0);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vceqd_f64
  #define vceqd_f64(a, b) simde_vceqd_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vceqd_s64(int64_t a, int64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return HEDLEY_STATIC_CAST(uint64_t, vceqd_s64(a, b));
  #else
    return (a == b) ? ~UINT64_C(0) : UINT64_C(0);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vceqd_s64
  #define vceqd_s64(a, b) simde_vceqd_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vceqd_u64(uint64_t a, uint64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceqd_u64(a, b);
  #else
    return (a == b) ? ~UINT64_C(0) : UINT64_C(0);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vceqd_u64
  #define vceqd_u64(a, b) simde_vceqd_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vceq_f16(simde_float16x4_t a, simde_float16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V8_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vceq_f16(a, b);
  #else
    simde_uint16x4_private r_;
    simde_float16x4_private
      a_ = simde_float16x4_to_private(a),
      b_ = simde_float16x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vceqh_f16(a_.values[i], b_.values[i]);
    }
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V8_ENABLE_NATIVE_ALIASES)
  #undef vceq_f16
  #define vceq_f16(a, b) simde_vceq_f16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vceq_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceq_f32(a, b);
  #else
    simde_uint32x2_private r_;
    simde_float32x2_private
      a_ = simde_float32x2_to_private(a),
      b_ = simde_float32x2_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT32_C(0) : UINT32_C(0);
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_f32
  #define vceq_f32(a, b) simde_vceq_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vceq_f64(simde_float64x1_t a, simde_float64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceq_f64(a, b);
  #else
    simde_uint64x1_private r_;
    simde_float64x1_private
      a_ = simde_float64x1_to_private(a),
      b_ = simde_float64x1_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT64_C(0) : UINT64_C(0);
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vceq_f64
  #define vceq_f64(a, b) simde_vceq_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vceq_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceq_s8(a, b);
  #else
    simde_uint8x8_private r_;
    simde_int8x8_private
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_cmpeq_pi8(a_.m64, b_.m64);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT8_C(0) : UINT8_C(0);
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_s8
  #define vceq_s8(a, b) simde_vceq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vceq_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceq_s16(a, b);
  #else
    simde_uint16x4_private r_;
    simde_int16x4_private
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_cmpeq_pi16(a_.m64, b_.m64);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT16_C(0) : UINT16_C(0);
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_s16
  #define vceq_s16(a, b) simde_vceq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vceq_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceq_s32(a, b);
  #else
    simde_uint32x2_private r_;
    simde_int32x2_private
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_cmpeq_pi32(a_.m64, b_.m64);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT32_C(0) : UINT32_C(0);
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_s32
  #define vceq_s32(a, b) simde_vceq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vceq_s64(simde_int64x1_t a, simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceq_s64(a, b);
  #else
    simde_uint64x1_private r_;
    simde_int64x1_private
      a_ = simde_int64x1_to_private(a),
      b_ = simde_int64x1_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT64_C(0) : UINT64_C(0);
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_s64
  #define vceq_s64(a, b) simde_vceq_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vceq_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceq_u8(a, b);
  #else
    simde_uint8x8_private r_;
    simde_uint8x8_private
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT8_C(0) : UINT8_C(0);
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_u8
  #define vceq_u8(a, b) simde_vceq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vceq_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceq_u16(a, b);
  #else
    simde_uint16x4_private r_;
    simde_uint16x4_private
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT16_C(0) : UINT16_C(0);
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_u16
  #define vceq_u16(a, b) simde_vceq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vceq_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceq_u32(a, b);
  #else
    simde_uint32x2_private r_;
    simde_uint32x2_private
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT32_C(0) : UINT32_C(0);
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_u32
  #define vceq_u32(a, b) simde_vceq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vceq_u64(simde_uint64x1_t a, simde_uint64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceq_u64(a, b);
  #else
    simde_uint64x1_private r_;
    simde_uint64x1_private
      a_ = simde_uint64x1_to_private(a),
      b_ = simde_uint64x1_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT64_C(0) : UINT64_C(0);
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceq_u64
  #define vceq_u64(a, b) simde_vceq_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vceqq_f16(simde_float16x8_t a, simde_float16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V8_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vceqq_f16(a, b);
  #else
    simde_uint16x8_private r_;
    simde_float16x8_private
      a_ = simde_float16x8_to_private(a),
      b_ = simde_float16x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vceqh_f16(a_.values[i], b_.values[i]);
    }

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V8_ENABLE_NATIVE_ALIASES)
  #undef vceqq_f16
  #define vceqq_f16(a, b) simde_vceqq_f16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vceqq_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceqq_f32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), vec_cmpeq(a, b));
  #else
    simde_uint32x4_private r_;
    simde_float32x4_private
      a_ = simde_float32x4_to_private(a),
      b_ = simde_float32x4_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_castps_si128(_mm_cmpeq_ps(a_.m128, b_.m128));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f32x4_eq(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT32_C(0) : UINT32_C(0);
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_f32
  #define vceqq_f32(a, b) simde_vceqq_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vceqq_f64(simde_float64x2_t a, simde_float64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceqq_f64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), vec_cmpeq(a, b));
  #else
    simde_uint64x2_private r_;
    simde_float64x2_private
      a_ = simde_float64x2_to_private(a),
      b_ = simde_float64x2_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_castpd_si128(_mm_cmpeq_pd(a_.m128d, b_.m128d));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f64x2_eq(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT64_C(0) : UINT64_C(0);
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vceqq_f64
  #define vceqq_f64(a, b) simde_vceqq_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vceqq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceqq_s8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), vec_cmpeq(a, b));
  #else
    simde_uint8x16_private r_;
    simde_int8x16_private
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_cmpeq_epi8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_eq(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT8_C(0) : UINT8_C(0);
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_s8
  #define vceqq_s8(a, b) simde_vceqq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vceqq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceqq_s16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short), vec_cmpeq(a, b));
  #else
    simde_uint16x8_private r_;
    simde_int16x8_private
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_cmpeq_epi16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_eq(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT16_C(0) : UINT16_C(0);
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_s16
  #define vceqq_s16(a, b) simde_vceqq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vceqq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceqq_s32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), vec_cmpeq(a, b));
  #else
    simde_uint32x4_private r_;
    simde_int32x4_private
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_cmpeq_epi32(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_eq(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT32_C(0) : UINT32_C(0);
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_s32
  #define vceqq_s32(a, b) simde_vceqq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vceqq_s64(simde_int64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceqq_s64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), vec_cmpeq(a, b));
  #else
    simde_uint64x2_private r_;
    simde_int64x2_private
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128i = _mm_cmpeq_epi64(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT64_C(0) : UINT64_C(0);
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_s64
  #define vceqq_s64(a, b) simde_vceqq_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vceqq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceqq_u8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), vec_cmpeq(a, b));
  #else
    simde_uint8x16_private r_;
    simde_uint8x16_private
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_cmpeq_epi8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT8_C(0) : UINT8_C(0);
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_u8
  #define vceqq_u8(a, b) simde_vceqq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vceqq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceqq_u16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short), vec_cmpeq(a, b));
  #else
    simde_uint16x8_private r_;
    simde_uint16x8_private
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_cmpeq_epi16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT16_C(0) : UINT16_C(0);
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_u16
  #define vceqq_u16(a, b) simde_vceqq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vceqq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vceqq_u32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), vec_cmpeq(a, b));
  #else
    simde_uint32x4_private r_;
    simde_uint32x4_private
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_cmpeq_epi32(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT32_C(0) : UINT32_C(0);
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_u32
  #define vceqq_u32(a, b) simde_vceqq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vceqq_u64(simde_uint64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vceqq_u64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), vec_cmpeq(a, b));
  #else
    simde_uint64x2_private r_;
    simde_uint64x2_private
      a_ = simde_uint64x2_to_private(a),
      b_ = simde_uint64x2_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128i = _mm_cmpeq_epi64(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values == b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] == b_.values[i]) ? ~UINT64_C(0) : UINT64_C(0);
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vceqq_u64
  #define vceqq_u64(a, b) simde_vceqq_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_CEQ_H) */
