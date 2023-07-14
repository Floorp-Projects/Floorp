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

#if !defined(SIMDE_X86_AVX512_ANDNOT_H)
#define SIMDE_X86_AVX512_ANDNOT_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_andnot_ps(a, b) _mm512_andnot_ps(a, b)
#else
  #define simde_mm512_andnot_ps(a, b) simde_mm512_castsi512_ps(simde_mm512_andnot_si512(simde_mm512_castps_si512(a), simde_mm512_castps_si512(b)))
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_andnot_ps
  #define _mm512_andnot_ps(a, b) simde_mm512_andnot_ps(a, b)
#endif

#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_mask_andnot_ps(src, k, a, b) _mm512_mask_andnot_ps((src), (k), (a), (b))
#else
  #define simde_mm512_mask_andnot_ps(src, k, a, b) simde_mm512_castsi512_ps(simde_mm512_mask_andnot_epi32(simde_mm512_castps_si512(src), k, simde_mm512_castps_si512(a), simde_mm512_castps_si512(b)))
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_andnot_ps
  #define _mm512_mask_andnot_ps(src, k, a, b) simde_mm512_mask_andnot_ps(src, k, a, b)
#endif

#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_maskz_andnot_ps(k, a, b) _mm512_maskz_andnot_ps((k), (a), (b))
#else
  #define simde_mm512_maskz_andnot_ps(k, a, b) simde_mm512_castsi512_ps(simde_mm512_maskz_andnot_epi32(k, simde_mm512_castps_si512(a), simde_mm512_castps_si512(b)))
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_andnot_ps
  #define _mm512_maskz_andnot_ps(k, a, b) simde_mm512_maskz_andnot_ps(k, a, b)
#endif

#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_andnot_pd(a, b) _mm512_andnot_pd(a, b)
#else
  #define simde_mm512_andnot_pd(a, b) simde_mm512_castsi512_pd(simde_mm512_andnot_si512(simde_mm512_castpd_si512(a), simde_mm512_castpd_si512(b)))
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_andnot_pd
  #define _mm512_andnot_pd(a, b) simde_mm512_andnot_pd(a, b)
#endif

#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_mask_andnot_pd(src, k, a, b) _mm512_mask_andnot_pd((src), (k), (a), (b))
#else
  #define simde_mm512_mask_andnot_pd(src, k, a, b) simde_mm512_castsi512_pd(simde_mm512_mask_andnot_epi64(simde_mm512_castpd_si512(src), k, simde_mm512_castpd_si512(a), simde_mm512_castpd_si512(b)))
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_andnot_pd
  #define _mm512_mask_andnot_pd(src, k, a, b) simde_mm512_mask_andnot_pd(src, k, a, b)
#endif

#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_maskz_andnot_pd(k, a, b) _mm512_maskz_andnot_pd((k), (a), (b))
#else
  #define simde_mm512_maskz_andnot_pd(k, a, b) simde_mm512_castsi512_pd(simde_mm512_maskz_andnot_epi64(k, simde_mm512_castpd_si512(a), simde_mm512_castpd_si512(b)))
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_andnot_pd
  #define _mm512_maskz_andnot_pd(k, a, b) simde_mm512_maskz_andnot_pd(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_andnot_si512 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_andnot_si512(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if defined(SIMDE_X86_AVX2_NATIVE)
      r_.m256i[0] = simde_mm256_andnot_si256(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_andnot_si256(a_.m256i[1], b_.m256i[1]);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32f) / sizeof(r_.i32f[0])) ; i++) {
        r_.i32f[i] = ~(a_.i32f[i]) & b_.i32f[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#define simde_mm512_andnot_epi32(a, b) simde_mm512_andnot_si512(a, b)
#define simde_mm512_andnot_epi64(a, b) simde_mm512_andnot_si512(a, b)
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_andnot_si512
  #define _mm512_andnot_si512(a, b) simde_mm512_andnot_si512(a, b)
  #undef _mm512_andnot_epi32
  #define _mm512_andnot_epi32(a, b) simde_mm512_andnot_si512(a, b)
  #undef _mm512_andnot_epi64
  #define _mm512_andnot_epi64(a, b) simde_mm512_andnot_si512(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_andnot_epi32(simde__m512i src, simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_andnot_epi32(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_andnot_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_andnot_epi32
  #define _mm512_mask_andnot_epi32(src, k, a, b) simde_mm512_mask_andnot_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_andnot_epi32(simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_andnot_epi32(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_andnot_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_andnot_epi32
  #define _mm512_maskz_andnot_epi32(k, a, b) simde_mm512_maskz_andnot_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_andnot_epi64(simde__m512i src, simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_andnot_epi64(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_andnot_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_andnot_epi64
  #define _mm512_mask_andnot_epi64(src, k, a, b) simde_mm512_mask_andnot_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_andnot_epi64(simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_andnot_epi64(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_andnot_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_andnot_epi64
  #define _mm512_maskz_andnot_epi64(k, a, b) simde_mm512_maskz_andnot_epi64(k, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_ANDNOT_H) */
