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

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_scale_rtcd.h"

#include "aom_mem/aom_mem.h"
#include "aom_ports/system_state.h"
#include "aom_ports/aom_once.h"
#include "aom_ports/aom_timer.h"
#include "aom_scale/aom_scale.h"
#include "aom_util/aom_thread.h"

#include "av1/common/alloccommon.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"

#include "av1/decoder/decodeframe.h"
#include "av1/decoder/decoder.h"
#if CONFIG_NCOBMC_ADAPT_WEIGHT
#include "av1/common/ncobmc_kernels.h"
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#if !CONFIG_PVQ
#include "av1/decoder/detokenize.h"
#endif

static void initialize_dec(void) {
  static volatile int init_done = 0;

  if (!init_done) {
    av1_rtcd();
    aom_dsp_rtcd();
    aom_scale_rtcd();
    av1_init_intra_predictors();
    av1_init_wedge_masks();
    init_done = 1;
  }
}

static void av1_dec_setup_mi(AV1_COMMON *cm) {
  cm->mi = cm->mip + cm->mi_stride + 1;
  cm->mi_grid_visible = cm->mi_grid_base + cm->mi_stride + 1;
  memset(cm->mi_grid_base, 0,
         cm->mi_stride * (cm->mi_rows + 1) * sizeof(*cm->mi_grid_base));
}

static int av1_dec_alloc_mi(AV1_COMMON *cm, int mi_size) {
  cm->mip = aom_calloc(mi_size, sizeof(*cm->mip));
  if (!cm->mip) return 1;
  cm->mi_alloc_size = mi_size;
  cm->mi_grid_base = (MODE_INFO **)aom_calloc(mi_size, sizeof(MODE_INFO *));
  if (!cm->mi_grid_base) return 1;
  return 0;
}

static void av1_dec_free_mi(AV1_COMMON *cm) {
  aom_free(cm->mip);
  cm->mip = NULL;
  aom_free(cm->mi_grid_base);
  cm->mi_grid_base = NULL;
}

AV1Decoder *av1_decoder_create(BufferPool *const pool) {
  AV1Decoder *volatile const pbi = aom_memalign(32, sizeof(*pbi));
  AV1_COMMON *volatile const cm = pbi ? &pbi->common : NULL;

  if (!cm) return NULL;

  av1_zero(*pbi);

  if (setjmp(cm->error.jmp)) {
    cm->error.setjmp = 0;
    av1_decoder_remove(pbi);
    return NULL;
  }

  cm->error.setjmp = 1;

  CHECK_MEM_ERROR(cm, cm->fc,
                  (FRAME_CONTEXT *)aom_memalign(32, sizeof(*cm->fc)));
  CHECK_MEM_ERROR(cm, cm->frame_contexts,
                  (FRAME_CONTEXT *)aom_memalign(
                      32, FRAME_CONTEXTS * sizeof(*cm->frame_contexts)));
  memset(cm->fc, 0, sizeof(*cm->fc));
  memset(cm->frame_contexts, 0, FRAME_CONTEXTS * sizeof(*cm->frame_contexts));

  pbi->need_resync = 1;
  once(initialize_dec);

  // Initialize the references to not point to any frame buffers.
  memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));
  memset(&cm->next_ref_frame_map, -1, sizeof(cm->next_ref_frame_map));

  cm->current_video_frame = 0;
  pbi->ready_for_new_data = 1;
  pbi->common.buffer_pool = pool;

  cm->bit_depth = AOM_BITS_8;
  cm->dequant_bit_depth = AOM_BITS_8;

  cm->alloc_mi = av1_dec_alloc_mi;
  cm->free_mi = av1_dec_free_mi;
  cm->setup_mi = av1_dec_setup_mi;

  av1_loop_filter_init(cm);

#if CONFIG_NCOBMC_ADAPT_WEIGHT
  get_default_ncobmc_kernels(cm);
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT

#if CONFIG_AOM_QM
  aom_qm_init(cm);
#endif
#if CONFIG_LOOP_RESTORATION
  av1_loop_restoration_precal();
#endif  // CONFIG_LOOP_RESTORATION
#if CONFIG_ACCOUNTING
  pbi->acct_enabled = 1;
  aom_accounting_init(&pbi->accounting);
#endif

  cm->error.setjmp = 0;

  aom_get_worker_interface()->init(&pbi->lf_worker);

  return pbi;
}

