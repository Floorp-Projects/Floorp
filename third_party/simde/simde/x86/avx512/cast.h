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

#if !defined(SIMDE_X86_AVX512_CAST_H)
#define SIMDE_X86_AVX512_CAST_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_castpd_ps (simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castpd_ps(a);
  #else
    simde__m512 r;
    simde_memcpy(&r, &a, sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castpd_ps
  #define _mm512_castpd_ps(a) simde_mm512_castpd_ps(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_castpd_si512 (simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castpd_si512(a);
  #else
    simde__m512i r;
    simde_memcpy(&r, &a, sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castpd_si512
  #define _mm512_castpd_si512(a) simde_mm512_castpd_si512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_castps_pd (simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castps_pd(a);
  #else
    simde__m512d r;
    simde_memcpy(&r, &a, sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castps_pd
  #define _mm512_castps_pd(a) simde_mm512_castps_pd(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_castps_si512 (simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castps_si512(a);
  #else
    simde__m512i r;
    simde_memcpy(&r, &a, sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castps_si512
  #define _mm512_castps_si512(a) simde_mm512_castps_si512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_castsi512_ps (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castsi512_ps(a);
  #else
    simde__m512 r;
    simde_memcpy(&r, &a, sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castsi512_ps
  #define _mm512_castsi512_ps(a) simde_mm512_castsi512_ps(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_castsi512_pd (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castsi512_pd(a);
  #else
    simde__m512d r;
    simde_memcpy(&r, &a, sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castsi512_pd
  #define _mm512_castsi512_pd(a) simde_mm512_castsi512_pd(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_castpd128_pd512 (simde__m128d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castpd128_pd512(a);
  #else
    simde__m512d_private r_;
    r_.m128d[0] = a;
    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castpd128_pd512
  #define _mm512_castpd128_pd512(a) simde_mm512_castpd128_pd512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_castpd256_pd512 (simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castpd256_pd512(a);
  #else
    simde__m512d_private r_;
    r_.m256d[0] = a;
    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castpd256_pd512
  #define _mm512_castpd256_pd512(a) simde_mm512_castpd256_pd512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_mm512_castpd512_pd128 (simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castpd512_pd128(a);
  #else
    simde__m512d_private a_ = simde__m512d_to_private(a);
    return a_.m128d[0];
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castpd512_pd128
  #define _mm512_castpd512_pd128(a) simde_mm512_castpd512_pd128(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm512_castpd512_pd256 (simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castpd512_pd256(a);
  #else
    simde__m512d_private a_ = simde__m512d_to_private(a);
    return a_.m256d[0];
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castpd512_pd256
  #define _mm512_castpd512_pd256(a) simde_mm512_castpd512_pd256(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_castps128_ps512 (simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castps128_ps512(a);
  #else
    simde__m512_private r_;
    r_.m128[0] = a;
    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castps128_ps512
  #define _mm512_castps128_ps512(a) simde_mm512_castps128_ps512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_castps256_ps512 (simde__m256 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castps256_ps512(a);
  #else
    simde__m512_private r_;
    r_.m256[0] = a;
    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castps256_ps512
  #define _mm512_castps256_ps512(a) simde_mm512_castps256_ps512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_mm512_castps512_ps128 (simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castps512_ps128(a);
  #else
    simde__m512_private a_ = simde__m512_to_private(a);
    return a_.m128[0];
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castps512_ps128
  #define _mm512_castps512_ps128(a) simde_mm512_castps512_ps128(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm512_castps512_ps256 (simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castps512_ps256(a);
  #else
    simde__m512_private a_ = simde__m512_to_private(a);
    return a_.m256[0];
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castps512_ps256
  #define _mm512_castps512_ps256(a) simde_mm512_castps512_ps256(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_castsi128_si512 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castsi128_si512(a);
  #else
    simde__m512i_private r_;
    r_.m128i[0] = a;
    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castsi128_si512
  #define _mm512_castsi128_si512(a) simde_mm512_castsi128_si512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_castsi256_si512 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castsi256_si512(a);
  #else
    simde__m512i_private r_;
    r_.m256i[0] = a;
    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castsi256_si512
  #define _mm512_castsi256_si512(a) simde_mm512_castsi256_si512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_castsi512_si128 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castsi512_si128(a);
  #else
    simde__m512i_private a_ = simde__m512i_to_private(a);
    return a_.m128i[0];
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castsi512_si128
  #define _mm512_castsi512_si128(a) simde_mm512_castsi512_si128(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_castsi512_si256 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_castsi512_si256(a);
  #else
    simde__m512i_private a_ = simde__m512i_to_private(a);
    return a_.m256i[0];
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_castsi512_si256
  #define _mm512_castsi512_si256(a) simde_mm512_castsi512_si256(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_CAST_H) */
