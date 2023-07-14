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
 *   2021      Atharva Nimbalkar <atharvakn@gmail.com>
 */

#if !defined(SIMDE_ARM_NEON_BCAX_H)
#define SIMDE_ARM_NEON_BCAX_H

#include "types.h"

#include "eor.h"
#include "bic.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vbcaxq_u8(simde_uint8x16_t a, simde_uint8x16_t b, simde_uint8x16_t c) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
    return vbcaxq_u8(a, b, c);
  #else
    return simde_veorq_u8(a, simde_vbicq_u8(b, c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vbcaxq_u8
  #define vbcaxq_u8(a, b, c) simde_vbcaxq_u8(a, b, c)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vbcaxq_u16(simde_uint16x8_t a, simde_uint16x8_t b, simde_uint16x8_t c) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
    return vbcaxq_u16(a, b, c);
  #else
    return simde_veorq_u16(a, simde_vbicq_u16(b, c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vbcaxq_u16
  #define vbcaxq_u16(a, b, c) simde_vbcaxq_u16(a, b, c)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vbcaxq_u32(simde_uint32x4_t a, simde_uint32x4_t b, simde_uint32x4_t c) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
    return vbcaxq_u32(a, b, c);
  #else
    return simde_veorq_u32(a, simde_vbicq_u32(b, c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vbcaxq_u32
  #define vbcaxq_u32(a, b, c) simde_vbcaxq_u32(a, b, c)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vbcaxq_u64(simde_uint64x2_t a, simde_uint64x2_t b, simde_uint64x2_t c) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
    return vbcaxq_u64(a, b, c);
  #else
    return simde_veorq_u64(a, simde_vbicq_u64(b, c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vbcaxq_u64
  #define vbcaxq_u64(a, b, c) simde_vbcaxq_u64(a, b, c)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vbcaxq_s8(simde_int8x16_t a, simde_int8x16_t b, simde_int8x16_t c) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
    return vbcaxq_s8(a, b, c);
  #else
    return simde_veorq_s8(a, simde_vbicq_s8(b, c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vbcaxq_s8
  #define vbcaxq_s8(a, b, c) simde_vbcaxq_s8(a, b, c)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vbcaxq_s16(simde_int16x8_t a, simde_int16x8_t b, simde_int16x8_t c) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
    return vbcaxq_s16(a, b, c);
  #else
    return simde_veorq_s16(a,simde_vbicq_s16(b, c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vbcaxq_s16
  #define vbcaxq_s16(a, b, c) simde_vbcaxq_s16(a, b, c)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vbcaxq_s32(simde_int32x4_t a, simde_int32x4_t b, simde_int32x4_t c) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
    return vbcaxq_s32(a, b, c);
  #else
    return simde_veorq_s32(a, simde_vbicq_s32(b, c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vbcaxq_s32
  #define vbcaxq_s32(a, b, c) simde_vbcaxq_s32(a, b, c)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vbcaxq_s64(simde_int64x2_t a, simde_int64x2_t b, simde_int64x2_t c) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_SHA3)
    return vbcaxq_s64(a, b, c);
  #else
    return simde_veorq_s64(a, simde_vbicq_s64(b, c));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES) || (defined(SIMDE_ENABLE_NATIVE_ALIASES) && !defined(__ARM_FEATURE_SHA3))
  #undef vbcaxq_s64
  #define vbcaxq_s64(a, b, c) simde_vbcaxq_s64(a, b, c)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_BCAX_H) */
