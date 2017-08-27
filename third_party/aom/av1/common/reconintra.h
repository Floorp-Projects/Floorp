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

#ifndef AV1_COMMON_RECONINTRA_H_
#define AV1_COMMON_RECONINTRA_H_

#include "aom/aom_integer.h"
#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_DPCM_INTRA
static INLINE int av1_use_dpcm_intra(int plane, PREDICTION_MODE mode,
                                     TX_TYPE tx_type,
                                     const MB_MODE_INFO *const mbmi) {
  (void)mbmi;
  (void)plane;
#if CONFIG_EXT_INTRA
  if (mbmi->sb_type >= BLOCK_8X8 && mbmi->angle_delta[plane != 0]) return 0;
#endif  // CONFIG_EXT_INTRA
  return (mode == V_PRED && (tx_type == IDTX || tx_type == H_DCT)) ||
         (mode == H_PRED && (tx_type == IDTX || tx_type == V_DCT));
}
#endif  // CONFIG_DPCM_INTRA

void av1_init_intra_predictors(void);
void av1_predict_intra_block_facade(MACROBLOCKD *xd, int plane, int block_idx,
                                    int blk_col, int blk_row, TX_SIZE tx_size);
void av1_predict_intra_block(const MACROBLOCKD *xd, int bw, int bh,
                             BLOCK_SIZE bsize, PREDICTION_MODE mode,
                             const uint8_t *ref, int ref_stride, uint8_t *dst,
                             int dst_stride, int aoff, int loff, int plane);

#if CONFIG_EXT_INTER && CONFIG_INTERINTRA
// Mapping of interintra to intra mode for use in the intra component
static const PREDICTION_MODE interintra_to_intra_mode[INTERINTRA_MODES] = {
  DC_PRED, V_PRED, H_PRED,
#if CONFIG_ALT_INTRA
  SMOOTH_PRED
#else
  TM_PRED
#endif
};

// Mapping of intra mode to the interintra mode
static const INTERINTRA_MODE intra_to_interintra_mode[INTRA_MODES] = {
  II_DC_PRED,     II_V_PRED,     II_H_PRED, II_V_PRED,
#if CONFIG_ALT_INTRA
  II_SMOOTH_PRED,
#else
  II_TM_PRED,
#endif
  II_V_PRED,      II_H_PRED,     II_H_PRED, II_V_PRED,
#if CONFIG_ALT_INTRA
  II_SMOOTH_PRED, II_SMOOTH_PRED
#else
  II_TM_PRED
#endif
};
#endif  // CONFIG_EXT_INTER && CONFIG_INTERINTRA

#if CONFIG_FILTER_INTRA
#define FILTER_INTRA_PREC_BITS 10
#endif  // CONFIG_FILTER_INTRA

#define CONFIG_INTRA_EDGE_UPSAMPLE CONFIG_INTRA_EDGE
#define CONFIG_USE_ANGLE_DELTA_SUB8X8 0

#if CONFIG_EXT_INTRA
static INLINE int av1_is_directional_mode(PREDICTION_MODE mode,
                                          BLOCK_SIZE bsize) {
#if CONFIG_INTRA_EDGE_UPSAMPLE
  (void)bsize;
  return mode >= V_PRED && mode <= D63_PRED;
#else
  return mode >= V_PRED && mode <= D63_PRED && bsize >= BLOCK_8X8;
#endif
}

static INLINE int av1_use_angle_delta(BLOCK_SIZE bsize) {
  (void)bsize;
#if CONFIG_USE_ANGLE_DELTA_SUB8X8
  return 1;
#else
  return bsize >= BLOCK_8X8;
#endif
}
#endif  // CONFIG_EXT_INTRA

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AV1_COMMON_RECONINTRA_H_
