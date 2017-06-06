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
                         int diff_stride, TX_TYPE tx_type, int lossless) {
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

  (void)tx_type;
  (void)lossless;
}
#endif

static void fwd_txfm_4x4(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TX_TYPE tx_type, int lossless) {
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_fwht4x4(src_diff, coeff, diff_stride);
    return;
  }

  av1_fht4x4(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_4x8(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TX_TYPE tx_type,
                         FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht4x8(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_8x4(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TX_TYPE tx_type,
                         FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht8x4(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_8x16(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TX_TYPE tx_type,
                          FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht8x16(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_16x8(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TX_TYPE tx_type,
                          FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht16x8(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_16x32(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TX_TYPE tx_type,
                           FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht16x32(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_32x16(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TX_TYPE tx_type,
                           FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht32x16(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_8x8(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TX_TYPE tx_type,
                         FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht8x8(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_16x16(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TX_TYPE tx_type,
                           FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht16x16(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_32x32(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TX_TYPE tx_type,
                           FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht32x32(src_diff, coeff, diff_stride, tx_type);
}

#if CONFIG_TX64X64
static void fwd_txfm_64x64(const int16_t *src_diff, tran_low_t *coeff,
                           int diff_stride, TX_TYPE tx_type,
                           FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
#if CONFIG_EXT_TX
  if (tx_type == IDTX)
    av1_fwd_idtx_c(src_diff, coeff, diff_stride, 64, tx_type);
  else
#endif
    av1_fht64x64(src_diff, coeff, diff_stride, tx_type);
}
#endif  // CONFIG_TX64X64

#if CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT
static void fwd_txfm_16x4(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TX_TYPE tx_type,
                          FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht16x4(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_4x16(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TX_TYPE tx_type,
                          FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht4x16(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_32x8(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TX_TYPE tx_type,
                          FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht32x8(src_diff, coeff, diff_stride, tx_type);
}

static void fwd_txfm_8x32(const int16_t *src_diff, tran_low_t *coeff,
                          int diff_stride, TX_TYPE tx_type,
                          FWD_TXFM_OPT fwd_txfm_opt) {
  (void)fwd_txfm_opt;
  av1_fht8x32(src_diff, coeff, diff_stride, tx_type);
}
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT

#if CONFIG_HIGHBITDEPTH
#if CONFIG_CHROMA_2X2
static void highbd_fwd_txfm_2x2(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TX_TYPE tx_type, int lossless,
                                const int bd) {
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

  (void)tx_type;
  (void)lossless;
  (void)bd;
}
#endif

static void highbd_fwd_txfm_4x4(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TX_TYPE tx_type, int lossless,
                                const int bd) {
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_highbd_fwht4x4(src_diff, coeff, diff_stride);
    return;
  }

  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_fwd_txfm2d_4x4(src_diff, coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_fwd_txfm2d_4x4(src_diff, coeff, diff_stride, tx_type, bd);
      break;
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      av1_highbd_fht4x4_c(src_diff, coeff, diff_stride, tx_type);
      break;
    case IDTX: av1_fwd_idtx_c(src_diff, coeff, diff_stride, 4, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static void highbd_fwd_txfm_4x8(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TX_TYPE tx_type,
                                FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  (void)bd;
  av1_highbd_fht4x8(src_diff, coeff, diff_stride, tx_type);
}

static void highbd_fwd_txfm_8x4(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TX_TYPE tx_type,
                                FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  (void)bd;
  av1_highbd_fht8x4(src_diff, coeff, diff_stride, tx_type);
}

static void highbd_fwd_txfm_8x16(const int16_t *src_diff, tran_low_t *coeff,
                                 int diff_stride, TX_TYPE tx_type,
                                 FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  (void)bd;
  av1_highbd_fht8x16(src_diff, coeff, diff_stride, tx_type);
}

static void highbd_fwd_txfm_16x8(const int16_t *src_diff, tran_low_t *coeff,
                                 int diff_stride, TX_TYPE tx_type,
                                 FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  (void)bd;
  av1_highbd_fht16x8(src_diff, coeff, diff_stride, tx_type);
}

static void highbd_fwd_txfm_16x32(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TX_TYPE tx_type,
                                  FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  (void)bd;
  av1_highbd_fht16x32(src_diff, coeff, diff_stride, tx_type);
}

static void highbd_fwd_txfm_32x16(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TX_TYPE tx_type,
                                  FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  (void)bd;
  av1_highbd_fht32x16(src_diff, coeff, diff_stride, tx_type);
}

static void highbd_fwd_txfm_8x8(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TX_TYPE tx_type,
                                FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_fwd_txfm2d_8x8(src_diff, coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_fwd_txfm2d_8x8(src_diff, coeff, diff_stride, tx_type, bd);
      break;
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST exists only in C
      av1_highbd_fht8x8_c(src_diff, coeff, diff_stride, tx_type);
      break;
    case IDTX: av1_fwd_idtx_c(src_diff, coeff, diff_stride, 8, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static void highbd_fwd_txfm_16x16(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TX_TYPE tx_type,
                                  FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      av1_fwd_txfm2d_16x16(src_diff, coeff, diff_stride, tx_type, bd);
      break;
#if CONFIG_EXT_TX
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      av1_fwd_txfm2d_16x16(src_diff, coeff, diff_stride, tx_type, bd);
      break;
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
    case V_FLIPADST:
    case H_FLIPADST:
      // Use C version since DST exists only in C
      av1_highbd_fht16x16_c(src_diff, coeff, diff_stride, tx_type);
      break;
    case IDTX: av1_fwd_idtx_c(src_diff, coeff, diff_stride, 16, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0);
  }
}

static void highbd_fwd_txfm_32x32(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TX_TYPE tx_type,
                                  FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  switch (tx_type) {
    case DCT_DCT:
      av1_fwd_txfm2d_32x32(src_diff, coeff, diff_stride, tx_type, bd);
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
      av1_highbd_fht32x32_c(src_diff, coeff, diff_stride, tx_type);
      break;
    case IDTX: av1_fwd_idtx_c(src_diff, coeff, diff_stride, 32, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}

#if CONFIG_TX64X64
static void highbd_fwd_txfm_64x64(const int16_t *src_diff, tran_low_t *coeff,
                                  int diff_stride, TX_TYPE tx_type,
                                  FWD_TXFM_OPT fwd_txfm_opt, const int bd) {
  (void)fwd_txfm_opt;
  (void)bd;
  switch (tx_type) {
    case DCT_DCT:
      av1_highbd_fht64x64(src_diff, coeff, diff_stride, tx_type);
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
      av1_highbd_fht64x64(src_diff, coeff, diff_stride, tx_type);
      break;
    case IDTX: av1_fwd_idtx_c(src_diff, coeff, diff_stride, 64, tx_type); break;
#endif  // CONFIG_EXT_TX
    default: assert(0); break;
  }
}
#endif  // CONFIG_TX64X64
#endif  // CONFIG_HIGHBITDEPTH

void av1_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff, int diff_stride,
                  FWD_TXFM_PARAM *fwd_txfm_param) {
  const int fwd_txfm_opt = FWD_TXFM_OPT_NORMAL;
  const TX_TYPE tx_type = fwd_txfm_param->tx_type;
  const TX_SIZE tx_size = fwd_txfm_param->tx_size;
  const int lossless = fwd_txfm_param->lossless;
  switch (tx_size) {
#if CONFIG_TX64X64
    case TX_64X64:
      fwd_txfm_64x64(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
#endif  // CONFIG_TX64X64
    case TX_32X32:
      fwd_txfm_32x32(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_16X16:
      fwd_txfm_16x16(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_8X8:
      fwd_txfm_8x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_4X8:
      fwd_txfm_4x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_8X4:
      fwd_txfm_8x4(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_8X16:
      fwd_txfm_8x16(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_16X8:
      fwd_txfm_16x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_16X32:
      fwd_txfm_16x32(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_32X16:
      fwd_txfm_32x16(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_4X4:
      fwd_txfm_4x4(src_diff, coeff, diff_stride, tx_type, lossless);
      break;
#if CONFIG_CHROMA_2X2
    case TX_2X2:
      fwd_txfm_2x2(src_diff, coeff, diff_stride, tx_type, lossless);
      break;
#endif
#if CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT
    case TX_4X16:
      fwd_txfm_4x16(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_16X4:
      fwd_txfm_16x4(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_8X32:
      fwd_txfm_8x32(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
    case TX_32X8:
      fwd_txfm_32x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt);
      break;
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT
    default: assert(0); break;
  }
}

#if CONFIG_HIGHBITDEPTH
void av1_highbd_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, FWD_TXFM_PARAM *fwd_txfm_param) {
  const int fwd_txfm_opt = FWD_TXFM_OPT_NORMAL;
  const TX_TYPE tx_type = fwd_txfm_param->tx_type;
  const TX_SIZE tx_size = fwd_txfm_param->tx_size;
  const int lossless = fwd_txfm_param->lossless;
  const int bd = fwd_txfm_param->bd;
  switch (tx_size) {
#if CONFIG_TX64X64
    case TX_64X64:
      highbd_fwd_txfm_64x64(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                            bd);
      break;
#endif  // CONFIG_TX64X64
    case TX_32X32:
      highbd_fwd_txfm_32x32(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                            bd);
      break;
    case TX_16X16:
      highbd_fwd_txfm_16x16(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                            bd);
      break;
    case TX_8X8:
      highbd_fwd_txfm_8x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                          bd);
      break;
    case TX_4X8:
      highbd_fwd_txfm_4x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                          bd);
      break;
    case TX_8X4:
      highbd_fwd_txfm_8x4(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                          bd);
      break;
    case TX_8X16:
      highbd_fwd_txfm_8x16(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                           bd);
      break;
    case TX_16X8:
      highbd_fwd_txfm_16x8(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                           bd);
      break;
    case TX_16X32:
      highbd_fwd_txfm_16x32(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                            bd);
      break;
    case TX_32X16:
      highbd_fwd_txfm_32x16(src_diff, coeff, diff_stride, tx_type, fwd_txfm_opt,
                            bd);
      break;
    case TX_4X4:
      highbd_fwd_txfm_4x4(src_diff, coeff, diff_stride, tx_type, lossless, bd);
      break;
#if CONFIG_CHROMA_2X2
    case TX_2X2:
      highbd_fwd_txfm_2x2(src_diff, coeff, diff_stride, tx_type, lossless, bd);
      break;
#endif
    default: assert(0); break;
  }
}
#endif  // CONFIG_HIGHBITDEPTH
