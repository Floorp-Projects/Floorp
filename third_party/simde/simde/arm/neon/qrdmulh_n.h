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
 *   2020      Sean Maher <seanptmaher@gmail.com> (Copyright owned by Google, LLC)
 */

#if !defined(SIMDE_ARM_NEON_QRDMULH_N_H)
#define SIMDE_ARM_NEON_QRDMULH_N_H

#include "types.h"

#include "combine.h"
#include "qrdmulh.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vqrdmulh_n_s16(simde_int16x4_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqrdmulh_n_s16(a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a);


    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqrdmulhh_s16(a_.values[i], b);
    }

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulh_n_s16
  #define vqrdmulh_n_s16(a, b) simde_vqrdmulh_n_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vqrdmulh_n_s32(simde_int32x2_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqrdmulh_n_s32(a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqrdmulhs_s32(a_.values[i], b);
    }

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulh_n_s32
  #define vqrdmulh_n_s32(a, b) simde_vqrdmulh_n_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vqrdmulhq_n_s16(simde_int16x8_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqrdmulhq_n_s16(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqrdmulhh_s16(a_.values[i], b);
    }

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhq_n_s16
  #define vqrdmulhq_n_s16(a, b) simde_vqrdmulhq_n_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vqrdmulhq_n_s32(simde_int32x4_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqrdmulhq_n_s32(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqrdmulhs_s32(a_.values[i], b);
    }

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhq_n_s32
  #define vqrdmulhq_n_s32(a, b) simde_vqrdmulhq_n_s32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QRDMULH_H) */
