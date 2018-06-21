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

#include "config/aom_config.h"
#include "config/aom_version.h"

#include "aom/internal/aom_codec_internal.h"
#include "aom/aomdx.h"
#include "aom/aom_decoder.h"
#include "aom_dsp/bitreader_buffer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem_ops.h"
#include "aom_util/aom_thread.h"

#include "av1/common/alloccommon.h"
#include "av1/common/frame_buffers.h"
#include "av1/common/enums.h"

#include "av1/decoder/decoder.h"
#include "av1/decoder/decodeframe.h"
#include "av1/decoder/obu.h"

#include "av1/av1_iface_common.h"

struct aom_codec_alg_priv {
  aom_codec_priv_t base;
  aom_codec_dec_cfg_t cfg;
  aom_codec_stream_info_t si;
  int postproc_cfg_set;
  aom_postproc_cfg_t postproc_cfg;
  aom_image_t img;
  int img_avail;
  int flushed;
  int invert_tile_order;
  int last_show_frame;  // Index of last output frame.
  int byte_alignment;
  int skip_loop_filter;
  int decode_tile_row;
  int decode_tile_col;
  unsigned int tile_mode;
  unsigned int ext_tile_debug;
  EXTERNAL_REFERENCES ext_refs;
  unsigned int is_annexb;
  int operating_point;
  int output_all_layers;

  AVxWorker *frame_workers;
  int num_frame_workers;
  int next_submit_worker_id;
  int last_submit_worker_id;
  int next_output_worker_id;
  int available_threads;
  aom_image_t *image_with_grain;
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

    // TODO(tdaede): this should not be exposed to the API
    priv->cfg.allow_lowbitdepth = CONFIG_LOWBITDEPTH;
    if (ctx->config.dec) {
      priv->cfg = *ctx->config.dec;
      ctx->config.dec = &priv->cfg;
      // default values
      priv->cfg.cfg.ext_partition = 1;
    }
    priv->image_with_grain = NULL;
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
      aom_free(frame_worker_data->pbi->common.tpl_mvs);
      frame_worker_data->pbi->common.tpl_mvs = NULL;
      av1_remove_common(&frame_worker_data->pbi->common);
      av1_free_restoration_buffers(&frame_worker_data->pbi->common);
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
  if (ctx->image_with_grain) aom_img_free(ctx->image_with_grain);
  aom_free(ctx);
  return AOM_CODEC_OK;
}

// Parses the operating points (including operating_point_idc, seq_level_idx,
// and seq_tier) and then sets si->number_spatial_layers and
// si->number_temporal_layers based on operating_point_idc[0].
static aom_codec_err_t parse_operating_points(struct aom_read_bit_buffer *rb,
                                              int is_reduced_header,
                                              aom_codec_stream_info_t *si) {
  int operating_point_idc0 = 0;

  if (is_reduced_header) {
    aom_rb_read_literal(rb, LEVEL_BITS);  // level
  } else {
    const uint8_t operating_points_cnt_minus_1 =
        aom_rb_read_literal(rb, OP_POINTS_CNT_MINUS_1_BITS);
    for (int i = 0; i < operating_points_cnt_minus_1 + 1; i++) {
      int operating_point_idc;
      operating_point_idc = aom_rb_read_literal(rb, OP_POINTS_IDC_BITS);
      if (i == 0) operating_point_idc0 = operating_point_idc;
      int seq_level_idx = aom_rb_read_literal(rb, LEVEL_BITS);  // level
      if (seq_level_idx > 7) aom_rb_read_bit(rb);               // tier
    }
  }

  if (aom_get_num_layers_from_operating_point_idc(
          operating_point_idc0, &si->number_spatial_layers,
          &si->number_temporal_layers) != AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }

  return AOM_CODEC_OK;
}

