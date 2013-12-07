/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include "vpx/vpx_codec.h"
#include "vpx/internal/vpx_codec_internal.h"
#include "./vpx_version.h"
#include "vp9/encoder/vp9_onyx_int.h"
#include "vpx/vp8cx.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/common/vp9_onyx.h"
#include "vp9/vp9_iface_common.h"

struct vp9_extracfg {
  struct vpx_codec_pkt_list *pkt_list;
  int                         cpu_used;  /* available cpu percentage in 1/16 */
  unsigned int                enable_auto_alt_ref;
  unsigned int                noise_sensitivity;
  unsigned int                Sharpness;
  unsigned int                static_thresh;
  unsigned int                tile_columns;
  unsigned int                tile_rows;
  unsigned int                arnr_max_frames;
  unsigned int                arnr_strength;
  unsigned int                arnr_type;
  unsigned int                experimental;
  vp8e_tuning                 tuning;
  unsigned int                cq_level;         /* constrained quality level */
  unsigned int                rc_max_intra_bitrate_pct;
  unsigned int                lossless;
  unsigned int                frame_parallel_decoding_mode;
  unsigned int                aq_mode;
};

struct extraconfig_map {
  int                 usage;
  struct vp9_extracfg cfg;
};

static const struct extraconfig_map extracfg_map[] = {
  {
    0,
    { // NOLINT
      NULL,
      0,                          /* cpu_used      */
      1,                          /* enable_auto_alt_ref */
      0,                          /* noise_sensitivity */
      0,                          /* Sharpness */
      0,                          /* static_thresh */
      0,                          /* tile_columns */
      0,                          /* tile_rows */
      7,                          /* arnr_max_frames */
      5,                          /* arnr_strength */
      3,                          /* arnr_type*/
      0,                          /* experimental mode */
      0,                          /* tuning*/
      10,                         /* cq_level */
      0,                          /* rc_max_intra_bitrate_pct */
      0,                          /* lossless */
      0,                          /* frame_parallel_decoding_mode */
      0,                          /* aq_mode */
    }
  }
};

struct vpx_codec_alg_priv {
  vpx_codec_priv_t        base;
  vpx_codec_enc_cfg_t     cfg;
  struct vp9_extracfg     vp8_cfg;
  VP9_CONFIG              oxcf;
  VP9_PTR             cpi;
  unsigned char          *cx_data;
  unsigned int            cx_data_sz;
  unsigned char          *pending_cx_data;
  unsigned int            pending_cx_data_sz;
  int                     pending_frame_count;
  uint32_t                pending_frame_sizes[8];
  uint32_t                pending_frame_magnitude;
  vpx_image_t             preview_img;
  vp8_postproc_cfg_t      preview_ppcfg;
  vpx_codec_pkt_list_decl(64) pkt_list;
  unsigned int                fixed_kf_cntr;
};

static VP9_REFFRAME ref_frame_to_vp9_reframe(vpx_ref_frame_type_t frame) {
  switch (frame) {
    case VP8_LAST_FRAME:
      return VP9_LAST_FLAG;
    case VP8_GOLD_FRAME:
      return VP9_GOLD_FLAG;
    case VP8_ALTR_FRAME:
      return VP9_ALT_FLAG;
  }
  assert(!"Invalid Reference Frame");
  return VP9_LAST_FLAG;
}

static vpx_codec_err_t
update_error_state(vpx_codec_alg_priv_t                 *ctx,
                   const struct vpx_internal_error_info *error) {
  vpx_codec_err_t res;

  if ((res = error->error_code))
    ctx->base.err_detail = error->has_detail
                           ? error->detail
                           : NULL;

  return res;
}


#undef ERROR
#define ERROR(str) do {\
    ctx->base.err_detail = str;\
    return VPX_CODEC_INVALID_PARAM;\
  } while (0)

#define RANGE_CHECK(p, memb, lo, hi) do {\
    if (!(((p)->memb == lo || (p)->memb > (lo)) && (p)->memb <= hi)) \
      ERROR(#memb " out of range ["#lo".."#hi"]");\
  } while (0)

#define RANGE_CHECK_HI(p, memb, hi) do {\
    if (!((p)->memb <= (hi))) \
      ERROR(#memb " out of range [.."#hi"]");\
  } while (0)

#define RANGE_CHECK_LO(p, memb, lo) do {\
    if (!((p)->memb >= (lo))) \
      ERROR(#memb " out of range ["#lo"..]");\
  } while (0)

#define RANGE_CHECK_BOOL(p, memb) do {\
    if (!!((p)->memb) != (p)->memb) ERROR(#memb " expected boolean");\
  } while (0)

