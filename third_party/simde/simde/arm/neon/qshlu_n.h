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

#if !defined(SIMDE_ARM_NEON_QSHLU_N_H)
#define SIMDE_ARM_NEON_QSHLU_N_H

#include "types.h"
#if defined(SIMDE_WASM_SIMD128_NATIVE)
  #include "reinterpret.h"
  #include "movl.h"
  #include "movn.h"
  #include "combine.h"
  #include "get_low.h"
#endif

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
uint8_t
simde_vqshlub_n_s8(int8_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 7) {
  uint8_t r = HEDLEY_STATIC_CAST(uint8_t, a << n);
  r |= (((r >> n) != HEDLEY_STATIC_CAST(uint8_t, a)) ? UINT8_MAX : 0);
  return (a < 0) ? 0 : r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqshlub_n_s8(a, n) HEDLEY_STATIC_CAST(uint8_t, vqshlub_n_s8(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshlub_n_s8
  #define vqshlub_n_s8(a, n) simde_vqshlub_n_s8((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vqshlus_n_s32(int32_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 31) {
  uint32_t r = HEDLEY_STATIC_CAST(uint32_t, a << n);
  r |= (((r >> n) != HEDLEY_STATIC_CAST(uint32_t, a)) ? UINT32_MAX : 0);
  return (a < 0) ? 0 : r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqshlus_n_s32(a, n) HEDLEY_STATIC_CAST(uint32_t, vqshlus_n_s32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshlus_n_s32
  #define vqshlus_n_s32(a, n) simde_vqshlus_n_s32((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vqshlud_n_s64(int64_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 63) {
  uint32_t r = HEDLEY_STATIC_CAST(uint32_t, a << n);
  r |= (((r >> n) != HEDLEY_STATIC_CAST(uint32_t, a)) ? UINT32_MAX : 0);
  return (a < 0) ? 0 : r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqshlud_n_s64(a, n) HEDLEY_STATIC_CAST(uint64_t, vqshlud_n_s64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshlud_n_s64
  #define vqshlud_n_s64(a, n) simde_vqshlud_n_s64((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vqshlu_n_s8(simde_int8x8_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 7) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int16x8_private
      R_,
      A_ = simde_int16x8_to_private(simde_vmovl_s8(a));

    const v128_t shifted = wasm_i16x8_shl(A_.v128, HEDLEY_STATIC_CAST(uint32_t, n));
    R_.v128 = wasm_i16x8_min(shifted, wasm_i16x8_const_splat(UINT8_MAX));
    R_.v128 = wasm_i16x8_max(R_.v128, wasm_i16x8_const_splat(0));

    return simde_vmovn_u16(simde_vreinterpretq_u16_s16( simde_int16x8_from_private(R_)));
  #else
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_uint8x8_private r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      __typeof__(r_.values) shifted = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values) << n;

      __typeof__(r_.values) overflow = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (shifted >> n) != HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values));

      r_.values = (shifted & ~overflow) | overflow;

      r_.values &= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (a_.values >= 0));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint8_t, a_.values[i] << n);
        r_.values[i] |= (((r_.values[i] >> n) != HEDLEY_STATIC_CAST(uint8_t, a_.values[i])) ? UINT8_MAX : 0);
        r_.values[i] = (a_.values[i] < 0) ? 0 : r_.values[i];
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshlu_n_s8(a, n) vqshlu_n_s8(a, n)
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlu_n_s8
  #define vqshlu_n_s8(a, n) simde_vqshlu_n_s8((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vqshlu_n_s16(simde_int16x4_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 15) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int32x4_private
      R_,
      A_ = simde_int32x4_to_private(simde_vmovl_s16(a));

    const v128_t shifted = wasm_i32x4_shl(A_.v128, HEDLEY_STATIC_CAST(uint32_t, n));
    R_.v128 = wasm_i32x4_min(shifted, wasm_i32x4_const_splat(UINT16_MAX));
    R_.v128 = wasm_i32x4_max(R_.v128, wasm_i32x4_const_splat(0));

    return simde_vmovn_u32(simde_vreinterpretq_u32_s32( simde_int32x4_from_private(R_)));
  #else
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_uint16x4_private r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      __typeof__(r_.values) shifted = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values) << n;

      __typeof__(r_.values) overflow = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (shifted >> n) != HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values));

      r_.values = (shifted & ~overflow) | overflow;

      r_.values &= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (a_.values >= 0));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint16_t, a_.values[i] << n);
        r_.values[i] |= (((r_.values[i] >> n) != HEDLEY_STATIC_CAST(uint16_t, a_.values[i])) ? UINT16_MAX : 0);
        r_.values[i] = (a_.values[i] < 0) ? 0 : r_.values[i];
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshlu_n_s16(a, n) vqshlu_n_s16(a, n)
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlu_n_s16
  #define vqshlu_n_s16(a, n) simde_vqshlu_n_s16((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vqshlu_n_s32(simde_int32x2_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 31) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int64x2_private
      R_,
      A_ = simde_int64x2_to_private(simde_vmovl_s32(a));

    const v128_t max = wasm_i64x2_const_splat(UINT32_MAX);

    const v128_t shifted = wasm_i64x2_shl(A_.v128, HEDLEY_STATIC_CAST(uint32_t, n));
    R_.v128 = wasm_v128_bitselect(shifted, max, wasm_i64x2_gt(max, shifted));
    R_.v128 = wasm_v128_and(R_.v128, wasm_i64x2_gt(R_.v128, wasm_i64x2_const_splat(0)));

    return simde_vmovn_u64(simde_vreinterpretq_u64_s64( simde_int64x2_from_private(R_)));
  #else
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_uint32x2_private r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      __typeof__(r_.values) shifted = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values) << n;

      __typeof__(r_.values) overflow = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (shifted >> n) != HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values));

      r_.values = (shifted & ~overflow) | overflow;

      r_.values &= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (a_.values >= 0));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint32_t, a_.values[i] << n);
        r_.values[i] |= (((r_.values[i] >> n) != HEDLEY_STATIC_CAST(uint32_t, a_.values[i])) ? UINT32_MAX : 0);
        r_.values[i] = (a_.values[i] < 0) ? 0 : r_.values[i];
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshlu_n_s32(a, n) vqshlu_n_s32(a, n)
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlu_n_s32
  #define vqshlu_n_s32(a, n) simde_vqshlu_n_s32((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vqshlu_n_s64(simde_int64x1_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 63) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_uint64x2_private
      R_,
      A_ = simde_uint64x2_to_private(simde_vreinterpretq_u64_s64(simde_vcombine_s64(a, a)));

    R_.v128 = wasm_i64x2_shl(A_.v128, HEDLEY_STATIC_CAST(uint32_t, n));
    const v128_t overflow = wasm_i64x2_ne(A_.v128, wasm_u64x2_shr(R_.v128, HEDLEY_STATIC_CAST(uint32_t, n)));
    R_.v128 = wasm_v128_or(R_.v128, overflow);
    R_.v128 = wasm_v128_andnot(R_.v128, wasm_i64x2_shr(A_.v128, 63));

    return simde_vget_low_u64(simde_uint64x2_from_private(R_));
  #else
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_uint64x1_private r_;

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      __typeof__(r_.values) shifted = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values) << n;

      __typeof__(r_.values) overflow = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (shifted >> n) != HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values));

      r_.values = (shifted & ~overflow) | overflow;

      r_.values &= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (a_.values >= 0));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint64_t, a_.values[i] << n);
        r_.values[i] |= (((r_.values[i] >> n) != HEDLEY_STATIC_CAST(uint64_t, a_.values[i])) ? UINT64_MAX : 0);
        r_.values[i] = (a_.values[i] < 0) ? 0 : r_.values[i];
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshlu_n_s64(a, n) vqshlu_n_s64(a, n)
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlu_n_s64
  #define vqshlu_n_s64(a, n) simde_vqshlu_n_s64((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vqshluq_n_s8(simde_int8x16_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 7) {
  simde_int8x16_private a_ = simde_int8x16_to_private(a);
  simde_uint8x16_private r_;

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    r_.v128 = wasm_i8x16_shl(a_.v128, HEDLEY_STATIC_CAST(uint32_t, n));
    const v128_t overflow = wasm_i8x16_ne(a_.v128, wasm_u8x16_shr(r_.v128, HEDLEY_STATIC_CAST(uint32_t, n)));
    r_.v128 = wasm_v128_or(r_.v128, overflow);
    r_.v128 = wasm_v128_andnot(r_.v128, wasm_i8x16_shr(a_.v128, 7));
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
    __typeof__(r_.values) shifted = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values) << n;

    __typeof__(r_.values) overflow = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (shifted >> n) != HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values));

    r_.values = (shifted & ~overflow) | overflow;

    r_.values &= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (a_.values >= 0));
  #else
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = HEDLEY_STATIC_CAST(uint8_t, a_.values[i] << n);
      r_.values[i] |= (((r_.values[i] >> n) != HEDLEY_STATIC_CAST(uint8_t, a_.values[i])) ? UINT8_MAX : 0);
      r_.values[i] = (a_.values[i] < 0) ? 0 : r_.values[i];
    }
  #endif

  return simde_uint8x16_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshluq_n_s8(a, n) vqshluq_n_s8(a, n)
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshluq_n_s8
  #define vqshluq_n_s8(a, n) simde_vqshluq_n_s8((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vqshluq_n_s16(simde_int16x8_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 15) {
  simde_int16x8_private a_ = simde_int16x8_to_private(a);
  simde_uint16x8_private r_;

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    r_.v128 = wasm_i16x8_shl(a_.v128, HEDLEY_STATIC_CAST(uint32_t, n));
    const v128_t overflow = wasm_i16x8_ne(a_.v128, wasm_u16x8_shr(r_.v128, HEDLEY_STATIC_CAST(uint32_t, n)));
    r_.v128 = wasm_v128_or(r_.v128, overflow);
    r_.v128 = wasm_v128_andnot(r_.v128, wasm_i16x8_shr(a_.v128, 15));
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
    __typeof__(r_.values) shifted = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values) << n;

    __typeof__(r_.values) overflow = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (shifted >> n) != HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values));

    r_.values = (shifted & ~overflow) | overflow;

    r_.values &= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (a_.values >= 0));
  #else
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = HEDLEY_STATIC_CAST(uint16_t, a_.values[i] << n);
      r_.values[i] |= (((r_.values[i] >> n) != HEDLEY_STATIC_CAST(uint16_t, a_.values[i])) ? UINT16_MAX : 0);
      r_.values[i] = (a_.values[i] < 0) ? 0 : r_.values[i];
    }
  #endif

  return simde_uint16x8_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshluq_n_s16(a, n) vqshluq_n_s16(a, n)
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshluq_n_s16
  #define vqshluq_n_s16(a, n) simde_vqshluq_n_s16((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vqshluq_n_s32(simde_int32x4_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 31) {
  simde_int32x4_private a_ = simde_int32x4_to_private(a);
  simde_uint32x4_private r_;

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    r_.v128 = wasm_i32x4_shl(a_.v128, HEDLEY_STATIC_CAST(uint32_t, n));
    const v128_t overflow = wasm_i32x4_ne(a_.v128, wasm_u32x4_shr(r_.v128, HEDLEY_STATIC_CAST(uint32_t, n)));
    r_.v128 = wasm_v128_or(r_.v128, overflow);
    r_.v128 = wasm_v128_andnot(r_.v128, wasm_i32x4_shr(a_.v128, 31));
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
    __typeof__(r_.values) shifted = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values) << n;

    __typeof__(r_.values) overflow = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (shifted >> n) != HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values));

    r_.values = (shifted & ~overflow) | overflow;

    r_.values &= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (a_.values >= 0));
  #else
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = HEDLEY_STATIC_CAST(uint32_t, a_.values[i] << n);
      r_.values[i] |= (((r_.values[i] >> n) != HEDLEY_STATIC_CAST(uint32_t, a_.values[i])) ? UINT32_MAX : 0);
      r_.values[i] = (a_.values[i] < 0) ? 0 : r_.values[i];
    }
  #endif

  return simde_uint32x4_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshluq_n_s32(a, n) vqshluq_n_s32(a, n)
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshluq_n_s32
  #define vqshluq_n_s32(a, n) simde_vqshluq_n_s32((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vqshluq_n_s64(simde_int64x2_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 0, 63) {
  simde_int64x2_private a_ = simde_int64x2_to_private(a);
  simde_uint64x2_private r_;

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    r_.v128 = wasm_i64x2_shl(a_.v128, HEDLEY_STATIC_CAST(uint32_t, n));
    const v128_t overflow = wasm_i64x2_ne(a_.v128, wasm_u64x2_shr(r_.v128, HEDLEY_STATIC_CAST(uint32_t, n)));
    r_.v128 = wasm_v128_or(r_.v128, overflow);
    r_.v128 = wasm_v128_andnot(r_.v128, wasm_i64x2_shr(a_.v128, 63));
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
    __typeof__(r_.values) shifted = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values) << n;

    __typeof__(r_.values) overflow = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (shifted >> n) != HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), a_.values));

    r_.values = (shifted & ~overflow) | overflow;

    r_.values &= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (a_.values >= 0));
  #else
    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = HEDLEY_STATIC_CAST(uint64_t, a_.values[i] << n);
      r_.values[i] |= (((r_.values[i] >> n) != HEDLEY_STATIC_CAST(uint64_t, a_.values[i])) ? UINT64_MAX : 0);
      r_.values[i] = (a_.values[i] < 0) ? 0 : r_.values[i];
    }
  #endif

  return simde_uint64x2_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshluq_n_s64(a, n) vqshluq_n_s64(a, n)
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshluq_n_s64
  #define vqshluq_n_s64(a, n) simde_vqshluq_n_s64((a), (n))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QSHLU_N_H) */
