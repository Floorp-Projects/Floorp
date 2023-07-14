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

#if !defined(SIMDE_ARM_NEON_UQADD_H)
#define SIMDE_ARM_NEON_UQADD_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

// Workaround on ARM64 windows due to windows SDK bug
// https://developercommunity.visualstudio.com/t/In-arm64_neonh-vsqaddb_u8-vsqaddh_u16/10271747?sort=newest
#if (defined _MSC_VER) && (defined SIMDE_ARM_NEON_A64V8_NATIVE)
#undef vuqaddh_s16
#define vuqaddh_s16(src1, src2) neon_suqadds16(__int16ToN16_v(src1), __uint16ToN16_v(src2)).n16_i16[0]
#undef vuqadds_s32
#define vuqadds_s32(src1, src2) _CopyInt32FromFloat(neon_suqadds32(_CopyFloatFromInt32(src1), _CopyFloatFromUInt32(src2)))
#undef vuqaddd_s64
#define vuqaddd_s64(src1, src2) neon_suqadds64(__int64ToN64_v(src1), __uint64ToN64_v(src2)).n64_i64[0]
#endif

SIMDE_FUNCTION_ATTRIBUTES
int8_t
simde_vuqaddb_s8(int8_t a, uint8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(SIMDE_BUG_CLANG_GIT_4EC445B8)
      return vuqaddb_s8(a, HEDLEY_STATIC_CAST(int8_t, b));
    #else
      return vuqaddb_s8(a, b);
    #endif
  #else
    int16_t r_ = HEDLEY_STATIC_CAST(int16_t, a) + HEDLEY_STATIC_CAST(int16_t, b);
    return (r_ < INT8_MIN) ? INT8_MIN : ((r_ > INT8_MAX) ? INT8_MAX : HEDLEY_STATIC_CAST(int8_t, r_));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqaddb_s8
  #define vuqaddb_s8(a, b) simde_vuqaddb_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int16_t
simde_vuqaddh_s16(int16_t a, uint16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(SIMDE_BUG_CLANG_GIT_4EC445B8)
      return vuqaddh_s16(a, HEDLEY_STATIC_CAST(int16_t, b));
    #else
      return vuqaddh_s16(a, b);
    #endif
  #else
    int32_t r_ = HEDLEY_STATIC_CAST(int32_t, a) + HEDLEY_STATIC_CAST(int32_t, b);
    return (r_ < INT16_MIN) ? INT16_MIN : ((r_ > INT16_MAX) ? INT16_MAX : HEDLEY_STATIC_CAST(int16_t, r_));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqaddh_s16
  #define vuqaddh_s16(a, b) simde_vuqaddh_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vuqadds_s32(int32_t a, uint32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(SIMDE_BUG_CLANG_GIT_4EC445B8)
      return vuqadds_s32(a, HEDLEY_STATIC_CAST(int32_t, b));
    #else
      return vuqadds_s32(a, b);
    #endif
  #else
    int64_t r_ = HEDLEY_STATIC_CAST(int64_t, a) + HEDLEY_STATIC_CAST(int64_t, b);
    return (r_ < INT32_MIN) ? INT32_MIN : ((r_ > INT32_MAX) ? INT32_MAX : HEDLEY_STATIC_CAST(int32_t, r_));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqadds_s32
  #define vuqadds_s32(a, b) simde_vuqadds_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vuqaddd_s64(int64_t a, uint64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(SIMDE_BUG_CLANG_GIT_4EC445B8)
      return vuqaddd_s64(a, HEDLEY_STATIC_CAST(int64_t, b));
    #else
      return vuqaddd_s64(a, b);
    #endif
  #else
    /* TODO: I suspect there is room for improvement here.  This is
     * just the first thing that worked, and I don't feel like messing
     * with it now. */
    int64_t r;

    if (a < 0) {
      uint64_t na = HEDLEY_STATIC_CAST(uint64_t, -a);
      if (na > b) {
        uint64_t t = na - b;
        r = (t > (HEDLEY_STATIC_CAST(uint64_t, INT64_MAX) + 1)) ? INT64_MIN : -HEDLEY_STATIC_CAST(int64_t, t);
      } else {
        uint64_t t = b - na;
        r = (t > (HEDLEY_STATIC_CAST(uint64_t, INT64_MAX)    )) ? INT64_MAX :  HEDLEY_STATIC_CAST(int64_t, t);
      }
    } else {
      uint64_t ua = HEDLEY_STATIC_CAST(uint64_t, a);
      r = ((INT64_MAX - ua) < b) ? INT64_MAX : HEDLEY_STATIC_CAST(int64_t, ua + b);
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqaddd_s64
  #define vuqaddd_s64(a, b) simde_vuqaddd_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vuqadd_s8(simde_int8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vuqadd_s8(a, b);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a);
    simde_uint8x8_private b_ = simde_uint8x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vuqaddb_s8(a_.values[i], b_.values[i]);
    }

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqadd_s8
  #define vuqadd_s8(a, b) simde_vuqadd_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vuqadd_s16(simde_int16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vuqadd_s16(a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a);
    simde_uint16x4_private b_ = simde_uint16x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vuqaddh_s16(a_.values[i], b_.values[i]);
    }

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqadd_s16
  #define vuqadd_s16(a, b) simde_vuqadd_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vuqadd_s32(simde_int32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vuqadd_s32(a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a);
    simde_uint32x2_private b_ = simde_uint32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vuqadds_s32(a_.values[i], b_.values[i]);
    }

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqadd_s32
  #define vuqadd_s32(a, b) simde_vuqadd_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vuqadd_s64(simde_int64x1_t a, simde_uint64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vuqadd_s64(a, b);
  #else
    simde_int64x1_private
      r_,
      a_ = simde_int64x1_to_private(a);
    simde_uint64x1_private b_ = simde_uint64x1_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vuqaddd_s64(a_.values[i], b_.values[i]);
    }

    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqadd_s64
  #define vuqadd_s64(a, b) simde_vuqadd_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vuqaddq_s8(simde_int8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vuqaddq_s8(a, b);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a);
    simde_uint8x16_private b_ = simde_uint8x16_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vuqaddb_s8(a_.values[i], b_.values[i]);
    }

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqaddq_s8
  #define vuqaddq_s8(a, b) simde_vuqaddq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vuqaddq_s16(simde_int16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vuqaddq_s16(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a);
    simde_uint16x8_private b_ = simde_uint16x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vuqaddh_s16(a_.values[i], b_.values[i]);
    }

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqaddq_s16
  #define vuqaddq_s16(a, b) simde_vuqaddq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vuqaddq_s32(simde_int32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vuqaddq_s32(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a);
    simde_uint32x4_private b_ = simde_uint32x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vuqadds_s32(a_.values[i], b_.values[i]);
    }

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqaddq_s32
  #define vuqaddq_s32(a, b) simde_vuqaddq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vuqaddq_s64(simde_int64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vuqaddq_s64(a, b);
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a);
    simde_uint64x2_private b_ = simde_uint64x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vuqaddd_s64(a_.values[i], b_.values[i]);
    }

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vuqaddq_s64
  #define vuqaddq_s64(a, b) simde_vuqaddq_s64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_UQADD_H) */
