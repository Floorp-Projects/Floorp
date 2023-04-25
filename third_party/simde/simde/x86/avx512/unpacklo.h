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
 *   2020      Hidayat Khan <huk2209@gmail.com>
 */

#if !defined(SIMDE_X86_AVX512_UNPACKLO_H)
#define SIMDE_X86_AVX512_UNPACKLO_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_unpacklo_epi8 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_unpacklo_epi8(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_)
      r_.i8 = SIMDE_SHUFFLE_VECTOR_(8, 64, a_.i8, b_.i8,
                                    0,   64,  1,  65,  2,  66,  3,  67,
                                    4,   68,  5,  69,  6,  70,  7,  71,
                                    16,  80, 17,  81, 18,  82, 19,  83,
                                    20,  84, 21,  85, 22,  86, 23,  87,
                                    32,  96, 33,  97, 34,  98, 35,  99,
                                    36, 100, 37, 101, 38, 102, 39, 103,
                                    48, 112, 49, 113, 50, 114, 51, 115,
                                    52, 116, 53, 117, 54, 118, 55, 119);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_unpacklo_epi8(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_unpacklo_epi8(a_.m256i[1], b_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0]) / 2) ; i++) {
        r_.i8[2 * i] = a_.i8[i + ~(~i | 7)];
        r_.i8[2 * i + 1] = b_.i8[i + ~(~i | 7)];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_unpacklo_epi8
  #define _mm512_unpacklo_epi8(a, b) simde_mm512_unpacklo_epi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_unpacklo_epi8(simde__m512i src, simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_unpacklo_epi8(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_unpacklo_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_unpacklo_epi8
  #define _mm512_mask_unpacklo_epi8(src, k, a, b) simde_mm512_mask_unpacklo_epi8(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_unpacklo_epi8(simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_unpacklo_epi8(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_unpacklo_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_unpacklo_epi8
  #define _mm512_maskz_unpacklo_epi8(k, a, b) simde_mm512_maskz_unpacklo_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_unpacklo_epi8(simde__m256i src, simde__mmask32 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_unpacklo_epi8(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi8(src, k, simde_mm256_unpacklo_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_unpacklo_epi8
  #define _mm256_mask_unpacklo_epi8(src, k, a, b) simde_mm256_mask_unpacklo_epi8(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_unpacklo_epi8(simde__mmask32 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_unpacklo_epi8(k, a, b);
  #else
    return simde_mm256_maskz_mov_epi8(k, simde_mm256_unpacklo_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_unpacklo_epi8
  #define _mm256_maskz_unpacklo_epi8(k, a, b) simde_mm256_maskz_unpacklo_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_unpacklo_epi8(simde__m128i src, simde__mmask16 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_unpacklo_epi8(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi8(src, k, simde_mm_unpacklo_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_unpacklo_epi8
  #define _mm_mask_unpacklo_epi8(src, k, a, b) simde_mm_mask_unpacklo_epi8(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_unpacklo_epi8(simde__mmask16 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_unpacklo_epi8(k, a, b);
  #else
    return simde_mm_maskz_mov_epi8(k, simde_mm_unpacklo_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_unpacklo_epi8
  #define _mm_maskz_unpacklo_epi8(k, a, b) simde_mm_maskz_unpacklo_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_unpacklo_epi16 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_unpacklo_epi16(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_)
      r_.i16 = SIMDE_SHUFFLE_VECTOR_(16, 64, a_.i16, b_.i16,
                                    0,  32,  1, 33,  2, 34,  3, 35,  8, 40,  9, 41, 10, 42, 11, 43,
                                    16, 48, 17, 49, 18, 50, 19, 51, 24, 56, 25, 57, 26, 58, 27, 59);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_unpacklo_epi16(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_unpacklo_epi16(a_.m256i[1], b_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0]) / 2) ; i++) {
        r_.i16[2 * i] = a_.i16[i + ~(~i | 3)];
        r_.i16[2 * i + 1] = b_.i16[i + ~(~i | 3)];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_unpacklo_epi16
  #define _mm512_unpacklo_epi16(a, b) simde_mm512_unpacklo_epi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_unpacklo_epi16(simde__m512i src, simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_unpacklo_epi16(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_unpacklo_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_unpacklo_epi16
  #define _mm512_mask_unpacklo_epi16(src, k, a, b) simde_mm512_mask_unpacklo_epi16(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_unpacklo_epi16(simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_unpacklo_epi16(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi16(k, simde_mm512_unpacklo_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_unpacklo_epi16
  #define _mm512_maskz_unpacklo_epi16(k, a, b) simde_mm512_maskz_unpacklo_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_unpacklo_epi16(simde__m256i src, simde__mmask16 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_unpacklo_epi16(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi16(src, k, simde_mm256_unpacklo_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_unpacklo_epi16
  #define _mm256_mask_unpacklo_epi16(src, k, a, b) simde_mm256_mask_unpacklo_epi16(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_unpacklo_epi16(simde__mmask16 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_unpacklo_epi16(k, a, b);
  #else
    return simde_mm256_maskz_mov_epi16(k, simde_mm256_unpacklo_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_unpacklo_epi16
  #define _mm256_maskz_unpacklo_epi16(k, a, b) simde_mm256_maskz_unpacklo_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_unpacklo_epi16(simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_unpacklo_epi16(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi16(src, k, simde_mm_unpacklo_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_unpacklo_epi16
  #define _mm_mask_unpacklo_epi16(src, k, a, b) simde_mm_mask_unpacklo_epi16(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_unpacklo_epi16(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_unpacklo_epi16(k, a, b);
  #else
    return simde_mm_maskz_mov_epi16(k, simde_mm_unpacklo_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_unpacklo_epi16
  #define _mm_maskz_unpacklo_epi16(k, a, b) simde_mm_maskz_unpacklo_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_unpacklo_epi32 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_unpacklo_epi32(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_)
      r_.i32 = SIMDE_SHUFFLE_VECTOR_(32, 64, a_.i32, b_.i32,
                                    0, 16, 1, 17, 4, 20, 5, 21,
                                    8, 24, 9, 25, 12, 28, 13, 29);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_unpacklo_epi32(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_unpacklo_epi32(a_.m256i[1], b_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0]) / 2) ; i++) {
        r_.i32[2 * i] = a_.i32[i + ~(~i | 1)];
        r_.i32[2 * i + 1] = b_.i32[i + ~(~i | 1)];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_unpacklo_epi32
  #define _mm512_unpacklo_epi32(a, b) simde_mm512_unpacklo_epi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_unpacklo_epi32(simde__m512i src, simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_unpacklo_epi32(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_unpacklo_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_unpacklo_epi32
  #define _mm512_mask_unpacklo_epi32(src, k, a, b) simde_mm512_mask_unpacklo_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_unpacklo_epi32(simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_unpacklo_epi32(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_unpacklo_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_unpacklo_epi32
  #define _mm512_maskz_unpacklo_epi32(k, a, b) simde_mm512_maskz_unpacklo_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_unpacklo_epi32(simde__m256i src, simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_unpacklo_epi32(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi32(src, k, simde_mm256_unpacklo_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_unpacklo_epi32
  #define _mm256_mask_unpacklo_epi32(src, k, a, b) simde_mm256_mask_unpacklo_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_unpacklo_epi32(simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_unpacklo_epi32(k, a, b);
  #else
    return simde_mm256_maskz_mov_epi32(k, simde_mm256_unpacklo_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_unpacklo_epi32
  #define _mm256_maskz_unpacklo_epi32(k, a, b) simde_mm256_maskz_unpacklo_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_unpacklo_epi32(simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_unpacklo_epi32(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi32(src, k, simde_mm_unpacklo_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_unpacklo_epi32
  #define _mm_mask_unpacklo_epi32(src, k, a, b) simde_mm_mask_unpacklo_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_unpacklo_epi32(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_unpacklo_epi32(k, a, b);
  #else
    return simde_mm_maskz_mov_epi32(k, simde_mm_unpacklo_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_unpacklo_epi32
  #define _mm_maskz_unpacklo_epi32(k, a, b) simde_mm_maskz_unpacklo_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_unpacklo_epi64 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_unpacklo_epi64(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_)
      r_.i64 = SIMDE_SHUFFLE_VECTOR_(64, 64, a_.i64, b_.i64, 0, 8, 2, 10, 4, 12, 6, 14);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_unpacklo_epi64(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_unpacklo_epi64(a_.m256i[1], b_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0]) / 2) ; i++) {
        r_.i64[2 * i] = a_.i64[2 * i];
        r_.i64[2 * i + 1] = b_.i64[2 * i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_unpacklo_epi64
  #define _mm512_unpacklo_epi64(a, b) simde_mm512_unpacklo_epi64(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_unpacklo_epi64(simde__m512i src, simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_unpacklo_epi64(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_unpacklo_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_unpacklo_epi64
  #define _mm512_mask_unpacklo_epi64(src, k, a, b) simde_mm512_mask_unpacklo_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_unpacklo_epi64(simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_unpacklo_epi64(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_unpacklo_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_unpacklo_epi64
  #define _mm512_maskz_unpacklo_epi64(k, a, b) simde_mm512_maskz_unpacklo_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_unpacklo_epi64(simde__m256i src, simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_unpacklo_epi64(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi64(src, k, simde_mm256_unpacklo_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_unpacklo_epi64
  #define _mm256_mask_unpacklo_epi64(src, k, a, b) simde_mm256_mask_unpacklo_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_unpacklo_epi64(simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_unpacklo_epi64(k, a, b);
  #else
    return simde_mm256_maskz_mov_epi64(k, simde_mm256_unpacklo_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_unpacklo_epi64
  #define _mm256_maskz_unpacklo_epi64(k, a, b) simde_mm256_maskz_unpacklo_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_unpacklo_epi64(simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_unpacklo_epi64(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi64(src, k, simde_mm_unpacklo_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_unpacklo_epi64
  #define _mm_mask_unpacklo_epi64(src, k, a, b) simde_mm_mask_unpacklo_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_unpacklo_epi64(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_unpacklo_epi64(k, a, b);
  #else
    return simde_mm_maskz_mov_epi64(k, simde_mm_unpacklo_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_unpacklo_epi64
  #define _mm_maskz_unpacklo_epi64(k, a, b) simde_mm_maskz_unpacklo_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_unpacklo_ps (simde__m512 a, simde__m512 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_unpacklo_ps(a, b);
  #else
    simde__m512_private
      r_,
      a_ = simde__m512_to_private(a),
      b_ = simde__m512_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_)
      r_.f32 = SIMDE_SHUFFLE_VECTOR_(32, 64, a_.f32, b_.f32,
                                    0, 16, 1, 17, 4, 20, 5, 21,
                                    8, 24, 9, 25, 12, 28, 13, 29);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256[0] = simde_mm256_unpacklo_ps(a_.m256[0], b_.m256[0]);
      r_.m256[1] = simde_mm256_unpacklo_ps(a_.m256[1], b_.m256[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0]) / 2) ; i++) {
        r_.f32[2 * i] = a_.f32[i + ~(~i | 1)];
        r_.f32[2 * i + 1] = b_.f32[i + ~(~i | 1)];
      }
    #endif

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_unpacklo_ps
  #define _mm512_unpacklo_ps(a, b) simde_mm512_unpacklo_ps(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_unpacklo_ps(simde__m512 src, simde__mmask16 k, simde__m512 a, simde__m512 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_unpacklo_ps(src, k, a, b);
  #else
    return simde_mm512_mask_mov_ps(src, k, simde_mm512_unpacklo_ps(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_unpacklo_ps
  #define _mm512_mask_unpacklo_ps(src, k, a, b) simde_mm512_mask_unpacklo_ps(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_unpacklo_ps(simde__mmask16 k, simde__m512 a, simde__m512 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_unpacklo_ps(k, a, b);
  #else
    return simde_mm512_maskz_mov_ps(k, simde_mm512_unpacklo_ps(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_unpacklo_ps
  #define _mm512_maskz_unpacklo_ps(k, a, b) simde_mm512_maskz_unpacklo_ps(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_mask_unpacklo_ps(simde__m256 src, simde__mmask8 k, simde__m256 a, simde__m256 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_unpacklo_ps(src, k, a, b);
  #else
    return simde_mm256_mask_mov_ps(src, k, simde_mm256_unpacklo_ps(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_unpacklo_ps
  #define _mm256_mask_unpacklo_ps(src, k, a, b) simde_mm256_mask_unpacklo_ps(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_maskz_unpacklo_ps(simde__mmask8 k, simde__m256 a, simde__m256 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_unpacklo_ps(k, a, b);
  #else
    return simde_mm256_maskz_mov_ps(k, simde_mm256_unpacklo_ps(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_unpacklo_ps
  #define _mm256_maskz_unpacklo_ps(k, a, b) simde_mm256_maskz_unpacklo_ps(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_mm_mask_unpacklo_ps(simde__m128 src, simde__mmask8 k, simde__m128 a, simde__m128 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_unpacklo_ps(src, k, a, b);
  #else
    return simde_mm_mask_mov_ps(src, k, simde_mm_unpacklo_ps(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_unpacklo_ps
  #define _mm_mask_unpacklo_ps(src, k, a, b) simde_mm_mask_unpacklo_ps(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_mm_maskz_unpacklo_ps(simde__mmask8 k, simde__m128 a, simde__m128 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_unpacklo_ps(k, a, b);
  #else
    return simde_mm_maskz_mov_ps(k, simde_mm_unpacklo_ps(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_unpacklo_ps
  #define _mm_maskz_unpacklo_ps(k, a, b) simde_mm_maskz_unpacklo_ps(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_unpacklo_pd (simde__m512d a, simde__m512d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_unpacklo_pd(a, b);
  #else
    simde__m512d_private
      r_,
      a_ = simde__m512d_to_private(a),
      b_ = simde__m512d_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_)
      r_.f64 = SIMDE_SHUFFLE_VECTOR_(64, 64, a_.f64, b_.f64, 0, 8, 2, 10, 4, 12, 6, 14);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256d[0] = simde_mm256_unpacklo_pd(a_.m256d[0], b_.m256d[0]);
      r_.m256d[1] = simde_mm256_unpacklo_pd(a_.m256d[1], b_.m256d[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0]) / 2) ; i++) {
        r_.f64[2 * i] = a_.f64[2 * i];
        r_.f64[2 * i + 1] = b_.f64[2 * i];
      }
    #endif

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_unpacklo_pd
  #define _mm512_unpacklo_pd(a, b) simde_mm512_unpacklo_pd(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_unpacklo_pd(simde__m512d src, simde__mmask8 k, simde__m512d a, simde__m512d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_unpacklo_pd(src, k, a, b);
  #else
    return simde_mm512_mask_mov_pd(src, k, simde_mm512_unpacklo_pd(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_unpacklo_pd
  #define _mm512_mask_unpacklo_pd(src, k, a, b) simde_mm512_mask_unpacklo_pd(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_unpacklo_pd(simde__mmask8 k, simde__m512d a, simde__m512d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_unpacklo_pd(k, a, b);
  #else
    return simde_mm512_maskz_mov_pd(k, simde_mm512_unpacklo_pd(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_unpacklo_pd
  #define _mm512_maskz_unpacklo_pd(k, a, b) simde_mm512_maskz_unpacklo_pd(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_mask_unpacklo_pd(simde__m256d src, simde__mmask8 k, simde__m256d a, simde__m256d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_unpacklo_pd(src, k, a, b);
  #else
    return simde_mm256_mask_mov_pd(src, k, simde_mm256_unpacklo_pd(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_unpacklo_pd
  #define _mm256_mask_unpacklo_pd(src, k, a, b) simde_mm256_mask_unpacklo_pd(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_maskz_unpacklo_pd(simde__mmask8 k, simde__m256d a, simde__m256d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_unpacklo_pd(k, a, b);
  #else
    return simde_mm256_maskz_mov_pd(k, simde_mm256_unpacklo_pd(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_unpacklo_pd
  #define _mm256_maskz_unpacklo_pd(k, a, b) simde_mm256_maskz_unpacklo_pd(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm_mask_unpacklo_pd(simde__m128d src, simde__mmask8 k, simde__m128d a, simde__m128d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_unpacklo_pd(src, k, a, b);
  #else
    return simde_mm_mask_mov_pd(src, k, simde_mm_unpacklo_pd(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_unpacklo_pd
  #define _mm_mask_unpacklo_pd(src, k, a, b) simde_mm_mask_unpacklo_pd(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm_maskz_unpacklo_pd(simde__mmask8 k, simde__m128d a, simde__m128d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_unpacklo_pd(k, a, b);
  #else
    return simde_mm_maskz_mov_pd(k, simde_mm_unpacklo_pd(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_unpacklo_pd
  #define _mm_maskz_unpacklo_pd(k, a, b) simde_mm_maskz_unpacklo_pd(k, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_UNPACKLO_H) */
