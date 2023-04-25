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

/* Implementation notes (seanptmaher):
 *
 * It won't overflow during the multiplication, it'll ever only double
 * the bit length, we only care about the overflow during the shift,
 * so do the multiplication, then the shift with saturation
 */

#if !defined(SIMDE_ARM_NEON_QDMULL_H)
#define SIMDE_ARM_NEON_QDMULL_H

#include "combine.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vqdmullh_s16(int16_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqdmullh_s16(a, b);
  #else
    int32_t mul = (HEDLEY_STATIC_CAST(int32_t, a) * HEDLEY_STATIC_CAST(int32_t, b));
    return (simde_math_labs(mul) & (1 << 30)) ? ((mul < 0) ? INT32_MIN : INT32_MAX) : mul << 1;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqdmullh_s16
  #define vqdmullh_s16(a, b) simde_vqdmullh_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vqdmulls_s32(int32_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqdmulls_s32(a, b);
  #else
    int64_t mul = (HEDLEY_STATIC_CAST(int64_t, a) * HEDLEY_STATIC_CAST(int64_t, b));
    return ((a > 0 ? a : -a) & (HEDLEY_STATIC_CAST(int64_t, 1) << 62)) ? ((mul < 0) ? INT64_MIN : INT64_MAX) : mul << 1;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqdmulls_s16
  #define vqdmulls_s16(a, b) simde_vqdmulls_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vqdmull_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqdmull_s16(a, b);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int32x4_private r_;
    simde_int16x8_private v_ = simde_int16x8_to_private(simde_vcombine_s16(a, b));

    const v128_t lo = wasm_i32x4_extend_low_i16x8(v_.v128);
    const v128_t hi = wasm_i32x4_extend_high_i16x8(v_.v128);

    const v128_t product = wasm_i32x4_mul(lo, hi);
    const v128_t uflow = wasm_i32x4_lt(product, wasm_i32x4_splat(-INT32_C(0x40000000)));
    const v128_t oflow = wasm_i32x4_gt(product, wasm_i32x4_splat( INT32_C(0x3FFFFFFF)));
    r_.v128 = wasm_i32x4_shl(product, 1);
    r_.v128 = wasm_v128_bitselect(wasm_i32x4_splat(INT32_MIN), r_.v128, uflow);
    r_.v128 = wasm_v128_bitselect(wasm_i32x4_splat(INT32_MAX), r_.v128, oflow);

    return simde_int32x4_from_private(r_);
  #else
    simde_int32x4_private r_;
    simde_int16x4_private
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqdmullh_s16(a_.values[i], b_.values[i]);
    }

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqdmull_s16
  #define vqdmull_s16(a, b) simde_vqdmull_s16((a), (b))
#endif
SIMDE_FUNCTION_ATTRIBUTES

simde_int64x2_t
simde_vqdmull_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqdmull_s32(a, b);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int64x2_private r_;
    simde_int32x4_private v_ = simde_int32x4_to_private(simde_vcombine_s32(a, b));

    const v128_t lo = wasm_i64x2_extend_low_i32x4(v_.v128);
    const v128_t hi = wasm_i64x2_extend_high_i32x4(v_.v128);

    const v128_t product = wasm_i64x2_mul(lo, hi);
    const v128_t uflow = wasm_i64x2_lt(product, wasm_i64x2_splat(-INT64_C(0x4000000000000000)));
    const v128_t oflow = wasm_i64x2_gt(product, wasm_i64x2_splat( INT64_C(0x3FFFFFFFFFFFFFFF)));
    r_.v128 = wasm_i64x2_shl(product, 1);
    r_.v128 = wasm_v128_bitselect(wasm_i64x2_splat(INT64_MIN), r_.v128, uflow);
    r_.v128 = wasm_v128_bitselect(wasm_i64x2_splat(INT64_MAX), r_.v128, oflow);

    return simde_int64x2_from_private(r_);
  #else
    simde_int64x2_private r_;
    simde_int32x2_private
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqdmulls_s32(a_.values[i], b_.values[i]);
    }

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqdmull_s32
  #define vqdmull_s32(a, b) simde_vqdmull_s32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QDMULL_H) */
