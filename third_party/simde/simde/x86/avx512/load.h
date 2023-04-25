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

#if !defined(SIMDE_X86_AVX512_LOAD_H)
#define SIMDE_X86_AVX512_LOAD_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_load_pd (void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_load_pd(SIMDE_ALIGN_ASSUME_LIKE(mem_addr, simde__m512d));
  #else
    simde__m512d r;
    simde_memcpy(&r, SIMDE_ALIGN_ASSUME_LIKE(mem_addr, simde__m512d), sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_load_pd
  #define _mm512_load_pd(a) simde_mm512_load_pd(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_load_ps (void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_load_ps(SIMDE_ALIGN_ASSUME_LIKE(mem_addr, simde__m512));
  #else
    simde__m512 r;
    simde_memcpy(&r, SIMDE_ALIGN_ASSUME_LIKE(mem_addr, simde__m512), sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_load_ps
  #define _mm512_load_ps(a) simde_mm512_load_ps(a)
#endif
SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_load_si512 (void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_load_si512(SIMDE_ALIGN_ASSUME_LIKE(mem_addr, simde__m512i));
  #else
    simde__m512i r;
    simde_memcpy(&r, SIMDE_ALIGN_ASSUME_LIKE(mem_addr, simde__m512i), sizeof(r));
    return r;
  #endif
}
#define simde_mm512_load_epi8(mem_addr) simde_mm512_load_si512(mem_addr)
#define simde_mm512_load_epi16(mem_addr) simde_mm512_load_si512(mem_addr)
#define simde_mm512_load_epi32(mem_addr) simde_mm512_load_si512(mem_addr)
#define simde_mm512_load_epi64(mem_addr) simde_mm512_load_si512(mem_addr)
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_load_epi8
  #undef _mm512_load_epi16
  #undef _mm512_load_epi32
  #undef _mm512_load_epi64
  #undef _mm512_load_si512
  #define _mm512_load_si512(a) simde_mm512_load_si512(a)
  #define _mm512_load_epi8(a) simde_mm512_load_si512(a)
  #define _mm512_load_epi16(a) simde_mm512_load_si512(a)
  #define _mm512_load_epi32(a) simde_mm512_load_si512(a)
  #define _mm512_load_epi64(a) simde_mm512_load_si512(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_LOAD_H) */
