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

// sum of fwd_shift_##
#if CONFIG_CHROMA_2X2
#if CONFIG_TX64X64
static const int8_t fwd_shift_sum[TX_SIZES] = { 3, 2, 1, 0, -2, -4 };
#else   // CONFIG_TX64X64
static const int8_t fwd_shift_sum[TX_SIZES] = { 3, 2, 1, 0, -2 };
#endif  // CONFIG_TX64X64
#else   // CONFIG_CHROMA_2X2
#if CONFIG_TX64X64
static const int8_t fwd_shift_sum[TX_SIZES] = { 2, 1, 0, -2, -4 };
#else  // CONFIG_TX64X64
static const int8_t fwd_shift_sum[TX_SIZES] = { 2, 1, 0, -2 };
#endif  // CONFIG_TX64X64
#endif  // CONFIG_CHROMA_2X2

//  ---------------- 4x4 1D config -----------------------
// shift
static const int8_t inv_shift_4[2] = { 0, -4 };

// stage range
static const int8_t inv_stage_range_col_dct_4[4] = { 3, 3, 2, 2 };
static const int8_t inv_stage_range_row_dct_4[4] = { 3, 3, 3, 3 };
static const int8_t inv_stage_range_col_adst_4[6] = { 3, 3, 3, 3, 2, 2 };
static const int8_t inv_stage_range_row_adst_4[6] = { 3, 3, 3, 3, 3, 3 };
static const int8_t inv_stage_range_idx_4[1] = { 0 };

