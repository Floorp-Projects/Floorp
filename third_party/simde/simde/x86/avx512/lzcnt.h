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

#if !defined(SIMDE_X86_AVX512_LZCNT_H)
#define SIMDE_X86_AVX512_LZCNT_H

#include "types.h"
#include "mov.h"
#if HEDLEY_MSVC_VERSION_CHECK(14,0,0)
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
  #if defined(_M_AMD64) || defined(_M_ARM64)
  #pragma intrinsic(_BitScanReverse64)
  #endif
#endif

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if \
    ( HEDLEY_HAS_BUILTIN(__builtin_clz) || \
      HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
      HEDLEY_ARM_VERSION_CHECK(4,1,0) ) && \
    defined(__INT_MAX__) && defined(__LONG_MAX__) && defined(__LONG_LONG_MAX__) && \
    defined(__INT32_MAX__) && defined(__INT64_MAX__)
  #if __INT_MAX__ == __INT32_MAX__
    #define simde_x_clz32(v) __builtin_clz(HEDLEY_STATIC_CAST(unsigned int, (v)))
  #elif __LONG_MAX__ == __INT32_MAX__
    #define simde_x_clz32(v) __builtin_clzl(HEDLEY_STATIC_CAST(unsigned long, (v)))
  #elif __LONG_LONG_MAX__ == __INT32_MAX__
    #define simde_x_clz32(v) __builtin_clzll(HEDLEY_STATIC_CAST(unsigned long long, (v)))
  #endif

  #if __INT_MAX__ == __INT64_MAX__
    #define simde_x_clz64(v) __builtin_clz(HEDLEY_STATIC_CAST(unsigned int, (v)))
  #elif __LONG_MAX__ == __INT64_MAX__
    #define simde_x_clz64(v) __builtin_clzl(HEDLEY_STATIC_CAST(unsigned long, (v)))
  #elif __LONG_LONG_MAX__ == __INT64_MAX__
    #define simde_x_clz64(v) __builtin_clzll(HEDLEY_STATIC_CAST(unsigned long long, (v)))
  #endif
#elif HEDLEY_MSVC_VERSION_CHECK(14,0,0)
  static int simde_x_clz32(uint32_t x) {
    unsigned long r;
    _BitScanReverse(&r, x);
    return 31 - HEDLEY_STATIC_CAST(int, r);
  }
  #define simde_x_clz32 simde_x_clz32

  static int simde_x_clz64(uint64_t x) {
    unsigned long r;

    #if defined(_M_AMD64) || defined(_M_ARM64)
      _BitScanReverse64(&r, x);
      return 63 - HEDLEY_STATIC_CAST(int, r);
    #else
      uint32_t high = HEDLEY_STATIC_CAST(uint32_t, x >> 32);
      if (high != 0)
        return _BitScanReverse(&r, HEDLEY_STATIC_CAST(unsigned long, high));
      else
        return _BitScanReverse(&r, HEDLEY_STATIC_CAST(unsigned long, x & ~UINT32_C(0))) + 32;
    #endif
  }
  #define simde_x_clz64 simde_x_clz64
#endif

#if !defined(simde_x_clz32) || !defined(simde_x_clz64)
  static uint8_t simde_x_avx512cd_lz_lookup(const uint8_t value) {
    static const uint8_t lut[256] = {
      7, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    return lut[value];
  };

  #if !defined(simde_x_clz32)
    static int simde_x_clz32(uint32_t x) {
      size_t s = sizeof(x) * 8;
      uint32_t r;

      while ((s -= 8) != 0) {
        r = x >> s;
        if (r != 0)
          return simde_x_avx512cd_lz_lookup(HEDLEY_STATIC_CAST(uint8_t, r)) +
            (((sizeof(x) - 1) * 8) - s);
      }

      if (x == 0)
        return (int) ((sizeof(x) * 8) - 1);
      else
        return simde_x_avx512cd_lz_lookup(HEDLEY_STATIC_CAST(uint8_t, x)) +
          ((sizeof(x) - 1) * 8);
    }
  #endif

  #if !defined(simde_x_clz64)
    static int simde_x_clz64(uint64_t x) {
      size_t s = sizeof(x) * 8;
      uint64_t r;

      while ((s -= 8) != 0) {
        r = x >> s;
        if (r != 0)
          return simde_x_avx512cd_lz_lookup(HEDLEY_STATIC_CAST(uint8_t, r)) +
            (((sizeof(x) - 1) * 8) - s);
      }

      if (x == 0)
        return (int) ((sizeof(x) * 8) - 1);
      else
        return simde_x_avx512cd_lz_lookup(HEDLEY_STATIC_CAST(uint8_t, x)) +
          ((sizeof(x) - 1) * 8);
    }
  #endif
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_lzcnt_epi32(simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512CD_NATIVE)
    return _mm_lzcnt_epi32(a);
  #elif defined(SIMDE_X86_SSE2_NATIVE)
    /* https://stackoverflow.com/a/58827596/501126 */
    a = _mm_andnot_si128(_mm_srli_epi32(a, 8), a);
    a = _mm_castps_si128(_mm_cvtepi32_ps(a));
    a = _mm_srli_epi32(a, 23);
    a = _mm_subs_epu16(_mm_set1_epi32(158), a);
    a = _mm_min_epi16(a, _mm_set1_epi32(32));
    return a;
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a);

    #if defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_u32 = vec_cntlz(a_.altivec_u32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++) {
        r_.i32[i] = (HEDLEY_UNLIKELY(a_.i32[i] == 0) ? HEDLEY_STATIC_CAST(int32_t, sizeof(int32_t) * CHAR_BIT) : HEDLEY_STATIC_CAST(int32_t, simde_x_clz32(HEDLEY_STATIC_CAST(uint32_t, a_.i32[i]))));
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512CD_ENABLE_NATIVE_ALIASES)
  #undef _mm_lzcnt_epi32
  #define _mm_lzcnt_epi32(a) simde_mm_lzcnt_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_lzcnt_epi32(simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512CD_NATIVE)
    return _mm_mask_lzcnt_epi32(src, k, a);
  #else
    return simde_mm_mask_mov_epi32(src, k, simde_mm_lzcnt_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_lzcnt_epi32
  #define _mm_mask_lzcnt_epi32(src, k, a) simde_mm_mask_lzcnt_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_lzcnt_epi32(simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512CD_NATIVE)
    return _mm_maskz_lzcnt_epi32(k, a);
  #else
    return simde_mm_maskz_mov_epi32(k, simde_mm_lzcnt_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_lzcnt_epi32
  #define _mm_maskz_lzcnt_epi32(k, a) simde_mm_maskz_lzcnt_epi32(k, a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_LZCNT_H) */
