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
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_DUP_H)
#define SIMDE_ARM_SVE_DUP_H

#include "types.h"
#include "reinterpret.h"
#include "sel.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_n_s8(int8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s8(op);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_s8(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi8(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi8(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi8(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s8
  #define svdup_n_s8(op) simde_svdup_n_s8((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_s8(int8_t op) {
  return simde_svdup_n_s8(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s8
  #define svdup_s8(op) simde_svdup_n_s8((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_n_s8_z(simde_svbool_t pg, int8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s8_z(pg, op);
  #else
    return simde_x_svsel_s8_z(pg, simde_svdup_n_s8(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s8_z
  #define svdup_n_s8_z(pg, op) simde_svdup_n_s8_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_s8_z(simde_svbool_t pg, int8_t op) {
  return simde_svdup_n_s8_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s8_z
  #define svdup_s8_z(pg, op) simde_svdup_n_s8_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_n_s8_m(simde_svint8_t inactive, simde_svbool_t pg, int8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s8_m(inactive, pg, op);
  #else
    return simde_svsel_s8(pg, simde_svdup_n_s8(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s8_m
  #define svdup_n_s8_m(inactive, pg, op) simde_svdup_n_s8_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_s8_m(simde_svint8_t inactive, simde_svbool_t pg, int8_t op) {
  return simde_svdup_n_s8_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s8_m
  #define svdup_s8_m(inactive, pg, op) simde_svdup_n_s8_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_n_s16(int16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s16(op);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_s16(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi16(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi16(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi16(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s16
  #define svdup_n_s16(op) simde_svdup_n_s16((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_s16(int16_t op) {
  return simde_svdup_n_s16(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s16
  #define svdup_s16(op) simde_svdup_n_s16((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_n_s16_z(simde_svbool_t pg, int16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s16_z(pg, op);
  #else
    return simde_x_svsel_s16_z(pg, simde_svdup_n_s16(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s16_z
  #define svdup_n_s16_z(pg, op) simde_svdup_n_s16_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_s16_z(simde_svbool_t pg, int8_t op) {
  return simde_svdup_n_s16_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s16_z
  #define svdup_s16_z(pg, op) simde_svdup_n_s16_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_n_s16_m(simde_svint16_t inactive, simde_svbool_t pg, int16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s16_m(inactive, pg, op);
  #else
    return simde_svsel_s16(pg, simde_svdup_n_s16(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s16_m
  #define svdup_n_s16_m(inactive, pg, op) simde_svdup_n_s16_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_s16_m(simde_svint16_t inactive, simde_svbool_t pg, int16_t op) {
  return simde_svdup_n_s16_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s16_m
  #define svdup_s16_m(inactive, pg, op) simde_svdup_n_s16_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_n_s32(int32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s32(op);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_s32(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi32(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi32(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi32(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s32
  #define svdup_n_s32(op) simde_svdup_n_s32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_s32(int8_t op) {
  return simde_svdup_n_s32(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s32
  #define svdup_s32(op) simde_svdup_n_s32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_n_s32_z(simde_svbool_t pg, int32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s32_z(pg, op);
  #else
    return simde_x_svsel_s32_z(pg, simde_svdup_n_s32(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s32_z
  #define svdup_n_s32_z(pg, op) simde_svdup_n_s32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_s32_z(simde_svbool_t pg, int32_t op) {
  return simde_svdup_n_s32_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s32_z
  #define svdup_s32_z(pg, op) simde_svdup_n_s32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_n_s32_m(simde_svint32_t inactive, simde_svbool_t pg, int32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s32_m(inactive, pg, op);
  #else
    return simde_svsel_s32(pg, simde_svdup_n_s32(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s32_m
  #define svdup_n_s32_m(inactive, pg, op) simde_svdup_n_s32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_s32_m(simde_svint32_t inactive, simde_svbool_t pg, int32_t op) {
  return simde_svdup_n_s32_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s32_m
  #define svdup_s32_m(inactive, pg, op) simde_svdup_n_s32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_n_s64(int64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s64(op);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_s64(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi64(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi64x(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi64x(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(HEDLEY_STATIC_CAST(signed long long int, op));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s64
  #define svdup_n_s64(op) simde_svdup_n_s64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_s64(int64_t op) {
  return simde_svdup_n_s64(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s64
  #define svdup_s64(op) simde_svdup_n_s64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_n_s64_z(simde_svbool_t pg, int64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s64_z(pg, op);
  #else
    return simde_x_svsel_s64_z(pg, simde_svdup_n_s64(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s64_z
  #define svdup_n_s64_z(pg, op) simde_svdup_n_s64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_s64_z(simde_svbool_t pg, int64_t op) {
  return simde_svdup_n_s64_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s64_z
  #define svdup_s64_z(pg, op) simde_svdup_n_f64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_n_s64_m(simde_svint64_t inactive, simde_svbool_t pg, int64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s64_m(inactive, pg, op);
  #else
    return simde_svsel_s64(pg, simde_svdup_n_s64(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s64_m
  #define svdup_n_s64_m(inactive, pg, op) simde_svdup_n_s64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_s64_m(simde_svint64_t inactive, simde_svbool_t pg, int64_t op) {
  return simde_svdup_n_s64_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s64_m
  #define svdup_s64_m(inactive, pg, op) simde_svdup_n_s64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_n_u8(uint8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u8(op);
  #else
    simde_svuint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_u8(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi8(HEDLEY_STATIC_CAST(int8_t, op));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi8(HEDLEY_STATIC_CAST(int8_t, op));
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi8(HEDLEY_STATIC_CAST(int8_t, op));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_splat(HEDLEY_STATIC_CAST(int8_t, op));
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u8
  #define svdup_n_u8(op) simde_svdup_n_u8((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_u8(uint8_t op) {
  return simde_svdup_n_u8(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u8
  #define svdup_u8(op) simde_svdup_n_u8((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_n_u8_z(simde_svbool_t pg, uint8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u8_z(pg, op);
  #else
    return simde_x_svsel_u8_z(pg, simde_svdup_n_u8(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u8_z
  #define svdup_n_u8_z(pg, op) simde_svdup_n_u8_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_u8_z(simde_svbool_t pg, uint8_t op) {
  return simde_svdup_n_u8_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u8_z
  #define svdup_u8_z(pg, op) simde_svdup_n_u8_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_n_u8_m(simde_svuint8_t inactive, simde_svbool_t pg, uint8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u8_m(inactive, pg, op);
  #else
    return simde_svsel_u8(pg, simde_svdup_n_u8(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u8_m
  #define svdup_n_u8_m(inactive, pg, op) simde_svdup_n_u8_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_u8_m(simde_svuint8_t inactive, simde_svbool_t pg, uint8_t op) {
  return simde_svdup_n_u8_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u8_m
  #define svdup_u8_m(inactive, pg, op) simde_svdup_n_u8_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_n_u16(uint16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u16(op);
  #else
    simde_svuint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_u16(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi16(HEDLEY_STATIC_CAST(int16_t, op));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi16(HEDLEY_STATIC_CAST(int16_t, op));
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi16(HEDLEY_STATIC_CAST(int16_t, op));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_splat(HEDLEY_STATIC_CAST(int16_t, op));
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u16
  #define svdup_n_u16(op) simde_svdup_n_u16((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_u16(uint16_t op) {
  return simde_svdup_n_u16(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u16
  #define svdup_u16(op) simde_svdup_n_u16((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_n_u16_z(simde_svbool_t pg, uint16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u16_z(pg, op);
  #else
    return simde_x_svsel_u16_z(pg, simde_svdup_n_u16(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u16_z
  #define svdup_n_u16_z(pg, op) simde_svdup_n_u16_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_u16_z(simde_svbool_t pg, uint8_t op) {
  return simde_svdup_n_u16_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u16_z
  #define svdup_u16_z(pg, op) simde_svdup_n_u16_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_n_u16_m(simde_svuint16_t inactive, simde_svbool_t pg, uint16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u16_m(inactive, pg, op);
  #else
    return simde_svsel_u16(pg, simde_svdup_n_u16(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u16_m
  #define svdup_n_u16_m(inactive, pg, op) simde_svdup_n_u16_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_u16_m(simde_svuint16_t inactive, simde_svbool_t pg, uint16_t op) {
  return simde_svdup_n_u16_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u16_m
  #define svdup_u16_m(inactive, pg, op) simde_svdup_n_u16_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_n_u32(uint32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u32(op);
  #else
    simde_svuint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_u32(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi32(HEDLEY_STATIC_CAST(int32_t, op));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi32(HEDLEY_STATIC_CAST(int32_t, op));
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi32(HEDLEY_STATIC_CAST(int32_t, op));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_splat(HEDLEY_STATIC_CAST(int32_t, op));
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u32
  #define svdup_n_u32(op) simde_svdup_n_u32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_u32(uint8_t op) {
  return simde_svdup_n_u32(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u32
  #define svdup_u32(op) simde_svdup_n_u32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_n_u32_z(simde_svbool_t pg, uint32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u32_z(pg, op);
  #else
    return simde_x_svsel_u32_z(pg, simde_svdup_n_u32(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u32_z
  #define svdup_n_u32_z(pg, op) simde_svdup_n_u32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_u32_z(simde_svbool_t pg, uint32_t op) {
  return simde_svdup_n_u32_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u32_z
  #define svdup_u32_z(pg, op) simde_svdup_n_u32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_n_u32_m(simde_svuint32_t inactive, simde_svbool_t pg, uint32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u32_m(inactive, pg, op);
  #else
    return simde_svsel_u32(pg, simde_svdup_n_u32(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u32_m
  #define svdup_n_u32_m(inactive, pg, op) simde_svdup_n_u32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_u32_m(simde_svuint32_t inactive, simde_svbool_t pg, uint32_t op) {
  return simde_svdup_n_u32_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u32_m
  #define svdup_u32_m(inactive, pg, op) simde_svdup_n_u32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_n_u64(uint64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u64(op);
  #else
    simde_svuint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_u64(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi64(HEDLEY_STATIC_CAST(int64_t, op));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi64x(HEDLEY_STATIC_CAST(int64_t, op));
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi64x(HEDLEY_STATIC_CAST(int64_t, op));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(HEDLEY_STATIC_CAST(unsigned long long int, op));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_splat(HEDLEY_STATIC_CAST(int64_t, op));
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u64
  #define svdup_n_u64(op) simde_svdup_n_u64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_u64(uint64_t op) {
  return simde_svdup_n_u64(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u64
  #define svdup_u64(op) simde_svdup_n_u64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_n_u64_z(simde_svbool_t pg, uint64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u64_z(pg, op);
  #else
    return simde_x_svsel_u64_z(pg, simde_svdup_n_u64(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u64_z
  #define svdup_n_u64_z(pg, op) simde_svdup_n_u64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_u64_z(simde_svbool_t pg, uint64_t op) {
  return simde_svdup_n_u64_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u64_z
  #define svdup_u64_z(pg, op) simde_svdup_n_f64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_n_u64_m(simde_svuint64_t inactive, simde_svbool_t pg, uint64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u64_m(inactive, pg, op);
  #else
    return simde_svsel_u64(pg, simde_svdup_n_u64(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u64_m
  #define svdup_n_u64_m(inactive, pg, op) simde_svdup_n_u64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_u64_m(simde_svuint64_t inactive, simde_svbool_t pg, uint64_t op) {
  return simde_svdup_n_u64_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u64_m
  #define svdup_u64_m(inactive, pg, op) simde_svdup_n_u64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_n_f32(simde_float32 op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f32(op);
  #else
    simde_svfloat32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_f32(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512 = _mm512_set1_ps(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256) / sizeof(r.m256[0])) ; i++) {
        r.m256[i] = _mm256_set1_ps(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128) / sizeof(r.m128[0])) ; i++) {
        r.m128[i] = _mm_set1_ps(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f32x4_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f32
  #define svdup_n_f32(op) simde_svdup_n_f32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_f32(int8_t op) {
  return simde_svdup_n_f32(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f32
  #define svdup_f32(op) simde_svdup_n_f32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_n_f32_z(simde_svbool_t pg, simde_float32 op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f32_z(pg, op);
  #else
    return simde_x_svsel_f32_z(pg, simde_svdup_n_f32(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f32_z
  #define svdup_n_f32_z(pg, op) simde_svdup_n_f32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_f32_z(simde_svbool_t pg, simde_float32 op) {
  return simde_svdup_n_f32_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f32_z
  #define svdup_f32_z(pg, op) simde_svdup_n_f32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_n_f32_m(simde_svfloat32_t inactive, simde_svbool_t pg, simde_float32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f32_m(inactive, pg, op);
  #else
    return simde_svsel_f32(pg, simde_svdup_n_f32(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f32_m
  #define svdup_n_f32_m(inactive, pg, op) simde_svdup_n_f32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_f32_m(simde_svfloat32_t inactive, simde_svbool_t pg, simde_float32_t op) {
  return simde_svdup_n_f32_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f32_m
  #define svdup_f32_m(inactive, pg, op) simde_svdup_n_f32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_n_f64(simde_float64 op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f64(op);
  #else
    simde_svfloat64_t r;

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r.neon = vdupq_n_f64(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512d = _mm512_set1_pd(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256d) / sizeof(r.m256d[0])) ; i++) {
        r.m256d[i] = _mm256_set1_pd(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128d) / sizeof(r.m128d[0])) ; i++) {
        r.m128d[i] = _mm_set1_pd(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f64x2_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f64
  #define svdup_n_f64(op) simde_svdup_n_f64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_f64(simde_float64 op) {
  return simde_svdup_n_f64(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f64
  #define svdup_f64(op) simde_svdup_n_f64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_n_f64_z(simde_svbool_t pg, simde_float64 op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f64_z(pg, op);
  #else
    return simde_x_svsel_f64_z(pg, simde_svdup_n_f64(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f64_z
  #define svdup_n_f64_z(pg, op) simde_svdup_n_f64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_f64_z(simde_svbool_t pg, simde_float64 op) {
  return simde_svdup_n_f64_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f64_z
  #define svdup_f64_z(pg, op) simde_svdup_n_f64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_n_f64_m(simde_svfloat64_t inactive, simde_svbool_t pg, simde_float64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f64_m(inactive, pg, op);
  #else
    return simde_svsel_f64(pg, simde_svdup_n_f64(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f64_m
  #define svdup_n_f64_m(inactive, pg, op) simde_svdup_n_f64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_f64_m(simde_svfloat64_t inactive, simde_svbool_t pg, simde_float64_t op) {
  return simde_svdup_n_f64_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f64_m
  #define svdup_f64_m(inactive, pg, op) simde_svdup_n_f64_m((inactive), (pg), (op))
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svdup_n  (                          int8_t  op) { return simde_svdup_n_s8    (    op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svdup    (                          int8_t  op) { return simde_svdup_n_s8    (    op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svdup_n_z(simde_svbool_t pg,        int8_t  op) { return simde_svdup_n_s8_z  (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svdup_z  (simde_svbool_t pg,        int8_t  op) { return simde_svdup_n_s8_z  (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svdup_n  (                         int16_t  op) { return simde_svdup_n_s16   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svdup    (                         int16_t  op) { return simde_svdup_n_s16   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svdup_n_z(simde_svbool_t pg,       int16_t  op) { return simde_svdup_n_s16_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svdup_z  (simde_svbool_t pg,       int16_t  op) { return simde_svdup_n_s16_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svdup_n  (                         int32_t  op) { return simde_svdup_n_s32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svdup    (                         int32_t  op) { return simde_svdup_n_s32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svdup_n_z(simde_svbool_t pg,       int32_t  op) { return simde_svdup_n_s32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svdup_z  (simde_svbool_t pg,       int32_t  op) { return simde_svdup_n_s32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svdup_n  (                         int64_t  op) { return simde_svdup_n_s64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svdup    (                         int64_t  op) { return simde_svdup_n_s64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svdup_n_z(simde_svbool_t pg,       int64_t  op) { return simde_svdup_n_s64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svdup_z  (simde_svbool_t pg,       int64_t  op) { return simde_svdup_n_s64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svdup_n  (                         uint8_t  op) { return simde_svdup_n_u8    (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svdup    (                         uint8_t  op) { return simde_svdup_n_u8    (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svdup_n_z(simde_svbool_t pg,       uint8_t  op) { return simde_svdup_n_u8_z  (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svdup_z  (simde_svbool_t pg,       uint8_t  op) { return simde_svdup_n_u8_z  (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svdup_n  (                        uint16_t  op) { return simde_svdup_n_u16   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svdup    (                        uint16_t  op) { return simde_svdup_n_u16   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svdup_n_z(simde_svbool_t pg,      uint16_t  op) { return simde_svdup_n_u16_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svdup_z  (simde_svbool_t pg,      uint16_t  op) { return simde_svdup_n_u16_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svdup_n  (                        uint32_t  op) { return simde_svdup_n_u32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svdup    (                        uint32_t  op) { return simde_svdup_n_u32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svdup_n_z(simde_svbool_t pg,      uint32_t  op) { return simde_svdup_n_u32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svdup_z  (simde_svbool_t pg,      uint32_t  op) { return simde_svdup_n_u32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svdup_n  (                        uint64_t  op) { return simde_svdup_n_u64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svdup    (                        uint64_t  op) { return simde_svdup_n_u64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svdup_n_z(simde_svbool_t pg,      uint64_t  op) { return simde_svdup_n_u64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svdup_z  (simde_svbool_t pg,      uint64_t  op) { return simde_svdup_n_u64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svdup_n  (                   simde_float32  op) { return simde_svdup_n_f32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svdup    (                   simde_float32  op) { return simde_svdup_n_f32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svdup_n_z(simde_svbool_t pg, simde_float32  op) { return simde_svdup_n_f32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svdup_z  (simde_svbool_t pg, simde_float32  op) { return simde_svdup_n_f32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svdup_n  (                   simde_float64  op) { return simde_svdup_n_f64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svdup    (                   simde_float64  op) { return simde_svdup_n_f64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svdup_n_z(simde_svbool_t pg, simde_float64  op) { return simde_svdup_n_f64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svdup_z  (simde_svbool_t pg, simde_float64  op) { return simde_svdup_n_f64_z (pg, op); }

  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
    SIMDE_FUNCTION_ATTRIBUTES    svint8_t svdup_n  (                    int8_t  op) { return svdup_n_s8    (    op); }
    SIMDE_FUNCTION_ATTRIBUTES    svint8_t svdup    (                    int8_t  op) { return svdup_n_s8    (    op); }
    SIMDE_FUNCTION_ATTRIBUTES    svint8_t svdup_n_z(svbool_t pg,        int8_t  op) { return svdup_n_s8_z  (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES    svint8_t svdup_z  (svbool_t pg,        int8_t  op) { return svdup_n_s8_z  (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint16_t svdup_n  (                   int16_t  op) { return svdup_n_s16   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint16_t svdup    (                   int16_t  op) { return svdup_n_s16   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint16_t svdup_n_z(svbool_t pg,       int16_t  op) { return svdup_n_s16_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint16_t svdup_z  (svbool_t pg,       int16_t  op) { return svdup_n_s16_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint32_t svdup_n  (                   int32_t  op) { return svdup_n_s32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint32_t svdup    (                   int32_t  op) { return svdup_n_s32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint32_t svdup_n_z(svbool_t pg,       int32_t  op) { return svdup_n_s32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint32_t svdup_z  (svbool_t pg,       int32_t  op) { return svdup_n_s32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint64_t svdup_n  (                   int64_t  op) { return svdup_n_s64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint64_t svdup    (                   int64_t  op) { return svdup_n_s64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint64_t svdup_n_z(svbool_t pg,       int64_t  op) { return svdup_n_s64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint64_t svdup_z  (svbool_t pg,       int64_t  op) { return svdup_n_s64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svuint8_t svdup_n  (                   uint8_t  op) { return svdup_n_u8    (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svuint8_t svdup    (                   uint8_t  op) { return svdup_n_u8    (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svuint8_t svdup_n_z(svbool_t pg,       uint8_t  op) { return svdup_n_u8_z  (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svuint8_t svdup_z  (svbool_t pg,       uint8_t  op) { return svdup_n_u8_z  (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint16_t svdup_n  (                  uint16_t  op) { return svdup_n_u16   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint16_t svdup    (                  uint16_t  op) { return svdup_n_u16   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint16_t svdup_n_z(svbool_t pg,      uint16_t  op) { return svdup_n_u16_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint16_t svdup_z  (svbool_t pg,      uint16_t  op) { return svdup_n_u16_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint32_t svdup_n  (                  uint32_t  op) { return svdup_n_u32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint32_t svdup    (                  uint32_t  op) { return svdup_n_u32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint32_t svdup_n_z(svbool_t pg,      uint32_t  op) { return svdup_n_u32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint32_t svdup_z  (svbool_t pg,      uint32_t  op) { return svdup_n_u32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint64_t svdup_n  (                  uint64_t  op) { return svdup_n_u64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint64_t svdup    (                  uint64_t  op) { return svdup_n_u64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint64_t svdup_n_z(svbool_t pg,      uint64_t  op) { return svdup_n_u64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint64_t svdup_z  (svbool_t pg,      uint64_t  op) { return svdup_n_u64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat32_t svdup_n  (             simde_float32  op) { return svdup_n_f32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat32_t svdup    (             simde_float32  op) { return svdup_n_f32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat32_t svdup_n_z(svbool_t pg, simde_float32  op) { return svdup_n_f32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat32_t svdup_z  (svbool_t pg, simde_float32  op) { return svdup_n_f32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat64_t svdup_n  (             simde_float64  op) { return svdup_n_f64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat64_t svdup    (             simde_float64  op) { return svdup_n_f64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat64_t svdup_n_z(svbool_t pg, simde_float64  op) { return svdup_n_f64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat64_t svdup_z  (svbool_t pg, simde_float64  op) { return svdup_n_f64_z (pg, op); }
  #endif
#elif defined(SIMDE_GENERIC_)
  #define simde_svdup_n(op) \
    (SIMDE_GENERIC_((op), \
         int8_t: simde_svdup_n_s8, \
        int16_t: simde_svdup_n_s16, \
        int32_t: simde_svdup_n_s32, \
        int64_t: simde_svdup_n_s64, \
        uint8_t: simde_svdup_n_u8, \
       uint16_t: simde_svdup_n_u16, \
       uint32_t: simde_svdup_n_u32, \
       uint64_t: simde_svdup_n_u64, \
      float32_t: simde_svdup_n_f32, \
      float64_t: simde_svdup_n_f64)((op)))
  #define simde_svdup(op) simde_svdup_n((op))

  #define simde_svdup_n_z(pg, op) \
    (SIMDE_GENERIC_((op), \
         int8_t: simde_svdup_n_s8_z, \
        int16_t: simde_svdup_n_s16_z, \
        int32_t: simde_svdup_n_s32_z, \
        int64_t: simde_svdup_n_s64_z, \
        uint8_t: simde_svdup_n_s8_z, \
       uint16_t: simde_svdup_n_u16_z, \
       uint32_t: simde_svdup_n_u32_z, \
       uint64_t: simde_svdup_n_u64_z, \
      float32_t: simde_svdup_n_u32_z, \
      float64_t: simde_svdup_n_f64_z)((pg), (op)))
  #define simde_svdup_z(pg, op) simde_svdup_n_z((pg), (op))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svdup
  #undef svdup_z
  #undef svdup_n
  #undef svdup_n_z
  #define svdup_n(op) simde_svdup_n((op))
  #define svdup_n_z(pg, op) simde_svdup_n_z((pg), (op))
  #define svdup(op) simde_svdup((op))
  #define svdup_z(pg, op) simde_svdup_z((pg), (op))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_DUP_H */
