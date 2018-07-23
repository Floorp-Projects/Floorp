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

#ifndef AV1_ENCODER_FIRSTPASS_H_
#define AV1_ENCODER_FIRSTPASS_H_

#include "av1/common/enums.h"
#include "av1/common/onyxc_int.h"
#include "av1/encoder/lookahead.h"
#include "av1/encoder/ratectrl.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_FP_MB_STATS

#define FPMB_DCINTRA_MASK 0x01

#define FPMB_MOTION_ZERO_MASK 0x02
#define FPMB_MOTION_LEFT_MASK 0x04
#define FPMB_MOTION_RIGHT_MASK 0x08
#define FPMB_MOTION_UP_MASK 0x10
#define FPMB_MOTION_DOWN_MASK 0x20

#define FPMB_ERROR_SMALL_MASK 0x40
#define FPMB_ERROR_LARGE_MASK 0x80
#define FPMB_ERROR_SMALL_TH 2000
#define FPMB_ERROR_LARGE_TH 48000

typedef struct {
  uint8_t *mb_stats_start;
  uint8_t *mb_stats_end;
} FIRSTPASS_MB_STATS;
#endif

// Length of the bi-predictive frame group (BFG)
// NOTE: Currently each BFG contains one backward ref (BWF) frame plus a certain
//       number of bi-predictive frames.
#define BFG_INTERVAL 2
// The maximum number of extra ALTREF's except ALTREF_FRAME
// NOTE: REF_FRAMES indicates the maximum number of frames that may be buffered
//       to serve as references. Currently REF_FRAMES == 8.
#define USE_GF16_MULTI_LAYER 0

#if USE_GF16_MULTI_LAYER
#define MAX_EXT_ARFS (REF_FRAMES - BWDREF_FRAME)
#else  // !USE_GF16_MULTI_LAYER
#define MAX_EXT_ARFS (REF_FRAMES - BWDREF_FRAME - 1)
#endif  // USE_GF16_MULTI_LAYER

#define MIN_EXT_ARF_INTERVAL 4

#define MIN_ZERO_MOTION 0.95
#define MAX_SR_CODED_ERROR 40
#define MAX_RAW_ERR_VAR 2000
#define MIN_MV_IN_OUT 0.4

#define VLOW_MOTION_THRESHOLD 950

typedef struct {
  double frame;
  double weight;
  double intra_error;
  double frame_avg_wavelet_energy;
  double coded_error;
  double sr_coded_error;
  double pcnt_inter;
  double pcnt_motion;
  double pcnt_second_ref;
  double pcnt_neutral;
  double intra_skip_pct;
  double inactive_zone_rows;  // Image mask rows top and bottom.
  double inactive_zone_cols;  // Image mask columns at left and right edges.
  double MVr;
  double mvr_abs;
  double MVc;
  double mvc_abs;
  double MVrv;
  double MVcv;
  double mv_in_out_count;
  double new_mv_count;
  double duration;
  double count;
  // standard deviation for (0, 0) motion prediction error
  double raw_error_stdev;
} FIRSTPASS_STATS;

typedef enum {
  KF_UPDATE = 0,
  LF_UPDATE = 1,
  GF_UPDATE = 2,
  ARF_UPDATE = 3,
  OVERLAY_UPDATE = 4,
  BRF_UPDATE = 5,            // Backward Reference Frame
  LAST_BIPRED_UPDATE = 6,    // Last Bi-predictive Frame
  BIPRED_UPDATE = 7,         // Bi-predictive Frame, but not the last one
  INTNL_OVERLAY_UPDATE = 8,  // Internal Overlay Frame
  INTNL_ARF_UPDATE = 9,      // Internal Altref Frame (candidate for ALTREF2)
  FRAME_UPDATE_TYPES = 10
} FRAME_UPDATE_TYPE;

#define FC_ANIMATION_THRESH 0.15
typedef enum {
  FC_NORMAL = 0,
  FC_GRAPHICS_ANIMATION = 1,
  FRAME_CONTENT_TYPES = 2
} FRAME_CONTENT_TYPE;

