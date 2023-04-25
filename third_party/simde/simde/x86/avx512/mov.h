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
 *   2020      Christopher Moore <moore@free.fr>
 */

#if !defined(SIMDE_X86_AVX512_MOV_H)
#define SIMDE_X86_AVX512_MOV_H

#include "types.h"
#include "cast.h"
#include "set.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_mov_epi8 (simde__m128i src, simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_mov_epi8(src, k, a);
  #else
    simde__m128i_private
      src_ = simde__m128i_to_private(src),
      a_ = simde__m128i_to_private(a),
      r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
      r_.i8[i] = ((k >> i) & 1) ? a_.i8[i] : src_.i8[i];
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_mov_epi8
  #define _mm_mask_mov_epi8(src, k, a) simde_mm_mask_mov_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_mov_epi16 (simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_mov_epi16(src, k, a);
  #else
    simde__m128i_private
      src_ = simde__m128i_to_private(src),
      a_ = simde__m128i_to_private(a),
      r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
      r_.i16[i] = ((k >> i) & 1) ? a_.i16[i] : src_.i16[i];
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_mov_epi16
  #define _mm_mask_mov_epi16(src, k, a) simde_mm_mask_mov_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_mov_epi32 (simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_mov_epi32(src, k, a);
  #else
    simde__m128i_private
      src_ = simde__m128i_to_private(src),
      a_ = simde__m128i_to_private(a),
      r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
      r_.i32[i] = ((k >> i) & 1) ? a_.i32[i] : src_.i32[i];
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_mov_epi32
  #define _mm_mask_mov_epi32(src, k, a) simde_mm_mask_mov_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_mov_epi64 (simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_mov_epi64(src, k, a);
  #else
    simde__m128i_private
      src_ = simde__m128i_to_private(src),
      a_ = simde__m128i_to_private(a),
      r_;

    /* N.B. CM: No fallbacks as there are only two elements */
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
      r_.i64[i] = ((k >> i) & 1) ? a_.i64[i] : src_.i64[i];
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_mov_epi64
  #define _mm_mask_mov_epi64(src, k, a) simde_mm_mask_mov_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm_mask_mov_pd(simde__m128d src, simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_mov_pd(src, k, a);
  #else
    return simde_mm_castsi128_pd(simde_mm_mask_mov_epi64(simde_mm_castpd_si128(src), k, simde_mm_castpd_si128(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_mov_pd
  #define _mm_mask_mov_pd(src, k, a) simde_mm_mask_mov_pd(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_mm_mask_mov_ps (simde__m128 src, simde__mmask8 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_mov_ps(src, k, a);
  #else
    return simde_mm_castsi128_ps(simde_mm_mask_mov_epi32(simde_mm_castps_si128(src), k, simde_mm_castps_si128(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_mov_ps
  #define _mm_mask_mov_ps(src, k, a) simde_mm_mask_mov_ps(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_mov_epi8 (simde__m256i src, simde__mmask32 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_mov_epi8(src, k, a);
  #else
    simde__m256i_private
      r_,
      src_ = simde__m256i_to_private(src),
      a_ = simde__m256i_to_private(a);

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i[0] = simde_mm_mask_mov_epi8(src_.m128i[0], HEDLEY_STATIC_CAST(simde__mmask16, k      ), a_.m128i[0]);
      r_.m128i[1] = simde_mm_mask_mov_epi8(src_.m128i[1], HEDLEY_STATIC_CAST(simde__mmask16, k >> 16), a_.m128i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = ((k >> i) & 1) ? a_.i8[i] : src_.i8[i];
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_mov_epi8
  #define _mm256_mask_mov_epi8(src, k, a) simde_mm256_mask_mov_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_mov_epi16 (simde__m256i src, simde__mmask16 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_mov_epi16(src, k, a);
  #else
    simde__m256i_private
      src_ = simde__m256i_to_private(src),
      a_ = simde__m256i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i[0] = simde_mm_mask_mov_epi16(src_.m128i[0], HEDLEY_STATIC_CAST(simde__mmask8, k     ), a_.m128i[0]);
      r_.m128i[1] = simde_mm_mask_mov_epi16(src_.m128i[1], HEDLEY_STATIC_CAST(simde__mmask8, k >> 8), a_.m128i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = ((k >> i) & 1) ? a_.i16[i] : src_.i16[i];
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_mov_epi16
  #define _mm256_mask_mov_epi16(src, k, a) simde_mm256_mask_mov_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_mov_epi32 (simde__m256i src, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_mov_epi32(src, k, a);
  #else
    simde__m256i_private
      src_ = simde__m256i_to_private(src),
      a_ = simde__m256i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i[0] = simde_mm_mask_mov_epi32(src_.m128i[0], k     , a_.m128i[0]);
      r_.m128i[1] = simde_mm_mask_mov_epi32(src_.m128i[1], k >> 4, a_.m128i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = ((k >> i) & 1) ? a_.i32[i] : src_.i32[i];
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_mov_epi32
  #define _mm256_mask_mov_epi32(src, k, a) simde_mm256_mask_mov_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_mov_epi64 (simde__m256i src, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_mov_epi64(src, k, a);
  #else
    simde__m256i_private
      src_ = simde__m256i_to_private(src),
      a_ = simde__m256i_to_private(a),
      r_;

    /* N.B. CM: This fallback may not be faster as there are only four elements */
    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i[0] = simde_mm_mask_mov_epi64(src_.m128i[0], k     , a_.m128i[0]);
      r_.m128i[1] = simde_mm_mask_mov_epi64(src_.m128i[1], k >> 2, a_.m128i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
        r_.i64[i] = ((k >> i) & 1) ? a_.i64[i] : src_.i64[i];
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_mov_epi64
  #define _mm256_mask_mov_epi64(src, k, a) simde_mm256_mask_mov_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_mask_mov_pd (simde__m256d src, simde__mmask8 k, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_mov_pd(src, k, a);
  #else
    return simde_mm256_castsi256_pd(simde_mm256_mask_mov_epi64(simde_mm256_castpd_si256(src), k, simde_mm256_castpd_si256(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_mov_pd
  #define _mm256_mask_mov_pd(src, k, a) simde_mm256_mask_mov_pd(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_mask_mov_ps (simde__m256 src, simde__mmask8 k, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_mov_ps(src, k, a);
  #else
    return simde_mm256_castsi256_ps(simde_mm256_mask_mov_epi32(simde_mm256_castps_si256(src), k, simde_mm256_castps_si256(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_mov_ps
  #define _mm256_mask_mov_ps(src, k, a) simde_mm256_mask_mov_ps(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_mov_epi8 (simde__m512i src, simde__mmask64 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_mov_epi8(src, k, a);
  #else
    simde__m512i_private
      src_ = simde__m512i_to_private(src),
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m256i[0] = simde_mm256_mask_mov_epi8(src_.m256i[0], HEDLEY_STATIC_CAST(simde__mmask32, k      ), a_.m256i[0]);
      r_.m256i[1] = simde_mm256_mask_mov_epi8(src_.m256i[1], HEDLEY_STATIC_CAST(simde__mmask32, k >> 32), a_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = ((k >> i) & 1) ? a_.i8[i] : src_.i8[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_mov_epi8
  #define _mm512_mask_mov_epi8(src, k, a) simde_mm512_mask_mov_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_mov_epi16 (simde__m512i src, simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_mov_epi16(src, k, a);
  #else
    simde__m512i_private
      src_ = simde__m512i_to_private(src),
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m256i[0] = simde_mm256_mask_mov_epi16(src_.m256i[0], HEDLEY_STATIC_CAST(simde__mmask16, k      ), a_.m256i[0]);
      r_.m256i[1] = simde_mm256_mask_mov_epi16(src_.m256i[1], HEDLEY_STATIC_CAST(simde__mmask16, k >> 16), a_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = ((k >> i) & 1) ? a_.i16[i] : src_.i16[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_mov_epi16
  #define _mm512_mask_mov_epi16(src, k, a) simde_mm512_mask_mov_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_mov_epi32 (simde__m512i src, simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_mov_epi32(src, k, a);
  #else
    simde__m512i_private
      src_ = simde__m512i_to_private(src),
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m256i[0] = simde_mm256_mask_mov_epi32(src_.m256i[0], HEDLEY_STATIC_CAST(simde__mmask8, k     ), a_.m256i[0]);
      r_.m256i[1] = simde_mm256_mask_mov_epi32(src_.m256i[1], HEDLEY_STATIC_CAST(simde__mmask8, k >> 8), a_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = ((k >> i) & 1) ? a_.i32[i] : src_.i32[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_mov_epi32
  #define _mm512_mask_mov_epi32(src, k, a) simde_mm512_mask_mov_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_mov_epi64 (simde__m512i src, simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_mov_epi64(src, k, a);
  #else
    simde__m512i_private
      src_ = simde__m512i_to_private(src),
      a_ = simde__m512i_to_private(a),
      r_;

    /* N.B. CM: Without AVX2 this fallback may not be faster as there are only eight elements */
    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m256i[0] = simde_mm256_mask_mov_epi64(src_.m256i[0], k     , a_.m256i[0]);
      r_.m256i[1] = simde_mm256_mask_mov_epi64(src_.m256i[1], k >> 4, a_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
        r_.i64[i] = ((k >> i) & 1) ? a_.i64[i] : src_.i64[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_mov_epi64
  #define _mm512_mask_mov_epi64(src, k, a) simde_mm512_mask_mov_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_mov_pd (simde__m512d src, simde__mmask8 k, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_mov_pd(src, k, a);
  #else
    return simde_mm512_castsi512_pd(simde_mm512_mask_mov_epi64(simde_mm512_castpd_si512(src), k, simde_mm512_castpd_si512(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_mov_pd
  #define _mm512_mask_mov_pd(src, k, a) simde_mm512_mask_mov_pd(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_mov_ps (simde__m512 src, simde__mmask16 k, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_mov_ps(src, k, a);
  #else
    return simde_mm512_castsi512_ps(simde_mm512_mask_mov_epi32(simde_mm512_castps_si512(src), k, simde_mm512_castps_si512(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_mov_ps
  #define _mm512_mask_mov_ps(src, k, a) simde_mm512_mask_mov_ps(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_mov_epi8 (simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_mov_epi8(k, a);
  #else
    simde__m128i_private
      a_ = simde__m128i_to_private(a),
      r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
      r_.i8[i] = ((k >> i) & 1) ? a_.i8[i] : INT8_C(0);
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_mov_epi8
  #define _mm_maskz_mov_epi8(k, a) simde_mm_maskz_mov_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_mov_epi16 (simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_mov_epi16(k, a);
  #else
    simde__m128i_private
      a_ = simde__m128i_to_private(a),
      r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
      r_.i16[i] = ((k >> i) & 1) ? a_.i16[i] : INT16_C(0);
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_mov_epi16
  #define _mm_maskz_mov_epi16(k, a) simde_mm_maskz_mov_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_mov_epi32 (simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_mov_epi32(k, a);
  #else
    simde__m128i_private
      a_ = simde__m128i_to_private(a),
      r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
      r_.i32[i] = ((k >> i) & 1) ? a_.i32[i] : INT32_C(0);
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_mov_epi32
  #define _mm_maskz_mov_epi32(k, a) simde_mm_maskz_mov_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_mov_epi64 (simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_mov_epi64(k, a);
  #else
    simde__m128i_private
      a_ = simde__m128i_to_private(a),
      r_;

    /* N.B. CM: No fallbacks as there are only two elements */
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
      r_.i64[i] = ((k >> i) & 1) ? a_.i64[i] : INT64_C(0);
    }

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_mov_epi64
  #define _mm_maskz_mov_epi64(k, a) simde_mm_maskz_mov_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm_maskz_mov_pd (simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_mov_pd(k, a);
  #else
    return simde_mm_castsi128_pd(simde_mm_maskz_mov_epi64(k, simde_mm_castpd_si128(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_mov_pd
  #define _mm_maskz_mov_pd(k, a) simde_mm_maskz_mov_pd(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_mm_maskz_mov_ps (simde__mmask8 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_mov_ps(k, a);
  #else
    return simde_mm_castsi128_ps(simde_mm_maskz_mov_epi32(k, simde_mm_castps_si128(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_mov_ps
  #define _mm_maskz_mov_ps(k, a) simde_mm_maskz_mov_ps(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_mov_epi8 (simde__mmask32 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_mov_epi8(k, a);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i[0] = simde_mm_maskz_mov_epi8(HEDLEY_STATIC_CAST(simde__mmask16, k      ), a_.m128i[0]);
      r_.m128i[1] = simde_mm_maskz_mov_epi8(HEDLEY_STATIC_CAST(simde__mmask16, k >> 16), a_.m128i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = ((k >> i) & 1) ? a_.i8[i] : INT8_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_mov_epi8
  #define _mm256_maskz_mov_epi8(k, a) simde_mm256_maskz_mov_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_mov_epi16 (simde__mmask16 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_mov_epi16(k, a);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i[0] = simde_mm_maskz_mov_epi16(HEDLEY_STATIC_CAST(simde__mmask8, k     ), a_.m128i[0]);
      r_.m128i[1] = simde_mm_maskz_mov_epi16(HEDLEY_STATIC_CAST(simde__mmask8, k >> 8), a_.m128i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = ((k >> i) & 1) ? a_.i16[i] : INT16_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_mov_epi16
  #define _mm256_maskz_mov_epi16(k, a) simde_mm256_maskz_mov_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_mov_epi32 (simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_mov_epi32(k, a);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i[0] = simde_mm_maskz_mov_epi32(k     , a_.m128i[0]);
      r_.m128i[1] = simde_mm_maskz_mov_epi32(k >> 4, a_.m128i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = ((k >> i) & 1) ? a_.i32[i] : INT32_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_mov_epi32
  #define _mm256_maskz_mov_epi32(k, a) simde_mm256_maskz_mov_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_mov_epi64 (simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_mov_epi64(k, a);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a),
      r_;

    /* N.B. CM: This fallback may not be faster as there are only four elements */
    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i[0] = simde_mm_maskz_mov_epi64(k     , a_.m128i[0]);
      r_.m128i[1] = simde_mm_maskz_mov_epi64(k >> 2, a_.m128i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
        r_.i64[i] = ((k >> i) & 1) ? a_.i64[i] : INT64_C(0);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_mov_epi64
  #define _mm256_maskz_mov_epi64(k, a) simde_mm256_maskz_mov_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_maskz_mov_pd (simde__mmask8 k, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_mov_pd(k, a);
  #else
    return simde_mm256_castsi256_pd(simde_mm256_maskz_mov_epi64(k, simde_mm256_castpd_si256(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_mov_pd
  #define _mm256_maskz_mov_pd(k, a) simde_mm256_maskz_mov_pd(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_maskz_mov_ps (simde__mmask8 k, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_mov_ps(k, a);
  #else
    return simde_mm256_castsi256_ps(simde_mm256_maskz_mov_epi32(k, simde_mm256_castps_si256(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_mov_ps
  #define _mm256_maskz_mov_ps(k, a) simde_mm256_maskz_mov_ps(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_mov_epi8 (simde__mmask64 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_mov_epi8(k, a);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m256i[0] = simde_mm256_maskz_mov_epi8(HEDLEY_STATIC_CAST(simde__mmask32, k      ), a_.m256i[0]);
      r_.m256i[1] = simde_mm256_maskz_mov_epi8(HEDLEY_STATIC_CAST(simde__mmask32, k >> 32), a_.m256i[1]);
    #else
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = ((k >> i) & 1) ? a_.i8[i] : INT8_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_mov_epi8
  #define _mm512_maskz_mov_epi8(k, a) simde_mm512_maskz_mov_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_mov_epi16 (simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_mov_epi16(k, a);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m256i[0] = simde_mm256_maskz_mov_epi16(HEDLEY_STATIC_CAST(simde__mmask16, k      ), a_.m256i[0]);
      r_.m256i[1] = simde_mm256_maskz_mov_epi16(HEDLEY_STATIC_CAST(simde__mmask16, k >> 16), a_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = ((k >> i) & 1) ? a_.i16[i] : INT16_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_mov_epi16
  #define _mm512_maskz_mov_epi16(k, a) simde_mm512_maskz_mov_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_mov_epi32 (simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_mov_epi32(k, a);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m256i[0] = simde_mm256_maskz_mov_epi32(HEDLEY_STATIC_CAST(simde__mmask8, k     ), a_.m256i[0]);
      r_.m256i[1] = simde_mm256_maskz_mov_epi32(HEDLEY_STATIC_CAST(simde__mmask8, k >> 8), a_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = ((k >> i) & 1) ? a_.i32[i] : INT32_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_mov_epi32
  #define _mm512_maskz_mov_epi32(k, a) simde_mm512_maskz_mov_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_mov_epi64 (simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_mov_epi64(k, a);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      r_;

    /* N.B. CM: Without AVX2 this fallback may not be faster as there are only eight elements */
    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m256i[0] = simde_mm256_maskz_mov_epi64(k     , a_.m256i[0]);
      r_.m256i[1] = simde_mm256_maskz_mov_epi64(k >> 4, a_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
        r_.i64[i] = ((k >> i) & 1) ? a_.i64[i] : INT64_C(0);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_mov_epi64
  #define _mm512_maskz_mov_epi64(k, a) simde_mm512_maskz_mov_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_mov_pd (simde__mmask8 k, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_mov_pd(k, a);
  #else
    return simde_mm512_castsi512_pd(simde_mm512_maskz_mov_epi64(k, simde_mm512_castpd_si512(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_mov_pd
  #define _mm512_maskz_mov_pd(k, a) simde_mm512_maskz_mov_pd(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_mov_ps (simde__mmask16 k, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_mov_ps(k, a);
  #else
    return simde_mm512_castsi512_ps(simde_mm512_maskz_mov_epi32(k, simde_mm512_castps_si512(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_mov_ps
  #define _mm512_maskz_mov_ps(k, a) simde_mm512_maskz_mov_ps(k, a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_MOV_H) */
