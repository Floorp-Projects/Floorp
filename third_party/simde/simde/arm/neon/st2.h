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
 */

#if !defined(SIMDE_ARM_NEON_ST2_H)
#define SIMDE_ARM_NEON_ST2_H

#include "types.h"
#include "zip.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_f32(simde_float32_t *ptr, simde_float32x2x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_f32(ptr, val);
  #else
    simde_float32_t buf[4];
    simde_float32x2_private a_[2] = {simde_float32x2_to_private(val.val[0]),
                                     simde_float32x2_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_f32
  #define vst2_f32(a, b) simde_vst2_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_f64(simde_float64_t *ptr, simde_float64x1x2_t val) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    vst2_f64(ptr, val);
  #else
    simde_float64_t buf[2];
    simde_float64x1_private a_[2] = {simde_float64x1_to_private(val.val[0]),
                                     simde_float64x1_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst2_f64
  #define vst2_f64(a, b) simde_vst2_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_s8(int8_t *ptr, simde_int8x8x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_s8(ptr, val);
  #else
    int8_t buf[16];
    simde_int8x8_private a_[2] = {simde_int8x8_to_private(val.val[0]),
                                  simde_int8x8_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_s8
  #define vst2_s8(a, b) simde_vst2_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_s16(int16_t *ptr, simde_int16x4x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_s16(ptr, val);
  #else
    int16_t buf[8];
    simde_int16x4_private a_[2] = {simde_int16x4_to_private(val.val[0]),
                                   simde_int16x4_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_s16
  #define vst2_s16(a, b) simde_vst2_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_s32(int32_t *ptr, simde_int32x2x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_s32(ptr, val);
  #else
    int32_t buf[4];
    simde_int32x2_private a_[2] = {simde_int32x2_to_private(val.val[0]),
                                   simde_int32x2_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_s32
  #define vst2_s32(a, b) simde_vst2_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_s64(int64_t *ptr, simde_int64x1x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_s64(ptr, val);
  #else
    int64_t buf[2];
    simde_int64x1_private a_[2] = {simde_int64x1_to_private(val.val[0]),
                                   simde_int64x1_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_s64
  #define vst2_s64(a, b) simde_vst2_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_u8(uint8_t *ptr, simde_uint8x8x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_u8(ptr, val);
  #else
    uint8_t buf[16];
    simde_uint8x8_private a_[2] = {simde_uint8x8_to_private(val.val[0]),
                                   simde_uint8x8_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_u8
  #define vst2_u8(a, b) simde_vst2_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_u16(uint16_t *ptr, simde_uint16x4x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_u16(ptr, val);
  #else
    uint16_t buf[8];
    simde_uint16x4_private a_[2] = {simde_uint16x4_to_private(val.val[0]),
                                    simde_uint16x4_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_u16
  #define vst2_u16(a, b) simde_vst2_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_u32(uint32_t *ptr, simde_uint32x2x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_u32(ptr, val);
  #else
    uint32_t buf[4];
    simde_uint32x2_private a_[2] = {simde_uint32x2_to_private(val.val[0]),
                                    simde_uint32x2_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_u32
  #define vst2_u32(a, b) simde_vst2_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2_u64(uint64_t *ptr, simde_uint64x1x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2_u64(ptr, val);
  #else
    uint64_t buf[2];
    simde_uint64x1_private a_[2] = {simde_uint64x1_to_private(val.val[0]),
                                   simde_uint64x1_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2_u64
  #define vst2_u64(a, b) simde_vst2_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_f32(simde_float32_t *ptr, simde_float32x4x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2q_f32(ptr, val);
  #else
    simde_float32x4x2_t r = simde_vzipq_f32(val.val[0], val.val[1]);
    simde_vst1q_f32(ptr, r.val[0]);
    simde_vst1q_f32(ptr+4, r.val[1]);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2q_f32
  #define vst2q_f32(a, b) simde_vst2q_f32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_f64(simde_float64_t *ptr, simde_float64x2x2_t val) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    vst2q_f64(ptr, val);
  #else
    simde_float64_t buf[4];
    simde_float64x2_private a_[2] = {simde_float64x2_to_private(val.val[0]),
                                   simde_float64x2_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst2q_f64
  #define vst2q_f64(a, b) simde_vst2q_f64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_s8(int8_t *ptr, simde_int8x16x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2q_s8(ptr, val);
  #else
    simde_int8x16x2_t r = simde_vzipq_s8(val.val[0], val.val[1]);
    simde_vst1q_s8(ptr, r.val[0]);
    simde_vst1q_s8(ptr+16, r.val[1]);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2q_s8
  #define vst2q_s8(a, b) simde_vst2q_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_s16(int16_t *ptr, simde_int16x8x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2q_s16(ptr, val);
  #else
    simde_int16x8x2_t r = simde_vzipq_s16(val.val[0], val.val[1]);
    simde_vst1q_s16(ptr, r.val[0]);
    simde_vst1q_s16(ptr+8, r.val[1]);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2q_s16
  #define vst2q_s16(a, b) simde_vst2q_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_s32(int32_t *ptr, simde_int32x4x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2q_s32(ptr, val);
  #else
    simde_int32x4x2_t r = simde_vzipq_s32(val.val[0], val.val[1]);
    simde_vst1q_s32(ptr, r.val[0]);
    simde_vst1q_s32(ptr+4, r.val[1]);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2q_s32
  #define vst2q_s32(a, b) simde_vst2q_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_s64(int64_t *ptr, simde_int64x2x2_t val) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    vst2q_s64(ptr, val);
  #else
    int64_t buf[4];
    simde_int64x2_private a_[2] = {simde_int64x2_to_private(val.val[0]),
                                   simde_int64x2_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst2q_s64
  #define vst2q_s64(a, b) simde_vst2q_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_u8(uint8_t *ptr, simde_uint8x16x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2q_u8(ptr, val);
  #else
    simde_uint8x16x2_t r = simde_vzipq_u8(val.val[0], val.val[1]);
    simde_vst1q_u8(ptr, r.val[0]);
    simde_vst1q_u8(ptr+16, r.val[1]);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2q_u8
  #define vst2q_u8(a, b) simde_vst2q_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_u16(uint16_t *ptr, simde_uint16x8x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2q_u16(ptr, val);
  #else
    simde_uint16x8x2_t r = simde_vzipq_u16(val.val[0], val.val[1]);
    simde_vst1q_u16(ptr, r.val[0]);
    simde_vst1q_u16(ptr+8, r.val[1]);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2q_u16
  #define vst2q_u16(a, b) simde_vst2q_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_u32(uint32_t *ptr, simde_uint32x4x2_t val) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    vst2q_u32(ptr, val);
  #else
    simde_uint32x4x2_t r = simde_vzipq_u32(val.val[0], val.val[1]);
    simde_vst1q_u32(ptr, r.val[0]);
    simde_vst1q_u32(ptr+4, r.val[1]);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst2q_u32
  #define vst2q_u32(a, b) simde_vst2q_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst2q_u64(uint64_t *ptr, simde_uint64x2x2_t val) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    vst2q_u64(ptr, val);
  #else
    uint64_t buf[4];
    simde_uint64x2_private a_[2] = {simde_uint64x2_to_private(val.val[0]),
                                   simde_uint64x2_to_private(val.val[1])};
    for (size_t i = 0; i < (sizeof(val.val[0]) / sizeof(*ptr)) * 2 ; i++) {
      buf[i] = a_[i % 2].values[i / 2];
    }
    simde_memcpy(ptr, buf, sizeof(buf));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst2q_u64
  #define vst2q_u64(a, b) simde_vst2q_u64((a), (b))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_ST2_H) */
