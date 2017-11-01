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

#include <stdlib.h>
#include <string.h>

#include "./aom_config.h"
#include "./aom_version.h"

#include "aom/internal/aom_codec_internal.h"
#include "aom/aomdx.h"
#include "aom/aom_decoder.h"
#include "aom_dsp/bitreader_buffer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_util/aom_thread.h"

#include "av1/common/alloccommon.h"
#include "av1/common/frame_buffers.h"
#include "av1/common/enums.h"

#include "av1/decoder/decoder.h"
#include "av1/decoder/decodeframe.h"

#include "av1/av1_iface_common.h"

// This limit is due to framebuffer numbers.
// TODO(hkuang): Remove this limit after implementing ondemand framebuffers.
#define FRAME_CACHE_SIZE 6  // Cache maximum 6 decoded frames.

typedef struct cache_frame {
  int fb_idx;
  aom_image_t img;
} cache_frame;

struct aom_codec_alg_priv {
  aom_codec_priv_t base;
  aom_codec_dec_cfg_t cfg;
  aom_codec_stream_info_t si;
  int postproc_cfg_set;
  aom_postproc_cfg_t postproc_cfg;
  aom_decrypt_cb decrypt_cb;
  void *decrypt_state;
  aom_image_t img;
  int img_avail;
  int flushed;
  int invert_tile_order;
  int last_show_frame;  // Index of last output frame.
  int byte_alignment;
  int skip_loop_filter;
  int decode_tile_row;
  int decode_tile_col;

  // Frame parallel related.
  int frame_parallel_decode;  // frame-based threading.
  AVxWorker *frame_workers;
  int num_frame_workers;
  int next_submit_worker_id;
  int last_submit_worker_id;
  int next_output_worker_id;
  int available_threads;
  cache_frame frame_cache[FRAME_CACHE_SIZE];
  int frame_cache_write;
  int frame_cache_read;
  int num_cache_frames;
  int need_resync;  // wait for key/intra-only frame
  // BufferPool that holds all reference frames. Shared by all the FrameWorkers.
  BufferPool *buffer_pool;

  // External frame buffer info to save for AV1 common.
  void *ext_priv;  // Private data associated with the external frame buffers.
  aom_get_frame_buffer_cb_fn_t get_ext_fb_cb;
  aom_release_frame_buffer_cb_fn_t release_ext_fb_cb;

#if CONFIG_INSPECTION
  aom_inspect_cb inspect_cb;
  void *inspect_ctx;
#endif
};

static aom_codec_err_t decoder_init(aom_codec_ctx_t *ctx,
                                    aom_codec_priv_enc_mr_cfg_t *data) {
  // This function only allocates space for the aom_codec_alg_priv_t
  // structure. More memory may be required at the time the stream
  // information becomes known.
  (void)data;

  if (!ctx->priv) {
    aom_codec_alg_priv_t *const priv =
        (aom_codec_alg_priv_t *)aom_calloc(1, sizeof(*priv));
    if (priv == NULL) return AOM_CODEC_MEM_ERROR;

    ctx->priv = (aom_codec_priv_t *)priv;
    ctx->priv->init_flags = ctx->init_flags;
    priv->flushed = 0;
    // Only do frame parallel decode when threads > 1.
    priv->frame_parallel_decode =
        (ctx->config.dec && (ctx->config.dec->threads > 1) &&
         (ctx->init_flags & AOM_CODEC_USE_FRAME_THREADING))
            ? 1
            : 0;
    // TODO(tdaede): this should not be exposed to the API
    priv->cfg.allow_lowbitdepth = CONFIG_LOWBITDEPTH;
    if (ctx->config.dec) {
      priv->cfg = *ctx->config.dec;
      ctx->config.dec = &priv->cfg;
    }
  }

  return AOM_CODEC_OK;
}

static aom_codec_err_t decoder_destroy(aom_codec_alg_priv_t *ctx) {
  if (ctx->frame_workers != NULL) {
    int i;
    for (i = 0; i < ctx->num_frame_workers; ++i) {
      AVxWorker *const worker = &ctx->frame_workers[i];
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      aom_get_worker_interface()->end(worker);
      av1_remove_common(&frame_worker_data->pbi->common);
#if CONFIG_LOOP_RESTORATION
      av1_free_restoration_buffers(&frame_worker_data->pbi->common);
#endif  // CONFIG_LOOP_RESTORATION
      av1_decoder_remove(frame_worker_data->pbi);
      aom_free(frame_worker_data->scratch_buffer);
#if CONFIG_MULTITHREAD
      pthread_mutex_destroy(&frame_worker_data->stats_mutex);
      pthread_cond_destroy(&frame_worker_data->stats_cond);
#endif
      aom_free(frame_worker_data);
    }
#if CONFIG_MULTITHREAD
    pthread_mutex_destroy(&ctx->buffer_pool->pool_mutex);
#endif
  }

  if (ctx->buffer_pool) {
    av1_free_ref_frame_buffers(ctx->buffer_pool);
    av1_free_internal_frame_buffers(&ctx->buffer_pool->int_frame_buffers);
  }

  aom_free(ctx->frame_workers);
  aom_free(ctx->buffer_pool);
  aom_free(ctx);
  return AOM_CODEC_OK;
}

