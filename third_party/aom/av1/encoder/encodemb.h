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

#ifndef AV1_ENCODER_ENCODEMB_H_
#define AV1_ENCODER_ENCODEMB_H_

#include "./aom_config.h"
#include "av1/common/onyxc_int.h"
#include "av1/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

struct optimize_ctx {
  ENTROPY_CONTEXT ta[MAX_MB_PLANE][2 * MAX_MIB_SIZE];
  ENTROPY_CONTEXT tl[MAX_MB_PLANE][2 * MAX_MIB_SIZE];
};

struct encode_b_args {
  AV1_COMMON *cm;
  MACROBLOCK *x;
  struct optimize_ctx *ctx;
  int8_t *skip;
  ENTROPY_CONTEXT *ta;
  ENTROPY_CONTEXT *tl;
  int8_t enable_optimize_b;
};

typedef enum AV1_XFORM_QUANT {
  AV1_XFORM_QUANT_FP = 0,
  AV1_XFORM_QUANT_B = 1,
  AV1_XFORM_QUANT_DC = 2,
  AV1_XFORM_QUANT_SKIP_QUANT,
  AV1_XFORM_QUANT_TYPES,
} AV1_XFORM_QUANT;

void av1_encode_sb(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize, int mi_row,
                   int mi_col);
#if CONFIG_SUPERTX
void av1_encode_sb_supertx(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
#endif  // CONFIG_SUPERTX
void av1_encode_sby_pass1(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
void av1_xform_quant(const AV1_COMMON *cm, MACROBLOCK *x, int plane, int block,
                     int blk_row, int blk_col, BLOCK_SIZE plane_bsize,
                     TX_SIZE tx_size, int ctx, AV1_XFORM_QUANT xform_quant_idx);

int av1_optimize_b(const AV1_COMMON *cm, MACROBLOCK *mb, int plane, int blk_row,
                   int blk_col, int block, BLOCK_SIZE plane_bsize,
                   TX_SIZE tx_size, const ENTROPY_CONTEXT *a,
                   const ENTROPY_CONTEXT *l);

void av1_subtract_txb(MACROBLOCK *x, int plane, BLOCK_SIZE plane_bsize,
                      int blk_col, int blk_row, TX_SIZE tx_size);

void av1_subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane);

void av1_set_txb_context(MACROBLOCK *x, int plane, int block, TX_SIZE tx_size,
                         ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l);

void av1_encode_block_intra(int plane, int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg);

void av1_encode_intra_block_plane(AV1_COMMON *cm, MACROBLOCK *x,
                                  BLOCK_SIZE bsize, int plane,
                                  int enable_optimize_b, int mi_row,
                                  int mi_col);

#if CONFIG_PVQ
PVQ_SKIP_TYPE av1_pvq_encode_helper(MACROBLOCK *x, tran_low_t *const coeff,
                                    tran_low_t *ref_coeff,
                                    tran_low_t *const dqcoeff, uint16_t *eob,
                                    const int16_t *quant, int plane,
                                    int tx_size, TX_TYPE tx_type, int *rate,
                                    int speed, PVQ_INFO *pvq_info);

void av1_store_pvq_enc_info(PVQ_INFO *pvq_info, int *qg, int *theta, int *k,
                            od_coeff *y, int nb_bands, const int *off,
                            int *size, int skip_rest, int skip_dir, int bs);
#endif

#if CONFIG_DPCM_INTRA
void av1_encode_block_intra_dpcm(const AV1_COMMON *cm, MACROBLOCK *x,
                                 PREDICTION_MODE mode, int plane, int block,
                                 int blk_row, int blk_col,
                                 BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                                 TX_TYPE tx_type, ENTROPY_CONTEXT *ta,
                                 ENTROPY_CONTEXT *tl, int8_t *skip);
#endif  // CONFIG_DPCM_INTRA

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_ENCODEMB_H_
