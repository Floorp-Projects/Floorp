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
 *   2021      Atharva Nimbalkar <atharvakn@gmail.com>
 */

#if !defined(SIMDE_ARM_NEON_XAR_H)
#define SIMDE_ARM_NEON_XAR_H

#include "types.h"
#include "eor.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vxarq_u64(simde_uint64x2_t a, simde_uint64x2_t b, const int d)
    SIMDE_REQUIRE_CONSTANT_RANGE(d, 0, 63) {
  simde_uint64x2_private
    r_,
    t = simde_uint64x2_to_private(simde_veorq_u64(a,b));

  SIMDE_VECTORIZE
  for (size_t i=0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = ((t.values[i] >> d) | (t.values[i] << (64 - d)));
  }

  return simde_uint64x2_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
  #define simde_vxarq_u64(a, b, d) vxarq_u64((a), (b), (d))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vxarq_u64
  #define vxarq_u64(a, b, d) simde_vxarq_u64((a), (b), (d))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_XAR_H) */
