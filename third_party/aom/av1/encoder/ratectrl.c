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

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/alloccommon.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/common/common.h"
#include "av1/common/entropymode.h"
#include "av1/common/quant_common.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/encodemv.h"
#include "av1/encoder/random.h"
#include "av1/encoder/ratectrl.h"

// Max rate target for 1080P and below encodes under normal circumstances
// (1920 * 1080 / (16 * 16)) * MAX_MB_RATE bits per MB
#define MAX_MB_RATE 250
#define MAXRATE_1080P 2025000

#define DEFAULT_KF_BOOST 2000
#define DEFAULT_GF_BOOST 2000

#define MIN_BPB_FACTOR 0.005
#define MAX_BPB_FACTOR 50

#define FRAME_OVERHEAD_BITS 200
#define ASSIGN_MINQ_TABLE(bit_depth, name)                   \
  do {                                                       \
    switch (bit_depth) {                                     \
      case AOM_BITS_8: name = name##_8; break;               \
      case AOM_BITS_10: name = name##_10; break;             \
      case AOM_BITS_12: name = name##_12; break;             \
      default:                                               \
        assert(0 &&                                          \
               "bit_depth should be AOM_BITS_8, AOM_BITS_10" \
               " or AOM_BITS_12");                           \
        name = NULL;                                         \
    }                                                        \
  } while (0)

// Tables relating active max Q to active min Q
static int kf_low_motion_minq_8[QINDEX_RANGE];
static int kf_high_motion_minq_8[QINDEX_RANGE];
static int arfgf_low_motion_minq_8[QINDEX_RANGE];
static int arfgf_high_motion_minq_8[QINDEX_RANGE];
static int inter_minq_8[QINDEX_RANGE];
static int rtc_minq_8[QINDEX_RANGE];

static int kf_low_motion_minq_10[QINDEX_RANGE];
static int kf_high_motion_minq_10[QINDEX_RANGE];
static int arfgf_low_motion_minq_10[QINDEX_RANGE];
static int arfgf_high_motion_minq_10[QINDEX_RANGE];
static int inter_minq_10[QINDEX_RANGE];
static int rtc_minq_10[QINDEX_RANGE];
static int kf_low_motion_minq_12[QINDEX_RANGE];
static int kf_high_motion_minq_12[QINDEX_RANGE];
static int arfgf_low_motion_minq_12[QINDEX_RANGE];
static int arfgf_high_motion_minq_12[QINDEX_RANGE];
static int inter_minq_12[QINDEX_RANGE];
static int rtc_minq_12[QINDEX_RANGE];

static int gf_high = 2000;
static int gf_low = 400;
static int kf_high = 5000;
static int kf_low = 400;

// How many times less pixels there are to encode given the current scaling.
// Temporary replacement for rcf_mult and rate_thresh_mult.
static double resize_rate_factor(const AV1_COMP *cpi, int width, int height) {
  return (double)(cpi->oxcf.width * cpi->oxcf.height) / (width * height);
}

// Functions to compute the active minq lookup table entries based on a
// formulaic approach to facilitate easier adjustment of the Q tables.
// The formulae were derived from computing a 3rd order polynomial best
// fit to the original data (after plotting real maxq vs minq (not q index))
static int get_minq_index(double maxq, double x3, double x2, double x1,
                          aom_bit_depth_t bit_depth) {
  int i;
  const double minqtarget = AOMMIN(((x3 * maxq + x2) * maxq + x1) * maxq, maxq);

  // Special case handling to deal with the step from q2.0
  // down to lossless mode represented by q 1.0.
  if (minqtarget <= 2.0) return 0;

  for (i = 0; i < QINDEX_RANGE; i++) {
    if (minqtarget <= av1_convert_qindex_to_q(i, bit_depth)) return i;
  }

  return QINDEX_RANGE - 1;
}

static void init_minq_luts(int *kf_low_m, int *kf_high_m, int *arfgf_low,
                           int *arfgf_high, int *inter, int *rtc,
                           aom_bit_depth_t bit_depth) {
  int i;
  for (i = 0; i < QINDEX_RANGE; i++) {
    const double maxq = av1_convert_qindex_to_q(i, bit_depth);
    kf_low_m[i] = get_minq_index(maxq, 0.000001, -0.0004, 0.150, bit_depth);
    kf_high_m[i] = get_minq_index(maxq, 0.0000021, -0.00125, 0.55, bit_depth);
    arfgf_low[i] = get_minq_index(maxq, 0.0000015, -0.0009, 0.30, bit_depth);
    arfgf_high[i] = get_minq_index(maxq, 0.0000021, -0.00125, 0.55, bit_depth);
    inter[i] = get_minq_index(maxq, 0.00000271, -0.00113, 0.90, bit_depth);
    rtc[i] = get_minq_index(maxq, 0.00000271, -0.00113, 0.70, bit_depth);
  }
}

void av1_rc_init_minq_luts(void) {
  init_minq_luts(kf_low_motion_minq_8, kf_high_motion_minq_8,
                 arfgf_low_motion_minq_8, arfgf_high_motion_minq_8,
                 inter_minq_8, rtc_minq_8, AOM_BITS_8);
  init_minq_luts(kf_low_motion_minq_10, kf_high_motion_minq_10,
                 arfgf_low_motion_minq_10, arfgf_high_motion_minq_10,
                 inter_minq_10, rtc_minq_10, AOM_BITS_10);
  init_minq_luts(kf_low_motion_minq_12, kf_high_motion_minq_12,
                 arfgf_low_motion_minq_12, arfgf_high_motion_minq_12,
                 inter_minq_12, rtc_minq_12, AOM_BITS_12);
}

// These functions use formulaic calculations to make playing with the
// quantizer tables easier. If necessary they can be replaced by lookup
// tables if and when things settle down in the experimental bitstream
double av1_convert_qindex_to_q(int qindex, aom_bit_depth_t bit_depth) {
  // Convert the index to a real Q value (scaled down to match old Q values)
  switch (bit_depth) {
    case AOM_BITS_8: return av1_ac_quant_Q3(qindex, 0, bit_depth) / 4.0;
    case AOM_BITS_10: return av1_ac_quant_Q3(qindex, 0, bit_depth) / 16.0;
    case AOM_BITS_12: return av1_ac_quant_Q3(qindex, 0, bit_depth) / 64.0;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1.0;
  }
}

int av1_rc_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                       double correction_factor, aom_bit_depth_t bit_depth) {
  const double q = av1_convert_qindex_to_q(qindex, bit_depth);
  int enumerator = frame_type == KEY_FRAME ? 2700000 : 1800000;

  assert(correction_factor <= MAX_BPB_FACTOR &&
         correction_factor >= MIN_BPB_FACTOR);

  // q based adjustment to baseline enumerator
  enumerator += (int)(enumerator * q) >> 12;
  return (int)(enumerator * correction_factor / q);
}

int av1_estimate_bits_at_q(FRAME_TYPE frame_type, int q, int mbs,
                           double correction_factor,
                           aom_bit_depth_t bit_depth) {
  const int bpm =
      (int)(av1_rc_bits_per_mb(frame_type, q, correction_factor, bit_depth));
  return AOMMAX(FRAME_OVERHEAD_BITS,
                (int)((uint64_t)bpm * mbs) >> BPER_MB_NORMBITS);
}

int av1_rc_clamp_pframe_target_size(const AV1_COMP *const cpi, int target) {
  const RATE_CONTROL *rc = &cpi->rc;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  const int min_frame_target =
      AOMMAX(rc->min_frame_bandwidth, rc->avg_frame_bandwidth >> 5);
  // Clip the frame target to the minimum setup value.
  if (cpi->rc.is_src_frame_alt_ref) {
    // If there is an active ARF at this location use the minimum
    // bits on this frame even if it is a constructed arf.
    // The active maximum quantizer insures that an appropriate
    // number of bits will be spent if needed for constructed ARFs.
    target = min_frame_target;
  } else if (target < min_frame_target) {
    target = min_frame_target;
  }

  // Clip the frame target to the maximum allowed value.
  if (target > rc->max_frame_bandwidth) target = rc->max_frame_bandwidth;
  if (oxcf->rc_max_inter_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * oxcf->rc_max_inter_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }

  return target;
}

int av1_rc_clamp_iframe_target_size(const AV1_COMP *const cpi, int target) {
  const RATE_CONTROL *rc = &cpi->rc;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  if (oxcf->rc_max_intra_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * oxcf->rc_max_intra_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }
  if (target > rc->max_frame_bandwidth) target = rc->max_frame_bandwidth;
  return target;
}

