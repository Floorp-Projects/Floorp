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

#if !defined(SIMDE_ARM_SVE_PTEST_H)
#define SIMDE_ARM_SVE_PTEST_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_bool
simde_svptest_first(simde_svbool_t pg, simde_svbool_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptest_first(pg, op);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE)
    if (HEDLEY_LIKELY(pg.value & 1))
      return op.value & 1;

    if (pg.value == 0 || op.value == 0)
      return 0;

    #if defined(_MSC_VER)
      unsigned long r = 0;
      _BitScanForward64(&r, HEDLEY_STATIC_CAST(uint64_t, pg.value));
      return (op.value >> r) & 1;
    #else
      return (op.value >> __builtin_ctzll(HEDLEY_STATIC_CAST(unsigned long long, pg.value))) & 1;
    #endif
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      if (pg.values_i8[i]) {
        return !!op.values_i8[i];
      }
    }

    return 0;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptest_first
  #define svptest_first(pg, op) simde_svptest_first(pg, op)
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_PTEST_H */
