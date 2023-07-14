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

#if !defined(SIMDE_ARM_NEON_ZIP1_H)
#define SIMDE_ARM_NEON_ZIP1_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vzip1_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1_f32(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    float32x2x2_t tmp = vzip_f32(a, b);
    return tmp.val[0];
  #else
    simde_float32x2_private
      r_,
      a_ = simde_float32x2_to_private(a),
      b_ = simde_float32x2_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_unpacklo_pi32(a_.m64, b_.m64);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(32, 8, a_.values, b_.values, 0, 2);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1_f32
  #define vzip1_f32(a, b) simde_vzip1_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vzip1_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1_s8(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int8x8x2_t tmp = vzip_s8(a, b);
    return tmp.val[0];
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_unpacklo_pi8(a_.m64, b_.m64);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(8, 8, a_.values, b_.values, 0, 8, 1, 9, 2, 10, 3, 11);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1_s8
  #define vzip1_s8(a, b) simde_vzip1_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vzip1_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1_s16(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int16x4x2_t tmp = vzip_s16(a, b);
    return tmp.val[0];
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_unpacklo_pi16(a_.m64, b_.m64);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(16, 8, a_.values, b_.values, 0, 4, 1, 5);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1_s16
  #define vzip1_s16(a, b) simde_vzip1_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vzip1_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1_s32(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int32x2x2_t tmp = vzip_s32(a, b);
    return tmp.val[0];
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_unpacklo_pi32(a_.m64, b_.m64);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(32, 8, a_.values, b_.values, 0, 2);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1_s32
  #define vzip1_s32(a, b) simde_vzip1_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vzip1_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1_u8(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint8x8x2_t tmp = vzip_u8(a, b);
    return tmp.val[0];
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_unpacklo_pi8(a_.m64, b_.m64);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(8, 8, a_.values, b_.values, 0, 8, 1, 9, 2, 10, 3, 11);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1_u8
  #define vzip1_u8(a, b) simde_vzip1_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vzip1_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1_u16(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint16x4x2_t tmp = vzip_u16(a, b);
    return tmp.val[0];
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_unpacklo_pi16(a_.m64, b_.m64);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(16, 8, a_.values, b_.values, 0, 4, 1, 5);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1_u16
  #define vzip1_u16(a, b) simde_vzip1_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vzip1_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1_u32(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint32x2x2_t tmp = vzip_u32(a, b);
    return tmp.val[0];
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_unpacklo_pi32(a_.m64, b_.m64);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(32, 8, a_.values, b_.values, 0, 2);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1_u32
  #define vzip1_u32(a, b) simde_vzip1_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vzip1q_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_f32(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    float32x2x2_t tmp = vzip_f32(vget_low_f32(a), vget_low_f32(b));
    return vcombine_f32(tmp.val[0], tmp.val[1]);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_float32x4_private
      r_,
      a_ = simde_float32x4_to_private(a),
      b_ = simde_float32x4_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_shuffle(a_.v128, b_.v128, 0, 4, 1, 5);
    #elif defined(SIMDE_X86_SSE_NATIVE)
      r_.m128 = _mm_unpacklo_ps(a_.m128, b_.m128);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(32, 16, a_.values, b_.values, 0, 4, 1, 5);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_f32
  #define vzip1q_f32(a, b) simde_vzip1q_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vzip1q_f64(simde_float64x2_t a, simde_float64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_f64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_float64x2_private
      r_,
      a_ = simde_float64x2_to_private(a),
      b_ = simde_float64x2_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_shuffle(a_.v128, b_.v128, 0, 2);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128d = _mm_unpacklo_pd(a_.m128d, b_.m128d);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.values, b_.values, 0, 2);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_f64
  #define vzip1q_f64(a, b) simde_vzip1q_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vzip1q_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_s8(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int8x8x2_t tmp = vzip_s8(vget_low_s8(a), vget_low_s8(b));
    return vcombine_s8(tmp.val[0], tmp.val[1]);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_shuffle(a_.v128, b_.v128, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_unpacklo_epi8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(8, 16, a_.values, b_.values, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_s8
  #define vzip1q_s8(a, b) simde_vzip1q_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vzip1q_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_s16(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int16x4x2_t tmp = vzip_s16(vget_low_s16(a), vget_low_s16(b));
    return vcombine_s16(tmp.val[0], tmp.val[1]);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_shuffle(a_.v128, b_.v128, 0, 8, 1, 9, 2, 10, 3, 11);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_unpacklo_epi16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(16, 16, a_.values, b_.values, 0, 8, 1, 9, 2, 10, 3, 11);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_s16
  #define vzip1q_s16(a, b) simde_vzip1q_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vzip1q_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_s32(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int32x2x2_t tmp = vzip_s32(vget_low_s32(a), vget_low_s32(b));
    return vcombine_s32(tmp.val[0], tmp.val[1]);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_shuffle(a_.v128, b_.v128, 0, 4, 1, 5);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_unpacklo_epi32(a_.m128i, b_.m128i);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(32, 16, a_.values, b_.values, 0, 4, 1, 5);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_s32
  #define vzip1q_s32(a, b) simde_vzip1q_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vzip1q_s64(simde_int64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_s64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_shuffle(a_.v128, b_.v128, 0, 2);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_unpacklo_epi64(a_.m128i, b_.m128i);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.values, b_.values, 0, 2);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_s64
  #define vzip1q_s64(a, b) simde_vzip1q_s64((a), (b))
#endif


SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vzip1q_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_u8(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint8x8x2_t tmp = vzip_u8(vget_low_u8(a), vget_low_u8(b));
    return vcombine_u8(tmp.val[0], tmp.val[1]);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_shuffle(a_.v128, b_.v128, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_unpacklo_epi8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(8, 16, a_.values, b_.values, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_u8
  #define vzip1q_u8(a, b) simde_vzip1q_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vzip1q_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_u16(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint16x4x2_t tmp = vzip_u16(vget_low_u16(a), vget_low_u16(b));
    return vcombine_u16(tmp.val[0], tmp.val[1]);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_shuffle(a_.v128, b_.v128, 0, 8, 1, 9, 2, 10, 3, 11);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_unpacklo_epi16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(16, 16, a_.values, b_.values, 0, 8, 1, 9, 2, 10, 3, 11);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_u16
  #define vzip1q_u16(a, b) simde_vzip1q_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vzip1q_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_u32(a, b);
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint32x2x2_t tmp = vzip_u32(vget_low_u32(a), vget_low_u32(b));
    return vcombine_u32(tmp.val[0], tmp.val[1]);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_shuffle(a_.v128, b_.v128, 0, 4, 1, 5);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_unpacklo_epi32(a_.m128i, b_.m128i);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(32, 16, a_.values, b_.values, 0, 4, 1, 5);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_u32
  #define vzip1q_u32(a, b) simde_vzip1q_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vzip1q_u64(simde_uint64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vzip1q_u64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    return vec_mergeh(a, b);
  #else
    simde_uint64x2_private
      r_,
      a_ = simde_uint64x2_to_private(a),
      b_ = simde_uint64x2_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i64x2_shuffle(a_.v128, b_.v128, 0, 2);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_unpacklo_epi64(a_.m128i, b_.m128i);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_.values = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.values, b_.values, 0, 2);
    #else
      const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway_point ; i++) {
        r_.values[2 * i    ] = a_.values[i];
        r_.values[2 * i + 1] = b_.values[i];
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vzip1q_u64
  #define vzip1q_u64(a, b) simde_vzip1q_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_ZIP1_H) */
