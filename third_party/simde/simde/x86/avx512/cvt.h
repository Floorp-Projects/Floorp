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
 *   2020-2021 Evan Nemerson <evan@nemerson.com>
 *   2020      Himanshi Mathur <himanshi18037@iiitd.ac.in>
 *   2020      Hidayat Khan <huk2209@gmail.com>
 *   2021      Andrew Rodriguez <anrodriguez@linkedin.com>
 */

#if !defined(SIMDE_X86_AVX512_CVT_H)
#define SIMDE_X86_AVX512_CVT_H

#include "types.h"
#include "mov.h"
#include "../f16c.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm_cvtepi64_pd (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm_cvtepi64_pd(a);
  #else
    simde__m128d_private r_;
    simde__m128i_private a_ = simde__m128i_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      /* https://stackoverflow.com/questions/41144668/how-to-efficiently-perform-double-int64-conversions-with-sse-avx */
      __m128i xH = _mm_srai_epi32(a_.n, 16);
      #if defined(SIMDE_X86_SSE4_2_NATIVE)
        xH = _mm_blend_epi16(xH, _mm_setzero_si128(), 0x33);
      #else
        xH = _mm_and_si128(xH, _mm_set_epi16(~INT16_C(0), ~INT16_C(0), INT16_C(0), INT16_C(0), ~INT16_C(0), ~INT16_C(0), INT16_C(0), INT16_C(0)));
      #endif
      xH = _mm_add_epi64(xH, _mm_castpd_si128(_mm_set1_pd(442721857769029238784.0)));
      const __m128i e = _mm_castpd_si128(_mm_set1_pd(0x0010000000000000));
      #if defined(SIMDE_X86_SSE4_2_NATIVE)
        __m128i xL = _mm_blend_epi16(a_.n, e, 0x88);
      #else
        __m128i m = _mm_set_epi16(INT16_C(0), ~INT16_C(0), ~INT16_C(0), ~INT16_C(0), INT16_C(0), ~INT16_C(0), ~INT16_C(0), ~INT16_C(0));
        __m128i xL = _mm_or_si128(_mm_and_si128(m, a_.n), _mm_andnot_si128(m, e));
      #endif
      __m128d f = _mm_sub_pd(_mm_castsi128_pd(xH), _mm_set1_pd(442726361368656609280.0));
      return _mm_add_pd(f, _mm_castsi128_pd(xL));
    #elif defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.f64, a_.i64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
        r_.f64[i] = HEDLEY_STATIC_CAST(simde_float64, a_.i64[i]);
      }
    #endif

    return simde__m128d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_cvtepi64_pd
  #define _mm_cvtepi64_pd(a) simde_mm_cvtepi64_pd(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm_mask_cvtepi64_pd(simde__m128d src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm_mask_cvtepi64_pd(src, k, a);
  #else
    return simde_mm_mask_mov_pd(src, k, simde_mm_cvtepi64_pd(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cvtepi64_pd
  #define _mm_mask_cvtepi64_pd(src, k, a) simde_mm_mask_cvtepi64_pd(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm_maskz_cvtepi64_pd(simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm_maskz_cvtepi64_pd(k, a);
  #else
    return simde_mm_maskz_mov_pd(k, simde_mm_cvtepi64_pd(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_cvtepi64_pd
  #define _mm_maskz_cvtepi64_pd(k, a) simde_mm_maskz_cvtepi64_pd(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_cvtepi16_epi8 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cvtepi16_epi8(a);
  #else
    simde__m256i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.i8, a_.i16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = HEDLEY_STATIC_CAST(int8_t, a_.i16[i]);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtepi16_epi8
  #define _mm512_cvtepi16_epi8(a) simde_mm512_cvtepi16_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_mask_cvtepi16_epi8 (simde__m256i src, simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_cvtepi16_epi8(src, k, a);
  #else
    return simde_mm256_mask_mov_epi8(src, k, simde_mm512_cvtepi16_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cvtepi16_epi8
  #define _mm512_mask_cvtepi16_epi8(src, k, a) simde_mm512_mask_cvtepi16_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_maskz_cvtepi16_epi8 (simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_cvtepi16_epi8(k, a);
  #else
    return simde_mm256_maskz_mov_epi8(k, simde_mm512_cvtepi16_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_cvtepi16_epi8
  #define _mm512_maskz_cvtepi16_epi8(k, a) simde_mm512_maskz_cvtepi16_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_cvtepi8_epi16 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_cvtepi8_epi16(a);
  #else
    simde__m512i_private r_;
    simde__m256i_private a_ = simde__m256i_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.i16, a_.i8);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = a_.i8[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtepi8_epi16
  #define _mm512_cvtepi8_epi16(a) simde_mm512_cvtepi8_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_cvtepi32_ps (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtepi32_ps(a);
  #else
    simde__m512_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.f32, a_.i32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.f32[i] = HEDLEY_STATIC_CAST(simde_float32, a_.i32[i]);
      }
    #endif

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtepi32_ps
  #define _mm512_cvtepi32_ps(a) simde_mm512_cvtepi32_ps(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_cvtepi64_epi32 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtepi64_epi32(a);
  #else
    simde__m256i_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.i32, a_.i64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = HEDLEY_STATIC_CAST(int32_t, a_.i64[i]);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtepi64_epi32
  #define _mm512_cvtepi64_epi32(a) simde_mm512_cvtepi64_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_cvtepu32_ps (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtepu32_ps(a);
  #else
    simde__m512_private r_;
    simde__m512i_private a_ = simde__m512i_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128) / sizeof(r_.m128[0])) ; i++) {
        /* https://stackoverflow.com/a/34067907/501126 */
        const __m128 tmp = _mm_cvtepi32_ps(_mm_srli_epi32(a_.m128i[i], 1));
        r_.m128[i] =
          _mm_add_ps(
            _mm_add_ps(tmp, tmp),
            _mm_cvtepi32_ps(_mm_and_si128(a_.m128i[i], _mm_set1_epi32(1)))
          );
      }
    #elif defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.f32, a_.u32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
        r_.f32[i] = HEDLEY_STATIC_CAST(float, a_.u32[i]);
      }
    #endif

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtepu32_ps
  #define _mm512_cvtepu32_ps(a) simde_mm512_cvtepu32_ps(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_cvtph_ps(simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_cvtph_ps(a);
  #endif
  simde__m256i_private a_ = simde__m256i_to_private(a);
  simde__m512_private r_;

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
    r_.f32[i] = simde_float16_to_float32(simde_uint16_as_float16(a_.u16[i]));
  }

  return simde__m512_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cvtph_ps
  #define _mm512_cvtph_ps(a) simde_mm512_cvtph_ps(a)
#endif


SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_CVT_H) */
