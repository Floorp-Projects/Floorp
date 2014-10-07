/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_AQ_CYCLICREFRESH_H_
#define VP9_ENCODER_VP9_AQ_CYCLICREFRESH_H_

#include "vp9/common/vp9_blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

struct VP9_COMP;

struct CYCLIC_REFRESH;
typedef struct CYCLIC_REFRESH CYCLIC_REFRESH;

CYCLIC_REFRESH *vp9_cyclic_refresh_alloc(int mi_rows, int mi_cols);

void vp9_cyclic_refresh_free(CYCLIC_REFRESH *cr);

// Prior to coding a given prediction block, of size bsize at (mi_row, mi_col),
// check if we should reset the segment_id, and update the cyclic_refresh map
// and segmentation map.
void vp9_cyclic_refresh_update_segment(struct VP9_COMP *const cpi,
                                       MB_MODE_INFO *const mbmi,
                                       int mi_row, int mi_col,
                                       BLOCK_SIZE bsize, int use_rd);

// Setup cyclic background refresh: set delta q and segmentation map.
void vp9_cyclic_refresh_setup(struct VP9_COMP *const cpi);

void vp9_cyclic_refresh_set_rate_and_dist_sb(CYCLIC_REFRESH *cr,
                                             int64_t rate_sb, int64_t dist_sb);

int vp9_cyclic_refresh_get_rdmult(const CYCLIC_REFRESH *cr);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_AQ_CYCLICREFRESH_H_
