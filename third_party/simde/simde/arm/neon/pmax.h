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

#if !defined(SIMDE_ARM_NEON_PMAX_H)
#define SIMDE_ARM_NEON_PMAX_H

#include "types.h"
#include "max.h"
#include "uzp1.h"
#include "uzp2.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32_t
simde_vpmaxs_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxs_f32(a);
  #else
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    return (a_.values[0] > a_.values[1]) ? a_.values[0] : a_.values[1];
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vpmaxs_f32
  #define vpmaxs_f32(a) simde_vpmaxs_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64_t
simde_vpmaxqd_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxqd_f64(a);
  #else
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    return (a_.values[0] > a_.values[1]) ? a_.values[0] : a_.values[1];
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vpmaxqd_f64
  #define vpmaxqd_f64(a) simde_vpmaxqd_f64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vpmax_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpmax_f32(a, b);
  #else
    return simde_vmax_f32(simde_vuzp1_f32(a, b), simde_vuzp2_f32(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmax_f32
  #define vpmax_f32(a, b) simde_vpmax_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vpmax_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpmax_s8(a, b);
  #else
    return simde_vmax_s8(simde_vuzp1_s8(a, b), simde_vuzp2_s8(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmax_s8
  #define vpmax_s8(a, b) simde_vpmax_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vpmax_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpmax_s16(a, b);
  #else
    return simde_vmax_s16(simde_vuzp1_s16(a, b), simde_vuzp2_s16(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmax_s16
  #define vpmax_s16(a, b) simde_vpmax_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vpmax_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpmax_s32(a, b);
  #else
    return simde_vmax_s32(simde_vuzp1_s32(a, b), simde_vuzp2_s32(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmax_s32
  #define vpmax_s32(a, b) simde_vpmax_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vpmax_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpmax_u8(a, b);
  #else
    return simde_vmax_u8(simde_vuzp1_u8(a, b), simde_vuzp2_u8(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmax_u8
  #define vpmax_u8(a, b) simde_vpmax_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vpmax_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpmax_u16(a, b);
  #else
    return simde_vmax_u16(simde_vuzp1_u16(a, b), simde_vuzp2_u16(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmax_u16
  #define vpmax_u16(a, b) simde_vpmax_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vpmax_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpmax_u32(a, b);
  #else
    return simde_vmax_u32(simde_vuzp1_u32(a, b), simde_vuzp2_u32(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmax_u32
  #define vpmax_u32(a, b) simde_vpmax_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vpmaxq_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxq_f32(a, b);
  #else
    return simde_vmaxq_f32(simde_vuzp1q_f32(a, b), simde_vuzp2q_f32(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmaxq_f32
  #define vpmaxq_f32(a, b) simde_vpmaxq_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vpmaxq_f64(simde_float64x2_t a, simde_float64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxq_f64(a, b);
  #else
    return simde_vmaxq_f64(simde_vuzp1q_f64(a, b), simde_vuzp2q_f64(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vpmaxq_f64
  #define vpmaxq_f64(a, b) simde_vpmaxq_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vpmaxq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxq_s8(a, b);
  #else
    return simde_vmaxq_s8(simde_vuzp1q_s8(a, b), simde_vuzp2q_s8(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmaxq_s8
  #define vpmaxq_s8(a, b) simde_vpmaxq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vpmaxq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxq_s16(a, b);
  #else
    return simde_vmaxq_s16(simde_vuzp1q_s16(a, b), simde_vuzp2q_s16(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmaxq_s16
  #define vpmaxq_s16(a, b) simde_vpmaxq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vpmaxq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxq_s32(a, b);
  #else
    return simde_vmaxq_s32(simde_vuzp1q_s32(a, b), simde_vuzp2q_s32(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmaxq_s32
  #define vpmaxq_s32(a, b) simde_vpmaxq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vpmaxq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxq_u8(a, b);
  #else
    return simde_vmaxq_u8(simde_vuzp1q_u8(a, b), simde_vuzp2q_u8(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmaxq_u8
  #define vpmaxq_u8(a, b) simde_vpmaxq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vpmaxq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxq_u16(a, b);
  #else
    return simde_vmaxq_u16(simde_vuzp1q_u16(a, b), simde_vuzp2q_u16(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmaxq_u16
  #define vpmaxq_u16(a, b) simde_vpmaxq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vpmaxq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vpmaxq_u32(a, b);
  #else
    return simde_vmaxq_u32(simde_vuzp1q_u32(a, b), simde_vuzp2q_u32(a, b));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpmaxq_u32
  #define vpmaxq_u32(a, b) simde_vpmaxq_u32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_PMAX_H) */
