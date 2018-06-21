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

#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "config/aom_dsp_rtcd.h"

#include "av1/common/idct.h"
#include "av1/encoder/hybrid_fwd_txfm.h"

/* 4-point reversible, orthonormal Walsh-Hadamard in 3.5 adds, 0.5 shifts per
   pixel. */
void av1_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride) {
  int i;
  tran_high_t a1, b1, c1, d1, e1;
  const int16_t *ip_pass0 = input;
  const tran_low_t *ip = NULL;
  tran_low_t *op = output;

  for (i = 0; i < 4; i++) {
    a1 = ip_pass0[0 * stride];
    b1 = ip_pass0[1 * stride];
    c1 = ip_pass0[2 * stride];
    d1 = ip_pass0[3 * stride];

    a1 += b1;
    d1 = d1 - c1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= c1;
    d1 += b1;
    op[0] = (tran_low_t)a1;
    op[4] = (tran_low_t)c1;
    op[8] = (tran_low_t)d1;
    op[12] = (tran_low_t)b1;

    ip_pass0++;
    op++;
  }
  ip = output;
  op = output;

  for (i = 0; i < 4; i++) {
    a1 = ip[0];
    b1 = ip[1];
    c1 = ip[2];
    d1 = ip[3];

    a1 += b1;
    d1 -= c1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= c1;
    d1 += b1;
    op[0] = (tran_low_t)(a1 * UNIT_QUANT_FACTOR);
    op[1] = (tran_low_t)(c1 * UNIT_QUANT_FACTOR);
    op[2] = (tran_low_t)(d1 * UNIT_QUANT_FACTOR);
    op[3] = (tran_low_t)(b1 * UNIT_QUANT_FACTOR);

    ip += 4;
    op += 4;
  }
}

void av1_highbd_fwht4x4_c(const int16_t *input, tran_low_t *output,
                          int stride) {
  av1_fwht4x4_c(input, output, stride);
}

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
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_fwd_txfm2d_4x4_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
    default:
      av1_fwd_txfm2d_4x4(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
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

static void highbd_fwd_txfm_16x4(const int16_t *src_diff, tran_low_t *coeff,
                                 int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_16x4_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                        txfm_param->bd);
}

static void highbd_fwd_txfm_4x16(const int16_t *src_diff, tran_low_t *coeff,
                                 int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_4x16_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                        txfm_param->bd);
}

static void highbd_fwd_txfm_32x8(const int16_t *src_diff, tran_low_t *coeff,
                                 int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_32x8_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                        txfm_param->bd);
}

static void highbd_fwd_txfm_8x32(const int16_t *src_diff, tran_low_t *coeff,
                                 int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  av1_fwd_txfm2d_8x32_c(src_diff, dst_coeff, diff_stride, txfm_param->tx_type,
                        txfm_param->bd);
}

static void highbd_fwd_txfm_8x8(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_fwd_txfm2d_8x8_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
    default:
      av1_fwd_txfm2d_8x8(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
  }
}

static void highbd_fwd_txfm_16x16(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_fwd_txfm2d_16x16_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
    default:
      av1_fwd_txfm2d_16x16(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
  }
}

static void highbd_fwd_txfm_32x32(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  int32_t *dst_coeff = (int32_t *)coeff;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int bd = txfm_param->bd;
  switch (tx_type) {
    // use the c version for anything including identity for now
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
    case IDTX:
      av1_fwd_txfm2d_32x32_c(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
    default:
      av1_fwd_txfm2d_32x32(src_diff, dst_coeff, diff_stride, tx_type, bd);
      break;
  }
}

static void highbd_fwd_txfm_32x64(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  assert(txfm_param->tx_type == DCT_DCT);
  int32_t *dst_coeff = (int32_t *)coeff;
  const int bd = txfm_param->bd;
  av1_fwd_txfm2d_32x64_c(src_diff, dst_coeff, diff_stride, DCT_DCT, bd);
}

static void highbd_fwd_txfm_64x32(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  assert(txfm_param->tx_type == DCT_DCT);
  int32_t *dst_coeff = (int32_t *)coeff;
  const int bd = txfm_param->bd;
  av1_fwd_txfm2d_64x32_c(src_diff, dst_coeff, diff_stride, DCT_DCT, bd);
}

static void highbd_fwd_txfm_16x64(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  assert(txfm_param->tx_type == DCT_DCT);
  int32_t *dst_coeff = (int32_t *)coeff;
  const int bd = txfm_param->bd;
  av1_fwd_txfm2d_16x64_c(src_diff, dst_coeff, diff_stride, DCT_DCT, bd);
}

static void highbd_fwd_txfm_64x16(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  assert(txfm_param->tx_type == DCT_DCT);
  int32_t *dst_coeff = (int32_t *)coeff;
  const int bd = txfm_param->bd;
  av1_fwd_txfm2d_64x16_c(src_diff, dst_coeff, diff_stride, DCT_DCT, bd);
}

static void highbd_fwd_txfm_64x64(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TxfmParam *txfm_param) {
  assert(txfm_param->tx_type == DCT_DCT);
  int32_t *dst_coeff = (int32_t *)coeff;
  const int bd = txfm_param->bd;
  av1_fwd_txfm2d_64x64(src_diff, dst_coeff, diff_stride, DCT_DCT, bd);
}

void av1_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff, int diff_stride,
                  TxfmParam *txfm_param) {
  if (txfm_param->bd == 8)
    av1_lowbd_fwd_txfm(src_diff, coeff, diff_stride, txfm_param);
  else
    av1_highbd_fwd_txfm(src_diff, coeff, diff_stride, txfm_param);
}

void av1_lowbd_fwd_txfm_c(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TxfmParam *txfm_param) {
  av1_highbd_fwd_txfm(src_diff, coeff, diff_stride, txfm_param);
}

void av1_highbd_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const TX_SIZE tx_size = txfm_param->tx_size;
  switch (tx_size) {
    case TX_64X64:
      highbd_fwd_txfm_64x64(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_32X64:
      highbd_fwd_txfm_32x64(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_64X32:
      highbd_fwd_txfm_64x32(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X64:
      highbd_fwd_txfm_16x64(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_64X16:
      highbd_fwd_txfm_64x16(src_diff, coeff, diff_stride, txfm_param);
      break;
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
    case TX_4X16:
      highbd_fwd_txfm_4x16(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_16X4:
      highbd_fwd_txfm_16x4(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_8X32:
      highbd_fwd_txfm_8x32(src_diff, coeff, diff_stride, txfm_param);
      break;
    case TX_32X8:
      highbd_fwd_txfm_32x8(src_diff, coeff, diff_stride, txfm_param);
      break;
    default: assert(0); break;
  }
}
