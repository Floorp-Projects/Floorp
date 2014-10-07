/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/**
 * @file
 * VP9 SVC encoding support via libvpx
 */

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VPX_DISABLE_CTRL_TYPECHECKS 1
#define VPX_CODEC_DISABLE_COMPAT 1
#include "./vpx_config.h"
#include "vpx/svc_context.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#include "vpx_mem/vpx_mem.h"
#include "vp9/common/vp9_onyxc_int.h"

#ifdef __MINGW32__
#define strtok_r strtok_s
#ifndef MINGW_HAS_SECURE_API
// proto from /usr/x86_64-w64-mingw32/include/sec_api/string_s.h
_CRTIMP char *__cdecl strtok_s(char *str, const char *delim, char **context);
#endif  /* MINGW_HAS_SECURE_API */
#endif  /* __MINGW32__ */

#ifdef _MSC_VER
#define strdup _strdup
#define strtok_r strtok_s
#endif

#define SVC_REFERENCE_FRAMES 8
#define SUPERFRAME_SLOTS (8)
#define SUPERFRAME_BUFFER_SIZE (SUPERFRAME_SLOTS * sizeof(uint32_t) + 2)
#define OPTION_BUFFER_SIZE 256
#define COMPONENTS 4  // psnr & sse statistics maintained for total, y, u, v

static const char *DEFAULT_QUANTIZER_VALUES = "60,53,39,33,27";
static const char *DEFAULT_SCALE_FACTORS = "4/16,5/16,7/16,11/16,16/16";

// One encoded frame
typedef struct FrameData {
  void                     *buf;    // compressed data buffer
  size_t                    size;  // length of compressed data
  vpx_codec_frame_flags_t   flags;    /**< flags for this frame */
  struct FrameData         *next;
} FrameData;

typedef struct SvcInternal {
  char options[OPTION_BUFFER_SIZE];        // set by vpx_svc_set_options
  char quantizers[OPTION_BUFFER_SIZE];     // set by vpx_svc_set_quantizers
  char scale_factors[OPTION_BUFFER_SIZE];  // set by vpx_svc_set_scale_factors

  // values extracted from option, quantizers
  int scaling_factor_num[VPX_SS_MAX_LAYERS];
  int scaling_factor_den[VPX_SS_MAX_LAYERS];
  int quantizer[VPX_SS_MAX_LAYERS];
  int enable_auto_alt_ref[VPX_SS_MAX_LAYERS];

  // accumulated statistics
  double psnr_sum[VPX_SS_MAX_LAYERS][COMPONENTS];   // total/Y/U/V
  uint64_t sse_sum[VPX_SS_MAX_LAYERS][COMPONENTS];
  uint32_t bytes_sum[VPX_SS_MAX_LAYERS];

  // codec encoding values
  int width;    // width of highest layer
  int height;   // height of highest layer
  int kf_dist;  // distance between keyframes

  // state variables
  int encode_frame_count;
  int frame_received;
  int frame_within_gop;
  int layers;
  int layer;
  int is_keyframe;
  int use_multiple_frame_contexts;

  FrameData *frame_list;
  FrameData *frame_temp;

  char *rc_stats_buf;
  size_t rc_stats_buf_size;
  size_t rc_stats_buf_used;

  char message_buffer[2048];
  vpx_codec_ctx_t *codec_ctx;
} SvcInternal;

// create FrameData from encoder output
static struct FrameData *fd_create(void *buf, size_t size,
                                   vpx_codec_frame_flags_t flags) {
  struct FrameData *const frame_data =
      (struct FrameData *)vpx_malloc(sizeof(*frame_data));
  if (frame_data == NULL) {
    return NULL;
  }
  frame_data->buf = vpx_malloc(size);
  if (frame_data->buf == NULL) {
    vpx_free(frame_data);
    return NULL;
  }
  vpx_memcpy(frame_data->buf, buf, size);
  frame_data->size = size;
  frame_data->flags = flags;
  return frame_data;
}

// free FrameData
static void fd_free(struct FrameData *p) {
  if (p) {
    if (p->buf)
      vpx_free(p->buf);
    vpx_free(p);
  }
}

// add FrameData to list
static void fd_list_add(struct FrameData **list, struct FrameData *layer_data) {
  struct FrameData **p = list;

  while (*p != NULL) p = &(*p)->next;
  *p = layer_data;
  layer_data->next = NULL;
}