static vpx_codec_err_t validate_config(vpx_codec_alg_priv_t      *ctx,
                                       const vpx_codec_enc_cfg_t *cfg,
                                       const struct vp9_extracfg *vp8_cfg) {
  RANGE_CHECK(cfg, g_w,                   1, 65535); /* 16 bits available */
  RANGE_CHECK(cfg, g_h,                   1, 65535); /* 16 bits available */
  RANGE_CHECK(cfg, g_timebase.den,        1, 1000000000);
  RANGE_CHECK(cfg, g_timebase.num,        1, cfg->g_timebase.den);
  RANGE_CHECK_HI(cfg, g_profile,          3);

  RANGE_CHECK_HI(cfg, rc_max_quantizer,   63);
  RANGE_CHECK_HI(cfg, rc_min_quantizer,   cfg->rc_max_quantizer);
  RANGE_CHECK_BOOL(vp8_cfg, lossless);
  if (vp8_cfg->lossless) {
    RANGE_CHECK_HI(cfg, rc_max_quantizer, 0);
    RANGE_CHECK_HI(cfg, rc_min_quantizer, 0);
  }
  RANGE_CHECK(vp8_cfg, aq_mode,           0, AQ_MODES_COUNT - 1);

  RANGE_CHECK_HI(cfg, g_threads,          64);
  RANGE_CHECK_HI(cfg, g_lag_in_frames,    MAX_LAG_BUFFERS);
  RANGE_CHECK(cfg, rc_end_usage,          VPX_VBR, VPX_Q);
  RANGE_CHECK_HI(cfg, rc_undershoot_pct,  1000);
  RANGE_CHECK_HI(cfg, rc_overshoot_pct,   1000);
  RANGE_CHECK_HI(cfg, rc_2pass_vbr_bias_pct, 100);
  RANGE_CHECK(cfg, kf_mode,               VPX_KF_DISABLED, VPX_KF_AUTO);
  // RANGE_CHECK_BOOL(cfg,                 g_delete_firstpassfile);
  RANGE_CHECK_BOOL(cfg,                   rc_resize_allowed);
  RANGE_CHECK_HI(cfg, rc_dropframe_thresh,   100);
  RANGE_CHECK_HI(cfg, rc_resize_up_thresh,   100);
  RANGE_CHECK_HI(cfg, rc_resize_down_thresh, 100);
  RANGE_CHECK(cfg,        g_pass,         VPX_RC_ONE_PASS, VPX_RC_LAST_PASS);

  RANGE_CHECK(cfg, ss_number_layers,      1,
              VPX_SS_MAX_LAYERS); /*Spatial layers max */
  /* VP8 does not support a lower bound on the keyframe interval in
   * automatic keyframe placement mode.
   */
  if (cfg->kf_mode != VPX_KF_DISABLED && cfg->kf_min_dist != cfg->kf_max_dist
      && cfg->kf_min_dist > 0)
    ERROR("kf_min_dist not supported in auto mode, use 0 "
          "or kf_max_dist instead.");

  RANGE_CHECK_BOOL(vp8_cfg,               enable_auto_alt_ref);
  RANGE_CHECK(vp8_cfg, cpu_used,           -16, 16);

  RANGE_CHECK_HI(vp8_cfg, noise_sensitivity,  6);

  RANGE_CHECK(vp8_cfg, tile_columns, 0, 6);
  RANGE_CHECK(vp8_cfg, tile_rows, 0, 2);
  RANGE_CHECK_HI(vp8_cfg, Sharpness,       7);
  RANGE_CHECK(vp8_cfg, arnr_max_frames, 0, 15);
  RANGE_CHECK_HI(vp8_cfg, arnr_strength,   6);
  RANGE_CHECK(vp8_cfg, arnr_type,       1, 3);
  RANGE_CHECK(vp8_cfg, cq_level, 0, 63);

  if (cfg->g_pass == VPX_RC_LAST_PASS) {
    size_t           packet_sz = sizeof(FIRSTPASS_STATS);
    int              n_packets = (int)(cfg->rc_twopass_stats_in.sz / packet_sz);
    FIRSTPASS_STATS *stats;

    if (!cfg->rc_twopass_stats_in.buf)
      ERROR("rc_twopass_stats_in.buf not set.");

    if (cfg->rc_twopass_stats_in.sz % packet_sz)
      ERROR("rc_twopass_stats_in.sz indicates truncated packet.");

    if (cfg->rc_twopass_stats_in.sz < 2 * packet_sz)
      ERROR("rc_twopass_stats_in requires at least two packets.");

    stats = (void *)((char *)cfg->rc_twopass_stats_in.buf
                     + (n_packets - 1) * packet_sz);

    if ((int)(stats->count + 0.5) != n_packets - 1)
      ERROR("rc_twopass_stats_in missing EOS stats packet");
  }

  return VPX_CODEC_OK;
}


static vpx_codec_err_t validate_img(vpx_codec_alg_priv_t *ctx,
                                    const vpx_image_t    *img) {
  switch (img->fmt) {
    case VPX_IMG_FMT_YV12:
    case VPX_IMG_FMT_I420:
    case VPX_IMG_FMT_I422:
    case VPX_IMG_FMT_I444:
      break;
    default:
      ERROR("Invalid image format. Only YV12, I420, I422, I444 images are "
            "supported.");
  }

  if ((img->d_w != ctx->cfg.g_w) || (img->d_h != ctx->cfg.g_h))
    ERROR("Image size must match encoder init configuration size");

  return VPX_CODEC_OK;
}


