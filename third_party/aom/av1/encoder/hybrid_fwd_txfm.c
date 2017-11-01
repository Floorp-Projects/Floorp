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

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "av1/common/idct.h"
#include "av1/encoder/hybrid_fwd_txfm.h"

#if CONFIG_CHROMA_2X2
static void fwd_txfm_2x2(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TxfmParam *txfm_param) {
  tran_high_t a1 = src_diff[0];
  tran_high_t b1 = src_diff[1];
  tran_high_t c1 = src_diff[diff_stride];
  tran_high_t d1 = src_diff[1 + diff_stride];

  tran_high_t a2 = a1 + c1;
  tran_high_t b2 = b1 + d1;
  tran_high_t c2 = a1 - c1;
  tran_high_t d2 = b1 - d1;

  a1 = a2 + b2;
  b1 = a2 - b2;
  c1 = c2 + d2;
  d1 = c2 - d2;

  coeff[0] = (tran_low_t)(4 * a1);
  coeff[1] = (tran_low_t)(4 * b1);
  coeff[2] = (tran_low_t)(4 * c1);
  coeff[3] = (tran_low_t)(4 * d1);

  (void)txfm_param;
}
#endif

static void fwd_txfm_4x4(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TxfmParam *txfm_param) {
  if (txfm_param->lossless) {
    assert(txfm_param->tx_type == DCT_DCT);
    av1_fwht4x4(src_diff, coeff, diff_stride);
    return;
  }

#if CONFIG_LGT || CONFIG_DAALA_DCT4
  // only C version has LGTs
  av1_fht4x4_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht4x4(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_4x8(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_fht4x8_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht4x8(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_8x4(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_fht8x4_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht8x4(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_8x16(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_fht8x16_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht8x16(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_16x8(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_fht16x8_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht16x8(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_16x32(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TxfmParam *txfm_param) {
  av1_fht16x32(src_diff, coeff, diff_stride, txfm_param);
}

static void fwd_txfm_32x16(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TxfmParam *txfm_param) {
  av1_fht32x16(src_diff, coeff, diff_stride, txfm_param);
}

static void fwd_txfm_8x8(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT || CONFIG_DAALA_DCT8
  av1_fht8x8_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht8x8(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_16x16(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_DAALA_DCT16
  av1_fht16x16_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht16x16(src_diff, coeff, diff_stride, txfm_param);
#endif  // CONFIG_DAALA_DCT16
}

static void fwd_txfm_32x32(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_MRC_TX
  // MRC_DCT currently only has a C implementation
  if (txfm_param->tx_type == MRC_DCT) {
    av1_fht32x32_c(src_diff, coeff, diff_stride, txfm_param);
    return;
  }
#endif  // CONFIG_MRC_TX
  av1_fht32x32(src_diff, coeff, diff_stride, txfm_param);
}

#if CONFIG_TX64X64
static void fwd_txfm_64x64(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_EXT_TX
  if (txfm_param->tx_type == IDTX)
    av1_fwd_idtx_c(src_diff, coeff, diff_stride, 64, 64, txfm_param->tx_type);
  else
#endif
    av1_fht64x64(src_diff, coeff, diff_stride, txfm_param);
}

static void fwd_txfm_32x64(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_EXT_TX
  if (txfm_param->tx_type == IDTX)
    av1_fwd_idtx_c(src_diff, coeff, diff_stride, 32, 64, txfm_param->tx_type);
  else
#endif
    av1_fht32x64(src_diff, coeff, diff_stride, txfm_param);
}

static void fwd_txfm_64x32(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_EXT_TX
  if (txfm_param->tx_type == IDTX)
    av1_fwd_idtx_c(src_diff, coeff, diff_stride, 64, 32, txfm_param->tx_type);
  else
#endif
    av1_fht64x32(src_diff, coeff, diff_stride, txfm_param);
}
#endif  // CONFIG_TX64X64

#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
static void fwd_txfm_16x4(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_fht16x4_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht16x4(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_4x16(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_fht4x16_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht4x16(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_32x8(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_fht32x8_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht32x8(src_diff, coeff, diff_stride, txfm_param);
#endif
}

static void fwd_txfm_8x32(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TxfmParam *txfm_param) {
#if CONFIG_LGT
  av1_fht8x32_c(src_diff, coeff, diff_stride, txfm_param);
#else
  av1_fht8x32(src_diff, coeff, diff_stride, txfm_param);
#endif
}
#endif

#if CONFIG_CHROMA_2X2
static void highbd_fwd_txfm_2x2(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TxfmParam *txfm_param) {
  tran_high_t a1 = src_diff[0];
  tran_high_t b1 = src_diff[1];
  tran_high_t c1 = src_diff[diff_stride];
  tran_high_t d1 = src_diff[1 + diff_stride];

  tran_high_t a2 = a1 + c1;
  tran_high_t b2 = b1 + d1;
  tran_high_t c2 = a1 - c1;
  tran_high_t d2 = b1 - d1;

  a1 = a2 + b2;
  b1 = a2 - b2;
  c1 = c2 + d2;
  d1 = c2 - d2;

  coeff[0] = (tran_low_t)(4 * a1);
  coeff[1] = (tran_low_t)(4 * b1);
  coeff[2] = (tran_low_t)(4 * c1);
  coeff[3] = (tran_low_t)(4 * d1);

  (void)txfm_param;
}
#endif

static void highbd_fwd_txfm_4x4(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  if (txfm_param->lossless) {
    assert(tx_type == DCT_DCT);
    av1_highbd_fwht4x4(src_diff, coeff, diff_stride);
    return;
  }
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      // fallthrough intended
      av1_fwd_txfm2d_4x4(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      // fallthrough intended
      av1_fwd_txfm2d_4x4(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      // fallthrough intended
      av1_fwd_txfm2d_4x4_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static void highbd_fwd_txfm_4x8(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_4x8_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                       txfm_param->bd);
}

static void highbd_fwd_txfm_8x4(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_8x4_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                       txfm_param->bd);
}

static void highbd_fwd_txfm_8x16(const int16_t *src_diff, tran_low_t *coeff,
                                 int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_8x16_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                        txfm_param->bd);
}

static void highbd_fwd_txfm_16x8(const int16_t *src_diff, tran_low_t *coeff,
                                 int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_16x8_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                        txfm_param->bd);
}

static void highbd_fwd_txfm_16x32(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_16x32_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                         txfm_param->bd);
}

static void highbd_fwd_txfm_32x16(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_32x16_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                         txfm_param->bd);
}

static void highbd_fwd_txfm_8x8(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      // fallthrough intended
      av1_fwd_txfm2d_8x8(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      // fallthrough intended
      av1_fwd_txfm2d_8x8(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      // fallthrough intended
      av1_fwd_txfm2d_8x8_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static void highbd_fwd_txfm_16x16(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      // fallthrough intended
      av1_fwd_txfm2d_16x16(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      // fallthrough intended
      av1_fwd_txfm2d_16x16(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      // fallthrough intended
      av1_fwd_txfm2d_16x16_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static void highbd_fwd_txfm_32x32(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      // fallthrough intended
      av1_fwd_txfm2d_32x32(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      // fallthrough intended
      av1_fwd_txfm2d_32x32(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      // fallthrough intended
      av1_fwd_txfm2d_32x32_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

#if CONFIG_TX64X64
static void highbd_fwd_txfm_32x64(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    case DCT_DCT:
      av1_fwd_txfm2d_32x64_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // TODO(sarahparker)
      // I've deleted the 64x64 implementations that existed in lieu
      // of adst, flipadst and identity for simplicity but will bring back
      // in a later change. This shouldn't impact performance since
      // DCT_DCT is the only extended type currently allowed for 64x64,
      // as dictated by get_ext_tx_set_type in blockd.h.
      av1_fwd_txfm2d_32x64_c(src_diff, dst_coeff, diff_stride, DCT_DCT, bd);
      break;
    case IDTX:
      av1_fwd_idtx_c(src_diff, dst_coeff, diff_stride, 32, 64, tx_type);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

static void highbd_fwd_txfm_64x32(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    case DCT_DCT:
      av1_fwd_txfm2d_64x32_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // TODO(sarahparker)
      // I've deleted the 64x64 implementations that existed in lieu
      // of adst, flipadst and identity for simplicity but will bring back
      // in a later change. This shouldn't impact performance since
      // DCT_DCT is the only extended type currently allowed for 64x64,
      // as dictated by get_ext_tx_set_type in blockd.h.
      av1_fwd_txfm2d_64x32_c(src_diff, dst_coeff, diff_stride, DCT_DCT, bd);
      break;
    case IDTX:
      av1_fwd_idtx_c(src_diff, dst_coeff, diff_stride, 64, 32, tx_type);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}
static void highbd_fwd_txfm_64x64(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    case DCT_DCT:
      av1_fwd_txfm2d_64x64(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // TODO(sarahparker)
      // I've deleted the 64x64 implementations that existed in lieu
      // of adst, flipadst and identity for simplicity but will bring back
      // in a later change. This shouldn't impact performance since
      // DCT_DCT is the only extended type currently allowed for 64x64,
      // as dictated by get_ext_tx_set_type in blockd.h.
      av1_fwd_txfm2d_64x64_c(src_diff, dst_coeff, diff_stride, DCT_DCT, bd);
      break;
    case IDTX:
      av1_fwd_idtx_c(src_diff, dst_coeff, diff_stride, 64, 64, tx_type);
      break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}
#endif  // CONFIG_TX64X64

void av1_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff, int diff_stride,
                  TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
#if CONFIG_LGT_FROM_PRED
  if (txfm_param->use_lgt) {
    // if use_lgt is 1, it will override tx_type
    assert(is_lgt_allowed(txfm_param->mode, tx_size));
    flgt2d_from_pred_c(src_diff, coeff, diff_stride, txfm_param);
    return;
  }
#endif  // CONFIG_LGT_FROM_PRED
  switch (tx_size) {
#if CONFIG_TX64X64
    case TX_64X64:
      fwd_txfm_64x64(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_32X64:
      fwd_txfm_32x64(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_64X32:
      fwd_txfm_64x32(src_diff, coeff, diff_stride, txfm_param);
      break;
#endif  // CONFIG_TX64X64
    case TX_32X32:
      fwd_txfm_32x32(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X16:
      fwd_txfm_16x16(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_8X8: fwd_txfm_8x8(src_diff, coeff, diff_stride, txfm_param); break;
    case TX_4X8: fwd_txfm_4x8(src_diff, coeff, diff_stride, txfm_param); break;
    case TX_8X4: fwd_txfm_8x4(src_diff, coeff, diff_stride, txfm_param); break;
    case TX_8X16:
      fwd_txfm_8x16(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X8:
      fwd_txfm_16x8(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X32:
      fwd_txfm_16x32(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_32X16:
      fwd_txfm_32x16(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_4X4: fwd_txfm_4x4(src_diff, coeff, diff_stride, txfm_param); break;
#if CONFIG_CHROMA_2X2
    case TX_2X2: fwd_txfm_2x2(src_diff, coeff, diff_stride, txfm_param); break;
#endif
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
    case TX_4X16:
      fwd_txfm_4x16(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X4:
      fwd_txfm_16x4(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_8X32:
      fwd_txfm_8x32(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_32X8:
      fwd_txfm_32x8(src_diff, coeff, diff_stride, txfm_param);
      break;
#endif
    default: assert(0); break;
  }
}

void av1_highbd_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  switch (tx_size) {
#if CONFIG_TX64X64
    case TX_64X64:
      highbd_fwd_txfm_64x64(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_32X64:
      highbd_fwd_txfm_32x64(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_64X32:
      highbd_fwd_txfm_64x32(src_diff, coeff, diff_stride, txfm_param);
      break;
#endif  // CONFIG_TX64X64
    case TX_32X32:
      highbd_fwd_txfm_32x32(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X16:
      highbd_fwd_txfm_16x16(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_8X8:
      highbd_fwd_txfm_8x8(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_4X8:
      highbd_fwd_txfm_4x8(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_8X4:
      highbd_fwd_txfm_8x4(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_8X16:
      highbd_fwd_txfm_8x16(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X8:
      highbd_fwd_txfm_16x8(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X32:
      highbd_fwd_txfm_16x32(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_32X16:
      highbd_fwd_txfm_32x16(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_4X4:
      highbd_fwd_txfm_4x4(src_diff, coeff, diff_stride, txfm_param);
      break;
#if CONFIG_CHROMA_2X2
    case TX_2X2:
      highbd_fwd_txfm_2x2(src_diff, coeff, diff_stride, txfm_param);
      break;
#endif
    default: assert(0); break;
  }
}