// free FrameData list
static void fd_free_list(struct FrameData *list) {
  struct FrameData *p = list;

  while (p) {
    list = list->next;
    fd_free(p);
    p = list;
  }
}

static SvcInternal *get_svc_internal(SvcContext *svc_ctx) {
  if (svc_ctx == NULL) return NULL;
  if (svc_ctx->internal == NULL) {
    SvcInternal *const si = (SvcInternal *)malloc(sizeof(*si));
    if (si != NULL) {
      memset(si, 0, sizeof(*si));
    }
    svc_ctx->internal = si;
  }
  return (SvcInternal *)svc_ctx->internal;
}

static const SvcInternal *get_const_svc_internal(const SvcContext *svc_ctx) {
  if (svc_ctx == NULL) return NULL;
  return (const SvcInternal *)svc_ctx->internal;
}

static void svc_log_reset(SvcContext *svc_ctx) {
  SvcInternal *const si = (SvcInternal *)svc_ctx->internal;
  si->message_buffer[0] = '\0';
}

static int svc_log(SvcContext *svc_ctx, SVC_LOG_LEVEL level,
                   const char *fmt, ...) {
  char buf[512];
  int retval = 0;
  va_list ap;
  SvcInternal *const si = get_svc_internal(svc_ctx);

  if (level > svc_ctx->log_level) {
    return retval;
  }

  va_start(ap, fmt);
  retval = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (svc_ctx->log_print) {
    printf("%s", buf);
  } else {
    strncat(si->message_buffer, buf,
            sizeof(si->message_buffer) - strlen(si->message_buffer) - 1);
  }

  if (level == SVC_LOG_ERROR) {
    si->codec_ctx->err_detail = si->message_buffer;
  }
  return retval;
}

static vpx_codec_err_t parse_quantizer_values(SvcContext *svc_ctx,
                                              const char *quantizer_values) {
  char *input_string;
  char *token;
  const char *delim = ",";
  char *save_ptr;
  int found = 0;
  int i, q;
  vpx_codec_err_t res = VPX_CODEC_OK;
  SvcInternal *const si = get_svc_internal(svc_ctx);

  if (quantizer_values == NULL || strlen(quantizer_values) == 0) {
    input_string = strdup(DEFAULT_QUANTIZER_VALUES);
  } else {
    input_string = strdup(quantizer_values);
  }

  token = strtok_r(input_string, delim, &save_ptr);
  for (i = 0; i < svc_ctx->spatial_layers; ++i) {
    if (token != NULL) {
      q = atoi(token);
      if (q <= 0 || q > 100) {
        svc_log(svc_ctx, SVC_LOG_ERROR,
                "svc-quantizer-values: invalid value %s\n", token);
        res = VPX_CODEC_INVALID_PARAM;
        break;
      }
      token = strtok_r(NULL, delim, &save_ptr);
      found = i + 1;
    } else {
      q = 0;
    }
    si->quantizer[i + VPX_SS_MAX_LAYERS - svc_ctx->spatial_layers] = q;
  }
  if (res == VPX_CODEC_OK && found != svc_ctx->spatial_layers) {
    svc_log(svc_ctx, SVC_LOG_ERROR,
            "svc: quantizers: %d values required, but only %d specified\n",
            svc_ctx->spatial_layers, found);
    res = VPX_CODEC_INVALID_PARAM;
  }
  free(input_string);
  return res;
}

