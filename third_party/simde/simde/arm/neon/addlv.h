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

#if !defined(SIMDE_ARM_NEON_ADDLV_H)
#define SIMDE_ARM_NEON_ADDLV_H

#include "types.h"
#include "movl.h"
#include "addv.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int16_t
simde_vaddlv_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlv_s8(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vaddvq_s16(simde_vmovl_s8(a));
  #else
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    int16_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlv_s8
  #define vaddlv_s8(a) simde_vaddlv_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vaddlv_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlv_s16(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vaddvq_s32(simde_vmovl_s16(a));
  #else
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    int32_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlv_s16
  #define vaddlv_s16(a) simde_vaddlv_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vaddlv_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlv_s32(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vaddvq_s64(simde_vmovl_s32(a));
  #else
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    int64_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlv_s32
  #define vaddlv_s32(a) simde_vaddlv_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vaddlv_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlv_u8(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vaddvq_u16(simde_vmovl_u8(a));
  #else
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    uint16_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlv_u8
  #define vaddlv_u8(a) simde_vaddlv_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vaddlv_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlv_u16(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vaddvq_u32(simde_vmovl_u16(a));
  #else
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    uint32_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlv_u16
  #define vaddlv_u16(a) simde_vaddlv_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vaddlv_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlv_u32(a);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vaddvq_u64(simde_vmovl_u32(a));
  #else
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    uint64_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlv_u32
  #define vaddlv_u32(a) simde_vaddlv_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int16_t
simde_vaddlvq_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlvq_s8(a);
  #else
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    int16_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlvq_s8
  #define vaddlvq_s8(a) simde_vaddlvq_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vaddlvq_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlvq_s16(a);
  #else
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    int32_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlvq_s16
  #define vaddlvq_s16(a) simde_vaddlvq_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vaddlvq_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlvq_s32(a);
  #else
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    int64_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlvq_s32
  #define vaddlvq_s32(a) simde_vaddlvq_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vaddlvq_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlvq_u8(a);
  #else
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    uint16_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlvq_u8
  #define vaddlvq_u8(a) simde_vaddlvq_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vaddlvq_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlvq_u16(a);
  #else
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    uint32_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlvq_u16
  #define vaddlvq_u16(a) simde_vaddlvq_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vaddlvq_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vaddlvq_u32(a);
  #else
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    uint64_t r = 0;

    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vaddlvq_u32
  #define vaddlvq_u32(a) simde_vaddlvq_u32(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_ADDLV_H) */