#if !CONFIG_OBU
static int parse_bitdepth_colorspace_sampling(BITSTREAM_PROFILE profile,
                                              struct aom_read_bit_buffer *rb) {
  aom_color_space_t color_space;
#if CONFIG_COLORSPACE_HEADERS
  int subsampling_x = 0;
  int subsampling_y = 0;
#endif

  if (profile >= PROFILE_2) rb->bit_offset += 1;  // Bit-depth 10 or 12.
#if CONFIG_COLORSPACE_HEADERS
  color_space = (aom_color_space_t)aom_rb_read_literal(rb, 5);
  rb->bit_offset += 5;  // Transfer function
#else
  color_space = (aom_color_space_t)aom_rb_read_literal(rb, 3);
#endif
  if (color_space != AOM_CS_SRGB) {
    rb->bit_offset += 1;  // [16,235] (including xvycc) vs [0,255] range.

    if (profile == PROFILE_1 || profile == PROFILE_3) {
#if CONFIG_COLORSPACE_HEADERS
      subsampling_x = aom_rb_read_bit(rb);
      subsampling_y = aom_rb_read_bit(rb);
#else
      rb->bit_offset += 2;  // subsampling x/y.
#endif
      rb->bit_offset += 1;  // unused.
#if CONFIG_COLORSPACE_HEADERS
    } else {
      subsampling_x = 1;
      subsampling_y = 1;
    }
    if (subsampling_x == 1 && subsampling_y == 1) {
      rb->bit_offset += 2;
    }
#else
    }
#endif
  } else {
    if (profile == PROFILE_1 || profile == PROFILE_3) {
      rb->bit_offset += 1;  // unused
    } else {
      // RGB is only available in version 1.
      return 0;
    }
  }
  return 1;
}
#endif

static aom_codec_err_t decoder_peek_si_internal(
    const uint8_t *data, unsigned int data_sz, aom_codec_stream_info_t *si,
    int *is_intra_only, aom_decrypt_cb decrypt_cb, void *decrypt_state) {
  int intra_only_flag = 0;
  uint8_t clear_buffer[9];

  if (data + data_sz <= data) return AOM_CODEC_INVALID_PARAM;

  si->is_kf = 0;
  si->w = si->h = 0;

  if (decrypt_cb) {
    data_sz = AOMMIN(sizeof(clear_buffer), data_sz);
    decrypt_cb(decrypt_state, data, clear_buffer, data_sz);
    data = clear_buffer;
  }

  // skip a potential superframe index
  {
    uint32_t frame_sizes[8];
    int frame_count;
    int index_size = 0;
    aom_codec_err_t res = av1_parse_superframe_index(
        data, data_sz, frame_sizes, &frame_count, &index_size, NULL, NULL);
    if (res != AOM_CODEC_OK) return res;

    data += index_size;
    data_sz -= index_size;
#if CONFIG_OBU
    if (data + data_sz <= data) return AOM_CODEC_INVALID_PARAM;
#endif
  }

  {
#if CONFIG_OBU
    // Proper fix needed
    si->is_kf = 1;
    intra_only_flag = 1;
    si->h = 1;
#else
    int show_frame;
    int error_resilient;
    struct aom_read_bit_buffer rb = { data, data + data_sz, 0, NULL, NULL };
    const int frame_marker = aom_rb_read_literal(&rb, 2);
    const BITSTREAM_PROFILE profile = av1_read_profile(&rb);
#if CONFIG_EXT_TILE
    unsigned int large_scale_tile;
#endif  // CONFIG_EXT_TILE

    if (frame_marker != AOM_FRAME_MARKER) return AOM_CODEC_UNSUP_BITSTREAM;

    if (profile >= MAX_PROFILES) return AOM_CODEC_UNSUP_BITSTREAM;

    if ((profile >= 2 && data_sz <= 1) || data_sz < 1)
      return AOM_CODEC_UNSUP_BITSTREAM;

#if CONFIG_EXT_TILE
    large_scale_tile = aom_rb_read_literal(&rb, 1);
#endif  // CONFIG_EXT_TILE

    if (aom_rb_read_bit(&rb)) {     // show an existing frame
      aom_rb_read_literal(&rb, 3);  // Frame buffer to show.
      return AOM_CODEC_OK;
    }

    if (data_sz <= 8) return AOM_CODEC_UNSUP_BITSTREAM;

    si->is_kf = !aom_rb_read_bit(&rb);
    show_frame = aom_rb_read_bit(&rb);
    if (!si->is_kf) {
      if (!show_frame) intra_only_flag = show_frame ? 0 : aom_rb_read_bit(&rb);
    }
    error_resilient = aom_rb_read_bit(&rb);
#if CONFIG_REFERENCE_BUFFER
    SequenceHeader seq_params = { 0, 0, 0 };
    if (si->is_kf) {
      /* TODO: Move outside frame loop or inside key-frame branch */
      read_sequence_header(&seq_params, &rb);
#if CONFIG_EXT_TILE
      if (large_scale_tile) seq_params.frame_id_numbers_present_flag = 0;
#endif  // CONFIG_EXT_TILE
    }
#endif  // CONFIG_REFERENCE_BUFFER
#if CONFIG_REFERENCE_BUFFER
    if (seq_params.frame_id_numbers_present_flag) {
      int frame_id_len;
      frame_id_len = seq_params.frame_id_length_minus7 + 7;
      aom_rb_read_literal(&rb, frame_id_len);
    }
#endif  // CONFIG_REFERENCE_BUFFER
    if (si->is_kf) {
      if (!parse_bitdepth_colorspace_sampling(profile, &rb))
        return AOM_CODEC_UNSUP_BITSTREAM;
      av1_read_frame_size(&rb, (int *)&si->w, (int *)&si->h);
    } else {
      rb.bit_offset += error_resilient ? 0 : 2;  // reset_frame_context

      if (intra_only_flag) {
        if (profile > PROFILE_0) {
          if (!parse_bitdepth_colorspace_sampling(profile, &rb))
            return AOM_CODEC_UNSUP_BITSTREAM;
        }
        rb.bit_offset += REF_FRAMES;  // refresh_frame_flags
        av1_read_frame_size(&rb, (int *)&si->w, (int *)&si->h);
      }
    }
#endif  // CONFIG_OBU
  }
  if (is_intra_only != NULL) *is_intra_only = intra_only_flag;
  return AOM_CODEC_OK;
}

