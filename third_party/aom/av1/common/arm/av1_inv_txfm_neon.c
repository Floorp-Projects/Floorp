/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "av1/common/av1_inv_txfm1d.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/enums.h"
#include "av1/common/idct.h"
#include "av1/common/arm/av1_inv_txfm_neon.h"

static INLINE TxSetType find_TxSetType(TX_SIZE tx_size) {
  const TX_SIZE tx_size_sqr_up = txsize_sqr_up_map[tx_size];
  TxSetType tx_set_type;
  if (tx_size_sqr_up > TX_32X32) {
    tx_set_type = EXT_TX_SET_DCTONLY;
  } else if (tx_size_sqr_up == TX_32X32) {
    tx_set_type = EXT_TX_SET_DCT_IDTX;
  } else {
    tx_set_type = EXT_TX_SET_ALL16;
  }
  return tx_set_type;
}

// 1D itx types
typedef enum ATTRIBUTE_PACKED {
  IDCT_1D,
  IADST_1D,
  IFLIPADST_1D = IADST_1D,
  IIDENTITY_1D,
  ITX_TYPES_1D,
} ITX_TYPE_1D;

static const ITX_TYPE_1D vitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IADST_1D,     IDCT_1D,      IADST_1D,
  IFLIPADST_1D, IDCT_1D,      IFLIPADST_1D, IADST_1D,
  IFLIPADST_1D, IIDENTITY_1D, IDCT_1D,      IIDENTITY_1D,
  IADST_1D,     IIDENTITY_1D, IFLIPADST_1D, IIDENTITY_1D,
};

static const ITX_TYPE_1D hitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IDCT_1D,      IADST_1D,     IADST_1D,
  IDCT_1D,      IFLIPADST_1D, IFLIPADST_1D, IFLIPADST_1D,
  IADST_1D,     IIDENTITY_1D, IIDENTITY_1D, IDCT_1D,
  IIDENTITY_1D, IADST_1D,     IIDENTITY_1D, IFLIPADST_1D,
};

// 1D functions
static const transform_1d_neon lowbd_txfm_all_1d_arr[TX_SIZES][ITX_TYPES_1D] = {
  { av1_idct4_new, av1_iadst4_new, av1_iidentity4_c },
  { av1_idct8_new, av1_iadst8_new, av1_iidentity8_c },
  { av1_idct16_new, av1_iadst16_new, av1_iidentity16_c },
  { av1_idct32_new, NULL, NULL },
  { av1_idct64_new, NULL, NULL },
};

// Functions for blocks with eob at DC and within
// topleft 8x8, 16x16, 32x32 corner
static const transform_1d_neon
    lowbd_txfm_all_1d_zeros_w8_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { av1_idct4_new, av1_idct4_new, NULL, NULL },
          { av1_iadst4_new, av1_iadst4_new, NULL, NULL },
          { av1_iidentity4_c, av1_iidentity4_c, NULL, NULL },
      },
      { { av1_idct8_new, av1_idct8_new, NULL, NULL },
        { av1_iadst8_new, av1_iadst8_new, NULL, NULL },
        { av1_iidentity8_c, av1_iidentity8_c, NULL, NULL } },
      {
          { av1_idct16_new, av1_idct16_new, av1_idct16_new, NULL },
          { av1_iadst16_new, av1_iadst16_new, av1_iadst16_new, NULL },
          { av1_iidentity16_c, av1_iidentity16_c, av1_iidentity16_c, NULL },
      },
      { { av1_idct32_new, av1_idct32_new, av1_idct32_new, av1_idct32_new },
        { NULL, NULL, NULL, NULL },
        { av1_iidentity32_c, av1_iidentity32_c, av1_iidentity32_c,
          av1_iidentity32_c } },
      { { av1_idct64_new, av1_idct64_new, av1_idct64_new, av1_idct64_new },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };
