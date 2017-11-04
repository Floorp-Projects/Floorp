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
#include "aom_dsp/inv_txfm.h"
#include "av1/common/enums.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/av1_inv_txfm1d.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"

static INLINE TxfmFunc inv_txfm_type_to_func(TXFM_TYPE txfm_type) {
  switch (txfm_type) {
    case TXFM_TYPE_DCT4: return av1_idct4_new;
    case TXFM_TYPE_DCT8: return av1_idct8_new;
    case TXFM_TYPE_DCT16: return av1_idct16_new;
    case TXFM_TYPE_DCT32: return av1_idct32_new;
#if CONFIG_TX64X64
    case TXFM_TYPE_DCT64: return av1_idct64_new;
#endif  // CONFIG_TX64X64
    case TXFM_TYPE_ADST4: return av1_iadst4_new;
    case TXFM_TYPE_ADST8: return av1_iadst8_new;
    case TXFM_TYPE_ADST16: return av1_iadst16_new;
    case TXFM_TYPE_ADST32: return av1_iadst32_new;
#if CONFIG_EXT_TX
    case TXFM_TYPE_IDENTITY4: return av1_iidentity4_c;
    case TXFM_TYPE_IDENTITY8: return av1_iidentity8_c;
    case TXFM_TYPE_IDENTITY16: return av1_iidentity16_c;
    case TXFM_TYPE_IDENTITY32: return av1_iidentity32_c;
#if CONFIG_TX64X64
    case TXFM_TYPE_IDENTITY64: return av1_iidentity64_c;
#endif  // CONFIG_TX64X64
#endif  // CONFIG_EXT_TX
    default: assert(0); return NULL;
  }
}

static const TXFM_1D_CFG *inv_txfm_col_cfg_ls[TX_TYPES_1D][TX_SIZES] = {
  // DCT
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &inv_txfm_1d_col_cfg_dct_4, &inv_txfm_1d_col_cfg_dct_8,
      &inv_txfm_1d_col_cfg_dct_16, &inv_txfm_1d_col_cfg_dct_32,
#if CONFIG_TX64X64
      &inv_txfm_1d_col_cfg_dct_64
#endif  // CONFIG_TX64X64
  },
  // ADST
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &inv_txfm_1d_col_cfg_adst_4, &inv_txfm_1d_col_cfg_adst_8,
      &inv_txfm_1d_col_cfg_adst_16, &inv_txfm_1d_col_cfg_adst_32,
#if CONFIG_TX64X64
      NULL
#endif  // CONFIG_TX64X64
  },
#if CONFIG_EXT_TX
  // FLIPADST
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &inv_txfm_1d_col_cfg_adst_4, &inv_txfm_1d_col_cfg_adst_8,
      &inv_txfm_1d_col_cfg_adst_16, &inv_txfm_1d_col_cfg_adst_32,
#if CONFIG_TX64X64
      NULL
#endif  // CONFIG_TX64X64
  },
  // IDENTITY
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &inv_txfm_1d_cfg_identity_4, &inv_txfm_1d_cfg_identity_8,
      &inv_txfm_1d_cfg_identity_16, &inv_txfm_1d_cfg_identity_32,
#if CONFIG_TX64X64
      &inv_txfm_1d_cfg_identity_64
#endif  // CONFIG_TX64X64
  },
#endif  // CONFIG_EXT_TX
};

static const TXFM_1D_CFG *inv_txfm_row_cfg_ls[TX_TYPES_1D][TX_SIZES] = {
  // DCT
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &inv_txfm_1d_row_cfg_dct_4, &inv_txfm_1d_row_cfg_dct_8,
      &inv_txfm_1d_row_cfg_dct_16, &inv_txfm_1d_row_cfg_dct_32,
#if CONFIG_TX64X64
      &inv_txfm_1d_row_cfg_dct_64,
#endif  // CONFIG_TX64X64
  },
  // ADST
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &inv_txfm_1d_row_cfg_adst_4, &inv_txfm_1d_row_cfg_adst_8,
      &inv_txfm_1d_row_cfg_adst_16, &inv_txfm_1d_row_cfg_adst_32,
#if CONFIG_TX64X64
      NULL
#endif  // CONFIG_TX64X64
  },
#if CONFIG_EXT_TX
  // FLIPADST
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &inv_txfm_1d_row_cfg_adst_4, &inv_txfm_1d_row_cfg_adst_8,
      &inv_txfm_1d_row_cfg_adst_16, &inv_txfm_1d_row_cfg_adst_32,
#if CONFIG_TX64X64
      NULL
#endif  // CONFIG_TX64X64
  },
  // IDENTITY
  {
#if CONFIG_CHROMA_2X2
      NULL,
#endif
      &inv_txfm_1d_cfg_identity_4, &inv_txfm_1d_cfg_identity_8,
      &inv_txfm_1d_cfg_identity_16, &inv_txfm_1d_cfg_identity_32,
#if CONFIG_TX64X64
      &inv_txfm_1d_cfg_identity_64
#endif  // CONFIG_TX64X64
  },
