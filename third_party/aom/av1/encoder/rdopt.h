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

#ifndef AV1_ENCODER_RDOPT_H_
#define AV1_ENCODER_RDOPT_H_

#include "av1/common/blockd.h"

#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TileInfo;
struct AV1_COMP;
struct macroblock;
struct RD_STATS;

#if CONFIG_RD_DEBUG
static INLINE void av1_update_txb_coeff_cost(RD_STATS *rd_stats, int plane,
                                             TX_SIZE tx_size, int blk_row,
                                             int blk_col, int txb_coeff_cost) {
  (void)blk_row;
  (void)blk_col;
  (void)tx_size;
  rd_stats->txb_coeff_cost[plane] += txb_coeff_cost;

#if CONFIG_VAR_TX
  {
    const int txb_h = tx_size_high_unit[tx_size];
    const int txb_w = tx_size_wide_unit[tx_size];
    int idx, idy;
    for (idy = 0; idy < txb_h; ++idy)
      for (idx = 0; idx < txb_w; ++idx)
        rd_stats->txb_coeff_cost_map[plane][blk_row + idy][blk_col + idx] = 0;

    rd_stats->txb_coeff_cost_map[plane][blk_row][blk_col] = txb_coeff_cost;
  }
  assert(blk_row < TXB_COEFF_COST_MAP_SIZE);
  assert(blk_col < TXB_COEFF_COST_MAP_SIZE);
#endif
}
#endif

typedef enum OUTPUT_STATUS {
  OUTPUT_HAS_PREDICTED_PIXELS,
  OUTPUT_HAS_DECODED_PIXELS
} OUTPUT_STATUS;

// Returns the number of colors in 'src'.
int av1_count_colors(const uint8_t *src, int stride, int rows, int cols);
#if CONFIG_HIGHBITDEPTH
// Same as av1_count_colors(), but for high-bitdepth mode.
int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth);
#endif  // CONFIG_HIGHBITDEPTH

void av1_dist_block(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                    BLOCK_SIZE plane_bsize, int block, int blk_row, int blk_col,
                    TX_SIZE tx_size, int64_t *out_dist, int64_t *out_sse,
                    OUTPUT_STATUS output_status);

#if CONFIG_DIST_8X8
int64_t av1_dist_8x8(const AV1_COMP *const cpi, const MACROBLOCK *x,
                     const uint8_t *src, int src_stride, const uint8_t *dst,
                     int dst_stride, const BLOCK_SIZE tx_bsize, int bsw,
                     int bsh, int visible_w, int visible_h, int qindex);
#endif

#if !CONFIG_PVQ || CONFIG_VAR_TX
int av1_cost_coeffs(const AV1_COMP *const cpi, MACROBLOCK *x, int plane,
                    int blk_row, int blk_col, int block, TX_SIZE tx_size,
                    const SCAN_ORDER *scan_order, const ENTROPY_CONTEXT *a,
                    const ENTROPY_CONTEXT *l, int use_fast_coef_costing);
#endif
void av1_rd_pick_intra_mode_sb(const struct AV1_COMP *cpi, struct macroblock *x,
                               struct RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd);

unsigned int av1_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs);
#if CONFIG_HIGHBITDEPTH
unsigned int av1_high_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd);
#endif

void av1_rd_pick_inter_mode_sb(const struct AV1_COMP *cpi,
                               struct TileDataEnc *tile_data,
                               struct macroblock *x, int mi_row, int mi_col,
                               struct RD_STATS *rd_cost,
#if CONFIG_SUPERTX
                               int *returnrate_nocoef,
#endif  // CONFIG_SUPERTX
                               BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                               int64_t best_rd_so_far);

void av1_rd_pick_inter_mode_sb_seg_skip(
    const struct AV1_COMP *cpi, struct TileDataEnc *tile_data,
    struct macroblock *x, int mi_row, int mi_col, struct RD_STATS *rd_cost,
    BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

int av1_internal_image_edge(const struct AV1_COMP *cpi);
int av1_active_h_edge(const struct AV1_COMP *cpi, int mi_row, int mi_step);
int av1_active_v_edge(const struct AV1_COMP *cpi, int mi_col, int mi_step);
int av1_active_edge_sb(const struct AV1_COMP *cpi, int mi_row, int mi_col);

#if CONFIG_MOTION_VAR && CONFIG_NCOBMC
void av1_check_ncobmc_rd(const struct AV1_COMP *cpi, struct macroblock *x,
                         int mi_row, int mi_col);
#endif  // CONFIG_MOTION_VAR && CONFIG_NCOBMC

#if CONFIG_SUPERTX
#if CONFIG_VAR_TX
void av1_tx_block_rd_b(const AV1_COMP *cpi, MACROBLOCK *x, TX_SIZE tx_size,
                       int blk_row, int blk_col, int plane, int block,
                       int plane_bsize, const ENTROPY_CONTEXT *a,
                       const ENTROPY_CONTEXT *l, RD_STATS *rd_stats);
#endif

void av1_txfm_rd_in_plane_supertx(MACROBLOCK *x, const AV1_COMP *cpi, int *rate,
                                  int64_t *distortion, int *skippable,
                                  int64_t *sse, int64_t ref_best_rd, int plane,
                                  BLOCK_SIZE bsize, TX_SIZE tx_size,
                                  int use_fast_coef_casting);
#endif  // CONFIG_SUPERTX

#ifdef __cplusplus
}  // extern "C"
#endif

int av1_tx_type_cost(const AV1_COMMON *cm, const MACROBLOCK *x,
                     const MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane,
                     TX_SIZE tx_size, TX_TYPE tx_type);

int64_t get_prediction_rd_cost(const struct AV1_COMP *cpi, struct macroblock *x,
                               int mi_row, int mi_col, int *skip_blk,
                               MB_MODE_INFO *backup_mbmi);

#if CONFIG_NCOBMC_ADAPT_WEIGHT
void av1_check_ncobmc_adapt_weight_rd(const struct AV1_COMP *cpi,
                                      struct macroblock *x, int mi_row,
                                      int mi_col);
int get_ncobmc_mode(const AV1_COMP *const cpi, MACROBLOCK *const x,
                    MACROBLOCKD *xd, int mi_row, int mi_col, int bsize);

#endif

#endif  // AV1_ENCODER_RDOPT_H_
