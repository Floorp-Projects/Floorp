/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_ethread.h"
#include "vpx_dsp/vpx_dsp_common.h"

static void accumulate_rd_opt(ThreadData *td, ThreadData *td_t) {
  int i, j, k, l, m, n;

  for (i = 0; i < REFERENCE_MODES; i++)
    td->rd_counts.comp_pred_diff[i] += td_t->rd_counts.comp_pred_diff[i];

  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
    td->rd_counts.filter_diff[i] += td_t->rd_counts.filter_diff[i];

  for (i = 0; i < TX_SIZES; i++)
    for (j = 0; j < PLANE_TYPES; j++)
      for (k = 0; k < REF_TYPES; k++)
        for (l = 0; l < COEF_BANDS; l++)
          for (m = 0; m < COEFF_CONTEXTS; m++)
            for (n = 0; n < ENTROPY_TOKENS; n++)
              td->rd_counts.coef_counts[i][j][k][l][m][n] +=
                  td_t->rd_counts.coef_counts[i][j][k][l][m][n];
}

static int enc_worker_hook(EncWorkerData *const thread_data, void *unused) {
  VP9_COMP *const cpi = thread_data->cpi;
  const VP9_COMMON *const cm = &cpi->common;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const int tile_rows = 1 << cm->log2_tile_rows;
  int t;

  (void)unused;

  for (t = thread_data->start; t < tile_rows * tile_cols;
       t += cpi->num_workers) {
    int tile_row = t / tile_cols;
    int tile_col = t % tile_cols;

    vp9_encode_tile(cpi, thread_data->td, tile_row, tile_col);
  }

  return 0;
}

static int get_max_tile_cols(VP9_COMP *cpi) {
  const int aligned_width = ALIGN_POWER_OF_TWO(cpi->oxcf.width, MI_SIZE_LOG2);
  int mi_cols = aligned_width >> MI_SIZE_LOG2;
  int min_log2_tile_cols, max_log2_tile_cols;
  int log2_tile_cols;

  vp9_get_tile_n_bits(mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);
  log2_tile_cols =
      clamp(cpi->oxcf.tile_columns, min_log2_tile_cols, max_log2_tile_cols);
  return (1 << log2_tile_cols);
}

void vp9_encode_tiles_mt(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const VPxWorkerInterface *const winterface = vpx_get_worker_interface();
  const int num_workers = VPXMIN(cpi->oxcf.max_threads, tile_cols);
  int i;

  vp9_init_tile_data(cpi);

  // Only run once to create threads and allocate thread data.
  if (cpi->num_workers == 0) {
    int allocated_workers = num_workers;

    // While using SVC, we need to allocate threads according to the highest
    // resolution.
    if (cpi->use_svc) {
      int max_tile_cols = get_max_tile_cols(cpi);
      allocated_workers = VPXMIN(cpi->oxcf.max_threads, max_tile_cols);
    }

    CHECK_MEM_ERROR(cm, cpi->workers,
                    vpx_malloc(allocated_workers * sizeof(*cpi->workers)));

    CHECK_MEM_ERROR(cm, cpi->tile_thr_data,
                    vpx_calloc(allocated_workers, sizeof(*cpi->tile_thr_data)));

    for (i = 0; i < allocated_workers; i++) {
      VPxWorker *const worker = &cpi->workers[i];
      EncWorkerData *thread_data = &cpi->tile_thr_data[i];

      ++cpi->num_workers;
      winterface->init(worker);

      if (i < allocated_workers - 1) {
        thread_data->cpi = cpi;

        // Allocate thread data.
        CHECK_MEM_ERROR(cm, thread_data->td,
                        vpx_memalign(32, sizeof(*thread_data->td)));
        vp9_zero(*thread_data->td);

        // Set up pc_tree.
        thread_data->td->leaf_tree = NULL;
        thread_data->td->pc_tree = NULL;
        vp9_setup_pc_tree(cm, thread_data->td);

        // Allocate frame counters in thread data.
        CHECK_MEM_ERROR(cm, thread_data->td->counts,
                        vpx_calloc(1, sizeof(*thread_data->td->counts)));

        // Create threads
        if (!winterface->reset(worker))
          vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                             "Tile encoder thread creation failed");
      } else {
        // Main thread acts as a worker and uses the thread data in cpi.
        thread_data->cpi = cpi;
        thread_data->td = &cpi->td;
      }

      winterface->sync(worker);
    }
  }

  for (i = 0; i < num_workers; i++) {
    VPxWorker *const worker = &cpi->workers[i];
    EncWorkerData *thread_data;

    worker->hook = (VPxWorkerHook)enc_worker_hook;
    worker->data1 = &cpi->tile_thr_data[i];
    worker->data2 = NULL;
    thread_data = (EncWorkerData *)worker->data1;

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
      thread_data->td->rd_counts = cpi->td.rd_counts;
    }
    if (thread_data->td->counts != &cpi->common.counts) {
      memcpy(thread_data->td->counts, &cpi->common.counts,
             sizeof(cpi->common.counts));
    }

    // Handle use_nonrd_pick_mode case.
    if (cpi->sf.use_nonrd_pick_mode) {
      MACROBLOCK *const x = &thread_data->td->mb;
      MACROBLOCKD *const xd = &x->e_mbd;
      struct macroblock_plane *const p = x->plane;
      struct macroblockd_plane *const pd = xd->plane;
      PICK_MODE_CONTEXT *ctx = &thread_data->td->pc_root->none;
      int j;

      for (j = 0; j < MAX_MB_PLANE; ++j) {
        p[j].coeff = ctx->coeff_pbuf[j][0];
        p[j].qcoeff = ctx->qcoeff_pbuf[j][0];
        pd[j].dqcoeff = ctx->dqcoeff_pbuf[j][0];
        p[j].eobs = ctx->eobs_pbuf[j][0];
      }
    }
  }

  // Encode a frame
  for (i = 0; i < num_workers; i++) {
    VPxWorker *const worker = &cpi->workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;

    // Set the starting tile for each thread.
    thread_data->start = i;

    if (i == cpi->num_workers - 1)
      winterface->execute(worker);
    else
      winterface->launch(worker);
  }

  // Encoding ends.
  for (i = 0; i < num_workers; i++) {
    VPxWorker *const worker = &cpi->workers[i];
    winterface->sync(worker);
  }

  for (i = 0; i < num_workers; i++) {
    VPxWorker *const worker = &cpi->workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;

    // Accumulate counters.
    if (i < cpi->num_workers - 1) {
      vp9_accumulate_frame_counts(&cm->counts, thread_data->td->counts, 0);
      accumulate_rd_opt(&cpi->td, thread_data->td);
    }
  }
}
