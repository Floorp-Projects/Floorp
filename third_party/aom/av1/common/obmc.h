/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_COMMON_OBMC_H_
#define AV1_COMMON_OBMC_H_

typedef void (*overlappable_nb_visitor_t)(MACROBLOCKD *xd, int rel_mi_pos,
                                          uint8_t nb_mi_size,
                                          MB_MODE_INFO *nb_mi, void *fun_ctxt,
                                          const int num_planes);

static INLINE void foreach_overlappable_nb_above(const AV1_COMMON *cm,
                                                 MACROBLOCKD *xd, int mi_col,
                                                 int nb_max,
                                                 overlappable_nb_visitor_t fun,
                                                 void *fun_ctxt) {
  const int num_planes = av1_num_planes(cm);
  if (!xd->up_available) return;

  int nb_count = 0;

  // prev_row_mi points into the mi array, starting at the beginning of the
  // previous row.
  MB_MODE_INFO **prev_row_mi = xd->mi - mi_col - 1 * xd->mi_stride;
  const int end_col = AOMMIN(mi_col + xd->n8_w, cm->mi_cols);
  uint8_t mi_step;
  for (int above_mi_col = mi_col; above_mi_col < end_col && nb_count < nb_max;
       above_mi_col += mi_step) {
    MB_MODE_INFO **above_mi = prev_row_mi + above_mi_col;
    mi_step =
        AOMMIN(mi_size_wide[above_mi[0]->sb_type], mi_size_wide[BLOCK_64X64]);
    // If we're considering a block with width 4, it should be treated as
    // half of a pair of blocks with chroma information in the second. Move
    // above_mi_col back to the start of the pair if needed, set above_mbmi
    // to point at the block with chroma information, and set mi_step to 2 to
    // step over the entire pair at the end of the iteration.
    if (mi_step == 1) {
      above_mi_col &= ~1;
      above_mi = prev_row_mi + above_mi_col + 1;
      mi_step = 2;
    }
    if (is_neighbor_overlappable(*above_mi)) {
      ++nb_count;
      fun(xd, above_mi_col - mi_col, AOMMIN(xd->n8_w, mi_step), *above_mi,
          fun_ctxt, num_planes);
    }
  }
}

static INLINE void foreach_overlappable_nb_left(const AV1_COMMON *cm,
                                                MACROBLOCKD *xd, int mi_row,
                                                int nb_max,
                                                overlappable_nb_visitor_t fun,
                                                void *fun_ctxt) {
  const int num_planes = av1_num_planes(cm);
  if (!xd->left_available) return;

  int nb_count = 0;

  // prev_col_mi points into the mi array, starting at the top of the
  // previous column
  MB_MODE_INFO **prev_col_mi = xd->mi - 1 - mi_row * xd->mi_stride;
  const int end_row = AOMMIN(mi_row + xd->n8_h, cm->mi_rows);
  uint8_t mi_step;
  for (int left_mi_row = mi_row; left_mi_row < end_row && nb_count < nb_max;
       left_mi_row += mi_step) {
    MB_MODE_INFO **left_mi = prev_col_mi + left_mi_row * xd->mi_stride;
    mi_step =
        AOMMIN(mi_size_high[left_mi[0]->sb_type], mi_size_high[BLOCK_64X64]);
    if (mi_step == 1) {
      left_mi_row &= ~1;
      left_mi = prev_col_mi + (left_mi_row + 1) * xd->mi_stride;
      mi_step = 2;
    }
    if (is_neighbor_overlappable(*left_mi)) {
      ++nb_count;
      fun(xd, left_mi_row - mi_row, AOMMIN(xd->n8_h, mi_step), *left_mi,
          fun_ctxt, num_planes);
    }
  }
}

#endif  // AV1_COMMON_OBMC_H_