static vpx_codec_err_t set_vp9e_config(VP9_CONFIG *oxcf,
                                       vpx_codec_enc_cfg_t cfg,
                                       struct vp9_extracfg vp8_cfg) {
  oxcf->version = cfg.g_profile | (vp8_cfg.experimental ? 0x4 : 0);
  oxcf->width   = cfg.g_w;
  oxcf->height  = cfg.g_h;
  /* guess a frame rate if out of whack, use 30 */
  oxcf->framerate = (double)(cfg.g_timebase.den)
                    / (double)(cfg.g_timebase.num);

  if (oxcf->framerate > 180) {
    oxcf->framerate = 30;
  }

  switch (cfg.g_pass) {
    case VPX_RC_ONE_PASS:
      oxcf->Mode = MODE_GOODQUALITY;
      break;
    case VPX_RC_FIRST_PASS:
      oxcf->Mode = MODE_FIRSTPASS;
      break;
    case VPX_RC_LAST_PASS:
      oxcf->Mode = MODE_SECONDPASS_BEST;
      break;
  }

  if (cfg.g_pass == VPX_RC_FIRST_PASS) {
    oxcf->allow_lag     = 0;
    oxcf->lag_in_frames = 0;
  } else {
    oxcf->allow_lag     = (cfg.g_lag_in_frames) > 0;
    oxcf->lag_in_frames = cfg.g_lag_in_frames;
  }

  // VBR only supported for now.
  // CBR code has been deprectated for experimental phase.
  // CQ mode not yet tested
  oxcf->end_usage        = USAGE_LOCAL_FILE_PLAYBACK;
  if (cfg.rc_end_usage == VPX_CQ)
    oxcf->end_usage      = USAGE_CONSTRAINED_QUALITY;
  else if (cfg.rc_end_usage == VPX_Q)
    oxcf->end_usage      = USAGE_CONSTANT_QUALITY;
  else if (cfg.rc_end_usage == VPX_CBR)
    oxcf->end_usage = USAGE_STREAM_FROM_SERVER;

  oxcf->target_bandwidth         = cfg.rc_target_bitrate;
  oxcf->rc_max_intra_bitrate_pct = vp8_cfg.rc_max_intra_bitrate_pct;

  oxcf->best_allowed_q          = cfg.rc_min_quantizer;
  oxcf->worst_allowed_q         = cfg.rc_max_quantizer;
  oxcf->cq_level                = vp8_cfg.cq_level;
  oxcf->fixed_q = -1;

  oxcf->under_shoot_pct         = cfg.rc_undershoot_pct;
  oxcf->over_shoot_pct          = cfg.rc_overshoot_pct;

  oxcf->maximum_buffer_size     = cfg.rc_buf_sz;
  oxcf->starting_buffer_level   = cfg.rc_buf_initial_sz;
  oxcf->optimal_buffer_level    = cfg.rc_buf_optimal_sz;

  oxcf->two_pass_vbrbias         = cfg.rc_2pass_vbr_bias_pct;
  oxcf->two_pass_vbrmin_section  = cfg.rc_2pass_vbr_minsection_pct;
  oxcf->two_pass_vbrmax_section  = cfg.rc_2pass_vbr_maxsection_pct;

  oxcf->auto_key               = cfg.kf_mode == VPX_KF_AUTO
                                 && cfg.kf_min_dist != cfg.kf_max_dist;
  // oxcf->kf_min_dist         = cfg.kf_min_dis;
  oxcf->key_freq               = cfg.kf_max_dist;

  // oxcf->delete_first_pass_file = cfg.g_delete_firstpassfile;
  // strcpy(oxcf->first_pass_file, cfg.g_firstpass_file);

  oxcf->cpu_used               =  vp8_cfg.cpu_used;
  oxcf->encode_breakout        =  vp8_cfg.static_thresh;
  oxcf->play_alternate         =  vp8_cfg.enable_auto_alt_ref;
  oxcf->noise_sensitivity      =  vp8_cfg.noise_sensitivity;
  oxcf->Sharpness              =  vp8_cfg.Sharpness;

  oxcf->two_pass_stats_in      =  cfg.rc_twopass_stats_in;
  oxcf->output_pkt_list        =  vp8_cfg.pkt_list;

  oxcf->arnr_max_frames = vp8_cfg.arnr_max_frames;
  oxcf->arnr_strength   = vp8_cfg.arnr_strength;
  oxcf->arnr_type       = vp8_cfg.arnr_type;

  oxcf->tuning = vp8_cfg.tuning;

  oxcf->tile_columns = vp8_cfg.tile_columns;
  oxcf->tile_rows    = vp8_cfg.tile_rows;

  oxcf->lossless = vp8_cfg.lossless;

  oxcf->error_resilient_mode         = cfg.g_error_resilient;
  oxcf->frame_parallel_decoding_mode = vp8_cfg.frame_parallel_decoding_mode;

  oxcf->aq_mode = vp8_cfg.aq_mode;

  oxcf->ss_number_layers = cfg.ss_number_layers;
  /*
  printf("Current VP9 Settings: \n");
  printf("target_bandwidth: %d\n", oxcf->target_bandwidth);
  printf("noise_sensitivity: %d\n", oxcf->noise_sensitivity);
  printf("Sharpness: %d\n",    oxcf->Sharpness);
  printf("cpu_used: %d\n",  oxcf->cpu_used);
  printf("Mode: %d\n",     oxcf->Mode);
  // printf("delete_first_pass_file: %d\n",  oxcf->delete_first_pass_file);
  printf("auto_key: %d\n",  oxcf->auto_key);
  printf("key_freq: %d\n", oxcf->key_freq);
  printf("end_usage: %d\n", oxcf->end_usage);
  printf("under_shoot_pct: %d\n", oxcf->under_shoot_pct);
  printf("over_shoot_pct: %d\n", oxcf->over_shoot_pct);
  printf("starting_buffer_level: %d\n", oxcf->starting_buffer_level);
  printf("optimal_buffer_level: %d\n",  oxcf->optimal_buffer_level);
  printf("maximum_buffer_size: %d\n", oxcf->maximum_buffer_size);
  printf("fixed_q: %d\n",  oxcf->fixed_q);
  printf("worst_allowed_q: %d\n", oxcf->worst_allowed_q);
  printf("best_allowed_q: %d\n", oxcf->best_allowed_q);
  printf("two_pass_vbrbias: %d\n",  oxcf->two_pass_vbrbias);
  printf("two_pass_vbrmin_section: %d\n", oxcf->two_pass_vbrmin_section);
  printf("two_pass_vbrmax_section: %d\n", oxcf->two_pass_vbrmax_section);
  printf("allow_lag: %d\n", oxcf->allow_lag);
  printf("lag_in_frames: %d\n", oxcf->lag_in_frames);
  printf("play_alternate: %d\n", oxcf->play_alternate);
  printf("Version: %d\n", oxcf->Version);
  printf("encode_breakout: %d\n", oxcf->encode_breakout);
  printf("error resilient: %d\n", oxcf->error_resilient_mode);
  printf("frame parallel detokenization: %d\n",
         oxcf->frame_parallel_decoding_mode);
  */
  return VPX_CODEC_OK;
}

static vpx_codec_err_t vp9e_set_config(vpx_codec_alg_priv_t       *ctx,
                                       const vpx_codec_enc_cfg_t  *cfg) {
  vpx_codec_err_t res;

  if ((cfg->g_w != ctx->cfg.g_w) || (cfg->g_h != ctx->cfg.g_h))
    ERROR("Cannot change width or height after initialization");

  /* Prevent increasing lag_in_frames. This check is stricter than it needs
   * to be -- the limit is not increasing past the first lag_in_frames
   * value, but we don't track the initial config, only the last successful
   * config.
   */
  if ((cfg->g_lag_in_frames > ctx->cfg.g_lag_in_frames))
    ERROR("Cannot increase lag_in_frames");

  res = validate_config(ctx, cfg, &ctx->vp8_cfg);

  if (!res) {
    ctx->cfg = *cfg;
    set_vp9e_config(&ctx->oxcf, ctx->cfg, ctx->vp8_cfg);
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }

  return res;
}


int vp9_reverse_trans(int q);


static vpx_codec_err_t get_param(vpx_codec_alg_priv_t *ctx,
                                 int                   ctrl_id,
                                 va_list               args) {
  void *arg = va_arg(args, void *);

#define MAP(id, var) case id: *(RECAST(id, arg)) = var; break

  if (!arg)
    return VPX_CODEC_INVALID_PARAM;

  switch (ctrl_id) {
      MAP(VP8E_GET_LAST_QUANTIZER, vp9_get_quantizer(ctx->cpi));
      MAP(VP8E_GET_LAST_QUANTIZER_64,
          vp9_reverse_trans(vp9_get_quantizer(ctx->cpi)));
  }

  return VPX_CODEC_OK;
#undef MAP
}


