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

#include <assert.h>

#include "./av1_rtcd.h"
#include "av1/common/enums.h"
#include "av1/common/av1_fwd_txfm1d.h"
#include "av1/common/av1_fwd_txfm1d_cfg.h"
#include "av1/common/av1_txfm.h"

static INLINE TxfmFunc fwd_txfm_type_to_func(TXFM_TYPE txfm_type) {
  switch (txfm_type) {
    case TXFM_TYPE_DCT4: return av1_fdct4_new;
    case TXFM_TYPE_DCT8: return av1_fdct8_new;
    case TXFM_TYPE_DCT16: return av1_fdct16_new;
    case TXFM_TYPE_DCT32: return av1_fdct32_new;
    case TXFM_TYPE_ADST4: return av1_fadst4_new;
    case TXFM_TYPE_ADST8: return av1_fadst8_new;
    case TXFM_TYPE_ADST16: return av1_fadst16_new;
    case TXFM_TYPE_ADST32: return av1_fadst32_new;
#if CONFIG_EXT_TX
    case TXFM_TYPE_IDENTITY4: return av1_fidentity4_c;
    case TXFM_TYPE_IDENTITY8: return av1_fidentity8_c;
    case TXFM_TYPE_IDENTITY16: return av1_fidentity16_c;
    case TXFM_TYPE_IDENTITY32: return av1_fidentity32_c;
#endif  // CONFIG_EXT_TX
    default: assert(0); return NULL;
  }
}

static INLINE void fwd_txfm2d_c(const int16_t *input, int32_t *output,
                                const int stride, const TXFM_2D_FLIP_CFG *cfg,
                                int32_t *buf) {
  int c, r;
  // TODO(sarahparker) must correct for rectangular transforms in follow up
  const int txfm_size = cfg->row_cfg->txfm_size;
  const int8_t *shift = cfg->row_cfg->shift;
  const int8_t *stage_range_col = cfg->col_cfg->stage_range;
  const int8_t *stage_range_row = cfg->row_cfg->stage_range;
  const int8_t *cos_bit_col = cfg->col_cfg->cos_bit;
  const int8_t *cos_bit_row = cfg->row_cfg->cos_bit;
  const TxfmFunc txfm_func_col = fwd_txfm_type_to_func(cfg->col_cfg->txfm_type);
  const TxfmFunc txfm_func_row = fwd_txfm_type_to_func(cfg->row_cfg->txfm_type);

  // use output buffer as temp buffer
  int32_t *temp_in = output;
  int32_t *temp_out = output + txfm_size;

  // Columns
  for (c = 0; c < txfm_size; ++c) {
    if (cfg->ud_flip == 0) {
      for (r = 0; r < txfm_size; ++r) temp_in[r] = input[r * stride + c];
    } else {
      for (r = 0; r < txfm_size; ++r)
        // flip upside down
        temp_in[r] = input[(txfm_size - r - 1) * stride + c];
    }
    round_shift_array(temp_in, txfm_size, -shift[0]);
    txfm_func_col(temp_in, temp_out, cos_bit_col, stage_range_col);
    round_shift_array(temp_out, txfm_size, -shift[1]);
    if (cfg->lr_flip == 0) {
      for (r = 0; r < txfm_size; ++r) buf[r * txfm_size + c] = temp_out[r];
    } else {
      for (r = 0; r < txfm_size; ++r)
        // flip from left to right
        buf[r * txfm_size + (txfm_size - c - 1)] = temp_out[r];
    }
  }

  // Rows
  for (r = 0; r < txfm_size; ++r) {
    txfm_func_row(buf + r * txfm_size, output + r * txfm_size, cos_bit_row,
                  stage_range_row);
    round_shift_array(output + r * txfm_size, txfm_size, -shift[2]);
  }
}

void av1_fwd_txfm2d_4x4_c(const int16_t *input, int32_t *output, int stride,
                          int tx_type, int bd) {
  int32_t txfm_buf[4 * 4];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_4X4);
  (void)bd;
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf);
}

void av1_fwd_txfm2d_8x8_c(const int16_t *input, int32_t *output, int stride,
                          int tx_type, int bd) {
  int32_t txfm_buf[8 * 8];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_8X8);
  (void)bd;
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf);
}

