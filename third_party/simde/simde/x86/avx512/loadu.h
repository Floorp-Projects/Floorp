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

#if !defined(SIMDE_X86_AVX512_LOADU_H)
#define SIMDE_X86_AVX512_LOADU_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_loadu_ps (void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    #if defined(SIMDE_BUG_CLANG_REV_298042)
      return _mm512_loadu_ps(SIMDE_ALIGN_CAST(const float *, mem_addr));
    #else
      return _mm512_loadu_ps(mem_addr);
    #endif
  #else
    simde__m512 r;
    simde_memcpy(&r, mem_addr, sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_loadu_ps
  #define _mm512_loadu_ps(a) simde_mm512_loadu_ps(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_loadu_pd (void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    #if defined(SIMDE_BUG_CLANG_REV_298042)
      return _mm512_loadu_pd(SIMDE_ALIGN_CAST(const double *, mem_addr));
    #else
      return _mm512_loadu_pd(mem_addr);
    #endif
  #else
    simde__m512d r;
    simde_memcpy(&r, mem_addr, sizeof(r));
    return r;
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_loadu_pd
  #define _mm512_loadu_pd(a) simde_mm512_loadu_pd(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_loadu_si512 (void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_loadu_si512(HEDLEY_REINTERPRET_CAST(void const*, mem_addr));
  #else
    simde__m512i r;

    #if HEDLEY_GNUC_HAS_ATTRIBUTE(may_alias,3,3,0)
      HEDLEY_DIAGNOSTIC_PUSH
      SIMDE_DIAGNOSTIC_DISABLE_PACKED_
      struct simde_mm512_loadu_si512_s {
        __typeof__(r) v;
      } __attribute__((__packed__, __may_alias__));
      r = HEDLEY_REINTERPRET_CAST(const struct simde_mm512_loadu_si512_s *, mem_addr)->v;
      HEDLEY_DIAGNOSTIC_POP
    #else
      simde_memcpy(&r, mem_addr, sizeof(r));
    #endif

    return r;
  #endif
}
#define simde_mm512_loadu_epi8(mem_addr) simde_mm512_loadu_si512(mem_addr)
#define simde_mm512_loadu_epi16(mem_addr) simde_mm512_loadu_si512(mem_addr)
#define simde_mm512_loadu_epi32(mem_addr) simde_mm512_loadu_si512(mem_addr)
#define simde_mm512_loadu_epi64(mem_addr) simde_mm512_loadu_si512(mem_addr)
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_loadu_epi8
  #undef _mm512_loadu_epi16
  #define _mm512_loadu_epi8(a) simde_mm512_loadu_si512(a)
  #define _mm512_loadu_epi16(a) simde_mm512_loadu_si512(a)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_loadu_epi32
  #undef _mm512_loadu_epi64
  #undef _mm512_loadu_si512
  #define _mm512_loadu_si512(a) simde_mm512_loadu_si512(a)
  #define _mm512_loadu_epi32(a) simde_mm512_loadu_si512(a)
  #define _mm512_loadu_epi64(a) simde_mm512_loadu_si512(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_loadu_epi16 (simde__mmask16 k, void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_loadu_epi16(k, HEDLEY_REINTERPRET_CAST(void const*, mem_addr));
  #else
    return simde_mm256_maskz_mov_epi16(k, simde_mm256_loadu_epi16(mem_addr));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_loadu_epi16
  #define _mm256_maskz_loadu_epi16(k, mem_addr) simde_mm256_maskz_loadu_epi16(k, mem_addr)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_loadu_epi16 (simde__m512i src, simde__mmask32 k, void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_loadu_epi16(src, k, HEDLEY_REINTERPRET_CAST(void const*, mem_addr));
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_loadu_epi16(mem_addr));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_loadu_epi16
  #define _mm512_mask_loadu_epi16(src, k, mem_addr) simde_mm512_mask_loadu_epi16(src, k, mem_addr)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_loadu_ps (simde__mmask16 k, void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_loadu_ps(k, HEDLEY_REINTERPRET_CAST(void const*, mem_addr));
  #else
    return simde_mm512_maskz_mov_ps(k, simde_mm512_loadu_ps(mem_addr));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_loadu_ps
  #define _mm256_maskz_loadu_ps(k, mem_addr) simde_mm256_maskz_loadu_ps(k, mem_addr)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_loadu_pd (simde__mmask8 k, void const * mem_addr) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_loadu_pd(k, HEDLEY_REINTERPRET_CAST(void const*, mem_addr));
  #else
    return simde_mm512_maskz_mov_pd(k, simde_mm512_loadu_pd(mem_addr));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_loadu_pd
  #define _mm256_maskz_loadu_pd(k, mem_addr) simde_mm256_maskz_loadu_pd(k, mem_addr)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_LOADU_H) */
