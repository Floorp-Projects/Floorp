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

#ifndef DECODETXB_H_
#define DECODETXB_H_

#include "./aom_config.h"
#include "av1/common/blockd.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/txb_common.h"
#include "aom_dsp/bitreader.h"

uint8_t av1_read_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                            aom_reader *r, int blk_row, int blk_col, int block,
                            int plane, tran_low_t *tcoeffs, TXB_CTX *txb_ctx,
                            TX_SIZE tx_size, int16_t *max_scan_line, int *eob);

uint8_t av1_read_coeffs_txb_facade(AV1_COMMON *cm, MACROBLOCKD *xd,
                                   aom_reader *r, int row, int col, int block,
                                   int plane, tran_low_t *tcoeffs,
                                   TX_SIZE tx_size, int16_t *max_scan_line,
                                   int *eob);
#if !LV_MAP_PROB
void av1_read_txb_probs(FRAME_CONTEXT *fc, TX_MODE tx_mode, aom_reader *r,
                        FRAME_COUNTS *counts);
#endif  // !LV_MAP_PROB
#endif  //  DECODETXB_H_
