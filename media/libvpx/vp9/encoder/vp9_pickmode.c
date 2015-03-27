/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "vp9/common/vp9_pragmas.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vp9/encoder/vp9_treewriter.h"
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_reconintra.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_variance.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vpx_mem/vpx_mem.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_entropy.h"
#include "./vp9_rtcd.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_common.h"

static int full_pixel_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                     const TileInfo *const tile,
                                     BLOCK_SIZE bsize, int mi_row, int mi_col,
                                     int_mv *tmp_mv, int *rate_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  struct buf_2d backup_yv12[MAX_MB_PLANE] = {{0}};
  int bestsme = INT_MAX;
  int further_steps, step_param;
  int sadpb = x->sadperbit16;
  MV mvp_full;
  int ref = mbmi->ref_frame[0];
  int_mv ref_mv = mbmi->ref_mvs[ref][0];
  int i;

  int tmp_col_min = x->mv_col_min;
  int tmp_col_max = x->mv_col_max;
  int tmp_row_min = x->mv_row_min;
  int tmp_row_max = x->mv_row_max;

  int buf_offset;
  int stride = xd->plane[0].pre[0].stride;

  const YV12_BUFFER_CONFIG *scaled_ref_frame = vp9_get_scaled_ref_frame(cpi,
                                                                        ref);
  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[0];

    setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  vp9_set_mv_search_range(x, &ref_mv.as_mv);

  // TODO(jingning) exploiting adaptive motion search control in non-RD
  // mode decision too.
  step_param = 6;
  further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;

  for (i = LAST_FRAME; i <= LAST_FRAME && cpi->common.show_frame; ++i) {
    if ((x->pred_mv_sad[ref] >> 3) > x->pred_mv_sad[i]) {
      tmp_mv->as_int = INVALID_MV;

      if (scaled_ref_frame) {
        int i;
        for (i = 0; i < MAX_MB_PLANE; i++)
          xd->plane[i].pre[0] = backup_yv12[i];
      }
      return INT_MAX;
    }
  }

  mvp_full = mbmi->ref_mvs[ref][x->mv_best_ref_index[ref]].as_mv;

  mvp_full.col >>= 3;
  mvp_full.row >>= 3;

  vp9_full_pixel_diamond(cpi, x, &mvp_full, step_param, sadpb, further_steps, 1,
                         &cpi->fn_ptr[bsize], &ref_mv.as_mv, &tmp_mv->as_mv);
  x->mv_col_min = tmp_col_min;
  x->mv_col_max = tmp_col_max;
  x->mv_row_min = tmp_row_min;
  x->mv_row_max = tmp_row_max;

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[0] = backup_yv12[i];
  }

  // TODO(jingning) This step can be merged into full pixel search step in the
  // re-designed log-diamond search
  buf_offset = tmp_mv->as_mv.row * stride + tmp_mv->as_mv.col;

  // Find sad for current vector.
  bestsme = cpi->fn_ptr[bsize].sdf(x->plane[0].src.buf, x->plane[0].src.stride,
                                   xd->plane[0].pre[0].buf + buf_offset,
                                   stride, 0x7fffffff);

  // scale to 1/8 pixel resolution
  tmp_mv->as_mv.row = tmp_mv->as_mv.row * 8;
  tmp_mv->as_mv.col = tmp_mv->as_mv.col * 8;

  // calculate the bit cost on motion vector
  *rate_mv = vp9_mv_bit_cost(&tmp_mv->as_mv, &ref_mv.as_mv,
                             x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);


  return bestsme;
}

