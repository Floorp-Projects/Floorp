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

#ifndef AV1_COMMON_CFL_H_
#define AV1_COMMON_CFL_H_

#include <assert.h>

#include "av1/common/enums.h"

// Forward declaration of AV1_COMMON, in order to avoid creating a cyclic
// dependency by importing av1/common/onyxc_int.h
typedef struct AV1Common AV1_COMMON;

// Forward declaration of MACROBLOCK, in order to avoid creating a cyclic
// dependency by importing av1/common/blockd.h
typedef struct macroblockd MACROBLOCKD;

typedef struct {
  // Pixel buffer containing the luma pixels used as prediction for chroma
  uint8_t y_pix[MAX_SB_SQUARE];

  // Height and width of the luma prediction block currently in the pixel buffer
  int y_height, y_width;

  // Chroma subsampling
  int subsampling_x, subsampling_y;

  // CfL Performs its own block level DC_PRED for each chromatic plane
  double dc_pred[CFL_PRED_PLANES];

  // The rate associated with each alpha codeword
  int costs[CFL_ALPHABET_SIZE];

  // Count the number of TX blocks in a predicted block to know when you are at
  // the last one, so you can check for skips.
  // TODO(any) Is there a better way to do this?
  int num_tx_blk[CFL_PRED_PLANES];
} CFL_CTX;

static const double cfl_alpha_mags[CFL_MAGS_SIZE] = {
  0., 0.125, -0.125, 0.25, -0.25, 0.5, -0.5
};

static const int cfl_alpha_codes[CFL_ALPHABET_SIZE][CFL_PRED_PLANES] = {
  // barrbrain's simple 1D quant ordered by subset 3 likelihood
  { 0, 0 }, { 1, 1 }, { 3, 0 }, { 3, 1 }, { 1, 0 }, { 3, 3 },
  { 0, 1 }, { 5, 5 }, { 5, 3 }, { 1, 3 }, { 5, 3 }, { 3, 5 },
  { 0, 3 }, { 5, 1 }, { 1, 5 }, { 0, 5 }
};

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm, int subsampling_x,
              int subsampling_y);

void cfl_dc_pred(MACROBLOCKD *xd, BLOCK_SIZE plane_bsize, TX_SIZE tx_size);

static INLINE double cfl_idx_to_alpha(int alpha_idx, CFL_SIGN_TYPE alpha_sign,
                                      CFL_PRED_TYPE pred_type) {
  const int mag_idx = cfl_alpha_codes[alpha_idx][pred_type];
  const double abs_alpha = cfl_alpha_mags[mag_idx];
  if (alpha_sign == CFL_SIGN_POS) {
    return abs_alpha;
  } else {
    assert(abs_alpha != 0.0);
    assert(cfl_alpha_mags[mag_idx + 1] == -abs_alpha);
    return -abs_alpha;
  }
}

void cfl_predict_block(const CFL_CTX *cfl, uint8_t *dst, int dst_stride,
                       int row, int col, TX_SIZE tx_size, double dc_pred,
                       double alpha);

void cfl_store(CFL_CTX *cfl, const uint8_t *input, int input_stride, int row,
               int col, TX_SIZE tx_size);

double cfl_load(const CFL_CTX *cfl, uint8_t *output, int output_stride, int row,
                int col, int width, int height);
#endif  // AV1_COMMON_CFL_H_
