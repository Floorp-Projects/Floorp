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

#if !defined(SIMDE_ARM_SVE_SUB_H)
#define SIMDE_ARM_SVE_SUB_H

#include "types.h"
#include "sel.h"
#include "dup.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_s8_x(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s8_x(pg, op1, op2);
  #else
    simde_svint8_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_s8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi8(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s8_x
  #define svsub_s8_x(pg, op1, op2) simde_svsub_s8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_s8_z(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s8_z(pg, op1, op2);
  #else
    return simde_x_svsel_s8_z(pg, simde_svsub_s8_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s8_z
  #define svsub_s8_z(pg, op1, op2) simde_svsub_s8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_s8_m(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s8_m(pg, op1, op2);
  #else
    return simde_svsel_s8(pg, simde_svsub_s8_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s8_m
  #define svsub_s8_m(pg, op1, op2) simde_svsub_s8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_n_s8_x(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s8_x(pg, op1, op2);
  #else
    return simde_svsub_s8_x(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s8_x
  #define svsub_n_s8_x(pg, op1, op2) simde_svsub_n_s8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_n_s8_z(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s8_z(pg, op1, op2);
  #else
    return simde_svsub_s8_z(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s8_z
  #define svsub_n_s8_z(pg, op1, op2) simde_svsub_n_s8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_n_s8_m(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s8_m(pg, op1, op2);
  #else
    return simde_svsub_s8_m(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s8_m
  #define svsub_n_s8_m(pg, op1, op2) simde_svsub_n_s8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_s16_x(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s16_x(pg, op1, op2);
  #else
    simde_svint16_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_s16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi16(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s16_x
  #define svsub_s16_x(pg, op1, op2) simde_svsub_s16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_s16_z(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s16_z(pg, op1, op2);
  #else
    return simde_x_svsel_s16_z(pg, simde_svsub_s16_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s16_z
  #define svsub_s16_z(pg, op1, op2) simde_svsub_s16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_s16_m(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s16_m(pg, op1, op2);
  #else
    return simde_svsel_s16(pg, simde_svsub_s16_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s16_m
  #define svsub_s16_m(pg, op1, op2) simde_svsub_s16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_n_s16_x(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s16_x(pg, op1, op2);
  #else
    return simde_svsub_s16_x(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s16_x
  #define svsub_n_s16_x(pg, op1, op2) simde_svsub_n_s16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_n_s16_z(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s16_z(pg, op1, op2);
  #else
    return simde_svsub_s16_z(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s16_z
  #define svsub_n_s16_z(pg, op1, op2) simde_svsub_n_s16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_n_s16_m(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s16_m(pg, op1, op2);
  #else
    return simde_svsub_s16_m(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s16_m
  #define svsub_n_s16_m(pg, op1, op2) simde_svsub_n_s16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_s32_x(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s32_x(pg, op1, op2);
  #else
    simde_svint32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_s32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi32(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi32(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi32(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi32(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s32_x
  #define svsub_s32_x(pg, op1, op2) simde_svsub_s32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_s32_z(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s32_z(pg, op1, op2);
  #else
    return simde_x_svsel_s32_z(pg, simde_svsub_s32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s32_z
  #define svsub_s32_z(pg, op1, op2) simde_svsub_s32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_s32_m(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s32_m(pg, op1, op2);
  #else
    return simde_svsel_s32(pg, simde_svsub_s32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s32_m
  #define svsub_s32_m(pg, op1, op2) simde_svsub_s32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_n_s32_x(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s32_x(pg, op1, op2);
  #else
    return simde_svsub_s32_x(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s32_x
  #define svsub_n_s32_x(pg, op1, op2) simde_svsub_n_s32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_n_s32_z(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s32_z(pg, op1, op2);
  #else
    return simde_svsub_s32_z(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s32_z
  #define svsub_n_s32_z(pg, op1, op2) simde_svsub_n_s32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_n_s32_m(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s32_m(pg, op1, op2);
  #else
    return simde_svsub_s32_m(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s32_m
  #define svsub_n_s32_m(pg, op1, op2) simde_svsub_n_s32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_s64_x(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s64_x(pg, op1, op2);
  #else
    simde_svint64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_s64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi64(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi64(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi64(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi64(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s64_x
  #define svsub_s64_x(pg, op1, op2) simde_svsub_s64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_s64_z(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s64_z(pg, op1, op2);
  #else
    return simde_x_svsel_s64_z(pg, simde_svsub_s64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s64_z
  #define svsub_s64_z(pg, op1, op2) simde_svsub_s64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_s64_m(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s64_m(pg, op1, op2);
  #else
    return simde_svsel_s64(pg, simde_svsub_s64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s64_m
  #define svsub_s64_m(pg, op1, op2) simde_svsub_s64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_n_s64_x(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s64_x(pg, op1, op2);
  #else
    return simde_svsub_s64_x(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s64_x
  #define svsub_n_s64_x(pg, op1, op2) simde_svsub_n_s64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_n_s64_z(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s64_z(pg, op1, op2);
  #else
    return simde_svsub_s64_z(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s64_z
  #define svsub_n_s64_z(pg, op1, op2) simde_svsub_n_s64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_n_s64_m(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s64_m(pg, op1, op2);
  #else
    return simde_svsub_s64_m(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s64_m
  #define svsub_n_s64_m(pg, op1, op2) simde_svsub_n_s64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_u8_x(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u8_x(pg, op1, op2);
  #else
    simde_svuint8_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_u8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi8(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u8_x
  #define svsub_u8_x(pg, op1, op2) simde_svsub_u8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_u8_z(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u8_z(pg, op1, op2);
  #else
    return simde_x_svsel_u8_z(pg, simde_svsub_u8_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u8_z
  #define svsub_u8_z(pg, op1, op2) simde_svsub_u8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_u8_m(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u8_m(pg, op1, op2);
  #else
    return simde_svsel_u8(pg, simde_svsub_u8_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u8_m
  #define svsub_u8_m(pg, op1, op2) simde_svsub_u8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_n_u8_x(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u8_x(pg, op1, op2);
  #else
    return simde_svsub_u8_x(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u8_x
  #define svsub_n_u8_x(pg, op1, op2) simde_svsub_n_u8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_n_u8_z(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u8_z(pg, op1, op2);
  #else
    return simde_svsub_u8_z(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u8_z
  #define svsub_n_u8_z(pg, op1, op2) simde_svsub_n_u8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_n_u8_m(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u8_m(pg, op1, op2);
  #else
    return simde_svsub_u8_m(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u8_m
  #define svsub_n_u8_m(pg, op1, op2) simde_svsub_n_u8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_u16_x(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u16_x(pg, op1, op2);
  #else
    simde_svuint16_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_u16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi16(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u16_x
  #define svsub_u16_x(pg, op1, op2) simde_svsub_u16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_u16_z(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u16_z(pg, op1, op2);
  #else
    return simde_x_svsel_u16_z(pg, simde_svsub_u16_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u16_z
  #define svsub_u16_z(pg, op1, op2) simde_svsub_u16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_u16_m(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u16_m(pg, op1, op2);
  #else
    return simde_svsel_u16(pg, simde_svsub_u16_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u16_m
  #define svsub_u16_m(pg, op1, op2) simde_svsub_u16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_n_u16_x(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u16_x(pg, op1, op2);
  #else
    return simde_svsub_u16_x(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u16_x
  #define svsub_n_u16_x(pg, op1, op2) simde_svsub_n_u16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_n_u16_z(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u16_z(pg, op1, op2);
  #else
    return simde_svsub_u16_z(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u16_z
  #define svsub_n_u16_z(pg, op1, op2) simde_svsub_n_u16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_n_u16_m(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u16_m(pg, op1, op2);
  #else
    return simde_svsub_u16_m(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u16_m
  #define svsub_n_u16_m(pg, op1, op2) simde_svsub_n_u16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_u32_x(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u32_x(pg, op1, op2);
  #else
    simde_svuint32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_u32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi32(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi32(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi32(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi32(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u32_x
  #define svsub_u32_x(pg, op1, op2) simde_svsub_u32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_u32_z(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u32_z(pg, op1, op2);
  #else
    return simde_x_svsel_u32_z(pg, simde_svsub_u32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u32_z
  #define svsub_u32_z(pg, op1, op2) simde_svsub_u32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_u32_m(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u32_m(pg, op1, op2);
  #else
    return simde_svsel_u32(pg, simde_svsub_u32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u32_m
  #define svsub_u32_m(pg, op1, op2) simde_svsub_u32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_n_u32_x(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u32_x(pg, op1, op2);
  #else
    return simde_svsub_u32_x(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u32_x
  #define svsub_n_u32_x(pg, op1, op2) simde_svsub_n_u32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_n_u32_z(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u32_z(pg, op1, op2);
  #else
    return simde_svsub_u32_z(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u32_z
  #define svsub_n_u32_z(pg, op1, op2) simde_svsub_n_u32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_n_u32_m(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u32_m(pg, op1, op2);
  #else
    return simde_svsub_u32_m(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u32_m
  #define svsub_n_u32_m(pg, op1, op2) simde_svsub_n_u32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_u64_x(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u64_x(pg, op1, op2);
  #else
    simde_svuint64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_u64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi64(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi64(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi64(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi64(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u64_x
  #define svsub_u64_x(pg, op1, op2) simde_svsub_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_u64_z(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u64_z(pg, op1, op2);
  #else
    return simde_x_svsel_u64_z(pg, simde_svsub_u64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u64_z
  #define svsub_u64_z(pg, op1, op2) simde_svsub_u64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_u64_m(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u64_m(pg, op1, op2);
  #else
    return simde_svsel_u64(pg, simde_svsub_u64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u64_m
  #define svsub_u64_m(pg, op1, op2) simde_svsub_u64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_n_u64_x(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u64_x(pg, op1, op2);
  #else
    return simde_svsub_u64_x(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u64_x
  #define svsub_n_u64_x(pg, op1, op2) simde_svsub_n_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_n_u64_z(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u64_z(pg, op1, op2);
  #else
    return simde_svsub_u64_z(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u64_z
  #define svsub_n_u64_z(pg, op1, op2) simde_svsub_n_u64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_n_u64_m(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u64_m(pg, op1, op2);
  #else
    return simde_svsub_u64_m(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u64_m
  #define svsub_n_u64_m(pg, op1, op2) simde_svsub_n_u64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_f32_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f32_x(pg, op1, op2);
  #else
    simde_svfloat32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_f32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512 = _mm512_sub_ps(op1.m512, op2.m512);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256[0] = _mm256_sub_ps(op1.m256[0], op2.m256[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256) / sizeof(r.m256[0])) ; i++) {
        r.m256[i] = _mm256_sub_ps(op1.m256[i], op2.m256[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128) / sizeof(r.m128[0])) ; i++) {
        r.m128[i] = _mm_sub_ps(op1.m128[i], op2.m128[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f32x4_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f32_x
  #define svsub_f32_x(pg, op1, op2) simde_svsub_f32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_f32_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f32_z(pg, op1, op2);
  #else
    return simde_x_svsel_f32_z(pg, simde_svsub_f32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f32_z
  #define svsub_f32_z(pg, op1, op2) simde_svsub_f32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_f32_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f32_m(pg, op1, op2);
  #else
    return simde_svsel_f32(pg, simde_svsub_f32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f32_m
  #define svsub_f32_m(pg, op1, op2) simde_svsub_f32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_n_f32_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f32_x(pg, op1, op2);
  #else
    return simde_svsub_f32_x(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f32_x
  #define svsub_n_f32_x(pg, op1, op2) simde_svsub_n_f32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_n_f32_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f32_z(pg, op1, op2);
  #else
    return simde_svsub_f32_z(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f32_z
  #define svsub_n_f32_z(pg, op1, op2) simde_svsub_n_f32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_n_f32_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f32_m(pg, op1, op2);
  #else
    return simde_svsub_f32_m(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f32_m
  #define svsub_n_f32_m(pg, op1, op2) simde_svsub_n_f32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_f64_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f64_x(pg, op1, op2);
  #else
    simde_svfloat64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r.neon = vsubq_f64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512d = _mm512_sub_pd(op1.m512d, op2.m512d);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256d[0] = _mm256_sub_pd(op1.m256d[0], op2.m256d[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256d) / sizeof(r.m256d[0])) ; i++) {
        r.m256d[i] = _mm256_sub_pd(op1.m256d[i], op2.m256d[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128d) / sizeof(r.m128d[0])) ; i++) {
        r.m128d[i] = _mm_sub_pd(op1.m128d[i], op2.m128d[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f64x2_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f64_x
  #define svsub_f64_x(pg, op1, op2) simde_svsub_f64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_f64_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f64_z(pg, op1, op2);
  #else
    return simde_x_svsel_f64_z(pg, simde_svsub_f64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f64_z
  #define svsub_f64_z(pg, op1, op2) simde_svsub_f64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_f64_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f64_m(pg, op1, op2);
  #else
    return simde_svsel_f64(pg, simde_svsub_f64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f64_m
  #define svsub_f64_m(pg, op1, op2) simde_svsub_f64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_n_f64_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f64_x(pg, op1, op2);
  #else
    return simde_svsub_f64_x(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f64_x
  #define svsub_n_f64_x(pg, op1, op2) simde_svsub_n_f64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_n_f64_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f64_z(pg, op1, op2);
  #else
    return simde_svsub_f64_z(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f64_z
  #define svsub_n_f64_z(pg, op1, op2) simde_svsub_n_f64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_n_f64_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f64_m(pg, op1, op2);
  #else
    return simde_svsub_f64_m(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f64_m
  #define svsub_n_f64_m(pg, op1, op2) simde_svsub_n_f64_m(pg, op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_x(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svsub_s8_x   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_x(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svsub_s16_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_x(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svsub_s32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_x(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svsub_s64_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_x(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svsub_u8_x   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_x(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svsub_u16_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_x(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svsub_u32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_x(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svsub_u64_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svsub_f32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svsub_f64_x  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_z(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svsub_s8_z   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_z(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svsub_s16_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_z(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svsub_s32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_z(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svsub_s64_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_z(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svsub_u8_z   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_z(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svsub_u16_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_z(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svsub_u32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_z(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svsub_u64_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svsub_f32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svsub_f64_z  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_m(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svsub_s8_m   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_m(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svsub_s16_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_m(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svsub_s32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_m(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svsub_s64_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_m(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svsub_u8_m   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_m(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svsub_u16_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_m(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svsub_u32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_m(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svsub_u64_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svsub_f32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svsub_f64_m  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_x(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svsub_n_s8_x (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_x(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svsub_n_s16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_x(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svsub_n_s32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_x(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svsub_n_s64_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_x(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svsub_n_u8_x (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_x(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svsub_n_u16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_x(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svsub_n_u32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_x(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svsub_n_u64_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_x(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svsub_n_f32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_x(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svsub_n_f64_x(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_z(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svsub_n_s8_z (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_z(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svsub_n_s16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_z(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svsub_n_s32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_z(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svsub_n_s64_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_z(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svsub_n_u8_z (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_z(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svsub_n_u16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_z(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svsub_n_u32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_z(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svsub_n_u64_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_z(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svsub_n_f32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_z(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svsub_n_f64_z(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_m(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svsub_n_s8_m (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_m(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svsub_n_s16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_m(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svsub_n_s32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_m(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svsub_n_s64_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_m(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svsub_n_u8_m (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_m(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svsub_n_u16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_m(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svsub_n_u32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_m(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svsub_n_u64_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_m(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svsub_n_f32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_m(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svsub_n_f64_m(pg, op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svsub_x(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svsub_s8_x, \
        simde_svint16_t: simde_svsub_s16_x, \
        simde_svint32_t: simde_svsub_s32_x, \
        simde_svint64_t: simde_svsub_s64_x, \
        simde_svuint8_t: simde_svsub_u8_x, \
       simde_svuint16_t: simde_svsub_u16_x, \
       simde_svuint32_t: simde_svsub_u32_x, \
       simde_svuint64_t: simde_svsub_u64_x, \
      simde_svfloat32_t: simde_svsub_f32_x, \
      simde_svfloat64_t: simde_svsub_f64_x, \
                 int8_t: simde_svsub_n_s8_x, \
                int16_t: simde_svsub_n_s16_x, \
                int32_t: simde_svsub_n_s32_x, \
                int64_t: simde_svsub_n_s64_x, \
                uint8_t: simde_svsub_n_u8_x, \
               uint16_t: simde_svsub_n_u16_x, \
               uint32_t: simde_svsub_n_u32_x, \
               uint64_t: simde_svsub_n_u64_x, \
          simde_float32: simde_svsub_n_f32_x, \
          simde_float64: simde_svsub_n_f64_x)((pg), (op1), (op2)))

  #define simde_svsub_z(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svsub_s8_z, \
        simde_svint16_t: simde_svsub_s16_z, \
        simde_svint32_t: simde_svsub_s32_z, \
        simde_svint64_t: simde_svsub_s64_z, \
        simde_svuint8_t: simde_svsub_u8_z, \
       simde_svuint16_t: simde_svsub_u16_z, \
       simde_svuint32_t: simde_svsub_u32_z, \
       simde_svuint64_t: simde_svsub_u64_z, \
      simde_svfloat32_t: simde_svsub_f32_z, \
      simde_svfloat64_t: simde_svsub_f64_z, \
                 int8_t: simde_svsub_n_s8_z, \
                int16_t: simde_svsub_n_s16_z, \
                int32_t: simde_svsub_n_s32_z, \
                int64_t: simde_svsub_n_s64_z, \
                uint8_t: simde_svsub_n_u8_z, \
               uint16_t: simde_svsub_n_u16_z, \
               uint32_t: simde_svsub_n_u32_z, \
               uint64_t: simde_svsub_n_u64_z, \
          simde_float32: simde_svsub_n_f32_z, \
          simde_float64: simde_svsub_n_f64_z)((pg), (op1), (op2)))

  #define simde_svsub_m(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svsub_s8_m, \
        simde_svint16_t: simde_svsub_s16_m, \
        simde_svint32_t: simde_svsub_s32_m, \
        simde_svint64_t: simde_svsub_s64_m, \
        simde_svuint8_t: simde_svsub_u8_m, \
       simde_svuint16_t: simde_svsub_u16_m, \
       simde_svuint32_t: simde_svsub_u32_m, \
       simde_svuint64_t: simde_svsub_u64_m, \
      simde_svfloat32_t: simde_svsub_f32_m, \
      simde_svfloat64_t: simde_svsub_f64_m, \
                 int8_t: simde_svsub_n_s8_m, \
                int16_t: simde_svsub_n_s16_m, \
                int32_t: simde_svsub_n_s32_m, \
                int64_t: simde_svsub_n_s64_m, \
                uint8_t: simde_svsub_n_u8_m, \
               uint16_t: simde_svsub_n_u16_m, \
               uint32_t: simde_svsub_n_u32_m, \
               uint64_t: simde_svsub_n_u64_m, \
          simde_float32: simde_svsub_n_f32_m, \
          simde_float64: simde_svsub_n_f64_m)((pg), (op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svsub_x
  #undef svsub_z
  #undef svsub_m
  #undef svsub_n_x
  #undef svsub_n_z
  #undef svsub_n_m
  #define svsub_x(pg, op1, op2) simde_svsub_x((pg), (op1), (op2))
  #define svsub_z(pg, op1, op2) simde_svsub_z((pg), (op1), (op2))
  #define svsub_m(pg, op1, op2) simde_svsub_m((pg), (op1), (op2))
  #define svsub_n_x(pg, op1, op2) simde_svsub_n_x((pg), (op1), (op2))
  #define svsub_n_z(pg, op1, op2) simde_svsub_n_z((pg), (op1), (op2))
  #define svsub_n_m(pg, op1, op2) simde_svsub_n_m((pg), (op1), (op2))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_SUB_H */
