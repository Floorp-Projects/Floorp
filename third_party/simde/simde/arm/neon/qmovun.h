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

#if !defined(SIMDE_ARM_NEON_QMOVUN_H)
#define SIMDE_ARM_NEON_QMOVUN_H

#include "types.h"
#include "dup_n.h"
#include "min.h"
#include "max.h"
#include "movn.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
uint8_t
simde_vqmovunh_s16(int16_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return HEDLEY_STATIC_CAST(uint8_t, vqmovunh_s16(a));
  #else
    return (a > UINT8_MAX) ? UINT8_MAX : ((a < 0) ? 0 : HEDLEY_STATIC_CAST(uint8_t, a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqmovunh_s16
  #define vqmovunh_s16(a) simde_vqmovunh_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vqmovuns_s32(int32_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return HEDLEY_STATIC_CAST(uint16_t, vqmovuns_s32(a));
  #else
    return (a > UINT16_MAX) ? UINT16_MAX : ((a < 0) ? 0 : HEDLEY_STATIC_CAST(uint16_t, a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqmovuns_s32
  #define vqmovuns_s32(a) simde_vqmovuns_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vqmovund_s64(int64_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return HEDLEY_STATIC_CAST(uint32_t, vqmovund_s64(a));
  #else
    return (a > UINT32_MAX) ? UINT32_MAX : ((a < 0) ? 0 : HEDLEY_STATIC_CAST(uint32_t, a));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqmovund_s64
  #define vqmovund_s64(a) simde_vqmovund_s64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vqmovun_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqmovun_s16(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vmovn_u16(simde_vreinterpretq_u16_s16(simde_vmaxq_s16(simde_vdupq_n_s16(0), simde_vminq_s16(simde_vdupq_n_s16(UINT8_MAX), a))));
  #else
    simde_uint8x8_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqmovunh_s16(a_.values[i]);
    }

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqmovun_s16
  #define vqmovun_s16(a) simde_vqmovun_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vqmovun_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqmovun_s32(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vmovn_u32(simde_vreinterpretq_u32_s32(simde_vmaxq_s32(simde_vdupq_n_s32(0), simde_vminq_s32(simde_vdupq_n_s32(UINT16_MAX), a))));
  #else
    simde_uint16x4_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqmovuns_s32(a_.values[i]);
    }

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqmovun_s32
  #define vqmovun_s32(a) simde_vqmovun_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vqmovun_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqmovun_s64(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE > 0
    return simde_vmovn_u64(simde_vreinterpretq_u64_s64(simde_x_vmaxq_s64(simde_vdupq_n_s64(0), simde_x_vminq_s64(simde_vdupq_n_s64(UINT32_MAX), a))));
  #else
    simde_uint32x2_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqmovund_s64(a_.values[i]);
    }

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqmovun_s64
  #define vqmovun_s64(a) simde_vqmovun_s64((a))
#endif


SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QMOVUN_H) */
