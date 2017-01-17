/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_DENOISER_H_
#define VP9_ENCODER_DENOISER_H_

#include "vp9/encoder/vp9_block.h"
#include "vp9/encoder/vp9_skin_detection.h"
#include "vpx_scale/yv12config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_MAGNITUDE_THRESHOLD (8 * 3)

typedef enum vp9_denoiser_decision {
  COPY_BLOCK,
  FILTER_BLOCK,
  FILTER_ZEROMV_BLOCK
} VP9_DENOISER_DECISION;

typedef enum vp9_denoiser_level {
  kDenLowLow,
  kDenLow,
  kDenMedium,
  kDenHigh
} VP9_DENOISER_LEVEL;

typedef struct vp9_denoiser {
  YV12_BUFFER_CONFIG running_avg_y[MAX_REF_FRAMES];
  YV12_BUFFER_CONFIG mc_running_avg_y;
  YV12_BUFFER_CONFIG last_source;
  int increase_denoising;
  int frame_buffer_initialized;
  int reset;
  VP9_DENOISER_LEVEL denoising_level;
  VP9_DENOISER_LEVEL prev_denoising_level;
} VP9_DENOISER;

typedef struct {
  int64_t zero_last_cost_orig;
  int *ref_frame_cost;
  int_mv (*frame_mv)[MAX_REF_FRAMES];
  int reuse_inter_pred;
  TX_SIZE best_tx_size;
  PREDICTION_MODE best_mode;
  MV_REFERENCE_FRAME best_ref_frame;
  INTERP_FILTER best_pred_filter;
  uint8_t best_mode_skip_txfm;
} VP9_PICKMODE_CTX_DEN;

struct VP9_COMP;

void vp9_denoiser_update_frame_info(VP9_DENOISER *denoiser,
                                    YV12_BUFFER_CONFIG src,
                                    FRAME_TYPE frame_type,
                                    int refresh_alt_ref_frame,
                                    int refresh_golden_frame,
                                    int refresh_last_frame,
                                    int resized);

void vp9_denoiser_denoise(struct VP9_COMP *cpi, MACROBLOCK *mb,
                          int mi_row, int mi_col, BLOCK_SIZE bs,
                          PICK_MODE_CONTEXT *ctx ,
                          VP9_DENOISER_DECISION *denoiser_decision);

void vp9_denoiser_reset_frame_stats(PICK_MODE_CONTEXT *ctx);

void vp9_denoiser_update_frame_stats(MODE_INFO *mi,
                                     unsigned int sse, PREDICTION_MODE mode,
                                     PICK_MODE_CONTEXT *ctx);

int vp9_denoiser_alloc(VP9_DENOISER *denoiser, int width, int height,
                       int ssx, int ssy,
#if CONFIG_VP9_HIGHBITDEPTH
                       int use_highbitdepth,
#endif
                       int border);

#if CONFIG_VP9_TEMPORAL_DENOISING
// This function is used by both c and sse2 denoiser implementations.
// Define it as a static function within the scope where vp9_denoiser.h
// is referenced.
static INLINE int total_adj_strong_thresh(BLOCK_SIZE bs,
                                          int increase_denoising) {
  return (1 << num_pels_log2_lookup[bs]) * (increase_denoising ? 3 : 2);
}
#endif

void vp9_denoiser_free(VP9_DENOISER *denoiser);

void vp9_denoiser_set_noise_level(VP9_DENOISER *denoiser,
                                  int noise_level);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_DENOISER_H_
