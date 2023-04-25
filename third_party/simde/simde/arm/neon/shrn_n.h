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

#if !defined(SIMDE_ARM_NEON_SHRN_N_H)
#define SIMDE_ARM_NEON_SHRN_N_H

#include "types.h"
#include "reinterpret.h"
#include "movn.h"
#include "shr_n.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vshrn_n_s16 (const simde_int16x8_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 8) {
  simde_int8x8_private r_;
  simde_int16x8_private a_ = simde_int16x8_to_private(a);
  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(int8_t, (a_.values[i] >> n) & UINT8_MAX);
  }
  return simde_int8x8_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vshrn_n_s16(a, n) vshrn_n_s16((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vshrn_n_s16(a, n) simde_vmovn_s16(simde_vshrq_n_s16((a), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vshrn_n_s16
  #define vshrn_n_s16(a, n) simde_vshrn_n_s16((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vshrn_n_s32 (const simde_int32x4_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 16) {
  simde_int16x4_private r_;
  simde_int32x4_private a_ = simde_int32x4_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(int16_t, (a_.values[i] >> n) & UINT16_MAX);
  }

  return simde_int16x4_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vshrn_n_s32(a, n) vshrn_n_s32((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vshrn_n_s32(a, n) simde_vmovn_s32(simde_vshrq_n_s32((a), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vshrn_n_s32
  #define vshrn_n_s32(a, n) simde_vshrn_n_s32((a), (n))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vshrn_n_s64 (const simde_int64x2_t a, const int n)
    SIMDE_REQUIRE_CONSTANT_RANGE(n, 1, 32) {
  simde_int32x2_private r_;
  simde_int64x2_private a_ = simde_int64x2_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
    r_.values[i] = HEDLEY_STATIC_CAST(int32_t, (a_.values[i] >> n) & UINT32_MAX);
  }

  return simde_int32x2_from_private(r_);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vshrn_n_s64(a, n) vshrn_n_s64((a), (n))
#elif SIMDE_NATURAL_VECTOR_SIZE > 0
  #define simde_vshrn_n_s64(a, n) simde_vmovn_s64(simde_vshrq_n_s64((a), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vshrn_n_s64
  #define vshrn_n_s64(a, n) simde_vshrn_n_s64((a), (n))
#endif

#define simde_vshrn_n_u16(a, n) \
  simde_vreinterpret_u8_s8(     \
      simde_vshrn_n_s16(simde_vreinterpretq_s16_u16(a), (n)))

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #undef simde_vshrn_n_u16
  #define simde_vshrn_n_u16(a, n) vshrn_n_u16((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vshrn_n_u16
  #define vshrn_n_u16(a, n) simde_vshrn_n_u16((a), (n))
#endif

#define simde_vshrn_n_u32(a, n) \
  simde_vreinterpret_u16_s16( \
      simde_vshrn_n_s32(simde_vreinterpretq_s32_u32(a), (n)))

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #undef simde_vshrn_n_u32
  #define simde_vshrn_n_u32(a, n) vshrn_n_u32((a), (n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vshrn_n_u32
  #define vshrn_n_u32(a, n) simde_vshrn_n_u32((a), (n))
#endif

#define simde_vshrn_n_u64(a, n) \
  simde_vreinterpret_u32_s32( \
      simde_vshrn_n_s64(simde_vreinterpretq_s64_u64(a), (n)))

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #undef simde_vshrn_n_u64
  #define simde_vshrn_n_u64(a, n) vshrn_n_u64((a), (n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vshrn_n_u64
  #define vshrn_n_u64(a, n) simde_vshrn_n_u64((a), (n))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_SHRN_N_H) */