static aom_codec_err_t decoder_peek_si_internal(const uint8_t *data,
                                                size_t data_sz,
                                                aom_codec_stream_info_t *si,
                                                int *is_intra_only) {
  int intra_only_flag = 0;
  int got_sequence_header = 0;
  int found_keyframe = 0;

  if (data + data_sz <= data || data_sz < 1) return AOM_CODEC_INVALID_PARAM;

  si->w = 0;
  si->h = 0;
  si->is_kf = 0;  // is_kf indicates whether the current packet contains a RAP

  ObuHeader obu_header;
  memset(&obu_header, 0, sizeof(obu_header));
  size_t payload_size = 0;
  size_t bytes_read = 0;
  int reduced_still_picture_hdr = 0;
  aom_codec_err_t status = aom_read_obu_header_and_size(
      data, data_sz, si->is_annexb, &obu_header, &payload_size, &bytes_read);
  if (status != AOM_CODEC_OK) return status;

  // If the first OBU is a temporal delimiter, skip over it and look at the next
  // OBU in the bitstream
  if (obu_header.type == OBU_TEMPORAL_DELIMITER) {
    // Skip any associated payload (there shouldn't be one, but just in case)
    if (data_sz < bytes_read + payload_size) return AOM_CODEC_CORRUPT_FRAME;
    data += bytes_read + payload_size;
    data_sz -= bytes_read + payload_size;

    status = aom_read_obu_header_and_size(
        data, data_sz, si->is_annexb, &obu_header, &payload_size, &bytes_read);
    if (status != AOM_CODEC_OK) return status;
  }
  while (1) {
    data += bytes_read;
    data_sz -= bytes_read;
    const uint8_t *payload_start = data;
    // Check that the selected OBU is a sequence header
    if (obu_header.type == OBU_SEQUENCE_HEADER) {
      // Sanity check on sequence header size
      if (data_sz < 2) return AOM_CODEC_CORRUPT_FRAME;
      // Read a few values from the sequence header payload
      struct aom_read_bit_buffer rb = { data, data + data_sz, 0, NULL, NULL };

      av1_read_profile(&rb);  // profile
      const int still_picture = aom_rb_read_bit(&rb);
      reduced_still_picture_hdr = aom_rb_read_bit(&rb);

      if (!still_picture && reduced_still_picture_hdr) {
        return AOM_CODEC_UNSUP_BITSTREAM;
      }

      if (parse_operating_points(&rb, reduced_still_picture_hdr, si) !=
          AOM_CODEC_OK) {
        return AOM_CODEC_ERROR;
      }

      int num_bits_width = aom_rb_read_literal(&rb, 4) + 1;
      int num_bits_height = aom_rb_read_literal(&rb, 4) + 1;
      int max_frame_width = aom_rb_read_literal(&rb, num_bits_width) + 1;
      int max_frame_height = aom_rb_read_literal(&rb, num_bits_height) + 1;
      si->w = max_frame_width;
      si->h = max_frame_height;
      got_sequence_header = 1;
    } else if (obu_header.type == OBU_FRAME_HEADER ||
               obu_header.type == OBU_FRAME) {
      if (got_sequence_header && reduced_still_picture_hdr) {
        found_keyframe = 1;
        break;
      } else {
        // make sure we have enough bits to get the frame type out
        if (data_sz < 1) return AOM_CODEC_CORRUPT_FRAME;
        struct aom_read_bit_buffer rb = { data, data + data_sz, 0, NULL, NULL };
        const int show_existing_frame = aom_rb_read_bit(&rb);
        if (!show_existing_frame) {
          const FRAME_TYPE frame_type = (FRAME_TYPE)aom_rb_read_literal(&rb, 2);
          if (frame_type == KEY_FRAME) {
            found_keyframe = 1;
            break;  // Stop here as no further OBUs will change the outcome.
          }
        }
      }
    }
    // skip past any unread OBU header data
    data = payload_start + payload_size;
    data_sz -= payload_size;
    if (data_sz <= 0) break;  // exit if we're out of OBUs
    status = aom_read_obu_header_and_size(
        data, data_sz, si->is_annexb, &obu_header, &payload_size, &bytes_read);
    if (status != AOM_CODEC_OK) return status;
  }
  if (got_sequence_header && found_keyframe) si->is_kf = 1;
  if (is_intra_only != NULL) *is_intra_only = intra_only_flag;
  return AOM_CODEC_OK;
}

