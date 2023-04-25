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

#if !defined(SIMDE_X86_AVX512_BLEND_H)
#define SIMDE_X86_AVX512_BLEND_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_blend_epi8(simde__mmask16 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_blend_epi8(k, a, b);
  #else
    return simde_mm_mask_mov_epi8(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_blend_epi8
  #define _mm_mask_blend_epi8(k, a, b) simde_mm_mask_blend_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_blend_epi16(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_blend_epi16(k, a, b);
  #else
    return simde_mm_mask_mov_epi16(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_blend_epi16
  #define _mm_mask_blend_epi16(k, a, b) simde_mm_mask_blend_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_blend_epi32(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_blend_epi32(k, a, b);
  #else
    return simde_mm_mask_mov_epi32(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_blend_epi32
  #define _mm_mask_blend_epi32(k, a, b) simde_mm_mask_blend_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_blend_epi64(simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_blend_epi64(k, a, b);
  #else
    return simde_mm_mask_mov_epi64(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_blend_epi64
  #define _mm_mask_blend_epi64(k, a, b) simde_mm_mask_blend_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_mm_mask_blend_ps(simde__mmask8 k, simde__m128 a, simde__m128 b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_blend_ps(k, a, b);
  #else
    return simde_mm_mask_mov_ps(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_blend_ps
  #define _mm_mask_blend_ps(k, a, b) simde_mm_mask_blend_ps(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm_mask_blend_pd(simde__mmask8 k, simde__m128d a, simde__m128d b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_blend_pd(k, a, b);
  #else
    return simde_mm_mask_mov_pd(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_blend_pd
  #define _mm_mask_blend_pd(k, a, b) simde_mm_mask_blend_pd(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_blend_epi8(simde__mmask32 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_blend_epi8(k, a, b);
  #else
    return simde_mm256_mask_mov_epi8(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_blend_epi8
  #define _mm256_mask_blend_epi8(k, a, b) simde_mm256_mask_blend_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_blend_epi16(simde__mmask16 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_blend_epi16(k, a, b);
  #else
    return simde_mm256_mask_mov_epi16(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_blend_epi16
  #define _mm256_mask_blend_epi16(k, a, b) simde_mm256_mask_blend_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_blend_epi32(simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_blend_epi32(k, a, b);
  #else
    return simde_mm256_mask_mov_epi32(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_blend_epi32
  #define _mm256_mask_blend_epi32(k, a, b) simde_mm256_mask_blend_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_blend_epi64(simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_blend_epi64(k, a, b);
  #else
    return simde_mm256_mask_mov_epi64(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_blend_epi64
  #define _mm256_mask_blend_epi64(k, a, b) simde_mm256_mask_blend_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_mask_blend_ps(simde__mmask8 k, simde__m256 a, simde__m256 b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_blend_ps(k, a, b);
  #else
    return simde_mm256_mask_mov_ps(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_blend_ps
  #define _mm256_mask_blend_ps(k, a, b) simde_mm256_mask_blend_ps(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_mask_blend_pd(simde__mmask8 k, simde__m256d a, simde__m256d b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_blend_pd(k, a, b);
  #else
    return simde_mm256_mask_mov_pd(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_blend_pd
  #define _mm256_mask_blend_pd(k, a, b) simde_mm256_mask_blend_pd(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_blend_epi8(simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_blend_epi8(k, a, b);
  #else
    return simde_mm512_mask_mov_epi8(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_blend_epi8
  #define _mm512_mask_blend_epi8(k, a, b) simde_mm512_mask_blend_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_blend_epi16(simde__mmask32 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_blend_epi16(k, a, b);
  #else
    return simde_mm512_mask_mov_epi16(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_blend_epi16
  #define _mm512_mask_blend_epi16(k, a, b) simde_mm512_mask_blend_epi16(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_blend_epi32(simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_blend_epi32(k, a, b);
  #else
    return simde_mm512_mask_mov_epi32(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_blend_epi32
  #define _mm512_mask_blend_epi32(k, a, b) simde_mm512_mask_blend_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_blend_epi64(simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_blend_epi64(k, a, b);
  #else
    return simde_mm512_mask_mov_epi64(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_blend_epi64
  #define _mm512_mask_blend_epi64(k, a, b) simde_mm512_mask_blend_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_blend_ps(simde__mmask16 k, simde__m512 a, simde__m512 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_blend_ps(k, a, b);
  #else
    return simde_mm512_mask_mov_ps(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_blend_ps
  #define _mm512_mask_blend_ps(k, a, b) simde_mm512_mask_blend_ps(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_blend_pd(simde__mmask8 k, simde__m512d a, simde__m512d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_blend_pd(k, a, b);
  #else
    return simde_mm512_mask_mov_pd(a, k, b);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_blend_pd
  #define _mm512_mask_blend_pd(k, a, b) simde_mm512_mask_blend_pd(k, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_BLEND_H) */
