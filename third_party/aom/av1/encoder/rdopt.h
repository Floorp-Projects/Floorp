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
#include "av1/common/txb_common.h"

#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_REF_MV_SERCH 3

struct TileInfo;
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
}
#endif

// Returns the number of colors in 'src'.
int av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                     int *val_count);
// Same as av1_count_colors(), but for high-bitdepth mode.
int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth, int *val_count);

#if CONFIG_DIST_8X8
int64_t av1_dist_8x8(const struct AV1_COMP *const cpi, const MACROBLOCK *x,
                     const uint8_t *src, int src_stride, const uint8_t *dst,
                     int dst_stride, const BLOCK_SIZE tx_bsize, int bsw,
                     int bsh, int visible_w, int visible_h, int qindex);
#endif

static INLINE int av1_cost_skip_txb(MACROBLOCK *x, const TXB_CTX *const txb_ctx,
                                    int plane, TX_SIZE tx_size) {
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const LV_MAP_COEFF_COST *const coeff_costs =
      &x->coeff_costs[txs_ctx][plane_type];
  return coeff_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
}

static INLINE int av1_cost_coeffs(const AV1_COMMON *const cm, MACROBLOCK *x,
                                  int plane, int blk_row, int blk_col,
                                  int block, TX_SIZE tx_size,
                                  const TXB_CTX *const txb_ctx,
                                  int use_fast_coef_costing) {
#if TXCOEFF_COST_TIMER
  struct aom_usec_timer timer;
  aom_usec_timer_start(&timer);
#endif
  (void)use_fast_coef_costing;
  const int cost = av1_cost_coeffs_txb(cm, x, plane, blk_row, blk_col, block,
                                       tx_size, txb_ctx);
#if TXCOEFF_COST_TIMER
  AV1_COMMON *tmp_cm = (AV1_COMMON *)&cpi->common;
  aom_usec_timer_mark(&timer);
  const int64_t elapsed_time = aom_usec_timer_elapsed(&timer);
  tmp_cm->txcoeff_cost_timer += elapsed_time;
  ++tmp_cm->txcoeff_cost_count;
#endif
  return cost;
}

void av1_rd_pick_intra_mode_sb(const struct AV1_COMP *cpi, struct macroblock *x,
                               int mi_row, int mi_col, struct RD_STATS *rd_cost,
                               BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                               int64_t best_rd);

unsigned int av1_get_sby_perpixel_variance(const struct AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs);
unsigned int av1_high_get_sby_perpixel_variance(const struct AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd);

void av1_rd_pick_inter_mode_sb(const struct AV1_COMP *cpi,
                               struct TileDataEnc *tile_data,
                               struct macroblock *x, int mi_row, int mi_col,
                               struct RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

void av1_rd_pick_inter_mode_sb_seg_skip(
    const struct AV1_COMP *cpi, struct TileDataEnc *tile_data,
    struct macroblock *x, int mi_row, int mi_col, struct RD_STATS *rd_cost,
    BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

#if CONFIG_COLLECT_INTER_MODE_RD_STATS
#define INTER_MODE_RD_TEST 0
void av1_inter_mode_data_init();
void av1_inter_mode_data_fit(int rdmult);
void av1_inter_mode_data_show(const AV1_COMMON *cm);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_RDOPT_H_
