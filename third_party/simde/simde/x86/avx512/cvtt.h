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

#if !defined(SIMDE_X86_AVX512_CVTT_H)
#define SIMDE_X86_AVX512_CVTT_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_cvttpd_epi64 (simde__m128d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm_cvttpd_epi64(a);
  #else
    simde__m128i_private r_;
    simde__m128d_private a_ = simde__m128d_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_ARCH_AMD64)
      r_.n =
        _mm_set_epi64x(
          _mm_cvttsd_si64(_mm_unpackhi_pd(a_.n, a_.n)),
          _mm_cvttsd_si64(a_.n)
        );
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r_.neon_i64 = vcvtq_s64_f64(a_.neon_f64);
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_i64 = vec_signed(a_.altivec_f64);
    #elif defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.i64, a_.f64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
        r_.i64[i] = HEDLEY_STATIC_CAST(int64_t, a_.f64[i]);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm_cvttpd_epi64
  #define _mm_cvttpd_epi64(a) simde_mm_cvttpd_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_cvttpd_epi64(simde__m128i src, simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm_mask_cvttpd_epi64(src, k, a);
  #else
    return simde_mm_mask_mov_epi64(src, k, simde_mm_cvttpd_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_cvttpd_epi64
  #define _mm_mask_cvttpd_epi64(src, k, a) simde_mm_mask_cvttpd_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_cvttpd_epi64(simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm_maskz_cvttpd_epi64(k, a);
  #else
    return simde_mm_maskz_mov_epi64(k, simde_mm_cvttpd_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_cvttpd_epi64
  #define _mm_maskz_cvttpd_epi64(k, a) simde_mm_maskz_cvttpd_epi64(k, a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_CVTT_H) */
