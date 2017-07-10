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

#include "av1/common/mvref_common.h"
#if CONFIG_WARPED_MOTION
#include "av1/common/warped_motion.h"
#endif  // CONFIG_WARPED_MOTION

static uint8_t add_ref_mv_candidate(
    const MODE_INFO *const candidate_mi, const MB_MODE_INFO *const candidate,
    const MV_REFERENCE_FRAME rf[2], uint8_t *refmv_count,
    CANDIDATE_MV *ref_mv_stack, const int use_hp, int len, int block, int col) {
  int index = 0, ref;
  int newmv_count = 0;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif

  if (rf[1] == NONE_FRAME) {
    // single reference frame
    for (ref = 0; ref < 2; ++ref) {
      if (candidate->ref_frame[ref] == rf[0]) {
        int_mv this_refmv = get_sub_block_mv(candidate_mi, ref, col, block);
        lower_mv_precision(&this_refmv.as_mv, use_hp);

        for (index = 0; index < *refmv_count; ++index)
          if (ref_mv_stack[index].this_mv.as_int == this_refmv.as_int) break;

        if (index < *refmv_count) ref_mv_stack[index].weight += 2 * len;

        // Add a new item to the list.
        if (index == *refmv_count) {
          ref_mv_stack[index].this_mv = this_refmv;
          ref_mv_stack[index].pred_diff[0] = av1_get_pred_diff_ctx(
              get_sub_block_pred_mv(candidate_mi, ref, col, block), this_refmv);
          ref_mv_stack[index].weight = 2 * len;
          ++(*refmv_count);

          if (candidate->mode == NEWMV) ++newmv_count;
        }

        if (candidate_mi->mbmi.sb_type < BLOCK_8X8 && block >= 0 &&
            !unify_bsize) {
          int alt_block = 3 - block;
          this_refmv = get_sub_block_mv(candidate_mi, ref, col, alt_block);
          lower_mv_precision(&this_refmv.as_mv, use_hp);

          for (index = 0; index < *refmv_count; ++index)
            if (ref_mv_stack[index].this_mv.as_int == this_refmv.as_int) break;

          if (index < *refmv_count) ref_mv_stack[index].weight += len;

          // Add a new item to the list.
          if (index == *refmv_count) {
            ref_mv_stack[index].this_mv = this_refmv;
            ref_mv_stack[index].pred_diff[0] = av1_get_pred_diff_ctx(
                get_sub_block_pred_mv(candidate_mi, ref, col, alt_block),
                this_refmv);
            ref_mv_stack[index].weight = len;
            ++(*refmv_count);

            if (candidate->mode == NEWMV) ++newmv_count;
          }
        }
      }
    }
  } else {
    // compound reference frame
    if (candidate->ref_frame[0] == rf[0] && candidate->ref_frame[1] == rf[1]) {
      int_mv this_refmv[2];

      for (ref = 0; ref < 2; ++ref) {
        this_refmv[ref] = get_sub_block_mv(candidate_mi, ref, col, block);
        lower_mv_precision(&this_refmv[ref].as_mv, use_hp);
      }

      for (index = 0; index < *refmv_count; ++index)
        if ((ref_mv_stack[index].this_mv.as_int == this_refmv[0].as_int) &&
            (ref_mv_stack[index].comp_mv.as_int == this_refmv[1].as_int))
          break;

      if (index < *refmv_count) ref_mv_stack[index].weight += 2 * len;

      // Add a new item to the list.
      if (index == *refmv_count) {
        ref_mv_stack[index].this_mv = this_refmv[0];
        ref_mv_stack[index].comp_mv = this_refmv[1];
        ref_mv_stack[index].pred_diff[0] = av1_get_pred_diff_ctx(
            get_sub_block_pred_mv(candidate_mi, 0, col, block), this_refmv[0]);
        ref_mv_stack[index].pred_diff[1] = av1_get_pred_diff_ctx(
            get_sub_block_pred_mv(candidate_mi, 1, col, block), this_refmv[1]);
        ref_mv_stack[index].weight = 2 * len;
        ++(*refmv_count);

#if CONFIG_EXT_INTER
        if (candidate->mode == NEW_NEWMV)
#else
        if (candidate->mode == NEWMV)
#endif  // CONFIG_EXT_INTER
          ++newmv_count;
      }

      if (candidate_mi->mbmi.sb_type < BLOCK_8X8 && block >= 0 &&
          !unify_bsize) {
        int alt_block = 3 - block;
        this_refmv[0] = get_sub_block_mv(candidate_mi, 0, col, alt_block);
        this_refmv[1] = get_sub_block_mv(candidate_mi, 1, col, alt_block);

        for (ref = 0; ref < 2; ++ref)
          lower_mv_precision(&this_refmv[ref].as_mv, use_hp);

        for (index = 0; index < *refmv_count; ++index)
          if (ref_mv_stack[index].this_mv.as_int == this_refmv[0].as_int &&
              ref_mv_stack[index].comp_mv.as_int == this_refmv[1].as_int)
            break;

        if (index < *refmv_count) ref_mv_stack[index].weight += len;

        // Add a new item to the list.
        if (index == *refmv_count) {
          ref_mv_stack[index].this_mv = this_refmv[0];
          ref_mv_stack[index].comp_mv = this_refmv[1];
          ref_mv_stack[index].pred_diff[0] = av1_get_pred_diff_ctx(
              get_sub_block_pred_mv(candidate_mi, 0, col, block),
              this_refmv[0]);
          ref_mv_stack[index].pred_diff[0] = av1_get_pred_diff_ctx(
              get_sub_block_pred_mv(candidate_mi, 1, col, block),
              this_refmv[1]);
          ref_mv_stack[index].weight = len;
          ++(*refmv_count);

#if CONFIG_EXT_INTER
          if (candidate->mode == NEW_NEWMV)
#else
          if (candidate->mode == NEWMV)
#endif  // CONFIG_EXT_INTER
            ++newmv_count;
        }
      }
    }
  }
  return newmv_count;
}

static uint8_t scan_row_mbmi(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             const int mi_row, const int mi_col, int block,
                             const MV_REFERENCE_FRAME rf[2], int row_offset,
                             CANDIDATE_MV *ref_mv_stack, uint8_t *refmv_count) {
  const TileInfo *const tile = &xd->tile;
  int i;
  uint8_t newmv_count = 0;
#if CONFIG_CB4X4
  const int bsize = xd->mi[0]->mbmi.sb_type;
  const int mi_offset =
      bsize < BLOCK_8X8 ? mi_size_wide[BLOCK_4X4] : mi_size_wide[BLOCK_8X8];
  // TODO(jingning): Revisit this part after cb4x4 is stable.
  if (bsize >= BLOCK_8X8) row_offset *= 2;
#else
  const int mi_offset = mi_size_wide[BLOCK_8X8];
#endif

  for (i = 0; i < xd->n8_w && *refmv_count < MAX_REF_MV_STACK_SIZE;) {
    POSITION mi_pos;
#if CONFIG_CB4X4
    const int use_step_16 = (xd->n8_w >= 16);
#else
    const int use_step_16 = (xd->n8_w >= 8);
#endif

    mi_pos.row = row_offset;
    mi_pos.col = i;
    if (is_inside(tile, mi_col, mi_row, cm->mi_rows, cm, &mi_pos)) {
      const MODE_INFO *const candidate_mi =
          xd->mi[mi_pos.row * xd->mi_stride + mi_pos.col];
      const MB_MODE_INFO *const candidate = &candidate_mi->mbmi;
      int len = AOMMIN(xd->n8_w, mi_size_wide[candidate->sb_type]);
      if (use_step_16) len = AOMMAX(mi_size_wide[BLOCK_16X16], len);
      newmv_count += add_ref_mv_candidate(
          candidate_mi, candidate, rf, refmv_count, ref_mv_stack,
          cm->allow_high_precision_mv, len, block, mi_pos.col);
      i += len;
    } else {
      if (use_step_16)
        i += (mi_offset << 1);
      else
        i += mi_offset;
    }
  }

  return newmv_count;
}