static aom_codec_err_t decoder_peek_si(const uint8_t *data,
                                       unsigned int data_sz,
                                       aom_codec_stream_info_t *si) {
  return decoder_peek_si_internal(data, data_sz, si, NULL, NULL, NULL);
}

static aom_codec_err_t decoder_get_si(aom_codec_alg_priv_t *ctx,
                                      aom_codec_stream_info_t *si) {
  memcpy(si, &ctx->si, sizeof(*si));

  return AOM_CODEC_OK;
}

static void set_error_detail(aom_codec_alg_priv_t *ctx,
                             const char *const error) {
  ctx->base.err_detail = error;
}

static aom_codec_err_t update_error_state(
    aom_codec_alg_priv_t *ctx, const struct aom_internal_error_info *error) {
  if (error->error_code)
    set_error_detail(ctx, error->has_detail ? error->detail : NULL);

  return error->error_code;
}

static void init_buffer_callbacks(aom_codec_alg_priv_t *ctx) {
  int i;

  for (i = 0; i < ctx->num_frame_workers; ++i) {
    AVxWorker *const worker = &ctx->frame_workers[i];
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    AV1_COMMON *const cm = &frame_worker_data->pbi->common;
    BufferPool *const pool = cm->buffer_pool;

    cm->new_fb_idx = INVALID_IDX;
    cm->byte_alignment = ctx->byte_alignment;
    cm->skip_loop_filter = ctx->skip_loop_filter;

    if (ctx->get_ext_fb_cb != NULL && ctx->release_ext_fb_cb != NULL) {
      pool->get_fb_cb = ctx->get_ext_fb_cb;
      pool->release_fb_cb = ctx->release_ext_fb_cb;
      pool->cb_priv = ctx->ext_priv;
    } else {
      pool->get_fb_cb = av1_get_frame_buffer;
      pool->release_fb_cb = av1_release_frame_buffer;

      if (av1_alloc_internal_frame_buffers(&pool->int_frame_buffers))
        aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to initialize internal frame buffers");

      pool->cb_priv = &pool->int_frame_buffers;
    }
  }
}

static void set_default_ppflags(aom_postproc_cfg_t *cfg) {
  cfg->post_proc_flag = AOM_DEBLOCK | AOM_DEMACROBLOCK;
  cfg->deblocking_level = 4;
  cfg->noise_level = 0;
}

static int frame_worker_hook(void *arg1, void *arg2) {
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)arg1;
  const uint8_t *data = frame_worker_data->data;
  (void)arg2;

  frame_worker_data->result = av1_receive_compressed_data(
      frame_worker_data->pbi, frame_worker_data->data_size, &data);
  frame_worker_data->data_end = data;

  if (frame_worker_data->pbi->common.frame_parallel_decode) {
    // In frame parallel decoding, a worker thread must successfully decode all
    // the compressed data.
    if (frame_worker_data->result != 0 ||
        frame_worker_data->data + frame_worker_data->data_size - 1 > data) {
      AVxWorker *const worker = frame_worker_data->pbi->frame_worker_owner;
      BufferPool *const pool = frame_worker_data->pbi->common.buffer_pool;
      // Signal all the other threads that are waiting for this frame.
      av1_frameworker_lock_stats(worker);
      frame_worker_data->frame_context_ready = 1;
      lock_buffer_pool(pool);
      frame_worker_data->pbi->cur_buf->buf.corrupted = 1;
      unlock_buffer_pool(pool);
      frame_worker_data->pbi->need_resync = 1;
      av1_frameworker_signal_stats(worker);
      av1_frameworker_unlock_stats(worker);
      return 0;
    }
  } else if (frame_worker_data->result != 0) {
    // Check decode result in serial decode.
    frame_worker_data->pbi->cur_buf->buf.corrupted = 1;
    frame_worker_data->pbi->need_resync = 1;
  }
  return !frame_worker_data->result;
}