static vpx_codec_err_t parse_auto_alt_ref(SvcContext *svc_ctx,
                                          const char *alt_ref_options) {
  char *input_string;
  char *token;
  const char *delim = ",";
  char *save_ptr;
  int found = 0, enabled = 0;
  int i, value;
  vpx_codec_err_t res = VPX_CODEC_OK;
  SvcInternal *const si = get_svc_internal(svc_ctx);

  if (alt_ref_options == NULL || strlen(alt_ref_options) == 0) {
    return VPX_CODEC_INVALID_PARAM;
  } else {
    input_string = strdup(alt_ref_options);
  }

  token = strtok_r(input_string, delim, &save_ptr);
  for (i = 0; i < svc_ctx->spatial_layers; ++i) {
    if (token != NULL) {
      value = atoi(token);
      if (value < 0 || value > 1) {
        svc_log(svc_ctx, SVC_LOG_ERROR,
                "enable auto alt ref values: invalid value %s\n", token);
        res = VPX_CODEC_INVALID_PARAM;
        break;
      }
      token = strtok_r(NULL, delim, &save_ptr);
      found = i + 1;
    } else {
      value = 0;
    }
    si->enable_auto_alt_ref[i] = value;
    if (value > 0)
      ++enabled;
  }
  if (res == VPX_CODEC_OK && found != svc_ctx->spatial_layers) {
    svc_log(svc_ctx, SVC_LOG_ERROR,
            "svc: quantizers: %d values required, but only %d specified\n",
            svc_ctx->spatial_layers, found);
    res = VPX_CODEC_INVALID_PARAM;
  }
  if (enabled > REF_FRAMES - svc_ctx->spatial_layers) {
    svc_log(svc_ctx, SVC_LOG_ERROR,
            "svc: auto alt ref: Maxinum %d(REF_FRAMES - layers) layers could"
            "enabled auto alt reference frame, but % layers are enabled\n",
            REF_FRAMES - svc_ctx->spatial_layers, enabled);
    res = VPX_CODEC_INVALID_PARAM;
  }
  free(input_string);
  return res;
}

static void log_invalid_scale_factor(SvcContext *svc_ctx, const char *value) {
  svc_log(svc_ctx, SVC_LOG_ERROR, "svc scale-factors: invalid value %s\n",
          value);
}

static vpx_codec_err_t parse_scale_factors(SvcContext *svc_ctx,
                                           const char *scale_factors) {
  char *input_string;
  char *token;
  const char *delim = ",";
  char *save_ptr;
  int found = 0;
  int i;
  int64_t num, den;
  vpx_codec_err_t res = VPX_CODEC_OK;
  SvcInternal *const si = get_svc_internal(svc_ctx);

  if (scale_factors == NULL || strlen(scale_factors) == 0) {
    input_string = strdup(DEFAULT_SCALE_FACTORS);
  } else {
    input_string = strdup(scale_factors);
  }
  token = strtok_r(input_string, delim, &save_ptr);
  for (i = 0; i < svc_ctx->spatial_layers; ++i) {
    num = den = 0;
    if (token != NULL) {
      num = strtol(token, &token, 10);
      if (num <= 0) {
        log_invalid_scale_factor(svc_ctx, token);
        res = VPX_CODEC_INVALID_PARAM;
        break;
      }
      if (*token++ != '/') {
        log_invalid_scale_factor(svc_ctx, token);
        res = VPX_CODEC_INVALID_PARAM;
        break;
      }
      den = strtol(token, &token, 10);
      if (den <= 0) {
        log_invalid_scale_factor(svc_ctx, token);
        res = VPX_CODEC_INVALID_PARAM;
        break;
      }
      token = strtok_r(NULL, delim, &save_ptr);
      found = i + 1;
    }
    si->scaling_factor_num[i + VPX_SS_MAX_LAYERS - svc_ctx->spatial_layers] =
        (int)num;
    si->scaling_factor_den[i + VPX_SS_MAX_LAYERS - svc_ctx->spatial_layers] =
        (int)den;
  }
  if (res == VPX_CODEC_OK && found != svc_ctx->spatial_layers) {
    svc_log(svc_ctx, SVC_LOG_ERROR,
            "svc: scale-factors: %d values required, but only %d specified\n",
            svc_ctx->spatial_layers, found);
    res = VPX_CODEC_INVALID_PARAM;
  }
  free(input_string);
  return res;
}

/**
 * Parse SVC encoding options
 * Format: encoding-mode=<svc_mode>,layers=<layer_count>
 *         scale-factors=<n1>/<d1>,<n2>/<d2>,...
 *         quantizers=<q1>,<q2>,...
 * svc_mode = [i|ip|alt_ip|gf]
 */
