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

#include <assert.h>

#include "av1/common/av1_loopfilter.h"
#include "av1/common/blockd.h"
#include "av1/common/seg_common.h"
#include "av1/common/quant_common.h"

#if CONFIG_LOOPFILTER_LEVEL
static const int seg_feature_data_signed[SEG_LVL_MAX] = { 1, 1, 1, 1, 0, 0 };

static const int seg_feature_data_max[SEG_LVL_MAX] = {
  MAXQ, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, 0
};
#else
static const int seg_feature_data_signed[SEG_LVL_MAX] = { 1, 1, 0, 0 };

static const int seg_feature_data_max[SEG_LVL_MAX] = { MAXQ, MAX_LOOP_FILTER, 3,
                                                       0 };
#endif  // CONFIG_LOOPFILTER_LEVEL

// These functions provide access to new segment level features.
// Eventually these function may be "optimized out" but for the moment,
// the coding mechanism is still subject to change so these provide a
// convenient single point of change.

void av1_clearall_segfeatures(struct segmentation *seg) {
  av1_zero(seg->feature_data);
  av1_zero(seg->feature_mask);
}

void av1_enable_segfeature(struct segmentation *seg, int segment_id,
                           SEG_LVL_FEATURES feature_id) {
  seg->feature_mask[segment_id] |= 1 << feature_id;
}

int av1_seg_feature_data_max(SEG_LVL_FEATURES feature_id) {
  return seg_feature_data_max[feature_id];
}

int av1_is_segfeature_signed(SEG_LVL_FEATURES feature_id) {
  return seg_feature_data_signed[feature_id];
}

void av1_set_segdata(struct segmentation *seg, int segment_id,
                     SEG_LVL_FEATURES feature_id, int seg_data) {
  if (seg_data < 0) {
    assert(seg_feature_data_signed[feature_id]);
    assert(-seg_data <= seg_feature_data_max[feature_id]);
  } else {
    assert(seg_data <= seg_feature_data_max[feature_id]);
  }

  seg->feature_data[segment_id][feature_id] = seg_data;
}

const aom_tree_index av1_segment_tree[TREE_SIZE(MAX_SEGMENTS)] = {
  2, 4, 6, 8, 10, 12, 0, -1, -2, -3, -4, -5, -6, -7
};

// TBD? Functions to read and write segment data with range / validity checking