// Update the buffer level: leaky bucket model.
static void update_buffer_level(AV1_COMP *cpi, int encoded_frame_size) {
  const AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;

  // Non-viewable frames are a special case and are treated as pure overhead.
  // TODO(zoeliu): To further explore whether we should treat BWDREF_FRAME
  //               differently, since it is a no-show frame.
  if (!cm->show_frame && !rc->is_bwd_ref_frame)
    rc->bits_off_target -= encoded_frame_size;
  else
    rc->bits_off_target += rc->avg_frame_bandwidth - encoded_frame_size;

  // Clip the buffer level to the maximum specified buffer size.
  rc->bits_off_target = AOMMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = rc->bits_off_target;
}

int av1_rc_get_default_min_gf_interval(int width, int height,
                                       double framerate) {
  // Assume we do not need any constraint lower than 4K 20 fps
  static const double factor_safe = 3840 * 2160 * 20.0;
  const double factor = width * height * framerate;
  const int default_interval =
      clamp((int)(framerate * 0.125), MIN_GF_INTERVAL, MAX_GF_INTERVAL);

  if (factor <= factor_safe)
    return default_interval;
  else
    return AOMMAX(default_interval,
                  (int)(MIN_GF_INTERVAL * factor / factor_safe + 0.5));
  // Note this logic makes:
  // 4K24: 5
  // 4K30: 6
  // 4K60: 12
}

int av1_rc_get_default_max_gf_interval(double framerate, int min_gf_interval) {
  int interval = AOMMIN(MAX_GF_INTERVAL, (int)(framerate * 0.75));
  interval += (interval & 0x01);  // Round to even value
  return AOMMAX(interval, min_gf_interval);
}

void av1_rc_init(const AV1EncoderConfig *oxcf, int pass, RATE_CONTROL *rc) {
  int i;

  if (pass == 0 && oxcf->rc_mode == AOM_CBR) {
    rc->avg_frame_qindex[KEY_FRAME] = oxcf->worst_allowed_q;
    rc->avg_frame_qindex[INTER_FRAME] = oxcf->worst_allowed_q;
  } else {
    rc->avg_frame_qindex[KEY_FRAME] =
        (oxcf->worst_allowed_q + oxcf->best_allowed_q) / 2;
    rc->avg_frame_qindex[INTER_FRAME] =
        (oxcf->worst_allowed_q + oxcf->best_allowed_q) / 2;
  }

  rc->last_q[KEY_FRAME] = oxcf->best_allowed_q;
  rc->last_q[INTER_FRAME] = oxcf->worst_allowed_q;

  rc->buffer_level = rc->starting_buffer_level;
  rc->bits_off_target = rc->starting_buffer_level;

  rc->rolling_target_bits = rc->avg_frame_bandwidth;
  rc->rolling_actual_bits = rc->avg_frame_bandwidth;
  rc->long_rolling_target_bits = rc->avg_frame_bandwidth;
  rc->long_rolling_actual_bits = rc->avg_frame_bandwidth;

  rc->total_actual_bits = 0;
  rc->total_target_bits = 0;
  rc->total_target_vs_actual = 0;

  rc->frames_since_key = 8;  // Sensible default for first frame.
  rc->this_key_frame_forced = 0;
  rc->next_key_frame_forced = 0;
  rc->source_alt_ref_pending = 0;
  rc->source_alt_ref_active = 0;

  rc->frames_till_gf_update_due = 0;
  rc->ni_av_qi = oxcf->worst_allowed_q;
  rc->ni_tot_qi = 0;
  rc->ni_frames = 0;

  rc->tot_q = 0.0;
  rc->avg_q = av1_convert_qindex_to_q(oxcf->worst_allowed_q, oxcf->bit_depth);

  for (i = 0; i < RATE_FACTOR_LEVELS; ++i) {
    rc->rate_correction_factors[i] = 1.0;
  }

  rc->min_gf_interval = oxcf->min_gf_interval;
  rc->max_gf_interval = oxcf->max_gf_interval;
  if (rc->min_gf_interval == 0)
    rc->min_gf_interval = av1_rc_get_default_min_gf_interval(
        oxcf->width, oxcf->height, oxcf->init_framerate);
  if (rc->max_gf_interval == 0)
    rc->max_gf_interval = av1_rc_get_default_max_gf_interval(
        oxcf->init_framerate, rc->min_gf_interval);
  rc->baseline_gf_interval = (rc->min_gf_interval + rc->max_gf_interval) / 2;
}

int av1_rc_drop_frame(AV1_COMP *cpi) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
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
      int drop_mark =
          (int)(oxcf->drop_frames_water_mark * rc->optimal_buffer_level / 100);
      if ((rc->buffer_level > drop_mark) && (rc->decimation_factor > 0)) {
        --rc->decimation_factor;
      } else if (rc->buffer_level <= drop_mark && rc->decimation_factor == 0) {
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

static double get_rate_correction_factor(const AV1_COMP *cpi, int width,
                                         int height) {
  const RATE_CONTROL *const rc = &cpi->rc;
  double rcf;

  if (cpi->common.frame_type == KEY_FRAME) {
    rcf = rc->rate_correction_factors[KF_STD];
  } else if (cpi->oxcf.pass == 2) {
    RATE_FACTOR_LEVEL rf_lvl =
        cpi->twopass.gf_group.rf_level[cpi->twopass.gf_group.index];
    rcf = rc->rate_correction_factors[rf_lvl];
  } else {
    if ((cpi->refresh_alt_ref_frame || cpi->refresh_golden_frame) &&
        !rc->is_src_frame_alt_ref &&
        (cpi->oxcf.rc_mode != AOM_CBR || cpi->oxcf.gf_cbr_boost_pct > 20))
      rcf = rc->rate_correction_factors[GF_ARF_STD];
    else
      rcf = rc->rate_correction_factors[INTER_NORMAL];
  }
  rcf *= resize_rate_factor(cpi, width, height);
  return fclamp(rcf, MIN_BPB_FACTOR, MAX_BPB_FACTOR);
}

static void set_rate_correction_factor(AV1_COMP *cpi, double factor, int width,
                                       int height) {
  RATE_CONTROL *const rc = &cpi->rc;

  // Normalize RCF to account for the size-dependent scaling factor.
  factor /= resize_rate_factor(cpi, width, height);

  factor = fclamp(factor, MIN_BPB_FACTOR, MAX_BPB_FACTOR);

  if (cpi->common.frame_type == KEY_FRAME) {
    rc->rate_correction_factors[KF_STD] = factor;
  } else if (cpi->oxcf.pass == 2) {
    RATE_FACTOR_LEVEL rf_lvl =
        cpi->twopass.gf_group.rf_level[cpi->twopass.gf_group.index];
    rc->rate_correction_factors[rf_lvl] = factor;
  } else {
    if ((cpi->refresh_alt_ref_frame || cpi->refresh_golden_frame) &&
        !rc->is_src_frame_alt_ref &&
        (cpi->oxcf.rc_mode != AOM_CBR || cpi->oxcf.gf_cbr_boost_pct > 20))
      rc->rate_correction_factors[GF_ARF_STD] = factor;
    else
      rc->rate_correction_factors[INTER_NORMAL] = factor;
  }
}

void av1_rc_update_rate_correction_factors(AV1_COMP *cpi, int width,
                                           int height) {
  const AV1_COMMON *const cm = &cpi->common;
  int correction_factor = 100;
  double rate_correction_factor =
      get_rate_correction_factor(cpi, width, height);
  double adjustment_limit;
  const int MBs = av1_get_MBs(width, height);

  int projected_size_based_on_q = 0;

  // Do not update the rate factors for arf overlay frames.
  if (cpi->rc.is_src_frame_alt_ref) return;

  // Clear down mmx registers to allow floating point in what follows
  aom_clear_system_state();

  // Work out how big we would have expected the frame to be at this Q given
  // the current correction factor.
  // Stay in double to avoid int overflow when values are large
  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ && cpi->common.seg.enabled) {
    projected_size_based_on_q =
        av1_cyclic_refresh_estimate_bits_at_q(cpi, rate_correction_factor);
  } else {
    projected_size_based_on_q = av1_estimate_bits_at_q(
        cpi->common.frame_type, cm->base_qindex, MBs, rate_correction_factor,
        cm->seq_params.bit_depth);
  }
  // Work out a size correction factor.
  if (projected_size_based_on_q > FRAME_OVERHEAD_BITS)
    correction_factor = (int)((100 * (int64_t)cpi->rc.projected_frame_size) /
                              projected_size_based_on_q);

  // More heavily damped adjustment used if we have been oscillating either side
  // of target.
  if (correction_factor > 0) {
    adjustment_limit =
        0.25 + 0.5 * AOMMIN(1, fabs(log10(0.01 * correction_factor)));
  } else {
    adjustment_limit = 0.75;
  }

  cpi->rc.q_2_frame = cpi->rc.q_1_frame;
  cpi->rc.q_1_frame = cm->base_qindex;
  cpi->rc.rc_2_frame = cpi->rc.rc_1_frame;
  if (correction_factor > 110)
    cpi->rc.rc_1_frame = -1;
  else if (correction_factor < 90)
    cpi->rc.rc_1_frame = 1;
  else
    cpi->rc.rc_1_frame = 0;

  if (correction_factor > 102) {
    // We are not already at the worst allowable quality
    correction_factor =
        (int)(100 + ((correction_factor - 100) * adjustment_limit));
    rate_correction_factor = (rate_correction_factor * correction_factor) / 100;
    // Keep rate_correction_factor within limits
    if (rate_correction_factor > MAX_BPB_FACTOR)
      rate_correction_factor = MAX_BPB_FACTOR;
  } else if (correction_factor < 99) {
    // We are not already at the best allowable quality
    correction_factor =
        (int)(100 - ((100 - correction_factor) * adjustment_limit));
    rate_correction_factor = (rate_correction_factor * correction_factor) / 100;

    // Keep rate_correction_factor within limits
    if (rate_correction_factor < MIN_BPB_FACTOR)
      rate_correction_factor = MIN_BPB_FACTOR;
  }

  set_rate_correction_factor(cpi, rate_correction_factor, width, height);
}

