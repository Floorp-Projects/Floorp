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

PREDICTION_MODE av1_left_block_mode(const MODE_INFO *cur_mi,
                                    const MODE_INFO *left_mi, int b) {
  if (b == 0 || b == 2) {
    if (!left_mi || is_inter_block(&left_mi->mbmi)) return DC_PRED;

    return get_y_mode(left_mi, b + 1);
  } else {
    assert(b == 1 || b == 3);
    return cur_mi->bmi[b - 1].as_mode;
  }
}

PREDICTION_MODE av1_above_block_mode(const MODE_INFO *cur_mi,
                                     const MODE_INFO *above_mi, int b) {
  if (b == 0 || b == 1) {
    if (!above_mi || is_inter_block(&above_mi->mbmi)) return DC_PRED;

    return get_y_mode(above_mi, b + 2);
  } else {
    assert(b == 2 || b == 3);
    return cur_mi->bmi[b - 2].as_mode;
  }
}

#if CONFIG_COEF_INTERLEAVE
void av1_foreach_transformed_block_interleave(
    const MACROBLOCKD *const xd, BLOCK_SIZE bsize,
    foreach_transformed_block_visitor visit, void *arg) {
  const struct macroblockd_plane *const pd_y = &xd->plane[0];
  const struct macroblockd_plane *const pd_c = &xd->plane[1];
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

  const TX_SIZE tx_log2_y = mbmi->tx_size;
  const TX_SIZE tx_log2_c = av1_get_uv_tx_size(mbmi, pd_c);
  const int tx_sz_y = (1 << tx_log2_y);
  const int tx_sz_c = (1 << tx_log2_c);

  const BLOCK_SIZE plane_bsize_y = get_plane_block_size(bsize, pd_y);
  const BLOCK_SIZE plane_bsize_c = get_plane_block_size(bsize, pd_c);

  const int num_4x4_w_y = num_4x4_blocks_wide_lookup[plane_bsize_y];
  const int num_4x4_w_c = num_4x4_blocks_wide_lookup[plane_bsize_c];
  const int num_4x4_h_y = num_4x4_blocks_high_lookup[plane_bsize_y];
  const int num_4x4_h_c = num_4x4_blocks_high_lookup[plane_bsize_c];

  const int step_y = 1 << (tx_log2_y << 1);
  const int step_c = 1 << (tx_log2_c << 1);

  const int max_4x4_w_y =
      get_max_4x4_size(num_4x4_w_y, xd->mb_to_right_edge, pd_y->subsampling_x);
  const int max_4x4_h_y =
      get_max_4x4_size(num_4x4_h_y, xd->mb_to_bottom_edge, pd_y->subsampling_y);

  const int extra_step_y = ((num_4x4_w_y - max_4x4_w_y) >> tx_log2_y) * step_y;

  const int max_4x4_w_c =
      get_max_4x4_size(num_4x4_w_c, xd->mb_to_right_edge, pd_c->subsampling_x);
  const int max_4x4_h_c =
      get_max_4x4_size(num_4x4_h_c, xd->mb_to_bottom_edge, pd_c->subsampling_y);

  const int extra_step_c = ((num_4x4_w_c - max_4x4_w_c) >> tx_log2_c) * step_c;

  // The max_4x4_w/h may be smaller than tx_sz under some corner cases,
  // i.e. when the SB is splitted by tile boundaries.
  const int tu_num_w_y = (max_4x4_w_y + tx_sz_y - 1) / tx_sz_y;
  const int tu_num_h_y = (max_4x4_h_y + tx_sz_y - 1) / tx_sz_y;
  const int tu_num_w_c = (max_4x4_w_c + tx_sz_c - 1) / tx_sz_c;
  const int tu_num_h_c = (max_4x4_h_c + tx_sz_c - 1) / tx_sz_c;
  const int tu_num_c = tu_num_w_c * tu_num_h_c;

  int tu_idx_c = 0;
  int offset_y, row_y, col_y;
  int offset_c, row_c, col_c;

  for (row_y = 0; row_y < tu_num_h_y; row_y++) {
    for (col_y = 0; col_y < tu_num_w_y; col_y++) {
      // luma
      offset_y = (row_y * tu_num_w_y + col_y) * step_y + row_y * extra_step_y;
      visit(0, offset_y, row_y * tx_sz_y, col_y * tx_sz_y, plane_bsize_y,
            tx_log2_y, arg);
      // chroma
      if (tu_idx_c < tu_num_c) {
        row_c = (tu_idx_c / tu_num_w_c) * tx_sz_c;
        col_c = (tu_idx_c % tu_num_w_c) * tx_sz_c;
        offset_c = tu_idx_c * step_c + (tu_idx_c / tu_num_w_c) * extra_step_c;
        visit(1, offset_c, row_c, col_c, plane_bsize_c, tx_log2_c, arg);
        visit(2, offset_c, row_c, col_c, plane_bsize_c, tx_log2_c, arg);
        tu_idx_c++;
      }
    }
  }

  // In 422 case, it's possible that Chroma has more TUs than Luma
  while (tu_idx_c < tu_num_c) {
    row_c = (tu_idx_c / tu_num_w_c) * tx_sz_c;
    col_c = (tu_idx_c % tu_num_w_c) * tx_sz_c;
    offset_c = tu_idx_c * step_c + row_c * extra_step_c;
    visit(1, offset_c, row_c, col_c, plane_bsize_c, tx_log2_c, arg);
    visit(2, offset_c, row_c, col_c, plane_bsize_c, tx_log2_c, arg);
    tu_idx_c++;
  }
}
#endif

