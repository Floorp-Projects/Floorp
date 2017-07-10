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

#ifndef AV1_COMMON_IDCT_H_
#define AV1_COMMON_IDCT_H_

#include <assert.h>

#include "./aom_config.h"
#include "av1/common/blockd.h"
#include "av1/common/common.h"
#include "av1/common/enums.h"
#include "aom_dsp/inv_txfm.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO(kslu) move the common stuff in idct.h to av1_txfm.h or txfm_common.h
typedef void (*transform_1d)(const tran_low_t *, tran_low_t *);

typedef struct {
  transform_1d cols, rows;  // vertical and horizontal
} transform_2d;

#if CONFIG_HIGHBITDEPTH
typedef void (*highbd_transform_1d)(const tran_low_t *, tran_low_t *, int bd);

typedef struct {
  highbd_transform_1d cols, rows;  // vertical and horizontal
} highbd_transform_2d;
#endif  // CONFIG_HIGHBITDEPTH

#define MAX_TX_SCALE 1
int av1_get_tx_scale(const TX_SIZE tx_size);

void av1_iwht4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                     const TxfmParam *txfm_param);
void av1_idct4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                     const TxfmParam *txfm_param);

void av1_inv_txfm_add(const tran_low_t *input, uint8_t *dest, int stride,
                      TxfmParam *txfm_param);
void av1_inverse_transform_block(const MACROBLOCKD *xd,
                                 const tran_low_t *dqcoeff,
#if CONFIG_LGT
                                 PREDICTION_MODE mode,
#endif
                                 TX_TYPE tx_type, TX_SIZE tx_size, uint8_t *dst,
                                 int stride, int eob);
void av1_inverse_transform_block_facade(MACROBLOCKD *xd, int plane, int block,
                                        int blk_row, int blk_col, int eob);

void av1_highbd_iwht4x4_add(const tran_low_t *input, uint8_t *dest, int stride,
                            int eob, int bd);
void av1_highbd_inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, const TxfmParam *param);
void av1_highbd_inv_txfm_add_4x8(const tran_low_t *input, uint8_t *dest,
                                 int stride, const TxfmParam *param);
void av1_highbd_inv_txfm_add_8x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, const TxfmParam *param);
void av1_highbd_inv_txfm_add(const tran_low_t *input, uint8_t *dest, int stride,
                             TxfmParam *txfm_param);

#if CONFIG_DPCM_INTRA
void av1_dpcm_inv_txfm_add_4_c(const tran_low_t *input, int stride,
                               TX_TYPE_1D tx_type, uint8_t *dest);
void av1_dpcm_inv_txfm_add_8_c(const tran_low_t *input, int stride,
                               TX_TYPE_1D tx_type, uint8_t *dest);
void av1_dpcm_inv_txfm_add_16_c(const tran_low_t *input, int stride,
                                TX_TYPE_1D tx_type, uint8_t *dest);
void av1_dpcm_inv_txfm_add_32_c(const tran_low_t *input, int stride,
                                TX_TYPE_1D tx_type, uint8_t *dest);
typedef void (*dpcm_inv_txfm_add_func)(const tran_low_t *input, int stride,
                                       TX_TYPE_1D tx_type, uint8_t *dest);
dpcm_inv_txfm_add_func av1_get_dpcm_inv_txfm_add_func(int tx_length);
#if CONFIG_HIGHBITDEPTH
void av1_hbd_dpcm_inv_txfm_add_4_c(const tran_low_t *input, int stride,
                                   TX_TYPE_1D tx_type, int bd, uint16_t *dest,
                                   int dir);
void av1_hbd_dpcm_inv_txfm_add_8_c(const tran_low_t *input, int stride,
                                   TX_TYPE_1D tx_type, int bd, uint16_t *dest,
                                   int dir);
void av1_hbd_dpcm_inv_txfm_add_16_c(const tran_low_t *input, int stride,
                                    TX_TYPE_1D tx_type, int bd, uint16_t *dest,
                                    int dir);
void av1_hbd_dpcm_inv_txfm_add_32_c(const tran_low_t *input, int stride,
                                    TX_TYPE_1D tx_type, int bd, uint16_t *dest,
                                    int dir);
typedef void (*hbd_dpcm_inv_txfm_add_func)(const tran_low_t *input, int stride,
                                           TX_TYPE_1D tx_type, int bd,
                                           uint16_t *dest, int dir);
hbd_dpcm_inv_txfm_add_func av1_get_hbd_dpcm_inv_txfm_add_func(int tx_length);
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_DPCM_INTRA
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_IDCT_H_
