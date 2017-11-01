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
#include "aom_dsp/txfm_common.h"
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
#if CONFIG_TX64X64
    case TXFM_TYPE_DCT64: return av1_fdct64_new;
#endif  // CONFIG_TX64X64
    case TXFM_TYPE_ADST4: return av1_fadst4_new;
    case TXFM_TYPE_ADST8: return av1_fadst8_new;
    case TXFM_TYPE_ADST16: return av1_fadst16_new;
    case TXFM_TYPE_ADST32: return av1_fadst32_new;
#if CONFIG_EXT_TX
    case TXFM_TYPE_IDENTITY4: return av1_fidentity4_c;
    case TXFM_TYPE_IDENTITY8: return av1_fidentity8_c;
    case TXFM_TYPE_IDENTITY16: return av1_fidentity16_c;
    case TXFM_TYPE_IDENTITY32: return av1_fidentity32_c;
#if CONFIG_TX64X64
    case TXFM_TYPE_IDENTITY64: return av1_fidentity64_c;
#endif  // CONFIG_TX64X64
#endif  // CONFIG_EXT_TX
    default: assert(0); return NULL;
  }
}

void av1_gen_fwd_stage_range(int8_t *stage_range_col, int8_t *stage_range_row,
                             const TXFM_2D_FLIP_CFG *cfg, int bd) {
  // Note when assigning txfm_size_col, we use the txfm_size from the
  // row configuration and vice versa. This is intentionally done to
  // accurately perform rectangular transforms. When the transform is
  // rectangular, the number of columns will be the same as the
  // txfm_size stored in the row cfg struct. It will make no difference
  // for square transforms.
  const int txfm_size_col = cfg->row_cfg->txfm_size;
  const int txfm_size_row = cfg->col_cfg->txfm_size;
  // Take the shift from the larger dimension in the rectangular case.
  const int8_t *shift = (txfm_size_col > txfm_size_row) ? cfg->row_cfg->shift
                                                        : cfg->col_cfg->shift;
  // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
  for (int i = 0; i < cfg->col_cfg->stage_num && i < MAX_TXFM_STAGE_NUM; ++i) {
    stage_range_col[i] = cfg->col_cfg->stage_range[i] + shift[0] + bd + 1;
  }

  // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
  for (int i = 0; i < cfg->row_cfg->stage_num && i < MAX_TXFM_STAGE_NUM; ++i) {
    stage_range_row[i] =
        cfg->row_cfg->stage_range[i] + shift[0] + shift[1] + bd + 1;
  }
}

static INLINE void fwd_txfm2d_c(const int16_t *input, int32_t *output,
                                const int stride, const TXFM_2D_FLIP_CFG *cfg,
                                int32_t *buf, int bd) {
  int c, r;
  // Note when assigning txfm_size_col, we use the txfm_size from the
  // row configuration and vice versa. This is intentionally done to
  // accurately perform rectangular transforms. When the transform is
  // rectangular, the number of columns will be the same as the
  // txfm_size stored in the row cfg struct. It will make no difference
  // for square transforms.
  const int txfm_size_col = cfg->row_cfg->txfm_size;
  const int txfm_size_row = cfg->col_cfg->txfm_size;
  // Take the shift from the larger dimension in the rectangular case.
  const int8_t *shift = (txfm_size_col > txfm_size_row) ? cfg->row_cfg->shift
                                                        : cfg->col_cfg->shift;
  int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
  int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
  assert(cfg->col_cfg->stage_num <= MAX_TXFM_STAGE_NUM);
  assert(cfg->row_cfg->stage_num <= MAX_TXFM_STAGE_NUM);
  av1_gen_fwd_stage_range(stage_range_col, stage_range_row, cfg, bd);

  const int8_t *cos_bit_col = cfg->col_cfg->cos_bit;
  const int8_t *cos_bit_row = cfg->row_cfg->cos_bit;
  const TxfmFunc txfm_func_col = fwd_txfm_type_to_func(cfg->col_cfg->txfm_type);
  const TxfmFunc txfm_func_row = fwd_txfm_type_to_func(cfg->row_cfg->txfm_type);

  // use output buffer as temp buffer
  int32_t *temp_in = output;
  int32_t *temp_out = output + txfm_size_row;

  // Columns
  for (c = 0; c < txfm_size_col; ++c) {
    if (cfg->ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) temp_in[r] = input[r * stride + c];
    } else {
      for (r = 0; r < txfm_size_row; ++r)
        // flip upside down
        temp_in[r] = input[(txfm_size_row - r - 1) * stride + c];
    }
    round_shift_array(temp_in, txfm_size_row, -shift[0]);
    // Multiply everything by Sqrt2 on the larger dimension if the
    // transform is rectangular
    if (txfm_size_col > txfm_size_row) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = (int32_t)fdct_round_shift(temp_in[r] * Sqrt2);
    }
    txfm_func_col(temp_in, temp_out, cos_bit_col, stage_range_col);
    round_shift_array(temp_out, txfm_size_row, -shift[1]);
    if (cfg->lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        buf[r * txfm_size_col + c] = temp_out[r];
    } else {
      for (r = 0; r < txfm_size_row; ++r)
        // flip from left to right
        buf[r * txfm_size_col + (txfm_size_col - c - 1)] = temp_out[r];
    }
  }

  // Rows
  for (r = 0; r < txfm_size_row; ++r) {
    // Multiply everything by Sqrt2 on the larger dimension if the
    // transform is rectangular
    if (txfm_size_row > txfm_size_col) {
      for (c = 0; c < txfm_size_col; ++c)
        buf[r * txfm_size_col + c] =
            (int32_t)fdct_round_shift(buf[r * txfm_size_col + c] * Sqrt2);
    }
    txfm_func_row(buf + r * txfm_size_col, output + r * txfm_size_col,
                  cos_bit_row, stage_range_row);
    round_shift_array(output + r * txfm_size_col, txfm_size_col, -shift[2]);
  }
}

