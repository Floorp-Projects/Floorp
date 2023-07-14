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

#if !defined(SIMDE_ARM_SVE_SEL_H)
#define SIMDE_ARM_SVE_SEL_H

#include "types.h"
#include "reinterpret.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_x_svsel_s8_z(simde_svbool_t pg, simde_svint8_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s8_z(pg, op1, op1);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s8(pg.neon_i8, op1.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_mov_epi8(simde_svbool_to_mmask64(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_mov_epi8(simde_svbool_to_mmask32(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(pg.m256i[i], op1.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], op1.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(pg.altivec_b8, op1.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = pg.values_i8 & op1.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, op1.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = pg.values_i8 & op1.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = pg.values_i8[i] & op1.values[i];
      }
    #endif

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsel_s8(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_s8(pg, op1, op2);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vbslq_s8(pg.neon_u8, op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_mask_mov_epi8(op2.m512i, simde_svbool_to_mmask64(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_mask_mov_epi8(op2.m256i[0], simde_svbool_to_mmask32(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_blendv_epi8(op2.m256i[i], op1.m256i[i], pg.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_blendv_epi8(op2.m128i[i], op1.m128i[i], pg.m128i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_or_si128(_mm_and_si128(pg.m128i[i], op1.m128i[i]), _mm_andnot_si128(pg.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_sel(op2.altivec, op1.altivec, pg.altivec_b8);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_bitselect(op1.v128, op2.v128, pg.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = (pg.values_i8 & op1.values) | (~pg.values_i8 & op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = (pg.values_i8[i] & op1.values[i]) | (~pg.values_i8[i] & op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_s8
  #define svsel_s8(pg, op1, op2) simde_svsel_s8(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_x_svsel_s16_z(simde_svbool_t pg, simde_svint16_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s16_z(pg, op1, op1);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s16(pg.neon_i16, op1.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_mov_epi16(simde_svbool_to_mmask32(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_mov_epi16(simde_svbool_to_mmask16(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(pg.m256i[i], op1.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], op1.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(pg.altivec_b16, op1.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = pg.values_i16 & op1.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, op1.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = pg.values_i16 & op1.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = pg.values_i16[i] & op1.values[i];
      }
    #endif

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsel_s16(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_s16(pg, op1, op2);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vbslq_s16(pg.neon_u16, op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_mask_mov_epi16(op2.m512i, simde_svbool_to_mmask32(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_mask_mov_epi16(op2.m256i[0], simde_svbool_to_mmask16(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_blendv_epi8(op2.m256i[i], op1.m256i[i], pg.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_blendv_epi8(op2.m128i[i], op1.m128i[i], pg.m128i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_or_si128(_mm_and_si128(pg.m128i[i], op1.m128i[i]), _mm_andnot_si128(pg.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_sel(op2.altivec, op1.altivec, pg.altivec_b16);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_bitselect(op1.v128, op2.v128, pg.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = (pg.values_i16 & op1.values) | (~pg.values_i16 & op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = (pg.values_i16[i] & op1.values[i]) | (~pg.values_i16[i] & op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_s16
  #define svsel_s16(pg, op1, op2) simde_svsel_s16(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_x_svsel_s32_z(simde_svbool_t pg, simde_svint32_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s32_z(pg, op1, op1);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s32(pg.neon_i32, op1.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_mov_epi32(simde_svbool_to_mmask16(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_mov_epi32(simde_svbool_to_mmask8(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(pg.m256i[i], op1.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], op1.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(pg.altivec_b32, op1.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = pg.values_i32 & op1.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, op1.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = pg.values_i32 & op1.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = pg.values_i32[i] & op1.values[i];
      }
    #endif

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsel_s32(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_s32(pg, op1, op2);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vbslq_s32(pg.neon_u32, op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_mask_mov_epi32(op2.m512i, simde_svbool_to_mmask16(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_mask_mov_epi32(op2.m256i[0], simde_svbool_to_mmask8(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_blendv_epi8(op2.m256i[i], op1.m256i[i], pg.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_blendv_epi8(op2.m128i[i], op1.m128i[i], pg.m128i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_or_si128(_mm_and_si128(pg.m128i[i], op1.m128i[i]), _mm_andnot_si128(pg.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_sel(op2.altivec, op1.altivec, pg.altivec_b32);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_bitselect(op1.v128, op2.v128, pg.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = (pg.values_i32 & op1.values) | (~pg.values_i32 & op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = (pg.values_i32[i] & op1.values[i]) | (~pg.values_i32[i] & op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_s32
  #define svsel_s32(pg, op1, op2) simde_svsel_s32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_x_svsel_s64_z(simde_svbool_t pg, simde_svint64_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s64_z(pg, op1, op1);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s64(pg.neon_i64, op1.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_mov_epi64(simde_svbool_to_mmask8(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_mov_epi64(simde_svbool_to_mmask4(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(pg.m256i[i], op1.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], op1.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r.altivec = vec_and(pg.altivec_b64, op1.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = HEDLEY_REINTERPRET_CAST(__typeof__(op1.altivec), pg.values_i64) & op1.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, op1.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = pg.values_i64 & op1.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = pg.values_i64[i] & op1.values[i];
      }
    #endif

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsel_s64(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_s64(pg, op1, op2);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vbslq_s64(pg.neon_u64, op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_mask_mov_epi64(op2.m512i, simde_svbool_to_mmask8(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_mask_mov_epi64(op2.m256i[0], simde_svbool_to_mmask4(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_blendv_epi8(op2.m256i[i], op1.m256i[i], pg.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_blendv_epi8(op2.m128i[i], op1.m128i[i], pg.m128i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_or_si128(_mm_and_si128(pg.m128i[i], op1.m128i[i]), _mm_andnot_si128(pg.m128i[i], op2.m128i[i]));
      }
    #elif (defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)) && !defined(SIMDE_BUG_CLANG_46770)
      r.altivec = vec_sel(op2.altivec, op1.altivec, pg.altivec_b64);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_bitselect(op1.v128, op2.v128, pg.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = (pg.values_i64 & op1.values) | (~pg.values_i64 & op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = (pg.values_i64[i] & op1.values[i]) | (~pg.values_i64[i] & op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_s64
  #define svsel_s64(pg, op1, op2) simde_svsel_s64(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_x_svsel_u8_z(simde_svbool_t pg, simde_svuint8_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u8_z(pg, op1, op1);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && ((SIMDE_ARM_SVE_VECTOR_SIZE >= 512) || defined(SIMDE_X86_AVX512VL_NATIVE)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svuint8_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r.m512i = _mm512_maskz_mov_epi8(simde_svbool_to_mmask64(pg), op1.m512i);
    #else
      r.m256i[0] = _mm256_maskz_mov_epi8(simde_svbool_to_mmask32(pg), op1.m256i[0]);
    #endif

    return r;
  #else
    return simde_svreinterpret_u8_s8(simde_x_svsel_s8_z(pg, simde_svreinterpret_s8_u8(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsel_u8(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_u8(pg, op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && ((SIMDE_ARM_SVE_VECTOR_SIZE >= 512) || defined(SIMDE_X86_AVX512VL_NATIVE)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svuint8_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r.m512i = _mm512_mask_mov_epi8(op2.m512i, simde_svbool_to_mmask64(pg), op1.m512i);
    #else
      r.m256i[0] = _mm256_mask_mov_epi8(op2.m256i[0], simde_svbool_to_mmask32(pg), op1.m256i[0]);
    #endif

    return r;
  #else
    return simde_svreinterpret_u8_s8(simde_svsel_s8(pg, simde_svreinterpret_s8_u8(op1), simde_svreinterpret_s8_u8(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_u8
  #define svsel_u8(pg, op1, op2) simde_svsel_u8(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_x_svsel_u16_z(simde_svbool_t pg, simde_svuint16_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u16_z(pg, op1, op1);
  #else
    return simde_svreinterpret_u16_s16(simde_x_svsel_s16_z(pg, simde_svreinterpret_s16_u16(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsel_u16(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_u16(pg, op1, op2);
  #else
    return simde_svreinterpret_u16_s16(simde_svsel_s16(pg, simde_svreinterpret_s16_u16(op1), simde_svreinterpret_s16_u16(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_u16
  #define svsel_u16(pg, op1, op2) simde_svsel_u16(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_x_svsel_u32_z(simde_svbool_t pg, simde_svuint32_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u32_z(pg, op1, op1);
  #else
    return simde_svreinterpret_u32_s32(simde_x_svsel_s32_z(pg, simde_svreinterpret_s32_u32(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsel_u32(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_u32(pg, op1, op2);
  #else
    return simde_svreinterpret_u32_s32(simde_svsel_s32(pg, simde_svreinterpret_s32_u32(op1), simde_svreinterpret_s32_u32(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_u32
  #define svsel_u32(pg, op1, op2) simde_svsel_u32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_x_svsel_u64_z(simde_svbool_t pg, simde_svuint64_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u64_z(pg, op1, op1);
  #else
    return simde_svreinterpret_u64_s64(simde_x_svsel_s64_z(pg, simde_svreinterpret_s64_u64(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsel_u64(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_u64(pg, op1, op2);
  #else
    return simde_svreinterpret_u64_s64(simde_svsel_s64(pg, simde_svreinterpret_s64_u64(op1), simde_svreinterpret_s64_u64(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_u64
  #define svsel_u64(pg, op1, op2) simde_svsel_u64(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_x_svsel_f32_z(simde_svbool_t pg, simde_svfloat32_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return simde_svreinterpret_f32_s32(svand_s32_z(pg, simde_svreinterpret_s32_f32(op1), simde_svreinterpret_s32_f32(op1)));
  #else
    return simde_svreinterpret_f32_s32(simde_x_svsel_s32_z(pg, simde_svreinterpret_s32_f32(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsel_f32(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_f32(pg, op1, op2);
  #else
    return simde_svreinterpret_f32_s32(simde_svsel_s32(pg, simde_svreinterpret_s32_f32(op1), simde_svreinterpret_s32_f32(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_f32
  #define svsel_f32(pg, op1, op2) simde_svsel_f32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_x_svsel_f64_z(simde_svbool_t pg, simde_svfloat64_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return simde_svreinterpret_f64_s64(svand_s64_z(pg, simde_svreinterpret_s64_f64(op1), simde_svreinterpret_s64_f64(op1)));
  #else
    return simde_svreinterpret_f64_s64(simde_x_svsel_s64_z(pg, simde_svreinterpret_s64_f64(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsel_f64(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_f64(pg, op1, op2);
  #else
    return simde_svreinterpret_f64_s64(simde_svsel_s64(pg, simde_svreinterpret_s64_f64(op1), simde_svreinterpret_s64_f64(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_f64
  #define svsel_f64(pg, op1, op2) simde_svsel_f64(pg, op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_x_svsel_z(simde_svbool_t pg,    simde_svint8_t op1) { return simde_x_svsel_s8_z (pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_x_svsel_z(simde_svbool_t pg,   simde_svint16_t op1) { return simde_x_svsel_s16_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_x_svsel_z(simde_svbool_t pg,   simde_svint32_t op1) { return simde_x_svsel_s32_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_x_svsel_z(simde_svbool_t pg,   simde_svint64_t op1) { return simde_x_svsel_s64_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_x_svsel_z(simde_svbool_t pg,   simde_svuint8_t op1) { return simde_x_svsel_u8_z (pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_x_svsel_z(simde_svbool_t pg,  simde_svuint16_t op1) { return simde_x_svsel_u16_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_x_svsel_z(simde_svbool_t pg,  simde_svuint32_t op1) { return simde_x_svsel_u32_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_x_svsel_z(simde_svbool_t pg,  simde_svuint64_t op1) { return simde_x_svsel_u64_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_x_svsel_z(simde_svbool_t pg, simde_svfloat32_t op1) { return simde_x_svsel_f32_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_x_svsel_z(simde_svbool_t pg, simde_svfloat64_t op1) { return simde_x_svsel_f64_z(pg, op1); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsel(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svsel_s8 (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsel(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svsel_s16(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsel(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svsel_s32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsel(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svsel_s64(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsel(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svsel_u8 (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsel(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svsel_u16(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsel(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svsel_u32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsel(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svsel_u64(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsel(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svsel_f32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsel(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svsel_f64(pg, op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_x_svsel_z(pg, op1) \
    (SIMDE_GENERIC_((op1), \
         simde_svint8_t: simde_x_svsel_s8_z, \
        simde_svint16_t: simde_x_svsel_s16_z, \
        simde_svint32_t: simde_x_svsel_s32_z, \
        simde_svint64_t: simde_x_svsel_s64_z, \
        simde_svuint8_t: simde_x_svsel_u8_z, \
       simde_svuint16_t: simde_x_svsel_u16_z, \
       simde_svuint32_t: simde_x_svsel_u32_z, \
       simde_svuint64_t: simde_x_svsel_u64_z, \
      simde_svfloat32_t: simde_x_svsel_f32_z, \
      simde_svfloat64_t: simde_x_svsel_f64_z)((pg), (op1)))

  #define simde_svsel(pg, op1, op2) \
    (SIMDE_GENERIC_((op1), \
         simde_svint8_t: simde_svsel_s8, \
        simde_svint16_t: simde_svsel_s16, \
        simde_svint32_t: simde_svsel_s32, \
        simde_svint64_t: simde_svsel_s64, \
        simde_svuint8_t: simde_svsel_u8, \
       simde_svuint16_t: simde_svsel_u16, \
       simde_svuint32_t: simde_svsel_u32, \
       simde_svuint64_t: simde_svsel_u64, \
      simde_svfloat32_t: simde_svsel_f32, \
      simde_svfloat64_t: simde_svsel_f64)((pg), (op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svsel
  #define svsel(pg, op1) simde_svsel((pg), (op1))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_SEL_H */
