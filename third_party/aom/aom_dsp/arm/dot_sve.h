/*
 *  Copyright (c) 2023, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AOM_DSP_ARM_DOT_SVE_H_
#define AOM_AOM_DSP_ARM_DOT_SVE_H_

#include <arm_neon_sve_bridge.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"

// Dot product instructions operating on 16-bit input elements are exclusive to
// the SVE instruction set. However, we can access these instructions from a
// predominantly Neon context by making use of the Neon-SVE bridge intrinsics
// to reinterpret Neon vectors as SVE vectors - with the high part of the SVE
// vector (if it's longer than 128 bits) being "don't care".

// While sub-optimal on machines that have SVE vector length > 128-bit - as the
// remainder of the vector is unused - this approach is still beneficial when
// compared to a Neon-only solution.

static INLINE uint64x2_t aom_udotq_u16(uint64x2_t acc, uint16x8_t x,
                                       uint16x8_t y) {
  return svget_neonq_u64(svdot_u64(svset_neonq_u64(svundef_u64(), acc),
                                   svset_neonq_u16(svundef_u16(), x),
                                   svset_neonq_u16(svundef_u16(), y)));
}

static INLINE int64x2_t aom_sdotq_s16(int64x2_t acc, int16x8_t x, int16x8_t y) {
  return svget_neonq_s64(svdot_s64(svset_neonq_s64(svundef_s64(), acc),
                                   svset_neonq_s16(svundef_s16(), x),
                                   svset_neonq_s16(svundef_s16(), y)));
}

#endif  // AOM_AOM_DSP_ARM_DOT_SVE_H_
