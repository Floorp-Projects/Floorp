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
#include "aom_dsp/aom_dsp_common.h"

#if CONFIG_DEPENDENT_HORZTILES
void av1_tile_set_tg_boundary(TileInfo *tile, const AV1_COMMON *const cm,
                              int row, int col) {
  const int tg_start_row = cm->tile_group_start_row[row][col];
  const int tg_start_col = cm->tile_group_start_col[row][col];
  tile->tg_horz_boundary = ((row == tg_start_row && col >= tg_start_col) ||
                            (row == tg_start_row + 1 && col < tg_start_col));
#if CONFIG_MAX_TILE
  if (cm->tile_row_independent[row]) {
    tile->tg_horz_boundary = 1;  // this tile row is independent
  }
#endif
}
#endif
void av1_tile_init(TileInfo *tile, const AV1_COMMON *cm, int row, int col) {
  av1_tile_set_row(tile, cm, row);
  av1_tile_set_col(tile, cm, col);
#if CONFIG_DEPENDENT_HORZTILES
  av1_tile_set_tg_boundary(tile, cm, row, col);
#endif
}

#if CONFIG_MAX_TILE

// Find smallest k>=0 such that (blk_size << k) >= target
static int tile_log2(int blk_size, int target) {
  int k;
  for (k = 0; (blk_size << k) < target; k++) {
  }
  return k;
}

void av1_get_tile_limits(AV1_COMMON *const cm) {
  int mi_cols = ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2);
  int mi_rows = ALIGN_POWER_OF_TWO(cm->mi_rows, MAX_MIB_SIZE_LOG2);
  int sb_cols = mi_cols >> MAX_MIB_SIZE_LOG2;
  int sb_rows = mi_rows >> MAX_MIB_SIZE_LOG2;

  cm->min_log2_tile_cols = tile_log2(MAX_TILE_WIDTH_SB, sb_cols);
  cm->max_log2_tile_cols = tile_log2(1, AOMMIN(sb_cols, MAX_TILE_COLS));
  cm->max_log2_tile_rows = tile_log2(1, AOMMIN(sb_rows, MAX_TILE_ROWS));
  cm->min_log2_tiles = tile_log2(MAX_TILE_AREA_SB, sb_cols * sb_rows);
  cm->min_log2_tiles = AOMMAX(cm->min_log2_tiles, cm->min_log2_tile_cols);
  // TODO(dominic.symes@arm.com):
  // Add in levelMinLog2Tiles as a lower limit when levels are defined
}

void av1_calculate_tile_cols(AV1_COMMON *const cm) {
  int mi_cols = ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2);
  int mi_rows = ALIGN_POWER_OF_TWO(cm->mi_rows, MAX_MIB_SIZE_LOG2);
  int sb_cols = mi_cols >> MAX_MIB_SIZE_LOG2;
  int sb_rows = mi_rows >> MAX_MIB_SIZE_LOG2;
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
  } else {
    int max_tile_area_sb = (sb_rows * sb_cols);
    int max_tile_width_sb = 0;
    cm->log2_tile_cols = tile_log2(1, cm->tile_cols);
    for (i = 0; i < cm->tile_cols; i++) {
      int size_sb = cm->tile_col_start_sb[i + 1] - cm->tile_col_start_sb[i];
      max_tile_width_sb = AOMMAX(max_tile_width_sb, size_sb);
    }
    if (cm->min_log2_tiles) {
      max_tile_area_sb >>= (cm->min_log2_tiles + 1);
    }
    cm->max_tile_height_sb = AOMMAX(max_tile_area_sb / max_tile_width_sb, 1);
  }
}

void av1_calculate_tile_rows(AV1_COMMON *const cm) {
  int mi_rows = ALIGN_POWER_OF_TWO(cm->mi_rows, MAX_MIB_SIZE_LOG2);
  int sb_rows = mi_rows >> MAX_MIB_SIZE_LOG2;
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
  } else {
    cm->log2_tile_rows = tile_log2(1, cm->tile_rows);
  }

#if CONFIG_DEPENDENT_HORZTILES
  // Record which tile rows must be indpendent for parallelism
  for (i = 0, start_sb = 0; i < cm->tile_rows; i++) {
    cm->tile_row_independent[i] = 0;
    if (cm->tile_row_start_sb[i + 1] - start_sb > cm->max_tile_height_sb) {
      cm->tile_row_independent[i] = 1;
      start_sb = cm->tile_row_start_sb[i];
    }
  }
#endif
}

void av1_tile_set_row(TileInfo *tile, const AV1_COMMON *cm, int row) {
  assert(row < cm->tile_rows);
  int mi_row_start = cm->tile_row_start_sb[row] << MAX_MIB_SIZE_LOG2;
  int mi_row_end = cm->tile_row_start_sb[row + 1] << MAX_MIB_SIZE_LOG2;
  tile->mi_row_start = mi_row_start;
  tile->mi_row_end = AOMMIN(mi_row_end, cm->mi_rows);
}

void av1_tile_set_col(TileInfo *tile, const AV1_COMMON *cm, int col) {
  assert(col < cm->tile_cols);
  int mi_col_start = cm->tile_col_start_sb[col] << MAX_MIB_SIZE_LOG2;
  int mi_col_end = cm->tile_col_start_sb[col + 1] << MAX_MIB_SIZE_LOG2;
  tile->mi_col_start = mi_col_start;
  tile->mi_col_end = AOMMIN(mi_col_end, cm->mi_cols);
}

