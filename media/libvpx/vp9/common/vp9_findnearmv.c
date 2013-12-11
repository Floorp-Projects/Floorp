/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/common/vp9_findnearmv.h"
#include "vp9/common/vp9_mvref_common.h"

static void lower_mv_precision(MV *mv, int allow_hp) {
  const int use_hp = allow_hp && vp9_use_mv_hp(mv);
  if (!use_hp) {
    if (mv->row & 1)
      mv->row += (mv->row > 0 ? -1 : 1);
    if (mv->col & 1)
      mv->col += (mv->col > 0 ? -1 : 1);
  }
}


void vp9_find_best_ref_mvs(MACROBLOCKD *xd, int allow_hp,
                           int_mv *mvlist, int_mv *nearest, int_mv *near) {
  int i;
  // Make sure all the candidates are properly clamped etc
  for (i = 0; i < MAX_MV_REF_CANDIDATES; ++i) {
    lower_mv_precision(&mvlist[i].as_mv, allow_hp);
    clamp_mv2(&mvlist[i].as_mv, xd);
  }
  *nearest = mvlist[0];
  *near = mvlist[1];
}

void vp9_append_sub8x8_mvs_for_idx(VP9_COMMON *cm, MACROBLOCKD *xd,
                                   const TileInfo *const tile,
                                   int_mv *dst_nearest,
                                   int_mv *dst_near,
                                   int block_idx, int ref_idx,
                                   int mi_row, int mi_col) {
  int_mv dst_list[MAX_MV_REF_CANDIDATES];
  int_mv mv_list[MAX_MV_REF_CANDIDATES];
  MODE_INFO *const mi = xd->mi_8x8[0];

  assert(ref_idx == 0 || ref_idx == 1);
  assert(MAX_MV_REF_CANDIDATES == 2);  // makes code here slightly easier

  vp9_find_mv_refs_idx(cm, xd, tile, mi, xd->last_mi,
                       mi->mbmi.ref_frame[ref_idx],
                       mv_list, block_idx, mi_row, mi_col);

  dst_list[1].as_int = 0;
  if (block_idx == 0) {
    vpx_memcpy(dst_list, mv_list, MAX_MV_REF_CANDIDATES * sizeof(int_mv));
  } else if (block_idx == 1 || block_idx == 2) {
    int dst = 0, n;
    b_mode_info *bmi = mi->bmi;

    dst_list[dst++].as_int = bmi[0].as_mv[ref_idx].as_int;
    for (n = 0; dst < MAX_MV_REF_CANDIDATES &&
                n < MAX_MV_REF_CANDIDATES; n++)
      if (mv_list[n].as_int != dst_list[0].as_int)
        dst_list[dst++].as_int = mv_list[n].as_int;
  } else {
    int dst = 0, n;
    b_mode_info *bmi = mi->bmi;

    assert(block_idx == 3);
    dst_list[dst++].as_int = bmi[2].as_mv[ref_idx].as_int;
    if (dst_list[0].as_int != bmi[1].as_mv[ref_idx].as_int)
      dst_list[dst++].as_int = bmi[1].as_mv[ref_idx].as_int;
    if (dst < MAX_MV_REF_CANDIDATES &&
        dst_list[0].as_int != bmi[0].as_mv[ref_idx].as_int)
      dst_list[dst++].as_int = bmi[0].as_mv[ref_idx].as_int;
    for (n = 0; dst < MAX_MV_REF_CANDIDATES &&
                n < MAX_MV_REF_CANDIDATES; n++)
      if (mv_list[n].as_int != dst_list[0].as_int)
        dst_list[dst++].as_int = mv_list[n].as_int;
  }

  dst_nearest->as_int = dst_list[0].as_int;
  dst_near->as_int = dst_list[1].as_int;
}