static uint8_t scan_col_mbmi(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             const int mi_row, const int mi_col, int block,
                             const MV_REFERENCE_FRAME rf[2], int col_offset,
                             CANDIDATE_MV *ref_mv_stack, uint8_t *refmv_count) {
  const TileInfo *const tile = &xd->tile;
  int i;
  uint8_t newmv_count = 0;
#if CONFIG_CB4X4
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const int mi_offset =
      (bsize < BLOCK_8X8) ? mi_size_high[BLOCK_4X4] : mi_size_high[BLOCK_8X8];
  if (bsize >= BLOCK_8X8) col_offset *= 2;
#else
  const int mi_offset = mi_size_wide[BLOCK_8X8];
#endif

  for (i = 0; i < xd->n8_h && *refmv_count < MAX_REF_MV_STACK_SIZE;) {
    POSITION mi_pos;
#if CONFIG_CB4X4
    const int use_step_16 = (xd->n8_h >= 16);
#else
    const int use_step_16 = (xd->n8_h >= 8);
#endif

    mi_pos.row = i;
    mi_pos.col = col_offset;
    if (is_inside(tile, mi_col, mi_row, cm->mi_rows, cm, &mi_pos)) {
      const MODE_INFO *const candidate_mi =
          xd->mi[mi_pos.row * xd->mi_stride + mi_pos.col];
      const MB_MODE_INFO *const candidate = &candidate_mi->mbmi;
      int len = AOMMIN(xd->n8_h, mi_size_high[candidate->sb_type]);
      if (use_step_16) len = AOMMAX(mi_size_high[BLOCK_16X16], len);
      newmv_count += add_ref_mv_candidate(
          candidate_mi, candidate, rf, refmv_count, ref_mv_stack,
          cm->allow_high_precision_mv, len, block, mi_pos.col);
      i += len;
    } else {
      if (use_step_16)
        i += (mi_offset << 1);
      else
        i += mi_offset;
    }
  }

  return newmv_count;
}

static uint8_t scan_blk_mbmi(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             const int mi_row, const int mi_col, int block,
                             const MV_REFERENCE_FRAME rf[2], int row_offset,
                             int col_offset, CANDIDATE_MV *ref_mv_stack,
                             uint8_t *refmv_count) {
  const TileInfo *const tile = &xd->tile;
  POSITION mi_pos;
  uint8_t newmv_count = 0;

  mi_pos.row = row_offset;
  mi_pos.col = col_offset;

  if (is_inside(tile, mi_col, mi_row, cm->mi_rows, cm, &mi_pos) &&
      *refmv_count < MAX_REF_MV_STACK_SIZE) {
    const MODE_INFO *const candidate_mi =
        xd->mi[mi_pos.row * xd->mi_stride + mi_pos.col];
    const MB_MODE_INFO *const candidate = &candidate_mi->mbmi;
    const int len = mi_size_wide[BLOCK_8X8];

    newmv_count += add_ref_mv_candidate(
        candidate_mi, candidate, rf, refmv_count, ref_mv_stack,
        cm->allow_high_precision_mv, len, block, mi_pos.col);
  }  // Analyze a single 8x8 block motion information.

  return newmv_count;
}

static int has_top_right(const MACROBLOCKD *xd, int mi_row, int mi_col,
                         int bs) {
  const int mask_row = mi_row & MAX_MIB_MASK;
  const int mask_col = mi_col & MAX_MIB_MASK;

  // In a split partition all apart from the bottom right has a top right
  int has_tr = !((mask_row & bs) && (mask_col & bs));

  // bs > 0 and bs is a power of 2
  assert(bs > 0 && !(bs & (bs - 1)));

  // For each 4x4 group of blocks, when the bottom right is decoded the blocks
  // to the right have not been decoded therefore the bottom right does
  // not have a top right
  while (bs < MAX_MIB_SIZE) {
    if (mask_col & bs) {
      if ((mask_col & (2 * bs)) && (mask_row & (2 * bs))) {
        has_tr = 0;
        break;
      }
    } else {
      break;
    }
    bs <<= 1;
  }

  // The left hand of two vertical rectangles always has a top right (as the
  // block above will have been decoded)
  if (xd->n8_w < xd->n8_h)
    if (!xd->is_sec_rect) has_tr = 1;

  // The bottom of two horizontal rectangles never has a top right (as the block
  // to the right won't have been decoded)
  if (xd->n8_w > xd->n8_h)
    if (xd->is_sec_rect) has_tr = 0;

#if CONFIG_EXT_PARTITION_TYPES
  // The bottom left square of a Vertical A does not have a top right as it is
  // decoded before the right hand rectangle of the partition
  if (xd->mi[0]->mbmi.partition == PARTITION_VERT_A)
    if ((mask_row & bs) && !(mask_col & bs)) has_tr = 0;
#endif  // CONFIG_EXT_PARTITION_TYPES

  return has_tr;
}

static int add_col_ref_mv(const AV1_COMMON *cm,
                          const MV_REF *prev_frame_mvs_base,
                          const MACROBLOCKD *xd, int mi_row, int mi_col,
                          MV_REFERENCE_FRAME ref_frame, int blk_row,
                          int blk_col, uint8_t *refmv_count,
                          CANDIDATE_MV *ref_mv_stack, int16_t *mode_context) {
  const MV_REF *prev_frame_mvs =
      prev_frame_mvs_base + blk_row * cm->mi_cols + blk_col;
  POSITION mi_pos;
  int ref, idx;
  int coll_blk_count = 0;
  const int weight_unit = mi_size_wide[BLOCK_8X8];

#if CONFIG_MV_COMPRESS
  mi_pos.row = (mi_row & 0x01) ? blk_row : blk_row + 1;
  mi_pos.col = (mi_col & 0x01) ? blk_col : blk_col + 1;
#else
  mi_pos.row = blk_row;
  mi_pos.col = blk_col;
#endif

  if (!is_inside(&xd->tile, mi_col, mi_row, cm->mi_rows, cm, &mi_pos))
    return coll_blk_count;
  for (ref = 0; ref < 2; ++ref) {
    if (prev_frame_mvs->ref_frame[ref] == ref_frame) {
      int_mv this_refmv = prev_frame_mvs->mv[ref];
      lower_mv_precision(&this_refmv.as_mv, cm->allow_high_precision_mv);

      if (abs(this_refmv.as_mv.row) >= 16 || abs(this_refmv.as_mv.col) >= 16)
        mode_context[ref_frame] |= (1 << ZEROMV_OFFSET);

      for (idx = 0; idx < *refmv_count; ++idx)
        if (this_refmv.as_int == ref_mv_stack[idx].this_mv.as_int) break;

      if (idx < *refmv_count) ref_mv_stack[idx].weight += 2 * weight_unit;

      if (idx == *refmv_count && *refmv_count < MAX_REF_MV_STACK_SIZE) {
        ref_mv_stack[idx].this_mv.as_int = this_refmv.as_int;
        ref_mv_stack[idx].pred_diff[0] =
            av1_get_pred_diff_ctx(prev_frame_mvs->pred_mv[ref], this_refmv);
        ref_mv_stack[idx].weight = 2 * weight_unit;
        ++(*refmv_count);
      }

      ++coll_blk_count;
    }
  }

  return coll_blk_count;
}

