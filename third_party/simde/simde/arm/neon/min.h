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

#if !defined(SIMDE_ARM_NEON_MIN_H)
#define SIMDE_ARM_NEON_MIN_H

#include "types.h"
#include "cgt.h"
#include "ceq.h"
#include "bsl.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vmin_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmin_f32(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(64)
    simde_float32x2_t r = simde_vbsl_f32(simde_vcgt_f32(b, a), a, b);

    #if !defined(SIMDE_FAST_NANS)
      r = simde_vbsl_f32(simde_vceq_f32(a, a), simde_vbsl_f32(simde_vceq_f32(b, b), r, b), a);
    #endif

    return r;
  #else
    simde_float32x2_private
      r_,
      a_ = simde_float32x2_to_private(a),
      b_ = simde_float32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      #if !defined(SIMDE_FAST_NANS)
        if (simde_math_isnanf(a_.values[i])) {
          r_.values[i] = a_.values[i];
        } else if (simde_math_isnanf(b_.values[i])) {
          r_.values[i] = b_.values[i];
        } else {
          r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
        }
      #else
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      #endif
    }

    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmin_f32
  #define vmin_f32(a, b) simde_vmin_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vmin_f64(simde_float64x1_t a, simde_float64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vmin_f64(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(64)
    simde_float64x1_t r = simde_vbsl_f64(simde_vcgt_f64(b, a), a, b);

    #if !defined(SIMDE_FAST_NANS)
      r = simde_vbsl_f64(simde_vceq_f64(a, a), simde_vbsl_f64(simde_vceq_f64(b, b), r, b), a);
    #endif

    return r;
  #else
    simde_float64x1_private
      r_,
      a_ = simde_float64x1_to_private(a),
      b_ = simde_float64x1_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      #if !defined(SIMDE_FAST_NANS)
        if (simde_math_isnan(a_.values[i])) {
          r_.values[i] = a_.values[i];
        } else if (simde_math_isnan(b_.values[i])) {
          r_.values[i] = b_.values[i];
        } else {
          r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
        }
      #else
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      #endif
    }

    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vmin_f64
  #define vmin_f64(a, b) simde_vmin_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vmin_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmin_s8(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vbsl_s8(simde_vcgt_s8(b, a), a, b);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
    }

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmin_s8
  #define vmin_s8(a, b) simde_vmin_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vmin_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmin_s16(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vbsl_s16(simde_vcgt_s16(b, a), a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_sub_pi16(a_.m64, _mm_subs_pu16(b_.m64));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmin_s16
  #define vmin_s16(a, b) simde_vmin_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vmin_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmin_s32(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vbsl_s32(simde_vcgt_s32(b, a), a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
    }

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmin_s32
  #define vmin_s32(a, b) simde_vmin_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_x_vmin_s64(simde_int64x1_t a, simde_int64x1_t b) {
  #if SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vbsl_s64(simde_vcgt_s64(b, a), a, b);
  #else
    simde_int64x1_private
      r_,
      a_ = simde_int64x1_to_private(a),
      b_ = simde_int64x1_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
    }

    return simde_int64x1_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vmin_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmin_u8(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vbsl_u8(simde_vcgt_u8(b, a), a, b);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
    }

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmin_u8
  #define vmin_u8(a, b) simde_vmin_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vmin_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmin_u16(a, b);
  #elif (SIMDE_NATURAL_VECTOR_SIZE > 0) && !defined(SIMDE_X86_SSE2_NATIVE)
    return simde_vbsl_u16(simde_vcgt_u16(b, a), a, b);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      /* https://github.com/simd-everywhere/simde/issues/855#issuecomment-881656284 */
      r_.m64 = _mm_sub_pi16(a_.m64, _mm_subs_pu16(a_.m64, b_.m64));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmin_u16
  #define vmin_u16(a, b) simde_vmin_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vmin_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmin_u32(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vbsl_u32(simde_vcgt_u32(b, a), a, b);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
    }

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmin_u32
  #define vmin_u32(a, b) simde_vmin_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_x_vmin_u64(simde_uint64x1_t a, simde_uint64x1_t b) {
  #if SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vbsl_u64(simde_vcgt_u64(b, a), a, b);
  #else
    simde_uint64x1_private
      r_,
      a_ = simde_uint64x1_to_private(a),
      b_ = simde_uint64x1_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
    }

    return simde_uint64x1_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vminq_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vminq_f32(a, b);
  #elif (defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)) && defined(SIMDE_FAST_NANS)
    return vec_min(a, b);
  #else
    simde_float32x4_private
      r_,
      a_ = simde_float32x4_to_private(a),
      b_ = simde_float32x4_to_private(b);

    #if defined(SIMDE_X86_SSE_NATIVE) && defined(SIMDE_FAST_NANS)
      r_.m128 = _mm_min_ps(a_.m128, b_.m128);
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128 = _mm_blendv_ps(_mm_set1_ps(SIMDE_MATH_NANF), _mm_min_ps(a_.m128, b_.m128), _mm_cmpord_ps(a_.m128, b_.m128));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f32x4_min(a_.v128, b_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        #if !defined(SIMDE_FAST_NANS)
          if (simde_math_isnanf(a_.values[i])) {
            r_.values[i] = a_.values[i];
          } else if (simde_math_isnanf(b_.values[i])) {
            r_.values[i] = b_.values[i];
          } else {
            r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
          }
        #else
          r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
        #endif
      }
    #endif

    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vminq_f32
  #define vminq_f32(a, b) simde_vminq_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vminq_f64(simde_float64x2_t a, simde_float64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vminq_f64(a, b);
  #elif (defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)) && defined(SIMDE_FAST_NANS)
    return vec_min(a, b);
  #else
    simde_float64x2_private
      r_,
      a_ = simde_float64x2_to_private(a),
      b_ = simde_float64x2_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_FAST_NANS)
      r_.m128d = _mm_min_pd(a_.m128d, b_.m128d);
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128d = _mm_blendv_pd(_mm_set1_pd(SIMDE_MATH_NAN), _mm_min_pd(a_.m128d, b_.m128d), _mm_cmpord_pd(a_.m128d, b_.m128d));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f64x2_min(a_.v128, b_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        #if !defined(SIMDE_FAST_NANS)
          if (simde_math_isnan(a_.values[i])) {
            r_.values[i] = a_.values[i];
          } else if (simde_math_isnan(b_.values[i])) {
            r_.values[i] = b_.values[i];
          } else {
            r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
          }
        #else
          r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
        #endif
      }
    #endif

    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vminq_f64
  #define vminq_f64(a, b) simde_vminq_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vminq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vminq_s8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_min(a, b);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128i = _mm_min_epi8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_min(a_.v128, b_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vminq_s8
  #define vminq_s8(a, b) simde_vminq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vminq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vminq_s16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_min(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_min_epi16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_min(a_.v128, b_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vminq_s16
  #define vminq_s16(a, b) simde_vminq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vminq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vminq_s32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_min(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128i = _mm_min_epi32(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_min(a_.v128, b_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vminq_s32
  #define vminq_s32(a, b) simde_vminq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_x_vminq_s64(simde_int64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_min(a, b);
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b);

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm_min_epi64(a_.m128i, b_.m128i);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vminq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vminq_u8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_min(a, b);
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_min_epu8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u8x16_min(a_.v128, b_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vminq_u8
  #define vminq_u8(a, b) simde_vminq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vminq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vminq_u16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_min(a, b);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128i = _mm_min_epu16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      /* https://github.com/simd-everywhere/simde/issues/855#issuecomment-881656284 */
      r_.m128i = _mm_sub_epi16(a_.m128i, _mm_subs_epu16(a_.m128i, b_.m128i));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u16x8_min(a_.v128, b_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vminq_u16
  #define vminq_u16(a, b) simde_vminq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vminq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vminq_u32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_min(a, b);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.m128i = _mm_min_epu32(a_.m128i, b_.m128i);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i i32_min = _mm_set1_epi32(INT32_MIN);
      const __m128i difference = _mm_sub_epi32(a_.m128i, b_.m128i);
      __m128i m =
        _mm_cmpeq_epi32(
          /* _mm_subs_epu32(a_.sse_m128i, b_.sse_m128i) */
          _mm_and_si128(
            difference,
            _mm_xor_si128(
              _mm_cmpgt_epi32(
                _mm_xor_si128(difference, i32_min),
                _mm_xor_si128(a_.m128i, i32_min)
              ),
              _mm_set1_epi32(~INT32_C(0))
            )
          ),
          _mm_setzero_si128()
        );
      r_.m128i =
        _mm_or_si128(
          _mm_and_si128(m, a_.m128i),
          _mm_andnot_si128(m, b_.m128i)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u32x4_min(a_.v128, b_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vminq_u32
  #define vminq_u32(a, b) simde_vminq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_x_vminq_u64(simde_uint64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_min(a, b);
  #else
    simde_uint64x2_private
      r_,
      a_ = simde_uint64x2_to_private(a),
      b_ = simde_uint64x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = (a_.values[i] < b_.values[i]) ? a_.values[i] : b_.values[i];
    }

    return simde_uint64x2_from_private(r_);
  #endif
}

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_MIN_H) */
