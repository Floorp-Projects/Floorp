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

#ifndef ENCODETXB_H_
#define ENCODETXB_H_

#include "./aom_config.h"
#include "av1/common/blockd.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/txb_common.h"
#include "av1/encoder/block.h"
#include "av1/encoder/encoder.h"
#include "aom_dsp/bitwriter.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct TxbInfo {
  tran_low_t *qcoeff;
  tran_low_t *dqcoeff;
  const tran_low_t *tcoeff;
  const int16_t *dequant;
  int shift;
  TX_SIZE tx_size;
  TX_SIZE txs_ctx;
  int bwl;
  int stride;
  int height;
  int eob;
  int seg_eob;
  const SCAN_ORDER *scan_order;
  TXB_CTX *txb_ctx;
  int64_t rdmult;
} TxbInfo;

typedef struct TxbCache {
  int nz_count_arr[MAX_TX_SQUARE];
  int nz_ctx_arr[MAX_TX_SQUARE][2];
  int base_count_arr[NUM_BASE_LEVELS][MAX_TX_SQUARE];
  int base_mag_arr[MAX_TX_SQUARE]
                  [2];  // [0]: max magnitude [1]: num of max magnitude
  int base_ctx_arr[NUM_BASE_LEVELS][MAX_TX_SQUARE][2];  // [1]: not used

  int br_count_arr[MAX_TX_SQUARE];
  int br_mag_arr[MAX_TX_SQUARE]
                [2];  // [0]: max magnitude [1]: num of max magnitude
  int br_ctx_arr[MAX_TX_SQUARE][2];  // [1]: not used
} TxbCache;

typedef struct TxbProbs {
  const aom_prob *dc_sign_prob;
  const aom_prob *nz_map;
  aom_prob (*coeff_base)[COEFF_BASE_CONTEXTS];
  const aom_prob *coeff_lps;
  const aom_prob *eob_flag;
  const aom_prob *txb_skip;
} TxbProbs;

void av1_alloc_txb_buf(AV1_COMP *cpi);
void av1_free_txb_buf(AV1_COMP *cpi);
int av1_cost_coeffs_txb(const AV1_COMP *const cpi, MACROBLOCK *x, int plane,
                        int blk_row, int blk_col, int block, TX_SIZE tx_size,
                        TXB_CTX *txb_ctx);
void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                          aom_writer *w, int blk_row, int blk_col, int block,
                          int plane, TX_SIZE tx_size, const tran_low_t *tcoeff,
                          uint16_t eob, TXB_CTX *txb_ctx);
void av1_write_coeffs_mb(const AV1_COMMON *const cm, MACROBLOCK *x,
                         aom_writer *w, int plane);
int av1_get_txb_entropy_context(const tran_low_t *qcoeff,
                                const SCAN_ORDER *scan_order, int eob);
void av1_update_txb_context(const AV1_COMP *cpi, ThreadData *td,
                            RUN_TYPE dry_run, BLOCK_SIZE bsize, int *rate,
                            const int mi_row, const int mi_col);
void av1_write_txb_probs(AV1_COMP *cpi, aom_writer *w);

void av1_update_txb_context_b(int plane, int block, int blk_row, int blk_col,
                              BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                              void *arg);

void av1_update_and_record_txb_context(int plane, int block, int blk_row,
                                       int blk_col, BLOCK_SIZE plane_bsize,
                                       TX_SIZE tx_size, void *arg);

#if CONFIG_TXK_SEL
int64_t av1_search_txk_type(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                            int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            const ENTROPY_CONTEXT *a, const ENTROPY_CONTEXT *l,
                            int use_fast_coef_costing, RD_STATS *rd_stats);
#endif
int av1_optimize_txb(const AV1_COMMON *cm, MACROBLOCK *x, int plane,
                     int blk_row, int blk_col, int block, TX_SIZE tx_size,
                     TXB_CTX *txb_ctx);
#ifdef __cplusplus
}
#endif

#endif  // COEFFS_CODING_H_
