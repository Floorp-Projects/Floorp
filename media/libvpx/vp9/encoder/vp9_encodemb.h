/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_ENCODEMB_H_
#define VP9_ENCODER_VP9_ENCODEMB_H_

#include "./vpx_config.h"
#include "vp9/encoder/vp9_block.h"
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/common/vp9_onyxc_int.h"

typedef struct {
  MB_PREDICTION_MODE mode;
  MV_REFERENCE_FRAME ref_frame;
  MV_REFERENCE_FRAME second_ref_frame;
} MODE_DEFINITION;

typedef struct {
  MV_REFERENCE_FRAME ref_frame;
  MV_REFERENCE_FRAME second_ref_frame;
} REF_DEFINITION;

struct optimize_ctx {
  ENTROPY_CONTEXT ta[MAX_MB_PLANE][16];
  ENTROPY_CONTEXT tl[MAX_MB_PLANE][16];
};

struct encode_b_args {
  MACROBLOCK *x;
  struct optimize_ctx *ctx;
};

void vp9_encode_sb(MACROBLOCK *x, BLOCK_SIZE bsize);
void vp9_encode_sby(MACROBLOCK *x, BLOCK_SIZE bsize);

void vp9_xform_quant(int plane, int block, BLOCK_SIZE plane_bsize,
                     TX_SIZE tx_size, void *arg);

void vp9_subtract_sby(MACROBLOCK *x, BLOCK_SIZE bsize);
void vp9_subtract_sbuv(MACROBLOCK *x, BLOCK_SIZE bsize);
void vp9_subtract_sb(MACROBLOCK *x, BLOCK_SIZE bsize);

void vp9_encode_intra_block_y(MACROBLOCK *x, BLOCK_SIZE bsize);
void vp9_encode_intra_block_uv(MACROBLOCK *x, BLOCK_SIZE bsize);


#endif  // VP9_ENCODER_VP9_ENCODEMB_H_