static aom_codec_err_t init_decoder(aom_codec_alg_priv_t *ctx) {
  int i;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  ctx->last_show_frame = -1;
  ctx->next_submit_worker_id = 0;
  ctx->last_submit_worker_id = 0;
  ctx->next_output_worker_id = 0;
  ctx->frame_cache_read = 0;
  ctx->frame_cache_write = 0;
  ctx->num_cache_frames = 0;
  ctx->need_resync = 1;
  ctx->num_frame_workers =
      (ctx->frame_parallel_decode == 1) ? ctx->cfg.threads : 1;
  if (ctx->num_frame_workers > MAX_DECODE_THREADS)
    ctx->num_frame_workers = MAX_DECODE_THREADS;
  ctx->available_threads = ctx->num_frame_workers;
  ctx->flushed = 0;

  ctx->buffer_pool = (BufferPool *)aom_calloc(1, sizeof(BufferPool));
  if (ctx->buffer_pool == NULL) return AOM_CODEC_MEM_ERROR;

#if CONFIG_MULTITHREAD
  if (pthread_mutex_init(&ctx->buffer_pool->pool_mutex, NULL)) {
    set_error_detail(ctx, "Failed to allocate buffer pool mutex");
    return AOM_CODEC_MEM_ERROR;
  }
#endif

  ctx->frame_workers = (AVxWorker *)aom_malloc(ctx->num_frame_workers *
                                               sizeof(*ctx->frame_workers));
  if (ctx->frame_workers == NULL) {
    set_error_detail(ctx, "Failed to allocate frame_workers");
    return AOM_CODEC_MEM_ERROR;
  }

  for (i = 0; i < ctx->num_frame_workers; ++i) {
    AVxWorker *const worker = &ctx->frame_workers[i];
    FrameWorkerData *frame_worker_data = NULL;
    winterface->init(worker);
    worker->data1 = aom_memalign(32, sizeof(FrameWorkerData));
    if (worker->data1 == NULL) {
      set_error_detail(ctx, "Failed to allocate frame_worker_data");
      return AOM_CODEC_MEM_ERROR;
    }
    frame_worker_data = (FrameWorkerData *)worker->data1;
    frame_worker_data->pbi = av1_decoder_create(ctx->buffer_pool);
    if (frame_worker_data->pbi == NULL) {
      set_error_detail(ctx, "Failed to allocate frame_worker_data");
      return AOM_CODEC_MEM_ERROR;
    }
    frame_worker_data->pbi->frame_worker_owner = worker;
    frame_worker_data->worker_id = i;
    frame_worker_data->scratch_buffer = NULL;
    frame_worker_data->scratch_buffer_size = 0;
    frame_worker_data->frame_context_ready = 0;
    frame_worker_data->received_frame = 0;
#if CONFIG_MULTITHREAD
    if (pthread_mutex_init(&frame_worker_data->stats_mutex, NULL)) {
      set_error_detail(ctx, "Failed to allocate frame_worker_data mutex");
      return AOM_CODEC_MEM_ERROR;
    }

    if (pthread_cond_init(&frame_worker_data->stats_cond, NULL)) {
      set_error_detail(ctx, "Failed to allocate frame_worker_data cond");
      return AOM_CODEC_MEM_ERROR;
    }
#endif
    frame_worker_data->pbi->allow_lowbitdepth = ctx->cfg.allow_lowbitdepth;

    // If decoding in serial mode, FrameWorker thread could create tile worker
    // thread or loopfilter thread.
    frame_worker_data->pbi->max_threads =
        (ctx->frame_parallel_decode == 0) ? ctx->cfg.threads : 0;

    frame_worker_data->pbi->inv_tile_order = ctx->invert_tile_order;
    frame_worker_data->pbi->common.frame_parallel_decode =
        ctx->frame_parallel_decode;
    worker->hook = (AVxWorkerHook)frame_worker_hook;
    if (!winterface->reset(worker)) {
      set_error_detail(ctx, "Frame Worker thread creation failed");
      return AOM_CODEC_MEM_ERROR;
    }
  }

  // If postprocessing was enabled by the application and a
  // configuration has not been provided, default it.
  if (!ctx->postproc_cfg_set && (ctx->base.init_flags & AOM_CODEC_USE_POSTPROC))
    set_default_ppflags(&ctx->postproc_cfg);

  init_buffer_callbacks(ctx);

  return AOM_CODEC_OK;
}

static INLINE void check_resync(aom_codec_alg_priv_t *const ctx,
                                const AV1Decoder *const pbi) {
  // Clear resync flag if worker got a key frame or intra only frame.
  if (ctx->need_resync == 1 && pbi->need_resync == 0 &&
      (pbi->common.intra_only || pbi->common.frame_type == KEY_FRAME))
    ctx->need_resync = 0;
}

static aom_codec_err_t decode_one(aom_codec_alg_priv_t *ctx,
                                  const uint8_t **data, unsigned int data_sz,
                                  void *user_priv, int64_t deadline) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  (void)deadline;

  // Determine the stream parameters. Note that we rely on peek_si to
  // validate that we have a buffer that does not wrap around the top
  // of the heap.
  if (!ctx->si.h) {
    int is_intra_only = 0;
    const aom_codec_err_t res =
        decoder_peek_si_internal(*data, data_sz, &ctx->si, &is_intra_only,
                                 ctx->decrypt_cb, ctx->decrypt_state);
    if (res != AOM_CODEC_OK) return res;

    if (!ctx->si.is_kf && !is_intra_only) return AOM_CODEC_ERROR;
  }

  if (!ctx->frame_parallel_decode) {
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    frame_worker_data->data = *data;
    frame_worker_data->data_size = data_sz;
    frame_worker_data->user_priv = user_priv;
    frame_worker_data->received_frame = 1;

    // Set these even if already initialized.  The caller may have changed the
    // decrypt config between frames.
    frame_worker_data->pbi->decrypt_cb = ctx->decrypt_cb;
    frame_worker_data->pbi->decrypt_state = ctx->decrypt_state;
#if CONFIG_INSPECTION
    frame_worker_data->pbi->inspect_cb = ctx->inspect_cb;
    frame_worker_data->pbi->inspect_ctx = ctx->inspect_ctx;
#endif

#if CONFIG_EXT_TILE
    frame_worker_data->pbi->dec_tile_row = ctx->decode_tile_row;
    frame_worker_data->pbi->dec_tile_col = ctx->decode_tile_col;
#endif  // CONFIG_EXT_TILE

    worker->had_error = 0;
    winterface->execute(worker);

    // Update data pointer after decode.
    *data = frame_worker_data->data_end;

    if (worker->had_error)
      return update_error_state(ctx, &frame_worker_data->pbi->common.error);

    check_resync(ctx, frame_worker_data->pbi);
  } else {
    AVxWorker *const worker = &ctx->frame_workers[ctx->next_submit_worker_id];
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    // Copy context from last worker thread to next worker thread.
    if (ctx->next_submit_worker_id != ctx->last_submit_worker_id)
      av1_frameworker_copy_context(
          &ctx->frame_workers[ctx->next_submit_worker_id],
          &ctx->frame_workers[ctx->last_submit_worker_id]);

    frame_worker_data->pbi->ready_for_new_data = 0;
    // Copy the compressed data into worker's internal buffer.
    // TODO(hkuang): Will all the workers allocate the same size
    // as the size of the first intra frame be better? This will
    // avoid too many deallocate and allocate.
    if (frame_worker_data->scratch_buffer_size < data_sz) {
      aom_free(frame_worker_data->scratch_buffer);
      frame_worker_data->scratch_buffer = (uint8_t *)aom_malloc(data_sz);
      if (frame_worker_data->scratch_buffer == NULL) {
        set_error_detail(ctx, "Failed to reallocate scratch buffer");
        return AOM_CODEC_MEM_ERROR;
      }
      frame_worker_data->scratch_buffer_size = data_sz;
    }
    frame_worker_data->data_size = data_sz;
    memcpy(frame_worker_data->scratch_buffer, *data, data_sz);

    frame_worker_data->frame_decoded = 0;
    frame_worker_data->frame_context_ready = 0;
    frame_worker_data->received_frame = 1;
    frame_worker_data->data = frame_worker_data->scratch_buffer;
    frame_worker_data->user_priv = user_priv;

    if (ctx->next_submit_worker_id != ctx->last_submit_worker_id)
      ctx->last_submit_worker_id =
          (ctx->last_submit_worker_id + 1) % ctx->num_frame_workers;

    ctx->next_submit_worker_id =
        (ctx->next_submit_worker_id + 1) % ctx->num_frame_workers;
    --ctx->available_threads;
    worker->had_error = 0;
    winterface->launch(worker);
  }

  return AOM_CODEC_OK;
}

