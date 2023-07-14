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

#if !defined(SIMDE_ARM_NEON_TRN_H) && !defined(SIMDE_BUG_INTEL_857088)
#define SIMDE_ARM_NEON_TRN_H

#include "types.h"
#include "trn1.h"
#include "trn2.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2x2_t
simde_vtrn_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrn_f32(a, b);
  #else
    simde_float32x2x2_t r = { { simde_vtrn1_f32(a, b), simde_vtrn2_f32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrn_f32
  #define vtrn_f32(a, b) simde_vtrn_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8x2_t
simde_vtrn_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrn_s8(a, b);
  #else
    simde_int8x8x2_t r = { { simde_vtrn1_s8(a, b), simde_vtrn2_s8(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrn_s8
  #define vtrn_s8(a, b) simde_vtrn_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4x2_t
simde_vtrn_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrn_s16(a, b);
  #else
    simde_int16x4x2_t r = { { simde_vtrn1_s16(a, b), simde_vtrn2_s16(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrn_s16
  #define vtrn_s16(a, b) simde_vtrn_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2x2_t
simde_vtrn_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrn_s32(a, b);
  #else
    simde_int32x2x2_t r = { { simde_vtrn1_s32(a, b), simde_vtrn2_s32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrn_s32
  #define vtrn_s32(a, b) simde_vtrn_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8x2_t
simde_vtrn_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrn_u8(a, b);
  #else
    simde_uint8x8x2_t r = { { simde_vtrn1_u8(a, b), simde_vtrn2_u8(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrn_u8
  #define vtrn_u8(a, b) simde_vtrn_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4x2_t
simde_vtrn_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrn_u16(a, b);
  #else
    simde_uint16x4x2_t r = { { simde_vtrn1_u16(a, b), simde_vtrn2_u16(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrn_u16
  #define vtrn_u16(a, b) simde_vtrn_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2x2_t
simde_vtrn_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrn_u32(a, b);
  #else
    simde_uint32x2x2_t r = { { simde_vtrn1_u32(a, b), simde_vtrn2_u32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrn_u32
  #define vtrn_u32(a, b) simde_vtrn_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4x2_t
simde_vtrnq_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrnq_f32(a, b);
  #else
    simde_float32x4x2_t r = { { simde_vtrn1q_f32(a, b), simde_vtrn2q_f32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrnq_f32
  #define vtrnq_f32(a, b) simde_vtrnq_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16x2_t
simde_vtrnq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrnq_s8(a, b);
  #else
    simde_int8x16x2_t r = { { simde_vtrn1q_s8(a, b), simde_vtrn2q_s8(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrnq_s8
  #define vtrnq_s8(a, b) simde_vtrnq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8x2_t
simde_vtrnq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrnq_s16(a, b);
  #else
    simde_int16x8x2_t r = { { simde_vtrn1q_s16(a, b), simde_vtrn2q_s16(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrnq_s16
  #define vtrnq_s16(a, b) simde_vtrnq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4x2_t
simde_vtrnq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrnq_s32(a, b);
  #else
    simde_int32x4x2_t r = { { simde_vtrn1q_s32(a, b), simde_vtrn2q_s32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrnq_s32
  #define vtrnq_s32(a, b) simde_vtrnq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16x2_t
simde_vtrnq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrnq_u8(a, b);
  #else
    simde_uint8x16x2_t r = { { simde_vtrn1q_u8(a, b), simde_vtrn2q_u8(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrnq_u8
  #define vtrnq_u8(a, b) simde_vtrnq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8x2_t
simde_vtrnq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrnq_u16(a, b);
  #else
    simde_uint16x8x2_t r = { { simde_vtrn1q_u16(a, b), simde_vtrn2q_u16(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrnq_u16
  #define vtrnq_u16(a, b) simde_vtrnq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4x2_t
simde_vtrnq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtrnq_u32(a, b);
  #else
    simde_uint32x4x2_t r = { { simde_vtrn1q_u32(a, b), simde_vtrn2q_u32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtrnq_u32
  #define vtrnq_u32(a, b) simde_vtrnq_u32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_TRN_H) */
