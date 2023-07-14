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
 *   2020      Sean Maher <seanptmaher@gmail.com>
 */

#if !defined(SIMDE_ARM_NEON_LD3_H)
#define SIMDE_ARM_NEON_LD3_H

#include "types.h"
#include "ld1.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
#if HEDLEY_GCC_VERSION_CHECK(7,0,0)
  SIMDE_DIAGNOSTIC_DISABLE_MAYBE_UNINITIAZILED_
#endif
SIMDE_BEGIN_DECLS_

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2x3_t
simde_vld3_f32(simde_float32 const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_f32(ptr);
  #else
    simde_float32x2_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_float32x2x3_t r = { {
      simde_float32x2_from_private(r_[0]),
      simde_float32x2_from_private(r_[1]),
      simde_float32x2_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3_f32
  #define vld3_f32(a) simde_vld3_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1x3_t
simde_vld3_f64(simde_float64 const *ptr) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vld3_f64(ptr);
  #else
    simde_float64x1_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_float64x1x3_t r = { {
      simde_float64x1_from_private(r_[0]),
      simde_float64x1_from_private(r_[1]),
      simde_float64x1_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld3_f64
  #define vld3_f64(a) simde_vld3_f64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8x3_t
simde_vld3_s8(int8_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_s8(ptr);
  #else
    simde_int8x8_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int8x8x3_t r = { {
      simde_int8x8_from_private(r_[0]),
      simde_int8x8_from_private(r_[1]),
      simde_int8x8_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3_s8
  #define vld3_s8(a) simde_vld3_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4x3_t
simde_vld3_s16(int16_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_s16(ptr);
  #else
    simde_int16x4_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int16x4x3_t r = { {
      simde_int16x4_from_private(r_[0]),
      simde_int16x4_from_private(r_[1]),
      simde_int16x4_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3_s16
  #define vld3_s16(a) simde_vld3_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2x3_t
simde_vld3_s32(int32_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_s32(ptr);
  #else
    simde_int32x2_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int32x2x3_t r = { {
      simde_int32x2_from_private(r_[0]),
      simde_int32x2_from_private(r_[1]),
      simde_int32x2_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3_s32
  #define vld3_s32(a) simde_vld3_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1x3_t
simde_vld3_s64(int64_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_s64(ptr);
  #else
    simde_int64x1_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int64x1x3_t r = { {
      simde_int64x1_from_private(r_[0]),
      simde_int64x1_from_private(r_[1]),
      simde_int64x1_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld3_s64
  #define vld3_s64(a) simde_vld3_s64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8x3_t
simde_vld3_u8(uint8_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_u8(ptr);
  #else
    simde_uint8x8_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint8x8x3_t r = { {
      simde_uint8x8_from_private(r_[0]),
      simde_uint8x8_from_private(r_[1]),
      simde_uint8x8_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3_u8
  #define vld3_u8(a) simde_vld3_u8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4x3_t
simde_vld3_u16(uint16_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_u16(ptr);
  #else
    simde_uint16x4_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint16x4x3_t r = { {
      simde_uint16x4_from_private(r_[0]),
      simde_uint16x4_from_private(r_[1]),
      simde_uint16x4_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3_u16
  #define vld3_u16(a) simde_vld3_u16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2x3_t
simde_vld3_u32(uint32_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_u32(ptr);
  #else
    simde_uint32x2_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint32x2x3_t r = { {
      simde_uint32x2_from_private(r_[0]),
      simde_uint32x2_from_private(r_[1]),
      simde_uint32x2_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3_u32
  #define vld3_u32(a) simde_vld3_u32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1x3_t
simde_vld3_u64(uint64_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3_u64(ptr);
  #else
    simde_uint64x1_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint64x1x3_t r = { {
      simde_uint64x1_from_private(r_[0]),
      simde_uint64x1_from_private(r_[1]),
      simde_uint64x1_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld3_u64
  #define vld3_u64(a) simde_vld3_u64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4x3_t
simde_vld3q_f32(simde_float32 const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3q_f32(ptr);
  #else
    simde_float32x4_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_float32x4x3_t r = { {
      simde_float32x4_from_private(r_[0]),
      simde_float32x4_from_private(r_[1]),
      simde_float32x4_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3q_f32
  #define vld3q_f32(a) simde_vld3q_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2x3_t
simde_vld3q_f64(simde_float64 const *ptr) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vld3q_f64(ptr);
  #else
    simde_float64x2_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_float64x2x3_t r = { {
      simde_float64x2_from_private(r_[0]),
      simde_float64x2_from_private(r_[1]),
      simde_float64x2_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld3q_f64
  #define vld3q_f64(a) simde_vld3q_f64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16x3_t
simde_vld3q_s8(int8_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3q_s8(ptr);
  #else
    simde_int8x16_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int8x16x3_t r = { {
      simde_int8x16_from_private(r_[0]),
      simde_int8x16_from_private(r_[1]),
      simde_int8x16_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3q_s8
  #define vld3q_s8(a) simde_vld3q_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8x3_t
simde_vld3q_s16(int16_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3q_s16(ptr);
  #else
    simde_int16x8_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int16x8x3_t r = { {
      simde_int16x8_from_private(r_[0]),
      simde_int16x8_from_private(r_[1]),
      simde_int16x8_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3q_s16
  #define vld3q_s16(a) simde_vld3q_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4x3_t
simde_vld3q_s32(int32_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3q_s32(ptr);
  #else
    simde_int32x4_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int32x4x3_t r = { {
      simde_int32x4_from_private(r_[0]),
      simde_int32x4_from_private(r_[1]),
      simde_int32x4_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3q_s32
  #define vld3q_s32(a) simde_vld3q_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2x3_t
simde_vld3q_s64(int64_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vld3q_s64(ptr);
  #else
    simde_int64x2_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_int64x2x3_t r = { {
      simde_int64x2_from_private(r_[0]),
      simde_int64x2_from_private(r_[1]),
      simde_int64x2_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld3q_s64
  #define vld3q_s64(a) simde_vld3q_s64((a))
#endif


SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16x3_t
simde_vld3q_u8(uint8_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3q_u8(ptr);
  #else
    simde_uint8x16_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint8x16x3_t r = { {
      simde_uint8x16_from_private(r_[0]),
      simde_uint8x16_from_private(r_[1]),
      simde_uint8x16_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3q_u8
  #define vld3q_u8(a) simde_vld3q_u8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8x3_t
simde_vld3q_u16(uint16_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3q_u16(ptr);
  #else
    simde_uint16x8_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint16x8x3_t r = { {
      simde_uint16x8_from_private(r_[0]),
      simde_uint16x8_from_private(r_[1]),
      simde_uint16x8_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3q_u16
  #define vld3q_u16(a) simde_vld3q_u16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4x3_t
simde_vld3q_u32(uint32_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vld3q_u32(ptr);
  #else
    simde_uint32x4_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint32x4x3_t r = { {
      simde_uint32x4_from_private(r_[0]),
      simde_uint32x4_from_private(r_[1]),
      simde_uint32x4_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld3q_u32
  #define vld3q_u32(a) simde_vld3q_u32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2x3_t
simde_vld3q_u64(uint64_t const *ptr) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vld3q_u64(ptr);
  #else
    simde_uint64x2_private r_[3];

    for (size_t i = 0; i < (sizeof(r_) / sizeof(r_[0])); i++) {
      for (size_t j = 0 ; j < (sizeof(r_[0].values) / sizeof(r_[0].values[0])) ; j++) {
        r_[i].values[j] = ptr[i + (j * (sizeof(r_) / sizeof(r_[0])))];
      }
    }

    simde_uint64x2x3_t r = { {
      simde_uint64x2_from_private(r_[0]),
      simde_uint64x2_from_private(r_[1]),
      simde_uint64x2_from_private(r_[2])
    } };

    return r;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld3q_u64
  #define vld3q_u64(a) simde_vld3q_u64((a))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_LD3_H) */