void av1_decoder_remove(AV1Decoder *pbi) {
  int i;

  if (!pbi) return;

  aom_get_worker_interface()->end(&pbi->lf_worker);
  aom_free(pbi->lf_worker.data1);
  aom_free(pbi->tile_data);
  for (i = 0; i < pbi->num_tile_workers; ++i) {
    AVxWorker *const worker = &pbi->tile_workers[i];
    aom_get_worker_interface()->end(worker);
  }
  aom_free(pbi->tile_worker_data);
  aom_free(pbi->tile_worker_info);
  aom_free(pbi->tile_workers);

  if (pbi->num_tile_workers > 0) {
    av1_loop_filter_dealloc(&pbi->lf_row_sync);
  }

#if CONFIG_ACCOUNTING
  aom_accounting_clear(&pbi->accounting);
#endif

  aom_free(pbi);
}

static int equal_dimensions(const YV12_BUFFER_CONFIG *a,
                            const YV12_BUFFER_CONFIG *b) {
  return a->y_height == b->y_height && a->y_width == b->y_width &&
         a->uv_height == b->uv_height && a->uv_width == b->uv_width;
}

aom_codec_err_t av1_copy_reference_dec(AV1Decoder *pbi, int idx,
                                       YV12_BUFFER_CONFIG *sd) {
  AV1_COMMON *cm = &pbi->common;

  const YV12_BUFFER_CONFIG *const cfg = get_ref_frame(cm, idx);
  if (cfg == NULL) {
    aom_internal_error(&cm->error, AOM_CODEC_ERROR, "No reference frame");
    return AOM_CODEC_ERROR;
  }
  if (!equal_dimensions(cfg, sd))
    aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                       "Incorrect buffer dimensions");
  else
    aom_yv12_copy_frame(cfg, sd);

  return cm->error.error_code;
}

aom_codec_err_t av1_set_reference_dec(AV1_COMMON *cm, int idx,
                                      YV12_BUFFER_CONFIG *sd) {
  YV12_BUFFER_CONFIG *ref_buf = NULL;

  // Get the destination reference buffer.
  ref_buf = get_ref_frame(cm, idx);

  if (ref_buf == NULL) {
    aom_internal_error(&cm->error, AOM_CODEC_ERROR, "No reference frame");
    return AOM_CODEC_ERROR;
  }

  if (!equal_dimensions(ref_buf, sd)) {
    aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                       "Incorrect buffer dimensions");
  } else {
    // Overwrite the reference frame buffer.
    aom_yv12_copy_frame(sd, ref_buf);
  }

  return cm->error.error_code;
}

/* If any buffer updating is signaled it should be done here. */
static void swap_frame_buffers(AV1Decoder *pbi) {
  int ref_index = 0, mask;
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;
  RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;

  lock_buffer_pool(pool);
  for (mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
    const int old_idx = cm->ref_frame_map[ref_index];
    // Current thread releases the holding of reference frame.
    decrease_ref_count(old_idx, frame_bufs, pool);

    // Release the reference frame holding in the reference map for the decoding
    // of the next frame.
    if (mask & 1) decrease_ref_count(old_idx, frame_bufs, pool);
    cm->ref_frame_map[ref_index] = cm->next_ref_frame_map[ref_index];
    ++ref_index;
  }

  // Current thread releases the holding of reference frame.
  for (; ref_index < REF_FRAMES && !cm->show_existing_frame; ++ref_index) {
    const int old_idx = cm->ref_frame_map[ref_index];
    decrease_ref_count(old_idx, frame_bufs, pool);
    cm->ref_frame_map[ref_index] = cm->next_ref_frame_map[ref_index];
  }

  unlock_buffer_pool(pool);
  pbi->hold_ref_buf = 0;
  cm->frame_to_show = get_frame_new_buffer(cm);

  // TODO(zoeliu): To fix the ref frame buffer update for the scenario of
  //               cm->frame_parellel_decode == 1
  if (!cm->frame_parallel_decode || !cm->show_frame) {
    lock_buffer_pool(pool);
    --frame_bufs[cm->new_fb_idx].ref_count;
    unlock_buffer_pool(pool);
  }

  // Invalidate these references until the next frame starts.
  for (ref_index = 0; ref_index < INTER_REFS_PER_FRAME; ref_index++) {
    cm->frame_refs[ref_index].idx = INVALID_IDX;
    cm->frame_refs[ref_index].buf = NULL;
  }
}