static INLINE void lowbd_inv_txfm2d_add_idtx_neon(const int32_t *input,
                                                  uint8_t *output, int stride,
                                                  TX_TYPE tx_type,
                                                  TX_SIZE tx_size, int eob) {
  DECLARE_ALIGNED(32, int, txfm_buf[32 * 32 + 32 + 32]);
  int32_t *temp_in = txfm_buf;

  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;

  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);

  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);

  // row tx
  int row_start = (buf_size_nonzero_h_div8 * 8);
  for (int i = 0; i < row_start; i++) {
    if (abs(rect_type) == 1) {
      for (int j = 0; j < txfm_size_col; j++)
        temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);
      row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    } else {
      row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  // Doing memset for the rows which are not processed in row transform.
  memset(buf_ptr, 0,
         sizeof(int32_t) * txfm_size_col * (txfm_size_row - row_start));

  // col tx
  for (int c = 0; c < txfm_size_col; c++) {
    for (r = 0; r < txfm_size_row; ++r) temp_in[r] = buf[r * txfm_size_col + c];

    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    for (r = 0; r < txfm_size_row; ++r) {
      output[r * stride + c] =
          highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
    }
  }
}

static INLINE void lowbd_inv_txfm2d_add_v_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  DECLARE_ALIGNED(32, int, txfm_buf[32 * 32 + 32 + 32]);
  int32_t *temp_in = txfm_buf;

  int eobx, eoby;
  get_eobx_eoby_scan_v_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;

  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);

  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // row tx
  int row_start = (buf_size_nonzero_h_div8 * 8);
  for (int i = 0; i < row_start; i++) {
    if (abs(rect_type) == 1) {
      for (int j = 0; j < txfm_size_col; j++)
        temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);
      row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    } else {
      row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }
  // Doing memset for the rows which are not processed in row transform.
  memset(buf_ptr, 0,
         sizeof(int32_t) * txfm_size_col * (txfm_size_row - row_start));

  // col tx
  for (int c = 0; c < txfm_size_col; c++) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
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

static INLINE void lowbd_inv_txfm2d_add_h_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  DECLARE_ALIGNED(32, int, txfm_buf[32 * 32 + 32 + 32]);
  int32_t *temp_in = txfm_buf;

  int eobx, eoby;
  get_eobx_eoby_scan_h_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;

  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);

  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // row tx
  int row_start = (buf_size_nonzero_h_div8 * 8);
  for (int i = 0; i < row_start; i++) {
    if (abs(rect_type) == 1) {
      for (int j = 0; j < txfm_size_col; j++)
        temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);
      row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    } else {
      row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }
  // Doing memset for the rows which are not processed in row transform.
  memset(buf_ptr, 0,
         sizeof(int32_t) * txfm_size_col * (txfm_size_row - row_start));

  // col tx
  for (int c = 0; c < txfm_size_col; c++) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
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

static INLINE void lowbd_inv_txfm2d_add_4x4_neon(const int32_t *input,
                                                 uint8_t *output, int stride,
                                                 TX_TYPE tx_type,
                                                 TX_SIZE tx_size, int eob) {
  (void)eob;
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 4 + 8 + 8]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    row_txfm(input, buf_ptr, cos_bit_row, stage_range);

    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
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

void lowbd_inv_txfm2d_add_4x8_neon(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob) {
  (void)eob;
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 8 + 8 + 8]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    for (int j = 0; j < txfm_size_col; j++)
      temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);

    row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
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

