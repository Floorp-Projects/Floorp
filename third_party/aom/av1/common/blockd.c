/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>

#include "aom_ports/system_state.h"

#include "av1/common/blockd.h"
#include "av1/common/onyxc_int.h"

PREDICTION_MODE av1_left_block_mode(const MB_MODE_INFO *left_mi) {
  if (!left_mi) return DC_PRED;
  assert(!is_inter_block(left_mi) || is_intrabc_block(left_mi));
  return left_mi->mode;
}

PREDICTION_MODE av1_above_block_mode(const MB_MODE_INFO *above_mi) {
  if (!above_mi) return DC_PRED;
  assert(!is_inter_block(above_mi) || is_intrabc_block(above_mi));
  return above_mi->mode;
}

void av1_foreach_transformed_block_in_plane(
    const MACROBLOCKD *const xd, BLOCK_SIZE bsize, int plane,
    foreach_transformed_block_visitor visit, void *arg) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  // block and transform sizes, in number of 4x4 blocks log 2 ("*_b")
  // 4x4=0, 8x8=2, 16x16=4, 32x32=6, 64x64=8
  // transform size varies per plane, look it up in a common way.
  const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
  const uint8_t txw_unit = tx_size_wide_unit[tx_size];
  const uint8_t txh_unit = tx_size_high_unit[tx_size];
  const int step = txw_unit * txh_unit;
  int i = 0, r, c;

  // If mb_to_right_edge is < 0 we are in a situation in which
  // the current block size extends into the UMV and we won't
  // visit the sub blocks that are wholly within the UMV.
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);

  int blk_row, blk_col;

  const BLOCK_SIZE max_unit_bsize =
      get_plane_block_size(BLOCK_64X64, pd->subsampling_x, pd->subsampling_y);
  int mu_blocks_wide = block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
  int mu_blocks_high = block_size_high[max_unit_bsize] >> tx_size_high_log2[0];
  mu_blocks_wide = AOMMIN(max_blocks_wide, mu_blocks_wide);
  mu_blocks_high = AOMMIN(max_blocks_high, mu_blocks_high);

  // Keep track of the row and column of the blocks we use so that we know
  // if we are in the unrestricted motion border.
  for (r = 0; r < max_blocks_high; r += mu_blocks_high) {
    const int unit_height = AOMMIN(mu_blocks_high + r, max_blocks_high);
    // Skip visiting the sub blocks that are wholly within the UMV.
    for (c = 0; c < max_blocks_wide; c += mu_blocks_wide) {
      const int unit_width = AOMMIN(mu_blocks_wide + c, max_blocks_wide);
      for (blk_row = r; blk_row < unit_height; blk_row += txh_unit) {
        for (blk_col = c; blk_col < unit_width; blk_col += txw_unit) {
          visit(plane, i, blk_row, blk_col, plane_bsize, tx_size, arg);
          i += step;
        }
      }
    }
  }
}

void av1_foreach_transformed_block(const MACROBLOCKD *const xd,
                                   BLOCK_SIZE bsize, int mi_row, int mi_col,
                                   foreach_transformed_block_visitor visit,
                                   void *arg, const int num_planes) {
  for (int plane = 0; plane < num_planes; ++plane) {
    if (!is_chroma_reference(mi_row, mi_col, bsize,
                             xd->plane[plane].subsampling_x,
                             xd->plane[plane].subsampling_y))
      continue;
    av1_foreach_transformed_block_in_plane(xd, bsize, plane, visit, arg);
  }
}