static aom_codec_err_t decoder_peek_si(const uint8_t *data, size_t data_sz,
                                       aom_codec_stream_info_t *si) {
  return decoder_peek_si_internal(data, data_sz, si, NULL);
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

  if (frame_worker_data->result != 0) {
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
  ctx->need_resync = 1;
  ctx->num_frame_workers = 1;
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
    frame_worker_data->pbi->common.options = &ctx->cfg.cfg;
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
    frame_worker_data->pbi->max_threads = ctx->cfg.threads;
    frame_worker_data->pbi->inv_tile_order = ctx->invert_tile_order;
    frame_worker_data->pbi->common.large_scale_tile = ctx->tile_mode;
    frame_worker_data->pbi->common.is_annexb = ctx->is_annexb;
    frame_worker_data->pbi->dec_tile_row = ctx->decode_tile_row;
    frame_worker_data->pbi->dec_tile_col = ctx->decode_tile_col;
    frame_worker_data->pbi->operating_point = ctx->operating_point;
    frame_worker_data->pbi->output_all_layers = ctx->output_all_layers;
    frame_worker_data->pbi->ext_tile_debug = ctx->ext_tile_debug;

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
                                  const uint8_t **data, size_t data_sz,
                                  void *user_priv) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  // Determine the stream parameters. Note that we rely on peek_si to
  // validate that we have a buffer that does not wrap around the top
  // of the heap.
  if (!ctx->si.h) {
    int is_intra_only = 0;
    ctx->si.is_annexb = ctx->is_annexb;
    const aom_codec_err_t res =
        decoder_peek_si_internal(*data, data_sz, &ctx->si, &is_intra_only);
    if (res != AOM_CODEC_OK) return res;

    if (!ctx->si.is_kf && !is_intra_only) return AOM_CODEC_ERROR;
  }

  AVxWorker *const worker = ctx->frame_workers;
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
  frame_worker_data->data = *data;
  frame_worker_data->data_size = data_sz;
  frame_worker_data->user_priv = user_priv;
  frame_worker_data->received_frame = 1;

#if CONFIG_INSPECTION
  frame_worker_data->pbi->inspect_cb = ctx->inspect_cb;
  frame_worker_data->pbi->inspect_ctx = ctx->inspect_ctx;
#endif

  frame_worker_data->pbi->common.large_scale_tile = ctx->tile_mode;
  frame_worker_data->pbi->dec_tile_row = ctx->decode_tile_row;
  frame_worker_data->pbi->dec_tile_col = ctx->decode_tile_col;
  frame_worker_data->pbi->ext_tile_debug = ctx->ext_tile_debug;
  frame_worker_data->pbi->ext_refs = ctx->ext_refs;

  frame_worker_data->pbi->common.is_annexb = ctx->is_annexb;

  worker->had_error = 0;
  winterface->execute(worker);

  // Update data pointer after decode.
  *data = frame_worker_data->data_end;

  if (worker->had_error)
    return update_error_state(ctx, &frame_worker_data->pbi->common.error);

  check_resync(ctx, frame_worker_data->pbi);

  return AOM_CODEC_OK;
}

