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

/* Note: we don't have vector implementations for most of these because
 * we can't just load everything and mask out the uninteresting bits;
 * that might cause a fault, for example if the end of the buffer buts
 * up against a protected page.
 *
 * One thing we might be able to do would be to check if the predicate
 * is all ones and, if so, use an unpredicated load instruction.  This
 * would probably we worthwhile for smaller types, though perhaps not
 * for larger types since it would mean branching for every load plus
 * the overhead of checking whether all bits are 1. */

#if !defined(SIMDE_ARM_SVE_LD1_H)
#define SIMDE_ARM_SVE_LD1_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svld1_s8(simde_svbool_t pg, const int8_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_s8(pg, base);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_maskz_loadu_epi8(simde_svbool_to_mmask64(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_maskz_loadu_epi8(simde_svbool_to_mmask32(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
        r.values[i] = pg.values_i8[i] ? base[i] : INT8_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_s8
  #define svld1_s8(pg, base) simde_svld1_s8((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svld1_s16(simde_svbool_t pg, const int16_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_s16(pg, base);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_maskz_loadu_epi16(simde_svbool_to_mmask32(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_maskz_loadu_epi16(simde_svbool_to_mmask16(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
        r.values[i] = pg.values_i16[i] ? base[i] : INT16_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_s16
  #define svld1_s16(pg, base) simde_svld1_s16((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svld1_s32(simde_svbool_t pg, const int32_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_s32(pg, base);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_maskz_loadu_epi32(simde_svbool_to_mmask16(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_maskz_loadu_epi32(simde_svbool_to_mmask8(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
        r.values[i] = pg.values_i32[i] ? base[i] : INT32_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_s32
  #define svld1_s32(pg, base) simde_svld1_s32((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svld1_s64(simde_svbool_t pg, const int64_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_s64(pg, base);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_maskz_loadu_epi64(simde_svbool_to_mmask8(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_maskz_loadu_epi64(simde_svbool_to_mmask4(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
        r.values[i] = pg.values_i64[i] ? base[i] : INT64_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_s64
  #define svld1_s64(pg, base) simde_svld1_s64((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svld1_u8(simde_svbool_t pg, const uint8_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_u8(pg, base);
  #else
    simde_svuint8_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_maskz_loadu_epi8(simde_svbool_to_mmask64(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_maskz_loadu_epi8(simde_svbool_to_mmask32(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
        r.values[i] = pg.values_i8[i] ? base[i] : UINT8_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_u8
  #define svld1_u8(pg, base) simde_svld1_u8((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svld1_u16(simde_svbool_t pg, const uint16_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_u16(pg, base);
  #else
    simde_svuint16_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_maskz_loadu_epi16(simde_svbool_to_mmask32(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_maskz_loadu_epi16(simde_svbool_to_mmask16(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
        r.values[i] = pg.values_i16[i] ? base[i] : UINT16_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_u16
  #define svld1_u16(pg, base) simde_svld1_u16((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svld1_u32(simde_svbool_t pg, const uint32_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_u32(pg, base);
  #else
    simde_svuint32_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_maskz_loadu_epi32(simde_svbool_to_mmask16(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_maskz_loadu_epi32(simde_svbool_to_mmask8(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
        r.values[i] = pg.values_i32[i] ? base[i] : UINT32_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_u32
  #define svld1_u32(pg, base) simde_svld1_u32((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svld1_u64(simde_svbool_t pg, const uint64_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_u64(pg, base);
  #else
    simde_svuint64_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_maskz_loadu_epi64(simde_svbool_to_mmask8(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_maskz_loadu_epi64(simde_svbool_to_mmask4(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
        r.values[i] = pg.values_i64[i] ? base[i] : UINT64_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_u64
  #define svld1_u64(pg, base) simde_svld1_u64((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svld1_f32(simde_svbool_t pg, const simde_float32 * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_f32(pg, base);
  #else
    simde_svfloat32_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512 = _mm512_maskz_loadu_ps(simde_svbool_to_mmask16(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256[0] = _mm256_maskz_loadu_ps(simde_svbool_to_mmask8(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
        r.values[i] = pg.values_i32[i] ? base[i] : SIMDE_FLOAT32_C(0.0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_f32
  #define svld1_f32(pg, base) simde_svld1_f32((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svld1_f64(simde_svbool_t pg, const simde_float64 * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_f64(pg, base);
  #else
    simde_svfloat64_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512d = _mm512_maskz_loadu_pd(simde_svbool_to_mmask8(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256d[0] = _mm256_maskz_loadu_pd(simde_svbool_to_mmask4(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
        r.values[i] = pg.values_i64[i] ? base[i] : SIMDE_FLOAT64_C(0.0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_f64
  #define svld1_f64(pg, base) simde_svld1_f64((pg), (base))
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svld1(simde_svbool_t pg, const        int8_t * base) { return simde_svld1_s8 (pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svld1(simde_svbool_t pg, const       int16_t * base) { return simde_svld1_s16(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svld1(simde_svbool_t pg, const       int32_t * base) { return simde_svld1_s32(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svld1(simde_svbool_t pg, const       int64_t * base) { return simde_svld1_s64(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svld1(simde_svbool_t pg, const       uint8_t * base) { return simde_svld1_u8 (pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svld1(simde_svbool_t pg, const      uint16_t * base) { return simde_svld1_u16(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svld1(simde_svbool_t pg, const      uint32_t * base) { return simde_svld1_u32(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svld1(simde_svbool_t pg, const      uint64_t * base) { return simde_svld1_u64(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svld1(simde_svbool_t pg, const simde_float32 * base) { return simde_svld1_f32(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svld1(simde_svbool_t pg, const simde_float64 * base) { return simde_svld1_f64(pg, base); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svld1(pg, base) \
    (SIMDE_GENERIC_((base), \
      const        int8_t *: simde_svld1_s8 , \
      const       int16_t *: simde_svld1_s16, \
      const       int32_t *: simde_svld1_s32, \
      const       int64_t *: simde_svld1_s64, \
      const       uint8_t *: simde_svld1_u8 , \
      const      uint16_t *: simde_svld1_u16, \
      const      uint32_t *: simde_svld1_u32, \
      const      uint64_t *: simde_svld1_u64, \
      const simde_float32 *: simde_svld1_f32, \
      const simde_float64 *: simde_svld1_f64)(pg, base))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svld1
  #define svld1(pg, base) simde_svld1((pg), (base))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_LD1_H */
