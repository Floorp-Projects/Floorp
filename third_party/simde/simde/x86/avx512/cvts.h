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
 *   2020      Hidayat Khan <huk2209@gmail.com>
 */

#if !defined(SIMDE_X86_AVX512_CVTS_H)
#define SIMDE_X86_AVX512_CVTS_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_cvtsepi16_epi8 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cvtsepi16_epi8(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m128i_private a_ = simde__m128i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0])) ; i++) {
      r_.i8[i] =
          (a_.i16[i] < INT8_MIN)
            ? (INT8_MIN)
            : ((a_.i16[i] > INT8_MAX)
              ? (INT8_MAX)
              : HEDLEY_STATIC_CAST(int8_t, a_.i16[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cvtsepi16_epi8
  #define _mm_cvtsepi16_epi8(a) simde_mm_cvtsepi16_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm256_cvtsepi16_epi8 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cvtsepi16_epi8(a);
  #else
    simde__m128i_private r_;
    simde__m256i_private a_ = simde__m256i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
      r_.i8[i] =
          (a_.i16[i] < INT8_MIN)
            ? (INT8_MIN)
            : ((a_.i16[i] > INT8_MAX)
              ? (INT8_MAX)
              : HEDLEY_STATIC_CAST(int8_t, a_.i16[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cvtsepi16_epi8
  #define _mm256_cvtsepi16_epi8(a) simde_mm256_cvtsepi16_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_cvtsepi32_epi8 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cvtsepi32_epi8(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m128i_private a_ = simde__m128i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i8[i] =
          (a_.i32[i] < INT8_MIN)
            ? (INT8_MIN)
            : ((a_.i32[i] > INT8_MAX)
              ? (INT8_MAX)
              : HEDLEY_STATIC_CAST(int8_t, a_.i32[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cvtsepi32_epi8
  #define _mm_cvtsepi32_epi8(a) simde_mm_cvtsepi32_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm256_cvtsepi32_epi8 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cvtsepi32_epi8(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m256i_private a_ = simde__m256i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i8[i] =
          (a_.i32[i] < INT8_MIN)
            ? (INT8_MIN)
            : ((a_.i32[i] > INT8_MAX)
              ? (INT8_MAX)
              : HEDLEY_STATIC_CAST(int8_t, a_.i32[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cvtsepi32_epi8
  #define _mm256_cvtsepi32_epi8(a) simde_mm256_cvtsepi32_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_cvtsepi32_epi16 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cvtsepi32_epi16(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m128i_private a_ = simde__m128i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i16[i] =
          (a_.i32[i] < INT16_MIN)
            ? (INT16_MIN)
            : ((a_.i32[i] > INT16_MAX)
              ? (INT16_MAX)
              : HEDLEY_STATIC_CAST(int16_t, a_.i32[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cvtsepi32_epi16
  #define _mm_cvtsepi32_epi16(a) simde_mm_cvtsepi32_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm256_cvtsepi32_epi16 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cvtsepi32_epi16(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m256i_private a_ = simde__m256i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i16[i] =
          (a_.i32[i] < INT16_MIN)
            ? (INT16_MIN)
            : ((a_.i32[i] > INT16_MAX)
              ? (INT16_MAX)
              : HEDLEY_STATIC_CAST(int16_t, a_.i32[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cvtsepi32_epi16
  #define _mm256_cvtsepi32_epi16(a) simde_mm256_cvtsepi32_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_cvtsepi64_epi8 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cvtsepi64_epi8(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m128i_private a_ = simde__m128i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r_.i8[i] =
          (a_.i64[i] < INT8_MIN)
            ? (INT8_MIN)
            : ((a_.i64[i] > INT8_MAX)
              ? (INT8_MAX)
              : HEDLEY_STATIC_CAST(int8_t, a_.i64[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cvtsepi64_epi8
  #define _mm_cvtsepi64_epi8(a) simde_mm_cvtsepi64_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm256_cvtsepi64_epi8 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cvtsepi64_epi8(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m256i_private a_ = simde__m256i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r_.i8[i] =
          (a_.i64[i] < INT8_MIN)
            ? (INT8_MIN)
            : ((a_.i64[i] > INT8_MAX)
              ? (INT8_MAX)
              : HEDLEY_STATIC_CAST(int8_t, a_.i64[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cvtsepi64_epi8
  #define _mm256_cvtsepi64_epi8(a) simde_mm256_cvtsepi64_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_cvtsepi16_epi8 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cvtsepi16_epi8(a);
  #else
    simde__m256i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
      r_.i8[i] =
          (a_.i16[i] < INT8_MIN)
            ? (INT8_MIN)
            : ((a_.i16[i] > INT8_MAX)
              ? (INT8_MAX)
              : HEDLEY_STATIC_CAST(int8_t, a_.i16[i]));
    }

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtsepi16_epi8
  #define _mm512_cvtsepi16_epi8(a) simde_mm512_cvtsepi16_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_mask_cvtsepi16_epi8 (simde__m256i src, simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_cvtsepi16_epi8(src, k, a);
  #else
    return simde_mm256_mask_mov_epi8(src, k, simde_mm512_cvtsepi16_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cvtsepi16_epi8
  #define _mm512_mask_cvtsepi16_epi8(src, k, a) simde_mm512_mask_cvtsepi16_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_maskz_cvtsepi16_epi8 (simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_cvtsepi16_epi8(k, a);
  #else
    return simde_mm256_maskz_mov_epi8(k, simde_mm512_cvtsepi16_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_cvtsepi16_epi8
  #define _mm512_maskz_cvtsepi16_epi8(k, a) simde_mm512_maskz_cvtsepi16_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_cvtsepi32_epi8 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtsepi32_epi8(a);
  #else
    simde__m128i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i8[i] =
        (a_.i32[i] < INT8_MIN)
          ? (INT8_MIN)
          : ((a_.i32[i] > INT8_MAX)
            ? (INT8_MAX)
            : HEDLEY_STATIC_CAST(int8_t, a_.i32[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtsepi32_epi8
  #define _mm512_cvtsepi32_epi8(a) simde_mm512_cvtsepi32_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_mask_cvtsepi32_epi8 (simde__m128i src, simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cvtsepi32_epi8(src, k, a);
  #else
    simde__m128i_private r_;
    simde__m128i_private src_ = simde__m128i_to_private(src);
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i8[i] = ((k>>i) &1 ) ?
        ((a_.i32[i] < INT8_MIN)
        ? (INT8_MIN)
        : ((a_.i32[i] > INT8_MAX)
          ? (INT8_MAX)
          : HEDLEY_STATIC_CAST(int8_t, a_.i32[i]))) : src_.i8[i] ;
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cvtsepi32_epi8
  #define _mm512_mask_cvtsepi32_epi8(src, k, a) simde_mm512_mask_cvtsepi32_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_maskz_cvtsepi32_epi8 (simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_cvtsepi32_epi8(k, a);
  #else
    simde__m128i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i8[i] = ((k>>i) &1 ) ?
                ((a_.i32[i] < INT8_MIN)
                  ? (INT8_MIN)
                  : ((a_.i32[i] > INT8_MAX)
                    ? (INT8_MAX)
                    : HEDLEY_STATIC_CAST(int8_t, a_.i32[i]))) : INT8_C(0) ;
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_cvtsepi32_epi8
  #define _mm512_maskz_cvtsepi32_epi8(k, a) simde_mm512_maskz_cvtsepi32_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_cvtsepi32_epi16 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtsepi32_epi16(a);
  #else
    simde__m256i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i16[i] =
        (a_.i32[i] < INT16_MIN)
          ? (INT16_MIN)
          : ((a_.i32[i] > INT16_MAX)
            ? (INT16_MAX)
            : HEDLEY_STATIC_CAST(int16_t, a_.i32[i]));
    }

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtsepi32_epi16
  #define _mm512_cvtsepi32_epi16(a) simde_mm512_cvtsepi32_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_mask_cvtsepi32_epi16 (simde__m256i src, simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cvtsepi32_epi16(src, k, a);
  #else
    simde__m256i_private r_;
    simde__m256i_private src_ = simde__m256i_to_private(src);
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i16[i] = ((k>>i) &1 ) ?
        ((a_.i32[i] < INT16_MIN)
        ? (INT16_MIN)
        : ((a_.i32[i] > INT16_MAX)
          ? (INT16_MAX)
          : HEDLEY_STATIC_CAST(int16_t, a_.i32[i]))) : src_.i16[i];
    }

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cvtsepi32_epi16
  #define _mm512_mask_cvtsepi32_epi16(src, k, a) simde_mm512_mask_cvtsepi32_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_maskz_cvtsepi32_epi16 (simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_cvtsepi32_epi16(k, a);
  #else
    simde__m256i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      r_.i16[i] = ((k>>i) &1 ) ?
                  ((a_.i32[i] < INT16_MIN)
                  ? (INT16_MIN)
                  : ((a_.i32[i] > INT16_MAX)
                      ? (INT16_MAX)
                      : HEDLEY_STATIC_CAST(int16_t, a_.i32[i]))) : INT16_C(0);
    }

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_cvtsepi32_epi16
  #define _mm512_maskz_cvtsepi32_epi16(k, a) simde_mm512_maskz_cvtsepi32_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_cvtsepi64_epi8 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtsepi64_epi8(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
       r_.i8[i] =
        (a_.i64[i] < INT8_MIN)
          ? (INT8_MIN)
          : ((a_.i64[i] > INT8_MAX)
            ? (INT8_MAX)
            : HEDLEY_STATIC_CAST(int8_t, a_.i64[i]));
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtsepi64_epi8
  #define _mm512_cvtsepi64_epi8(a) simde_mm512_cvtsepi64_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_mask_cvtsepi64_epi8 (simde__m128i src, simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cvtsepi64_epi8(src, k, a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m128i_private src_ = simde__m128i_to_private(src);
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r_.i8[i] = ((k>>i) &1 ) ?
                ((a_.i64[i] < INT8_MIN)
                  ? (INT8_MIN)
                  : ((a_.i64[i] > INT8_MAX)
                    ? (INT8_MAX)
                    : HEDLEY_STATIC_CAST(int8_t, a_.i64[i]))) : src_.i8[i];
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cvtsepi64_epi8
  #define _mm512_mask_cvtsepi64_epi8(src, k, a) simde_mm512_mask_cvtsepi64_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_maskz_cvtsepi64_epi8 (simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_cvtsepi64_epi8(k, a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r_.i8[i] = ((k>>i) &1 ) ?
                ((a_.i64[i] < INT8_MIN)
                  ? (INT8_MIN)
                  : ((a_.i64[i] > INT8_MAX)
                    ? (INT8_MAX)
                    : HEDLEY_STATIC_CAST(int8_t, a_.i64[i]))) : INT8_C(0);
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_cvtsepi64_epi8
  #define _mm512_maskz_cvtsepi64_epi8(k, a) simde_mm512_maskz_cvtsepi64_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_cvtsepi64_epi16 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtsepi64_epi16(a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m512i_private a_ = simde__m512i_to_private(a);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
        r_.i16[i] =
        (a_.i64[i] < INT16_MIN)
          ? (INT16_MIN)
          : ((a_.i64[i] > INT16_MAX)
            ? (INT16_MAX)
            : HEDLEY_STATIC_CAST(int16_t, a_.i64[i]));
      }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtsepi64_epi16
  #define _mm512_cvtsepi64_epi16(a) simde_mm512_cvtsepi64_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_mask_cvtsepi64_epi16 (simde__m128i src, simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cvtsepi64_epi16(src, k, a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m128i_private src_ = simde__m128i_to_private(src);
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r_.i16[i] = ((k>>i) & 1) ?
                  ((a_.i64[i] < INT16_MIN)
                  ? (INT16_MIN)
                  : ((a_.i64[i] > INT16_MAX)
                      ? (INT16_MAX)
                      : HEDLEY_STATIC_CAST(int16_t, a_.i64[i]))) : src_.i16[i];
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cvtsepi64_epi16
  #define _mm512_mask_cvtsepi64_epi16(src, k, a) simde_mm512_mask_cvtsepi64_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_maskz_cvtsepi64_epi16 (simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_cvtsepi64_epi16(k, a);
  #else
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r_.i16[i] = ((k>>i) & 1) ?
                  ((a_.i64[i] < INT16_MIN)
                  ? (INT16_MIN)
                  : ((a_.i64[i] > INT16_MAX)
                      ? (INT16_MAX)
                      : HEDLEY_STATIC_CAST(int16_t, a_.i64[i]))) : INT16_C(0);
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_cvtsepi64_epi16
  #define _mm512_maskz_cvtsepi64_epi16(k, a) simde_mm512_maskz_cvtsepi64_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_cvtsepi64_epi32 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtsepi64_epi32(a);
  #else
    simde__m256i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
        r_.i32[i] =
        (a_.i64[i] < INT32_MIN)
          ? (INT32_MIN)
          : ((a_.i64[i] > INT32_MAX)
            ? (INT32_MAX)
            : HEDLEY_STATIC_CAST(int32_t, a_.i64[i]));
      }

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtsepi64_epi32
  #define _mm512_cvtsepi64_epi32(a) simde_mm512_cvtsepi64_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_mask_cvtsepi64_epi32 (simde__m256i src, simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_cvtsepi64_epi32(src, k, a);
  #else
    simde__m256i_private r_;
    simde__m256i_private src_ = simde__m256i_to_private(src);
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r_.i32[i] = ((k>>i) & 1) ?
        ((a_.i64[i] < INT32_MIN)
        ? (INT32_MIN)
        : ((a_.i64[i] > INT32_MAX)
          ? (INT32_MAX)
          : HEDLEY_STATIC_CAST(int32_t, a_.i64[i]))) : src_.i32[i];
    }

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cvtsepi64_epi32
  #define _mm512_mask_cvtsepi64_epi32(src, k, a) simde_mm512_mask_cvtsepi64_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_maskz_cvtsepi64_epi32 (simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_cvtsepi64_epi32(k, a);
  #else
    simde__m256i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      r_.i32[i] = ((k>>i) & 1) ?
                  ((a_.i64[i] < INT32_MIN)
                  ? (INT32_MIN)
                  : ((a_.i64[i] > INT32_MAX)
                      ? (INT32_MAX)
                      : HEDLEY_STATIC_CAST(int32_t, a_.i64[i]))) : INT32_C(0);
    }

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_cvtsepi64_epi32
  #define _mm512_maskz_cvtsepi64_epi32(k, a) simde_mm512_maskz_cvtsepi64_epi32(k, a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_CVTS_H) */
