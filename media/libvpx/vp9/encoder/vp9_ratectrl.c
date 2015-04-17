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
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <math.h>

#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_common.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/common/vp9_entropymode.h"
#include "vpx_mem/vpx_mem.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_seg_common.h"

#define LIMIT_QRANGE_FOR_ALTREF_AND_KEY 1

#define MIN_BPB_FACTOR 0.005
#define MAX_BPB_FACTOR 50

// Bits Per MB at different Q (Multiplied by 512)
#define BPER_MB_NORMBITS    9

// Tables relating active max Q to active min Q
static int kf_low_motion_minq[QINDEX_RANGE];
static int kf_high_motion_minq[QINDEX_RANGE];
static int gf_low_motion_minq[QINDEX_RANGE];
static int gf_high_motion_minq[QINDEX_RANGE];
static int inter_minq[QINDEX_RANGE];
static int afq_low_motion_minq[QINDEX_RANGE];
static int afq_high_motion_minq[QINDEX_RANGE];
static int gf_high = 2000;
static int gf_low = 400;
static int kf_high = 5000;
static int kf_low = 400;

// Functions to compute the active minq lookup table entries based on a
// formulaic approach to facilitate easier adjustment of the Q tables.
// The formulae were derived from computing a 3rd order polynomial best
// fit to the original data (after plotting real maxq vs minq (not q index))
static int calculate_minq_index(double maxq,
                                double x3, double x2, double x1, double c) {
  int i;
  const double minqtarget = MIN(((x3 * maxq + x2) * maxq + x1) * maxq + c,
                                maxq);

  // Special case handling to deal with the step from q2.0
  // down to lossless mode represented by q 1.0.
  if (minqtarget <= 2.0)
    return 0;

  for (i = 0; i < QINDEX_RANGE; i++) {
    if (minqtarget <= vp9_convert_qindex_to_q(i))
      return i;
  }

  return QINDEX_RANGE - 1;
}

void vp9_rc_init_minq_luts(void) {
  int i;

  for (i = 0; i < QINDEX_RANGE; i++) {
    const double maxq = vp9_convert_qindex_to_q(i);


    kf_low_motion_minq[i] = calculate_minq_index(maxq,
                                                 0.000001,
                                                 -0.0004,
                                                 0.15,
                                                 0.0);
    kf_high_motion_minq[i] = calculate_minq_index(maxq,
                                                  0.000002,
                                                  -0.0012,
                                                  0.50,
                                                  0.0);

    gf_low_motion_minq[i] = calculate_minq_index(maxq,
                                                 0.0000015,
                                                 -0.0009,
                                                 0.32,
                                                 0.0);
    gf_high_motion_minq[i] = calculate_minq_index(maxq,
                                                  0.0000021,
                                                  -0.00125,
                                                  0.50,
                                                  0.0);
    afq_low_motion_minq[i] = calculate_minq_index(maxq,
                                                  0.0000015,
                                                  -0.0009,
                                                  0.33,
                                                  0.0);
    afq_high_motion_minq[i] = calculate_minq_index(maxq,
                                                   0.0000021,
                                                   -0.00125,
                                                   0.55,
                                                   0.0);
    inter_minq[i] = calculate_minq_index(maxq,
                                         0.00000271,
                                         -0.00113,
                                         0.75,
                                         0.0);
  }
}

// These functions use formulaic calculations to make playing with the
// quantizer tables easier. If necessary they can be replaced by lookup
// tables if and when things settle down in the experimental bitstream
double vp9_convert_qindex_to_q(int qindex) {
  // Convert the index to a real Q value (scaled down to match old Q values)
  return vp9_ac_quant(qindex, 0) / 4.0;
}

int vp9_rc_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                       double correction_factor) {
  const double q = vp9_convert_qindex_to_q(qindex);
  int enumerator = frame_type == KEY_FRAME ? 3300000 : 2250000;

  // q based adjustment to baseline enumerator
  enumerator += (int)(enumerator * q) >> 12;
  return (int)(0.5 + (enumerator * correction_factor / q));
}

void vp9_save_coding_context(VP9_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  VP9_COMMON *cm = &cpi->common;

  // Stores a snapshot of key state variables which can subsequently be
  // restored with a call to vp9_restore_coding_context. These functions are
  // intended for use in a re-code loop in vp9_compress_frame where the
  // quantizer value is adjusted between loop iterations.
  vp9_copy(cc->nmvjointcost,  cpi->mb.nmvjointcost);
  vp9_copy(cc->nmvcosts,  cpi->mb.nmvcosts);
  vp9_copy(cc->nmvcosts_hp,  cpi->mb.nmvcosts_hp);

  vp9_copy(cc->segment_pred_probs, cm->seg.pred_probs);

  vpx_memcpy(cpi->coding_context.last_frame_seg_map_copy,
             cm->last_frame_seg_map, (cm->mi_rows * cm->mi_cols));

  vp9_copy(cc->last_ref_lf_deltas, cm->lf.last_ref_deltas);
  vp9_copy(cc->last_mode_lf_deltas, cm->lf.last_mode_deltas);

  cc->fc = cm->fc;
}

void vp9_restore_coding_context(VP9_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  VP9_COMMON *cm = &cpi->common;

  // Restore key state variables to the snapshot state stored in the
  // previous call to vp9_save_coding_context.
  vp9_copy(cpi->mb.nmvjointcost, cc->nmvjointcost);
  vp9_copy(cpi->mb.nmvcosts, cc->nmvcosts);
  vp9_copy(cpi->mb.nmvcosts_hp, cc->nmvcosts_hp);

  vp9_copy(cm->seg.pred_probs, cc->segment_pred_probs);

  vpx_memcpy(cm->last_frame_seg_map,
             cpi->coding_context.last_frame_seg_map_copy,
             (cm->mi_rows * cm->mi_cols));

  vp9_copy(cm->lf.last_ref_deltas, cc->last_ref_lf_deltas);
  vp9_copy(cm->lf.last_mode_deltas, cc->last_mode_lf_deltas);

  cm->fc = cc->fc;
}

void vp9_setup_key_frame(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;

  vp9_setup_past_independence(cm);

  /* All buffers are implicitly updated on key frames. */
  cpi->refresh_golden_frame = 1;
  cpi->refresh_alt_ref_frame = 1;
}

void vp9_setup_inter_frame(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  if (cm->error_resilient_mode || cm->intra_only)
    vp9_setup_past_independence(cm);

  assert(cm->frame_context_idx < FRAME_CONTEXTS);
  cm->fc = cm->frame_contexts[cm->frame_context_idx];
}