static void wait_worker_and_cache_frame(aom_codec_alg_priv_t *ctx) {
  YV12_BUFFER_CONFIG sd;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  AVxWorker *const worker = &ctx->frame_workers[ctx->next_output_worker_id];
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
  ctx->next_output_worker_id =
      (ctx->next_output_worker_id + 1) % ctx->num_frame_workers;
  // TODO(hkuang): Add worker error handling here.
  winterface->sync(worker);
  frame_worker_data->received_frame = 0;
  ++ctx->available_threads;

  check_resync(ctx, frame_worker_data->pbi);

  if (av1_get_raw_frame(frame_worker_data->pbi, &sd) == 0) {
    AV1_COMMON *const cm = &frame_worker_data->pbi->common;
    RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
    ctx->frame_cache[ctx->frame_cache_write].fb_idx = cm->new_fb_idx;
    yuvconfig2image(&ctx->frame_cache[ctx->frame_cache_write].img, &sd,
                    frame_worker_data->user_priv);
    ctx->frame_cache[ctx->frame_cache_write].img.fb_priv =
        frame_bufs[cm->new_fb_idx].raw_frame_buffer.priv;
    ctx->frame_cache_write = (ctx->frame_cache_write + 1) % FRAME_CACHE_SIZE;
    ++ctx->num_cache_frames;
  }
}

static aom_codec_err_t decoder_decode(aom_codec_alg_priv_t *ctx,
                                      const uint8_t *data, unsigned int data_sz,
                                      void *user_priv, long deadline) {
  const uint8_t *data_start = data;
  const uint8_t *const data_end = data + data_sz;
  aom_codec_err_t res;
  uint32_t frame_sizes[8];
  int frame_count;

  if (data == NULL && data_sz == 0) {
    ctx->flushed = 1;
    return AOM_CODEC_OK;
  }

  // Reset flushed when receiving a valid frame.
  ctx->flushed = 0;

  // Initialize the decoder workers on the first frame.
  if (ctx->frame_workers == NULL) {
    res = init_decoder(ctx);
    if (res != AOM_CODEC_OK) return res;
  }

  int index_size = 0;
  res = av1_parse_superframe_index(data, data_sz, frame_sizes, &frame_count,
                                   &index_size, ctx->decrypt_cb,
                                   ctx->decrypt_state);
  if (res != AOM_CODEC_OK) return res;

  data_start += index_size;

  if (ctx->frame_parallel_decode) {
    // Decode in frame parallel mode. When decoding in this mode, the frame
    // passed to the decoder must be either a normal frame or a superframe with
    // superframe index so the decoder could get each frame's start position
    // in the superframe.
    if (frame_count > 0) {
      int i;

      for (i = 0; i < frame_count; ++i) {
        const uint8_t *data_start_copy = data_start;
        const uint32_t frame_size = frame_sizes[i];
        if (data_start < data ||
            frame_size > (uint32_t)(data_end - data_start)) {
          set_error_detail(ctx, "Invalid frame size in index");
          return AOM_CODEC_CORRUPT_FRAME;
        }

        if (ctx->available_threads == 0) {
          // No more threads for decoding. Wait until the next output worker
          // finishes decoding. Then copy the decoded frame into cache.
          if (ctx->num_cache_frames < FRAME_CACHE_SIZE) {
            wait_worker_and_cache_frame(ctx);
          } else {
            // TODO(hkuang): Add unit test to test this path.
            set_error_detail(ctx, "Frame output cache is full.");
            return AOM_CODEC_ERROR;
          }
        }

        res =
            decode_one(ctx, &data_start_copy, frame_size, user_priv, deadline);
        if (res != AOM_CODEC_OK) return res;
        data_start += frame_size;
      }
    } else {
      if (ctx->available_threads == 0) {
        // No more threads for decoding. Wait until the next output worker
        // finishes decoding. Then copy the decoded frame into cache.
        if (ctx->num_cache_frames < FRAME_CACHE_SIZE) {
          wait_worker_and_cache_frame(ctx);
        } else {
          // TODO(hkuang): Add unit test to test this path.
          set_error_detail(ctx, "Frame output cache is full.");
          return AOM_CODEC_ERROR;
        }
      }

      res = decode_one(ctx, &data, data_sz, user_priv, deadline);
      if (res != AOM_CODEC_OK) return res;
    }
  } else {
    // Decode in serial mode.
    if (frame_count > 0) {
      int i;

      for (i = 0; i < frame_count; ++i) {
        const uint8_t *data_start_copy = data_start;
        const uint32_t frame_size = frame_sizes[i];
        if (data_start < data ||
            frame_size > (uint32_t)(data_end - data_start)) {
          set_error_detail(ctx, "Invalid frame size in index");
          return AOM_CODEC_CORRUPT_FRAME;
        }

        res =
            decode_one(ctx, &data_start_copy, frame_size, user_priv, deadline);
        if (res != AOM_CODEC_OK) return res;

        data_start += frame_size;
      }
    } else {
      while (data_start < data_end) {
        const uint32_t frame_size = (uint32_t)(data_end - data_start);
        res = decode_one(ctx, &data_start, frame_size, user_priv, deadline);
        if (res != AOM_CODEC_OK) return res;

        // Account for suboptimal termination by the encoder.
        while (data_start < data_end) {
          const uint8_t marker =
              read_marker(ctx->decrypt_cb, ctx->decrypt_state, data_start);
          if (marker) break;
          ++data_start;
        }
      }
    }
  }

  return res;
}