int av1_rc_regulate_q(const AV1_COMP *cpi, int target_bits_per_frame,
                      int active_best_quality, int active_worst_quality,
                      int width, int height) {
  const AV1_COMMON *const cm = &cpi->common;
  int q = active_worst_quality;
  int last_error = INT_MAX;
  int i, target_bits_per_mb, bits_per_mb_at_this_q;
  const int MBs = av1_get_MBs(width, height);
  const double correction_factor =
      get_rate_correction_factor(cpi, width, height);

  // Calculate required scaling factor based on target frame size and size of
  // frame produced using previous Q.
  target_bits_per_mb =
      (int)((uint64_t)(target_bits_per_frame) << BPER_MB_NORMBITS) / MBs;

  i = active_best_quality;

  do {
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ && cm->seg.enabled) {
      bits_per_mb_at_this_q =
          (int)av1_cyclic_refresh_rc_bits_per_mb(cpi, i, correction_factor);
    } else {
      bits_per_mb_at_this_q = (int)av1_rc_bits_per_mb(
          cm->frame_type, i, correction_factor, cm->seq_params.bit_depth);
    }

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

  // In CBR mode, this makes sure q is between oscillating Qs to prevent
  // resonance.
  if (cpi->oxcf.rc_mode == AOM_CBR &&
      (cpi->rc.rc_1_frame * cpi->rc.rc_2_frame == -1) &&
      cpi->rc.q_1_frame != cpi->rc.q_2_frame) {
    q = clamp(q, AOMMIN(cpi->rc.q_1_frame, cpi->rc.q_2_frame),
              AOMMAX(cpi->rc.q_1_frame, cpi->rc.q_2_frame));
  }
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

static int get_kf_active_quality(const RATE_CONTROL *const rc, int q,
                                 aom_bit_depth_t bit_depth) {
  int *kf_low_motion_minq;
  int *kf_high_motion_minq;
  ASSIGN_MINQ_TABLE(bit_depth, kf_low_motion_minq);
  ASSIGN_MINQ_TABLE(bit_depth, kf_high_motion_minq);
  return get_active_quality(q, rc->kf_boost, kf_low, kf_high,
                            kf_low_motion_minq, kf_high_motion_minq);
}

static int get_gf_active_quality(const RATE_CONTROL *const rc, int q,
                                 aom_bit_depth_t bit_depth) {
  int *arfgf_low_motion_minq;
  int *arfgf_high_motion_minq;
  ASSIGN_MINQ_TABLE(bit_depth, arfgf_low_motion_minq);
  ASSIGN_MINQ_TABLE(bit_depth, arfgf_high_motion_minq);
  return get_active_quality(q, rc->gfu_boost, gf_low, gf_high,
                            arfgf_low_motion_minq, arfgf_high_motion_minq);
}

static int calc_active_worst_quality_one_pass_vbr(const AV1_COMP *cpi) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const unsigned int curr_frame = cpi->common.current_video_frame;
  int active_worst_quality;

  if (cpi->common.frame_type == KEY_FRAME) {
    active_worst_quality =
        curr_frame == 0 ? rc->worst_quality : rc->last_q[KEY_FRAME] * 2;
  } else {
    if (!rc->is_src_frame_alt_ref &&
        (cpi->refresh_golden_frame || cpi->refresh_alt2_ref_frame ||
         cpi->refresh_alt_ref_frame)) {
      active_worst_quality = curr_frame == 1 ? rc->last_q[KEY_FRAME] * 5 / 4
                                             : rc->last_q[INTER_FRAME];
    } else {
      active_worst_quality = curr_frame == 1 ? rc->last_q[KEY_FRAME] * 2
                                             : rc->last_q[INTER_FRAME] * 2;
    }
  }
  return AOMMIN(active_worst_quality, rc->worst_quality);
}

// Adjust active_worst_quality level based on buffer level.
static int calc_active_worst_quality_one_pass_cbr(const AV1_COMP *cpi) {
  // Adjust active_worst_quality: If buffer is above the optimal/target level,
  // bring active_worst_quality down depending on fullness of buffer.
  // If buffer is below the optimal level, let the active_worst_quality go from
  // ambient Q (at buffer = optimal level) to worst_quality level
  // (at buffer = critical level).
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *rc = &cpi->rc;
  // Buffer level below which we push active_worst to worst_quality.
  int64_t critical_level = rc->optimal_buffer_level >> 3;
  int64_t buff_lvl_step = 0;
  int adjustment = 0;
  int active_worst_quality;
  int ambient_qp;
  if (cm->frame_type == KEY_FRAME) return rc->worst_quality;
  // For ambient_qp we use minimum of avg_frame_qindex[KEY_FRAME/INTER_FRAME]
  // for the first few frames following key frame. These are both initialized
  // to worst_quality and updated with (3/4, 1/4) average in postencode_update.
  // So for first few frames following key, the qp of that key frame is weighted
  // into the active_worst_quality setting.
  ambient_qp = (cm->current_video_frame < 5)
                   ? AOMMIN(rc->avg_frame_qindex[INTER_FRAME],
                            rc->avg_frame_qindex[KEY_FRAME])
                   : rc->avg_frame_qindex[INTER_FRAME];
  active_worst_quality = AOMMIN(rc->worst_quality, ambient_qp * 5 / 4);
  if (rc->buffer_level > rc->optimal_buffer_level) {
    // Adjust down.
    // Maximum limit for down adjustment, ~30%.
    int max_adjustment_down = active_worst_quality / 3;
    if (max_adjustment_down) {
      buff_lvl_step = ((rc->maximum_buffer_size - rc->optimal_buffer_level) /
                       max_adjustment_down);
      if (buff_lvl_step)
        adjustment = (int)((rc->buffer_level - rc->optimal_buffer_level) /
                           buff_lvl_step);
      active_worst_quality -= adjustment;
    }
  } else if (rc->buffer_level > critical_level) {
    // Adjust up from ambient Q.
    if (critical_level) {
      buff_lvl_step = (rc->optimal_buffer_level - critical_level);
      if (buff_lvl_step) {
        adjustment = (int)((rc->worst_quality - ambient_qp) *
                           (rc->optimal_buffer_level - rc->buffer_level) /
                           buff_lvl_step);
      }
      active_worst_quality = ambient_qp + adjustment;
    }
  } else {
    // Set to worst_quality if buffer is below critical level.
    active_worst_quality = rc->worst_quality;
  }
  return active_worst_quality;
}

