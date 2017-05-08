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
void av1_alloc_txb_buf(AV1_COMP *cpi);
void av1_free_txb_buf(AV1_COMP *cpi);
int av1_cost_coeffs_txb(const AV1_COMP *const cpi, MACROBLOCK *x, int plane,
                        int block, TXB_CTX *txb_ctx);
void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                          aom_writer *w, int block, int plane,
                          const tran_low_t *tcoeff, uint16_t eob,
                          TXB_CTX *txb_ctx);
void av1_write_coeffs_mb(const AV1_COMMON *const cm, MACROBLOCK *x,
                         aom_writer *w, int plane);
int av1_get_txb_entropy_context(const tran_low_t *qcoeff,
                                const SCAN_ORDER *scan_order, int eob);
void av1_update_txb_context(const AV1_COMP *cpi, ThreadData *td,
                            RUN_TYPE dry_run, BLOCK_SIZE bsize, int *rate,
                            const int mi_row, const int mi_col);
void av1_write_txb_probs(AV1_COMP *cpi, aom_writer *w);

#if CONFIG_TXK_SEL
int64_t av1_search_txk_type(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                            int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            const ENTROPY_CONTEXT *a, const ENTROPY_CONTEXT *l,
                            int use_fast_coef_costing, RD_STATS *rd_stats);
#endif
#ifdef __cplusplus
}
#endif

#endif  // COEFFS_CODING_H_
