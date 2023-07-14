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

#if !defined(SIMDE_X86_AVX512_CMPNEQ_H)
#define SIMDE_X86_AVX512_CMPNEQ_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"
#include "mov_mask.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm_cmpneq_epi8_mask(simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cmpneq_epi8_mask(a, b);
  #else
    return ~simde_mm_movepi8_mask(simde_mm_cmpeq_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpneq_epi8_mask
  #define _mm_cmpneq_epi8_mask(a, b) simde_mm_cmpneq_epi8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm_mask_cmpneq_epi8_mask(simde__mmask16 k1, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_cmpneq_epi8_mask(k1, a, b);
  #else
    return simde_mm_cmpneq_epi8_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpneq_epi8_mask
  #define _mm_mask_cmpneq_epi8_mask(a, b) simde_mm_mask_cmpneq_epi8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm_cmpneq_epu8_mask(simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cmpneq_epu8_mask(a, b);
  #else
    return simde_mm_cmpneq_epi8_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpneq_epu8_mask
  #define _mm_cmpneq_epu8_mask(a, b) simde_mm_cmpneq_epu8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm_mask_cmpneq_epu8_mask(simde__mmask16 k1, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_cmpneq_epu8_mask(k1, a, b);
  #else
    return simde_mm_mask_cmpneq_epi8_mask(k1, a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpneq_epu8_mask
  #define _mm_mask_cmpneq_epu8_mask(a, b) simde_mm_mask_cmpneq_epu8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpneq_epi16_mask(simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cmpneq_epi16_mask(a, b);
  #else
    return ~simde_mm_movepi16_mask(simde_mm_cmpeq_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpneq_epi16_mask
  #define _mm_cmpneq_epi16_mask(a, b) simde_mm_cmpneq_epi16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpneq_epi16_mask(simde__mmask8 k1, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_cmpneq_epi16_mask(k1, a, b);
  #else
    return simde_mm_cmpneq_epi16_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpneq_epi16_mask
  #define _mm_mask_cmpneq_epi16_mask(a, b) simde_mm_mask_cmpneq_epi16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpneq_epu16_mask(simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_cmpneq_epu16_mask(a, b);
  #else
    return simde_mm_cmpneq_epi16_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpneq_epu16_mask
  #define _mm_cmpneq_epu16_mask(a, b) simde_mm_cmpneq_epu16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpneq_epu16_mask(simde__mmask8 k1, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_cmpneq_epu16_mask(k1, a, b);
  #else
    return simde_mm_mask_cmpneq_epi16_mask(k1, a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpneq_epu16_mask
  #define _mm_mask_cmpneq_epu16_mask(a, b) simde_mm_mask_cmpneq_epu16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpneq_epi32_mask(simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_cmpneq_epi32_mask(a, b);
  #else
    return (~simde_mm_movepi32_mask(simde_mm_cmpeq_epi32(a, b))) & 15;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpneq_epi32_mask
  #define _mm_cmpneq_epi32_mask(a, b) simde_mm_cmpneq_epi32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpneq_epi32_mask(simde__mmask8 k1, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_cmpneq_epi32_mask(k1, a, b);
  #else
    return simde_mm_cmpneq_epi32_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpneq_epi32_mask
  #define _mm_mask_cmpneq_epi32_mask(a, b) simde_mm_mask_cmpneq_epi32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpneq_epu32_mask(simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_cmpneq_epu32_mask(a, b);
  #else
    return simde_mm_cmpneq_epi32_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpneq_epu32_mask
  #define _mm_cmpneq_epu32_mask(a, b) simde_mm_cmpneq_epu32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpneq_epu32_mask(simde__mmask8 k1, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_cmpneq_epu32_mask(k1, a, b);
  #else
    return simde_mm_mask_cmpneq_epi32_mask(k1, a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpneq_epu32_mask
  #define _mm_mask_cmpneq_epu32_mask(a, b) simde_mm_mask_cmpneq_epu32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpneq_epi64_mask(simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_cmpneq_epi64_mask(a, b);
  #else
    return (~simde_mm_movepi64_mask(simde_mm_cmpeq_epi64(a, b))) & 3;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpneq_epi64_mask
  #define _mm_cmpneq_epi64_mask(a, b) simde_mm_cmpneq_epi64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpneq_epi64_mask(simde__mmask8 k1, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_cmpneq_epi64_mask(k1, a, b);
  #else
    return simde_mm_cmpneq_epi64_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpneq_epi64_mask
  #define _mm_mask_cmpneq_epi64_mask(a, b) simde_mm_mask_cmpneq_epi64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_cmpneq_epu64_mask(simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_cmpneq_epu64_mask(a, b);
  #else
    return simde_mm_cmpneq_epi64_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmpneq_epu64_mask
  #define _mm_cmpneq_epu64_mask(a, b) simde_mm_cmpneq_epu64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm_mask_cmpneq_epu64_mask(simde__mmask8 k1, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_cmpneq_epu64_mask(k1, a, b);
  #else
    return simde_mm_mask_cmpneq_epi64_mask(k1, a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cmpneq_epu64_mask
  #define _mm_mask_cmpneq_epu64_mask(a, b) simde_mm_mask_cmpneq_epu64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm256_cmpneq_epi8_mask(simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cmpneq_epi8_mask(a, b);
  #else
    return ~simde_mm256_movepi8_mask(simde_mm256_cmpeq_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpneq_epi8_mask
  #define _mm256_cmpneq_epi8_mask(a, b) simde_mm256_cmpneq_epi8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm256_mask_cmpneq_epi8_mask(simde__mmask32 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_cmpneq_epi8_mask(k1, a, b);
  #else
    return simde_mm256_cmpneq_epi8_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpneq_epi8_mask
  #define _mm256_mask_cmpneq_epi8_mask(a, b) simde_mm256_mask_cmpneq_epi8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm256_cmpneq_epu8_mask(simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cmpneq_epu8_mask(a, b);
  #else
    return simde_mm256_cmpneq_epi8_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpneq_epu8_mask
  #define _mm256_cmpneq_epu8_mask(a, b) simde_mm256_cmpneq_epu8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm256_mask_cmpneq_epu8_mask(simde__mmask32 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_cmpneq_epu8_mask(k1, a, b);
  #else
    return simde_mm256_mask_cmpneq_epi8_mask(k1, a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpneq_epu8_mask
  #define _mm256_mask_cmpneq_epu8_mask(a, b) simde_mm256_mask_cmpneq_epu8_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm256_cmpneq_epi16_mask(simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cmpneq_epi16_mask(a, b);
  #else
    return ~simde_mm256_movepi16_mask(simde_mm256_cmpeq_epi16(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpneq_epi16_mask
  #define _mm256_cmpneq_epi16_mask(a, b) simde_mm256_cmpneq_epi16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm256_mask_cmpneq_epi16_mask(simde__mmask16 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_cmpneq_epi16_mask(k1, a, b);
  #else
    return simde_mm256_cmpneq_epi16_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpneq_epi16_mask
  #define _mm256_mask_cmpneq_epi16_mask(a, b) simde_mm256_mask_cmpneq_epi16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm256_cmpneq_epu16_mask(simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_cmpneq_epu16_mask(a, b);
  #else
    return simde_mm256_cmpneq_epi16_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpneq_epu16_mask
  #define _mm256_cmpneq_epu16_mask(a, b) simde_mm256_cmpneq_epu16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm256_mask_cmpneq_epu16_mask(simde__mmask16 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm256_mask_cmpneq_epu16_mask(k1, a, b);
  #else
    return simde_mm256_mask_cmpneq_epi16_mask(k1, a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpneq_epu16_mask
  #define _mm256_mask_cmpneq_epu16_mask(a, b) simde_mm256_mask_cmpneq_epu16_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_cmpneq_epi32_mask(simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_cmpneq_epi32_mask(a, b);
  #else
    return (~simde_mm256_movepi32_mask(simde_mm256_cmpeq_epi32(a, b)));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpneq_epi32_mask
  #define _mm256_cmpneq_epi32_mask(a, b) simde_mm256_cmpneq_epi32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_cmpneq_epi32_mask(simde__mmask8 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_cmpneq_epi32_mask(k1, a, b);
  #else
    return simde_mm256_cmpneq_epi32_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpneq_epi32_mask
  #define _mm256_mask_cmpneq_epi32_mask(a, b) simde_mm256_mask_cmpneq_epi32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_cmpneq_epu32_mask(simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_cmpneq_epu32_mask(a, b);
  #else
    return simde_mm256_cmpneq_epi32_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpneq_epu32_mask
  #define _mm256_cmpneq_epu32_mask(a, b) simde_mm256_cmpneq_epu32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_cmpneq_epu32_mask(simde__mmask8 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_cmpneq_epu32_mask(k1, a, b);
  #else
    return simde_mm256_mask_cmpneq_epi32_mask(k1, a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpneq_epu32_mask
  #define _mm256_mask_cmpneq_epu32_mask(a, b) simde_mm256_mask_cmpneq_epu32_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_cmpneq_epi64_mask(simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_cmpneq_epi64_mask(a, b);
  #else
    return (~simde_mm256_movepi64_mask(simde_mm256_cmpeq_epi64(a, b))) & 15;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpneq_epi64_mask
  #define _mm256_cmpneq_epi64_mask(a, b) simde_mm256_cmpneq_epi64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_cmpneq_epi64_mask(simde__mmask8 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_cmpneq_epi64_mask(k1, a, b);
  #else
    return simde_mm256_cmpneq_epi64_mask(a, b) & k1;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpneq_epi64_mask
  #define _mm256_mask_cmpneq_epi64_mask(a, b) simde_mm256_mask_cmpneq_epi64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_cmpneq_epu64_mask(simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_cmpneq_epu64_mask(a, b);
  #else
    return simde_mm256_cmpneq_epi64_mask(a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmpneq_epu64_mask
  #define _mm256_cmpneq_epu64_mask(a, b) simde_mm256_cmpneq_epu64_mask((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm256_mask_cmpneq_epu64_mask(simde__mmask8 k1, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_cmpneq_epu64_mask(k1, a, b);
  #else
    return simde_mm256_mask_cmpneq_epi64_mask(k1, a, b);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_cmpneq_epu64_mask
  #define _mm256_mask_cmpneq_epu64_mask(a, b) simde_mm256_mask_cmpneq_epu64_mask((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_CMPNEQ_H) */
