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

#include "config/av1_rtcd.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

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
#include "av1/decoder/detokenize.h"
#include "av1/decoder/obu.h"

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

static void dec_setup_mi(AV1_COMMON *cm) {
  cm->mi = cm->mip;
  cm->mi_grid_visible = cm->mi_grid_base;
  memset(cm->mi_grid_base, 0,
         cm->mi_stride * cm->mi_rows * sizeof(*cm->mi_grid_base));
}

static int av1_dec_alloc_mi(AV1_COMMON *cm, int mi_size) {
  cm->mip = aom_calloc(mi_size, sizeof(*cm->mip));
  if (!cm->mip) return 1;
  cm->mi_alloc_size = mi_size;
  cm->mi_grid_base =
      (MB_MODE_INFO **)aom_calloc(mi_size, sizeof(MB_MODE_INFO *));
  if (!cm->mi_grid_base) return 1;
  return 0;
}

static void dec_free_mi(AV1_COMMON *cm) {
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
  pbi->decoding_first_frame = 1;
  pbi->common.buffer_pool = pool;

  cm->bit_depth = AOM_BITS_8;
  cm->dequant_bit_depth = AOM_BITS_8;

  cm->alloc_mi = av1_dec_alloc_mi;
  cm->free_mi = dec_free_mi;
  cm->setup_mi = dec_setup_mi;

  av1_loop_filter_init(cm);

  av1_qm_init(cm);
  av1_loop_restoration_precal();
#if CONFIG_ACCOUNTING
  pbi->acct_enabled = 1;
  aom_accounting_init(&pbi->accounting);
#endif

  cm->error.setjmp = 0;

  aom_get_worker_interface()->init(&pbi->lf_worker);

  return pbi;
}

void av1_dealloc_dec_jobs(struct AV1DecTileMTData *tile_mt_info) {
  if (tile_mt_info != NULL) {
#if CONFIG_MULTITHREAD
    if (tile_mt_info->job_mutex != NULL) {
      pthread_mutex_destroy(tile_mt_info->job_mutex);
      aom_free(tile_mt_info->job_mutex);
    }
#endif
    aom_free(tile_mt_info->job_queue);
    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*tile_mt_info);
  }
}

void av1_decoder_remove(AV1Decoder *pbi) {
  int i;

  if (!pbi) return;

  // Free the tile list output buffer.
  if (pbi->tile_list_output != NULL) aom_free(pbi->tile_list_output);
  pbi->tile_list_output = NULL;

  aom_get_worker_interface()->end(&pbi->lf_worker);
  aom_free(pbi->lf_worker.data1);

  if (pbi->thread_data) {
    for (int worker_idx = 0; worker_idx < pbi->max_threads - 1; worker_idx++) {
      DecWorkerData *const thread_data = pbi->thread_data + worker_idx;
      const int use_highbd = pbi->common.use_highbitdepth ? 1 : 0;
      av1_free_mc_tmp_buf(thread_data->td, use_highbd);
      aom_free(thread_data->td);
    }
    aom_free(pbi->thread_data);
  }

  for (i = 0; i < pbi->num_workers; ++i) {
    AVxWorker *const worker = &pbi->tile_workers[i];
    aom_get_worker_interface()->end(worker);
  }
  aom_free(pbi->tile_data);
  aom_free(pbi->tile_workers);

  if (pbi->num_workers > 0) {
    av1_loop_filter_dealloc(&pbi->lf_row_sync);
    av1_loop_restoration_dealloc(&pbi->lr_row_sync, pbi->num_workers);
    av1_dealloc_dec_jobs(&pbi->tile_mt_info);
  }

#if CONFIG_ACCOUNTING
  aom_accounting_clear(&pbi->accounting);
#endif
  const int use_highbd = pbi->common.use_highbitdepth ? 1 : 0;
  av1_free_mc_tmp_buf(&pbi->td, use_highbd);

  aom_free(pbi);
}