#endif  // CONFIG_EXT_TX
};

TXFM_2D_FLIP_CFG av1_get_inv_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size) {
  TXFM_2D_FLIP_CFG cfg;
  set_flip_cfg(tx_type, &cfg);
  const TX_TYPE_1D tx_type_col = vtx_tab[tx_type];
  const TX_TYPE_1D tx_type_row = htx_tab[tx_type];
  const TX_SIZE tx_size_col = txsize_vert_map[tx_size];
  const TX_SIZE tx_size_row = txsize_horz_map[tx_size];
  cfg.col_cfg = inv_txfm_col_cfg_ls[tx_type_col][tx_size_col];
  cfg.row_cfg = inv_txfm_row_cfg_ls[tx_type_row][tx_size_row];
  return cfg;
}

#if CONFIG_TX64X64
TXFM_2D_FLIP_CFG av1_get_inv_txfm_64x64_cfg(TX_TYPE tx_type) {
  TXFM_2D_FLIP_CFG cfg = { 0, 0, NULL, NULL };
  switch (tx_type) {
    case DCT_DCT:
      cfg.col_cfg = &inv_txfm_1d_col_cfg_dct_64;
      cfg.row_cfg = &inv_txfm_1d_row_cfg_dct_64;
      set_flip_cfg(tx_type, &cfg);
      break;
    default: assert(0);
  }
  return cfg;
}

TXFM_2D_FLIP_CFG av1_get_inv_txfm_32x64_cfg(int tx_type) {
  TXFM_2D_FLIP_CFG cfg = { 0, 0, NULL, NULL };
  switch (tx_type) {
    case DCT_DCT:
      cfg.col_cfg = &inv_txfm_1d_col_cfg_dct_64;
      cfg.row_cfg = &inv_txfm_1d_row_cfg_dct_32;
      set_flip_cfg(tx_type, &cfg);
      break;
    default: assert(0);
  }
  return cfg;
}

TXFM_2D_FLIP_CFG av1_get_inv_txfm_64x32_cfg(int tx_type) {
  TXFM_2D_FLIP_CFG cfg = { 0, 0, NULL, NULL };
  switch (tx_type) {
    case DCT_DCT:
      cfg.col_cfg = &inv_txfm_1d_col_cfg_dct_32;
      cfg.row_cfg = &inv_txfm_1d_row_cfg_dct_64;
      set_flip_cfg(tx_type, &cfg);
      break;
    default: assert(0);
  }
  return cfg;
}
#endif  // CONFIG_TX64X64

void av1_gen_inv_stage_range(int8_t *stage_range_col, int8_t *stage_range_row,
                             const TXFM_2D_FLIP_CFG *cfg, int8_t fwd_shift,
                             int bd) {
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
  for (int i = 0; i < cfg->row_cfg->stage_num && i < MAX_TXFM_STAGE_NUM; ++i) {
    stage_range_row[i] = cfg->row_cfg->stage_range[i] + fwd_shift + bd + 1;
  }
  // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
  for (int i = 0; i < cfg->col_cfg->stage_num && i < MAX_TXFM_STAGE_NUM; ++i) {
    stage_range_col[i] =
        cfg->col_cfg->stage_range[i] + fwd_shift + shift[0] + bd + 1;
  }
}

