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

#if !defined(SIMDE_ARM_NEON_UZP_H) && !defined(SIMDE_BUG_INTEL_857088)
#define SIMDE_ARM_NEON_UZP_H

#include "types.h"
#include "uzp1.h"
#include "uzp2.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2x2_t
simde_vuzp_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzp_f32(a, b);
  #else
    simde_float32x2x2_t r = { { simde_vuzp1_f32(a, b), simde_vuzp2_f32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzp_f32
  #define vuzp_f32(a, b) simde_vuzp_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8x2_t
simde_vuzp_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzp_s8(a, b);
  #else
    simde_int8x8x2_t r = { { simde_vuzp1_s8(a, b), simde_vuzp2_s8(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzp_s8
  #define vuzp_s8(a, b) simde_vuzp_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4x2_t
simde_vuzp_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzp_s16(a, b);
  #else
    simde_int16x4x2_t r = { { simde_vuzp1_s16(a, b), simde_vuzp2_s16(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzp_s16
  #define vuzp_s16(a, b) simde_vuzp_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2x2_t
simde_vuzp_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzp_s32(a, b);
  #else
    simde_int32x2x2_t r = { { simde_vuzp1_s32(a, b), simde_vuzp2_s32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzp_s32
  #define vuzp_s32(a, b) simde_vuzp_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8x2_t
simde_vuzp_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzp_u8(a, b);
  #else
    simde_uint8x8x2_t r = { { simde_vuzp1_u8(a, b), simde_vuzp2_u8(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzp_u8
  #define vuzp_u8(a, b) simde_vuzp_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4x2_t
simde_vuzp_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzp_u16(a, b);
  #else
    simde_uint16x4x2_t r = { { simde_vuzp1_u16(a, b), simde_vuzp2_u16(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzp_u16
  #define vuzp_u16(a, b) simde_vuzp_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2x2_t
simde_vuzp_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzp_u32(a, b);
  #else
    simde_uint32x2x2_t r = { { simde_vuzp1_u32(a, b), simde_vuzp2_u32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzp_u32
  #define vuzp_u32(a, b) simde_vuzp_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4x2_t
simde_vuzpq_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzpq_f32(a, b);
  #else
    simde_float32x4x2_t r = { { simde_vuzp1q_f32(a, b), simde_vuzp2q_f32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzpq_f32
  #define vuzpq_f32(a, b) simde_vuzpq_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16x2_t
simde_vuzpq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzpq_s8(a, b);
  #else
    simde_int8x16x2_t r = { { simde_vuzp1q_s8(a, b), simde_vuzp2q_s8(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzpq_s8
  #define vuzpq_s8(a, b) simde_vuzpq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8x2_t
simde_vuzpq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzpq_s16(a, b);
  #else
    simde_int16x8x2_t r = { { simde_vuzp1q_s16(a, b), simde_vuzp2q_s16(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzpq_s16
  #define vuzpq_s16(a, b) simde_vuzpq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4x2_t
simde_vuzpq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzpq_s32(a, b);
  #else
    simde_int32x4x2_t r = { { simde_vuzp1q_s32(a, b), simde_vuzp2q_s32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzpq_s32
  #define vuzpq_s32(a, b) simde_vuzpq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16x2_t
simde_vuzpq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzpq_u8(a, b);
  #else
    simde_uint8x16x2_t r = { { simde_vuzp1q_u8(a, b), simde_vuzp2q_u8(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzpq_u8
  #define vuzpq_u8(a, b) simde_vuzpq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8x2_t
simde_vuzpq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzpq_u16(a, b);
  #else
    simde_uint16x8x2_t r = { { simde_vuzp1q_u16(a, b), simde_vuzp2q_u16(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzpq_u16
  #define vuzpq_u16(a, b) simde_vuzpq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4x2_t
simde_vuzpq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vuzpq_u32(a, b);
  #else
    simde_uint32x4x2_t r = { { simde_vuzp1q_u32(a, b), simde_vuzp2q_u32(a, b) } };
    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vuzpq_u32
  #define vuzpq_u32(a, b) simde_vuzpq_u32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_UZP_H) */