void av1_visit_palette(AV1Decoder *const pbi, MACROBLOCKD *const xd, int mi_row,
                       int mi_col, aom_reader *r, BLOCK_SIZE bsize,
                       palette_visitor_fn_t visit) {
  if (!is_inter_block(xd->mi[0])) {
    for (int plane = 0; plane < AOMMIN(2, av1_num_planes(&pbi->common));
         ++plane) {
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      if (is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                              pd->subsampling_y)) {
        if (xd->mi[0]->palette_mode_info.palette_size[plane])
          visit(xd, plane, r);
      } else {
        assert(xd->mi[0]->palette_mode_info.palette_size[plane] == 0);
      }
    }
  }
}

static int equal_dimensions(const YV12_BUFFER_CONFIG *a,
                            const YV12_BUFFER_CONFIG *b) {
  return a->y_height == b->y_height && a->y_width == b->y_width &&
         a->uv_height == b->uv_height && a->uv_width == b->uv_width;
}

aom_codec_err_t av1_copy_reference_dec(AV1Decoder *pbi, int idx,
                                       YV12_BUFFER_CONFIG *sd) {
  AV1_COMMON *cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);

  const YV12_BUFFER_CONFIG *const cfg = get_ref_frame(cm, idx);
  if (cfg == NULL) {
    aom_internal_error(&cm->error, AOM_CODEC_ERROR, "No reference frame");
    return AOM_CODEC_ERROR;
  }
  if (!equal_dimensions(cfg, sd))
    aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                       "Incorrect buffer dimensions");
  else
    aom_yv12_copy_frame(cfg, sd, num_planes);

  return cm->error.error_code;
}

static int equal_dimensions_and_border(const YV12_BUFFER_CONFIG *a,
                                       const YV12_BUFFER_CONFIG *b) {
  return a->y_height == b->y_height && a->y_width == b->y_width &&
         a->uv_height == b->uv_height && a->uv_width == b->uv_width &&
         a->y_stride == b->y_stride && a->uv_stride == b->uv_stride &&
         a->border == b->border &&
         (a->flags & YV12_FLAG_HIGHBITDEPTH) ==
             (b->flags & YV12_FLAG_HIGHBITDEPTH);
}

aom_codec_err_t av1_set_reference_dec(AV1_COMMON *cm, int idx,
                                      int use_external_ref,
                                      YV12_BUFFER_CONFIG *sd) {
  const int num_planes = av1_num_planes(cm);
  YV12_BUFFER_CONFIG *ref_buf = NULL;

  // Get the destination reference buffer.
  ref_buf = get_ref_frame(cm, idx);

  if (ref_buf == NULL) {
    aom_internal_error(&cm->error, AOM_CODEC_ERROR, "No reference frame");
    return AOM_CODEC_ERROR;
  }

  if (!use_external_ref) {
    if (!equal_dimensions(ref_buf, sd)) {
      aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                         "Incorrect buffer dimensions");
    } else {
      // Overwrite the reference frame buffer.
      aom_yv12_copy_frame(sd, ref_buf, num_planes);
    }
  } else {
    if (!equal_dimensions_and_border(ref_buf, sd)) {
      aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                         "Incorrect buffer dimensions");
    } else {
      // Overwrite the reference frame buffer pointers.
      // Once we no longer need the external reference buffer, these pointers
      // are restored.
      ref_buf->store_buf_adr[0] = ref_buf->y_buffer;
      ref_buf->store_buf_adr[1] = ref_buf->u_buffer;
      ref_buf->store_buf_adr[2] = ref_buf->v_buffer;
      ref_buf->y_buffer = sd->y_buffer;
      ref_buf->u_buffer = sd->u_buffer;
      ref_buf->v_buffer = sd->v_buffer;
      ref_buf->use_external_refernce_buffers = 1;
    }
  }

  return cm->error.error_code;
}

