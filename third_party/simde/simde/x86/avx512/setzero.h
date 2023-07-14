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
 *   2020      Christopher Moore <moore@free.fr>
 */

#if !defined(SIMDE_X86_AVX512_SETZERO_H)
#define SIMDE_X86_AVX512_SETZERO_H

#include "types.h"
#include "cast.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_setzero_si512(void) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_setzero_si512();
  #else
    simde__m512i r;
    simde_memset(&r, 0, sizeof(r));
    return r;
  #endif
}
#define simde_mm512_setzero_epi32() simde_mm512_setzero_si512()
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setzero_si512
  #define _mm512_setzero_si512() simde_mm512_setzero_si512()
  #undef _mm512_setzero_epi32
  #define _mm512_setzero_epi32() simde_mm512_setzero_si512()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_setzero_ps(void) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_setzero_ps();
  #else
    return simde_mm512_castsi512_ps(simde_mm512_setzero_si512());
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setzero_ps
  #define _mm512_setzero_ps() simde_mm512_setzero_ps()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_setzero_pd(void) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_setzero_pd();
  #else
    return simde_mm512_castsi512_pd(simde_mm512_setzero_si512());
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setzero_pd
  #define _mm512_setzero_pd() simde_mm512_setzero_pd()
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SETZERO_H) */
