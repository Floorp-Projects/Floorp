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
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_NEON_SRI_N_H)
#define SIMDE_ARM_NEON_SRI_N_H

#include "types.h"
#include "shr_n.h"
#include "dup_n.h"
#include "and.h"
#include "orr.h"
#include "reinterpret.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vsrid_n_s64(a, b, n) vsrid_n_s64(a, b, n)
#else
  #define simde_vsrid_n_s64(a, b, n) \
    HEDLEY_STATIC_CAST(int64_t, \
      simde_vsrid_n_u64(HEDLEY_STATIC_CAST(uint64_t, a), HEDLEY_STATIC_CAST(uint64_t, b), n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsrid_n_s64
  #define vsrid_n_s64(a, b, n) simde_vsrid_n_s64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vsrid_n_u64(a, b, n) vsrid_n_u64(a, b, n)
#else
#define simde_vsrid_n_u64(a, b, n) \
    (((a & (UINT64_C(0xffffffffffffffff) >> (64 - n) << (64 - n))) | simde_vshrd_n_u64((b), (n))))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsrid_n_u64
  #define vsrid_n_u64(a, b, n) simde_vsrid_n_u64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsri_n_s8(a, b, n) vsri_n_s8((a), (b), (n))
#else
  #define simde_vsri_n_s8(a, b, n) \
    simde_vreinterpret_s8_u8(simde_vsri_n_u8( \
        simde_vreinterpret_u8_s8((a)), simde_vreinterpret_u8_s8((b)), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsri_n_s8
  #define vsri_n_s8(a, b, n) simde_vsri_n_s8((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsri_n_u8(a, b, n) vsri_n_u8((a), (b), (n))
#else
  #define simde_vsri_n_u8(a, b, n) \
    simde_vorr_u8( \
        simde_vand_u8((a), simde_vdup_n_u8((UINT8_C(0xff) >> (8 - n) << (8 - n)))), \
        simde_vshr_n_u8((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsri_n_u8
  #define vsri_n_u8(a, b, n) simde_vsri_n_u8((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsri_n_s16(a, b, n) vsri_n_s16((a), (b), (n))
#else
  #define simde_vsri_n_s16(a, b, n) \
    simde_vreinterpret_s16_u16(simde_vsri_n_u16( \
        simde_vreinterpret_u16_s16((a)), simde_vreinterpret_u16_s16((b)), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsri_n_s16
  #define vsri_n_s16(a, b, n) simde_vsri_n_s16((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsri_n_u16(a, b, n) vsri_n_u16((a), (b), (n))
#else
  #define simde_vsri_n_u16(a, b, n) \
    simde_vorr_u16( \
        simde_vand_u16((a), simde_vdup_n_u16((UINT16_C(0xffff) >> (16 - n) << (16 - n)))), \
        simde_vshr_n_u16((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsri_n_u16
  #define vsri_n_u16(a, b, n) simde_vsri_n_u16((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsri_n_s32(a, b, n) vsri_n_s32((a), (b), (n))
#else
  #define simde_vsri_n_s32(a, b, n) \
    simde_vreinterpret_s32_u32(simde_vsri_n_u32( \
        simde_vreinterpret_u32_s32((a)), simde_vreinterpret_u32_s32((b)), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsri_n_s32
  #define vsri_n_s32(a, b, n) simde_vsri_n_s32((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsri_n_u32(a, b, n) vsri_n_u32((a), (b), (n))
#else
  #define simde_vsri_n_u32(a, b, n) \
    simde_vorr_u32( \
        simde_vand_u32((a), \
                      simde_vdup_n_u32((UINT32_C(0xffffffff) >> (32 - n) << (32 - n)))), \
        simde_vshr_n_u32((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsri_n_u32
  #define vsri_n_u32(a, b, n) simde_vsri_n_u32((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsri_n_s64(a, b, n) vsri_n_s64((a), (b), (n))
#else
  #define simde_vsri_n_s64(a, b, n) \
    simde_vreinterpret_s64_u64(simde_vsri_n_u64( \
        simde_vreinterpret_u64_s64((a)), simde_vreinterpret_u64_s64((b)), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsri_n_s64
  #define vsri_n_s64(a, b, n) simde_vsri_n_s64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsri_n_u64(a, b, n) vsri_n_u64((a), (b), (n))
#else
#define simde_vsri_n_u64(a, b, n) \
    simde_vorr_u64( \
        simde_vand_u64((a), simde_vdup_n_u64( \
                                (UINT64_C(0xffffffffffffffff) >> (64 - n) << (64 - n)))), \
        simde_vshr_n_u64((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsri_n_u64
  #define vsri_n_u64(a, b, n) simde_vsri_n_u64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsriq_n_s8(a, b, n) vsriq_n_s8((a), (b), (n))
#else
  #define simde_vsriq_n_s8(a, b, n) \
    simde_vreinterpretq_s8_u8(simde_vsriq_n_u8( \
        simde_vreinterpretq_u8_s8((a)), simde_vreinterpretq_u8_s8((b)), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsriq_n_s8
  #define vsriq_n_s8(a, b, n) simde_vsriq_n_s8((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsriq_n_u8(a, b, n) vsriq_n_u8((a), (b), (n))
#else
  #define simde_vsriq_n_u8(a, b, n) \
    simde_vorrq_u8( \
        simde_vandq_u8((a), simde_vdupq_n_u8((UINT8_C(0xff) >> (8 - n) << (8 - n)))), \
        simde_vshrq_n_u8((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsriq_n_u8
  #define vsriq_n_u8(a, b, n) simde_vsriq_n_u8((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsriq_n_s16(a, b, n) vsriq_n_s16((a), (b), (n))
#else
  #define simde_vsriq_n_s16(a, b, n) \
    simde_vreinterpretq_s16_u16(simde_vsriq_n_u16( \
        simde_vreinterpretq_u16_s16((a)), simde_vreinterpretq_u16_s16((b)), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsriq_n_s16
  #define vsriq_n_s16(a, b, n) simde_vsriq_n_s16((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsriq_n_u16(a, b, n) vsriq_n_u16((a), (b), (n))
#else
  #define simde_vsriq_n_u16(a, b, n) \
    simde_vorrq_u16( \
        simde_vandq_u16((a), simde_vdupq_n_u16((UINT16_C(0xffff) >> (16 - n) << (16 - n)))), \
        simde_vshrq_n_u16((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsriq_n_u16
  #define vsriq_n_u16(a, b, n) simde_vsriq_n_u16((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsriq_n_s32(a, b, n) vsriq_n_s32((a), (b), (n))
#else
  #define simde_vsriq_n_s32(a, b, n) \
    simde_vreinterpretq_s32_u32(simde_vsriq_n_u32( \
        simde_vreinterpretq_u32_s32((a)), simde_vreinterpretq_u32_s32((b)), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsriq_n_s32
  #define vsriq_n_s32(a, b, n) simde_vsriq_n_s32((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsriq_n_u32(a, b, n) vsriq_n_u32((a), (b), (n))
#else
  #define simde_vsriq_n_u32(a, b, n) \
    simde_vorrq_u32( \
        simde_vandq_u32((a), \
                      simde_vdupq_n_u32((UINT32_C(0xffffffff) >> (32 - n) << (32 - n)))), \
        simde_vshrq_n_u32((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsriq_n_u32
  #define vsriq_n_u32(a, b, n) simde_vsriq_n_u32((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsriq_n_s64(a, b, n) vsriq_n_s64((a), (b), (n))
#else
  #define simde_vsriq_n_s64(a, b, n) \
    simde_vreinterpretq_s64_u64(simde_vsriq_n_u64( \
        simde_vreinterpretq_u64_s64((a)), simde_vreinterpretq_u64_s64((b)), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsriq_n_s64
  #define vsriq_n_s64(a, b, n) simde_vsriq_n_s64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsriq_n_u64(a, b, n) vsriq_n_u64((a), (b), (n))
#else
#define simde_vsriq_n_u64(a, b, n) \
    simde_vorrq_u64( \
        simde_vandq_u64((a), simde_vdupq_n_u64( \
                                (UINT64_C(0xffffffffffffffff) >> (64 - n) << (64 - n)))), \
        simde_vshrq_n_u64((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsriq_n_u64
  #define vsriq_n_u64(a, b, n) simde_vsriq_n_u64((a), (b), (n))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_SRI_N_H) */