// cos bit
static const int8_t inv_cos_bit_col_dct_4[4] = { 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_4[4] = { 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_4[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_4[6] = { 13, 13, 13, 13, 13, 13 };

//  ---------------- 8x8 1D constants -----------------------
// shift
static const int8_t inv_shift_8[2] = { 0, -5 };

// stage range
static const int8_t inv_stage_range_col_dct_8[6] = { 5, 5, 5, 5, 4, 4 };
static const int8_t inv_stage_range_row_dct_8[6] = { 5, 5, 5, 5, 5, 5 };
static const int8_t inv_stage_range_col_adst_8[8] = { 5, 5, 5, 5, 5, 5, 4, 4 };
static const int8_t inv_stage_range_row_adst_8[8] = { 5, 5, 5, 5, 5, 5, 5, 5 };
static const int8_t inv_stage_range_idx_8[1] = { 0 };

// cos bit
static const int8_t inv_cos_bit_col_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_8[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};
static const int8_t inv_cos_bit_row_adst_8[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};

//  ---------------- 16x16 1D constants -----------------------
// shift
static const int8_t inv_shift_16[2] = { -1, -5 };

// stage range
static const int8_t inv_stage_range_col_dct_16[8] = { 7, 7, 7, 7, 7, 7, 6, 6 };
static const int8_t inv_stage_range_row_dct_16[8] = { 7, 7, 7, 7, 7, 7, 7, 7 };
static const int8_t inv_stage_range_col_adst_16[10] = { 7, 7, 7, 7, 7,
                                                        7, 7, 7, 6, 6 };
static const int8_t inv_stage_range_row_adst_16[10] = { 7, 7, 7, 7, 7,
                                                        7, 7, 7, 7, 7 };
static const int8_t inv_stage_range_idx_16[1] = { 0 };

// cos bit
static const int8_t inv_cos_bit_col_dct_16[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};
static const int8_t inv_cos_bit_row_dct_16[8] = {
  12, 12, 12, 12, 12, 12, 12, 12
};
static const int8_t inv_cos_bit_col_adst_16[10] = { 13, 13, 13, 13, 13,
                                                    13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_16[10] = { 12, 12, 12, 12, 12,
                                                    12, 12, 12, 12, 12 };

//  ---------------- 32x32 1D constants -----------------------
// shift
static const int8_t inv_shift_32[2] = { -1, -5 };

// stage range
static const int8_t inv_stage_range_col_dct_32[10] = { 9, 9, 9, 9, 9,
                                                       9, 9, 9, 8, 8 };
static const int8_t inv_stage_range_row_dct_32[10] = { 9, 9, 9, 9, 9,
                                                       9, 9, 9, 9, 9 };
static const int8_t inv_stage_range_col_adst_32[12] = { 9, 9, 9, 9, 9, 9,
                                                        9, 9, 9, 9, 8, 8 };
static const int8_t inv_stage_range_row_adst_32[12] = { 9, 9, 9, 9, 9, 9,
                                                        9, 9, 9, 9, 9, 9 };
static const int8_t inv_stage_range_idx_32[1] = { 0 };

// cos bit
static const int8_t inv_cos_bit_col_dct_32[10] = { 13, 13, 13, 13, 13,
                                                   13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_32[10] = { 12, 12, 12, 12, 12,
                                                   12, 12, 12, 12, 12 };
static const int8_t inv_cos_bit_col_adst_32[12] = { 13, 13, 13, 13, 13, 13,
                                                    13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_32[12] = { 12, 12, 12, 12, 12, 12,
                                                    12, 12, 12, 12, 12, 12 };

//  ---------------- 64x64 1D constants -----------------------
// shift
static const int8_t inv_shift_64[2] = { -1, -5 };

// stage range
static const int8_t inv_stage_range_col_dct_64[12] = { 11, 11, 11, 11, 11, 11,
                                                       11, 11, 11, 11, 10, 10 };
static const int8_t inv_stage_range_row_dct_64[12] = { 11, 11, 11, 11, 11, 11,
                                                       11, 11, 11, 11, 11, 11 };

static const int8_t inv_stage_range_idx_64[1] = { 0 };

// cos bit
static const int8_t inv_cos_bit_col_dct_64[12] = { 13, 13, 13, 13, 13, 13,
                                                   13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_64[12] = { 12, 12, 12, 12, 12, 12,
                                                   12, 12, 12, 12, 12, 12 };

//  ---------------- row config inv_dct_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_4 = {
  4,                          // .txfm_size
  4,                          // .stage_num
  inv_shift_4,                // .shift
  inv_stage_range_row_dct_4,  // .stage_range
  inv_cos_bit_row_dct_4,      // .cos_bit
  TXFM_TYPE_DCT4              // .txfm_type
};

//  ---------------- row config inv_dct_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_8 = {
  8,                          // .txfm_size
  6,                          // .stage_num
  inv_shift_8,                // .shift
  inv_stage_range_row_dct_8,  // .stage_range
  inv_cos_bit_row_dct_8,      // .cos_bit_
  TXFM_TYPE_DCT8              // .txfm_type
};
//  ---------------- row config inv_dct_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_16 = {
  16,                          // .txfm_size
  8,                           // .stage_num
  inv_shift_16,                // .shift
  inv_stage_range_row_dct_16,  // .stage_range
  inv_cos_bit_row_dct_16,      // .cos_bit
  TXFM_TYPE_DCT16              // .txfm_type
};

//  ---------------- row config inv_dct_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_32 = {
  32,                          // .txfm_size
  10,                          // .stage_num
  inv_shift_32,                // .shift
  inv_stage_range_row_dct_32,  // .stage_range
  inv_cos_bit_row_dct_32,      // .cos_bit_row
  TXFM_TYPE_DCT32              // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- row config inv_dct_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_64 = {
  64,                          // .txfm_size
  12,                          // .stage_num
  inv_shift_64,                // .shift
  inv_stage_range_row_dct_64,  // .stage_range
  inv_cos_bit_row_dct_64,      // .cos_bit
  TXFM_TYPE_DCT64,             // .txfm_type_col
};
#endif  // CONFIG_TX64X64

//  ---------------- row config inv_adst_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_4 = {
  4,                           // .txfm_size
  6,                           // .stage_num
  inv_shift_4,                 // .shift
  inv_stage_range_row_adst_4,  // .stage_range
  inv_cos_bit_row_adst_4,      // .cos_bit
  TXFM_TYPE_ADST4,             // .txfm_type
};

//  ---------------- row config inv_adst_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_8 = {
  8,                           // .txfm_size
  8,                           // .stage_num
  inv_shift_8,                 // .shift
  inv_stage_range_row_adst_8,  // .stage_range
  inv_cos_bit_row_adst_8,      // .cos_bit
  TXFM_TYPE_ADST8,             // .txfm_type_col
};

//  ---------------- row config inv_adst_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_16 = {
  16,                           // .txfm_size
  10,                           // .stage_num
  inv_shift_16,                 // .shift
  inv_stage_range_row_adst_16,  // .stage_range
  inv_cos_bit_row_adst_16,      // .cos_bit
  TXFM_TYPE_ADST16,             // .txfm_type
};

//  ---------------- row config inv_adst_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_32 = {
  32,                           // .txfm_size
  12,                           // .stage_num
  inv_shift_32,                 // .shift
  inv_stage_range_row_adst_32,  // .stage_range
  inv_cos_bit_row_adst_32,      // .cos_bit
  TXFM_TYPE_ADST32,             // .txfm_type
};

//  ---------------- col config inv_dct_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_4 = {
  4,                          // .txfm_size
  4,                          // .stage_num
  inv_shift_4,                // .shift
  inv_stage_range_col_dct_4,  // .stage_range
  inv_cos_bit_col_dct_4,      // .cos_bit
  TXFM_TYPE_DCT4              // .txfm_type
};

