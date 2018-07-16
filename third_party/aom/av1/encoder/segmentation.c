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

#include <limits.h>

#include "aom_mem/aom_mem.h"

#include "av1/common/pred_common.h"
#include "av1/common/tile_common.h"

#include "av1/encoder/cost.h"
#include "av1/encoder/segmentation.h"

void av1_enable_segmentation(struct segmentation *seg) {
  seg->enabled = 1;
  seg->update_map = 1;
  seg->update_data = 1;
  seg->temporal_update = 0;
}

void av1_disable_segmentation(struct segmentation *seg) {
  seg->enabled = 0;
  seg->update_map = 0;
  seg->update_data = 0;
  seg->temporal_update = 0;
}

void av1_disable_segfeature(struct segmentation *seg, int segment_id,
                            SEG_LVL_FEATURES feature_id) {
  seg->feature_mask[segment_id] &= ~(1 << feature_id);
}

void av1_clear_segdata(struct segmentation *seg, int segment_id,
                       SEG_LVL_FEATURES feature_id) {
  seg->feature_data[segment_id][feature_id] = 0;
}

static void count_segs(const AV1_COMMON *cm, MACROBLOCKD *xd,
                       const TileInfo *tile, MB_MODE_INFO **mi,
                       unsigned *no_pred_segcounts,
                       unsigned (*temporal_predictor_count)[2],
                       unsigned *t_unpred_seg_counts, int bw, int bh,
                       int mi_row, int mi_col) {
  int segment_id;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  xd->mi = mi;
  segment_id = xd->mi[0]->segment_id;

  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw, cm->mi_rows, cm->mi_cols);

  // Count the number of hits on each segment with no prediction
  no_pred_segcounts[segment_id]++;

  // Temporal prediction not allowed on key frames
  if (cm->frame_type != KEY_FRAME) {
    const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
    // Test to see if the segment id matches the predicted value.
    const int pred_segment_id =
        cm->last_frame_seg_map
            ? get_segment_id(cm, cm->last_frame_seg_map, bsize, mi_row, mi_col)
            : 0;
    const int pred_flag = pred_segment_id == segment_id;
    const int pred_context = av1_get_pred_context_seg_id(xd);

    // Store the prediction status for this mb and update counts
    // as appropriate
    xd->mi[0]->seg_id_predicted = pred_flag;
    temporal_predictor_count[pred_context][pred_flag]++;

    // Update the "unpredicted" segment count
    if (!pred_flag) t_unpred_seg_counts[segment_id]++;
  }
}

static void count_segs_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                          const TileInfo *tile, MB_MODE_INFO **mi,
                          unsigned *no_pred_segcounts,
                          unsigned (*temporal_predictor_count)[2],
                          unsigned *t_unpred_seg_counts, int mi_row, int mi_col,
                          BLOCK_SIZE bsize) {
  const int mis = cm->mi_stride;
  const int bs = mi_size_wide[bsize], hbs = bs / 2;
  PARTITION_TYPE partition;
  const int qbs = bs / 4;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

#define CSEGS(cs_bw, cs_bh, cs_rowoff, cs_coloff)                              \
  count_segs(cm, xd, tile, mi + mis * (cs_rowoff) + (cs_coloff),               \
             no_pred_segcounts, temporal_predictor_count, t_unpred_seg_counts, \
             (cs_bw), (cs_bh), mi_row + (cs_rowoff), mi_col + (cs_coloff));

  if (bsize == BLOCK_8X8)
    partition = PARTITION_NONE;
  else
    partition = get_partition(cm, mi_row, mi_col, bsize);
  switch (partition) {
    case PARTITION_NONE: CSEGS(bs, bs, 0, 0); break;
    case PARTITION_HORZ:
      CSEGS(bs, hbs, 0, 0);
      CSEGS(bs, hbs, hbs, 0);
      break;
    case PARTITION_VERT:
      CSEGS(hbs, bs, 0, 0);
      CSEGS(hbs, bs, 0, hbs);
      break;
    case PARTITION_HORZ_A:
      CSEGS(hbs, hbs, 0, 0);
      CSEGS(hbs, hbs, 0, hbs);
      CSEGS(bs, hbs, hbs, 0);
      break;
    case PARTITION_HORZ_B:
      CSEGS(bs, hbs, 0, 0);
      CSEGS(hbs, hbs, hbs, 0);
      CSEGS(hbs, hbs, hbs, hbs);
      break;
    case PARTITION_VERT_A:
      CSEGS(hbs, hbs, 0, 0);
      CSEGS(hbs, hbs, hbs, 0);
      CSEGS(hbs, bs, 0, hbs);
      break;
    case PARTITION_VERT_B:
      CSEGS(hbs, bs, 0, 0);
      CSEGS(hbs, hbs, 0, hbs);
      CSEGS(hbs, hbs, hbs, hbs);
      break;
    case PARTITION_HORZ_4:
      CSEGS(bs, qbs, 0, 0);
      CSEGS(bs, qbs, qbs, 0);
      CSEGS(bs, qbs, 2 * qbs, 0);
      if (mi_row + 3 * qbs < cm->mi_rows) CSEGS(bs, qbs, 3 * qbs, 0);
      break;

    case PARTITION_VERT_4:
      CSEGS(qbs, bs, 0, 0);
      CSEGS(qbs, bs, 0, qbs);
      CSEGS(qbs, bs, 0, 2 * qbs);
      if (mi_col + 3 * qbs < cm->mi_cols) CSEGS(qbs, bs, 0, 3 * qbs);
      break;

    case PARTITION_SPLIT: {
      const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
      int n;

      for (n = 0; n < 4; n++) {
        const int mi_dc = hbs * (n & 1);
        const int mi_dr = hbs * (n >> 1);

        count_segs_sb(cm, xd, tile, &mi[mi_dr * mis + mi_dc], no_pred_segcounts,
                      temporal_predictor_count, t_unpred_seg_counts,
                      mi_row + mi_dr, mi_col + mi_dc, subsize);
      }
    } break;
    default: assert(0);
  }

#undef CSEGS
}

