/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_SEG_COMMON_H_
#define VP9_COMMON_VP9_SEG_COMMON_H_

#include "vp9/common/vp9_prob.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEGMENT_DELTADATA   0
#define SEGMENT_ABSDATA     1

#define MAX_SEGMENTS     8
#define SEG_TREE_PROBS   (MAX_SEGMENTS-1)

#define PREDICTION_PROBS 3

// Segment level features.
typedef enum {
  SEG_LVL_ALT_Q = 0,               // Use alternate Quantizer ....
  SEG_LVL_ALT_LF = 1,              // Use alternate loop filter value...
  SEG_LVL_REF_FRAME = 2,           // Optional Segment reference frame
  SEG_LVL_SKIP = 3,                // Optional Segment (0,0) + skip mode
  SEG_LVL_MAX = 4                  // Number of features supported
} SEG_LVL_FEATURES;


struct segmentation {
  uint8_t enabled;
  uint8_t update_map;
  uint8_t update_data;
  uint8_t abs_delta;
  uint8_t temporal_update;

  vp9_prob tree_probs[SEG_TREE_PROBS];
  vp9_prob pred_probs[PREDICTION_PROBS];

  int16_t feature_data[MAX_SEGMENTS][SEG_LVL_MAX];
  unsigned int feature_mask[MAX_SEGMENTS];
};

int vp9_segfeature_active(const struct segmentation *seg,
                          int segment_id,
                          SEG_LVL_FEATURES feature_id);

void vp9_clearall_segfeatures(struct segmentation *seg);

void vp9_enable_segfeature(struct segmentation *seg,
                           int segment_id,
                           SEG_LVL_FEATURES feature_id);

int vp9_seg_feature_data_max(SEG_LVL_FEATURES feature_id);

int vp9_is_segfeature_signed(SEG_LVL_FEATURES feature_id);

void vp9_set_segdata(struct segmentation *seg,
                     int segment_id,
                     SEG_LVL_FEATURES feature_id,
                     int seg_data);

int vp9_get_segdata(const struct segmentation *seg,
                    int segment_id,
                    SEG_LVL_FEATURES feature_id);

extern const vp9_tree_index vp9_segment_tree[TREE_SIZE(MAX_SEGMENTS)];

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_SEG_COMMON_H_