static void setup_ref_mv_list(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                              MV_REFERENCE_FRAME ref_frame,
                              uint8_t *refmv_count, CANDIDATE_MV *ref_mv_stack,
                              int_mv *mv_ref_list, int block, int mi_row,
                              int mi_col, int16_t *mode_context) {
  int idx, nearest_refmv_count = 0;
  uint8_t newmv_count = 0;
  CANDIDATE_MV tmp_mv;
  int len, nr_len;

#if CONFIG_MV_COMPRESS
  const MV_REF *const prev_frame_mvs_base =
      cm->use_prev_frame_mvs
          ? cm->prev_frame->mvs + (((mi_row >> 1) << 1) + 1) * cm->mi_cols +
                ((mi_col >> 1) << 1) + 1
          : NULL;
#else
  const MV_REF *const prev_frame_mvs_base =
      cm->use_prev_frame_mvs
          ? cm->prev_frame->mvs + mi_row * cm->mi_cols + mi_col
          : NULL;
#endif

  const int bs = AOMMAX(xd->n8_w, xd->n8_h);
  const int has_tr = has_top_right(xd, mi_row, mi_col, bs);
  MV_REFERENCE_FRAME rf[2];

  av1_set_ref_frame(rf, ref_frame);
  mode_context[ref_frame] = 0;
  *refmv_count = 0;

  // Scan the first above row mode info.
  newmv_count += scan_row_mbmi(cm, xd, mi_row, mi_col, block, rf, -1,
                               ref_mv_stack, refmv_count);
  // Scan the first left column mode info.
  newmv_count += scan_col_mbmi(cm, xd, mi_row, mi_col, block, rf, -1,
                               ref_mv_stack, refmv_count);

  // Check top-right boundary
  if (has_tr)
    newmv_count += scan_blk_mbmi(cm, xd, mi_row, mi_col, block, rf, -1,
                                 xd->n8_w, ref_mv_stack, refmv_count);

  nearest_refmv_count = *refmv_count;

  for (idx = 0; idx < nearest_refmv_count; ++idx)
    ref_mv_stack[idx].weight += REF_CAT_LEVEL;
#if CONFIG_TEMPMV_SIGNALING
  if (cm->use_prev_frame_mvs && rf[1] == NONE_FRAME) {
#else
  if (prev_frame_mvs_base && cm->show_frame && cm->last_show_frame &&
      rf[1] == NONE_FRAME) {
#endif
    int blk_row, blk_col;
    int coll_blk_count = 0;
#if CONFIG_CB4X4
    const int mi_step = (xd->n8_w == 1 || xd->n8_h == 1)
                            ? mi_size_wide[BLOCK_8X8]
                            : mi_size_wide[BLOCK_16X16];
#else
    const int mi_step = mi_size_wide[BLOCK_16X16];
#endif

#if CONFIG_TPL_MV
    int tpl_sample_pos[5][2] = { { -1, xd->n8_w },
                                 { 0, xd->n8_w },
                                 { xd->n8_h, xd->n8_w },
                                 { xd->n8_h, 0 },
                                 { xd->n8_h, -1 } };
    int i;
#endif

    for (blk_row = 0; blk_row < xd->n8_h; blk_row += mi_step) {
      for (blk_col = 0; blk_col < xd->n8_w; blk_col += mi_step) {
        coll_blk_count += add_col_ref_mv(
            cm, prev_frame_mvs_base, xd, mi_row, mi_col, ref_frame, blk_row,
            blk_col, refmv_count, ref_mv_stack, mode_context);
      }
    }

#if CONFIG_TPL_MV
    for (i = 0; i < 5; ++i) {
      blk_row = tpl_sample_pos[i][0];
      blk_col = tpl_sample_pos[i][1];
      coll_blk_count += add_col_ref_mv(cm, prev_frame_mvs_base, xd, mi_row,
                                       mi_col, ref_frame, blk_row, blk_col,
                                       refmv_count, ref_mv_stack, mode_context);
    }
#endif

    if (coll_blk_count == 0) mode_context[ref_frame] |= (1 << ZEROMV_OFFSET);
  } else {
    mode_context[ref_frame] |= (1 << ZEROMV_OFFSET);
  }

  // Scan the second outer area.
  scan_blk_mbmi(cm, xd, mi_row, mi_col, block, rf, -1, -1, ref_mv_stack,
                refmv_count);
  for (idx = 2; idx <= 3; ++idx) {
    scan_row_mbmi(cm, xd, mi_row, mi_col, block, rf, -idx, ref_mv_stack,
                  refmv_count);
    scan_col_mbmi(cm, xd, mi_row, mi_col, block, rf, -idx, ref_mv_stack,
                  refmv_count);
  }
  scan_col_mbmi(cm, xd, mi_row, mi_col, block, rf, -4, ref_mv_stack,
                refmv_count);

  switch (nearest_refmv_count) {
    case 0:
      mode_context[ref_frame] |= 0;
      if (*refmv_count >= 1) mode_context[ref_frame] |= 1;

      if (*refmv_count == 1)
        mode_context[ref_frame] |= (1 << REFMV_OFFSET);
      else if (*refmv_count >= 2)
        mode_context[ref_frame] |= (2 << REFMV_OFFSET);
      break;
    case 1:
      mode_context[ref_frame] |= (newmv_count > 0) ? 2 : 3;

      if (*refmv_count == 1)
        mode_context[ref_frame] |= (3 << REFMV_OFFSET);
      else if (*refmv_count >= 2)
        mode_context[ref_frame] |= (4 << REFMV_OFFSET);
      break;

    case 2:
    default:
      if (newmv_count >= 2)
        mode_context[ref_frame] |= 4;
      else if (newmv_count == 1)
        mode_context[ref_frame] |= 5;
      else
        mode_context[ref_frame] |= 6;

      mode_context[ref_frame] |= (5 << REFMV_OFFSET);
      break;
  }

  // Rank the likelihood and assign nearest and near mvs.
  len = nearest_refmv_count;
  while (len > 0) {
    nr_len = 0;
    for (idx = 1; idx < len; ++idx) {
      if (ref_mv_stack[idx - 1].weight < ref_mv_stack[idx].weight) {
        tmp_mv = ref_mv_stack[idx - 1];
        ref_mv_stack[idx - 1] = ref_mv_stack[idx];
        ref_mv_stack[idx] = tmp_mv;
        nr_len = idx;
      }
    }
    len = nr_len;
  }

  len = *refmv_count;
  while (len > nearest_refmv_count) {
    nr_len = nearest_refmv_count;
    for (idx = nearest_refmv_count + 1; idx < len; ++idx) {
      if (ref_mv_stack[idx - 1].weight < ref_mv_stack[idx].weight) {
        tmp_mv = ref_mv_stack[idx - 1];
        ref_mv_stack[idx - 1] = ref_mv_stack[idx];
        ref_mv_stack[idx] = tmp_mv;
        nr_len = idx;
      }
    }
    len = nr_len;
  }

  if (rf[1] > NONE_FRAME) {
    for (idx = 0; idx < *refmv_count; ++idx) {
      clamp_mv_ref(&ref_mv_stack[idx].this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                   xd->n8_h << MI_SIZE_LOG2, xd);
      clamp_mv_ref(&ref_mv_stack[idx].comp_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                   xd->n8_h << MI_SIZE_LOG2, xd);
    }
  } else {
    for (idx = 0; idx < AOMMIN(MAX_MV_REF_CANDIDATES, *refmv_count); ++idx) {
      mv_ref_list[idx].as_int = ref_mv_stack[idx].this_mv.as_int;
      clamp_mv_ref(&mv_ref_list[idx].as_mv, xd->n8_w << MI_SIZE_LOG2,
                   xd->n8_h << MI_SIZE_LOG2, xd);
    }
  }
}

// This function searches the neighbourhood of a given MB/SB
// to try and find candidate reference vectors.
static void find_mv_refs_idx(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             MODE_INFO *mi, MV_REFERENCE_FRAME ref_frame,
                             int_mv *mv_ref_list, int block, int mi_row,
                             int mi_col, find_mv_refs_sync sync,
                             void *const data, int16_t *mode_context,
                             int_mv zeromv) {
  const int *ref_sign_bias = cm->ref_frame_sign_bias;
  int i, refmv_count = 0;
  int different_ref_found = 0;
  int context_counter = 0;
#if CONFIG_MV_COMPRESS
  const TileInfo *const tile_ = &xd->tile;
  int mi_row_end = tile_->mi_row_end;
  int mi_col_end = tile_->mi_col_end;
  const MV_REF *const prev_frame_mvs =
      cm->use_prev_frame_mvs
          ? cm->prev_frame->mvs +
                AOMMIN(((mi_row >> 1) << 1) + 1 + (((xd->n8_h - 1) >> 1) << 1),
                       mi_row_end - 1) *
                    cm->mi_cols +
                AOMMIN(((mi_col >> 1) << 1) + 1 + (((xd->n8_w - 1) >> 1) << 1),
                       mi_col_end - 1)
          : NULL;
#else
  const MV_REF *const prev_frame_mvs =
      cm->use_prev_frame_mvs
          ? cm->prev_frame->mvs + mi_row * cm->mi_cols + mi_col
          : NULL;
#endif
#if CONFIG_INTRABC
  assert(IMPLIES(ref_frame == INTRA_FRAME, cm->use_prev_frame_mvs == 0));
#endif
  const TileInfo *const tile = &xd->tile;
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;
  const int bw = block_size_wide[AOMMAX(bsize, BLOCK_8X8)];
  const int bh = block_size_high[AOMMAX(bsize, BLOCK_8X8)];
  POSITION mv_ref_search[MVREF_NEIGHBOURS];
  const int num_8x8_blocks_wide = num_8x8_blocks_wide_lookup[bsize];
  const int num_8x8_blocks_high = num_8x8_blocks_high_lookup[bsize];
  mv_ref_search[0].row = num_8x8_blocks_high - 1;
  mv_ref_search[0].col = -1;
  mv_ref_search[1].row = -1;
  mv_ref_search[1].col = num_8x8_blocks_wide - 1;
  mv_ref_search[2].row = -1;
  mv_ref_search[2].col = (num_8x8_blocks_wide - 1) >> 1;
  mv_ref_search[3].row = (num_8x8_blocks_high - 1) >> 1;
  mv_ref_search[3].col = -1;
  mv_ref_search[4].row = -1;
  mv_ref_search[4].col = -1;
#if CONFIG_EXT_PARTITION_TYPES
  if (num_8x8_blocks_wide == num_8x8_blocks_high) {
    mv_ref_search[5].row = -1;
    mv_ref_search[5].col = 0;
    mv_ref_search[6].row = 0;
    mv_ref_search[6].col = -1;
  } else {
    mv_ref_search[5].row = -1;
    mv_ref_search[5].col = num_8x8_blocks_wide;
    mv_ref_search[6].row = num_8x8_blocks_high;
    mv_ref_search[6].col = -1;
  }
#else
  mv_ref_search[5].row = -1;
  mv_ref_search[5].col = num_8x8_blocks_wide;
  mv_ref_search[6].row = num_8x8_blocks_high;
  mv_ref_search[6].col = -1;
#endif  // CONFIG_EXT_PARTITION_TYPES
  mv_ref_search[7].row = -1;
  mv_ref_search[7].col = -3;
  mv_ref_search[8].row = num_8x8_blocks_high - 1;
  mv_ref_search[8].col = -3;

#if CONFIG_CB4X4
  for (i = 0; i < MVREF_NEIGHBOURS; ++i) {
    mv_ref_search[i].row *= 2;
    mv_ref_search[i].col *= 2;
  }
#endif  // CONFIG_CB4X4

  // The nearest 2 blocks are treated differently
  // if the size < 8x8 we get the mv from the bmi substructure,
  // and we also need to keep a mode count.
  for (i = 0; i < 2; ++i) {
    const POSITION *const mv_ref = &mv_ref_search[i];
    if (is_inside(tile, mi_col, mi_row, cm->mi_rows, cm, mv_ref)) {
      const MODE_INFO *const candidate_mi =
          xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride];
      const MB_MODE_INFO *const candidate = &candidate_mi->mbmi;
      // Keep counts for entropy encoding.
      context_counter += mode_2_counter[candidate->mode];
      different_ref_found = 1;

      if (candidate->ref_frame[0] == ref_frame)
        ADD_MV_REF_LIST(get_sub_block_mv(candidate_mi, 0, mv_ref->col, block),
                        refmv_count, mv_ref_list, bw, bh, xd, Done);
      else if (candidate->ref_frame[1] == ref_frame)
        ADD_MV_REF_LIST(get_sub_block_mv(candidate_mi, 1, mv_ref->col, block),
                        refmv_count, mv_ref_list, bw, bh, xd, Done);
    }
  }

  // Check the rest of the neighbors in much the same way
  // as before except we don't need to keep track of sub blocks or
  // mode counts.
  for (; i < MVREF_NEIGHBOURS; ++i) {
    const POSITION *const mv_ref = &mv_ref_search[i];
    if (is_inside(tile, mi_col, mi_row, cm->mi_rows, cm, mv_ref)) {
      const MB_MODE_INFO *const candidate =
          !xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride]
              ? NULL
              : &xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride]->mbmi;
      if (candidate == NULL) continue;
      if ((mi_row % MAX_MIB_SIZE) + mv_ref->row >= MAX_MIB_SIZE ||
          (mi_col % MAX_MIB_SIZE) + mv_ref->col >= MAX_MIB_SIZE)
        continue;
      different_ref_found = 1;

      if (candidate->ref_frame[0] == ref_frame)
        ADD_MV_REF_LIST(candidate->mv[0], refmv_count, mv_ref_list, bw, bh, xd,
                        Done);
      else if (candidate->ref_frame[1] == ref_frame)
        ADD_MV_REF_LIST(candidate->mv[1], refmv_count, mv_ref_list, bw, bh, xd,
                        Done);
    }
  }