static vpx_codec_err_t parse_options(SvcContext *svc_ctx, const char *options) {
  char *input_string;
  char *option_name;
  char *option_value;
  char *input_ptr;
  SvcInternal *const si = get_svc_internal(svc_ctx);
  vpx_codec_err_t res = VPX_CODEC_OK;

  if (options == NULL) return VPX_CODEC_OK;
  input_string = strdup(options);

  // parse option name
  option_name = strtok_r(input_string, "=", &input_ptr);
  while (option_name != NULL) {
    // parse option value
    option_value = strtok_r(NULL, " ", &input_ptr);
    if (option_value == NULL) {
      svc_log(svc_ctx, SVC_LOG_ERROR, "option missing value: %s\n",
              option_name);
      res = VPX_CODEC_INVALID_PARAM;
      break;
    }
    if (strcmp("spatial-layers", option_name) == 0) {
      svc_ctx->spatial_layers = atoi(option_value);
    } else if (strcmp("temporal-layers", option_name) == 0) {
      svc_ctx->temporal_layers = atoi(option_value);
    } else if (strcmp("scale-factors", option_name) == 0) {
      res = parse_scale_factors(svc_ctx, option_value);
      if (res != VPX_CODEC_OK) break;
    } else if (strcmp("quantizers", option_name) == 0) {
      res = parse_quantizer_values(svc_ctx, option_value);
      if (res != VPX_CODEC_OK) break;
    } else if (strcmp("auto-alt-refs", option_name) == 0) {
      res = parse_auto_alt_ref(svc_ctx, option_value);
      if (res != VPX_CODEC_OK) break;
    } else if (strcmp("multi-frame-contexts", option_name) == 0) {
      si->use_multiple_frame_contexts = atoi(option_value);
    } else {
      svc_log(svc_ctx, SVC_LOG_ERROR, "invalid option: %s\n", option_name);
      res = VPX_CODEC_INVALID_PARAM;
      break;
    }
    option_name = strtok_r(NULL, "=", &input_ptr);
  }
  free(input_string);

  if (si->use_multiple_frame_contexts &&
      (svc_ctx->spatial_layers > 3 ||
       svc_ctx->spatial_layers * svc_ctx->temporal_layers > 4))
    res = VPX_CODEC_INVALID_PARAM;

  return res;
}

vpx_codec_err_t vpx_svc_set_options(SvcContext *svc_ctx, const char *options) {
  SvcInternal *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || options == NULL || si == NULL) {
    return VPX_CODEC_INVALID_PARAM;
  }
  strncpy(si->options, options, sizeof(si->options));
  si->options[sizeof(si->options) - 1] = '\0';
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_svc_set_quantizers(SvcContext *svc_ctx,
                                       const char *quantizers) {
  SvcInternal *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || quantizers == NULL || si == NULL) {
    return VPX_CODEC_INVALID_PARAM;
  }
  strncpy(si->quantizers, quantizers, sizeof(si->quantizers));
  si->quantizers[sizeof(si->quantizers) - 1] = '\0';
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_svc_set_scale_factors(SvcContext *svc_ctx,
                                          const char *scale_factors) {
  SvcInternal *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || scale_factors == NULL || si == NULL) {
    return VPX_CODEC_INVALID_PARAM;
  }
  strncpy(si->scale_factors, scale_factors, sizeof(si->scale_factors));
  si->scale_factors[sizeof(si->scale_factors) - 1] = '\0';
  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_svc_init(SvcContext *svc_ctx, vpx_codec_ctx_t *codec_ctx,
                             vpx_codec_iface_t *iface,
                             vpx_codec_enc_cfg_t *enc_cfg) {
  vpx_codec_err_t res;
  int i;
  SvcInternal *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || codec_ctx == NULL || iface == NULL ||
      enc_cfg == NULL) {
    return VPX_CODEC_INVALID_PARAM;
  }
  if (si == NULL) return VPX_CODEC_MEM_ERROR;

  si->codec_ctx = codec_ctx;

  si->width = enc_cfg->g_w;
  si->height = enc_cfg->g_h;

  if (enc_cfg->kf_max_dist < 2) {
    svc_log(svc_ctx, SVC_LOG_ERROR, "key frame distance too small: %d\n",
            enc_cfg->kf_max_dist);
    return VPX_CODEC_INVALID_PARAM;
  }
  si->kf_dist = enc_cfg->kf_max_dist;

  if (svc_ctx->spatial_layers == 0)
    svc_ctx->spatial_layers = VPX_SS_DEFAULT_LAYERS;
  if (svc_ctx->spatial_layers < 1 ||
      svc_ctx->spatial_layers > VPX_SS_MAX_LAYERS) {
    svc_log(svc_ctx, SVC_LOG_ERROR, "spatial layers: invalid value: %d\n",
            svc_ctx->spatial_layers);
    return VPX_CODEC_INVALID_PARAM;
  }

  res = parse_quantizer_values(svc_ctx, si->quantizers);
  if (res != VPX_CODEC_OK) return res;

  res = parse_scale_factors(svc_ctx, si->scale_factors);
  if (res != VPX_CODEC_OK) return res;

  // Parse aggregate command line options. Options must start with
  // "layers=xx" then followed by other options
  res = parse_options(svc_ctx, si->options);
  if (res != VPX_CODEC_OK) return res;

  if (svc_ctx->spatial_layers < 1)
    svc_ctx->spatial_layers = 1;
  if (svc_ctx->spatial_layers > VPX_SS_MAX_LAYERS)
    svc_ctx->spatial_layers = VPX_SS_MAX_LAYERS;

  if (svc_ctx->temporal_layers < 1)
    svc_ctx->temporal_layers = 1;
  if (svc_ctx->temporal_layers > VPX_TS_MAX_LAYERS)
    svc_ctx->temporal_layers = VPX_TS_MAX_LAYERS;

  si->layers = svc_ctx->spatial_layers;

  // Assign target bitrate for each layer. We calculate the ratio
  // from the resolution for now.
  // TODO(Minghai): Optimize the mechanism of allocating bits after
  // implementing svc two pass rate control.
  if (si->layers > 1) {
    float total = 0;
    float alloc_ratio[VPX_SS_MAX_LAYERS] = {0};

    assert(si->layers <= VPX_SS_MAX_LAYERS);
    for (i = 0; i < si->layers; ++i) {
      int pos = i + VPX_SS_MAX_LAYERS - svc_ctx->spatial_layers;
      if (pos < VPX_SS_MAX_LAYERS && si->scaling_factor_den[pos] > 0) {
        alloc_ratio[i] = (float)(si->scaling_factor_num[pos] * 1.0 /
            si->scaling_factor_den[pos]);

        alloc_ratio[i] *= alloc_ratio[i];
        total += alloc_ratio[i];
      }
    }

    for (i = 0; i < si->layers; ++i) {
      if (total > 0) {
        enc_cfg->ss_target_bitrate[i] = (unsigned int)
            (enc_cfg->rc_target_bitrate * alloc_ratio[i] / total);
      }
    }
  }

