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

#ifndef AV1_ENCODER_MBGRAPH_H_
#define AV1_ENCODER_MBGRAPH_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  struct {
    int err;
    union {
      int_mv mv;
      PREDICTION_MODE mode;
    } m;
  } ref[TOTAL_REFS_PER_FRAME];
} MBGRAPH_MB_STATS;

typedef struct { MBGRAPH_MB_STATS *mb_stats; } MBGRAPH_FRAME_STATS;

struct AV1_COMP;

void av1_update_mbgraph_stats(struct AV1_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_MBGRAPH_H_
