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
 *   2020      Himanshi Mathur <himanshi18037@iiitd.ac.in>
 */

#if !defined(SIMDE_X86_AVX512_SET1_H)
#define SIMDE_X86_AVX512_SET1_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_set1_epi8 (int8_t a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_set1_epi8(a);
  #else
    simde__m512i_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
      r_.i8[i] = a;
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set1_epi8
  #define _mm512_set1_epi8(a) simde_mm512_set1_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_set1_epi8(simde__m512i src, simde__mmask64 k, int8_t a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_set1_epi8(src, k, a);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_set1_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_set1_epi8
  #define _mm512_mask_set1_epi8(src, k, a) simde_mm512_mask_set1_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_set1_epi8(simde__mmask64 k, int8_t a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_set1_epi8(k, a);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_set1_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_set1_epi8
  #define _mm512_maskz_set1_epi8(k, a) simde_mm512_maskz_set1_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_set1_epi16 (int16_t a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_set1_epi16(a);
  #else
    simde__m512i_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
      r_.i16[i] = a;
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set1_epi16
  #define _mm512_set1_epi16(a) simde_mm512_set1_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_set1_epi16(simde__m512i src, simde__mmask32 k, int16_t a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_set1_epi16(src, k, a);
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_set1_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_set1_epi16
  #define _mm512_mask_set1_epi16(src, k, a) simde_mm512_mask_set1_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_set1_epi16(simde__mmask32 k, int16_t a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_set1_epi16(k, a);
  #else
    return simde_mm512_maskz_mov_epi16(k, simde_mm512_set1_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_set1_epi16
  #define _mm512_maskz_set1_epi16(k, a) simde_mm512_maskz_set1_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_set1_epi32 (int32_t a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_set1_epi32(a);
  #else
    simde__m512i_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
      r_.i32[i] = a;
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set1_epi32
  #define _mm512_set1_epi32(a) simde_mm512_set1_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_set1_epi32(simde__m512i src, simde__mmask16 k, int32_t a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_set1_epi32(src, k, a);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_set1_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_set1_epi32
  #define _mm512_mask_set1_epi32(src, k, a) simde_mm512_mask_set1_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_set1_epi32(simde__mmask16 k, int32_t a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_set1_epi32(k, a);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_set1_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_set1_epi32
  #define _mm512_maskz_set1_epi32(k, a) simde_mm512_maskz_set1_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_set1_epi64 (int64_t a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_set1_epi64(a);
  #else
    simde__m512i_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
      r_.i64[i] = a;
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set1_epi64
  #define _mm512_set1_epi64(a) simde_mm512_set1_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_set1_epi64(simde__m512i src, simde__mmask8 k, int64_t a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_set1_epi64(src, k, a);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_set1_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_set1_epi64
  #define _mm512_mask_set1_epi64(src, k, a) simde_mm512_mask_set1_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_set1_epi64(simde__mmask8 k, int64_t a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_set1_epi64(k, a);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_set1_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_set1_epi64
  #define _mm512_maskz_set1_epi64(k, a) simde_mm512_maskz_set1_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set1_epu8 (uint8_t a) {
  simde__m512i_private r_;

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.u8) / sizeof(r_.u8[0])) ; i++) {
    r_.u8[i] = a;
  }

  return simde__m512i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set1_epu16 (uint16_t a) {
  simde__m512i_private r_;

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
    r_.u16[i] = a;
  }

  return simde__m512i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set1_epu32 (uint32_t a) {
  simde__m512i_private r_;

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
    r_.u32[i] = a;
  }

  return simde__m512i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set1_epu64 (uint64_t a) {
  simde__m512i_private r_;

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
    r_.u64[i] = a;
  }

  return simde__m512i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_set1_ps (simde_float32 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_set1_ps(a);
  #else
    simde__m512_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
      r_.f32[i] = a;
    }

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set1_ps
  #define _mm512_set1_ps(a) simde_mm512_set1_ps(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_set1_pd (simde_float64 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_set1_pd(a);
  #else
    simde__m512d_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
      r_.f64[i] = a;
    }

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set1_pd
  #define _mm512_set1_pd(a) simde_mm512_set1_pd(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SET1_H) */