#if CONFIG_SPATIAL_SVC
  for (i = 0; i < si->layers; ++i)
    enc_cfg->ss_enable_auto_alt_ref[i] = si->enable_auto_alt_ref[i];
#endif

  if (svc_ctx->temporal_layers > 1) {
    int i;
    for (i = 0; i < svc_ctx->temporal_layers; ++i) {
      enc_cfg->ts_target_bitrate[i] = enc_cfg->rc_target_bitrate /
                                      svc_ctx->temporal_layers;
      enc_cfg->ts_rate_decimator[i] = 1 << (svc_ctx->temporal_layers - 1 - i);
    }
  }

  // modify encoder configuration
  enc_cfg->ss_number_layers = si->layers;
  enc_cfg->ts_number_layers = svc_ctx->temporal_layers;

  // TODO(ivanmaltz): determine if these values need to be set explicitly for
  // svc, or if the normal default/override mechanism can be used
  enc_cfg->rc_dropframe_thresh = 0;
  enc_cfg->rc_resize_allowed = 0;

  if (enc_cfg->g_pass == VPX_RC_ONE_PASS) {
    enc_cfg->rc_min_quantizer = 33;
    enc_cfg->rc_max_quantizer = 33;
  }

  enc_cfg->rc_undershoot_pct = 100;
  enc_cfg->rc_overshoot_pct = 15;
  enc_cfg->rc_buf_initial_sz = 500;
  enc_cfg->rc_buf_optimal_sz = 600;
  enc_cfg->rc_buf_sz = 1000;
  if (enc_cfg->g_error_resilient == 0 && si->use_multiple_frame_contexts == 0)
    enc_cfg->g_error_resilient = 1;

  // Initialize codec
  res = vpx_codec_enc_init(codec_ctx, iface, enc_cfg, VPX_CODEC_USE_PSNR);
  if (res != VPX_CODEC_OK) {
    svc_log(svc_ctx, SVC_LOG_ERROR, "svc_enc_init error\n");
    return res;
  }

  vpx_codec_control(codec_ctx, VP9E_SET_SVC, 1);
  vpx_codec_control(codec_ctx, VP8E_SET_TOKEN_PARTITIONS, 1);

  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_svc_get_layer_resolution(const SvcContext *svc_ctx,
                                             int layer,
                                             unsigned int *width,
                                             unsigned int *height) {
  int w, h, index, num, den;
  const SvcInternal *const si = get_const_svc_internal(svc_ctx);

  if (svc_ctx == NULL || si == NULL || width == NULL || height == NULL) {
    return VPX_CODEC_INVALID_PARAM;
  }
  if (layer < 0 || layer >= si->layers) return VPX_CODEC_INVALID_PARAM;

  index = layer + VPX_SS_MAX_LAYERS - si->layers;
  num = si->scaling_factor_num[index];
  den = si->scaling_factor_den[index];
  if (num == 0 || den == 0) return VPX_CODEC_INVALID_PARAM;

  w = si->width * num / den;
  h = si->height * num / den;

  // make height and width even to make chrome player happy
  w += w % 2;
  h += h % 2;

  *width = w;
  *height = h;

  return VPX_CODEC_OK;
}

