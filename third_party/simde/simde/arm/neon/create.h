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
 *   2020      Christopher Moore <moore@free.fr>
 */

/* N.B. CM: vcreate_f16 and vcreate_bf16 are omitted as
 * SIMDe has no 16-bit floating point support.
 * Idem for the poly types. */

#if !defined(SIMDE_ARM_NEON_CREATE_H)
#define SIMDE_ARM_NEON_CREATE_H

#include "dup_n.h"
#include "reinterpret.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vcreate_s8(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_s8(a);
  #else
    return simde_vreinterpret_s8_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_s8
  #define vcreate_s8(a) simde_vcreate_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vcreate_s16(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_s16(a);
  #else
    return simde_vreinterpret_s16_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_s16
  #define vcreate_s16(a) simde_vcreate_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vcreate_s32(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_s32(a);
  #else
    return simde_vreinterpret_s32_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_s32
  #define vcreate_s32(a) simde_vcreate_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vcreate_s64(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_s64(a);
  #else
    return simde_vreinterpret_s64_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_s64
  #define vcreate_s64(a) simde_vcreate_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vcreate_u8(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_u8(a);
  #else
    return simde_vreinterpret_u8_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_u8
  #define vcreate_u8(a) simde_vcreate_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vcreate_u16(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_u16(a);
  #else
    return simde_vreinterpret_u16_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_u16
  #define vcreate_u16(a) simde_vcreate_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vcreate_u32(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_u32(a);
  #else
    return simde_vreinterpret_u32_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_u32
  #define vcreate_u32(a) simde_vcreate_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vcreate_u64(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_u64(a);
  #else
    return simde_vdup_n_u64(a);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_u64
  #define vcreate_u64(a) simde_vcreate_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vcreate_f32(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vcreate_f32(a);
  #else
    return simde_vreinterpret_f32_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_f32
  #define vcreate_f32(a) simde_vcreate_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vcreate_f64(uint64_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcreate_f64(a);
  #else
    return simde_vreinterpret_f64_u64(simde_vdup_n_u64(a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcreate_f64
  #define vcreate_f64(a) simde_vcreate_f64(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_CREATE_H) */
