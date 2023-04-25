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

#if !defined(SIMDE_ARM_NEON_SRA_N_H)
#define SIMDE_ARM_NEON_SRA_N_H

#include "add.h"
#include "shr_n.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vsrad_n_s64(a, b, n) vsrad_n_s64((a), (b), (n))
#else
  #define simde_vsrad_n_s64(a, b, n) simde_vaddd_s64((a), simde_vshrd_n_s64((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsrad_n_s64
  #define vsrad_n_s64(a, b, n) simde_vsrad_n_s64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vsrad_n_u64(a, b, n) vsrad_n_u64((a), (b), (n))
#else
  #define simde_vsrad_n_u64(a, b, n) simde_vaddd_u64((a), simde_vshrd_n_u64((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsrad_n_u64
  #define vsrad_n_u64(a, b, n) simde_vsrad_n_u64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsra_n_s8(a, b, n) vsra_n_s8((a), (b), (n))
#else
  #define simde_vsra_n_s8(a, b, n) simde_vadd_s8((a), simde_vshr_n_s8((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsra_n_s8
  #define vsra_n_s8(a, b, n) simde_vsra_n_s8((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsra_n_s16(a, b, n) vsra_n_s16((a), (b), (n))
#else
  #define simde_vsra_n_s16(a, b, n) simde_vadd_s16((a), simde_vshr_n_s16((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsra_n_s16
  #define vsra_n_s16(a, b, n) simde_vsra_n_s16((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsra_n_s32(a, b, n) vsra_n_s32((a), (b), (n))
#else
  #define simde_vsra_n_s32(a, b, n) simde_vadd_s32((a), simde_vshr_n_s32((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsra_n_s32
  #define vsra_n_s32(a, b, n) simde_vsra_n_s32((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsra_n_s64(a, b, n) vsra_n_s64((a), (b), (n))
#else
  #define simde_vsra_n_s64(a, b, n) simde_vadd_s64((a), simde_vshr_n_s64((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsra_n_s64
  #define vsra_n_s64(a, b, n) simde_vsra_n_s64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsra_n_u8(a, b, n) vsra_n_u8((a), (b), (n))
#else
  #define simde_vsra_n_u8(a, b, n) simde_vadd_u8((a), simde_vshr_n_u8((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsra_n_u8
  #define vsra_n_u8(a, b, n) simde_vsra_n_u8((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsra_n_u16(a, b, n) vsra_n_u16((a), (b), (n))
#else
  #define simde_vsra_n_u16(a, b, n) simde_vadd_u16((a), simde_vshr_n_u16((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsra_n_u16
  #define vsra_n_u16(a, b, n) simde_vsra_n_u16((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsra_n_u32(a, b, n) vsra_n_u32((a), (b), (n))
#else
  #define simde_vsra_n_u32(a, b, n) simde_vadd_u32((a), simde_vshr_n_u32((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsra_n_u32
  #define vsra_n_u32(a, b, n) simde_vsra_n_u32((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsra_n_u64(a, b, n) vsra_n_u64((a), (b), (n))
#else
  #define simde_vsra_n_u64(a, b, n) simde_vadd_u64((a), simde_vshr_n_u64((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsra_n_u64
  #define vsra_n_u64(a, b, n) simde_vsra_n_u64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsraq_n_s8(a, b, n) vsraq_n_s8((a), (b), (n))
#else
  #define simde_vsraq_n_s8(a, b, n) simde_vaddq_s8((a), simde_vshrq_n_s8((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsraq_n_s8
  #define vsraq_n_s8(a, b, n) simde_vsraq_n_s8((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsraq_n_s16(a, b, n) vsraq_n_s16((a), (b), (n))
#else
  #define simde_vsraq_n_s16(a, b, n) simde_vaddq_s16((a), simde_vshrq_n_s16((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsraq_n_s16
  #define vsraq_n_s16(a, b, n) simde_vsraq_n_s16((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsraq_n_s32(a, b, n) vsraq_n_s32((a), (b), (n))
#else
  #define simde_vsraq_n_s32(a, b, n) simde_vaddq_s32((a), simde_vshrq_n_s32((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsraq_n_s32
  #define vsraq_n_s32(a, b, n) simde_vsraq_n_s32((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsraq_n_s64(a, b, n) vsraq_n_s64((a), (b), (n))
#else
  #define simde_vsraq_n_s64(a, b, n) simde_vaddq_s64((a), simde_vshrq_n_s64((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsraq_n_s64
  #define vsraq_n_s64(a, b, n) simde_vsraq_n_s64((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsraq_n_u8(a, b, n) vsraq_n_u8((a), (b), (n))
#else
  #define simde_vsraq_n_u8(a, b, n) simde_vaddq_u8((a), simde_vshrq_n_u8((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsraq_n_u8
  #define vsraq_n_u8(a, b, n) simde_vsraq_n_u8((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsraq_n_u16(a, b, n) vsraq_n_u16((a), (b), (n))
#else
  #define simde_vsraq_n_u16(a, b, n) simde_vaddq_u16((a), simde_vshrq_n_u16((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsraq_n_u16
  #define vsraq_n_u16(a, b, n) simde_vsraq_n_u16((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsraq_n_u32(a, b, n) vsraq_n_u32((a), (b), (n))
#else
  #define simde_vsraq_n_u32(a, b, n) simde_vaddq_u32((a), simde_vshrq_n_u32((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsraq_n_u32
  #define vsraq_n_u32(a, b, n) simde_vsraq_n_u32((a), (b), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vsraq_n_u64(a, b, n) vsraq_n_u64((a), (b), (n))
#else
  #define simde_vsraq_n_u64(a, b, n) simde_vaddq_u64((a), simde_vshrq_n_u64((b), (n)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsraq_n_u64
  #define vsraq_n_u64(a, b, n) simde_vsraq_n_u64((a), (b), (n))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_SRA_N_H) */
