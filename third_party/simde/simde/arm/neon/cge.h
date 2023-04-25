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

#if !defined(SIMDE_ARM_NEON_CGE_H)
#define SIMDE_ARM_NEON_CGE_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vcgeh_f16(simde_float16_t a, simde_float16_t b){
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return HEDLEY_STATIC_CAST(uint16_t, vcgeh_f16(a, b));
  #else
    return (simde_float16_to_float32(a) >= simde_float16_to_float32(b)) ? UINT16_MAX : 0;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcgeh_f16
  #define vcgeh_f16(a, b) simde_vcgeh_f16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vcgeq_f16(simde_float16x8_t a, simde_float16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V8_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vcgeq_f16(a, b);
  #else
    simde_float16x8_private
      a_ = simde_float16x8_to_private(a),
      b_ = simde_float16x8_to_private(b);
    simde_uint16x8_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vcgeh_f16(a_.values[i], b_.values[i]);
    }

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V8_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_f16
  #define vcgeq_f16(a, b) simde_vcgeq_f16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vcgeq_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcgeq_f32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), vec_cmpge(a, b));
  #else
    simde_float32x4_private
      a_ = simde_float32x4_to_private(a),
      b_ = simde_float32x4_to_private(b);
    simde_uint32x4_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_castps_si128(_mm_cmpge_ps(a_.m128, b_.m128));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f32x4_ge(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT32_MAX : 0;
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_f32
  #define vcgeq_f32(a, b) simde_vcgeq_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vcgeq_f64(simde_float64x2_t a, simde_float64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcgeq_f64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), vec_cmpge(a, b));
  #else
    simde_float64x2_private
      a_ = simde_float64x2_to_private(a),
      b_ = simde_float64x2_to_private(b);
    simde_uint64x2_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_castpd_si128(_mm_cmpge_pd(a_.m128d, b_.m128d));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f64x2_ge(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT64_MAX : 0;
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_f64
  #define vcgeq_f64(a, b) simde_vcgeq_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vcgeq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcgeq_s8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), vec_cmpge(a, b));
  #else
    simde_int8x16_private
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);
    simde_uint8x16_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_or_si128(_mm_cmpgt_epi8(a_.m128i, b_.m128i), _mm_cmpeq_epi8(a_.m128i, b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_ge(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT8_MAX : 0;
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_s8
  #define vcgeq_s8(a, b) simde_vcgeq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vcgeq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcgeq_s16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short), vec_cmpge(a, b));
  #else
    simde_int16x8_private
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);
    simde_uint16x8_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_or_si128(_mm_cmpgt_epi16(a_.m128i, b_.m128i), _mm_cmpeq_epi16(a_.m128i, b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_ge(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT16_MAX : 0;
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_s16
  #define vcgeq_s16(a, b) simde_vcgeq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vcgeq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcgeq_s32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), vec_cmpge(a, b));
  #else
    simde_int32x4_private
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);
    simde_uint32x4_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_or_si128(_mm_cmpgt_epi32(a_.m128i, b_.m128i), _mm_cmpeq_epi32(a_.m128i, b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_ge(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT32_MAX : 0;
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_s32
  #define vcgeq_s32(a, b) simde_vcgeq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vcgeq_s64(simde_int64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcgeq_s64(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_s32(vmvnq_s32(vreinterpretq_s32_s64(vshrq_n_s64(vqsubq_s64(a, b), 63))));
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), vec_cmpge(a, b));
  #else
    simde_int64x2_private
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b);
    simde_uint64x2_private r_;

    #if defined(SIMDE_X86_SSE4_2_NATIVE)
      r_.m128i = _mm_or_si128(_mm_cmpgt_epi64(a_.m128i, b_.m128i), _mm_cmpeq_epi64(a_.m128i, b_.m128i));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT64_MAX : 0;
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_s64
  #define vcgeq_s64(a, b) simde_vcgeq_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vcgeq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcgeq_u8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), vec_cmpge(a, b));
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i =
        _mm_cmpeq_epi8(
          _mm_min_epu8(b_.m128i, a_.m128i),
          b_.m128i
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u8x16_ge(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT8_MAX : 0;
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_u8
  #define vcgeq_u8(a, b) simde_vcgeq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vcgeq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcgeq_u16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short), vec_cmpge(a, b));
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128i =
        _mm_cmpeq_epi16(
          _mm_min_epu16(b_.m128i, a_.m128i),
          b_.m128i
        );
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      __m128i sign_bits = _mm_set1_epi16(INT16_MIN);
      r_.m128i = _mm_or_si128(_mm_cmpgt_epi16(_mm_xor_si128(a_.m128i, sign_bits), _mm_xor_si128(b_.m128i, sign_bits)), _mm_cmpeq_epi16(a_.m128i, b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u16x8_ge(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT16_MAX : 0;
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_u16
  #define vcgeq_u16(a, b) simde_vcgeq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vcgeq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcgeq_u32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), vec_cmpge(a, b));
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128i =
        _mm_cmpeq_epi32(
          _mm_min_epu32(b_.m128i, a_.m128i),
          b_.m128i
        );
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      __m128i sign_bits = _mm_set1_epi32(INT32_MIN);
      r_.m128i = _mm_or_si128(_mm_cmpgt_epi32(_mm_xor_si128(a_.m128i, sign_bits), _mm_xor_si128(b_.m128i, sign_bits)), _mm_cmpeq_epi32(a_.m128i, b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u32x4_ge(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT32_MAX : 0;
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_u32
  #define vcgeq_u32(a, b) simde_vcgeq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vcgeq_u64(simde_uint64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcgeq_u64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    return HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), vec_cmpge(a, b));
  #else
    simde_uint64x2_private
      r_,
      a_ = simde_uint64x2_to_private(a),
      b_ = simde_uint64x2_to_private(b);

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i =
        _mm_cmpeq_epi64(
          _mm_min_epu64(b_.m128i, a_.m128i),
          b_.m128i
        );
    #elif defined(SIMDE_X86_SSE4_2_NATIVE)
      __m128i sign_bits = _mm_set1_epi64x(INT64_MIN);
      r_.m128i = _mm_or_si128(_mm_cmpgt_epi64(_mm_xor_si128(a_.m128i, sign_bits), _mm_xor_si128(b_.m128i, sign_bits)), _mm_cmpeq_epi64(a_.m128i, b_.m128i));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT64_MAX : 0;
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcgeq_u64
  #define vcgeq_u64(a, b) simde_vcgeq_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vcge_f16(simde_float16x4_t a, simde_float16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V8_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vcge_f16(a, b);
  #else
    simde_float16x4_private
      a_ = simde_float16x4_to_private(a),
      b_ = simde_float16x4_to_private(b);
    simde_uint16x4_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vcgeh_f16(a_.values[i], b_.values[i]);
    }

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V8_ENABLE_NATIVE_ALIASES)
  #undef vcge_f16
  #define vcge_f16(a, b) simde_vcge_f16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vcge_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcge_f32(a, b);
  #else
    simde_float32x2_private
      a_ = simde_float32x2_to_private(a),
      b_ = simde_float32x2_to_private(b);
    simde_uint32x2_private r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT32_MAX : 0;
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcge_f32
  #define vcge_f32(a, b) simde_vcge_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vcge_f64(simde_float64x1_t a, simde_float64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcge_f64(a, b);
  #else
    simde_float64x1_private
      a_ = simde_float64x1_to_private(a),
      b_ = simde_float64x1_to_private(b);
    simde_uint64x1_private r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT64_MAX : 0;
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcge_f64
  #define vcge_f64(a, b) simde_vcge_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vcge_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcge_s8(a, b);
  #else
    simde_int8x8_private
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);
    simde_uint8x8_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_or_si64(_mm_cmpgt_pi8(a_.m64, b_.m64), _mm_cmpeq_pi8(a_.m64, b_.m64));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT8_MAX : 0;
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcge_s8
  #define vcge_s8(a, b) simde_vcge_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vcge_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcge_s16(a, b);
  #else
    simde_int16x4_private
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);
    simde_uint16x4_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_or_si64(_mm_cmpgt_pi16(a_.m64, b_.m64), _mm_cmpeq_pi16(a_.m64, b_.m64));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT16_MAX : 0;
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcge_s16
  #define vcge_s16(a, b) simde_vcge_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vcge_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcge_s32(a, b);
  #else
    simde_int32x2_private
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);
    simde_uint32x2_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_or_si64(_mm_cmpgt_pi32(a_.m64, b_.m64), _mm_cmpeq_pi32(a_.m64, b_.m64));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT32_MAX : 0;
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcge_s32
  #define vcge_s32(a, b) simde_vcge_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vcge_s64(simde_int64x1_t a, simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcge_s64(a, b);
  #else
    simde_int64x1_private
      a_ = simde_int64x1_to_private(a),
      b_ = simde_int64x1_to_private(b);
    simde_uint64x1_private r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT64_MAX : 0;
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcge_s64
  #define vcge_s64(a, b) simde_vcge_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vcge_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcge_u8(a, b);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      __m64 sign_bits = _mm_set1_pi8(INT8_MIN);
      r_.m64 = _mm_or_si64(_mm_cmpgt_pi8(_mm_xor_si64(a_.m64, sign_bits), _mm_xor_si64(b_.m64, sign_bits)), _mm_cmpeq_pi8(a_.m64, b_.m64));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT8_MAX : 0;
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcge_u8
  #define vcge_u8(a, b) simde_vcge_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vcge_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcge_u16(a, b);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      __m64 sign_bits = _mm_set1_pi16(INT16_MIN);
      r_.m64 = _mm_or_si64(_mm_cmpgt_pi16(_mm_xor_si64(a_.m64, sign_bits), _mm_xor_si64(b_.m64, sign_bits)), _mm_cmpeq_pi16(a_.m64, b_.m64));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT16_MAX : 0;
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcge_u16
  #define vcge_u16(a, b) simde_vcge_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vcge_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcge_u32(a, b);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      __m64 sign_bits = _mm_set1_pi32(INT32_MIN);
      r_.m64 = _mm_or_si64(_mm_cmpgt_pi32(_mm_xor_si64(a_.m64, sign_bits), _mm_xor_si64(b_.m64, sign_bits)), _mm_cmpeq_pi32(a_.m64, b_.m64));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT32_MAX : 0;
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcge_u32
  #define vcge_u32(a, b) simde_vcge_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vcge_u64(simde_uint64x1_t a, simde_uint64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcge_u64(a, b);
  #else
    simde_uint64x1_private
      r_,
      a_ = simde_uint64x1_to_private(a),
      b_ = simde_uint64x1_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values >= b_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] >= b_.values[i]) ? UINT64_MAX : 0;
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcge_u64
  #define vcge_u64(a, b) simde_vcge_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vcged_f64(simde_float64_t a, simde_float64_t b){
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return HEDLEY_STATIC_CAST(uint64_t, vcged_f64(a, b));
  #else
    return (a >= b) ? UINT64_MAX : 0;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcged_f64
  #define vcged_f64(a, b) simde_vcged_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vcged_s64(int64_t a, int64_t b){
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return HEDLEY_STATIC_CAST(uint64_t, vcged_s64(a, b));
  #else
    return (a >= b) ? UINT64_MAX : 0;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcged_s64
  #define vcged_s64(a, b) simde_vcged_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vcged_u64(uint64_t a, uint64_t b){
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return HEDLEY_STATIC_CAST(uint64_t, vcged_u64(a, b));
  #else
    return (a >= b) ? UINT64_MAX : 0;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcged_u64
  #define vcged_u64(a, b) simde_vcged_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vcges_f32(simde_float32_t a, simde_float32_t b){
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return HEDLEY_STATIC_CAST(uint32_t, vcges_f32(a, b));
  #else
    return (a >= b) ? UINT32_MAX : 0;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcges_f32
  #define vcges_f32(a, b) simde_vcges_f32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_CGE_H) */
