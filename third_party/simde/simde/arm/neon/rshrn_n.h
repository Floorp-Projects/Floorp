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

#if !defined(SIMDE_ARM_NEON_RSHRN_N_H)
#define SIMDE_ARM_NEON_RSHRN_N_H

#include "rshr_n.h"
#include "movn.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrn_n_s16(a, n) vrshrn_n_s16((a), (n))
#else
  #define simde_vrshrn_n_s16(a, n) simde_vmovn_s16(simde_vrshrq_n_s16(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrn_n_s16
  #define vrshrn_n_s16(a, n) simde_vrshrn_n_s16((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrn_n_s32(a, n) vrshrn_n_s32((a), (n))
#else
  #define simde_vrshrn_n_s32(a, n) simde_vmovn_s32(simde_vrshrq_n_s32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrn_n_s32
  #define vrshrn_n_s32(a, n) simde_vrshrn_n_s32((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrn_n_s64(a, n) vrshrn_n_s64((a), (n))
#else
  #define simde_vrshrn_n_s64(a, n) simde_vmovn_s64(simde_vrshrq_n_s64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrn_n_s64
  #define vrshrn_n_s64(a, n) simde_vrshrn_n_s64((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrn_n_u16(a, n) vrshrn_n_u16((a), (n))
#else
  #define simde_vrshrn_n_u16(a, n) simde_vmovn_u16(simde_vrshrq_n_u16(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrn_n_u16
  #define vrshrn_n_u16(a, n) simde_vrshrn_n_u16((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrn_n_u32(a, n) vrshrn_n_u32((a), (n))
#else
  #define simde_vrshrn_n_u32(a, n) simde_vmovn_u32(simde_vrshrq_n_u32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrn_n_u32
  #define vrshrn_n_u32(a, n) simde_vrshrn_n_u32((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vrshrn_n_u64(a, n) vrshrn_n_u64((a), (n))
#else
  #define simde_vrshrn_n_u64(a, n) simde_vmovn_u64(simde_vrshrq_n_u64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshrn_n_u64
  #define vrshrn_n_u64(a, n) simde_vrshrn_n_u64((a), (n))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_RSHRN_N_H) */