static INLINE void inv_txfm2d_add_c(const int32_t *input, uint16_t *output,
                                    int stride, TXFM_2D_FLIP_CFG *cfg,
                                    int32_t *txfm_buf, int8_t fwd_shift,
                                    int bd) {
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
  int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
  int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
  assert(cfg->row_cfg->stage_num <= MAX_TXFM_STAGE_NUM);
  assert(cfg->col_cfg->stage_num <= MAX_TXFM_STAGE_NUM);
  av1_gen_inv_stage_range(stage_range_col, stage_range_row, cfg, fwd_shift, bd);

  const int8_t *cos_bit_col = cfg->col_cfg->cos_bit;
  const int8_t *cos_bit_row = cfg->row_cfg->cos_bit;
  const TxfmFunc txfm_func_col = inv_txfm_type_to_func(cfg->col_cfg->txfm_type);
  const TxfmFunc txfm_func_row = inv_txfm_type_to_func(cfg->row_cfg->txfm_type);

  // txfm_buf's length is  txfm_size_row * txfm_size_col + 2 * txfm_size_row
  // it is used for intermediate data buffering
  int32_t *temp_in = txfm_buf;
  int32_t *temp_out = temp_in + txfm_size_row;
  int32_t *buf = temp_out + txfm_size_row;
  int32_t *buf_ptr = buf;
  int c, r;

  // Rows
  for (r = 0; r < txfm_size_row; ++r) {
    txfm_func_row(input, buf_ptr, cos_bit_row, stage_range_row);
    round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    // Multiply everything by Sqrt2 if the transform is rectangular
    if (txfm_size_row != txfm_size_col) {
      for (c = 0; c < txfm_size_col; ++c)
        buf_ptr[c] = (int32_t)dct_const_round_shift(buf_ptr[c] * Sqrt2);
    }
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  // Columns
  for (c = 0; c < txfm_size_col; ++c) {
    if (cfg->lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    txfm_func_col(temp_in, temp_out, cos_bit_col, stage_range_col);
    round_shift_array(temp_out, txfm_size_row, -shift[1]);
    if (cfg->ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

static INLINE void inv_txfm2d_add_facade(const int32_t *input, uint16_t *output,
                                         int stride, int32_t *txfm_buf,
                                         TX_TYPE tx_type, TX_SIZE tx_size,
                                         int bd) {
  TXFM_2D_FLIP_CFG cfg = av1_get_inv_txfm_cfg(tx_type, tx_size);
  TX_SIZE tx_size_sqr = txsize_sqr_map[tx_size];
  inv_txfm2d_add_c(input, output, stride, &cfg, txfm_buf,
                   fwd_shift_sum[tx_size_sqr], bd);
}

void av1_inv_txfm2d_add_4x8_c(const int32_t *input, uint16_t *output,
                              int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[4 * 8 + 8 + 8];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_4X8, bd);
}

void av1_inv_txfm2d_add_8x4_c(const int32_t *input, uint16_t *output,
                              int stride, TX_TYPE tx_type, int bd) {
#if CONFIG_TXMG
  int txfm_buf[8 * 4 + 8 + 8];
  int32_t rinput[8 * 4];
  uint16_t routput[8 * 4];
  TX_SIZE tx_size = TX_8X4;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#else
  int txfm_buf[8 * 4 + 4 + 4];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X4, bd);
#endif
}

void av1_inv_txfm2d_add_8x16_c(const int32_t *input, uint16_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[8 * 16 + 16 + 16];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X16, bd);
}

void av1_inv_txfm2d_add_16x8_c(const int32_t *input, uint16_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
#if CONFIG_TXMG
  int txfm_buf[16 * 8 + 16 + 16];
  int32_t rinput[16 * 8];
  uint16_t routput[16 * 8];
  TX_SIZE tx_size = TX_16X8;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#else
  int txfm_buf[16 * 8 + 8 + 8];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X8, bd);
#endif
}

void av1_inv_txfm2d_add_16x32_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[16 * 32 + 32 + 32];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X32, bd);
}

void av1_inv_txfm2d_add_32x16_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
#if CONFIG_TXMG
  int txfm_buf[32 * 16 + 32 + 32];
  int32_t rinput[32 * 16];
  uint16_t routput[32 * 16];
  TX_SIZE tx_size = TX_32X16;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#else
  int txfm_buf[32 * 16 + 16 + 16];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_32X16, bd);
#endif
}

void av1_inv_txfm2d_add_4x4_c(const int32_t *input, uint16_t *output,
                              int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[4 * 4 + 4 + 4];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_4X4, bd);
}

void av1_inv_txfm2d_add_8x8_c(const int32_t *input, uint16_t *output,
                              int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[8 * 8 + 8 + 8];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X8, bd);
}

void av1_inv_txfm2d_add_16x16_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[16 * 16 + 16 + 16];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X16, bd);
}

void av1_inv_txfm2d_add_32x32_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[32 * 32 + 32 + 32];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_32X32, bd);
}

#if CONFIG_TX64X64
void av1_inv_txfm2d_add_64x64_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[64 * 64 + 64 + 64];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_64X64, bd);
}

void av1_inv_txfm2d_add_64x32_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
#if CONFIG_TXMG
  int txfm_buf[64 * 32 + 64 + 64];
  int32_t rinput[64 * 32];
  uint16_t routput[64 * 32];
  TX_SIZE tx_size = TX_64X32;
  TX_SIZE rtx_size = av1_rotate_tx_size(tx_size);
  TX_TYPE rtx_type = av1_rotate_tx_type(tx_type);
  int w = tx_size_wide[tx_size];
  int h = tx_size_high[tx_size];
  int rw = h;
  int rh = w;
  transpose_int32(rinput, rw, input, w, w, h);
  transpose_uint16(routput, rw, output, stride, w, h);
  inv_txfm2d_add_facade(rinput, routput, rw, txfm_buf, rtx_type, rtx_size, bd);
  transpose_uint16(output, stride, routput, rw, rw, rh);
#else
  int txfm_buf[64 * 32 + 64 + 64];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_64X32, bd);
#endif
}

void av1_inv_txfm2d_add_32x64_c(const int32_t *input, uint16_t *output,
                                int stride, TX_TYPE tx_type, int bd) {
  int txfm_buf[64 * 32 + 64 + 64];
  inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_32X64, bd);
}
#endif  // CONFIG_TX64X64