static int estimate_bits_at_q(int frame_kind, int q, int mbs,
                              double correction_factor) {
  const int bpm = (int)(vp9_rc_bits_per_mb(frame_kind, q, correction_factor));

  // Attempt to retain reasonable accuracy without overflow. The cutoff is
  // chosen such that the maximum product of Bpm and MBs fits 31 bits. The
  // largest Bpm takes 20 bits.
  return (mbs > (1 << 11)) ? (bpm >> BPER_MB_NORMBITS) * mbs
                           : (bpm * mbs) >> BPER_MB_NORMBITS;
}

int vp9_rc_clamp_pframe_target_size(const VP9_COMP *const cpi, int target) {
  const RATE_CONTROL *rc = &cpi->rc;
  const int min_frame_target = MAX(rc->min_frame_bandwidth,
                                   rc->av_per_frame_bandwidth >> 5);
  if (target < min_frame_target)
    target = min_frame_target;
  if (cpi->refresh_golden_frame && rc->is_src_frame_alt_ref) {
    // If there is an active ARF at this location use the minimum
    // bits on this frame even if it is a constructed arf.
    // The active maximum quantizer insures that an appropriate
    // number of bits will be spent if needed for constructed ARFs.
    target = min_frame_target;
  }
  // Clip the frame target to the maximum allowed value.
  if (target > rc->max_frame_bandwidth)
    target = rc->max_frame_bandwidth;
  return target;
}

int vp9_rc_clamp_iframe_target_size(const VP9_COMP *const cpi, int target) {
  const RATE_CONTROL *rc = &cpi->rc;
  const VP9_CONFIG *oxcf = &cpi->oxcf;
  if (oxcf->rc_max_intra_bitrate_pct) {
    const int max_rate = rc->av_per_frame_bandwidth *
        oxcf->rc_max_intra_bitrate_pct / 100;
    target = MIN(target, max_rate);
  }
  if (target > rc->max_frame_bandwidth)
    target = rc->max_frame_bandwidth;
  return target;
}


// Update the buffer level for higher layers, given the encoded current layer.
static void update_layer_buffer_level(VP9_COMP *const cpi,
                                      int encoded_frame_size) {
  int temporal_layer = 0;
  int current_temporal_layer = cpi->svc.temporal_layer_id;
  for (temporal_layer = current_temporal_layer + 1;
      temporal_layer < cpi->svc.number_temporal_layers; ++temporal_layer) {
    LAYER_CONTEXT *lc = &cpi->svc.layer_context[temporal_layer];
    RATE_CONTROL *lrc = &lc->rc;
    int bits_off_for_this_layer = (int)(lc->target_bandwidth / lc->framerate -
        encoded_frame_size);
    lrc->bits_off_target += bits_off_for_this_layer;

    // Clip buffer level to maximum buffer size for the layer.
    lrc->bits_off_target = MIN(lrc->bits_off_target, lc->maximum_buffer_size);
    lrc->buffer_level = lrc->bits_off_target;
  }
}

// Update the buffer level: leaky bucket model.
static void update_buffer_level(VP9_COMP *cpi, int encoded_frame_size) {
  const VP9_COMMON *const cm = &cpi->common;
  const VP9_CONFIG *oxcf = &cpi->oxcf;
  RATE_CONTROL *const rc = &cpi->rc;

  // Non-viewable frames are a special case and are treated as pure overhead.
  if (!cm->show_frame) {
    rc->bits_off_target -= encoded_frame_size;
  } else {
    rc->bits_off_target += rc->av_per_frame_bandwidth - encoded_frame_size;
  }

  // Clip the buffer level to the maximum specified buffer size.
  rc->bits_off_target = MIN(rc->bits_off_target, oxcf->maximum_buffer_size);
  rc->buffer_level = rc->bits_off_target;

  if (cpi->use_svc && cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
    update_layer_buffer_level(cpi, encoded_frame_size);
  }
}

int vp9_rc_drop_frame(VP9_COMP *cpi) {
  const VP9_CONFIG *oxcf = &cpi->oxcf;
  RATE_CONTROL *const rc = &cpi->rc;

  if (!oxcf->drop_frames_water_mark) {
    return 0;
  } else {
    if (rc->buffer_level < 0) {
      // Always drop if buffer is below 0.
      return 1;
    } else {
      // If buffer is below drop_mark, for now just drop every other frame
      // (starting with the next frame) until it increases back over drop_mark.
      int drop_mark = (int)(oxcf->drop_frames_water_mark *
          oxcf->optimal_buffer_level / 100);
      if ((rc->buffer_level > drop_mark) &&
          (rc->decimation_factor > 0)) {
        --rc->decimation_factor;
      } else if (rc->buffer_level <= drop_mark &&
          rc->decimation_factor == 0) {
        rc->decimation_factor = 1;
      }
      if (rc->decimation_factor > 0) {
        if (rc->decimation_count > 0) {
          --rc->decimation_count;
          return 1;
        } else {
          rc->decimation_count = rc->decimation_factor;
          return 0;
        }
      } else {
        rc->decimation_count = 0;
        return 0;
      }
    }
  }
}

static double get_rate_correction_factor(const VP9_COMP *cpi) {
  if (cpi->common.frame_type == KEY_FRAME) {
    return cpi->rc.key_frame_rate_correction_factor;
  } else {
    if ((cpi->refresh_alt_ref_frame || cpi->refresh_golden_frame) &&
        !(cpi->use_svc && cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER))
      return cpi->rc.gf_rate_correction_factor;
    else
      return cpi->rc.rate_correction_factor;
  }
}

static void set_rate_correction_factor(VP9_COMP *cpi, double factor) {
  if (cpi->common.frame_type == KEY_FRAME) {
    cpi->rc.key_frame_rate_correction_factor = factor;
  } else {
    if ((cpi->refresh_alt_ref_frame || cpi->refresh_golden_frame) &&
        !(cpi->use_svc && cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER))
      cpi->rc.gf_rate_correction_factor = factor;
    else
      cpi->rc.rate_correction_factor = factor;
  }
}

