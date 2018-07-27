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

#include "av1/common/tile_common.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/resize.h"
#include "aom_dsp/aom_dsp_common.h"

void av1_tile_init(TileInfo *tile, const AV1_COMMON *cm, int row, int col) {
  av1_tile_set_row(tile, cm, row);
  av1_tile_set_col(tile, cm, col);
}

// Find smallest k>=0 such that (blk_size << k) >= target
static int tile_log2(int blk_size, int target) {
  int k;
  for (k = 0; (blk_size << k) < target; k++) {
  }
  return k;
}

void av1_get_tile_limits(AV1_COMMON *const cm) {
  int mi_cols = ALIGN_POWER_OF_TWO(cm->mi_cols, cm->seq_params.mib_size_log2);
  int mi_rows = ALIGN_POWER_OF_TWO(cm->mi_rows, cm->seq_params.mib_size_log2);
  int sb_cols = mi_cols >> cm->seq_params.mib_size_log2;
  int sb_rows = mi_rows >> cm->seq_params.mib_size_log2;

  int sb_size_log2 = cm->seq_params.mib_size_log2 + MI_SIZE_LOG2;
  cm->max_tile_width_sb = MAX_TILE_WIDTH >> sb_size_log2;
  int max_tile_area_sb = MAX_TILE_AREA >> (2 * sb_size_log2);

  cm->min_log2_tile_cols = tile_log2(cm->max_tile_width_sb, sb_cols);
  cm->max_log2_tile_cols = tile_log2(1, AOMMIN(sb_cols, MAX_TILE_COLS));
  cm->max_log2_tile_rows = tile_log2(1, AOMMIN(sb_rows, MAX_TILE_ROWS));
  cm->min_log2_tiles = tile_log2(max_tile_area_sb, sb_cols * sb_rows);
  cm->min_log2_tiles = AOMMAX(cm->min_log2_tiles, cm->min_log2_tile_cols);
}

void av1_calculate_tile_cols(AV1_COMMON *const cm) {
  int mi_cols = ALIGN_POWER_OF_TWO(cm->mi_cols, cm->seq_params.mib_size_log2);
  int mi_rows = ALIGN_POWER_OF_TWO(cm->mi_rows, cm->seq_params.mib_size_log2);
  int sb_cols = mi_cols >> cm->seq_params.mib_size_log2;
  int sb_rows = mi_rows >> cm->seq_params.mib_size_log2;
  int i;

  if (cm->uniform_tile_spacing_flag) {
    int start_sb;
    int size_sb = ALIGN_POWER_OF_TWO(sb_cols, cm->log2_tile_cols);
    size_sb >>= cm->log2_tile_cols;
    assert(size_sb > 0);
    for (i = 0, start_sb = 0; start_sb < sb_cols; i++) {
      cm->tile_col_start_sb[i] = start_sb;
      start_sb += size_sb;
    }
    cm->tile_cols = i;
    cm->tile_col_start_sb[i] = sb_cols;
    cm->min_log2_tile_rows = AOMMAX(cm->min_log2_tiles - cm->log2_tile_cols, 0);
    cm->max_tile_height_sb = sb_rows >> cm->min_log2_tile_rows;

    cm->tile_width = size_sb << cm->seq_params.mib_size_log2;
    cm->tile_width = AOMMIN(cm->tile_width, cm->mi_cols);
  } else {
    int max_tile_area_sb = (sb_rows * sb_cols);
    int widest_tile_sb = 1;
    cm->log2_tile_cols = tile_log2(1, cm->tile_cols);
    for (i = 0; i < cm->tile_cols; i++) {
      int size_sb = cm->tile_col_start_sb[i + 1] - cm->tile_col_start_sb[i];
      widest_tile_sb = AOMMAX(widest_tile_sb, size_sb);
    }
    if (cm->min_log2_tiles) {
      max_tile_area_sb >>= (cm->min_log2_tiles + 1);
    }
    cm->max_tile_height_sb = AOMMAX(max_tile_area_sb / widest_tile_sb, 1);
  }
}

void av1_calculate_tile_rows(AV1_COMMON *const cm) {
  int mi_rows = ALIGN_POWER_OF_TWO(cm->mi_rows, cm->seq_params.mib_size_log2);
  int sb_rows = mi_rows >> cm->seq_params.mib_size_log2;
  int start_sb, size_sb, i;

  if (cm->uniform_tile_spacing_flag) {
    size_sb = ALIGN_POWER_OF_TWO(sb_rows, cm->log2_tile_rows);
    size_sb >>= cm->log2_tile_rows;
    assert(size_sb > 0);
    for (i = 0, start_sb = 0; start_sb < sb_rows; i++) {
      cm->tile_row_start_sb[i] = start_sb;
      start_sb += size_sb;
    }
    cm->tile_rows = i;
    cm->tile_row_start_sb[i] = sb_rows;

    cm->tile_height = size_sb << cm->seq_params.mib_size_log2;
    cm->tile_height = AOMMIN(cm->tile_height, cm->mi_rows);
  } else {
    cm->log2_tile_rows = tile_log2(1, cm->tile_rows);
  }
}