static void set_svc_parameters(SvcContext *svc_ctx,
                               vpx_codec_ctx_t *codec_ctx) {
  int layer, layer_index;
  vpx_svc_parameters_t svc_params;
  SvcInternal *const si = get_svc_internal(svc_ctx);

  memset(&svc_params, 0, sizeof(svc_params));
  svc_params.temporal_layer = 0;
  svc_params.spatial_layer = si->layer;

  layer = si->layer;
  if (VPX_CODEC_OK != vpx_svc_get_layer_resolution(svc_ctx, layer,
                                                   &svc_params.width,
                                                   &svc_params.height)) {
    svc_log(svc_ctx, SVC_LOG_ERROR, "vpx_svc_get_layer_resolution failed\n");
  }
  layer_index = layer + VPX_SS_MAX_LAYERS - si->layers;

  if (codec_ctx->config.enc->g_pass == VPX_RC_ONE_PASS) {
    svc_params.min_quantizer = si->quantizer[layer_index];
    svc_params.max_quantizer = si->quantizer[layer_index];
  } else {
    svc_params.min_quantizer = codec_ctx->config.enc->rc_min_quantizer;
    svc_params.max_quantizer = codec_ctx->config.enc->rc_max_quantizer;
  }

  svc_params.distance_from_i_frame = si->frame_within_gop;
  vpx_codec_control(codec_ctx, VP9E_SET_SVC_PARAMETERS, &svc_params);
}

/**
 * Encode a frame into multiple layers
 * Create a superframe containing the individual layers
 */