aom_codec_err_t av1_copy_new_frame_dec(AV1_COMMON *cm,
                                       YV12_BUFFER_CONFIG *new_frame,
                                       YV12_BUFFER_CONFIG *sd) {
  const int num_planes = av1_num_planes(cm);

  if (!equal_dimensions_and_border(new_frame, sd))
    aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                       "Incorrect buffer dimensions");
  else
    aom_yv12_copy_frame(new_frame, sd, num_planes);

  return cm->error.error_code;
}

/* If any buffer updating is signaled it should be done here.
   Consumes a reference to cm->new_fb_idx.
*/
static void swap_frame_buffers(AV1Decoder *pbi, int frame_decoded) {
  int ref_index = 0, mask;
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;
  RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;

  if (frame_decoded) {
    lock_buffer_pool(pool);

    // In ext-tile decoding, the camera frame header is only decoded once. So,
    // we don't release the references here.
    if (!pbi->camera_frame_header_ready) {
      for (mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
        const int old_idx = cm->ref_frame_map[ref_index];
        // Current thread releases the holding of reference frame.
        decrease_ref_count(old_idx, frame_bufs, pool);

        // Release the reference frame holding in the reference map for the
        // decoding of the next frame.
        if (mask & 1) decrease_ref_count(old_idx, frame_bufs, pool);
        cm->ref_frame_map[ref_index] = cm->next_ref_frame_map[ref_index];
        ++ref_index;
      }

      // Current thread releases the holding of reference frame.
      const int check_on_show_existing_frame =
          !cm->show_existing_frame || cm->reset_decoder_state;
      for (; ref_index < REF_FRAMES && check_on_show_existing_frame;
           ++ref_index) {
        const int old_idx = cm->ref_frame_map[ref_index];
        decrease_ref_count(old_idx, frame_bufs, pool);
        cm->ref_frame_map[ref_index] = cm->next_ref_frame_map[ref_index];
      }
    }

    YV12_BUFFER_CONFIG *cur_frame = get_frame_new_buffer(cm);

    if (cm->show_existing_frame || cm->show_frame) {
      if (pbi->output_all_layers) {
        // Append this frame to the output queue
        if (pbi->num_output_frames >= MAX_NUM_SPATIAL_LAYERS) {
          // We can't store the new frame anywhere, so drop it and return an
          // error
          decrease_ref_count(cm->new_fb_idx, frame_bufs, pool);
          cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
        } else {
          pbi->output_frames[pbi->num_output_frames] = cur_frame;
          pbi->output_frame_index[pbi->num_output_frames] = cm->new_fb_idx;
          pbi->num_output_frames++;
        }
      } else {
        // Replace any existing output frame
        assert(pbi->num_output_frames == 0 || pbi->num_output_frames == 1);
        if (pbi->num_output_frames > 0) {
          decrease_ref_count((int)pbi->output_frame_index[0], frame_bufs, pool);
        }
        pbi->output_frames[0] = cur_frame;
        pbi->output_frame_index[0] = cm->new_fb_idx;
        pbi->num_output_frames = 1;
      }
    } else {
      decrease_ref_count(cm->new_fb_idx, frame_bufs, pool);
    }

    unlock_buffer_pool(pool);
  } else {
    // Nothing was decoded, so just drop this frame buffer
    lock_buffer_pool(pool);
    decrease_ref_count(cm->new_fb_idx, frame_bufs, pool);
    unlock_buffer_pool(pool);
  }

  if (!pbi->camera_frame_header_ready) {
    pbi->hold_ref_buf = 0;

    // Invalidate these references until the next frame starts.
    for (ref_index = 0; ref_index < INTER_REFS_PER_FRAME; ref_index++) {
      cm->frame_refs[ref_index].idx = INVALID_IDX;
      cm->frame_refs[ref_index].buf = NULL;
    }
  }
}