static vpx_codec_err_t set_param(vpx_codec_alg_priv_t *ctx,
                                 int                   ctrl_id,
                                 va_list               args) {
  vpx_codec_err_t     res  = VPX_CODEC_OK;
  struct vp9_extracfg xcfg = ctx->vp8_cfg;

#define MAP(id, var) case id: var = CAST(id, args); break;

  switch (ctrl_id) {
      MAP(VP8E_SET_CPUUSED,                 xcfg.cpu_used);
      MAP(VP8E_SET_ENABLEAUTOALTREF,        xcfg.enable_auto_alt_ref);
      MAP(VP8E_SET_NOISE_SENSITIVITY,       xcfg.noise_sensitivity);
      MAP(VP8E_SET_SHARPNESS,               xcfg.Sharpness);
      MAP(VP8E_SET_STATIC_THRESHOLD,        xcfg.static_thresh);
      MAP(VP9E_SET_TILE_COLUMNS,            xcfg.tile_columns);
      MAP(VP9E_SET_TILE_ROWS,               xcfg.tile_rows);
      MAP(VP8E_SET_ARNR_MAXFRAMES,          xcfg.arnr_max_frames);
      MAP(VP8E_SET_ARNR_STRENGTH,           xcfg.arnr_strength);
      MAP(VP8E_SET_ARNR_TYPE,               xcfg.arnr_type);
      MAP(VP8E_SET_TUNING,                  xcfg.tuning);
      MAP(VP8E_SET_CQ_LEVEL,                xcfg.cq_level);
      MAP(VP8E_SET_MAX_INTRA_BITRATE_PCT,   xcfg.rc_max_intra_bitrate_pct);
      MAP(VP9E_SET_LOSSLESS,                xcfg.lossless);
      MAP(VP9E_SET_FRAME_PARALLEL_DECODING, xcfg.frame_parallel_decoding_mode);
      MAP(VP9E_SET_AQ_MODE,                 xcfg.aq_mode);
  }

  res = validate_config(ctx, &ctx->cfg, &xcfg);

  if (!res) {
    ctx->vp8_cfg = xcfg;
    set_vp9e_config(&ctx->oxcf, ctx->cfg, ctx->vp8_cfg);
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }

  return res;
#undef MAP
}


static vpx_codec_err_t vp9e_common_init(vpx_codec_ctx_t *ctx,
                                        int              experimental) {
  vpx_codec_err_t            res = VPX_CODEC_OK;
  struct vpx_codec_alg_priv *priv;
  vpx_codec_enc_cfg_t       *cfg;
  unsigned int               i;

  VP9_PTR optr;

  if (!ctx->priv) {
    priv = calloc(1, sizeof(struct vpx_codec_alg_priv));

    if (!priv) {
      return VPX_CODEC_MEM_ERROR;
    }

    ctx->priv = &priv->base;
    ctx->priv->sz = sizeof(*ctx->priv);
    ctx->priv->iface = ctx->iface;
    ctx->priv->alg_priv = priv;
    ctx->priv->init_flags = ctx->init_flags;
    ctx->priv->enc.total_encoders = 1;

    if (ctx->config.enc) {
      /* Update the reference to the config structure to an
       * internal copy.
       */
      ctx->priv->alg_priv->cfg = *ctx->config.enc;
      ctx->config.enc = &ctx->priv->alg_priv->cfg;
    }

    cfg =  &ctx->priv->alg_priv->cfg;

    /* Select the extra vp6 configuration table based on the current
     * usage value. If the current usage value isn't found, use the
     * values for usage case 0.
     */
    for (i = 0;
         extracfg_map[i].usage && extracfg_map[i].usage != cfg->g_usage;
         i++) {}

    priv->vp8_cfg = extracfg_map[i].cfg;
    priv->vp8_cfg.pkt_list = &priv->pkt_list.head;
    priv->vp8_cfg.experimental = experimental;

    // TODO(agrange) Check the limits set on this buffer, or the check that is
    // applied in vp9e_encode.
    priv->cx_data_sz = priv->cfg.g_w * priv->cfg.g_h * 3 / 2 * 8;
//    priv->cx_data_sz = priv->cfg.g_w * priv->cfg.g_h * 3 / 2 * 2;

    if (priv->cx_data_sz < 4096) priv->cx_data_sz = 4096;

    priv->cx_data = malloc(priv->cx_data_sz);

    if (!priv->cx_data) {
      return VPX_CODEC_MEM_ERROR;
    }

    vp9_initialize_enc();

    res = validate_config(priv, &priv->cfg, &priv->vp8_cfg);

    if (!res) {
      set_vp9e_config(&ctx->priv->alg_priv->oxcf,
                      ctx->priv->alg_priv->cfg,
                      ctx->priv->alg_priv->vp8_cfg);
      optr = vp9_create_compressor(&ctx->priv->alg_priv->oxcf);

      if (!optr)
        res = VPX_CODEC_MEM_ERROR;
      else
        ctx->priv->alg_priv->cpi = optr;
    }
  }

  return res;
}


static vpx_codec_err_t vp9e_init(vpx_codec_ctx_t *ctx,
                                 vpx_codec_priv_enc_mr_cfg_t *data) {
  return vp9e_common_init(ctx, 0);
}


#if CONFIG_EXPERIMENTAL
static vpx_codec_err_t vp9e_exp_init(vpx_codec_ctx_t *ctx,
                                     vpx_codec_priv_enc_mr_cfg_t *data) {
  return vp9e_common_init(ctx, 1);
}
#endif


static vpx_codec_err_t vp9e_destroy(vpx_codec_alg_priv_t *ctx) {
  free(ctx->cx_data);
  vp9_remove_compressor(&ctx->cpi);
  free(ctx);
  return VPX_CODEC_OK;
}

