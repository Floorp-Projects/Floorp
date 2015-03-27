/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VP9_COMMON_VP9_MVREF_COMMON_H_
#define VP9_COMMON_VP9_MVREF_COMMON_H_

#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/common/vp9_blockd.h"

#ifdef __cplusplus
extern "C" {
#endif


void vp9_find_mv_refs_idx(const VP9_COMMON *cm, const MACROBLOCKD *xd,
                          const TileInfo *const tile,
                          MODE_INFO *mi, const MODE_INFO *prev_mi,
                          MV_REFERENCE_FRAME ref_frame,
                          int_mv *mv_ref_list,
                          int block_idx,
                          int mi_row, int mi_col);

static INLINE void vp9_find_mv_refs(const VP9_COMMON *cm, const MACROBLOCKD *xd,
                                    const TileInfo *const tile,
                                    MODE_INFO *mi, const MODE_INFO *prev_mi,
                                    MV_REFERENCE_FRAME ref_frame,
                                    int_mv *mv_ref_list,
                                    int mi_row, int mi_col) {
  vp9_find_mv_refs_idx(cm, xd, tile, mi, prev_mi, ref_frame,
                       mv_ref_list, -1, mi_row, mi_col);
}

#define LEFT_TOP_MARGIN     ((VP9_ENC_BORDER_IN_PIXELS  \
                            - VP9_INTERP_EXTEND) << 3)
#define RIGHT_BOTTOM_MARGIN ((VP9_ENC_BORDER_IN_PIXELS  \
                            - VP9_INTERP_EXTEND) << 3)

// check a list of motion vectors by sad score using a number rows of pixels
// above and a number cols of pixels in the left to select the one with best
// score to use as ref motion vector
void vp9_find_best_ref_mvs(MACROBLOCKD *xd, int allow_hp,
                           int_mv *mvlist, int_mv *nearest, int_mv *near);

// TODO(jingning): this mv clamping function should be block size dependent.
static INLINE void clamp_mv2(MV *mv, const MACROBLOCKD *xd) {
  clamp_mv(mv, xd->mb_to_left_edge - LEFT_TOP_MARGIN,
               xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN,
               xd->mb_to_top_edge - LEFT_TOP_MARGIN,
               xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN);
}

void vp9_append_sub8x8_mvs_for_idx(VP9_COMMON *cm, MACROBLOCKD *xd,
                                   const TileInfo *const tile,
                                   int block, int ref, int mi_row, int mi_col,
                                   int_mv *nearest, int_mv *near);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_MVREF_COMMON_H_
