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
 *   2021      Zhi An Ng <zhin@google.com> (Copyright owned by Google, LLC)
 */

#if !defined(SIMDE_ARM_NEON_LD2_H)
#define SIMDE_ARM_NEON_LD2_H

#include "get_low.h"
#include "get_high.h"
#include "ld1.h"
#include "uzp.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
#if HEDLEY_GCC_VERSION_CHECK(7,0,0)
  SIMDE_DIAGNOSTIC_DISABLE_MAYBE_UNINITIAZILED_
#endif
SIMDE_BEGIN_DECLS_

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8x2_t
simde_vld2_s8(int8_t const ptr[HEDLEY_ARRAY_PARAM(16)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_s8(ptr);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t a = wasm_v128_load(ptr);
    simde_int8x16_private q_;
    q_.v128 = wasm_i8x16_shuffle(a, a, 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);
    simde_int8x16_t q = simde_int8x16_from_private(q_);

    simde_int8x8x2_t u = {
      simde_vget_low_s8(q),
      simde_vget_high_s8(q)
    };
    return u;
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_int8x16_private a_ = simde_int8x16_to_private(simde_vld1q_s8(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(8, 16, a_.values, a_.values, 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);
    simde_int8x8x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    simde_int8x8_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int8x8x2_t r = { {
      simde_int8x8_from_private(r_[0]),
      simde_int8x8_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_s8
  #define vld2_s8(a) simde_vld2_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4x2_t
simde_vld2_s16(int16_t const ptr[HEDLEY_ARRAY_PARAM(8)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_s16(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_int16x8_private a_ = simde_int16x8_to_private(simde_vld1q_s16(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(16, 16, a_.values, a_.values, 0, 2, 4, 6, 1, 3, 5, 7);
    simde_int16x4x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_PUSH
      SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_
    #endif
    simde_int16x4_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_POP
    #endif

    simde_int16x4x2_t r = { {
      simde_int16x4_from_private(r_[0]),
      simde_int16x4_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_s16
  #define vld2_s16(a) simde_vld2_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2x2_t
simde_vld2_s32(int32_t const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_s32(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_int32x4_private a_ = simde_int32x4_to_private(simde_vld1q_s32(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(32, 16, a_.values, a_.values, 0, 2, 1, 3);
    simde_int32x2x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    simde_int32x2_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int32x2x2_t r = { {
      simde_int32x2_from_private(r_[0]),
      simde_int32x2_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_s32
  #define vld2_s32(a) simde_vld2_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1x2_t
simde_vld2_s64(int64_t const ptr[HEDLEY_ARRAY_PARAM(2)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_s64(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_int64x2_private a_ = simde_int64x2_to_private(simde_vld1q_s64(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.values, a_.values, 0, 1);
    simde_int64x1x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    simde_int64x1_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int64x1x2_t r = { {
      simde_int64x1_from_private(r_[0]),
      simde_int64x1_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_s64
  #define vld2_s64(a) simde_vld2_s64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8x2_t
simde_vld2_u8(uint8_t const ptr[HEDLEY_ARRAY_PARAM(16)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_u8(ptr);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t a = wasm_v128_load(ptr);
    simde_uint8x16_private q_;
    q_.v128 = wasm_i8x16_shuffle(a, a, 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);
    simde_uint8x16_t q = simde_uint8x16_from_private(q_);

    simde_uint8x8x2_t u = {
      simde_vget_low_u8(q),
      simde_vget_high_u8(q)
    };
    return u;
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_uint8x16_private a_ = simde_uint8x16_to_private(simde_vld1q_u8(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(8, 16, a_.values, a_.values, 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);
    simde_uint8x8x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    simde_uint8x8_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint8x8x2_t r = { {
      simde_uint8x8_from_private(r_[0]),
      simde_uint8x8_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_u8
  #define vld2_u8(a) simde_vld2_u8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4x2_t
simde_vld2_u16(uint16_t const ptr[HEDLEY_ARRAY_PARAM(8)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_u16(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_uint16x8_private a_ = simde_uint16x8_to_private(simde_vld1q_u16(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(16, 16, a_.values, a_.values, 0, 2, 4, 6, 1, 3, 5, 7);
    simde_uint16x4x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_PUSH
      SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_
    #endif
    simde_uint16x4_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_POP
    #endif

    simde_uint16x4x2_t r = { {
      simde_uint16x4_from_private(r_[0]),
      simde_uint16x4_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_u16
  #define vld2_u16(a) simde_vld2_u16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2x2_t
simde_vld2_u32(uint32_t const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_u32(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_uint32x4_private a_ = simde_uint32x4_to_private(simde_vld1q_u32(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(32, 16, a_.values, a_.values, 0, 2, 1, 3);
    simde_uint32x2x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    simde_uint32x2_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint32x2x2_t r = { {
      simde_uint32x2_from_private(r_[0]),
      simde_uint32x2_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_u32
  #define vld2_u32(a) simde_vld2_u32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1x2_t
simde_vld2_u64(uint64_t const ptr[HEDLEY_ARRAY_PARAM(2)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_u64(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_uint64x2_private a_ = simde_uint64x2_to_private(simde_vld1q_u64(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.values, a_.values, 0, 1);
    simde_uint64x1x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    simde_uint64x1_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint64x1x2_t r = { {
      simde_uint64x1_from_private(r_[0]),
      simde_uint64x1_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_u64
  #define vld2_u64(a) simde_vld2_u64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2x2_t
simde_vld2_f32(simde_float32_t const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2_f32(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_float32x4_private a_ = simde_float32x4_to_private(simde_vld1q_f32(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(32, 16, a_.values, a_.values, 0, 2, 1, 3);
    simde_float32x2x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    simde_float32x2_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_float32x2x2_t r = { {
      simde_float32x2_from_private(r_[0]),
      simde_float32x2_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2_f32
  #define vld2_f32(a) simde_vld2_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1x2_t
simde_vld2_f64(simde_float64_t const ptr[HEDLEY_ARRAY_PARAM(2)]) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vld2_f64(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128) && defined(SIMDE_SHUFFLE_VECTOR_)
    simde_float64x2_private a_ = simde_float64x2_to_private(simde_vld1q_f64(ptr));
    a_.values = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.values, a_.values, 0, 1);
    simde_float64x1x2_t r;
    simde_memcpy(&r, &a_, sizeof(r));
    return r;
  #else
    simde_float64x1_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_float64x1x2_t r = { {
      simde_float64x1_from_private(r_[0]),
      simde_float64x1_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld2_f64
  #define vld2_f64(a) simde_vld2_f64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16x2_t
simde_vld2q_s8(int8_t const ptr[HEDLEY_ARRAY_PARAM(32)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2q_s8(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return
      simde_vuzpq_s8(
        simde_vld1q_s8(&(ptr[0])),
        simde_vld1q_s8(&(ptr[16]))
      );
  #else
    simde_int8x16_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int8x16x2_t r = { {
      simde_int8x16_from_private(r_[0]),
      simde_int8x16_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2q_s8
  #define vld2q_s8(a) simde_vld2q_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4x2_t
simde_vld2q_s32(int32_t const ptr[HEDLEY_ARRAY_PARAM(8)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2q_s32(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return
      simde_vuzpq_s32(
        simde_vld1q_s32(&(ptr[0])),
        simde_vld1q_s32(&(ptr[4]))
      );
  #else
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_PUSH
      SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_
    #endif
    simde_int32x4_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_POP
    #endif

    simde_int32x4x2_t r = { {
      simde_int32x4_from_private(r_[0]),
      simde_int32x4_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2q_s32
  #define vld2q_s32(a) simde_vld2q_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8x2_t
simde_vld2q_s16(int16_t const ptr[HEDLEY_ARRAY_PARAM(16)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2q_s16(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return
      simde_vuzpq_s16(
        simde_vld1q_s16(&(ptr[0])),
        simde_vld1q_s16(&(ptr[8]))
      );
  #else
    simde_int16x8_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int16x8x2_t r = { {
      simde_int16x8_from_private(r_[0]),
      simde_int16x8_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2q_s16
  #define vld2q_s16(a) simde_vld2q_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2x2_t
simde_vld2q_s64(int64_t const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vld2q_s64(ptr);
  #else
    simde_int64x2_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int64x2x2_t r = { {
      simde_int64x2_from_private(r_[0]),
      simde_int64x2_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld2q_s64
  #define vld2q_s64(a) simde_vld2q_s64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16x2_t
simde_vld2q_u8(uint8_t const ptr[HEDLEY_ARRAY_PARAM(32)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2q_u8(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return
      simde_vuzpq_u8(
        simde_vld1q_u8(&(ptr[ 0])),
        simde_vld1q_u8(&(ptr[16]))
      );
  #else
    simde_uint8x16_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint8x16x2_t r = { {
      simde_uint8x16_from_private(r_[0]),
      simde_uint8x16_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2q_u8
  #define vld2q_u8(a) simde_vld2q_u8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8x2_t
simde_vld2q_u16(uint16_t const ptr[HEDLEY_ARRAY_PARAM(16)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2q_u16(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return
      simde_vuzpq_u16(
        simde_vld1q_u16(&(ptr[0])),
        simde_vld1q_u16(&(ptr[8]))
      );
  #else
    simde_uint16x8_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint16x8x2_t r = { {
      simde_uint16x8_from_private(r_[0]),
      simde_uint16x8_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2q_u16
  #define vld2q_u16(a) simde_vld2q_u16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4x2_t
simde_vld2q_u32(uint32_t const ptr[HEDLEY_ARRAY_PARAM(8)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2q_u32(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return
      simde_vuzpq_u32(
        simde_vld1q_u32(&(ptr[0])),
        simde_vld1q_u32(&(ptr[4]))
      );
  #else
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_PUSH
      SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_
    #endif
    simde_uint32x4_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_POP
    #endif

    simde_uint32x4x2_t r = { {
      simde_uint32x4_from_private(r_[0]),
      simde_uint32x4_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2q_u32
  #define vld2q_u32(a) simde_vld2q_u32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2x2_t
simde_vld2q_u64(uint64_t const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vld2q_u64(ptr);
  #else
    simde_uint64x2_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint64x2x2_t r = { {
      simde_uint64x2_from_private(r_[0]),
      simde_uint64x2_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld2q_u64
  #define vld2q_u64(a) simde_vld2q_u64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4x2_t
simde_vld2q_f32(simde_float32_t const ptr[HEDLEY_ARRAY_PARAM(8)]) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld2q_f32(ptr);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return
      simde_vuzpq_f32(
        simde_vld1q_f32(&(ptr[0])),
        simde_vld1q_f32(&(ptr[4]))
      );
  #else
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_PUSH
      SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_
    #endif
    simde_float32x4_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }
    #if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_) && HEDLEY_GCC_VERSION_CHECK(12,0,0)
      HEDLEY_DIAGNOSTIC_POP
    #endif

    simde_float32x4x2_t r = { {
      simde_float32x4_from_private(r_[0]),
      simde_float32x4_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld2q_f32
  #define vld2q_f32(a) simde_vld2q_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2x2_t
simde_vld2q_f64(simde_float64_t const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vld2q_f64(ptr);
  #else
    simde_float64x2_private r_[2];

    for (size_t i = 0 ; i < (sizeof(r_) / sizeof(r_[0])) ; i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_float64x2x2_t r = { {
      simde_float64x2_from_private(r_[0]),
      simde_float64x2_from_private(r_[1]),
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld2q_f64
  #define vld2q_f64(a) simde_vld2q_f64((a))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_LD2_H) */
