/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_PICKMODE_H_
#define VP9_ENCODER_VP9_PICKMODE_H_

#include "vp9/encoder/vp9_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void vp9_pick_inter_mode(VP9_COMP *cpi, MACROBLOCK *x,
                         const struct TileInfo *const tile,
                         int mi_row, int mi_col,
                         int *returnrate,
                         int64_t *returndistortion,
                         BLOCK_SIZE bsize,
                         PICK_MODE_CONTEXT *ctx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_PICKMODE_H_
