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

#ifndef AV1_ENCODER_SEGMENTATION_H_
#define AV1_ENCODER_SEGMENTATION_H_

#include "av1/common/blockd.h"
#include "av1/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_enable_segmentation(struct segmentation *seg);
void av1_disable_segmentation(struct segmentation *seg);

void av1_disable_segfeature(struct segmentation *seg, int segment_id,
                            SEG_LVL_FEATURES feature_id);
void av1_clear_segdata(struct segmentation *seg, int segment_id,
                       SEG_LVL_FEATURES feature_id);

// The values given for each segment can be either deltas (from the default
// value chosen for the frame) or absolute values.
//
// Valid range for abs values is (0-127 for MB_LVL_ALT_Q), (0-63 for
// SEGMENT_ALT_LF)
// Valid range for delta values are (+/-127 for MB_LVL_ALT_Q), (+/-63 for
// SEGMENT_ALT_LF)
//
// abs_delta = SEGMENT_DELTADATA (deltas) abs_delta = SEGMENT_ABSDATA (use
// the absolute values given).
void av1_set_segment_data(struct segmentation *seg, int8_t *feature_data,
                          unsigned char abs_delta);

void av1_choose_segmap_coding_method(AV1_COMMON *cm, MACROBLOCKD *xd);

void av1_reset_segment_features(AV1_COMMON *cm);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_SEGMENTATION_H_
