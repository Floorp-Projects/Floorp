/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_RATECTRL_H_
#define VP9_ENCODER_VP9_RATECTRL_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_OVERHEAD_BITS 200

typedef struct {
  // Rate targetting variables
  int this_frame_target;
  int projected_frame_size;
  int sb64_target_rate;
  int last_q[3];                   // Separate values for Intra/Inter/ARF-GF
  int last_boosted_qindex;         // Last boosted GF/KF/ARF q

  int gfu_boost;
  int last_boost;
  int kf_boost;

  double rate_correction_factor;
  double key_frame_rate_correction_factor;
  double gf_rate_correction_factor;

  unsigned int frames_since_golden;
  unsigned int frames_till_gf_update_due;  // Count down till next GF
  unsigned int max_gf_interval;
  unsigned int baseline_gf_interval;
  unsigned int frames_to_key;
  unsigned int frames_since_key;
  unsigned int this_key_frame_forced;
  unsigned int next_key_frame_forced;
  unsigned int source_alt_ref_pending;
  unsigned int source_alt_ref_active;
  unsigned int is_src_frame_alt_ref;

  int av_per_frame_bandwidth;     // Average frame size target for clip
  int min_frame_bandwidth;        // Minimum allocation used for any frame
  int max_frame_bandwidth;        // Maximum burst rate allowed for a frame.

  int ni_av_qi;
  int ni_tot_qi;
  int ni_frames;
  int avg_frame_qindex[3];  // 0 - KEY, 1 - INTER, 2 - ARF/GF
  double tot_q;
  double avg_q;

  int buffer_level;
  int bits_off_target;

  int decimation_factor;
  int decimation_count;

  int rolling_target_bits;
  int rolling_actual_bits;

  int long_rolling_target_bits;
  int long_rolling_actual_bits;

  int64_t total_actual_bits;
  int total_target_vs_actual;        // debug stats

  int worst_quality;
  int best_quality;
  // int active_best_quality;
} RATE_CONTROL;

struct VP9_COMP;

void vp9_save_coding_context(struct VP9_COMP *cpi);
void vp9_restore_coding_context(struct VP9_COMP *cpi);

void vp9_setup_key_frame(struct VP9_COMP *cpi);
void vp9_setup_inter_frame(struct VP9_COMP *cpi);

double vp9_convert_qindex_to_q(int qindex);

// initialize luts for minq
void vp9_rc_init_minq_luts(void);

// Generally at the high level, the following flow is expected
// to be enforced for rate control:
// First call per frame, one of:
//   vp9_rc_get_one_pass_vbr_params()
//   vp9_rc_get_one_pass_cbr_params()
//   vp9_rc_get_svc_params()
//   vp9_rc_get_first_pass_params()
//   vp9_rc_get_second_pass_params()
// depending on the usage to set the rate control encode parameters desired.
//
// Then, call encode_frame_to_data_rate() to perform the
// actual encode. This function will in turn call encode_frame()
// one or more times, followed by one of:
//   vp9_rc_postencode_update()
//   vp9_rc_postencode_update_drop_frame()
//
// The majority of rate control parameters are only expected
// to be set in the vp9_rc_get_..._params() functions and
// updated during the vp9_rc_postencode_update...() functions.
// The only exceptions are vp9_rc_drop_frame() and
// vp9_rc_update_rate_correction_factors() functions.

// Functions to set parameters for encoding before the actual
// encode_frame_to_data_rate() function.
void vp9_rc_get_one_pass_vbr_params(struct VP9_COMP *cpi);
void vp9_rc_get_one_pass_cbr_params(struct VP9_COMP *cpi);
void vp9_rc_get_svc_params(struct VP9_COMP *cpi);

// Post encode update of the rate control parameters based
// on bytes used
void vp9_rc_postencode_update(struct VP9_COMP *cpi,
                              uint64_t bytes_used);
// Post encode update of the rate control parameters for dropped frames
void vp9_rc_postencode_update_drop_frame(struct VP9_COMP *cpi);

// Updates rate correction factors
// Changes only the rate correction factors in the rate control structure.
void vp9_rc_update_rate_correction_factors(struct VP9_COMP *cpi, int damp_var);

// Decide if we should drop this frame: For 1-pass CBR.
// Changes only the decimation count in the rate control structure
int vp9_rc_drop_frame(struct VP9_COMP *cpi);

// Computes frame size bounds.
void vp9_rc_compute_frame_size_bounds(const struct VP9_COMP *cpi,
                                      int this_frame_target,
                                      int *frame_under_shoot_limit,
                                      int *frame_over_shoot_limit);

// Picks q and q bounds given the target for bits
int vp9_rc_pick_q_and_bounds(const struct VP9_COMP *cpi,
                             int *bottom_index,
                             int *top_index);

// Estimates q to achieve a target bits per frame
int vp9_rc_regulate_q(const struct VP9_COMP *cpi, int target_bits_per_frame,
                      int active_best_quality, int active_worst_quality);

// Estimates bits per mb for a given qindex and correction factor.
int vp9_rc_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                       double correction_factor);

// Clamping utilities for bitrate targets for iframes and pframes.
int vp9_rc_clamp_iframe_target_size(const struct VP9_COMP *const cpi,
                                    int target);
int vp9_rc_clamp_pframe_target_size(const struct VP9_COMP *const cpi,
                                    int target);
// Utility to set frame_target into the RATE_CONTROL structure
// This function is called only from the vp9_rc_get_..._params() functions.
void vp9_rc_set_frame_target(struct VP9_COMP *cpi, int target);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_RATECTRL_H_
