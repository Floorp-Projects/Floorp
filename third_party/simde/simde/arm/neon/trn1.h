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

#if !defined(SIMDE_ARM_NEON_TRN1_H)
#define SIMDE_ARM_NEON_TRN1_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vtrn1_f32(simde_float32x2_t a, simde_float32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1_f32(a, b);
  #else
    simde_float32x2_private
      r_,
      a_ = simde_float32x2_to_private(a),
      b_ = simde_float32x2_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1_f32
  #define vtrn1_f32(a, b) simde_vtrn1_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vtrn1_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1_s8(a, b);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1_s8
  #define vtrn1_s8(a, b) simde_vtrn1_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vtrn1_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1_s16(a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1_s16
  #define vtrn1_s16(a, b) simde_vtrn1_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vtrn1_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1_s32(a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1_s32
  #define vtrn1_s32(a, b) simde_vtrn1_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vtrn1_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1_u8(a, b);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1_u8
  #define vtrn1_u8(a, b) simde_vtrn1_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vtrn1_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1_u16(a, b);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1_u16
  #define vtrn1_u16(a, b) simde_vtrn1_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vtrn1_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1_u32(a, b);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1_u32
  #define vtrn1_u32(a, b) simde_vtrn1_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vtrn1q_f32(simde_float32x4_t a, simde_float32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_f32(a, b);
  #else
    simde_float32x4_private
      r_,
      a_ = simde_float32x4_to_private(a),
      b_ = simde_float32x4_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_f32
  #define vtrn1q_f32(a, b) simde_vtrn1q_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vtrn1q_f64(simde_float64x2_t a, simde_float64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_f64(a, b);
  #else
    simde_float64x2_private
      r_,
      a_ = simde_float64x2_to_private(a),
      b_ = simde_float64x2_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_f64
  #define vtrn1q_f64(a, b) simde_vtrn1q_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vtrn1q_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_s8(a, b);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_s8
  #define vtrn1q_s8(a, b) simde_vtrn1q_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vtrn1q_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_s16(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_s16
  #define vtrn1q_s16(a, b) simde_vtrn1q_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vtrn1q_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_s32(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_s32
  #define vtrn1q_s32(a, b) simde_vtrn1q_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vtrn1q_s64(simde_int64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_s64(a, b);
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_s64
  #define vtrn1q_s64(a, b) simde_vtrn1q_s64((a), (b))
#endif


SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vtrn1q_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_u8(a, b);
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_u8
  #define vtrn1q_u8(a, b) simde_vtrn1q_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vtrn1q_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_u16(a, b);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_u16
  #define vtrn1q_u16(a, b) simde_vtrn1q_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vtrn1q_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_u32(a, b);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_u32
  #define vtrn1q_u32(a, b) simde_vtrn1q_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vtrn1q_u64(simde_uint64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vtrn1q_u64(a, b);
  #else
    simde_uint64x2_private
      r_,
      a_ = simde_uint64x2_to_private(a),
      b_ = simde_uint64x2_to_private(b);

    const size_t halfway_point = sizeof(r_.values) / sizeof(r_.values[0]) / 2;
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < halfway_point ; i++) {
      const size_t idx = i << 1;
      r_.values[idx] = a_.values[idx];
      r_.values[idx | 1] = b_.values[idx];
    }

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vtrn1q_u64
  #define vtrn1q_u64(a, b) simde_vtrn1q_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_TRN1_H) */