// TODO(hkuang): Remove this sync after fixing pthread_cond_broadcast
// on windows platform. The sync here is unncessary if use_perv_frame_mvs
// is 0. But after removing it, there will be hang in the unit test on windows
// due to several threads waiting for a thread's signal.
#if defined(_WIN32) && !HAVE_PTHREAD_H
  if (cm->frame_parallel_decode && sync != NULL) {
    sync(data, mi_row);
  }
#endif

  // Check the last frame's mode and mv info.
  if (cm->use_prev_frame_mvs) {
    // Synchronize here for frame parallel decode if sync function is provided.
    if (cm->frame_parallel_decode && sync != NULL) {
      sync(data, mi_row);
    }

    if (prev_frame_mvs->ref_frame[0] == ref_frame) {
      ADD_MV_REF_LIST(prev_frame_mvs->mv[0], refmv_count, mv_ref_list, bw, bh,
                      xd, Done);
    } else if (prev_frame_mvs->ref_frame[1] == ref_frame) {
      ADD_MV_REF_LIST(prev_frame_mvs->mv[1], refmv_count, mv_ref_list, bw, bh,
                      xd, Done);
    }
  }

  // Since we couldn't find 2 mvs from the same reference frame
  // go back through the neighbors and find motion vectors from
  // different reference frames.
  if (different_ref_found) {
    for (i = 0; i < MVREF_NEIGHBOURS; ++i) {
      const POSITION *mv_ref = &mv_ref_search[i];
      if (is_inside(tile, mi_col, mi_row, cm->mi_rows, cm, mv_ref)) {
        const MB_MODE_INFO *const candidate =
            !xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride]
                ? NULL
                : &xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride]->mbmi;
        if (candidate == NULL) continue;
        if ((mi_row % MAX_MIB_SIZE) + mv_ref->row >= MAX_MIB_SIZE ||
            (mi_col % MAX_MIB_SIZE) + mv_ref->col >= MAX_MIB_SIZE)
          continue;

        // If the candidate is INTRA we don't want to consider its mv.
        IF_DIFF_REF_FRAME_ADD_MV(candidate, ref_frame, ref_sign_bias,
                                 refmv_count, mv_ref_list, bw, bh, xd, Done);
      }
    }
  }

  // Since we still don't have a candidate we'll try the last frame.
  if (cm->use_prev_frame_mvs) {
    if (prev_frame_mvs->ref_frame[0] != ref_frame &&
        prev_frame_mvs->ref_frame[0] > INTRA_FRAME) {
      int_mv mv = prev_frame_mvs->mv[0];
      if (ref_sign_bias[prev_frame_mvs->ref_frame[0]] !=
          ref_sign_bias[ref_frame]) {
        mv.as_mv.row *= -1;
        mv.as_mv.col *= -1;
      }
      ADD_MV_REF_LIST(mv, refmv_count, mv_ref_list, bw, bh, xd, Done);
    }

    if (prev_frame_mvs->ref_frame[1] > INTRA_FRAME &&
        prev_frame_mvs->ref_frame[1] != ref_frame) {
      int_mv mv = prev_frame_mvs->mv[1];
      if (ref_sign_bias[prev_frame_mvs->ref_frame[1]] !=
          ref_sign_bias[ref_frame]) {
        mv.as_mv.row *= -1;
        mv.as_mv.col *= -1;
      }
      ADD_MV_REF_LIST(mv, refmv_count, mv_ref_list, bw, bh, xd, Done);
    }
  }

