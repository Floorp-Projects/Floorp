/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_svc_layercontext.h"
#include "vp9/encoder/vp9_extend.h"

void vp9_init_layer_context(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  int layer;
  int layer_end;
  int alt_ref_idx = svc->number_spatial_layers * svc->number_temporal_layers;

  svc->spatial_layer_id = 0;
  svc->temporal_layer_id = 0;

  if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
    layer_end = svc->number_temporal_layers;
  } else {
    layer_end = svc->number_spatial_layers;
  }

  for (layer = 0; layer < layer_end; ++layer) {
    LAYER_CONTEXT *const lc = &svc->layer_context[layer];
    RATE_CONTROL *const lrc = &lc->rc;
    int i;
    lc->current_video_frame_in_layer = 0;
    lc->layer_size = 0;
    lc->frames_from_key_frame = 0;
    lc->last_frame_type = FRAME_TYPES;
    lrc->ni_av_qi = oxcf->worst_allowed_q;
    lrc->total_actual_bits = 0;
    lrc->total_target_vs_actual = 0;
    lrc->ni_tot_qi = 0;
    lrc->tot_q = 0.0;
    lrc->avg_q = 0.0;
    lrc->ni_frames = 0;
    lrc->decimation_count = 0;
    lrc->decimation_factor = 0;

    for (i = 0; i < RATE_FACTOR_LEVELS; ++i) {
      lrc->rate_correction_factors[i] = 1.0;
    }

    if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
      lc->target_bandwidth = oxcf->ts_target_bitrate[layer];
      lrc->last_q[INTER_FRAME] = oxcf->worst_allowed_q;
      lrc->avg_frame_qindex[INTER_FRAME] = oxcf->worst_allowed_q;
    } else {
      lc->target_bandwidth = oxcf->ss_target_bitrate[layer];
      lrc->last_q[KEY_FRAME] = oxcf->best_allowed_q;
      lrc->last_q[INTER_FRAME] = oxcf->best_allowed_q;
      lrc->avg_frame_qindex[KEY_FRAME] = (oxcf->worst_allowed_q +
                                          oxcf->best_allowed_q) / 2;
      lrc->avg_frame_qindex[INTER_FRAME] = (oxcf->worst_allowed_q +
                                            oxcf->best_allowed_q) / 2;
      if (oxcf->ss_play_alternate[layer])
        lc->alt_ref_idx = alt_ref_idx++;
      else
        lc->alt_ref_idx = -1;
      lc->gold_ref_idx = -1;
    }

    lrc->buffer_level = oxcf->starting_buffer_level_ms *
                            lc->target_bandwidth / 1000;
    lrc->bits_off_target = lrc->buffer_level;
  }

  // Still have extra buffer for base layer golden frame
  if (!(svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR)
      && alt_ref_idx < REF_FRAMES)
    svc->layer_context[0].gold_ref_idx = alt_ref_idx;
}

// Update the layer context from a change_config() call.
void vp9_update_layer_context_change_config(VP9_COMP *const cpi,
                                            const int target_bandwidth) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  const RATE_CONTROL *const rc = &cpi->rc;
  int layer;
  int layer_end;
  float bitrate_alloc = 1.0;

  if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
    layer_end = svc->number_temporal_layers;
  } else {
    layer_end = svc->number_spatial_layers;
  }

  for (layer = 0; layer < layer_end; ++layer) {
    LAYER_CONTEXT *const lc = &svc->layer_context[layer];
    RATE_CONTROL *const lrc = &lc->rc;

    if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
      lc->target_bandwidth = oxcf->ts_target_bitrate[layer];
    } else {
      lc->target_bandwidth = oxcf->ss_target_bitrate[layer];
    }
    bitrate_alloc = (float)lc->target_bandwidth / target_bandwidth;
    // Update buffer-related quantities.
    lrc->starting_buffer_level =
        (int64_t)(rc->starting_buffer_level * bitrate_alloc);
    lrc->optimal_buffer_level =
        (int64_t)(rc->optimal_buffer_level * bitrate_alloc);
    lrc->maximum_buffer_size =
        (int64_t)(rc->maximum_buffer_size * bitrate_alloc);
    lrc->bits_off_target = MIN(lrc->bits_off_target, lrc->maximum_buffer_size);
    lrc->buffer_level = MIN(lrc->buffer_level, lrc->maximum_buffer_size);
    // Update framerate-related quantities.
    if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
      lc->framerate = cpi->framerate / oxcf->ts_rate_decimator[layer];
    } else {
      lc->framerate = cpi->framerate;
    }
    lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
    lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
    // Update qp-related quantities.
    lrc->worst_quality = rc->worst_quality;
    lrc->best_quality = rc->best_quality;
  }
}

static LAYER_CONTEXT *get_layer_context(VP9_COMP *const cpi) {
  return (cpi->svc.number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) ?
         &cpi->svc.layer_context[cpi->svc.temporal_layer_id] :
         &cpi->svc.layer_context[cpi->svc.spatial_layer_id];
}

