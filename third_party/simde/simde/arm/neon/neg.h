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

#if !defined(SIMDE_ARM_NEON_NEG_H)
#define SIMDE_ARM_NEON_NEG_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vnegd_s64(int64_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(9,0,0))
    return vnegd_s64(a);
  #else
    return -a;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vnegd_s64
  #define vnegd_s64(a) simde_vnegd_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vneg_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vneg_f32(a);
  #else
    simde_float32x2_private
      r_,
      a_ = simde_float32x2_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vneg_f32
  #define vneg_f32(a) simde_vneg_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vneg_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vneg_f64(a);
  #else
    simde_float64x1_private
      r_,
      a_ = simde_float64x1_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vneg_f64
  #define vneg_f64(a) simde_vneg_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vneg_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vneg_s8(a);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vneg_s8
  #define vneg_s8(a) simde_vneg_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vneg_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vneg_s16(a);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vneg_s16
  #define vneg_s16(a) simde_vneg_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vneg_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vneg_s32(a);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vneg_s32
  #define vneg_s32(a) simde_vneg_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vneg_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vneg_s64(a);
  #else
    simde_int64x1_private
      r_,
      a_ = simde_int64x1_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vnegd_s64(a_.values[i]);
      }
    #endif

    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vneg_s64
  #define vneg_s64(a) simde_vneg_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vnegq_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vnegq_f32(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,1,0))
    return vec_neg(a);
  #else
    simde_float32x4_private
      r_,
      a_ = simde_float32x4_to_private(a);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f32x4_neg(a_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128 = _mm_castsi128_ps(_mm_xor_si128(_mm_set1_epi32(HEDLEY_STATIC_CAST(int32_t, UINT32_C(1) << 31)), _mm_castps_si128(a_.m128)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vnegq_f32
  #define vnegq_f32(a) simde_vnegq_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vnegq_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vnegq_f64(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,1,0))
    return vec_neg(a);
  #else
    simde_float64x2_private
      r_,
      a_ = simde_float64x2_to_private(a);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f64x2_neg(a_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128d = _mm_castsi128_pd(_mm_xor_si128(_mm_set1_epi64x(HEDLEY_STATIC_CAST(int64_t, UINT64_C(1) << 63)), _mm_castpd_si128(a_.m128d)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vnegq_f64
  #define vnegq_f64(a) simde_vnegq_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vnegq_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vnegq_s8(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,1,0))
    return vec_neg(a);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_neg(a_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_sub_epi8(_mm_setzero_si128(), a_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vnegq_s8
  #define vnegq_s8(a) simde_vnegq_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vnegq_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vnegq_s16(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,1,0))
    return vec_neg(a);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_neg(a_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_sub_epi16(_mm_setzero_si128(), a_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vnegq_s16
  #define vnegq_s16(a) simde_vnegq_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vnegq_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vnegq_s32(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,1,0))
    return vec_neg(a);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_neg(a_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_sub_epi32(_mm_setzero_si128(), a_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = -(a_.values[i]);
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vnegq_s32
  #define vnegq_s32(a) simde_vnegq_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vnegq_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vnegq_s64(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,1,0))
    return vec_neg(a);
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_neg(a_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_sub_epi64(_mm_setzero_si128(), a_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = -a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vnegd_s64(a_.values[i]);
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vnegq_s64
  #define vnegq_s64(a) simde_vnegq_s64(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_NEG_H) */
