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

#include "./aom_config.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "av1/common/entropymode.h"
#include "av1/common/thread_common.h"
#include "av1/common/reconinter.h"

#if CONFIG_MULTITHREAD
static INLINE void mutex_lock(pthread_mutex_t *const mutex) {
  const int kMaxTryLocks = 4000;
  int locked = 0;
  int i;

  for (i = 0; i < kMaxTryLocks; ++i) {
    if (!pthread_mutex_trylock(mutex)) {
      locked = 1;
      break;
    }
  }

  if (!locked) pthread_mutex_lock(mutex);
}
#endif  // CONFIG_MULTITHREAD

static INLINE void sync_read(AV1LfSync *const lf_sync, int r, int c) {
#if CONFIG_MULTITHREAD
  const int nsync = lf_sync->sync_range;

  if (r && !(c & (nsync - 1))) {
    pthread_mutex_t *const mutex = &lf_sync->mutex_[r - 1];
    mutex_lock(mutex);

    while (c > lf_sync->cur_sb_col[r - 1] - nsync) {
      pthread_cond_wait(&lf_sync->cond_[r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)lf_sync;
  (void)r;
  (void)c;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void sync_write(AV1LfSync *const lf_sync, int r, int c,
                              const int sb_cols) {
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
    mutex_lock(&lf_sync->mutex_[r]);

    lf_sync->cur_sb_col[r] = cur;

    pthread_cond_signal(&lf_sync->cond_[r]);
    pthread_mutex_unlock(&lf_sync->mutex_[r]);
  }
#else
  (void)lf_sync;
  (void)r;
  (void)c;
  (void)sb_cols;
#endif  // CONFIG_MULTITHREAD
}

#if !CONFIG_EXT_PARTITION_TYPES
static INLINE enum lf_path get_loop_filter_path(
    int y_only, struct macroblockd_plane *planes) {
  if (y_only)
    return LF_PATH_444;
  else if (planes[1].subsampling_y == 1 && planes[1].subsampling_x == 1)
    return LF_PATH_420;
  else if (planes[1].subsampling_y == 0 && planes[1].subsampling_x == 0)
    return LF_PATH_444;
  else
    return LF_PATH_SLOW;
}

static INLINE void loop_filter_block_plane_ver(
    AV1_COMMON *cm, struct macroblockd_plane *planes, int plane,
    MODE_INFO **mi, int mi_row, int mi_col, enum lf_path path,
    LOOP_FILTER_MASK *lfm) {
  if (plane == 0) {
    av1_filter_block_plane_ss00_ver(cm, &planes[0], mi_row, lfm);
  } else {
    switch (path) {
      case LF_PATH_420:
        av1_filter_block_plane_ss11_ver(cm, &planes[plane], mi_row, lfm);
        break;
      case LF_PATH_444:
        av1_filter_block_plane_ss00_ver(cm, &planes[plane], mi_row, lfm);
        break;
      case LF_PATH_SLOW:
        av1_filter_block_plane_non420_ver(cm, &planes[plane], mi, mi_row,
                                          mi_col, plane);
        break;
    }
  }
}

static INLINE void loop_filter_block_plane_hor(
    AV1_COMMON *cm, struct macroblockd_plane *planes, int plane,
    MODE_INFO **mi, int mi_row, int mi_col, enum lf_path path,
    LOOP_FILTER_MASK *lfm) {
  if (plane == 0) {
    av1_filter_block_plane_ss00_hor(cm, &planes[0], mi_row, lfm);
  } else {
    switch (path) {
      case LF_PATH_420:
        av1_filter_block_plane_ss11_hor(cm, &planes[plane], mi_row, lfm);
        break;
      case LF_PATH_444:
        av1_filter_block_plane_ss00_hor(cm, &planes[plane], mi_row, lfm);
        break;
      case LF_PATH_SLOW:
        av1_filter_block_plane_non420_hor(cm, &planes[plane], mi, mi_row,
                                          mi_col, plane);
        break;
    }
  }
}
#endif
// Row-based multi-threaded loopfilter hook
#if CONFIG_PARALLEL_DEBLOCKING
static int loop_filter_ver_row_worker(AV1LfSync *const lf_sync,
                                      LFWorkerData *const lf_data) {
  const int num_planes = lf_data->y_only ? 1 : MAX_MB_PLANE;
  int mi_row, mi_col;
#if !CONFIG_EXT_PARTITION_TYPES
  enum lf_path path = get_loop_filter_path(lf_data->y_only, lf_data->planes);
#endif
  for (mi_row = lf_data->start; mi_row < lf_data->stop;
       mi_row += lf_sync->num_workers * lf_data->cm->mib_size) {
    MODE_INFO **const mi =
        lf_data->cm->mi_grid_visible + mi_row * lf_data->cm->mi_stride;

    for (mi_col = 0; mi_col < lf_data->cm->mi_cols;
         mi_col += lf_data->cm->mib_size) {
      LOOP_FILTER_MASK lfm;
      int plane;

      av1_setup_dst_planes(lf_data->planes, lf_data->cm->sb_size,
                           lf_data->frame_buffer, mi_row, mi_col);
      av1_setup_mask(lf_data->cm, mi_row, mi_col, mi + mi_col,
                     lf_data->cm->mi_stride, &lfm);

#if CONFIG_EXT_PARTITION_TYPES
      for (plane = 0; plane < num_planes; ++plane)
        av1_filter_block_plane_non420_ver(lf_data->cm, &lf_data->planes[plane],
                                          mi + mi_col, mi_row, mi_col, plane);
#else

      for (plane = 0; plane < num_planes; ++plane)
        loop_filter_block_plane_ver(lf_data->cm, lf_data->planes, plane,
                                    mi + mi_col, mi_row, mi_col, path, &lfm);
#endif
    }
  }
  return 1;
}

static int loop_filter_hor_row_worker(AV1LfSync *const lf_sync,
                                      LFWorkerData *const lf_data) {
  const int num_planes = lf_data->y_only ? 1 : MAX_MB_PLANE;
  const int sb_cols =
      mi_cols_aligned_to_sb(lf_data->cm) >> lf_data->cm->mib_size_log2;
  int mi_row, mi_col;
#if !CONFIG_EXT_PARTITION_TYPES
  enum lf_path path = get_loop_filter_path(lf_data->y_only, lf_data->planes);
#endif

  for (mi_row = lf_data->start; mi_row < lf_data->stop;
       mi_row += lf_sync->num_workers * lf_data->cm->mib_size) {
    MODE_INFO **const mi =
        lf_data->cm->mi_grid_visible + mi_row * lf_data->cm->mi_stride;

    for (mi_col = 0; mi_col < lf_data->cm->mi_cols;
         mi_col += lf_data->cm->mib_size) {
      const int r = mi_row >> lf_data->cm->mib_size_log2;
      const int c = mi_col >> lf_data->cm->mib_size_log2;
      LOOP_FILTER_MASK lfm;
      int plane;

      // TODO(wenhao.zhang@intel.com): For better parallelization, reorder
      // the outer loop to column-based and remove the synchronizations here.
      sync_read(lf_sync, r, c);

      av1_setup_dst_planes(lf_data->planes, lf_data->cm->sb_size,
                           lf_data->frame_buffer, mi_row, mi_col);
      av1_setup_mask(lf_data->cm, mi_row, mi_col, mi + mi_col,
                     lf_data->cm->mi_stride, &lfm);
#if CONFIG_EXT_PARTITION_TYPES
      for (plane = 0; plane < num_planes; ++plane)
        av1_filter_block_plane_non420_hor(lf_data->cm, &lf_data->planes[plane],
                                          mi + mi_col, mi_row, mi_col, plane);
#else
      for (plane = 0; plane < num_planes; ++plane)
        loop_filter_block_plane_hor(lf_data->cm, lf_data->planes, plane,
                                    mi + mi_col, mi_row, mi_col, path, &lfm);
#endif
      sync_write(lf_sync, r, c, sb_cols);
    }
  }
  return 1;
}
#else  //  CONFIG_PARALLEL_DEBLOCKING
static int loop_filter_row_worker(AV1LfSync *const lf_sync,
                                  LFWorkerData *const lf_data) {
  const int num_planes = lf_data->y_only ? 1 : MAX_MB_PLANE;
  const int sb_cols =
      mi_cols_aligned_to_sb(lf_data->cm) >> lf_data->cm->mib_size_log2;
  int mi_row, mi_col;
#if !CONFIG_EXT_PARTITION_TYPES
  enum lf_path path = get_loop_filter_path(lf_data->y_only, lf_data->planes);
#endif  // !CONFIG_EXT_PARTITION_TYPES

#if CONFIG_EXT_PARTITION
  printf(
      "STOPPING: This code has not been modified to work with the "
      "extended coding unit size experiment");
  exit(EXIT_FAILURE);
#endif  // CONFIG_EXT_PARTITION

  for (mi_row = lf_data->start; mi_row < lf_data->stop;
       mi_row += lf_sync->num_workers * lf_data->cm->mib_size) {
    MODE_INFO **const mi =
        lf_data->cm->mi_grid_visible + mi_row * lf_data->cm->mi_stride;

    for (mi_col = 0; mi_col < lf_data->cm->mi_cols;
         mi_col += lf_data->cm->mib_size) {
      const int r = mi_row >> lf_data->cm->mib_size_log2;
      const int c = mi_col >> lf_data->cm->mib_size_log2;
#if !CONFIG_EXT_PARTITION_TYPES
      LOOP_FILTER_MASK lfm;
#endif
      int plane;

      sync_read(lf_sync, r, c);

      av1_setup_dst_planes(lf_data->planes, lf_data->cm->sb_size,
                           lf_data->frame_buffer, mi_row, mi_col);
#if CONFIG_EXT_PARTITION_TYPES
      for (plane = 0; plane < num_planes; ++plane) {
        av1_filter_block_plane_non420_ver(lf_data->cm, &lf_data->planes[plane],
                                          mi + mi_col, mi_row, mi_col, plane);
        av1_filter_block_plane_non420_hor(lf_data->cm, &lf_data->planes[plane],
                                          mi + mi_col, mi_row, mi_col, plane);
      }
#else
      av1_setup_mask(lf_data->cm, mi_row, mi_col, mi + mi_col,
                     lf_data->cm->mi_stride, &lfm);

      for (plane = 0; plane < num_planes; ++plane) {
        loop_filter_block_plane_ver(lf_data->cm, lf_data->planes, plane,
                                    mi + mi_col, mi_row, mi_col, path, &lfm);
        loop_filter_block_plane_hor(lf_data->cm, lf_data->planes, plane,
                                    mi + mi_col, mi_row, mi_col, path, &lfm);
      }
#endif  // CONFIG_EXT_PARTITION_TYPES
      sync_write(lf_sync, r, c, sb_cols);
    }
  }
  return 1;
}
#endif  //  CONFIG_PARALLEL_DEBLOCKING

static void loop_filter_rows_mt(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                struct macroblockd_plane *planes, int start,
                                int stop, int y_only, AVxWorker *workers,
                                int nworkers, AV1LfSync *lf_sync) {
#if CONFIG_EXT_PARTITION
  printf(
      "STOPPING: This code has not been modified to work with the "
      "extended coding unit size experiment");
  exit(EXIT_FAILURE);
#endif  // CONFIG_EXT_PARTITION

  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  // Number of superblock rows and cols
  const int sb_rows = mi_rows_aligned_to_sb(cm) >> cm->mib_size_log2;
  // Decoder may allocate more threads than number of tiles based on user's
  // input.
  const int tile_cols = cm->tile_cols;
  const int num_workers = AOMMIN(nworkers, tile_cols);
  int i;

  if (!lf_sync->sync_range || sb_rows != lf_sync->rows ||
      num_workers > lf_sync->num_workers) {
    av1_loop_filter_dealloc(lf_sync);
    av1_loop_filter_alloc(lf_sync, cm, sb_rows, cm->width, num_workers);
  }

// Set up loopfilter thread data.
// The decoder is capping num_workers because it has been observed that using
// more threads on the loopfilter than there are cores will hurt performance
// on Android. This is because the system will only schedule the tile decode
// workers on cores equal to the number of tile columns. Then if the decoder
// tries to use more threads for the loopfilter, it will hurt performance
// because of contention. If the multithreading code changes in the future
// then the number of workers used by the loopfilter should be revisited.

#if CONFIG_PARALLEL_DEBLOCKING
  // Initialize cur_sb_col to -1 for all SB rows.
  memset(lf_sync->cur_sb_col, -1, sizeof(*lf_sync->cur_sb_col) * sb_rows);

  // Filter all the vertical edges in the whole frame
  for (i = 0; i < num_workers; ++i) {
    AVxWorker *const worker = &workers[i];
    LFWorkerData *const lf_data = &lf_sync->lfdata[i];

    worker->hook = (AVxWorkerHook)loop_filter_ver_row_worker;
    worker->data1 = lf_sync;
    worker->data2 = lf_data;

    // Loopfilter data
    av1_loop_filter_data_reset(lf_data, frame, cm, planes);
    lf_data->start = start + i * cm->mib_size;
    lf_data->stop = stop;
    lf_data->y_only = y_only;

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

  memset(lf_sync->cur_sb_col, -1, sizeof(*lf_sync->cur_sb_col) * sb_rows);
  // Filter all the horizontal edges in the whole frame
  for (i = 0; i < num_workers; ++i) {
    AVxWorker *const worker = &workers[i];
    LFWorkerData *const lf_data = &lf_sync->lfdata[i];

    worker->hook = (AVxWorkerHook)loop_filter_hor_row_worker;
    worker->data1 = lf_sync;
    worker->data2 = lf_data;

    // Loopfilter data
    av1_loop_filter_data_reset(lf_data, frame, cm, planes);
    lf_data->start = start + i * cm->mib_size;
    lf_data->stop = stop;
    lf_data->y_only = y_only;

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
#else   // CONFIG_PARALLEL_DEBLOCKING
  // Initialize cur_sb_col to -1 for all SB rows.
  memset(lf_sync->cur_sb_col, -1, sizeof(*lf_sync->cur_sb_col) * sb_rows);

  for (i = 0; i < num_workers; ++i) {
    AVxWorker *const worker = &workers[i];
    LFWorkerData *const lf_data = &lf_sync->lfdata[i];

    worker->hook = (AVxWorkerHook)loop_filter_row_worker;
    worker->data1 = lf_sync;
    worker->data2 = lf_data;

    // Loopfilter data
    av1_loop_filter_data_reset(lf_data, frame, cm, planes);
    lf_data->start = start + i * cm->mib_size;
    lf_data->stop = stop;
    lf_data->y_only = y_only;

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
#endif  // CONFIG_PARALLEL_DEBLOCKING
}

void av1_loop_filter_frame_mt(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                              struct macroblockd_plane *planes,
                              int frame_filter_level,
#if CONFIG_LOOPFILTER_LEVEL
                              int frame_filter_level_r,
#endif
                              int y_only, int partial_frame, AVxWorker *workers,
                              int num_workers, AV1LfSync *lf_sync) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;

  if (!frame_filter_level) return;

  start_mi_row = 0;
  mi_rows_to_filter = cm->mi_rows;
  if (partial_frame && cm->mi_rows > 8) {
    start_mi_row = cm->mi_rows >> 1;
    start_mi_row &= 0xfffffff8;
    mi_rows_to_filter = AOMMAX(cm->mi_rows / 8, 8);
  }
  end_mi_row = start_mi_row + mi_rows_to_filter;
#if CONFIG_LOOPFILTER_LEVEL
  av1_loop_filter_frame_init(cm, frame_filter_level, frame_filter_level_r,
                             y_only);
#else
  av1_loop_filter_frame_init(cm, frame_filter_level, frame_filter_level);
#endif  // CONFIG_LOOPFILTER_LEVEL
  loop_filter_rows_mt(frame, cm, planes, start_mi_row, end_mi_row, y_only,
                      workers, num_workers, lf_sync);
}

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

// Allocate memory for lf row synchronization
void av1_loop_filter_alloc(AV1LfSync *lf_sync, AV1_COMMON *cm, int rows,
                           int width, int num_workers) {
  lf_sync->rows = rows;
#if CONFIG_MULTITHREAD
  {
    int i;

    CHECK_MEM_ERROR(cm, lf_sync->mutex_,
                    aom_malloc(sizeof(*lf_sync->mutex_) * rows));
    if (lf_sync->mutex_) {
      for (i = 0; i < rows; ++i) {
        pthread_mutex_init(&lf_sync->mutex_[i], NULL);
      }
    }

    CHECK_MEM_ERROR(cm, lf_sync->cond_,
                    aom_malloc(sizeof(*lf_sync->cond_) * rows));
    if (lf_sync->cond_) {
      for (i = 0; i < rows; ++i) {
        pthread_cond_init(&lf_sync->cond_[i], NULL);
      }
    }
  }
#endif  // CONFIG_MULTITHREAD

  CHECK_MEM_ERROR(cm, lf_sync->lfdata,
                  aom_malloc(num_workers * sizeof(*lf_sync->lfdata)));
  lf_sync->num_workers = num_workers;

  CHECK_MEM_ERROR(cm, lf_sync->cur_sb_col,
                  aom_malloc(sizeof(*lf_sync->cur_sb_col) * rows));

  // Set up nsync.
  lf_sync->sync_range = get_sync_range(width);
}

// Deallocate lf synchronization related mutex and data
void av1_loop_filter_dealloc(AV1LfSync *lf_sync) {
  if (lf_sync != NULL) {
#if CONFIG_MULTITHREAD
    int i;

    if (lf_sync->mutex_ != NULL) {
      for (i = 0; i < lf_sync->rows; ++i) {
        pthread_mutex_destroy(&lf_sync->mutex_[i]);
      }
      aom_free(lf_sync->mutex_);
    }
    if (lf_sync->cond_ != NULL) {
      for (i = 0; i < lf_sync->rows; ++i) {
        pthread_cond_destroy(&lf_sync->cond_[i]);
      }
      aom_free(lf_sync->cond_);
    }
#endif  // CONFIG_MULTITHREAD
    aom_free(lf_sync->lfdata);
    aom_free(lf_sync->cur_sb_col);
    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*lf_sync);
  }
}

// Accumulate frame counts. FRAME_COUNTS consist solely of 'unsigned int'
// members, so we treat it as an array, and sum over the whole length.
void av1_accumulate_frame_counts(FRAME_COUNTS *acc_counts,
                                 FRAME_COUNTS *counts) {
  unsigned int *const acc = (unsigned int *)acc_counts;
  const unsigned int *const cnt = (unsigned int *)counts;

  const unsigned int n_counts = sizeof(FRAME_COUNTS) / sizeof(unsigned int);
  unsigned int i;

  for (i = 0; i < n_counts; i++) acc[i] += cnt[i];
}
