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

#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/ethread.h"
#include "aom_dsp/aom_dsp_common.h"

static void accumulate_rd_opt(ThreadData *td, ThreadData *td_t) {
  for (int i = 0; i < REFERENCE_MODES; i++)
    td->rd_counts.comp_pred_diff[i] += td_t->rd_counts.comp_pred_diff[i];

  for (int i = 0; i < REF_FRAMES; i++)
    td->rd_counts.global_motion_used[i] +=
        td_t->rd_counts.global_motion_used[i];

  td->rd_counts.compound_ref_used_flag |=
      td_t->rd_counts.compound_ref_used_flag;
  td->rd_counts.skip_mode_used_flag |= td_t->rd_counts.skip_mode_used_flag;
}

static int enc_worker_hook(void *arg1, void *unused) {
  EncWorkerData *const thread_data = (EncWorkerData *)arg1;
  AV1_COMP *const cpi = thread_data->cpi;
  const AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  int t;

  (void)unused;

  for (t = thread_data->start; t < tile_rows * tile_cols;
       t += cpi->num_workers) {
    int tile_row = t / tile_cols;
    int tile_col = t % tile_cols;

    av1_encode_tile(cpi, thread_data->td, tile_row, tile_col);
  }

  return 1;
}

static void create_enc_workers(AV1_COMP *cpi, int num_workers) {
  AV1_COMMON *const cm = &cpi->common;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  CHECK_MEM_ERROR(cm, cpi->workers,
                  aom_malloc(num_workers * sizeof(*cpi->workers)));

  CHECK_MEM_ERROR(cm, cpi->tile_thr_data,
                  aom_calloc(num_workers, sizeof(*cpi->tile_thr_data)));

  for (int i = 0; i < num_workers; i++) {
    AVxWorker *const worker = &cpi->workers[i];
    EncWorkerData *const thread_data = &cpi->tile_thr_data[i];

    ++cpi->num_workers;
    winterface->init(worker);

    thread_data->cpi = cpi;

    if (i < num_workers - 1) {
      // Allocate thread data.
      CHECK_MEM_ERROR(cm, thread_data->td,
                      aom_memalign(32, sizeof(*thread_data->td)));
      av1_zero(*thread_data->td);

      // Set up pc_tree.
      thread_data->td->pc_tree = NULL;
      av1_setup_pc_tree(cm, thread_data->td);

      CHECK_MEM_ERROR(cm, thread_data->td->above_pred_buf,
                      (uint8_t *)aom_memalign(
                          16, MAX_MB_PLANE * MAX_SB_SQUARE *
                                  sizeof(*thread_data->td->above_pred_buf)));
      CHECK_MEM_ERROR(cm, thread_data->td->left_pred_buf,
                      (uint8_t *)aom_memalign(
                          16, MAX_MB_PLANE * MAX_SB_SQUARE *
                                  sizeof(*thread_data->td->left_pred_buf)));

      CHECK_MEM_ERROR(
          cm, thread_data->td->wsrc_buf,
          (int32_t *)aom_memalign(
              16, MAX_SB_SQUARE * sizeof(*thread_data->td->wsrc_buf)));

      for (int x = 0; x < 2; x++)
        for (int y = 0; y < 2; y++)
          CHECK_MEM_ERROR(
              cm, thread_data->td->hash_value_buffer[x][y],
              (uint32_t *)aom_malloc(
                  AOM_BUFFER_SIZE_FOR_BLOCK_HASH *
                  sizeof(*thread_data->td->hash_value_buffer[0][0])));

      CHECK_MEM_ERROR(
          cm, thread_data->td->mask_buf,
          (int32_t *)aom_memalign(
              16, MAX_SB_SQUARE * sizeof(*thread_data->td->mask_buf)));
      // Allocate frame counters in thread data.
      CHECK_MEM_ERROR(cm, thread_data->td->counts,
                      aom_calloc(1, sizeof(*thread_data->td->counts)));

      // Allocate buffers used by palette coding mode.
      CHECK_MEM_ERROR(
          cm, thread_data->td->palette_buffer,
          aom_memalign(16, sizeof(*thread_data->td->palette_buffer)));

      CHECK_MEM_ERROR(
          cm, thread_data->td->tmp_conv_dst,
          aom_memalign(32, MAX_SB_SIZE * MAX_SB_SIZE *
                               sizeof(*thread_data->td->tmp_conv_dst)));
      for (int j = 0; j < 2; ++j) {
        CHECK_MEM_ERROR(
            cm, thread_data->td->tmp_obmc_bufs[j],
            aom_memalign(16, 2 * MAX_MB_PLANE * MAX_SB_SQUARE *
                                 sizeof(*thread_data->td->tmp_obmc_bufs[j])));
      }

      // Create threads
      if (!winterface->reset(worker))
        aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                           "Tile encoder thread creation failed");
    } else {
      // Main thread acts as a worker and uses the thread data in cpi.
      thread_data->td = &cpi->td;
    }
    winterface->sync(worker);
  }
}