void av1_fwd_txfm2d_4x8_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd) {
#if CONFIG_TXMG
  int32_t txfm_buf[4 * 8];
  int16_t rinput[4 * 8];
  TX_SIZE tx_size = TX_4X8;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int16(rinput, rw, input, stride, w, h);
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(rtx_type, rtx_size);
  fwd_txfm2d_c(rinput, txfm_buf, rw, &cfg, output, bd);
  transpose_int32(output, w, txfm_buf, rw, rw, rh);
#else
  int32_t txfm_buf[4 * 8];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_4X8);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
#endif
}

void av1_fwd_txfm2d_8x4_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[8 * 4];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_8X4);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_8x16_c(const int16_t *input, int32_t *output, int stride,
                           TX_TYPE tx_type, int bd) {
#if CONFIG_TXMG
  int32_t txfm_buf[8 * 16];
  int16_t rinput[8 * 16];
  TX_SIZE tx_size = TX_8X16;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int16(rinput, rw, input, stride, w, h);
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(rtx_type, rtx_size);
  fwd_txfm2d_c(rinput, txfm_buf, rw, &cfg, output, bd);
  transpose_int32(output, w, txfm_buf, rw, rw, rh);
#else
  int32_t txfm_buf[8 * 16];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_8X16);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
#endif
}

void av1_fwd_txfm2d_16x8_c(const int16_t *input, int32_t *output, int stride,
                           TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[16 * 8];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_16X8);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_16x32_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
#if CONFIG_TXMG
  int32_t txfm_buf[16 * 32];
  int16_t rinput[16 * 32];
  TX_SIZE tx_size = TX_16X32;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int16(rinput, rw, input, stride, w, h);
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(rtx_type, rtx_size);
  fwd_txfm2d_c(rinput, txfm_buf, rw, &cfg, output, bd);
  transpose_int32(output, w, txfm_buf, rw, rw, rh);
#else
  int32_t txfm_buf[16 * 32];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_16X32);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
#endif
}

void av1_fwd_txfm2d_32x16_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[32 * 16];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_32X16);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_4x4_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[4 * 4];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_4X4);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_8x8_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[8 * 8];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_8X8);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_16x16_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[16 * 16];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_16X16);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_32x32_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[32 * 32];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(tx_type, TX_32X32);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

#if CONFIG_TX64X64
void av1_fwd_txfm2d_64x64_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[64 * 64];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_64x64_cfg(tx_type);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_32x64_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[32 * 64];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_32x64_cfg(tx_type);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_64x32_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[64 * 32];
  TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_64x32_cfg(tx_type);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}
#endif  // CONFIG_TX64X64

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

TXFM_2D_FLIP_CFG av1_get_fwd_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size) {
  TXFM_2D_FLIP_CFG cfg;
  set_flip_cfg(tx_type, &cfg);
  const TX_TYPE_1D tx_type_col = vtx_tab[tx_type];
  const TX_TYPE_1D tx_type_row = htx_tab[tx_type];
  const TX_SIZE tx_size_col = txsize_vert_map[tx_size];
  const TX_SIZE tx_size_row = txsize_horz_map[tx_size];
  cfg.col_cfg = fwd_txfm_col_cfg_ls[tx_type_col][tx_size_col];
  cfg.row_cfg = fwd_txfm_row_cfg_ls[tx_type_row][tx_size_row];
  return cfg;
}

#if CONFIG_TX64X64
TXFM_2D_FLIP_CFG av1_get_fwd_txfm_32x64_cfg(TX_TYPE tx_type) {
  TXFM_2D_FLIP_CFG cfg;
  const TX_TYPE_1D tx_type_row = htx_tab[tx_type];
  const TX_SIZE tx_size_row = txsize_horz_map[TX_32X64];
  switch (tx_type) {
    case DCT_DCT:
      cfg.col_cfg = &fwd_txfm_1d_col_cfg_dct_64;
      cfg.row_cfg = fwd_txfm_row_cfg_ls[tx_type_row][tx_size_row];
      cfg.ud_flip = 0;
      cfg.lr_flip = 0;
      break;
    default: assert(0);
  }
  return cfg;
}

TXFM_2D_FLIP_CFG av1_get_fwd_txfm_64x32_cfg(TX_TYPE tx_type) {
  TXFM_2D_FLIP_CFG cfg;
  const TX_TYPE_1D tx_type_col = vtx_tab[tx_type];
  const TX_SIZE tx_size_col = txsize_vert_map[TX_64X32];
  switch (tx_type) {
    case DCT_DCT:
      cfg.col_cfg = fwd_txfm_col_cfg_ls[tx_type_col][tx_size_col];
      cfg.row_cfg = &fwd_txfm_1d_row_cfg_dct_64;
      cfg.ud_flip = 0;
      cfg.lr_flip = 0;
      break;
    default: assert(0);
  }
  return cfg;
}

TXFM_2D_FLIP_CFG av1_get_fwd_txfm_64x64_cfg(TX_TYPE tx_type) {
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
#endif  // CONFIG_TX64X64