#else

void av1_tile_set_row(TileInfo *tile, const AV1_COMMON *cm, int row) {
  tile->mi_row_start = row * cm->tile_height;
  tile->mi_row_end = AOMMIN(tile->mi_row_start + cm->tile_height, cm->mi_rows);
}

void av1_tile_set_col(TileInfo *tile, const AV1_COMMON *cm, int col) {
  tile->mi_col_start = col * cm->tile_width;
  tile->mi_col_end = AOMMIN(tile->mi_col_start + cm->tile_width, cm->mi_cols);
}

#if CONFIG_EXT_PARTITION
#define MIN_TILE_WIDTH_MAX_SB 2
#define MAX_TILE_WIDTH_MAX_SB 32
#else
#define MIN_TILE_WIDTH_MAX_SB 4
#define MAX_TILE_WIDTH_MAX_SB 64
#endif  // CONFIG_EXT_PARTITION

static int get_min_log2_tile_cols(int max_sb_cols) {
  int min_log2 = 0;
  while ((MAX_TILE_WIDTH_MAX_SB << min_log2) < max_sb_cols) ++min_log2;
  return min_log2;
}

static int get_max_log2_tile_cols(int max_sb_cols) {
  int max_log2 = 1;
  while ((max_sb_cols >> max_log2) >= MIN_TILE_WIDTH_MAX_SB) ++max_log2;
  return max_log2 - 1;
}

void av1_get_tile_n_bits(int mi_cols, int *min_log2_tile_cols,
                         int *max_log2_tile_cols) {
  const int max_sb_cols =
      ALIGN_POWER_OF_TWO(mi_cols, MAX_MIB_SIZE_LOG2) >> MAX_MIB_SIZE_LOG2;
  *min_log2_tile_cols = get_min_log2_tile_cols(max_sb_cols);
  *max_log2_tile_cols = get_max_log2_tile_cols(max_sb_cols);
  assert(*min_log2_tile_cols <= *max_log2_tile_cols);
}
#endif  // CONFIG_MAX_TILE

void av1_setup_frame_boundary_info(const AV1_COMMON *const cm) {
  MODE_INFO *mi = cm->mi;
  int col;
  for (col = 0; col < cm->mi_cols; ++col) {
    mi->mbmi.boundary_info |= FRAME_ABOVE_BOUNDARY | TILE_ABOVE_BOUNDARY;
    mi += 1;
  }

  mi = cm->mi;
  int row;
  for (row = 0; row < cm->mi_rows; ++row) {
    mi->mbmi.boundary_info |= FRAME_LEFT_BOUNDARY | TILE_LEFT_BOUNDARY;
    mi += cm->mi_stride;
  }

  mi = cm->mi + (cm->mi_rows - 1) * cm->mi_stride;
  for (col = 0; col < cm->mi_cols; ++col) {
    mi->mbmi.boundary_info |= FRAME_BOTTOM_BOUNDARY | TILE_BOTTOM_BOUNDARY;
    mi += 1;
  }

  mi = cm->mi + cm->mi_cols - 1;
  for (row = 0; row < cm->mi_rows; ++row) {
    mi->mbmi.boundary_info |= FRAME_RIGHT_BOUNDARY | TILE_RIGHT_BOUNDARY;
    mi += cm->mi_stride;
  }
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

#if CONFIG_LOOPFILTERING_ACROSS_TILES
void av1_setup_across_tile_boundary_info(const AV1_COMMON *const cm,
                                         const TileInfo *const tile_info) {
  if (cm->tile_cols * cm->tile_rows > 1) {
    const int mi_row = tile_info->mi_row_start;
    const int mi_col = tile_info->mi_col_start;
    MODE_INFO *const mi_start = cm->mi + mi_row * cm->mi_stride + mi_col;
    assert(mi_start < cm->mip + cm->mi_alloc_size);
    MODE_INFO *mi = 0;
    const int row_diff = tile_info->mi_row_end - tile_info->mi_row_start;
    const int col_diff = tile_info->mi_col_end - tile_info->mi_col_start;
    int row, col;

#if CONFIG_DEPENDENT_HORZTILES
    if (!cm->dependent_horz_tiles || tile_info->tg_horz_boundary)
#endif  // CONFIG_DEPENDENT_HORZTILES
    {
      mi = mi_start;
      for (col = 0; col < col_diff; ++col) {
        mi->mbmi.boundary_info |= TILE_ABOVE_BOUNDARY;
        mi += 1;
      }
    }

    mi = mi_start;
    for (row = 0; row < row_diff; ++row) {
      mi->mbmi.boundary_info |= TILE_LEFT_BOUNDARY;
      mi += cm->mi_stride;
    }

    mi = mi_start + (row_diff - 1) * cm->mi_stride;

    // explicit bounds checking
    assert(mi + col_diff <= cm->mip + cm->mi_alloc_size);

    for (col = 0; col < col_diff; ++col) {
      mi->mbmi.boundary_info |= TILE_BOTTOM_BOUNDARY;
      mi += 1;
    }

    mi = mi_start + col_diff - 1;
    for (row = 0; row < row_diff; ++row) {
      mi->mbmi.boundary_info |= TILE_RIGHT_BOUNDARY;
      mi += cm->mi_stride;
    }
  }
}

int av1_disable_loopfilter_on_tile_boundary(const struct AV1Common *cm) {
  return (!cm->loop_filter_across_tiles_enabled &&
          (cm->tile_cols * cm->tile_rows > 1));
}
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
