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

#if !defined(SIMDE_ARM_NEON_MLS_N_H)
#define SIMDE_ARM_NEON_MLS_N_H

#include "sub.h"
#include "dup_n.h"
#include "mls.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vmls_n_f32(simde_float32x2_t a, simde_float32x2_t b, simde_float32 c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmls_n_f32(a, b, c);
  #else
    return simde_vmls_f32(a, b, simde_vdup_n_f32(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmls_n_f32
  #define vmls_n_f32(a, b, c) simde_vmls_n_f32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vmls_n_s16(simde_int16x4_t a, simde_int16x4_t b, int16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmls_n_s16(a, b, c);
  #else
    return simde_vmls_s16(a, b, simde_vdup_n_s16(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmls_n_s16
  #define vmls_n_s16(a, b, c) simde_vmls_n_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vmls_n_s32(simde_int32x2_t a, simde_int32x2_t b, int32_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmls_n_s32(a, b, c);
  #else
    return simde_vmls_s32(a, b, simde_vdup_n_s32(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmls_n_s32
  #define vmls_n_s32(a, b, c) simde_vmls_n_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vmls_n_u16(simde_uint16x4_t a, simde_uint16x4_t b, uint16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmls_n_u16(a, b, c);
  #else
    return simde_vmls_u16(a, b, simde_vdup_n_u16(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmls_n_u16
  #define vmls_n_u16(a, b, c) simde_vmls_n_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vmls_n_u32(simde_uint32x2_t a, simde_uint32x2_t b, uint32_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmls_n_u32(a, b, c);
  #else
    return simde_vmls_u32(a, b, simde_vdup_n_u32(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmls_n_u32
  #define vmls_n_u32(a, b, c) simde_vmls_n_u32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vmlsq_n_f32(simde_float32x4_t a, simde_float32x4_t b, simde_float32 c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlsq_n_f32(a, b, c);
  #else
    return simde_vmlsq_f32(a, b, simde_vdupq_n_f32(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlsq_n_f32
  #define vmlsq_n_f32(a, b, c) simde_vmlsq_n_f32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vmlsq_n_s16(simde_int16x8_t a, simde_int16x8_t b, int16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlsq_n_s16(a, b, c);
  #else
    return simde_vmlsq_s16(a, b, simde_vdupq_n_s16(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlsq_n_s16
  #define vmlsq_n_s16(a, b, c) simde_vmlsq_n_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vmlsq_n_s32(simde_int32x4_t a, simde_int32x4_t b, int32_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlsq_n_s32(a, b, c);
  #else
    return simde_vmlsq_s32(a, b, simde_vdupq_n_s32(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlsq_n_s32
  #define vmlsq_n_s32(a, b, c) simde_vmlsq_n_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vmlsq_n_u16(simde_uint16x8_t a, simde_uint16x8_t b, uint16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlsq_n_u16(a, b, c);
  #else
    return simde_vmlsq_u16(a, b, simde_vdupq_n_u16(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlsq_n_u16
  #define vmlsq_n_u16(a, b, c) simde_vmlsq_n_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vmlsq_n_u32(simde_uint32x4_t a, simde_uint32x4_t b, uint32_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlsq_n_u32(a, b, c);
  #else
    return simde_vmlsq_u32(a, b, simde_vdupq_n_u32(c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlsq_n_u32
  #define vmlsq_n_u32(a, b, c) simde_vmlsq_n_u32((a), (b), (c))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_MLS_N_H) */
