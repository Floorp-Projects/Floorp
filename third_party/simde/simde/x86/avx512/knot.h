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

#if !defined(SIMDE_X86_AVX512_KNOT_H)
#define SIMDE_X86_AVX512_KNOT_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_knot_mask8 (simde__mmask8 a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE) \
      && (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    return _knot_mask8(a);
  #else
    return ~a;
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _knot_mask8
  #define _knot_mask8(a) simde_knot_mask8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_knot_mask16 (simde__mmask16 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) \
      && (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    return _knot_mask16(a);
  #else
    return ~a;
  #endif
}
#define simde_mm512_knot(a) simde_knot_mask16(a)
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _knot_mask16
  #undef _mm512_knot
  #define _knot_mask16(a) simde_knot_mask16(a)
  #define _mm512_knot(a) simde_knot_mask16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_knot_mask32 (simde__mmask32 a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) \
      && (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    return _knot_mask32(a);
  #else
    return ~a;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _knot_mask32
  #define _knot_mask32(a) simde_knot_mask32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_knot_mask64 (simde__mmask64 a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) \
      && (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    return _knot_mask64(a);
  #else
    return ~a;
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _knot_mask64
  #define _knot_mask64(a) simde_knot_mask64(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_KNOT_H) */