void vp9_update_temporal_layer_framerate(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  RATE_CONTROL *const lrc = &lc->rc;
  const int layer = svc->temporal_layer_id;

  lc->framerate = cpi->framerate / oxcf->ts_rate_decimator[layer];
  lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->max_frame_bandwidth = cpi->rc.max_frame_bandwidth;
  // Update the average layer frame size (non-cumulative per-frame-bw).
  if (layer == 0) {
    lc->avg_frame_size = lrc->avg_frame_bandwidth;
  } else {
    const double prev_layer_framerate =
        cpi->framerate / oxcf->ts_rate_decimator[layer - 1];
    const int prev_layer_target_bandwidth = oxcf->ts_target_bitrate[layer - 1];
    lc->avg_frame_size =
        (int)((lc->target_bandwidth - prev_layer_target_bandwidth) /
              (lc->framerate - prev_layer_framerate));
  }
}

void vp9_update_spatial_layer_framerate(VP9_COMP *const cpi, double framerate) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  RATE_CONTROL *const lrc = &lc->rc;

  lc->framerate = framerate;
  lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->min_frame_bandwidth = (int)(lrc->avg_frame_bandwidth *
                                   oxcf->two_pass_vbrmin_section / 100);
  lrc->max_frame_bandwidth = (int)(((int64_t)lrc->avg_frame_bandwidth *
                                   oxcf->two_pass_vbrmax_section) / 100);
  vp9_rc_set_gf_max_interval(cpi, lrc);
}

void vp9_restore_layer_context(VP9_COMP *const cpi) {
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  const int old_frame_since_key = cpi->rc.frames_since_key;
  const int old_frame_to_key = cpi->rc.frames_to_key;

  cpi->rc = lc->rc;
  cpi->twopass = lc->twopass;
  cpi->oxcf.target_bandwidth = lc->target_bandwidth;
  cpi->alt_ref_source = lc->alt_ref_source;
  // Reset the frames_since_key and frames_to_key counters to their values
  // before the layer restore. Keep these defined for the stream (not layer).
  if (cpi->svc.number_temporal_layers > 1) {
    cpi->rc.frames_since_key = old_frame_since_key;
    cpi->rc.frames_to_key = old_frame_to_key;
  }
}

void vp9_save_layer_context(VP9_COMP *const cpi) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);

  lc->rc = cpi->rc;
  lc->twopass = cpi->twopass;
  lc->target_bandwidth = (int)oxcf->target_bandwidth;
  lc->alt_ref_source = cpi->alt_ref_source;
}

void vp9_init_second_pass_spatial_svc(VP9_COMP *cpi) {
  SVC *const svc = &cpi->svc;
  int i;

  for (i = 0; i < svc->number_spatial_layers; ++i) {
    TWO_PASS *const twopass = &svc->layer_context[i].twopass;

    svc->spatial_layer_id = i;
    vp9_init_second_pass(cpi);

    twopass->total_stats.spatial_layer_id = i;
    twopass->total_left_stats.spatial_layer_id = i;
  }
  svc->spatial_layer_id = 0;
}

void vp9_inc_frame_in_layer(VP9_COMP *const cpi) {
  LAYER_CONTEXT *const lc =
      (cpi->svc.number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) ?
      &cpi->svc.layer_context[cpi->svc.temporal_layer_id] :
      &cpi->svc.layer_context[cpi->svc.spatial_layer_id];
  ++lc->current_video_frame_in_layer;
  ++lc->frames_from_key_frame;
}

int vp9_is_upper_layer_key_frame(const VP9_COMP *const cpi) {
  return is_two_pass_svc(cpi) &&
         cpi->svc.spatial_layer_id > 0 &&
         cpi->svc.layer_context[cpi->svc.spatial_layer_id].is_key_frame;
}

#if CONFIG_SPATIAL_SVC
int vp9_svc_lookahead_push(const VP9_COMP *const cpi, struct lookahead_ctx *ctx,
                           YV12_BUFFER_CONFIG *src, int64_t ts_start,
                           int64_t ts_end, unsigned int flags) {
  struct lookahead_entry *buf;
  int i, index;

  if (vp9_lookahead_push(ctx, src, ts_start, ts_end, flags))
    return 1;

  index = ctx->write_idx - 1;
  if (index < 0)
    index += ctx->max_sz;

  buf = ctx->buf + index;

  if (buf == NULL)
    return 1;

  // Store svc parameters for each layer
  for (i = 0; i < cpi->svc.number_spatial_layers; ++i)
    buf->svc_params[i] = cpi->svc.layer_context[i].svc_params_received;

  return 0;
}

