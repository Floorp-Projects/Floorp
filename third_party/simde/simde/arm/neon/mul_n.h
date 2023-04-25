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

#if !defined(SIMDE_ARM_NEON_MUL_N_H)
#define SIMDE_ARM_NEON_MUL_N_H

#include "types.h"
#include "mul.h"
#include "dup_n.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vmul_n_f32(simde_float32x2_t a, simde_float32 b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmul_n_f32(a, b);
  #else
    return simde_vmul_f32(a, simde_vdup_n_f32(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmul_n_f32
  #define vmul_n_f32(a, b) simde_vmul_n_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vmul_n_f64(simde_float64x1_t a, simde_float64 b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vmul_n_f64(a, b);
  #else
    return simde_vmul_f64(a, simde_vdup_n_f64(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vmul_n_f64
  #define vmul_n_f64(a, b) simde_vmul_n_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vmul_n_s16(simde_int16x4_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmul_n_s16(a, b);
  #else
    return simde_vmul_s16(a, simde_vdup_n_s16(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmul_n_s16
  #define vmul_n_s16(a, b) simde_vmul_n_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vmul_n_s32(simde_int32x2_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmul_n_s32(a, b);
  #else
    return simde_vmul_s32(a, simde_vdup_n_s32(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmul_n_s32
  #define vmul_n_s32(a, b) simde_vmul_n_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vmul_n_u16(simde_uint16x4_t a, uint16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmul_n_u16(a, b);
  #else
    return simde_vmul_u16(a, simde_vdup_n_u16(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmul_n_u16
  #define vmul_n_u16(a, b) simde_vmul_n_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vmul_n_u32(simde_uint32x2_t a, uint32_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmul_n_u32(a, b);
  #else
    return simde_vmul_u32(a, simde_vdup_n_u32(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmul_n_u32
  #define vmul_n_u32(a, b) simde_vmul_n_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vmulq_n_f32(simde_float32x4_t a, simde_float32 b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmulq_n_f32(a, b);
  #else
    return simde_vmulq_f32(a, simde_vdupq_n_f32(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmulq_n_f32
  #define vmulq_n_f32(a, b) simde_vmulq_n_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vmulq_n_f64(simde_float64x2_t a, simde_float64 b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vmulq_n_f64(a, b);
  #else
    return simde_vmulq_f64(a, simde_vdupq_n_f64(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vmulq_n_f64
  #define vmulq_n_f64(a, b) simde_vmulq_n_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vmulq_n_s16(simde_int16x8_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmulq_n_s16(a, b);
  #else
    return simde_vmulq_s16(a, simde_vdupq_n_s16(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmulq_n_s16
  #define vmulq_n_s16(a, b) simde_vmulq_n_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vmulq_n_s32(simde_int32x4_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmulq_n_s32(a, b);
  #else
    return simde_vmulq_s32(a, simde_vdupq_n_s32(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmulq_n_s32
  #define vmulq_n_s32(a, b) simde_vmulq_n_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vmulq_n_u16(simde_uint16x8_t a, uint16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmulq_n_u16(a, b);
  #else
    return simde_vmulq_u16(a, simde_vdupq_n_u16(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmulq_n_u16
  #define vmulq_n_u16(a, b) simde_vmulq_n_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vmulq_n_u32(simde_uint32x4_t a, uint32_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmulq_n_u32(a, b);
  #else
    return simde_vmulq_u32(a, simde_vdupq_n_u32(b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmulq_n_u32
  #define vmulq_n_u32(a, b) simde_vmulq_n_u32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_MUL_N_H) */