void av1_foreach_transformed_block_in_plane(
    const MACROBLOCKD *const xd, BLOCK_SIZE bsize, int plane,
    foreach_transformed_block_visitor visit, void *arg) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  // block and transform sizes, in number of 4x4 blocks log 2 ("*_b")
  // 4x4=0, 8x8=2, 16x16=4, 32x32=6, 64x64=8
  // transform size varies per plane, look it up in a common way.
  const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#else
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#endif
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

  const BLOCK_SIZE max_unit_bsize = get_plane_block_size(BLOCK_64X64, pd);
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

#if CONFIG_LV_MAP
void av1_foreach_transformed_block(const MACROBLOCKD *const xd,
                                   BLOCK_SIZE bsize, int mi_row, int mi_col,
                                   foreach_transformed_block_visitor visit,
                                   void *arg) {
  int plane;

  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_CB4X4
    if (!is_chroma_reference(mi_row, mi_col, bsize,
                             xd->plane[plane].subsampling_x,
                             xd->plane[plane].subsampling_y))
      continue;
#else
    (void)mi_row;
    (void)mi_col;
#endif
    av1_foreach_transformed_block_in_plane(xd, bsize, plane, visit, arg);
  }
}
#endif

#if !CONFIG_PVQ || CONFIG_VAR_TX
void av1_set_contexts(const MACROBLOCKD *xd, struct macroblockd_plane *pd,
                      int plane, TX_SIZE tx_size, int has_eob, int aoff,
                      int loff) {
  ENTROPY_CONTEXT *const a = pd->above_context + aoff;
  ENTROPY_CONTEXT *const l = pd->left_context + loff;
  const int txs_wide = tx_size_wide_unit[tx_size];
  const int txs_high = tx_size_high_unit[tx_size];
#if CONFIG_CB4X4
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
#else
  const BLOCK_SIZE bsize = AOMMAX(xd->mi[0]->mbmi.sb_type, BLOCK_8X8);
#endif
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);

  // above
  if (has_eob && xd->mb_to_right_edge < 0) {
    int i;
    const int blocks_wide = max_block_wide(xd, plane_bsize, plane);
    int above_contexts = txs_wide;
    if (above_contexts + aoff > blocks_wide)
      above_contexts = blocks_wide - aoff;

    for (i = 0; i < above_contexts; ++i) a[i] = has_eob;
    for (i = above_contexts; i < txs_wide; ++i) a[i] = 0;
  } else {
    memset(a, has_eob, sizeof(ENTROPY_CONTEXT) * txs_wide);
  }

  // left
  if (has_eob && xd->mb_to_bottom_edge < 0) {
    int i;
    const int blocks_high = max_block_high(xd, plane_bsize, plane);
    int left_contexts = txs_high;
    if (left_contexts + loff > blocks_high) left_contexts = blocks_high - loff;

    for (i = 0; i < left_contexts; ++i) l[i] = has_eob;
    for (i = left_contexts; i < txs_high; ++i) l[i] = 0;
  } else {
    memset(l, has_eob, sizeof(ENTROPY_CONTEXT) * txs_high);
  }
}
#endif

void av1_reset_skip_context(MACROBLOCKD *xd, int mi_row, int mi_col,
                            BLOCK_SIZE bsize) {
  int i;
  int nplanes;
#if CONFIG_CB4X4
  int chroma_ref;
  chroma_ref =
      is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y);
  nplanes = 1 + (MAX_MB_PLANE - 1) * chroma_ref;
#else
  (void)mi_row;
  (void)mi_col;
  nplanes = MAX_MB_PLANE;
#endif
  for (i = 0; i < nplanes; i++) {
    struct macroblockd_plane *const pd = &xd->plane[i];
#if CONFIG_CHROMA_SUB8X8
    const BLOCK_SIZE plane_bsize =
        AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#else
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#endif
    const int txs_wide = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int txs_high = block_size_high[plane_bsize] >> tx_size_high_log2[0];
    memset(pd->above_context, 0, sizeof(ENTROPY_CONTEXT) * txs_wide);
    memset(pd->left_context, 0, sizeof(ENTROPY_CONTEXT) * txs_high);
  }
}

void av1_setup_block_planes(MACROBLOCKD *xd, int ss_x, int ss_y) {
  int i;

  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].plane_type = get_plane_type(i);
    xd->plane[i].subsampling_x = i ? ss_x : 0;
    xd->plane[i].subsampling_y = i ? ss_y : 0;
  }
}

#if CONFIG_EXT_INTRA
const int16_t dr_intra_derivative[90] = {
  1,    14666, 7330, 4884, 3660, 2926, 2435, 2084, 1821, 1616, 1451, 1317, 1204,
  1108, 1026,  955,  892,  837,  787,  743,  703,  666,  633,  603,  574,  548,
  524,  502,   481,  461,  443,  426,  409,  394,  379,  365,  352,  339,  327,
  316,  305,   294,  284,  274,  265,  256,  247,  238,  230,  222,  214,  207,
  200,  192,   185,  179,  172,  166,  159,  153,  147,  141,  136,  130,  124,
  119,  113,   108,  103,  98,   93,   88,   83,   78,   73,   68,   63,   59,
  54,   49,    45,   40,   35,   31,   26,   22,   17,   13,   8,    4,
};

#if CONFIG_INTRA_INTERP
int av1_is_intra_filter_switchable(int angle) {
  assert(angle > 0 && angle < 270);
  if (angle % 45 == 0) return 0;
  if (angle > 90 && angle < 180) {
    return 1;
  } else {
    return ((angle < 90 ? dr_intra_derivative[angle]
                        : dr_intra_derivative[270 - angle]) &
            0xFF) > 0;
  }
}
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