int av1_receive_compressed_data(AV1Decoder *pbi, size_t size,
                                const uint8_t **psource) {
  AV1_COMMON *volatile const cm = &pbi->common;
  BufferPool *volatile const pool = cm->buffer_pool;
  RefCntBuffer *volatile const frame_bufs = cm->buffer_pool->frame_bufs;
  const uint8_t *source = *psource;
  int retcode = 0;
  cm->error.error_code = AOM_CODEC_OK;

  if (size == 0) {
    // This is used to signal that we are missing frames.
    // We do not know if the missing frame(s) was supposed to update
    // any of the reference buffers, but we act conservative and
    // mark only the last buffer as corrupted.
    //
    // TODO(jkoleszar): Error concealment is undefined and non-normative
    // at this point, but if it becomes so, [0] may not always be the correct
    // thing to do here.
    if (cm->frame_refs[0].idx > 0) {
      assert(cm->frame_refs[0].buf != NULL);
      cm->frame_refs[0].buf->corrupted = 1;
    }
  }

  pbi->ready_for_new_data = 0;

  // Find a free buffer for the new frame, releasing the reference previously
  // held.

  // Check if the previous frame was a frame without any references to it.
  // Release frame buffer if not decoding in frame parallel mode.
  if (!cm->frame_parallel_decode && cm->new_fb_idx >= 0 &&
      frame_bufs[cm->new_fb_idx].ref_count == 0)
    pool->release_fb_cb(pool->cb_priv,
                        &frame_bufs[cm->new_fb_idx].raw_frame_buffer);

  // Find a free frame buffer. Return error if can not find any.
  cm->new_fb_idx = get_free_fb(cm);
  if (cm->new_fb_idx == INVALID_IDX) return AOM_CODEC_MEM_ERROR;

  // Assign a MV array to the frame buffer.
  cm->cur_frame = &pool->frame_bufs[cm->new_fb_idx];

  pbi->hold_ref_buf = 0;
  if (cm->frame_parallel_decode) {
    AVxWorker *const worker = pbi->frame_worker_owner;
    av1_frameworker_lock_stats(worker);
    frame_bufs[cm->new_fb_idx].frame_worker_owner = worker;
    // Reset decoding progress.
    pbi->cur_buf = &frame_bufs[cm->new_fb_idx];
    pbi->cur_buf->row = -1;
    pbi->cur_buf->col = -1;
    av1_frameworker_unlock_stats(worker);
  } else {
    pbi->cur_buf = &frame_bufs[cm->new_fb_idx];
  }

  if (setjmp(cm->error.jmp)) {
    const AVxWorkerInterface *const winterface = aom_get_worker_interface();
    int i;

    cm->error.setjmp = 0;
    pbi->ready_for_new_data = 1;

    // Synchronize all threads immediately as a subsequent decode call may
    // cause a resize invalidating some allocations.
    winterface->sync(&pbi->lf_worker);
    for (i = 0; i < pbi->num_tile_workers; ++i) {
      winterface->sync(&pbi->tile_workers[i]);
    }

    lock_buffer_pool(pool);
    // Release all the reference buffers if worker thread is holding them.
    if (pbi->hold_ref_buf == 1) {
      int ref_index = 0, mask;
      for (mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
        const int old_idx = cm->ref_frame_map[ref_index];
        // Current thread releases the holding of reference frame.
        decrease_ref_count(old_idx, frame_bufs, pool);

        // Release the reference frame holding in the reference map for the
        // decoding of the next frame.
        if (mask & 1) decrease_ref_count(old_idx, frame_bufs, pool);
        ++ref_index;
      }

      // Current thread releases the holding of reference frame.
      for (; ref_index < REF_FRAMES && !cm->show_existing_frame; ++ref_index) {
        const int old_idx = cm->ref_frame_map[ref_index];
        decrease_ref_count(old_idx, frame_bufs, pool);
      }
      pbi->hold_ref_buf = 0;
    }
    // Release current frame.
    decrease_ref_count(cm->new_fb_idx, frame_bufs, pool);
    unlock_buffer_pool(pool);

    aom_clear_system_state();
    return -1;
  }

  cm->error.setjmp = 1;

#if !CONFIG_OBU
  av1_decode_frame_headers_and_setup(pbi, source, source + size, psource);
  if (!cm->show_existing_frame) {
    av1_decode_tg_tiles_and_wrapup(pbi, source, source + size, psource, 0,
                                   cm->tile_rows * cm->tile_cols - 1, 1);
  }
#else
  av1_decode_frame_from_obus(pbi, source, source + size, psource);
#endif

  swap_frame_buffers(pbi);

#if CONFIG_EXT_TILE
  // For now, we only extend the frame borders when the whole frame is decoded.
  // Later, if needed, extend the border for the decoded tile on the frame
  // border.
  if (pbi->dec_tile_row == -1 && pbi->dec_tile_col == -1)
#endif  // CONFIG_EXT_TILE
    // TODO(debargha): Fix encoder side mv range, so that we can use the
    // inner border extension. As of now use the larger extension.
    // aom_extend_frame_inner_borders(cm->frame_to_show);
    aom_extend_frame_borders(cm->frame_to_show);

  aom_clear_system_state();

  if (!cm->show_existing_frame) {
    cm->last_show_frame = cm->show_frame;

#if CONFIG_EXT_REFS
    // NOTE: It is not supposed to ref to any frame not used as reference
    if (cm->is_reference_frame)
#endif  // CONFIG_EXT_REFS
      cm->prev_frame = cm->cur_frame;

    if (cm->seg.enabled && !cm->frame_parallel_decode)
      av1_swap_current_and_last_seg_map(cm);
  }

  // Update progress in frame parallel decode.
  if (cm->frame_parallel_decode) {
    // Need to lock the mutex here as another thread may
    // be accessing this buffer.
    AVxWorker *const worker = pbi->frame_worker_owner;
    FrameWorkerData *const frame_worker_data = worker->data1;
    av1_frameworker_lock_stats(worker);

    if (cm->show_frame) {
      cm->current_video_frame++;
    }
    frame_worker_data->frame_decoded = 1;
    frame_worker_data->frame_context_ready = 1;
    av1_frameworker_signal_stats(worker);
    av1_frameworker_unlock_stats(worker);
  } else {
    cm->last_width = cm->width;
    cm->last_height = cm->height;
    cm->last_tile_cols = cm->tile_cols;
    cm->last_tile_rows = cm->tile_rows;
    if (cm->show_frame) {
      cm->current_video_frame++;
    }
  }

  cm->error.setjmp = 0;
  return retcode;
}

