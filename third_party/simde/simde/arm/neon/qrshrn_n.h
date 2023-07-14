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

#if !defined(SIMDE_ARM_NEON_QRSHRN_N_H)
#define SIMDE_ARM_NEON_QRSHRN_N_H

#include "types.h"
#include "rshr_n.h"
#include "qmovn.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqrshrns_n_s32(a, n) vqrshrns_n_s32(a, n)
#else
  #define simde_vqrshrns_n_s32(a, n) simde_vqmovns_s32(simde_x_vrshrs_n_s32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrshrns_n_s32
  #define vqrshrns_n_s32(a, n) simde_vqrshrns_n_s32(a, n)
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqrshrns_n_u32(a, n) vqrshrns_n_u32(a, n)
#else
  #define simde_vqrshrns_n_u32(a, n) simde_vqmovns_u32(simde_x_vrshrs_n_u32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrshrns_n_u32
  #define vqrshrns_n_u32(a, n) simde_vqrshrns_n_u32(a, n)
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqrshrnd_n_s64(a, n) vqrshrnd_n_s64(a, n)
#else
  #define simde_vqrshrnd_n_s64(a, n) simde_vqmovnd_s64(simde_vrshrd_n_s64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrshrnd_n_s64
  #define vqrshrnd_n_s64(a, n) simde_vqrshrnd_n_s64(a, n)
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqrshrnd_n_u64(a, n) vqrshrnd_n_u64(a, n)
#else
  #define simde_vqrshrnd_n_u64(a, n) simde_vqmovnd_u64(simde_vrshrd_n_u64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrshrnd_n_u64
  #define vqrshrnd_n_u64(a, n) simde_vqrshrnd_n_u64(a, n)
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrshrn_n_s16(a, n) vqrshrn_n_s16((a), (n))
#else
  #define simde_vqrshrn_n_s16(a, n) simde_vqmovn_s16(simde_vrshrq_n_s16(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrshrn_n_s16
  #define vqrshrn_n_s16(a, n) simde_vqrshrn_n_s16((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrshrn_n_s32(a, n) vqrshrn_n_s32((a), (n))
#else
  #define simde_vqrshrn_n_s32(a, n) simde_vqmovn_s32(simde_vrshrq_n_s32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrshrn_n_s32
  #define vqrshrn_n_s32(a, n) simde_vqrshrn_n_s32((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrshrn_n_s64(a, n) vqrshrn_n_s64((a), (n))
#else
  #define simde_vqrshrn_n_s64(a, n) simde_vqmovn_s64(simde_vrshrq_n_s64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrshrn_n_s64
  #define vqrshrn_n_s64(a, n) simde_vqrshrn_n_s64((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrshrn_n_u16(a, n) vqrshrn_n_u16((a), (n))
#else
  #define simde_vqrshrn_n_u16(a, n) simde_vqmovn_u16(simde_vrshrq_n_u16(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrshrn_n_u16
  #define vqrshrn_n_u16(a, n) simde_vqrshrn_n_u16((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrshrn_n_u32(a, n) vqrshrn_n_u32((a), (n))
#else
  #define simde_vqrshrn_n_u32(a, n) simde_vqmovn_u32(simde_vrshrq_n_u32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrshrn_n_u32
  #define vqrshrn_n_u32(a, n) simde_vqrshrn_n_u32((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrshrn_n_u64(a, n) vqrshrn_n_u64((a), (n))
#else
  #define simde_vqrshrn_n_u64(a, n) simde_vqmovn_u64(simde_vrshrq_n_u64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrshrn_n_u64
  #define vqrshrn_n_u64(a, n) simde_vqrshrn_n_u64((a), (n))
#endif


SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QRSHRN_N_H) */