void vp9_rc_update_rate_correction_factors(VP9_COMP *cpi, int damp_var) {
  const int q = cpi->common.base_qindex;
  int correction_factor = 100;
  double rate_correction_factor = get_rate_correction_factor(cpi);
  double adjustment_limit;

  int projected_size_based_on_q = 0;

  // Clear down mmx registers to allow floating point in what follows
  vp9_clear_system_state();  // __asm emms;

  // Work out how big we would have expected the frame to be at this Q given
  // the current correction factor.
  // Stay in double to avoid int overflow when values are large
  projected_size_based_on_q = estimate_bits_at_q(cpi->common.frame_type, q,
                                                 cpi->common.MBs,
                                                 rate_correction_factor);
  // Work out a size correction factor.
  if (projected_size_based_on_q > 0)
    correction_factor = (100 * cpi->rc.projected_frame_size) /
                            projected_size_based_on_q;

  // More heavily damped adjustment used if we have been oscillating either side
  // of target.
  switch (damp_var) {
    case 0:
      adjustment_limit = 0.75;
      break;
    case 1:
      adjustment_limit = 0.375;
      break;
    case 2:
    default:
      adjustment_limit = 0.25;
      break;
  }

  if (correction_factor > 102) {
    // We are not already at the worst allowable quality
    correction_factor =
        (int)(100 + ((correction_factor - 100) * adjustment_limit));
    rate_correction_factor =
        ((rate_correction_factor * correction_factor) / 100);

    // Keep rate_correction_factor within limits
    if (rate_correction_factor > MAX_BPB_FACTOR)
      rate_correction_factor = MAX_BPB_FACTOR;
  } else if (correction_factor < 99) {
    // We are not already at the best allowable quality
    correction_factor =
        (int)(100 - ((100 - correction_factor) * adjustment_limit));
    rate_correction_factor =
        ((rate_correction_factor * correction_factor) / 100);

    // Keep rate_correction_factor within limits
    if (rate_correction_factor < MIN_BPB_FACTOR)
      rate_correction_factor = MIN_BPB_FACTOR;
  }

  set_rate_correction_factor(cpi, rate_correction_factor);
}


int vp9_rc_regulate_q(const VP9_COMP *cpi, int target_bits_per_frame,
                      int active_best_quality, int active_worst_quality) {
  const VP9_COMMON *const cm = &cpi->common;
  int q = active_worst_quality;
  int last_error = INT_MAX;
  int i, target_bits_per_mb;
  const double correction_factor = get_rate_correction_factor(cpi);

  // Calculate required scaling factor based on target frame size and size of
  // frame produced using previous Q.
  if (target_bits_per_frame >= (INT_MAX >> BPER_MB_NORMBITS))
    // Case where we would overflow int
    target_bits_per_mb = (target_bits_per_frame / cm->MBs) << BPER_MB_NORMBITS;
  else
    target_bits_per_mb = (target_bits_per_frame << BPER_MB_NORMBITS) / cm->MBs;

  i = active_best_quality;

  do {
    const int bits_per_mb_at_this_q = (int)vp9_rc_bits_per_mb(cm->frame_type, i,
                                                             correction_factor);

    if (bits_per_mb_at_this_q <= target_bits_per_mb) {
      if ((target_bits_per_mb - bits_per_mb_at_this_q) <= last_error)
        q = i;
      else
        q = i - 1;

      break;
    } else {
      last_error = bits_per_mb_at_this_q - target_bits_per_mb;
    }
  } while (++i <= active_worst_quality);

  return q;
}

static int get_active_quality(int q, int gfu_boost, int low, int high,
                              int *low_motion_minq, int *high_motion_minq) {
  if (gfu_boost > high) {
    return low_motion_minq[q];
  } else if (gfu_boost < low) {
    return high_motion_minq[q];
  } else {
    const int gap = high - low;
    const int offset = high - gfu_boost;
    const int qdiff = high_motion_minq[q] - low_motion_minq[q];
    const int adjustment = ((offset * qdiff) + (gap >> 1)) / gap;
    return low_motion_minq[q] + adjustment;
  }
}

static int calc_active_worst_quality_one_pass_vbr(const VP9_COMP *cpi) {
  int active_worst_quality;
  if (cpi->common.frame_type == KEY_FRAME) {
    if (cpi->common.current_video_frame == 0) {
      active_worst_quality = cpi->rc.worst_quality;
    } else {
      // Choose active worst quality twice as large as the last q.
      active_worst_quality = cpi->rc.last_q[KEY_FRAME] * 2;
    }
  } else if (!cpi->rc.is_src_frame_alt_ref &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    if (cpi->common.current_video_frame == 1) {
      active_worst_quality = cpi->rc.last_q[KEY_FRAME] * 5 / 4;
    } else {
      // Choose active worst quality twice as large as the last q.
      active_worst_quality = cpi->rc.last_q[INTER_FRAME];
    }
  } else {
    if (cpi->common.current_video_frame == 1) {
      active_worst_quality = cpi->rc.last_q[KEY_FRAME] * 2;
    } else {
      // Choose active worst quality twice as large as the last q.
      active_worst_quality = cpi->rc.last_q[INTER_FRAME] * 2;
    }
  }
  if (active_worst_quality > cpi->rc.worst_quality)
    active_worst_quality = cpi->rc.worst_quality;
  return active_worst_quality;
}

// Adjust active_worst_quality level based on buffer level.
static int calc_active_worst_quality_one_pass_cbr(const VP9_COMP *cpi) {
  // Adjust active_worst_quality: If buffer is above the optimal/target level,
  // bring active_worst_quality down depending on fullness of buffer.
  // If buffer is below the optimal level, let the active_worst_quality go from
  // ambient Q (at buffer = optimal level) to worst_quality level
  // (at buffer = critical level).
  const VP9_CONFIG *oxcf = &cpi->oxcf;
  const RATE_CONTROL *rc = &cpi->rc;
  // Buffer level below which we push active_worst to worst_quality.
  int critical_level = oxcf->optimal_buffer_level >> 2;
  int adjustment = 0;
  int buff_lvl_step = 0;
  int active_worst_quality;
  if (cpi->common.frame_type == KEY_FRAME)
    return rc->worst_quality;
  if (cpi->common.current_video_frame > 1)
    active_worst_quality = MIN(rc->worst_quality,
                               rc->avg_frame_qindex[INTER_FRAME] * 5 / 4);
  else
    active_worst_quality = MIN(rc->worst_quality,
                               rc->avg_frame_qindex[KEY_FRAME] * 3 / 2);
  if (rc->buffer_level > oxcf->optimal_buffer_level) {
    // Adjust down.
    // Maximum limit for down adjustment, ~30%.
    int max_adjustment_down = active_worst_quality / 3;
    if (max_adjustment_down) {
      buff_lvl_step = (int)((oxcf->maximum_buffer_size -
          oxcf->optimal_buffer_level) / max_adjustment_down);
      if (buff_lvl_step)
        adjustment = (int)((rc->buffer_level - oxcf->optimal_buffer_level) /
                            buff_lvl_step);
      active_worst_quality -= adjustment;
    }
  } else if (rc->buffer_level > critical_level) {
    // Adjust up from ambient Q.
    if (critical_level) {
      buff_lvl_step = (oxcf->optimal_buffer_level - critical_level);
      if (buff_lvl_step) {
        adjustment = (rc->worst_quality - rc->avg_frame_qindex[INTER_FRAME]) *
                         (oxcf->optimal_buffer_level - rc->buffer_level) /
                             buff_lvl_step;
      }
      active_worst_quality = rc->avg_frame_qindex[INTER_FRAME] + adjustment;
    }
  } else {
    // Set to worst_quality if buffer is below critical level.
    active_worst_quality = rc->worst_quality;
  }
  return active_worst_quality;
}