static void launch_enc_workers(AV1_COMP *cpi, int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  // Encode a frame
  for (int i = 0; i < num_workers; i++) {
    AVxWorker *const worker = &cpi->workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;

    // Set the starting tile for each thread.
    thread_data->start = i;

    if (i == cpi->num_workers - 1)
      winterface->execute(worker);
    else
      winterface->launch(worker);
  }
}

static void sync_enc_workers(AV1_COMP *cpi, int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  // Encoding ends.
  for (int i = 0; i < num_workers; i++) {
    AVxWorker *const worker = &cpi->workers[i];
    winterface->sync(worker);
  }
}

static void accumulate_counters_enc_workers(AV1_COMP *cpi, int num_workers) {
  for (int i = 0; i < num_workers; i++) {
    AVxWorker *const worker = &cpi->workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;
    cpi->intrabc_used |= thread_data->td->intrabc_used_this_tile;
    // Accumulate counters.
    if (i < cpi->num_workers - 1) {
      av1_accumulate_frame_counts(&cpi->counts, thread_data->td->counts);
      accumulate_rd_opt(&cpi->td, thread_data->td);
      cpi->td.mb.txb_split_count += thread_data->td->mb.txb_split_count;
    }
  }
}

static void prepare_enc_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                int num_workers) {
  for (int i = 0; i < num_workers; i++) {
    AVxWorker *const worker = &cpi->workers[i];
    EncWorkerData *const thread_data = &cpi->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
      thread_data->td->rd_counts = cpi->td.rd_counts;
      thread_data->td->mb.above_pred_buf = thread_data->td->above_pred_buf;
      thread_data->td->mb.left_pred_buf = thread_data->td->left_pred_buf;
      thread_data->td->mb.wsrc_buf = thread_data->td->wsrc_buf;
      for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
          memcpy(thread_data->td->hash_value_buffer[x][y],
                 cpi->td.mb.hash_value_buffer[x][y],
                 AOM_BUFFER_SIZE_FOR_BLOCK_HASH *
                     sizeof(*thread_data->td->hash_value_buffer[0][0]));
          thread_data->td->mb.hash_value_buffer[x][y] =
              thread_data->td->hash_value_buffer[x][y];
        }
      }
      thread_data->td->mb.mask_buf = thread_data->td->mask_buf;
    }
    if (thread_data->td->counts != &cpi->counts) {
      memcpy(thread_data->td->counts, &cpi->counts, sizeof(cpi->counts));
    }

    if (i < num_workers - 1) {
      thread_data->td->mb.palette_buffer = thread_data->td->palette_buffer;
      thread_data->td->mb.tmp_conv_dst = thread_data->td->tmp_conv_dst;
      for (int j = 0; j < 2; ++j) {
        thread_data->td->mb.tmp_obmc_bufs[j] =
            thread_data->td->tmp_obmc_bufs[j];
      }

      thread_data->td->mb.e_mbd.tmp_conv_dst = thread_data->td->mb.tmp_conv_dst;
      for (int j = 0; j < 2; ++j) {
        thread_data->td->mb.e_mbd.tmp_obmc_bufs[j] =
            thread_data->td->mb.tmp_obmc_bufs[j];
      }
    }
  }
}

void av1_encode_tiles_mt(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  int num_workers = AOMMIN(cpi->oxcf.max_threads, tile_cols * tile_rows);

  if (cpi->tile_data == NULL || cpi->allocated_tiles < tile_cols * tile_rows)
    av1_alloc_tile_data(cpi);

  av1_init_tile_data(cpi);
  // Only run once to create threads and allocate thread data.
  if (cpi->num_workers == 0) {
    create_enc_workers(cpi, num_workers);
  } else {
    num_workers = AOMMIN(num_workers, cpi->num_workers);
  }
  prepare_enc_workers(cpi, enc_worker_hook, num_workers);
  launch_enc_workers(cpi, num_workers);
  sync_enc_workers(cpi, num_workers);
  accumulate_counters_enc_workers(cpi, num_workers);
}

// Accumulate frame counts. FRAME_COUNTS consist solely of 'unsigned int'
// members, so we treat it as an array, and sum over the whole length.
void av1_accumulate_frame_counts(FRAME_COUNTS *acc_counts,
                                 const FRAME_COUNTS *counts) {
  unsigned int *const acc = (unsigned int *)acc_counts;
  const unsigned int *const cnt = (const unsigned int *)counts;

  const unsigned int n_counts = sizeof(FRAME_COUNTS) / sizeof(unsigned int);

  for (unsigned int i = 0; i < n_counts; i++) acc[i] += cnt[i];
}
