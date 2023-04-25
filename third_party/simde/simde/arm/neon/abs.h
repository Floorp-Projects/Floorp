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

#if !defined(SIMDE_ARM_NEON_ABS_H)
#define SIMDE_ARM_NEON_ABS_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vabsd_s64(int64_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(9,1,0))
    return vabsd_s64(a);
  #else
    return a < 0 ? -a : a;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vabsd_s64
  #define vabsd_s64(a) simde_vabsd_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vabs_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabs_f32(a);
  #else
    simde_float32x2_private
      r_,
      a_ = simde_float32x2_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
    }

    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabs_f32
  #define vabs_f32(a) simde_vabs_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vabs_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vabs_f64(a);
  #else
    simde_float64x1_private
      r_,
      a_ = simde_float64x1_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
    }

    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vabs_f64
  #define vabs_f64(a) simde_vabs_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vabs_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabs_s8(a);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_abs_pi8(a_.m64);
    #elif (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      __typeof__(r_.values) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values < INT8_C(0));
      r_.values = (-a_.values & m) | (a_.values & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabs_s8
  #define vabs_s8(a) simde_vabs_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vabs_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabs_s16(a);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_abs_pi16(a_.m64);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100761)
      __typeof__(r_.values) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values < INT16_C(0));
      r_.values = (-a_.values & m) | (a_.values & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabs_s16
  #define vabs_s16(a) simde_vabs_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vabs_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabs_s32(a);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_abs_pi32(a_.m64);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100761)
      __typeof__(r_.values) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values < INT32_C(0));
      r_.values = (-a_.values & m) | (a_.values & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabs_s32
  #define vabs_s32(a) simde_vabs_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vabs_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vabs_s64(a);
  #else
    simde_int64x1_private
      r_,
      a_ = simde_int64x1_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      __typeof__(r_.values) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values < INT64_C(0));
      r_.values = (-a_.values & m) | (a_.values & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
      }
    #endif

    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabs_s64
  #define vabs_s64(a) simde_vabs_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vabsq_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabsq_f32(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_abs(a);
  #else
    simde_float32x4_private
      r_,
      a_ = simde_float32x4_to_private(a);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f32x4_abs(a_.v128);
    #elif defined(SIMDE_X86_SSE_NATIVE)
      simde_float32 mask_;
      uint32_t u32_ = UINT32_C(0x7FFFFFFF);
      simde_memcpy(&mask_, &u32_, sizeof(u32_));
      r_.m128 = _mm_and_ps(_mm_set1_ps(mask_), a_.m128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_math_fabsf(a_.values[i]);
      }
    #endif

    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabsq_f32
  #define vabsq_f32(a) simde_vabsq_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vabsq_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vabsq_f64(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return vec_abs(a);
  #else
    simde_float64x2_private
      r_,
      a_ = simde_float64x2_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      simde_float64 mask_;
      uint64_t u64_ = UINT64_C(0x7FFFFFFFFFFFFFFF);
      simde_memcpy(&mask_, &u64_, sizeof(u64_));
      r_.m128d = _mm_and_pd(_mm_set1_pd(mask_), a_.m128d);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_math_fabs(a_.values[i]);
      }
    #endif

    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vabsq_f64
  #define vabsq_f64(a) simde_vabsq_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vabsq_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabsq_s8(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_abs(a);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_abs_epi8(a_.m128i);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_min_epu8(a_.m128i, _mm_sub_epi8(_mm_setzero_si128(), a_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_abs(a_.v128);
    #elif (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
        __typeof__(r_.values) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values < INT8_C(0));
        r_.values = (-a_.values & m) | (a_.values & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabsq_s8
  #define vabsq_s8(a) simde_vabsq_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vabsq_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabsq_s16(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_abs(a);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_abs_epi16(a_.m128i);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_max_epi16(a_.m128i, _mm_sub_epi16(_mm_setzero_si128(), a_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_abs(a_.v128);
    #elif (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      __typeof__(r_.values) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values < INT16_C(0));
      r_.values = (-a_.values & m) | (a_.values & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabsq_s16
  #define vabsq_s16(a) simde_vabsq_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vabsq_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabsq_s32(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_abs(a);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE)
    r_.m128i = _mm_abs_epi32(a_.m128i);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i m = _mm_cmpgt_epi32(_mm_setzero_si128(), a_.m128i);
      r_.m128i = _mm_sub_epi32(_mm_xor_si128(a_.m128i, m), m);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_abs(a_.v128);
    #elif (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      __typeof__(r_.values) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values < INT32_C(0));
      r_.values = (-a_.values & m) | (a_.values & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabsq_s32
  #define vabsq_s32(a) simde_vabsq_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vabsq_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vabsq_s64(a);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vbslq_s64(vreinterpretq_u64_s64(vshrq_n_s64(a, 63)), vsubq_s64(vdupq_n_s64(0), a), a);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && !defined(HEDLEY_IBM_VERSION)
    return vec_abs(a);
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a);

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_abs_epi64(a_.m128i);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i m = _mm_srai_epi32(_mm_shuffle_epi32(a_.m128i, 0xF5), 31);
      r_.m128i = _mm_sub_epi64(_mm_xor_si128(a_.m128i, m), m);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_abs(a_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      __typeof__(r_.values) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values < INT64_C(0));
      r_.values = (-a_.values & m) | (a_.values & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] < 0 ? -a_.values[i] : a_.values[i];
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabsq_s64
  #define vabsq_s64(a) simde_vabsq_s64(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_ABS_H) */