static void release_last_output_frame(aom_codec_alg_priv_t *ctx) {
  RefCntBuffer *const frame_bufs = ctx->buffer_pool->frame_bufs;
  // Decrease reference count of last output frame in frame parallel mode.
  if (ctx->frame_parallel_decode && ctx->last_show_frame >= 0) {
    BufferPool *const pool = ctx->buffer_pool;
    lock_buffer_pool(pool);
    decrease_ref_count(ctx->last_show_frame, frame_bufs, pool);
    unlock_buffer_pool(pool);
  }
}

static aom_image_t *decoder_get_frame(aom_codec_alg_priv_t *ctx,
                                      aom_codec_iter_t *iter) {
  aom_image_t *img = NULL;

  // Only return frame when all the cpu are busy or
  // application fluhsed the decoder in frame parallel decode.
  if (ctx->frame_parallel_decode && ctx->available_threads > 0 &&
      !ctx->flushed) {
    return NULL;
  }

  // Output the frames in the cache first.
  if (ctx->num_cache_frames > 0) {
    release_last_output_frame(ctx);
    ctx->last_show_frame = ctx->frame_cache[ctx->frame_cache_read].fb_idx;
    if (ctx->need_resync) return NULL;
    img = &ctx->frame_cache[ctx->frame_cache_read].img;
    ctx->frame_cache_read = (ctx->frame_cache_read + 1) % FRAME_CACHE_SIZE;
    --ctx->num_cache_frames;
    return img;
  }

  // iter acts as a flip flop, so an image is only returned on the first
  // call to get_frame.
  if (*iter == NULL && ctx->frame_workers != NULL) {
    do {
      YV12_BUFFER_CONFIG sd;
      const AVxWorkerInterface *const winterface = aom_get_worker_interface();
      AVxWorker *const worker = &ctx->frame_workers[ctx->next_output_worker_id];
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      ctx->next_output_worker_id =
          (ctx->next_output_worker_id + 1) % ctx->num_frame_workers;
      // Wait for the frame from worker thread.
      if (winterface->sync(worker)) {
        // Check if worker has received any frames.
        if (frame_worker_data->received_frame == 1) {
          ++ctx->available_threads;
          frame_worker_data->received_frame = 0;
          check_resync(ctx, frame_worker_data->pbi);
        }
        if (av1_get_raw_frame(frame_worker_data->pbi, &sd) == 0) {
          AV1_COMMON *const cm = &frame_worker_data->pbi->common;
          RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
          release_last_output_frame(ctx);
          ctx->last_show_frame = frame_worker_data->pbi->common.new_fb_idx;
          if (ctx->need_resync) return NULL;
          yuvconfig2image(&ctx->img, &sd, frame_worker_data->user_priv);

#if CONFIG_EXT_TILE
          if (cm->single_tile_decoding &&
              frame_worker_data->pbi->dec_tile_row >= 0) {
            const int tile_row =
                AOMMIN(frame_worker_data->pbi->dec_tile_row, cm->tile_rows - 1);
            const int mi_row = tile_row * cm->tile_height;
            const int ssy = ctx->img.y_chroma_shift;
            int plane;
            ctx->img.planes[0] += mi_row * MI_SIZE * ctx->img.stride[0];
            for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
              ctx->img.planes[plane] +=
                  mi_row * (MI_SIZE >> ssy) * ctx->img.stride[plane];
            }
            ctx->img.d_h =
                AOMMIN(cm->tile_height, cm->mi_rows - mi_row) * MI_SIZE;
          }

          if (cm->single_tile_decoding &&
              frame_worker_data->pbi->dec_tile_col >= 0) {
            const int tile_col =
                AOMMIN(frame_worker_data->pbi->dec_tile_col, cm->tile_cols - 1);
            const int mi_col = tile_col * cm->tile_width;
            const int ssx = ctx->img.x_chroma_shift;
            int plane;
            ctx->img.planes[0] += mi_col * MI_SIZE;
            for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
              ctx->img.planes[plane] += mi_col * (MI_SIZE >> ssx);
            }
            ctx->img.d_w =
                AOMMIN(cm->tile_width, cm->mi_cols - mi_col) * MI_SIZE;
          }
#endif  // CONFIG_EXT_TILE

          ctx->img.fb_priv = frame_bufs[cm->new_fb_idx].raw_frame_buffer.priv;
          img = &ctx->img;
          return img;
        }
      } else {
        // Decoding failed. Release the worker thread.
        frame_worker_data->received_frame = 0;
        ++ctx->available_threads;
        ctx->need_resync = 1;
        if (ctx->flushed != 1) return NULL;
      }
    } while (ctx->next_output_worker_id != ctx->next_submit_worker_id);
  }
  return NULL;
}

static aom_codec_err_t decoder_set_fb_fn(
    aom_codec_alg_priv_t *ctx, aom_get_frame_buffer_cb_fn_t cb_get,
    aom_release_frame_buffer_cb_fn_t cb_release, void *cb_priv) {
  if (cb_get == NULL || cb_release == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  } else if (ctx->frame_workers == NULL) {
    // If the decoder has already been initialized, do not accept changes to
    // the frame buffer functions.
    ctx->get_ext_fb_cb = cb_get;
    ctx->release_ext_fb_cb = cb_release;
    ctx->ext_priv = cb_priv;
    return AOM_CODEC_OK;
  }

  return AOM_CODEC_ERROR;
}

