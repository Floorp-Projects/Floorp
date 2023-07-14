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

#if !defined(SIMDE_ARM_NEON_QSHL_H)
#define SIMDE_ARM_NEON_QSHL_H

#include "types.h"
#include "cls.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int8_t
simde_vqshlb_s8(int8_t a, int8_t b) {
  int8_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vqshlb_s8(a, b);
  #else
    if (b < -7)
      b = -7;

    if (b <= 0) {
      r = a >> -b;
    } else if (b < 7) {
      r = HEDLEY_STATIC_CAST(int8_t, a << b);
      if ((r >> b) != a) {
        r = (a < 0) ? INT8_MIN : INT8_MAX;
      }
    } else if (a == 0) {
      r = 0;
    } else {
      r = (a < 0) ? INT8_MIN : INT8_MAX;
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshlb_s8
  #define vqshlb_s8(a, b) simde_vqshlb_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int16_t
simde_vqshlh_s16(int16_t a, int16_t b) {
  int16_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vqshlh_s16(a, b);
  #else
    int8_t b8 = HEDLEY_STATIC_CAST(int8_t, b);

    if (b8 < -15)
      b8 = -15;

    if (b8 <= 0) {
      r = a >> -b8;
    } else if (b8 < 15) {
      r = HEDLEY_STATIC_CAST(int16_t, a << b8);
      if ((r >> b8) != a) {
        r = (a < 0) ? INT16_MIN : INT16_MAX;
      }
    } else if (a == 0) {
      r = 0;
    } else {
      r = (a < 0) ? INT16_MIN : INT16_MAX;
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshlh_s16
  #define vqshlh_s16(a, b) simde_vqshlh_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vqshls_s32(int32_t a, int32_t b) {
  int32_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vqshls_s32(a, b);
  #else
    int8_t b8 = HEDLEY_STATIC_CAST(int8_t, b);

    if (b8 < -31)
      b8 = -31;

    if (b8 <= 0) {
      r = a >> -b8;
    } else if (b8 < 31) {
      r = HEDLEY_STATIC_CAST(int32_t, a << b8);
      if ((r >> b8) != a) {
        r = (a < 0) ? INT32_MIN : INT32_MAX;
      }
    } else if (a == 0) {
      r = 0;
    } else {
      r = (a < 0) ? INT32_MIN : INT32_MAX;
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshls_s32
  #define vqshls_s32(a, b) simde_vqshls_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vqshld_s64(int64_t a, int64_t b) {
  int64_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vqshld_s64(a, b);
  #else
    int8_t b8 = HEDLEY_STATIC_CAST(int8_t, b);

    if (b8 < -63)
      b8 = -63;

    if (b8 <= 0) {
      r = a >> -b8;
    } else if (b8 < 63) {
      r = HEDLEY_STATIC_CAST(int64_t, a << b8);
      if ((r >> b8) != a) {
        r = (a < 0) ? INT64_MIN : INT64_MAX;
      }
    } else if (a == 0) {
      r = 0;
    } else {
      r = (a < 0) ? INT64_MIN : INT64_MAX;
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshld_s64
  #define vqshld_s64(a, b) simde_vqshld_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint8_t
simde_vqshlb_u8(uint8_t a, int8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(HEDLEY_GCC_VERSION) && !HEDLEY_GCC_VERSION_CHECK(11,0,0)
      return vqshlb_u8(a, HEDLEY_STATIC_CAST(uint8_t, b));
    #elif HEDLEY_HAS_WARNING("-Wsign-conversion")
      /* https://github.com/llvm/llvm-project/commit/f0a78bdfdc6d56b25e0081884580b3960a3c2429 */
      HEDLEY_DIAGNOSTIC_PUSH
      #pragma clang diagnostic ignored "-Wsign-conversion"
      return vqshlb_u8(a, b);
      HEDLEY_DIAGNOSTIC_POP
    #else
      return vqshlb_u8(a, b);
    #endif
  #else
    uint8_t r;

    if (b < -7)
      b = -7;

    if (b <= 0) {
      r = a >> -b;
    } else if (b < 7) {
      r = HEDLEY_STATIC_CAST(uint8_t, a << b);
      if ((r >> b) != a) {
        r = UINT8_MAX;
      }
    } else if (a == 0) {
      r = 0;
    } else {
      r = UINT8_MAX;
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshlb_u8
  #define vqshlb_u8(a, b) simde_vqshlb_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vqshlh_u16(uint16_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(HEDLEY_GCC_VERSION) && !HEDLEY_GCC_VERSION_CHECK(11,0,0)
      return vqshlh_u16(a, HEDLEY_STATIC_CAST(uint16_t, b));
    #elif HEDLEY_HAS_WARNING("-Wsign-conversion")
      HEDLEY_DIAGNOSTIC_PUSH
      #pragma clang diagnostic ignored "-Wsign-conversion"
      return vqshlh_u16(a, b);
      HEDLEY_DIAGNOSTIC_POP
    #else
      return vqshlh_u16(a, b);
    #endif
  #else
    uint16_t r;

    if (b < -15)
      b = -15;

    if (b <= 0) {
      r = a >> -b;
    } else if (b < 15) {
      r = HEDLEY_STATIC_CAST(uint16_t, a << b);
      if ((r >> b) != a) {
        r = UINT16_MAX;
      }
    } else if (a == 0) {
      r = 0;
    } else {
      r = UINT16_MAX;
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshlh_u16
  #define vqshlh_u16(a, b) simde_vqshlh_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vqshls_u32(uint32_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(HEDLEY_GCC_VERSION) && !HEDLEY_GCC_VERSION_CHECK(11,0,0)
      return vqshls_u32(a, HEDLEY_STATIC_CAST(uint16_t, b));
    #elif HEDLEY_HAS_WARNING("-Wsign-conversion")
      HEDLEY_DIAGNOSTIC_PUSH
      #pragma clang diagnostic ignored "-Wsign-conversion"
      return vqshls_u32(a, b);
      HEDLEY_DIAGNOSTIC_POP
    #else
      return vqshls_u32(a, b);
    #endif
  #else
    uint32_t r;

    if (b < -31)
      b = -31;

    if (b <= 0) {
      r = HEDLEY_STATIC_CAST(uint32_t, a >> -b);
    } else if (b < 31) {
      r = a << b;
      if ((r >> b) != a) {
        r = UINT32_MAX;
      }
    } else if (a == 0) {
      r = 0;
    } else {
      r = UINT32_MAX;
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshls_u32
  #define vqshls_u32(a, b) simde_vqshls_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vqshld_u64(uint64_t a, int64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #if defined(HEDLEY_GCC_VERSION) && !HEDLEY_GCC_VERSION_CHECK(11,0,0)
      return vqshld_u64(a, HEDLEY_STATIC_CAST(uint16_t, b));
    #elif HEDLEY_HAS_WARNING("-Wsign-conversion")
      HEDLEY_DIAGNOSTIC_PUSH
      #pragma clang diagnostic ignored "-Wsign-conversion"
      return vqshld_u64(a, b);
      HEDLEY_DIAGNOSTIC_POP
    #else
      return vqshld_u64(a, b);
    #endif
  #else
    uint64_t r;

    if (b < -63)
      b = -63;

    if (b <= 0) {
      r = a >> -b;
    } else if (b < 63) {
      r = HEDLEY_STATIC_CAST(uint64_t, a << b);
      if ((r >> b) != a) {
        r = UINT64_MAX;
      }
    } else if (a == 0) {
      r = 0;
    } else {
      r = UINT64_MAX;
    }

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshldb_u64
  #define vqshld_u64(a, b) simde_vqshld_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vqshl_s8 (const simde_int8x8_t a, const simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshl_s8(a, b);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshlb_s8(a_.values[i], b_.values[i]);
    }

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshl_s8
  #define vqshl_s8(a, b) simde_vqshl_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vqshl_s16 (const simde_int16x4_t a, const simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshl_s16(a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshlh_s16(a_.values[i], b_.values[i]);
    }

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshl_s16
  #define vqshl_s16(a, b) simde_vqshl_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vqshl_s32 (const simde_int32x2_t a, const simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshl_s32(a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshls_s32(a_.values[i], b_.values[i]);
    }

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshl_s32
  #define vqshl_s32(a, b) simde_vqshl_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vqshl_s64 (const simde_int64x1_t a, const simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshl_s64(a, b);
  #else
    simde_int64x1_private
      r_,
      a_ = simde_int64x1_to_private(a),
      b_ = simde_int64x1_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshld_s64(a_.values[i], b_.values[i]);
    }

    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshl_s64
  #define vqshl_s64(a, b) simde_vqshl_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vqshl_u8 (const simde_uint8x8_t a, const simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshl_u8(a, b);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a);
    simde_int8x8_private
      b_ = simde_int8x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshlb_u8(a_.values[i], b_.values[i]);
    }

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshl_u8
  #define vqshl_u8(a, b) simde_vqshl_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vqshl_u16 (const simde_uint16x4_t a, const simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshl_u16(a, b);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a);
    simde_int16x4_private
      b_ = simde_int16x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshlh_u16(a_.values[i], b_.values[i]);
    }

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshl_u16
  #define vqshl_u16(a, b) simde_vqshl_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vqshl_u32 (const simde_uint32x2_t a, const simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshl_u32(a, b);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a);
    simde_int32x2_private
      b_ = simde_int32x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshls_u32(a_.values[i], b_.values[i]);
    }

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshl_u32
  #define vqshl_u32(a, b) simde_vqshl_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vqshl_u64 (const simde_uint64x1_t a, const simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshl_u64(a, b);
  #else
    simde_uint64x1_private
      r_,
      a_ = simde_uint64x1_to_private(a);
    simde_int64x1_private
      b_ = simde_int64x1_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshld_u64(a_.values[i], b_.values[i]);
    }

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshl_u64
  #define vqshl_u64(a, b) simde_vqshl_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vqshlq_s8 (const simde_int8x16_t a, const simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshlq_s8(a, b);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshlb_s8(a_.values[i], b_.values[i]);
    }

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlq_s8
  #define vqshlq_s8(a, b) simde_vqshlq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vqshlq_s16 (const simde_int16x8_t a, const simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshlq_s16(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshlh_s16(a_.values[i], b_.values[i]);
    }

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlq_s16
  #define vqshlq_s16(a, b) simde_vqshlq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vqshlq_s32 (const simde_int32x4_t a, const simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshlq_s32(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshls_s32(a_.values[i], b_.values[i]);
    }

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlq_s32
  #define vqshlq_s32(a, b) simde_vqshlq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vqshlq_s64 (const simde_int64x2_t a, const simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshlq_s64(a, b);
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshld_s64(a_.values[i], b_.values[i]);
    }

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlq_s64
  #define vqshlq_s64(a, b) simde_vqshlq_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vqshlq_u8 (const simde_uint8x16_t a, const simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshlq_u8(a, b);
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a);
    simde_int8x16_private
      b_ = simde_int8x16_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshlb_u8(a_.values[i], b_.values[i]);
    }

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlq_u8
  #define vqshlq_u8(a, b) simde_vqshlq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vqshlq_u16 (const simde_uint16x8_t a, const simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshlq_u16(a, b);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a);
    simde_int16x8_private
      b_ = simde_int16x8_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshlh_u16(a_.values[i], b_.values[i]);
    }

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlq_u16
  #define vqshlq_u16(a, b) simde_vqshlq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vqshlq_u32 (const simde_uint32x4_t a, const simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshlq_u32(a, b);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a);
    simde_int32x4_private
      b_ = simde_int32x4_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshls_u32(a_.values[i], b_.values[i]);
    }

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlq_u32
  #define vqshlq_u32(a, b) simde_vqshlq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vqshlq_u64 (const simde_uint64x2_t a, const simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqshlq_u64(a, b);
  #else
    simde_uint64x2_private
      r_,
      a_ = simde_uint64x2_to_private(a);
    simde_int64x2_private
      b_ = simde_int64x2_to_private(b);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vqshld_u64(a_.values[i], b_.values[i]);
    }

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshlq_u64
  #define vqshlq_u64(a, b) simde_vqshlq_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QSHL_H) */
