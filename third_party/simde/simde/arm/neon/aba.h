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
 */

#if !defined(SIMDE_ARM_NEON_ABA_H)
#define SIMDE_ARM_NEON_ABA_H

#include "abd.h"
#include "add.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vaba_s8(simde_int8x8_t a, simde_int8x8_t b, simde_int8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaba_s8(a, b, c);
  #else
    return simde_vadd_s8(simde_vabd_s8(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaba_s8
  #define vaba_s8(a, b, c) simde_vaba_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vaba_s16(simde_int16x4_t a, simde_int16x4_t b, simde_int16x4_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaba_s16(a, b, c);
  #else
    return simde_vadd_s16(simde_vabd_s16(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaba_s16
  #define vaba_s16(a, b, c) simde_vaba_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vaba_s32(simde_int32x2_t a, simde_int32x2_t b, simde_int32x2_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaba_s32(a, b, c);
  #else
    return simde_vadd_s32(simde_vabd_s32(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaba_s32
  #define vaba_s32(a, b, c) simde_vaba_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vaba_u8(simde_uint8x8_t a, simde_uint8x8_t b, simde_uint8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaba_u8(a, b, c);
  #else
    return simde_vadd_u8(simde_vabd_u8(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaba_u8
  #define vaba_u8(a, b, c) simde_vaba_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vaba_u16(simde_uint16x4_t a, simde_uint16x4_t b, simde_uint16x4_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaba_u16(a, b, c);
  #else
    return simde_vadd_u16(simde_vabd_u16(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaba_u16
  #define vaba_u16(a, b, c) simde_vaba_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vaba_u32(simde_uint32x2_t a, simde_uint32x2_t b, simde_uint32x2_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vaba_u32(a, b, c);
  #else
    return simde_vadd_u32(simde_vabd_u32(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaba_u32
  #define vaba_u32(a, b, c) simde_vaba_u32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vabaq_s8(simde_int8x16_t a, simde_int8x16_t b, simde_int8x16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabaq_s8(a, b, c);
  #else
    return simde_vaddq_s8(simde_vabdq_s8(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabaq_s8
  #define vabaq_s8(a, b, c) simde_vabaq_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vabaq_s16(simde_int16x8_t a, simde_int16x8_t b, simde_int16x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabaq_s16(a, b, c);
  #else
    return simde_vaddq_s16(simde_vabdq_s16(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabaq_s16
  #define vabaq_s16(a, b, c) simde_vabaq_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vabaq_s32(simde_int32x4_t a, simde_int32x4_t b, simde_int32x4_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabaq_s32(a, b, c);
  #else
    return simde_vaddq_s32(simde_vabdq_s32(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabaq_s32
  #define vabaq_s32(a, b, c) simde_vabaq_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vabaq_u8(simde_uint8x16_t a, simde_uint8x16_t b, simde_uint8x16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabaq_u8(a, b, c);
  #else
    return simde_vaddq_u8(simde_vabdq_u8(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabaq_u8
  #define vabaq_u8(a, b, c) simde_vabaq_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vabaq_u16(simde_uint16x8_t a, simde_uint16x8_t b, simde_uint16x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabaq_u16(a, b, c);
  #else
    return simde_vaddq_u16(simde_vabdq_u16(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabaq_u16
  #define vabaq_u16(a, b, c) simde_vabaq_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vabaq_u32(simde_uint32x4_t a, simde_uint32x4_t b, simde_uint32x4_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vabaq_u32(a, b, c);
  #else
    return simde_vaddq_u32(simde_vabdq_u32(b, c), a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vabaq_u32
  #define vabaq_u32(a, b, c) simde_vabaq_u32((a), (b), (c))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_ABA_H) */
