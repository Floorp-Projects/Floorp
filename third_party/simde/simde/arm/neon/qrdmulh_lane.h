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
 */

#if !defined(SIMDE_ARM_NEON_QRDMULH_LANE_H)
#define SIMDE_ARM_NEON_QRDMULH_LANE_H

#include "types.h"
#include "qrdmulh.h"
#include "dup_lane.h"
#include "get_lane.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(11,0,0)
    #define simde_vqrdmulhs_lane_s32(a, v, lane) \
    SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vqrdmulhs_lane_s32((a), (v), (lane)))
  #else
    #define simde_vqrdmulhs_lane_s32(a, v, lane) vqrdmulhs_lane_s32((a), (v), (lane))
  #endif
#else
  #define simde_vqrdmulhs_lane_s32(a, v, lane) simde_vqrdmulhs_s32((a), simde_vget_lane_s32((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhs_lane_s32
  #define vqrdmulhs_lane_s32(a, v, lane) simde_vqrdmulhs_lane_s32((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(11,0,0)
    #define simde_vqrdmulhs_laneq_s32(a, v, lane) \
    SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vqrdmulhs_laneq_s32((a), (v), (lane)))
  #else
    #define simde_vqrdmulhs_laneq_s32(a, v, lane) vqrdmulhs_laneq_s32((a), (v), (lane))
  #endif
#else
  #define simde_vqrdmulhs_laneq_s32(a, v, lane) simde_vqrdmulhs_s32((a), simde_vgetq_lane_s32((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhs_laneq_s32
  #define vqrdmulhs_laneq_s32(a, v, lane) simde_vqrdmulhs_laneq_s32((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrdmulh_lane_s16(a, v, lane) vqrdmulh_lane_s16((a), (v), (lane))
#else
  #define simde_vqrdmulh_lane_s16(a, v, lane) simde_vqrdmulh_s16((a), simde_vdup_lane_s16((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulh_lane_s16
  #define vqrdmulh_lane_s16(a, v, lane) simde_vqrdmulh_lane_s16((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrdmulh_lane_s32(a, v, lane) vqrdmulh_lane_s32((a), (v), (lane))
#else
  #define simde_vqrdmulh_lane_s32(a, v, lane) simde_vqrdmulh_s32((a), simde_vdup_lane_s32((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulh_lane_s32
  #define vqrdmulh_lane_s32(a, v, lane) simde_vqrdmulh_lane_s32((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrdmulhq_lane_s16(a, v, lane) vqrdmulhq_lane_s16((a), (v), (lane))
#else
  #define simde_vqrdmulhq_lane_s16(a, v, lane) simde_vqrdmulhq_s16((a), simde_vdupq_lane_s16((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhq_lane_s16
  #define vqrdmulhq_lane_s16(a, v, lane) simde_vqrdmulhq_lane_s16((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vqrdmulhq_lane_s32(a, v, lane) vqrdmulhq_lane_s32((a), (v), (lane))
#else
  #define simde_vqrdmulhq_lane_s32(a, v, lane) simde_vqrdmulhq_s32((a), simde_vdupq_lane_s32((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhq_lane_s32
  #define vqrdmulhq_lane_s32(a, v, lane) simde_vqrdmulhq_lane_s32((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqrdmulh_laneq_s16(a, v, lane) vqrdmulh_laneq_s16((a), (v), (lane))
#else
  #define simde_vqrdmulh_laneq_s16(a, v, lane) simde_vqrdmulh_s16((a), simde_vdup_laneq_s16((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulh_laneq_s16
  #define vqrdmulh_laneq_s16(a, v, lane) simde_vqrdmulh_laneq_s16((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqrdmulh_laneq_s32(a, v, lane) vqrdmulh_laneq_s32((a), (v), (lane))
#else
  #define simde_vqrdmulh_laneq_s32(a, v, lane) simde_vqrdmulh_s32((a), simde_vdup_laneq_s32((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulh_laneq_s32
  #define vqrdmulh_laneq_s32(a, v, lane) simde_vqrdmulh_laneq_s32((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqrdmulhq_laneq_s16(a, v, lane) vqrdmulhq_laneq_s16((a), (v), (lane))
#else
  #define simde_vqrdmulhq_laneq_s16(a, v, lane) simde_vqrdmulhq_s16((a), simde_vdupq_laneq_s16((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhq_laneq_s16
  #define vqrdmulhq_laneq_s16(a, v, lane) simde_vqrdmulhq_laneq_s16((a), (v), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vqrdmulhq_laneq_s32(a, v, lane) vqrdmulhq_laneq_s32((a), (v), (lane))
#else
  #define simde_vqrdmulhq_laneq_s32(a, v, lane) simde_vqrdmulhq_s32((a), simde_vdupq_laneq_s32((v), (lane)))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqrdmulhq_laneq_s32
  #define vqrdmulhq_laneq_s32(a, v, lane) simde_vqrdmulhq_laneq_s32((a), (v), (lane))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QRDMULH_LANE_H) */
