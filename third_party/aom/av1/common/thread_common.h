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

#ifndef AV1_COMMON_LOOPFILTER_THREAD_H_
#define AV1_COMMON_LOOPFILTER_THREAD_H_
#include "./aom_config.h"
#include "av1/common/av1_loopfilter.h"
#include "aom_util/aom_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1Common;
struct FRAME_COUNTS;

// Loopfilter row synchronization
typedef struct AV1LfSyncData {
#if CONFIG_MULTITHREAD
  pthread_mutex_t *mutex_;
  pthread_cond_t *cond_;
#endif
  // Allocate memory to store the loop-filtered superblock index in each row.
  int *cur_sb_col;
  // The optimal sync_range for different resolution and platform should be
  // determined by testing. Currently, it is chosen to be a power-of-2 number.
  int sync_range;
  int rows;

  // Row-based parallel loopfilter data
  LFWorkerData *lfdata;
  int num_workers;
} AV1LfSync;

// Allocate memory for loopfilter row synchronization.
void av1_loop_filter_alloc(AV1LfSync *lf_sync, struct AV1Common *cm, int rows,
                           int width, int num_workers);

// Deallocate loopfilter synchronization related mutex and data.
void av1_loop_filter_dealloc(AV1LfSync *lf_sync);

// Multi-threaded loopfilter that uses the tile threads.
void av1_loop_filter_frame_mt(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                              struct macroblockd_plane *planes,
                              int frame_filter_level,
#if CONFIG_LOOPFILTER_LEVEL
                              int frame_filter_level_r,
#endif
                              int y_only, int partial_frame, AVxWorker *workers,
                              int num_workers, AV1LfSync *lf_sync);

void av1_accumulate_frame_counts(struct FRAME_COUNTS *acc_counts,
                                 struct FRAME_COUNTS *counts);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_LOOPFILTER_THREAD_H_
