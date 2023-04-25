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
 *   2021      Zhi An Ng <zhin@google.com> (Copyright owned by Google, LLC)
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_NEON_RSQRTS_H)
#define SIMDE_ARM_NEON_RSQRTS_H

#include "types.h"
#include "mls.h"
#include "mul_n.h"
#include "dup_n.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32_t
simde_vrsqrtss_f32(simde_float32_t a, simde_float32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrsqrtss_f32(a, b);
  #else
    return SIMDE_FLOAT32_C(0.5) * (SIMDE_FLOAT32_C(3.0) - (a * b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrsqrtss_f32
  #define vrsqrtss_f32(a, b) simde_vrsqrtss_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64_t
simde_vrsqrtsd_f64(simde_float64_t a, simde_float64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrsqrtsd_f64(a, b);
  #else
    return SIMDE_FLOAT64_C(0.5) * (SIMDE_FLOAT64_C(3.0) - (a * b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrsqrtsd_f64
  #define vrsqrtsd_f64(a, b) simde_vrsqrtsd_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vrsqrts_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrsqrts_f32(a, b);
  #else
    return
      simde_vmul_n_f32(
        simde_vmls_f32(
          simde_vdup_n_f32(SIMDE_FLOAT32_C(3.0)),
          a,
          b),
        SIMDE_FLOAT32_C(0.5)
      );
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrsqrts_f32
  #define vrsqrts_f32(a, b) simde_vrsqrts_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vrsqrts_f64(simde_float64x1_t a, simde_float64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrsqrts_f64(a, b);
  #else
    return
      simde_vmul_n_f64(
        simde_vmls_f64(
          simde_vdup_n_f64(SIMDE_FLOAT64_C(3.0)),
          a,
          b),
        SIMDE_FLOAT64_C(0.5)
      );
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrsqrts_f64
  #define vrsqrts_f64(a, b) simde_vrsqrts_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vrsqrtsq_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrsqrtsq_f32(a, b);
  #else
    return
      simde_vmulq_n_f32(
        simde_vmlsq_f32(
          simde_vdupq_n_f32(SIMDE_FLOAT32_C(3.0)),
          a,
          b),
        SIMDE_FLOAT32_C(0.5)
      );
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrsqrtsq_f32
  #define vrsqrtsq_f32(a, b) simde_vrsqrtsq_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vrsqrtsq_f64(simde_float64x2_t a, simde_float64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrsqrtsq_f64(a, b);
  #else
    return
      simde_vmulq_n_f64(
        simde_vmlsq_f64(
          simde_vdupq_n_f64(SIMDE_FLOAT64_C(3.0)),
          a,
          b),
        SIMDE_FLOAT64_C(0.5)
      );
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrsqrtsq_f64
  #define vrsqrtsq_f64(a, b) simde_vrsqrtsq_f64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP
#endif /* !defined(SIMDE_ARM_NEON_RSQRTS_H) */