int av1_get_raw_frame(AV1Decoder *pbi, YV12_BUFFER_CONFIG *sd) {
  AV1_COMMON *const cm = &pbi->common;
  int ret = -1;
  if (pbi->ready_for_new_data == 1) return ret;

  pbi->ready_for_new_data = 1;

  /* no raw frame to show!!! */
  if (!cm->show_frame) return ret;

  *sd = *cm->frame_to_show;
  ret = 0;
  aom_clear_system_state();
  return ret;
}

int av1_get_frame_to_show(AV1Decoder *pbi, YV12_BUFFER_CONFIG *frame) {
  AV1_COMMON *const cm = &pbi->common;

  if (!cm->show_frame || !cm->frame_to_show) return -1;

  *frame = *cm->frame_to_show;
  return 0;
}

aom_codec_err_t av1_parse_superframe_index(const uint8_t *data, size_t data_sz,
                                           uint32_t sizes[8], int *count,
                                           int *index_size,
                                           aom_decrypt_cb decrypt_cb,
                                           void *decrypt_state) {
  // A chunk ending with a byte matching 0xc0 is an invalid chunk unless
  // it is a super frame index. If the last byte of real video compression
  // data is 0xc0 the encoder must add a 0 byte. If we have the marker but
  // not the associated matching marker byte at the front of the index we have
  // an invalid bitstream and need to return an error.

  uint8_t marker;
  size_t frame_sz_sum = 0;

  assert(data_sz);
  marker = read_marker(decrypt_cb, decrypt_state, data);
  *count = 0;

  if ((marker & 0xe0) == 0xc0) {
    const uint32_t frames = (marker & 0x7) + 1;
    const uint32_t mag = ((marker >> 3) & 0x3) + 1;
    const size_t index_sz = 2 + mag * (frames - 1);
    *index_size = (int)index_sz;

    // This chunk is marked as having a superframe index but doesn't have
    // enough data for it, thus it's an invalid superframe index.
    if (data_sz < index_sz) return AOM_CODEC_CORRUPT_FRAME;

    {
      const uint8_t marker2 =
          read_marker(decrypt_cb, decrypt_state, data + index_sz - 1);

      // This chunk is marked as having a superframe index but doesn't have
      // the matching marker byte at the front of the index therefore it's an
      // invalid chunk.
      if (marker != marker2) return AOM_CODEC_CORRUPT_FRAME;
    }

    {
      // Found a valid superframe index.
      uint32_t i, j;
      const uint8_t *x = &data[1];

      // Frames has a maximum of 8 and mag has a maximum of 4.
      uint8_t clear_buffer[28];
      assert(sizeof(clear_buffer) >= (frames - 1) * mag);
      if (decrypt_cb) {
        decrypt_cb(decrypt_state, x, clear_buffer, (frames - 1) * mag);
        x = clear_buffer;
      }

      for (i = 0; i < frames - 1; ++i) {
        uint32_t this_sz = 0;

        for (j = 0; j < mag; ++j) this_sz |= (*x++) << (j * 8);
        this_sz += 1;
        sizes[i] = this_sz;
        frame_sz_sum += this_sz;
      }
      sizes[i] = (uint32_t)(data_sz - index_sz - frame_sz_sum);
      *count = frames;
    }
  }
  return AOM_CODEC_OK;
}