void av1_fwd_txfm2d_16x16_c(const int16_t *input, int32_t *output, int stride,
                            int tx_type, int bd) {
  int32_t txfm_buf[16 * 16];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_16X16);
  (void)bd;
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf);
}

void av1_fwd_txfm2d_32x32_c(const int16_t *input, int32_t *output, int stride,
                            int tx_type, int bd) {
  int32_t txfm_buf[32 * 32];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_32X32);
  (void)bd;
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf);
}

void av1_fwd_txfm2d_64x64_c(const int16_t *input, int32_t *output, int stride,
                            int tx_type, int bd) {
  int32_t txfm_buf[64 * 64];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_64x64_cfg(tx_type);
  (void)bd;
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf);
}

static const TXFM_1D_CFG *fwd_txfm_col_cfg_ls[TX_TYPES_1D][TX_SIZES] = {
  // DCT
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &fwd_txfm_1d_col_cfg_dct_4, &fwd_txfm_1d_col_cfg_dct_8,
      &fwd_txfm_1d_col_cfg_dct_16, &fwd_txfm_1d_col_cfg_dct_32 },
  // ADST
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &fwd_txfm_1d_col_cfg_adst_4, &fwd_txfm_1d_col_cfg_adst_8,
      &fwd_txfm_1d_col_cfg_adst_16, &fwd_txfm_1d_col_cfg_adst_32 },
#if CONFIG_EXT_TX
  // FLIPADST
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &fwd_txfm_1d_col_cfg_adst_4, &fwd_txfm_1d_col_cfg_adst_8,
      &fwd_txfm_1d_col_cfg_adst_16, &fwd_txfm_1d_col_cfg_adst_32 },
  // IDENTITY
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &fwd_txfm_1d_cfg_identity_4, &fwd_txfm_1d_cfg_identity_8,
      &fwd_txfm_1d_cfg_identity_16, &fwd_txfm_1d_cfg_identity_32 },
#endif  // CONFIG_EXT_TX
};

static const TXFM_1D_CFG *fwd_txfm_row_cfg_ls[TX_TYPES_1D][TX_SIZES] = {
  // DCT
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &fwd_txfm_1d_row_cfg_dct_4, &fwd_txfm_1d_row_cfg_dct_8,
      &fwd_txfm_1d_row_cfg_dct_16, &fwd_txfm_1d_row_cfg_dct_32 },
  // ADST
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &fwd_txfm_1d_row_cfg_adst_4, &fwd_txfm_1d_row_cfg_adst_8,
      &fwd_txfm_1d_row_cfg_adst_16, &fwd_txfm_1d_row_cfg_adst_32 },
#if CONFIG_EXT_TX
  // FLIPADST
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &fwd_txfm_1d_row_cfg_adst_4, &fwd_txfm_1d_row_cfg_adst_8,
      &fwd_txfm_1d_row_cfg_adst_16, &fwd_txfm_1d_row_cfg_adst_32 },
  // IDENTITY
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &fwd_txfm_1d_cfg_identity_4, &fwd_txfm_1d_cfg_identity_8,
      &fwd_txfm_1d_cfg_identity_16, &fwd_txfm_1d_cfg_identity_32 },
#endif  // CONFIG_EXT_TX
};

TXFM_2D_FLIP_CFG av1_get_fwd_txfm_cfg(int tx_type, int tx_size) {
  TXFM_2D_FLIP_CFG cfg;
  set_flip_cfg(tx_type, &cfg);
  int tx_type_col = vtx_tab[tx_type];
  int tx_type_row = htx_tab[tx_type];
  cfg.col_cfg = fwd_txfm_col_cfg_ls[tx_type_col][tx_size];
  cfg.row_cfg = fwd_txfm_row_cfg_ls[tx_type_row][tx_size];
  return cfg;
}

TXFM_2D_FLIP_CFG av1_get_fwd_txfm_64x64_cfg(int tx_type) {
  TXFM_2D_FLIP_CFG cfg;
  switch (tx_type) {
    case DCT_DCT:
      cfg.col_cfg = &fwd_txfm_1d_col_cfg_dct_64;
      cfg.row_cfg = &fwd_txfm_1d_row_cfg_dct_64;
      cfg.ud_flip = 0;
      cfg.lr_flip = 0;
      break;
    default:
      cfg.ud_flip = 0;
      cfg.lr_flip = 0;
      assert(0);
  }
  return cfg;
}
