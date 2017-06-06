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

void av1_tile_set_row(TileInfo *tile, const AV1_COMMON *cm, int row) {
  tile->mi_row_start = row * cm->tile_height;
  tile->mi_row_end = AOMMIN(tile->mi_row_start + cm->tile_height, cm->mi_rows);
}

void av1_tile_set_col(TileInfo *tile, const AV1_COMMON *cm, int col) {
  tile->mi_col_start = col * cm->tile_width;
  tile->mi_col_end = AOMMIN(tile->mi_col_start + cm->tile_width, cm->mi_cols);
}

#if CONFIG_DEPENDENT_HORZTILES && CONFIG_TILE_GROUPS
void av1_tile_set_tg_boundary(TileInfo *tile, const AV1_COMMON *const cm,
                              int row, int col) {
  if (row < cm->tile_rows - 1) {
    tile->tg_horz_boundary =
        col >= cm->tile_group_start_col[row][col]
            ? (row == cm->tile_group_start_row[row][col] ? 1 : 0)
            : (row == cm->tile_group_start_row[row + 1][col] ? 1 : 0);
  } else {
    assert(col >= cm->tile_group_start_col[row][col]);
    tile->tg_horz_boundary =
        (row == cm->tile_group_start_row[row][col] ? 1 : 0);
  }
}
#endif
void av1_tile_init(TileInfo *tile, const AV1_COMMON *cm, int row, int col) {
  av1_tile_set_row(tile, cm, row);
  av1_tile_set_col(tile, cm, col);
#if CONFIG_DEPENDENT_HORZTILES && CONFIG_TILE_GROUPS
  av1_tile_set_tg_boundary(tile, cm, row, col);
#endif
}

#if !CONFIG_EXT_TILE

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
#endif  // !CONFIG_EXT_TILE

void av1_update_boundary_info(const struct AV1Common *cm,
                              const TileInfo *const tile_info, int mi_row,
                              int mi_col) {
  int row, col;
  for (row = mi_row; row < (mi_row + cm->mib_size); row++)
    for (col = mi_col; col < (mi_col + cm->mib_size); col++) {
      MODE_INFO *const mi = cm->mi + row * cm->mi_stride + col;
      mi->mbmi.boundary_info = 0;

      // If horizontal dependent tile is enabled, then the horizontal
      // tile boundary is not treated as real tile boundary for loop
      // filtering, only the horizontal tile group boundary is treated
      // as tile boundary.
      // Otherwise, tile group boundary is treated the same as tile boundary.
      // Loop filtering operation is done based on the
      // loopfilter_across_tiles_enabled flag for both tile boundary and tile
      // group boundary.

      if (cm->tile_cols * cm->tile_rows > 1) {
#if CONFIG_DEPENDENT_HORZTILES
#if CONFIG_TILE_GROUPS
        if (row == tile_info->mi_row_start &&
            (!cm->dependent_horz_tiles || tile_info->tg_horz_boundary))
#else
        if (row == tile_info->mi_row_start && !cm->dependent_horz_tiles)
#endif  // CONFIG_TILE_GROUPS
#else
        if (row == tile_info->mi_row_start)
#endif  // CONFIG_DEPENDENT_HORZTILES

          mi->mbmi.boundary_info |= TILE_ABOVE_BOUNDARY;
        if (col == tile_info->mi_col_start)
          mi->mbmi.boundary_info |= TILE_LEFT_BOUNDARY;
        if ((row + 1) >= tile_info->mi_row_end)
          mi->mbmi.boundary_info |= TILE_BOTTOM_BOUNDARY;
        if ((col + 1) >= tile_info->mi_col_end)
          mi->mbmi.boundary_info |= TILE_RIGHT_BOUNDARY;
      }
      // Frame boundary is treated as tile boundary
      if (row == 0)
        mi->mbmi.boundary_info |= FRAME_ABOVE_BOUNDARY | TILE_ABOVE_BOUNDARY;
      if (col == 0)
        mi->mbmi.boundary_info |= FRAME_LEFT_BOUNDARY | TILE_LEFT_BOUNDARY;
      if ((row + 1) >= cm->mi_rows)
        mi->mbmi.boundary_info |= FRAME_BOTTOM_BOUNDARY | TILE_BOTTOM_BOUNDARY;
      if ((col + 1) >= cm->mi_cols)
        mi->mbmi.boundary_info |= FRAME_RIGHT_BOUNDARY | TILE_RIGHT_BOUNDARY;
    }
}

#if CONFIG_LOOPFILTERING_ACROSS_TILES
int av1_disable_loopfilter_on_tile_boundary(const struct AV1Common *cm) {
  return (!cm->loop_filter_across_tiles_enabled &&
          (cm->tile_cols * cm->tile_rows > 1));
}
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
