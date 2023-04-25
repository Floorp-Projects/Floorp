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

#if !defined(SIMDE_ARM_SVE_ST1_H)
#define SIMDE_ARM_SVE_ST1_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_s8(simde_svbool_t pg, int8_t * base, simde_svint8_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_s8(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
    _mm512_mask_storeu_epi8(base, simde_svbool_to_mmask64(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    _mm256_mask_storeu_epi8(base, simde_svbool_to_mmask32(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      if (pg.values_i8[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_s8
  #define svst1_s8(pg, base, data) simde_svst1_s8((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_s16(simde_svbool_t pg, int16_t * base, simde_svint16_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_s16(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_epi16(base, simde_svbool_to_mmask32(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_epi16(base, simde_svbool_to_mmask16(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      if (pg.values_i16[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_s16
  #define svst1_s16(pg, base, data) simde_svst1_s16((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_s32(simde_svbool_t pg, int32_t * base, simde_svint32_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_s32(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_epi32(base, simde_svbool_to_mmask16(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_epi32(base, simde_svbool_to_mmask8(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      if (pg.values_i32[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_s32
  #define svst1_s32(pg, base, data) simde_svst1_s32((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_s64(simde_svbool_t pg, int64_t * base, simde_svint64_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_s64(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_epi64(base, simde_svbool_to_mmask8(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_epi64(base, simde_svbool_to_mmask4(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      if (pg.values_i64[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_s64
  #define svst1_s64(pg, base, data) simde_svst1_s64((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_u8(simde_svbool_t pg, uint8_t * base, simde_svuint8_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_u8(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_epi8(base, simde_svbool_to_mmask64(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_epi8(base, simde_svbool_to_mmask32(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      if (pg.values_u8[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_u8
  #define svst1_u8(pg, base, data) simde_svst1_u8((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_u16(simde_svbool_t pg, uint16_t * base, simde_svuint16_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_u16(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_epi16(base, simde_svbool_to_mmask32(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_epi16(base, simde_svbool_to_mmask16(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      if (pg.values_u16[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_u16
  #define svst1_u16(pg, base, data) simde_svst1_u16((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_u32(simde_svbool_t pg, uint32_t * base, simde_svuint32_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_u32(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_epi32(base, simde_svbool_to_mmask16(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_epi32(base, simde_svbool_to_mmask8(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      if (pg.values_u32[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_u32
  #define svst1_u32(pg, base, data) simde_svst1_u32((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_u64(simde_svbool_t pg, uint64_t * base, simde_svuint64_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_u64(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_epi64(base, simde_svbool_to_mmask8(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_epi64(base, simde_svbool_to_mmask4(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      if (pg.values_u64[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_u64
  #define svst1_u64(pg, base, data) simde_svst1_u64((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_f32(simde_svbool_t pg, simde_float32 * base, simde_svfloat32_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_f32(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_ps(base, simde_svbool_to_mmask16(pg), data.m512);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_ps(base, simde_svbool_to_mmask8(pg), data.m256[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      if (pg.values_i32[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_f32
  #define svst1_f32(pg, base, data) simde_svst1_f32((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_f64(simde_svbool_t pg, simde_float64 * base, simde_svfloat64_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_f64(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
     _mm512_mask_storeu_pd(base, simde_svbool_to_mmask8(pg), data.m512d);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
     _mm256_mask_storeu_pd(base, simde_svbool_to_mmask4(pg), data.m256d[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      if (pg.values_i64[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_f64
  #define svst1_f64(pg, base, data) simde_svst1_f64((pg), (base), (data))
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,        int8_t * base,    simde_svint8_t data) { simde_svst1_s8 (pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,       int16_t * base,   simde_svint16_t data) { simde_svst1_s16(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,       int32_t * base,   simde_svint32_t data) { simde_svst1_s32(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,       int64_t * base,   simde_svint64_t data) { simde_svst1_s64(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,       uint8_t * base,   simde_svuint8_t data) { simde_svst1_u8 (pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,      uint16_t * base,  simde_svuint16_t data) { simde_svst1_u16(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,      uint32_t * base,  simde_svuint32_t data) { simde_svst1_u32(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,      uint64_t * base,  simde_svuint64_t data) { simde_svst1_u64(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg, simde_float32 * base, simde_svfloat32_t data) { simde_svst1_f32(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg, simde_float64 * base, simde_svfloat64_t data) { simde_svst1_f64(pg, base, data); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svst1(pg, base, data) \
    (SIMDE_GENERIC_((data), \
         simde_svint8_t: simde_svst1_s8 , \
        simde_svint16_t: simde_svst1_s16, \
        simde_svint32_t: simde_svst1_s32, \
        simde_svint64_t: simde_svst1_s64, \
        simde_svuint8_t: simde_svst1_u8 , \
       simde_svuint16_t: simde_svst1_u16, \
       simde_svuint32_t: simde_svst1_u32, \
       simde_svuint64_t: simde_svst1_u64, \
      simde_svfloat32_t: simde_svst1_f32, \
      simde_svfloat64_t: simde_svst1_f64)((pg), (base), (data)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svst1
  #define svst1(pg, base, data) simde_svst1((pg), (base), (data))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_ST1_H */