static void pick_quickcompress_mode(vpx_codec_alg_priv_t  *ctx,
                                    unsigned long          duration,
                                    unsigned long          deadline) {
  unsigned int new_qc;

  /* Use best quality mode if no deadline is given. */
  if (deadline)
    new_qc = MODE_GOODQUALITY;
  else
    new_qc = MODE_BESTQUALITY;

  if (ctx->cfg.g_pass == VPX_RC_FIRST_PASS)
    new_qc = MODE_FIRSTPASS;
  else if (ctx->cfg.g_pass == VPX_RC_LAST_PASS)
    new_qc = (new_qc == MODE_BESTQUALITY)
             ? MODE_SECONDPASS_BEST
             : MODE_SECONDPASS;

  if (ctx->oxcf.Mode != new_qc) {
    ctx->oxcf.Mode = new_qc;
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }
}


static int write_superframe_index(vpx_codec_alg_priv_t *ctx) {
  uint8_t marker = 0xc0;
  unsigned int mask;
  int mag, index_sz;

  assert(ctx->pending_frame_count);
  assert(ctx->pending_frame_count <= 8);

  /* Add the number of frames to the marker byte */
  marker |= ctx->pending_frame_count - 1;

  /* Choose the magnitude */
  for (mag = 0, mask = 0xff; mag < 4; mag++) {
    if (ctx->pending_frame_magnitude < mask)
      break;
    mask <<= 8;
    mask |= 0xff;
  }
  marker |= mag << 3;

  /* Write the index */
  index_sz = 2 + (mag + 1) * ctx->pending_frame_count;
  if (ctx->pending_cx_data_sz + index_sz < ctx->cx_data_sz) {
    uint8_t *x = ctx->pending_cx_data + ctx->pending_cx_data_sz;
    int i, j;

    *x++ = marker;
    for (i = 0; i < ctx->pending_frame_count; i++) {
      int this_sz = ctx->pending_frame_sizes[i];

      for (j = 0; j <= mag; j++) {
        *x++ = this_sz & 0xff;
        this_sz >>= 8;
      }
    }
    *x++ = marker;
    ctx->pending_cx_data_sz += index_sz;
  }
  return index_sz;
}

