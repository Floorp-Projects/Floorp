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

#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/entropymode.h"
#include "av1/common/thread_common.h"
#include "av1/common/reconinter.h"

// Set up nsync by width.
static INLINE int get_sync_range(int width) {
  // nsync numbers are picked by testing. For example, for 4k
  // video, using 4 gives best performance.
  if (width < 640)
    return 1;
  else if (width <= 1280)
    return 2;
  else if (width <= 4096)
    return 4;
  else
    return 8;
}

static INLINE int get_lr_sync_range(int width) {
#if 0
  // nsync numbers are picked by testing. For example, for 4k
  // video, using 4 gives best performance.
  if (width < 640)
    return 1;
  else if (width <= 1280)
    return 2;
  else if (width <= 4096)
    return 4;
  else
    return 8;
#else
  (void)width;
  return 1;
#endif
}

// Allocate memory for lf row synchronization
static void loop_filter_alloc(AV1LfSync *lf_sync, AV1_COMMON *cm, int rows,
                              int width, int num_workers) {
  lf_sync->rows = rows;
#if CONFIG_MULTITHREAD
  {
    int i, j;

    for (j = 0; j < MAX_MB_PLANE; j++) {
      CHECK_MEM_ERROR(cm, lf_sync->mutex_[j],
                      aom_malloc(sizeof(*(lf_sync->mutex_[j])) * rows));
      if (lf_sync->mutex_[j]) {
        for (i = 0; i < rows; ++i) {
          pthread_mutex_init(&lf_sync->mutex_[j][i], NULL);
        }
      }

      CHECK_MEM_ERROR(cm, lf_sync->cond_[j],
                      aom_malloc(sizeof(*(lf_sync->cond_[j])) * rows));
      if (lf_sync->cond_[j]) {
        for (i = 0; i < rows; ++i) {
          pthread_cond_init(&lf_sync->cond_[j][i], NULL);
        }
      }
    }

    CHECK_MEM_ERROR(cm, lf_sync->job_mutex,
                    aom_malloc(sizeof(*(lf_sync->job_mutex))));
    if (lf_sync->job_mutex) {
      pthread_mutex_init(lf_sync->job_mutex, NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD
  CHECK_MEM_ERROR(cm, lf_sync->lfdata,
                  aom_malloc(num_workers * sizeof(*(lf_sync->lfdata))));
  lf_sync->num_workers = num_workers;

  for (int j = 0; j < MAX_MB_PLANE; j++) {
    CHECK_MEM_ERROR(cm, lf_sync->cur_sb_col[j],
                    aom_malloc(sizeof(*(lf_sync->cur_sb_col[j])) * rows));
  }
  CHECK_MEM_ERROR(
      cm, lf_sync->job_queue,
      aom_malloc(sizeof(*(lf_sync->job_queue)) * rows * MAX_MB_PLANE * 2));
  // Set up nsync.
  lf_sync->sync_range = get_sync_range(width);
}

// Deallocate lf synchronization related mutex and data
void av1_loop_filter_dealloc(AV1LfSync *lf_sync) {
  if (lf_sync != NULL) {
    int j;
#if CONFIG_MULTITHREAD
    int i;
    for (j = 0; j < MAX_MB_PLANE; j++) {
      if (lf_sync->mutex_[j] != NULL) {
        for (i = 0; i < lf_sync->rows; ++i) {
          pthread_mutex_destroy(&lf_sync->mutex_[j][i]);
        }
        aom_free(lf_sync->mutex_[j]);
      }
      if (lf_sync->cond_[j] != NULL) {
        for (i = 0; i < lf_sync->rows; ++i) {
          pthread_cond_destroy(&lf_sync->cond_[j][i]);
        }
        aom_free(lf_sync->cond_[j]);
      }
    }
    if (lf_sync->job_mutex != NULL) {
      pthread_mutex_destroy(lf_sync->job_mutex);
      aom_free(lf_sync->job_mutex);
    }
#endif  // CONFIG_MULTITHREAD
    aom_free(lf_sync->lfdata);
    for (j = 0; j < MAX_MB_PLANE; j++) {
      aom_free(lf_sync->cur_sb_col[j]);
    }

    aom_free(lf_sync->job_queue);
    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*lf_sync);
  }
}

static void loop_filter_data_reset(LFWorkerData *lf_data,
                                   YV12_BUFFER_CONFIG *frame_buffer,
                                   struct AV1Common *cm, MACROBLOCKD *xd) {
  struct macroblockd_plane *pd = xd->plane;
  lf_data->frame_buffer = frame_buffer;
  lf_data->cm = cm;
  lf_data->xd = xd;
  for (int i = 0; i < MAX_MB_PLANE; i++) {
    memcpy(&lf_data->planes[i].dst, &pd[i].dst, sizeof(lf_data->planes[i].dst));
    lf_data->planes[i].subsampling_x = pd[i].subsampling_x;
    lf_data->planes[i].subsampling_y = pd[i].subsampling_y;
  }
}

static INLINE void sync_read(AV1LfSync *const lf_sync, int r, int c,
                             int plane) {
#if CONFIG_MULTITHREAD
  const int nsync = lf_sync->sync_range;

  if (r && !(c & (nsync - 1))) {
    pthread_mutex_t *const mutex = &lf_sync->mutex_[plane][r - 1];
    pthread_mutex_lock(mutex);

    while (c > lf_sync->cur_sb_col[plane][r - 1] - nsync) {
      pthread_cond_wait(&lf_sync->cond_[plane][r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)lf_sync;
  (void)r;
  (void)c;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void sync_write(AV1LfSync *const lf_sync, int r, int c,
                              const int sb_cols, int plane) {
#if CONFIG_MULTITHREAD
  const int nsync = lf_sync->sync_range;
  int cur;
  // Only signal when there are enough filtered SB for next row to run.
  int sig = 1;

  if (c < sb_cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = sb_cols + nsync;
  }

  if (sig) {
    pthread_mutex_lock(&lf_sync->mutex_[plane][r]);

    lf_sync->cur_sb_col[plane][r] = cur;

    pthread_cond_broadcast(&lf_sync->cond_[plane][r]);
    pthread_mutex_unlock(&lf_sync->mutex_[plane][r]);
  }
#else
  (void)lf_sync;
  (void)r;
  (void)c;
  (void)sb_cols;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

static void enqueue_lf_jobs(AV1LfSync *lf_sync, AV1_COMMON *cm, int start,
                            int stop, int plane_start, int plane_end) {
  int mi_row, plane, dir;
  AV1LfMTInfo *lf_job_queue = lf_sync->job_queue;
  lf_sync->jobs_enqueued = 0;
  lf_sync->jobs_dequeued = 0;

  for (dir = 0; dir < 2; dir++) {
    for (plane = plane_start; plane < plane_end; plane++) {
      if (plane == 0 && !(cm->lf.filter_level[0]) && !(cm->lf.filter_level[1]))
        break;
      else if (plane == 1 && !(cm->lf.filter_level_u))
        continue;
      else if (plane == 2 && !(cm->lf.filter_level_v))
        continue;
      for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
        lf_job_queue->mi_row = mi_row;
        lf_job_queue->plane = plane;
        lf_job_queue->dir = dir;
        lf_job_queue++;
        lf_sync->jobs_enqueued++;
      }
    }
  }
}

AV1LfMTInfo *get_lf_job_info(AV1LfSync *lf_sync) {
  AV1LfMTInfo *cur_job_info = NULL;

#if CONFIG_MULTITHREAD
  pthread_mutex_lock(lf_sync->job_mutex);

  if (lf_sync->jobs_dequeued < lf_sync->jobs_enqueued) {
    cur_job_info = lf_sync->job_queue + lf_sync->jobs_dequeued;
    lf_sync->jobs_dequeued++;
  }

  pthread_mutex_unlock(lf_sync->job_mutex);
#else
  (void)lf_sync;
#endif

  return cur_job_info;
}

// Implement row loopfiltering for each thread.
static INLINE void thread_loop_filter_rows(
    const YV12_BUFFER_CONFIG *const frame_buffer, AV1_COMMON *const cm,
    struct macroblockd_plane *planes, MACROBLOCKD *xd,
    AV1LfSync *const lf_sync) {
  const int sb_cols =
      ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2) >> MAX_MIB_SIZE_LOG2;
  int mi_row, mi_col, plane, dir;
  int r, c;

  while (1) {
    AV1LfMTInfo *cur_job_info = get_lf_job_info(lf_sync);

    if (cur_job_info != NULL) {
      mi_row = cur_job_info->mi_row;
      plane = cur_job_info->plane;
      dir = cur_job_info->dir;
      r = mi_row >> MAX_MIB_SIZE_LOG2;

      if (dir == 0) {
        for (mi_col = 0; mi_col < cm->mi_cols; mi_col += MAX_MIB_SIZE) {
          c = mi_col >> MAX_MIB_SIZE_LOG2;

          av1_setup_dst_planes(planes, cm->seq_params.sb_size, frame_buffer,
                               mi_row, mi_col, plane, plane + 1);

          av1_filter_block_plane_vert(cm, xd, plane, &planes[plane], mi_row,
                                      mi_col);
          sync_write(lf_sync, r, c, sb_cols, plane);
        }
      } else if (dir == 1) {
        for (mi_col = 0; mi_col < cm->mi_cols; mi_col += MAX_MIB_SIZE) {
          c = mi_col >> MAX_MIB_SIZE_LOG2;

          // Wait for vertical edge filtering of the top-right block to be
          // completed
          sync_read(lf_sync, r, c, plane);

          // Wait for vertical edge filtering of the right block to be
          // completed
          sync_read(lf_sync, r + 1, c, plane);

          av1_setup_dst_planes(planes, cm->seq_params.sb_size, frame_buffer,
                               mi_row, mi_col, plane, plane + 1);
          av1_filter_block_plane_horz(cm, xd, plane, &planes[plane], mi_row,
                                      mi_col);
        }
      }
    } else {
      break;
    }
  }
}

// Row-based multi-threaded loopfilter hook
static int loop_filter_row_worker(AV1LfSync *const lf_sync,
                                  LFWorkerData *const lf_data) {
  thread_loop_filter_rows(lf_data->frame_buffer, lf_data->cm, lf_data->planes,
                          lf_data->xd, lf_sync);
  return 1;
}

static void loop_filter_rows_mt(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                MACROBLOCKD *xd, int start, int stop,
                                int plane_start, int plane_end,
                                AVxWorker *workers, int nworkers,
                                AV1LfSync *lf_sync) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  // Number of superblock rows and cols
  const int sb_rows =
      ALIGN_POWER_OF_TWO(cm->mi_rows, MAX_MIB_SIZE_LOG2) >> MAX_MIB_SIZE_LOG2;
  const int num_workers = nworkers;
  int i;

  if (!lf_sync->sync_range || sb_rows != lf_sync->rows ||
      num_workers > lf_sync->num_workers) {
    av1_loop_filter_dealloc(lf_sync);
    loop_filter_alloc(lf_sync, cm, sb_rows, cm->width, num_workers);
  }

  // Initialize cur_sb_col to -1 for all SB rows.
  for (i = 0; i < MAX_MB_PLANE; i++) {
    memset(lf_sync->cur_sb_col[i], -1,
           sizeof(*(lf_sync->cur_sb_col[i])) * sb_rows);
  }

  enqueue_lf_jobs(lf_sync, cm, start, stop, plane_start, plane_end);

  // Set up loopfilter thread data.
  for (i = 0; i < num_workers; ++i) {
    AVxWorker *const worker = &workers[i];
    LFWorkerData *const lf_data = &lf_sync->lfdata[i];

    worker->hook = (AVxWorkerHook)loop_filter_row_worker;
    worker->data1 = lf_sync;
    worker->data2 = lf_data;

    // Loopfilter data
    loop_filter_data_reset(lf_data, frame, cm, xd);

    // Start loopfiltering
    if (i == num_workers - 1) {
      winterface->execute(worker);
    } else {
      winterface->launch(worker);
    }
  }

  // Wait till all rows are finished
  for (i = 0; i < num_workers; ++i) {
    winterface->sync(&workers[i]);
  }
}

void av1_loop_filter_frame_mt(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                              MACROBLOCKD *xd, int plane_start, int plane_end,
                              int partial_frame, AVxWorker *workers,
                              int num_workers, AV1LfSync *lf_sync) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;

  start_mi_row = 0;
  mi_rows_to_filter = cm->mi_rows;
  if (partial_frame && cm->mi_rows > 8) {
    start_mi_row = cm->mi_rows >> 1;
    start_mi_row &= 0xfffffff8;
    mi_rows_to_filter = AOMMAX(cm->mi_rows / 8, 8);
  }
  end_mi_row = start_mi_row + mi_rows_to_filter;
  av1_loop_filter_frame_init(cm, plane_start, plane_end);

  loop_filter_rows_mt(frame, cm, xd, start_mi_row, end_mi_row, plane_start,
                      plane_end, workers, num_workers, lf_sync);
}

static INLINE void lr_sync_read(void *const lr_sync, int r, int c, int plane) {
#if CONFIG_MULTITHREAD
  AV1LrSync *const loop_res_sync = (AV1LrSync *)lr_sync;
  const int nsync = loop_res_sync->sync_range;

  if (r && !(c & (nsync - 1))) {
    pthread_mutex_t *const mutex = &loop_res_sync->mutex_[plane][r - 1];
    pthread_mutex_lock(mutex);

    while (c > loop_res_sync->cur_sb_col[plane][r - 1] - nsync) {
      pthread_cond_wait(&loop_res_sync->cond_[plane][r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)lr_sync;
  (void)r;
  (void)c;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void lr_sync_write(void *const lr_sync, int r, int c,
                                 const int sb_cols, int plane) {
#if CONFIG_MULTITHREAD
  AV1LrSync *const loop_res_sync = (AV1LrSync *)lr_sync;
  const int nsync = loop_res_sync->sync_range;
  int cur;
  // Only signal when there are enough filtered SB for next row to run.
  int sig = 1;

  if (c < sb_cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = sb_cols + nsync;
  }

  if (sig) {
    pthread_mutex_lock(&loop_res_sync->mutex_[plane][r]);

    loop_res_sync->cur_sb_col[plane][r] = cur;

    pthread_cond_broadcast(&loop_res_sync->cond_[plane][r]);
    pthread_mutex_unlock(&loop_res_sync->mutex_[plane][r]);
  }
#else
  (void)lr_sync;
  (void)r;
  (void)c;
  (void)sb_cols;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

// Allocate memory for loop restoration row synchronization
static void loop_restoration_alloc(AV1LrSync *lr_sync, AV1_COMMON *cm,
                                   int num_workers, int num_rows_lr,
                                   int num_planes, int width) {
  lr_sync->rows = num_rows_lr;
  lr_sync->num_planes = num_planes;
#if CONFIG_MULTITHREAD
  {
    int i, j;

    for (j = 0; j < num_planes; j++) {
      CHECK_MEM_ERROR(cm, lr_sync->mutex_[j],
                      aom_malloc(sizeof(*(lr_sync->mutex_[j])) * num_rows_lr));
      if (lr_sync->mutex_[j]) {
        for (i = 0; i < num_rows_lr; ++i) {
          pthread_mutex_init(&lr_sync->mutex_[j][i], NULL);
        }
      }

      CHECK_MEM_ERROR(cm, lr_sync->cond_[j],
                      aom_malloc(sizeof(*(lr_sync->cond_[j])) * num_rows_lr));
      if (lr_sync->cond_[j]) {
        for (i = 0; i < num_rows_lr; ++i) {
          pthread_cond_init(&lr_sync->cond_[j][i], NULL);
        }
      }
    }

    CHECK_MEM_ERROR(cm, lr_sync->job_mutex,
                    aom_malloc(sizeof(*(lr_sync->job_mutex))));
    if (lr_sync->job_mutex) {
      pthread_mutex_init(lr_sync->job_mutex, NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD
  CHECK_MEM_ERROR(cm, lr_sync->lrworkerdata,
                  aom_malloc(num_workers * sizeof(*(lr_sync->lrworkerdata))));

  for (int worker_idx = 0; worker_idx < num_workers; ++worker_idx) {
    if (worker_idx < num_workers - 1) {
      CHECK_MEM_ERROR(cm, lr_sync->lrworkerdata[worker_idx].rst_tmpbuf,
                      (int32_t *)aom_memalign(16, RESTORATION_TMPBUF_SIZE));
      CHECK_MEM_ERROR(cm, lr_sync->lrworkerdata[worker_idx].rlbs,
                      aom_malloc(sizeof(RestorationLineBuffers)));

    } else {
      lr_sync->lrworkerdata[worker_idx].rst_tmpbuf = cm->rst_tmpbuf;
      lr_sync->lrworkerdata[worker_idx].rlbs = cm->rlbs;
    }
  }

  lr_sync->num_workers = num_workers;

  for (int j = 0; j < num_planes; j++) {
    CHECK_MEM_ERROR(
        cm, lr_sync->cur_sb_col[j],
        aom_malloc(sizeof(*(lr_sync->cur_sb_col[j])) * num_rows_lr));
  }
  CHECK_MEM_ERROR(
      cm, lr_sync->job_queue,
      aom_malloc(sizeof(*(lr_sync->job_queue)) * num_rows_lr * num_planes));
  // Set up nsync.
  lr_sync->sync_range = get_lr_sync_range(width);
}

// Deallocate loop restoration synchronization related mutex and data
void av1_loop_restoration_dealloc(AV1LrSync *lr_sync, int num_workers) {
  if (lr_sync != NULL) {
    int j;
#if CONFIG_MULTITHREAD
    int i;
    for (j = 0; j < MAX_MB_PLANE; j++) {
      if (lr_sync->mutex_[j] != NULL) {
        for (i = 0; i < lr_sync->rows; ++i) {
          pthread_mutex_destroy(&lr_sync->mutex_[j][i]);
        }
        aom_free(lr_sync->mutex_[j]);
      }
      if (lr_sync->cond_[j] != NULL) {
        for (i = 0; i < lr_sync->rows; ++i) {
          pthread_cond_destroy(&lr_sync->cond_[j][i]);
        }
        aom_free(lr_sync->cond_[j]);
      }
    }
    if (lr_sync->job_mutex != NULL) {
      pthread_mutex_destroy(lr_sync->job_mutex);
      aom_free(lr_sync->job_mutex);
    }
#endif  // CONFIG_MULTITHREAD
    for (j = 0; j < MAX_MB_PLANE; j++) {
      aom_free(lr_sync->cur_sb_col[j]);
    }

    aom_free(lr_sync->job_queue);

    if (lr_sync->lrworkerdata) {
      for (int worker_idx = 0; worker_idx < num_workers - 1; worker_idx++) {
        LRWorkerData *const workerdata_data =
            lr_sync->lrworkerdata + worker_idx;

        aom_free(workerdata_data->rst_tmpbuf);
        aom_free(workerdata_data->rlbs);
      }
      aom_free(lr_sync->lrworkerdata);
    }

    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*lr_sync);
  }
}

static void enqueue_lr_jobs(AV1LrSync *lr_sync, AV1LrStruct *lr_ctxt,
                            AV1_COMMON *cm) {
  FilterFrameCtxt *ctxt = lr_ctxt->ctxt;

  const int num_planes = av1_num_planes(cm);
  AV1LrMTInfo *lr_job_queue = lr_sync->job_queue;
  int32_t lr_job_counter[2], num_even_lr_jobs = 0;
  lr_sync->jobs_enqueued = 0;
  lr_sync->jobs_dequeued = 0;

  for (int plane = 0; plane < num_planes; plane++) {
    if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) continue;
    num_even_lr_jobs =
        num_even_lr_jobs + ((ctxt[plane].rsi->vert_units_per_tile + 1) >> 1);
  }
  lr_job_counter[0] = 0;
  lr_job_counter[1] = num_even_lr_jobs;

  for (int plane = 0; plane < num_planes; plane++) {
    if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) continue;
    const int is_uv = plane > 0;
    const int ss_y = is_uv && cm->seq_params.subsampling_y;

    AV1PixelRect tile_rect = ctxt[plane].tile_rect;
    const int unit_size = ctxt[plane].rsi->restoration_unit_size;

    const int tile_h = tile_rect.bottom - tile_rect.top;
    const int ext_size = unit_size * 3 / 2;

    int y0 = 0, i = 0;
    while (y0 < tile_h) {
      int remaining_h = tile_h - y0;
      int h = (remaining_h < ext_size) ? remaining_h : unit_size;

      RestorationTileLimits limits;
      limits.v_start = tile_rect.top + y0;
      limits.v_end = tile_rect.top + y0 + h;
      assert(limits.v_end <= tile_rect.bottom);
      // Offset the tile upwards to align with the restoration processing stripe
      const int voffset = RESTORATION_UNIT_OFFSET >> ss_y;
      limits.v_start = AOMMAX(tile_rect.top, limits.v_start - voffset);
      if (limits.v_end < tile_rect.bottom) limits.v_end -= voffset;

      assert(lr_job_counter[0] <= num_even_lr_jobs);

      lr_job_queue[lr_job_counter[i & 1]].lr_unit_row = i;
      lr_job_queue[lr_job_counter[i & 1]].plane = plane;
      lr_job_queue[lr_job_counter[i & 1]].v_start = limits.v_start;
      lr_job_queue[lr_job_counter[i & 1]].v_end = limits.v_end;
      lr_job_queue[lr_job_counter[i & 1]].sync_mode = i & 1;
      if ((i & 1) == 0) {
        lr_job_queue[lr_job_counter[i & 1]].v_copy_start =
            limits.v_start + RESTORATION_BORDER;
        lr_job_queue[lr_job_counter[i & 1]].v_copy_end =
            limits.v_end - RESTORATION_BORDER;
        if (i == 0) {
          assert(limits.v_start == tile_rect.top);
          lr_job_queue[lr_job_counter[i & 1]].v_copy_start = tile_rect.top;
        }
        if (i == (ctxt[plane].rsi->vert_units_per_tile - 1)) {
          assert(limits.v_end == tile_rect.bottom);
          lr_job_queue[lr_job_counter[i & 1]].v_copy_end = tile_rect.bottom;
        }
      } else {
        lr_job_queue[lr_job_counter[i & 1]].v_copy_start =
            AOMMAX(limits.v_start - RESTORATION_BORDER, tile_rect.top);
        lr_job_queue[lr_job_counter[i & 1]].v_copy_end =
            AOMMIN(limits.v_end + RESTORATION_BORDER, tile_rect.bottom);
      }
      lr_job_counter[i & 1]++;
      lr_sync->jobs_enqueued++;

      y0 += h;
      ++i;
    }
  }
}

AV1LrMTInfo *get_lr_job_info(AV1LrSync *lr_sync) {
  AV1LrMTInfo *cur_job_info = NULL;

#if CONFIG_MULTITHREAD
  pthread_mutex_lock(lr_sync->job_mutex);

  if (lr_sync->jobs_dequeued < lr_sync->jobs_enqueued) {
    cur_job_info = lr_sync->job_queue + lr_sync->jobs_dequeued;
    lr_sync->jobs_dequeued++;
  }

  pthread_mutex_unlock(lr_sync->job_mutex);
#else
  (void)lr_sync;
#endif

  return cur_job_info;
}

// Implement row loop restoration for each thread.
static int loop_restoration_row_worker(AV1LrSync *const lr_sync,
                                       LRWorkerData *lrworkerdata) {
  AV1LrStruct *lr_ctxt = (AV1LrStruct *)lrworkerdata->lr_ctxt;
  FilterFrameCtxt *ctxt = lr_ctxt->ctxt;
  int lr_unit_row;
  int plane;
  const int tile_row = LR_TILE_ROW;
  const int tile_col = LR_TILE_COL;
  const int tile_cols = LR_TILE_COLS;
  const int tile_idx = tile_col + tile_row * tile_cols;
  typedef void (*copy_fun)(const YV12_BUFFER_CONFIG *src_ybc,
                           YV12_BUFFER_CONFIG *dst_ybc, int hstart, int hend,
                           int vstart, int vend);
  static const copy_fun copy_funs[3] = {
    aom_yv12_partial_copy_y, aom_yv12_partial_copy_u, aom_yv12_partial_copy_v
  };

  while (1) {
    AV1LrMTInfo *cur_job_info = get_lr_job_info(lr_sync);
    if (cur_job_info != NULL) {
      RestorationTileLimits limits;
      sync_read_fn_t on_sync_read;
      sync_write_fn_t on_sync_write;
      limits.v_start = cur_job_info->v_start;
      limits.v_end = cur_job_info->v_end;
      lr_unit_row = cur_job_info->lr_unit_row;
      plane = cur_job_info->plane;
      const int unit_idx0 = tile_idx * ctxt[plane].rsi->units_per_tile;

      // sync_mode == 1 implies only sync read is required in LR Multi-threading
      // sync_mode == 0 implies only sync write is required.
      on_sync_read =
          cur_job_info->sync_mode == 1 ? lr_sync_read : av1_lr_sync_read_dummy;
      on_sync_write = cur_job_info->sync_mode == 0 ? lr_sync_write
                                                   : av1_lr_sync_write_dummy;

      av1_foreach_rest_unit_in_row(
          &limits, &(ctxt[plane].tile_rect), lr_ctxt->on_rest_unit, lr_unit_row,
          ctxt[plane].rsi->restoration_unit_size, unit_idx0,
          ctxt[plane].rsi->horz_units_per_tile,
          ctxt[plane].rsi->vert_units_per_tile, plane, &ctxt[plane],
          lrworkerdata->rst_tmpbuf, lrworkerdata->rlbs, on_sync_read,
          on_sync_write, lr_sync);

      copy_funs[plane](lr_ctxt->dst, lr_ctxt->frame, ctxt[plane].tile_rect.left,
                       ctxt[plane].tile_rect.right, cur_job_info->v_copy_start,
                       cur_job_info->v_copy_end);
    } else {
      break;
    }
  }
  return 1;
}

static void foreach_rest_unit_in_planes_mt(AV1LrStruct *lr_ctxt,
                                           AVxWorker *workers, int nworkers,
                                           AV1LrSync *lr_sync, AV1_COMMON *cm) {
  FilterFrameCtxt *ctxt = lr_ctxt->ctxt;

  const int num_planes = av1_num_planes(cm);

  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int num_rows_lr = 0;

  for (int plane = 0; plane < num_planes; plane++) {
    const AV1PixelRect tile_rect = ctxt[plane].tile_rect;
    const int max_tile_h = tile_rect.bottom - tile_rect.top;

    const int unit_size = cm->seq_params.sb_size == BLOCK_128X128 ? 128 : 64;

    num_rows_lr =
        AOMMAX(num_rows_lr, av1_lr_count_units_in_tile(unit_size, max_tile_h));
  }

  const int num_workers = nworkers;
  int i;
  assert(MAX_MB_PLANE == 3);

  if (!lr_sync->sync_range || num_rows_lr != lr_sync->rows ||
      num_workers > lr_sync->num_workers || num_planes != lr_sync->num_planes) {
    av1_loop_restoration_dealloc(lr_sync, num_workers);
    loop_restoration_alloc(lr_sync, cm, num_workers, num_rows_lr, num_planes,
                           cm->width);
  }

  // Initialize cur_sb_col to -1 for all SB rows.
  for (i = 0; i < num_planes; i++) {
    memset(lr_sync->cur_sb_col[i], -1,
           sizeof(*(lr_sync->cur_sb_col[i])) * num_rows_lr);
  }

  enqueue_lr_jobs(lr_sync, lr_ctxt, cm);

  // Set up looprestoration thread data.
  for (i = 0; i < num_workers; ++i) {
    AVxWorker *const worker = &workers[i];
    lr_sync->lrworkerdata[i].lr_ctxt = (void *)lr_ctxt;
    worker->hook = (AVxWorkerHook)loop_restoration_row_worker;
    worker->data1 = lr_sync;
    worker->data2 = &lr_sync->lrworkerdata[i];

    // Start loopfiltering
    if (i == num_workers - 1) {
      winterface->execute(worker);
    } else {
      winterface->launch(worker);
    }
  }

  // Wait till all rows are finished
  for (i = 0; i < num_workers; ++i) {
    winterface->sync(&workers[i]);
  }
}

void av1_loop_restoration_filter_frame_mt(YV12_BUFFER_CONFIG *frame,
                                          AV1_COMMON *cm, int optimized_lr,
                                          AVxWorker *workers, int num_workers,
                                          AV1LrSync *lr_sync, void *lr_ctxt) {
  assert(!cm->all_lossless);

  const int num_planes = av1_num_planes(cm);

  AV1LrStruct *loop_rest_ctxt = (AV1LrStruct *)lr_ctxt;

  av1_loop_restoration_filter_frame_init(loop_rest_ctxt, frame, cm,
                                         optimized_lr, num_planes);

  foreach_rest_unit_in_planes_mt(loop_rest_ctxt, workers, num_workers, lr_sync,
                                 cm);
}