void av1_tile_set_row(TileInfo *tile, const AV1_COMMON *cm, int row) {
  assert(row < cm->tile_rows);
  int mi_row_start = cm->tile_row_start_sb[row] << cm->seq_params.mib_size_log2;
  int mi_row_end = cm->tile_row_start_sb[row + 1]
                   << cm->seq_params.mib_size_log2;
  tile->tile_row = row;
  tile->mi_row_start = mi_row_start;
  tile->mi_row_end = AOMMIN(mi_row_end, cm->mi_rows);
  assert(tile->mi_row_end > tile->mi_row_start);
}

void av1_tile_set_col(TileInfo *tile, const AV1_COMMON *cm, int col) {
  assert(col < cm->tile_cols);
  int mi_col_start = cm->tile_col_start_sb[col] << cm->seq_params.mib_size_log2;
  int mi_col_end = cm->tile_col_start_sb[col + 1]
                   << cm->seq_params.mib_size_log2;
  tile->tile_col = col;
  tile->mi_col_start = mi_col_start;
  tile->mi_col_end = AOMMIN(mi_col_end, cm->mi_cols);
  assert(tile->mi_col_end > tile->mi_col_start);
}

int get_tile_size(int mi_frame_size, int log2_tile_num, int *ntiles) {
  // Round the frame up to a whole number of max superblocks
  mi_frame_size = ALIGN_POWER_OF_TWO(mi_frame_size, MAX_MIB_SIZE_LOG2);

  // Divide by the signalled number of tiles, rounding up to the multiple of
  // the max superblock size. To do this, shift right (and round up) to get the
  // tile size in max super-blocks and then shift left again to convert it to
  // mi units.
  const int shift = log2_tile_num + MAX_MIB_SIZE_LOG2;
  const int max_sb_tile_size =
      ALIGN_POWER_OF_TWO(mi_frame_size, shift) >> shift;
  const int mi_tile_size = max_sb_tile_size << MAX_MIB_SIZE_LOG2;

  // The actual number of tiles is the ceiling of the frame size in mi units
  // divided by mi_size. This is at most 1 << log2_tile_num but might be
  // strictly less if max_sb_tile_size got rounded up significantly.
  if (ntiles) {
    *ntiles = (mi_frame_size + mi_tile_size - 1) / mi_tile_size;
    assert(*ntiles <= (1 << log2_tile_num));
  }

  return mi_tile_size;
}

AV1PixelRect av1_get_tile_rect(const TileInfo *tile_info, const AV1_COMMON *cm,
                               int is_uv) {
  AV1PixelRect r;

  // Calculate position in the Y plane
  r.left = tile_info->mi_col_start * MI_SIZE;
  r.right = tile_info->mi_col_end * MI_SIZE;
  r.top = tile_info->mi_row_start * MI_SIZE;
  r.bottom = tile_info->mi_row_end * MI_SIZE;

  // If upscaling is enabled, the tile limits need scaling to match the
  // upscaled frame where the restoration units live. To do this, scale up the
  // top-left and bottom-right of the tile.
  if (av1_superres_scaled(cm)) {
    av1_calculate_unscaled_superres_size(&r.left, &r.top,
                                         cm->superres_scale_denominator);
    av1_calculate_unscaled_superres_size(&r.right, &r.bottom,
                                         cm->superres_scale_denominator);
  }

  const int frame_w = cm->superres_upscaled_width;
  const int frame_h = cm->superres_upscaled_height;

  // Make sure we don't fall off the bottom-right of the frame.
  r.right = AOMMIN(r.right, frame_w);
  r.bottom = AOMMIN(r.bottom, frame_h);

  // Convert to coordinates in the appropriate plane
  const int ss_x = is_uv && cm->seq_params.subsampling_x;
  const int ss_y = is_uv && cm->seq_params.subsampling_y;

  r.left = ROUND_POWER_OF_TWO(r.left, ss_x);
  r.right = ROUND_POWER_OF_TWO(r.right, ss_x);
  r.top = ROUND_POWER_OF_TWO(r.top, ss_y);
  r.bottom = ROUND_POWER_OF_TWO(r.bottom, ss_y);

  return r;
}
