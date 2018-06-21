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

#include "config/aom_config.h"

#include "av1/common/av1_loopfilter.h"
#include "aom_util/aom_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1Common;

typedef struct AV1LfMTInfo {
  int mi_row;
  int plane;
  int dir;
} AV1LfMTInfo;

// Loopfilter row synchronization
typedef struct AV1LfSyncData {
#if CONFIG_MULTITHREAD
  pthread_mutex_t *mutex_[MAX_MB_PLANE];
  pthread_cond_t *cond_[MAX_MB_PLANE];
#endif
  // Allocate memory to store the loop-filtered superblock index in each row.
  int *cur_sb_col[MAX_MB_PLANE];
  // The optimal sync_range for different resolution and platform should be
  // determined by testing. Currently, it is chosen to be a power-of-2 number.
  int sync_range;
  int rows;

  // Row-based parallel loopfilter data
  LFWorkerData *lfdata;
  int num_workers;

#if CONFIG_MULTITHREAD
  pthread_mutex_t *job_mutex;
#endif
  AV1LfMTInfo *job_queue;
  int jobs_enqueued;
  int jobs_dequeued;
} AV1LfSync;

typedef struct AV1LrMTInfo {
  int v_start;
  int v_end;
  int lr_unit_row;
  int plane;
  int sync_mode;
  int v_copy_start;
  int v_copy_end;
} AV1LrMTInfo;

typedef struct LoopRestorationWorkerData {
  int32_t *rst_tmpbuf;
  void *rlbs;
  void *lr_ctxt;
} LRWorkerData;

// Looprestoration row synchronization
typedef struct AV1LrSyncData {
#if CONFIG_MULTITHREAD
  pthread_mutex_t *mutex_[MAX_MB_PLANE];
  pthread_cond_t *cond_[MAX_MB_PLANE];
#endif
  // Allocate memory to store the loop-restoration block index in each row.
  int *cur_sb_col[MAX_MB_PLANE];
  // The optimal sync_range for different resolution and platform should be
  // determined by testing. Currently, it is chosen to be a power-of-2 number.
  int sync_range;
  int rows;
  int num_planes;

  int num_workers;

#if CONFIG_MULTITHREAD
  pthread_mutex_t *job_mutex;
#endif
  // Row-based parallel loopfilter data
  LRWorkerData *lrworkerdata;

  AV1LrMTInfo *job_queue;
  int jobs_enqueued;
  int jobs_dequeued;
} AV1LrSync;

// Deallocate loopfilter synchronization related mutex and data.
void av1_loop_filter_dealloc(AV1LfSync *lf_sync);

void av1_loop_filter_frame_mt(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                              struct macroblockd *mbd, int plane_start,
                              int plane_end, int partial_frame,
                              AVxWorker *workers, int num_workers,
                              AV1LfSync *lf_sync);
void av1_loop_restoration_filter_frame_mt(YV12_BUFFER_CONFIG *frame,
                                          struct AV1Common *cm,
                                          int optimized_lr, AVxWorker *workers,
                                          int num_workers, AV1LrSync *lr_sync,
                                          void *lr_ctxt);
void av1_loop_restoration_dealloc(AV1LrSync *lr_sync, int num_workers);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_LOOPFILTER_THREAD_H_