typedef struct {
  unsigned char index;
  RATE_FACTOR_LEVEL rf_level[(MAX_LAG_BUFFERS * 2) + 1];
  FRAME_UPDATE_TYPE update_type[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char arf_src_offset[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char arf_update_idx[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char arf_ref_idx[(MAX_LAG_BUFFERS * 2) + 1];
#if USE_SYMM_MULTI_LAYER
  unsigned char arf_pos_in_gf[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char pyramid_level[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char pyramid_height;
#endif
  unsigned char brf_src_offset[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char bidir_pred_enabled[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char ref_fb_idx_map[(MAX_LAG_BUFFERS * 2) + 1][REF_FRAMES];
  unsigned char refresh_idx[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char refresh_flag[(MAX_LAG_BUFFERS * 2) + 1];
  int bit_allocation[(MAX_LAG_BUFFERS * 2) + 1];
} GF_GROUP;

typedef struct {
  unsigned int section_intra_rating;
  FIRSTPASS_STATS total_stats;
  FIRSTPASS_STATS this_frame_stats;
  const FIRSTPASS_STATS *stats_in;
  const FIRSTPASS_STATS *stats_in_start;
  const FIRSTPASS_STATS *stats_in_end;
  FIRSTPASS_STATS total_left_stats;
  int first_pass_done;
  int64_t bits_left;
  double modified_error_min;
  double modified_error_max;
  double modified_error_left;
  double mb_av_energy;
  double frame_avg_haar_energy;

#if CONFIG_FP_MB_STATS
  uint8_t *frame_mb_stats_buf;
  uint8_t *this_frame_mb_stats;
  FIRSTPASS_MB_STATS firstpass_mb_stats;
#endif
  // An indication of the content type of the current frame
  FRAME_CONTENT_TYPE fr_content_type;

  // Projected total bits available for a key frame group of frames
  int64_t kf_group_bits;

  // Error score of frames still to be coded in kf group
  int64_t kf_group_error_left;

  // The fraction for a kf groups total bits allocated to the inter frames
  double kfgroup_inter_fraction;

  int sr_update_lag;

  int kf_zeromotion_pct;
  int last_kfgroup_zeromotion_pct;
  int gf_zeromotion_pct;
  int active_worst_quality;
  int baseline_active_worst_quality;
  int extend_minq;
  int extend_maxq;
  int extend_minq_fast;

  GF_GROUP gf_group;
} TWO_PASS;

struct AV1_COMP;

void av1_init_first_pass(struct AV1_COMP *cpi);
void av1_rc_get_first_pass_params(struct AV1_COMP *cpi);
void av1_first_pass(struct AV1_COMP *cpi, const struct lookahead_entry *source);
void av1_end_first_pass(struct AV1_COMP *cpi);

void av1_init_second_pass(struct AV1_COMP *cpi);
void av1_rc_get_second_pass_params(struct AV1_COMP *cpi);
void av1_configure_buffer_updates_firstpass(struct AV1_COMP *cpi,
                                            FRAME_UPDATE_TYPE update_type);

// Post encode update of the rate control parameters for 2-pass
void av1_twopass_postencode_update(struct AV1_COMP *cpi);

#if USE_GF16_MULTI_LAYER
void av1_ref_frame_map_idx_updates(struct AV1_COMP *cpi, int gf_frame_index);
#endif  // USE_GF16_MULTI_LAYER

static INLINE int get_number_of_extra_arfs(int interval, int arf_pending) {
  if (arf_pending && MAX_EXT_ARFS > 0)
    return interval >= MIN_EXT_ARF_INTERVAL * (MAX_EXT_ARFS + 1)
               ? MAX_EXT_ARFS
               : interval >= MIN_EXT_ARF_INTERVAL * MAX_EXT_ARFS
                     ? MAX_EXT_ARFS - 1
                     : 0;
  else
    return 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_FIRSTPASS_H_
