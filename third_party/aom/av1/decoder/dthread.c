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

#include "aom_mem/aom_mem.h"
#include "av1/common/reconinter.h"
#include "av1/decoder/dthread.h"
#include "av1/decoder/decoder.h"

// #define DEBUG_THREAD

// TODO(hkuang): Clean up all the #ifdef in this file.
void av1_frameworker_lock_stats(AVxWorker *const worker) {
#if CONFIG_MULTITHREAD
  FrameWorkerData *const worker_data = worker->data1;
  pthread_mutex_lock(&worker_data->stats_mutex);
#else
  (void)worker;
#endif
}

void av1_frameworker_unlock_stats(AVxWorker *const worker) {
#if CONFIG_MULTITHREAD
  FrameWorkerData *const worker_data = worker->data1;
  pthread_mutex_unlock(&worker_data->stats_mutex);
#else
  (void)worker;
#endif
}

void av1_frameworker_signal_stats(AVxWorker *const worker) {
#if CONFIG_MULTITHREAD
  FrameWorkerData *const worker_data = worker->data1;

// TODO(hkuang): Fix the pthread_cond_broadcast in windows wrapper.
#if defined(_WIN32) && !HAVE_PTHREAD_H
  pthread_cond_signal(&worker_data->stats_cond);
#else
  pthread_cond_broadcast(&worker_data->stats_cond);
#endif

#else
  (void)worker;
#endif
}

// This macro prevents thread_sanitizer from reporting known concurrent writes.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define BUILDING_WITH_TSAN
#endif
#endif

// TODO(hkuang): Remove worker parameter as it is only used in debug code.
void av1_frameworker_wait(AVxWorker *const worker, RefCntBuffer *const ref_buf,
                          int row) {
#if CONFIG_MULTITHREAD
  if (!ref_buf) return;

#ifndef BUILDING_WITH_TSAN
  // The following line of code will get harmless tsan error but it is the key
  // to get best performance.
  if (ref_buf->row >= row && ref_buf->buf.corrupted != 1) return;
#endif

  {
    // Find the worker thread that owns the reference frame. If the reference
    // frame has been fully decoded, it may not have owner.
    AVxWorker *const ref_worker = ref_buf->frame_worker_owner;
    FrameWorkerData *const ref_worker_data =
        (FrameWorkerData *)ref_worker->data1;
    const AV1Decoder *const pbi = ref_worker_data->pbi;

#ifdef DEBUG_THREAD
    {
      FrameWorkerData *const worker_data = (FrameWorkerData *)worker->data1;
      printf("%d %p worker is waiting for %d %p worker (%d)  ref %d \r\n",
             worker_data->worker_id, worker, ref_worker_data->worker_id,
             ref_buf->frame_worker_owner, row, ref_buf->row);
    }
#endif

    av1_frameworker_lock_stats(ref_worker);
    while (ref_buf->row < row && pbi->cur_buf == ref_buf &&
           ref_buf->buf.corrupted != 1) {
      pthread_cond_wait(&ref_worker_data->stats_cond,
                        &ref_worker_data->stats_mutex);
    }

    if (ref_buf->buf.corrupted == 1) {
      FrameWorkerData *const worker_data = (FrameWorkerData *)worker->data1;
      av1_frameworker_unlock_stats(ref_worker);
      aom_internal_error(&worker_data->pbi->common.error,
                         AOM_CODEC_CORRUPT_FRAME,
                         "Worker %p failed to decode frame", worker);
    }
    av1_frameworker_unlock_stats(ref_worker);
  }
#else
  (void)worker;
  (void)ref_buf;
  (void)row;
  (void)ref_buf;
#endif  // CONFIG_MULTITHREAD
}

void av1_frameworker_broadcast(RefCntBuffer *const buf, int row) {
#if CONFIG_MULTITHREAD
  AVxWorker *worker = buf->frame_worker_owner;

#ifdef DEBUG_THREAD
  {
    FrameWorkerData *const worker_data = (FrameWorkerData *)worker->data1;
    printf("%d %p worker decode to (%d) \r\n", worker_data->worker_id,
           buf->frame_worker_owner, row);
  }
#endif

  av1_frameworker_lock_stats(worker);
  buf->row = row;
  av1_frameworker_signal_stats(worker);
  av1_frameworker_unlock_stats(worker);
#else
  (void)buf;
  (void)row;
#endif  // CONFIG_MULTITHREAD
}

void av1_frameworker_copy_context(AVxWorker *const dst_worker,
                                  AVxWorker *const src_worker) {
#if CONFIG_MULTITHREAD
  FrameWorkerData *const src_worker_data = (FrameWorkerData *)src_worker->data1;
  FrameWorkerData *const dst_worker_data = (FrameWorkerData *)dst_worker->data1;
  AV1_COMMON *const src_cm = &src_worker_data->pbi->common;
  AV1_COMMON *const dst_cm = &dst_worker_data->pbi->common;
  int i;

  // Wait until source frame's context is ready.
  av1_frameworker_lock_stats(src_worker);
  while (!src_worker_data->frame_context_ready) {
    pthread_cond_wait(&src_worker_data->stats_cond,
                      &src_worker_data->stats_mutex);
  }

  dst_cm->last_frame_seg_map = src_cm->seg.enabled
                                   ? src_cm->current_frame_seg_map
                                   : src_cm->last_frame_seg_map;
  dst_worker_data->pbi->need_resync = src_worker_data->pbi->need_resync;
  av1_frameworker_unlock_stats(src_worker);

  dst_cm->seq_params.bit_depth = src_cm->seq_params.bit_depth;
  dst_cm->seq_params.use_highbitdepth = src_cm->seq_params.use_highbitdepth;
  // TODO(zoeliu): To handle parallel decoding
  dst_cm->prev_frame =
      src_cm->show_existing_frame ? src_cm->prev_frame : src_cm->cur_frame;
  dst_cm->last_width =
      !src_cm->show_existing_frame ? src_cm->width : src_cm->last_width;
  dst_cm->last_height =
      !src_cm->show_existing_frame ? src_cm->height : src_cm->last_height;
  dst_cm->seq_params.subsampling_x = src_cm->seq_params.subsampling_x;
  dst_cm->seq_params.subsampling_y = src_cm->seq_params.subsampling_y;
  dst_cm->frame_type = src_cm->frame_type;
  dst_cm->last_show_frame = !src_cm->show_existing_frame
                                ? src_cm->show_frame
                                : src_cm->last_show_frame;
  for (i = 0; i < REF_FRAMES; ++i)
    dst_cm->ref_frame_map[i] = src_cm->next_ref_frame_map[i];

  memcpy(dst_cm->lf_info.lfthr, src_cm->lf_info.lfthr,
         (MAX_LOOP_FILTER + 1) * sizeof(loop_filter_thresh));
  dst_cm->lf.sharpness_level = src_cm->lf.sharpness_level;
  dst_cm->lf.filter_level[0] = src_cm->lf.filter_level[0];
  dst_cm->lf.filter_level[1] = src_cm->lf.filter_level[1];
  memcpy(dst_cm->lf.ref_deltas, src_cm->lf.ref_deltas, REF_FRAMES);
  memcpy(dst_cm->lf.mode_deltas, src_cm->lf.mode_deltas, MAX_MODE_LF_DELTAS);
  dst_cm->seg = src_cm->seg;
  memcpy(dst_cm->frame_contexts, src_cm->frame_contexts,
         FRAME_CONTEXTS * sizeof(dst_cm->frame_contexts[0]));
#else
  (void)dst_worker;
  (void)src_worker;
#endif  // CONFIG_MULTITHREAD
}