Done:
  if (mode_context)
    mode_context[ref_frame] = counter_to_context[context_counter];
  for (i = refmv_count; i < MAX_MV_REF_CANDIDATES; ++i)
    mv_ref_list[i].as_int = zeromv.as_int;
}

#if CONFIG_EXT_INTER
// This function keeps a mode count for a given MB/SB
void av1_update_mv_context(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                           MODE_INFO *mi, MV_REFERENCE_FRAME ref_frame,
                           int_mv *mv_ref_list, int block, int mi_row,
                           int mi_col, int16_t *mode_context) {
  int i, refmv_count = 0;
  int context_counter = 0;
  const int bw = block_size_wide[mi->mbmi.sb_type];
  const int bh = block_size_high[mi->mbmi.sb_type];
  const TileInfo *const tile = &xd->tile;
  POSITION mv_ref_search[2];
  const int num_8x8_blocks_wide = mi_size_wide[mi->mbmi.sb_type];
  const int num_8x8_blocks_high = mi_size_high[mi->mbmi.sb_type];

  mv_ref_search[0].row = num_8x8_blocks_high - 1;
  mv_ref_search[0].col = -1;
  mv_ref_search[1].row = -1;
  mv_ref_search[1].col = num_8x8_blocks_wide - 1;

  // Blank the reference vector list
  memset(mv_ref_list, 0, sizeof(*mv_ref_list) * MAX_MV_REF_CANDIDATES);

  // The nearest 2 blocks are examined only.
  // If the size < 8x8, we get the mv from the bmi substructure;
  for (i = 0; i < 2; ++i) {
    const POSITION *const mv_ref = &mv_ref_search[i];
    if (is_inside(tile, mi_col, mi_row, cm->mi_rows, cm, mv_ref)) {
      const MODE_INFO *const candidate_mi =
          xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride];
      const MB_MODE_INFO *const candidate = &candidate_mi->mbmi;

      // Keep counts for entropy encoding.
      context_counter += mode_2_counter[candidate->mode];

      if (candidate->ref_frame[0] == ref_frame) {
        ADD_MV_REF_LIST(get_sub_block_mv(candidate_mi, 0, mv_ref->col, block),
                        refmv_count, mv_ref_list, bw, bh, xd, Done);
      } else if (candidate->ref_frame[1] == ref_frame) {
        ADD_MV_REF_LIST(get_sub_block_mv(candidate_mi, 1, mv_ref->col, block),
                        refmv_count, mv_ref_list, bw, bh, xd, Done);
      }
    }
  }

Done:

  if (mode_context)
    mode_context[ref_frame] = counter_to_context[context_counter];
}
#endif  // CONFIG_EXT_INTER

