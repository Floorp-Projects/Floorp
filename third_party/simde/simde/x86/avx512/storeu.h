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
 */

#if !defined(SIMDE_X86_AVX512_STOREU_H)
#define SIMDE_X86_AVX512_STOREU_H

#include "types.h"
#include "mov.h"
#include "setzero.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#define simde_mm256_storeu_epi8(mem_addr, a) simde_mm256_storeu_si256(mem_addr, a)
#define simde_mm256_storeu_epi16(mem_addr, a) simde_mm256_storeu_si256(mem_addr, a)
#define simde_mm256_storeu_epi32(mem_addr, a) simde_mm256_storeu_si256(mem_addr, a)
#define simde_mm256_storeu_epi64(mem_addr, a) simde_mm256_storeu_si256(mem_addr, a)
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_storeu_epi8
  #undef _mm256_storeu_epi16
  #define _mm256_storeu_epi8(mem_addr, a) simde_mm512_storeu_si256(mem_addr, a)
  #define _mm256_storeu_epi16(mem_addr, a) simde_mm512_storeu_si256(mem_addr, a)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_storeu_epi32
  #undef _mm256_storeu_epi64
  #define _mm256_storeu_epi32(mem_addr, a) simde_mm512_storeu_si256(mem_addr, a)
  #define _mm256_storeu_epi64(mem_addr, a) simde_mm512_storeu_si256(mem_addr, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm256_mask_storeu_epi16 (void * mem_addr, simde__mmask16 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    _mm256_mask_storeu_epi16(HEDLEY_REINTERPRET_CAST(void*, mem_addr), k, a);
  #else
    const simde__m256i zero = simde_mm256_setzero_si256();
    simde_mm256_storeu_epi16(mem_addr, simde_mm256_mask_mov_epi16(zero, k, a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_storeu_epi16
  #define _mm256_mask_storeu_epi16(mem_addr, k, a) simde_mm256_mask_storeu_epi16(mem_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_storeu_ps (void * mem_addr, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    _mm512_storeu_ps(mem_addr, a);
  #else
    simde_memcpy(mem_addr, &a, sizeof(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_storeu_ps
  #define _mm512_storeu_ps(mem_addr, a) simde_mm512_storeu_ps(mem_addr, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_storeu_pd (void * mem_addr, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    _mm512_storeu_pd(mem_addr, a);
  #else
    simde_memcpy(mem_addr, &a, sizeof(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_storeu_pd
  #define _mm512_storeu_pd(mem_addr, a) simde_mm512_storeu_pd(mem_addr, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_storeu_si512 (void * mem_addr, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    _mm512_storeu_si512(HEDLEY_REINTERPRET_CAST(void*, mem_addr), a);
  #else
    simde_memcpy(mem_addr, &a, sizeof(a));
  #endif
}
#define simde_mm512_storeu_epi8(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
#define simde_mm512_storeu_epi16(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
#define simde_mm512_storeu_epi32(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
#define simde_mm512_storeu_epi64(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_storeu_epi8
  #undef _mm512_storeu_epi16
  #define _mm512_storeu_epi16(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
  #define _mm512_storeu_epi8(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_storeu_epi32
  #undef _mm512_storeu_epi64
  #undef _mm512_storeu_si512
  #define _mm512_storeu_si512(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
  #define _mm512_storeu_epi32(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
  #define _mm512_storeu_epi64(mem_addr, a) simde_mm512_storeu_si512(mem_addr, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_mask_storeu_epi16 (void * mem_addr, simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    _mm512_mask_storeu_epi16(HEDLEY_REINTERPRET_CAST(void*, mem_addr), k, a);
  #else
    const simde__m512i zero = simde_mm512_setzero_si512();
    simde_mm512_storeu_epi16(mem_addr, simde_mm512_mask_mov_epi16(zero, k, a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_storeu_epi16
  #define _mm512_mask_storeu_epi16(mem_addr, k, a) simde_mm512_mask_storeu_epi16(mem_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_mask_storeu_ps (void * mem_addr, simde__mmask16 k, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    _mm512_mask_storeu_ps(HEDLEY_REINTERPRET_CAST(void*, mem_addr), k, a);
  #else
    const simde__m512 zero = simde_mm512_setzero_ps();
    simde_mm512_storeu_ps(mem_addr, simde_mm512_mask_mov_ps(zero, k, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_storeu_ps
  #define _mm512_mask_storeu_ps(mem_addr, k, a) simde_mm512_mask_storeu_ps(mem_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_mask_storeu_pd (void * mem_addr, simde__mmask8 k, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    _mm512_mask_storeu_pd(HEDLEY_REINTERPRET_CAST(void*, mem_addr), k, a);
  #else
    const simde__m512d zero = simde_mm512_setzero_pd();
    simde_mm512_storeu_pd(mem_addr, simde_mm512_mask_mov_pd(zero, k, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_storeu_pd
  #define _mm512_mask_storeu_pd(mem_addr, k, a) simde_mm512_mask_storeu_pd(mem_addr, k, a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_STOREU_H) */