//  ---------------- col config inv_dct_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8 = {
  8,                          // .txfm_size
  6,                          // .stage_num
  inv_shift_8,                // .shift
  inv_stage_range_col_dct_8,  // .stage_range
  inv_cos_bit_col_dct_8,      // .cos_bit_
  TXFM_TYPE_DCT8              // .txfm_type
};
//  ---------------- col config inv_dct_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16 = {
  16,                          // .txfm_size
  8,                           // .stage_num
  inv_shift_16,                // .shift
  inv_stage_range_col_dct_16,  // .stage_range
  inv_cos_bit_col_dct_16,      // .cos_bit
  TXFM_TYPE_DCT16              // .txfm_type
};

//  ---------------- col config inv_dct_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32 = {
  32,                          // .txfm_size
  10,                          // .stage_num
  inv_shift_32,                // .shift
  inv_stage_range_col_dct_32,  // .stage_range
  inv_cos_bit_col_dct_32,      // .cos_bit_col
  TXFM_TYPE_DCT32              // .txfm_type
};

//  ---------------- col config inv_dct_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_64 = {
  64,                          // .txfm_size
  12,                          // .stage_num
  inv_shift_64,                // .shift
  inv_stage_range_col_dct_64,  // .stage_range
  inv_cos_bit_col_dct_64,      // .cos_bit
  TXFM_TYPE_DCT64,             // .txfm_type_col
};

//  ---------------- col config inv_adst_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_4 = {
  4,                           // .txfm_size
  6,                           // .stage_num
  inv_shift_4,                 // .shift
  inv_stage_range_col_adst_4,  // .stage_range
  inv_cos_bit_col_adst_4,      // .cos_bit
  TXFM_TYPE_ADST4,             // .txfm_type
};

//  ---------------- col config inv_adst_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8 = {
  8,                           // .txfm_size
  8,                           // .stage_num
  inv_shift_8,                 // .shift
  inv_stage_range_col_adst_8,  // .stage_range
  inv_cos_bit_col_adst_8,      // .cos_bit
  TXFM_TYPE_ADST8,             // .txfm_type_col
};

//  ---------------- col config inv_adst_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16 = {
  16,                           // .txfm_size
  10,                           // .stage_num
  inv_shift_16,                 // .shift
  inv_stage_range_col_adst_16,  // .stage_range
  inv_cos_bit_col_adst_16,      // .cos_bit
  TXFM_TYPE_ADST16,             // .txfm_type
};

//  ---------------- col config inv_adst_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_32 = {
  32,                           // .txfm_size
  12,                           // .stage_num
  inv_shift_32,                 // .shift
  inv_stage_range_col_adst_32,  // .stage_range
  inv_cos_bit_col_adst_32,      // .cos_bit
  TXFM_TYPE_ADST32,             // .txfm_type
};

#if CONFIG_EXT_TX
// identity does not need to differentiate between row and col
//  ---------------- row/col config inv_identity_4 ----------
static const TXFM_1D_CFG inv_txfm_1d_cfg_identity_4 = {
  4,                      // .txfm_size
  1,                      // .stage_num
  inv_shift_4,            // .shift
  inv_stage_range_idx_4,  // .stage_range
  NULL,                   // .cos_bit
  TXFM_TYPE_IDENTITY4,    // .txfm_type
};

//  ---------------- row/col config inv_identity_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_cfg_identity_8 = {
  8,                      // .txfm_size
  1,                      // .stage_num
  inv_shift_8,            // .shift
  inv_stage_range_idx_8,  // .stage_range
  NULL,                   // .cos_bit
  TXFM_TYPE_IDENTITY8,    // .txfm_type
};

//  ---------------- row/col config inv_identity_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_cfg_identity_16 = {
  16,                      // .txfm_size
  1,                       // .stage_num
  inv_shift_16,            // .shift
  inv_stage_range_idx_16,  // .stage_range
  NULL,                    // .cos_bit
  TXFM_TYPE_IDENTITY16,    // .txfm_type
};

//  ---------------- row/col config inv_identity_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_cfg_identity_32 = {
  32,                      // .txfm_size
  1,                       // .stage_num
  inv_shift_32,            // .shift
  inv_stage_range_idx_32,  // .stage_range
  NULL,                    // .cos_bit
  TXFM_TYPE_IDENTITY32,    // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- row/col config inv_identity_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_cfg_identity_64 = {
  64,                      // .txfm_size
  1,                       // .stage_num
  inv_shift_64,            // .shift
  inv_stage_range_idx_64,  // .stage_range
  NULL,                    // .cos_bit
  TXFM_TYPE_IDENTITY64,    // .txfm_type
};
#endif  // CONFIG_TX64X64
#endif  // CONFIG_EXT_TX
#endif  // AV1_INV_TXFM2D_CFG_H_