void av1_find_mv_refs(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                      MODE_INFO *mi, MV_REFERENCE_FRAME ref_frame,
                      uint8_t *ref_mv_count, CANDIDATE_MV *ref_mv_stack,
#if CONFIG_EXT_INTER
                      int16_t *compound_mode_context,
#endif  // CONFIG_EXT_INTER
                      int_mv *mv_ref_list, int mi_row, int mi_col,
                      find_mv_refs_sync sync, void *const data,
                      int16_t *mode_context) {
  int_mv zeromv[2];
#if CONFIG_GLOBAL_MOTION
  BLOCK_SIZE bsize = mi->mbmi.sb_type;
#endif  // CONFIG_GLOBAL_MOTION
  int idx, all_zero = 1;
#if CONFIG_GLOBAL_MOTION
  MV_REFERENCE_FRAME rf[2];
#endif  // CONFIG_GLOBAL_MOTION

#if CONFIG_EXT_INTER
  av1_update_mv_context(cm, xd, mi, ref_frame, mv_ref_list, -1, mi_row, mi_col,
                        compound_mode_context);
#endif  // CONFIG_EXT_INTER

#if CONFIG_GLOBAL_MOTION
  if (!CONFIG_INTRABC || ref_frame != INTRA_FRAME) {
    av1_set_ref_frame(rf, ref_frame);
    zeromv[0].as_int = gm_get_motion_vector(&cm->global_motion[rf[0]],
                                            cm->allow_high_precision_mv, bsize,
                                            mi_col, mi_row, 0)
                           .as_int;
    zeromv[1].as_int = (rf[1] != NONE_FRAME)
                           ? gm_get_motion_vector(&cm->global_motion[rf[1]],
                                                  cm->allow_high_precision_mv,
                                                  bsize, mi_col, mi_row, 0)
                                 .as_int
                           : 0;
  } else {
    zeromv[0].as_int = zeromv[1].as_int = 0;
  }
#else
  zeromv[0].as_int = zeromv[1].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION

  if (ref_frame <= ALTREF_FRAME)
    find_mv_refs_idx(cm, xd, mi, ref_frame, mv_ref_list, -1, mi_row, mi_col,
                     sync, data, mode_context, zeromv[0]);

  setup_ref_mv_list(cm, xd, ref_frame, ref_mv_count, ref_mv_stack, mv_ref_list,
                    -1, mi_row, mi_col, mode_context);
  /* Note: If global motion is enabled, then we want to set the ALL_ZERO flag
     iff all of the MVs we could generate with NEARMV/NEARESTMV are equivalent
     to the global motion vector.
     Note: For the following to work properly, the encoder can't throw away
     any global motion models after calling this function, even if they are
     unused. Instead we rely on the recode loop: If any non-IDENTITY model
     is unused, the whole frame will be re-encoded without it.
     The problem is that, otherwise, we can end up in the following situation:
     * Encoder has a global motion model with nonzero translational part,
       and all candidate MVs are zero. So the ALL_ZERO flag is unset.
     * Encoder throws away global motion because it is never used.
     * Decoder sees that there is no global motion and all candidate MVs are
       zero, so sets the ALL_ZERO flag.
     * This leads to an encode/decode mismatch.
  */
  if (*ref_mv_count >= 2) {
    for (idx = 0; idx < AOMMIN(3, *ref_mv_count); ++idx) {
      if (ref_mv_stack[idx].this_mv.as_int != zeromv[0].as_int) all_zero = 0;
      if (ref_frame > ALTREF_FRAME)
        if (ref_mv_stack[idx].comp_mv.as_int != zeromv[1].as_int) all_zero = 0;
    }
  } else if (ref_frame <= ALTREF_FRAME) {
    for (idx = 0; idx < MAX_MV_REF_CANDIDATES; ++idx)
      if (mv_ref_list[idx].as_int != zeromv[0].as_int) all_zero = 0;
  }

  if (all_zero) mode_context[ref_frame] |= (1 << ALL_ZERO_FLAG_OFFSET);
}

void av1_find_best_ref_mvs(int allow_hp, int_mv *mvlist, int_mv *nearest_mv,
                           int_mv *near_mv) {
  int i;
  // Make sure all the candidates are properly clamped etc
  for (i = 0; i < MAX_MV_REF_CANDIDATES; ++i) {
    lower_mv_precision(&mvlist[i].as_mv, allow_hp);
  }
  *nearest_mv = mvlist[0];
  *near_mv = mvlist[1];
}

void av1_append_sub8x8_mvs_for_idx(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                   int block, int ref, int mi_row, int mi_col,
                                   CANDIDATE_MV *ref_mv_stack,
                                   uint8_t *ref_mv_count,
#if CONFIG_EXT_INTER
                                   int_mv *mv_list,
#endif  // CONFIG_EXT_INTER
                                   int_mv *nearest_mv, int_mv *near_mv) {
#if !CONFIG_EXT_INTER
  int_mv mv_list[MAX_MV_REF_CANDIDATES];
#endif  // !CONFIG_EXT_INTER
  MODE_INFO *const mi = xd->mi[0];
  b_mode_info *bmi = mi->bmi;
  int n;
  int_mv zeromv;
  CANDIDATE_MV tmp_mv;
  uint8_t idx;
  uint8_t above_count = 0, left_count = 0;
  MV_REFERENCE_FRAME rf[2] = { mi->mbmi.ref_frame[ref], NONE_FRAME };
  *ref_mv_count = 0;

  assert(MAX_MV_REF_CANDIDATES == 2);

#if CONFIG_GLOBAL_MOTION
  zeromv.as_int = gm_get_motion_vector(&cm->global_motion[rf[0]],
                                       cm->allow_high_precision_mv,
                                       mi->mbmi.sb_type, mi_col, mi_row, block)
                      .as_int;
#else
  zeromv.as_int = 0;
#endif
  find_mv_refs_idx(cm, xd, mi, mi->mbmi.ref_frame[ref], mv_list, block, mi_row,
                   mi_col, NULL, NULL, NULL, zeromv);

  scan_blk_mbmi(cm, xd, mi_row, mi_col, block, rf, -1, 0, ref_mv_stack,
                ref_mv_count);
  above_count = *ref_mv_count;

  scan_blk_mbmi(cm, xd, mi_row, mi_col, block, rf, 0, -1, ref_mv_stack,
                ref_mv_count);
  left_count = *ref_mv_count - above_count;

  if (above_count > 1 && left_count > 0) {
    tmp_mv = ref_mv_stack[1];
    ref_mv_stack[1] = ref_mv_stack[above_count];
    ref_mv_stack[above_count] = tmp_mv;
  }

  for (idx = 0; idx < *ref_mv_count; ++idx)
    clamp_mv_ref(&ref_mv_stack[idx].this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                 xd->n8_h << MI_SIZE_LOG2, xd);

  for (idx = 0; idx < AOMMIN(MAX_MV_REF_CANDIDATES, *ref_mv_count); ++idx)
    mv_list[idx].as_int = ref_mv_stack[idx].this_mv.as_int;

  near_mv->as_int = 0;
  switch (block) {
    case 0:
      nearest_mv->as_int = mv_list[0].as_int;
      near_mv->as_int = mv_list[1].as_int;
      break;
    case 1:
    case 2:
      nearest_mv->as_int = bmi[0].as_mv[ref].as_int;
      for (n = 0; n < MAX_MV_REF_CANDIDATES; ++n)
        if (nearest_mv->as_int != mv_list[n].as_int) {
          near_mv->as_int = mv_list[n].as_int;
          break;
        }
      break;
    case 3: {
      int_mv candidates[2 + MAX_MV_REF_CANDIDATES];
      candidates[0] = bmi[1].as_mv[ref];
      candidates[1] = bmi[0].as_mv[ref];
      candidates[2] = mv_list[0];
      candidates[3] = mv_list[1];

      nearest_mv->as_int = bmi[2].as_mv[ref].as_int;
      for (n = 0; n < 2 + MAX_MV_REF_CANDIDATES; ++n)
        if (nearest_mv->as_int != candidates[n].as_int) {
          near_mv->as_int = candidates[n].as_int;
          break;
        }
      break;
    }
    default: assert(0 && "Invalid block index.");
  }
}

