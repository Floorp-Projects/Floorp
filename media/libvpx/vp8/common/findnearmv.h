/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_FINDNEARMV_H
#define __INC_FINDNEARMV_H

#include "mv.h"
#include "blockd.h"
#include "modecont.h"
#include "treecoder.h"

void vp8_find_near_mvs
(
    MACROBLOCKD *xd,
    const MODE_INFO *here,
    MV *nearest, MV *nearby, MV *best,
    int near_mv_ref_cts[4],
    int refframe,
    int *ref_frame_sign_bias
);

vp8_prob *vp8_mv_ref_probs(
    vp8_prob p[VP8_MVREFS-1], const int near_mv_ref_ct[4]
);

const B_MODE_INFO *vp8_left_bmi(const MODE_INFO *cur_mb, int b);

const B_MODE_INFO *vp8_above_bmi(const MODE_INFO *cur_mb, int b, int mi_stride);

#define LEFT_TOP_MARGIN (16 << 3)
#define RIGHT_BOTTOM_MARGIN (16 << 3)


#endif