static aom_codec_err_t decoder_decode(aom_codec_alg_priv_t *ctx,
                                      const uint8_t *data, size_t data_sz,
                                      void *user_priv) {
  const uint8_t *data_start = data;
  const uint8_t *data_end = data + data_sz;
  aom_codec_err_t res = AOM_CODEC_OK;

  // Release any pending output frames from the previous decoder call.
  // We need to do this even if the decoder is being flushed
  if (ctx->frame_workers) {
    BufferPool *const pool = ctx->buffer_pool;
    RefCntBuffer *const frame_bufs = pool->frame_bufs;
    lock_buffer_pool(pool);
    for (int i = 0; i < ctx->num_frame_workers; ++i) {
      AVxWorker *const worker = &ctx->frame_workers[i];
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      struct AV1Decoder *pbi = frame_worker_data->pbi;
      for (size_t j = 0; j < pbi->num_output_frames; j++) {
        decrease_ref_count((int)pbi->output_frame_index[j], frame_bufs, pool);
      }
      pbi->num_output_frames = 0;
    }
    unlock_buffer_pool(ctx->buffer_pool);
  }

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

  if (ctx->is_annexb) {
    // read the size of this temporal unit
    size_t length_of_size;
    uint64_t temporal_unit_size;
    if (aom_uleb_decode(data_start, data_sz, &temporal_unit_size,
                        &length_of_size) != 0) {
      return AOM_CODEC_CORRUPT_FRAME;
    }
    data_start += length_of_size;
    if (temporal_unit_size > (size_t)(data_end - data_start))
      return AOM_CODEC_CORRUPT_FRAME;
    data_end = data_start + temporal_unit_size;
  }

  // Decode in serial mode.
  while (data_start < data_end) {
    uint64_t frame_size;
    if (ctx->is_annexb) {
      // read the size of this frame unit
      size_t length_of_size;
      if (aom_uleb_decode(data_start, (size_t)(data_end - data_start),
                          &frame_size, &length_of_size) != 0) {
        return AOM_CODEC_CORRUPT_FRAME;
      }
      data_start += length_of_size;
      if (frame_size > (size_t)(data_end - data_start))
        return AOM_CODEC_CORRUPT_FRAME;
    } else {
      frame_size = (uint64_t)(data_end - data_start);
    }

    res = decode_one(ctx, &data_start, (size_t)frame_size, user_priv);
    if (res != AOM_CODEC_OK) return res;

    // Allow extra zero bytes after the frame end
    while (data_start < data_end) {
      const uint8_t marker = data_start[0];
      if (marker) break;
      ++data_start;
    }
  }

  return res;
}

aom_image_t *add_grain_if_needed(aom_image_t *img, aom_image_t *grain_img_buf,
                                 aom_film_grain_t *grain_params) {
  if (!grain_params->apply_grain) return img;

  if (grain_img_buf &&
      (img->d_w != grain_img_buf->d_w || img->d_h != grain_img_buf->d_h ||
       img->fmt != grain_img_buf->fmt || !(img->d_h % 2) || !(img->d_w % 2))) {
    aom_img_free(grain_img_buf);
    grain_img_buf = NULL;
  }
  if (!grain_img_buf) {
    int w_even = img->d_w % 2 ? img->d_w + 1 : img->d_w;
    int h_even = img->d_h % 2 ? img->d_h + 1 : img->d_h;
    grain_img_buf = aom_img_alloc(NULL, img->fmt, w_even, h_even, 16);
    grain_img_buf->bit_depth = img->bit_depth;
  }

  av1_add_film_grain(grain_params, img, grain_img_buf);

  return grain_img_buf;
}

