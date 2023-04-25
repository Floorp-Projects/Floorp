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
 */

#if !defined(SIMDE_X86_AVX512_DIV_H)
#define SIMDE_X86_AVX512_DIV_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_div_ps (simde__m512 a, simde__m512 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_div_ps(a, b);
  #else
    simde__m512_private
      r_,
      a_ = simde__m512_to_private(a),
      b_ = simde__m512_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256) / sizeof(r_.m256[0])) ; i++) {
        r_.m256[i] = simde_mm256_div_ps(a_.m256[i], b_.m256[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.f32 = a_.f32 / b_.f32;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m256) / sizeof(r_.m256[0])) ; i++) {
        r_.m256[i] = simde_mm256_div_ps(a_.m256[i], b_.m256[i]);
      }
    #endif

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_div_ps
  #define _mm512_div_ps(a, b) simde_mm512_div_ps(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_div_ps(simde__m512 src, simde__mmask16 k, simde__m512 a, simde__m512 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_div_ps(src, k, a, b);
  #else
    return simde_mm512_mask_mov_ps(src, k, simde_mm512_div_ps(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_div_ps
  #define _mm512_mask_div_ps(src, k, a, b) simde_mm512_mask_div_ps(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_div_ps(simde__mmask16 k, simde__m512 a, simde__m512 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_div_ps(k, a, b);
  #else
    return simde_mm512_maskz_mov_ps(k, simde_mm512_div_ps(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_div_ps
  #define _mm512_maskz_div_ps(k, a, b) simde_mm512_maskz_div_ps(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_div_pd (simde__m512d a, simde__m512d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_div_pd(a, b);
  #else
    simde__m512d_private
      r_,
      a_ = simde__m512d_to_private(a),
      b_ = simde__m512d_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256d) / sizeof(r_.m256d[0])) ; i++) {
        r_.m256d[i] = simde_mm256_div_pd(a_.m256d[i], b_.m256d[i]);
      }
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r_.f64 = a_.f64 / b_.f64;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m256d) / sizeof(r_.m256d[0])) ; i++) {
        r_.m256d[i] = simde_mm256_div_pd(a_.m256d[i], b_.m256d[i]);
      }
    #endif

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_div_pd
  #define _mm512_div_pd(a, b) simde_mm512_div_pd(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_div_pd(simde__m512d src, simde__mmask8 k, simde__m512d a, simde__m512d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_div_pd(src, k, a, b);
  #else
    return simde_mm512_mask_mov_pd(src, k, simde_mm512_div_pd(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_div_pd
  #define _mm512_mask_div_pd(src, k, a, b) simde_mm512_mask_div_pd(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_div_pd(simde__mmask8 k, simde__m512d a, simde__m512d b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_div_pd(k, a, b);
  #else
    return simde_mm512_maskz_mov_pd(k, simde_mm512_div_pd(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_div_pd
  #define _mm512_maskz_div_pd(k, a, b) simde_mm512_maskz_div_pd(k, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_DIV_H) */
