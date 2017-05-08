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

#ifndef AV1_INV_TXFM2D_CFG_H_
#define AV1_INV_TXFM2D_CFG_H_
#include "av1/common/av1_inv_txfm1d.h"
//  ---------------- config inv_dct_dct_4 ----------------
static const int8_t inv_shift_dct_dct_4[2] = { 0, -4 };
static const int8_t inv_stage_range_col_dct_dct_4[4] = { 18, 18, 17, 17 };
static const int8_t inv_stage_range_row_dct_dct_4[4] = { 18, 18, 18, 18 };
static const int8_t inv_cos_bit_col_dct_dct_4[4] = { 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_dct_4[4] = { 13, 13, 13, 13 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_dct_4 = {
  4,  // .txfm_size
  4,  // .stage_num_col
  4,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_dct_dct_4,            // .shift
  inv_stage_range_col_dct_dct_4,  // .stage_range_col
  inv_stage_range_row_dct_dct_4,  // .stage_range_row
  inv_cos_bit_col_dct_dct_4,      // .cos_bit_col
  inv_cos_bit_row_dct_dct_4,      // .cos_bit_row
  TXFM_TYPE_DCT4,                 // .txfm_type_col
  TXFM_TYPE_DCT4
};  // .txfm_type_row

//  ---------------- config inv_dct_dct_8 ----------------
static const int8_t inv_shift_dct_dct_8[2] = { 0, -5 };
static const int8_t inv_stage_range_col_dct_dct_8[6] = {
  19, 19, 19, 19, 18, 18
};
static const int8_t inv_stage_range_row_dct_dct_8[6] = {
  19, 19, 19, 19, 19, 19
};
static const int8_t inv_cos_bit_col_dct_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_dct_8[6] = { 13, 13, 13, 13, 13, 13 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_dct_8 = {
  8,  // .txfm_size
  6,  // .stage_num_col
  6,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_dct_dct_8,            // .shift
  inv_stage_range_col_dct_dct_8,  // .stage_range_col
  inv_stage_range_row_dct_dct_8,  // .stage_range_row
  inv_cos_bit_col_dct_dct_8,      // .cos_bit_col
  inv_cos_bit_row_dct_dct_8,      // .cos_bit_row
  TXFM_TYPE_DCT8,                 // .txfm_type_col
  TXFM_TYPE_DCT8
};  // .txfm_type_row

//  ---------------- config inv_dct_dct_16 ----------------
static const int8_t inv_shift_dct_dct_16[2] = { -1, -5 };
static const int8_t inv_stage_range_col_dct_dct_16[8] = { 19, 19, 19, 19,
                                                          19, 19, 18, 18 };
static const int8_t inv_stage_range_row_dct_dct_16[8] = { 20, 20, 20, 20,
                                                          20, 20, 20, 20 };
static const int8_t inv_cos_bit_col_dct_dct_16[8] = { 13, 13, 13, 13,
                                                      13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_dct_16[8] = { 12, 12, 12, 12,
                                                      12, 12, 12, 12 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_dct_16 = {
  16,  // .txfm_size
  8,   // .stage_num_col
  8,   // .stage_num_row
  // 0,  // .log_scale
  inv_shift_dct_dct_16,            // .shift
  inv_stage_range_col_dct_dct_16,  // .stage_range_col
  inv_stage_range_row_dct_dct_16,  // .stage_range_row
  inv_cos_bit_col_dct_dct_16,      // .cos_bit_col
  inv_cos_bit_row_dct_dct_16,      // .cos_bit_row
  TXFM_TYPE_DCT16,                 // .txfm_type_col
  TXFM_TYPE_DCT16
};  // .txfm_type_row

//  ---------------- config inv_dct_dct_32 ----------------
static const int8_t inv_shift_dct_dct_32[2] = { -1, -5 };
static const int8_t inv_stage_range_col_dct_dct_32[10] = { 19, 19, 19, 19, 19,
                                                           19, 19, 19, 18, 18 };
static const int8_t inv_stage_range_row_dct_dct_32[10] = { 20, 20, 20, 20, 20,
                                                           20, 20, 20, 20, 20 };
static const int8_t inv_cos_bit_col_dct_dct_32[10] = { 13, 13, 13, 13, 13,
                                                       13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_dct_32[10] = { 12, 12, 12, 12, 12,
                                                       12, 12, 12, 12, 12 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_dct_32 = {
  32,  // .txfm_size
  10,  // .stage_num_col
  10,  // .stage_num_row
  // 1,  // .log_scale
  inv_shift_dct_dct_32,            // .shift
  inv_stage_range_col_dct_dct_32,  // .stage_range_col
  inv_stage_range_row_dct_dct_32,  // .stage_range_row
  inv_cos_bit_col_dct_dct_32,      // .cos_bit_col
  inv_cos_bit_row_dct_dct_32,      // .cos_bit_row
  TXFM_TYPE_DCT32,                 // .txfm_type_col
  TXFM_TYPE_DCT32
};  // .txfm_type_row

//  ---------------- config inv_dct_dct_64 ----------------
static const int8_t inv_shift_dct_dct_64[2] = { -1, -7 };
static const int8_t inv_stage_range_col_dct_dct_64[12] = {
  19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 18, 18
};
static const int8_t inv_stage_range_row_dct_dct_64[12] = {
  20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20
};
static const int8_t inv_cos_bit_col_dct_dct_64[12] = { 13, 13, 13, 13, 13, 13,
                                                       13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_dct_64[12] = { 12, 12, 12, 12, 12, 12,
                                                       12, 12, 12, 12, 12, 12 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_dct_64 = {
  64,                              // .txfm_size
  12,                              // .stage_num_col
  12,                              // .stage_num_row
  inv_shift_dct_dct_64,            // .shift
  inv_stage_range_col_dct_dct_64,  // .stage_range_col
  inv_stage_range_row_dct_dct_64,  // .stage_range_row
  inv_cos_bit_col_dct_dct_64,      // .cos_bit_col
  inv_cos_bit_row_dct_dct_64,      // .cos_bit_row
  TXFM_TYPE_DCT64,                 // .txfm_type_col
  TXFM_TYPE_DCT64
};  // .txfm_type_row

//  ---------------- config inv_dct_adst_4 ----------------
static const int8_t inv_shift_dct_adst_4[2] = { 0, -4 };
static const int8_t inv_stage_range_col_dct_adst_4[4] = { 18, 18, 17, 17 };
static const int8_t inv_stage_range_row_dct_adst_4[6] = {
  18, 18, 18, 18, 18, 18
};
static const int8_t inv_cos_bit_col_dct_adst_4[4] = { 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_adst_4[6] = { 13, 13, 13, 13, 13, 13 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_adst_4 = {
  4,  // .txfm_size
  4,  // .stage_num_col
  6,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_dct_adst_4,            // .shift
  inv_stage_range_col_dct_adst_4,  // .stage_range_col
  inv_stage_range_row_dct_adst_4,  // .stage_range_row
  inv_cos_bit_col_dct_adst_4,      // .cos_bit_col
  inv_cos_bit_row_dct_adst_4,      // .cos_bit_row
  TXFM_TYPE_DCT4,                  // .txfm_type_col
  TXFM_TYPE_ADST4
};  // .txfm_type_row

//  ---------------- config inv_dct_adst_8 ----------------
static const int8_t inv_shift_dct_adst_8[2] = { 0, -5 };
static const int8_t inv_stage_range_col_dct_adst_8[6] = {
  19, 19, 19, 19, 18, 18
};
static const int8_t inv_stage_range_row_dct_adst_8[8] = { 19, 19, 19, 19,
                                                          19, 19, 19, 19 };
static const int8_t inv_cos_bit_col_dct_adst_8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_adst_8[8] = { 13, 13, 13, 13,
                                                      13, 13, 13, 13 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_adst_8 = {
  8,  // .txfm_size
  6,  // .stage_num_col
  8,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_dct_adst_8,            // .shift
  inv_stage_range_col_dct_adst_8,  // .stage_range_col
  inv_stage_range_row_dct_adst_8,  // .stage_range_row
  inv_cos_bit_col_dct_adst_8,      // .cos_bit_col
  inv_cos_bit_row_dct_adst_8,      // .cos_bit_row
  TXFM_TYPE_DCT8,                  // .txfm_type_col
  TXFM_TYPE_ADST8
};  // .txfm_type_row

//  ---------------- config inv_dct_adst_16 ----------------
static const int8_t inv_shift_dct_adst_16[2] = { -1, -5 };
static const int8_t inv_stage_range_col_dct_adst_16[8] = { 19, 19, 19, 19,
                                                           19, 19, 18, 18 };
static const int8_t inv_stage_range_row_dct_adst_16[10] = {
  20, 20, 20, 20, 20, 20, 20, 20, 20, 20
};
static const int8_t inv_cos_bit_col_dct_adst_16[8] = { 13, 13, 13, 13,
                                                       13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_adst_16[10] = { 12, 12, 12, 12, 12,
                                                        12, 12, 12, 12, 12 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_adst_16 = {
  16,  // .txfm_size
  8,   // .stage_num_col
  10,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_dct_adst_16,            // .shift
  inv_stage_range_col_dct_adst_16,  // .stage_range_col
  inv_stage_range_row_dct_adst_16,  // .stage_range_row
  inv_cos_bit_col_dct_adst_16,      // .cos_bit_col
  inv_cos_bit_row_dct_adst_16,      // .cos_bit_row
  TXFM_TYPE_DCT16,                  // .txfm_type_col
  TXFM_TYPE_ADST16
};  // .txfm_type_row

//  ---------------- config inv_dct_adst_32 ----------------
static const int8_t inv_shift_dct_adst_32[2] = { -1, -5 };
static const int8_t inv_stage_range_col_dct_adst_32[10] = {
  19, 19, 19, 19, 19, 19, 19, 19, 18, 18
};
static const int8_t inv_stage_range_row_dct_adst_32[12] = {
  20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20
};
static const int8_t inv_cos_bit_col_dct_adst_32[10] = { 13, 13, 13, 13, 13,
                                                        13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_adst_32[12] = {
  12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12
};

static const TXFM_2D_CFG inv_txfm_2d_cfg_dct_adst_32 = {
  32,  // .txfm_size
  10,  // .stage_num_col
  12,  // .stage_num_row
  // 1,  // .log_scale
  inv_shift_dct_adst_32,            // .shift
  inv_stage_range_col_dct_adst_32,  // .stage_range_col
  inv_stage_range_row_dct_adst_32,  // .stage_range_row
  inv_cos_bit_col_dct_adst_32,      // .cos_bit_col
  inv_cos_bit_row_dct_adst_32,      // .cos_bit_row
  TXFM_TYPE_DCT32,                  // .txfm_type_col
  TXFM_TYPE_ADST32
};  // .txfm_type_row

//  ---------------- config inv_adst_adst_4 ----------------
static const int8_t inv_shift_adst_adst_4[2] = { 0, -4 };
static const int8_t inv_stage_range_col_adst_adst_4[6] = { 18, 18, 18,
                                                           18, 17, 17 };
static const int8_t inv_stage_range_row_adst_adst_4[6] = { 18, 18, 18,
                                                           18, 18, 18 };
static const int8_t inv_cos_bit_col_adst_adst_4[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_adst_4[6] = { 13, 13, 13, 13, 13, 13 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_adst_adst_4 = {
  4,  // .txfm_size
  6,  // .stage_num_col
  6,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_adst_adst_4,            // .shift
  inv_stage_range_col_adst_adst_4,  // .stage_range_col
  inv_stage_range_row_adst_adst_4,  // .stage_range_row
  inv_cos_bit_col_adst_adst_4,      // .cos_bit_col
  inv_cos_bit_row_adst_adst_4,      // .cos_bit_row
  TXFM_TYPE_ADST4,                  // .txfm_type_col
  TXFM_TYPE_ADST4
};  // .txfm_type_row

//  ---------------- config inv_adst_adst_8 ----------------
static const int8_t inv_shift_adst_adst_8[2] = { 0, -5 };
static const int8_t inv_stage_range_col_adst_adst_8[8] = { 19, 19, 19, 19,
                                                           19, 19, 18, 18 };
static const int8_t inv_stage_range_row_adst_adst_8[8] = { 19, 19, 19, 19,
                                                           19, 19, 19, 19 };
static const int8_t inv_cos_bit_col_adst_adst_8[8] = { 13, 13, 13, 13,
                                                       13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_adst_8[8] = { 13, 13, 13, 13,
                                                       13, 13, 13, 13 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_adst_adst_8 = {
  8,  // .txfm_size
  8,  // .stage_num_col
  8,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_adst_adst_8,            // .shift
  inv_stage_range_col_adst_adst_8,  // .stage_range_col
  inv_stage_range_row_adst_adst_8,  // .stage_range_row
  inv_cos_bit_col_adst_adst_8,      // .cos_bit_col
  inv_cos_bit_row_adst_adst_8,      // .cos_bit_row
  TXFM_TYPE_ADST8,                  // .txfm_type_col
  TXFM_TYPE_ADST8
};  // .txfm_type_row

//  ---------------- config inv_adst_adst_16 ----------------
static const int8_t inv_shift_adst_adst_16[2] = { -1, -5 };
static const int8_t inv_stage_range_col_adst_adst_16[10] = {
  19, 19, 19, 19, 19, 19, 19, 19, 18, 18
};
static const int8_t inv_stage_range_row_adst_adst_16[10] = {
  20, 20, 20, 20, 20, 20, 20, 20, 20, 20
};
static const int8_t inv_cos_bit_col_adst_adst_16[10] = { 13, 13, 13, 13, 13,
                                                         13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_adst_16[10] = { 12, 12, 12, 12, 12,
                                                         12, 12, 12, 12, 12 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_adst_adst_16 = {
  16,  // .txfm_size
  10,  // .stage_num_col
  10,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_adst_adst_16,            // .shift
  inv_stage_range_col_adst_adst_16,  // .stage_range_col
  inv_stage_range_row_adst_adst_16,  // .stage_range_row
  inv_cos_bit_col_adst_adst_16,      // .cos_bit_col
  inv_cos_bit_row_adst_adst_16,      // .cos_bit_row
  TXFM_TYPE_ADST16,                  // .txfm_type_col
  TXFM_TYPE_ADST16
};  // .txfm_type_row

//  ---------------- config inv_adst_adst_32 ----------------
static const int8_t inv_shift_adst_adst_32[2] = { -1, -5 };
static const int8_t inv_stage_range_col_adst_adst_32[12] = {
  19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 18, 18
};
static const int8_t inv_stage_range_row_adst_adst_32[12] = {
  20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20
};
static const int8_t inv_cos_bit_col_adst_adst_32[12] = {
  13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13
};
static const int8_t inv_cos_bit_row_adst_adst_32[12] = {
  12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12
};

static const TXFM_2D_CFG inv_txfm_2d_cfg_adst_adst_32 = {
  32,  // .txfm_size
  12,  // .stage_num_col
  12,  // .stage_num_row
  // 1,  // .log_scale
  inv_shift_adst_adst_32,            // .shift
  inv_stage_range_col_adst_adst_32,  // .stage_range_col
  inv_stage_range_row_adst_adst_32,  // .stage_range_row
  inv_cos_bit_col_adst_adst_32,      // .cos_bit_col
  inv_cos_bit_row_adst_adst_32,      // .cos_bit_row
  TXFM_TYPE_ADST32,                  // .txfm_type_col
  TXFM_TYPE_ADST32
};  // .txfm_type_row

//  ---------------- config inv_adst_dct_4 ----------------
static const int8_t inv_shift_adst_dct_4[2] = { 0, -4 };
static const int8_t inv_stage_range_col_adst_dct_4[6] = {
  18, 18, 18, 18, 17, 17
};
static const int8_t inv_stage_range_row_adst_dct_4[4] = { 18, 18, 18, 18 };
static const int8_t inv_cos_bit_col_adst_dct_4[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_dct_4[4] = { 13, 13, 13, 13 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_adst_dct_4 = {
  4,  // .txfm_size
  6,  // .stage_num_col
  4,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_adst_dct_4,            // .shift
  inv_stage_range_col_adst_dct_4,  // .stage_range_col
  inv_stage_range_row_adst_dct_4,  // .stage_range_row
  inv_cos_bit_col_adst_dct_4,      // .cos_bit_col
  inv_cos_bit_row_adst_dct_4,      // .cos_bit_row
  TXFM_TYPE_ADST4,                 // .txfm_type_col
  TXFM_TYPE_DCT4
};  // .txfm_type_row

//  ---------------- config inv_adst_dct_8 ----------------
static const int8_t inv_shift_adst_dct_8[2] = { 0, -5 };
static const int8_t inv_stage_range_col_adst_dct_8[8] = { 19, 19, 19, 19,
                                                          19, 19, 18, 18 };
static const int8_t inv_stage_range_row_adst_dct_8[6] = {
  19, 19, 19, 19, 19, 19
};
static const int8_t inv_cos_bit_col_adst_dct_8[8] = { 13, 13, 13, 13,
                                                      13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_dct_8[6] = { 13, 13, 13, 13, 13, 13 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_adst_dct_8 = {
  8,  // .txfm_size
  8,  // .stage_num_col
  6,  // .stage_num_row
  // 0,  // .log_scale
  inv_shift_adst_dct_8,            // .shift
  inv_stage_range_col_adst_dct_8,  // .stage_range_col
  inv_stage_range_row_adst_dct_8,  // .stage_range_row
  inv_cos_bit_col_adst_dct_8,      // .cos_bit_col
  inv_cos_bit_row_adst_dct_8,      // .cos_bit_row
  TXFM_TYPE_ADST8,                 // .txfm_type_col
  TXFM_TYPE_DCT8
};  // .txfm_type_row

//  ---------------- config inv_adst_dct_16 ----------------
static const int8_t inv_shift_adst_dct_16[2] = { -1, -5 };
static const int8_t inv_stage_range_col_adst_dct_16[10] = {
  19, 19, 19, 19, 19, 19, 19, 19, 18, 18
};
static const int8_t inv_stage_range_row_adst_dct_16[8] = { 20, 20, 20, 20,
                                                           20, 20, 20, 20 };
static const int8_t inv_cos_bit_col_adst_dct_16[10] = { 13, 13, 13, 13, 13,
                                                        13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_dct_16[8] = { 12, 12, 12, 12,
                                                       12, 12, 12, 12 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_adst_dct_16 = {
  16,  // .txfm_size
  10,  // .stage_num_col
  8,   // .stage_num_row
  // 0,  // .log_scale
  inv_shift_adst_dct_16,            // .shift
  inv_stage_range_col_adst_dct_16,  // .stage_range_col
  inv_stage_range_row_adst_dct_16,  // .stage_range_row
  inv_cos_bit_col_adst_dct_16,      // .cos_bit_col
  inv_cos_bit_row_adst_dct_16,      // .cos_bit_row
  TXFM_TYPE_ADST16,                 // .txfm_type_col
  TXFM_TYPE_DCT16
};  // .txfm_type_row

//  ---------------- config inv_adst_dct_32 ----------------
static const int8_t inv_shift_adst_dct_32[2] = { -1, -5 };
static const int8_t inv_stage_range_col_adst_dct_32[12] = {
  19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 18, 18
};
static const int8_t inv_stage_range_row_adst_dct_32[10] = {
  20, 20, 20, 20, 20, 20, 20, 20, 20, 20
};
static const int8_t inv_cos_bit_col_adst_dct_32[12] = {
  13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13
};
static const int8_t inv_cos_bit_row_adst_dct_32[10] = { 12, 12, 12, 12, 12,
                                                        12, 12, 12, 12, 12 };

static const TXFM_2D_CFG inv_txfm_2d_cfg_adst_dct_32 = {
  32,  // .txfm_size
  12,  // .stage_num_col
  10,  // .stage_num_row
  // 1,  // .log_scale
  inv_shift_adst_dct_32,            // .shift
  inv_stage_range_col_adst_dct_32,  // .stage_range_col
  inv_stage_range_row_adst_dct_32,  // .stage_range_row
  inv_cos_bit_col_adst_dct_32,      // .cos_bit_col
  inv_cos_bit_row_adst_dct_32,      // .cos_bit_row
  TXFM_TYPE_ADST32,                 // .txfm_type_col
  TXFM_TYPE_DCT32
};  // .txfm_type_row

extern const TXFM_2D_CFG *inv_txfm_cfg_ls[TX_TYPES][TX_SIZES];

#endif  // AV1_INV_TXFM2D_CFG_H_