// TODO(jingning) placeholder for inter-frame non-RD mode decision.
// this needs various further optimizations. to be continued..
int64_t vp9_pick_inter_mode(VP9_COMP *cpi, MACROBLOCK *x,
                            const TileInfo *const tile,
                            int mi_row, int mi_col,
                            int *returnrate,
                            int64_t *returndistortion,
                            BLOCK_SIZE bsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  const BLOCK_SIZE block_size = get_plane_block_size(bsize, &xd->plane[0]);
  MB_PREDICTION_MODE this_mode;
  MV_REFERENCE_FRAME ref_frame;
  int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
  struct buf_2d yv12_mb[4][MAX_MB_PLANE];
  static const int flag_list[4] = { 0, VP9_LAST_FLAG, VP9_GOLD_FLAG,
                                    VP9_ALT_FLAG };
  int64_t best_rd = INT64_MAX;
  int64_t this_rd;
  int64_t cost[4]= { 0, 100, 150,  205 };

  x->skip_encode = cpi->sf.skip_encode_frame && x->q_index < QIDX_SKIP_THRESH;

  x->skip = 0;
  if (cpi->active_map_enabled && x->active_ptr[0] == 0)
    x->skip = 1;

  // initialize mode decisions
  *returnrate = INT_MAX;
  vpx_memset(mbmi, 0, sizeof(MB_MODE_INFO));
  mbmi->sb_type = bsize;
  mbmi->ref_frame[0] = NONE;
  mbmi->ref_frame[1] = NONE;
  mbmi->tx_size = MIN(max_txsize_lookup[bsize],
                      tx_mode_to_biggest_tx_size[cpi->common.tx_mode]);
  mbmi->interp_filter = cpi->common.interp_filter == SWITCHABLE ?
                        EIGHTTAP : cpi->common.interp_filter;
  mbmi->skip = 0;
  mbmi->segment_id = 0;

  for (ref_frame = LAST_FRAME; ref_frame <= LAST_FRAME ; ++ref_frame) {
    x->pred_mv_sad[ref_frame] = INT_MAX;
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      vp9_setup_buffer_inter(cpi, x, tile,
                             ref_frame, block_size, mi_row, mi_col,
                             frame_mv[NEARESTMV], frame_mv[NEARMV], yv12_mb);
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[ZEROMV][ref_frame].as_int = 0;
  }

  for (ref_frame = LAST_FRAME; ref_frame <= LAST_FRAME ; ++ref_frame) {
    int rate_mv = 0;

    if (!(cpi->ref_frame_flags & flag_list[ref_frame]))
      continue;

    // Select prediction reference frames.
    xd->plane[0].pre[0] = yv12_mb[ref_frame][0];

    clamp_mv2(&frame_mv[NEARESTMV][ref_frame].as_mv, xd);
    clamp_mv2(&frame_mv[NEARMV][ref_frame].as_mv, xd);

    for (this_mode = NEARESTMV; this_mode <= NEWMV; ++this_mode) {
      int rate = cost[INTER_OFFSET(this_mode)];
      int64_t dist;

      if (this_mode == NEWMV) {
        if (this_rd < 300)
          continue;

        x->mode_sad[ref_frame][INTER_OFFSET(NEWMV)] =
            full_pixel_motion_search(cpi, x, tile, bsize, mi_row, mi_col,
                                     &frame_mv[NEWMV][ref_frame], &rate_mv);

        if (frame_mv[NEWMV][ref_frame].as_int == INVALID_MV)
          continue;
      }

      dist = x->mode_sad[ref_frame][INTER_OFFSET(this_mode)];
      this_rd = rate + dist;

      if (this_rd < best_rd) {
        best_rd = this_rd;
        mbmi->mode = this_mode;
        mbmi->ref_frame[0] = ref_frame;
        mbmi->mv[0].as_int = frame_mv[this_mode][ref_frame].as_int;
        xd->mi_8x8[0]->bmi[0].as_mv[0].as_int = mbmi->mv[0].as_int;
        mbmi->uv_mode = this_mode;
      }
    }
  }

  // TODO(jingning) sub-pixel motion search, if NEWMV is chosen

  // TODO(jingning) intra prediction search, if the best SAD is above a certain
  // threshold.

  return INT64_MAX;
}