static aom_image_t *decoder_get_frame(aom_codec_alg_priv_t *ctx,
                                      aom_codec_iter_t *iter) {
  aom_image_t *img = NULL;

  if (!iter) {
    return NULL;
  }

  // To avoid having to allocate any extra storage, treat 'iter' as
  // simply a pointer to an integer index
  uintptr_t *index = (uintptr_t *)iter;

  if (ctx->frame_workers != NULL) {
    do {
      YV12_BUFFER_CONFIG *sd;
      // NOTE(david.barker): This code does not support multiple worker threads
      // yet. We should probably move the iteration over threads into *iter
      // instead of using ctx->next_output_worker_id.
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
        aom_film_grain_t *grain_params;
        if (av1_get_raw_frame(frame_worker_data->pbi, *index, &sd,
                              &grain_params) == 0) {
          *index += 1;  // Advance the iterator to point to the next image

          AV1Decoder *const pbi = frame_worker_data->pbi;
          AV1_COMMON *const cm = &pbi->common;
          RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
          ctx->last_show_frame = cm->new_fb_idx;
          if (ctx->need_resync) return NULL;
          yuvconfig2image(&ctx->img, sd, frame_worker_data->user_priv);

          if (!pbi->ext_tile_debug && cm->large_scale_tile) {
            img = &ctx->img;
            img->img_data = pbi->tile_list_output;
            img->sz = pbi->tile_list_size;
            return img;
          }

          const int num_planes = av1_num_planes(cm);
          if (pbi->ext_tile_debug && cm->single_tile_decoding &&
              pbi->dec_tile_row >= 0) {
            const int tile_row = AOMMIN(pbi->dec_tile_row, cm->tile_rows - 1);
            const int mi_row = tile_row * cm->tile_height;
            const int ssy = ctx->img.y_chroma_shift;
            int plane;
            ctx->img.planes[0] += mi_row * MI_SIZE * ctx->img.stride[0];
            if (num_planes > 1) {
              for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
                ctx->img.planes[plane] +=
                    mi_row * (MI_SIZE >> ssy) * ctx->img.stride[plane];
              }
            }
            ctx->img.d_h =
                AOMMIN(cm->tile_height, cm->mi_rows - mi_row) * MI_SIZE;
          }

          if (pbi->ext_tile_debug && cm->single_tile_decoding &&
              pbi->dec_tile_col >= 0) {
            const int tile_col = AOMMIN(pbi->dec_tile_col, cm->tile_cols - 1);
            const int mi_col = tile_col * cm->tile_width;
            const int ssx = ctx->img.x_chroma_shift;
            int plane;
            ctx->img.planes[0] += mi_col * MI_SIZE;
            if (num_planes > 1) {
              for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
                ctx->img.planes[plane] += mi_col * (MI_SIZE >> ssx);
              }
            }
            ctx->img.d_w =
                AOMMIN(cm->tile_width, cm->mi_cols - mi_col) * MI_SIZE;
          }

          ctx->img.fb_priv = frame_bufs[cm->new_fb_idx].raw_frame_buffer.priv;
          img = &ctx->img;
          img->temporal_id = cm->temporal_layer_id;
          img->spatial_id = cm->spatial_layer_id;
          return add_grain_if_needed(img, ctx->image_with_grain, grain_params);
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

  if (data) {
    av1_ref_frame_t *const frame = data;
    YV12_BUFFER_CONFIG sd;
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
    image2yuvconfig(&frame->img, &sd);
    return av1_set_reference_dec(&frame_worker_data->pbi->common, frame->idx,
                                 frame->use_external_ref, &sd);
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_copy_reference(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  const av1_ref_frame_t *const frame = va_arg(args, av1_ref_frame_t *);
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

static aom_codec_err_t ctrl_copy_new_frame_image(aom_codec_alg_priv_t *ctx,
                                                 va_list args) {
  aom_image_t *img = va_arg(args, aom_image_t *);
  if (img) {
    YV12_BUFFER_CONFIG new_frame;
    AVxWorker *const worker = ctx->frame_workers;
    FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;

    if (av1_get_frame_to_show(frame_worker_data->pbi, &new_frame) == 0) {
      YV12_BUFFER_CONFIG sd;
      image2yuvconfig(img, &sd);
      return av1_copy_new_frame_dec(&frame_worker_data->pbi->common, &new_frame,
                                    &sd);
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
      AV1Decoder *const pbi = frame_worker_data->pbi;
      RefCntBuffer *const frame_bufs = pbi->common.buffer_pool->frame_bufs;
      if (pbi->seen_frame_header && pbi->num_output_frames == 0)
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

static aom_codec_err_t ctrl_get_frame_header_info(aom_codec_alg_priv_t *ctx,
                                                  va_list args) {
  aom_tile_data *const frame_header_info = va_arg(args, aom_tile_data *);

  if (frame_header_info) {
    if (ctx->frame_workers) {
      AVxWorker *const worker = ctx->frame_workers;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      frame_header_info->coded_tile_data_size = pbi->obu_size_hdr.size;
      frame_header_info->coded_tile_data = pbi->obu_size_hdr.data;
      frame_header_info->extra_size = pbi->frame_header_size;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_get_tile_data(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  aom_tile_data *const tile_data = va_arg(args, aom_tile_data *);

  if (tile_data) {
    if (ctx->frame_workers) {
      AVxWorker *const worker = ctx->frame_workers;
      FrameWorkerData *const frame_worker_data =
          (FrameWorkerData *)worker->data1;
      const AV1Decoder *pbi = frame_worker_data->pbi;
      tile_data->coded_tile_data_size =
          pbi->tile_buffers[pbi->dec_tile_row][pbi->dec_tile_col].size;
      tile_data->coded_tile_data =
          pbi->tile_buffers[pbi->dec_tile_row][pbi->dec_tile_col].data;
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  }

  return AOM_CODEC_INVALID_PARAM;
}

static aom_codec_err_t ctrl_set_ext_ref_ptr(aom_codec_alg_priv_t *ctx,
                                            va_list args) {
  av1_ext_ref_frame_t *const data = va_arg(args, av1_ext_ref_frame_t *);

  if (data) {
    av1_ext_ref_frame_t *const ext_frames = data;
    ctx->ext_refs.num = ext_frames->num;
    for (int i = 0; i < ctx->ext_refs.num; i++) {
      image2yuvconfig(ext_frames->img++, &ctx->ext_refs.refs[i]);
    }
    return AOM_CODEC_OK;
  } else {
    return AOM_CODEC_INVALID_PARAM;
  }
}

static aom_codec_err_t ctrl_get_render_size(aom_codec_alg_priv_t *ctx,
                                            va_list args) {
  int *const render_size = va_arg(args, int *);

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

static aom_codec_err_t ctrl_set_tile_mode(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  ctx->tile_mode = va_arg(args, unsigned int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_is_annexb(aom_codec_alg_priv_t *ctx,
                                          va_list args) {
  ctx->is_annexb = va_arg(args, unsigned int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_operating_point(aom_codec_alg_priv_t *ctx,
                                                va_list args) {
  ctx->operating_point = va_arg(args, int);
  return AOM_CODEC_OK;
}

static aom_codec_err_t ctrl_set_output_all_layers(aom_codec_alg_priv_t *ctx,
                                                  va_list args) {
  ctx->output_all_layers = va_arg(args, int);
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

static aom_codec_err_t ctrl_ext_tile_debug(aom_codec_alg_priv_t *ctx,
                                           va_list args) {
  ctx->ext_tile_debug = va_arg(args, int);
  return AOM_CODEC_OK;
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
  { AV1_SET_BYTE_ALIGNMENT, ctrl_set_byte_alignment },
  { AV1_SET_SKIP_LOOP_FILTER, ctrl_set_skip_loop_filter },
  { AV1_SET_DECODE_TILE_ROW, ctrl_set_decode_tile_row },
  { AV1_SET_DECODE_TILE_COL, ctrl_set_decode_tile_col },
  { AV1_SET_TILE_MODE, ctrl_set_tile_mode },
  { AV1D_SET_IS_ANNEXB, ctrl_set_is_annexb },
  { AV1D_SET_OPERATING_POINT, ctrl_set_operating_point },
  { AV1D_SET_OUTPUT_ALL_LAYERS, ctrl_set_output_all_layers },
  { AV1_SET_INSPECTION_CALLBACK, ctrl_set_inspection_callback },
  { AV1D_EXT_TILE_DEBUG, ctrl_ext_tile_debug },
  { AV1D_SET_EXT_REF_PTR, ctrl_set_ext_ref_ptr },

  // Getters
  { AOMD_GET_FRAME_CORRUPTED, ctrl_get_frame_corrupted },
  { AOMD_GET_LAST_QUANTIZER, ctrl_get_last_quantizer },
  { AOMD_GET_LAST_REF_UPDATES, ctrl_get_last_ref_updates },
  { AV1D_GET_BIT_DEPTH, ctrl_get_bit_depth },
  { AV1D_GET_DISPLAY_SIZE, ctrl_get_render_size },
  { AV1D_GET_FRAME_SIZE, ctrl_get_frame_size },
  { AV1_GET_ACCOUNTING, ctrl_get_accounting },
  { AV1_GET_NEW_FRAME_IMAGE, ctrl_get_new_frame_image },
  { AV1_COPY_NEW_FRAME_IMAGE, ctrl_copy_new_frame_image },
  { AV1_GET_REFERENCE, ctrl_get_reference },
  { AV1D_GET_FRAME_HEADER_INFO, ctrl_get_frame_header_info },
  { AV1D_GET_TILE_DATA, ctrl_get_tile_data },

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