vpx_codec_err_t vpx_svc_encode(SvcContext *svc_ctx, vpx_codec_ctx_t *codec_ctx,
                               struct vpx_image *rawimg, vpx_codec_pts_t pts,
                               int64_t duration, int deadline) {
  vpx_codec_err_t res;
  vpx_codec_iter_t iter;
  const vpx_codec_cx_pkt_t *cx_pkt;
  int layer_for_psnr = 0;
  SvcInternal *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || codec_ctx == NULL || si == NULL) {
    return VPX_CODEC_INVALID_PARAM;
  }

  svc_log_reset(svc_ctx);
  si->rc_stats_buf_used = 0;

  si->layers = svc_ctx->spatial_layers;
  if (si->encode_frame_count == 0) {
    si->frame_within_gop = 0;
  }
  si->is_keyframe = (si->frame_within_gop == 0);

  if (rawimg != NULL) {
    svc_log(svc_ctx, SVC_LOG_DEBUG,
            "vpx_svc_encode  layers: %d, frame_count: %d, "
            "frame_within_gop: %d\n", si->layers, si->encode_frame_count,
            si->frame_within_gop);
  }

  if (rawimg != NULL) {
    // encode each layer
    for (si->layer = 0; si->layer < si->layers; ++si->layer) {
      set_svc_parameters(svc_ctx, codec_ctx);
    }
  }

  res = vpx_codec_encode(codec_ctx, rawimg, pts, (uint32_t)duration, 0,
                         deadline);
  if (res != VPX_CODEC_OK) {
    return res;
  }
  // save compressed data
  iter = NULL;
  while ((cx_pkt = vpx_codec_get_cx_data(codec_ctx, &iter))) {
    switch (cx_pkt->kind) {
      case VPX_CODEC_CX_FRAME_PKT: {
        fd_list_add(&si->frame_list, fd_create(cx_pkt->data.frame.buf,
                                               cx_pkt->data.frame.sz,
                                               cx_pkt->data.frame.flags));

        svc_log(svc_ctx, SVC_LOG_DEBUG, "SVC frame: %d, kf: %d, size: %d, "
                "pts: %d\n", si->frame_received,
                (cx_pkt->data.frame.flags & VPX_FRAME_IS_KEY) ? 1 : 0,
                (int)cx_pkt->data.frame.sz, (int)cx_pkt->data.frame.pts);

        ++si->frame_received;
        layer_for_psnr = 0;
        break;
      }
      case VPX_CODEC_PSNR_PKT: {
        int i;
        svc_log(svc_ctx, SVC_LOG_DEBUG,
                "SVC frame: %d, layer: %d, PSNR(Total/Y/U/V): "
                "%2.3f  %2.3f  %2.3f  %2.3f \n",
                si->frame_received, layer_for_psnr,
                cx_pkt->data.psnr.psnr[0], cx_pkt->data.psnr.psnr[1],
                cx_pkt->data.psnr.psnr[2], cx_pkt->data.psnr.psnr[3]);
        svc_log(svc_ctx, SVC_LOG_DEBUG,
                "SVC frame: %d, layer: %d, SSE(Total/Y/U/V): "
                "%2.3f  %2.3f  %2.3f  %2.3f \n",
                si->frame_received, layer_for_psnr,
                cx_pkt->data.psnr.sse[0], cx_pkt->data.psnr.sse[1],
                cx_pkt->data.psnr.sse[2], cx_pkt->data.psnr.sse[3]);
        for (i = 0; i < COMPONENTS; i++) {
          si->psnr_sum[layer_for_psnr][i] += cx_pkt->data.psnr.psnr[i];
          si->sse_sum[layer_for_psnr][i] += cx_pkt->data.psnr.sse[i];
        }
        ++layer_for_psnr;
        break;
      }
      case VPX_CODEC_STATS_PKT: {
        size_t new_size = si->rc_stats_buf_used +
            cx_pkt->data.twopass_stats.sz;

        if (new_size > si->rc_stats_buf_size) {
          char *p = (char*)realloc(si->rc_stats_buf, new_size);
          if (p == NULL) {
            svc_log(svc_ctx, SVC_LOG_ERROR, "Error allocating stats buf\n");
            return VPX_CODEC_MEM_ERROR;
          }
          si->rc_stats_buf = p;
          si->rc_stats_buf_size = new_size;
        }

        memcpy(si->rc_stats_buf + si->rc_stats_buf_used,
               cx_pkt->data.twopass_stats.buf, cx_pkt->data.twopass_stats.sz);
        si->rc_stats_buf_used += cx_pkt->data.twopass_stats.sz;
        break;
      }
#if CONFIG_SPATIAL_SVC
      case VPX_CODEC_SPATIAL_SVC_LAYER_SIZES: {
        int i;
        for (i = 0; i < si->layers; ++i)
          si->bytes_sum[i] += cx_pkt->data.layer_sizes[i];
        break;
      }
#endif
      default: {
        break;
      }
    }
  }

  if (rawimg != NULL) {
    ++si->frame_within_gop;
    ++si->encode_frame_count;
  }

  return VPX_CODEC_OK;
}

const char *vpx_svc_get_message(const SvcContext *svc_ctx) {
  const SvcInternal *const si = get_const_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL) return NULL;
  return si->message_buffer;
}

// We will maintain a list of output frame buffers since with lag_in_frame
// we need to output all frame buffers at the end. vpx_svc_get_buffer() will
// remove a frame buffer from the list the put it to a temporal pointer, which
// will be removed at the next vpx_svc_get_buffer() or when closing encoder.
void *vpx_svc_get_buffer(SvcContext *svc_ctx) {
  SvcInternal *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL || si->frame_list == NULL) return NULL;

  if (si->frame_temp)
    fd_free(si->frame_temp);

  si->frame_temp = si->frame_list;
  si->frame_list = si->frame_list->next;

  return si->frame_temp->buf;
}

size_t vpx_svc_get_frame_size(const SvcContext *svc_ctx) {
  const SvcInternal *const si = get_const_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL || si->frame_list == NULL) return 0;
  return si->frame_list->size;
}

int vpx_svc_get_encode_frame_count(const SvcContext *svc_ctx) {
  const SvcInternal *const si = get_const_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL) return 0;
  return si->encode_frame_count;
}

