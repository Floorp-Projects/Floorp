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
 *   2023      Michael R. Crusoe <crusoe@debian.org>
 */

#if !defined(SIMDE_X86_AVX512_KXOR_H)
#define SIMDE_X86_AVX512_KXOR_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_kxor_mask8 (simde__mmask8 a, simde__mmask8 b) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE) \
      && (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    return _kxor_mask8(a, b);
  #else
    return a^b;
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _kxor_mask8
  #define _kxor_mask8(a, b) simde_kxor_mask8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_kxor_mask16 (simde__mmask16 a, simde__mmask16 b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) \
      && (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    return _kxor_mask16(a, b);
  #else
    return a^b;
  #endif
}
#define simde_mm512_kxor(a, b) simde_kxor_mask16(a, b)
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _kxor_mask16
  #undef _mm512_kxor
  #define _kxor_mask16(a, b) simde_kxor_mask16(a, b)
  #define _mm512_kxor(a, b) simde_kxor_mask16(a, b)
#endif


SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_kxor_mask32 (simde__mmask32 a, simde__mmask32 b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) \
      && (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    return _kxor_mask32(a, b);
  #else
    return a^b;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _kxor_mask32
  #define _kxor_mask32(a, b) simde_kxor_mask32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_kxor_mask64 (simde__mmask64 a, simde__mmask64 b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) \
      && (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    return _kxor_mask64(a, b);
  #else
    return a^b;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _kxor_mask64
  #define _kxor_mask64(a, b) simde_kxor_mask64(a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_KXOR_H) */
