/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_RDOPT_H_
#define VP9_ENCODER_VP9_RDOPT_H_

#define RDDIV_BITS          7

#define RDCOST(RM, DM, R, D) \
  (((128 + ((int64_t)R) * (RM)) >> 8) + (D << DM))
#define QIDX_SKIP_THRESH     115

struct TileInfo;

int vp9_compute_rd_mult(VP9_COMP *cpi, int qindex);

void vp9_initialize_rd_consts(VP9_COMP *cpi);

void vp9_initialize_me_consts(VP9_COMP *cpi, int qindex);

void vp9_rd_pick_intra_mode_sb(VP9_COMP *cpi, MACROBLOCK *x,
                               int *r, int64_t *d, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd);

int64_t vp9_rd_pick_inter_mode_sb(VP9_COMP *cpi, MACROBLOCK *x,
                                  const struct TileInfo *const tile,
                                  int mi_row, int mi_col,
                                  int *returnrate,
                                  int64_t *returndistortion,
                                  BLOCK_SIZE bsize,
                                  PICK_MODE_CONTEXT *ctx,
                                  int64_t best_rd_so_far);

int64_t vp9_rd_pick_inter_mode_sub8x8(VP9_COMP *cpi, MACROBLOCK *x,
                                      const struct TileInfo *const tile,
                                      int mi_row, int mi_col,
                                      int *returnrate,
                                      int64_t *returndistortion,
                                      BLOCK_SIZE bsize,
                                      PICK_MODE_CONTEXT *ctx,
                                      int64_t best_rd_so_far);

void vp9_init_me_luts();

void vp9_set_mbmode_and_mvs(MACROBLOCK *x,
                            MB_PREDICTION_MODE mb, int_mv *mv);

void vp9_get_entropy_contexts(TX_SIZE tx_size,
    ENTROPY_CONTEXT t_above[16], ENTROPY_CONTEXT t_left[16],
    const ENTROPY_CONTEXT *above, const ENTROPY_CONTEXT *left,
    int num_4x4_w, int num_4x4_h);

#endif  // VP9_ENCODER_VP9_RDOPT_H_