int vpx_svc_is_keyframe(const SvcContext *svc_ctx) {
  const SvcInternal *const si = get_const_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL || si->frame_list == NULL) return 0;
  return (si->frame_list->flags & VPX_FRAME_IS_KEY) != 0;
}

void vpx_svc_set_keyframe(SvcContext *svc_ctx) {
  SvcInternal *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL) return;
  si->frame_within_gop = 0;
}

static double calc_psnr(double d) {
  if (d == 0) return 100;
  return -10.0 * log(d) / log(10.0);
}

// dump accumulated statistics and reset accumulated values
const char *vpx_svc_dump_statistics(SvcContext *svc_ctx) {
  int number_of_frames, encode_frame_count;
  int i, j;
  uint32_t bytes_total = 0;
  double scale[COMPONENTS];
  double psnr[COMPONENTS];
  double mse[COMPONENTS];
  double y_scale;

  SvcInternal *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL) return NULL;

  svc_log_reset(svc_ctx);

  encode_frame_count = si->encode_frame_count;
  if (si->encode_frame_count <= 0) return vpx_svc_get_message(svc_ctx);

  svc_log(svc_ctx, SVC_LOG_INFO, "\n");
  for (i = 0; i < si->layers; ++i) {
    number_of_frames = encode_frame_count;

    svc_log(svc_ctx, SVC_LOG_INFO,
            "Layer %d Average PSNR=[%2.3f, %2.3f, %2.3f, %2.3f], Bytes=[%u]\n",
            i, (double)si->psnr_sum[i][0] / number_of_frames,
            (double)si->psnr_sum[i][1] / number_of_frames,
            (double)si->psnr_sum[i][2] / number_of_frames,
            (double)si->psnr_sum[i][3] / number_of_frames, si->bytes_sum[i]);
    // the following psnr calculation is deduced from ffmpeg.c#print_report
    y_scale = si->width * si->height * 255.0 * 255.0 * number_of_frames;
    scale[1] = y_scale;
    scale[2] = scale[3] = y_scale / 4;  // U or V
    scale[0] = y_scale * 1.5;           // total

    for (j = 0; j < COMPONENTS; j++) {
      psnr[j] = calc_psnr(si->sse_sum[i][j] / scale[j]);
      mse[j] = si->sse_sum[i][j] * 255.0 * 255.0 / scale[j];
    }
    svc_log(svc_ctx, SVC_LOG_INFO,
            "Layer %d Overall PSNR=[%2.3f, %2.3f, %2.3f, %2.3f]\n", i, psnr[0],
            psnr[1], psnr[2], psnr[3]);
    svc_log(svc_ctx, SVC_LOG_INFO,
            "Layer %d Overall MSE=[%2.3f, %2.3f, %2.3f, %2.3f]\n", i, mse[0],
            mse[1], mse[2], mse[3]);

    bytes_total += si->bytes_sum[i];
    // clear sums for next time
    si->bytes_sum[i] = 0;
    for (j = 0; j < COMPONENTS; ++j) {
      si->psnr_sum[i][j] = 0;
      si->sse_sum[i][j] = 0;
    }
  }

  // only display statistics once
  si->encode_frame_count = 0;

  svc_log(svc_ctx, SVC_LOG_INFO, "Total Bytes=[%u]\n", bytes_total);
  return vpx_svc_get_message(svc_ctx);
}

void vpx_svc_release(SvcContext *svc_ctx) {
  SvcInternal *si;
  if (svc_ctx == NULL) return;
  // do not use get_svc_internal as it will unnecessarily allocate an
  // SvcInternal if it was not already allocated
  si = (SvcInternal *)svc_ctx->internal;
  if (si != NULL) {
    fd_free(si->frame_temp);
    fd_free_list(si->frame_list);
    if (si->rc_stats_buf) {
      free(si->rc_stats_buf);
    }
    free(si);
    svc_ctx->internal = NULL;
  }
}

size_t vpx_svc_get_rc_stats_buffer_size(const SvcContext *svc_ctx) {
  const SvcInternal *const si = get_const_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL) return 0;
  return si->rc_stats_buf_used;
}

char *vpx_svc_get_rc_stats_buffer(const SvcContext *svc_ctx) {
  const SvcInternal *const si = get_const_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL) return NULL;
  return si->rc_stats_buf;
}