int av1_receive_compressed_data(AV1Decoder *pbi, size_t size,
                                const uint8_t **psource) {
  AV1_COMMON *volatile const cm = &pbi->common;
  BufferPool *volatile const pool = cm->buffer_pool;
  RefCntBuffer *volatile const frame_bufs = cm->buffer_pool->frame_bufs;
  const uint8_t *source = *psource;
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

  // Find a free buffer for the new frame, releasing the reference previously
  // held.

  // Find a free frame buffer. Return error if can not find any.
  cm->new_fb_idx = get_free_fb(cm);
  if (cm->new_fb_idx == INVALID_IDX) return AOM_CODEC_MEM_ERROR;

  // Assign a MV array to the frame buffer.
  cm->cur_frame = &pool->frame_bufs[cm->new_fb_idx];

  if (!pbi->camera_frame_header_ready) pbi->hold_ref_buf = 0;

  pbi->cur_buf = &frame_bufs[cm->new_fb_idx];

  if (setjmp(cm->error.jmp)) {
    const AVxWorkerInterface *const winterface = aom_get_worker_interface();
    int i;

    cm->error.setjmp = 0;

    // Synchronize all threads immediately as a subsequent decode call may
    // cause a resize invalidating some allocations.
    winterface->sync(&pbi->lf_worker);
    for (i = 0; i < pbi->num_workers; ++i) {
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
      const int check_on_show_existing_frame =
          !cm->show_existing_frame || cm->reset_decoder_state;
      for (; ref_index < REF_FRAMES && check_on_show_existing_frame;
           ++ref_index) {
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

  int frame_decoded =
      aom_decode_frame_from_obus(pbi, source, source + size, psource);

  if (cm->error.error_code != AOM_CODEC_OK) return 1;

#if TXCOEFF_TIMER
  cm->cum_txcoeff_timer += cm->txcoeff_timer;
  fprintf(stderr,
          "txb coeff block number: %d, frame time: %ld, cum time %ld in us\n",
          cm->txb_count, cm->txcoeff_timer, cm->cum_txcoeff_timer);
  cm->txcoeff_timer = 0;
  cm->txb_count = 0;
#endif

  // Note: At this point, this function holds a reference to cm->new_fb_idx
  // in the buffer pool. This reference is consumed by swap_frame_buffers().
  swap_frame_buffers(pbi, frame_decoded);

  if (frame_decoded) {
    pbi->decoding_first_frame = 0;
  }

  if (cm->error.error_code != AOM_CODEC_OK) return 1;

  aom_clear_system_state();

  if (!cm->show_existing_frame) {
    cm->last_show_frame = cm->show_frame;

    if (cm->seg.enabled) {
      if (cm->prev_frame && (cm->mi_rows == cm->prev_frame->mi_rows) &&
          (cm->mi_cols == cm->prev_frame->mi_cols)) {
        cm->last_frame_seg_map = cm->prev_frame->seg_map;
      } else {
        cm->last_frame_seg_map = NULL;
      }
    }
  }

  // Update progress in frame parallel decode.
  cm->last_width = cm->width;
  cm->last_height = cm->height;
  cm->last_tile_cols = cm->tile_cols;
  cm->last_tile_rows = cm->tile_rows;
  cm->error.setjmp = 0;

  return 0;
}

// Get the frame at a particular index in the output queue
int av1_get_raw_frame(AV1Decoder *pbi, size_t index, YV12_BUFFER_CONFIG **sd,
                      aom_film_grain_t **grain_params) {
  RefCntBuffer *const frame_bufs = pbi->common.buffer_pool->frame_bufs;

  if (index >= pbi->num_output_frames) return -1;
  *sd = pbi->output_frames[index];
  *grain_params = &frame_bufs[pbi->output_frame_index[index]].film_grain_params;
  aom_clear_system_state();
  return 0;
}

// Get the highest-spatial-layer output
// TODO(david.barker): What should this do?
int av1_get_frame_to_show(AV1Decoder *pbi, YV12_BUFFER_CONFIG *frame) {
  if (pbi->num_output_frames == 0) return -1;

  *frame = *pbi->output_frames[pbi->num_output_frames - 1];
  return 0;
}