static vpx_codec_err_t vp9e_encode(vpx_codec_alg_priv_t  *ctx,
                                   const vpx_image_t     *img,
                                   vpx_codec_pts_t        pts,
                                   unsigned long          duration,
                                   vpx_enc_frame_flags_t  flags,
                                   unsigned long          deadline) {
  vpx_codec_err_t res = VPX_CODEC_OK;

  if (img)
    res = validate_img(ctx, img);

  pick_quickcompress_mode(ctx, duration, deadline);
  vpx_codec_pkt_list_init(&ctx->pkt_list);

  /* Handle Flags */
  if (((flags & VP8_EFLAG_NO_UPD_GF) && (flags & VP8_EFLAG_FORCE_GF))
      || ((flags & VP8_EFLAG_NO_UPD_ARF) && (flags & VP8_EFLAG_FORCE_ARF))) {
    ctx->base.err_detail = "Conflicting flags.";
    return VPX_CODEC_INVALID_PARAM;
  }

  if (flags & (VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_GF
               | VP8_EFLAG_NO_REF_ARF)) {
    int ref = 7;

    if (flags & VP8_EFLAG_NO_REF_LAST)
      ref ^= VP9_LAST_FLAG;

    if (flags & VP8_EFLAG_NO_REF_GF)
      ref ^= VP9_GOLD_FLAG;

    if (flags & VP8_EFLAG_NO_REF_ARF)
      ref ^= VP9_ALT_FLAG;

    vp9_use_as_reference(ctx->cpi, ref);
  }

  if (flags & (VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF
               | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_FORCE_GF
               | VP8_EFLAG_FORCE_ARF)) {
    int upd = 7;

    if (flags & VP8_EFLAG_NO_UPD_LAST)
      upd ^= VP9_LAST_FLAG;

    if (flags & VP8_EFLAG_NO_UPD_GF)
      upd ^= VP9_GOLD_FLAG;

    if (flags & VP8_EFLAG_NO_UPD_ARF)
      upd ^= VP9_ALT_FLAG;

    vp9_update_reference(ctx->cpi, upd);
  }

  if (flags & VP8_EFLAG_NO_UPD_ENTROPY) {
    vp9_update_entropy(ctx->cpi, 0);
  }

  /* Handle fixed keyframe intervals */
  if (ctx->cfg.kf_mode == VPX_KF_AUTO
      && ctx->cfg.kf_min_dist == ctx->cfg.kf_max_dist) {
    if (++ctx->fixed_kf_cntr > ctx->cfg.kf_min_dist) {
      flags |= VPX_EFLAG_FORCE_KF;
      ctx->fixed_kf_cntr = 1;
    }
  }

  /* Initialize the encoder instance on the first frame*/
  if (!res && ctx->cpi) {
    unsigned int lib_flags;
    YV12_BUFFER_CONFIG sd;
    int64_t dst_time_stamp, dst_end_time_stamp;
    unsigned long size, cx_data_sz;
    unsigned char *cx_data;

    /* Set up internal flags */
    if (ctx->base.init_flags & VPX_CODEC_USE_PSNR)
      ((VP9_COMP *)ctx->cpi)->b_calculate_psnr = 1;

    // if (ctx->base.init_flags & VPX_CODEC_USE_OUTPUT_PARTITION)
    //    ((VP9_COMP *)ctx->cpi)->output_partition = 1;

    /* Convert API flags to internal codec lib flags */
    lib_flags = (flags & VPX_EFLAG_FORCE_KF) ? FRAMEFLAGS_KEY : 0;

    /* vp8 use 10,000,000 ticks/second as time stamp */
    dst_time_stamp = pts * 10000000 * ctx->cfg.g_timebase.num
                     / ctx->cfg.g_timebase.den;
    dst_end_time_stamp = (pts + duration) * 10000000 * ctx->cfg.g_timebase.num /
                         ctx->cfg.g_timebase.den;

    if (img != NULL) {
      res = image2yuvconfig(img, &sd);

      if (vp9_receive_raw_frame(ctx->cpi, lib_flags,
                                &sd, dst_time_stamp, dst_end_time_stamp)) {
        VP9_COMP *cpi = (VP9_COMP *)ctx->cpi;
        res = update_error_state(ctx, &cpi->common.error);
      }
    }

    cx_data = ctx->cx_data;
    cx_data_sz = ctx->cx_data_sz;
    lib_flags = 0;

    /* Any pending invisible frames? */
    if (ctx->pending_cx_data) {
      memmove(cx_data, ctx->pending_cx_data, ctx->pending_cx_data_sz);
      ctx->pending_cx_data = cx_data;
      cx_data += ctx->pending_cx_data_sz;
      cx_data_sz -= ctx->pending_cx_data_sz;

      /* TODO: this is a minimal check, the underlying codec doesn't respect
       * the buffer size anyway.
       */
      if (cx_data_sz < ctx->cx_data_sz / 2) {
        ctx->base.err_detail = "Compressed data buffer too small";
        return VPX_CODEC_ERROR;
      }
    }

    while (cx_data_sz >= ctx->cx_data_sz / 2 &&
           -1 != vp9_get_compressed_data(ctx->cpi, &lib_flags, &size,
                                         cx_data, &dst_time_stamp,
                                         &dst_end_time_stamp, !img)) {
      if (size) {
        vpx_codec_pts_t    round, delta;
        vpx_codec_cx_pkt_t pkt;
        VP9_COMP *cpi = (VP9_COMP *)ctx->cpi;

        /* Pack invisible frames with the next visible frame */
        if (!cpi->common.show_frame) {
          if (!ctx->pending_cx_data)
            ctx->pending_cx_data = cx_data;
          ctx->pending_cx_data_sz += size;
          ctx->pending_frame_sizes[ctx->pending_frame_count++] = size;
          ctx->pending_frame_magnitude |= size;
          cx_data += size;
          cx_data_sz -= size;
          continue;
        }

        /* Add the frame packet to the list of returned packets. */
        round = (vpx_codec_pts_t)1000000 * ctx->cfg.g_timebase.num / 2 - 1;
        delta = (dst_end_time_stamp - dst_time_stamp);
        pkt.kind = VPX_CODEC_CX_FRAME_PKT;
        pkt.data.frame.pts =
          (dst_time_stamp * ctx->cfg.g_timebase.den + round)
          / ctx->cfg.g_timebase.num / 10000000;
        pkt.data.frame.duration = (unsigned long)
          ((delta * ctx->cfg.g_timebase.den + round)
          / ctx->cfg.g_timebase.num / 10000000);
        pkt.data.frame.flags = lib_flags << 16;

        if (lib_flags & FRAMEFLAGS_KEY)
          pkt.data.frame.flags |= VPX_FRAME_IS_KEY;

        if (!cpi->common.show_frame) {
          pkt.data.frame.flags |= VPX_FRAME_IS_INVISIBLE;

          // This timestamp should be as close as possible to the
          // prior PTS so that if a decoder uses pts to schedule when
          // to do this, we start right after last frame was decoded.
          // Invisible frames have no duration.
          pkt.data.frame.pts = ((cpi->last_time_stamp_seen
                                 * ctx->cfg.g_timebase.den + round)
                                / ctx->cfg.g_timebase.num / 10000000) + 1;
          pkt.data.frame.duration = 0;
        }

        if (cpi->droppable)
          pkt.data.frame.flags |= VPX_FRAME_IS_DROPPABLE;

        /*if (cpi->output_partition)
        {
            int i;
            const int num_partitions = 1;

            pkt.data.frame.flags |= VPX_FRAME_IS_FRAGMENT;

            for (i = 0; i < num_partitions; ++i)
            {
                pkt.data.frame.buf = cx_data;
                pkt.data.frame.sz = cpi->partition_sz[i];
                pkt.data.frame.partition_id = i;
                // don't set the fragment bit for the last partition
                if (i == (num_partitions - 1))
                    pkt.data.frame.flags &= ~VPX_FRAME_IS_FRAGMENT;
                vpx_codec_pkt_list_add(&ctx->pkt_list.head, &pkt);
                cx_data += cpi->partition_sz[i];
                cx_data_sz -= cpi->partition_sz[i];
            }
        }
        else*/
        {
          if (ctx->pending_cx_data) {
            ctx->pending_frame_sizes[ctx->pending_frame_count++] = size;
            ctx->pending_frame_magnitude |= size;
            ctx->pending_cx_data_sz += size;
            size += write_superframe_index(ctx);
            pkt.data.frame.buf = ctx->pending_cx_data;
            pkt.data.frame.sz  = ctx->pending_cx_data_sz;
            ctx->pending_cx_data = NULL;
            ctx->pending_cx_data_sz = 0;
            ctx->pending_frame_count = 0;
            ctx->pending_frame_magnitude = 0;
          } else {
            pkt.data.frame.buf = cx_data;
            pkt.data.frame.sz  = size;
          }
          pkt.data.frame.partition_id = -1;
          vpx_codec_pkt_list_add(&ctx->pkt_list.head, &pkt);
          cx_data += size;
          cx_data_sz -= size;
        }
      }
    }
  }

  return res;
}


static const vpx_codec_cx_pkt_t *vp9e_get_cxdata(vpx_codec_alg_priv_t  *ctx,
                                                 vpx_codec_iter_t      *iter) {
  return vpx_codec_pkt_list_get(&ctx->pkt_list.head, iter);
}

