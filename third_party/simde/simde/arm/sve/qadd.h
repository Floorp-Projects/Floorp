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

#if !defined(SIMDE_ARM_SVE_QADD_H)
#define SIMDE_ARM_SVE_QADD_H

#include "types.h"
#include "sel.h"
#include "dup.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svqadd_s8(simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_s8(op1, op2);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_s8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_adds_epi8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_adds_epi8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_adds_epi8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_add_sat(op1.v128, op2.v128);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_i8(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_s8
  #define svqadd_s8(op1, op2) simde_svqadd_s8(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svqadd_n_s8(simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_s8(op1, op2);
  #else
    return simde_svqadd_s8(op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_s8
  #define svqadd_n_s8(op1, op2) simde_svqadd_n_s8(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svqadd_s16(simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_s16(op1, op2);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_s16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_adds_epi16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_adds_epi16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_adds_epi16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_add_sat(op1.v128, op2.v128);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_i16(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_s16
  #define svqadd_s16(op1, op2) simde_svqadd_s16(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svqadd_n_s16(simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_s16(op1, op2);
  #else
    return simde_svqadd_s16(op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_s16
  #define svqadd_n_s16(op1, op2) simde_svqadd_n_s16(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svqadd_s32(simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_s32(op1, op2);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_s32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512VL_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm512_cvtsepi64_epi32(_mm512_add_epi64(_mm512_cvtepi32_epi64(op1.m256i[i]), _mm512_cvtepi32_epi64(op2.m256i[i])));
      }
    #elif defined(SIMDE_X86_AVX512VL_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm256_cvtsepi64_epi32(_mm256_add_epi64(_mm256_cvtepi32_epi64(op1.m128i[i]), _mm256_cvtepi32_epi64(op2.m128i[i])));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_i32(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_s32
  #define svqadd_s32(op1, op2) simde_svqadd_s32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svqadd_n_s32(simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_s32(op1, op2);
  #else
    return simde_svqadd_s32(op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_s32
  #define svqadd_n_s32(op1, op2) simde_svqadd_n_s32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svqadd_s64(simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_s64(op1, op2);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_s64(op1.neon, op2.neon);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_i64(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_s64
  #define svqadd_s64(op1, op2) simde_svqadd_s64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svqadd_n_s64(simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_s64(op1, op2);
  #else
    return simde_svqadd_s64(op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_s64
  #define svqadd_n_s64(op1, op2) simde_svqadd_n_s64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svqadd_u8(simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_u8(op1, op2);
  #else
    simde_svuint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_u8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_adds_epu8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_adds_epu8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_adds_epu8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_u8x16_add_sat(op1.v128, op2.v128);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_u8(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_u8
  #define svqadd_u8(op1, op2) simde_svqadd_u8(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svqadd_n_u8(simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_u8(op1, op2);
  #else
    return simde_svqadd_u8(op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_u8
  #define svqadd_n_u8(op1, op2) simde_svqadd_n_u8(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svqadd_u16(simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_u16(op1, op2);
  #else
    simde_svuint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_u16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_adds_epu16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_adds_epu16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_adds_epu16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_u16x8_add_sat(op1.v128, op2.v128);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_u16(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_u16
  #define svqadd_u16(op1, op2) simde_svqadd_u16(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svqadd_n_u16(simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_u16(op1, op2);
  #else
    return simde_svqadd_u16(op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_u16
  #define svqadd_n_u16(op1, op2) simde_svqadd_n_u16(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svqadd_u32(simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_u32(op1, op2);
  #else
    simde_svuint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_u32(op1.neon, op2.neon);
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_u32(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_u32
  #define svqadd_u32(op1, op2) simde_svqadd_u32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svqadd_n_u32(simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_u32(op1, op2);
  #else
    return simde_svqadd_u32(op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_u32
  #define svqadd_n_u32(op1, op2) simde_svqadd_n_u32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svqadd_u64(simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_u64(op1, op2);
  #else
    simde_svuint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_u64(op1.neon, op2.neon);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_u64(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_u64
  #define svqadd_u64(op1, op2) simde_svqadd_u64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svqadd_n_u64(simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_u64(op1, op2);
  #else
    return simde_svqadd_u64(op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_u64
  #define svqadd_n_u64(op1, op2) simde_svqadd_n_u64(op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svqadd(   simde_svint8_t op1,    simde_svint8_t op2) { return simde_svqadd_s8   (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svqadd(  simde_svint16_t op1,   simde_svint16_t op2) { return simde_svqadd_s16  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svqadd(  simde_svint32_t op1,   simde_svint32_t op2) { return simde_svqadd_s32  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svqadd(  simde_svint64_t op1,   simde_svint64_t op2) { return simde_svqadd_s64  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svqadd(  simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svqadd_u8   (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svqadd( simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svqadd_u16  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svqadd( simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svqadd_u32  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svqadd( simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svqadd_u64  (op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svqadd(   simde_svint8_t op1,            int8_t op2) { return simde_svqadd_n_s8 (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svqadd(  simde_svint16_t op1,           int16_t op2) { return simde_svqadd_n_s16(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svqadd(  simde_svint32_t op1,           int32_t op2) { return simde_svqadd_n_s32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svqadd(  simde_svint64_t op1,           int64_t op2) { return simde_svqadd_n_s64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svqadd(  simde_svuint8_t op1,           uint8_t op2) { return simde_svqadd_n_u8 (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svqadd( simde_svuint16_t op1,          uint16_t op2) { return simde_svqadd_n_u16(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svqadd( simde_svuint32_t op1,          uint32_t op2) { return simde_svqadd_n_u32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svqadd( simde_svuint64_t op1,          uint64_t op2) { return simde_svqadd_n_u64(op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svqadd_x(op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svqadd_s8, \
        simde_svint16_t: simde_svqadd_s16, \
        simde_svint32_t: simde_svqadd_s32, \
        simde_svint64_t: simde_svqadd_s64, \
        simde_svuint8_t: simde_svqadd_u8, \
       simde_svuint16_t: simde_svqadd_u16, \
       simde_svuint32_t: simde_svqadd_u32, \
       simde_svuint64_t: simde_svqadd_u64, \
                 int8_t: simde_svqadd_n_s8, \
                int16_t: simde_svqadd_n_s16, \
                int32_t: simde_svqadd_n_s32, \
                int64_t: simde_svqadd_n_s64, \
                uint8_t: simde_svqadd_n_u8, \
               uint16_t: simde_svqadd_n_u16, \
               uint32_t: simde_svqadd_n_u32, \
               uint64_t: simde_svqadd_n_u64)((pg), (op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svqadd
  #define svqadd(op1, op2) simde_svqadd((pg), (op1), (op2))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_QADD_H */
