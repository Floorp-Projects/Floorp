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

#if !defined(SIMDE_ARM_SVE_CNT_H)
#define SIMDE_ARM_SVE_CNT_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_svcntb(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcntb();
  #else
    return sizeof(simde_svint8_t) / sizeof(int8_t);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcntb
  #define svcntb() simde_svcntb()
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_svcnth(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcnth();
  #else
    return sizeof(simde_svint16_t) / sizeof(int16_t);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcnth
  #define svcnth() simde_svcnth()
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_svcntw(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcntw();
  #else
    return sizeof(simde_svint32_t) / sizeof(int32_t);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcntw
  #define svcntw() simde_svcntw()
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_svcntd(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcntd();
  #else
    return sizeof(simde_svint64_t) / sizeof(int64_t);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcntd
  #define svcntd() simde_svcntd()
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_CNT_H */