static vpx_codec_err_t vp9e_set_reference(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id,
                                          va_list args) {
  vpx_ref_frame_t *data = va_arg(args, vpx_ref_frame_t *);

  if (data) {
    vpx_ref_frame_t *frame = (vpx_ref_frame_t *)data;
    YV12_BUFFER_CONFIG sd;

    image2yuvconfig(&frame->img, &sd);
    vp9_set_reference_enc(ctx->cpi, ref_frame_to_vp9_reframe(frame->frame_type),
                          &sd);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t vp9e_copy_reference(vpx_codec_alg_priv_t *ctx,
                                           int ctr_id,
                                           va_list args) {
  vpx_ref_frame_t *data = va_arg(args, vpx_ref_frame_t *);

  if (data) {
    vpx_ref_frame_t *frame = (vpx_ref_frame_t *)data;
    YV12_BUFFER_CONFIG sd;

    image2yuvconfig(&frame->img, &sd);
    vp9_copy_reference_enc(ctx->cpi,
                           ref_frame_to_vp9_reframe(frame->frame_type), &sd);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t get_reference(vpx_codec_alg_priv_t *ctx,
                                     int ctr_id,
                                     va_list args) {
  vp9_ref_frame_t *data = va_arg(args, vp9_ref_frame_t *);

  if (data) {
    YV12_BUFFER_CONFIG* fb;

    vp9_get_reference_enc(ctx->cpi, data->idx, &fb);
    yuvconfig2image(&data->img, fb, NULL);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t vp9e_set_previewpp(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id,
                                          va_list args) {
#if CONFIG_VP9_POSTPROC
  vp8_postproc_cfg_t *data = va_arg(args, vp8_postproc_cfg_t *);
  (void)ctr_id;

  if (data) {
    ctx->preview_ppcfg = *((vp8_postproc_cfg_t *)data);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
#else
  (void)ctx;
  (void)ctr_id;
  (void)args;
  return VPX_CODEC_INCAPABLE;
#endif
}


static vpx_image_t *vp9e_get_preview(vpx_codec_alg_priv_t *ctx) {
  YV12_BUFFER_CONFIG sd;
  vp9_ppflags_t flags = {0};

  if (ctx->preview_ppcfg.post_proc_flag) {
    flags.post_proc_flag        = ctx->preview_ppcfg.post_proc_flag;
    flags.deblocking_level      = ctx->preview_ppcfg.deblocking_level;
    flags.noise_level           = ctx->preview_ppcfg.noise_level;
  }

  if (0 == vp9_get_preview_raw_frame(ctx->cpi, &sd, &flags)) {
    yuvconfig2image(&ctx->preview_img, &sd, NULL);
    return &ctx->preview_img;
  } else {
    return NULL;
  }
}

static vpx_codec_err_t vp9e_update_entropy(vpx_codec_alg_priv_t *ctx,
                                           int ctr_id,
                                           va_list args) {
  int update = va_arg(args, int);
  vp9_update_entropy(ctx->cpi, update);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t vp9e_update_reference(vpx_codec_alg_priv_t *ctx,
                                             int ctr_id,
                                             va_list args) {
  int update = va_arg(args, int);
  vp9_update_reference(ctx->cpi, update);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t vp9e_use_reference(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id,
                                          va_list args) {
  int reference_flag = va_arg(args, int);
  vp9_use_as_reference(ctx->cpi, reference_flag);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t vp9e_set_roi_map(vpx_codec_alg_priv_t *ctx,
                                        int ctr_id,
                                        va_list args) {
  // TODO(yaowu): Need to re-implement and test for VP9.
  return VPX_CODEC_INVALID_PARAM;
}


static vpx_codec_err_t vp9e_set_activemap(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id,
                                          va_list args) {
  // TODO(yaowu): Need to re-implement and test for VP9.
  return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t vp9e_set_scalemode(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id,
                                          va_list args) {
  vpx_scaling_mode_t *data =  va_arg(args, vpx_scaling_mode_t *);

  if (data) {
    int res;
    vpx_scaling_mode_t scalemode = *(vpx_scaling_mode_t *)data;
    res = vp9_set_internal_size(ctx->cpi,
                                (VPX_SCALING)scalemode.h_scaling_mode,
                                (VPX_SCALING)scalemode.v_scaling_mode);

    if (!res) {
      return VPX_CODEC_OK;
    } else {
      return VPX_CODEC_INVALID_PARAM;
    }
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t vp9e_set_svc(vpx_codec_alg_priv_t *ctx, int ctr_id,
                                    va_list args) {
  int data = va_arg(args, int);
  vp9_set_svc(ctx->cpi, data);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t vp9e_set_svc_parameters(vpx_codec_alg_priv_t *ctx,
                                               int ctr_id, va_list args) {
  vpx_svc_parameters_t *data = va_arg(args, vpx_svc_parameters_t *);
  VP9_COMP *cpi = (VP9_COMP *)ctx->cpi;
  vpx_svc_parameters_t params;

  if (data == NULL) {
    return VPX_CODEC_INVALID_PARAM;
  }

  params = *(vpx_svc_parameters_t *)data;

  cpi->current_layer = params.layer;
  cpi->lst_fb_idx = params.lst_fb_idx;
  cpi->gld_fb_idx = params.gld_fb_idx;
  cpi->alt_fb_idx = params.alt_fb_idx;

  if (vp9_set_size_literal(ctx->cpi, params.width, params.height) != 0) {
    return VPX_CODEC_INVALID_PARAM;
  }

  ctx->cfg.rc_max_quantizer = params.max_quantizer;
  ctx->cfg.rc_min_quantizer = params.min_quantizer;

  set_vp9e_config(&ctx->oxcf, ctx->cfg, ctx->vp8_cfg);
  vp9_change_config(ctx->cpi, &ctx->oxcf);

  return VPX_CODEC_OK;
}

static vpx_codec_ctrl_fn_map_t vp9e_ctf_maps[] = {
  {VP8_SET_REFERENCE,                 vp9e_set_reference},
  {VP8_COPY_REFERENCE,                vp9e_copy_reference},
  {VP8_SET_POSTPROC,                  vp9e_set_previewpp},
  {VP8E_UPD_ENTROPY,                  vp9e_update_entropy},
  {VP8E_UPD_REFERENCE,                vp9e_update_reference},
  {VP8E_USE_REFERENCE,                vp9e_use_reference},
  {VP8E_SET_ROI_MAP,                  vp9e_set_roi_map},
  {VP8E_SET_ACTIVEMAP,                vp9e_set_activemap},
  {VP8E_SET_SCALEMODE,                vp9e_set_scalemode},
  {VP8E_SET_CPUUSED,                  set_param},
  {VP8E_SET_NOISE_SENSITIVITY,        set_param},
  {VP8E_SET_ENABLEAUTOALTREF,         set_param},
  {VP8E_SET_SHARPNESS,                set_param},
  {VP8E_SET_STATIC_THRESHOLD,         set_param},
  {VP9E_SET_TILE_COLUMNS,             set_param},
  {VP9E_SET_TILE_ROWS,                set_param},
  {VP8E_GET_LAST_QUANTIZER,           get_param},
  {VP8E_GET_LAST_QUANTIZER_64,        get_param},
  {VP8E_SET_ARNR_MAXFRAMES,           set_param},
  {VP8E_SET_ARNR_STRENGTH,            set_param},
  {VP8E_SET_ARNR_TYPE,                set_param},
  {VP8E_SET_TUNING,                   set_param},
  {VP8E_SET_CQ_LEVEL,                 set_param},
  {VP8E_SET_MAX_INTRA_BITRATE_PCT,    set_param},
  {VP9E_SET_LOSSLESS,                 set_param},
  {VP9E_SET_FRAME_PARALLEL_DECODING,  set_param},
  {VP9E_SET_AQ_MODE,                  set_param},
  {VP9_GET_REFERENCE,                 get_reference},
  {VP9E_SET_SVC,                      vp9e_set_svc},
  {VP9E_SET_SVC_PARAMETERS,           vp9e_set_svc_parameters},
  { -1, NULL},
};

static vpx_codec_enc_cfg_map_t vp9e_usage_cfg_map[] = {
  {
    0,
    {  // NOLINT
      0,                  /* g_usage */
      0,                  /* g_threads */
      0,                  /* g_profile */

      320,                /* g_width */
      240,                /* g_height */
      {1, 30},            /* g_timebase */

      0,                  /* g_error_resilient */

      VPX_RC_ONE_PASS,    /* g_pass */

      25,                 /* g_lag_in_frames */

      0,                  /* rc_dropframe_thresh */
      0,                  /* rc_resize_allowed */
      60,                 /* rc_resize_down_thresold */
      30,                 /* rc_resize_up_thresold */

      VPX_VBR,            /* rc_end_usage */
#if VPX_ENCODER_ABI_VERSION > (1 + VPX_CODEC_ABI_VERSION)
      {0},                /* rc_twopass_stats_in */
#endif
      256,                /* rc_target_bandwidth */
      0,                  /* rc_min_quantizer */
      63,                 /* rc_max_quantizer */
      100,                /* rc_undershoot_pct */
      100,                /* rc_overshoot_pct */

      6000,               /* rc_max_buffer_size */
      4000,               /* rc_buffer_initial_size; */
      5000,               /* rc_buffer_optimal_size; */

      50,                 /* rc_two_pass_vbrbias  */
      0,                  /* rc_two_pass_vbrmin_section */
      2000,               /* rc_two_pass_vbrmax_section */

      /* keyframing settings (kf) */
      VPX_KF_AUTO,        /* g_kfmode*/
      0,                  /* kf_min_dist */
      9999,               /* kf_max_dist */

      VPX_SS_DEFAULT_LAYERS, /* ss_number_layers */

#if VPX_ENCODER_ABI_VERSION == (1 + VPX_CODEC_ABI_VERSION)
      1,                  /* g_delete_first_pass_file */
      "vp8.fpf"           /* first pass filename */
#endif
    }
  },
  { -1, {NOT_IMPLEMENTED}}
};


#ifndef VERSION_STRING
#define VERSION_STRING
#endif
CODEC_INTERFACE(vpx_codec_vp9_cx) = {
  "WebM Project VP9 Encoder" VERSION_STRING,
  VPX_CODEC_INTERNAL_ABI_VERSION,
  VPX_CODEC_CAP_ENCODER | VPX_CODEC_CAP_PSNR |
  VPX_CODEC_CAP_OUTPUT_PARTITION,
  /* vpx_codec_caps_t          caps; */
  vp9e_init,          /* vpx_codec_init_fn_t       init; */
  vp9e_destroy,       /* vpx_codec_destroy_fn_t    destroy; */
  vp9e_ctf_maps,      /* vpx_codec_ctrl_fn_map_t  *ctrl_maps; */
  NOT_IMPLEMENTED,    /* vpx_codec_get_mmap_fn_t   get_mmap; */
  NOT_IMPLEMENTED,    /* vpx_codec_set_mmap_fn_t   set_mmap; */
  {  // NOLINT
    NOT_IMPLEMENTED,    /* vpx_codec_peek_si_fn_t    peek_si; */
    NOT_IMPLEMENTED,    /* vpx_codec_get_si_fn_t     get_si; */
    NOT_IMPLEMENTED,    /* vpx_codec_decode_fn_t     decode; */
    NOT_IMPLEMENTED,    /* vpx_codec_frame_get_fn_t  frame_get; */
  },
  {  // NOLINT
    vp9e_usage_cfg_map, /* vpx_codec_enc_cfg_map_t    peek_si; */
    vp9e_encode,        /* vpx_codec_encode_fn_t      encode; */
    vp9e_get_cxdata,    /* vpx_codec_get_cx_data_fn_t   frame_get; */
    vp9e_set_config,
    NOT_IMPLEMENTED,
    vp9e_get_preview,
  } /* encoder functions */
};


#if CONFIG_EXPERIMENTAL

CODEC_INTERFACE(vpx_codec_vp9x_cx) = {
  "VP8 Experimental Encoder" VERSION_STRING,
  VPX_CODEC_INTERNAL_ABI_VERSION,
  VPX_CODEC_CAP_ENCODER | VPX_CODEC_CAP_PSNR,
  /* vpx_codec_caps_t          caps; */
  vp9e_exp_init,      /* vpx_codec_init_fn_t       init; */
  vp9e_destroy,       /* vpx_codec_destroy_fn_t    destroy; */
  vp9e_ctf_maps,      /* vpx_codec_ctrl_fn_map_t  *ctrl_maps; */
  NOT_IMPLEMENTED,    /* vpx_codec_get_mmap_fn_t   get_mmap; */
  NOT_IMPLEMENTED,    /* vpx_codec_set_mmap_fn_t   set_mmap; */
  {  // NOLINT
    NOT_IMPLEMENTED,    /* vpx_codec_peek_si_fn_t    peek_si; */
    NOT_IMPLEMENTED,    /* vpx_codec_get_si_fn_t     get_si; */
    NOT_IMPLEMENTED,    /* vpx_codec_decode_fn_t     decode; */
    NOT_IMPLEMENTED,    /* vpx_codec_frame_get_fn_t  frame_get; */
  },
  {  // NOLINT
    vp9e_usage_cfg_map, /* vpx_codec_enc_cfg_map_t    peek_si; */
    vp9e_encode,        /* vpx_codec_encode_fn_t      encode; */
    vp9e_get_cxdata,    /* vpx_codec_get_cx_data_fn_t   frame_get; */
    vp9e_set_config,
    NOT_IMPLEMENTED,
    vp9e_get_preview,
  } /* encoder functions */
};
#endif