static int copy_svc_params(VP9_COMP *const cpi, struct lookahead_entry *buf) {
  int layer_id;
  vpx_svc_parameters_t *layer_param;
  LAYER_CONTEXT *lc;
  int count = 1 << (cpi->svc.number_temporal_layers - 1);

  // Find the next layer to be encoded
  for (layer_id = 0; layer_id < cpi->svc.number_spatial_layers; ++layer_id) {
    if (buf->svc_params[layer_id].spatial_layer >=0)
      break;
  }

  if (layer_id == cpi->svc.number_spatial_layers)
    return 1;

  layer_param = &buf->svc_params[layer_id];
  cpi->svc.spatial_layer_id = layer_param->spatial_layer;
  cpi->svc.temporal_layer_id = layer_param->temporal_layer;
  cpi->ref_frame_flags = VP9_ALT_FLAG | VP9_GOLD_FLAG | VP9_LAST_FLAG;

  lc = &cpi->svc.layer_context[cpi->svc.spatial_layer_id];

  cpi->svc.temporal_layer_id = 0;
  while ((lc->current_video_frame_in_layer % count) != 0) {
    ++cpi->svc.temporal_layer_id;
    count >>= 1;
  }

  cpi->lst_fb_idx =
      cpi->svc.spatial_layer_id * cpi->svc.number_temporal_layers +
      cpi->svc.temporal_layer_id;
  if (lc->frames_from_key_frame < cpi->svc.number_temporal_layers)
    cpi->ref_frame_flags &= ~VP9_LAST_FLAG;

  if (cpi->svc.spatial_layer_id == 0) {
    if (cpi->svc.temporal_layer_id == 0)
      cpi->gld_fb_idx = lc->gold_ref_idx >= 0 ?
                        lc->gold_ref_idx : cpi->lst_fb_idx;
    else
      cpi->gld_fb_idx = cpi->lst_fb_idx - 1;
  } else {
    if (cpi->svc.temporal_layer_id == 0)
      cpi->gld_fb_idx = cpi->svc.spatial_layer_id -
                        cpi->svc.number_temporal_layers;
    else
      cpi->gld_fb_idx = cpi->lst_fb_idx - 1;
  }

  if (lc->current_video_frame_in_layer == 0) {
    if (cpi->svc.spatial_layer_id >= 2) {
      cpi->alt_fb_idx =
          cpi->svc.spatial_layer_id - 2 * cpi->svc.number_temporal_layers;
    } else {
      cpi->alt_fb_idx = cpi->lst_fb_idx;
      cpi->ref_frame_flags &= (~VP9_LAST_FLAG & ~VP9_ALT_FLAG);
    }
  } else {
    if (cpi->oxcf.ss_play_alternate[cpi->svc.spatial_layer_id]) {
      cpi->alt_fb_idx = lc->alt_ref_idx;
      if (!lc->has_alt_frame)
        cpi->ref_frame_flags &= (~VP9_ALT_FLAG);
    } else {
      // Find a proper alt_fb_idx for layers that don't have alt ref frame
      if (cpi->svc.spatial_layer_id == 0) {
        cpi->alt_fb_idx = cpi->lst_fb_idx;
      } else {
        LAYER_CONTEXT *lc_lower =
            &cpi->svc.layer_context[cpi->svc.spatial_layer_id - 1];

        if (cpi->oxcf.ss_play_alternate[cpi->svc.spatial_layer_id - 1] &&
            lc_lower->alt_ref_source != NULL)
          cpi->alt_fb_idx = lc_lower->alt_ref_idx;
        else if (cpi->svc.spatial_layer_id >= 2)
          cpi->alt_fb_idx =
              cpi->svc.spatial_layer_id - 2 * cpi->svc.number_temporal_layers;
        else
          cpi->alt_fb_idx = cpi->lst_fb_idx;
      }
    }
  }

  if (vp9_set_size_literal(cpi, layer_param->width, layer_param->height) != 0)
    return VPX_CODEC_INVALID_PARAM;

  cpi->oxcf.worst_allowed_q =
      vp9_quantizer_to_qindex(layer_param->max_quantizer);
  cpi->oxcf.best_allowed_q =
      vp9_quantizer_to_qindex(layer_param->min_quantizer);

  vp9_change_config(cpi, &cpi->oxcf);

  vp9_set_high_precision_mv(cpi, 1);

  cpi->alt_ref_source = get_layer_context(cpi)->alt_ref_source;

  return 0;
}

struct lookahead_entry *vp9_svc_lookahead_peek(VP9_COMP *const cpi,
                                               struct lookahead_ctx *ctx,
                                               int index, int copy_params) {
  struct lookahead_entry *buf = vp9_lookahead_peek(ctx, index);

  if (buf != NULL && copy_params != 0) {
    if (copy_svc_params(cpi, buf) != 0)
      return NULL;
  }
  return buf;
}

struct lookahead_entry *vp9_svc_lookahead_pop(VP9_COMP *const cpi,
                                              struct lookahead_ctx *ctx,
                                              int drain) {
  struct lookahead_entry *buf = NULL;

  if (ctx->sz && (drain || ctx->sz == ctx->max_sz - MAX_PRE_FRAMES)) {
    buf = vp9_svc_lookahead_peek(cpi, ctx, 0, 1);
    if (buf != NULL) {
      // Only remove the buffer when pop the highest layer. Simply set the
      // spatial_layer to -1 for lower layers.
      buf->svc_params[cpi->svc.spatial_layer_id].spatial_layer = -1;
      if (cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1) {
        vp9_lookahead_pop(ctx, drain);
      }
    }
  }

  return buf;
}
#endif
