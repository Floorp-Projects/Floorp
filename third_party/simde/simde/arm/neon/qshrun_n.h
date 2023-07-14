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

#if !defined(SIMDE_ARM_NEON_QSHRUN_N_H)
#define SIMDE_ARM_NEON_QSHRUN_N_H

#include "types.h"
#include "shr_n.h"
#include "qmovun.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqshruns_n_s32(a, n) HEDLEY_STATIC_CAST(uint16_t, vqshruns_n_s32((a), (n)))
#else
  #define simde_vqshruns_n_s32(a, n) simde_vqmovuns_s32(simde_x_vshrs_n_s32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshruns_n_s32
  #define vqshruns_n_s32(a, n) simde_vqshruns_n_s32(a, n)
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqshrund_n_s64(a, n) HEDLEY_STATIC_CAST(uint32_t, vqshrund_n_s64((a), (n)))
#else
  #define simde_vqshrund_n_s64(a, n) simde_vqmovund_s64(simde_vshrd_n_s64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqshrund_n_s64
  #define vqshrund_n_s64(a, n) simde_vqshrund_n_s64(a, n)
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshrun_n_s16(a, n) vqshrun_n_s16((a), (n))
#else
  #define simde_vqshrun_n_s16(a, n) simde_vqmovun_s16(simde_vshrq_n_s16(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshrun_n_s16
  #define vqshrun_n_s16(a, n) simde_vqshrun_n_s16((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshrun_n_s32(a, n) vqshrun_n_s32((a), (n))
#else
  #define simde_vqshrun_n_s32(a, n) simde_vqmovun_s32(simde_vshrq_n_s32(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshrun_n_s32
  #define vqshrun_n_s32(a, n) simde_vqshrun_n_s32((a), (n))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqshrun_n_s64(a, n) vqshrun_n_s64((a), (n))
#else
  #define simde_vqshrun_n_s64(a, n) simde_vqmovun_s64(simde_vshrq_n_s64(a, n))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqshrun_n_s64
  #define vqshrun_n_s64(a, n) simde_vqshrun_n_s64((a), (n))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QSHRUN_N_H) */
