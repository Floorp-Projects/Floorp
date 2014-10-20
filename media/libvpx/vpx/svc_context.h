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
 * SvcContext - input parameters and state to encode a multi-layered
 * spatial SVC frame
 */

#ifndef VPX_SVC_CONTEXT_H_
#define VPX_SVC_CONTEXT_H_

#include "./vp8cx.h"
#include "./vpx_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SVC_LOG_LEVEL {
  SVC_LOG_ERROR,
  SVC_LOG_INFO,
  SVC_LOG_DEBUG
} SVC_LOG_LEVEL;

typedef struct {
  // public interface to svc_command options
  int spatial_layers;               // number of spatial layers
  int temporal_layers;               // number of temporal layers
  SVC_LOG_LEVEL log_level;  // amount of information to display
  int log_print;  // when set, printf log messages instead of returning the
                  // message with svc_get_message

  // private storage for vpx_svc_encode
  void *internal;
} SvcContext;

/**
 * Set SVC options
 * options are supplied as a single string separated by spaces
 * Format: encoding-mode=<i|ip|alt-ip|gf>
 *         layers=<layer_count>
 *         scaling-factors=<n1>/<d1>,<n2>/<d2>,...
 *         quantizers=<q1>,<q2>,...
 */
vpx_codec_err_t vpx_svc_set_options(SvcContext *svc_ctx, const char *options);

/**
 * Set SVC quantizer values
 * values comma separated, ordered from lowest resolution to highest
 * e.g., "60,53,39,33,27"
 */
vpx_codec_err_t vpx_svc_set_quantizers(SvcContext *svc_ctx,
                                       const char *quantizer_values);

/**
 * Set SVC scale factors
 * values comma separated, ordered from lowest resolution to highest
 * e.g.,  "4/16,5/16,7/16,11/16,16/16"
 */
vpx_codec_err_t vpx_svc_set_scale_factors(SvcContext *svc_ctx,
                                          const char *scale_factors);

/**
 * initialize SVC encoding
 */
vpx_codec_err_t vpx_svc_init(SvcContext *svc_ctx, vpx_codec_ctx_t *codec_ctx,
                             vpx_codec_iface_t *iface,
                             vpx_codec_enc_cfg_t *cfg);
/**
 * encode a frame of video with multiple layers
 */
vpx_codec_err_t vpx_svc_encode(SvcContext *svc_ctx, vpx_codec_ctx_t *codec_ctx,
                               struct vpx_image *rawimg, vpx_codec_pts_t pts,
                               int64_t duration, int deadline);

/**
 * finished with svc encoding, release allocated resources
 */
void vpx_svc_release(SvcContext *svc_ctx);

/**
 * dump accumulated statistics and reset accumulated values
 */
const char *vpx_svc_dump_statistics(SvcContext *svc_ctx);

/**
 *  get status message from previous encode
 */
const char *vpx_svc_get_message(const SvcContext *svc_ctx);

/**
 * return size of encoded data to be returned by vpx_svc_get_buffer.
 * it needs to be called before vpx_svc_get_buffer.
 */
size_t vpx_svc_get_frame_size(const SvcContext *svc_ctx);

/**
 * return buffer with encoded data. encoder will maintain a list of frame
 * buffers. each call of vpx_svc_get_buffer() will return one frame.
 */
void *vpx_svc_get_buffer(SvcContext *svc_ctx);

/**
 * return size of two pass rate control stats data to be returned by
 * vpx_svc_get_rc_stats_buffer
 */
size_t vpx_svc_get_rc_stats_buffer_size(const SvcContext *svc_ctx);

/**
 * return buffer two pass of rate control stats data
 */
char *vpx_svc_get_rc_stats_buffer(const SvcContext *svc_ctx);

/**
 * return spatial resolution of the specified layer
 */
vpx_codec_err_t vpx_svc_get_layer_resolution(const SvcContext *svc_ctx,
                                             int layer,
                                             unsigned int *width,
                                             unsigned int *height);
/**
 * return number of frames that have been encoded
 */
int vpx_svc_get_encode_frame_count(const SvcContext *svc_ctx);

/**
 * return 1 if last encoded frame was a keyframe
 */
int vpx_svc_is_keyframe(const SvcContext *svc_ctx);

/**
 * force the next frame to be a keyframe
 */
void vpx_svc_set_keyframe(SvcContext *svc_ctx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_SVC_CONTEXT_H_
