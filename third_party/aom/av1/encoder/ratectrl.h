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

#ifndef AV1_ENCODER_RATECTRL_H_
#define AV1_ENCODER_RATECTRL_H_

#include "aom/aom_codec.h"
#include "aom/aom_integer.h"

#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bits Per MB at different Q (Multiplied by 512)
#define BPER_MB_NORMBITS 9

#define CUSTOMIZED_GF 1
#define FIX_GF_INTERVAL_LENGTH 0

#if FIX_GF_INTERVAL_LENGTH
#define FIXED_GF_LENGTH 16
#define USE_SYMM_MULTI_LAYER 1
#else
#define USE_SYMM_MULTI_LAYER 0
#endif

#if USE_SYMM_MULTI_LAYER
#define USE_MANUAL_GF4_STRUCT 0
#endif

#define MIN_GF_INTERVAL 4
#define MAX_GF_INTERVAL 16
#define FIXED_GF_INTERVAL 8  // Used in some testing modes only

typedef enum {
  INTER_NORMAL = 0,
  INTER_LOW = 1,
  INTER_HIGH = 2,
  GF_ARF_LOW = 3,
  GF_ARF_STD = 4,
  KF_STD = 5,
  RATE_FACTOR_LEVELS = 6
} RATE_FACTOR_LEVEL;

static const double rate_factor_deltas[RATE_FACTOR_LEVELS] = {
  1.00,  // INTER_NORMAL
  0.80,  // INTER_LOW
  1.50,  // INTER_HIGH
  1.25,  // GF_ARF_LOW
  2.00,  // GF_ARF_STD
  2.00,  // KF_STD
};

typedef struct {
  int resize_width;
  int resize_height;
  uint8_t superres_denom;
} size_params_type;

typedef struct {
  // Rate targetting variables
  int base_frame_target;  // A baseline frame target before adjustment
                          // for previous under or over shoot.
  int this_frame_target;  // Actual frame target after rc adjustment.
  int projected_frame_size;
  int sb64_target_rate;
  int last_q[FRAME_TYPES];  // Separate values for Intra/Inter
  int last_boosted_qindex;  // Last boosted GF/KF/ARF q
  int last_kf_qindex;       // Q index of the last key frame coded.

  int gfu_boost;
  int last_boost;
  int kf_boost;

  double rate_correction_factors[RATE_FACTOR_LEVELS];

  int frames_since_golden;
  int frames_till_gf_update_due;
  int min_gf_interval;
  int max_gf_interval;
  int static_scene_max_gf_interval;
  int baseline_gf_interval;
  int constrained_gf_group;
  int frames_to_key;
  int frames_since_key;
  int this_key_frame_forced;
  int next_key_frame_forced;
  int source_alt_ref_pending;
  int source_alt_ref_active;
  int is_src_frame_alt_ref;
  int sframe_due;

  // Length of the bi-predictive frame group interval
  int bipred_group_interval;

  // NOTE: Different types of frames may have different bits allocated
  //       accordingly, aiming to achieve the overall optimal RD performance.
  int is_bwd_ref_frame;
  int is_last_bipred_frame;
  int is_bipred_frame;
  int is_src_frame_ext_arf;

  int avg_frame_bandwidth;  // Average frame size target for clip
  int min_frame_bandwidth;  // Minimum allocation used for any frame
  int max_frame_bandwidth;  // Maximum burst rate allowed for a frame.

  int ni_av_qi;
  int ni_tot_qi;
  int ni_frames;
  int avg_frame_qindex[FRAME_TYPES];
  double tot_q;
  double avg_q;

  int64_t buffer_level;
  int64_t bits_off_target;
  int64_t vbr_bits_off_target;
  int64_t vbr_bits_off_target_fast;

  int decimation_factor;
  int decimation_count;

  int rolling_target_bits;
  int rolling_actual_bits;

  int long_rolling_target_bits;
  int long_rolling_actual_bits;

  int rate_error_estimate;

  int64_t total_actual_bits;
  int64_t total_target_bits;
  int64_t total_target_vs_actual;

  int worst_quality;
  int best_quality;

  int64_t starting_buffer_level;
  int64_t optimal_buffer_level;
  int64_t maximum_buffer_size;

  // rate control history for last frame(1) and the frame before(2).
  // -1: undershot
  //  1: overshoot
  //  0: not initialized.
  int rc_1_frame;
  int rc_2_frame;
  int q_1_frame;
  int q_2_frame;

  // Auto frame-scaling variables.
  int rf_level_maxq[RATE_FACTOR_LEVELS];
} RATE_CONTROL;

struct AV1_COMP;
struct AV1EncoderConfig;

void av1_rc_init(const struct AV1EncoderConfig *oxcf, int pass,
                 RATE_CONTROL *rc);