static aom_codec_err_t ctrl_set_reference(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  av1_ref_frame_t *const data = va_arg(args, av1_ref_frame_t *);

  // Only support this function in serial decode.
  if (ctx->frame_parallel_decode) {
    set_error_detail(ctx, "Not supported in frame parallel decode");
    return AOM_CODEC_INCAPABLE;
  }

  if (data) {
    av1_ref_frame_t *const frame = data;
    YV12_BUFFER_CONFIG sd;
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    image2yuvconfig(&frame->img, &sd);
    return av1_set_reference_dec(&frame_worker_data->pbi->common, frame->idx,
                                 &sd);
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_copy_reference(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  const av1_ref_frame_t *const frame = va_arg(args, av1_ref_frame_t *);

  // Only support this function in serial decode.
  if (ctx->frame_parallel_decode) {
    set_error_detail(ctx, "Not supported in frame parallel decode");
    return AOM_CODEC_INCAPABLE;
  }

  if (frame) {
    YV12_BUFFER_CONFIG sd;
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    image2yuvconfig(&frame->img, &sd);
    return av1_copy_reference_dec(frame_worker_data->pbi, frame->idx, &sd);
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_get_reference(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  av1_ref_frame_t *data = va_arg(args, av1_ref_frame_t *);

  // Only support this function in serial decode.
  if (ctx->frame_parallel_decode) {
    set_error_detail(ctx, "Not supported in frame parallel decode");
    return AOM_CODEC_INCAPABLE;
  }

  if (data) {
    YV12_BUFFER_CONFIG *fb;
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    fb = get_ref_frame(&frame_worker_data->pbi->common, data->idx);
    if (fb == NULL) return AOM_CODEC_ERROR;
    yuvconfig2image(&data->img, fb, NULL);
    return AOM_CODEC_OK;
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_get_new_frame_image(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  aom_image_t *new_img = va_arg(args, aom_image_t *);

  // Only support this function in serial decode.
  if (ctx->frame_parallel_decode) {
    set_error_detail(ctx, "Not supported in frame parallel decode");
    return AOM_CODEC_INCAPABLE;
  }

  if (new_img) {
    YV12_BUFFER_CONFIG new_frame;
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;

    if (av1_get_frame_to_show(frame_worker_data->pbi, &new_frame) == 0) {
      yuvconfig2image(new_img, &new_frame, NULL);
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_set_postproc(aom_codec_alg_priv_t *ctx,
                                         va_list args) {
  (void)ctx;
  (void)args;
  return AOM_CODEC_INCAPABLE;
}

static aom_codec_err_t ctrl_set_dbg_options(aom_codec_alg_priv_t *ctx,
                                            va_list args) {
  (void)ctx;
  (void)args;
  return AOM_CODEC_INCAPABLE;
}

static aom_codec_err_t ctrl_get_last_ref_updates(aom_codec_alg_priv_t *ctx,
                                                 va_list args) {
  int *const update_info = va_arg(args, int *);

  // Only support this function in serial decode.
  if (ctx->frame_parallel_decode) {
    set_error_detail(ctx, "Not supported in frame parallel decode");
    return AOM_CODEC_INCAPABLE;
  }

  if (update_info) {
    if (ctx->frame_workers) {
      AVxWorker *const worker = ctx->frame_workers;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      *update_info = frame_worker_data->pbi->refresh_frame_flags;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_last_quantizer(aom_codec_alg_priv_t *ctx,
                                               va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return AOM_CODEC_INVALID_PARAM;
  *arg =
      ((FrameWorkerData *)ctx->frame_workers[0].data1)->pbi->common.base_qindex;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_frame_corrupted(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  int *corrupted = va_arg(args, int *);

  if (corrupted) {
    if (ctx->frame_workers) {
      AVxWorker *const worker = ctx->frame_workers;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      RefCntBuffer *const frame_bufs =
          frame_worker_data->pbi->common.buffer_pool->frame_bufs;
      if (frame_worker_data->pbi->common.frame_to_show == NULL)
        return AOM_CODEC_ERROR;
      if (ctx->last_show_frame >= 0)
        *corrupted = frame_bufs[ctx->last_show_frame].buf.corrupted;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_frame_size(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  int *const frame_size = va_arg(args, int *);

  // Only support this function in serial decode.
  if (ctx->frame_parallel_decode) {
    set_error_detail(ctx, "Not supported in frame parallel decode");
    return AOM_CODEC_INCAPABLE;
  }

  if (frame_size) {
    if (ctx->frame_workers) {
      AVxWorker *const worker = ctx->frame_workers;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1_COMMON *const cm = &frame_worker_data->pbi->common;
      frame_size[0] = cm->width;
      frame_size[1] = cm->height;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_render_size(aom_codec_alg_priv_t *ctx,
                                            va_list args) {
  int *const render_size = va_arg(args, int *);

  // Only support this function in serial decode.
  if (ctx->frame_parallel_decode) {
    set_error_detail(ctx, "Not supported in frame parallel decode");
    return AOM_CODEC_INCAPABLE;
  }

  if (render_size) {
    if (ctx->frame_workers) {
      AVxWorker *const worker = ctx->frame_workers;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1_COMMON *const cm = &frame_worker_data->pbi->common;
      render_size[0] = cm->render_width;
      render_size[1] = cm->render_height;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_bit_depth(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  unsigned int *const bit_depth = va_arg(args, unsigned int *);
  AVxWorker *const worker = &ctx->frame_workers[ctx->next_output_worker_id];

  if (bit_depth) {
    if (worker) {
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1_COMMON *const cm = &frame_worker_data->pbi->common;
      *bit_depth = cm->bit_depth;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_set_invert_tile_order(aom_codec_alg_priv_t *ctx,
                                                  va_list args) {
  ctx->invert_tile_order = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_decryptor(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  aom_decrypt_init *init = va_arg(args, aom_decrypt_init *);
  ctx->decrypt_cb = init ? init->decrypt_cb : NULL;
  ctx->decrypt_state = init ? init->decrypt_state : NULL;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_byte_alignment(aom_codec_alg_priv_t *ctx,
                                               va_list args) {
  const int legacy_byte_alignment = 0;
  const int min_byte_alignment = 32;
  const int max_byte_alignment = 1024;
  const int byte_alignment = va_arg(args, int);

  if (byte_alignment != legacy_byte_alignment &&
      (byte_alignment < min_byte_alignment ||
       byte_alignment > max_byte_alignment ||
       (byte_alignment & (byte_alignment - 1)) != 0))
    return AOM_CODEC_INVALID_PARAM;

  ctx->byte_alignment = byte_alignment;
  if (ctx->frame_workers) {
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    frame_worker_data->pbi->common.byte_alignment = byte_alignment;
  }
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_skip_loop_filter(aom_codec_alg_priv_t *ctx,
                                                 va_list args) {
  ctx->skip_loop_filter = va_arg(args, int);

  if (ctx->frame_workers) {
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    frame_worker_data->pbi->common.skip_loop_filter = ctx->skip_loop_filter;
  }

  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_get_accounting(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
#if !CONFIG_ACCOUNTING
  (void)ctx;
  (void)args;
  return AOM_CODEC_INCAPABLE;
#else
  if (ctx->frame_workers) {
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    AV1Decoder *pbi = frame_worker_data->pbi;
    Accounting **acct = va_arg(args, Accounting **);
    *acct = &pbi->accounting;
    return AOM_CODEC_OK;
  }
  return AOM_CODEC_ERROR;
#endif
}
static aom_codec_err_t ctrl_set_decode_tile_row(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  ctx->decode_tile_row = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_decode_tile_col(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  ctx->decode_tile_col = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_inspection_callback(aom_codec_alg_priv_t *ctx,
                                                    va_list args) {
#if !CONFIG_INSPECTION
  (void)ctx;
  (void)args;
  return AOM_CODEC_INCAPABLE;
#else
  aom_inspect_init *init = va_arg(args, aom_inspect_init *);
  ctx->inspect_cb = init->inspect_cb;
  ctx->inspect_ctx = init->inspect_ctx;
  return AOM_CODEC_OK;
#endif
}

static aom_codec_ctrl_fn_map_t decoder_ctrl_maps[] = {
  { AV1_COPY_REFERENCE, ctrl_copy_reference },

  // Setters
  { AV1_SET_REFERENCE, ctrl_set_reference },
  { AOM_SET_POSTPROC, ctrl_set_postproc },
  { AOM_SET_DBG_COLOR_REF_FRAME, ctrl_set_dbg_options },
  { AOM_SET_DBG_COLOR_MB_MODES, ctrl_set_dbg_options },
  { AOM_SET_DBG_COLOR_B_MODES, ctrl_set_dbg_options },
  { AOM_SET_DBG_DISPLAY_MV, ctrl_set_dbg_options },
  { AV1_INVERT_TILE_DECODE_ORDER, ctrl_set_invert_tile_order },
  { AOMD_SET_DECRYPTOR, ctrl_set_decryptor },
  { AV1_SET_BYTE_ALIGNMENT, ctrl_set_byte_alignment },
  { AV1_SET_SKIP_LOOP_FILTER, ctrl_set_skip_loop_filter },
  { AV1_SET_DECODE_TILE_ROW, ctrl_set_decode_tile_row },
  { AV1_SET_DECODE_TILE_COL, ctrl_set_decode_tile_col },
  { AV1_SET_INSPECTION_CALLBACK, ctrl_set_inspection_callback },

  // Getters
  { AOMD_GET_FRAME_CORRUPTED, ctrl_get_frame_corrupted },
  { AOMD_GET_LAST_QUANTIZER, ctrl_get_last_quantizer },
  { AOMD_GET_LAST_REF_UPDATES, ctrl_get_last_ref_updates },
  { AV1D_GET_BIT_DEPTH, ctrl_get_bit_depth },
  { AV1D_GET_DISPLAY_SIZE, ctrl_get_render_size },
  { AV1D_GET_FRAME_SIZE, ctrl_get_frame_size },
  { AV1_GET_ACCOUNTING, ctrl_get_accounting },
  { AV1_GET_NEW_FRAME_IMAGE, ctrl_get_new_frame_image },
  { AV1_GET_REFERENCE, ctrl_get_reference },

  { -1, NULL },
};

#ifndef VERSION_STRING
#define VERSION_STRING
#endif
CODEC_INTERFACE(aom_codec_av1_dx) = {
  "AOMedia Project AV1 Decoder" VERSION_STRING,
  AOM_CODEC_INTERNAL_ABI_VERSION,
  AOM_CODEC_CAP_DECODER |
      AOM_CODEC_CAP_EXTERNAL_FRAME_BUFFER,  // aom_codec_caps_t
  decoder_init,                             // aom_codec_init_fn_t
  decoder_destroy,                          // aom_codec_destroy_fn_t
  decoder_ctrl_maps,                        // aom_codec_ctrl_fn_map_t
  {
      // NOLINT
      decoder_peek_si,    // aom_codec_peek_si_fn_t
      decoder_get_si,     // aom_codec_get_si_fn_t
      decoder_decode,     // aom_codec_decode_fn_t
      decoder_get_frame,  // aom_codec_frame_get_fn_t
      decoder_set_fb_fn,  // aom_codec_set_fb_fn_t
  },
  {
      // NOLINT
      0,
      NULL,  // aom_codec_enc_cfg_map_t
      NULL,  // aom_codec_encode_fn_t
      NULL,  // aom_codec_get_cx_data_fn_t
      NULL,  // aom_codec_enc_config_set_fn_t
      NULL,  // aom_codec_get_global_headers_fn_t
      NULL,  // aom_codec_get_preview_frame_fn_t
      NULL   // aom_codec_enc_mr_get_mem_loc_fn_t
  }
};
