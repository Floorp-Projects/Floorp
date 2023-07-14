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
 *   2020      Christopher Moore <moore@free.fr>
 */

#if !defined(SIMDE_X86_AVX512_KSHIFT_H)
#define SIMDE_X86_AVX512_KSHIFT_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_kshiftli_mask16 (simde__mmask16 a, unsigned int count)
    SIMDE_REQUIRE_CONSTANT_RANGE(count, 0, 255) {
  return HEDLEY_STATIC_CAST(simde__mmask16, (count <= 15) ? (a << count) : 0);
}
#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && (!defined(SIMDE_DETECT_CLANG_VERSION) && SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0))
  #define simde_kshiftli_mask16(a, count) _kshiftli_mask16(a, count)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _kshiftli_mask16
  #define _kshiftli_mask16(a, count) simde_kshiftli_mask16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_kshiftli_mask32 (simde__mmask32 a, unsigned int count)
    SIMDE_REQUIRE_CONSTANT_RANGE(count, 0, 255) {
  return (count <= 31) ? (a << count) : 0;
}
#if defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && (!defined(SIMDE_DETECT_CLANG_VERSION) && SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0))
  #define simde_kshiftli_mask32(a, count) _kshiftli_mask32(a, count)
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _kshiftli_mask32
  #define _kshiftli_mask32(a, count) simde_kshiftli_mask32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_kshiftli_mask64 (simde__mmask64 a, unsigned int count)
    SIMDE_REQUIRE_CONSTANT_RANGE(count, 0, 255) {
  return (count <= 63) ? (a << count) : 0;
}
#if defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && (!defined(SIMDE_DETECT_CLANG_VERSION) && SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0))
  #define simde_kshiftli_mask64(a, count) _kshiftli_mask64(a, count)
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _kshiftli_mask64
  #define _kshiftli_mask64(a, count) simde_kshiftli_mask64(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_kshiftli_mask8 (simde__mmask8 a, unsigned int count)
    SIMDE_REQUIRE_CONSTANT_RANGE(count, 0, 255) {
  return HEDLEY_STATIC_CAST(simde__mmask8, (count <= 7) ? (a << count) : 0);
}
#if defined(SIMDE_X86_AVX512DQ_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && (!defined(SIMDE_DETECT_CLANG_VERSION) && SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0))
  #define simde_kshiftli_mask8(a, count) _kshiftli_mask8(a, count)
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _kshiftli_mask8
  #define _kshiftli_mask8(a, count) simde_kshiftli_mask8(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_kshiftri_mask16 (simde__mmask16 a, unsigned int count)
    SIMDE_REQUIRE_CONSTANT_RANGE(count, 0, 255) {
  return HEDLEY_STATIC_CAST(simde__mmask16, (count <= 15) ? (a >> count) : 0);
}
#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && (!defined(SIMDE_DETECT_CLANG_VERSION) && SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0))
  #define simde_kshiftri_mask16(a, count) _kshiftri_mask16(a, count)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _kshiftri_mask16
  #define _kshiftri_mask16(a, count) simde_kshiftri_mask16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_kshiftri_mask32 (simde__mmask32 a, unsigned int count)
    SIMDE_REQUIRE_CONSTANT_RANGE(count, 0, 255) {
  return (count <= 31) ? (a >> count) : 0;
}
#if defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && (!defined(SIMDE_DETECT_CLANG_VERSION) && SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0))
  #define simde_kshiftri_mask32(a, count) _kshiftri_mask32(a, count)
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _kshiftri_mask32
  #define _kshiftri_mask32(a, count) simde_kshiftri_mask32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask64
simde_kshiftri_mask64 (simde__mmask64 a, unsigned int count)
    SIMDE_REQUIRE_CONSTANT_RANGE(count, 0, 255) {
  return (count <= 63) ? (a >> count) : 0;
}
#if defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && (!defined(SIMDE_DETECT_CLANG_VERSION) && SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0))
  #define simde_kshiftri_mask64(a, count) _kshiftri_mask64(a, count)
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _kshiftri_mask64
  #define _kshiftri_mask64(a, count) simde_kshiftri_mask64(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_kshiftri_mask8 (simde__mmask8 a, unsigned int count)
    SIMDE_REQUIRE_CONSTANT_RANGE(count, 0, 255) {
  return HEDLEY_STATIC_CAST(simde__mmask8, (count <= 7) ? (a >> count) : 0);
}
#if defined(SIMDE_X86_AVX512DQ_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && (!defined(SIMDE_DETECT_CLANG_VERSION) && SIMDE_DETECT_CLANG_VERSION_CHECK(8,0,0))
  #define simde_kshiftri_mask8(a, count) _kshiftri_mask8(a, count)
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _kshiftri_mask8
  #define _kshiftri_mask8(a, count) simde_kshiftri_mask8(a, count)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_KSHIFT_H) */