static int rc_pick_q_and_bounds_one_pass_cbr(const AV1_COMP *cpi, int width,
                                             int height, int *bottom_index,
                                             int *top_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  int active_best_quality;
  int active_worst_quality = calc_active_worst_quality_one_pass_cbr(cpi);
  int q;
  int *rtc_minq;
  const int bit_depth = cm->seq_params.bit_depth;
  ASSIGN_MINQ_TABLE(bit_depth, rtc_minq);

  if (frame_is_intra_only(cm)) {
    active_best_quality = rc->best_quality;
    // Handle the special case for key frames forced when we have reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (rc->this_key_frame_forced) {
      int qindex = rc->last_boosted_qindex;
      double last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
      int delta_qindex = av1_compute_qdelta(rc, last_boosted_q,
                                            (last_boosted_q * 0.75), bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else if (cm->current_video_frame > 0) {
      // not first frame of one pass and kf_boost is set
      double q_adj_factor = 1.0;
      double q_val;

      active_best_quality =
          get_kf_active_quality(rc, rc->avg_frame_qindex[KEY_FRAME], bit_depth);

      // Allow somewhat lower kf minq with small image formats.
      if ((width * height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = av1_convert_qindex_to_q(active_best_quality, bit_depth);
      active_best_quality +=
          av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);
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
    active_best_quality = get_gf_active_quality(rc, q, bit_depth);
  } else {
    // Use the lower of active_worst_quality and recent/average Q.
    if (cm->current_video_frame > 1) {
      if (rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality)
        active_best_quality = rtc_minq[rc->avg_frame_qindex[INTER_FRAME]];
      else
        active_best_quality = rtc_minq[active_worst_quality];
    } else {
      if (rc->avg_frame_qindex[KEY_FRAME] < active_worst_quality)
        active_best_quality = rtc_minq[rc->avg_frame_qindex[KEY_FRAME]];
      else
        active_best_quality = rtc_minq[active_worst_quality];
    }
  }

  // Clip the active best and worst quality values to limits
  active_best_quality =
      clamp(active_best_quality, rc->best_quality, rc->worst_quality);
  active_worst_quality =
      clamp(active_worst_quality, active_best_quality, rc->worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

  // Limit Q range for the adaptive loop.
  if (cm->frame_type == KEY_FRAME && !rc->this_key_frame_forced &&
      !(cm->current_video_frame == 0)) {
    int qdelta = 0;
    aom_clear_system_state();
    qdelta = av1_compute_qdelta_by_rate(&cpi->rc, cm->frame_type,
                                        active_worst_quality, 2.0, bit_depth);
    *top_index = active_worst_quality + qdelta;
    *top_index = AOMMAX(*top_index, *bottom_index);
  }

  // Special case code to try and match quality with forced key frames
  if (cm->frame_type == KEY_FRAME && rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = av1_rc_regulate_q(cpi, rc->this_frame_target, active_best_quality,
                          active_worst_quality, width, height);
    if (q > *top_index) {
      // Special case when we are targeting the max allowed rate
      if (rc->this_frame_target >= rc->max_frame_bandwidth)
        *top_index = q;
      else
        q = *top_index;
    }
  }

  assert(*top_index <= rc->worst_quality && *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

static int get_active_cq_level(const RATE_CONTROL *rc,
                               const AV1EncoderConfig *const oxcf) {
  static const double cq_adjust_threshold = 0.1;
  int active_cq_level = oxcf->cq_level;
  if (oxcf->rc_mode == AOM_CQ && rc->total_target_bits > 0) {
    const double x = (double)rc->total_actual_bits / rc->total_target_bits;
    if (x < cq_adjust_threshold) {
      active_cq_level = (int)(active_cq_level * x / cq_adjust_threshold);
    }
  }
  return active_cq_level;
}

static int rc_pick_q_and_bounds_one_pass_vbr(const AV1_COMP *cpi, int width,
                                             int height, int *bottom_index,
                                             int *top_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const int cq_level = get_active_cq_level(rc, oxcf);
  int active_best_quality;
  int active_worst_quality = calc_active_worst_quality_one_pass_vbr(cpi);
  int q;
  int *inter_minq;
  const int bit_depth = cm->seq_params.bit_depth;
  ASSIGN_MINQ_TABLE(bit_depth, inter_minq);

  if (frame_is_intra_only(cm)) {
    if (oxcf->rc_mode == AOM_Q) {
      const int qindex = cq_level;
      const double q_val = av1_convert_qindex_to_q(qindex, bit_depth);
      const int delta_qindex =
          av1_compute_qdelta(rc, q_val, q_val * 0.25, bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else if (rc->this_key_frame_forced) {
      const int qindex = rc->last_boosted_qindex;
      const double last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
      const int delta_qindex = av1_compute_qdelta(
          rc, last_boosted_q, last_boosted_q * 0.75, bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else {  // not first frame of one pass and kf_boost is set
      double q_adj_factor = 1.0;

      active_best_quality =
          get_kf_active_quality(rc, rc->avg_frame_qindex[KEY_FRAME], bit_depth);

      // Allow somewhat lower kf minq with small image formats.
      if ((width * height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Convert the adjustment factor to a qindex delta on active_best_quality.
      {
        const double q_val =
            av1_convert_qindex_to_q(active_best_quality, bit_depth);
        active_best_quality +=
            av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);
      }
    }
  } else if (!rc->is_src_frame_alt_ref &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    // Use the lower of active_worst_quality and recent
    // average Q as basis for GF/ARF best Q limit unless last frame was
    // a key frame.
    q = (rc->frames_since_key > 1 &&
         rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality)
            ? rc->avg_frame_qindex[INTER_FRAME]
            : rc->avg_frame_qindex[KEY_FRAME];
    // For constrained quality dont allow Q less than the cq level
    if (oxcf->rc_mode == AOM_CQ) {
      if (q < cq_level) q = cq_level;
      active_best_quality = get_gf_active_quality(rc, q, bit_depth);
      // Constrained quality use slightly lower active best.
      active_best_quality = active_best_quality * 15 / 16;
    } else if (oxcf->rc_mode == AOM_Q) {
      const int qindex = cq_level;
      const double q_val = av1_convert_qindex_to_q(qindex, bit_depth);
      const int delta_qindex =
          (cpi->refresh_alt_ref_frame)
              ? av1_compute_qdelta(rc, q_val, q_val * 0.40, bit_depth)
              : av1_compute_qdelta(rc, q_val, q_val * 0.50, bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else {
      active_best_quality = get_gf_active_quality(rc, q, bit_depth);
    }
  } else {
    if (oxcf->rc_mode == AOM_Q) {
      const int qindex = cq_level;
      const double q_val = av1_convert_qindex_to_q(qindex, bit_depth);
      const double delta_rate[FIXED_GF_INTERVAL] = { 0.50, 1.0, 0.85, 1.0,
                                                     0.70, 1.0, 0.85, 1.0 };
      const int delta_qindex = av1_compute_qdelta(
          rc, q_val,
          q_val * delta_rate[cm->current_video_frame % FIXED_GF_INTERVAL],
          bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else {
      // Use the lower of active_worst_quality and recent/average Q.
      active_best_quality = (cm->current_video_frame > 1)
                                ? inter_minq[rc->avg_frame_qindex[INTER_FRAME]]
                                : inter_minq[rc->avg_frame_qindex[KEY_FRAME]];
      // For the constrained quality mode we don't want
      // q to fall below the cq level.
      if ((oxcf->rc_mode == AOM_CQ) && (active_best_quality < cq_level)) {
        active_best_quality = cq_level;
      }
    }
  }

  // Clip the active best and worst quality values to limits
  active_best_quality =
      clamp(active_best_quality, rc->best_quality, rc->worst_quality);
  active_worst_quality =
      clamp(active_worst_quality, active_best_quality, rc->worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

  // Limit Q range for the adaptive loop.
  {
    int qdelta = 0;
    aom_clear_system_state();
    if (cm->frame_type == KEY_FRAME && !rc->this_key_frame_forced &&
        !(cm->current_video_frame == 0)) {
      qdelta = av1_compute_qdelta_by_rate(&cpi->rc, cm->frame_type,
                                          active_worst_quality, 2.0, bit_depth);
    } else if (!rc->is_src_frame_alt_ref &&
               (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
      qdelta = av1_compute_qdelta_by_rate(
          &cpi->rc, cm->frame_type, active_worst_quality, 1.75, bit_depth);
    }
    *top_index = active_worst_quality + qdelta;
    *top_index = AOMMAX(*top_index, *bottom_index);
  }

  if (oxcf->rc_mode == AOM_Q) {
    q = active_best_quality;
    // Special case code to try and match quality with forced key frames
  } else if ((cm->frame_type == KEY_FRAME) && rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = av1_rc_regulate_q(cpi, rc->this_frame_target, active_best_quality,
                          active_worst_quality, width, height);
    if (q > *top_index) {
      // Special case when we are targeting the max allowed rate
      if (rc->this_frame_target >= rc->max_frame_bandwidth)
        *top_index = q;
      else
        q = *top_index;
    }
  }

  assert(*top_index <= rc->worst_quality && *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

int av1_frame_type_qdelta(const AV1_COMP *cpi, int rf_level, int q) {
  static const FRAME_TYPE frame_type[RATE_FACTOR_LEVELS] = {
    INTER_FRAME, INTER_FRAME, INTER_FRAME, INTER_FRAME, INTER_FRAME, KEY_FRAME
  };
  const AV1_COMMON *const cm = &cpi->common;
  int qdelta = av1_compute_qdelta_by_rate(&cpi->rc, frame_type[rf_level], q,
                                          rate_factor_deltas[rf_level],
                                          cm->seq_params.bit_depth);
  return qdelta;
}

#define STATIC_MOTION_THRESH 95
static int rc_pick_q_and_bounds_two_pass(const AV1_COMP *cpi, int width,
                                         int height, int *bottom_index,
                                         int *top_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const GF_GROUP *gf_group = &cpi->twopass.gf_group;
  const int cq_level = get_active_cq_level(rc, oxcf);
  int active_best_quality;
  int active_worst_quality = cpi->twopass.active_worst_quality;
  int q;
  int *inter_minq;
  const int bit_depth = cm->seq_params.bit_depth;
  ASSIGN_MINQ_TABLE(bit_depth, inter_minq);

#if CUSTOMIZED_GF
  const int is_intrl_arf_boost =
      gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE;
#else
  const int is_intrl_arf_boost = cpi->refresh_alt2_ref_frame;
#endif  // CUSTOMIZED_GF

  if (frame_is_intra_only(cm)) {
    // Handle the special case for key frames forced when we have reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (rc->this_key_frame_forced) {
      double last_boosted_q;
      int delta_qindex;
      int qindex;

      if (cpi->twopass.last_kfgroup_zeromotion_pct >= STATIC_MOTION_THRESH) {
        qindex = AOMMIN(rc->last_kf_qindex, rc->last_boosted_qindex);
        active_best_quality = qindex;
        last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
        delta_qindex = av1_compute_qdelta(rc, last_boosted_q,
                                          last_boosted_q * 1.25, bit_depth);
        active_worst_quality =
            AOMMIN(qindex + delta_qindex, active_worst_quality);
      } else {
        qindex = rc->last_boosted_qindex;
        last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
        delta_qindex = av1_compute_qdelta(rc, last_boosted_q,
                                          last_boosted_q * 0.75, bit_depth);
        active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
      }
    } else {
      // Not forced keyframe.
      double q_adj_factor = 1.0;
      double q_val;

      // Baseline value derived from cpi->active_worst_quality and kf boost.
      active_best_quality =
          get_kf_active_quality(rc, active_worst_quality, bit_depth);

      // Allow somewhat lower kf minq with small image formats.
      if ((width * height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Make a further adjustment based on the kf zero motion measure.
      q_adj_factor += 0.05 - (0.001 * (double)cpi->twopass.kf_zeromotion_pct);

      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = av1_convert_qindex_to_q(active_best_quality, bit_depth);
      active_best_quality +=
          av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);
    }
  } else if (!rc->is_src_frame_alt_ref &&
             (cpi->refresh_golden_frame || is_intrl_arf_boost ||
              cpi->refresh_alt_ref_frame)) {
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
    if (oxcf->rc_mode == AOM_CQ) {
      if (q < cq_level) q = cq_level;

      active_best_quality = get_gf_active_quality(rc, q, bit_depth);

      // Constrained quality use slightly lower active best.
      active_best_quality = active_best_quality * 15 / 16;

    } else if (oxcf->rc_mode == AOM_Q) {
      if (!cpi->refresh_alt_ref_frame && !is_intrl_arf_boost) {
        active_best_quality = cq_level;
      } else {
        active_best_quality = get_gf_active_quality(rc, q, bit_depth);
#if USE_SYMM_MULTI_LAYER
        if (cpi->new_bwdref_update_rule && is_intrl_arf_boost) {
          int this_height = gf_group->pyramid_level[gf_group->index];
          while (this_height < gf_group->pyramid_height) {
            active_best_quality = (active_best_quality + cq_level + 1) / 2;
            ++this_height;
          }
        } else {
#endif
          // Modify best quality for second level arfs. For mode AOM_Q this
          // becomes the baseline frame q.
          if (gf_group->rf_level[gf_group->index] == GF_ARF_LOW)
            active_best_quality = (active_best_quality + cq_level + 1) / 2;
#if USE_SYMM_MULTI_LAYER
        }
#endif
      }
    } else {
      active_best_quality = get_gf_active_quality(rc, q, bit_depth);
#if USE_SYMM_MULTI_LAYER
      if (cpi->new_bwdref_update_rule && is_intrl_arf_boost) {
        int this_height = gf_group->pyramid_level[gf_group->index];
        while (this_height < gf_group->pyramid_height) {
          active_best_quality =
              (active_best_quality + active_worst_quality + 1) / 2;
          ++this_height;
        }
      }
#endif
    }
  } else {
    if (oxcf->rc_mode == AOM_Q) {
      active_best_quality = cq_level;
    } else {
      active_best_quality = inter_minq[active_worst_quality];

      // For the constrained quality mode we don't want
      // q to fall below the cq level.
      if ((oxcf->rc_mode == AOM_CQ) && (active_best_quality < cq_level)) {
        active_best_quality = cq_level;
      }
    }
  }

  // Extension to max or min Q if undershoot or overshoot is outside
  // the permitted range.
  if ((cpi->oxcf.rc_mode != AOM_Q) &&
      (cpi->twopass.gf_zeromotion_pct < VLOW_MOTION_THRESHOLD)) {
    if (frame_is_intra_only(cm) ||
        (!rc->is_src_frame_alt_ref &&
         (cpi->refresh_golden_frame || is_intrl_arf_boost ||
          cpi->refresh_alt_ref_frame))) {
      active_best_quality -=
          (cpi->twopass.extend_minq + cpi->twopass.extend_minq_fast);
      active_worst_quality += (cpi->twopass.extend_maxq / 2);
    } else {
      active_best_quality -=
          (cpi->twopass.extend_minq + cpi->twopass.extend_minq_fast) / 2;
      active_worst_quality += cpi->twopass.extend_maxq;
    }
  }

  aom_clear_system_state();
  // Static forced key frames Q restrictions dealt with elsewhere.
  if (!(frame_is_intra_only(cm)) || !rc->this_key_frame_forced ||
      (cpi->twopass.last_kfgroup_zeromotion_pct < STATIC_MOTION_THRESH)) {
    int qdelta = av1_frame_type_qdelta(cpi, gf_group->rf_level[gf_group->index],
                                       active_worst_quality);
    active_worst_quality =
        AOMMAX(active_worst_quality + qdelta, active_best_quality);
  }

  // Modify active_best_quality for downscaled normal frames.
  if (av1_frame_scaled(cm) && !frame_is_kf_gf_arf(cpi)) {
    int qdelta = av1_compute_qdelta_by_rate(
        rc, cm->frame_type, active_best_quality, 2.0, bit_depth);
    active_best_quality =
        AOMMAX(active_best_quality + qdelta, rc->best_quality);
  }

  active_best_quality =
      clamp(active_best_quality, rc->best_quality, rc->worst_quality);
  active_worst_quality =
      clamp(active_worst_quality, active_best_quality, rc->worst_quality);

  if (oxcf->rc_mode == AOM_Q) {
    q = active_best_quality;
    // Special case code to try and match quality with forced key frames.
  } else if (frame_is_intra_only(cm) && rc->this_key_frame_forced) {
    // If static since last kf use better of last boosted and last kf q.
    if (cpi->twopass.last_kfgroup_zeromotion_pct >= STATIC_MOTION_THRESH) {
      q = AOMMIN(rc->last_kf_qindex, rc->last_boosted_qindex);
    } else {
      q = rc->last_boosted_qindex;
    }
  } else {
    q = av1_rc_regulate_q(cpi, rc->this_frame_target, active_best_quality,
                          active_worst_quality, width, height);
    if (q > active_worst_quality) {
      // Special case when we are targeting the max allowed rate.
      if (rc->this_frame_target >= rc->max_frame_bandwidth)
        active_worst_quality = q;
      else
        q = active_worst_quality;
    }
  }
  clamp(q, active_best_quality, active_worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

  assert(*top_index <= rc->worst_quality && *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

int av1_rc_pick_q_and_bounds(const AV1_COMP *cpi, int width, int height,
                             int *bottom_index, int *top_index) {
  int q;
  if (cpi->oxcf.pass == 0) {
    if (cpi->oxcf.rc_mode == AOM_CBR)
      q = rc_pick_q_and_bounds_one_pass_cbr(cpi, width, height, bottom_index,
                                            top_index);
    else
      q = rc_pick_q_and_bounds_one_pass_vbr(cpi, width, height, bottom_index,
                                            top_index);
  } else {
    q = rc_pick_q_and_bounds_two_pass(cpi, width, height, bottom_index,
                                      top_index);
  }

  return q;
}

void av1_rc_compute_frame_size_bounds(const AV1_COMP *cpi, int frame_target,
                                      int *frame_under_shoot_limit,
                                      int *frame_over_shoot_limit) {
  if (cpi->oxcf.rc_mode == AOM_Q) {
    *frame_under_shoot_limit = 0;
    *frame_over_shoot_limit = INT_MAX;
  } else {
    // For very small rate targets where the fractional adjustment
    // may be tiny make sure there is at least a minimum range.
    const int tolerance = (cpi->sf.recode_tolerance * frame_target) / 100;
    *frame_under_shoot_limit = AOMMAX(frame_target - tolerance - 200, 0);
    *frame_over_shoot_limit =
        AOMMIN(frame_target + tolerance + 200, cpi->rc.max_frame_bandwidth);
  }
}

static void rc_set_frame_target(AV1_COMP *cpi, int target, int width,
                                int height) {
  const AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;

  rc->this_frame_target = target;

  // Modify frame size target when down-scaled.
  if (av1_frame_scaled(cm))
    rc->this_frame_target =
        (int)(rc->this_frame_target * resize_rate_factor(cpi, width, height));

  // Target rate per SB64 (including partial SB64s.
  rc->sb64_target_rate =
      (int)((int64_t)rc->this_frame_target * 64 * 64) / (width * height);
}

static void update_alt_ref_frame_stats(AV1_COMP *cpi) {
  // this frame refreshes means next frames don't unless specified by user
  RATE_CONTROL *const rc = &cpi->rc;
  rc->frames_since_golden = 0;

  // Mark the alt ref as done (setting to 0 means no further alt refs pending).
  rc->source_alt_ref_pending = 0;

  // Set the alternate reference frame active flag
  rc->source_alt_ref_active = 1;
}

static void update_golden_frame_stats(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
#if CUSTOMIZED_GF
  const TWO_PASS *const twopass = &cpi->twopass;
  const GF_GROUP *const gf_group = &twopass->gf_group;
  const int is_intrnl_arf =
      cpi->oxcf.pass == 2
          ? gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE
          : cpi->refresh_alt2_ref_frame;
#else
  const int is_intnl_arf = cpi->refresh_alt2_ref_frame;
#endif

  // Update the Golden frame usage counts.
  // NOTE(weitinglin): If we use show_existing_frame for an OVERLAY frame,
  //                   only the virtual indices for the reference frame will be
  //                   updated and cpi->refresh_golden_frame will still be zero.
  if (cpi->refresh_golden_frame || rc->is_src_frame_alt_ref) {
    // We will not use internal overlay frames to replace the golden frame
    if (!rc->is_src_frame_ext_arf)
      // this frame refreshes means next frames don't unless specified by user
      rc->frames_since_golden = 0;

    // If we are not using alt ref in the up and coming group clear the arf
    // active flag. In multi arf group case, if the index is not 0 then
    // we are overlaying a mid group arf so should not reset the flag.
    if (cpi->oxcf.pass == 2) {
      if (!rc->source_alt_ref_pending && (cpi->twopass.gf_group.index == 0))
        rc->source_alt_ref_active = 0;
    } else if (!rc->source_alt_ref_pending) {
      rc->source_alt_ref_active = 0;
    }
  } else if (!cpi->refresh_alt_ref_frame && !is_intrnl_arf) {
    rc->frames_since_golden++;
  }
}

void av1_rc_postencode_update(AV1_COMP *cpi, uint64_t bytes_used) {
  const AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
#if CUSTOMIZED_GF
  const TWO_PASS *const twopass = &cpi->twopass;
  const GF_GROUP *const gf_group = &twopass->gf_group;
  const int is_intrnl_arf =
      cpi->oxcf.pass == 2
          ? gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE
          : cpi->refresh_alt2_ref_frame;
#else
  const int is_intrnl_arf = cpi->refresh_alt2_ref_frame;
#endif

  const int qindex = cm->base_qindex;

  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ && cm->seg.enabled) {
    av1_cyclic_refresh_postencode(cpi);
  }

  // Update rate control heuristics
  rc->projected_frame_size = (int)(bytes_used << 3);

  // Post encode loop adjustment of Q prediction.
  av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);

  // Keep a record of last Q and ambient average Q.
  if (cm->frame_type == KEY_FRAME) {
    rc->last_q[KEY_FRAME] = qindex;
    rc->avg_frame_qindex[KEY_FRAME] =
        ROUND_POWER_OF_TWO(3 * rc->avg_frame_qindex[KEY_FRAME] + qindex, 2);
  } else {
    if (!rc->is_src_frame_alt_ref &&
        !(cpi->refresh_golden_frame || is_intrnl_arf ||
          cpi->refresh_alt_ref_frame)) {
      rc->last_q[INTER_FRAME] = qindex;
      rc->avg_frame_qindex[INTER_FRAME] =
          ROUND_POWER_OF_TWO(3 * rc->avg_frame_qindex[INTER_FRAME] + qindex, 2);
      rc->ni_frames++;
      rc->tot_q += av1_convert_qindex_to_q(qindex, cm->seq_params.bit_depth);
      rc->avg_q = rc->tot_q / rc->ni_frames;
      // Calculate the average Q for normal inter frames (not key or GFU
      // frames).
      rc->ni_tot_qi += qindex;
      rc->ni_av_qi = rc->ni_tot_qi / rc->ni_frames;
    }
  }

  // Keep record of last boosted (KF/GF/ARF) Q value.
  // If the current frame is coded at a lower Q then we also update it.
  // If all mbs in this group are skipped only update if the Q value is
  // better than that already stored.
  // This is used to help set quality in forced key frames to reduce popping
  if ((qindex < rc->last_boosted_qindex) || (cm->frame_type == KEY_FRAME) ||
      (!rc->constrained_gf_group &&
       (cpi->refresh_alt_ref_frame || is_intrnl_arf ||
        (cpi->refresh_golden_frame && !rc->is_src_frame_alt_ref)))) {
    rc->last_boosted_qindex = qindex;
  }
  if (cm->frame_type == KEY_FRAME) rc->last_kf_qindex = qindex;

  update_buffer_level(cpi, rc->projected_frame_size);

  // Rolling monitors of whether we are over or underspending used to help
  // regulate min and Max Q in two pass.
  if (av1_frame_scaled(cm))
    rc->this_frame_target =
        (int)(rc->this_frame_target /
              resize_rate_factor(cpi, cm->width, cm->height));
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
  // TODO(zoeliu): To investigate whether we should treat BWDREF_FRAME
  //               differently here for rc->avg_frame_bandwidth.
  rc->total_target_bits +=
      (cm->show_frame || rc->is_bwd_ref_frame) ? rc->avg_frame_bandwidth : 0;

  rc->total_target_vs_actual = rc->total_actual_bits - rc->total_target_bits;

  if (is_altref_enabled(cpi) && cpi->refresh_alt_ref_frame &&
      (cm->frame_type != KEY_FRAME))
    // Update the alternate reference frame stats as appropriate.
    update_alt_ref_frame_stats(cpi);
  else
    // Update the Golden frame stats as appropriate.
    update_golden_frame_stats(cpi);

  if (cm->frame_type == KEY_FRAME) rc->frames_since_key = 0;

  // TODO(zoeliu): To investigate whether we should treat BWDREF_FRAME
  //               differently here for rc->avg_frame_bandwidth.
  if (cm->show_frame || rc->is_bwd_ref_frame) {
    rc->frames_since_key++;
    rc->frames_to_key--;
  }
  // if (cm->current_video_frame == 1 && cm->show_frame)
  /*
  rc->this_frame_target =
      (int)(rc->this_frame_target / resize_rate_factor(cpi, cm->width,
  cm->height));
      */
}

void av1_rc_postencode_update_drop_frame(AV1_COMP *cpi) {
  // Update buffer level with zero size, update frame counters, and return.
  update_buffer_level(cpi, 0);
  cpi->rc.frames_since_key++;
  cpi->rc.frames_to_key--;
  cpi->rc.rc_2_frame = 0;
  cpi->rc.rc_1_frame = 0;
}

// Use this macro to turn on/off use of alt-refs in one-pass mode.
#define USE_ALTREF_FOR_ONE_PASS 1

static int calc_pframe_target_size_one_pass_vbr(const AV1_COMP *const cpi) {
  static const int af_ratio = 10;
  const RATE_CONTROL *const rc = &cpi->rc;
  int target;
#if USE_ALTREF_FOR_ONE_PASS
  target =
      (!rc->is_src_frame_alt_ref &&
       (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame))
          ? (rc->avg_frame_bandwidth * rc->baseline_gf_interval * af_ratio) /
                (rc->baseline_gf_interval + af_ratio - 1)
          : (rc->avg_frame_bandwidth * rc->baseline_gf_interval) /
                (rc->baseline_gf_interval + af_ratio - 1);
#else
  target = rc->avg_frame_bandwidth;
#endif
  return av1_rc_clamp_pframe_target_size(cpi, target);
}

static int calc_iframe_target_size_one_pass_vbr(const AV1_COMP *const cpi) {
  static const int kf_ratio = 25;
  const RATE_CONTROL *rc = &cpi->rc;
  const int target = rc->avg_frame_bandwidth * kf_ratio;
  return av1_rc_clamp_iframe_target_size(cpi, target);
}

void av1_rc_get_one_pass_vbr_params(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int target;
  int altref_enabled = is_altref_enabled(cpi);
  int sframe_dist = cpi->oxcf.sframe_dist;
  int sframe_mode = cpi->oxcf.sframe_mode;
  int sframe_enabled = cpi->oxcf.sframe_enabled;
  // TODO(yaowu): replace the "auto_key && 0" below with proper decision logic.
  if (!cpi->refresh_alt_ref_frame &&
      (cm->current_video_frame == 0 || (cpi->frame_flags & FRAMEFLAGS_KEY) ||
       rc->frames_to_key == 0 || (cpi->oxcf.auto_key && 0))) {
    cm->frame_type = KEY_FRAME;
    rc->this_key_frame_forced =
        cm->current_video_frame != 0 && rc->frames_to_key == 0;
    rc->frames_to_key = cpi->oxcf.key_freq;
    rc->kf_boost = DEFAULT_KF_BOOST;
    rc->source_alt_ref_active = 0;
  } else {
    cm->frame_type = INTER_FRAME;
    if (sframe_enabled) {
      if (altref_enabled) {
        if (sframe_mode == 1) {
          // sframe_mode == 1: insert sframe if it matches altref frame.

          if (cm->current_video_frame % sframe_dist == 0 &&
              cm->frame_type != KEY_FRAME && cm->current_video_frame != 0 &&
              cpi->refresh_alt_ref_frame) {
            cm->frame_type = S_FRAME;
          }
        } else {
          // sframe_mode != 1: if sframe will be inserted at the next available
          // altref frame

          if (cm->current_video_frame % sframe_dist == 0 &&
              cm->frame_type != KEY_FRAME && cm->current_video_frame != 0) {
            rc->sframe_due = 1;
          }

          if (rc->sframe_due && cpi->refresh_alt_ref_frame) {
            cm->frame_type = S_FRAME;
            rc->sframe_due = 0;
          }
        }
      } else {
        if (cm->current_video_frame % sframe_dist == 0 &&
            cm->frame_type != KEY_FRAME && cm->current_video_frame != 0) {
          cm->frame_type = S_FRAME;
        }
      }
    }
  }
  if (rc->frames_till_gf_update_due == 0) {
    rc->baseline_gf_interval = (rc->min_gf_interval + rc->max_gf_interval) / 2;
    rc->frames_till_gf_update_due = rc->baseline_gf_interval;
    // NOTE: frames_till_gf_update_due must be <= frames_to_key.
    if (rc->frames_till_gf_update_due > rc->frames_to_key) {
      rc->frames_till_gf_update_due = rc->frames_to_key;
      rc->constrained_gf_group = 1;
    } else {
      rc->constrained_gf_group = 0;
    }
    cpi->refresh_golden_frame = 1;
    rc->source_alt_ref_pending = USE_ALTREF_FOR_ONE_PASS;
    rc->gfu_boost = DEFAULT_GF_BOOST;
  }

  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
    av1_cyclic_refresh_update_parameters(cpi);

  if (cm->frame_type == KEY_FRAME)
    target = calc_iframe_target_size_one_pass_vbr(cpi);
  else
    target = calc_pframe_target_size_one_pass_vbr(cpi);
  rc_set_frame_target(cpi, target, cm->width, cm->height);
}

static int calc_pframe_target_size_one_pass_cbr(const AV1_COMP *cpi) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  const RATE_CONTROL *rc = &cpi->rc;
  const int64_t diff = rc->optimal_buffer_level - rc->buffer_level;
  const int64_t one_pct_bits = 1 + rc->optimal_buffer_level / 100;
  int min_frame_target =
      AOMMAX(rc->avg_frame_bandwidth >> 4, FRAME_OVERHEAD_BITS);
  int target;

  if (oxcf->gf_cbr_boost_pct) {
    const int af_ratio_pct = oxcf->gf_cbr_boost_pct + 100;
    target = cpi->refresh_golden_frame
                 ? (rc->avg_frame_bandwidth * rc->baseline_gf_interval *
                    af_ratio_pct) /
                       (rc->baseline_gf_interval * 100 + af_ratio_pct - 100)
                 : (rc->avg_frame_bandwidth * rc->baseline_gf_interval * 100) /
                       (rc->baseline_gf_interval * 100 + af_ratio_pct - 100);
  } else {
    target = rc->avg_frame_bandwidth;
  }

  if (diff > 0) {
    // Lower the target bandwidth for this frame.
    const int pct_low = (int)AOMMIN(diff / one_pct_bits, oxcf->under_shoot_pct);
    target -= (target * pct_low) / 200;
  } else if (diff < 0) {
    // Increase the target bandwidth for this frame.
    const int pct_high =
        (int)AOMMIN(-diff / one_pct_bits, oxcf->over_shoot_pct);
    target += (target * pct_high) / 200;
  }
  if (oxcf->rc_max_inter_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * oxcf->rc_max_inter_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }
  return AOMMAX(min_frame_target, target);
}

static int calc_iframe_target_size_one_pass_cbr(const AV1_COMP *cpi) {
  const RATE_CONTROL *rc = &cpi->rc;
  int target;
  if (cpi->common.current_video_frame == 0) {
    target = ((rc->starting_buffer_level / 2) > INT_MAX)
                 ? INT_MAX
                 : (int)(rc->starting_buffer_level / 2);
  } else {
    int kf_boost = 32;
    double framerate = cpi->framerate;

    kf_boost = AOMMAX(kf_boost, (int)(2 * framerate - 16));
    if (rc->frames_since_key < framerate / 2) {
      kf_boost = (int)(kf_boost * rc->frames_since_key / (framerate / 2));
    }
    target = ((16 + kf_boost) * rc->avg_frame_bandwidth) >> 4;
  }
  return av1_rc_clamp_iframe_target_size(cpi, target);
}

void av1_rc_get_one_pass_cbr_params(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int target;
  // TODO(yaowu): replace the "auto_key && 0" below with proper decision logic.
  if ((cm->current_video_frame == 0 || (cpi->frame_flags & FRAMEFLAGS_KEY) ||
       rc->frames_to_key == 0 || (cpi->oxcf.auto_key && 0))) {
    cm->frame_type = KEY_FRAME;
    rc->this_key_frame_forced =
        cm->current_video_frame != 0 && rc->frames_to_key == 0;
    rc->frames_to_key = cpi->oxcf.key_freq;
    rc->kf_boost = DEFAULT_KF_BOOST;
    rc->source_alt_ref_active = 0;
  } else {
    cm->frame_type = INTER_FRAME;
  }
  if (rc->frames_till_gf_update_due == 0) {
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      av1_cyclic_refresh_set_golden_update(cpi);
    else
      rc->baseline_gf_interval =
          (rc->min_gf_interval + rc->max_gf_interval) / 2;
    rc->frames_till_gf_update_due = rc->baseline_gf_interval;
    // NOTE: frames_till_gf_update_due must be <= frames_to_key.
    if (rc->frames_till_gf_update_due > rc->frames_to_key)
      rc->frames_till_gf_update_due = rc->frames_to_key;
    cpi->refresh_golden_frame = 1;
    rc->gfu_boost = DEFAULT_GF_BOOST;
  }

  // Any update/change of global cyclic refresh parameters (amount/delta-qp)
  // should be done here, before the frame qp is selected.
  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
    av1_cyclic_refresh_update_parameters(cpi);

  if (cm->frame_type == KEY_FRAME)
    target = calc_iframe_target_size_one_pass_cbr(cpi);
  else
    target = calc_pframe_target_size_one_pass_cbr(cpi);

  rc_set_frame_target(cpi, target, cm->width, cm->height);
  // TODO(afergs): Decide whether to scale up, down, or not at all
}

int av1_compute_qdelta(const RATE_CONTROL *rc, double qstart, double qtarget,
                       aom_bit_depth_t bit_depth) {
  int start_index = rc->worst_quality;
  int target_index = rc->worst_quality;
  int i;

  // Convert the average q value to an index.
  for (i = rc->best_quality; i < rc->worst_quality; ++i) {
    start_index = i;
    if (av1_convert_qindex_to_q(i, bit_depth) >= qstart) break;
  }

  // Convert the q target to an index
  for (i = rc->best_quality; i < rc->worst_quality; ++i) {
    target_index = i;
    if (av1_convert_qindex_to_q(i, bit_depth) >= qtarget) break;
  }

  return target_index - start_index;
}

int av1_compute_qdelta_by_rate(const RATE_CONTROL *rc, FRAME_TYPE frame_type,
                               int qindex, double rate_target_ratio,
                               aom_bit_depth_t bit_depth) {
  int target_index = rc->worst_quality;
  int i;

  // Look up the current projected bits per block for the base index
  const int base_bits_per_mb =
      av1_rc_bits_per_mb(frame_type, qindex, 1.0, bit_depth);

  // Find the target bits per mb based on the base value and given ratio.
  const int target_bits_per_mb = (int)(rate_target_ratio * base_bits_per_mb);

  // Convert the q target to an index
  for (i = rc->best_quality; i < rc->worst_quality; ++i) {
    if (av1_rc_bits_per_mb(frame_type, i, 1.0, bit_depth) <=
        target_bits_per_mb) {
      target_index = i;
      break;
    }
  }
  return target_index - qindex;
}

void av1_rc_set_gf_interval_range(const AV1_COMP *const cpi,
                                  RATE_CONTROL *const rc) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  // Special case code for 1 pass fixed Q mode tests
  if ((oxcf->pass == 0) && (oxcf->rc_mode == AOM_Q)) {
    rc->max_gf_interval = FIXED_GF_INTERVAL;
    rc->min_gf_interval = FIXED_GF_INTERVAL;
    rc->static_scene_max_gf_interval = FIXED_GF_INTERVAL;
  } else {
    // Set Maximum gf/arf interval
    rc->max_gf_interval = oxcf->max_gf_interval;
    rc->min_gf_interval = oxcf->min_gf_interval;
    if (rc->min_gf_interval == 0)
      rc->min_gf_interval = av1_rc_get_default_min_gf_interval(
          oxcf->width, oxcf->height, cpi->framerate);
    if (rc->max_gf_interval == 0)
      rc->max_gf_interval = av1_rc_get_default_max_gf_interval(
          cpi->framerate, rc->min_gf_interval);

    // Extended interval for genuinely static scenes
    rc->static_scene_max_gf_interval = MAX_LAG_BUFFERS * 2;

    if (is_altref_enabled(cpi)) {
      if (rc->static_scene_max_gf_interval > oxcf->lag_in_frames - 1)
        rc->static_scene_max_gf_interval = oxcf->lag_in_frames - 1;
    }

    if (rc->max_gf_interval > rc->static_scene_max_gf_interval)
      rc->max_gf_interval = rc->static_scene_max_gf_interval;

#if FIX_GF_INTERVAL_LENGTH
    rc->max_gf_interval = FIXED_GF_LENGTH + 1;
#endif

    // Clamp min to max
    rc->min_gf_interval = AOMMIN(rc->min_gf_interval, rc->max_gf_interval);
  }
}

void av1_rc_update_framerate(AV1_COMP *cpi, int width, int height) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  RATE_CONTROL *const rc = &cpi->rc;
  int vbr_max_bits;
  const int MBs = av1_get_MBs(width, height);

  rc->avg_frame_bandwidth = (int)(oxcf->target_bandwidth / cpi->framerate);
  rc->min_frame_bandwidth =
      (int)(rc->avg_frame_bandwidth * oxcf->two_pass_vbrmin_section / 100);

  rc->min_frame_bandwidth =
      AOMMAX(rc->min_frame_bandwidth, FRAME_OVERHEAD_BITS);

  // A maximum bitrate for a frame is defined.
  // The baseline for this aligns with HW implementations that
  // can support decode of 1080P content up to a bitrate of MAX_MB_RATE bits
  // per 16x16 MB (averaged over a frame). However this limit is extended if
  // a very high rate is given on the command line or the the rate cannnot
  // be acheived because of a user specificed max q (e.g. when the user
  // specifies lossless encode.
  vbr_max_bits =
      (int)(((int64_t)rc->avg_frame_bandwidth * oxcf->two_pass_vbrmax_section) /
            100);
  rc->max_frame_bandwidth =
      AOMMAX(AOMMAX((MBs * MAX_MB_RATE), MAXRATE_1080P), vbr_max_bits);

  av1_rc_set_gf_interval_range(cpi, rc);
}

#define VBR_PCT_ADJUSTMENT_LIMIT 50
// For VBR...adjustment to the frame target based on error from previous frames
static void vbr_rate_correction(AV1_COMP *cpi, int *this_frame_target) {
  RATE_CONTROL *const rc = &cpi->rc;
  int64_t vbr_bits_off_target = rc->vbr_bits_off_target;
  int max_delta;
  double position_factor = 1.0;

  // How far through the clip are we.
  // This number is used to damp the per frame rate correction.
  // Range 0 - 1.0
  if (cpi->twopass.total_stats.count != 0.) {
    position_factor = sqrt((double)cpi->common.current_video_frame /
                           cpi->twopass.total_stats.count);
  }
  max_delta = (int)(position_factor *
                    ((*this_frame_target * VBR_PCT_ADJUSTMENT_LIMIT) / 100));

  // vbr_bits_off_target > 0 means we have extra bits to spend
  if (vbr_bits_off_target > 0) {
    *this_frame_target += (vbr_bits_off_target > max_delta)
                              ? max_delta
                              : (int)vbr_bits_off_target;
  } else {
    *this_frame_target -= (vbr_bits_off_target < -max_delta)
                              ? max_delta
                              : (int)-vbr_bits_off_target;
  }

  // Fast redistribution of bits arising from massive local undershoot.
  // Dont do it for kf,arf,gf or overlay frames.
  if (!frame_is_kf_gf_arf(cpi) && !rc->is_src_frame_alt_ref &&
      rc->vbr_bits_off_target_fast) {
    int one_frame_bits = AOMMAX(rc->avg_frame_bandwidth, *this_frame_target);
    int fast_extra_bits;
    fast_extra_bits = (int)AOMMIN(rc->vbr_bits_off_target_fast, one_frame_bits);
    fast_extra_bits = (int)AOMMIN(
        fast_extra_bits,
        AOMMAX(one_frame_bits / 8, rc->vbr_bits_off_target_fast / 8));
    *this_frame_target += (int)fast_extra_bits;
    rc->vbr_bits_off_target_fast -= fast_extra_bits;
  }
}

void av1_set_target_rate(AV1_COMP *cpi, int width, int height) {
  RATE_CONTROL *const rc = &cpi->rc;
  int target_rate = rc->base_frame_target;

  // Correction to rate target based on prior over or under shoot.
  if (cpi->oxcf.rc_mode == AOM_VBR || cpi->oxcf.rc_mode == AOM_CQ)
    vbr_rate_correction(cpi, &target_rate);
  rc_set_frame_target(cpi, target_rate, width, height);
}
