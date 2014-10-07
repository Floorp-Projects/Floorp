/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/common/vp9_tile_common.h"

#include "vp9/common/vp9_onyxc_int.h"

#define MIN_TILE_WIDTH_B64 4
#define MAX_TILE_WIDTH_B64 64

static int get_tile_offset(int idx, int mis, int log2) {
  const int sb_cols = mi_cols_aligned_to_sb(mis) >> MI_BLOCK_SIZE_LOG2;
  const int offset = ((idx * sb_cols) >> log2) << MI_BLOCK_SIZE_LOG2;
  return MIN(offset, mis);
}

void vp9_tile_set_row(TileInfo *tile, const VP9_COMMON *cm, int row) {
  tile->mi_row_start = get_tile_offset(row, cm->mi_rows, cm->log2_tile_rows);
  tile->mi_row_end = get_tile_offset(row + 1, cm->mi_rows, cm->log2_tile_rows);
}

void vp9_tile_set_col(TileInfo *tile, const VP9_COMMON *cm, int col) {
  tile->mi_col_start = get_tile_offset(col, cm->mi_cols, cm->log2_tile_cols);
  tile->mi_col_end = get_tile_offset(col + 1, cm->mi_cols, cm->log2_tile_cols);
}

void vp9_tile_init(TileInfo *tile, const VP9_COMMON *cm, int row, int col) {
  vp9_tile_set_row(tile, cm, row);
  vp9_tile_set_col(tile, cm, col);
}

void vp9_get_tile_n_bits(int mi_cols,
                         int *min_log2_tile_cols, int *max_log2_tile_cols) {
  const int sb_cols = mi_cols_aligned_to_sb(mi_cols) >> MI_BLOCK_SIZE_LOG2;
  int min_log2 = 0, max_log2 = 0;

  // max
  while ((sb_cols >> max_log2) >= MIN_TILE_WIDTH_B64)
    ++max_log2;
  --max_log2;
  if (max_log2 < 0)
    max_log2 = 0;

  // min
  while ((MAX_TILE_WIDTH_B64 << min_log2) < sb_cols)
    ++min_log2;

  assert(min_log2 <= max_log2);

  *min_log2_tile_cols = min_log2;
  *max_log2_tile_cols = max_log2;
}