int av1_estimate_bits_at_q(FRAME_TYPE frame_kind, int q, int mbs,
                           double correction_factor, aom_bit_depth_t bit_depth);

double av1_convert_qindex_to_q(int qindex, aom_bit_depth_t bit_depth);

void av1_rc_init_minq_luts(void);

int av1_rc_get_default_min_gf_interval(int width, int height, double framerate);
// Note av1_rc_get_default_max_gf_interval() requires the min_gf_interval to
// be passed in to ensure that the max_gf_interval returned is at least as bis
// as that.
int av1_rc_get_default_max_gf_interval(double framerate, int min_frame_rate);

// Generally at the high level, the following flow is expected
// to be enforced for rate control:
// First call per frame, one of:
//   av1_rc_get_one_pass_vbr_params()
//   av1_rc_get_one_pass_cbr_params()
//   av1_rc_get_first_pass_params()
//   av1_rc_get_second_pass_params()
// depending on the usage to set the rate control encode parameters desired.
//
// Then, call encode_frame_to_data_rate() to perform the
// actual encode. This function will in turn call encode_frame()
// one or more times, followed by one of:
//   av1_rc_postencode_update()
//   av1_rc_postencode_update_drop_frame()
//
// The majority of rate control parameters are only expected
// to be set in the av1_rc_get_..._params() functions and
// updated during the av1_rc_postencode_update...() functions.
// The only exceptions are av1_rc_drop_frame() and
// av1_rc_update_rate_correction_factors() functions.

// Functions to set parameters for encoding before the actual
// encode_frame_to_data_rate() function.
void av1_rc_get_one_pass_vbr_params(struct AV1_COMP *cpi);
void av1_rc_get_one_pass_cbr_params(struct AV1_COMP *cpi);

// Post encode update of the rate control parameters based
// on bytes used
void av1_rc_postencode_update(struct AV1_COMP *cpi, uint64_t bytes_used);
// Post encode update of the rate control parameters for dropped frames
void av1_rc_postencode_update_drop_frame(struct AV1_COMP *cpi);

// Updates rate correction factors
// Changes only the rate correction factors in the rate control structure.
void av1_rc_update_rate_correction_factors(struct AV1_COMP *cpi, int width,
                                           int height);

// Decide if we should drop this frame: For 1-pass CBR.
// Changes only the decimation count in the rate control structure
int av1_rc_drop_frame(struct AV1_COMP *cpi);

// Computes frame size bounds.
void av1_rc_compute_frame_size_bounds(const struct AV1_COMP *cpi,
                                      int this_frame_target,
                                      int *frame_under_shoot_limit,
                                      int *frame_over_shoot_limit);

// Picks q and q bounds given the target for bits
int av1_rc_pick_q_and_bounds(const struct AV1_COMP *cpi, int width, int height,
                             int *bottom_index, int *top_index);

// Estimates q to achieve a target bits per frame
int av1_rc_regulate_q(const struct AV1_COMP *cpi, int target_bits_per_frame,
                      int active_best_quality, int active_worst_quality,
                      int width, int height);

// Estimates bits per mb for a given qindex and correction factor.
int av1_rc_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                       double correction_factor, aom_bit_depth_t bit_depth);

// Clamping utilities for bitrate targets for iframes and pframes.
int av1_rc_clamp_iframe_target_size(const struct AV1_COMP *const cpi,
                                    int target);
int av1_rc_clamp_pframe_target_size(const struct AV1_COMP *const cpi,
                                    int target);
// Utility to set frame_target into the RATE_CONTROL structure
// This function is called only from the av1_rc_get_..._params() functions.
void av1_rc_set_frame_target(struct AV1_COMP *cpi, int target);

// Computes a q delta (in "q index" terms) to get from a starting q value
// to a target q value
int av1_compute_qdelta(const RATE_CONTROL *rc, double qstart, double qtarget,
                       aom_bit_depth_t bit_depth);

// Computes a q delta (in "q index" terms) to get from a starting q value
// to a value that should equate to the given rate ratio.
int av1_compute_qdelta_by_rate(const RATE_CONTROL *rc, FRAME_TYPE frame_type,
                               int qindex, double rate_target_ratio,
                               aom_bit_depth_t bit_depth);

int av1_frame_type_qdelta(const struct AV1_COMP *cpi, int rf_level, int q);

void av1_rc_update_framerate(struct AV1_COMP *cpi, int width, int height);

void av1_rc_set_gf_interval_range(const struct AV1_COMP *const cpi,
                                  RATE_CONTROL *const rc);

void av1_set_target_rate(struct AV1_COMP *cpi, int width, int height);

int av1_resize_one_pass_cbr(struct AV1_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_RATECTRL_H_