#if CONFIG_WARPED_MOTION
#if WARPED_MOTION_SORT_SAMPLES
static INLINE void record_samples(MB_MODE_INFO *mbmi, int *pts, int *pts_inref,
                                  int *pts_mv, int global_offset_r,
                                  int global_offset_c, int row_offset,
                                  int sign_r, int col_offset, int sign_c) {
  int bw = block_size_wide[mbmi->sb_type];
  int bh = block_size_high[mbmi->sb_type];
  int cr_offset = row_offset * MI_SIZE + sign_r * AOMMAX(bh, MI_SIZE) / 2 - 1;
  int cc_offset = col_offset * MI_SIZE + sign_c * AOMMAX(bw, MI_SIZE) / 2 - 1;
  int x = cc_offset + global_offset_c;
  int y = cr_offset + global_offset_r;

  pts[0] = (x * 8);
  pts[1] = (y * 8);
  pts_inref[0] = (x * 8) + mbmi->mv[0].as_mv.col;
  pts_inref[1] = (y * 8) + mbmi->mv[0].as_mv.row;
  pts_mv[0] = mbmi->mv[0].as_mv.col;
  pts_mv[1] = mbmi->mv[0].as_mv.row;
}

// Only sort pts and pts_inref, and pts_mv is not sorted.
#define TRIM_THR 16
int sortSamples(int *pts_mv, MV *mv, int *pts, int *pts_inref, int len) {
  int pts_mvd[SAMPLES_ARRAY_SIZE] = { 0 };
  int i, j, k;
  int ret = len;

  for (i = 0; i < len; ++i)
    pts_mvd[i] =
        abs(pts_mv[2 * i] - mv->col) + abs(pts_mv[2 * i + 1] - mv->row);

  for (i = 1; i <= len - 1; ++i) {
    for (j = 0; j < i; ++j) {
      if (pts_mvd[j] > pts_mvd[i]) {
        int temp, tempi, tempj, ptempi, ptempj;

        temp = pts_mvd[i];
        tempi = pts[2 * i];
        tempj = pts[2 * i + 1];
        ptempi = pts_inref[2 * i];
        ptempj = pts_inref[2 * i + 1];

        for (k = i; k > j; k--) {
          pts_mvd[k] = pts_mvd[k - 1];
          pts[2 * k] = pts[2 * (k - 1)];
          pts[2 * k + 1] = pts[2 * (k - 1) + 1];
          pts_inref[2 * k] = pts_inref[2 * (k - 1)];
          pts_inref[2 * k + 1] = pts_inref[2 * (k - 1) + 1];
        }

        pts_mvd[j] = temp;
        pts[2 * j] = tempi;
        pts[2 * j + 1] = tempj;
        pts_inref[2 * j] = ptempi;
        pts_inref[2 * j + 1] = ptempj;
        break;
      }
    }
  }

  for (i = len - 1; i >= 1; i--) {
    int low = (i == 1) ? 1 : AOMMAX((pts_mvd[i - 1] - pts_mvd[0]) / (i - 1), 1);

    if ((pts_mvd[i] - pts_mvd[i - 1]) >= TRIM_THR * low) ret = i;
  }

  if (ret > LEAST_SQUARES_SAMPLES_MAX) ret = LEAST_SQUARES_SAMPLES_MAX;
  return ret;
}

// Note: Samples returned are at 1/8-pel precision
int findSamples(const AV1_COMMON *cm, MACROBLOCKD *xd, int mi_row, int mi_col,
                int *pts, int *pts_inref, int *pts_mv) {
  MB_MODE_INFO *const mbmi0 = &(xd->mi[0]->mbmi);
  int ref_frame = mbmi0->ref_frame[0];
  int up_available = xd->up_available;
  int left_available = xd->left_available;
  int i, mi_step = 1, np = 0, n, j, k;
  int global_offset_c = mi_col * MI_SIZE;
  int global_offset_r = mi_row * MI_SIZE;

  const TileInfo *const tile = &xd->tile;
  // Search nb range in the unit of mi
  int bs =
      (AOMMAX(xd->n8_w, xd->n8_h) > 1) ? (AOMMAX(xd->n8_w, xd->n8_h) >> 1) : 1;
  int marked[16 * 32];  // max array size for 128x128
  int do_tl = 1;
  int do_tr = 1;

  // scan the above rows
  if (up_available) {
    for (n = 0; n < bs; n++) {
      int mi_row_offset = -1 * (n + 1);

      if (!n) {
        MODE_INFO *mi = xd->mi[mi_row_offset * xd->mi_stride];
        MB_MODE_INFO *mbmi = &mi->mbmi;
        uint8_t n8_w = mi_size_wide[mbmi->sb_type];

        // Handle "current block width <= above block width" case.
        if (xd->n8_w <= n8_w) {
          int col_offset = -mi_col % n8_w;

          if (col_offset < 0) do_tl = 0;
          if (col_offset + n8_w > xd->n8_w) do_tr = 0;

          if (mbmi->ref_frame[0] == ref_frame &&
              mbmi->ref_frame[1] == NONE_FRAME) {
            record_samples(mbmi, pts, pts_inref, pts_mv, global_offset_r,
                           global_offset_c, 0, -1, col_offset, 1);
            pts += 2;
            pts_inref += 2;
            pts_mv += 2;
            np++;
          }
          break;
        }
      }

      // Handle "current block width > above block width" case.
      if (!n) memset(marked, 0, bs * xd->n8_w * sizeof(*marked));

      for (i = 0; i < AOMMIN(xd->n8_w, cm->mi_cols - mi_col); i += mi_step) {
        int mi_col_offset = i;
        MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
        MB_MODE_INFO *mbmi = &mi->mbmi;
        uint8_t n8_w = mi_size_wide[mbmi->sb_type];
        uint8_t n8_h = mi_size_high[mbmi->sb_type];

        mi_step = AOMMIN(xd->n8_w, n8_w);

        // Processed already
        if (marked[n * xd->n8_w + i]) continue;

        for (j = 0; j < AOMMIN(bs, n8_h); j++)
          for (k = 0; k < AOMMIN(xd->n8_w, n8_w); k++)
            marked[(n + j) * xd->n8_w + i + k] = 1;

        if (mbmi->ref_frame[0] == ref_frame &&
            mbmi->ref_frame[1] == NONE_FRAME) {
          record_samples(mbmi, pts, pts_inref, pts_mv, global_offset_r,
                         global_offset_c, -n, -1, i, 1);
          pts += 2;
          pts_inref += 2;
          pts_mv += 2;
          np++;
        }
      }
    }
  }
  assert(2 * np <= SAMPLES_ARRAY_SIZE);

  // scan the left columns
  if (left_available) {
    for (n = 0; n < bs; n++) {
      int mi_col_offset = -1 * (n + 1);

      if (!n) {
        MODE_INFO *mi = xd->mi[mi_col_offset];
        MB_MODE_INFO *mbmi = &mi->mbmi;
        uint8_t n8_h = mi_size_high[mbmi->sb_type];

        // Handle "current block height <= above block height" case.
        if (xd->n8_h <= n8_h) {
          int row_offset = -mi_row % n8_h;

          if (row_offset < 0) do_tl = 0;

          if (mbmi->ref_frame[0] == ref_frame &&
              mbmi->ref_frame[1] == NONE_FRAME) {
            record_samples(mbmi, pts, pts_inref, pts_mv, global_offset_r,
                           global_offset_c, row_offset, 1, 0, -1);
            pts += 2;
            pts_inref += 2;
            pts_mv += 2;
            np++;
          }
          break;
        }
      }

      // Handle "current block height > above block height" case.
      if (!n) memset(marked, 0, bs * xd->n8_h * sizeof(*marked));

      for (i = 0; i < AOMMIN(xd->n8_h, cm->mi_rows - mi_row); i += mi_step) {
        int mi_row_offset = i;
        MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
        MB_MODE_INFO *mbmi = &mi->mbmi;
        uint8_t n8_w = mi_size_wide[mbmi->sb_type];
        uint8_t n8_h = mi_size_high[mbmi->sb_type];

        mi_step = AOMMIN(xd->n8_h, n8_h);

        // Processed already
        if (marked[n * xd->n8_h + i]) continue;

        for (j = 0; j < AOMMIN(bs, n8_w); j++)
          for (k = 0; k < AOMMIN(xd->n8_h, n8_h); k++)
            marked[(n + j) * xd->n8_h + i + k] = 1;

        if (mbmi->ref_frame[0] == ref_frame &&
            mbmi->ref_frame[1] == NONE_FRAME) {
          record_samples(mbmi, pts, pts_inref, pts_mv, global_offset_r,
                         global_offset_c, i, 1, -n, -1);
          pts += 2;
          pts_inref += 2;
          pts_mv += 2;
          np++;
        }
      }
    }
  }
  assert(2 * np <= SAMPLES_ARRAY_SIZE);

  // Top-left block
  if (do_tl && left_available && up_available) {
    int mi_row_offset = -1;
    int mi_col_offset = -1;

    MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
    MB_MODE_INFO *mbmi = &mi->mbmi;

    if (mbmi->ref_frame[0] == ref_frame && mbmi->ref_frame[1] == NONE_FRAME) {
      record_samples(mbmi, pts, pts_inref, pts_mv, global_offset_r,
                     global_offset_c, 0, -1, 0, -1);
      pts += 2;
      pts_inref += 2;
      pts_mv += 2;
      np++;
    }
  }
  assert(2 * np <= SAMPLES_ARRAY_SIZE);

  // Top-right block
  if (do_tr && has_top_right(xd, mi_row, mi_col, AOMMAX(xd->n8_w, xd->n8_h))) {
    POSITION trb_pos = { -1, xd->n8_w };

    if (is_inside(tile, mi_col, mi_row, cm->mi_rows, cm, &trb_pos)) {
      int mi_row_offset = -1;
      int mi_col_offset = xd->n8_w;

      MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
      MB_MODE_INFO *mbmi = &mi->mbmi;

      if (mbmi->ref_frame[0] == ref_frame && mbmi->ref_frame[1] == NONE_FRAME) {
        record_samples(mbmi, pts, pts_inref, pts_mv, global_offset_r,
                       global_offset_c, 0, -1, xd->n8_w, 1);
        np++;
      }
    }
  }
  assert(2 * np <= SAMPLES_ARRAY_SIZE);

  return np;
}
#else
void calc_projection_samples(MB_MODE_INFO *const mbmi, int x, int y,
                             int *pts_inref) {
  pts_inref[0] = (x * 8) + mbmi->mv[0].as_mv.col;
  pts_inref[1] = (y * 8) + mbmi->mv[0].as_mv.row;
}

