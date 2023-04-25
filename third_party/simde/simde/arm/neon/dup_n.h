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
 *   2020      Sean Maher <seanptmaher@gmail.com> (Copyright owned by Google, LLC)
 *   2020      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_NEON_DUP_N_H)
#define SIMDE_ARM_NEON_DUP_N_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float16x4_t
simde_vdup_n_f16(simde_float16 value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vdup_n_f16(value);
  #else
    simde_float16x4_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = value;
    }

    return simde_float16x4_from_private(r_);
  #endif
}
#define simde_vmov_n_f16 simde_vdup_n_f16
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_f16
  #define vdup_n_f16(value) simde_vdup_n_f16((value))
  #undef vmov_n_f16
  #define vmov_n_f16(value) simde_vmov_n_f16((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vdup_n_f32(float value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_f32(value);
  #else
    simde_float32x2_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = value;
    }

    return simde_float32x2_from_private(r_);
  #endif
}
#define simde_vmov_n_f32 simde_vdup_n_f32
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_f32
  #define vdup_n_f32(value) simde_vdup_n_f32((value))
  #undef vmov_n_f32
  #define vmov_n_f32(value) simde_vmov_n_f32((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vdup_n_f64(double value) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vdup_n_f64(value);
  #else
    simde_float64x1_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = value;
    }

    return simde_float64x1_from_private(r_);
  #endif
}
#define simde_vmov_n_f64 simde_vdup_n_f64
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_f64
  #define vdup_n_f64(value) simde_vdup_n_f64((value))
  #undef vmov_n_f64
  #define vmov_n_f64(value) simde_vmov_n_f64((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vdup_n_s8(int8_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_s8(value);
  #else
    simde_int8x8_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_set1_pi8(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#define simde_vmov_n_s8 simde_vdup_n_s8
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_s8
  #define vdup_n_s8(value) simde_vdup_n_s8((value))
  #undef vmov_n_s8
  #define vmov_n_s8(value) simde_vmov_n_s8((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vdup_n_s16(int16_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_s16(value);
  #else
    simde_int16x4_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_set1_pi16(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#define simde_vmov_n_s16 simde_vdup_n_s16
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_s16
  #define vdup_n_s16(value) simde_vdup_n_s16((value))
  #undef vmov_n_s16
  #define vmov_n_s16(value) simde_vmov_n_s16((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vdup_n_s32(int32_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_s32(value);
  #else
    simde_int32x2_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_set1_pi32(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#define simde_vmov_n_s32 simde_vdup_n_s32
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_s32
  #define vdup_n_s32(value) simde_vdup_n_s32((value))
  #undef vmov_n_s32
  #define vmov_n_s32(value) simde_vmov_n_s32((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vdup_n_s64(int64_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_s64(value);
  #else
    simde_int64x1_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = value;
    }

    return simde_int64x1_from_private(r_);
  #endif
}
#define simde_vmov_n_s64 simde_vdup_n_s64
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_s64
  #define vdup_n_s64(value) simde_vdup_n_s64((value))
  #undef vmov_n_s64
  #define vmov_n_s64(value) simde_vmov_n_s64((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vdup_n_u8(uint8_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_u8(value);
  #else
    simde_uint8x8_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_set1_pi8(HEDLEY_STATIC_CAST(int8_t, value));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#define simde_vmov_n_u8 simde_vdup_n_u8
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_u8
  #define vdup_n_u8(value) simde_vdup_n_u8((value))
  #undef vmov_n_u8
  #define vmov_n_u8(value) simde_vmov_n_u8((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vdup_n_u16(uint16_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_u16(value);
  #else
    simde_uint16x4_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_set1_pi16(HEDLEY_STATIC_CAST(int16_t, value));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#define simde_vmov_n_u16 simde_vdup_n_u16
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_u16
  #define vdup_n_u16(value) simde_vdup_n_u16((value))
  #undef vmov_n_u16
  #define vmov_n_u16(value) simde_vmov_n_u16((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vdup_n_u32(uint32_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_u32(value);
  #else
    simde_uint32x2_private r_;

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_set1_pi32(HEDLEY_STATIC_CAST(int32_t, value));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#define simde_vmov_n_u32 simde_vdup_n_u32
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_u32
  #define vdup_n_u32(value) simde_vdup_n_u32((value))
  #undef vmov_n_u32
  #define vmov_n_u32(value) simde_vmov_n_u32((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vdup_n_u64(uint64_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdup_n_u64(value);
  #else
    simde_uint64x1_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = value;
    }

    return simde_uint64x1_from_private(r_);
  #endif
}
#define simde_vmov_n_u64 simde_vdup_n_u64
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_n_u64
  #define vdup_n_u64(value) simde_vdup_n_u64((value))
  #undef vmov_n_u64
  #define vmov_n_u64(value) simde_vmov_n_u64((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float16x8_t
