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

#include <math.h>

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_ports/mem.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/idct.h"

int av1_get_tx_scale(const TX_SIZE tx_size) {
  const int pels = tx_size_2d[tx_size];
  // Largest possible pels is 4096 (64x64).
  return (pels > 256) + (pels > 1024);
}

// NOTE: The implementation of all inverses need to be aware of the fact
// that input and output could be the same buffer.

// idct
static void highbd_iwht4x4_add(const tran_low_t *input, uint8_t *dest,
                               int stride, int eob, int bd) {
  if (eob > 1)
    av1_highbd_iwht4x4_16_add(input, dest, stride, bd);
  else
    av1_highbd_iwht4x4_1_add(input, dest, stride, bd);
}

static const int32_t *cast_to_int32(const tran_low_t *input) {
  assert(sizeof(int32_t) == sizeof(tran_low_t));
  return (const int32_t *)input;
}

void av1_highbd_inv_txfm_add_4x4(const tran_low_t *input, uint8_t *dest,
                                 int stride, const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  int eob = txfm_param->eob;
  int bd = txfm_param->bd;
  int lossless = txfm_param->lossless;
  const int32_t *src = cast_to_int32(input);
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (lossless) {
    assert(tx_type == DCT_DCT);
    highbd_iwht4x4_add(input, dest, stride, eob, bd);
    return;
  }
  switch (tx_type) {
    // Assembly version doesn't support some transform types, so use C version
    // for those.
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_4x4_c(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
    default:
      av1_inv_txfm2d_add_4x4(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
  }
}

static void highbd_inv_txfm_add_4x8(const tran_low_t *input, uint8_t *dest,
                                    int stride, const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_4x8(src, CONVERT_TO_SHORTPTR(dest), stride,
                         txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_8x4(const tran_low_t *input, uint8_t *dest,
                                    int stride, const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_8x4(src, CONVERT_TO_SHORTPTR(dest), stride,
                         txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_8x16(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_8x16(src, CONVERT_TO_SHORTPTR(dest), stride,
                          txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x8(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x8(src, CONVERT_TO_SHORTPTR(dest), stride,
                          txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x32(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_32x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_32x16(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x4(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x4(src, CONVERT_TO_SHORTPTR(dest), stride,
                          txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_4x16(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_4x16(src, CONVERT_TO_SHORTPTR(dest), stride,
                          txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_32x8(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_32x8(src, CONVERT_TO_SHORTPTR(dest), stride,
                          txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_8x32(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_8x32(src, CONVERT_TO_SHORTPTR(dest), stride,
                          txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_32x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_32x64(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_64x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_64x32(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_16x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_16x64(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_64x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int32_t *src = cast_to_int32(input);
  av1_inv_txfm2d_add_64x16(src, CONVERT_TO_SHORTPTR(dest), stride,
                           txfm_param->tx_type, txfm_param->bd);
}

static void highbd_inv_txfm_add_8x8(const tran_low_t *input, uint8_t *dest,
                                    int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    // Assembly version doesn't support some transform types, so use C version
    // for those.
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_8x8_c(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
    default:
      av1_inv_txfm2d_add_8x8(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                             bd);
      break;
  }
}

static void highbd_inv_txfm_add_16x16(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    // Assembly version doesn't support some transform types, so use C version
    // for those.
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_inv_txfm2d_add_16x16_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 tx_type, bd);
      break;
    default:
      av1_inv_txfm2d_add_16x16(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
  }
}

static void highbd_inv_txfm_add_32x32(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case DCT_DCT:
      av1_inv_txfm2d_add_32x32(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                               bd);
      break;
    // Assembly version doesn't support IDTX, so use C version for it.
    case IDTX:
      av1_inv_txfm2d_add_32x32_c(src, CONVERT_TO_SHORTPTR(dest), stride,
                                 tx_type, bd);
      break;

    default: assert(0);
  }
}

static void highbd_inv_txfm_add_64x64(const tran_low_t *input, uint8_t *dest,
                                      int stride, const TxfmParam *txfm_param) {
  const int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  assert(tx_type == DCT_DCT);
  av1_inv_txfm2d_add_64x64(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type, bd);
}

static void init_txfm_param(const MACROBLOCKD *xd, int plane, TX_SIZE tx_size,
                            TX_TYPE tx_type, int eob, int reduced_tx_set,
                            TxfmParam *txfm_param) {
  (void)plane;
  txfm_param->tx_type = tx_type;
  txfm_param->tx_size = tx_size;
  txfm_param->eob = eob;
  txfm_param->lossless = xd->lossless[xd->mi[0]->segment_id];
  txfm_param->bd = xd->bd;
  txfm_param->is_hbd = get_bitdepth_data_path_index(xd);
  txfm_param->tx_set_type = av1_get_ext_tx_set_type(
      txfm_param->tx_size, is_inter_block(xd->mi[0]), reduced_tx_set);
}

static void highbd_inv_txfm_add(const tran_low_t *input, uint8_t *dest,
                                int stride, const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const TX_SIZE tx_size = txfm_param->tx_size;
  switch (tx_size) {
    case TX_32X32:
      highbd_inv_txfm_add_32x32(input, dest, stride, txfm_param);
      break;
    case TX_16X16:
      highbd_inv_txfm_add_16x16(input, dest, stride, txfm_param);
      break;
    case TX_8X8:
      highbd_inv_txfm_add_8x8(input, dest, stride, txfm_param);
      break;
    case TX_4X8:
      highbd_inv_txfm_add_4x8(input, dest, stride, txfm_param);
      break;
    case TX_8X4:
      highbd_inv_txfm_add_8x4(input, dest, stride, txfm_param);
      break;
    case TX_8X16:
      highbd_inv_txfm_add_8x16(input, dest, stride, txfm_param);
      break;
    case TX_16X8:
      highbd_inv_txfm_add_16x8(input, dest, stride, txfm_param);
      break;
    case TX_16X32:
      highbd_inv_txfm_add_16x32(input, dest, stride, txfm_param);
      break;
    case TX_32X16:
      highbd_inv_txfm_add_32x16(input, dest, stride, txfm_param);
      break;
    case TX_64X64:
      highbd_inv_txfm_add_64x64(input, dest, stride, txfm_param);
      break;
    case TX_32X64:
      highbd_inv_txfm_add_32x64(input, dest, stride, txfm_param);
      break;
    case TX_64X32:
      highbd_inv_txfm_add_64x32(input, dest, stride, txfm_param);
      break;
    case TX_16X64:
      highbd_inv_txfm_add_16x64(input, dest, stride, txfm_param);
      break;
    case TX_64X16:
      highbd_inv_txfm_add_64x16(input, dest, stride, txfm_param);
      break;
    case TX_4X4:
      // this is like av1_short_idct4x4 but has a special case around eob<=1
      // which is significant (not just an optimization) for the lossless
      // case.
      av1_highbd_inv_txfm_add_4x4(input, dest, stride, txfm_param);
      break;
    case TX_16X4:
      highbd_inv_txfm_add_16x4(input, dest, stride, txfm_param);
      break;
    case TX_4X16:
      highbd_inv_txfm_add_4x16(input, dest, stride, txfm_param);
      break;
    case TX_8X32:
      highbd_inv_txfm_add_8x32(input, dest, stride, txfm_param);
      break;
    case TX_32X8:
      highbd_inv_txfm_add_32x8(input, dest, stride, txfm_param);
      break;
    default: assert(0 && "Invalid transform size"); break;
  }
}

void av1_inv_txfm_add_c(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                        const TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  DECLARE_ALIGNED(32, uint16_t, tmp[MAX_TX_SQUARE]);
  int tmp_stride = MAX_TX_SIZE;
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      tmp[r * tmp_stride + c] = dst[r * stride + c];
    }
  }

  highbd_inv_txfm_add(dqcoeff, CONVERT_TO_BYTEPTR(tmp), tmp_stride, txfm_param);

  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      dst[r * stride + c] = (uint8_t)tmp[r * tmp_stride + c];
    }
  }
}

void av1_inverse_transform_block(const MACROBLOCKD *xd,
                                 const tran_low_t *dqcoeff, int plane,
                                 TX_TYPE tx_type, TX_SIZE tx_size, uint8_t *dst,
                                 int stride, int eob, int reduced_tx_set) {
  if (!eob) return;

  assert(eob <= av1_get_max_eob(tx_size));

  TxfmParam txfm_param;
  init_txfm_param(xd, plane, tx_size, tx_type, eob, reduced_tx_set,
                  &txfm_param);
  assert(av1_ext_tx_used[txfm_param.tx_set_type][txfm_param.tx_type]);

  if (txfm_param.is_hbd) {
    highbd_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
  } else {
    av1_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
  }
}
