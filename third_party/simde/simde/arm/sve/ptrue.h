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

#if !defined(SIMDE_ARM_SVE_PTRUE_H)
#define SIMDE_ARM_SVE_PTRUE_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svptrue_b8(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptrue_b8();
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svbool_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r = simde_svbool_from_mmask64(HEDLEY_STATIC_CAST(__mmask64, ~UINT64_C(0)));
    #else
      r = simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0)));
    #endif

    return r;
  #else
    simde_svint8_t r;

    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      r.values[i] = ~INT8_C(0);
    }

    return simde_svbool_from_svint8(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptrue_b8
  #define svptrue_b8() simde_svptrue_b8()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svptrue_b16(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptrue_b16();
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svbool_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r = simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0)));
    #else
      r = simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0)));
    #endif

    return r;
  #else
    simde_svint16_t r;

    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      r.values[i] = ~INT16_C(0);
    }

    return simde_svbool_from_svint16(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptrue_b16
  #define svptrue_b16() simde_svptrue_b16()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svptrue_b32(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptrue_b32();
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svbool_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r = simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0)));
    #else
      r = simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0)));
    #endif

    return r;
  #else
    simde_svint32_t r;

    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      r.values[i] = ~INT32_C(0);
    }

    return simde_svbool_from_svint32(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptrue_b32
  #define svptrue_b32() simde_svptrue_b32()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svptrue_b64(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptrue_b64();
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svbool_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r = simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0)));
    #else
      r = simde_svbool_from_mmask4(HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0)));
    #endif

    return r;
  #else
    simde_svint64_t r;

    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      r.values[i] = ~INT64_C(0);
    }

    return simde_svbool_from_svint64(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptrue_b64
  #define svptrue_b64() simde_svptrue_b64()
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_PTRUE_H */