void av1_choose_segmap_coding_method(AV1_COMMON *cm, MACROBLOCKD *xd) {
  struct segmentation *seg = &cm->seg;
  struct segmentation_probs *segp = &cm->fc->seg;
  int no_pred_cost;
  int t_pred_cost = INT_MAX;
  int tile_col, tile_row, mi_row, mi_col;
  unsigned temporal_predictor_count[SEG_TEMPORAL_PRED_CTXS][2] = { { 0 } };
  unsigned no_pred_segcounts[MAX_SEGMENTS] = { 0 };
  unsigned t_unpred_seg_counts[MAX_SEGMENTS] = { 0 };
  (void)xd;

  // First of all generate stats regarding how well the last segment map
  // predicts this one
  for (tile_row = 0; tile_row < cm->tile_rows; tile_row++) {
    TileInfo tile_info;
    av1_tile_set_row(&tile_info, cm, tile_row);
    for (tile_col = 0; tile_col < cm->tile_cols; tile_col++) {
      MB_MODE_INFO **mi_ptr;
      av1_tile_set_col(&tile_info, cm, tile_col);
      mi_ptr = cm->mi_grid_visible + tile_info.mi_row_start * cm->mi_stride +
               tile_info.mi_col_start;
      for (mi_row = tile_info.mi_row_start; mi_row < tile_info.mi_row_end;
           mi_row += cm->seq_params.mib_size,
          mi_ptr += cm->seq_params.mib_size * cm->mi_stride) {
        MB_MODE_INFO **mi = mi_ptr;
        for (mi_col = tile_info.mi_col_start; mi_col < tile_info.mi_col_end;
             mi_col += cm->seq_params.mib_size, mi += cm->seq_params.mib_size) {
          count_segs_sb(cm, xd, &tile_info, mi, no_pred_segcounts,
                        temporal_predictor_count, t_unpred_seg_counts, mi_row,
                        mi_col, cm->seq_params.sb_size);
        }
      }
    }
  }

  int seg_id_cost[MAX_SEGMENTS];
  av1_cost_tokens_from_cdf(seg_id_cost, segp->tree_cdf, NULL);
  no_pred_cost = 0;
  for (int i = 0; i < MAX_SEGMENTS; ++i)
    no_pred_cost += no_pred_segcounts[i] * seg_id_cost[i];

  // Frames without past dependency cannot use temporal prediction
  if (cm->primary_ref_frame != PRIMARY_REF_NONE) {
    int pred_flag_cost[SEG_TEMPORAL_PRED_CTXS][2];
    for (int i = 0; i < SEG_TEMPORAL_PRED_CTXS; ++i)
      av1_cost_tokens_from_cdf(pred_flag_cost[i], segp->pred_cdf[i], NULL);
    t_pred_cost = 0;
    // Cost for signaling the prediction flag.
    for (int i = 0; i < SEG_TEMPORAL_PRED_CTXS; ++i) {
      for (int j = 0; j < 2; ++j)
        t_pred_cost += temporal_predictor_count[i][j] * pred_flag_cost[i][j];
    }
    // Cost for signaling the unpredicted segment id.
    for (int i = 0; i < MAX_SEGMENTS; ++i)
      t_pred_cost += t_unpred_seg_counts[i] * seg_id_cost[i];
  }

  // Now choose which coding method to use.
  if (t_pred_cost < no_pred_cost) {
    assert(!cm->error_resilient_mode);
    seg->temporal_update = 1;
  } else {
    seg->temporal_update = 0;
  }
}

void av1_reset_segment_features(AV1_COMMON *cm) {
  struct segmentation *seg = &cm->seg;

  // Set up default state for MB feature flags
  seg->enabled = 0;
  seg->update_map = 0;
  seg->update_data = 0;
  av1_clearall_segfeatures(seg);
}