// Note: Samples returned are at 1/8-pel precision
int findSamples(const AV1_COMMON *cm, MACROBLOCKD *xd, int mi_row, int mi_col,
                int *pts, int *pts_inref) {
  MB_MODE_INFO *const mbmi0 = &(xd->mi[0]->mbmi);
  int ref_frame = mbmi0->ref_frame[0];
  int up_available = xd->up_available;
  int left_available = xd->left_available;
  int i, mi_step, np = 0;
  int global_offset_c = mi_col * MI_SIZE;
  int global_offset_r = mi_row * MI_SIZE;

  // scan the above row
  if (up_available) {
    for (i = 0; i < AOMMIN(xd->n8_w, cm->mi_cols - mi_col); i += mi_step) {
      int mi_row_offset = -1;
      int mi_col_offset = i;

      MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
      MB_MODE_INFO *mbmi = &mi->mbmi;

      mi_step = AOMMIN(xd->n8_w, mi_size_wide[mbmi->sb_type]);

      if (mbmi->ref_frame[0] == ref_frame && mbmi->ref_frame[1] == NONE_FRAME) {
        int bw = block_size_wide[mbmi->sb_type];
        int bh = block_size_high[mbmi->sb_type];
        int cr_offset = -AOMMAX(bh, MI_SIZE) / 2 - 1;
        int cc_offset = i * MI_SIZE + AOMMAX(bw, MI_SIZE) / 2 - 1;
        int x = cc_offset + global_offset_c;
        int y = cr_offset + global_offset_r;

        pts[0] = (x * 8);
        pts[1] = (y * 8);
        calc_projection_samples(mbmi, x, y, pts_inref);
        pts += 2;
        pts_inref += 2;
        np++;
        if (np >= LEAST_SQUARES_SAMPLES_MAX) return LEAST_SQUARES_SAMPLES_MAX;
      }
    }
  }
  assert(2 * np <= SAMPLES_ARRAY_SIZE);

  // scan the left column
  if (left_available) {
    for (i = 0; i < AOMMIN(xd->n8_h, cm->mi_rows - mi_row); i += mi_step) {
      int mi_row_offset = i;
      int mi_col_offset = -1;

      MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
      MB_MODE_INFO *mbmi = &mi->mbmi;

      mi_step = AOMMIN(xd->n8_h, mi_size_high[mbmi->sb_type]);

      if (mbmi->ref_frame[0] == ref_frame && mbmi->ref_frame[1] == NONE_FRAME) {
        int bw = block_size_wide[mbmi->sb_type];
        int bh = block_size_high[mbmi->sb_type];
        int cr_offset = i * MI_SIZE + AOMMAX(bh, MI_SIZE) / 2 - 1;
        int cc_offset = -AOMMAX(bw, MI_SIZE) / 2 - 1;
        int x = cc_offset + global_offset_c;
        int y = cr_offset + global_offset_r;

        pts[0] = (x * 8);
        pts[1] = (y * 8);
        calc_projection_samples(mbmi, x, y, pts_inref);
        pts += 2;
        pts_inref += 2;
        np++;
        if (np >= LEAST_SQUARES_SAMPLES_MAX) return LEAST_SQUARES_SAMPLES_MAX;
      }
    }
  }
  assert(2 * np <= SAMPLES_ARRAY_SIZE);

  if (left_available && up_available) {
    int mi_row_offset = -1;
    int mi_col_offset = -1;

    MODE_INFO *mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
    MB_MODE_INFO *mbmi = &mi->mbmi;

    if (mbmi->ref_frame[0] == ref_frame && mbmi->ref_frame[1] == NONE_FRAME) {
      int bw = block_size_wide[mbmi->sb_type];
      int bh = block_size_high[mbmi->sb_type];
      int cr_offset = -AOMMAX(bh, MI_SIZE) / 2 - 1;
      int cc_offset = -AOMMAX(bw, MI_SIZE) / 2 - 1;
      int x = cc_offset + global_offset_c;
      int y = cr_offset + global_offset_r;

      pts[0] = (x * 8);
      pts[1] = (y * 8);
      calc_projection_samples(mbmi, x, y, pts_inref);
      np++;
    }
  }
  assert(2 * np <= SAMPLES_ARRAY_SIZE);

  return np;
}
#endif  // WARPED_MOTION_SORT_SAMPLES
#endif  // CONFIG_WARPED_MOTION