simde_vdupq_n_f16(simde_float16 value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vdupq_n_f16(value);
  #else
    simde_float16x8_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = value;
    }

    return simde_float16x8_from_private(r_);
  #endif
}
#define simde_vmovq_n_f32 simde_vdupq_n_f32
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_f16
  #define vdupq_n_f16(value) simde_vdupq_n_f16((value))
  #undef vmovq_n_f16
  #define vmovq_n_f16(value) simde_vmovq_n_f16((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vdupq_n_f32(float value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_f32(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
    (void) value;
    return vec_splats(value);
  #else
    simde_float32x4_private r_;

    #if defined(SIMDE_X86_SSE_NATIVE)
      r_.m128 = _mm_set1_ps(value);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f32x4_splat(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_float32x4_from_private(r_);
  #endif
}
#define simde_vmovq_n_f32 simde_vdupq_n_f32
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_f32
  #define vdupq_n_f32(value) simde_vdupq_n_f32((value))
  #undef vmovq_n_f32
  #define vmovq_n_f32(value) simde_vmovq_n_f32((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vdupq_n_f64(double value) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vdupq_n_f64(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    (void) value;
    return vec_splats(value);
  #else
    simde_float64x2_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128d = _mm_set1_pd(value);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_f64x2_splat(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_float64x2_from_private(r_);
  #endif
}
#define simde_vmovq_n_f64 simde_vdupq_n_f64
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_f64
  #define vdupq_n_f64(value) simde_vdupq_n_f64((value))
  #undef vmovq_n_f64
  #define vmovq_n_f64(value) simde_vmovq_n_f64((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vdupq_n_s8(int8_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_s8(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_splats(value);
  #else
    simde_int8x16_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_set1_epi8(value);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_splat(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#define simde_vmovq_n_s8 simde_vdupq_n_s8
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_s8
  #define vdupq_n_s8(value) simde_vdupq_n_s8((value))
  #undef vmovq_n_s8
  #define vmovq_n_s8(value) simde_vmovq_n_s8((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vdupq_n_s16(int16_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_s16(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_splats(value);
  #else
    simde_int16x8_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_set1_epi16(value);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_splat(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#define simde_vmovq_n_s16 simde_vdupq_n_s16
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_s16
  #define vdupq_n_s16(value) simde_vdupq_n_s16((value))
  #undef vmovq_n_s16
  #define vmovq_n_s16(value) simde_vmovq_n_s16((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vdupq_n_s32(int32_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_s32(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_splats(value);
  #else
    simde_int32x4_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_set1_epi32(value);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_splat(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#define simde_vmovq_n_s32 simde_vdupq_n_s32
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_s32
  #define vdupq_n_s32(value) simde_vdupq_n_s32((value))
  #undef vmovq_n_s32
  #define vmovq_n_s32(value) simde_vmovq_n_s32((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vdupq_n_s64(int64_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_s64(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_splats(HEDLEY_STATIC_CAST(signed long long, value));
  #else
    simde_int64x2_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,0,0))
      r_.m128i = _mm_set1_epi64x(value);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_splat(value);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#define simde_vmovq_n_s64 simde_vdupq_n_s64
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_s64
  #define vdupq_n_s64(value) simde_vdupq_n_s64((value))
  #undef vmovq_n_s64
  #define vmovq_n_s64(value) simde_vmovq_n_s64((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vdupq_n_u8(uint8_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_u8(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_splats(value);
  #else
    simde_uint8x16_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_set1_epi8(HEDLEY_STATIC_CAST(int8_t, value));
    #elif defined (SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_splat(HEDLEY_STATIC_CAST(int8_t, value));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#define simde_vmovq_n_u8 simde_vdupq_n_u8
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_u8
  #define vdupq_n_u8(value) simde_vdupq_n_u8((value))
  #undef vmovq_n_u8
  #define vmovq_n_u8(value) simde_vmovq_n_u8((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vdupq_n_u16(uint16_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_u16(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_splats(value);
  #else
    simde_uint16x8_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_set1_epi16(HEDLEY_STATIC_CAST(int16_t, value));
    #elif defined (SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_splat(HEDLEY_STATIC_CAST(int16_t, value));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#define simde_vmovq_n_u16 simde_vdupq_n_u16
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_u16
  #define vdupq_n_u16(value) simde_vdupq_n_u16((value))
  #undef vmovq_n_u16
  #define vmovq_n_u16(value) simde_vmovq_n_u16((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vdupq_n_u32(uint32_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_u32(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_splats(value);
  #else
    simde_uint32x4_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_set1_epi32(HEDLEY_STATIC_CAST(int32_t, value));
    #elif defined (SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_splat(HEDLEY_STATIC_CAST(int32_t, value));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#define simde_vmovq_n_u32 simde_vdupq_n_u32
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_u32
  #define vdupq_n_u32(value) simde_vdupq_n_u32((value))
  #undef vmovq_n_u32
  #define vmovq_n_u32(value) simde_vmovq_n_u32((value))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vdupq_n_u64(uint64_t value) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vdupq_n_u64(value);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    return vec_splats(HEDLEY_STATIC_CAST(unsigned long long, value));
  #else
    simde_uint64x2_private r_;

    #if defined(SIMDE_X86_SSE2_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,0,0))
      r_.m128i = _mm_set1_epi64x(HEDLEY_STATIC_CAST(int64_t, value));
    #elif defined (SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_splat(HEDLEY_STATIC_CAST(int64_t, value));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = value;
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#define simde_vmovq_n_u64 simde_vdupq_n_u64
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_n_u64
  #define vdupq_n_u64(value) simde_vdupq_n_u64((value))
  #undef vmovq_n_u64
  #define vmovq_n_u64(value) simde_vmovq_n_u64((value))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_DUP_N_H) */