void lowbd_inv_txfm2d_add_8x4_neon(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob) {
  (void)eob;
  DECLARE_ALIGNED(32, int, txfm_buf[8 * 4 + 8 + 8]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    for (int j = 0; j < txfm_size_col; j++)
      temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);

    row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
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

void lowbd_inv_txfm2d_add_4x16_neon(const int32_t *input, uint8_t *output,
                                    int stride, TX_TYPE tx_type,
                                    TX_SIZE tx_size, int eob) {
  (void)eob;
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 16 + 16 + 16]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
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

void lowbd_inv_txfm2d_add_16x4_neon(const int32_t *input, uint8_t *output,
                                    int stride, TX_TYPE tx_type,
                                    TX_SIZE tx_size, int eob) {
  (void)eob;

  DECLARE_ALIGNED(32, int, txfm_buf[16 * 4 + 16 + 16]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
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

static INLINE void lowbd_inv_txfm2d_add_no_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  DECLARE_ALIGNED(32, int, txfm_buf[64 * 64 + 64 + 64]);
  int32_t *temp_in = txfm_buf;

  int eobx, eoby, ud_flip, lr_flip, row_start;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);

  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  const int bd = 8;
  int r;

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  row_start = (buf_size_nonzero_h_div8 << 3);

  for (int i = 0; i < row_start; i++) {
    if (abs(rect_type) == 1) {
      for (int j = 0; j < txfm_size_col; j++)
        temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);
      row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    } else {
      row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  // Doing memset for the rows which are not processed in row transform.
  memset(buf_ptr, 0,
         sizeof(int32_t) * txfm_size_col * (txfm_size_row - row_start));

  for (int c = 0; c < txfm_size_col; c++) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
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

static INLINE void lowbd_inv_txfm2d_add_universe_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  switch (tx_type) {
    case IDTX:
      lowbd_inv_txfm2d_add_idtx_neon(input, output, stride, tx_type, tx_size,
                                     eob);
      break;

    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      lowbd_inv_txfm2d_add_v_identity_neon(input, output, stride, tx_type,
                                           tx_size, eob);
      break;

    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      lowbd_inv_txfm2d_add_h_identity_neon(input, output, stride, tx_type,
                                           tx_size, eob);
      break;

    default:
      lowbd_inv_txfm2d_add_no_identity_neon(input, output, stride, tx_type,
                                            tx_size, eob);
      break;
  }
}
void av1_lowbd_inv_txfm2d_add_neon(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob) {
  int row;
  switch (tx_size) {
    case TX_4X4:
      lowbd_inv_txfm2d_add_4x4_neon(input, output, stride, tx_type, tx_size,
                                    eob);
      break;

    case TX_4X8:
      lowbd_inv_txfm2d_add_4x8_neon(input, output, stride, tx_type, tx_size,
                                    eob);
      break;

    case TX_8X4:
      lowbd_inv_txfm2d_add_8x4_neon(input, output, stride, tx_type, tx_size,
                                    eob);
      break;

    case TX_4X16:
      lowbd_inv_txfm2d_add_4x16_neon(input, output, stride, tx_type, tx_size,
                                     eob);
      break;

    case TX_16X4:
      lowbd_inv_txfm2d_add_16x4_neon(input, output, stride, tx_type, tx_size,
                                     eob);
      break;

    case TX_16X64: {
      lowbd_inv_txfm2d_add_universe_neon(input, output, stride, tx_type,
                                         tx_size, eob);
    } break;

    case TX_64X16: {
      int32_t mod_input[64 * 16];
      for (row = 0; row < 16; ++row) {
        memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
        memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
      }
      lowbd_inv_txfm2d_add_universe_neon(mod_input, output, stride, tx_type,
                                         tx_size, eob);
    } break;

    case TX_32X64: {
      lowbd_inv_txfm2d_add_universe_neon(input, output, stride, tx_type,
                                         tx_size, eob);
    } break;

    case TX_64X32: {
      int32_t mod_input[64 * 32];
      for (row = 0; row < 32; ++row) {
        memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
        memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
      }
      lowbd_inv_txfm2d_add_universe_neon(mod_input, output, stride, tx_type,
                                         tx_size, eob);
    } break;

    case TX_64X64: {
      int32_t mod_input[64 * 64];
      for (row = 0; row < 32; ++row) {
        memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
        memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
      }
      lowbd_inv_txfm2d_add_universe_neon(mod_input, output, stride, tx_type,
                                         tx_size, eob);
    } break;

    default:
      lowbd_inv_txfm2d_add_universe_neon(input, output, stride, tx_type,
                                         tx_size, eob);
      break;
  }
}
void av1_inv_txfm_add_neon(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (!txfm_param->lossless) {
    av1_lowbd_inv_txfm2d_add_neon(dqcoeff, dst, stride, tx_type,
                                  txfm_param->tx_size, txfm_param->eob);
  } else {
    av1_inv_txfm_add_c(dqcoeff, dst, stride, txfm_param);
  }
}
