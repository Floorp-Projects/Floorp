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

#if !defined(SIMDE_X86_AVX512_SETR_H)
#define SIMDE_X86_AVX512_SETR_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_setr_epi32 (int32_t e15, int32_t e14, int32_t e13, int32_t e12, int32_t e11, int32_t e10, int32_t  e9, int32_t  e8,
                       int32_t  e7, int32_t  e6, int32_t  e5, int32_t  e4, int32_t  e3, int32_t  e2, int32_t  e1, int32_t  e0) {
  simde__m512i_private r_;

  r_.i32[ 0] = e15;
  r_.i32[ 1] = e14;
  r_.i32[ 2] = e13;
  r_.i32[ 3] = e12;
  r_.i32[ 4] = e11;
  r_.i32[ 5] = e10;
  r_.i32[ 6] = e9;
  r_.i32[ 7] = e8;
  r_.i32[ 8] = e7;
  r_.i32[ 9] = e6;
  r_.i32[10] = e5;
  r_.i32[11] = e4;
  r_.i32[12] = e3;
  r_.i32[13] = e2;
  r_.i32[14] = e1;
  r_.i32[15] = e0;

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setr_epi32
  #define _mm512_setr_epi32(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_setr_epi32(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_setr_epi64 (int64_t e7, int64_t e6, int64_t e5, int64_t e4, int64_t e3, int64_t e2, int64_t e1, int64_t e0) {
  simde__m512i_private r_;

  r_.i64[0] = e7;
  r_.i64[1] = e6;
  r_.i64[2] = e5;
  r_.i64[3] = e4;
  r_.i64[4] = e3;
  r_.i64[5] = e2;
  r_.i64[6] = e1;
  r_.i64[7] = e0;

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setr_epi64
  #define _mm512_setr_epi64(e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_setr_epi64(e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_setr_ps (simde_float32 e15, simde_float32 e14, simde_float32 e13, simde_float32 e12,
                    simde_float32 e11, simde_float32 e10, simde_float32  e9, simde_float32  e8,
                    simde_float32  e7, simde_float32  e6, simde_float32  e5, simde_float32  e4,
                    simde_float32  e3, simde_float32  e2, simde_float32  e1, simde_float32  e0) {
  simde__m512_private r_;

  r_.f32[ 0] = e15;
  r_.f32[ 1] = e14;
  r_.f32[ 2] = e13;
  r_.f32[ 3] = e12;
  r_.f32[ 4] = e11;
  r_.f32[ 5] = e10;
  r_.f32[ 6] = e9;
  r_.f32[ 7] = e8;
  r_.f32[ 8] = e7;
  r_.f32[ 9] = e6;
  r_.f32[10] = e5;
  r_.f32[11] = e4;
  r_.f32[12] = e3;
  r_.f32[13] = e2;
  r_.f32[14] = e1;
  r_.f32[15] = e0;

  return simde__m512_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setr_ps
  #define _mm512_setr_ps(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_setr_ps(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_setr_pd (simde_float64 e7, simde_float64 e6, simde_float64 e5, simde_float64 e4, simde_float64 e3, simde_float64 e2, simde_float64 e1, simde_float64 e0) {
  simde__m512d_private r_;

  r_.f64[0] = e7;
  r_.f64[1] = e6;
  r_.f64[2] = e5;
  r_.f64[3] = e4;
  r_.f64[4] = e3;
  r_.f64[5] = e2;
  r_.f64[6] = e1;
  r_.f64[7] = e0;

  return simde__m512d_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_setr_pd
  #define _mm512_setr_pd(e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_setr_pd(e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SETR_H) */
