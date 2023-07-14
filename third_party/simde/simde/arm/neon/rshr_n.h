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

#if !defined(SIMDE_ARM_NEON_RSHR_N_H)
#define SIMDE_ARM_NEON_RSHR_N_H

#include "combine.h"
#include "dup_n.h"
#include "get_low.h"
#include "reinterpret.h"
#include "shr_n.h"
#include "sub.h"
#include "tst.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_x_vrshrs_n_s32(int32_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 32) {
  return (a >> ((n == 32) ? 31 : n)) + ((a & HEDLEY_STATIC_CAST(int32_t, UINT32_C(1) << (n - 1))) != 0);
}

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_x_vrshrs_n_u32(uint32_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 32) {
  return ((n == 32) ? 0 : (a >> n)) + ((a & (UINT32_C(1) << (n - 1))) != 0);
}

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vrshrd_n_s64(int64_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 64) {
  return (a >> ((n == 64) ? 63 : n)) + ((a & HEDLEY_STATIC_CAST(int64_t, UINT64_C(1) << (n - 1))) != 0);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vrshrd_n_s64(a, n) vrshrd_n_s64((a), (n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrshrd_n_s64
  #define vrshrd_n_s64(a, n) simde_vrshrd_n_s64((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vrshrd_n_u64(uint64_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 64) {
  return ((n == 64) ? 0 : (a >> n)) + ((a & (UINT64_C(1) << (n - 1))) != 0);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vrshrd_n_u64(a, n) vrshrd_n_u64((a), (n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrshrd_n_u64
  #define vrshrd_n_u64(a, n) simde_vrshrd_n_u64((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vrshrq_n_s8 (const simde_int8x16_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 8) {
  simde_int8x16_private
    r_,
    a_ = simde_int8x16_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(int8_t, (a_.values[i] + (1 << (n - 1))) >> n);
  }

  return simde_int8x16_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrq_n_s8(a, n) vrshrq_n_s8((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshrq_n_s8(a, n) simde_vsubq_s8(simde_vshrq_n_s8((a), (n)), simde_vreinterpretq_s8_u8( \
    simde_vtstq_u8(simde_vreinterpretq_u8_s8(a), \
                   simde_vdupq_n_u8(HEDLEY_STATIC_CAST(uint8_t, 1 << ((n) - 1))))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrq_n_s8
  #define vrshrq_n_s8(a, n) simde_vrshrq_n_s8((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vrshrq_n_s16 (const simde_int16x8_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 16) {
  simde_int16x8_private
    r_,
    a_ = simde_int16x8_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(int16_t, (a_.values[i] + (1 << (n - 1))) >> n);
  }

  return simde_int16x8_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrq_n_s16(a, n) vrshrq_n_s16((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshrq_n_s16(a, n) simde_vsubq_s16(simde_vshrq_n_s16((a), (n)), simde_vreinterpretq_s16_u16( \
    simde_vtstq_u16(simde_vreinterpretq_u16_s16(a),                              \
                    simde_vdupq_n_u16(HEDLEY_STATIC_CAST(uint16_t, 1 << ((n) - 1))))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrq_n_s16
  #define vrshrq_n_s16(a, n) simde_vrshrq_n_s16((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vrshrq_n_s32 (const simde_int32x4_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 32) {
  simde_int32x4_private
    r_,
    a_ = simde_int32x4_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = (a_.values[i] >> ((n == 32) ? 31 : n)) + ((a_.values[i] & HEDLEY_STATIC_CAST(int32_t, UINT32_C(1) << (n - 1))) != 0);
  }

  return simde_int32x4_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrq_n_s32(a, n) vrshrq_n_s32((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshrq_n_s32(a, n) simde_vsubq_s32(simde_vshrq_n_s32((a), (n)), \
    simde_vreinterpretq_s32_u32(simde_vtstq_u32(simde_vreinterpretq_u32_s32(a), \
      simde_vdupq_n_u32(UINT32_C(1) << ((n) - 1)))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrq_n_s32
  #define vrshrq_n_s32(a, n) simde_vrshrq_n_s32((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vrshrq_n_s64 (const simde_int64x2_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 64) {
  simde_int64x2_private
    r_,
    a_ = simde_int64x2_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = (a_.values[i] >> ((n == 64) ? 63 : n)) + ((a_.values[i] & HEDLEY_STATIC_CAST(int64_t, UINT64_C(1) << (n - 1))) != 0);
  }

  return simde_int64x2_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrq_n_s64(a, n) vrshrq_n_s64((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshrq_n_s64(a, n) simde_vsubq_s64(simde_vshrq_n_s64((a), (n)), \
    simde_vreinterpretq_s64_u64(simde_vtstq_u64(simde_vreinterpretq_u64_s64(a), \
      simde_vdupq_n_u64(UINT64_C(1) << ((n) - 1)))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrq_n_s64
  #define vrshrq_n_s64(a, n) simde_vrshrq_n_s64((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vrshrq_n_u8 (const simde_uint8x16_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 8) {
  simde_uint8x16_private
    r_,
    a_ = simde_uint8x16_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(uint8_t, (a_.values[i] + (1 << (n - 1))) >> n);
  }

  return simde_uint8x16_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrq_n_u8(a, n) vrshrq_n_u8((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshrq_n_u8(a, n) simde_vsubq_u8(simde_vshrq_n_u8((a), (n)), \
    simde_vtstq_u8((a), simde_vdupq_n_u8(HEDLEY_STATIC_CAST(uint8_t, 1 << ((n) - 1)))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrq_n_u8
  #define vrshrq_n_u8(a, n) simde_vrshrq_n_u8((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vrshrq_n_u16 (const simde_uint16x8_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 16) {
  simde_uint16x8_private
    r_,
    a_ = simde_uint16x8_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(uint16_t, (a_.values[i] + (1 << (n - 1))) >> n);
  }

  return simde_uint16x8_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrq_n_u16(a, n) vrshrq_n_u16((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshrq_n_u16(a, n) simde_vsubq_u16(simde_vshrq_n_u16((a), (n)), \
    simde_vtstq_u16((a), simde_vdupq_n_u16(HEDLEY_STATIC_CAST(uint16_t, 1 << ((n) - 1)))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrq_n_u16
  #define vrshrq_n_u16(a, n) simde_vrshrq_n_u16((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vrshrq_n_u32 (const simde_uint32x4_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 32) {
  simde_uint32x4_private
    r_,
    a_ = simde_uint32x4_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = ((n == 32) ? 0 : (a_.values[i] >> n)) + ((a_.values[i] & (UINT32_C(1) << (n - 1))) != 0);
  }

  return simde_uint32x4_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrq_n_u32(a, n) vrshrq_n_u32((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshrq_n_u32(a, n) simde_vsubq_u32(simde_vshrq_n_u32((a), (n)), \
    simde_vtstq_u32((a), simde_vdupq_n_u32(UINT32_C(1) << ((n) - 1))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrq_n_u32
  #define vrshrq_n_u32(a, n) simde_vrshrq_n_u32((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vrshrq_n_u64 (const simde_uint64x2_t a, const int n)
  SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 64) {
  simde_uint64x2_private
    r_,
    a_ = simde_uint64x2_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = ((n == 64) ? 0 : (a_.values[i] >> n)) + ((a_.values[i] & (UINT64_C(1) << (n - 1))) != 0);
  }

  return simde_uint64x2_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrq_n_u64(a, n) vrshrq_n_u64((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshrq_n_u64(a, n) simde_vsubq_u64(simde_vshrq_n_u64((a), (n)), \
    simde_vtstq_u64((a), simde_vdupq_n_u64(UINT64_C(1) << ((n) - 1))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrq_n_u64
  #define vrshrq_n_u64(a, n) simde_vrshrq_n_u64((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vrshr_n_s8 (const simde_int8x8_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 8) {
  simde_int8x8_private
    r_,
    a_ = simde_int8x8_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(int8_t, (a_.values[i] + (1 << (n - 1))) >> n);
  }

  return simde_int8x8_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshr_n_s8(a, n) vrshr_n_s8((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshr_n_s8(a, n) simde_vsub_s8(simde_vshr_n_s8((a), (n)), simde_vreinterpret_s8_u8( \
    simde_vtst_u8(simde_vreinterpret_u8_s8(a),                              \
                  simde_vdup_n_u8(HEDLEY_STATIC_CAST(uint8_t, 1 << ((n) - 1))))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshr_n_s8
  #define vrshr_n_s8(a, n) simde_vrshr_n_s8((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vrshr_n_s16 (const simde_int16x4_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 16) {
  simde_int16x4_private
    r_,
    a_ = simde_int16x4_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(int16_t, (a_.values[i] + (1 << (n - 1))) >> n);
  }

  return simde_int16x4_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshr_n_s16(a, n) vrshr_n_s16((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshr_n_s16(a, n) simde_vsub_s16(simde_vshr_n_s16((a), (n)), simde_vreinterpret_s16_u16( \
    simde_vtst_u16(simde_vreinterpret_u16_s16(a), \
                   simde_vdup_n_u16(HEDLEY_STATIC_CAST(uint16_t, 1 << ((n) - 1))))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshr_n_s16
  #define vrshr_n_s16(a, n) simde_vrshr_n_s16((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vrshr_n_s32 (const simde_int32x2_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 32) {
  simde_int32x2_private
    r_,
    a_ = simde_int32x2_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = (a_.values[i] >> ((n == 32) ? 31 : n)) + ((a_.values[i] & HEDLEY_STATIC_CAST(int32_t, UINT32_C(1) << (n - 1))) != 0);
  }

  return simde_int32x2_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshr_n_s32(a, n) vrshr_n_s32((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshr_n_s32(a, n) simde_vsub_s32(simde_vshr_n_s32((a), (n)), \
    simde_vreinterpret_s32_u32(simde_vtst_u32(simde_vreinterpret_u32_s32(a), \
      simde_vdup_n_u32(UINT32_C(1) << ((n) - 1)))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshr_n_s32
  #define vrshr_n_s32(a, n) simde_vrshr_n_s32((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vrshr_n_s64 (const simde_int64x1_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 64) {
  simde_int64x1_private
    r_,
    a_ = simde_int64x1_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = (a_.values[i] >> ((n == 64) ? 63 : n)) + ((a_.values[i] & HEDLEY_STATIC_CAST(int64_t, UINT64_C(1) << (n - 1))) != 0);
  }

  return simde_int64x1_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshr_n_s64(a, n) vrshr_n_s64((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshr_n_s64(a, n) simde_vsub_s64(simde_vshr_n_s64((a), (n)), \
    simde_vreinterpret_s64_u64(simde_vtst_u64(simde_vreinterpret_u64_s64(a), \
      simde_vdup_n_u64(UINT64_C(1) << ((n) - 1)))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshr_n_s64
  #define vrshr_n_s64(a, n) simde_vrshr_n_s64((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vrshr_n_u8 (const simde_uint8x8_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 8) {
  simde_uint8x8_private
    r_,
    a_ = simde_uint8x8_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(uint8_t, (a_.values[i] + (1 << (n - 1))) >> n);
  }

  return simde_uint8x8_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshr_n_u8(a, n) vrshr_n_u8((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshr_n_u8(a, n) simde_vsub_u8(simde_vshr_n_u8((a), (n)), \
    simde_vtst_u8((a), simde_vdup_n_u8(HEDLEY_STATIC_CAST(uint8_t, 1 << ((n) - 1)))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshr_n_u8
  #define vrshr_n_u8(a, n) simde_vrshr_n_u8((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vrshr_n_u16 (const simde_uint16x4_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 16) {
  simde_uint16x4_private
    r_,
    a_ = simde_uint16x4_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(uint16_t, (a_.values[i] + (1 << (n - 1))) >> n);
  }

  return simde_uint16x4_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshr_n_u16(a, n) vrshr_n_u16((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshr_n_u16(a, n) simde_vsub_u16(simde_vshr_n_u16((a), (n)), \
    simde_vtst_u16((a), simde_vdup_n_u16(HEDLEY_STATIC_CAST(uint16_t, 1 << ((n) - 1)))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshr_n_u16
  #define vrshr_n_u16(a, n) simde_vrshr_n_u16((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vrshr_n_u32 (const simde_uint32x2_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 32) {
  simde_uint32x2_private
    r_,
    a_ = simde_uint32x2_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = ((n == 32) ? 0 : (a_.values[i] >> n))  + ((a_.values[i] & (UINT32_C(1) << (n - 1))) != 0);
  }

  return simde_uint32x2_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshr_n_u32(a, n) vrshr_n_u32((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshr_n_u32(a, n) simde_vsub_u32(simde_vshr_n_u32((a), (n)), \
    simde_vtst_u32((a), simde_vdup_n_u32(UINT32_C(1) << ((n) - 1))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshr_n_u32
  #define vrshr_n_u32(a, n) simde_vrshr_n_u32((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vrshr_n_u64 (const simde_uint64x1_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 64) {
  simde_uint64x1_private
    r_,
    a_ = simde_uint64x1_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = ((n == 64) ? 0 : (a_.values[i] >> n))  + ((a_.values[i] & (UINT64_C(1) << (n - 1))) != 0);
  }

  return simde_uint64x1_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshr_n_u64(a, n) vrshr_n_u64((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vrshr_n_u64(a, n) simde_vsub_u64(simde_vshr_n_u64((a), (n)), \
    simde_vtst_u64((a), simde_vdup_n_u64(UINT64_C(1) << ((n) - 1))))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshr_n_u64
  #define vrshr_n_u64(a, n) simde_vrshr_n_u64((a), (n))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_RSHR_N_H) */
