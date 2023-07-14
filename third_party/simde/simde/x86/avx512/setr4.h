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
 */

#if !defined(SIMDE_X86_AVX512_SETR4_H)
#define SIMDE_X86_AVX512_SETR4_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_setr4_epi32 (int32_t d, int32_t c, int32_t b, int32_t a) {
  simde__m512i_private r_;

  r_.i32[ 0] = d;
  r_.i32[ 1] = c;
  r_.i32[ 2] = b;
  r_.i32[ 3] = a;
  r_.i32[ 4] = d;
  r_.i32[ 5] = c;
  r_.i32[ 6] = b;
  r_.i32[ 7] = a;
  r_.i32[ 8] = d;
  r_.i32[ 9] = c;
  r_.i32[10] = b;
  r_.i32[11] = a;
  r_.i32[12] = d;
  r_.i32[13] = c;
  r_.i32[14] = b;
  r_.i32[15] = a;

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setr4_epi32
  #define _mm512_setr4_epi32(d,c,b,a) simde_mm512_setr4_epi32(d,c,b,a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_setr4_epi64 (int64_t d, int64_t c, int64_t b, int64_t a) {
  simde__m512i_private r_;

  r_.i64[0] = d;
  r_.i64[1] = c;
  r_.i64[2] = b;
  r_.i64[3] = a;
  r_.i64[4] = d;
  r_.i64[5] = c;
  r_.i64[6] = b;
  r_.i64[7] = a;

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setr4_epi64
  #define _mm512_setr4_epi64(d,c,b,a) simde_mm512_setr4_epi64(d,c,b,a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_setr4_ps (simde_float32 d, simde_float32 c, simde_float32 b, simde_float32 a) {
  simde__m512_private r_;

  r_.f32[ 0] = d;
  r_.f32[ 1] = c;
  r_.f32[ 2] = b;
  r_.f32[ 3] = a;
  r_.f32[ 4] = d;
  r_.f32[ 5] = c;
  r_.f32[ 6] = b;
  r_.f32[ 7] = a;
  r_.f32[ 8] = d;
  r_.f32[ 9] = c;
  r_.f32[10] = b;
  r_.f32[11] = a;
  r_.f32[12] = d;
  r_.f32[13] = c;
  r_.f32[14] = b;
  r_.f32[15] = a;

  return simde__m512_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setr4_ps
  #define _mm512_setr4_ps(d,c,b,a) simde_mm512_setr4_ps(d,c,b,a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_setr4_pd (simde_float64 d, simde_float64 c, simde_float64 b, simde_float64 a) {
  simde__m512d_private r_;

  r_.f64[0] = d;
  r_.f64[1] = c;
  r_.f64[2] = b;
  r_.f64[3] = a;
  r_.f64[4] = d;
  r_.f64[5] = c;
  r_.f64[6] = b;
  r_.f64[7] = a;

  return simde__m512d_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setr4_pd
  #define _mm512_setr4_pd(d,c,b,a) simde_mm512_setr4_pd(d,c,b,a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SETR4_H) */