void av1_set_contexts(const MACROBLOCKD *xd, struct macroblockd_plane *pd,
                      int plane, BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                      int has_eob, int aoff, int loff) {
  ENTROPY_CONTEXT *const a = pd->above_context + aoff;
  ENTROPY_CONTEXT *const l = pd->left_context + loff;
  const int txs_wide = tx_size_wide_unit[tx_size];
  const int txs_high = tx_size_high_unit[tx_size];

  // above
  if (has_eob && xd->mb_to_right_edge < 0) {
    const int blocks_wide = max_block_wide(xd, plane_bsize, plane);
    const int above_contexts = AOMMIN(txs_wide, blocks_wide - aoff);
    memset(a, has_eob, sizeof(*a) * above_contexts);
    memset(a + above_contexts, 0, sizeof(*a) * (txs_wide - above_contexts));
  } else {
    memset(a, has_eob, sizeof(*a) * txs_wide);
  }

  // left
  if (has_eob && xd->mb_to_bottom_edge < 0) {
    const int blocks_high = max_block_high(xd, plane_bsize, plane);
    const int left_contexts = AOMMIN(txs_high, blocks_high - loff);
    memset(l, has_eob, sizeof(*l) * left_contexts);
    memset(l + left_contexts, 0, sizeof(*l) * (txs_high - left_contexts));
  } else {
    memset(l, has_eob, sizeof(*l) * txs_high);
  }
}
void av1_reset_skip_context(MACROBLOCKD *xd, int mi_row, int mi_col,
                            BLOCK_SIZE bsize, const int num_planes) {
  int i;
  int nplanes;
  int chroma_ref;
  chroma_ref =
      is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y);
  nplanes = 1 + (num_planes - 1) * chroma_ref;
  for (i = 0; i < nplanes; i++) {
    struct macroblockd_plane *const pd = &xd->plane[i];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    const int txs_wide = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int txs_high = block_size_high[plane_bsize] >> tx_size_high_log2[0];
    memset(pd->above_context, 0, sizeof(ENTROPY_CONTEXT) * txs_wide);
    memset(pd->left_context, 0, sizeof(ENTROPY_CONTEXT) * txs_high);
  }
}

void av1_reset_loop_filter_delta(MACROBLOCKD *xd, int num_planes) {
  xd->delta_lf_from_base = 0;
  const int frame_lf_count =
      num_planes > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
  for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) xd->delta_lf[lf_id] = 0;
}

void av1_reset_loop_restoration(MACROBLOCKD *xd, const int num_planes) {
  for (int p = 0; p < num_planes; ++p) {
    set_default_wiener(xd->wiener_info + p);
    set_default_sgrproj(xd->sgrproj_info + p);
  }
}

void av1_setup_block_planes(MACROBLOCKD *xd, int ss_x, int ss_y,
                            const int num_planes) {
  int i;

  for (i = 0; i < num_planes; i++) {
    xd->plane[i].plane_type = get_plane_type(i);
    xd->plane[i].subsampling_x = i ? ss_x : 0;
    xd->plane[i].subsampling_y = i ? ss_y : 0;
  }
}

const int16_t dr_intra_derivative[90] = {
  // More evenly spread out angles and limited to 10-bit
  // Values that are 0 will never be used
  //                    Approx angle
  0,    0, 0,        //
  1023, 0, 0,        // 3, ...
  547,  0, 0,        // 6, ...
  372,  0, 0, 0, 0,  // 9, ...
  273,  0, 0,        // 14, ...
  215,  0, 0,        // 17, ...
  178,  0, 0,        // 20, ...
  151,  0, 0,        // 23, ... (113 & 203 are base angles)
  132,  0, 0,        // 26, ...
  116,  0, 0,        // 29, ...
  102,  0, 0, 0,     // 32, ...
  90,   0, 0,        // 36, ...
  80,   0, 0,        // 39, ...
  71,   0, 0,        // 42, ...
  64,   0, 0,        // 45, ... (45 & 135 are base angles)
  57,   0, 0,        // 48, ...
  51,   0, 0,        // 51, ...
  45,   0, 0, 0,     // 54, ...
  40,   0, 0,        // 58, ...
  35,   0, 0,        // 61, ...
  31,   0, 0,        // 64, ...
  27,   0, 0,        // 67, ... (67 & 157 are base angles)
  23,   0, 0,        // 70, ...
  19,   0, 0,        // 73, ...
  15,   0, 0, 0, 0,  // 76, ...
  11,   0, 0,        // 81, ...
  7,    0, 0,        // 84, ...
  3,    0, 0,        // 87, ...
};