static int rc_pick_q_and_bounds_one_pass_cbr(const VP9_COMP *cpi,
                                             int *bottom_index,
                                             int *top_index) {
  const VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  int active_best_quality;
  int active_worst_quality = calc_active_worst_quality_one_pass_cbr(cpi);
  int q;

  if (frame_is_intra_only(cm)) {
    active_best_quality = rc->best_quality;
    // Handle the special case for key frames forced when we have75 reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (rc->this_key_frame_forced) {
      int qindex = rc->last_boosted_qindex;
      double last_boosted_q = vp9_convert_qindex_to_q(qindex);
      int delta_qindex = vp9_compute_qdelta(cpi, last_boosted_q,
                                            (last_boosted_q * 0.75));
      active_best_quality = MAX(qindex + delta_qindex, rc->best_quality);
    } else if (cm->current_video_frame > 0) {
      // not first frame of one pass and kf_boost is set
      double q_adj_factor = 1.0;
      double q_val;

      active_best_quality = get_active_quality(rc->avg_frame_qindex[KEY_FRAME],
                                               rc->kf_boost,
                                               kf_low, kf_high,
                                               kf_low_motion_minq,
                                               kf_high_motion_minq);

      // Allow somewhat lower kf minq with small image formats.
      if ((cm->width * cm->height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = vp9_convert_qindex_to_q(active_best_quality);
      active_best_quality += vp9_compute_qdelta(cpi, q_val, q_val *
                                                   q_adj_factor);
    }
  } else if (!rc->is_src_frame_alt_ref &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    // Use the lower of active_worst_quality and recent
    // average Q as basis for GF/ARF best Q limit unless last frame was
    // a key frame.
    if (rc->frames_since_key > 1 &&
        rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality) {
      q = rc->avg_frame_qindex[INTER_FRAME];
    } else {
      q = active_worst_quality;
    }
    active_best_quality = get_active_quality(
        q, rc->gfu_boost, gf_low, gf_high,
        gf_low_motion_minq, gf_high_motion_minq);
  } else {
    // Use the lower of active_worst_quality and recent/average Q.
    if (cm->current_video_frame > 1) {
      if (rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality)
        active_best_quality = inter_minq[rc->avg_frame_qindex[INTER_FRAME]];
      else
        active_best_quality = inter_minq[active_worst_quality];
    } else {
      if (rc->avg_frame_qindex[KEY_FRAME] < active_worst_quality)
        active_best_quality = inter_minq[rc->avg_frame_qindex[KEY_FRAME]];
      else
        active_best_quality = inter_minq[active_worst_quality];
    }
  }

  // Clip the active best and worst quality values to limits
  active_best_quality = clamp(active_best_quality,
                              rc->best_quality, rc->worst_quality);
  active_worst_quality = clamp(active_worst_quality,
                               active_best_quality, rc->worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

#if LIMIT_QRANGE_FOR_ALTREF_AND_KEY
  // Limit Q range for the adaptive loop.
  if (cm->frame_type == KEY_FRAME && !rc->this_key_frame_forced) {
    if (!(cm->current_video_frame == 0))
      *top_index = (active_worst_quality + active_best_quality * 3) / 4;
  }
#endif
  // Special case code to try and match quality with forced key frames
  if (cm->frame_type == KEY_FRAME && rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                          active_best_quality, active_worst_quality);
    if (q > *top_index) {
      // Special case when we are targeting the max allowed rate
      if (cpi->rc.this_frame_target >= cpi->rc.max_frame_bandwidth)
        *top_index = q;
      else
        q = *top_index;
    }
  }
  assert(*top_index <= rc->worst_quality &&
         *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

static int rc_pick_q_and_bounds_one_pass_vbr(const VP9_COMP *cpi,
                                             int *bottom_index,
                                             int *top_index) {
  const VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  int active_best_quality;
  int active_worst_quality = calc_active_worst_quality_one_pass_vbr(cpi);
  int q;

  if (frame_is_intra_only(cm)) {
    active_best_quality = rc->best_quality;
#if !CONFIG_MULTIPLE_ARF
    // Handle the special case for key frames forced when we have75 reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (rc->this_key_frame_forced) {
      int qindex = rc->last_boosted_qindex;
      double last_boosted_q = vp9_convert_qindex_to_q(qindex);
      int delta_qindex = vp9_compute_qdelta(cpi, last_boosted_q,
                                            (last_boosted_q * 0.75));
      active_best_quality = MAX(qindex + delta_qindex, rc->best_quality);
    } else if (cm->current_video_frame > 0) {
      // not first frame of one pass and kf_boost is set
      double q_adj_factor = 1.0;
      double q_val;

      active_best_quality = get_active_quality(rc->avg_frame_qindex[KEY_FRAME],
                                               rc->kf_boost,
                                               kf_low, kf_high,
                                               kf_low_motion_minq,
                                               kf_high_motion_minq);

      // Allow somewhat lower kf minq with small image formats.
      if ((cm->width * cm->height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = vp9_convert_qindex_to_q(active_best_quality);
      active_best_quality += vp9_compute_qdelta(cpi, q_val, q_val *
                                                   q_adj_factor);
    }
#else
    double current_q;
    // Force the KF quantizer to be 30% of the active_worst_quality.
    current_q = vp9_convert_qindex_to_q(active_worst_quality);
    active_best_quality = active_worst_quality
        + vp9_compute_qdelta(cpi, current_q, current_q * 0.3);
#endif
  } else if (!rc->is_src_frame_alt_ref &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    // Use the lower of active_worst_quality and recent
    // average Q as basis for GF/ARF best Q limit unless last frame was
    // a key frame.
    if (rc->frames_since_key > 1 &&
        rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality) {
      q = rc->avg_frame_qindex[INTER_FRAME];
    } else {
      q = rc->avg_frame_qindex[KEY_FRAME];
    }
    // For constrained quality dont allow Q less than the cq level
    if (oxcf->end_usage == USAGE_CONSTRAINED_QUALITY) {
      if (q < cpi->cq_target_quality)
        q = cpi->cq_target_quality;
      if (rc->frames_since_key > 1) {
        active_best_quality = get_active_quality(q, rc->gfu_boost,
                                                 gf_low, gf_high,
                                                 afq_low_motion_minq,
                                                 afq_high_motion_minq);
      } else {
        active_best_quality = get_active_quality(q, rc->gfu_boost,
                                                 gf_low, gf_high,
                                                 gf_low_motion_minq,
                                                 gf_high_motion_minq);
      }
      // Constrained quality use slightly lower active best.
      active_best_quality = active_best_quality * 15 / 16;

    } else if (oxcf->end_usage == USAGE_CONSTANT_QUALITY) {
      if (!cpi->refresh_alt_ref_frame) {
        active_best_quality = cpi->cq_target_quality;
      } else {
        if (rc->frames_since_key > 1) {
          active_best_quality = get_active_quality(
              q, rc->gfu_boost, gf_low, gf_high,
              afq_low_motion_minq, afq_high_motion_minq);
        } else {
          active_best_quality = get_active_quality(
              q, rc->gfu_boost, gf_low, gf_high,
              gf_low_motion_minq, gf_high_motion_minq);
        }
      }
    } else {
      active_best_quality = get_active_quality(
          q, rc->gfu_boost, gf_low, gf_high,
          gf_low_motion_minq, gf_high_motion_minq);
    }
  } else {
    if (oxcf->end_usage == USAGE_CONSTANT_QUALITY) {
      active_best_quality = cpi->cq_target_quality;
    } else {
      // Use the lower of active_worst_quality and recent/average Q.
      if (cm->current_video_frame > 1)
        active_best_quality = inter_minq[rc->avg_frame_qindex[INTER_FRAME]];
      else
        active_best_quality = inter_minq[rc->avg_frame_qindex[KEY_FRAME]];
      // For the constrained quality mode we don't want
      // q to fall below the cq level.
      if ((oxcf->end_usage == USAGE_CONSTRAINED_QUALITY) &&
          (active_best_quality < cpi->cq_target_quality)) {
        // If we are strongly undershooting the target rate in the last
        // frames then use the user passed in cq value not the auto
        // cq value.
        if (rc->rolling_actual_bits < rc->min_frame_bandwidth)
          active_best_quality = oxcf->cq_level;
        else
          active_best_quality = cpi->cq_target_quality;
      }
    }
  }

  // Clip the active best and worst quality values to limits
  active_best_quality = clamp(active_best_quality,
                              rc->best_quality, rc->worst_quality);
  active_worst_quality = clamp(active_worst_quality,
                               active_best_quality, rc->worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

#if LIMIT_QRANGE_FOR_ALTREF_AND_KEY
  // Limit Q range for the adaptive loop.
  if (cm->frame_type == KEY_FRAME && !rc->this_key_frame_forced) {
    if (!(cm->current_video_frame == 0))
      *top_index = (active_worst_quality + active_best_quality * 3) / 4;
  } else if (!rc->is_src_frame_alt_ref &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    *top_index = (active_worst_quality + active_best_quality) / 2;
  }
#endif
  if (oxcf->end_usage == USAGE_CONSTANT_QUALITY) {
    q = active_best_quality;
  // Special case code to try and match quality with forced key frames
  } else if ((cm->frame_type == KEY_FRAME) && rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                          active_best_quality, active_worst_quality);
    if (q > *top_index) {
      // Special case when we are targeting the max allowed rate
      if (cpi->rc.this_frame_target >= cpi->rc.max_frame_bandwidth)
        *top_index = q;
      else
        q = *top_index;
    }
  }
#if CONFIG_MULTIPLE_ARF
  // Force the quantizer determined by the coding order pattern.
  if (cpi->multi_arf_enabled && (cm->frame_type != KEY_FRAME) &&
      cpi->oxcf.end_usage != USAGE_CONSTANT_QUALITY) {
    double new_q;
    double current_q = vp9_convert_qindex_to_q(active_worst_quality);
    int level = cpi->this_frame_weight;
    assert(level >= 0);
    new_q = current_q * (1.0 - (0.2 * (cpi->max_arf_level - level)));
    q = active_worst_quality +
        vp9_compute_qdelta(cpi, current_q, new_q);

    *bottom_index = q;
    *top_index    = q;
    printf("frame:%d q:%d\n", cm->current_video_frame, q);
  }
#endif
  assert(*top_index <= rc->worst_quality &&
         *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

static int rc_pick_q_and_bounds_two_pass(const VP9_COMP *cpi,
                                         int *bottom_index,
                                         int *top_index) {
  const VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  int active_best_quality;
  int active_worst_quality = cpi->twopass.active_worst_quality;
  int q;

  if (frame_is_intra_only(cm)) {
#if !CONFIG_MULTIPLE_ARF
    // Handle the special case for key frames forced when we have75 reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (rc->this_key_frame_forced) {
      int qindex = rc->last_boosted_qindex;
      double last_boosted_q = vp9_convert_qindex_to_q(qindex);
      int delta_qindex = vp9_compute_qdelta(cpi, last_boosted_q,
                                            (last_boosted_q * 0.75));
      active_best_quality = MAX(qindex + delta_qindex, rc->best_quality);
    } else {
      // Not forced keyframe.
      double q_adj_factor = 1.0;
      double q_val;
      // Baseline value derived from cpi->active_worst_quality and kf boost.
      active_best_quality = get_active_quality(active_worst_quality,
                                               rc->kf_boost,
                                               kf_low, kf_high,
                                               kf_low_motion_minq,
                                               kf_high_motion_minq);

      // Allow somewhat lower kf minq with small image formats.
      if ((cm->width * cm->height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Make a further adjustment based on the kf zero motion measure.
      q_adj_factor += 0.05 - (0.001 * (double)cpi->twopass.kf_zeromotion_pct);

      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = vp9_convert_qindex_to_q(active_best_quality);
      active_best_quality += vp9_compute_qdelta(cpi, q_val, q_val *
                                                   q_adj_factor);
    }
#else
    double current_q;
    // Force the KF quantizer to be 30% of the active_worst_quality.
    current_q = vp9_convert_qindex_to_q(active_worst_quality);
    active_best_quality = active_worst_quality
        + vp9_compute_qdelta(cpi, current_q, current_q * 0.3);
#endif
  } else if (!rc->is_src_frame_alt_ref &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    // Use the lower of active_worst_quality and recent
    // average Q as basis for GF/ARF best Q limit unless last frame was
    // a key frame.
    if (rc->frames_since_key > 1 &&
        rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality) {
      q = rc->avg_frame_qindex[INTER_FRAME];
    } else {
      q = active_worst_quality;
    }
    // For constrained quality dont allow Q less than the cq level
    if (oxcf->end_usage == USAGE_CONSTRAINED_QUALITY) {
      if (q < cpi->cq_target_quality)
        q = cpi->cq_target_quality;
      if (rc->frames_since_key > 1) {
        active_best_quality = get_active_quality(q, rc->gfu_boost,
                                                 gf_low, gf_high,
                                                 afq_low_motion_minq,
                                                 afq_high_motion_minq);
      } else {
        active_best_quality = get_active_quality(q, rc->gfu_boost,
                                                 gf_low, gf_high,
                                                 gf_low_motion_minq,
                                                 gf_high_motion_minq);
      }
      // Constrained quality use slightly lower active best.
      active_best_quality = active_best_quality * 15 / 16;

    } else if (oxcf->end_usage == USAGE_CONSTANT_QUALITY) {
      if (!cpi->refresh_alt_ref_frame) {
        active_best_quality = cpi->cq_target_quality;
      } else {
        if (rc->frames_since_key > 1) {
          active_best_quality = get_active_quality(
              q, rc->gfu_boost, gf_low, gf_high,
              afq_low_motion_minq, afq_high_motion_minq);
        } else {
          active_best_quality = get_active_quality(
              q, rc->gfu_boost, gf_low, gf_high,
              gf_low_motion_minq, gf_high_motion_minq);
        }
      }
    } else {
      active_best_quality = get_active_quality(
          q, rc->gfu_boost, gf_low, gf_high,
          gf_low_motion_minq, gf_high_motion_minq);
    }
  } else {
    if (oxcf->end_usage == USAGE_CONSTANT_QUALITY) {
      active_best_quality = cpi->cq_target_quality;
    } else {
      active_best_quality = inter_minq[active_worst_quality];

      // For the constrained quality mode we don't want
      // q to fall below the cq level.
      if ((oxcf->end_usage == USAGE_CONSTRAINED_QUALITY) &&
          (active_best_quality < cpi->cq_target_quality)) {
        // If we are strongly undershooting the target rate in the last
        // frames then use the user passed in cq value not the auto
        // cq value.
        if (rc->rolling_actual_bits < rc->min_frame_bandwidth)
          active_best_quality = oxcf->cq_level;
        else
          active_best_quality = cpi->cq_target_quality;
      }
    }
  }

  // Clip the active best and worst quality values to limits.
  if (active_worst_quality > rc->worst_quality)
    active_worst_quality = rc->worst_quality;

  if (active_best_quality < rc->best_quality)
    active_best_quality = rc->best_quality;

  if (active_best_quality > rc->worst_quality)
    active_best_quality = rc->worst_quality;

  if (active_worst_quality < active_best_quality)
    active_worst_quality = active_best_quality;

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

#if LIMIT_QRANGE_FOR_ALTREF_AND_KEY
  // Limit Q range for the adaptive loop.
  if (cm->frame_type == KEY_FRAME && !rc->this_key_frame_forced) {
    *top_index = (active_worst_quality + active_best_quality * 3) / 4;
  } else if (!rc->is_src_frame_alt_ref &&
             (oxcf->end_usage != USAGE_STREAM_FROM_SERVER) &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    *top_index = (active_worst_quality + active_best_quality) / 2;
  }
#endif

  if (oxcf->end_usage == USAGE_CONSTANT_QUALITY) {
    q = active_best_quality;
  // Special case code to try and match quality with forced key frames.
  } else if ((cm->frame_type == KEY_FRAME) && rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                          active_best_quality, active_worst_quality);
    if (q > *top_index) {
      // Special case when we are targeting the max allowed rate.
      if (cpi->rc.this_frame_target >= cpi->rc.max_frame_bandwidth)
        *top_index = q;
      else
        q = *top_index;
    }
  }
#if CONFIG_MULTIPLE_ARF
  // Force the quantizer determined by the coding order pattern.
  if (cpi->multi_arf_enabled && (cm->frame_type != KEY_FRAME) &&
      cpi->oxcf.end_usage != USAGE_CONSTANT_QUALITY) {
    double new_q;
    double current_q = vp9_convert_qindex_to_q(active_worst_quality);
    int level = cpi->this_frame_weight;
    assert(level >= 0);
    new_q = current_q * (1.0 - (0.2 * (cpi->max_arf_level - level)));
    q = active_worst_quality +
        vp9_compute_qdelta(cpi, current_q, new_q);

    *bottom_index = q;
    *top_index    = q;
    printf("frame:%d q:%d\n", cm->current_video_frame, q);
  }
#endif
  assert(*top_index <= rc->worst_quality &&
         *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

int vp9_rc_pick_q_and_bounds(const VP9_COMP *cpi,
                             int *bottom_index,
                             int *top_index) {
  int q;
  if (cpi->pass == 0) {
    if (cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER)
      q = rc_pick_q_and_bounds_one_pass_cbr(cpi, bottom_index, top_index);
    else
      q = rc_pick_q_and_bounds_one_pass_vbr(cpi, bottom_index, top_index);
  } else {
    q = rc_pick_q_and_bounds_two_pass(cpi, bottom_index, top_index);
  }

  // JBB : This is realtime mode.  In real time mode the first frame
  // should be larger. Q of 0 is disabled because we force tx size to be
  // 16x16...
  if (cpi->sf.use_pick_mode) {
    if (cpi->common.current_video_frame == 0)
      q /= 3;
    if (q == 0)
      q++;
    if (q < *bottom_index)
      *bottom_index = q;
    else if (q > *top_index)
      *top_index = q;
  }
  return q;
}

void vp9_rc_compute_frame_size_bounds(const VP9_COMP *cpi,
                                      int this_frame_target,
                                      int *frame_under_shoot_limit,
                                      int *frame_over_shoot_limit) {
  // Set-up bounds on acceptable frame size:
  if (cpi->oxcf.end_usage == USAGE_CONSTANT_QUALITY) {
    *frame_under_shoot_limit = 0;
    *frame_over_shoot_limit  = INT_MAX;
  } else {
    if (cpi->common.frame_type == KEY_FRAME) {
      *frame_over_shoot_limit  = this_frame_target * 9 / 8;
      *frame_under_shoot_limit = this_frame_target * 7 / 8;
    } else {
      if (cpi->refresh_alt_ref_frame || cpi->refresh_golden_frame) {
        *frame_over_shoot_limit  = this_frame_target * 9 / 8;
        *frame_under_shoot_limit = this_frame_target * 7 / 8;
      } else {
        // Stron overshoot limit for constrained quality
        if (cpi->oxcf.end_usage == USAGE_CONSTRAINED_QUALITY) {
          *frame_over_shoot_limit  = this_frame_target * 11 / 8;
          *frame_under_shoot_limit = this_frame_target * 2 / 8;
        } else {
          *frame_over_shoot_limit  = this_frame_target * 11 / 8;
          *frame_under_shoot_limit = this_frame_target * 5 / 8;
        }
      }
    }

    // For very small rate targets where the fractional adjustment
    // (eg * 7/8) may be tiny make sure there is at least a minimum
    // range.
    *frame_over_shoot_limit += 200;
    *frame_under_shoot_limit -= 200;
    if (*frame_under_shoot_limit < 0)
      *frame_under_shoot_limit = 0;

    // Clip to maximum allowed rate for a frame.
    if (*frame_over_shoot_limit > cpi->rc.max_frame_bandwidth) {
      *frame_over_shoot_limit = cpi->rc.max_frame_bandwidth;
    }
  }
}

void vp9_rc_set_frame_target(VP9_COMP *cpi, int target) {
  const VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;

  rc->this_frame_target = target;
  // Target rate per SB64 (including partial SB64s.
  rc->sb64_target_rate = ((int64_t)rc->this_frame_target * 64 * 64) /
                             (cm->width * cm->height);
}

static void update_alt_ref_frame_stats(VP9_COMP *cpi) {
  // this frame refreshes means next frames don't unless specified by user
  cpi->rc.frames_since_golden = 0;

#if CONFIG_MULTIPLE_ARF
  if (!cpi->multi_arf_enabled)
#endif
    // Clear the alternate reference update pending flag.
    cpi->rc.source_alt_ref_pending = 0;

  // Set the alternate reference frame active flag
  cpi->rc.source_alt_ref_active = 1;
}

static void update_golden_frame_stats(VP9_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;

  // Update the Golden frame usage counts.
  if (cpi->refresh_golden_frame) {
    // this frame refreshes means next frames don't unless specified by user
    rc->frames_since_golden = 0;

    if (!rc->source_alt_ref_pending)
      rc->source_alt_ref_active = 0;

    // Decrement count down till next gf
    if (rc->frames_till_gf_update_due > 0)
      rc->frames_till_gf_update_due--;

  } else if (!cpi->refresh_alt_ref_frame) {
    // Decrement count down till next gf
    if (rc->frames_till_gf_update_due > 0)
      rc->frames_till_gf_update_due--;

    rc->frames_since_golden++;
  }
}

void vp9_rc_postencode_update(VP9_COMP *cpi, uint64_t bytes_used) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;

  cm->last_frame_type = cm->frame_type;
  // Update rate control heuristics
  rc->projected_frame_size = (bytes_used << 3);

  // Post encode loop adjustment of Q prediction.
  vp9_rc_update_rate_correction_factors(
      cpi, (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF ||
            cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) ? 2 : 0);

  // Keep a record of last Q and ambient average Q.
  if (cm->frame_type == KEY_FRAME) {
    rc->last_q[KEY_FRAME] = cm->base_qindex;
    rc->avg_frame_qindex[KEY_FRAME] = ROUND_POWER_OF_TWO(
        3 * rc->avg_frame_qindex[KEY_FRAME] + cm->base_qindex, 2);
  } else if (!rc->is_src_frame_alt_ref &&
      (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame) &&
      !(cpi->use_svc && cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER)) {
    rc->last_q[2] = cm->base_qindex;
    rc->avg_frame_qindex[2] = ROUND_POWER_OF_TWO(
        3 * rc->avg_frame_qindex[2] + cm->base_qindex, 2);
  } else {
    rc->last_q[INTER_FRAME] = cm->base_qindex;
    rc->avg_frame_qindex[INTER_FRAME] = ROUND_POWER_OF_TWO(
        3 * rc->avg_frame_qindex[INTER_FRAME] + cm->base_qindex, 2);
    rc->ni_frames++;
    rc->tot_q += vp9_convert_qindex_to_q(cm->base_qindex);
    rc->avg_q = rc->tot_q / (double)rc->ni_frames;

    // Calculate the average Q for normal inter frames (not key or GFU frames).
    rc->ni_tot_qi += cm->base_qindex;
    rc->ni_av_qi = rc->ni_tot_qi / rc->ni_frames;
  }

  // Keep record of last boosted (KF/KF/ARF) Q value.
  // If the current frame is coded at a lower Q then we also update it.
  // If all mbs in this group are skipped only update if the Q value is
  // better than that already stored.
  // This is used to help set quality in forced key frames to reduce popping
  if ((cm->base_qindex < rc->last_boosted_qindex) ||
      ((cpi->static_mb_pct < 100) &&
       ((cm->frame_type == KEY_FRAME) || cpi->refresh_alt_ref_frame ||
        (cpi->refresh_golden_frame && !rc->is_src_frame_alt_ref)))) {
    rc->last_boosted_qindex = cm->base_qindex;
  }

  update_buffer_level(cpi, rc->projected_frame_size);

  // Rolling monitors of whether we are over or underspending used to help
  // regulate min and Max Q in two pass.
  if (cm->frame_type != KEY_FRAME) {
    rc->rolling_target_bits = ROUND_POWER_OF_TWO(
        rc->rolling_target_bits * 3 + rc->this_frame_target, 2);
    rc->rolling_actual_bits = ROUND_POWER_OF_TWO(
        rc->rolling_actual_bits * 3 + rc->projected_frame_size, 2);
    rc->long_rolling_target_bits = ROUND_POWER_OF_TWO(
        rc->long_rolling_target_bits * 31 + rc->this_frame_target, 5);
    rc->long_rolling_actual_bits = ROUND_POWER_OF_TWO(
        rc->long_rolling_actual_bits * 31 + rc->projected_frame_size, 5);
  }

  // Actual bits spent
  rc->total_actual_bits += rc->projected_frame_size;

  // Debug stats
  rc->total_target_vs_actual += (rc->this_frame_target -
                                 rc->projected_frame_size);

  if (cpi->oxcf.play_alternate && cpi->refresh_alt_ref_frame &&
      (cm->frame_type != KEY_FRAME))
    // Update the alternate reference frame stats as appropriate.
    update_alt_ref_frame_stats(cpi);
  else
    // Update the Golden frame stats as appropriate.
    update_golden_frame_stats(cpi);

  if (cm->frame_type == KEY_FRAME)
    rc->frames_since_key = 0;
  if (cm->show_frame) {
    rc->frames_since_key++;
    rc->frames_to_key--;
  }
}

void vp9_rc_postencode_update_drop_frame(VP9_COMP *cpi) {
  // Update buffer level with zero size, update frame counters, and return.
  update_buffer_level(cpi, 0);
  cpi->common.last_frame_type = cpi->common.frame_type;
  cpi->rc.frames_since_key++;
  cpi->rc.frames_to_key--;
}

static int test_for_kf_one_pass(VP9_COMP *cpi) {
  // Placeholder function for auto key frame
  return 0;
}
// Use this macro to turn on/off use of alt-refs in one-pass mode.
#define USE_ALTREF_FOR_ONE_PASS   1

static int calc_pframe_target_size_one_pass_vbr(const VP9_COMP *const cpi) {
  static const int af_ratio = 10;
  const RATE_CONTROL *rc = &cpi->rc;
  int target;
#if USE_ALTREF_FOR_ONE_PASS
  target = (!rc->is_src_frame_alt_ref &&
            (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) ?
      (rc->av_per_frame_bandwidth * cpi->rc.baseline_gf_interval * af_ratio) /
      (cpi->rc.baseline_gf_interval + af_ratio - 1) :
      (rc->av_per_frame_bandwidth * cpi->rc.baseline_gf_interval) /
      (cpi->rc.baseline_gf_interval + af_ratio - 1);
#else
  target = rc->av_per_frame_bandwidth;
#endif
  return vp9_rc_clamp_pframe_target_size(cpi, target);
}

static int calc_iframe_target_size_one_pass_vbr(const VP9_COMP *const cpi) {
  static const int kf_ratio = 25;
  const RATE_CONTROL *rc = &cpi->rc;
  int target = rc->av_per_frame_bandwidth * kf_ratio;
  return vp9_rc_clamp_iframe_target_size(cpi, target);
}

void vp9_rc_get_one_pass_vbr_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int target;
  if (!cpi->refresh_alt_ref_frame &&
      (cm->current_video_frame == 0 ||
       cm->frame_flags & FRAMEFLAGS_KEY ||
       rc->frames_to_key == 0 ||
       (cpi->oxcf.auto_key && test_for_kf_one_pass(cpi)))) {
    cm->frame_type = KEY_FRAME;
    rc->this_key_frame_forced = cm->current_video_frame != 0 &&
                                rc->frames_to_key == 0;
    rc->frames_to_key = cpi->key_frame_frequency;
    rc->kf_boost = DEFAULT_KF_BOOST;
    rc->source_alt_ref_active = 0;
  } else {
    cm->frame_type = INTER_FRAME;
  }
  if (rc->frames_till_gf_update_due == 0) {
    rc->baseline_gf_interval = DEFAULT_GF_INTERVAL;
    rc->frames_till_gf_update_due = rc->baseline_gf_interval;
    // NOTE: frames_till_gf_update_due must be <= frames_to_key.
    if (rc->frames_till_gf_update_due > rc->frames_to_key)
      rc->frames_till_gf_update_due = rc->frames_to_key;
    cpi->refresh_golden_frame = 1;
    rc->source_alt_ref_pending = USE_ALTREF_FOR_ONE_PASS;
    rc->gfu_boost = DEFAULT_GF_BOOST;
  }
  if (cm->frame_type == KEY_FRAME)
    target = calc_iframe_target_size_one_pass_vbr(cpi);
  else
    target = calc_pframe_target_size_one_pass_vbr(cpi);
  vp9_rc_set_frame_target(cpi, target);
}

static int calc_pframe_target_size_one_pass_cbr(const VP9_COMP *cpi) {
  const VP9_CONFIG *oxcf = &cpi->oxcf;
  const RATE_CONTROL *rc = &cpi->rc;
  const int64_t diff = oxcf->optimal_buffer_level - rc->buffer_level;
  const int one_pct_bits = 1 + oxcf->optimal_buffer_level / 100;
  int min_frame_target = MAX(rc->av_per_frame_bandwidth >> 4,
                             FRAME_OVERHEAD_BITS);
  int target = rc->av_per_frame_bandwidth;
  if (cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
    // Note that for layers, av_per_frame_bandwidth is the cumulative
    // per-frame-bandwidth. For the target size of this frame, use the
    // layer average frame size (i.e., non-cumulative per-frame-bw).
    int current_temporal_layer = cpi->svc.temporal_layer_id;
    const LAYER_CONTEXT *lc = &cpi->svc.layer_context[current_temporal_layer];
    target = lc->avg_frame_size;
    min_frame_target = MAX(lc->avg_frame_size >> 4, FRAME_OVERHEAD_BITS);
  }
  if (diff > 0) {
    // Lower the target bandwidth for this frame.
    const int pct_low = MIN(diff / one_pct_bits, oxcf->under_shoot_pct);
    target -= (target * pct_low) / 200;
  } else if (diff < 0) {
    // Increase the target bandwidth for this frame.
    const int pct_high = MIN(-diff / one_pct_bits, oxcf->over_shoot_pct);
    target += (target * pct_high) / 200;
  }
  return MAX(min_frame_target, target);
}

static int calc_iframe_target_size_one_pass_cbr(const VP9_COMP *cpi) {
  const RATE_CONTROL *rc = &cpi->rc;

  if (cpi->common.current_video_frame == 0) {
    return cpi->oxcf.starting_buffer_level / 2;
  } else {
    const int initial_boost = 32;
    int kf_boost = MAX(initial_boost, (int)(2 * cpi->output_framerate - 16));
    if (rc->frames_since_key < cpi->output_framerate / 2) {
      kf_boost = (int)(kf_boost * rc->frames_since_key /
                       (cpi->output_framerate / 2));
    }
    return ((16 + kf_boost) * rc->av_per_frame_bandwidth) >> 4;
  }
}

void vp9_rc_get_svc_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  int target = cpi->rc.av_per_frame_bandwidth;
  if ((cm->current_video_frame == 0) ||
      (cm->frame_flags & FRAMEFLAGS_KEY) ||
      (cpi->oxcf.auto_key && (cpi->rc.frames_since_key %
                              cpi->key_frame_frequency == 0))) {
    cm->frame_type = KEY_FRAME;
    cpi->rc.source_alt_ref_active = 0;
    if (cpi->pass == 0 && cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
      target = calc_iframe_target_size_one_pass_cbr(cpi);
    }
  } else {
    cm->frame_type = INTER_FRAME;
    if (cpi->pass == 0 && cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
      target = calc_pframe_target_size_one_pass_cbr(cpi);
    }
  }
  vp9_rc_set_frame_target(cpi, target);
  cpi->rc.frames_till_gf_update_due = INT_MAX;
  cpi->rc.baseline_gf_interval = INT_MAX;
}

void vp9_rc_get_one_pass_cbr_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int target;
  if ((cm->current_video_frame == 0 ||
      cm->frame_flags & FRAMEFLAGS_KEY ||
      rc->frames_to_key == 0 ||
      (cpi->oxcf.auto_key && test_for_kf_one_pass(cpi)))) {
    cm->frame_type = KEY_FRAME;
    rc->this_key_frame_forced = cm->current_video_frame != 0 &&
                                rc->frames_to_key == 0;
    rc->frames_to_key = cpi->key_frame_frequency;
    rc->kf_boost = DEFAULT_KF_BOOST;
    rc->source_alt_ref_active = 0;
    target = calc_iframe_target_size_one_pass_cbr(cpi);
  } else {
    cm->frame_type = INTER_FRAME;
    target = calc_pframe_target_size_one_pass_cbr(cpi);
  }
  vp9_rc_set_frame_target(cpi, target);
  // Don't use gf_update by default in CBR mode.
  rc->frames_till_gf_update_due = INT_MAX;
  rc->baseline_gf_interval = INT_MAX;
}
