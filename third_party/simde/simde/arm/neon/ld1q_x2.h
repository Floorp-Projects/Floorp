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
 *   2021      Zhi An Ng <zhin@google.com> (Copyright owned by Google, LLC)
 *   2021      Décio Luiz Gazzoni Filho <decio@decpp.net>
 */

#if !defined(SIMDE_ARM_NEON_LD1Q_X2_H)
#define SIMDE_ARM_NEON_LD1Q_X2_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
#if HEDLEY_GCC_VERSION_CHECK(7,0,0)
  SIMDE_DIAGNOSTIC_DISABLE_MAYBE_UNINITIAZILED_
#endif
SIMDE_BEGIN_DECLS_

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4x2_t
simde_vld1q_f32_x2(simde_float32 const ptr[HEDLEY_ARRAY_PARAM(8)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_f32_x2(ptr);
  #else
    simde_float32x4_private a_[2];
    for (size_t i = 0; i < 8; i++) {
      a_[i / 4].values[i % 4] = ptr[i];
    }
    simde_float32x4x2_t s_ = { { simde_float32x4_from_private(a_[0]),
                                 simde_float32x4_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_f32_x2
  #define vld1q_f32_x2(a) simde_vld1q_f32_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2x2_t
simde_vld1q_f64_x2(simde_float64 const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_f64_x2(ptr);
  #else
    simde_float64x2_private a_[2];
    for (size_t i = 0; i < 4; i++) {
      a_[i / 2].values[i % 2] = ptr[i];
    }
    simde_float64x2x2_t s_ = { { simde_float64x2_from_private(a_[0]),
                                 simde_float64x2_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_f64_x2
  #define vld1q_f64_x2(a) simde_vld1q_f64_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16x2_t
simde_vld1q_s8_x2(int8_t const ptr[HEDLEY_ARRAY_PARAM(32)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_s8_x2(ptr);
  #else
    simde_int8x16_private a_[2];
    for (size_t i = 0; i < 32; i++) {
      a_[i / 16].values[i % 16] = ptr[i];
    }
    simde_int8x16x2_t s_ = { { simde_int8x16_from_private(a_[0]),
                               simde_int8x16_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_s8_x2
  #define vld1q_s8_x2(a) simde_vld1q_s8_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8x2_t
simde_vld1q_s16_x2(int16_t const ptr[HEDLEY_ARRAY_PARAM(16)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_s16_x2(ptr);
  #else
    simde_int16x8_private a_[2];
    for (size_t i = 0; i < 16; i++) {
      a_[i / 8].values[i % 8] = ptr[i];
    }
    simde_int16x8x2_t s_ = { { simde_int16x8_from_private(a_[0]),
                               simde_int16x8_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_s16_x2
  #define vld1q_s16_x2(a) simde_vld1q_s16_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4x2_t
simde_vld1q_s32_x2(int32_t const ptr[HEDLEY_ARRAY_PARAM(8)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_s32_x2(ptr);
  #else
    simde_int32x4_private a_[2];
    for (size_t i = 0; i < 8; i++) {
      a_[i / 4].values[i % 4] = ptr[i];
    }
    simde_int32x4x2_t s_ = { { simde_int32x4_from_private(a_[0]),
                               simde_int32x4_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_s32_x2
  #define vld1q_s32_x2(a) simde_vld1q_s32_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2x2_t
simde_vld1q_s64_x2(int64_t const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_s64_x2(ptr);
  #else
    simde_int64x2_private a_[2];
    for (size_t i = 0; i < 4; i++) {
      a_[i / 2].values[i % 2] = ptr[i];
    }
    simde_int64x2x2_t s_ = { { simde_int64x2_from_private(a_[0]),
                               simde_int64x2_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_s64_x2
  #define vld1q_s64_x2(a) simde_vld1q_s64_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16x2_t
simde_vld1q_u8_x2(uint8_t const ptr[HEDLEY_ARRAY_PARAM(32)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_u8_x2(ptr);
  #else
    simde_uint8x16_private a_[2];
    for (size_t i = 0; i < 32; i++) {
      a_[i / 16].values[i % 16] = ptr[i];
    }
    simde_uint8x16x2_t s_ = { { simde_uint8x16_from_private(a_[0]),
                                simde_uint8x16_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_u8_x2
  #define vld1q_u8_x2(a) simde_vld1q_u8_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8x2_t
simde_vld1q_u16_x2(uint16_t const ptr[HEDLEY_ARRAY_PARAM(16)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_u16_x2(ptr);
  #else
    simde_uint16x8_private a_[2];
    for (size_t i = 0; i < 16; i++) {
      a_[i / 8].values[i % 8] = ptr[i];
    }
    simde_uint16x8x2_t s_ = { { simde_uint16x8_from_private(a_[0]),
                                simde_uint16x8_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_u16_x2
  #define vld1q_u16_x2(a) simde_vld1q_u16_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4x2_t
simde_vld1q_u32_x2(uint32_t const ptr[HEDLEY_ARRAY_PARAM(8)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_u32_x2(ptr);
  #else
    simde_uint32x4_private a_[2];
    for (size_t i = 0; i < 8; i++) {
      a_[i / 4].values[i % 4] = ptr[i];
    }
    simde_uint32x4x2_t s_ = { { simde_uint32x4_from_private(a_[0]),
                                simde_uint32x4_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_u32_x2
  #define vld1q_u32_x2(a) simde_vld1q_u32_x2((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2x2_t
simde_vld1q_u64_x2(uint64_t const ptr[HEDLEY_ARRAY_PARAM(4)]) {
  #if \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) && \
      (!defined(HEDLEY_GCC_VERSION) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && defined(SIMDE_ARM_NEON_A64V8_NATIVE))) && \
      (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
    return vld1q_u64_x2(ptr);
  #else
    simde_uint64x2_private a_[2];
    for (size_t i = 0; i < 4; i++) {
      a_[i / 2].values[i % 2] = ptr[i];
    }
    simde_uint64x2x2_t s_ = { { simde_uint64x2_from_private(a_[0]),
                                simde_uint64x2_from_private(a_[1]) } };
    return s_;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld1q_u64_x2
  #define vld1q_u64_x2(a) simde_vld1q_u64_x2((a))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_LD1Q_X2_H) */
