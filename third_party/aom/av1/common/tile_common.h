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

#ifndef AOM_AV1_COMMON_TILE_COMMON_H_
#define AOM_AV1_COMMON_TILE_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "config/aom_config.h"

struct AV1Common;

#define DEFAULT_MAX_NUM_TG 1

typedef struct TileInfo {
  int mi_row_start, mi_row_end;
  int mi_col_start, mi_col_end;
  int tg_horz_boundary;
  int tile_row;
  int tile_col;
} TileInfo;

// initializes 'tile->mi_(row|col)_(start|end)' for (row, col) based on
// 'cm->log2_tile_(rows|cols)' & 'cm->mi_(rows|cols)'
void av1_tile_init(TileInfo *tile, const struct AV1Common *cm, int row,
                   int col);

void av1_tile_set_row(TileInfo *tile, const struct AV1Common *cm, int row);
void av1_tile_set_col(TileInfo *tile, const struct AV1Common *cm, int col);
void av1_get_tile_n_bits(int mi_cols, int *min_log2_tile_cols,
                         int *max_log2_tile_cols);

// Calculate the correct tile size (width or height) for (1 << log2_tile_num)
// tiles horizontally or vertically in the frame.
int get_tile_size(int mi_frame_size, int log2_tile_num, int *ntiles);

int av1_get_sb_rows_in_tile(struct AV1Common *cm, TileInfo tile);
int av1_get_sb_cols_in_tile(struct AV1Common *cm, TileInfo tile);

typedef struct {
  int left, top, right, bottom;
} AV1PixelRect;

// Return the pixel extents of the given tile
AV1PixelRect av1_get_tile_rect(const TileInfo *tile_info,
                               const struct AV1Common *cm, int is_uv);

// Define tile maximum width and area
// There is no maximum height since height is limited by area and width limits
// The minimum tile width or height is fixed at one superblock
#define MAX_TILE_WIDTH (4096)        // Max Tile width in pixels
#define MAX_TILE_AREA (4096 * 2304)  // Maximum tile area in pixels

void av1_get_tile_limits(struct AV1Common *const cm);
void av1_calculate_tile_cols(struct AV1Common *const cm);
void av1_calculate_tile_rows(struct AV1Common *const cm);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_TILE_COMMON_H_
