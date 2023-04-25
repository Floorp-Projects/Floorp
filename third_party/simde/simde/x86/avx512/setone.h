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

#if !defined(SIMDE_X86_AVX512_SETONE_H)
#define SIMDE_X86_AVX512_SETONE_H

#include "types.h"
#include "cast.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_setone_si512(void) {
  simde__m512i_private r_;

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])) ; i++) {
    r_.i32f[i] = ~HEDLEY_STATIC_CAST(int_fast32_t, 0);
  }

  return simde__m512i_from_private(r_);
}
#define simde_x_mm512_setone_epi32() simde_x_mm512_setone_si512()

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_x_mm512_setone_ps(void) {
  return simde_mm512_castsi512_ps(simde_x_mm512_setone_si512());
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_x_mm512_setone_pd(void) {
  return simde_mm512_castsi512_pd(simde_x_mm512_setone_si512());
}

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SETONE_H) */
