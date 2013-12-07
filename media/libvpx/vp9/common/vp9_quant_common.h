/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_QUANT_COMMON_H_
#define VP9_COMMON_VP9_QUANT_COMMON_H_

#include "vp9/common/vp9_blockd.h"

#define MINQ 0
#define MAXQ 255
#define QINDEX_RANGE (MAXQ - MINQ + 1)
#define QINDEX_BITS 8

void vp9_init_quant_tables();

int16_t vp9_dc_quant(int qindex, int delta);
int16_t vp9_ac_quant(int qindex, int delta);

int vp9_get_qindex(struct segmentation *seg, int segment_id, int base_qindex);

#endif  // VP9_COMMON_VP9_QUANT_COMMON_H_
