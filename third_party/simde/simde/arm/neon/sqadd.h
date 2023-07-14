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

#if !defined(SIMDE_ARM_NEON_SQADD_H)
#define SIMDE_ARM_NEON_SQADD_H

#include "types.h"
#include <limits.h>

// Workaround on ARM64 windows due to windows SDK bug
// https://developercommunity.visualstudio.com/t/In-arm64_neonh-vsqaddb_u8-vsqaddh_u16/10271747?sort=newest
#if (defined _MSC_VER) && (defined SIMDE_ARM_NEON_A64V8_NATIVE)
#undef vsqaddb_u8
#define vsqaddb_u8(src1, src2) neon_usqadds8(__uint8ToN8_v(src1), __int8ToN8_v(src2)).n8_u8[0]
#undef vsqaddh_u16
#define vsqaddh_u16(src1, src2) neon_usqadds16(__uint16ToN16_v(src1), __int16ToN16_v(src2)).n16_u16[0]
#undef vsqadds_u32
#define vsqadds_u32(src1, src2) _CopyUInt32FromFloat(neon_usqadds32(_CopyFloatFromUInt32(src1), _CopyFloatFromInt32(src2)))
#undef vsqaddd_u64
#define vsqaddd_u64(src1, src2) neon_usqadds64(__uint64ToN64_v(src1), __int64ToN64_v(src2)).n64_u64[0]
#endif

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
uint8_t
simde_vsqaddb_u8(uint8_t a, int8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(SIMDE_BUG_CLANG_REV_365298)
      return vsqaddb_u8(a, HEDLEY_STATIC_CAST(uint8_t, b));
    #else
      return vsqaddb_u8(a, b);
    #endif
  #else
    int16_t r_ = HEDLEY_STATIC_CAST(int16_t, a) + HEDLEY_STATIC_CAST(int16_t, b);
    return (r_ < 0) ? 0 : ((r_ > UINT8_MAX) ? UINT8_MAX : HEDLEY_STATIC_CAST(uint8_t, r_));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqaddb_u8
  #define vsqaddb_u8(a, b) simde_vsqaddb_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vsqaddh_u16(uint16_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(SIMDE_BUG_CLANG_REV_365298)
      return vsqaddh_u16(a, HEDLEY_STATIC_CAST(uint16_t, b));
    #else
      return vsqaddh_u16(a, b);
    #endif
  #else
    int32_t r_ = HEDLEY_STATIC_CAST(int32_t, a) + HEDLEY_STATIC_CAST(int32_t, b);
    return (r_ < 0) ? 0 : ((r_ > UINT16_MAX) ? UINT16_MAX : HEDLEY_STATIC_CAST(uint16_t, r_));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqaddh_u16
  #define vsqaddh_u16(a, b) simde_vsqaddh_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vsqadds_u32(uint32_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(SIMDE_BUG_CLANG_REV_365298)
      return vsqadds_u32(a, HEDLEY_STATIC_CAST(uint32_t, b));
    #else
      return vsqadds_u32(a, b);
    #endif
  #else
    int64_t r_ = HEDLEY_STATIC_CAST(int64_t, a) + HEDLEY_STATIC_CAST(int64_t, b);
    return (r_ < 0) ? 0 : ((r_ > UINT32_MAX) ? UINT32_MAX : HEDLEY_STATIC_CAST(uint32_t, r_));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqadds_u32
  #define vsqadds_u32(a, b) simde_vsqadds_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vsqaddd_u64(uint64_t a, int64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(SIMDE_BUG_CLANG_REV_365298)
      return vsqaddd_u64(a, HEDLEY_STATIC_CAST(uint64_t, b));
    #else
      return vsqaddd_u64(a, b);
    #endif
  #else
    uint64_t r_;

    if (b > 0) {
      uint64_t ub = HEDLEY_STATIC_CAST(uint64_t, b);
      r_ = ((UINT64_MAX - a) < ub) ? UINT64_MAX : a + ub;
    } else {
      uint64_t nb = HEDLEY_STATIC_CAST(uint64_t, -b);
      r_ = (nb > a) ? 0 : a - nb;
    }
    return r_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqaddd_u64
  #define vsqaddd_u64(a, b) simde_vsqaddd_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vsqadd_u8(simde_uint8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsqadd_u8(a, b);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a);
    simde_int8x8_private b_ = simde_int8x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vsqaddb_u8(a_.values[i], b_.values[i]);
    }

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqadd_u8
  #define vsqadd_u8(a, b) simde_vsqadd_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vsqadd_u16(simde_uint16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsqadd_u16(a, b);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a);
    simde_int16x4_private b_ = simde_int16x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vsqaddh_u16(a_.values[i], b_.values[i]);
    }

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqadd_u16
  #define vsqadd_u16(a, b) simde_vsqadd_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vsqadd_u32(simde_uint32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsqadd_u32(a, b);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a);
    simde_int32x2_private b_ = simde_int32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vsqadds_u32(a_.values[i], b_.values[i]);
    }

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqadd_u32
  #define vsqadd_u32(a, b) simde_vsqadd_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vsqadd_u64(simde_uint64x1_t a, simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsqadd_u64(a, b);
  #else
    simde_uint64x1_private
      r_,
      a_ = simde_uint64x1_to_private(a);
    simde_int64x1_private b_ = simde_int64x1_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vsqaddd_u64(a_.values[i], b_.values[i]);
    }

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqadd_u64
  #define vsqadd_u64(a, b) simde_vsqadd_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vsqaddq_u8(simde_uint8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsqaddq_u8(a, b);
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a);
    simde_int8x16_private b_ = simde_int8x16_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vsqaddb_u8(a_.values[i], b_.values[i]);
    }

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqaddq_u8
  #define vsqaddq_u8(a, b) simde_vsqaddq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vsqaddq_u16(simde_uint16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsqaddq_u16(a, b);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a);
    simde_int16x8_private  b_ = simde_int16x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vsqaddh_u16(a_.values[i], b_.values[i]);
    }

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqaddq_u16
  #define vsqaddq_u16(a, b) simde_vsqaddq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vsqaddq_u32(simde_uint32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsqaddq_u32(a, b);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a);
    simde_int32x4_private  b_ = simde_int32x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vsqadds_u32(a_.values[i], b_.values[i]);
    }

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqaddq_u32
  #define vsqaddq_u32(a, b) simde_vsqaddq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vsqaddq_u64(simde_uint64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsqaddq_u64(a, b);
  #else
    simde_uint64x2_private
      r_,
      a_ = simde_uint64x2_to_private(a);
    simde_int64x2_private  b_ = simde_int64x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vsqaddd_u64(a_.values[i], b_.values[i]);
    }

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsqaddq_u64
  #define vsqaddq_u64(a, b) simde_vsqaddq_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_SQADD_H) */
