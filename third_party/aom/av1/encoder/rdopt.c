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
#include <math.h>

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/common.h"
#include "av1/common/common_data.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/idct.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/scan.h"
#include "av1/common/seg_common.h"
#if CONFIG_LV_MAP
#include "av1/common/txb_common.h"
#endif
#if CONFIG_WARPED_MOTION
#include "av1/common/warped_motion.h"
#endif  // CONFIG_WARPED_MOTION

#include "av1/encoder/aq_variance.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/mcomp.h"
#if CONFIG_PALETTE
#include "av1/encoder/palette.h"
#endif  // CONFIG_PALETTE
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/tokenize.h"
#if CONFIG_PVQ
#include "av1/encoder/pvq_encoder.h"
#endif  // CONFIG_PVQ
#if CONFIG_PVQ || CONFIG_DAALA_DIST
#include "av1/common/pvq.h"
#endif  // CONFIG_PVQ || CONFIG_DAALA_DIST
#if CONFIG_DUAL_FILTER
#define DUAL_FILTER_SET_SIZE (SWITCHABLE_FILTERS * SWITCHABLE_FILTERS)
#if USE_EXTRA_FILTER
static const int filter_sets[DUAL_FILTER_SET_SIZE][2] = {
  { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 0 }, { 1, 1 },
  { 1, 2 }, { 1, 3 }, { 2, 0 }, { 2, 1 }, { 2, 2 }, { 2, 3 },
  { 3, 0 }, { 3, 1 }, { 3, 2 }, { 3, 3 },
};
#else   // USE_EXTRA_FILTER
static const int filter_sets[DUAL_FILTER_SET_SIZE][2] = {
  { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 0 }, { 1, 1 },
  { 1, 2 }, { 2, 0 }, { 2, 1 }, { 2, 2 },
};
#endif  // USE_EXTRA_FILTER
#endif  // CONFIG_DUAL_FILTER

#if CONFIG_EXT_REFS

#define LAST_FRAME_MODE_MASK                                      \
  ((1 << INTRA_FRAME) | (1 << LAST2_FRAME) | (1 << LAST3_FRAME) | \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME))
#define LAST2_FRAME_MODE_MASK                                    \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST3_FRAME) | \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME))
#define LAST3_FRAME_MODE_MASK                                    \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) | \
   (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME))
#define GOLDEN_FRAME_MODE_MASK                                   \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) | \
   (1 << LAST3_FRAME) | (1 << BWDREF_FRAME) | (1 << ALTREF_FRAME))
#define BWDREF_FRAME_MODE_MASK                                   \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) | \
   (1 << LAST3_FRAME) | (1 << GOLDEN_FRAME) | (1 << ALTREF_FRAME))
#define ALTREF_FRAME_MODE_MASK                                   \
  ((1 << INTRA_FRAME) | (1 << LAST_FRAME) | (1 << LAST2_FRAME) | \
   (1 << LAST3_FRAME) | (1 << GOLDEN_FRAME) | (1 << BWDREF_FRAME))

#else

#define LAST_FRAME_MODE_MASK \
  ((1 << GOLDEN_FRAME) | (1 << ALTREF_FRAME) | (1 << INTRA_FRAME))
#define GOLDEN_FRAME_MODE_MASK \
  ((1 << LAST_FRAME) | (1 << ALTREF_FRAME) | (1 << INTRA_FRAME))
#define ALTREF_FRAME_MODE_MASK \
  ((1 << LAST_FRAME) | (1 << GOLDEN_FRAME) | (1 << INTRA_FRAME))

#endif  // CONFIG_EXT_REFS

#if CONFIG_EXT_REFS
#define SECOND_REF_FRAME_MASK ((1 << ALTREF_FRAME) | (1 << BWDREF_FRAME) | 0x01)
#else
#define SECOND_REF_FRAME_MASK ((1 << ALTREF_FRAME) | 0x01)
#endif  // CONFIG_EXT_REFS

#define MIN_EARLY_TERM_INDEX 3
#define NEW_MV_DISCOUNT_FACTOR 8

#if CONFIG_EXT_INTRA
#define ANGLE_SKIP_THRESH 10
#define FILTER_FAST_SEARCH 1
#endif  // CONFIG_EXT_INTRA

const double ADST_FLIP_SVM[8] = { -6.6623, -2.8062, -3.2531, 3.1671,    // vert
                                  -7.7051, -3.2234, -3.6193, 3.4533 };  // horz

typedef struct {
  PREDICTION_MODE mode;
  MV_REFERENCE_FRAME ref_frame[2];
} MODE_DEFINITION;

typedef struct { MV_REFERENCE_FRAME ref_frame[2]; } REF_DEFINITION;

struct rdcost_block_args {
  const AV1_COMP *cpi;
  MACROBLOCK *x;
  ENTROPY_CONTEXT t_above[2 * MAX_MIB_SIZE];
  ENTROPY_CONTEXT t_left[2 * MAX_MIB_SIZE];
  RD_STATS rd_stats;
  int64_t this_rd;
  int64_t best_rd;
  int exit_early;
  int use_fast_coef_costing;
};

#define LAST_NEW_MV_INDEX 6
static const MODE_DEFINITION av1_mode_order[MAX_MODES] = {
  { NEARESTMV, { LAST_FRAME, NONE_FRAME } },
#if CONFIG_EXT_REFS
  { NEARESTMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARESTMV, { BWDREF_FRAME, NONE_FRAME } },
#endif  // CONFIG_EXT_REFS
  { NEARESTMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, NONE_FRAME } },

  { DC_PRED, { INTRA_FRAME, NONE_FRAME } },

  { NEWMV, { LAST_FRAME, NONE_FRAME } },
#if CONFIG_EXT_REFS
  { NEWMV, { LAST2_FRAME, NONE_FRAME } },
  { NEWMV, { LAST3_FRAME, NONE_FRAME } },
  { NEWMV, { BWDREF_FRAME, NONE_FRAME } },
#endif  // CONFIG_EXT_REFS
  { NEWMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEWMV, { GOLDEN_FRAME, NONE_FRAME } },

  { NEARMV, { LAST_FRAME, NONE_FRAME } },
#if CONFIG_EXT_REFS
  { NEARMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARMV, { BWDREF_FRAME, NONE_FRAME } },
#endif  // CONFIG_EXT_REFS
  { NEARMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARMV, { GOLDEN_FRAME, NONE_FRAME } },

  { ZEROMV, { LAST_FRAME, NONE_FRAME } },
#if CONFIG_EXT_REFS
  { ZEROMV, { LAST2_FRAME, NONE_FRAME } },
  { ZEROMV, { LAST3_FRAME, NONE_FRAME } },
  { ZEROMV, { BWDREF_FRAME, NONE_FRAME } },
#endif  // CONFIG_EXT_REFS
  { ZEROMV, { GOLDEN_FRAME, NONE_FRAME } },
  { ZEROMV, { ALTREF_FRAME, NONE_FRAME } },

// TODO(zoeliu): May need to reconsider the order on the modes to check

#if CONFIG_EXT_INTER
  { NEAREST_NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { NEAREST_NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { NEAREST_NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS

#else  // CONFIG_EXT_INTER

  { NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS
#endif  // CONFIG_EXT_INTER

  { TM_PRED, { INTRA_FRAME, NONE_FRAME } },

#if CONFIG_ALT_INTRA
  { SMOOTH_PRED, { INTRA_FRAME, NONE_FRAME } },
#if CONFIG_SMOOTH_HV
  { SMOOTH_V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_H_PRED, { INTRA_FRAME, NONE_FRAME } },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA

#if CONFIG_EXT_INTER
  { NEAR_NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { ZERO_ZEROMV, { LAST_FRAME, ALTREF_FRAME } },

#if CONFIG_EXT_REFS
  { NEAR_NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { ZERO_ZEROMV, { LAST2_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { ZERO_ZEROMV, { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS

  { NEAR_NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { ZERO_ZEROMV, { GOLDEN_FRAME, ALTREF_FRAME } },

#if CONFIG_EXT_REFS
  { NEAR_NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { ZERO_ZEROMV, { LAST_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { ZERO_ZEROMV, { LAST2_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { ZERO_ZEROMV, { LAST3_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { ZERO_ZEROMV, { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS

#else  // CONFIG_EXT_INTER

  { NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEWMV, { LAST_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },

#if CONFIG_EXT_REFS
  { NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS

  { ZEROMV, { LAST_FRAME, ALTREF_FRAME } },
#if CONFIG_EXT_REFS
  { ZEROMV, { LAST2_FRAME, ALTREF_FRAME } },
  { ZEROMV, { LAST3_FRAME, ALTREF_FRAME } },
#endif  // CONFIG_EXT_REFS
  { ZEROMV, { GOLDEN_FRAME, ALTREF_FRAME } },

#if CONFIG_EXT_REFS
  { ZEROMV, { LAST_FRAME, BWDREF_FRAME } },
  { ZEROMV, { LAST2_FRAME, BWDREF_FRAME } },
  { ZEROMV, { LAST3_FRAME, BWDREF_FRAME } },
  { ZEROMV, { GOLDEN_FRAME, BWDREF_FRAME } },
#endif  // CONFIG_EXT_REFS

#endif  // CONFIG_EXT_INTER

  { H_PRED, { INTRA_FRAME, NONE_FRAME } },
  { V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D135_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D207_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D153_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D63_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D117_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D45_PRED, { INTRA_FRAME, NONE_FRAME } },

#if CONFIG_EXT_INTER
  { ZEROMV, { LAST_FRAME, INTRA_FRAME } },
  { NEARESTMV, { LAST_FRAME, INTRA_FRAME } },
  { NEARMV, { LAST_FRAME, INTRA_FRAME } },
  { NEWMV, { LAST_FRAME, INTRA_FRAME } },

#if CONFIG_EXT_REFS
  { ZEROMV, { LAST2_FRAME, INTRA_FRAME } },
  { NEARESTMV, { LAST2_FRAME, INTRA_FRAME } },
  { NEARMV, { LAST2_FRAME, INTRA_FRAME } },
  { NEWMV, { LAST2_FRAME, INTRA_FRAME } },

  { ZEROMV, { LAST3_FRAME, INTRA_FRAME } },
  { NEARESTMV, { LAST3_FRAME, INTRA_FRAME } },
  { NEARMV, { LAST3_FRAME, INTRA_FRAME } },
  { NEWMV, { LAST3_FRAME, INTRA_FRAME } },
#endif  // CONFIG_EXT_REFS

  { ZEROMV, { GOLDEN_FRAME, INTRA_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, INTRA_FRAME } },
  { NEARMV, { GOLDEN_FRAME, INTRA_FRAME } },
  { NEWMV, { GOLDEN_FRAME, INTRA_FRAME } },

#if CONFIG_EXT_REFS
  { ZEROMV, { BWDREF_FRAME, INTRA_FRAME } },
  { NEARESTMV, { BWDREF_FRAME, INTRA_FRAME } },
  { NEARMV, { BWDREF_FRAME, INTRA_FRAME } },
  { NEWMV, { BWDREF_FRAME, INTRA_FRAME } },
#endif  // CONFIG_EXT_REFS

  { ZEROMV, { ALTREF_FRAME, INTRA_FRAME } },
  { NEARESTMV, { ALTREF_FRAME, INTRA_FRAME } },
  { NEARMV, { ALTREF_FRAME, INTRA_FRAME } },
  { NEWMV, { ALTREF_FRAME, INTRA_FRAME } },
#endif  // CONFIG_EXT_INTER
};

#if CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA || CONFIG_PALETTE
static INLINE int write_uniform_cost(int n, int v) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  if (l == 0) return 0;
  if (v < m)
    return (l - 1) * av1_cost_bit(128, 0);
  else
    return l * av1_cost_bit(128, 0);
}
#endif  // CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA || CONFIG_PALETTE

// constants for prune 1 and prune 2 decision boundaries
#define FAST_EXT_TX_CORR_MID 0.0
#define FAST_EXT_TX_EDST_MID 0.1
#define FAST_EXT_TX_CORR_MARGIN 0.5
#define FAST_EXT_TX_EDST_MARGIN 0.3

#if CONFIG_DAALA_DIST
static int od_compute_var_4x4(od_coeff *x, int stride) {
  int sum;
  int s2;
  int i;
  sum = 0;
  s2 = 0;
  for (i = 0; i < 4; i++) {
    int j;
    for (j = 0; j < 4; j++) {
      int t;

      t = x[i * stride + j];
      sum += t;
      s2 += t * t;
    }
  }
  // TODO(yushin) : Check wheter any changes are required for high bit depth.
  return (s2 - (sum * sum >> 4)) >> 4;
}

/* OD_DIST_LP_MID controls the frequency weighting filter used for computing
   the distortion. For a value X, the filter is [1 X 1]/(X + 2) and
   is applied both horizontally and vertically. For X=5, the filter is
   a good approximation for the OD_QM8_Q4_HVS quantization matrix. */
#define OD_DIST_LP_MID (5)
#define OD_DIST_LP_NORM (OD_DIST_LP_MID + 2)

static double od_compute_dist_8x8(int qm, int use_activity_masking, od_coeff *x,
                                  od_coeff *y, od_coeff *e_lp, int stride) {
  double sum;
  int min_var;
  double mean_var;
  double var_stat;
  double activity;
  double calibration;
  int i;
  int j;
  double vardist;

  vardist = 0;
  OD_ASSERT(qm != OD_FLAT_QM);
  (void)qm;
#if 1
  min_var = INT_MAX;
  mean_var = 0;
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      int varx;
      int vary;
      varx = od_compute_var_4x4(x + 2 * i * stride + 2 * j, stride);
      vary = od_compute_var_4x4(y + 2 * i * stride + 2 * j, stride);
      min_var = OD_MINI(min_var, varx);
      mean_var += 1. / (1 + varx);
      /* The cast to (double) is to avoid an overflow before the sqrt.*/
      vardist += varx - 2 * sqrt(varx * (double)vary) + vary;
    }
  }
  /* We use a different variance statistic depending on whether activity
     masking is used, since the harmonic mean appeared slghtly worse with
     masking off. The calibration constant just ensures that we preserve the
     rate compared to activity=1. */
  if (use_activity_masking) {
    calibration = 1.95;
    var_stat = 9. / mean_var;
  } else {
    calibration = 1.62;
    var_stat = min_var;
  }
  /* 1.62 is a calibration constant, 0.25 is a noise floor and 1/6 is the
     activity masking constant. */
  activity = calibration * pow(.25 + var_stat, -1. / 6);
#else
  activity = 1;
#endif  // 1
  sum = 0;
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++)
      sum += e_lp[i * stride + j] * (double)e_lp[i * stride + j];
  }
  /* Normalize the filter to unit DC response. */
  sum *= 1. / (OD_DIST_LP_NORM * OD_DIST_LP_NORM * OD_DIST_LP_NORM *
               OD_DIST_LP_NORM);
  return activity * activity * (sum + vardist);
}

// Note : Inputs x and y are in a pixel domain
static double od_compute_dist(int qm, int activity_masking, od_coeff *x,
                              od_coeff *y, int bsize_w, int bsize_h,
                              int qindex) {
  int i;
  double sum;
  sum = 0;

  assert(bsize_w >= 8 && bsize_h >= 8);

  if (qm == OD_FLAT_QM) {
    for (i = 0; i < bsize_w * bsize_h; i++) {
      double tmp;
      tmp = x[i] - y[i];
      sum += tmp * tmp;
    }
  } else {
    int j;
    DECLARE_ALIGNED(16, od_coeff, e[MAX_TX_SQUARE]);
    DECLARE_ALIGNED(16, od_coeff, tmp[MAX_TX_SQUARE]);
    DECLARE_ALIGNED(16, od_coeff, e_lp[MAX_TX_SQUARE]);
    int mid = OD_DIST_LP_MID;
    for (i = 0; i < bsize_h; i++) {
      for (j = 0; j < bsize_w; j++) {
        e[i * bsize_w + j] = x[i * bsize_w + j] - y[i * bsize_w + j];
      }
    }
    for (i = 0; i < bsize_h; i++) {
      tmp[i * bsize_w] = mid * e[i * bsize_w] + 2 * e[i * bsize_w + 1];
      tmp[i * bsize_w + bsize_w - 1] =
          mid * e[i * bsize_w + bsize_w - 1] + 2 * e[i * bsize_w + bsize_w - 2];
      for (j = 1; j < bsize_w - 1; j++) {
        tmp[i * bsize_w + j] = mid * e[i * bsize_w + j] +
                               e[i * bsize_w + j - 1] + e[i * bsize_w + j + 1];
      }
    }
    for (j = 0; j < bsize_w; j++) {
      e_lp[j] = mid * tmp[j] + 2 * tmp[bsize_w + j];
      e_lp[(bsize_h - 1) * bsize_w + j] =
          mid * tmp[(bsize_h - 1) * bsize_w + j] +
          2 * tmp[(bsize_h - 2) * bsize_w + j];
    }
    for (i = 1; i < bsize_h - 1; i++) {
      for (j = 0; j < bsize_w; j++) {
        e_lp[i * bsize_w + j] = mid * tmp[i * bsize_w + j] +
                                tmp[(i - 1) * bsize_w + j] +
                                tmp[(i + 1) * bsize_w + j];
      }
    }
    for (i = 0; i < bsize_h; i += 8) {
      for (j = 0; j < bsize_w; j += 8) {
        sum += od_compute_dist_8x8(qm, activity_masking, &x[i * bsize_w + j],
                                   &y[i * bsize_w + j], &e_lp[i * bsize_w + j],
                                   bsize_w);
      }
    }
    /* Scale according to linear regression against SSE, for 8x8 blocks. */
    if (activity_masking) {
      sum *= 2.2 + (1.7 - 2.2) * (qindex - 99) / (210 - 99) +
             (qindex < 99 ? 2.5 * (qindex - 99) / 99 * (qindex - 99) / 99 : 0);
    } else {
      sum *= qindex >= 128
                 ? 1.4 + (0.9 - 1.4) * (qindex - 128) / (209 - 128)
                 : qindex <= 43
                       ? 1.5 + (2.0 - 1.5) * (qindex - 43) / (16 - 43)
                       : 1.5 + (1.4 - 1.5) * (qindex - 43) / (128 - 43);
    }
  }
  return sum;
}

int64_t av1_daala_dist(const uint8_t *src, int src_stride, const uint8_t *dst,
                       int dst_stride, int bsw, int bsh, int qm,
                       int use_activity_masking, int qindex) {
  int i, j;
  int64_t d;
  DECLARE_ALIGNED(16, od_coeff, orig[MAX_TX_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, rec[MAX_TX_SQUARE]);

  assert(qm == OD_HVS_QM);

  for (j = 0; j < bsh; j++)
    for (i = 0; i < bsw; i++) orig[j * bsw + i] = src[j * src_stride + i];

  for (j = 0; j < bsh; j++)
    for (i = 0; i < bsw; i++) rec[j * bsw + i] = dst[j * dst_stride + i];

  d = (int64_t)od_compute_dist(qm, use_activity_masking, orig, rec, bsw, bsh,
                               qindex);
  return d;
}
#endif  // CONFIG_DAALA_DIST

static void get_energy_distribution_fine(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                                         const uint8_t *src, int src_stride,
                                         const uint8_t *dst, int dst_stride,
                                         double *hordist, double *verdist) {
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  unsigned int esq[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  const int f_index = bsize - BLOCK_16X16;
  if (f_index < 0) {
    const int w_shift = bw == 8 ? 1 : 2;
    const int h_shift = bh == 8 ? 1 : 2;
#if CONFIG_HIGHBITDEPTH
    if (cpi->common.use_highbitdepth) {
      const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
      const uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
      for (int i = 0; i < bh; ++i)
        for (int j = 0; j < bw; ++j) {
          const int index = (j >> w_shift) + ((i >> h_shift) << 2);
          esq[index] +=
              (src16[j + i * src_stride] - dst16[j + i * dst_stride]) *
              (src16[j + i * src_stride] - dst16[j + i * dst_stride]);
        }
    } else {
#endif  // CONFIG_HIGHBITDEPTH

      for (int i = 0; i < bh; ++i)
        for (int j = 0; j < bw; ++j) {
          const int index = (j >> w_shift) + ((i >> h_shift) << 2);
          esq[index] += (src[j + i * src_stride] - dst[j + i * dst_stride]) *
                        (src[j + i * src_stride] - dst[j + i * dst_stride]);
        }
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH
  } else {
    cpi->fn_ptr[f_index].vf(src, src_stride, dst, dst_stride, &esq[0]);
    cpi->fn_ptr[f_index].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[1]);
    cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[2]);
    cpi->fn_ptr[f_index].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[3]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[f_index].vf(src, src_stride, dst, dst_stride, &esq[4]);
    cpi->fn_ptr[f_index].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[5]);
    cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[6]);
    cpi->fn_ptr[f_index].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[7]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[f_index].vf(src, src_stride, dst, dst_stride, &esq[8]);
    cpi->fn_ptr[f_index].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[9]);
    cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[10]);
    cpi->fn_ptr[f_index].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[11]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[f_index].vf(src, src_stride, dst, dst_stride, &esq[12]);
    cpi->fn_ptr[f_index].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[13]);
    cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[14]);
    cpi->fn_ptr[f_index].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[15]);
  }

  double total = (double)esq[0] + esq[1] + esq[2] + esq[3] + esq[4] + esq[5] +
                 esq[6] + esq[7] + esq[8] + esq[9] + esq[10] + esq[11] +
                 esq[12] + esq[13] + esq[14] + esq[15];
  if (total > 0) {
    const double e_recip = 1.0 / total;
    hordist[0] = ((double)esq[0] + esq[4] + esq[8] + esq[12]) * e_recip;
    hordist[1] = ((double)esq[1] + esq[5] + esq[9] + esq[13]) * e_recip;
    hordist[2] = ((double)esq[2] + esq[6] + esq[10] + esq[14]) * e_recip;
    verdist[0] = ((double)esq[0] + esq[1] + esq[2] + esq[3]) * e_recip;
    verdist[1] = ((double)esq[4] + esq[5] + esq[6] + esq[7]) * e_recip;
    verdist[2] = ((double)esq[8] + esq[9] + esq[10] + esq[11]) * e_recip;
  } else {
    hordist[0] = verdist[0] = 0.25;
    hordist[1] = verdist[1] = 0.25;
    hordist[2] = verdist[2] = 0.25;
  }
}

static int adst_vs_flipadst(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                            const uint8_t *src, int src_stride,
                            const uint8_t *dst, int dst_stride) {
  int prune_bitmask = 0;
  double svm_proj_h = 0, svm_proj_v = 0;
  double hdist[3] = { 0, 0, 0 }, vdist[3] = { 0, 0, 0 };
  get_energy_distribution_fine(cpi, bsize, src, src_stride, dst, dst_stride,
                               hdist, vdist);

  svm_proj_v = vdist[0] * ADST_FLIP_SVM[0] + vdist[1] * ADST_FLIP_SVM[1] +
               vdist[2] * ADST_FLIP_SVM[2] + ADST_FLIP_SVM[3];
  svm_proj_h = hdist[0] * ADST_FLIP_SVM[4] + hdist[1] * ADST_FLIP_SVM[5] +
               hdist[2] * ADST_FLIP_SVM[6] + ADST_FLIP_SVM[7];
  if (svm_proj_v > FAST_EXT_TX_EDST_MID + FAST_EXT_TX_EDST_MARGIN)
    prune_bitmask |= 1 << FLIPADST_1D;
  else if (svm_proj_v < FAST_EXT_TX_EDST_MID - FAST_EXT_TX_EDST_MARGIN)
    prune_bitmask |= 1 << ADST_1D;

  if (svm_proj_h > FAST_EXT_TX_EDST_MID + FAST_EXT_TX_EDST_MARGIN)
    prune_bitmask |= 1 << (FLIPADST_1D + 8);
  else if (svm_proj_h < FAST_EXT_TX_EDST_MID - FAST_EXT_TX_EDST_MARGIN)
    prune_bitmask |= 1 << (ADST_1D + 8);

  return prune_bitmask;
}

#if CONFIG_EXT_TX
static void get_horver_correlation(const int16_t *diff, int stride, int w,
                                   int h, double *hcorr, double *vcorr) {
  // Returns hor/ver correlation coefficient
  const int num = (h - 1) * (w - 1);
  double num_r;
  int i, j;
  int64_t xy_sum = 0, xz_sum = 0;
  int64_t x_sum = 0, y_sum = 0, z_sum = 0;
  int64_t x2_sum = 0, y2_sum = 0, z2_sum = 0;
  double x_var_n, y_var_n, z_var_n, xy_var_n, xz_var_n;
  *hcorr = *vcorr = 1;

  assert(num > 0);
  num_r = 1.0 / num;
  for (i = 1; i < h; ++i) {
    for (j = 1; j < w; ++j) {
      const int16_t x = diff[i * stride + j];
      const int16_t y = diff[i * stride + j - 1];
      const int16_t z = diff[(i - 1) * stride + j];
      xy_sum += x * y;
      xz_sum += x * z;
      x_sum += x;
      y_sum += y;
      z_sum += z;
      x2_sum += x * x;
      y2_sum += y * y;
      z2_sum += z * z;
    }
  }
  x_var_n = x2_sum - (x_sum * x_sum) * num_r;
  y_var_n = y2_sum - (y_sum * y_sum) * num_r;
  z_var_n = z2_sum - (z_sum * z_sum) * num_r;
  xy_var_n = xy_sum - (x_sum * y_sum) * num_r;
  xz_var_n = xz_sum - (x_sum * z_sum) * num_r;
  if (x_var_n > 0 && y_var_n > 0) {
    *hcorr = xy_var_n / sqrt(x_var_n * y_var_n);
    *hcorr = *hcorr < 0 ? 0 : *hcorr;
  }
  if (x_var_n > 0 && z_var_n > 0) {
    *vcorr = xz_var_n / sqrt(x_var_n * z_var_n);
    *vcorr = *vcorr < 0 ? 0 : *vcorr;
  }
}

int dct_vs_idtx(const int16_t *diff, int stride, int w, int h) {
  double hcorr, vcorr;
  int prune_bitmask = 0;
  get_horver_correlation(diff, stride, w, h, &hcorr, &vcorr);

  if (vcorr > FAST_EXT_TX_CORR_MID + FAST_EXT_TX_CORR_MARGIN)
    prune_bitmask |= 1 << IDTX_1D;
  else if (vcorr < FAST_EXT_TX_CORR_MID - FAST_EXT_TX_CORR_MARGIN)
    prune_bitmask |= 1 << DCT_1D;

  if (hcorr > FAST_EXT_TX_CORR_MID + FAST_EXT_TX_CORR_MARGIN)
    prune_bitmask |= 1 << (IDTX_1D + 8);
  else if (hcorr < FAST_EXT_TX_CORR_MID - FAST_EXT_TX_CORR_MARGIN)
    prune_bitmask |= 1 << (DCT_1D + 8);
  return prune_bitmask;
}

// Performance drop: 0.5%, Speed improvement: 24%
static int prune_two_for_sby(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                             MACROBLOCK *x, const MACROBLOCKD *xd,
                             int adst_flipadst, int dct_idtx) {
  int prune = 0;

  if (adst_flipadst) {
    const struct macroblock_plane *const p = &x->plane[0];
    const struct macroblockd_plane *const pd = &xd->plane[0];
    prune |= adst_vs_flipadst(cpi, bsize, p->src.buf, p->src.stride,
                              pd->dst.buf, pd->dst.stride);
  }
  if (dct_idtx) {
    av1_subtract_plane(x, bsize, 0);
    const struct macroblock_plane *const p = &x->plane[0];
    const int bw = 4 << (b_width_log2_lookup[bsize]);
    const int bh = 4 << (b_height_log2_lookup[bsize]);
    prune |= dct_vs_idtx(p->src_diff, bw, bw, bh);
  }

  return prune;
}
#endif  // CONFIG_EXT_TX

// Performance drop: 0.3%, Speed improvement: 5%
static int prune_one_for_sby(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                             const MACROBLOCK *x, const MACROBLOCKD *xd) {
  const struct macroblock_plane *const p = &x->plane[0];
  const struct macroblockd_plane *const pd = &xd->plane[0];
  return adst_vs_flipadst(cpi, bsize, p->src.buf, p->src.stride, pd->dst.buf,
                          pd->dst.stride);
}

static int prune_tx_types(const AV1_COMP *cpi, BLOCK_SIZE bsize, MACROBLOCK *x,
                          const MACROBLOCKD *const xd, int tx_set) {
#if CONFIG_EXT_TX
  const int *tx_set_1D = tx_set >= 0 ? ext_tx_used_inter_1D[tx_set] : NULL;
#else
  const int tx_set_1D[TX_TYPES_1D] = { 0 };
#endif  // CONFIG_EXT_TX

  switch (cpi->sf.tx_type_search.prune_mode) {
    case NO_PRUNE: return 0; break;
    case PRUNE_ONE:
      if ((tx_set >= 0) && !(tx_set_1D[FLIPADST_1D] & tx_set_1D[ADST_1D]))
        return 0;
      return prune_one_for_sby(cpi, bsize, x, xd);
      break;
#if CONFIG_EXT_TX
    case PRUNE_TWO:
      if ((tx_set >= 0) && !(tx_set_1D[FLIPADST_1D] & tx_set_1D[ADST_1D])) {
        if (!(tx_set_1D[DCT_1D] & tx_set_1D[IDTX_1D])) return 0;
        return prune_two_for_sby(cpi, bsize, x, xd, 0, 1);
      }
      if ((tx_set >= 0) && !(tx_set_1D[DCT_1D] & tx_set_1D[IDTX_1D]))
        return prune_two_for_sby(cpi, bsize, x, xd, 1, 0);
      return prune_two_for_sby(cpi, bsize, x, xd, 1, 1);
      break;
#endif  // CONFIG_EXT_TX
  }
  assert(0);
  return 0;
}

static int do_tx_type_search(TX_TYPE tx_type, int prune) {
// TODO(sarahparker) implement for non ext tx
#if CONFIG_EXT_TX
  return !(((prune >> vtx_tab[tx_type]) & 1) |
           ((prune >> (htx_tab[tx_type] + 8)) & 1));
#else
  // temporary to avoid compiler warnings
  (void)vtx_tab;
  (void)htx_tab;
  (void)tx_type;
  (void)prune;
  return 1;
#endif  // CONFIG_EXT_TX
}

static void model_rd_from_sse(const AV1_COMP *const cpi,
                              const MACROBLOCKD *const xd, BLOCK_SIZE bsize,
                              int plane, int64_t sse, int *rate,
                              int64_t *dist) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int dequant_shift =
#if CONFIG_HIGHBITDEPTH
      (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd - 5 :
#endif  // CONFIG_HIGHBITDEPTH
                                                    3;

  // Fast approximate the modelling function.
  if (cpi->sf.simple_model_rd_from_var) {
    const int64_t square_error = sse;
    int quantizer = (pd->dequant[1] >> dequant_shift);

    if (quantizer < 120)
      *rate = (int)((square_error * (280 - quantizer)) >>
                    (16 - AV1_PROB_COST_SHIFT));
    else
      *rate = 0;
    *dist = (square_error * quantizer) >> 8;
  } else {
    av1_model_rd_from_var_lapndz(sse, num_pels_log2_lookup[bsize],
                                 pd->dequant[1] >> dequant_shift, rate, dist);
  }

  *dist <<= 4;
}

static void model_rd_for_sb(const AV1_COMP *const cpi, BLOCK_SIZE bsize,
                            MACROBLOCK *x, MACROBLOCKD *xd, int plane_from,
                            int plane_to, int *out_rate_sum,
                            int64_t *out_dist_sum, int *skip_txfm_sb,
                            int64_t *skip_sse_sb) {
  // Note our transform coeffs are 8 times an orthogonal transform.
  // Hence quantizer step is also 8 times. To get effective quantizer
  // we need to divide by 8 before sending to modeling function.
  int plane;
  const int ref = xd->mi[0]->mbmi.ref_frame[0];

  int64_t rate_sum = 0;
  int64_t dist_sum = 0;
  int64_t total_sse = 0;

  x->pred_sse[ref] = 0;

  for (plane = plane_from; plane <= plane_to; ++plane) {
    struct macroblock_plane *const p = &x->plane[plane];
    struct macroblockd_plane *const pd = &xd->plane[plane];
#if CONFIG_CB4X4 && !CONFIG_CHROMA_2X2
    const BLOCK_SIZE bs = AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#else
    const BLOCK_SIZE bs = get_plane_block_size(bsize, pd);
#endif  // CONFIG_CB4X4 && !CONFIG_CHROMA_2X2

    unsigned int sse;
    int rate;
    int64_t dist;

#if CONFIG_CB4X4
    if (x->skip_chroma_rd && plane) continue;
#endif  // CONFIG_CB4X4

    // TODO(geza): Write direct sse functions that do not compute
    // variance as well.
    cpi->fn_ptr[bs].vf(p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride,
                       &sse);

    if (plane == 0) x->pred_sse[ref] = sse;

    total_sse += sse;

    model_rd_from_sse(cpi, xd, bs, plane, sse, &rate, &dist);

    rate_sum += rate;
    dist_sum += dist;
  }

  *skip_txfm_sb = total_sse == 0;
  *skip_sse_sb = total_sse << 4;
  *out_rate_sum = (int)rate_sum;
  *out_dist_sum = dist_sum;
}

int64_t av1_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff,
                          intptr_t block_size, int64_t *ssz) {
  int i;
  int64_t error = 0, sqcoeff = 0;

  for (i = 0; i < block_size; i++) {
    const int diff = coeff[i] - dqcoeff[i];
    error += diff * diff;
    sqcoeff += coeff[i] * coeff[i];
  }

  *ssz = sqcoeff;
  return error;
}

int64_t av1_block_error_fp_c(const int16_t *coeff, const int16_t *dqcoeff,
                             int block_size) {
  int i;
  int64_t error = 0;

  for (i = 0; i < block_size; i++) {
    const int diff = coeff[i] - dqcoeff[i];
    error += diff * diff;
  }

  return error;
}

#if CONFIG_HIGHBITDEPTH
int64_t av1_highbd_block_error_c(const tran_low_t *coeff,
                                 const tran_low_t *dqcoeff, intptr_t block_size,
                                 int64_t *ssz, int bd) {
  int i;
  int64_t error = 0, sqcoeff = 0;
  int shift = 2 * (bd - 8);
  int rounding = shift > 0 ? 1 << (shift - 1) : 0;

  for (i = 0; i < block_size; i++) {
    const int64_t diff = coeff[i] - dqcoeff[i];
    error += diff * diff;
    sqcoeff += (int64_t)coeff[i] * (int64_t)coeff[i];
  }
  assert(error >= 0 && sqcoeff >= 0);
  error = (error + rounding) >> shift;
  sqcoeff = (sqcoeff + rounding) >> shift;

  *ssz = sqcoeff;
  return error;
}
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_PVQ
// Without PVQ, av1_block_error_c() return two kind of errors,
// 1) reconstruction (i.e. decoded) error and
// 2) Squared sum of transformed residue (i.e. 'coeff')
// However, if PVQ is enabled, coeff does not keep the transformed residue
// but instead a transformed original is kept.
// Hence, new parameter ref vector (i.e. transformed predicted signal)
// is required to derive the residue signal,
// i.e. coeff - ref = residue (all transformed).

#if CONFIG_HIGHBITDEPTH
static int64_t av1_highbd_block_error2_c(const tran_low_t *coeff,
                                         const tran_low_t *dqcoeff,
                                         const tran_low_t *ref,
                                         intptr_t block_size, int64_t *ssz,
                                         int bd) {
  int64_t error;
  int64_t sqcoeff;
  int shift = 2 * (bd - 8);
  int rounding = shift > 0 ? 1 << (shift - 1) : 0;
  // Use the existing sse codes for calculating distortion of decoded signal:
  // i.e. (orig - decoded)^2
  // For high bit depth, throw away ssz until a 32-bit version of
  // av1_block_error_fp is written.
  int64_t ssz_trash;
  error = av1_block_error(coeff, dqcoeff, block_size, &ssz_trash);
  // prediction residue^2 = (orig - ref)^2
  sqcoeff = av1_block_error(coeff, ref, block_size, &ssz_trash);
  error = (error + rounding) >> shift;
  sqcoeff = (sqcoeff + rounding) >> shift;
  *ssz = sqcoeff;
  return error;
}
#else
// TODO(yushin) : Since 4x4 case does not need ssz, better to refactor into
// a separate function that does not do the extra computations for ssz.
static int64_t av1_block_error2_c(const tran_low_t *coeff,
                                  const tran_low_t *dqcoeff,
                                  const tran_low_t *ref, intptr_t block_size,
                                  int64_t *ssz) {
  int64_t error;
  // Use the existing sse codes for calculating distortion of decoded signal:
  // i.e. (orig - decoded)^2
  error = av1_block_error_fp(coeff, dqcoeff, block_size);
  // prediction residue^2 = (orig - ref)^2
  *ssz = av1_block_error_fp(coeff, ref, block_size);
  return error;
}
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_PVQ

#if !CONFIG_PVQ || CONFIG_VAR_TX
/* The trailing '0' is a terminator which is used inside av1_cost_coeffs() to
 * decide whether to include cost of a trailing EOB node or not (i.e. we
 * can skip this if the last coefficient in this transform block, e.g. the
 * 16th coefficient in a 4x4 block or the 64th coefficient in a 8x8 block,
 * were non-zero). */
#if !CONFIG_LV_MAP
static int cost_coeffs(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                       int block, TX_SIZE tx_size, const SCAN_ORDER *scan_order,
                       const ENTROPY_CONTEXT *a, const ENTROPY_CONTEXT *l,
                       int use_fast_coef_costing) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  const struct macroblockd_plane *pd = &xd->plane[plane];
  const PLANE_TYPE type = pd->plane_type;
  const uint16_t *band_count = &band_count_table[tx_size][1];
  const int eob = p->eobs[block];
  const tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  const int tx_size_ctx = txsize_sqr_map[tx_size];
  unsigned int(*token_costs)[2][COEFF_CONTEXTS][ENTROPY_TOKENS] =
      x->token_costs[tx_size_ctx][type][is_inter_block(mbmi)];
  uint8_t token_cache[MAX_TX_SQUARE];
  int pt = combine_entropy_contexts(*a, *l);
  int c, cost;
  const int16_t *scan = scan_order->scan;
  const int16_t *nb = scan_order->neighbors;
  const int ref = is_inter_block(mbmi);
  aom_prob *blockz_probs =
      cm->fc->blockzero_probs[txsize_sqr_map[tx_size]][type][ref];

#if CONFIG_HIGHBITDEPTH
  const int cat6_bits = av1_get_cat6_extrabits_size(tx_size, xd->bd);
#else
  const int cat6_bits = av1_get_cat6_extrabits_size(tx_size, 8);
#endif  // CONFIG_HIGHBITDEPTH

#if !CONFIG_VAR_TX && !CONFIG_SUPERTX
  // Check for consistency of tx_size with mode info
  assert(tx_size == get_tx_size(plane, xd));
#endif  // !CONFIG_VAR_TX && !CONFIG_SUPERTX
  (void)cm;

  if (eob == 0) {
    // single eob token
    cost = av1_cost_bit(blockz_probs[pt], 0);
  } else {
    if (use_fast_coef_costing) {
      int band_left = *band_count++;

      // dc token
      int v = qcoeff[0];
      int16_t prev_t;
      cost = av1_get_token_cost(v, &prev_t, cat6_bits);
      cost += (*token_costs)[!prev_t][pt][prev_t];

      token_cache[0] = av1_pt_energy_class[prev_t];
      ++token_costs;

      // ac tokens
      for (c = 1; c < eob; c++) {
        const int rc = scan[c];
        int16_t t;

        v = qcoeff[rc];
        cost += av1_get_token_cost(v, &t, cat6_bits);
        cost += (*token_costs)[!t][!prev_t][t];
        prev_t = t;
        if (!--band_left) {
          band_left = *band_count++;
          ++token_costs;
        }
      }

      // eob token
      cost += (*token_costs)[0][!prev_t][EOB_TOKEN];

    } else {  // !use_fast_coef_costing
      int band_left = *band_count++;

      // dc token
      int v = qcoeff[0];
      int16_t tok;
      cost = av1_get_token_cost(v, &tok, cat6_bits);
      cost += (*token_costs)[!tok][pt][tok];

      token_cache[0] = av1_pt_energy_class[tok];
      ++token_costs;

      // ac tokens
      for (c = 1; c < eob; c++) {
        const int rc = scan[c];

        v = qcoeff[rc];
        cost += av1_get_token_cost(v, &tok, cat6_bits);
        pt = get_coef_context(nb, token_cache, c);
        cost += (*token_costs)[!tok][pt][tok];
        token_cache[rc] = av1_pt_energy_class[tok];
        if (!--band_left) {
          band_left = *band_count++;
          ++token_costs;
        }
      }

      // eob token
      pt = get_coef_context(nb, token_cache, c);
      cost += (*token_costs)[0][pt][EOB_TOKEN];
    }
  }

  return cost;
}
#endif  // !CONFIG_LV_MAP

int av1_cost_coeffs(const AV1_COMP *const cpi, MACROBLOCK *x, int plane,
                    int block, TX_SIZE tx_size, const SCAN_ORDER *scan_order,
                    const ENTROPY_CONTEXT *a, const ENTROPY_CONTEXT *l,
                    int use_fast_coef_costing) {
#if !CONFIG_LV_MAP
  const AV1_COMMON *const cm = &cpi->common;
  return cost_coeffs(cm, x, plane, block, tx_size, scan_order, a, l,
                     use_fast_coef_costing);
#else  // !CONFIG_LV_MAP
  (void)scan_order;
  (void)use_fast_coef_costing;
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblockd_plane *pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_CB4X4
#if CONFIG_CHROMA_2X2
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#endif  // CONFIG_CHROMA_2X2
#else   // CONFIG_CB4X4
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(AOMMAX(BLOCK_8X8, bsize), pd);
#endif  // CONFIG_CB4X4

  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, a, l, &txb_ctx);
  return av1_cost_coeffs_txb(cpi, x, plane, block, &txb_ctx);
#endif  // !CONFIG_LV_MAP
}
#endif  // !CONFIG_PVQ || CONFIG_VAR_TX

// Get transform block visible dimensions cropped to the MI units.
static void get_txb_dimensions(const MACROBLOCKD *xd, int plane,
                               BLOCK_SIZE plane_bsize, int blk_row, int blk_col,
                               BLOCK_SIZE tx_bsize, int *width, int *height,
                               int *visible_width, int *visible_height) {
#if !(CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT)
  assert(tx_bsize <= plane_bsize);
#endif  // !(CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT)
  int txb_height = block_size_high[tx_bsize];
  int txb_width = block_size_wide[tx_bsize];
  const int block_height = block_size_high[plane_bsize];
  const int block_width = block_size_wide[plane_bsize];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  // TODO(aconverse@google.com): Investigate using crop_width/height here rather
  // than the MI size
  const int block_rows =
      (xd->mb_to_bottom_edge >= 0)
          ? block_height
          : (xd->mb_to_bottom_edge >> (3 + pd->subsampling_y)) + block_height;
  const int block_cols =
      (xd->mb_to_right_edge >= 0)
          ? block_width
          : (xd->mb_to_right_edge >> (3 + pd->subsampling_x)) + block_width;
  const int tx_unit_size = tx_size_wide_log2[0];
  if (width) *width = txb_width;
  if (height) *height = txb_height;
  *visible_width = clamp(block_cols - (blk_col << tx_unit_size), 0, txb_width);
  *visible_height =
      clamp(block_rows - (blk_row << tx_unit_size), 0, txb_height);
}

// Compute the pixel domain sum square error on all visible 4x4s in the
// transform block.
static unsigned pixel_sse(const AV1_COMP *const cpi, const MACROBLOCKD *xd,
                          int plane, const uint8_t *src, const int src_stride,
                          const uint8_t *dst, const int dst_stride, int blk_row,
                          int blk_col, const BLOCK_SIZE plane_bsize,
                          const BLOCK_SIZE tx_bsize) {
  int txb_rows, txb_cols, visible_rows, visible_cols;
  get_txb_dimensions(xd, plane, plane_bsize, blk_row, blk_col, tx_bsize,
                     &txb_cols, &txb_rows, &visible_cols, &visible_rows);
  assert(visible_rows > 0);
  assert(visible_cols > 0);
#if CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT
  if ((txb_rows == visible_rows && txb_cols == visible_cols) &&
      tx_bsize < BLOCK_SIZES) {
#else
  if (txb_rows == visible_rows && txb_cols == visible_cols) {
#endif
    unsigned sse;
    cpi->fn_ptr[tx_bsize].vf(src, src_stride, dst, dst_stride, &sse);
    return sse;
  }
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    uint64_t sse = aom_highbd_sse_odd_size(src, src_stride, dst, dst_stride,
                                           visible_cols, visible_rows);
    return (unsigned int)ROUND_POWER_OF_TWO(sse, (xd->bd - 8) * 2);
  }
#endif  // CONFIG_HIGHBITDEPTH
  unsigned sse = aom_sse_odd_size(src, src_stride, dst, dst_stride,
                                  visible_cols, visible_rows);
  return sse;
}

// Compute the squares sum squares on all visible 4x4s in the transform block.
static int64_t sum_squares_visible(const MACROBLOCKD *xd, int plane,
                                   const int16_t *diff, const int diff_stride,
                                   int blk_row, int blk_col,
                                   const BLOCK_SIZE plane_bsize,
                                   const BLOCK_SIZE tx_bsize) {
  int visible_rows, visible_cols;
  get_txb_dimensions(xd, plane, plane_bsize, blk_row, blk_col, tx_bsize, NULL,
                     NULL, &visible_cols, &visible_rows);
  return aom_sum_squares_2d_i16(diff, diff_stride, visible_cols, visible_rows);
}

void av1_dist_block(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                    BLOCK_SIZE plane_bsize, int block, int blk_row, int blk_col,
                    TX_SIZE tx_size, int64_t *out_dist, int64_t *out_sse,
                    OUTPUT_STATUS output_status) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
#if CONFIG_DAALA_DIST
  int qm = OD_HVS_QM;
  int use_activity_masking = 0;
#if CONFIG_PVQ
  use_activity_masking = x->daala_enc.use_activity_masking;
#endif  // CONFIG_PVQ
  struct macroblockd_plane *const pd = &xd->plane[plane];
#else   // CONFIG_DAALA_DIST
  const struct macroblockd_plane *const pd = &xd->plane[plane];
#endif  // CONFIG_DAALA_DIST

  if (cpi->sf.use_transform_domain_distortion && !CONFIG_DAALA_DIST) {
    // Transform domain distortion computation is more efficient as it does
    // not involve an inverse transform, but it is less accurate.
    const int buffer_length = tx_size_2d[tx_size];
    int64_t this_sse;
    int shift = (MAX_TX_SCALE - av1_get_tx_scale(tx_size)) * 2;
    tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
    tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
#if CONFIG_PVQ
    tran_low_t *ref_coeff = BLOCK_OFFSET(pd->pvq_ref_coeff, block);

#if CONFIG_HIGHBITDEPTH
    const int bd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd : 8;
    *out_dist = av1_highbd_block_error2_c(coeff, dqcoeff, ref_coeff,
                                          buffer_length, &this_sse, bd) >>
                shift;
#else
    *out_dist = av1_block_error2_c(coeff, dqcoeff, ref_coeff, buffer_length,
                                   &this_sse) >>
                shift;
#endif  // CONFIG_HIGHBITDEPTH
#elif CONFIG_HIGHBITDEPTH
    const int bd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd : 8;
    *out_dist =
        av1_highbd_block_error(coeff, dqcoeff, buffer_length, &this_sse, bd) >>
        shift;
#else
    *out_dist =
        av1_block_error(coeff, dqcoeff, buffer_length, &this_sse) >> shift;
#endif  // CONFIG_PVQ
    *out_sse = this_sse >> shift;
  } else {
    const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
#if !CONFIG_PVQ || CONFIG_DAALA_DIST
    const int bsw = block_size_wide[tx_bsize];
    const int bsh = block_size_high[tx_bsize];
#endif
    const int src_stride = x->plane[plane].src.stride;
    const int dst_stride = xd->plane[plane].dst.stride;
    // Scale the transform block index to pixel unit.
    const int src_idx = (blk_row * src_stride + blk_col)
                        << tx_size_wide_log2[0];
    const int dst_idx = (blk_row * dst_stride + blk_col)
                        << tx_size_wide_log2[0];
    const uint8_t *src = &x->plane[plane].src.buf[src_idx];
    const uint8_t *dst = &xd->plane[plane].dst.buf[dst_idx];
    const tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
    const uint16_t eob = p->eobs[block];

    assert(cpi != NULL);
    assert(tx_size_wide_log2[0] == tx_size_high_log2[0]);

#if CONFIG_DAALA_DIST
    if (plane == 0 && bsw >= 8 && bsh >= 8) {
      if (output_status == OUTPUT_HAS_DECODED_PIXELS) {
        const int pred_stride = block_size_wide[plane_bsize];
        const int pred_idx = (blk_row * pred_stride + blk_col)
                             << tx_size_wide_log2[0];
        const int16_t *pred = &pd->pred[pred_idx];
        int i, j;
        DECLARE_ALIGNED(16, uint8_t, pred8[MAX_TX_SQUARE]);

        for (j = 0; j < bsh; j++)
          for (i = 0; i < bsw; i++)
            pred8[j * bsw + i] = pred[j * pred_stride + i];
        *out_sse = av1_daala_dist(src, src_stride, pred8, bsw, bsw, bsh, qm,
                                  use_activity_masking, x->qindex);
      } else {
        *out_sse = av1_daala_dist(src, src_stride, dst, dst_stride, bsw, bsh,
                                  qm, use_activity_masking, x->qindex);
      }
    } else
#endif  // CONFIG_DAALA_DIST
    {
      const int diff_stride = block_size_wide[plane_bsize];
      const int diff_idx = (blk_row * diff_stride + blk_col)
                           << tx_size_wide_log2[0];
      const int16_t *diff = &p->src_diff[diff_idx];
      *out_sse = sum_squares_visible(xd, plane, diff, diff_stride, blk_row,
                                     blk_col, plane_bsize, tx_bsize);
#if CONFIG_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
        *out_sse = ROUND_POWER_OF_TWO(*out_sse, (xd->bd - 8) * 2);
#endif  // CONFIG_HIGHBITDEPTH
    }
    *out_sse *= 16;

    if (eob) {
      if (output_status == OUTPUT_HAS_DECODED_PIXELS) {
#if CONFIG_DAALA_DIST
        if (plane == 0 && bsw >= 8 && bsh >= 8)
          *out_dist = av1_daala_dist(src, src_stride, dst, dst_stride, bsw, bsh,
                                     qm, use_activity_masking, x->qindex);
        else
#endif  // CONFIG_DAALA_DIST
          *out_dist =
              pixel_sse(cpi, xd, plane, src, src_stride, dst, dst_stride,
                        blk_row, blk_col, plane_bsize, tx_bsize);
      } else {
#if CONFIG_HIGHBITDEPTH
        uint8_t *recon;
        DECLARE_ALIGNED(16, uint16_t, recon16[MAX_TX_SQUARE]);

        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
          recon = CONVERT_TO_BYTEPTR(recon16);
        else
          recon = (uint8_t *)recon16;
#else
        DECLARE_ALIGNED(16, uint8_t, recon[MAX_TX_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH

#if !CONFIG_PVQ
#if CONFIG_HIGHBITDEPTH
        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          aom_highbd_convolve_copy(dst, dst_stride, recon, MAX_TX_SIZE, NULL, 0,
                                   NULL, 0, bsw, bsh, xd->bd);
        } else {
#endif  // CONFIG_HIGHBITDEPTH
          aom_convolve_copy(dst, dst_stride, recon, MAX_TX_SIZE, NULL, 0, NULL,
                            0, bsw, bsh);
#if CONFIG_HIGHBITDEPTH
        }
#endif  // CONFIG_HIGHBITDEPTH
#else
        (void)dst;
#endif  // !CONFIG_PVQ

        const PLANE_TYPE plane_type = get_plane_type(plane);
        TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);

        av1_inverse_transform_block(xd, dqcoeff, tx_type, tx_size, recon,
                                    MAX_TX_SIZE, eob);

#if CONFIG_DAALA_DIST
        if (plane == 0 && bsw >= 8 && bsh >= 8) {
          *out_dist = av1_daala_dist(src, src_stride, recon, MAX_TX_SIZE, bsw,
                                     bsh, qm, use_activity_masking, x->qindex);
        } else {
          if (plane == 0) {
            // Save decoded pixels for inter block in pd->pred to avoid
            // block_8x8_rd_txfm_daala_dist() need to produce them
            // by calling av1_inverse_transform_block() again.
            const int pred_stride = block_size_wide[plane_bsize];
            const int pred_idx = (blk_row * pred_stride + blk_col)
                                 << tx_size_wide_log2[0];
            int16_t *pred = &pd->pred[pred_idx];
            int i, j;

            for (j = 0; j < bsh; j++)
              for (i = 0; i < bsw; i++)
                pred[j * pred_stride + i] = recon[j * MAX_TX_SIZE + i];
          }
#endif  // CONFIG_DAALA_DIST
          *out_dist =
              pixel_sse(cpi, xd, plane, src, src_stride, recon, MAX_TX_SIZE,
                        blk_row, blk_col, plane_bsize, tx_bsize);
#if CONFIG_DAALA_DIST
        }
#endif  // CONFIG_DAALA_DIST
      }
      *out_dist *= 16;
    } else {
      *out_dist = *out_sse;
    }
  }
}

static void block_rd_txfm(int plane, int block, int blk_row, int blk_col,
                          BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
  struct rdcost_block_args *args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const AV1_COMP *cpi = args->cpi;
  ENTROPY_CONTEXT *a = args->t_above + blk_col;
  ENTROPY_CONTEXT *l = args->t_left + blk_row;
#if !CONFIG_TXK_SEL
  const AV1_COMMON *cm = &cpi->common;
#endif
  int64_t rd1, rd2, rd;
  RD_STATS this_rd_stats;

  assert(tx_size == get_tx_size(plane, xd));

  av1_init_rd_stats(&this_rd_stats);

  if (args->exit_early) return;

  if (!is_inter_block(mbmi)) {
#if CONFIG_CFL

#if CONFIG_EC_ADAPT
    FRAME_CONTEXT *const ec_ctx = xd->tile_ctx;
#else
    FRAME_CONTEXT *const ec_ctx = cm->fc;
#endif  // CONFIG_EC_ADAPT

    av1_predict_intra_block_encoder_facade(x, ec_ctx, plane, block, blk_col,
                                           blk_row, tx_size, plane_bsize);
#else
    av1_predict_intra_block_facade(xd, plane, block, blk_col, blk_row, tx_size);
#endif
#if CONFIG_DPCM_INTRA
    const int block_raster_idx =
        av1_block_index_to_raster_order(tx_size, block);
    const PREDICTION_MODE mode =
        (plane == 0) ? get_y_mode(xd->mi[0], block_raster_idx) : mbmi->uv_mode;
    TX_TYPE tx_type = get_tx_type((plane == 0) ? PLANE_TYPE_Y : PLANE_TYPE_UV,
                                  xd, block, tx_size);
    if (av1_use_dpcm_intra(plane, mode, tx_type, mbmi)) {
      int8_t skip;
      av1_encode_block_intra_dpcm(cm, x, mode, plane, block, blk_row, blk_col,
                                  plane_bsize, tx_size, tx_type, a, l, &skip);
      av1_dist_block(args->cpi, x, plane, plane_bsize, block, blk_row, blk_col,
                     tx_size, &this_rd_stats.dist, &this_rd_stats.sse,
                     OUTPUT_HAS_DECODED_PIXELS);
      goto CALCULATE_RD;
    }
#endif  // CONFIG_DPCM_INTRA
    av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size);
  }

#if !CONFIG_TXK_SEL
  // full forward transform and quantization
  const int coeff_ctx = combine_entropy_contexts(*a, *l);
  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  coeff_ctx, AV1_XFORM_QUANT_FP);
  av1_optimize_b(cm, x, plane, block, plane_bsize, tx_size, a, l);

  if (!is_inter_block(mbmi)) {
    struct macroblock_plane *const p = &x->plane[plane];
    av1_inverse_transform_block_facade(xd, plane, block, blk_row, blk_col,
                                       p->eobs[block]);
    av1_dist_block(args->cpi, x, plane, plane_bsize, block, blk_row, blk_col,
                   tx_size, &this_rd_stats.dist, &this_rd_stats.sse,
                   OUTPUT_HAS_DECODED_PIXELS);
  } else {
    av1_dist_block(args->cpi, x, plane, plane_bsize, block, blk_row, blk_col,
                   tx_size, &this_rd_stats.dist, &this_rd_stats.sse,
                   OUTPUT_HAS_PREDICTED_PIXELS);
  }
#if CONFIG_CFL
  if (plane == AOM_PLANE_Y && x->cfl_store_y) {
    struct macroblockd_plane *const pd = &xd->plane[plane];
    const int dst_stride = pd->dst.stride;
    uint8_t *dst =
        &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
    cfl_store(xd->cfl, dst, dst_stride, blk_row, blk_col, tx_size);
  }
#endif
#if CONFIG_DPCM_INTRA
CALCULATE_RD : {}
#endif  // CONFIG_DPCM_INTRA
  rd = RDCOST(x->rdmult, x->rddiv, 0, this_rd_stats.dist);
  if (args->this_rd + rd > args->best_rd) {
    args->exit_early = 1;
    return;
  }
#if !CONFIG_PVQ
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  const SCAN_ORDER *scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
  this_rd_stats.rate =
      av1_cost_coeffs(cpi, x, plane, block, tx_size, scan_order, a, l,
                      args->use_fast_coef_costing);
#else   // !CONFIG_PVQ
  this_rd_stats.rate = x->rate;
#endif  // !CONFIG_PVQ
#else   // !CONFIG_TXK_SEL
  av1_search_txk_type(cpi, x, plane, block, blk_row, blk_col, plane_bsize,
                      tx_size, a, l, args->use_fast_coef_costing,
                      &this_rd_stats);
#endif  // !CONFIG_TXK_SEL

#if !CONFIG_PVQ
#if CONFIG_RD_DEBUG
  av1_update_txb_coeff_cost(&this_rd_stats, plane, tx_size, blk_row, blk_col,
                            this_rd_stats.rate);
#endif  // CONFIG_RD_DEBUG
  av1_set_txb_context(x, plane, block, tx_size, a, l);
#endif  // !CONFIG_PVQ

  rd1 = RDCOST(x->rdmult, x->rddiv, this_rd_stats.rate, this_rd_stats.dist);
  rd2 = RDCOST(x->rdmult, x->rddiv, 0, this_rd_stats.sse);

  // TODO(jingning): temporarily enabled only for luma component
  rd = AOMMIN(rd1, rd2);

#if CONFIG_DAALA_DIST
  if (plane == 0 && plane_bsize >= BLOCK_8X8 &&
      (tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4)) {
    this_rd_stats.dist = 0;
    this_rd_stats.sse = 0;
    rd = 0;
    x->rate_4x4[block] = this_rd_stats.rate;
  }
#endif  // CONFIG_DAALA_DIST

#if !CONFIG_PVQ
  this_rd_stats.skip &= !x->plane[plane].eobs[block];
#else
  this_rd_stats.skip &= x->pvq_skip[plane];
#endif  // !CONFIG_PVQ
  av1_merge_rd_stats(&args->rd_stats, &this_rd_stats);

  args->this_rd += rd;

  if (args->this_rd > args->best_rd) {
    args->exit_early = 1;
    return;
  }
}

#if CONFIG_DAALA_DIST
static void block_8x8_rd_txfm_daala_dist(int plane, int block, int blk_row,
                                         int blk_col, BLOCK_SIZE plane_bsize,
                                         TX_SIZE tx_size, void *arg) {
  struct rdcost_block_args *args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int64_t rd, rd1, rd2;
  RD_STATS this_rd_stats;
  int qm = OD_HVS_QM;
  int use_activity_masking = 0;

  (void)tx_size;

  assert(plane == 0);
  assert(plane_bsize >= BLOCK_8X8);
#if CONFIG_PVQ
  use_activity_masking = x->daala_enc.use_activity_masking;
#endif  // CONFIG_PVQ
  av1_init_rd_stats(&this_rd_stats);

  if (args->exit_early) return;

  {
    const struct macroblock_plane *const p = &x->plane[plane];
    struct macroblockd_plane *const pd = &xd->plane[plane];

    const int src_stride = p->src.stride;
    const int dst_stride = pd->dst.stride;
    const int diff_stride = block_size_wide[plane_bsize];

    const uint8_t *src =
        &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
    const uint8_t *dst =
        &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];

    unsigned int tmp1, tmp2;
    int qindex = x->qindex;
    const int pred_stride = block_size_wide[plane_bsize];
    const int pred_idx = (blk_row * pred_stride + blk_col)
                         << tx_size_wide_log2[0];
    int16_t *pred = &pd->pred[pred_idx];
    int i, j;
    const int tx_blk_size = 8;

    DECLARE_ALIGNED(16, uint8_t, pred8[8 * 8]);

    for (j = 0; j < tx_blk_size; j++)
      for (i = 0; i < tx_blk_size; i++)
        pred8[j * tx_blk_size + i] = pred[j * diff_stride + i];

    tmp1 = av1_daala_dist(src, src_stride, pred8, tx_blk_size, 8, 8, qm,
                          use_activity_masking, qindex);
    tmp2 = av1_daala_dist(src, src_stride, dst, dst_stride, 8, 8, qm,
                          use_activity_masking, qindex);

    if (!is_inter_block(mbmi)) {
      this_rd_stats.sse = (int64_t)tmp1 * 16;
      this_rd_stats.dist = (int64_t)tmp2 * 16;
    } else {
      // For inter mode, the decoded pixels are provided in pd->pred,
      // while the predicted pixels are in dst.
      this_rd_stats.sse = (int64_t)tmp2 * 16;
      this_rd_stats.dist = (int64_t)tmp1 * 16;
    }
  }

  rd = RDCOST(x->rdmult, x->rddiv, 0, this_rd_stats.dist);
  if (args->this_rd + rd > args->best_rd) {
    args->exit_early = 1;
    return;
  }

  {
    const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
    const uint8_t txw_unit = tx_size_wide_unit[tx_size];
    const uint8_t txh_unit = tx_size_high_unit[tx_size];
    const int step = txw_unit * txh_unit;
    int offset_h = tx_size_high_unit[TX_4X4];
    // The rate of the current 8x8 block is the sum of four 4x4 blocks in it.
    this_rd_stats.rate =
        x->rate_4x4[block - max_blocks_wide * offset_h - step] +
        x->rate_4x4[block - max_blocks_wide * offset_h] +
        x->rate_4x4[block - step] + x->rate_4x4[block];
  }
  rd1 = RDCOST(x->rdmult, x->rddiv, this_rd_stats.rate, this_rd_stats.dist);
  rd2 = RDCOST(x->rdmult, x->rddiv, 0, this_rd_stats.sse);
  rd = AOMMIN(rd1, rd2);

  args->rd_stats.dist += this_rd_stats.dist;
  args->rd_stats.sse += this_rd_stats.sse;

  args->this_rd += rd;

  if (args->this_rd > args->best_rd) {
    args->exit_early = 1;
    return;
  }
}
#endif  // CONFIG_DAALA_DIST

static void txfm_rd_in_plane(MACROBLOCK *x, const AV1_COMP *cpi,
                             RD_STATS *rd_stats, int64_t ref_best_rd, int plane,
                             BLOCK_SIZE bsize, TX_SIZE tx_size,
                             int use_fast_coef_casting) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  struct rdcost_block_args args;
  av1_zero(args);
  args.x = x;
  args.cpi = cpi;
  args.best_rd = ref_best_rd;
  args.use_fast_coef_costing = use_fast_coef_casting;
  av1_init_rd_stats(&args.rd_stats);

  if (plane == 0) xd->mi[0]->mbmi.tx_size = tx_size;

  av1_get_entropy_contexts(bsize, tx_size, pd, args.t_above, args.t_left);

#if CONFIG_DAALA_DIST
  if (plane == 0 && bsize >= BLOCK_8X8 &&
      (tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4))
    av1_foreach_8x8_transformed_block_in_yplane(
        xd, bsize, block_rd_txfm, block_8x8_rd_txfm_daala_dist, &args);
  else
#endif  // CONFIG_DAALA_DIST
    av1_foreach_transformed_block_in_plane(xd, bsize, plane, block_rd_txfm,
                                           &args);

  if (args.exit_early) {
    av1_invalid_rd_stats(rd_stats);
  } else {
    *rd_stats = args.rd_stats;
  }
}

#if CONFIG_SUPERTX
void av1_txfm_rd_in_plane_supertx(MACROBLOCK *x, const AV1_COMP *cpi, int *rate,
                                  int64_t *distortion, int *skippable,
                                  int64_t *sse, int64_t ref_best_rd, int plane,
                                  BLOCK_SIZE bsize, TX_SIZE tx_size,
                                  int use_fast_coef_casting) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  struct rdcost_block_args args;
  av1_zero(args);
  args.cpi = cpi;
  args.x = x;
  args.best_rd = ref_best_rd;
  args.use_fast_coef_costing = use_fast_coef_casting;

#if CONFIG_EXT_TX
  assert(tx_size < TX_SIZES);
#endif  // CONFIG_EXT_TX

  if (plane == 0) xd->mi[0]->mbmi.tx_size = tx_size;

  av1_get_entropy_contexts(bsize, tx_size, pd, args.t_above, args.t_left);

  block_rd_txfm(plane, 0, 0, 0, get_plane_block_size(bsize, pd), tx_size,
                &args);

  if (args.exit_early) {
    *rate = INT_MAX;
    *distortion = INT64_MAX;
    *sse = INT64_MAX;
    *skippable = 0;
  } else {
    *distortion = args.rd_stats.dist;
    *rate = args.rd_stats.rate;
    *sse = args.rd_stats.sse;
    *skippable = !x->plane[plane].eobs[0];
  }
}
#endif  // CONFIG_SUPERTX

static int tx_size_cost(const AV1_COMP *const cpi, const MACROBLOCK *const x,
                        BLOCK_SIZE bsize, TX_SIZE tx_size) {
  const AV1_COMMON *const cm = &cpi->common;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  const int tx_select =
      cm->tx_mode == TX_MODE_SELECT && mbmi->sb_type >= BLOCK_8X8;

  if (tx_select) {
    const int is_inter = is_inter_block(mbmi);
    const int tx_size_cat = is_inter ? inter_tx_size_cat_lookup[bsize]
                                     : intra_tx_size_cat_lookup[bsize];
    const TX_SIZE coded_tx_size = txsize_sqr_up_map[tx_size];
    const int depth = tx_size_to_depth(coded_tx_size);
    const int tx_size_ctx = get_tx_size_context(xd);
    int r_tx_size = cpi->tx_size_cost[tx_size_cat][tx_size_ctx][depth];
#if CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT
    if (is_quarter_tx_allowed(xd, mbmi, is_inter) && tx_size != coded_tx_size)
      r_tx_size += av1_cost_bit(cm->fc->quarter_tx_size_prob,
                                tx_size == quarter_txsize_lookup[bsize]);
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX && CONFIG_RECT_TX_EXT
    return r_tx_size;
  } else {
    return 0;
  }
}

// #TODO(angiebird): use this function whenever it's possible
int av1_tx_type_cost(const AV1_COMP *cpi, const MACROBLOCKD *xd,
                     BLOCK_SIZE bsize, int plane, TX_SIZE tx_size,
                     TX_TYPE tx_type) {
  if (plane > 0) return 0;

  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int is_inter = is_inter_block(mbmi);
#if CONFIG_EXT_TX
  const AV1_COMMON *cm = &cpi->common;
  if (get_ext_tx_types(tx_size, bsize, is_inter, cm->reduced_tx_set_used) > 1 &&
      !xd->lossless[xd->mi[0]->mbmi.segment_id]) {
    const int ext_tx_set =
        get_ext_tx_set(tx_size, bsize, is_inter, cm->reduced_tx_set_used);
    if (is_inter) {
      if (ext_tx_set > 0)
        return cpi
            ->inter_tx_type_costs[ext_tx_set][txsize_sqr_map[tx_size]][tx_type];
    } else {
      if (ext_tx_set > 0 && ALLOW_INTRA_EXT_TX)
        return cpi->intra_tx_type_costs[ext_tx_set][txsize_sqr_map[tx_size]]
                                       [mbmi->mode][tx_type];
    }
  }
#else
  (void)bsize;
  if (tx_size < TX_32X32 && !xd->lossless[xd->mi[0]->mbmi.segment_id] &&
      !FIXED_TX_TYPE) {
    if (is_inter) {
      return cpi->inter_tx_type_costs[tx_size][tx_type];
    } else {
      return cpi->intra_tx_type_costs[tx_size]
                                     [intra_mode_to_tx_type_context[mbmi->mode]]
                                     [tx_type];
    }
  }
#endif  // CONFIG_EXT_TX
  return 0;
}
static int64_t txfm_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                        RD_STATS *rd_stats, int64_t ref_best_rd, BLOCK_SIZE bs,
                        TX_TYPE tx_type, int tx_size) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int64_t rd = INT64_MAX;
  aom_prob skip_prob = av1_get_skip_prob(cm, xd);
  int s0, s1;
  const int is_inter = is_inter_block(mbmi);
  const int tx_select =
      cm->tx_mode == TX_MODE_SELECT && mbmi->sb_type >= BLOCK_8X8;

  const int r_tx_size = tx_size_cost(cpi, x, bs, tx_size);

  assert(skip_prob > 0);
#if CONFIG_EXT_TX && CONFIG_RECT_TX
  assert(IMPLIES(is_rect_tx(tx_size), is_rect_tx_allowed_bsize(bs)));
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX

  s0 = av1_cost_bit(skip_prob, 0);
  s1 = av1_cost_bit(skip_prob, 1);

  mbmi->tx_type = tx_type;
  mbmi->tx_size = tx_size;
  txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, bs, tx_size,
                   cpi->sf.use_fast_coef_costing);
  if (rd_stats->rate == INT_MAX) return INT64_MAX;
#if !CONFIG_TXK_SEL
  int plane = 0;
  rd_stats->rate += av1_tx_type_cost(cpi, xd, bs, plane, tx_size, tx_type);
#endif

  if (rd_stats->skip) {
    if (is_inter) {
      rd = RDCOST(x->rdmult, x->rddiv, s1, rd_stats->sse);
    } else {
      rd = RDCOST(x->rdmult, x->rddiv, s1 + r_tx_size * tx_select,
                  rd_stats->sse);
    }
  } else {
    rd = RDCOST(x->rdmult, x->rddiv,
                rd_stats->rate + s0 + r_tx_size * tx_select, rd_stats->dist);
  }

  if (tx_select) rd_stats->rate += r_tx_size;

  if (is_inter && !xd->lossless[xd->mi[0]->mbmi.segment_id] &&
      !(rd_stats->skip))
    rd = AOMMIN(rd, RDCOST(x->rdmult, x->rddiv, s1, rd_stats->sse));

  return rd;
}

static int skip_txfm_search(const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs,
                            TX_TYPE tx_type, TX_SIZE tx_size) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  const int is_inter = is_inter_block(mbmi);
  int prune = 0;
  if (is_inter && cpi->sf.tx_type_search.prune_mode > NO_PRUNE)
    // passing -1 in for tx_type indicates that all 1D
    // transforms should be considered for pruning
    prune = prune_tx_types(cpi, bs, x, xd, -1);

  if (mbmi->ref_mv_idx > 0 && tx_type != DCT_DCT) return 1;
  if (FIXED_TX_TYPE && tx_type != get_default_tx_type(0, xd, 0, tx_size))
    return 1;
  if (!is_inter && x->use_default_intra_tx_type &&
      tx_type != get_default_tx_type(0, xd, 0, tx_size))
    return 1;
  if (is_inter && x->use_default_inter_tx_type &&
      tx_type != get_default_tx_type(0, xd, 0, tx_size))
    return 1;
  if (max_tx_size >= TX_32X32 && tx_size == TX_4X4) return 1;
#if CONFIG_EXT_TX
  const AV1_COMMON *const cm = &cpi->common;
  int ext_tx_set =
      get_ext_tx_set(tx_size, bs, is_inter, cm->reduced_tx_set_used);
  if (is_inter) {
    if (!ext_tx_used_inter[ext_tx_set][tx_type]) return 1;
    if (cpi->sf.tx_type_search.prune_mode > NO_PRUNE) {
      if (!do_tx_type_search(tx_type, prune)) return 1;
    }
  } else {
    if (!ALLOW_INTRA_EXT_TX && bs >= BLOCK_8X8) {
      if (tx_type != intra_mode_to_tx_type_context[mbmi->mode]) return 1;
    }
    if (!ext_tx_used_intra[ext_tx_set][tx_type]) return 1;
  }
#else   // CONFIG_EXT_TX
  if (tx_size >= TX_32X32 && tx_type != DCT_DCT) return 1;
  if (is_inter && cpi->sf.tx_type_search.prune_mode > NO_PRUNE &&
      !do_tx_type_search(tx_type, prune))
    return 1;
#endif  // CONFIG_EXT_TX
  return 0;
}

#if CONFIG_EXT_INTER && (CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT)
static int64_t estimate_yrd_for_sb(const AV1_COMP *const cpi, BLOCK_SIZE bs,
                                   MACROBLOCK *x, int *r, int64_t *d, int *s,
                                   int64_t *sse, int64_t ref_best_rd) {
  RD_STATS rd_stats;
  int64_t rd = txfm_yrd(cpi, x, &rd_stats, ref_best_rd, bs, DCT_DCT,
                        max_txsize_lookup[bs]);
  *r = rd_stats.rate;
  *d = rd_stats.dist;
  *s = rd_stats.skip;
  *sse = rd_stats.sse;
  return rd;
}
#endif  // CONFIG_EXT_INTER && (CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT)

static void choose_largest_tx_size(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   RD_STATS *rd_stats, int64_t ref_best_rd,
                                   BLOCK_SIZE bs) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  TX_TYPE tx_type, best_tx_type = DCT_DCT;
  int64_t this_rd, best_rd = INT64_MAX;
  aom_prob skip_prob = av1_get_skip_prob(cm, xd);
  int s0 = av1_cost_bit(skip_prob, 0);
  int s1 = av1_cost_bit(skip_prob, 1);
  const int is_inter = is_inter_block(mbmi);
  int prune = 0;
  const int plane = 0;
#if CONFIG_EXT_TX
  int ext_tx_set;
#endif  // CONFIG_EXT_TX
  av1_invalid_rd_stats(rd_stats);

  mbmi->tx_size = tx_size_from_tx_mode(bs, cm->tx_mode, is_inter);
#if CONFIG_VAR_TX
  mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
#endif  // CONFIG_VAR_TX
#if CONFIG_EXT_TX
  ext_tx_set =
      get_ext_tx_set(mbmi->tx_size, bs, is_inter, cm->reduced_tx_set_used);
#endif  // CONFIG_EXT_TX

  if (is_inter && cpi->sf.tx_type_search.prune_mode > NO_PRUNE)
#if CONFIG_EXT_TX
    prune = prune_tx_types(cpi, bs, x, xd, ext_tx_set);
#else
    prune = prune_tx_types(cpi, bs, x, xd, 0);
#endif  // CONFIG_EXT_TX
#if CONFIG_EXT_TX
  if (get_ext_tx_types(mbmi->tx_size, bs, is_inter, cm->reduced_tx_set_used) >
          1 &&
      !xd->lossless[mbmi->segment_id]) {
#if CONFIG_PVQ
    od_rollback_buffer pre_buf, post_buf;

    od_encode_checkpoint(&x->daala_enc, &pre_buf);
    od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ

    for (tx_type = DCT_DCT; tx_type < TX_TYPES; ++tx_type) {
      RD_STATS this_rd_stats;
      if (is_inter) {
        if (x->use_default_inter_tx_type &&
            tx_type != get_default_tx_type(0, xd, 0, mbmi->tx_size))
          continue;
        if (!ext_tx_used_inter[ext_tx_set][tx_type]) continue;
        if (cpi->sf.tx_type_search.prune_mode > NO_PRUNE) {
          if (!do_tx_type_search(tx_type, prune)) continue;
        }
      } else {
        if (x->use_default_intra_tx_type &&
            tx_type != get_default_tx_type(0, xd, 0, mbmi->tx_size))
          continue;
        if (!ALLOW_INTRA_EXT_TX && bs >= BLOCK_8X8) {
          if (tx_type != intra_mode_to_tx_type_context[mbmi->mode]) continue;
        }
        if (!ext_tx_used_intra[ext_tx_set][tx_type]) continue;
      }

      mbmi->tx_type = tx_type;

      txfm_rd_in_plane(x, cpi, &this_rd_stats, ref_best_rd, 0, bs,
                       mbmi->tx_size, cpi->sf.use_fast_coef_costing);
#if CONFIG_PVQ
      od_encode_rollback(&x->daala_enc, &pre_buf);
#endif  // CONFIG_PVQ
      if (this_rd_stats.rate == INT_MAX) continue;
      av1_tx_type_cost(cpi, xd, bs, plane, mbmi->tx_size, tx_type);

      if (this_rd_stats.skip)
        this_rd = RDCOST(x->rdmult, x->rddiv, s1, this_rd_stats.sse);
      else
        this_rd = RDCOST(x->rdmult, x->rddiv, this_rd_stats.rate + s0,
                         this_rd_stats.dist);
      if (is_inter_block(mbmi) && !xd->lossless[mbmi->segment_id] &&
          !this_rd_stats.skip)
        this_rd =
            AOMMIN(this_rd, RDCOST(x->rdmult, x->rddiv, s1, this_rd_stats.sse));

      if (this_rd < best_rd) {
        best_rd = this_rd;
        best_tx_type = mbmi->tx_type;
        *rd_stats = this_rd_stats;
#if CONFIG_PVQ
        od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ
      }
    }
#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ
  } else {
    mbmi->tx_type = DCT_DCT;
    txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, bs, mbmi->tx_size,
                     cpi->sf.use_fast_coef_costing);
  }
#else   // CONFIG_EXT_TX
  if (mbmi->tx_size < TX_32X32 && !xd->lossless[mbmi->segment_id]) {
    for (tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
      RD_STATS this_rd_stats;
      if (!is_inter && x->use_default_intra_tx_type &&
          tx_type != get_default_tx_type(0, xd, 0, mbmi->tx_size))
        continue;
      if (is_inter && x->use_default_inter_tx_type &&
          tx_type != get_default_tx_type(0, xd, 0, mbmi->tx_size))
        continue;
      mbmi->tx_type = tx_type;
      txfm_rd_in_plane(x, cpi, &this_rd_stats, ref_best_rd, 0, bs,
                       mbmi->tx_size, cpi->sf.use_fast_coef_costing);
      if (this_rd_stats.rate == INT_MAX) continue;

      av1_tx_type_cost(cpi, xd, bs, plane, mbmi->tx_size, tx_type);
      if (is_inter) {
        if (cpi->sf.tx_type_search.prune_mode > NO_PRUNE &&
            !do_tx_type_search(tx_type, prune))
          continue;
      }
      if (this_rd_stats.skip)
        this_rd = RDCOST(x->rdmult, x->rddiv, s1, this_rd_stats.sse);
      else
        this_rd = RDCOST(x->rdmult, x->rddiv, this_rd_stats.rate + s0,
                         this_rd_stats.dist);
      if (is_inter && !xd->lossless[mbmi->segment_id] && !this_rd_stats.skip)
        this_rd =
            AOMMIN(this_rd, RDCOST(x->rdmult, x->rddiv, s1, this_rd_stats.sse));

      if (this_rd < best_rd) {
        best_rd = this_rd;
        best_tx_type = mbmi->tx_type;
        *rd_stats = this_rd_stats;
      }
    }
  } else {
    mbmi->tx_type = DCT_DCT;
    txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, bs, mbmi->tx_size,
                     cpi->sf.use_fast_coef_costing);
  }
#endif  // CONFIG_EXT_TX
  mbmi->tx_type = best_tx_type;
}

static void choose_smallest_tx_size(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    RD_STATS *rd_stats, int64_t ref_best_rd,
                                    BLOCK_SIZE bs) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  mbmi->tx_size = TX_4X4;
  mbmi->tx_type = DCT_DCT;
#if CONFIG_VAR_TX
  mbmi->min_tx_size = get_min_tx_size(TX_4X4);
#endif  // CONFIG_VAR_TX

  txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, bs, mbmi->tx_size,
                   cpi->sf.use_fast_coef_costing);
}

#if CONFIG_TXK_SEL || CONFIG_VAR_TX
static INLINE int bsize_to_num_blk(BLOCK_SIZE bsize) {
  int num_blk = 1 << (num_pels_log2_lookup[bsize] - 2 * tx_size_wide_log2[0]);
  return num_blk;
}
#endif  // CONFIG_TXK_SEL || CONFIG_VAR_TX

static void choose_tx_size_type_from_rd(const AV1_COMP *const cpi,
                                        MACROBLOCK *x, RD_STATS *rd_stats,
                                        int64_t ref_best_rd, BLOCK_SIZE bs) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int64_t rd = INT64_MAX;
  int n;
  int start_tx, end_tx;
  int64_t best_rd = INT64_MAX, last_rd = INT64_MAX;
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  TX_SIZE best_tx_size = max_tx_size;
  TX_TYPE best_tx_type = DCT_DCT;
#if CONFIG_TXK_SEL
  TX_TYPE best_txk_type[MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
  const int num_blk = bsize_to_num_blk(bs);
#endif  // CONFIG_TXK_SEL
  const int tx_select = cm->tx_mode == TX_MODE_SELECT;
  const int is_inter = is_inter_block(mbmi);
#if CONFIG_PVQ
  od_rollback_buffer buf;
  od_encode_checkpoint(&x->daala_enc, &buf);
#endif  // CONFIG_PVQ

  av1_invalid_rd_stats(rd_stats);

#if CONFIG_EXT_TX && CONFIG_RECT_TX
  int evaluate_rect_tx = 0;
  if (tx_select) {
    evaluate_rect_tx = is_rect_tx_allowed(xd, mbmi);
  } else {
    const TX_SIZE chosen_tx_size =
        tx_size_from_tx_mode(bs, cm->tx_mode, is_inter);
    evaluate_rect_tx = is_rect_tx(chosen_tx_size);
    assert(IMPLIES(evaluate_rect_tx, is_rect_tx_allowed(xd, mbmi)));
  }
  if (evaluate_rect_tx) {
    TX_TYPE tx_start = DCT_DCT;
    TX_TYPE tx_end = TX_TYPES;
#if CONFIG_TXK_SEL
    // The tx_type becomes dummy when lv_map is on. The tx_type search will be
    // performed in av1_search_txk_type()
    tx_end = DCT_DCT + 1;
#endif
    TX_TYPE tx_type;
    for (tx_type = tx_start; tx_type < tx_end; ++tx_type) {
      if (mbmi->ref_mv_idx > 0 && tx_type != DCT_DCT) continue;
      const TX_SIZE rect_tx_size = max_txsize_rect_lookup[bs];
      RD_STATS this_rd_stats;
      int ext_tx_set =
          get_ext_tx_set(rect_tx_size, bs, is_inter, cm->reduced_tx_set_used);
      if ((is_inter && ext_tx_used_inter[ext_tx_set][tx_type]) ||
          (!is_inter && ext_tx_used_intra[ext_tx_set][tx_type])) {
        rd = txfm_yrd(cpi, x, &this_rd_stats, ref_best_rd, bs, tx_type,
                      rect_tx_size);
        if (rd < best_rd) {
#if CONFIG_TXK_SEL
          memcpy(best_txk_type, mbmi->txk_type,
                 sizeof(best_txk_type[0]) * num_blk);
#endif
          best_tx_type = tx_type;
          best_tx_size = rect_tx_size;
          best_rd = rd;
          *rd_stats = this_rd_stats;
        }
      }
#if CONFIG_CB4X4 && !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
      const int is_inter = is_inter_block(mbmi);
      if (mbmi->sb_type < BLOCK_8X8 && is_inter) break;
#endif  // CONFIG_CB4X4 && !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
    }
  }

#if CONFIG_RECT_TX_EXT
  // test 1:4/4:1 tx
  int evaluate_quarter_tx = 0;
  if (is_quarter_tx_allowed(xd, mbmi, is_inter)) {
    if (tx_select) {
      evaluate_quarter_tx = 1;
    } else {
      const TX_SIZE chosen_tx_size =
          tx_size_from_tx_mode(bs, cm->tx_mode, is_inter);
      evaluate_quarter_tx = chosen_tx_size == quarter_txsize_lookup[bs];
    }
  }
  if (evaluate_quarter_tx) {
    TX_TYPE tx_start = DCT_DCT;
    TX_TYPE tx_end = TX_TYPES;
#if CONFIG_TXK_SEL
    // The tx_type becomes dummy when lv_map is on. The tx_type search will be
    // performed in av1_search_txk_type()
    tx_end = DCT_DCT + 1;
#endif
    TX_TYPE tx_type;
    for (tx_type = tx_start; tx_type < tx_end; ++tx_type) {
      if (mbmi->ref_mv_idx > 0 && tx_type != DCT_DCT) continue;
      const TX_SIZE tx_size = quarter_txsize_lookup[bs];
      RD_STATS this_rd_stats;
      int ext_tx_set =
          get_ext_tx_set(tx_size, bs, is_inter, cm->reduced_tx_set_used);
      if ((is_inter && ext_tx_used_inter[ext_tx_set][tx_type]) ||
          (!is_inter && ext_tx_used_intra[ext_tx_set][tx_type])) {
        rd =
            txfm_yrd(cpi, x, &this_rd_stats, ref_best_rd, bs, tx_type, tx_size);
        if (rd < best_rd) {
#if CONFIG_TXK_SEL
          memcpy(best_txk_type, mbmi->txk_type,
                 sizeof(best_txk_type[0]) * num_blk);
#endif
          best_tx_type = tx_type;
          best_tx_size = tx_size;
          best_rd = rd;
          *rd_stats = this_rd_stats;
        }
      }
#if CONFIG_CB4X4 && !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
      const int is_inter = is_inter_block(mbmi);
      if (mbmi->sb_type < BLOCK_8X8 && is_inter) break;
#endif  // CONFIG_CB4X4 && !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
    }
  }
#endif  // CONFIG_RECT_TX_EXT
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX

  if (tx_select) {
    start_tx = max_tx_size;
    end_tx = (max_tx_size >= TX_32X32) ? TX_8X8 : TX_4X4;
  } else {
    const TX_SIZE chosen_tx_size =
        tx_size_from_tx_mode(bs, cm->tx_mode, is_inter);
    start_tx = chosen_tx_size;
    end_tx = chosen_tx_size;
  }

  last_rd = INT64_MAX;
  for (n = start_tx; n >= end_tx; --n) {
#if CONFIG_EXT_TX && CONFIG_RECT_TX
    if (is_rect_tx(n)) break;
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX
    TX_TYPE tx_start = DCT_DCT;
    TX_TYPE tx_end = TX_TYPES;
#if CONFIG_TXK_SEL
    // The tx_type becomes dummy when lv_map is on. The tx_type search will be
    // performed in av1_search_txk_type()
    tx_end = DCT_DCT + 1;
#endif
    TX_TYPE tx_type;
    for (tx_type = tx_start; tx_type < tx_end; ++tx_type) {
      RD_STATS this_rd_stats;
      if (skip_txfm_search(cpi, x, bs, tx_type, n)) continue;
      rd = txfm_yrd(cpi, x, &this_rd_stats, ref_best_rd, bs, tx_type, n);
#if CONFIG_PVQ
      od_encode_rollback(&x->daala_enc, &buf);
#endif  // CONFIG_PVQ
      // Early termination in transform size search.
      if (cpi->sf.tx_size_search_breakout &&
          (rd == INT64_MAX ||
           (this_rd_stats.skip == 1 && tx_type != DCT_DCT && n < start_tx) ||
           (n < (int)max_tx_size && rd > last_rd)))
        break;

      last_rd = rd;
      if (rd < best_rd) {
#if CONFIG_TXK_SEL
        memcpy(best_txk_type, mbmi->txk_type,
               sizeof(best_txk_type[0]) * num_blk);
#endif
        best_tx_type = tx_type;
        best_tx_size = n;
        best_rd = rd;
        *rd_stats = this_rd_stats;
      }
#if CONFIG_CB4X4 && !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
      const int is_inter = is_inter_block(mbmi);
      if (mbmi->sb_type < BLOCK_8X8 && is_inter) break;
#endif  // CONFIG_CB4X4 && !USE_TXTYPE_SEARCH_FOR_SUB8X8_IN_CB4X4
    }
  }
  mbmi->tx_size = best_tx_size;
  mbmi->tx_type = best_tx_type;
#if CONFIG_TXK_SEL
  memcpy(mbmi->txk_type, best_txk_type, sizeof(best_txk_type[0]) * num_blk);
#endif

#if CONFIG_VAR_TX
  mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
#endif  // CONFIG_VAR_TX

#if !CONFIG_EXT_TX
  if (mbmi->tx_size >= TX_32X32) assert(mbmi->tx_type == DCT_DCT);
#endif  // !CONFIG_EXT_TX
#if CONFIG_PVQ
  if (best_rd != INT64_MAX) {
    txfm_yrd(cpi, x, rd_stats, ref_best_rd, bs, best_tx_type, best_tx_size);
  }
#endif  // CONFIG_PVQ
}

static void super_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bs,
                            int64_t ref_best_rd) {
  MACROBLOCKD *xd = &x->e_mbd;
  av1_init_rd_stats(rd_stats);

  assert(bs == xd->mi[0]->mbmi.sb_type);

  if (xd->lossless[xd->mi[0]->mbmi.segment_id]) {
    choose_smallest_tx_size(cpi, x, rd_stats, ref_best_rd, bs);
  } else if (cpi->sf.tx_size_search_method == USE_LARGESTALL) {
    choose_largest_tx_size(cpi, x, rd_stats, ref_best_rd, bs);
  } else {
    choose_tx_size_type_from_rd(cpi, x, rd_stats, ref_best_rd, bs);
  }
}

static int conditional_skipintra(PREDICTION_MODE mode,
                                 PREDICTION_MODE best_intra_mode) {
  if (mode == D117_PRED && best_intra_mode != V_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  if (mode == D63_PRED && best_intra_mode != V_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D207_PRED && best_intra_mode != H_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D153_PRED && best_intra_mode != H_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  return 0;
}

// Model based RD estimation for luma intra blocks.
static int64_t intra_model_yrd(const AV1_COMP *const cpi, MACROBLOCK *const x,
                               BLOCK_SIZE bsize, int mode_cost) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  RD_STATS this_rd_stats;
  int row, col;
  int64_t temp_sse, this_rd;
  const TX_SIZE tx_size = tx_size_from_tx_mode(bsize, cpi->common.tx_mode, 0);
  const int stepr = tx_size_high_unit[tx_size];
  const int stepc = tx_size_wide_unit[tx_size];
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  mbmi->tx_size = tx_size;
  // Prediction.
  const int step = stepr * stepc;
  int block = 0;
  for (row = 0; row < max_blocks_high; row += stepr) {
    for (col = 0; col < max_blocks_wide; col += stepc) {
#if CONFIG_CFL
      const struct macroblockd_plane *const pd = &xd->plane[0];
      const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);

#if CONFIG_EC_ADAPT
      FRAME_CONTEXT *const ec_ctx = xd->tile_ctx;
#else
      FRAME_CONTEXT *const ec_ctx = cpi->common.fc;
#endif  // CONFIG_EC_ADAPT

      av1_predict_intra_block_encoder_facade(x, ec_ctx, 0, block, col, row,
                                             tx_size, plane_bsize);
#else
      av1_predict_intra_block_facade(xd, 0, block, col, row, tx_size);
#endif
      block += step;
    }
  }
  // RD estimation.
  model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &this_rd_stats.rate,
                  &this_rd_stats.dist, &this_rd_stats.skip, &temp_sse);
#if CONFIG_EXT_INTRA
  if (av1_is_directional_mode(mbmi->mode, bsize)) {
    mode_cost += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                    MAX_ANGLE_DELTA + mbmi->angle_delta[0]);
  }
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  if (mbmi->mode == DC_PRED) {
    const aom_prob prob = cpi->common.fc->filter_intra_probs[0];
    if (mbmi->filter_intra_mode_info.use_filter_intra_mode[0]) {
      const int mode = mbmi->filter_intra_mode_info.filter_intra_mode[0];
      mode_cost += (av1_cost_bit(prob, 1) +
                    write_uniform_cost(FILTER_INTRA_MODES, mode));
    } else {
      mode_cost += av1_cost_bit(prob, 0);
    }
  }
#endif  // CONFIG_FILTER_INTRA
  this_rd = RDCOST(x->rdmult, x->rddiv, this_rd_stats.rate + mode_cost,
                   this_rd_stats.dist);
  return this_rd;
}

#if CONFIG_PALETTE
// Extends 'color_map' array from 'orig_width x orig_height' to 'new_width x
// new_height'. Extra rows and columns are filled in by copying last valid
// row/column.
static void extend_palette_color_map(uint8_t *const color_map, int orig_width,
                                     int orig_height, int new_width,
                                     int new_height) {
  int j;
  assert(new_width >= orig_width);
  assert(new_height >= orig_height);
  if (new_width == orig_width && new_height == orig_height) return;

  for (j = orig_height - 1; j >= 0; --j) {
    memmove(color_map + j * new_width, color_map + j * orig_width, orig_width);
    // Copy last column to extra columns.
    memset(color_map + j * new_width + orig_width,
           color_map[j * new_width + orig_width - 1], new_width - orig_width);
  }
  // Copy last row to extra rows.
  for (j = orig_height; j < new_height; ++j) {
    memcpy(color_map + j * new_width, color_map + (orig_height - 1) * new_width,
           new_width);
  }
}

#if CONFIG_PALETTE_DELTA_ENCODING
// Bias toward using colors in the cache.
// TODO(huisu): Try other schemes to improve compression.
static void optimize_palette_colors(uint16_t *color_cache, int n_cache,
                                    int n_colors, int stride,
                                    float *centroids) {
  if (n_cache <= 0) return;
  for (int i = 0; i < n_colors * stride; i += stride) {
    float min_diff = fabsf(centroids[i] - color_cache[0]);
    int idx = 0;
    for (int j = 1; j < n_cache; ++j) {
      float this_diff = fabsf(centroids[i] - color_cache[j]);
      if (this_diff < min_diff) {
        min_diff = this_diff;
        idx = j;
      }
    }
    if (min_diff < 1.5) centroids[i] = color_cache[idx];
  }
}
#endif  // CONFIG_PALETTE_DELTA_ENCODING

static int rd_pick_palette_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                     BLOCK_SIZE bsize, int palette_ctx,
                                     int dc_mode_cost, MB_MODE_INFO *best_mbmi,
                                     uint8_t *best_palette_color_map,
                                     int64_t *best_rd, int64_t *best_model_rd,
                                     int *rate, int *rate_tokenonly,
                                     int64_t *distortion, int *skippable) {
  int rate_overhead = 0;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mic->mbmi;
  assert(!is_inter_block(mbmi));
  int this_rate, colors, n;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *const src = x->plane[0].src.buf;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  int block_width, block_height, rows, cols;
  av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                           &cols);

  assert(cpi->common.allow_screen_content_tools);

#if CONFIG_HIGHBITDEPTH
  if (cpi->common.use_highbitdepth)
    colors = av1_count_colors_highbd(src, src_stride, rows, cols,
                                     cpi->common.bit_depth);
  else
#endif  // CONFIG_HIGHBITDEPTH
    colors = av1_count_colors(src, src_stride, rows, cols);
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
#endif  // CONFIG_FILTER_INTRA

  if (colors > 1 && colors <= 64) {
    int r, c, i, j, k, palette_mode_cost;
    const int max_itr = 50;
    uint8_t color_order[PALETTE_MAX_SIZE];
    float *const data = x->palette_buffer->kmeans_data_buf;
    float centroids[PALETTE_MAX_SIZE];
    float lb, ub, val;
    RD_STATS tokenonly_rd_stats;
    int64_t this_rd, this_model_rd;
    PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
#if CONFIG_HIGHBITDEPTH
    uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
    if (cpi->common.use_highbitdepth)
      lb = ub = src16[0];
    else
#endif  // CONFIG_HIGHBITDEPTH
      lb = ub = src[0];

#if CONFIG_HIGHBITDEPTH
    if (cpi->common.use_highbitdepth) {
      for (r = 0; r < rows; ++r) {
        for (c = 0; c < cols; ++c) {
          val = src16[r * src_stride + c];
          data[r * cols + c] = val;
          if (val < lb)
            lb = val;
          else if (val > ub)
            ub = val;
        }
      }
    } else {
#endif  // CONFIG_HIGHBITDEPTH
      for (r = 0; r < rows; ++r) {
        for (c = 0; c < cols; ++c) {
          val = src[r * src_stride + c];
          data[r * cols + c] = val;
          if (val < lb)
            lb = val;
          else if (val > ub)
            ub = val;
        }
      }
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH

    mbmi->mode = DC_PRED;
#if CONFIG_FILTER_INTRA
    mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
#endif  // CONFIG_FILTER_INTRA

    if (rows * cols > PALETTE_MAX_BLOCK_SIZE) return 0;

#if CONFIG_PALETTE_DELTA_ENCODING
    const MODE_INFO *above_mi = xd->above_mi;
    const MODE_INFO *left_mi = xd->left_mi;
    uint16_t color_cache[2 * PALETTE_MAX_SIZE];
    const int n_cache =
        av1_get_palette_cache(above_mi, left_mi, 0, color_cache);
#endif  // CONFIG_PALETTE_DELTA_ENCODING

    for (n = colors > PALETTE_MAX_SIZE ? PALETTE_MAX_SIZE : colors; n >= 2;
         --n) {
      if (colors == PALETTE_MIN_SIZE) {
        // Special case: These colors automatically become the centroids.
        assert(colors == n);
        assert(colors == 2);
        centroids[0] = lb;
        centroids[1] = ub;
        k = 2;
      } else {
        for (i = 0; i < n; ++i) {
          centroids[i] = lb + (2 * i + 1) * (ub - lb) / n / 2;
        }
        av1_k_means(data, centroids, color_map, rows * cols, n, 1, max_itr);
#if CONFIG_PALETTE_DELTA_ENCODING
        optimize_palette_colors(color_cache, n_cache, n, 1, centroids);
#endif  // CONFIG_PALETTE_DELTA_ENCODING
        k = av1_remove_duplicates(centroids, n);
        if (k < PALETTE_MIN_SIZE) {
          // Too few unique colors to create a palette. And DC_PRED will work
          // well for that case anyway. So skip.
          continue;
        }
      }

#if CONFIG_HIGHBITDEPTH
      if (cpi->common.use_highbitdepth)
        for (i = 0; i < k; ++i)
          pmi->palette_colors[i] =
              clip_pixel_highbd((int)centroids[i], cpi->common.bit_depth);
      else
#endif  // CONFIG_HIGHBITDEPTH
        for (i = 0; i < k; ++i)
          pmi->palette_colors[i] = clip_pixel((int)centroids[i]);
      pmi->palette_size[0] = k;

      av1_calc_indices(data, centroids, color_map, rows * cols, k, 1);
      extend_palette_color_map(color_map, cols, rows, block_width,
                               block_height);
      palette_mode_cost =
          dc_mode_cost +
          cpi->palette_y_size_cost[bsize - BLOCK_8X8][k - PALETTE_MIN_SIZE] +
          write_uniform_cost(k, color_map[0]) +
          av1_cost_bit(
              av1_default_palette_y_mode_prob[bsize - BLOCK_8X8][palette_ctx],
              1);
      palette_mode_cost += av1_palette_color_cost_y(pmi,
#if CONFIG_PALETTE_DELTA_ENCODING
                                                    color_cache, n_cache,
#endif  // CONFIG_PALETTE_DELTA_ENCODING
                                                    cpi->common.bit_depth);
      for (i = 0; i < rows; ++i) {
        for (j = (i == 0 ? 1 : 0); j < cols; ++j) {
          int color_idx;
          const int color_ctx = av1_get_palette_color_index_context(
              color_map, block_width, i, j, k, color_order, &color_idx);
          assert(color_idx >= 0 && color_idx < k);
          palette_mode_cost += cpi->palette_y_color_cost[k - PALETTE_MIN_SIZE]
                                                        [color_ctx][color_idx];
        }
      }
      this_model_rd = intra_model_yrd(cpi, x, bsize, palette_mode_cost);
      if (*best_model_rd != INT64_MAX &&
          this_model_rd > *best_model_rd + (*best_model_rd >> 1))
        continue;
      if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
      super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
      if (tokenonly_rd_stats.rate == INT_MAX) continue;
      this_rate = tokenonly_rd_stats.rate + palette_mode_cost;
      this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, tokenonly_rd_stats.dist);
      if (!xd->lossless[mbmi->segment_id] && mbmi->sb_type >= BLOCK_8X8) {
        tokenonly_rd_stats.rate -= tx_size_cost(cpi, x, bsize, mbmi->tx_size);
      }
      if (this_rd < *best_rd) {
        *best_rd = this_rd;
        memcpy(best_palette_color_map, color_map,
               block_width * block_height * sizeof(color_map[0]));
        *best_mbmi = *mbmi;
        rate_overhead = this_rate - tokenonly_rd_stats.rate;
        if (rate) *rate = this_rate;
        if (rate_tokenonly) *rate_tokenonly = tokenonly_rd_stats.rate;
        if (distortion) *distortion = tokenonly_rd_stats.dist;
        if (skippable) *skippable = tokenonly_rd_stats.skip;
      }
    }
  }

  if (best_mbmi->palette_mode_info.palette_size[0] > 0) {
    memcpy(color_map, best_palette_color_map,
           rows * cols * sizeof(best_palette_color_map[0]));
  }
  *mbmi = *best_mbmi;
  return rate_overhead;
}
#endif  // CONFIG_PALETTE

static int64_t rd_pick_intra_sub_8x8_y_subblock_mode(
    const AV1_COMP *const cpi, MACROBLOCK *x, int row, int col,
    PREDICTION_MODE *best_mode, const int *bmode_costs, ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l, int *bestrate, int *bestratey, int64_t *bestdistortion,
    BLOCK_SIZE bsize, TX_SIZE tx_size, int *y_skip, int64_t rd_thresh) {
  const AV1_COMMON *const cm = &cpi->common;
  PREDICTION_MODE mode;
  MACROBLOCKD *const xd = &x->e_mbd;
  assert(!is_inter_block(&xd->mi[0]->mbmi));
  int64_t best_rd = rd_thresh;
  struct macroblock_plane *p = &x->plane[0];
  struct macroblockd_plane *pd = &xd->plane[0];
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  const uint8_t *src_init = &p->src.buf[row * 4 * src_stride + col * 4];
  uint8_t *dst_init = &pd->dst.buf[row * 4 * dst_stride + col * 4];
#if CONFIG_CHROMA_2X2
  // TODO(jingning): This is a temporal change. The whole function should be
  // out when cb4x4 is enabled.
  ENTROPY_CONTEXT ta[4], tempa[4];
  ENTROPY_CONTEXT tl[4], templ[4];
#else
  ENTROPY_CONTEXT ta[2], tempa[2];
  ENTROPY_CONTEXT tl[2], templ[2];
#endif  // CONFIG_CHROMA_2X2

  const int pred_width_in_4x4_blocks = num_4x4_blocks_wide_lookup[bsize];
  const int pred_height_in_4x4_blocks = num_4x4_blocks_high_lookup[bsize];
  const int tx_width_unit = tx_size_wide_unit[tx_size];
  const int tx_height_unit = tx_size_high_unit[tx_size];
  const int pred_block_width = block_size_wide[bsize];
  const int pred_block_height = block_size_high[bsize];
  const int tx_width = tx_size_wide[tx_size];
  const int tx_height = tx_size_high[tx_size];
  const int pred_width_in_transform_blocks = pred_block_width / tx_width;
  const int pred_height_in_transform_blocks = pred_block_height / tx_height;
  int idx, idy;
  int best_can_skip = 0;
  uint8_t best_dst[8 * 8];
#if CONFIG_HIGHBITDEPTH
  uint16_t best_dst16[8 * 8];
#endif  // CONFIG_HIGHBITDEPTH
  const int is_lossless = xd->lossless[xd->mi[0]->mbmi.segment_id];
#if CONFIG_EXT_TX && CONFIG_RECT_TX
  const int sub_bsize = bsize;
#else
  const int sub_bsize = BLOCK_4X4;
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX

#if CONFIG_PVQ
  od_rollback_buffer pre_buf, post_buf;
  od_encode_checkpoint(&x->daala_enc, &pre_buf);
  od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ

  assert(bsize < BLOCK_8X8);
  assert(tx_width < 8 || tx_height < 8);
#if CONFIG_EXT_TX && CONFIG_RECT_TX
  if (is_lossless)
    assert(tx_width == 4 && tx_height == 4);
  else
    assert(tx_width == pred_block_width && tx_height == pred_block_height);
#else
  assert(tx_width == 4 && tx_height == 4);
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX

  memcpy(ta, a, pred_width_in_transform_blocks * sizeof(a[0]));
  memcpy(tl, l, pred_height_in_transform_blocks * sizeof(l[0]));

  xd->mi[0]->mbmi.tx_size = tx_size;

#if CONFIG_PALETTE
  xd->mi[0]->mbmi.palette_mode_info.palette_size[0] = 0;
#endif  // CONFIG_PALETTE

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
#if CONFIG_PVQ
    od_encode_checkpoint(&x->daala_enc, &pre_buf);
#endif
    for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
      int64_t this_rd;
      int ratey = 0;
      int64_t distortion = 0;
      int rate = bmode_costs[mode];
      int can_skip = 1;

      if (!(cpi->sf.intra_y_mode_mask[txsize_sqr_up_map[tx_size]] &
            (1 << mode)))
        continue;

      // Only do the oblique modes if the best so far is
      // one of the neighboring directional modes
      if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
        if (conditional_skipintra(mode, *best_mode)) continue;
      }

      memcpy(tempa, ta, pred_width_in_transform_blocks * sizeof(ta[0]));
      memcpy(templ, tl, pred_height_in_transform_blocks * sizeof(tl[0]));

      for (idy = 0; idy < pred_height_in_transform_blocks; ++idy) {
        for (idx = 0; idx < pred_width_in_transform_blocks; ++idx) {
          const int block_raster_idx = (row + idy) * 2 + (col + idx);
          const int block =
              av1_raster_order_to_block_index(tx_size, block_raster_idx);
          const uint8_t *const src = &src_init[idx * 4 + idy * 4 * src_stride];
          uint8_t *const dst = &dst_init[idx * 4 + idy * 4 * dst_stride];
#if !CONFIG_PVQ
          int16_t *const src_diff = av1_raster_block_offset_int16(
              BLOCK_8X8, block_raster_idx, p->src_diff);
#endif
          int skip;
          assert(block < 4);
          assert(IMPLIES(tx_size == TX_4X8 || tx_size == TX_8X4,
                         idx == 0 && idy == 0));
          assert(IMPLIES(tx_size == TX_4X8 || tx_size == TX_8X4,
                         block == 0 || block == 2));
          xd->mi[0]->bmi[block_raster_idx].as_mode = mode;
          av1_predict_intra_block(
              xd, pd->width, pd->height, txsize_to_bsize[tx_size], mode, dst,
              dst_stride, dst, dst_stride, col + idx, row + idy, 0);
#if !CONFIG_PVQ
          aom_highbd_subtract_block(tx_height, tx_width, src_diff, 8, src,
                                    src_stride, dst, dst_stride, xd->bd);
#endif
          if (is_lossless) {
            TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block, tx_size);
            const SCAN_ORDER *scan_order = get_scan(cm, tx_size, tx_type, 0);
            const int coeff_ctx =
                combine_entropy_contexts(tempa[idx], templ[idy]);
#if !CONFIG_PVQ
            av1_xform_quant(cm, x, 0, block, row + idy, col + idx, BLOCK_8X8,
                            tx_size, coeff_ctx, AV1_XFORM_QUANT_FP);
            ratey += av1_cost_coeffs(cpi, x, 0, block, tx_size, scan_order,
                                     tempa + idx, templ + idy,
                                     cpi->sf.use_fast_coef_costing);
            skip = (p->eobs[block] == 0);
            can_skip &= skip;
            tempa[idx] = !skip;
            templ[idy] = !skip;
#if CONFIG_EXT_TX
            if (tx_size == TX_8X4) {
              tempa[idx + 1] = tempa[idx];
            } else if (tx_size == TX_4X8) {
              templ[idy + 1] = templ[idy];
            }
#endif  // CONFIG_EXT_TX
#else
            (void)scan_order;

            av1_xform_quant(cm, x, 0, block, row + idy, col + idx, BLOCK_8X8,
                            tx_size, coeff_ctx, AV1_XFORM_QUANT_B);

            ratey += x->rate;
            skip = x->pvq_skip[0];
            tempa[idx] = !skip;
            templ[idy] = !skip;
            can_skip &= skip;
#endif
            if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
              goto next_highbd;
#if CONFIG_PVQ
            if (!skip)
#endif
              av1_inverse_transform_block(xd, BLOCK_OFFSET(pd->dqcoeff, block),
                                          DCT_DCT, tx_size, dst, dst_stride,
                                          p->eobs[block]);
          } else {
            int64_t dist;
            unsigned int tmp;
            TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block, tx_size);
            const SCAN_ORDER *scan_order = get_scan(cm, tx_size, tx_type, 0);
            const int coeff_ctx =
                combine_entropy_contexts(tempa[idx], templ[idy]);
#if !CONFIG_PVQ
            av1_xform_quant(cm, x, 0, block, row + idy, col + idx, BLOCK_8X8,
                            tx_size, coeff_ctx, AV1_XFORM_QUANT_FP);
            av1_optimize_b(cm, x, 0, block, BLOCK_8X8, tx_size, tempa + idx,
                           templ + idy);
            ratey += av1_cost_coeffs(cpi, x, 0, block, tx_size, scan_order,
                                     tempa + idx, templ + idy,
                                     cpi->sf.use_fast_coef_costing);
            skip = (p->eobs[block] == 0);
            can_skip &= skip;
            tempa[idx] = !skip;
            templ[idy] = !skip;
#if CONFIG_EXT_TX
            if (tx_size == TX_8X4) {
              tempa[idx + 1] = tempa[idx];
            } else if (tx_size == TX_4X8) {
              templ[idy + 1] = templ[idy];
            }
#endif  // CONFIG_EXT_TX
#else
            (void)scan_order;

            av1_xform_quant(cm, x, 0, block, row + idy, col + idx, BLOCK_8X8,
                            tx_size, coeff_ctx, AV1_XFORM_QUANT_FP);
            ratey += x->rate;
            skip = x->pvq_skip[0];
            tempa[idx] = !skip;
            templ[idy] = !skip;
            can_skip &= skip;
#endif
#if CONFIG_PVQ
            if (!skip)
#endif
              av1_inverse_transform_block(xd, BLOCK_OFFSET(pd->dqcoeff, block),
                                          tx_type, tx_size, dst, dst_stride,
                                          p->eobs[block]);
            cpi->fn_ptr[sub_bsize].vf(src, src_stride, dst, dst_stride, &tmp);
            dist = (int64_t)tmp << 4;
            distortion += dist;
            if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
              goto next_highbd;
          }
        }
      }

      rate += ratey;
      this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

      if (this_rd < best_rd) {
        *bestrate = rate;
        *bestratey = ratey;
        *bestdistortion = distortion;
        best_rd = this_rd;
        best_can_skip = can_skip;
        *best_mode = mode;
        memcpy(a, tempa, pred_width_in_transform_blocks * sizeof(tempa[0]));
        memcpy(l, templ, pred_height_in_transform_blocks * sizeof(templ[0]));
#if CONFIG_PVQ
        od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif
        for (idy = 0; idy < pred_height_in_transform_blocks * 4; ++idy) {
          memcpy(best_dst16 + idy * 8,
                 CONVERT_TO_SHORTPTR(dst_init + idy * dst_stride),
                 pred_width_in_transform_blocks * 4 * sizeof(uint16_t));
        }
      }
    next_highbd : {}
#if CONFIG_PVQ
      od_encode_rollback(&x->daala_enc, &pre_buf);
#endif
    }

    if (best_rd >= rd_thresh) return best_rd;

#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &post_buf);
#endif

    if (y_skip) *y_skip &= best_can_skip;

    for (idy = 0; idy < pred_height_in_transform_blocks * 4; ++idy) {
      memcpy(CONVERT_TO_SHORTPTR(dst_init + idy * dst_stride),
             best_dst16 + idy * 8,
             pred_width_in_transform_blocks * 4 * sizeof(uint16_t));
    }

    return best_rd;
  }
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_PVQ
  od_encode_checkpoint(&x->daala_enc, &pre_buf);
#endif  // CONFIG_PVQ

  for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
    int64_t this_rd;
    int ratey = 0;
    int64_t distortion = 0;
    int rate = bmode_costs[mode];
    int can_skip = 1;

    if (!(cpi->sf.intra_y_mode_mask[txsize_sqr_up_map[tx_size]] &
          (1 << mode))) {
      continue;
    }

    // Only do the oblique modes if the best so far is
    // one of the neighboring directional modes
    if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
      if (conditional_skipintra(mode, *best_mode)) continue;
    }

    memcpy(tempa, ta, pred_width_in_transform_blocks * sizeof(ta[0]));
    memcpy(templ, tl, pred_height_in_transform_blocks * sizeof(tl[0]));

    for (idy = 0; idy < pred_height_in_4x4_blocks; idy += tx_height_unit) {
      for (idx = 0; idx < pred_width_in_4x4_blocks; idx += tx_width_unit) {
        const int block_raster_idx = (row + idy) * 2 + (col + idx);
        int block = av1_raster_order_to_block_index(tx_size, block_raster_idx);
        const uint8_t *const src = &src_init[idx * 4 + idy * 4 * src_stride];
        uint8_t *const dst = &dst_init[idx * 4 + idy * 4 * dst_stride];
#if !CONFIG_PVQ
        int16_t *const src_diff = av1_raster_block_offset_int16(
            BLOCK_8X8, block_raster_idx, p->src_diff);
#endif  // !CONFIG_PVQ
        int skip;
        assert(block < 4);
        assert(IMPLIES(tx_size == TX_4X8 || tx_size == TX_8X4,
                       idx == 0 && idy == 0));
        assert(IMPLIES(tx_size == TX_4X8 || tx_size == TX_8X4,
                       block == 0 || block == 2));
        xd->mi[0]->bmi[block_raster_idx].as_mode = mode;
        av1_predict_intra_block(xd, pd->width, pd->height,
                                txsize_to_bsize[tx_size], mode, dst, dst_stride,
                                dst, dst_stride,
#if CONFIG_CB4X4
                                2 * (col + idx), 2 * (row + idy),
#else
                                col + idx, row + idy,
#endif  // CONFIG_CB4X4
                                0);
#if !CONFIG_PVQ
        aom_subtract_block(tx_height, tx_width, src_diff, 8, src, src_stride,
                           dst, dst_stride);
#endif  // !CONFIG_PVQ

        TX_TYPE tx_type = get_tx_type(PLANE_TYPE_Y, xd, block, tx_size);
        const SCAN_ORDER *scan_order = get_scan(cm, tx_size, tx_type, 0);
        const int coeff_ctx = combine_entropy_contexts(tempa[idx], templ[idy]);
#if CONFIG_CB4X4
        block = 4 * block;
#endif  // CONFIG_CB4X4
#if !CONFIG_PVQ
        const AV1_XFORM_QUANT xform_quant =
            is_lossless ? AV1_XFORM_QUANT_B : AV1_XFORM_QUANT_FP;
        av1_xform_quant(cm, x, 0, block,
#if CONFIG_CB4X4
                        2 * (row + idy), 2 * (col + idx),
#else
                        row + idy, col + idx,
#endif  // CONFIG_CB4X4
                        BLOCK_8X8, tx_size, coeff_ctx, xform_quant);

        av1_optimize_b(cm, x, 0, block, BLOCK_8X8, tx_size, tempa + idx,
                       templ + idy);

        ratey +=
            av1_cost_coeffs(cpi, x, 0, block, tx_size, scan_order, tempa + idx,
                            templ + idy, cpi->sf.use_fast_coef_costing);
        skip = (p->eobs[block] == 0);
        can_skip &= skip;
        tempa[idx] = !skip;
        templ[idy] = !skip;
#if CONFIG_EXT_TX
        if (tx_size == TX_8X4) {
          tempa[idx + 1] = tempa[idx];
        } else if (tx_size == TX_4X8) {
          templ[idy + 1] = templ[idy];
        }
#endif  // CONFIG_EXT_TX
#else
        (void)scan_order;

        av1_xform_quant(cm, x, 0, block,
#if CONFIG_CB4X4
                        2 * (row + idy), 2 * (col + idx),
#else
                        row + idy, col + idx,
#endif  // CONFIG_CB4X4
                        BLOCK_8X8, tx_size, coeff_ctx, AV1_XFORM_QUANT_FP);

        ratey += x->rate;
        skip = x->pvq_skip[0];
        tempa[idx] = !skip;
        templ[idy] = !skip;
        can_skip &= skip;
#endif  // !CONFIG_PVQ

        if (!is_lossless) {  // To use the pixel domain distortion, we need to
                             // calculate inverse txfm *before* calculating RD
                             // cost. Compared to calculating the distortion in
                             // the frequency domain, the overhead of encoding
                             // effort is low.
#if CONFIG_PVQ
          if (!skip)
#endif  // CONFIG_PVQ
            av1_inverse_transform_block(xd, BLOCK_OFFSET(pd->dqcoeff, block),
                                        tx_type, tx_size, dst, dst_stride,
                                        p->eobs[block]);
          unsigned int tmp;
          cpi->fn_ptr[sub_bsize].vf(src, src_stride, dst, dst_stride, &tmp);
          const int64_t dist = (int64_t)tmp << 4;
          distortion += dist;
        }

        if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
          goto next;

        if (is_lossless) {  // Calculate inverse txfm *after* RD cost.
#if CONFIG_PVQ
          if (!skip)
#endif  // CONFIG_PVQ
            av1_inverse_transform_block(xd, BLOCK_OFFSET(pd->dqcoeff, block),
                                        DCT_DCT, tx_size, dst, dst_stride,
                                        p->eobs[block]);
        }
      }
    }

    rate += ratey;
    this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

    if (this_rd < best_rd) {
      *bestrate = rate;
      *bestratey = ratey;
      *bestdistortion = distortion;
      best_rd = this_rd;
      best_can_skip = can_skip;
      *best_mode = mode;
      memcpy(a, tempa, pred_width_in_transform_blocks * sizeof(tempa[0]));
      memcpy(l, templ, pred_height_in_transform_blocks * sizeof(templ[0]));
#if CONFIG_PVQ
      od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ
      for (idy = 0; idy < pred_height_in_transform_blocks * 4; ++idy)
        memcpy(best_dst + idy * 8, dst_init + idy * dst_stride,
               pred_width_in_transform_blocks * 4);
    }
  next : {}
#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &pre_buf);
#endif  // CONFIG_PVQ
  }     // mode decision loop

  if (best_rd >= rd_thresh) return best_rd;

#if CONFIG_PVQ
  od_encode_rollback(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ

  if (y_skip) *y_skip &= best_can_skip;

  for (idy = 0; idy < pred_height_in_transform_blocks * 4; ++idy)
    memcpy(dst_init + idy * dst_stride, best_dst + idy * 8,
           pred_width_in_transform_blocks * 4);

  return best_rd;
}

static int64_t rd_pick_intra_sub_8x8_y_mode(const AV1_COMP *const cpi,
                                            MACROBLOCK *mb, int *rate,
                                            int *rate_y, int64_t *distortion,
                                            int *y_skip, int64_t best_rd) {
  const MACROBLOCKD *const xd = &mb->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  MB_MODE_INFO *const mbmi = &mic->mbmi;
  assert(!is_inter_block(mbmi));
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int pred_width_in_4x4_blocks = num_4x4_blocks_wide_lookup[bsize];
  const int pred_height_in_4x4_blocks = num_4x4_blocks_high_lookup[bsize];
  int idx, idy;
  int cost = 0;
  int64_t total_distortion = 0;
  int tot_rate_y = 0;
  int64_t total_rd = 0;
  const int *bmode_costs = cpi->mbmode_cost[0];
  const int is_lossless = xd->lossless[mbmi->segment_id];
#if CONFIG_EXT_TX && CONFIG_RECT_TX
  const TX_SIZE tx_size = is_lossless ? TX_4X4 : max_txsize_rect_lookup[bsize];
#else
  const TX_SIZE tx_size = TX_4X4;
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX

#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
  mbmi->intra_filter = INTRA_FILTER_LINEAR;
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
#endif  // CONFIG_FILTER_INTRA

  // TODO(any): Add search of the tx_type to improve rd performance at the
  // expense of speed.
  mbmi->tx_type = DCT_DCT;
  mbmi->tx_size = tx_size;

  if (y_skip) *y_skip = 1;

  // Pick modes for each prediction sub-block (of size 4x4, 4x8, or 8x4) in this
  // 8x8 coding block.
  for (idy = 0; idy < 2; idy += pred_height_in_4x4_blocks) {
    for (idx = 0; idx < 2; idx += pred_width_in_4x4_blocks) {
      PREDICTION_MODE best_mode = DC_PRED;
      int r = INT_MAX, ry = INT_MAX;
      int64_t d = INT64_MAX, this_rd = INT64_MAX;
      int j;
      const int pred_block_idx = idy * 2 + idx;
      if (cpi->common.frame_type == KEY_FRAME) {
        const PREDICTION_MODE A =
            av1_above_block_mode(mic, above_mi, pred_block_idx);
        const PREDICTION_MODE L =
            av1_left_block_mode(mic, left_mi, pred_block_idx);

        bmode_costs = cpi->y_mode_costs[A][L];
      }
      this_rd = rd_pick_intra_sub_8x8_y_subblock_mode(
          cpi, mb, idy, idx, &best_mode, bmode_costs,
          xd->plane[0].above_context + idx, xd->plane[0].left_context + idy, &r,
          &ry, &d, bsize, tx_size, y_skip, best_rd - total_rd);
#if !CONFIG_DAALA_DIST
      if (this_rd >= best_rd - total_rd) return INT64_MAX;
#endif  // !CONFIG_DAALA_DIST
      total_rd += this_rd;
      cost += r;
      total_distortion += d;
      tot_rate_y += ry;

      mic->bmi[pred_block_idx].as_mode = best_mode;
      for (j = 1; j < pred_height_in_4x4_blocks; ++j)
        mic->bmi[pred_block_idx + j * 2].as_mode = best_mode;
      for (j = 1; j < pred_width_in_4x4_blocks; ++j)
        mic->bmi[pred_block_idx + j].as_mode = best_mode;

      if (total_rd >= best_rd) return INT64_MAX;
    }
  }
  mbmi->mode = mic->bmi[3].as_mode;

#if CONFIG_DAALA_DIST
  {
    const struct macroblock_plane *p = &mb->plane[0];
    const struct macroblockd_plane *pd = &xd->plane[0];
    const int src_stride = p->src.stride;
    const int dst_stride = pd->dst.stride;
    uint8_t *src = p->src.buf;
    uint8_t *dst = pd->dst.buf;
    int use_activity_masking = 0;
    int qm = OD_HVS_QM;

#if CONFIG_PVQ
    use_activity_masking = mb->daala_enc.use_activity_masking;
#endif  // CONFIG_PVQ
    // Daala-defined distortion computed for the block of 8x8 pixels
    total_distortion = av1_daala_dist(src, src_stride, dst, dst_stride, 8, 8,
                                      qm, use_activity_masking, mb->qindex)
                       << 4;
  }
#endif  // CONFIG_DAALA_DIST
  // Add in the cost of the transform type
  if (!is_lossless) {
    int rate_tx_type = 0;
#if CONFIG_EXT_TX
    if (get_ext_tx_types(tx_size, bsize, 0, cpi->common.reduced_tx_set_used) >
        1) {
      const int eset =
          get_ext_tx_set(tx_size, bsize, 0, cpi->common.reduced_tx_set_used);
      rate_tx_type = cpi->intra_tx_type_costs[eset][txsize_sqr_map[tx_size]]
                                             [mbmi->mode][mbmi->tx_type];
    }
#else
    rate_tx_type =
        cpi->intra_tx_type_costs[txsize_sqr_map[tx_size]]
                                [intra_mode_to_tx_type_context[mbmi->mode]]
                                [mbmi->tx_type];
#endif  // CONFIG_EXT_TX
    assert(mbmi->tx_size == tx_size);
    cost += rate_tx_type;
    tot_rate_y += rate_tx_type;
  }

  *rate = cost;
  *rate_y = tot_rate_y;
  *distortion = total_distortion;

  return RDCOST(mb->rdmult, mb->rddiv, cost, total_distortion);
}

#if CONFIG_FILTER_INTRA
// Return 1 if an filter intra mode is selected; return 0 otherwise.
static int rd_pick_filter_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, int mode_cost,
                                    int64_t *best_rd, int64_t *best_model_rd,
                                    uint16_t skip_mask) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  MB_MODE_INFO *mbmi = &mic->mbmi;
  int filter_intra_selected_flag = 0;
  FILTER_INTRA_MODE mode;
  TX_SIZE best_tx_size = TX_4X4;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  TX_TYPE best_tx_type;

  av1_zero(filter_intra_mode_info);
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 1;
  mbmi->mode = DC_PRED;
#if CONFIG_PALETTE
  mbmi->palette_mode_info.palette_size[0] = 0;
#endif  // CONFIG_PALETTE

  for (mode = 0; mode < FILTER_INTRA_MODES; ++mode) {
    int this_rate;
    int64_t this_rd, this_model_rd;
    RD_STATS tokenonly_rd_stats;
    if (skip_mask & (1 << mode)) continue;
    mbmi->filter_intra_mode_info.filter_intra_mode[0] = mode;
    this_model_rd = intra_model_yrd(cpi, x, bsize, mode_cost);
    if (*best_model_rd != INT64_MAX &&
        this_model_rd > *best_model_rd + (*best_model_rd >> 1))
      continue;
    if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
    super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) continue;
    this_rate = tokenonly_rd_stats.rate +
                av1_cost_bit(cpi->common.fc->filter_intra_probs[0], 1) +
                write_uniform_cost(FILTER_INTRA_MODES, mode) + mode_cost;
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, tokenonly_rd_stats.dist);

    if (this_rd < *best_rd) {
      *best_rd = this_rd;
      best_tx_size = mic->mbmi.tx_size;
      filter_intra_mode_info = mbmi->filter_intra_mode_info;
      best_tx_type = mic->mbmi.tx_type;
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip;
      filter_intra_selected_flag = 1;
    }
  }

  if (filter_intra_selected_flag) {
    mbmi->mode = DC_PRED;
    mbmi->tx_size = best_tx_size;
    mbmi->filter_intra_mode_info.use_filter_intra_mode[0] =
        filter_intra_mode_info.use_filter_intra_mode[0];
    mbmi->filter_intra_mode_info.filter_intra_mode[0] =
        filter_intra_mode_info.filter_intra_mode[0];
    mbmi->tx_type = best_tx_type;
    return 1;
  } else {
    return 0;
  }
}
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_EXT_INTRA
// Run RD calculation with given luma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t calc_rd_given_intra_angle(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int mode_cost,
    int64_t best_rd_in, int8_t angle_delta, int max_angle_delta, int *rate,
    RD_STATS *rd_stats, int *best_angle_delta, TX_SIZE *best_tx_size,
    TX_TYPE *best_tx_type,
#if CONFIG_INTRA_INTERP
    INTRA_FILTER *best_filter,
#endif  // CONFIG_INTRA_INTERP
    int64_t *best_rd, int64_t *best_model_rd) {
  int this_rate;
  RD_STATS tokenonly_rd_stats;
  int64_t this_rd, this_model_rd;
  MB_MODE_INFO *mbmi = &x->e_mbd.mi[0]->mbmi;
  assert(!is_inter_block(mbmi));

  mbmi->angle_delta[0] = angle_delta;
  this_model_rd = intra_model_yrd(cpi, x, bsize, mode_cost);
  if (*best_model_rd != INT64_MAX &&
      this_model_rd > *best_model_rd + (*best_model_rd >> 1))
    return INT64_MAX;
  if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
  super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in);
  if (tokenonly_rd_stats.rate == INT_MAX) return INT64_MAX;

  this_rate = tokenonly_rd_stats.rate + mode_cost +
              write_uniform_cost(2 * max_angle_delta + 1,
                                 mbmi->angle_delta[0] + max_angle_delta);
  this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, tokenonly_rd_stats.dist);

  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[0];
    *best_tx_size = mbmi->tx_size;
#if CONFIG_INTRA_INTERP
    *best_filter = mbmi->intra_filter;
#endif  // CONFIG_INTRA_INTERP
    *best_tx_type = mbmi->tx_type;
    *rate = this_rate;
    rd_stats->rate = tokenonly_rd_stats.rate;
    rd_stats->dist = tokenonly_rd_stats.dist;
    rd_stats->skip = tokenonly_rd_stats.skip;
  }
  return this_rd;
}

// With given luma directional intra prediction mode, pick the best angle delta
// Return the RD cost corresponding to the best angle delta.
static int64_t rd_pick_intra_angle_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, RD_STATS *rd_stats,
                                       BLOCK_SIZE bsize, int mode_cost,
                                       int64_t best_rd,
                                       int64_t *best_model_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  MB_MODE_INFO *mbmi = &mic->mbmi;
  assert(!is_inter_block(mbmi));
  int i, angle_delta, best_angle_delta = 0;
  int first_try = 1;
#if CONFIG_INTRA_INTERP
  int p_angle;
  const int intra_filter_ctx = av1_get_pred_context_intra_interp(xd);
  INTRA_FILTER filter, best_filter = INTRA_FILTER_LINEAR;
#endif  // CONFIG_INTRA_INTERP
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];
  TX_SIZE best_tx_size = mic->mbmi.tx_size;
  TX_TYPE best_tx_type = mbmi->tx_type;

  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
#if CONFIG_INTRA_INTERP
    for (filter = INTRA_FILTER_LINEAR; filter < INTRA_FILTERS; ++filter) {
      if (FILTER_FAST_SEARCH && filter != INTRA_FILTER_LINEAR) continue;
      mic->mbmi.intra_filter = filter;
#endif  // CONFIG_INTRA_INTERP
      for (i = 0; i < 2; ++i) {
        best_rd_in = (best_rd == INT64_MAX)
                         ? INT64_MAX
                         : (best_rd + (best_rd >> (first_try ? 3 : 5)));
        this_rd = calc_rd_given_intra_angle(
            cpi, x, bsize,
#if CONFIG_INTRA_INTERP
            mode_cost + cpi->intra_filter_cost[intra_filter_ctx][filter],
#else
          mode_cost,
#endif  // CONFIG_INTRA_INTERP
            best_rd_in, (1 - 2 * i) * angle_delta, MAX_ANGLE_DELTA, rate,
            rd_stats, &best_angle_delta, &best_tx_size, &best_tx_type,
#if CONFIG_INTRA_INTERP
            &best_filter,
#endif  // CONFIG_INTRA_INTERP
            &best_rd, best_model_rd);
        rd_cost[2 * angle_delta + i] = this_rd;
        if (first_try && this_rd == INT64_MAX) return best_rd;
        first_try = 0;
        if (angle_delta == 0) {
          rd_cost[1] = this_rd;
          break;
        }
      }
#if CONFIG_INTRA_INTERP
    }
#endif  // CONFIG_INTRA_INTERP
  }

  assert(best_rd != INT64_MAX);
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    int64_t rd_thresh;
#if CONFIG_INTRA_INTERP
    for (filter = INTRA_FILTER_LINEAR; filter < INTRA_FILTERS; ++filter) {
      if (FILTER_FAST_SEARCH && filter != INTRA_FILTER_LINEAR) continue;
      mic->mbmi.intra_filter = filter;
#endif  // CONFIG_INTRA_INTERP
      for (i = 0; i < 2; ++i) {
        int skip_search = 0;
        rd_thresh = best_rd + (best_rd >> 5);
        if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
            rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
          skip_search = 1;
        if (!skip_search) {
          calc_rd_given_intra_angle(
              cpi, x, bsize,
#if CONFIG_INTRA_INTERP
              mode_cost + cpi->intra_filter_cost[intra_filter_ctx][filter],
#else
            mode_cost,
#endif  // CONFIG_INTRA_INTERP
              best_rd, (1 - 2 * i) * angle_delta, MAX_ANGLE_DELTA, rate,
              rd_stats, &best_angle_delta, &best_tx_size, &best_tx_type,
#if CONFIG_INTRA_INTERP
              &best_filter,
#endif  // CONFIG_INTRA_INTERP
              &best_rd, best_model_rd);
        }
      }
#if CONFIG_INTRA_INTERP
    }
#endif  // CONFIG_INTRA_INTERP
  }

#if CONFIG_INTRA_INTERP
  if (FILTER_FAST_SEARCH && rd_stats->rate < INT_MAX) {
    p_angle = mode_to_angle_map[mbmi->mode] + best_angle_delta * ANGLE_STEP;
    if (av1_is_intra_filter_switchable(p_angle)) {
      for (filter = INTRA_FILTER_LINEAR + 1; filter < INTRA_FILTERS; ++filter) {
        mic->mbmi.intra_filter = filter;
        this_rd = calc_rd_given_intra_angle(
            cpi, x, bsize,
            mode_cost + cpi->intra_filter_cost[intra_filter_ctx][filter],
            best_rd, best_angle_delta, MAX_ANGLE_DELTA, rate, rd_stats,
            &best_angle_delta, &best_tx_size, &best_tx_type, &best_filter,
            &best_rd, best_model_rd);
      }
    }
  }
#endif  // CONFIG_INTRA_INTERP

  mbmi->tx_size = best_tx_size;
  mbmi->angle_delta[0] = best_angle_delta;
#if CONFIG_INTRA_INTERP
  mic->mbmi.intra_filter = best_filter;
#endif  // CONFIG_INTRA_INTERP
  mbmi->tx_type = best_tx_type;
  return best_rd;
}

// Indices are sign, integer, and fractional part of the gradient value
static const uint8_t gradient_to_angle_bin[2][7][16] = {
  {
      { 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 },
      { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
      { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
      { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
      { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
      { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
  },
  {
      { 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4 },
      { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3 },
      { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
      { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
      { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
      { 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
      { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
  },
};

/* clang-format off */
static const uint8_t mode_to_angle_bin[INTRA_MODES] = {
  0, 2, 6, 0, 4, 3, 5, 7, 1, 0,
#if CONFIG_ALT_INTRA
  0,
#endif  // CONFIG_ALT_INTRA
};
/* clang-format on */

static void angle_estimation(const uint8_t *src, int src_stride, int rows,
                             int cols, BLOCK_SIZE bsize,
                             uint8_t *directional_mode_skip_mask) {
  memset(directional_mode_skip_mask, 0,
         INTRA_MODES * sizeof(*directional_mode_skip_mask));
  // Sub-8x8 blocks do not use extra directions.
  if (bsize < BLOCK_8X8) return;
  uint64_t hist[DIRECTIONAL_MODES];
  memset(hist, 0, DIRECTIONAL_MODES * sizeof(hist[0]));
  src += src_stride;
  int r, c, dx, dy;
  for (r = 1; r < rows; ++r) {
    for (c = 1; c < cols; ++c) {
      dx = src[c] - src[c - 1];
      dy = src[c] - src[c - src_stride];
      int index;
      const int temp = dx * dx + dy * dy;
      if (dy == 0) {
        index = 2;
      } else {
        const int sn = (dx > 0) ^ (dy > 0);
        dx = abs(dx);
        dy = abs(dy);
        const int remd = (dx % dy) * 16 / dy;
        const int quot = dx / dy;
        index = gradient_to_angle_bin[sn][AOMMIN(quot, 6)][AOMMIN(remd, 15)];
      }
      hist[index] += temp;
    }
    src += src_stride;
  }

  int i;
  uint64_t hist_sum = 0;
  for (i = 0; i < DIRECTIONAL_MODES; ++i) hist_sum += hist[i];
  for (i = 0; i < INTRA_MODES; ++i) {
    if (av1_is_directional_mode(i, bsize)) {
      const uint8_t angle_bin = mode_to_angle_bin[i];
      uint64_t score = 2 * hist[angle_bin];
      int weight = 2;
      if (angle_bin > 0) {
        score += hist[angle_bin - 1];
        ++weight;
      }
      if (angle_bin < DIRECTIONAL_MODES - 1) {
        score += hist[angle_bin + 1];
        ++weight;
      }
      if (score * ANGLE_SKIP_THRESH < hist_sum * weight)
        directional_mode_skip_mask[i] = 1;
    }
  }
}

#if CONFIG_HIGHBITDEPTH
static void highbd_angle_estimation(const uint8_t *src8, int src_stride,
                                    int rows, int cols, BLOCK_SIZE bsize,
                                    uint8_t *directional_mode_skip_mask) {
  memset(directional_mode_skip_mask, 0,
         INTRA_MODES * sizeof(*directional_mode_skip_mask));
  // Sub-8x8 blocks do not use extra directions.
  if (bsize < BLOCK_8X8) return;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint64_t hist[DIRECTIONAL_MODES];
  memset(hist, 0, DIRECTIONAL_MODES * sizeof(hist[0]));
  src += src_stride;
  int r, c, dx, dy;
  for (r = 1; r < rows; ++r) {
    for (c = 1; c < cols; ++c) {
      dx = src[c] - src[c - 1];
      dy = src[c] - src[c - src_stride];
      int index;
      const int temp = dx * dx + dy * dy;
      if (dy == 0) {
        index = 2;
      } else {
        const int sn = (dx > 0) ^ (dy > 0);
        dx = abs(dx);
        dy = abs(dy);
        const int remd = (dx % dy) * 16 / dy;
        const int quot = dx / dy;
        index = gradient_to_angle_bin[sn][AOMMIN(quot, 6)][AOMMIN(remd, 15)];
      }
      hist[index] += temp;
    }
    src += src_stride;
  }

  int i;
  uint64_t hist_sum = 0;
  for (i = 0; i < DIRECTIONAL_MODES; ++i) hist_sum += hist[i];
  for (i = 0; i < INTRA_MODES; ++i) {
    if (av1_is_directional_mode(i, bsize)) {
      const uint8_t angle_bin = mode_to_angle_bin[i];
      uint64_t score = 2 * hist[angle_bin];
      int weight = 2;
      if (angle_bin > 0) {
        score += hist[angle_bin - 1];
        ++weight;
      }
      if (angle_bin < DIRECTIONAL_MODES - 1) {
        score += hist[angle_bin + 1];
        ++weight;
      }
      if (score * ANGLE_SKIP_THRESH < hist_sum * weight)
        directional_mode_skip_mask[i] = 1;
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_EXT_INTRA

// This function is used only for intra_only frames
static int64_t rd_pick_intra_sby_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      int *rate, int *rate_tokenonly,
                                      int64_t *distortion, int *skippable,
                                      BLOCK_SIZE bsize, int64_t best_rd) {
  uint8_t mode_idx;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mic->mbmi;
  assert(!is_inter_block(mbmi));
  MB_MODE_INFO best_mbmi = *mbmi;
  int64_t best_model_rd = INT64_MAX;
#if CONFIG_EXT_INTRA
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
#if CONFIG_INTRA_INTERP
  const int intra_filter_ctx = av1_get_pred_context_intra_interp(xd);
#endif  // CONFIG_INTRA_INTERP
  int is_directional_mode;
  uint8_t directional_mode_skip_mask[INTRA_MODES];
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *src = x->plane[0].src.buf;
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  int beat_best_rd = 0;
  uint16_t filter_intra_mode_skip_mask = (1 << FILTER_INTRA_MODES) - 1;
#endif  // CONFIG_FILTER_INTRA
  const int *bmode_costs;
#if CONFIG_PALETTE
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  uint8_t *best_palette_color_map =
      cpi->common.allow_screen_content_tools
          ? x->palette_buffer->best_palette_color_map
          : NULL;
  int palette_y_mode_ctx = 0;
  const int try_palette =
      cpi->common.allow_screen_content_tools && bsize >= BLOCK_8X8;
#endif  // CONFIG_PALETTE
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const PREDICTION_MODE A = av1_above_block_mode(mic, above_mi, 0);
  const PREDICTION_MODE L = av1_left_block_mode(mic, left_mi, 0);
  const PREDICTION_MODE FINAL_MODE_SEARCH = TM_PRED + 1;
#if CONFIG_PVQ
  od_rollback_buffer pre_buf, post_buf;

  od_encode_checkpoint(&x->daala_enc, &pre_buf);
  od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ
  bmode_costs = cpi->y_mode_costs[A][L];

#if CONFIG_EXT_INTRA
  mbmi->angle_delta[0] = 0;
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    highbd_angle_estimation(src, src_stride, rows, cols, bsize,
                            directional_mode_skip_mask);
  else
#endif  // CONFIG_HIGHBITDEPTH
    angle_estimation(src, src_stride, rows, cols, bsize,
                     directional_mode_skip_mask);
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_PALETTE
  pmi->palette_size[0] = 0;
  if (above_mi)
    palette_y_mode_ctx +=
        (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
  if (left_mi)
    palette_y_mode_ctx += (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
#endif  // CONFIG_PALETTE

  if (cpi->sf.tx_type_search.fast_intra_tx_type_search)
    x->use_default_intra_tx_type = 1;
  else
    x->use_default_intra_tx_type = 0;

  /* Y Search for intra prediction mode */
  for (mode_idx = DC_PRED; mode_idx <= FINAL_MODE_SEARCH; ++mode_idx) {
    RD_STATS this_rd_stats;
    int this_rate, this_rate_tokenonly, s;
    int64_t this_distortion, this_rd, this_model_rd;
    if (mode_idx == FINAL_MODE_SEARCH) {
      if (x->use_default_intra_tx_type == 0) break;
      mbmi->mode = best_mbmi.mode;
      x->use_default_intra_tx_type = 0;
    } else {
      mbmi->mode = mode_idx;
    }
#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &pre_buf);
#endif  // CONFIG_PVQ
#if CONFIG_EXT_INTRA
    mbmi->angle_delta[0] = 0;
#endif  // CONFIG_EXT_INTRA
    this_model_rd = intra_model_yrd(cpi, x, bsize, bmode_costs[mbmi->mode]);
    if (best_model_rd != INT64_MAX &&
        this_model_rd > best_model_rd + (best_model_rd >> 1))
      continue;
    if (this_model_rd < best_model_rd) best_model_rd = this_model_rd;
#if CONFIG_EXT_INTRA
    is_directional_mode = av1_is_directional_mode(mbmi->mode, bsize);
    if (is_directional_mode && directional_mode_skip_mask[mbmi->mode]) continue;
    if (is_directional_mode) {
      this_rd_stats.rate = INT_MAX;
      rd_pick_intra_angle_sby(cpi, x, &this_rate, &this_rd_stats, bsize,
                              bmode_costs[mbmi->mode], best_rd, &best_model_rd);
    } else {
      super_block_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
    }
#else
    super_block_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
#endif  // CONFIG_EXT_INTRA
    this_rate_tokenonly = this_rd_stats.rate;
    this_distortion = this_rd_stats.dist;
    s = this_rd_stats.skip;

    if (this_rate_tokenonly == INT_MAX) continue;

    this_rate = this_rate_tokenonly + bmode_costs[mbmi->mode];

    if (!xd->lossless[mbmi->segment_id] && mbmi->sb_type >= BLOCK_8X8) {
      // super_block_yrd above includes the cost of the tx_size in the
      // tokenonly rate, but for intra blocks, tx_size is always coded
      // (prediction granularity), so we account for it in the full rate,
      // not the tokenonly rate.
      this_rate_tokenonly -= tx_size_cost(cpi, x, bsize, mbmi->tx_size);
    }
#if CONFIG_PALETTE
    if (try_palette && mbmi->mode == DC_PRED) {
      this_rate +=
          av1_cost_bit(av1_default_palette_y_mode_prob[bsize - BLOCK_8X8]
                                                      [palette_y_mode_ctx],
                       0);
    }
#endif  // CONFIG_PALETTE
#if CONFIG_FILTER_INTRA
    if (mbmi->mode == DC_PRED)
      this_rate += av1_cost_bit(cpi->common.fc->filter_intra_probs[0], 0);
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA
    if (is_directional_mode) {
#if CONFIG_INTRA_INTERP
      const int p_angle =
          mode_to_angle_map[mbmi->mode] + mbmi->angle_delta[0] * ANGLE_STEP;
      if (av1_is_intra_filter_switchable(p_angle))
        this_rate +=
            cpi->intra_filter_cost[intra_filter_ctx][mbmi->intra_filter];
#endif  // CONFIG_INTRA_INTERP
      this_rate += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                      MAX_ANGLE_DELTA + mbmi->angle_delta[0]);
    }
#endif  // CONFIG_EXT_INTRA
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);
#if CONFIG_FILTER_INTRA
    if (best_rd == INT64_MAX || this_rd - best_rd < (best_rd >> 4)) {
      filter_intra_mode_skip_mask ^= (1 << mbmi->mode);
    }
#endif  // CONFIG_FILTER_INTRA

    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
#if CONFIG_FILTER_INTRA
      beat_best_rd = 1;
#endif  // CONFIG_FILTER_INTRA
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_distortion;
      *skippable = s;
#if CONFIG_PVQ
      od_encode_checkpoint(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ
    }
  }

#if CONFIG_PVQ
  od_encode_rollback(&x->daala_enc, &post_buf);
#endif  // CONFIG_PVQ

#if CONFIG_CFL
  // Perform one extra txfm_rd_in_plane() call, this time with the best value so
  // we can store reconstructed luma values
  RD_STATS this_rd_stats;
  x->cfl_store_y = 1;
  txfm_rd_in_plane(x, cpi, &this_rd_stats, INT64_MAX, 0, bsize,
                   mic->mbmi.tx_size, cpi->sf.use_fast_coef_costing);
  x->cfl_store_y = 0;
#endif

#if CONFIG_PALETTE
  if (try_palette) {
    rd_pick_palette_intra_sby(cpi, x, bsize, palette_y_mode_ctx,
                              bmode_costs[DC_PRED], &best_mbmi,
                              best_palette_color_map, &best_rd, &best_model_rd,
                              rate, rate_tokenonly, distortion, skippable);
  }
#endif  // CONFIG_PALETTE

#if CONFIG_FILTER_INTRA
  if (beat_best_rd) {
    if (rd_pick_filter_intra_sby(cpi, x, rate, rate_tokenonly, distortion,
                                 skippable, bsize, bmode_costs[DC_PRED],
                                 &best_rd, &best_model_rd,
                                 filter_intra_mode_skip_mask)) {
      best_mbmi = *mbmi;
    }
  }
#endif  // CONFIG_FILTER_INTRA

  *mbmi = best_mbmi;
  return best_rd;
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int super_block_uvrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bsize,
                            int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const TX_SIZE uv_tx_size = get_uv_tx_size(mbmi, &xd->plane[1]);
  int plane;
  int is_cost_valid = 1;
  av1_init_rd_stats(rd_stats);

  if (ref_best_rd < 0) is_cost_valid = 0;

#if CONFIG_CB4X4 && !CONFIG_CHROMA_2X2
  if (x->skip_chroma_rd) return is_cost_valid;

  bsize = scale_chroma_bsize(bsize, xd->plane[1].subsampling_x,
                             xd->plane[1].subsampling_y);
#endif  // CONFIG_CB4X4 && !CONFIG_CHROMA_2X2

#if !CONFIG_PVQ
  if (is_inter_block(mbmi) && is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane)
      av1_subtract_plane(x, bsize, plane);
  }
#endif  // !CONFIG_PVQ

  if (is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
      RD_STATS pn_rd_stats;
      txfm_rd_in_plane(x, cpi, &pn_rd_stats, ref_best_rd, plane, bsize,
                       uv_tx_size, cpi->sf.use_fast_coef_costing);
      if (pn_rd_stats.rate == INT_MAX) {
        is_cost_valid = 0;
        break;
      }
      av1_merge_rd_stats(rd_stats, &pn_rd_stats);
      if (RDCOST(x->rdmult, x->rddiv, rd_stats->rate, rd_stats->dist) >
              ref_best_rd &&
          RDCOST(x->rdmult, x->rddiv, 0, rd_stats->sse) > ref_best_rd) {
        is_cost_valid = 0;
        break;
      }
    }
  }

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }

  return is_cost_valid;
}

#if CONFIG_VAR_TX
// FIXME crop these calls
static uint64_t sum_squares_2d(const int16_t *diff, int diff_stride,
                               TX_SIZE tx_size) {
  return aom_sum_squares_2d_i16(diff, diff_stride, tx_size_wide[tx_size],
                                tx_size_high[tx_size]);
}

void av1_tx_block_rd_b(const AV1_COMP *cpi, MACROBLOCK *x, TX_SIZE tx_size,
                       int blk_row, int blk_col, int plane, int block,
                       int plane_bsize, const ENTROPY_CONTEXT *a,
                       const ENTROPY_CONTEXT *l, RD_STATS *rd_stats) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int64_t tmp;
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(&xd->mi[0]->mbmi));
  BLOCK_SIZE txm_bsize = txsize_to_bsize[tx_size];
  int bh = block_size_high[txm_bsize];
  int bw = block_size_wide[txm_bsize];
  int txb_h = tx_size_high_unit[tx_size];
  int txb_w = tx_size_wide_unit[tx_size];

  int src_stride = p->src.stride;
  uint8_t *src =
      &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
  uint8_t *dst =
      &pd->dst
           .buf[(blk_row * pd->dst.stride + blk_col) << tx_size_wide_log2[0]];
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, rec_buffer16[MAX_TX_SQUARE]);
  uint8_t *rec_buffer;
#else
  DECLARE_ALIGNED(16, uint8_t, rec_buffer[MAX_TX_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH
  int max_blocks_high = block_size_high[plane_bsize];
  int max_blocks_wide = block_size_wide[plane_bsize];
  const int diff_stride = max_blocks_wide;
  const int16_t *diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];
  int txb_coeff_cost;

  assert(tx_size < TX_SIZES_ALL);

  if (xd->mb_to_bottom_edge < 0)
    max_blocks_high += xd->mb_to_bottom_edge >> (3 + pd->subsampling_y);
  if (xd->mb_to_right_edge < 0)
    max_blocks_wide += xd->mb_to_right_edge >> (3 + pd->subsampling_x);

  max_blocks_high >>= tx_size_wide_log2[0];
  max_blocks_wide >>= tx_size_wide_log2[0];

  int coeff_ctx = get_entropy_context(tx_size, a, l);

  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  coeff_ctx, AV1_XFORM_QUANT_FP);

  av1_optimize_b(cm, x, plane, block, plane_bsize, tx_size, a, l);

// TODO(any): Use av1_dist_block to compute distortion
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    rec_buffer = CONVERT_TO_BYTEPTR(rec_buffer16);
    aom_highbd_convolve_copy(dst, pd->dst.stride, rec_buffer, MAX_TX_SIZE, NULL,
                             0, NULL, 0, bw, bh, xd->bd);
  } else {
    rec_buffer = (uint8_t *)rec_buffer16;
    aom_convolve_copy(dst, pd->dst.stride, rec_buffer, MAX_TX_SIZE, NULL, 0,
                      NULL, 0, bw, bh);
  }
#else
  aom_convolve_copy(dst, pd->dst.stride, rec_buffer, MAX_TX_SIZE, NULL, 0, NULL,
                    0, bw, bh);
#endif  // CONFIG_HIGHBITDEPTH

  if (blk_row + txb_h > max_blocks_high || blk_col + txb_w > max_blocks_wide) {
    int idx, idy;
    int blocks_height = AOMMIN(txb_h, max_blocks_high - blk_row);
    int blocks_width = AOMMIN(txb_w, max_blocks_wide - blk_col);
    tmp = 0;
    for (idy = 0; idy < blocks_height; ++idy) {
      for (idx = 0; idx < blocks_width; ++idx) {
        const int16_t *d =
            diff + ((idy * diff_stride + idx) << tx_size_wide_log2[0]);
        tmp += sum_squares_2d(d, diff_stride, 0);
      }
    }
  } else {
    tmp = sum_squares_2d(diff, diff_stride, tx_size);
  }

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    tmp = ROUND_POWER_OF_TWO(tmp, (xd->bd - 8) * 2);
#endif  // CONFIG_HIGHBITDEPTH
  rd_stats->sse += tmp * 16;
  const int eob = p->eobs[block];

  av1_inverse_transform_block(xd, dqcoeff, tx_type, tx_size, rec_buffer,
                              MAX_TX_SIZE, eob);
  if (eob > 0) {
    if (txb_w + blk_col > max_blocks_wide ||
        txb_h + blk_row > max_blocks_high) {
      int idx, idy;
      unsigned int this_dist;
      int blocks_height = AOMMIN(txb_h, max_blocks_high - blk_row);
      int blocks_width = AOMMIN(txb_w, max_blocks_wide - blk_col);
      tmp = 0;
      for (idy = 0; idy < blocks_height; ++idy) {
        for (idx = 0; idx < blocks_width; ++idx) {
          uint8_t *const s =
              src + ((idy * src_stride + idx) << tx_size_wide_log2[0]);
          uint8_t *const r =
              rec_buffer + ((idy * MAX_TX_SIZE + idx) << tx_size_wide_log2[0]);
          cpi->fn_ptr[0].vf(s, src_stride, r, MAX_TX_SIZE, &this_dist);
          tmp += this_dist;
        }
      }
    } else {
      uint32_t this_dist;
      cpi->fn_ptr[txm_bsize].vf(src, src_stride, rec_buffer, MAX_TX_SIZE,
                                &this_dist);
      tmp = this_dist;
    }
  }
  rd_stats->dist += tmp * 16;
  txb_coeff_cost =
      av1_cost_coeffs(cpi, x, plane, block, tx_size, scan_order, a, l, 0);
  rd_stats->rate += txb_coeff_cost;
  rd_stats->skip &= (eob == 0);

#if CONFIG_RD_DEBUG
  av1_update_txb_coeff_cost(rd_stats, plane, tx_size, blk_row, blk_col,
                            txb_coeff_cost);
#endif  // CONFIG_RD_DEBUG
}

static void select_tx_block(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                            int blk_col, int plane, int block, int block32,
                            TX_SIZE tx_size, int depth, BLOCK_SIZE plane_bsize,
                            ENTROPY_CONTEXT *ta, ENTROPY_CONTEXT *tl,
                            TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
                            RD_STATS *rd_stats, int64_t ref_best_rd,
                            int *is_cost_valid, RD_STATS *rd_stats_stack) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE(*const inter_tx_size)
  [MAX_MIB_SIZE] =
      (TX_SIZE(*)[MAX_MIB_SIZE]) & mbmi->inter_tx_size[tx_row][tx_col];
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const int bw = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  int64_t this_rd = INT64_MAX;
  ENTROPY_CONTEXT *pta = ta + blk_col;
  ENTROPY_CONTEXT *ptl = tl + blk_row;
  int coeff_ctx, i;
  int ctx = txfm_partition_context(tx_above + blk_col, tx_left + blk_row,
                                   mbmi->sb_type, tx_size);
  int64_t sum_rd = INT64_MAX;
  int tmp_eob = 0;
  int zero_blk_rate;
  RD_STATS sum_rd_stats;
  const int tx_size_ctx = txsize_sqr_map[tx_size];

  av1_init_rd_stats(&sum_rd_stats);

  assert(tx_size < TX_SIZES_ALL);

  if (ref_best_rd < 0) {
    *is_cost_valid = 0;
    return;
  }

  coeff_ctx = get_entropy_context(tx_size, pta, ptl);

  av1_init_rd_stats(rd_stats);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  zero_blk_rate = x->token_costs[tx_size_ctx][pd->plane_type][1][0][0]
                                [coeff_ctx][EOB_TOKEN];

  if (cpi->common.tx_mode == TX_MODE_SELECT || tx_size == TX_4X4) {
    inter_tx_size[0][0] = tx_size;

    if (tx_size == TX_32X32 && mbmi->tx_type != DCT_DCT &&
        rd_stats_stack[block32].rate != INT_MAX) {
      *rd_stats = rd_stats_stack[block32];
      p->eobs[block] = !rd_stats->skip;
      x->blk_skip[plane][blk_row * bw + blk_col] = rd_stats->skip;
    } else {
      av1_tx_block_rd_b(cpi, x, tx_size, blk_row, blk_col, plane, block,
                        plane_bsize, pta, ptl, rd_stats);
      if (tx_size == TX_32X32) {
        rd_stats_stack[block32] = *rd_stats;
      }
    }

    if ((RDCOST(x->rdmult, x->rddiv, rd_stats->rate, rd_stats->dist) >=
             RDCOST(x->rdmult, x->rddiv, zero_blk_rate, rd_stats->sse) ||
         rd_stats->skip == 1) &&
        !xd->lossless[mbmi->segment_id]) {
#if CONFIG_RD_DEBUG
      av1_update_txb_coeff_cost(rd_stats, plane, tx_size, blk_row, blk_col,
                                zero_blk_rate - rd_stats->rate);
#endif  // CONFIG_RD_DEBUG
      rd_stats->rate = zero_blk_rate;
      rd_stats->dist = rd_stats->sse;
      rd_stats->skip = 1;
      x->blk_skip[plane][blk_row * bw + blk_col] = 1;
      p->eobs[block] = 0;
    } else {
      x->blk_skip[plane][blk_row * bw + blk_col] = 0;
      rd_stats->skip = 0;
    }

    if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
      rd_stats->rate +=
          av1_cost_bit(cpi->common.fc->txfm_partition_prob[ctx], 0);
    this_rd = RDCOST(x->rdmult, x->rddiv, rd_stats->rate, rd_stats->dist);
    tmp_eob = p->eobs[block];
  }

  if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH) {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int sub_step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];
    RD_STATS this_rd_stats;
    int this_cost_valid = 1;
    int64_t tmp_rd = 0;

    sum_rd_stats.rate =
        av1_cost_bit(cpi->common.fc->txfm_partition_prob[ctx], 1);

    assert(tx_size < TX_SIZES_ALL);

    for (i = 0; i < 4 && this_cost_valid; ++i) {
      int offsetr = blk_row + (i >> 1) * bsl;
      int offsetc = blk_col + (i & 0x01) * bsl;

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      select_tx_block(cpi, x, offsetr, offsetc, plane, block, block32, sub_txs,
                      depth + 1, plane_bsize, ta, tl, tx_above, tx_left,
                      &this_rd_stats, ref_best_rd - tmp_rd, &this_cost_valid,
                      rd_stats_stack);

      av1_merge_rd_stats(&sum_rd_stats, &this_rd_stats);

      tmp_rd =
          RDCOST(x->rdmult, x->rddiv, sum_rd_stats.rate, sum_rd_stats.dist);
      if (this_rd < tmp_rd) break;
      block += sub_step;
    }
    if (this_cost_valid) sum_rd = tmp_rd;
  }

  if (this_rd < sum_rd) {
    int idx, idy;
    for (i = 0; i < tx_size_wide_unit[tx_size]; ++i) pta[i] = !(tmp_eob == 0);
    for (i = 0; i < tx_size_high_unit[tx_size]; ++i) ptl[i] = !(tmp_eob == 0);
    txfm_partition_update(tx_above + blk_col, tx_left + blk_row, tx_size,
                          tx_size);
    inter_tx_size[0][0] = tx_size;
    for (idy = 0; idy < tx_size_high_unit[tx_size] / 2; ++idy)
      for (idx = 0; idx < tx_size_wide_unit[tx_size] / 2; ++idx)
        inter_tx_size[idy][idx] = tx_size;
    mbmi->tx_size = tx_size;
    if (this_rd == INT64_MAX) *is_cost_valid = 0;
    x->blk_skip[plane][blk_row * bw + blk_col] = rd_stats->skip;
  } else {
    *rd_stats = sum_rd_stats;
    if (sum_rd == INT64_MAX) *is_cost_valid = 0;
  }
}

static void inter_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bsize,
                            int64_t ref_best_rd, RD_STATS *rd_stats_stack) {
  MACROBLOCKD *const xd = &x->e_mbd;
  int is_cost_valid = 1;
  int64_t this_rd = 0;

  if (ref_best_rd < 0) is_cost_valid = 0;

  av1_init_rd_stats(rd_stats);

  if (is_cost_valid) {
    const struct macroblockd_plane *const pd = &xd->plane[0];
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
    const TX_SIZE max_tx_size = max_txsize_rect_lookup[plane_bsize];
    const int bh = tx_size_high_unit[max_tx_size];
    const int bw = tx_size_wide_unit[max_tx_size];
    int idx, idy;
    int block = 0;
    int block32 = 0;
    int step = tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
    ENTROPY_CONTEXT ctxa[2 * MAX_MIB_SIZE];
    ENTROPY_CONTEXT ctxl[2 * MAX_MIB_SIZE];
    TXFM_CONTEXT tx_above[MAX_MIB_SIZE * 2];
    TXFM_CONTEXT tx_left[MAX_MIB_SIZE * 2];

    RD_STATS pn_rd_stats;
    av1_init_rd_stats(&pn_rd_stats);

    av1_get_entropy_contexts(bsize, 0, pd, ctxa, ctxl);
    memcpy(tx_above, xd->above_txfm_context, sizeof(TXFM_CONTEXT) * mi_width);
    memcpy(tx_left, xd->left_txfm_context, sizeof(TXFM_CONTEXT) * mi_height);

    for (idy = 0; idy < mi_height; idy += bh) {
      for (idx = 0; idx < mi_width; idx += bw) {
        select_tx_block(cpi, x, idy, idx, 0, block, block32, max_tx_size,
                        mi_height != mi_width, plane_bsize, ctxa, ctxl,
                        tx_above, tx_left, &pn_rd_stats, ref_best_rd - this_rd,
                        &is_cost_valid, rd_stats_stack);
        av1_merge_rd_stats(rd_stats, &pn_rd_stats);
        this_rd += AOMMIN(
            RDCOST(x->rdmult, x->rddiv, pn_rd_stats.rate, pn_rd_stats.dist),
            RDCOST(x->rdmult, x->rddiv, 0, pn_rd_stats.sse));
        block += step;
        ++block32;
      }
    }
  }

  this_rd = AOMMIN(RDCOST(x->rdmult, x->rddiv, rd_stats->rate, rd_stats->dist),
                   RDCOST(x->rdmult, x->rddiv, 0, rd_stats->sse));
  if (this_rd > ref_best_rd) is_cost_valid = 0;

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }
}

static int64_t select_tx_size_fix_type(const AV1_COMP *cpi, MACROBLOCK *x,
                                       RD_STATS *rd_stats, BLOCK_SIZE bsize,
                                       int64_t ref_best_rd, TX_TYPE tx_type,
                                       RD_STATS *rd_stats_stack) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_inter = is_inter_block(mbmi);
  aom_prob skip_prob = av1_get_skip_prob(cm, xd);
  int s0 = av1_cost_bit(skip_prob, 0);
  int s1 = av1_cost_bit(skip_prob, 1);
  int64_t rd;
  int row, col;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);

  mbmi->tx_type = tx_type;
  inter_block_yrd(cpi, x, rd_stats, bsize, ref_best_rd, rd_stats_stack);
  mbmi->min_tx_size = get_min_tx_size(mbmi->inter_tx_size[0][0]);

  if (rd_stats->rate == INT_MAX) return INT64_MAX;

  for (row = 0; row < max_blocks_high / 2; ++row)
    for (col = 0; col < max_blocks_wide / 2; ++col)
      mbmi->min_tx_size = AOMMIN(
          mbmi->min_tx_size, get_min_tx_size(mbmi->inter_tx_size[row][col]));

#if CONFIG_EXT_TX
  if (get_ext_tx_types(mbmi->min_tx_size, bsize, is_inter,
                       cm->reduced_tx_set_used) > 1 &&
      !xd->lossless[xd->mi[0]->mbmi.segment_id]) {
    const int ext_tx_set = get_ext_tx_set(mbmi->min_tx_size, bsize, is_inter,
                                          cm->reduced_tx_set_used);
    if (is_inter) {
      if (ext_tx_set > 0)
        rd_stats->rate +=
            cpi->inter_tx_type_costs[ext_tx_set]
                                    [txsize_sqr_map[mbmi->min_tx_size]]
                                    [mbmi->tx_type];
    } else {
      if (ext_tx_set > 0 && ALLOW_INTRA_EXT_TX)
        rd_stats->rate +=
            cpi->intra_tx_type_costs[ext_tx_set][mbmi->min_tx_size][mbmi->mode]
                                    [mbmi->tx_type];
    }
  }
#else   // CONFIG_EXT_TX
  if (mbmi->min_tx_size < TX_32X32 && !xd->lossless[xd->mi[0]->mbmi.segment_id])
    rd_stats->rate +=
        cpi->inter_tx_type_costs[mbmi->min_tx_size][mbmi->tx_type];
#endif  // CONFIG_EXT_TX

  if (rd_stats->skip)
    rd = RDCOST(x->rdmult, x->rddiv, s1, rd_stats->sse);
  else
    rd = RDCOST(x->rdmult, x->rddiv, rd_stats->rate + s0, rd_stats->dist);

  if (is_inter && !xd->lossless[xd->mi[0]->mbmi.segment_id] &&
      !(rd_stats->skip))
    rd = AOMMIN(rd, RDCOST(x->rdmult, x->rddiv, s1, rd_stats->sse));

  return rd;
}

static void select_tx_type_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                               RD_STATS *rd_stats, BLOCK_SIZE bsize,
                               int64_t ref_best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int64_t rd = INT64_MAX;
  int64_t best_rd = INT64_MAX;
  TX_TYPE tx_type, best_tx_type = DCT_DCT;
  const int is_inter = is_inter_block(mbmi);
  TX_SIZE best_tx_size[MAX_MIB_SIZE][MAX_MIB_SIZE];
  TX_SIZE best_tx = max_txsize_lookup[bsize];
  TX_SIZE best_min_tx_size = TX_SIZES_ALL;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE * 8];
  const int n4 = bsize_to_num_blk(bsize);
  int idx, idy;
  int prune = 0;
  const int count32 =
      1 << (2 * (cm->mib_size_log2 - mi_width_log2_lookup[BLOCK_32X32]));
#if CONFIG_EXT_PARTITION
  RD_STATS rd_stats_stack[16];
#else
  RD_STATS rd_stats_stack[4];
#endif  // CONFIG_EXT_PARTITION
#if CONFIG_EXT_TX
  const int ext_tx_set =
      get_ext_tx_set(max_tx_size, bsize, is_inter, cm->reduced_tx_set_used);
#endif  // CONFIG_EXT_TX

  if (is_inter && cpi->sf.tx_type_search.prune_mode > NO_PRUNE)
#if CONFIG_EXT_TX
    prune = prune_tx_types(cpi, bsize, x, xd, ext_tx_set);
#else
    prune = prune_tx_types(cpi, bsize, x, xd, 0);
#endif  // CONFIG_EXT_TX

  av1_invalid_rd_stats(rd_stats);

  for (idx = 0; idx < count32; ++idx)
    av1_invalid_rd_stats(&rd_stats_stack[idx]);

  for (tx_type = DCT_DCT; tx_type < TX_TYPES; ++tx_type) {
    RD_STATS this_rd_stats;
    av1_init_rd_stats(&this_rd_stats);
#if CONFIG_EXT_TX
    if (is_inter) {
      if (!ext_tx_used_inter[ext_tx_set][tx_type]) continue;
      if (cpi->sf.tx_type_search.prune_mode > NO_PRUNE) {
        if (!do_tx_type_search(tx_type, prune)) continue;
      }
    } else {
      if (!ALLOW_INTRA_EXT_TX && bsize >= BLOCK_8X8) {
        if (tx_type != intra_mode_to_tx_type_context[mbmi->mode]) continue;
      }
      if (!ext_tx_used_intra[ext_tx_set][tx_type]) continue;
    }
#else   // CONFIG_EXT_TX
    if (is_inter && cpi->sf.tx_type_search.prune_mode > NO_PRUNE &&
        !do_tx_type_search(tx_type, prune))
      continue;
#endif  // CONFIG_EXT_TX
    if (is_inter && x->use_default_inter_tx_type &&
        tx_type != get_default_tx_type(0, xd, 0, max_tx_size))
      continue;

    if (xd->lossless[mbmi->segment_id])
      if (tx_type != DCT_DCT) continue;

    rd = select_tx_size_fix_type(cpi, x, &this_rd_stats, bsize, ref_best_rd,
                                 tx_type, rd_stats_stack);

    if (rd < best_rd) {
      best_rd = rd;
      *rd_stats = this_rd_stats;
      best_tx_type = mbmi->tx_type;
      best_tx = mbmi->tx_size;
      best_min_tx_size = mbmi->min_tx_size;
      memcpy(best_blk_skip, x->blk_skip[0], sizeof(best_blk_skip[0]) * n4);
      for (idy = 0; idy < xd->n8_h; ++idy)
        for (idx = 0; idx < xd->n8_w; ++idx)
          best_tx_size[idy][idx] = mbmi->inter_tx_size[idy][idx];
    }
  }

  mbmi->tx_type = best_tx_type;
  for (idy = 0; idy < xd->n8_h; ++idy)
    for (idx = 0; idx < xd->n8_w; ++idx)
      mbmi->inter_tx_size[idy][idx] = best_tx_size[idy][idx];
  mbmi->tx_size = best_tx;
  mbmi->min_tx_size = best_min_tx_size;
  memcpy(x->blk_skip[0], best_blk_skip, sizeof(best_blk_skip[0]) * n4);
}

static void tx_block_rd(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                        int blk_col, int plane, int block, TX_SIZE tx_size,
                        BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *above_ctx,
                        ENTROPY_CONTEXT *left_ctx, RD_STATS *rd_stats) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE plane_tx_size;
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  assert(tx_size < TX_SIZES_ALL);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];

  if (tx_size == plane_tx_size) {
    int i;
    ENTROPY_CONTEXT *ta = above_ctx + blk_col;
    ENTROPY_CONTEXT *tl = left_ctx + blk_row;
    av1_tx_block_rd_b(cpi, x, tx_size, blk_row, blk_col, plane, block,
                      plane_bsize, ta, tl, rd_stats);

    for (i = 0; i < tx_size_wide_unit[tx_size]; ++i)
      ta[i] = !(p->eobs[block] == 0);
    for (i = 0; i < tx_size_high_unit[tx_size]; ++i)
      tl[i] = !(p->eobs[block] == 0);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];
    int i;

    assert(bsl > 0);

    for (i = 0; i < 4; ++i) {
      int offsetr = blk_row + (i >> 1) * bsl;
      int offsetc = blk_col + (i & 0x01) * bsl;

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      tx_block_rd(cpi, x, offsetr, offsetc, plane, block, sub_txs, plane_bsize,
                  above_ctx, left_ctx, rd_stats);
      block += step;
    }
  }
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int inter_block_uvrd(const AV1_COMP *cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bsize,
                            int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int plane;
  int is_cost_valid = 1;
  int64_t this_rd;

  if (ref_best_rd < 0) is_cost_valid = 0;

  av1_init_rd_stats(rd_stats);

#if CONFIG_CB4X4 && !CONFIG_CHROMA_2X2
  if (x->skip_chroma_rd) return is_cost_valid;
  bsize = scale_chroma_bsize(mbmi->sb_type, xd->plane[1].subsampling_x,
                             xd->plane[1].subsampling_y);
#endif  // CONFIG_CB4X4 && !CONFIG_CHROMA_2X2

#if CONFIG_EXT_TX && CONFIG_RECT_TX
  if (is_rect_tx(mbmi->tx_size)) {
    return super_block_uvrd(cpi, x, rd_stats, bsize, ref_best_rd);
  }
#endif  // CONFIG_EXT_TX && CONFIG_RECT_TX

  if (is_inter_block(mbmi) && is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane)
      av1_subtract_plane(x, bsize, plane);
  }

  for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
    const TX_SIZE max_tx_size = max_txsize_rect_lookup[plane_bsize];
    const int bh = tx_size_high_unit[max_tx_size];
    const int bw = tx_size_wide_unit[max_tx_size];
    int idx, idy;
    int block = 0;
    const int step = bh * bw;
    ENTROPY_CONTEXT ta[2 * MAX_MIB_SIZE];
    ENTROPY_CONTEXT tl[2 * MAX_MIB_SIZE];
    RD_STATS pn_rd_stats;
    av1_init_rd_stats(&pn_rd_stats);

    av1_get_entropy_contexts(bsize, 0, pd, ta, tl);

    for (idy = 0; idy < mi_height; idy += bh) {
      for (idx = 0; idx < mi_width; idx += bw) {
        tx_block_rd(cpi, x, idy, idx, plane, block, max_tx_size, plane_bsize,
                    ta, tl, &pn_rd_stats);
        block += step;
      }
    }

    if (pn_rd_stats.rate == INT_MAX) {
      is_cost_valid = 0;
      break;
    }

    av1_merge_rd_stats(rd_stats, &pn_rd_stats);

    this_rd =
        AOMMIN(RDCOST(x->rdmult, x->rddiv, rd_stats->rate, rd_stats->dist),
               RDCOST(x->rdmult, x->rddiv, 0, rd_stats->sse));

    if (this_rd > ref_best_rd) {
      is_cost_valid = 0;
      break;
    }
  }

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }

  return is_cost_valid;
}
#endif  // CONFIG_VAR_TX

#if CONFIG_PALETTE
static void rd_pick_palette_intra_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int dc_mode_cost,
                                       uint8_t *best_palette_color_map,
                                       MB_MODE_INFO *const best_mbmi,
                                       int64_t *best_rd, int *rate,
                                       int *rate_tokenonly, int64_t *distortion,
                                       int *skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int this_rate;
  int64_t this_rd;
  int colors_u, colors_v, colors;
  const int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  uint8_t *const color_map = xd->plane[1].color_index_map;
  RD_STATS tokenonly_rd_stats;
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(bsize, 1, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);
  if (rows * cols > PALETTE_MAX_BLOCK_SIZE) return;

  mbmi->uv_mode = DC_PRED;
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_HIGHBITDEPTH
  if (cpi->common.use_highbitdepth) {
    colors_u = av1_count_colors_highbd(src_u, src_stride, rows, cols,
                                       cpi->common.bit_depth);
    colors_v = av1_count_colors_highbd(src_v, src_stride, rows, cols,
                                       cpi->common.bit_depth);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    colors_u = av1_count_colors(src_u, src_stride, rows, cols);
    colors_v = av1_count_colors(src_v, src_stride, rows, cols);
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_PALETTE_DELTA_ENCODING
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(above_mi, left_mi, 1, color_cache);
#endif  // CONFIG_PALETTE_DELTA_ENCODING

  colors = colors_u > colors_v ? colors_u : colors_v;
  if (colors > 1 && colors <= 64) {
    int r, c, n, i, j;
    const int max_itr = 50;
    uint8_t color_order[PALETTE_MAX_SIZE];
    float lb_u, ub_u, val_u;
    float lb_v, ub_v, val_v;
    float *const data = x->palette_buffer->kmeans_data_buf;
    float centroids[2 * PALETTE_MAX_SIZE];

#if CONFIG_HIGHBITDEPTH
    uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src_u);
    uint16_t *src_v16 = CONVERT_TO_SHORTPTR(src_v);
    if (cpi->common.use_highbitdepth) {
      lb_u = src_u16[0];
      ub_u = src_u16[0];
      lb_v = src_v16[0];
      ub_v = src_v16[0];
    } else {
#endif  // CONFIG_HIGHBITDEPTH
      lb_u = src_u[0];
      ub_u = src_u[0];
      lb_v = src_v[0];
      ub_v = src_v[0];
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH

    for (r = 0; r < rows; ++r) {
      for (c = 0; c < cols; ++c) {
#if CONFIG_HIGHBITDEPTH
        if (cpi->common.use_highbitdepth) {
          val_u = src_u16[r * src_stride + c];
          val_v = src_v16[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        } else {
#endif  // CONFIG_HIGHBITDEPTH
          val_u = src_u[r * src_stride + c];
          val_v = src_v[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
#if CONFIG_HIGHBITDEPTH
        }
#endif  // CONFIG_HIGHBITDEPTH
        if (val_u < lb_u)
          lb_u = val_u;
        else if (val_u > ub_u)
          ub_u = val_u;
        if (val_v < lb_v)
          lb_v = val_v;
        else if (val_v > ub_v)
          ub_v = val_v;
      }
    }

    for (n = colors > PALETTE_MAX_SIZE ? PALETTE_MAX_SIZE : colors; n >= 2;
         --n) {
      for (i = 0; i < n; ++i) {
        centroids[i * 2] = lb_u + (2 * i + 1) * (ub_u - lb_u) / n / 2;
        centroids[i * 2 + 1] = lb_v + (2 * i + 1) * (ub_v - lb_v) / n / 2;
      }
      av1_k_means(data, centroids, color_map, rows * cols, n, 2, max_itr);
#if CONFIG_PALETTE_DELTA_ENCODING
      optimize_palette_colors(color_cache, n_cache, n, 2, centroids);
      // Sort the U channel colors in ascending order.
      for (i = 0; i < 2 * (n - 1); i += 2) {
        int min_idx = i;
        float min_val = centroids[i];
        for (j = i + 2; j < 2 * n; j += 2)
          if (centroids[j] < min_val) min_val = centroids[j], min_idx = j;
        if (min_idx != i) {
          float temp_u = centroids[i], temp_v = centroids[i + 1];
          centroids[i] = centroids[min_idx];
          centroids[i + 1] = centroids[min_idx + 1];
          centroids[min_idx] = temp_u, centroids[min_idx + 1] = temp_v;
        }
      }
      av1_calc_indices(data, centroids, color_map, rows * cols, n, 2);
#endif  // CONFIG_PALETTE_DELTA_ENCODING
      extend_palette_color_map(color_map, cols, rows, plane_block_width,
                               plane_block_height);
      pmi->palette_size[1] = n;
      for (i = 1; i < 3; ++i) {
        for (j = 0; j < n; ++j) {
#if CONFIG_HIGHBITDEPTH
          if (cpi->common.use_highbitdepth)
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] = clip_pixel_highbd(
                (int)centroids[j * 2 + i - 1], cpi->common.bit_depth);
          else
#endif  // CONFIG_HIGHBITDEPTH
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] =
                clip_pixel((int)centroids[j * 2 + i - 1]);
        }
      }

      super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
      if (tokenonly_rd_stats.rate == INT_MAX) continue;
      this_rate =
          tokenonly_rd_stats.rate + dc_mode_cost +
          cpi->palette_uv_size_cost[bsize - BLOCK_8X8][n - PALETTE_MIN_SIZE] +
          write_uniform_cost(n, color_map[0]) +
          av1_cost_bit(
              av1_default_palette_uv_mode_prob[pmi->palette_size[0] > 0], 1);
      this_rate += av1_palette_color_cost_uv(pmi,
#if CONFIG_PALETTE_DELTA_ENCODING
                                             color_cache, n_cache,
#endif  // CONFIG_PALETTE_DELTA_ENCODING
                                             cpi->common.bit_depth);
      for (i = 0; i < rows; ++i) {
        for (j = (i == 0 ? 1 : 0); j < cols; ++j) {
          int color_idx;
          const int color_ctx = av1_get_palette_color_index_context(
              color_map, plane_block_width, i, j, n, color_order, &color_idx);
          assert(color_idx >= 0 && color_idx < n);
          this_rate += cpi->palette_uv_color_cost[n - PALETTE_MIN_SIZE]
                                                 [color_ctx][color_idx];
        }
      }

      this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, tokenonly_rd_stats.dist);
      if (this_rd < *best_rd) {
        *best_rd = this_rd;
        *best_mbmi = *mbmi;
        memcpy(best_palette_color_map, color_map,
               plane_block_width * plane_block_height *
                   sizeof(best_palette_color_map[0]));
        *rate = this_rate;
        *distortion = tokenonly_rd_stats.dist;
        *rate_tokenonly = tokenonly_rd_stats.rate;
        *skippable = tokenonly_rd_stats.skip;
      }
    }
  }
  if (best_mbmi->palette_mode_info.palette_size[1] > 0) {
    memcpy(color_map, best_palette_color_map,
           rows * cols * sizeof(best_palette_color_map[0]));
  }
}
#endif  // CONFIG_PALETTE

#if CONFIG_FILTER_INTRA
// Return 1 if an filter intra mode is selected; return 0 otherwise.
static int rd_pick_filter_intra_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                     int *rate, int *rate_tokenonly,
                                     int64_t *distortion, int *skippable,
                                     BLOCK_SIZE bsize, int64_t *best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int filter_intra_selected_flag = 0;
  int this_rate;
  int64_t this_rd;
  FILTER_INTRA_MODE mode;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  RD_STATS tokenonly_rd_stats;

  av1_zero(filter_intra_mode_info);
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 1;
  mbmi->uv_mode = DC_PRED;
#if CONFIG_PALETTE
  mbmi->palette_mode_info.palette_size[1] = 0;
#endif  // CONFIG_PALETTE

  for (mode = 0; mode < FILTER_INTRA_MODES; ++mode) {
    mbmi->filter_intra_mode_info.filter_intra_mode[1] = mode;
    if (!super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd))
      continue;

    this_rate = tokenonly_rd_stats.rate +
                av1_cost_bit(cpi->common.fc->filter_intra_probs[1], 1) +
                cpi->intra_uv_mode_cost[mbmi->mode][mbmi->uv_mode] +
                write_uniform_cost(FILTER_INTRA_MODES, mode);
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, tokenonly_rd_stats.dist);
    if (this_rd < *best_rd) {
      *best_rd = this_rd;
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip;
      filter_intra_mode_info = mbmi->filter_intra_mode_info;
      filter_intra_selected_flag = 1;
    }
  }

  if (filter_intra_selected_flag) {
    mbmi->uv_mode = DC_PRED;
    mbmi->filter_intra_mode_info.use_filter_intra_mode[1] =
        filter_intra_mode_info.use_filter_intra_mode[1];
    mbmi->filter_intra_mode_info.filter_intra_mode[1] =
        filter_intra_mode_info.filter_intra_mode[1];
    return 1;
  } else {
    return 0;
  }
}
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_EXT_INTRA
// Run RD calculation with given chroma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t pick_intra_angle_routine_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int rate_overhead, int64_t best_rd_in, int *rate, RD_STATS *rd_stats,
    int *best_angle_delta, int64_t *best_rd) {
  MB_MODE_INFO *mbmi = &x->e_mbd.mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  int this_rate;
  int64_t this_rd;
  RD_STATS tokenonly_rd_stats;

  if (!super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in))
    return INT64_MAX;
  this_rate = tokenonly_rd_stats.rate + rate_overhead;
  this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, tokenonly_rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[1];
    *rate = this_rate;
    rd_stats->rate = tokenonly_rd_stats.rate;
    rd_stats->dist = tokenonly_rd_stats.dist;
    rd_stats->skip = tokenonly_rd_stats.skip;
  }
  return this_rd;
}

// With given chroma directional intra prediction mode, pick the best angle
// delta. Return true if a RD cost that is smaller than the input one is found.
static int rd_pick_intra_angle_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, int rate_overhead,
                                    int64_t best_rd, int *rate,
                                    RD_STATS *rd_stats) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  int i, angle_delta, best_angle_delta = 0;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];

  rd_stats->rate = INT_MAX;
  rd_stats->skip = 0;
  rd_stats->dist = INT64_MAX;
  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> ((angle_delta == 0) ? 3 : 5)));
      mbmi->angle_delta[1] = (1 - 2 * i) * angle_delta;
      this_rd = pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead,
                                              best_rd_in, rate, rd_stats,
                                              &best_angle_delta, &best_rd);
      rd_cost[2 * angle_delta + i] = this_rd;
      if (angle_delta == 0) {
        if (this_rd == INT64_MAX) return 0;
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    int64_t rd_thresh;
    for (i = 0; i < 2; ++i) {
      int skip_search = 0;
      rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        mbmi->angle_delta[1] = (1 - 2 * i) * angle_delta;
        pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                      rate, rd_stats, &best_angle_delta,
                                      &best_rd);
      }
    }
  }

  mbmi->angle_delta[1] = best_angle_delta;
  return rd_stats->rate != INT_MAX;
}
#endif  // CONFIG_EXT_INTRA

static void init_sbuv_mode(MB_MODE_INFO *const mbmi) {
  mbmi->uv_mode = DC_PRED;
#if CONFIG_PALETTE
  mbmi->palette_mode_info.palette_size[1] = 0;
#endif  // CONFIG_PALETTE
#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
}

static int64_t rd_pick_intra_sbuv_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, int *rate_tokenonly,
                                       int64_t *distortion, int *skippable,
                                       BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  assert(!is_inter_block(mbmi));
  MB_MODE_INFO best_mbmi = *mbmi;
  PREDICTION_MODE mode;
  int64_t best_rd = INT64_MAX, this_rd;
  int this_rate;
  RD_STATS tokenonly_rd_stats;
#if CONFIG_PVQ
  od_rollback_buffer buf;
  od_encode_checkpoint(&x->daala_enc, &buf);
#endif  // CONFIG_PVQ
#if CONFIG_PALETTE
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  uint8_t *best_palette_color_map = NULL;
#endif  // CONFIG_PALETTE

  for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
#if CONFIG_EXT_INTRA
    const int is_directional_mode =
        av1_is_directional_mode(mode, mbmi->sb_type);
#endif  // CONFIG_EXT_INTRA
    if (!(cpi->sf.intra_uv_mode_mask[txsize_sqr_up_map[max_tx_size]] &
          (1 << mode)))
      continue;

    mbmi->uv_mode = mode;
#if CONFIG_EXT_INTRA
    mbmi->angle_delta[1] = 0;
    if (is_directional_mode) {
      const int rate_overhead = cpi->intra_uv_mode_cost[mbmi->mode][mode] +
                                write_uniform_cost(2 * MAX_ANGLE_DELTA + 1, 0);
      if (!rd_pick_intra_angle_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                    &this_rate, &tokenonly_rd_stats))
        continue;
    } else {
#endif  // CONFIG_EXT_INTRA
      if (!super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd)) {
#if CONFIG_PVQ
        od_encode_rollback(&x->daala_enc, &buf);
#endif  // CONFIG_PVQ
        continue;
      }
#if CONFIG_EXT_INTRA
    }
#endif  // CONFIG_EXT_INTRA
    this_rate =
        tokenonly_rd_stats.rate + cpi->intra_uv_mode_cost[mbmi->mode][mode];

#if CONFIG_EXT_INTRA
    if (is_directional_mode) {
      this_rate += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                      MAX_ANGLE_DELTA + mbmi->angle_delta[1]);
    }
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
    if (mbmi->sb_type >= BLOCK_8X8 && mode == DC_PRED)
      this_rate += av1_cost_bit(cpi->common.fc->filter_intra_probs[1], 0);
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_PALETTE
    if (cpi->common.allow_screen_content_tools && mbmi->sb_type >= BLOCK_8X8 &&
        mode == DC_PRED)
      this_rate += av1_cost_bit(
          av1_default_palette_uv_mode_prob[pmi->palette_size[0] > 0], 0);
#endif  // CONFIG_PALETTE

#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &buf);
#endif  // CONFIG_PVQ
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, tokenonly_rd_stats.dist);

    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip;
    }
  }

#if CONFIG_PALETTE
  if (cpi->common.allow_screen_content_tools && mbmi->sb_type >= BLOCK_8X8) {
    best_palette_color_map = x->palette_buffer->best_palette_color_map;
    rd_pick_palette_intra_sbuv(cpi, x,
                               cpi->intra_uv_mode_cost[mbmi->mode][DC_PRED],
                               best_palette_color_map, &best_mbmi, &best_rd,
                               rate, rate_tokenonly, distortion, skippable);
  }
#endif  // CONFIG_PALETTE

#if CONFIG_FILTER_INTRA
  if (mbmi->sb_type >= BLOCK_8X8) {
    if (rd_pick_filter_intra_sbuv(cpi, x, rate, rate_tokenonly, distortion,
                                  skippable, bsize, &best_rd))
      best_mbmi = *mbmi;
  }
#endif  // CONFIG_FILTER_INTRA

  *mbmi = best_mbmi;
  // Make sure we actually chose a mode
  assert(best_rd < INT64_MAX);
  return best_rd;
}

static void choose_intra_uv_mode(const AV1_COMP *const cpi, MACROBLOCK *const x,
                                 PICK_MODE_CONTEXT *ctx, BLOCK_SIZE bsize,
                                 TX_SIZE max_tx_size, int *rate_uv,
                                 int *rate_uv_tokenonly, int64_t *dist_uv,
                                 int *skip_uv, PREDICTION_MODE *mode_uv) {
  // Use an estimated rd for uv_intra based on DC_PRED if the
  // appropriate speed flag is set.
  (void)ctx;
  init_sbuv_mode(&x->e_mbd.mi[0]->mbmi);
#if CONFIG_CB4X4
#if CONFIG_CHROMA_2X2
  rd_pick_intra_sbuv_mode(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                          bsize, max_tx_size);
#else
  if (x->skip_chroma_rd) {
    *rate_uv = 0;
    *rate_uv_tokenonly = 0;
    *dist_uv = 0;
    *skip_uv = 1;
    *mode_uv = DC_PRED;
    return;
  }
  BLOCK_SIZE bs = scale_chroma_bsize(bsize, x->e_mbd.plane[1].subsampling_x,
                                     x->e_mbd.plane[1].subsampling_y);
  rd_pick_intra_sbuv_mode(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                          bs, max_tx_size);
#endif  // CONFIG_CHROMA_2X2
#else
  rd_pick_intra_sbuv_mode(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                          bsize < BLOCK_8X8 ? BLOCK_8X8 : bsize, max_tx_size);
#endif  // CONFIG_CB4X4
  *mode_uv = x->e_mbd.mi[0]->mbmi.uv_mode;
}

static int cost_mv_ref(const AV1_COMP *const cpi, PREDICTION_MODE mode,
                       int16_t mode_context) {
#if CONFIG_EXT_INTER
  if (is_inter_compound_mode(mode)) {
    return cpi
        ->inter_compound_mode_cost[mode_context][INTER_COMPOUND_OFFSET(mode)];
  }
#endif

  int mode_cost = 0;
  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;
  int16_t is_all_zero_mv = mode_context & (1 << ALL_ZERO_FLAG_OFFSET);

  assert(is_inter_mode(mode));

  if (mode == NEWMV) {
    mode_cost = cpi->newmv_mode_cost[mode_ctx][0];
    return mode_cost;
  } else {
    mode_cost = cpi->newmv_mode_cost[mode_ctx][1];
    mode_ctx = (mode_context >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;

    if (is_all_zero_mv) return mode_cost;

    if (mode == ZEROMV) {
      mode_cost += cpi->zeromv_mode_cost[mode_ctx][0];
      return mode_cost;
    } else {
      mode_cost += cpi->zeromv_mode_cost[mode_ctx][1];
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;

      if (mode_context & (1 << SKIP_NEARESTMV_OFFSET)) mode_ctx = 6;
      if (mode_context & (1 << SKIP_NEARMV_OFFSET)) mode_ctx = 7;
      if (mode_context & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) mode_ctx = 8;

      mode_cost += cpi->refmv_mode_cost[mode_ctx][mode != NEARESTMV];
      return mode_cost;
    }
  }
}

#if CONFIG_EXT_INTER && (CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT)
static int get_interinter_compound_type_bits(BLOCK_SIZE bsize,
                                             COMPOUND_TYPE comp_type) {
  (void)bsize;
  switch (comp_type) {
    case COMPOUND_AVERAGE: return 0;
#if CONFIG_WEDGE
    case COMPOUND_WEDGE: return get_interinter_wedge_bits(bsize);
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
    case COMPOUND_SEG: return 1;
#endif  // CONFIG_COMPOUND_SEGMENT
    default: assert(0); return 0;
  }
}
#endif  // CONFIG_EXT_INTER && (CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT)

typedef struct {
  int eobs;
  int brate;
  int byrate;
  int64_t bdist;
  int64_t bsse;
  int64_t brdcost;
  int_mv mvs[2];
  int_mv pred_mv[2];
#if CONFIG_EXT_INTER
  int_mv ref_mv[2];
#endif  // CONFIG_EXT_INTER

#if CONFIG_CHROMA_2X2
  ENTROPY_CONTEXT ta[4];
  ENTROPY_CONTEXT tl[4];
#else
  ENTROPY_CONTEXT ta[2];
  ENTROPY_CONTEXT tl[2];
#endif  // CONFIG_CHROMA_2X2
} SEG_RDSTAT;

typedef struct {
  int_mv *ref_mv[2];
  int_mv mvp;

  int64_t segment_rd;
  int r;
  int64_t d;
  int64_t sse;
  int segment_yrate;
  PREDICTION_MODE modes[4];
#if CONFIG_EXT_INTER
  SEG_RDSTAT rdstat[4][INTER_MODES + INTER_COMPOUND_MODES];
#else
  SEG_RDSTAT rdstat[4][INTER_MODES];
#endif  // CONFIG_EXT_INTER
  int mvthresh;
} BEST_SEG_INFO;

static INLINE int mv_check_bounds(const MvLimits *mv_limits, const MV *mv) {
  return (mv->row >> 3) < mv_limits->row_min ||
         (mv->row >> 3) > mv_limits->row_max ||
         (mv->col >> 3) < mv_limits->col_min ||
         (mv->col >> 3) > mv_limits->col_max;
}

// Check if NEARESTMV/NEARMV/ZEROMV is the cheapest way encode zero motion.
// TODO(aconverse): Find out if this is still productive then clean up or remove
static int check_best_zero_mv(
    const AV1_COMP *const cpi, const int16_t mode_context[TOTAL_REFS_PER_FRAME],
#if CONFIG_EXT_INTER
    const int16_t compound_mode_context[TOTAL_REFS_PER_FRAME],
#endif  // CONFIG_EXT_INTER
    int_mv frame_mv[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME], int this_mode,
    const MV_REFERENCE_FRAME ref_frames[2], const BLOCK_SIZE bsize, int block,
    int mi_row, int mi_col) {
  int_mv zeromv[2];
  int comp_pred_mode = ref_frames[1] > INTRA_FRAME;
  int cur_frm;
  (void)mi_row;
  (void)mi_col;
  for (cur_frm = 0; cur_frm < 1 + comp_pred_mode; cur_frm++) {
#if CONFIG_GLOBAL_MOTION
    if (this_mode == ZEROMV
#if CONFIG_EXT_INTER
        || this_mode == ZERO_ZEROMV
#endif  // CONFIG_EXT_INTER
        )
      zeromv[cur_frm].as_int =
          gm_get_motion_vector(&cpi->common.global_motion[ref_frames[cur_frm]],
                               cpi->common.allow_high_precision_mv, bsize,
                               mi_col, mi_row, block)
              .as_int;
    else
#endif  // CONFIG_GLOBAL_MOTION
      zeromv[cur_frm].as_int = 0;
  }
#if !CONFIG_EXT_INTER
  assert(ref_frames[1] != INTRA_FRAME);  // Just sanity check
#endif                                   // !CONFIG_EXT_INTER
  if ((this_mode == NEARMV || this_mode == NEARESTMV || this_mode == ZEROMV) &&
      frame_mv[this_mode][ref_frames[0]].as_int == zeromv[0].as_int &&
      (ref_frames[1] <= INTRA_FRAME ||
       frame_mv[this_mode][ref_frames[1]].as_int == zeromv[1].as_int)) {
    int16_t rfc =
        av1_mode_context_analyzer(mode_context, ref_frames, bsize, block);
    int c1 = cost_mv_ref(cpi, NEARMV, rfc);
    int c2 = cost_mv_ref(cpi, NEARESTMV, rfc);
    int c3 = cost_mv_ref(cpi, ZEROMV, rfc);

    if (this_mode == NEARMV) {
      if (c1 > c3) return 0;
    } else if (this_mode == NEARESTMV) {
      if (c2 > c3) return 0;
    } else {
      assert(this_mode == ZEROMV);
      if (ref_frames[1] <= INTRA_FRAME) {
        if ((c3 >= c2 && frame_mv[NEARESTMV][ref_frames[0]].as_int == 0) ||
            (c3 >= c1 && frame_mv[NEARMV][ref_frames[0]].as_int == 0))
          return 0;
      } else {
        if ((c3 >= c2 && frame_mv[NEARESTMV][ref_frames[0]].as_int == 0 &&
             frame_mv[NEARESTMV][ref_frames[1]].as_int == 0) ||
            (c3 >= c1 && frame_mv[NEARMV][ref_frames[0]].as_int == 0 &&
             frame_mv[NEARMV][ref_frames[1]].as_int == 0))
          return 0;
      }
    }
  }
#if CONFIG_EXT_INTER
  else if ((this_mode == NEAREST_NEARESTMV || this_mode == NEAR_NEARMV ||
            this_mode == ZERO_ZEROMV) &&
           frame_mv[this_mode][ref_frames[0]].as_int == zeromv[0].as_int &&
           frame_mv[this_mode][ref_frames[1]].as_int == zeromv[1].as_int) {
    int16_t rfc = compound_mode_context[ref_frames[0]];
    int c2 = cost_mv_ref(cpi, NEAREST_NEARESTMV, rfc);
    int c3 = cost_mv_ref(cpi, ZERO_ZEROMV, rfc);
    int c5 = cost_mv_ref(cpi, NEAR_NEARMV, rfc);

    if (this_mode == NEAREST_NEARESTMV) {
      if (c2 > c3) return 0;
    } else if (this_mode == NEAR_NEARMV) {
      if (c5 > c3) return 0;
    } else {
      assert(this_mode == ZERO_ZEROMV);
      if ((c3 >= c2 && frame_mv[NEAREST_NEARESTMV][ref_frames[0]].as_int == 0 &&
           frame_mv[NEAREST_NEARESTMV][ref_frames[1]].as_int == 0) ||
          (c3 >= c5 && frame_mv[NEAR_NEARMV][ref_frames[0]].as_int == 0 &&
           frame_mv[NEAR_NEARMV][ref_frames[1]].as_int == 0))
        return 0;
    }
  }
#endif  // CONFIG_EXT_INTER
  return 1;
}

static void joint_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                BLOCK_SIZE bsize, int_mv *frame_mv, int mi_row,
                                int mi_col,
#if CONFIG_EXT_INTER
                                int_mv *ref_mv_sub8x8[2], const uint8_t *mask,
                                int mask_stride,
#endif  // CONFIG_EXT_INTER
                                int *rate_mv, const int block) {
  const AV1_COMMON *const cm = &cpi->common;
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));
  const int refs[2] = { mbmi->ref_frame[0], mbmi->ref_frame[1] };
  int_mv ref_mv[2];
  int ite, ref;
#if CONFIG_DUAL_FILTER
  InterpFilter interp_filter[4] = {
    mbmi->interp_filter[0], mbmi->interp_filter[1], mbmi->interp_filter[2],
    mbmi->interp_filter[3],
  };
#else
  const InterpFilter interp_filter = mbmi->interp_filter;
#endif  // CONFIG_DUAL_FILTER
  struct scale_factors sf;
  struct macroblockd_plane *const pd = &xd->plane[0];
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
  // ic and ir are the 4x4 coordiantes of the sub8x8 at index "block"
  const int ic = block & 1;
  const int ir = (block - ic) >> 1;
  const int p_col = ((mi_col * MI_SIZE) >> pd->subsampling_x) + 4 * ic;
  const int p_row = ((mi_row * MI_SIZE) >> pd->subsampling_y) + 4 * ir;
#if CONFIG_GLOBAL_MOTION
  int is_global[2];
  for (ref = 0; ref < 2; ++ref) {
    WarpedMotionParams *const wm =
        &xd->global_motion[xd->mi[0]->mbmi.ref_frame[ref]];
    is_global[ref] = is_global_mv_block(xd->mi[0], block, wm->wmtype);
  }
#endif  // CONFIG_GLOBAL_MOTION
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION

  // Do joint motion search in compound mode to get more accurate mv.
  struct buf_2d backup_yv12[2][MAX_MB_PLANE];
  int last_besterr[2] = { INT_MAX, INT_MAX };
  const YV12_BUFFER_CONFIG *const scaled_ref_frame[2] = {
    av1_get_scaled_ref_frame(cpi, refs[0]),
    av1_get_scaled_ref_frame(cpi, refs[1])
  };

// Prediction buffer from second frame.
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[MAX_SB_SQUARE]);
  uint8_t *second_pred;
#else
  DECLARE_ALIGNED(16, uint8_t, second_pred[MAX_SB_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_EXT_INTER && CONFIG_CB4X4
  (void)ref_mv_sub8x8;
#endif  // CONFIG_EXT_INTER && CONFIG_CB4X4

  for (ref = 0; ref < 2; ++ref) {
#if CONFIG_EXT_INTER && !CONFIG_CB4X4
    if (bsize < BLOCK_8X8 && ref_mv_sub8x8 != NULL)
      ref_mv[ref].as_int = ref_mv_sub8x8[ref]->as_int;
    else
#endif  // CONFIG_EXT_INTER && !CONFIG_CB4X4
      ref_mv[ref] = x->mbmi_ext->ref_mvs[refs[ref]][0];

    if (scaled_ref_frame[ref]) {
      int i;
      // Swap out the reference frame for a version that's been scaled to
      // match the resolution of the current frame, allowing the existing
      // motion search code to be used without additional modifications.
      for (i = 0; i < MAX_MB_PLANE; i++)
        backup_yv12[ref][i] = xd->plane[i].pre[ref];
      av1_setup_pre_planes(xd, ref, scaled_ref_frame[ref], mi_row, mi_col,
                           NULL);
    }
  }

// Since we have scaled the reference frames to match the size of the current
// frame we must use a unit scaling factor during mode selection.
#if CONFIG_HIGHBITDEPTH
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height, cm->use_highbitdepth);
#else
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height);
#endif  // CONFIG_HIGHBITDEPTH

  // Allow joint search multiple times iteratively for each reference frame
  // and break out of the search loop if it couldn't find a better mv.
  for (ite = 0; ite < 4; ite++) {
    struct buf_2d ref_yv12[2];
    int bestsme = INT_MAX;
    int sadpb = x->sadperbit16;
    MV *const best_mv = &x->best_mv.as_mv;
    int search_range = 3;

    MvLimits tmp_mv_limits = x->mv_limits;
    int id = ite % 2;  // Even iterations search in the first reference frame,
                       // odd iterations search in the second. The predictor
                       // found for the 'other' reference frame is factored in.
    const int plane = 0;
    ConvolveParams conv_params = get_conv_params(0, plane);
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
    WarpTypesAllowed warp_types;
#if CONFIG_GLOBAL_MOTION
    warp_types.global_warp_allowed = is_global[!id];
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
    warp_types.local_warp_allowed = mbmi->motion_mode == WARPED_CAUSAL;
#endif  // CONFIG_WARPED_MOTION
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION

    // Initialized here because of compiler problem in Visual Studio.
    ref_yv12[0] = xd->plane[plane].pre[0];
    ref_yv12[1] = xd->plane[plane].pre[1];

#if CONFIG_DUAL_FILTER
    // reload the filter types
    interp_filter[0] =
        (id == 0) ? mbmi->interp_filter[2] : mbmi->interp_filter[0];
    interp_filter[1] =
        (id == 0) ? mbmi->interp_filter[3] : mbmi->interp_filter[1];
#endif  // CONFIG_DUAL_FILTER

// Get the prediction block from the 'other' reference frame.
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
      av1_highbd_build_inter_predictor(
          ref_yv12[!id].buf, ref_yv12[!id].stride, second_pred, pw,
          &frame_mv[refs[!id]].as_mv, &sf, pw, ph, 0, interp_filter,
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
          &warp_types, p_col, p_row,
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
          plane, MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd);
    } else {
      second_pred = (uint8_t *)second_pred_alloc_16;
#endif  // CONFIG_HIGHBITDEPTH
      av1_build_inter_predictor(
          ref_yv12[!id].buf, ref_yv12[!id].stride, second_pred, pw,
          &frame_mv[refs[!id]].as_mv, &sf, pw, ph, &conv_params, interp_filter,
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
          &warp_types, p_col, p_row, plane, !id,
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
          MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd);
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH

    // Do compound motion search on the current reference frame.
    if (id) xd->plane[plane].pre[0] = ref_yv12[id];
    av1_set_mv_search_range(&x->mv_limits, &ref_mv[id].as_mv);

    // Use the mv result from the single mode as mv predictor.
    *best_mv = frame_mv[refs[id]].as_mv;

    best_mv->col >>= 3;
    best_mv->row >>= 3;

    av1_set_mvcost(x, refs[id], id, mbmi->ref_mv_idx);

    // Small-range full-pixel motion search.
    bestsme =
        av1_refining_search_8p_c(x, sadpb, search_range, &cpi->fn_ptr[bsize],
#if CONFIG_EXT_INTER
                                 mask, mask_stride, id,
#endif
                                 &ref_mv[id].as_mv, second_pred);
    if (bestsme < INT_MAX) {
#if CONFIG_EXT_INTER
      if (mask)
        bestsme = av1_get_mvpred_mask_var(x, best_mv, &ref_mv[id].as_mv,
                                          second_pred, mask, mask_stride, id,
                                          &cpi->fn_ptr[bsize], 1);
      else
#endif
        bestsme = av1_get_mvpred_av_var(x, best_mv, &ref_mv[id].as_mv,
                                        second_pred, &cpi->fn_ptr[bsize], 1);
    }

    x->mv_limits = tmp_mv_limits;

    if (bestsme < INT_MAX) {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;
      if (cpi->sf.use_upsampled_references) {
        // Use up-sampled reference frames.
        struct buf_2d backup_pred = pd->pre[0];
        const YV12_BUFFER_CONFIG *upsampled_ref =
            get_upsampled_ref(cpi, refs[id]);

        // Set pred for Y plane
        setup_pred_plane(&pd->pre[0], bsize, upsampled_ref->y_buffer,
                         upsampled_ref->y_crop_width,
                         upsampled_ref->y_crop_height, upsampled_ref->y_stride,
                         (mi_row << 3), (mi_col << 3), NULL, pd->subsampling_x,
                         pd->subsampling_y);

// If bsize < BLOCK_8X8, adjust pred pointer for this block
#if !CONFIG_CB4X4
        if (bsize < BLOCK_8X8)
          pd->pre[0].buf =
              &pd->pre[0].buf[(av1_raster_block_offset(BLOCK_8X8, block,
                                                       pd->pre[0].stride))
                              << 3];
#endif  // !CONFIG_CB4X4

        bestsme = cpi->find_fractional_mv_step(
            x, &ref_mv[id].as_mv, cpi->common.allow_high_precision_mv,
            x->errorperbit, &cpi->fn_ptr[bsize], 0,
            cpi->sf.mv.subpel_iters_per_step, NULL, x->nmvjointcost, x->mvcost,
            &dis, &sse, second_pred,
#if CONFIG_EXT_INTER
            mask, mask_stride, id,
#endif
            pw, ph, 1);

        // Restore the reference frames.
        pd->pre[0] = backup_pred;
      } else {
        (void)block;
        bestsme = cpi->find_fractional_mv_step(
            x, &ref_mv[id].as_mv, cpi->common.allow_high_precision_mv,
            x->errorperbit, &cpi->fn_ptr[bsize], 0,
            cpi->sf.mv.subpel_iters_per_step, NULL, x->nmvjointcost, x->mvcost,
            &dis, &sse, second_pred,
#if CONFIG_EXT_INTER
            mask, mask_stride, id,
#endif
            pw, ph, 0);
      }
    }

    // Restore the pointer to the first (possibly scaled) prediction buffer.
    if (id) xd->plane[plane].pre[0] = ref_yv12[0];

    if (bestsme < last_besterr[id]) {
      frame_mv[refs[id]].as_mv = *best_mv;
      last_besterr[id] = bestsme;
    } else {
      break;
    }
  }

  *rate_mv = 0;

  for (ref = 0; ref < 2; ++ref) {
    if (scaled_ref_frame[ref]) {
      // Restore the prediction frame pointers to their unscaled versions.
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[ref] = backup_yv12[ref][i];
    }
    av1_set_mvcost(x, refs[ref], ref, mbmi->ref_mv_idx);
#if CONFIG_EXT_INTER && !CONFIG_CB4X4
    if (bsize >= BLOCK_8X8)
#endif  // CONFIG_EXT_INTER && !CONFIG_CB4X4
      *rate_mv += av1_mv_bit_cost(&frame_mv[refs[ref]].as_mv,
                                  &x->mbmi_ext->ref_mvs[refs[ref]][0].as_mv,
                                  x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
#if CONFIG_EXT_INTER && !CONFIG_CB4X4
    else
      *rate_mv += av1_mv_bit_cost(&frame_mv[refs[ref]].as_mv,
                                  &ref_mv_sub8x8[ref]->as_mv, x->nmvjointcost,
                                  x->mvcost, MV_COST_WEIGHT);
#endif  // CONFIG_EXT_INTER && !CONFIG_CB4X4
  }
}

static void estimate_ref_frame_costs(const AV1_COMMON *cm,
                                     const MACROBLOCKD *xd, int segment_id,
                                     unsigned int *ref_costs_single,
                                     unsigned int *ref_costs_comp,
                                     aom_prob *comp_mode_p) {
  int seg_ref_active =
      segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME);
  if (seg_ref_active) {
    memset(ref_costs_single, 0,
           TOTAL_REFS_PER_FRAME * sizeof(*ref_costs_single));
    memset(ref_costs_comp, 0, TOTAL_REFS_PER_FRAME * sizeof(*ref_costs_comp));
    *comp_mode_p = 128;
  } else {
    aom_prob intra_inter_p = av1_get_intra_inter_prob(cm, xd);
    aom_prob comp_inter_p = 128;

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      comp_inter_p = av1_get_reference_mode_prob(cm, xd);
      *comp_mode_p = comp_inter_p;
    } else {
      *comp_mode_p = 128;
    }

    ref_costs_single[INTRA_FRAME] = av1_cost_bit(intra_inter_p, 0);

    if (cm->reference_mode != COMPOUND_REFERENCE) {
      aom_prob ref_single_p1 = av1_get_pred_prob_single_ref_p1(cm, xd);
      aom_prob ref_single_p2 = av1_get_pred_prob_single_ref_p2(cm, xd);
#if CONFIG_EXT_REFS
      aom_prob ref_single_p3 = av1_get_pred_prob_single_ref_p3(cm, xd);
      aom_prob ref_single_p4 = av1_get_pred_prob_single_ref_p4(cm, xd);
      aom_prob ref_single_p5 = av1_get_pred_prob_single_ref_p5(cm, xd);
#endif  // CONFIG_EXT_REFS

      unsigned int base_cost = av1_cost_bit(intra_inter_p, 1);

      ref_costs_single[LAST_FRAME] =
#if CONFIG_EXT_REFS
          ref_costs_single[LAST2_FRAME] = ref_costs_single[LAST3_FRAME] =
              ref_costs_single[BWDREF_FRAME] =
#endif  // CONFIG_EXT_REFS
                  ref_costs_single[GOLDEN_FRAME] =
                      ref_costs_single[ALTREF_FRAME] = base_cost;

#if CONFIG_EXT_REFS
      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[BWDREF_FRAME] += av1_cost_bit(ref_single_p1, 1);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p1, 1);

      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p3, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p3, 0);
      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p3, 1);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p3, 1);

      ref_costs_single[BWDREF_FRAME] += av1_cost_bit(ref_single_p2, 0);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p2, 1);

      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p4, 0);
      ref_costs_single[LAST2_FRAME] += av1_cost_bit(ref_single_p4, 1);

      ref_costs_single[LAST3_FRAME] += av1_cost_bit(ref_single_p5, 0);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p5, 1);
#else
      ref_costs_single[LAST_FRAME] += av1_cost_bit(ref_single_p1, 0);
      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p1, 1);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p1, 1);

      ref_costs_single[GOLDEN_FRAME] += av1_cost_bit(ref_single_p2, 0);
      ref_costs_single[ALTREF_FRAME] += av1_cost_bit(ref_single_p2, 1);
#endif  // CONFIG_EXT_REFS
    } else {
      ref_costs_single[LAST_FRAME] = 512;
#if CONFIG_EXT_REFS
      ref_costs_single[LAST2_FRAME] = 512;
      ref_costs_single[LAST3_FRAME] = 512;
      ref_costs_single[BWDREF_FRAME] = 512;
#endif  // CONFIG_EXT_REFS
      ref_costs_single[GOLDEN_FRAME] = 512;
      ref_costs_single[ALTREF_FRAME] = 512;
    }

    if (cm->reference_mode != SINGLE_REFERENCE) {
      aom_prob ref_comp_p = av1_get_pred_prob_comp_ref_p(cm, xd);
#if CONFIG_EXT_REFS
      aom_prob ref_comp_p1 = av1_get_pred_prob_comp_ref_p1(cm, xd);
      aom_prob ref_comp_p2 = av1_get_pred_prob_comp_ref_p2(cm, xd);
      aom_prob bwdref_comp_p = av1_get_pred_prob_comp_bwdref_p(cm, xd);
#endif  // CONFIG_EXT_REFS

      unsigned int base_cost = av1_cost_bit(intra_inter_p, 1);

      ref_costs_comp[LAST_FRAME] =
#if CONFIG_EXT_REFS
          ref_costs_comp[LAST2_FRAME] = ref_costs_comp[LAST3_FRAME] =
#endif  // CONFIG_EXT_REFS
              ref_costs_comp[GOLDEN_FRAME] = base_cost;

#if CONFIG_EXT_REFS
      ref_costs_comp[BWDREF_FRAME] = ref_costs_comp[ALTREF_FRAME] = 0;
#endif  // CONFIG_EXT_REFS

#if CONFIG_EXT_REFS
      ref_costs_comp[LAST_FRAME] += av1_cost_bit(ref_comp_p, 0);
      ref_costs_comp[LAST2_FRAME] += av1_cost_bit(ref_comp_p, 0);
      ref_costs_comp[LAST3_FRAME] += av1_cost_bit(ref_comp_p, 1);
      ref_costs_comp[GOLDEN_FRAME] += av1_cost_bit(ref_comp_p, 1);

      ref_costs_comp[LAST_FRAME] += av1_cost_bit(ref_comp_p1, 1);
      ref_costs_comp[LAST2_FRAME] += av1_cost_bit(ref_comp_p1, 0);

      ref_costs_comp[LAST3_FRAME] += av1_cost_bit(ref_comp_p2, 0);
      ref_costs_comp[GOLDEN_FRAME] += av1_cost_bit(ref_comp_p2, 1);

      // NOTE(zoeliu): BWDREF and ALTREF each add an extra cost by coding 1
      //               more bit.
      ref_costs_comp[BWDREF_FRAME] += av1_cost_bit(bwdref_comp_p, 0);
      ref_costs_comp[ALTREF_FRAME] += av1_cost_bit(bwdref_comp_p, 1);
#else
      ref_costs_comp[LAST_FRAME] += av1_cost_bit(ref_comp_p, 0);
      ref_costs_comp[GOLDEN_FRAME] += av1_cost_bit(ref_comp_p, 1);
#endif  // CONFIG_EXT_REFS
    } else {
      ref_costs_comp[LAST_FRAME] = 512;
#if CONFIG_EXT_REFS
      ref_costs_comp[LAST2_FRAME] = 512;
      ref_costs_comp[LAST3_FRAME] = 512;
      ref_costs_comp[BWDREF_FRAME] = 512;
      ref_costs_comp[ALTREF_FRAME] = 512;
#endif  // CONFIG_EXT_REFS
      ref_costs_comp[GOLDEN_FRAME] = 512;
    }
  }
}

static void store_coding_context(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
                                 int mode_index,
                                 int64_t comp_pred_diff[REFERENCE_MODES],
                                 int skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;

  // Take a snapshot of the coding context so it can be
  // restored if we decide to encode this way
  ctx->skip = x->skip;
  ctx->skippable = skippable;
  ctx->best_mode_index = mode_index;
  ctx->mic = *xd->mi[0];
  ctx->mbmi_ext = *x->mbmi_ext;
  ctx->single_pred_diff = (int)comp_pred_diff[SINGLE_REFERENCE];
  ctx->comp_pred_diff = (int)comp_pred_diff[COMPOUND_REFERENCE];
  ctx->hybrid_pred_diff = (int)comp_pred_diff[REFERENCE_MODE_SELECT];
}

static void setup_buffer_inter(
    const AV1_COMP *const cpi, MACROBLOCK *x, MV_REFERENCE_FRAME ref_frame,
    BLOCK_SIZE block_size, int mi_row, int mi_col,
    int_mv frame_nearest_mv[TOTAL_REFS_PER_FRAME],
    int_mv frame_near_mv[TOTAL_REFS_PER_FRAME],
    struct buf_2d yv12_mb[TOTAL_REFS_PER_FRAME][MAX_MB_PLANE]) {
  const AV1_COMMON *cm = &cpi->common;
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_buffer(cpi, ref_frame);
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mi = xd->mi[0];
  int_mv *const candidates = x->mbmi_ext->ref_mvs[ref_frame];
  const struct scale_factors *const sf = &cm->frame_refs[ref_frame - 1].sf;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;

  assert(yv12 != NULL);

  // TODO(jkoleszar): Is the UV buffer ever used here? If so, need to make this
  // use the UV scaling factors.
  av1_setup_pred_block(xd, yv12_mb[ref_frame], yv12, mi_row, mi_col, sf, sf);

  // Gets an initial list of candidate vectors from neighbours and orders them
  av1_find_mv_refs(cm, xd, mi, ref_frame, &mbmi_ext->ref_mv_count[ref_frame],
                   mbmi_ext->ref_mv_stack[ref_frame],
#if CONFIG_EXT_INTER
                   mbmi_ext->compound_mode_context,
#endif  // CONFIG_EXT_INTER
                   candidates, mi_row, mi_col, NULL, NULL,
                   mbmi_ext->mode_context);

  // Candidate refinement carried out at encoder and decoder
  av1_find_best_ref_mvs(cm->allow_high_precision_mv, candidates,
                        &frame_nearest_mv[ref_frame],
                        &frame_near_mv[ref_frame]);

// Further refinement that is encode side only to test the top few candidates
// in full and choose the best as the centre point for subsequent searches.
// The current implementation doesn't support scaling.
#if CONFIG_CB4X4
  av1_mv_pred(cpi, x, yv12_mb[ref_frame][0].buf, yv12->y_stride, ref_frame,
              block_size);
#else
  if (!av1_is_scaled(sf) && block_size >= BLOCK_8X8)
    av1_mv_pred(cpi, x, yv12_mb[ref_frame][0].buf, yv12->y_stride, ref_frame,
                block_size);
#endif  // CONFIG_CB4X4
}

static void single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize, int mi_row, int mi_col,
#if CONFIG_EXT_INTER
                                 int ref_idx,
#endif  // CONFIG_EXT_INTER
                                 int *rate_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  const AV1_COMMON *cm = &cpi->common;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0, 0, 0, 0 } };
  int bestsme = INT_MAX;
  int step_param;
  int sadpb = x->sadperbit16;
  MV mvp_full;
#if CONFIG_EXT_INTER
  int ref = mbmi->ref_frame[ref_idx];
#else
  int ref = mbmi->ref_frame[0];
  int ref_idx = 0;
#endif  // CONFIG_EXT_INTER
  MV ref_mv = x->mbmi_ext->ref_mvs[ref][0].as_mv;

  MvLimits tmp_mv_limits = x->mv_limits;
  int cost_list[5];

  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);

  MV pred_mv[3];
  pred_mv[0] = x->mbmi_ext->ref_mvs[ref][0].as_mv;
  pred_mv[1] = x->mbmi_ext->ref_mvs[ref][1].as_mv;
  pred_mv[2] = x->pred_mv[ref];

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[ref_idx];

    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  av1_set_mv_search_range(&x->mv_limits, &ref_mv);

  av1_set_mvcost(x, ref, ref_idx, mbmi->ref_mv_idx);

  // Work out the size of the first step in the mv step search.
  // 0 here is maximum length first step. 1 is AOMMAX >> 1 etc.
  if (cpi->sf.mv.auto_mv_step_size && cm->show_frame) {
    // Take wtd average of the step_params based on the last frame's
    // max mv magnitude and that based on the best ref mvs of the current
    // block for the given reference.
    step_param =
        (av1_init_search_range(x->max_mv_context[ref]) + cpi->mv_step_param) /
        2;
  } else {
    step_param = cpi->mv_step_param;
  }

  if (cpi->sf.adaptive_motion_search && bsize < cm->sb_size) {
    int boffset =
        2 * (b_width_log2_lookup[cm->sb_size] -
             AOMMIN(b_height_log2_lookup[bsize], b_width_log2_lookup[bsize]));
    step_param = AOMMAX(step_param, boffset);
  }

  if (cpi->sf.adaptive_motion_search) {
    int bwl = b_width_log2_lookup[bsize];
    int bhl = b_height_log2_lookup[bsize];
    int tlevel = x->pred_mv_sad[ref] >> (bwl + bhl + 4);

    if (tlevel < 5) step_param += 2;

    // prev_mv_sad is not setup for dynamically scaled frames.
    if (cpi->oxcf.resize_mode != RESIZE_DYNAMIC) {
      int i;
      for (i = LAST_FRAME; i <= ALTREF_FRAME && cm->show_frame; ++i) {
        if ((x->pred_mv_sad[ref] >> 3) > x->pred_mv_sad[i]) {
          x->pred_mv[ref].row = 0;
          x->pred_mv[ref].col = 0;
          x->best_mv.as_int = INVALID_MV;

          if (scaled_ref_frame) {
            int j;
            for (j = 0; j < MAX_MB_PLANE; ++j)
              xd->plane[j].pre[ref_idx] = backup_yv12[j];
          }
          return;
        }
      }
    }
  }

  av1_set_mv_search_range(&x->mv_limits, &ref_mv);

#if CONFIG_MOTION_VAR
  if (mbmi->motion_mode != SIMPLE_TRANSLATION)
    mvp_full = mbmi->mv[0].as_mv;
  else
#endif  // CONFIG_MOTION_VAR
    mvp_full = pred_mv[x->mv_best_ref_index[ref]];

  mvp_full.col >>= 3;
  mvp_full.row >>= 3;

  x->best_mv.as_int = x->second_best_mv.as_int = INVALID_MV;

#if CONFIG_MOTION_VAR
  switch (mbmi->motion_mode) {
    case SIMPLE_TRANSLATION:
#endif  // CONFIG_MOTION_VAR
      bestsme = av1_full_pixel_search(cpi, x, bsize, &mvp_full, step_param,
                                      sadpb, cond_cost_list(cpi, cost_list),
                                      &ref_mv, INT_MAX, 1);
#if CONFIG_MOTION_VAR
      break;
    case OBMC_CAUSAL:
      bestsme = av1_obmc_full_pixel_diamond(
          cpi, x, &mvp_full, step_param, sadpb,
          MAX_MVSEARCH_STEPS - 1 - step_param, 1, &cpi->fn_ptr[bsize], &ref_mv,
          &(x->best_mv.as_mv), 0);
      break;
    default: assert("Invalid motion mode!\n");
  }
#endif  // CONFIG_MOTION_VAR

  x->mv_limits = tmp_mv_limits;

  if (bestsme < INT_MAX) {
    int dis; /* TODO: use dis in distortion calculation later. */
#if CONFIG_MOTION_VAR
    switch (mbmi->motion_mode) {
      case SIMPLE_TRANSLATION:
#endif  // CONFIG_MOTION_VAR
        if (cpi->sf.use_upsampled_references) {
          int best_mv_var;
          const int try_second = x->second_best_mv.as_int != INVALID_MV &&
                                 x->second_best_mv.as_int != x->best_mv.as_int;
          const int pw = block_size_wide[bsize];
          const int ph = block_size_high[bsize];
          // Use up-sampled reference frames.
          struct macroblockd_plane *const pd = &xd->plane[0];
          struct buf_2d backup_pred = pd->pre[ref_idx];
          const YV12_BUFFER_CONFIG *upsampled_ref = get_upsampled_ref(cpi, ref);

          // Set pred for Y plane
          setup_pred_plane(
              &pd->pre[ref_idx], bsize, upsampled_ref->y_buffer,
              upsampled_ref->y_crop_width, upsampled_ref->y_crop_height,
              upsampled_ref->y_stride, (mi_row << 3), (mi_col << 3), NULL,
              pd->subsampling_x, pd->subsampling_y);

          best_mv_var = cpi->find_fractional_mv_step(
              x, &ref_mv, cm->allow_high_precision_mv, x->errorperbit,
              &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
              cpi->sf.mv.subpel_iters_per_step, cond_cost_list(cpi, cost_list),
              x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], NULL,
#if CONFIG_EXT_INTER
              NULL, 0, 0,
#endif
              pw, ph, 1);

          if (try_second) {
            const int minc =
                AOMMAX(x->mv_limits.col_min * 8, ref_mv.col - MV_MAX);
            const int maxc =
                AOMMIN(x->mv_limits.col_max * 8, ref_mv.col + MV_MAX);
            const int minr =
                AOMMAX(x->mv_limits.row_min * 8, ref_mv.row - MV_MAX);
            const int maxr =
                AOMMIN(x->mv_limits.row_max * 8, ref_mv.row + MV_MAX);
            int this_var;
            MV best_mv = x->best_mv.as_mv;

            x->best_mv = x->second_best_mv;
            if (x->best_mv.as_mv.row * 8 <= maxr &&
                x->best_mv.as_mv.row * 8 >= minr &&
                x->best_mv.as_mv.col * 8 <= maxc &&
                x->best_mv.as_mv.col * 8 >= minc) {
              this_var = cpi->find_fractional_mv_step(
                  x, &ref_mv, cm->allow_high_precision_mv, x->errorperbit,
                  &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
                  cpi->sf.mv.subpel_iters_per_step,
                  cond_cost_list(cpi, cost_list), x->nmvjointcost, x->mvcost,
                  &dis, &x->pred_sse[ref], NULL,
#if CONFIG_EXT_INTER
                  NULL, 0, 0,
#endif
                  pw, ph, 1);
              if (this_var < best_mv_var) best_mv = x->best_mv.as_mv;
              x->best_mv.as_mv = best_mv;
            }
          }

          // Restore the reference frames.
          pd->pre[ref_idx] = backup_pred;
        } else {
          cpi->find_fractional_mv_step(
              x, &ref_mv, cm->allow_high_precision_mv, x->errorperbit,
              &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
              cpi->sf.mv.subpel_iters_per_step, cond_cost_list(cpi, cost_list),
              x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], NULL,
#if CONFIG_EXT_INTER
              NULL, 0, 0,
#endif
              0, 0, 0);
        }
#if CONFIG_MOTION_VAR
        break;
      case OBMC_CAUSAL:
        av1_find_best_obmc_sub_pixel_tree_up(
            cpi, x, mi_row, mi_col, &x->best_mv.as_mv, &ref_mv,
            cm->allow_high_precision_mv, x->errorperbit, &cpi->fn_ptr[bsize],
            cpi->sf.mv.subpel_force_stop, cpi->sf.mv.subpel_iters_per_step,
            x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], 0,
            cpi->sf.use_upsampled_references);
        break;
      default: assert("Invalid motion mode!\n");
    }
#endif  // CONFIG_MOTION_VAR
  }
  *rate_mv = av1_mv_bit_cost(&x->best_mv.as_mv, &ref_mv, x->nmvjointcost,
                             x->mvcost, MV_COST_WEIGHT);

#if CONFIG_MOTION_VAR
  if (cpi->sf.adaptive_motion_search && mbmi->motion_mode == SIMPLE_TRANSLATION)
#else
  if (cpi->sf.adaptive_motion_search)
#endif  // CONFIG_MOTION_VAR
    x->pred_mv[ref] = x->best_mv.as_mv;

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
  }
}

static INLINE void restore_dst_buf(MACROBLOCKD *xd, BUFFER_SET dst) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].dst.buf = dst.plane[i];
    xd->plane[i].dst.stride = dst.stride[i];
  }
}

#if CONFIG_EXT_INTER
static void build_second_inter_pred(const AV1_COMP *cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, const MV *other_mv,
                                    int mi_row, int mi_col, const int block,
                                    int ref_idx, uint8_t *second_pred) {
  const AV1_COMMON *const cm = &cpi->common;
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int other_ref = mbmi->ref_frame[!ref_idx];
#if CONFIG_DUAL_FILTER
  InterpFilter interp_filter[2] = {
    (ref_idx == 0) ? mbmi->interp_filter[2] : mbmi->interp_filter[0],
    (ref_idx == 0) ? mbmi->interp_filter[3] : mbmi->interp_filter[1]
  };
#else
  const InterpFilter interp_filter = mbmi->interp_filter;
#endif  // CONFIG_DUAL_FILTER
  struct scale_factors sf;
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
  struct macroblockd_plane *const pd = &xd->plane[0];
  // ic and ir are the 4x4 coordiantes of the sub8x8 at index "block"
  const int ic = block & 1;
  const int ir = (block - ic) >> 1;
  const int p_col = ((mi_col * MI_SIZE) >> pd->subsampling_x) + 4 * ic;
  const int p_row = ((mi_row * MI_SIZE) >> pd->subsampling_y) + 4 * ir;
#if CONFIG_GLOBAL_MOTION
  WarpedMotionParams *const wm = &xd->global_motion[other_ref];
  int is_global = is_global_mv_block(xd->mi[0], block, wm->wmtype);
#endif  // CONFIG_GLOBAL_MOTION
#else
  (void)block;
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION

  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));

  struct buf_2d backup_yv12[MAX_MB_PLANE];
  const YV12_BUFFER_CONFIG *const scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, other_ref);

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[!ref_idx];
    av1_setup_pre_planes(xd, !ref_idx, scaled_ref_frame, mi_row, mi_col, NULL);
  }

// Since we have scaled the reference frames to match the size of the current
// frame we must use a unit scaling factor during mode selection.
#if CONFIG_HIGHBITDEPTH
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height, cm->use_highbitdepth);
#else
  av1_setup_scale_factors_for_frame(&sf, cm->width, cm->height, cm->width,
                                    cm->height);
#endif  // CONFIG_HIGHBITDEPTH

  struct buf_2d ref_yv12;

  const int plane = 0;
  ConvolveParams conv_params = get_conv_params(0, plane);
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
  WarpTypesAllowed warp_types;
#if CONFIG_GLOBAL_MOTION
  warp_types.global_warp_allowed = is_global;
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_WARPED_MOTION
  warp_types.local_warp_allowed = mbmi->motion_mode == WARPED_CAUSAL;
#endif  // CONFIG_WARPED_MOTION
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION

  // Initialized here because of compiler problem in Visual Studio.
  ref_yv12 = xd->plane[plane].pre[!ref_idx];

// Get the prediction block from the 'other' reference frame.
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    av1_highbd_build_inter_predictor(
        ref_yv12.buf, ref_yv12.stride, second_pred, pw, other_mv, &sf, pw, ph,
        0, interp_filter,
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
        &warp_types, p_col, p_row,
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
        plane, MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    av1_build_inter_predictor(
        ref_yv12.buf, ref_yv12.stride, second_pred, pw, other_mv, &sf, pw, ph,
        &conv_params, interp_filter,
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
        &warp_types, p_col, p_row, plane, !ref_idx,
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
        MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd);
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH

  if (scaled_ref_frame) {
    // Restore the prediction frame pointers to their unscaled versions.
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[!ref_idx] = backup_yv12[i];
  }
}

// Search for the best mv for one component of a compound,
// given that the other component is fixed.
static void compound_single_motion_search(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, MV *this_mv,
    int mi_row, int mi_col, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, int *rate_mv, const int block, int ref_idx) {
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int ref = mbmi->ref_frame[ref_idx];
  int_mv ref_mv = x->mbmi_ext->ref_mvs[ref][0];
  struct macroblockd_plane *const pd = &xd->plane[0];

  struct buf_2d backup_yv12[MAX_MB_PLANE];
  const YV12_BUFFER_CONFIG *const scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);

  // Check that this is either an interinter or an interintra block
  assert(has_second_ref(mbmi) ||
         (ref_idx == 0 && mbmi->ref_frame[1] == INTRA_FRAME));

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[ref_idx];
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  struct buf_2d orig_yv12;
  int bestsme = INT_MAX;
  int sadpb = x->sadperbit16;
  MV *const best_mv = &x->best_mv.as_mv;
  int search_range = 3;

  MvLimits tmp_mv_limits = x->mv_limits;

  // Initialized here because of compiler problem in Visual Studio.
  if (ref_idx) {
    orig_yv12 = pd->pre[0];
    pd->pre[0] = pd->pre[ref_idx];
  }

  // Do compound motion search on the current reference frame.
  av1_set_mv_search_range(&x->mv_limits, &ref_mv.as_mv);

  // Use the mv result from the single mode as mv predictor.
  *best_mv = *this_mv;

  best_mv->col >>= 3;
  best_mv->row >>= 3;

  av1_set_mvcost(x, ref, ref_idx, mbmi->ref_mv_idx);

  // Small-range full-pixel motion search.
  bestsme = av1_refining_search_8p_c(x, sadpb, search_range,
                                     &cpi->fn_ptr[bsize], mask, mask_stride,
                                     ref_idx, &ref_mv.as_mv, second_pred);
  if (bestsme < INT_MAX) {
    if (mask)
      bestsme =
          av1_get_mvpred_mask_var(x, best_mv, &ref_mv.as_mv, second_pred, mask,
                                  mask_stride, ref_idx, &cpi->fn_ptr[bsize], 1);
    else
      bestsme = av1_get_mvpred_av_var(x, best_mv, &ref_mv.as_mv, second_pred,
                                      &cpi->fn_ptr[bsize], 1);
  }

  x->mv_limits = tmp_mv_limits;

  if (bestsme < INT_MAX) {
    int dis; /* TODO: use dis in distortion calculation later. */
    unsigned int sse;
    if (cpi->sf.use_upsampled_references) {
      // Use up-sampled reference frames.
      struct buf_2d backup_pred = pd->pre[0];
      const YV12_BUFFER_CONFIG *upsampled_ref = get_upsampled_ref(cpi, ref);

      // Set pred for Y plane
      setup_pred_plane(&pd->pre[0], bsize, upsampled_ref->y_buffer,
                       upsampled_ref->y_crop_width,
                       upsampled_ref->y_crop_height, upsampled_ref->y_stride,
                       (mi_row << 3), (mi_col << 3), NULL, pd->subsampling_x,
                       pd->subsampling_y);

// If bsize < BLOCK_8X8, adjust pred pointer for this block
#if !CONFIG_CB4X4
      if (bsize < BLOCK_8X8)
        pd->pre[0].buf =
            &pd->pre[0].buf[(av1_raster_block_offset(BLOCK_8X8, block,
                                                     pd->pre[0].stride))
                            << 3];
#endif  // !CONFIG_CB4X4

      bestsme = cpi->find_fractional_mv_step(
          x, &ref_mv.as_mv, cpi->common.allow_high_precision_mv, x->errorperbit,
          &cpi->fn_ptr[bsize], 0, cpi->sf.mv.subpel_iters_per_step, NULL,
          x->nmvjointcost, x->mvcost, &dis, &sse, second_pred, mask,
          mask_stride, ref_idx, pw, ph, 1);

      // Restore the reference frames.
      pd->pre[0] = backup_pred;
    } else {
      (void)block;
      bestsme = cpi->find_fractional_mv_step(
          x, &ref_mv.as_mv, cpi->common.allow_high_precision_mv, x->errorperbit,
          &cpi->fn_ptr[bsize], 0, cpi->sf.mv.subpel_iters_per_step, NULL,
          x->nmvjointcost, x->mvcost, &dis, &sse, second_pred, mask,
          mask_stride, ref_idx, pw, ph, 0);
    }
  }

  // Restore the pointer to the first (possibly scaled) prediction buffer.
  if (ref_idx) pd->pre[0] = orig_yv12;

  if (bestsme < INT_MAX) *this_mv = *best_mv;

  *rate_mv = 0;

  if (scaled_ref_frame) {
    // Restore the prediction frame pointers to their unscaled versions.
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
  }

  av1_set_mvcost(x, ref, ref_idx, mbmi->ref_mv_idx);
  *rate_mv += av1_mv_bit_cost(this_mv, &ref_mv.as_mv, x->nmvjointcost,
                              x->mvcost, MV_COST_WEIGHT);
}

// Wrapper for compound_single_motion_search, for the common case
// where the second prediction is also an inter mode.
static void compound_single_motion_search_interinter(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int_mv *frame_mv,
    int mi_row, int mi_col, const uint8_t *mask, int mask_stride, int *rate_mv,
    const int block, int ref_idx) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));

// Prediction buffer from second frame.
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[MAX_SB_SQUARE]);
  uint8_t *second_pred;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
  else
    second_pred = (uint8_t *)second_pred_alloc_16;
#else
  DECLARE_ALIGNED(16, uint8_t, second_pred[MAX_SB_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH

  MV *this_mv = &frame_mv[mbmi->ref_frame[ref_idx]].as_mv;
  const MV *other_mv = &frame_mv[mbmi->ref_frame[!ref_idx]].as_mv;

  build_second_inter_pred(cpi, x, bsize, other_mv, mi_row, mi_col, block,
                          ref_idx, second_pred);

  compound_single_motion_search(cpi, x, bsize, this_mv, mi_row, mi_col,
                                second_pred, mask, mask_stride, rate_mv, block,
                                ref_idx);
}

#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
static void do_masked_motion_search_indexed(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE bsize,
    int mi_row, int mi_col, int_mv *tmp_mv, int *rate_mv, int which) {
  // NOTE: which values: 0 - 0 only, 1 - 1 only, 2 - both
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  BLOCK_SIZE sb_type = mbmi->sb_type;
  const uint8_t *mask;
  const int mask_stride = block_size_wide[bsize];

  mask = av1_get_compound_type_mask(comp_data, sb_type);

  int_mv frame_mv[TOTAL_REFS_PER_FRAME];
  MV_REFERENCE_FRAME rf[2] = { mbmi->ref_frame[0], mbmi->ref_frame[1] };
  assert(bsize >= BLOCK_8X8 || CONFIG_CB4X4);

  frame_mv[rf[0]].as_int = cur_mv[0].as_int;
  frame_mv[rf[1]].as_int = cur_mv[1].as_int;
  if (which == 0 || which == 1) {
    compound_single_motion_search_interinter(cpi, x, bsize, frame_mv, mi_row,
                                             mi_col, mask, mask_stride, rate_mv,
                                             0, which);
  } else if (which == 2) {
    joint_motion_search(cpi, x, bsize, frame_mv, mi_row, mi_col, NULL, mask,
                        mask_stride, rate_mv, 0);
  }
  tmp_mv[0].as_int = frame_mv[rf[0]].as_int;
  tmp_mv[1].as_int = frame_mv[rf[1]].as_int;
}
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
#endif  // CONFIG_EXT_INTER

// In some situations we want to discount tha pparent cost of a new motion
// vector. Where there is a subtle motion field and especially where there is
// low spatial complexity then it can be hard to cover the cost of a new motion
// vector in a single block, even if that motion vector reduces distortion.
// However, once established that vector may be usable through the nearest and
// near mv modes to reduce distortion in subsequent blocks and also improve
// visual quality.
static int discount_newmv_test(const AV1_COMP *const cpi, int this_mode,
                               int_mv this_mv,
                               int_mv (*mode_mv)[TOTAL_REFS_PER_FRAME],
                               int ref_frame) {
  return (!cpi->rc.is_src_frame_alt_ref && (this_mode == NEWMV) &&
          (this_mv.as_int != 0) &&
          ((mode_mv[NEARESTMV][ref_frame].as_int == 0) ||
           (mode_mv[NEARESTMV][ref_frame].as_int == INVALID_MV)) &&
          ((mode_mv[NEARMV][ref_frame].as_int == 0) ||
           (mode_mv[NEARMV][ref_frame].as_int == INVALID_MV)));
}

#define LEFT_TOP_MARGIN ((AOM_BORDER_IN_PIXELS - AOM_INTERP_EXTEND) << 3)
#define RIGHT_BOTTOM_MARGIN ((AOM_BORDER_IN_PIXELS - AOM_INTERP_EXTEND) << 3)

// TODO(jingning): this mv clamping function should be block size dependent.
static INLINE void clamp_mv2(MV *mv, const MACROBLOCKD *xd) {
  clamp_mv(mv, xd->mb_to_left_edge - LEFT_TOP_MARGIN,
           xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN,
           xd->mb_to_top_edge - LEFT_TOP_MARGIN,
           xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN);
}

#if CONFIG_EXT_INTER
#if CONFIG_WEDGE
static int estimate_wedge_sign(const AV1_COMP *cpi, const MACROBLOCK *x,
                               const BLOCK_SIZE bsize, const uint8_t *pred0,
                               int stride0, const uint8_t *pred1, int stride1) {
  const struct macroblock_plane *const p = &x->plane[0];
  const uint8_t *src = p->src.buf;
  int src_stride = p->src.stride;
  const int f_index = bsize - BLOCK_8X8;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  uint32_t esq[2][4];
  int64_t tl, br;

#if CONFIG_HIGHBITDEPTH
  if (x->e_mbd.cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    pred0 = CONVERT_TO_BYTEPTR(pred0);
    pred1 = CONVERT_TO_BYTEPTR(pred1);
  }
#endif  // CONFIG_HIGHBITDEPTH

  cpi->fn_ptr[f_index].vf(src, src_stride, pred0, stride0, &esq[0][0]);
  cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, pred0 + bw / 2, stride0,
                          &esq[0][1]);
  cpi->fn_ptr[f_index].vf(src + bh / 2 * src_stride, src_stride,
                          pred0 + bh / 2 * stride0, stride0, &esq[0][2]);
  cpi->fn_ptr[f_index].vf(src + bh / 2 * src_stride + bw / 2, src_stride,
                          pred0 + bh / 2 * stride0 + bw / 2, stride0,
                          &esq[0][3]);
  cpi->fn_ptr[f_index].vf(src, src_stride, pred1, stride1, &esq[1][0]);
  cpi->fn_ptr[f_index].vf(src + bw / 2, src_stride, pred1 + bw / 2, stride1,
                          &esq[1][1]);
  cpi->fn_ptr[f_index].vf(src + bh / 2 * src_stride, src_stride,
                          pred1 + bh / 2 * stride1, stride0, &esq[1][2]);
  cpi->fn_ptr[f_index].vf(src + bh / 2 * src_stride + bw / 2, src_stride,
                          pred1 + bh / 2 * stride1 + bw / 2, stride0,
                          &esq[1][3]);

  tl = (int64_t)(esq[0][0] + esq[0][1] + esq[0][2]) -
       (int64_t)(esq[1][0] + esq[1][1] + esq[1][2]);
  br = (int64_t)(esq[1][3] + esq[1][1] + esq[1][2]) -
       (int64_t)(esq[0][3] + esq[0][1] + esq[0][2]);
  return (tl + br > 0);
}
#endif  // CONFIG_WEDGE
#endif  // CONFIG_EXT_INTER

#if !CONFIG_DUAL_FILTER
static InterpFilter predict_interp_filter(
    const AV1_COMP *cpi, const MACROBLOCK *x, const BLOCK_SIZE bsize,
    const int mi_row, const int mi_col,
    InterpFilter (*single_filter)[TOTAL_REFS_PER_FRAME]) {
  InterpFilter best_filter = SWITCHABLE;
  const AV1_COMMON *cm = &cpi->common;
  const MACROBLOCKD *xd = &x->e_mbd;
  int bsl = mi_width_log2_lookup[bsize];
  int pred_filter_search =
      cpi->sf.cb_pred_filter_search
          ? (((mi_row + mi_col) >> bsl) +
             get_chessboard_index(cm->current_video_frame)) &
                0x1
          : 0;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int is_comp_pred = has_second_ref(mbmi);
  const int this_mode = mbmi->mode;
  int refs[2] = { mbmi->ref_frame[0],
                  (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  if (pred_filter_search) {
    InterpFilter af = SWITCHABLE, lf = SWITCHABLE;
    if (xd->up_available) af = xd->mi[-xd->mi_stride]->mbmi.interp_filter;
    if (xd->left_available) lf = xd->mi[-1]->mbmi.interp_filter;

#if CONFIG_EXT_INTER
    if ((this_mode != NEWMV && this_mode != NEW_NEWMV) || (af == lf))
#else
    if ((this_mode != NEWMV) || (af == lf))
#endif  // CONFIG_EXT_INTER
      best_filter = af;
  }
  if (is_comp_pred) {
    if (cpi->sf.adaptive_mode_search) {
#if CONFIG_EXT_INTER
      switch (this_mode) {
        case NEAREST_NEARESTMV:
          if (single_filter[NEARESTMV][refs[0]] ==
              single_filter[NEARESTMV][refs[1]])
            best_filter = single_filter[NEARESTMV][refs[0]];
          break;
        case NEAR_NEARMV:
          if (single_filter[NEARMV][refs[0]] == single_filter[NEARMV][refs[1]])
            best_filter = single_filter[NEARMV][refs[0]];
          break;
        case ZERO_ZEROMV:
          if (single_filter[ZEROMV][refs[0]] == single_filter[ZEROMV][refs[1]])
            best_filter = single_filter[ZEROMV][refs[0]];
          break;
        case NEW_NEWMV:
          if (single_filter[NEWMV][refs[0]] == single_filter[NEWMV][refs[1]])
            best_filter = single_filter[NEWMV][refs[0]];
          break;
        case NEAREST_NEWMV:
          if (single_filter[NEARESTMV][refs[0]] ==
              single_filter[NEWMV][refs[1]])
            best_filter = single_filter[NEARESTMV][refs[0]];
          break;
        case NEAR_NEWMV:
          if (single_filter[NEARMV][refs[0]] == single_filter[NEWMV][refs[1]])
            best_filter = single_filter[NEARMV][refs[0]];
          break;
        case NEW_NEARESTMV:
          if (single_filter[NEWMV][refs[0]] ==
              single_filter[NEARESTMV][refs[1]])
            best_filter = single_filter[NEWMV][refs[0]];
          break;
        case NEW_NEARMV:
          if (single_filter[NEWMV][refs[0]] == single_filter[NEARMV][refs[1]])
            best_filter = single_filter[NEWMV][refs[0]];
          break;
        default:
          if (single_filter[this_mode][refs[0]] ==
              single_filter[this_mode][refs[1]])
            best_filter = single_filter[this_mode][refs[0]];
          break;
      }
#else
      if (single_filter[this_mode][refs[0]] ==
          single_filter[this_mode][refs[1]])
        best_filter = single_filter[this_mode][refs[0]];
#endif  // CONFIG_EXT_INTER
    }
  }
  if (x->source_variance < cpi->sf.disable_filter_search_var_thresh) {
    best_filter = EIGHTTAP_REGULAR;
  }
  return best_filter;
}
#endif  // !CONFIG_DUAL_FILTER

#if CONFIG_EXT_INTER
// Choose the best wedge index and sign
#if CONFIG_WEDGE
static int64_t pick_wedge(const AV1_COMP *const cpi, const MACROBLOCK *const x,
                          const BLOCK_SIZE bsize, const uint8_t *const p0,
                          const uint8_t *const p1, int *const best_wedge_sign,
                          int *const best_wedge_index) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  int rate;
  int64_t dist;
  int64_t rd, best_rd = INT64_MAX;
  int wedge_index;
  int wedge_sign;
  int wedge_types = (1 << get_wedge_bits_lookup(bsize));
  const uint8_t *mask;
  uint64_t sse;
#if CONFIG_HIGHBITDEPTH
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
#else
  const int bd_round = 0;
#endif  // CONFIG_HIGHBITDEPTH

  DECLARE_ALIGNED(32, int16_t, r0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d10[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, ds[MAX_SB_SQUARE]);

  int64_t sign_limit;

#if CONFIG_HIGHBITDEPTH
  if (hbd) {
    aom_highbd_subtract_block(bh, bw, r0, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, r1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p1), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, d10, bw, CONVERT_TO_BYTEPTR(p1), bw,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
  } else  // NOLINT
#endif    // CONFIG_HIGHBITDEPTH
  {
    aom_subtract_block(bh, bw, r0, bw, src->buf, src->stride, p0, bw);
    aom_subtract_block(bh, bw, r1, bw, src->buf, src->stride, p1, bw);
    aom_subtract_block(bh, bw, d10, bw, p1, bw, p0, bw);
  }

  sign_limit = ((int64_t)aom_sum_squares_i16(r0, N) -
                (int64_t)aom_sum_squares_i16(r1, N)) *
               (1 << WEDGE_WEIGHT_BITS) / 2;

  if (N < 64)
    av1_wedge_compute_delta_squares_c(ds, r0, r1, N);
  else
    av1_wedge_compute_delta_squares(ds, r0, r1, N);

  for (wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    mask = av1_get_contiguous_soft_mask(wedge_index, 0, bsize);

    // TODO(jingning): Make sse2 functions support N = 16 case
    if (N < 64)
      wedge_sign = av1_wedge_sign_from_residuals_c(ds, mask, N, sign_limit);
    else
      wedge_sign = av1_wedge_sign_from_residuals(ds, mask, N, sign_limit);

    mask = av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
    if (N < 64)
      sse = av1_wedge_sse_from_residuals_c(r1, d10, mask, N);
    else
      sse = av1_wedge_sse_from_residuals(r1, d10, mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rd = RDCOST(x->rdmult, x->rddiv, rate, dist);

    if (rd < best_rd) {
      *best_wedge_index = wedge_index;
      *best_wedge_sign = wedge_sign;
      best_rd = rd;
    }
  }

  return best_rd;
}

// Choose the best wedge index the specified sign
static int64_t pick_wedge_fixed_sign(
    const AV1_COMP *const cpi, const MACROBLOCK *const x,
    const BLOCK_SIZE bsize, const uint8_t *const p0, const uint8_t *const p1,
    const int wedge_sign, int *const best_wedge_index) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  int rate;
  int64_t dist;
  int64_t rd, best_rd = INT64_MAX;
  int wedge_index;
  int wedge_types = (1 << get_wedge_bits_lookup(bsize));
  const uint8_t *mask;
  uint64_t sse;
#if CONFIG_HIGHBITDEPTH
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
#else
  const int bd_round = 0;
#endif  // CONFIG_HIGHBITDEPTH

  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d10[MAX_SB_SQUARE]);

#if CONFIG_HIGHBITDEPTH
  if (hbd) {
    aom_highbd_subtract_block(bh, bw, r1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p1), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, d10, bw, CONVERT_TO_BYTEPTR(p1), bw,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
  } else  // NOLINT
#endif    // CONFIG_HIGHBITDEPTH
  {
    aom_subtract_block(bh, bw, r1, bw, src->buf, src->stride, p1, bw);
    aom_subtract_block(bh, bw, d10, bw, p1, bw, p0, bw);
  }

  for (wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    mask = av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
    if (N < 64)
      sse = av1_wedge_sse_from_residuals_c(r1, d10, mask, N);
    else
      sse = av1_wedge_sse_from_residuals(r1, d10, mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rd = RDCOST(x->rdmult, x->rddiv, rate, dist);

    if (rd < best_rd) {
      *best_wedge_index = wedge_index;
      best_rd = rd;
    }
  }

  return best_rd;
}

static int64_t pick_interinter_wedge(const AV1_COMP *const cpi,
                                     MACROBLOCK *const x,
                                     const BLOCK_SIZE bsize,
                                     const uint8_t *const p0,
                                     const uint8_t *const p1) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int bw = block_size_wide[bsize];

  int64_t rd;
  int wedge_index = -1;
  int wedge_sign = 0;

  assert(is_interinter_compound_used(COMPOUND_WEDGE, bsize));
  assert(cpi->common.allow_masked_compound);

  if (cpi->sf.fast_wedge_sign_estimate) {
    wedge_sign = estimate_wedge_sign(cpi, x, bsize, p0, bw, p1, bw);
    rd = pick_wedge_fixed_sign(cpi, x, bsize, p0, p1, wedge_sign, &wedge_index);
  } else {
    rd = pick_wedge(cpi, x, bsize, p0, p1, &wedge_sign, &wedge_index);
  }

  mbmi->wedge_sign = wedge_sign;
  mbmi->wedge_index = wedge_index;
  return rd;
}
#endif  // CONFIG_WEDGE

#if CONFIG_COMPOUND_SEGMENT
static int64_t pick_interinter_seg(const AV1_COMP *const cpi,
                                   MACROBLOCK *const x, const BLOCK_SIZE bsize,
                                   const uint8_t *const p0,
                                   const uint8_t *const p1) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  int rate;
  uint64_t sse;
  int64_t dist;
  int64_t rd0;
  SEG_MASK_TYPE cur_mask_type;
  int64_t best_rd = INT64_MAX;
  SEG_MASK_TYPE best_mask_type = 0;
#if CONFIG_HIGHBITDEPTH
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
#else
  const int bd_round = 0;
#endif  // CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(32, int16_t, r0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d10[MAX_SB_SQUARE]);

#if CONFIG_HIGHBITDEPTH
  if (hbd) {
    aom_highbd_subtract_block(bh, bw, r0, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, r1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p1), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, d10, bw, CONVERT_TO_BYTEPTR(p1), bw,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
  } else  // NOLINT
#endif    // CONFIG_HIGHBITDEPTH
  {
    aom_subtract_block(bh, bw, r0, bw, src->buf, src->stride, p0, bw);
    aom_subtract_block(bh, bw, r1, bw, src->buf, src->stride, p1, bw);
    aom_subtract_block(bh, bw, d10, bw, p1, bw, p0, bw);
  }

  // try each mask type and its inverse
  for (cur_mask_type = 0; cur_mask_type < SEG_MASK_TYPES; cur_mask_type++) {
// build mask and inverse
#if CONFIG_HIGHBITDEPTH
    if (hbd)
      build_compound_seg_mask_highbd(
          xd->seg_mask, cur_mask_type, CONVERT_TO_BYTEPTR(p0), bw,
          CONVERT_TO_BYTEPTR(p1), bw, bsize, bh, bw, xd->bd);
    else
#endif  // CONFIG_HIGHBITDEPTH
      build_compound_seg_mask(xd->seg_mask, cur_mask_type, p0, bw, p1, bw,
                              bsize, bh, bw);

    // compute rd for mask
    sse = av1_wedge_sse_from_residuals(r1, d10, xd->seg_mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rd0 = RDCOST(x->rdmult, x->rddiv, rate, dist);

    if (rd0 < best_rd) {
      best_mask_type = cur_mask_type;
      best_rd = rd0;
    }
  }

  // make final mask
  mbmi->mask_type = best_mask_type;
#if CONFIG_HIGHBITDEPTH
  if (hbd)
    build_compound_seg_mask_highbd(
        xd->seg_mask, mbmi->mask_type, CONVERT_TO_BYTEPTR(p0), bw,
        CONVERT_TO_BYTEPTR(p1), bw, bsize, bh, bw, xd->bd);
  else
#endif  // CONFIG_HIGHBITDEPTH
    build_compound_seg_mask(xd->seg_mask, mbmi->mask_type, p0, bw, p1, bw,
                            bsize, bh, bw);

  return best_rd;
}
#endif  // CONFIG_COMPOUND_SEGMENT

#if CONFIG_WEDGE && CONFIG_INTERINTRA
static int64_t pick_interintra_wedge(const AV1_COMP *const cpi,
                                     const MACROBLOCK *const x,
                                     const BLOCK_SIZE bsize,
                                     const uint8_t *const p0,
                                     const uint8_t *const p1) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  int64_t rd;
  int wedge_index = -1;

  assert(is_interintra_wedge_used(bsize));
  assert(cpi->common.allow_interintra_compound);

  rd = pick_wedge_fixed_sign(cpi, x, bsize, p0, p1, 0, &wedge_index);

  mbmi->interintra_wedge_sign = 0;
  mbmi->interintra_wedge_index = wedge_index;
  return rd;
}
#endif  // CONFIG_WEDGE && CONFIG_INTERINTRA

#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
static int64_t pick_interinter_mask(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    const BLOCK_SIZE bsize,
                                    const uint8_t *const p0,
                                    const uint8_t *const p1) {
  const COMPOUND_TYPE compound_type =
      x->e_mbd.mi[0]->mbmi.interinter_compound_type;
  switch (compound_type) {
#if CONFIG_WEDGE
    case COMPOUND_WEDGE: return pick_interinter_wedge(cpi, x, bsize, p0, p1);
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
    case COMPOUND_SEG: return pick_interinter_seg(cpi, x, bsize, p0, p1);
#endif  // CONFIG_COMPOUND_SEGMENT
    default: assert(0); return 0;
  }
}

static int interinter_compound_motion_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const BLOCK_SIZE bsize, const int this_mode, int mi_row, int mi_col) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int_mv tmp_mv[2];
  int tmp_rate_mv = 0;
  const INTERINTER_COMPOUND_DATA compound_data = {
#if CONFIG_WEDGE
    mbmi->wedge_index,
    mbmi->wedge_sign,
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
    mbmi->mask_type,
    xd->seg_mask,
#endif  // CONFIG_COMPOUND_SEGMENT
    mbmi->interinter_compound_type
  };
  if (this_mode == NEW_NEWMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, &compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 2);
    mbmi->mv[0].as_int = tmp_mv[0].as_int;
    mbmi->mv[1].as_int = tmp_mv[1].as_int;
  } else if (this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, &compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 0);
    mbmi->mv[0].as_int = tmp_mv[0].as_int;
  } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, &compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 1);
    mbmi->mv[1].as_int = tmp_mv[1].as_int;
  }
  return tmp_rate_mv;
}

static int64_t build_and_cost_compound_type(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const BLOCK_SIZE bsize, const int this_mode, int rs2, int rate_mv,
    BUFFER_SET *ctx, int *out_rate_mv, uint8_t **preds0, uint8_t **preds1,
    int *strides, int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int rate_sum;
  int64_t dist_sum;
  int64_t best_rd_cur = INT64_MAX;
  int64_t rd = INT64_MAX;
  int tmp_skip_txfm_sb;
  int64_t tmp_skip_sse_sb;
  const COMPOUND_TYPE compound_type = mbmi->interinter_compound_type;

  best_rd_cur = pick_interinter_mask(cpi, x, bsize, *preds0, *preds1);
  best_rd_cur += RDCOST(x->rdmult, x->rddiv, rs2 + rate_mv, 0);

  if (have_newmv_in_inter_mode(this_mode) &&
      use_masked_motion_search(compound_type)) {
    *out_rate_mv = interinter_compound_motion_search(cpi, x, cur_mv, bsize,
                                                     this_mode, mi_row, mi_col);
    av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, ctx, bsize);
    model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                    &tmp_skip_txfm_sb, &tmp_skip_sse_sb);
    rd = RDCOST(x->rdmult, x->rddiv, rs2 + *out_rate_mv + rate_sum, dist_sum);
    if (rd >= best_rd_cur) {
      mbmi->mv[0].as_int = cur_mv[0].as_int;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
      *out_rate_mv = rate_mv;
      av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0,
#if CONFIG_SUPERTX
                                               0, 0,
#endif  // CONFIG_SUPERTX
                                               preds0, strides, preds1,
                                               strides);
    }
    av1_subtract_plane(x, bsize, 0);
    rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                             &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
    if (rd != INT64_MAX)
      rd = RDCOST(x->rdmult, x->rddiv, rs2 + *out_rate_mv + rate_sum, dist_sum);
    best_rd_cur = rd;

  } else {
    av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0,
#if CONFIG_SUPERTX
                                             0, 0,
#endif  // CONFIG_SUPERTX
                                             preds0, strides, preds1, strides);
    av1_subtract_plane(x, bsize, 0);
    rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                             &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
    if (rd != INT64_MAX)
      rd = RDCOST(x->rdmult, x->rddiv, rs2 + rate_mv + rate_sum, dist_sum);
    best_rd_cur = rd;
  }
  return best_rd_cur;
}
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
#endif  // CONFIG_EXT_INTER

typedef struct {
#if CONFIG_MOTION_VAR
  // Inter prediction buffers and respective strides
  uint8_t *above_pred_buf[MAX_MB_PLANE];
  int above_pred_stride[MAX_MB_PLANE];
  uint8_t *left_pred_buf[MAX_MB_PLANE];
  int left_pred_stride[MAX_MB_PLANE];
#endif  // CONFIG_MOTION_VAR
  int_mv *single_newmv;
#if CONFIG_EXT_INTER
  // Pointer to array of motion vectors to use for each ref and their rates
  // Should point to first of 2 arrays in 2D array
  int *single_newmv_rate;
  // Pointer to array of predicted rate-distortion
  // Should point to first of 2 arrays in 2D array
  int64_t (*modelled_rd)[TOTAL_REFS_PER_FRAME];
#endif  // CONFIG_EXT_INTER
  InterpFilter single_filter[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];
} HandleInterModeArgs;

static int64_t handle_newmv(const AV1_COMP *const cpi, MACROBLOCK *const x,
                            const BLOCK_SIZE bsize,
                            int_mv (*const mode_mv)[TOTAL_REFS_PER_FRAME],
                            const int mi_row, const int mi_col,
                            int *const rate_mv, int_mv *const single_newmv,
                            HandleInterModeArgs *const args) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;
#if CONFIG_EXT_INTER
  const int is_comp_interintra_pred = (mbmi->ref_frame[1] == INTRA_FRAME);
#endif  // CONFIG_EXT_INTER
  int_mv *const frame_mv = mode_mv[this_mode];
  const int refs[2] = { mbmi->ref_frame[0],
                        mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1] };
  int i;

  (void)args;

  if (is_comp_pred) {
#if CONFIG_EXT_INTER
    for (i = 0; i < 2; ++i) {
      single_newmv[refs[i]].as_int = args->single_newmv[refs[i]].as_int;
    }

    if (this_mode == NEW_NEWMV) {
      frame_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;
      frame_mv[refs[1]].as_int = single_newmv[refs[1]].as_int;

      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        joint_motion_search(cpi, x, bsize, frame_mv, mi_row, mi_col, NULL, NULL,
                            0, rate_mv, 0);
      } else {
        *rate_mv = 0;
        for (i = 0; i < 2; ++i) {
          av1_set_mvcost(x, refs[i], i, mbmi->ref_mv_idx);
          *rate_mv += av1_mv_bit_cost(
              &frame_mv[refs[i]].as_mv, &mbmi_ext->ref_mvs[refs[i]][0].as_mv,
              x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
        }
      }
    } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV) {
      frame_mv[refs[1]].as_int = single_newmv[refs[1]].as_int;
      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        frame_mv[refs[0]].as_int =
            mode_mv[compound_ref0_mode(this_mode)][refs[0]].as_int;
        compound_single_motion_search_interinter(
            cpi, x, bsize, frame_mv, mi_row, mi_col, NULL, 0, rate_mv, 0, 1);
      } else {
        av1_set_mvcost(x, refs[1], 1, mbmi->ref_mv_idx);
        *rate_mv = av1_mv_bit_cost(&frame_mv[refs[1]].as_mv,
                                   &mbmi_ext->ref_mvs[refs[1]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
    } else {
      assert(this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV);
      frame_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;
      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        frame_mv[refs[1]].as_int =
            mode_mv[compound_ref1_mode(this_mode)][refs[1]].as_int;
        compound_single_motion_search_interinter(
            cpi, x, bsize, frame_mv, mi_row, mi_col, NULL, 0, rate_mv, 0, 0);
      } else {
        av1_set_mvcost(x, refs[0], 0, mbmi->ref_mv_idx);
        *rate_mv = av1_mv_bit_cost(&frame_mv[refs[0]].as_mv,
                                   &mbmi_ext->ref_mvs[refs[0]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
    }
#else
    // Initialize mv using single prediction mode result.
    frame_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;
    frame_mv[refs[1]].as_int = single_newmv[refs[1]].as_int;

    if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
      joint_motion_search(cpi, x, bsize, frame_mv, mi_row, mi_col, rate_mv, 0);
    } else {
      *rate_mv = 0;
      for (i = 0; i < 2; ++i) {
        av1_set_mvcost(x, refs[i], i, mbmi->ref_mv_idx);
        *rate_mv += av1_mv_bit_cost(&frame_mv[refs[i]].as_mv,
                                    &mbmi_ext->ref_mvs[refs[i]][0].as_mv,
                                    x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
    }
#endif  // CONFIG_EXT_INTER
  } else {
#if CONFIG_EXT_INTER
    if (is_comp_interintra_pred) {
      x->best_mv = args->single_newmv[refs[0]];
      *rate_mv = args->single_newmv_rate[refs[0]];
    } else {
      single_motion_search(cpi, x, bsize, mi_row, mi_col, 0, rate_mv);
      args->single_newmv[refs[0]] = x->best_mv;
      args->single_newmv_rate[refs[0]] = *rate_mv;
    }
#else
    single_motion_search(cpi, x, bsize, mi_row, mi_col, rate_mv);
    single_newmv[refs[0]] = x->best_mv;
#endif  // CONFIG_EXT_INTER

    if (x->best_mv.as_int == INVALID_MV) return INT64_MAX;

    frame_mv[refs[0]] = x->best_mv;
    xd->mi[0]->bmi[0].as_mv[0] = x->best_mv;

    // Estimate the rate implications of a new mv but discount this
    // under certain circumstances where we want to help initiate a weak
    // motion field, where the distortion gain for a single block may not
    // be enough to overcome the cost of a new mv.
    if (discount_newmv_test(cpi, this_mode, x->best_mv, mode_mv, refs[0])) {
      *rate_mv = AOMMAX(*rate_mv / NEW_MV_DISCOUNT_FACTOR, 1);
    }
  }

  return 0;
}

int64_t interpolation_filter_search(
    MACROBLOCK *const x, const AV1_COMP *const cpi, BLOCK_SIZE bsize,
    int mi_row, int mi_col, const BUFFER_SET *const tmp_dst,
    BUFFER_SET *const orig_dst,
    InterpFilter (*const single_filter)[TOTAL_REFS_PER_FRAME],
    int64_t *const rd, int *const switchable_rate, int *const skip_txfm_sb,
    int64_t *const skip_sse_sb) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int i;
  int tmp_rate;
  int64_t tmp_dist;

  (void)single_filter;

  InterpFilter assign_filter = SWITCHABLE;

  if (cm->interp_filter == SWITCHABLE) {
#if !CONFIG_DUAL_FILTER
    assign_filter = av1_is_interp_needed(xd)
                        ? predict_interp_filter(cpi, x, bsize, mi_row, mi_col,
                                                single_filter)
                        : cm->interp_filter;
#endif  // !CONFIG_DUAL_FILTER
  } else {
    assign_filter = cm->interp_filter;
  }

  set_default_interp_filters(mbmi, assign_filter);

  *switchable_rate = av1_get_switchable_rate(cpi, xd);
  av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
  model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate, &tmp_dist,
                  skip_txfm_sb, skip_sse_sb);
  *rd = RDCOST(x->rdmult, x->rddiv, *switchable_rate + tmp_rate, tmp_dist);

  if (assign_filter == SWITCHABLE) {
    // do interp_filter search
    if (av1_is_interp_needed(xd) && av1_is_interp_search_needed(xd)) {
#if CONFIG_DUAL_FILTER
      const int filter_set_size = DUAL_FILTER_SET_SIZE;
#else
      const int filter_set_size = SWITCHABLE_FILTERS;
#endif  // CONFIG_DUAL_FILTER
      int best_in_temp = 0;
#if CONFIG_DUAL_FILTER
      InterpFilter best_filter[4];
      av1_copy(best_filter, mbmi->interp_filter);
#else
      InterpFilter best_filter = mbmi->interp_filter;
#endif  // CONFIG_DUAL_FILTER
      restore_dst_buf(xd, *tmp_dst);
      // EIGHTTAP_REGULAR mode is calculated beforehand
      for (i = 1; i < filter_set_size; ++i) {
        int tmp_skip_sb = 0;
        int64_t tmp_skip_sse = INT64_MAX;
        int tmp_rs;
        int64_t tmp_rd;
#if CONFIG_DUAL_FILTER
        mbmi->interp_filter[0] = filter_sets[i][0];
        mbmi->interp_filter[1] = filter_sets[i][1];
        mbmi->interp_filter[2] = filter_sets[i][0];
        mbmi->interp_filter[3] = filter_sets[i][1];
#else
        mbmi->interp_filter = (InterpFilter)i;
#endif  // CONFIG_DUAL_FILTER
        tmp_rs = av1_get_switchable_rate(cpi, xd);
        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
        model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate,
                        &tmp_dist, &tmp_skip_sb, &tmp_skip_sse);
        tmp_rd = RDCOST(x->rdmult, x->rddiv, tmp_rs + tmp_rate, tmp_dist);

        if (tmp_rd < *rd) {
          *rd = tmp_rd;
          *switchable_rate = av1_get_switchable_rate(cpi, xd);
#if CONFIG_DUAL_FILTER
          av1_copy(best_filter, mbmi->interp_filter);
#else
          best_filter = mbmi->interp_filter;
#endif  // CONFIG_DUAL_FILTER
          *skip_txfm_sb = tmp_skip_sb;
          *skip_sse_sb = tmp_skip_sse;
          best_in_temp = !best_in_temp;
          if (best_in_temp) {
            restore_dst_buf(xd, *orig_dst);
          } else {
            restore_dst_buf(xd, *tmp_dst);
          }
        }
      }
      if (best_in_temp) {
        restore_dst_buf(xd, *tmp_dst);
      } else {
        restore_dst_buf(xd, *orig_dst);
      }
#if CONFIG_DUAL_FILTER
      av1_copy(mbmi->interp_filter, best_filter);
#else
      mbmi->interp_filter = best_filter;
#endif  // CONFIG_DUAL_FILTER
    } else {
#if CONFIG_DUAL_FILTER
      for (i = 0; i < 4; ++i)
        assert(mbmi->interp_filter[i] == EIGHTTAP_REGULAR);
#else
      assert(mbmi->interp_filter == EIGHTTAP_REGULAR);
#endif  // CONFIG_DUAL_FILTER
    }
  }

  return 0;
}

// TODO(afergs): Refactor the MBMI references in here - there's four
// TODO(afergs): Refactor optional args - add them to a struct or remove
static int64_t motion_mode_rd(
    const AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize,
    RD_STATS *rd_stats, RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv,
    int *disable_skip, int_mv (*mode_mv)[TOTAL_REFS_PER_FRAME], int mi_row,
    int mi_col, HandleInterModeArgs *const args, const int64_t ref_best_rd,
    const int *refs, int rate_mv,
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    int_mv *const single_newmv,
#if CONFIG_EXT_INTER
    int rate2_bmc_nocoeff, MB_MODE_INFO *best_bmc_mbmi,
#if CONFIG_MOTION_VAR
    int rate_mv_bmc,
#endif  // CONFIG_MOTION_VAR
#endif  // CONFIG_EXT_INTER
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    int rs, int *skip_txfm_sb, int64_t *skip_sse_sb, BUFFER_SET *orig_dst) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MODE_INFO *mi = xd->mi[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;

  (void)mode_mv;
  (void)mi_row;
  (void)mi_col;
  (void)args;
  (void)refs;
  (void)rate_mv;
  (void)is_comp_pred;
  (void)this_mode;

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  MOTION_MODE motion_mode, last_motion_mode_allowed;
  int rate2_nocoeff = 0, best_xskip, best_disable_skip = 0;
  RD_STATS best_rd_stats, best_rd_stats_y, best_rd_stats_uv;
  MB_MODE_INFO base_mbmi, best_mbmi;
#if CONFIG_VAR_TX
  uint8_t best_blk_skip[MAX_MB_PLANE][MAX_MIB_SIZE * MAX_MIB_SIZE * 4];
#endif  // CONFIG_VAR_TX
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

#if CONFIG_WARPED_MOTION
  int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
#endif  // CONFIG_WARPED_MOTION

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  av1_invalid_rd_stats(&best_rd_stats);
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

  if (cm->interp_filter == SWITCHABLE) rd_stats->rate += rs;
#if CONFIG_WARPED_MOTION
  aom_clear_system_state();
  mbmi->num_proj_ref[0] = findSamples(cm, xd, mi_row, mi_col, pts, pts_inref);
#if CONFIG_EXT_INTER
  best_bmc_mbmi->num_proj_ref[0] = mbmi->num_proj_ref[0];
#endif  // CONFIG_EXT_INTER
#endif  // CONFIG_WARPED_MOTION
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  rate2_nocoeff = rd_stats->rate;
  last_motion_mode_allowed = motion_mode_allowed(
#if CONFIG_GLOBAL_MOTION && SEPARATE_GLOBAL_MOTION
      0, xd->global_motion,
#endif  // CONFIG_GLOBAL_MOTION && SEPARATE_GLOBAL_MOTION
      mi);
  base_mbmi = *mbmi;
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  int64_t best_rd = INT64_MAX;
  for (motion_mode = SIMPLE_TRANSLATION;
       motion_mode <= last_motion_mode_allowed; motion_mode++) {
    int64_t tmp_rd = INT64_MAX;
    int tmp_rate;
    int64_t tmp_dist;
#if CONFIG_EXT_INTER
    int tmp_rate2 =
        motion_mode != SIMPLE_TRANSLATION ? rate2_bmc_nocoeff : rate2_nocoeff;
#else
    int tmp_rate2 = rate2_nocoeff;
#endif  // CONFIG_EXT_INTER

    *mbmi = base_mbmi;
    mbmi->motion_mode = motion_mode;
#if CONFIG_MOTION_VAR
    if (mbmi->motion_mode == OBMC_CAUSAL) {
#if CONFIG_EXT_INTER
      *mbmi = *best_bmc_mbmi;
      mbmi->motion_mode = OBMC_CAUSAL;
#endif  // CONFIG_EXT_INTER
      if (!is_comp_pred && have_newmv_in_inter_mode(this_mode)) {
        int tmp_rate_mv = 0;

        single_motion_search(cpi, x, bsize, mi_row, mi_col,
#if CONFIG_EXT_INTER
                             0,
#endif  // CONFIG_EXT_INTER
                             &tmp_rate_mv);
        mbmi->mv[0].as_int = x->best_mv.as_int;
        if (discount_newmv_test(cpi, this_mode, mbmi->mv[0], mode_mv,
                                refs[0])) {
          tmp_rate_mv = AOMMAX((tmp_rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
        }
#if CONFIG_EXT_INTER
        tmp_rate2 = rate2_bmc_nocoeff - rate_mv_bmc + tmp_rate_mv;
#else
        tmp_rate2 = rate2_nocoeff - rate_mv + tmp_rate_mv;
#endif  // CONFIG_EXT_INTER
#if CONFIG_DUAL_FILTER
        if (!has_subpel_mv_component(xd->mi[0], xd, 0))
          mbmi->interp_filter[0] = EIGHTTAP_REGULAR;
        if (!has_subpel_mv_component(xd->mi[0], xd, 1))
          mbmi->interp_filter[1] = EIGHTTAP_REGULAR;
#endif  // CONFIG_DUAL_FILTER
        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
#if CONFIG_EXT_INTER
      } else {
        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
#endif  // CONFIG_EXT_INTER
      }
      av1_build_obmc_inter_prediction(
          cm, xd, mi_row, mi_col, args->above_pred_buf, args->above_pred_stride,
          args->left_pred_buf, args->left_pred_stride);
      model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate,
                      &tmp_dist, skip_txfm_sb, skip_sse_sb);
    }
#endif  // CONFIG_MOTION_VAR

#if CONFIG_WARPED_MOTION
    if (mbmi->motion_mode == WARPED_CAUSAL) {
#if CONFIG_EXT_INTER
      *mbmi = *best_bmc_mbmi;
      mbmi->motion_mode = WARPED_CAUSAL;
#endif  // CONFIG_EXT_INTER
      mbmi->wm_params[0].wmtype = DEFAULT_WMTYPE;
#if CONFIG_DUAL_FILTER
      for (int dir = 0; dir < 4; ++dir)
        mbmi->interp_filter[dir] = cm->interp_filter == SWITCHABLE
                                       ? EIGHTTAP_REGULAR
                                       : cm->interp_filter;
#else
      mbmi->interp_filter = cm->interp_filter == SWITCHABLE ? EIGHTTAP_REGULAR
                                                            : cm->interp_filter;
#endif  // CONFIG_DUAL_FILTER

      if (!find_projection(mbmi->num_proj_ref[0], pts, pts_inref, bsize,
                           mbmi->mv[0].as_mv.row, mbmi->mv[0].as_mv.col,
                           &mbmi->wm_params[0], mi_row, mi_col)) {
        // Refine MV for NEWMV mode
        if (!is_comp_pred && have_newmv_in_inter_mode(this_mode)) {
          int tmp_rate_mv = 0;
          const int_mv mv0 = mbmi->mv[0];
          WarpedMotionParams wm_params0 = mbmi->wm_params[0];

          // Refine MV in a small range.
          av1_refine_warped_mv(cpi, x, bsize, mi_row, mi_col, pts, pts_inref);

          // Keep the refined MV and WM parameters.
          if (mv0.as_int != mbmi->mv[0].as_int) {
            const int ref = refs[0];
            const MV ref_mv = x->mbmi_ext->ref_mvs[ref][0].as_mv;

            tmp_rate_mv =
                av1_mv_bit_cost(&mbmi->mv[0].as_mv, &ref_mv, x->nmvjointcost,
                                x->mvcost, MV_COST_WEIGHT);

            if (cpi->sf.adaptive_motion_search)
              x->pred_mv[ref] = mbmi->mv[0].as_mv;

            single_newmv[ref] = mbmi->mv[0];

            if (discount_newmv_test(cpi, this_mode, mbmi->mv[0], mode_mv,
                                    refs[0])) {
              tmp_rate_mv = AOMMAX((tmp_rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
            }
#if CONFIG_EXT_INTER
            tmp_rate2 = rate2_bmc_nocoeff - rate_mv_bmc + tmp_rate_mv;
#else
            tmp_rate2 = rate2_nocoeff - rate_mv + tmp_rate_mv;
#endif  // CONFIG_EXT_INTER
#if CONFIG_DUAL_FILTER
            if (!has_subpel_mv_component(xd->mi[0], xd, 0))
              mbmi->interp_filter[0] = EIGHTTAP_REGULAR;
            if (!has_subpel_mv_component(xd->mi[0], xd, 1))
              mbmi->interp_filter[1] = EIGHTTAP_REGULAR;
#endif  // CONFIG_DUAL_FILTER
          } else {
            // Restore the old MV and WM parameters.
            mbmi->mv[0] = mv0;
            mbmi->wm_params[0] = wm_params0;
          }
        }

        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
        model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate,
                        &tmp_dist, skip_txfm_sb, skip_sse_sb);
      } else {
        continue;
      }
    }
#endif  // CONFIG_WARPED_MOTION
    x->skip = 0;

    rd_stats->dist = 0;
    rd_stats->sse = 0;
    rd_stats->skip = 1;
    rd_stats->rate = tmp_rate2;
    if (last_motion_mode_allowed > SIMPLE_TRANSLATION) {
#if CONFIG_WARPED_MOTION && CONFIG_MOTION_VAR
      if (last_motion_mode_allowed == WARPED_CAUSAL)
#endif  // CONFIG_WARPED_MOTION && CONFIG_MOTION_VAR
        rd_stats->rate += cpi->motion_mode_cost[bsize][mbmi->motion_mode];
#if CONFIG_WARPED_MOTION && CONFIG_MOTION_VAR
      else
        rd_stats->rate += cpi->motion_mode_cost1[bsize][mbmi->motion_mode];
#endif  // CONFIG_WARPED_MOTION && CONFIG_MOTION_VAR
    }
#if CONFIG_WARPED_MOTION
    if (mbmi->motion_mode == WARPED_CAUSAL) {
      rd_stats->rate -= rs;
    }
#endif  // CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    if (!*skip_txfm_sb) {
      int64_t rdcosty = INT64_MAX;
      int is_cost_valid_uv = 0;

      // cost and distortion
      av1_subtract_plane(x, bsize, 0);
#if CONFIG_VAR_TX
      if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id]) {
        select_tx_type_yrd(cpi, x, rd_stats_y, bsize, ref_best_rd);
      } else {
        int idx, idy;
        super_block_yrd(cpi, x, rd_stats_y, bsize, ref_best_rd);
        for (idy = 0; idy < xd->n8_h; ++idy)
          for (idx = 0; idx < xd->n8_w; ++idx)
            mbmi->inter_tx_size[idy][idx] = mbmi->tx_size;
        memset(x->blk_skip[0], rd_stats_y->skip,
               sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
      }
#else
    /* clang-format off */
      super_block_yrd(cpi, x, rd_stats_y, bsize, ref_best_rd);
/* clang-format on */
#endif  // CONFIG_VAR_TX

      if (rd_stats_y->rate == INT_MAX) {
        av1_invalid_rd_stats(rd_stats);
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
        if (mbmi->motion_mode != SIMPLE_TRANSLATION) {
          continue;
        } else {
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
          restore_dst_buf(xd, *orig_dst);
          return INT64_MAX;
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
        }
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
      }

      av1_merge_rd_stats(rd_stats, rd_stats_y);

      rdcosty = RDCOST(x->rdmult, x->rddiv, rd_stats->rate, rd_stats->dist);
      rdcosty = AOMMIN(rdcosty, RDCOST(x->rdmult, x->rddiv, 0, rd_stats->sse));
/* clang-format off */
#if CONFIG_VAR_TX
      is_cost_valid_uv =
          inter_block_uvrd(cpi, x, rd_stats_uv, bsize, ref_best_rd - rdcosty);
#else
      is_cost_valid_uv =
          super_block_uvrd(cpi, x, rd_stats_uv, bsize, ref_best_rd - rdcosty);
#endif  // CONFIG_VAR_TX
      if (!is_cost_valid_uv) {
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
        continue;
#else
        restore_dst_buf(xd, *orig_dst);
        return INT64_MAX;
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
      }
      /* clang-format on */
      av1_merge_rd_stats(rd_stats, rd_stats_uv);
#if CONFIG_RD_DEBUG
      // record transform block coefficient cost
      // TODO(angiebird): So far rd_debug tool only detects discrepancy of
      // coefficient cost. Therefore, it is fine to copy rd_stats into mbmi
      // here because we already collect the coefficient cost. Move this part to
      // other place when we need to compare non-coefficient cost.
      mbmi->rd_stats = *rd_stats;
#endif  // CONFIG_RD_DEBUG
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
      if (rd_stats->skip) {
        rd_stats->rate -= rd_stats_uv->rate + rd_stats_y->rate;
        rd_stats_y->rate = 0;
        rd_stats_uv->rate = 0;
        rd_stats->rate += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
        mbmi->skip = 0;
        // here mbmi->skip temporarily plays a role as what this_skip2 does
      } else if (!xd->lossless[mbmi->segment_id] &&
                 (RDCOST(x->rdmult, x->rddiv,
                         rd_stats_y->rate + rd_stats_uv->rate +
                             av1_cost_bit(av1_get_skip_prob(cm, xd), 0),
                         rd_stats->dist) >=
                  RDCOST(x->rdmult, x->rddiv,
                         av1_cost_bit(av1_get_skip_prob(cm, xd), 1),
                         rd_stats->sse))) {
        rd_stats->rate -= rd_stats_uv->rate + rd_stats_y->rate;
        rd_stats->rate += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
        rd_stats->dist = rd_stats->sse;
        rd_stats_y->rate = 0;
        rd_stats_uv->rate = 0;
        mbmi->skip = 1;
      } else {
        rd_stats->rate += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
        mbmi->skip = 0;
      }
      *disable_skip = 0;
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    } else {
      x->skip = 1;
      *disable_skip = 1;
      mbmi->tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode, 1);

// The cost of skip bit needs to be added.
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
      mbmi->skip = 0;
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
      rd_stats->rate += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);

      rd_stats->dist = *skip_sse_sb;
      rd_stats->sse = *skip_sse_sb;
      rd_stats_y->rate = 0;
      rd_stats_uv->rate = 0;
      rd_stats->skip = 1;
    }

#if CONFIG_GLOBAL_MOTION
    if (this_mode == ZEROMV
#if CONFIG_EXT_INTER
        || this_mode == ZERO_ZEROMV
#endif  // CONFIG_EXT_INTER
        ) {
      if (is_nontrans_global_motion(xd)) {
        rd_stats->rate -= rs;
#if CONFIG_DUAL_FILTER
        mbmi->interp_filter[0] = cm->interp_filter == SWITCHABLE
                                     ? EIGHTTAP_REGULAR
                                     : cm->interp_filter;
        mbmi->interp_filter[1] = cm->interp_filter == SWITCHABLE
                                     ? EIGHTTAP_REGULAR
                                     : cm->interp_filter;
#else
        mbmi->interp_filter = cm->interp_filter == SWITCHABLE
                                  ? EIGHTTAP_REGULAR
                                  : cm->interp_filter;
#endif  // CONFIG_DUAL_FILTER
      }
    }
#endif  // CONFIG_GLOBAL_MOTION

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    tmp_rd = RDCOST(x->rdmult, x->rddiv, rd_stats->rate, rd_stats->dist);
    if (mbmi->motion_mode == SIMPLE_TRANSLATION || (tmp_rd < best_rd)) {
      best_mbmi = *mbmi;
      best_rd = tmp_rd;
      best_rd_stats = *rd_stats;
      best_rd_stats_y = *rd_stats_y;
      best_rd_stats_uv = *rd_stats_uv;
#if CONFIG_VAR_TX
      for (int i = 0; i < MAX_MB_PLANE; ++i)
        memcpy(best_blk_skip[i], x->blk_skip[i],
               sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
#endif  // CONFIG_VAR_TX
      best_xskip = x->skip;
      best_disable_skip = *disable_skip;
    }
  }

  if (best_rd == INT64_MAX) {
    av1_invalid_rd_stats(rd_stats);
    restore_dst_buf(xd, *orig_dst);
    return INT64_MAX;
  }
  *mbmi = best_mbmi;
  *rd_stats = best_rd_stats;
  *rd_stats_y = best_rd_stats_y;
  *rd_stats_uv = best_rd_stats_uv;
#if CONFIG_VAR_TX
  for (int i = 0; i < MAX_MB_PLANE; ++i)
    memcpy(x->blk_skip[i], best_blk_skip[i],
           sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
#endif  // CONFIG_VAR_TX
  x->skip = best_xskip;
  *disable_skip = best_disable_skip;
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

  restore_dst_buf(xd, *orig_dst);
  return 0;
}

static int64_t handle_inter_mode(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    RD_STATS *rd_stats, RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv,
    int *disable_skip, int_mv (*mode_mv)[TOTAL_REFS_PER_FRAME], int mi_row,
    int mi_col, HandleInterModeArgs *args, const int64_t ref_best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  (void)cm;
  MACROBLOCKD *xd = &x->e_mbd;
  MODE_INFO *mi = xd->mi[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int is_comp_pred = has_second_ref(mbmi);
  const int this_mode = mbmi->mode;
  int_mv *frame_mv = mode_mv[this_mode];
  int i;
  int refs[2] = { mbmi->ref_frame[0],
                  (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  int_mv cur_mv[2];
  int rate_mv = 0;
#if CONFIG_EXT_INTER
  int pred_exists = 1;
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
  const int bw = block_size_wide[bsize];
#endif  // ONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
  int_mv single_newmv[TOTAL_REFS_PER_FRAME];
#if CONFIG_INTERINTRA
  const unsigned int *const interintra_mode_cost =
      cpi->interintra_mode_cost[size_group_lookup[bsize]];
#endif  // CONFIG_INTERINTRA
  const int is_comp_interintra_pred = (mbmi->ref_frame[1] == INTRA_FRAME);
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
#else
  int_mv *const single_newmv = args->single_newmv;
#endif  // CONFIG_EXT_INTER
#if CONFIG_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint8_t, tmp_buf_[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
#else
  DECLARE_ALIGNED(16, uint8_t, tmp_buf_[MAX_MB_PLANE * MAX_SB_SQUARE]);
#endif  // CONFIG_HIGHBITDEPTH
  uint8_t *tmp_buf;

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#if CONFIG_EXT_INTER
  int rate2_bmc_nocoeff;
  MB_MODE_INFO best_bmc_mbmi;
#if CONFIG_MOTION_VAR
  int rate_mv_bmc;
#endif  // CONFIG_MOTION_VAR
#endif  // CONFIG_EXT_INTER
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  int64_t rd = INT64_MAX;
  BUFFER_SET orig_dst, tmp_dst;
  int rs = 0;

  int skip_txfm_sb = 0;
  int64_t skip_sse_sb = INT64_MAX;
  int16_t mode_ctx;

#if CONFIG_EXT_INTER
#if CONFIG_INTERINTRA
  int compmode_interintra_cost = 0;
  mbmi->use_wedge_interintra = 0;
#endif
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
  int compmode_interinter_cost = 0;
  mbmi->interinter_compound_type = COMPOUND_AVERAGE;
#endif

#if CONFIG_INTERINTRA
  if (!cm->allow_interintra_compound && is_comp_interintra_pred)
    return INT64_MAX;
#endif  // CONFIG_INTERINTRA

  // is_comp_interintra_pred implies !is_comp_pred
  assert(!is_comp_interintra_pred || (!is_comp_pred));
  // is_comp_interintra_pred implies is_interintra_allowed(mbmi->sb_type)
  assert(!is_comp_interintra_pred || is_interintra_allowed(mbmi));
#endif  // CONFIG_EXT_INTER

#if CONFIG_EXT_INTER
  if (is_comp_pred)
    mode_ctx = mbmi_ext->compound_mode_context[refs[0]];
  else
#endif  // CONFIG_EXT_INTER
    mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context,
                                         mbmi->ref_frame, bsize, -1);

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    tmp_buf = CONVERT_TO_BYTEPTR(tmp_buf_);
  else
#endif  // CONFIG_HIGHBITDEPTH
    tmp_buf = tmp_buf_;
  // Make sure that we didn't leave the plane destination buffers set
  // to tmp_buf at the end of the last iteration
  assert(xd->plane[0].dst.buf != tmp_buf);

#if CONFIG_WARPED_MOTION
  mbmi->num_proj_ref[0] = 0;
  mbmi->num_proj_ref[1] = 0;
#endif  // CONFIG_WARPED_MOTION

  if (is_comp_pred) {
    if (frame_mv[refs[0]].as_int == INVALID_MV ||
        frame_mv[refs[1]].as_int == INVALID_MV)
      return INT64_MAX;
  }

  mbmi->motion_mode = SIMPLE_TRANSLATION;
  if (have_newmv_in_inter_mode(this_mode)) {
    const int64_t ret_val = handle_newmv(cpi, x, bsize, mode_mv, mi_row, mi_col,
                                         &rate_mv, single_newmv, args);
    if (ret_val != 0)
      return ret_val;
    else
      rd_stats->rate += rate_mv;
  }
  for (i = 0; i < is_comp_pred + 1; ++i) {
    cur_mv[i] = frame_mv[refs[i]];
    // Clip "next_nearest" so that it does not extend to far out of image
    if (this_mode != NEWMV) clamp_mv2(&cur_mv[i].as_mv, xd);
    if (mv_check_bounds(&x->mv_limits, &cur_mv[i].as_mv)) return INT64_MAX;
    mbmi->mv[i].as_int = cur_mv[i].as_int;
  }

#if CONFIG_EXT_INTER
  if (this_mode == NEAREST_NEARESTMV)
#else
  if (this_mode == NEARESTMV && is_comp_pred)
#endif  // CONFIG_EXT_INTER
  {
#if !CONFIG_EXT_INTER
    uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
#endif  // !CONFIG_EXT_INTER
    if (mbmi_ext->ref_mv_count[ref_frame_type] > 0) {
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv;
      cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;

      for (i = 0; i < 2; ++i) {
        clamp_mv2(&cur_mv[i].as_mv, xd);
        if (mv_check_bounds(&x->mv_limits, &cur_mv[i].as_mv)) return INT64_MAX;
        mbmi->mv[i].as_int = cur_mv[i].as_int;
      }
    }
  }

#if CONFIG_EXT_INTER
  if (mbmi_ext->ref_mv_count[ref_frame_type] > 0) {
    if (this_mode == NEAREST_NEWMV) {
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv;

      lower_mv_precision(&cur_mv[0].as_mv, cm->allow_high_precision_mv);
      clamp_mv2(&cur_mv[0].as_mv, xd);
      if (mv_check_bounds(&x->mv_limits, &cur_mv[0].as_mv)) return INT64_MAX;
      mbmi->mv[0].as_int = cur_mv[0].as_int;
    }

    if (this_mode == NEW_NEARESTMV) {
      cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;

      lower_mv_precision(&cur_mv[1].as_mv, cm->allow_high_precision_mv);
      clamp_mv2(&cur_mv[1].as_mv, xd);
      if (mv_check_bounds(&x->mv_limits, &cur_mv[1].as_mv)) return INT64_MAX;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
    }
  }

  if (mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
    int ref_mv_idx = mbmi->ref_mv_idx + 1;
    if (this_mode == NEAR_NEWMV || this_mode == NEAR_NEARMV) {
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;

      lower_mv_precision(&cur_mv[0].as_mv, cm->allow_high_precision_mv);
      clamp_mv2(&cur_mv[0].as_mv, xd);
      if (mv_check_bounds(&x->mv_limits, &cur_mv[0].as_mv)) return INT64_MAX;
      mbmi->mv[0].as_int = cur_mv[0].as_int;
    }

    if (this_mode == NEW_NEARMV || this_mode == NEAR_NEARMV) {
      cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;

      lower_mv_precision(&cur_mv[1].as_mv, cm->allow_high_precision_mv);
      clamp_mv2(&cur_mv[1].as_mv, xd);
      if (mv_check_bounds(&x->mv_limits, &cur_mv[1].as_mv)) return INT64_MAX;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
    }
  }
#else
  if (this_mode == NEARMV && is_comp_pred) {
    uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
    if (mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
      int ref_mv_idx = mbmi->ref_mv_idx + 1;
      cur_mv[0] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
      cur_mv[1] = mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;

      for (i = 0; i < 2; ++i) {
        clamp_mv2(&cur_mv[i].as_mv, xd);
        if (mv_check_bounds(&x->mv_limits, &cur_mv[i].as_mv)) return INT64_MAX;
        mbmi->mv[i].as_int = cur_mv[i].as_int;
      }
    }
  }
#endif  // CONFIG_EXT_INTER

  // do first prediction into the destination buffer. Do the next
  // prediction into a temporary buffer. Then keep track of which one
  // of these currently holds the best predictor, and use the other
  // one for future predictions. In the end, copy from tmp_buf to
  // dst if necessary.
  for (i = 0; i < MAX_MB_PLANE; i++) {
    tmp_dst.plane[i] = tmp_buf + i * MAX_SB_SQUARE;
    tmp_dst.stride[i] = MAX_SB_SIZE;
  }
  for (i = 0; i < MAX_MB_PLANE; i++) {
    orig_dst.plane[i] = xd->plane[i].dst.buf;
    orig_dst.stride[i] = xd->plane[i].dst.stride;
  }

  // We don't include the cost of the second reference here, because there
  // are only three options: Last/Golden, ARF/Last or Golden/ARF, or in other
  // words if you present them in that order, the second one is always known
  // if the first is known.
  //
  // Under some circumstances we discount the cost of new mv mode to encourage
  // initiation of a motion field.
  if (discount_newmv_test(cpi, this_mode, frame_mv[refs[0]], mode_mv,
                          refs[0])) {
#if CONFIG_EXT_INTER
    rd_stats->rate +=
        AOMMIN(cost_mv_ref(cpi, this_mode, mode_ctx),
               cost_mv_ref(cpi, is_comp_pred ? NEAREST_NEARESTMV : NEARESTMV,
                           mode_ctx));
#else
    rd_stats->rate += AOMMIN(cost_mv_ref(cpi, this_mode, mode_ctx),
                             cost_mv_ref(cpi, NEARESTMV, mode_ctx));
#endif  // CONFIG_EXT_INTER
  } else {
    rd_stats->rate += cost_mv_ref(cpi, this_mode, mode_ctx);
  }

  if (RDCOST(x->rdmult, x->rddiv, rd_stats->rate, 0) > ref_best_rd &&
#if CONFIG_EXT_INTER
      mbmi->mode != NEARESTMV && mbmi->mode != NEAREST_NEARESTMV
#else
      mbmi->mode != NEARESTMV
#endif  // CONFIG_EXT_INTER
      )
    return INT64_MAX;

  int64_t ret_val = interpolation_filter_search(
      x, cpi, bsize, mi_row, mi_col, &tmp_dst, &orig_dst, args->single_filter,
      &rd, &rs, &skip_txfm_sb, &skip_sse_sb);
  if (ret_val != 0) return ret_val;

#if CONFIG_EXT_INTER
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  best_bmc_mbmi = *mbmi;
  rate2_bmc_nocoeff = rd_stats->rate;
  if (cm->interp_filter == SWITCHABLE) rate2_bmc_nocoeff += rs;
#if CONFIG_MOTION_VAR
  rate_mv_bmc = rate_mv;
#endif  // CONFIG_MOTION_VAR
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
  if (is_comp_pred) {
    int rate_sum, rs2;
    int64_t dist_sum;
    int64_t best_rd_compound = INT64_MAX, best_rd_cur = INT64_MAX;
    INTERINTER_COMPOUND_DATA best_compound_data;
    int_mv best_mv[2];
    int best_tmp_rate_mv = rate_mv;
    int tmp_skip_txfm_sb;
    int64_t tmp_skip_sse_sb;
    int compound_type_cost[COMPOUND_TYPES];
    uint8_t pred0[2 * MAX_SB_SQUARE];
    uint8_t pred1[2 * MAX_SB_SQUARE];
    uint8_t *preds0[1] = { pred0 };
    uint8_t *preds1[1] = { pred1 };
    int strides[1] = { bw };
    int tmp_rate_mv;
    int masked_compound_used = is_any_masked_compound_used(bsize);
#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
    masked_compound_used = masked_compound_used && cm->allow_masked_compound;
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
    COMPOUND_TYPE cur_type;

    best_mv[0].as_int = cur_mv[0].as_int;
    best_mv[1].as_int = cur_mv[1].as_int;
    memset(&best_compound_data, 0, sizeof(best_compound_data));
#if CONFIG_COMPOUND_SEGMENT
    uint8_t tmp_mask_buf[2 * MAX_SB_SQUARE];
    best_compound_data.seg_mask = tmp_mask_buf;
#endif  // CONFIG_COMPOUND_SEGMENT

    if (masked_compound_used) {
      av1_cost_tokens(compound_type_cost, cm->fc->compound_type_prob[bsize],
                      av1_compound_type_tree);
      // get inter predictors to use for masked compound modes
      av1_build_inter_predictors_for_planes_single_buf(
          xd, bsize, 0, 0, mi_row, mi_col, 0, preds0, strides);
      av1_build_inter_predictors_for_planes_single_buf(
          xd, bsize, 0, 0, mi_row, mi_col, 1, preds1, strides);
    }

    for (cur_type = COMPOUND_AVERAGE; cur_type < COMPOUND_TYPES; cur_type++) {
      if (cur_type != COMPOUND_AVERAGE && !masked_compound_used) break;
      if (!is_interinter_compound_used(cur_type, bsize)) break;
      tmp_rate_mv = rate_mv;
      best_rd_cur = INT64_MAX;
      mbmi->interinter_compound_type = cur_type;
      rs2 = av1_cost_literal(get_interinter_compound_type_bits(
                bsize, mbmi->interinter_compound_type)) +
            (masked_compound_used
                 ? compound_type_cost[mbmi->interinter_compound_type]
                 : 0);

      switch (cur_type) {
        case COMPOUND_AVERAGE:
          av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, &orig_dst,
                                         bsize);
          av1_subtract_plane(x, bsize, 0);
          rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                                   &tmp_skip_txfm_sb, &tmp_skip_sse_sb,
                                   INT64_MAX);
          if (rd != INT64_MAX)
            best_rd_cur =
                RDCOST(x->rdmult, x->rddiv, rs2 + rate_mv + rate_sum, dist_sum);
          best_rd_compound = best_rd_cur;
          break;
#if CONFIG_WEDGE
        case COMPOUND_WEDGE:
          if (x->source_variance > cpi->sf.disable_wedge_search_var_thresh &&
              best_rd_compound / 3 < ref_best_rd) {
            best_rd_cur = build_and_cost_compound_type(
                cpi, x, cur_mv, bsize, this_mode, rs2, rate_mv, &orig_dst,
                &tmp_rate_mv, preds0, preds1, strides, mi_row, mi_col);
          }
          break;
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
        case COMPOUND_SEG:
          if (x->source_variance > cpi->sf.disable_wedge_search_var_thresh &&
              best_rd_compound / 3 < ref_best_rd) {
            best_rd_cur = build_and_cost_compound_type(
                cpi, x, cur_mv, bsize, this_mode, rs2, rate_mv, &orig_dst,
                &tmp_rate_mv, preds0, preds1, strides, mi_row, mi_col);
          }
          break;
#endif  // CONFIG_COMPOUND_SEGMENT
        default: assert(0); return 0;
      }

      if (best_rd_cur < best_rd_compound) {
        best_rd_compound = best_rd_cur;
#if CONFIG_WEDGE
        best_compound_data.wedge_index = mbmi->wedge_index;
        best_compound_data.wedge_sign = mbmi->wedge_sign;
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
        best_compound_data.mask_type = mbmi->mask_type;
        memcpy(best_compound_data.seg_mask, xd->seg_mask,
               2 * MAX_SB_SQUARE * sizeof(uint8_t));
#endif  // CONFIG_COMPOUND_SEGMENT
        best_compound_data.interinter_compound_type =
            mbmi->interinter_compound_type;
        if (have_newmv_in_inter_mode(this_mode)) {
          if (use_masked_motion_search(cur_type)) {
            best_tmp_rate_mv = tmp_rate_mv;
            best_mv[0].as_int = mbmi->mv[0].as_int;
            best_mv[1].as_int = mbmi->mv[1].as_int;
          } else {
            best_mv[0].as_int = cur_mv[0].as_int;
            best_mv[1].as_int = cur_mv[1].as_int;
          }
        }
      }
      // reset to original mvs for next iteration
      mbmi->mv[0].as_int = cur_mv[0].as_int;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
    }
#if CONFIG_WEDGE
    mbmi->wedge_index = best_compound_data.wedge_index;
    mbmi->wedge_sign = best_compound_data.wedge_sign;
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
    mbmi->mask_type = best_compound_data.mask_type;
    memcpy(xd->seg_mask, best_compound_data.seg_mask,
           2 * MAX_SB_SQUARE * sizeof(uint8_t));
#endif  // CONFIG_COMPOUND_SEGMENT
    mbmi->interinter_compound_type =
        best_compound_data.interinter_compound_type;
    if (have_newmv_in_inter_mode(this_mode)) {
      mbmi->mv[0].as_int = best_mv[0].as_int;
      mbmi->mv[1].as_int = best_mv[1].as_int;
      xd->mi[0]->bmi[0].as_mv[0].as_int = mbmi->mv[0].as_int;
      xd->mi[0]->bmi[0].as_mv[1].as_int = mbmi->mv[1].as_int;
      if (use_masked_motion_search(mbmi->interinter_compound_type)) {
        rd_stats->rate += best_tmp_rate_mv - rate_mv;
        rate_mv = best_tmp_rate_mv;
      }
    }

    if (ref_best_rd < INT64_MAX && best_rd_compound / 3 > ref_best_rd) {
      restore_dst_buf(xd, orig_dst);
      return INT64_MAX;
    }

    pred_exists = 0;

    compmode_interinter_cost =
        av1_cost_literal(get_interinter_compound_type_bits(
            bsize, mbmi->interinter_compound_type)) +
        (masked_compound_used
             ? compound_type_cost[mbmi->interinter_compound_type]
             : 0);
  }
#endif  // CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT

#if CONFIG_INTERINTRA
  if (is_comp_interintra_pred) {
    INTERINTRA_MODE best_interintra_mode = II_DC_PRED;
    int64_t best_interintra_rd = INT64_MAX;
    int rmode, rate_sum;
    int64_t dist_sum;
    int j;
    int tmp_rate_mv = 0;
    int tmp_skip_txfm_sb;
    int64_t tmp_skip_sse_sb;
    DECLARE_ALIGNED(16, uint8_t, intrapred_[2 * MAX_SB_SQUARE]);
    uint8_t *intrapred;

#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      intrapred = CONVERT_TO_BYTEPTR(intrapred_);
    else
#endif  // CONFIG_HIGHBITDEPTH
      intrapred = intrapred_;

    mbmi->ref_frame[1] = NONE_FRAME;
    for (j = 0; j < MAX_MB_PLANE; j++) {
      xd->plane[j].dst.buf = tmp_buf + j * MAX_SB_SQUARE;
      xd->plane[j].dst.stride = bw;
    }
    av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, &orig_dst, bsize);
    restore_dst_buf(xd, orig_dst);
    mbmi->ref_frame[1] = INTRA_FRAME;
    mbmi->use_wedge_interintra = 0;

    for (j = 0; j < INTERINTRA_MODES; ++j) {
      mbmi->interintra_mode = (INTERINTRA_MODE)j;
      rmode = interintra_mode_cost[mbmi->interintra_mode];
      av1_build_intra_predictors_for_interintra(xd, bsize, 0, &orig_dst,
                                                intrapred, bw);
      av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
      model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                      &tmp_skip_txfm_sb, &tmp_skip_sse_sb);
      rd =
          RDCOST(x->rdmult, x->rddiv, tmp_rate_mv + rate_sum + rmode, dist_sum);
      if (rd < best_interintra_rd) {
        best_interintra_rd = rd;
        best_interintra_mode = mbmi->interintra_mode;
      }
    }
    mbmi->interintra_mode = best_interintra_mode;
    rmode = interintra_mode_cost[mbmi->interintra_mode];
    av1_build_intra_predictors_for_interintra(xd, bsize, 0, &orig_dst,
                                              intrapred, bw);
    av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
    av1_subtract_plane(x, bsize, 0);
    rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                             &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
    if (rd != INT64_MAX)
      rd = RDCOST(x->rdmult, x->rddiv, rate_mv + rmode + rate_sum, dist_sum);
    best_interintra_rd = rd;

    if (ref_best_rd < INT64_MAX && best_interintra_rd > 2 * ref_best_rd) {
      // Don't need to call restore_dst_buf here
      return INT64_MAX;
    }
#if CONFIG_WEDGE
    if (is_interintra_wedge_used(bsize)) {
      int64_t best_interintra_rd_nowedge = INT64_MAX;
      int64_t best_interintra_rd_wedge = INT64_MAX;
      int_mv tmp_mv;
      int rwedge = av1_cost_bit(cm->fc->wedge_interintra_prob[bsize], 0);
      if (rd != INT64_MAX)
        rd = RDCOST(x->rdmult, x->rddiv, rmode + rate_mv + rwedge + rate_sum,
                    dist_sum);
      best_interintra_rd_nowedge = best_interintra_rd;

      // Disable wedge search if source variance is small
      if (x->source_variance > cpi->sf.disable_wedge_search_var_thresh) {
        mbmi->use_wedge_interintra = 1;

        rwedge = av1_cost_literal(get_interintra_wedge_bits(bsize)) +
                 av1_cost_bit(cm->fc->wedge_interintra_prob[bsize], 1);

        best_interintra_rd_wedge =
            pick_interintra_wedge(cpi, x, bsize, intrapred_, tmp_buf_);

        best_interintra_rd_wedge +=
            RDCOST(x->rdmult, x->rddiv, rmode + rate_mv + rwedge, 0);
        // Refine motion vector.
        if (have_newmv_in_inter_mode(this_mode)) {
          // get negative of mask
          const uint8_t *mask = av1_get_contiguous_soft_mask(
              mbmi->interintra_wedge_index, 1, bsize);
          tmp_mv.as_int = x->mbmi_ext->ref_mvs[refs[0]][0].as_int;
          compound_single_motion_search(cpi, x, bsize, &tmp_mv.as_mv, mi_row,
                                        mi_col, intrapred, mask, bw,
                                        &tmp_rate_mv, 0, 0);
          mbmi->mv[0].as_int = tmp_mv.as_int;
          av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, &orig_dst,
                                         bsize);
          model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                          &tmp_skip_txfm_sb, &tmp_skip_sse_sb);
          rd = RDCOST(x->rdmult, x->rddiv,
                      rmode + tmp_rate_mv + rwedge + rate_sum, dist_sum);
          if (rd >= best_interintra_rd_wedge) {
            tmp_mv.as_int = cur_mv[0].as_int;
            tmp_rate_mv = rate_mv;
          }
        } else {
          tmp_mv.as_int = cur_mv[0].as_int;
          tmp_rate_mv = rate_mv;
          av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
        }
        // Evaluate closer to true rd
        av1_subtract_plane(x, bsize, 0);
        rd =
            estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                                &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
        if (rd != INT64_MAX)
          rd = RDCOST(x->rdmult, x->rddiv,
                      rmode + tmp_rate_mv + rwedge + rate_sum, dist_sum);
        best_interintra_rd_wedge = rd;
        if (best_interintra_rd_wedge < best_interintra_rd_nowedge) {
          mbmi->use_wedge_interintra = 1;
          mbmi->mv[0].as_int = tmp_mv.as_int;
          rd_stats->rate += tmp_rate_mv - rate_mv;
          rate_mv = tmp_rate_mv;
        } else {
          mbmi->use_wedge_interintra = 0;
          mbmi->mv[0].as_int = cur_mv[0].as_int;
        }
      } else {
        mbmi->use_wedge_interintra = 0;
      }
    }
#endif  // CONFIG_WEDGE

    pred_exists = 0;
    compmode_interintra_cost =
        av1_cost_bit(cm->fc->interintra_prob[size_group_lookup[bsize]], 1) +
        interintra_mode_cost[mbmi->interintra_mode];
    if (is_interintra_wedge_used(bsize)) {
      compmode_interintra_cost += av1_cost_bit(
          cm->fc->wedge_interintra_prob[bsize], mbmi->use_wedge_interintra);
      if (mbmi->use_wedge_interintra) {
        compmode_interintra_cost +=
            av1_cost_literal(get_interintra_wedge_bits(bsize));
      }
    }
  } else if (is_interintra_allowed(mbmi)) {
    compmode_interintra_cost =
        av1_cost_bit(cm->fc->interintra_prob[size_group_lookup[bsize]], 0);
  }
#endif  // CONFIG_INTERINTRA

  if (pred_exists == 0) {
    int tmp_rate;
    int64_t tmp_dist;
    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, &orig_dst, bsize);
    model_rd_for_sb(cpi, bsize, x, xd, 0, MAX_MB_PLANE - 1, &tmp_rate,
                    &tmp_dist, &skip_txfm_sb, &skip_sse_sb);
    rd = RDCOST(x->rdmult, x->rddiv, rs + tmp_rate, tmp_dist);
  }
#endif  // CONFIG_EXT_INTER

  if (!is_comp_pred)
#if CONFIG_DUAL_FILTER
    args->single_filter[this_mode][refs[0]] = mbmi->interp_filter[0];
#else
    args->single_filter[this_mode][refs[0]] = mbmi->interp_filter;
#endif  // CONFIG_DUAL_FILTER

#if CONFIG_EXT_INTER
  if (args->modelled_rd != NULL) {
    if (is_comp_pred) {
      const int mode0 = compound_ref0_mode(this_mode);
      const int mode1 = compound_ref1_mode(this_mode);
      const int64_t mrd = AOMMIN(args->modelled_rd[mode0][refs[0]],
                                 args->modelled_rd[mode1][refs[1]]);
      if (rd / 4 * 3 > mrd && ref_best_rd < INT64_MAX) {
        restore_dst_buf(xd, orig_dst);
        return INT64_MAX;
      }
    } else if (!is_comp_interintra_pred) {
      args->modelled_rd[this_mode][refs[0]] = rd;
    }
  }
#endif  // CONFIG_EXT_INTER

  if (cpi->sf.use_rd_breakout && ref_best_rd < INT64_MAX) {
    // if current pred_error modeled rd is substantially more than the best
    // so far, do not bother doing full rd
    if (rd / 2 > ref_best_rd) {
      restore_dst_buf(xd, orig_dst);
      return INT64_MAX;
    }
  }

#if CONFIG_EXT_INTER
#if CONFIG_INTERINTRA
  rd_stats->rate += compmode_interintra_cost;
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  rate2_bmc_nocoeff += compmode_interintra_cost;
#endif
#endif
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
  rd_stats->rate += compmode_interinter_cost;
#endif
#endif

  ret_val = motion_mode_rd(cpi, x, bsize, rd_stats, rd_stats_y, rd_stats_uv,
                           disable_skip, mode_mv, mi_row, mi_col, args,
                           ref_best_rd, refs, rate_mv,
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
                           single_newmv,
#if CONFIG_EXT_INTER
                           rate2_bmc_nocoeff, &best_bmc_mbmi,
#if CONFIG_MOTION_VAR
                           rate_mv_bmc,
#endif  // CONFIG_MOTION_VAR
#endif  // CONFIG_EXT_INTER
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
                           rs, &skip_txfm_sb, &skip_sse_sb, &orig_dst);
  if (ret_val != 0) return ret_val;

  return 0;  // The rate-distortion cost will be re-calculated by caller.
}

#if CONFIG_INTRABC
static int64_t rd_pick_intrabc_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x,
                                       RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                       int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  if (bsize < BLOCK_8X8 || !cm->allow_screen_content_tools) return INT64_MAX;

  MACROBLOCKD *const xd = &x->e_mbd;
  const TileInfo *tile = &xd->tile;
#if CONFIG_EC_ADAPT
  FRAME_CONTEXT *const ec_ctx = xd->tile_ctx;
#else
  FRAME_CONTEXT *const ec_ctx = cm->fc;
#endif  // CONFIG_EC_ADAPT
  MODE_INFO *const mi = xd->mi[0];
  const int mi_row = -xd->mb_to_top_edge / (8 * MI_SIZE);
  const int mi_col = -xd->mb_to_left_edge / (8 * MI_SIZE);
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int sb_row = mi_row / MAX_MIB_SIZE;
  const int sb_col = mi_col / MAX_MIB_SIZE;

  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  MV_REFERENCE_FRAME ref_frame = INTRA_FRAME;
  int_mv *const candidates = x->mbmi_ext->ref_mvs[ref_frame];
  av1_find_mv_refs(cm, xd, mi, ref_frame, &mbmi_ext->ref_mv_count[ref_frame],
                   mbmi_ext->ref_mv_stack[ref_frame],
#if CONFIG_EXT_INTER
                   mbmi_ext->compound_mode_context,
#endif  // CONFIG_EXT_INTER
                   candidates, mi_row, mi_col, NULL, NULL,
                   mbmi_ext->mode_context);

  int_mv nearestmv, nearmv;
  av1_find_best_ref_mvs(0, candidates, &nearestmv, &nearmv);

  int_mv dv_ref = nearestmv.as_int == 0 ? nearmv : nearestmv;
  if (dv_ref.as_int == 0) av1_find_ref_dv(&dv_ref, mi_row, mi_col);
  mbmi_ext->ref_mvs[INTRA_FRAME][0] = dv_ref;

  struct buf_2d yv12_mb[MAX_MB_PLANE];
  av1_setup_pred_block(xd, yv12_mb, xd->cur_buf, mi_row, mi_col, NULL, NULL);
  for (int i = 0; i < MAX_MB_PLANE; ++i) {
    xd->plane[i].pre[0] = yv12_mb[i];
  }

  enum IntrabcMotionDirection {
    IBC_MOTION_ABOVE,
    IBC_MOTION_LEFT,
    IBC_MOTION_DIRECTIONS
  };

  MB_MODE_INFO *mbmi = &mi->mbmi;
  MB_MODE_INFO best_mbmi = *mbmi;
  RD_STATS best_rdcost = *rd_cost;
  int best_skip = x->skip;

  for (enum IntrabcMotionDirection dir = IBC_MOTION_ABOVE;
       dir < IBC_MOTION_DIRECTIONS; ++dir) {
    const MvLimits tmp_mv_limits = x->mv_limits;
    switch (dir) {
      case IBC_MOTION_ABOVE:
        x->mv_limits.col_min = (tile->mi_col_start - mi_col) * MI_SIZE;
        x->mv_limits.col_max = (tile->mi_col_end - mi_col) * MI_SIZE - w;
        x->mv_limits.row_min = (tile->mi_row_start - mi_row) * MI_SIZE;
        x->mv_limits.row_max = (sb_row * MAX_MIB_SIZE - mi_row) * MI_SIZE - h;
        break;
      case IBC_MOTION_LEFT:
        x->mv_limits.col_min = (tile->mi_col_start - mi_col) * MI_SIZE;
        x->mv_limits.col_max = (sb_col * MAX_MIB_SIZE - mi_col) * MI_SIZE - w;
        // TODO(aconverse@google.com): Minimize the overlap between above and
        // left areas.
        x->mv_limits.row_min = (tile->mi_row_start - mi_row) * MI_SIZE;
        int bottom_coded_mi_edge =
            AOMMIN((sb_row + 1) * MAX_MIB_SIZE, tile->mi_row_end);
        x->mv_limits.row_max = (bottom_coded_mi_edge - mi_row) * MI_SIZE - h;
        break;
      default: assert(0);
    }
    assert(x->mv_limits.col_min >= tmp_mv_limits.col_min);
    assert(x->mv_limits.col_max <= tmp_mv_limits.col_max);
    assert(x->mv_limits.row_min >= tmp_mv_limits.row_min);
    assert(x->mv_limits.row_max <= tmp_mv_limits.row_max);
    av1_set_mv_search_range(&x->mv_limits, &dv_ref.as_mv);

    if (x->mv_limits.col_max < x->mv_limits.col_min ||
        x->mv_limits.row_max < x->mv_limits.row_min) {
      x->mv_limits = tmp_mv_limits;
      continue;
    }

    int step_param = cpi->mv_step_param;
    MV mvp_full = dv_ref.as_mv;
    mvp_full.col >>= 3;
    mvp_full.row >>= 3;
    int sadpb = x->sadperbit16;
    int cost_list[5];
    int bestsme = av1_full_pixel_search(cpi, x, bsize, &mvp_full, step_param,
                                        sadpb, cond_cost_list(cpi, cost_list),
                                        &dv_ref.as_mv, INT_MAX, 1);

    x->mv_limits = tmp_mv_limits;
    if (bestsme == INT_MAX) continue;
    mvp_full = x->best_mv.as_mv;
    MV dv = {.row = mvp_full.row * 8, .col = mvp_full.col * 8 };
    if (mv_check_bounds(&x->mv_limits, &dv)) continue;
    if (!is_dv_valid(dv, tile, mi_row, mi_col, bsize)) continue;

#if CONFIG_PALETTE
    memset(&mbmi->palette_mode_info, 0, sizeof(mbmi->palette_mode_info));
#endif
    mbmi->use_intrabc = 1;
    mbmi->mode = DC_PRED;
    mbmi->uv_mode = DC_PRED;
    mbmi->mv[0].as_mv = dv;
#if CONFIG_DUAL_FILTER
    for (int idx = 0; idx < 4; ++idx) mbmi->interp_filter[idx] = BILINEAR;
#else
    mbmi->interp_filter = BILINEAR;
#endif
    mbmi->skip = 0;
    x->skip = 0;
    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);

    int rate_mv = av1_mv_bit_cost(&dv, &dv_ref.as_mv, x->nmvjointcost,
                                  x->mvcost, MV_COST_WEIGHT);
    const PREDICTION_MODE A = av1_above_block_mode(mi, xd->above_mi, 0);
    const PREDICTION_MODE L = av1_left_block_mode(mi, xd->left_mi, 0);
    const int rate_mode = cpi->y_mode_costs[A][L][DC_PRED] +
                          av1_cost_bit(ec_ctx->intrabc_prob, 1);

    RD_STATS rd_stats, rd_stats_uv;
    av1_subtract_plane(x, bsize, 0);
    super_block_yrd(cpi, x, &rd_stats, bsize, INT64_MAX);
    super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
    av1_merge_rd_stats(&rd_stats, &rd_stats_uv);
#if CONFIG_RD_DEBUG
    mbmi->rd_stats = rd_stats;
#endif

#if CONFIG_VAR_TX
    // TODO(aconverse@google.com): Evaluate allowing VAR TX on intrabc blocks
    const int width = block_size_wide[bsize] >> tx_size_wide_log2[0];
    const int height = block_size_high[bsize] >> tx_size_high_log2[0];
    int idx, idy;
    for (idy = 0; idy < height; ++idy)
      for (idx = 0; idx < width; ++idx)
        mbmi->inter_tx_size[idy >> 1][idx >> 1] = mbmi->tx_size;
    mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
#endif  // CONFIG_VAR_TX

    const aom_prob skip_prob = av1_get_skip_prob(cm, xd);

    RD_STATS rdc_noskip;
    av1_init_rd_stats(&rdc_noskip);
    rdc_noskip.rate =
        rate_mode + rate_mv + rd_stats.rate + av1_cost_bit(skip_prob, 0);
    rdc_noskip.dist = rd_stats.dist;
    rdc_noskip.rdcost =
        RDCOST(x->rdmult, x->rddiv, rdc_noskip.rate, rdc_noskip.dist);
    if (rdc_noskip.rdcost < best_rd) {
      best_rd = rdc_noskip.rdcost;
      best_mbmi = *mbmi;
      best_skip = x->skip;
      best_rdcost = rdc_noskip;
    }

    x->skip = 1;
    mbmi->skip = 1;
    RD_STATS rdc_skip;
    av1_init_rd_stats(&rdc_skip);
    rdc_skip.rate = rate_mode + rate_mv + av1_cost_bit(skip_prob, 1);
    rdc_skip.dist = rd_stats.sse;
    rdc_skip.rdcost = RDCOST(x->rdmult, x->rddiv, rdc_skip.rate, rdc_skip.dist);
    if (rdc_skip.rdcost < best_rd) {
      best_rd = rdc_skip.rdcost;
      best_mbmi = *mbmi;
      best_skip = x->skip;
      best_rdcost = rdc_skip;
    }
  }
  *mbmi = best_mbmi;
  *rd_cost = best_rdcost;
  x->skip = best_skip;
  return best_rd;
}
#endif  // CONFIG_INTRABC

void av1_rd_pick_intra_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x,
                               RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblockd_plane *const pd = xd->plane;
  int rate_y = 0, rate_uv = 0, rate_y_tokenonly = 0, rate_uv_tokenonly = 0;
  int y_skip = 0, uv_skip = 0;
  int64_t dist_y = 0, dist_uv = 0;
  TX_SIZE max_uv_tx_size;
  const int unify_bsize = CONFIG_CB4X4;

  ctx->skip = 0;
  xd->mi[0]->mbmi.ref_frame[0] = INTRA_FRAME;
  xd->mi[0]->mbmi.ref_frame[1] = NONE_FRAME;
#if CONFIG_INTRABC
  xd->mi[0]->mbmi.use_intrabc = 0;
  xd->mi[0]->mbmi.mv[0].as_int = 0;
#endif  // CONFIG_INTRABC

  const int64_t intra_yrd =
      (bsize >= BLOCK_8X8 || unify_bsize)
          ? rd_pick_intra_sby_mode(cpi, x, &rate_y, &rate_y_tokenonly, &dist_y,
                                   &y_skip, bsize, best_rd)
          : rd_pick_intra_sub_8x8_y_mode(cpi, x, &rate_y, &rate_y_tokenonly,
                                         &dist_y, &y_skip, best_rd);

  if (intra_yrd < best_rd) {
    max_uv_tx_size = uv_txsize_lookup[bsize][xd->mi[0]->mbmi.tx_size]
                                     [pd[1].subsampling_x][pd[1].subsampling_y];
    init_sbuv_mode(&xd->mi[0]->mbmi);
#if CONFIG_CB4X4
    if (!x->skip_chroma_rd)
      rd_pick_intra_sbuv_mode(cpi, x, &rate_uv, &rate_uv_tokenonly, &dist_uv,
                              &uv_skip, bsize, max_uv_tx_size);
#else
    rd_pick_intra_sbuv_mode(cpi, x, &rate_uv, &rate_uv_tokenonly, &dist_uv,
                            &uv_skip, AOMMAX(BLOCK_8X8, bsize), max_uv_tx_size);
#endif  // CONFIG_CB4X4

    if (y_skip && uv_skip) {
      rd_cost->rate = rate_y + rate_uv - rate_y_tokenonly - rate_uv_tokenonly +
                      av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
      rd_cost->dist = dist_y + dist_uv;
    } else {
      rd_cost->rate =
          rate_y + rate_uv + av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
      rd_cost->dist = dist_y + dist_uv;
    }
    rd_cost->rdcost = RDCOST(x->rdmult, x->rddiv, rd_cost->rate, rd_cost->dist);
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
    rd_cost->dist_y = dist_y;
#endif
  } else {
    rd_cost->rate = INT_MAX;
  }

#if CONFIG_INTRABC
  if (rd_cost->rate != INT_MAX && rd_cost->rdcost < best_rd)
    best_rd = rd_cost->rdcost;
  if (rd_pick_intrabc_mode_sb(cpi, x, rd_cost, bsize, best_rd) < best_rd) {
    ctx->skip = x->skip;  // FIXME where is the proper place to set this?!
    assert(rd_cost->rate != INT_MAX);
    rd_cost->rdcost = RDCOST(x->rdmult, x->rddiv, rd_cost->rate, rd_cost->dist);
  }
#endif
  if (rd_cost->rate == INT_MAX) return;

  ctx->mic = *xd->mi[0];
  ctx->mbmi_ext = *x->mbmi_ext;
}

// Do we have an internal image edge (e.g. formatting bars).
int av1_internal_image_edge(const AV1_COMP *cpi) {
  return (cpi->oxcf.pass == 2) &&
         ((cpi->twopass.this_frame_stats.inactive_zone_rows > 0) ||
          (cpi->twopass.this_frame_stats.inactive_zone_cols > 0));
}

// Checks to see if a super block is on a horizontal image edge.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
int av1_active_h_edge(const AV1_COMP *cpi, int mi_row, int mi_step) {
  int top_edge = 0;
  int bottom_edge = cpi->common.mi_rows;
  int is_active_h_edge = 0;

  // For two pass account for any formatting bars detected.
  if (cpi->oxcf.pass == 2) {
    const TWO_PASS *const twopass = &cpi->twopass;

    // The inactive region is specified in MBs not mi units.
    // The image edge is in the following MB row.
    top_edge += (int)(twopass->this_frame_stats.inactive_zone_rows * 2);

    bottom_edge -= (int)(twopass->this_frame_stats.inactive_zone_rows * 2);
    bottom_edge = AOMMAX(top_edge, bottom_edge);
  }

  if (((top_edge >= mi_row) && (top_edge < (mi_row + mi_step))) ||
      ((bottom_edge >= mi_row) && (bottom_edge < (mi_row + mi_step)))) {
    is_active_h_edge = 1;
  }
  return is_active_h_edge;
}

// Checks to see if a super block is on a vertical image edge.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
int av1_active_v_edge(const AV1_COMP *cpi, int mi_col, int mi_step) {
  int left_edge = 0;
  int right_edge = cpi->common.mi_cols;
  int is_active_v_edge = 0;

  // For two pass account for any formatting bars detected.
  if (cpi->oxcf.pass == 2) {
    const TWO_PASS *const twopass = &cpi->twopass;

    // The inactive region is specified in MBs not mi units.
    // The image edge is in the following MB row.
    left_edge += (int)(twopass->this_frame_stats.inactive_zone_cols * 2);

    right_edge -= (int)(twopass->this_frame_stats.inactive_zone_cols * 2);
    right_edge = AOMMAX(left_edge, right_edge);
  }

  if (((left_edge >= mi_col) && (left_edge < (mi_col + mi_step))) ||
      ((right_edge >= mi_col) && (right_edge < (mi_col + mi_step)))) {
    is_active_v_edge = 1;
  }
  return is_active_v_edge;
}

// Checks to see if a super block is at the edge of the active image.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
int av1_active_edge_sb(const AV1_COMP *cpi, int mi_row, int mi_col) {
  return av1_active_h_edge(cpi, mi_row, cpi->common.mib_size) ||
         av1_active_v_edge(cpi, mi_col, cpi->common.mib_size);
}

#if CONFIG_PALETTE
static void restore_uv_color_map(const AV1_COMP *const cpi, MACROBLOCK *x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  float *const data = x->palette_buffer->kmeans_data_buf;
  float centroids[2 * PALETTE_MAX_SIZE];
  uint8_t *const color_map = xd->plane[1].color_index_map;
  int r, c;
#if CONFIG_HIGHBITDEPTH
  const uint16_t *const src_u16 = CONVERT_TO_SHORTPTR(src_u);
  const uint16_t *const src_v16 = CONVERT_TO_SHORTPTR(src_v);
#endif  // CONFIG_HIGHBITDEPTH
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(bsize, 1, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);
  (void)cpi;

  for (r = 0; r < rows; ++r) {
    for (c = 0; c < cols; ++c) {
#if CONFIG_HIGHBITDEPTH
      if (cpi->common.use_highbitdepth) {
        data[(r * cols + c) * 2] = src_u16[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v16[r * src_stride + c];
      } else {
#endif  // CONFIG_HIGHBITDEPTH
        data[(r * cols + c) * 2] = src_u[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v[r * src_stride + c];
#if CONFIG_HIGHBITDEPTH
      }
#endif  // CONFIG_HIGHBITDEPTH
    }
  }

  for (r = 1; r < 3; ++r) {
    for (c = 0; c < pmi->palette_size[1]; ++c) {
      centroids[c * 2 + r - 1] = pmi->palette_colors[r * PALETTE_MAX_SIZE + c];
    }
  }

  av1_calc_indices(data, centroids, color_map, rows * cols,
                   pmi->palette_size[1], 2);
  extend_palette_color_map(color_map, cols, rows, plane_block_width,
                           plane_block_height);
}
#endif  // CONFIG_PALETTE

#if CONFIG_FILTER_INTRA
static void pick_filter_intra_interframe(
    const AV1_COMP *cpi, MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
    BLOCK_SIZE bsize, int mi_row, int mi_col, int *rate_uv_intra,
    int *rate_uv_tokenonly, int64_t *dist_uv, int *skip_uv,
    PREDICTION_MODE *mode_uv, FILTER_INTRA_MODE_INFO *filter_intra_mode_info_uv,
#if CONFIG_EXT_INTRA
    int8_t *uv_angle_delta,
#endif  // CONFIG_EXT_INTRA
#if CONFIG_PALETTE
    PALETTE_MODE_INFO *pmi_uv, int palette_ctx,
#endif  // CONFIG_PALETTE
    int skip_mask, unsigned int *ref_costs_single, int64_t *best_rd,
    int64_t *best_intra_rd, PREDICTION_MODE *best_intra_mode,
    int *best_mode_index, int *best_skip2, int *best_mode_skippable,
#if CONFIG_SUPERTX
    int *returnrate_nocoef,
#endif  // CONFIG_SUPERTX
    int64_t *best_pred_rd, MB_MODE_INFO *best_mbmode, RD_STATS *rd_cost) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
#if CONFIG_PALETTE
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
#endif  // CONFIG_PALETTE
  int rate2 = 0, rate_y = INT_MAX, skippable = 0, rate_uv, rate_dummy, i;
  int dc_mode_index;
  const int *const intra_mode_cost = cpi->mbmode_cost[size_group_lookup[bsize]];
  int64_t distortion2 = 0, distortion_y = 0, this_rd = *best_rd;
  int64_t distortion_uv, model_rd = INT64_MAX;
  TX_SIZE uv_tx;

  for (i = 0; i < MAX_MODES; ++i)
    if (av1_mode_order[i].mode == DC_PRED &&
        av1_mode_order[i].ref_frame[0] == INTRA_FRAME)
      break;
  dc_mode_index = i;
  assert(i < MAX_MODES);

  // TODO(huisu): use skip_mask for further speedup.
  (void)skip_mask;
  mbmi->mode = DC_PRED;
  mbmi->uv_mode = DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  if (!rd_pick_filter_intra_sby(cpi, x, &rate_dummy, &rate_y, &distortion_y,
                                &skippable, bsize, intra_mode_cost[mbmi->mode],
                                &this_rd, &model_rd, 0)) {
    return;
  }
  if (rate_y == INT_MAX) return;

  uv_tx = uv_txsize_lookup[bsize][mbmi->tx_size][xd->plane[1].subsampling_x]
                          [xd->plane[1].subsampling_y];
  if (rate_uv_intra[uv_tx] == INT_MAX) {
    choose_intra_uv_mode(cpi, x, ctx, bsize, uv_tx, &rate_uv_intra[uv_tx],
                         &rate_uv_tokenonly[uv_tx], &dist_uv[uv_tx],
                         &skip_uv[uv_tx], &mode_uv[uv_tx]);
#if CONFIG_PALETTE
    if (cm->allow_screen_content_tools) pmi_uv[uv_tx] = *pmi;
#endif  // CONFIG_PALETTE
    filter_intra_mode_info_uv[uv_tx] = mbmi->filter_intra_mode_info;
#if CONFIG_EXT_INTRA
    uv_angle_delta[uv_tx] = mbmi->angle_delta[1];
#endif  // CONFIG_EXT_INTRA
  }

  rate_uv = rate_uv_tokenonly[uv_tx];
  distortion_uv = dist_uv[uv_tx];
  skippable = skippable && skip_uv[uv_tx];
  mbmi->uv_mode = mode_uv[uv_tx];
#if CONFIG_PALETTE
  if (cm->allow_screen_content_tools) {
    pmi->palette_size[1] = pmi_uv[uv_tx].palette_size[1];
    memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
           pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
           2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
  }
#endif  // CONFIG_PALETTE
#if CONFIG_EXT_INTRA
  mbmi->angle_delta[1] = uv_angle_delta[uv_tx];
#endif  // CONFIG_EXT_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] =
      filter_intra_mode_info_uv[uv_tx].use_filter_intra_mode[1];
  if (filter_intra_mode_info_uv[uv_tx].use_filter_intra_mode[1]) {
    mbmi->filter_intra_mode_info.filter_intra_mode[1] =
        filter_intra_mode_info_uv[uv_tx].filter_intra_mode[1];
  }

  rate2 = rate_y + intra_mode_cost[mbmi->mode] + rate_uv +
          cpi->intra_uv_mode_cost[mbmi->mode][mbmi->uv_mode];
#if CONFIG_PALETTE
  if (cpi->common.allow_screen_content_tools && mbmi->mode == DC_PRED &&
      bsize >= BLOCK_8X8)
    rate2 += av1_cost_bit(
        av1_default_palette_y_mode_prob[bsize - BLOCK_8X8][palette_ctx], 0);
#endif  // CONFIG_PALETTE

  if (!xd->lossless[mbmi->segment_id]) {
    // super_block_yrd above includes the cost of the tx_size in the
    // tokenonly rate, but for intra blocks, tx_size is always coded
    // (prediction granularity), so we account for it in the full rate,
    // not the tokenonly rate.
    rate_y -= tx_size_cost(cpi, x, bsize, mbmi->tx_size);
  }

  rate2 += av1_cost_bit(cm->fc->filter_intra_probs[0],
                        mbmi->filter_intra_mode_info.use_filter_intra_mode[0]);
  rate2 += write_uniform_cost(
      FILTER_INTRA_MODES, mbmi->filter_intra_mode_info.filter_intra_mode[0]);
#if CONFIG_EXT_INTRA
  if (av1_is_directional_mode(mbmi->uv_mode, bsize)) {
    rate2 += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                MAX_ANGLE_DELTA + mbmi->angle_delta[1]);
  }
#endif  // CONFIG_EXT_INTRA
  if (mbmi->mode == DC_PRED) {
    rate2 +=
        av1_cost_bit(cpi->common.fc->filter_intra_probs[1],
                     mbmi->filter_intra_mode_info.use_filter_intra_mode[1]);
    if (mbmi->filter_intra_mode_info.use_filter_intra_mode[1])
      rate2 +=
          write_uniform_cost(FILTER_INTRA_MODES,
                             mbmi->filter_intra_mode_info.filter_intra_mode[1]);
  }
  distortion2 = distortion_y + distortion_uv;
  av1_encode_intra_block_plane((AV1_COMMON *)cm, x, bsize, 0, 0, mi_row,
                               mi_col);

  rate2 += ref_costs_single[INTRA_FRAME];

  if (skippable) {
    rate2 -= (rate_y + rate_uv);
    rate_y = 0;
    rate_uv = 0;
    rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
  } else {
    rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
  }
  this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);

  if (this_rd < *best_intra_rd) {
    *best_intra_rd = this_rd;
    *best_intra_mode = mbmi->mode;
  }
  for (i = 0; i < REFERENCE_MODES; ++i)
    best_pred_rd[i] = AOMMIN(best_pred_rd[i], this_rd);

  if (this_rd < *best_rd) {
    *best_mode_index = dc_mode_index;
    mbmi->mv[0].as_int = 0;
    rd_cost->rate = rate2;
#if CONFIG_SUPERTX
    if (x->skip)
      *returnrate_nocoef = rate2;
    else
      *returnrate_nocoef = rate2 - rate_y - rate_uv;
    *returnrate_nocoef -= av1_cost_bit(av1_get_skip_prob(cm, xd), skippable);
    *returnrate_nocoef -= av1_cost_bit(av1_get_intra_inter_prob(cm, xd),
                                       mbmi->ref_frame[0] != INTRA_FRAME);
#endif  // CONFIG_SUPERTX
    rd_cost->dist = distortion2;
    rd_cost->rdcost = this_rd;
    *best_rd = this_rd;
    *best_mbmode = *mbmi;
    *best_skip2 = 0;
    *best_mode_skippable = skippable;
  }
}
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_MOTION_VAR
static void calc_target_weighted_pred(const AV1_COMMON *cm, const MACROBLOCK *x,
                                      const MACROBLOCKD *xd, int mi_row,
                                      int mi_col, const uint8_t *above,
                                      int above_stride, const uint8_t *left,
                                      int left_stride);
#endif  // CONFIG_MOTION_VAR

void av1_rd_pick_inter_mode_sb(const AV1_COMP *cpi, TileDataEnc *tile_data,
                               MACROBLOCK *x, int mi_row, int mi_col,
                               RD_STATS *rd_cost,
#if CONFIG_SUPERTX
                               int *returnrate_nocoef,
#endif  // CONFIG_SUPERTX
                               BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                               int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  const RD_OPT *const rd_opt = &cpi->rd;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
#if CONFIG_PALETTE
  const int try_palette =
      cpi->common.allow_screen_content_tools && bsize >= BLOCK_8X8;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
#endif  // CONFIG_PALETTE
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const struct segmentation *const seg = &cm->seg;
  PREDICTION_MODE this_mode;
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int comp_pred, i, k;
  int_mv frame_mv[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];
  struct buf_2d yv12_mb[TOTAL_REFS_PER_FRAME][MAX_MB_PLANE];
  int_mv single_newmv[TOTAL_REFS_PER_FRAME] = { { 0 } };
#if CONFIG_EXT_INTER
  int single_newmv_rate[TOTAL_REFS_PER_FRAME] = { 0 };
  int64_t modelled_rd[MB_MODE_COUNT][TOTAL_REFS_PER_FRAME];
#endif  // CONFIG_EXT_INTER
  static const int flag_list[TOTAL_REFS_PER_FRAME] = {
    0,
    AOM_LAST_FLAG,
#if CONFIG_EXT_REFS
    AOM_LAST2_FLAG,
    AOM_LAST3_FLAG,
#endif  // CONFIG_EXT_REFS
    AOM_GOLD_FLAG,
#if CONFIG_EXT_REFS
    AOM_BWD_FLAG,
#endif  // CONFIG_EXT_REFS
    AOM_ALT_FLAG
  };
  int64_t best_rd = best_rd_so_far;
  int best_rate_y = INT_MAX, best_rate_uv = INT_MAX;
  int64_t best_pred_diff[REFERENCE_MODES];
  int64_t best_pred_rd[REFERENCE_MODES];
  MB_MODE_INFO best_mbmode;
  int rate_skip0 = av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
  int rate_skip1 = av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
  int best_mode_skippable = 0;
  int midx, best_mode_index = -1;
  unsigned int ref_costs_single[TOTAL_REFS_PER_FRAME];
  unsigned int ref_costs_comp[TOTAL_REFS_PER_FRAME];
  aom_prob comp_mode_p;
  int64_t best_intra_rd = INT64_MAX;
  unsigned int best_pred_sse = UINT_MAX;
  PREDICTION_MODE best_intra_mode = DC_PRED;
  int rate_uv_intra[TX_SIZES_ALL], rate_uv_tokenonly[TX_SIZES_ALL];
  int64_t dist_uvs[TX_SIZES_ALL];
  int skip_uvs[TX_SIZES_ALL];
  PREDICTION_MODE mode_uv[TX_SIZES_ALL];
#if CONFIG_PALETTE
  PALETTE_MODE_INFO pmi_uv[TX_SIZES_ALL];
#endif  // CONFIG_PALETTE
#if CONFIG_EXT_INTRA
  int8_t uv_angle_delta[TX_SIZES_ALL];
  int is_directional_mode, angle_stats_ready = 0;
  uint8_t directional_mode_skip_mask[INTRA_MODES];
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  int8_t dc_skipped = 1;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info_uv[TX_SIZES_ALL];
#endif  // CONFIG_FILTER_INTRA
  const int intra_cost_penalty = av1_get_intra_cost_penalty(
      cm->base_qindex, cm->y_dc_delta_q, cm->bit_depth);
  const int *const intra_mode_cost = cpi->mbmode_cost[size_group_lookup[bsize]];
  int best_skip2 = 0;
  uint8_t ref_frame_skip_mask[2] = { 0 };
  uint32_t mode_skip_mask[TOTAL_REFS_PER_FRAME] = { 0 };
#if CONFIG_EXT_INTER && CONFIG_INTERINTRA
  MV_REFERENCE_FRAME best_single_inter_ref = LAST_FRAME;
  int64_t best_single_inter_rd = INT64_MAX;
#endif  // CONFIG_EXT_INTER && CONFIG_INTERINTRA
  int mode_skip_start = sf->mode_skip_start + 1;
  const int *const rd_threshes = rd_opt->threshes[segment_id][bsize];
  const int *const rd_thresh_freq_fact = tile_data->thresh_freq_fact[bsize];
  int64_t mode_threshold[MAX_MODES];
  int *mode_map = tile_data->mode_map[bsize];
  const int mode_search_skip_flags = sf->mode_search_skip_flags;
#if CONFIG_PVQ
  od_rollback_buffer pre_buf;
#endif  // CONFIG_PVQ

  HandleInterModeArgs args = {
#if CONFIG_MOTION_VAR
    { NULL },
    { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE },
    { NULL },
    { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE },
#endif  // CONFIG_MOTION_VAR
#if CONFIG_EXT_INTER
    NULL,
    NULL,
    NULL,
#else   // CONFIG_EXT_INTER
    NULL,
#endif  // CONFIG_EXT_INTER
    { { 0 } },
  };

#if CONFIG_PALETTE || CONFIG_EXT_INTRA
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
#endif  // CONFIG_PALETTE || CONFIG_EXT_INTRA
#if CONFIG_PALETTE
  int palette_ctx = 0;
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
#endif  // CONFIG_PALETTE
#if CONFIG_MOTION_VAR
  int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    args.above_pred_buf[0] = CONVERT_TO_BYTEPTR(x->above_pred_buf);
    args.above_pred_buf[1] =
        CONVERT_TO_BYTEPTR(x->above_pred_buf + MAX_SB_SQUARE * len);
    args.above_pred_buf[2] =
        CONVERT_TO_BYTEPTR(x->above_pred_buf + 2 * MAX_SB_SQUARE * len);
    args.left_pred_buf[0] = CONVERT_TO_BYTEPTR(x->left_pred_buf);
    args.left_pred_buf[1] =
        CONVERT_TO_BYTEPTR(x->left_pred_buf + MAX_SB_SQUARE * len);
    args.left_pred_buf[2] =
        CONVERT_TO_BYTEPTR(x->left_pred_buf + 2 * MAX_SB_SQUARE * len);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    args.above_pred_buf[0] = x->above_pred_buf;
    args.above_pred_buf[1] = x->above_pred_buf + MAX_SB_SQUARE;
    args.above_pred_buf[2] = x->above_pred_buf + 2 * MAX_SB_SQUARE;
    args.left_pred_buf[0] = x->left_pred_buf;
    args.left_pred_buf[1] = x->left_pred_buf + MAX_SB_SQUARE;
    args.left_pred_buf[2] = x->left_pred_buf + 2 * MAX_SB_SQUARE;
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_MOTION_VAR

  av1_zero(best_mbmode);

#if CONFIG_PALETTE
  av1_zero(pmi_uv);
  if (try_palette) {
    if (above_mi)
      palette_ctx += (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    if (left_mi)
      palette_ctx += (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
  }
#endif  // CONFIG_PALETTE

  estimate_ref_frame_costs(cm, xd, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < REFERENCE_MODES; ++i) best_pred_rd[i] = INT64_MAX;
  for (i = 0; i < TX_SIZES_ALL; i++) rate_uv_intra[i] = INT_MAX;
  for (i = 0; i < TOTAL_REFS_PER_FRAME; ++i) x->pred_sse[i] = INT_MAX;
  for (i = 0; i < MB_MODE_COUNT; ++i) {
    for (k = 0; k < TOTAL_REFS_PER_FRAME; ++k) {
      args.single_filter[i][k] = SWITCHABLE;
    }
  }

  rd_cost->rate = INT_MAX;
#if CONFIG_SUPERTX
  *returnrate_nocoef = INT_MAX;
#endif  // CONFIG_SUPERTX

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    x->pred_mv_sad[ref_frame] = INT_MAX;
    x->mbmi_ext->mode_context[ref_frame] = 0;
#if CONFIG_EXT_INTER
    x->mbmi_ext->compound_mode_context[ref_frame] = 0;
#endif  // CONFIG_EXT_INTER
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      assert(get_ref_frame_buffer(cpi, ref_frame) != NULL);
      setup_buffer_inter(cpi, x, ref_frame, bsize, mi_row, mi_col,
                         frame_mv[NEARESTMV], frame_mv[NEARMV], yv12_mb);
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
#if CONFIG_GLOBAL_MOTION
    frame_mv[ZEROMV][ref_frame].as_int =
        gm_get_motion_vector(&cm->global_motion[ref_frame],
                             cm->allow_high_precision_mv, bsize, mi_col, mi_row,
                             0)
            .as_int;
#else   // CONFIG_GLOBAL_MOTION
    frame_mv[ZEROMV][ref_frame].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_EXT_INTER
    frame_mv[NEW_NEWMV][ref_frame].as_int = INVALID_MV;
#if CONFIG_GLOBAL_MOTION
    frame_mv[ZERO_ZEROMV][ref_frame].as_int =
        gm_get_motion_vector(&cm->global_motion[ref_frame],
                             cm->allow_high_precision_mv, bsize, mi_col, mi_row,
                             0)
            .as_int;
#else   // CONFIG_GLOBAL_MOTION
    frame_mv[ZERO_ZEROMV][ref_frame].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION
#endif  // CONFIG_EXT_INTER
  }

  for (; ref_frame < MODE_CTX_REF_FRAMES; ++ref_frame) {
    MODE_INFO *const mi = xd->mi[0];
    int_mv *const candidates = x->mbmi_ext->ref_mvs[ref_frame];
    x->mbmi_ext->mode_context[ref_frame] = 0;
    av1_find_mv_refs(cm, xd, mi, ref_frame, &mbmi_ext->ref_mv_count[ref_frame],
                     mbmi_ext->ref_mv_stack[ref_frame],
#if CONFIG_EXT_INTER
                     mbmi_ext->compound_mode_context,
#endif  // CONFIG_EXT_INTER
                     candidates, mi_row, mi_col, NULL, NULL,
                     mbmi_ext->mode_context);
    if (mbmi_ext->ref_mv_count[ref_frame] < 2) {
      MV_REFERENCE_FRAME rf[2];
      av1_set_ref_frame(rf, ref_frame);
      if (mbmi_ext->ref_mvs[rf[0]][0].as_int !=
              frame_mv[ZEROMV][rf[0]].as_int ||
          mbmi_ext->ref_mvs[rf[0]][1].as_int !=
              frame_mv[ZEROMV][rf[0]].as_int ||
          mbmi_ext->ref_mvs[rf[1]][0].as_int !=
              frame_mv[ZEROMV][rf[1]].as_int ||
          mbmi_ext->ref_mvs[rf[1]][1].as_int != frame_mv[ZEROMV][rf[1]].as_int)
        mbmi_ext->mode_context[ref_frame] &= ~(1 << ALL_ZERO_FLAG_OFFSET);
    }
  }

#if CONFIG_MOTION_VAR
  av1_count_overlappable_neighbors(cm, xd, mi_row, mi_col);

  if (check_num_overlappable_neighbors(mbmi) &&
      is_motion_variation_allowed_bsize(bsize)) {
    av1_build_prediction_by_above_preds(cm, xd, mi_row, mi_col,
                                        args.above_pred_buf, dst_width1,
                                        dst_height1, args.above_pred_stride);
    av1_build_prediction_by_left_preds(cm, xd, mi_row, mi_col,
                                       args.left_pred_buf, dst_width2,
                                       dst_height2, args.left_pred_stride);
    av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                         mi_col);
    calc_target_weighted_pred(cm, x, xd, mi_row, mi_col, args.above_pred_buf[0],
                              args.above_pred_stride[0], args.left_pred_buf[0],
                              args.left_pred_stride[0]);
  }
#endif  // CONFIG_MOTION_VAR

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (!(cpi->ref_frame_flags & flag_list[ref_frame])) {
// Skip checking missing references in both single and compound reference
// modes. Note that a mode will be skipped iff both reference frames
// are masked out.
#if CONFIG_EXT_REFS
      if (ref_frame == BWDREF_FRAME || ref_frame == ALTREF_FRAME) {
        ref_frame_skip_mask[0] |= (1 << ref_frame);
        ref_frame_skip_mask[1] |= ((1 << ref_frame) | 0x01);
      } else {
#endif  // CONFIG_EXT_REFS
        ref_frame_skip_mask[0] |= (1 << ref_frame);
        ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
#if CONFIG_EXT_REFS
      }
#endif  // CONFIG_EXT_REFS
    } else {
      for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
        // Skip fixed mv modes for poor references
        if ((x->pred_mv_sad[ref_frame] >> 2) > x->pred_mv_sad[i]) {
          mode_skip_mask[ref_frame] |= INTER_NEAREST_NEAR_ZERO;
          break;
        }
      }
    }
    // If the segment reference frame feature is enabled....
    // then do nothing if the current ref frame is not allowed..
    if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
        get_segdata(seg, segment_id, SEG_LVL_REF_FRAME) != (int)ref_frame) {
      ref_frame_skip_mask[0] |= (1 << ref_frame);
      ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
    }
  }

  // Disable this drop out case if the ref frame
  // segment level feature is enabled for this segment. This is to
  // prevent the possibility that we end up unable to pick any mode.
  if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) {
    // Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
    // unless ARNR filtering is enabled in which case we want
    // an unfiltered alternative. We allow near/nearest as well
    // because they may result in zero-zero MVs but be cheaper.
    if (cpi->rc.is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0)) {
      int_mv zeromv;
      ref_frame_skip_mask[0] = (1 << LAST_FRAME) |
#if CONFIG_EXT_REFS
                               (1 << LAST2_FRAME) | (1 << LAST3_FRAME) |
                               (1 << BWDREF_FRAME) |
#endif  // CONFIG_EXT_REFS
                               (1 << GOLDEN_FRAME);
      ref_frame_skip_mask[1] = SECOND_REF_FRAME_MASK;
      // TODO(zoeliu): To further explore whether following needs to be done for
      //               BWDREF_FRAME as well.
      mode_skip_mask[ALTREF_FRAME] = ~INTER_NEAREST_NEAR_ZERO;
#if CONFIG_GLOBAL_MOTION
      zeromv.as_int = gm_get_motion_vector(&cm->global_motion[ALTREF_FRAME],
                                           cm->allow_high_precision_mv, bsize,
                                           mi_col, mi_row, 0)
                          .as_int;
#else
      zeromv.as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION
      if (frame_mv[NEARMV][ALTREF_FRAME].as_int != zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARMV);
      if (frame_mv[NEARESTMV][ALTREF_FRAME].as_int != zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARESTMV);
#if CONFIG_EXT_INTER
      if (frame_mv[NEAREST_NEARESTMV][ALTREF_FRAME].as_int != zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEAREST_NEARESTMV);
      if (frame_mv[NEAR_NEARMV][ALTREF_FRAME].as_int != zeromv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEAR_NEARMV);
#endif  // CONFIG_EXT_INTER
    }
  }

  if (cpi->rc.is_src_frame_alt_ref) {
    if (sf->alt_ref_search_fp) {
      assert(cpi->ref_frame_flags & flag_list[ALTREF_FRAME]);
      mode_skip_mask[ALTREF_FRAME] = 0;
      ref_frame_skip_mask[0] = ~(1 << ALTREF_FRAME);
      ref_frame_skip_mask[1] = SECOND_REF_FRAME_MASK;
    }
  }

  if (sf->alt_ref_search_fp)
    if (!cm->show_frame && x->pred_mv_sad[GOLDEN_FRAME] < INT_MAX)
      if (x->pred_mv_sad[ALTREF_FRAME] > (x->pred_mv_sad[GOLDEN_FRAME] << 1))
        mode_skip_mask[ALTREF_FRAME] |= INTER_ALL;

  if (sf->adaptive_mode_search) {
    if (cm->show_frame && !cpi->rc.is_src_frame_alt_ref &&
        cpi->rc.frames_since_golden >= 3)
      if ((x->pred_mv_sad[GOLDEN_FRAME] >> 1) > x->pred_mv_sad[LAST_FRAME])
        mode_skip_mask[GOLDEN_FRAME] |= INTER_ALL;
  }

  if (bsize > sf->max_intra_bsize) {
    ref_frame_skip_mask[0] |= (1 << INTRA_FRAME);
    ref_frame_skip_mask[1] |= (1 << INTRA_FRAME);
  }

  mode_skip_mask[INTRA_FRAME] |=
      ~(sf->intra_y_mode_mask[max_txsize_lookup[bsize]]);

  for (i = 0; i <= LAST_NEW_MV_INDEX; ++i) mode_threshold[i] = 0;
  for (i = LAST_NEW_MV_INDEX + 1; i < MAX_MODES; ++i)
    mode_threshold[i] = ((int64_t)rd_threshes[i] * rd_thresh_freq_fact[i]) >> 5;

  midx = sf->schedule_mode_search ? mode_skip_start : 0;
  while (midx > 4) {
    uint8_t end_pos = 0;
    for (i = 5; i < midx; ++i) {
      if (mode_threshold[mode_map[i - 1]] > mode_threshold[mode_map[i]]) {
        uint8_t tmp = mode_map[i];
        mode_map[i] = mode_map[i - 1];
        mode_map[i - 1] = tmp;
        end_pos = i;
      }
    }
    midx = end_pos;
  }

  if (cpi->sf.tx_type_search.fast_intra_tx_type_search)
    x->use_default_intra_tx_type = 1;
  else
    x->use_default_intra_tx_type = 0;

  if (cpi->sf.tx_type_search.fast_inter_tx_type_search)
    x->use_default_inter_tx_type = 1;
  else
    x->use_default_inter_tx_type = 0;
#if CONFIG_PVQ
  od_encode_checkpoint(&x->daala_enc, &pre_buf);
#endif  // CONFIG_PVQ
#if CONFIG_EXT_INTER
  for (i = 0; i < MB_MODE_COUNT; ++i)
    for (ref_frame = 0; ref_frame < TOTAL_REFS_PER_FRAME; ++ref_frame)
      modelled_rd[i][ref_frame] = INT64_MAX;
#endif  // CONFIG_EXT_INTER

  for (midx = 0; midx < MAX_MODES; ++midx) {
    int mode_index;
    int mode_excluded = 0;
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int compmode_cost = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0, distortion_y = 0, distortion_uv = 0;
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
    int64_t distortion2_y = 0;
    int64_t total_sse_y = INT64_MAX;
#endif
    int skippable = 0;
    int this_skip2 = 0;
    int64_t total_sse = INT64_MAX;
    uint8_t ref_frame_type;
#if CONFIG_PVQ
    od_encode_rollback(&x->daala_enc, &pre_buf);
#endif  // CONFIG_PVQ
    mode_index = mode_map[midx];
    this_mode = av1_mode_order[mode_index].mode;
    ref_frame = av1_mode_order[mode_index].ref_frame[0];
    second_ref_frame = av1_mode_order[mode_index].ref_frame[1];
    mbmi->ref_mv_idx = 0;

#if CONFIG_EXT_INTER
    if (ref_frame > INTRA_FRAME && second_ref_frame == INTRA_FRAME) {
      // Mode must by compatible
      if (!is_interintra_allowed_mode(this_mode)) continue;
      if (!is_interintra_allowed_bsize(bsize)) continue;
    }

    if (is_inter_compound_mode(this_mode)) {
      frame_mv[this_mode][ref_frame].as_int =
          frame_mv[compound_ref0_mode(this_mode)][ref_frame].as_int;
      frame_mv[this_mode][second_ref_frame].as_int =
          frame_mv[compound_ref1_mode(this_mode)][second_ref_frame].as_int;
    }
#endif  // CONFIG_EXT_INTER

    // Look at the reference frame of the best mode so far and set the
    // skip mask to look at a subset of the remaining modes.
    if (midx == mode_skip_start && best_mode_index >= 0) {
      switch (best_mbmode.ref_frame[0]) {
        case INTRA_FRAME: break;
        case LAST_FRAME:
          ref_frame_skip_mask[0] |= LAST_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
#if CONFIG_EXT_REFS
        case LAST2_FRAME:
          ref_frame_skip_mask[0] |= LAST2_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case LAST3_FRAME:
          ref_frame_skip_mask[0] |= LAST3_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
#endif  // CONFIG_EXT_REFS
        case GOLDEN_FRAME:
          ref_frame_skip_mask[0] |= GOLDEN_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
#if CONFIG_EXT_REFS
        case BWDREF_FRAME:
          ref_frame_skip_mask[0] |= BWDREF_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
#endif  // CONFIG_EXT_REFS
        case ALTREF_FRAME: ref_frame_skip_mask[0] |= ALTREF_FRAME_MODE_MASK;
#if CONFIG_EXT_REFS
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
#endif  // CONFIG_EXT_REFS
          break;
        case NONE_FRAME:
        case TOTAL_REFS_PER_FRAME:
          assert(0 && "Invalid Reference frame");
          break;
      }
    }

    if ((ref_frame_skip_mask[0] & (1 << ref_frame)) &&
        (ref_frame_skip_mask[1] & (1 << AOMMAX(0, second_ref_frame))))
      continue;

    if (mode_skip_mask[ref_frame] & (1 << this_mode)) continue;

    // Test best rd so far against threshold for trying this mode.
    if (best_mode_skippable && sf->schedule_mode_search)
      mode_threshold[mode_index] <<= 1;

    if (best_rd < mode_threshold[mode_index]) continue;

    // This is only used in motion vector unit test.
    if (cpi->oxcf.motion_vector_unit_test && ref_frame == INTRA_FRAME) continue;

#if CONFIG_ONE_SIDED_COMPOUND  // Changes LL bitstream
#if CONFIG_EXT_REFS
    if (cpi->oxcf.pass == 0) {
      // Complexity-compression trade-offs
      // if (ref_frame == ALTREF_FRAME) continue;
      // if (ref_frame == BWDREF_FRAME) continue;
      if (second_ref_frame == ALTREF_FRAME) continue;
      // if (second_ref_frame == BWDREF_FRAME) continue;
    }
#endif
#endif
    comp_pred = second_ref_frame > INTRA_FRAME;
    if (comp_pred) {
      if (!cpi->allow_comp_inter_inter) continue;

      // Skip compound inter modes if ARF is not available.
      if (!(cpi->ref_frame_flags & flag_list[second_ref_frame])) continue;

      // Do not allow compound prediction if the segment level reference frame
      // feature is in use as in this case there can only be one reference.
      if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) continue;

      if ((mode_search_skip_flags & FLAG_SKIP_COMP_BESTINTRA) &&
          best_mode_index >= 0 && best_mbmode.ref_frame[0] == INTRA_FRAME)
        continue;

      mode_excluded = cm->reference_mode == SINGLE_REFERENCE;
    } else {
      if (ref_frame != INTRA_FRAME)
        mode_excluded = cm->reference_mode == COMPOUND_REFERENCE;
    }

    if (ref_frame == INTRA_FRAME) {
      if (sf->adaptive_mode_search)
        if ((x->source_variance << num_pels_log2_lookup[bsize]) > best_pred_sse)
          continue;

      if (this_mode != DC_PRED) {
        // Disable intra modes other than DC_PRED for blocks with low variance
        // Threshold for intra skipping based on source variance
        // TODO(debargha): Specialize the threshold for super block sizes
        const unsigned int skip_intra_var_thresh = 64;
        if ((mode_search_skip_flags & FLAG_SKIP_INTRA_LOWVAR) &&
            x->source_variance < skip_intra_var_thresh)
          continue;
        // Only search the oblique modes if the best so far is
        // one of the neighboring directional modes
        if ((mode_search_skip_flags & FLAG_SKIP_INTRA_BESTINTER) &&
            (this_mode >= D45_PRED && this_mode <= TM_PRED)) {
          if (best_mode_index >= 0 && best_mbmode.ref_frame[0] > INTRA_FRAME)
            continue;
        }
        if (mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
          if (conditional_skipintra(this_mode, best_intra_mode)) continue;
        }
      }
#if CONFIG_GLOBAL_MOTION
    } else if (cm->global_motion[ref_frame].wmtype == IDENTITY &&
               (!comp_pred ||
                cm->global_motion[second_ref_frame].wmtype == IDENTITY)) {
#else   // CONFIG_GLOBAL_MOTION
    } else {
#endif  // CONFIG_GLOBAL_MOTION
      const MV_REFERENCE_FRAME ref_frames[2] = { ref_frame, second_ref_frame };
      if (!check_best_zero_mv(cpi, mbmi_ext->mode_context,
#if CONFIG_EXT_INTER
                              mbmi_ext->compound_mode_context,
#endif  // CONFIG_EXT_INTER
                              frame_mv, this_mode, ref_frames, bsize, -1,
                              mi_row, mi_col))
        continue;
    }

    mbmi->mode = this_mode;
    mbmi->uv_mode = DC_PRED;
    mbmi->ref_frame[0] = ref_frame;
    mbmi->ref_frame[1] = second_ref_frame;
#if CONFIG_PALETTE
    pmi->palette_size[0] = 0;
    pmi->palette_size[1] = 0;
#endif  // CONFIG_PALETTE
#if CONFIG_FILTER_INTRA
    mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
    mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
        // Evaluate all sub-pel filters irrespective of whether we can use
        // them for this frame.

    set_default_interp_filters(mbmi, cm->interp_filter);

    mbmi->mv[0].as_int = mbmi->mv[1].as_int = 0;
    mbmi->motion_mode = SIMPLE_TRANSLATION;

    x->skip = 0;
    set_ref_ptrs(cm, xd, ref_frame, second_ref_frame);

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred) xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

#if CONFIG_EXT_INTER && CONFIG_INTERINTRA
    mbmi->interintra_mode = (INTERINTRA_MODE)(II_DC_PRED - 1);
#endif  // CONFIG_EXT_INTER && CONFIG_INTERINTRA

    if (ref_frame == INTRA_FRAME) {
      RD_STATS rd_stats_y;
      TX_SIZE uv_tx;
      struct macroblockd_plane *const pd = &xd->plane[1];
#if CONFIG_EXT_INTRA
      is_directional_mode = av1_is_directional_mode(mbmi->mode, bsize);
      if (is_directional_mode) {
        int rate_dummy;
        int64_t model_rd = INT64_MAX;
        if (!angle_stats_ready) {
          const int src_stride = x->plane[0].src.stride;
          const uint8_t *src = x->plane[0].src.buf;
#if CONFIG_HIGHBITDEPTH
          if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
            highbd_angle_estimation(src, src_stride, rows, cols, bsize,
                                    directional_mode_skip_mask);
          else
#endif  // CONFIG_HIGHBITDEPTH
            angle_estimation(src, src_stride, rows, cols, bsize,
                             directional_mode_skip_mask);
          angle_stats_ready = 1;
        }
        if (directional_mode_skip_mask[mbmi->mode]) continue;
        rd_stats_y.rate = INT_MAX;
        rd_pick_intra_angle_sby(cpi, x, &rate_dummy, &rd_stats_y, bsize,
                                intra_mode_cost[mbmi->mode], best_rd,
                                &model_rd);
      } else {
        mbmi->angle_delta[0] = 0;
        super_block_yrd(cpi, x, &rd_stats_y, bsize, best_rd);
      }
#else
      super_block_yrd(cpi, x, &rd_stats_y, bsize, best_rd);
#endif  // CONFIG_EXT_INTRA
      rate_y = rd_stats_y.rate;
      distortion_y = rd_stats_y.dist;
      skippable = rd_stats_y.skip;

      if (rate_y == INT_MAX) continue;

#if CONFIG_FILTER_INTRA
      if (mbmi->mode == DC_PRED) dc_skipped = 0;
#endif  // CONFIG_FILTER_INTRA

      uv_tx = uv_txsize_lookup[bsize][mbmi->tx_size][pd->subsampling_x]
                              [pd->subsampling_y];
      if (rate_uv_intra[uv_tx] == INT_MAX) {
        choose_intra_uv_mode(cpi, x, ctx, bsize, uv_tx, &rate_uv_intra[uv_tx],
                             &rate_uv_tokenonly[uv_tx], &dist_uvs[uv_tx],
                             &skip_uvs[uv_tx], &mode_uv[uv_tx]);
#if CONFIG_PALETTE
        if (try_palette) pmi_uv[uv_tx] = *pmi;
#endif  // CONFIG_PALETTE

#if CONFIG_EXT_INTRA
        uv_angle_delta[uv_tx] = mbmi->angle_delta[1];
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
        filter_intra_mode_info_uv[uv_tx] = mbmi->filter_intra_mode_info;
#endif  // CONFIG_FILTER_INTRA
      }

      rate_uv = rate_uv_tokenonly[uv_tx];
      distortion_uv = dist_uvs[uv_tx];
      skippable = skippable && skip_uvs[uv_tx];
      mbmi->uv_mode = mode_uv[uv_tx];
#if CONFIG_PALETTE
      if (try_palette) {
        pmi->palette_size[1] = pmi_uv[uv_tx].palette_size[1];
        memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
               pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
               2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
      }
#endif  // CONFIG_PALETTE

#if CONFIG_EXT_INTRA
      mbmi->angle_delta[1] = uv_angle_delta[uv_tx];
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
      mbmi->filter_intra_mode_info.use_filter_intra_mode[1] =
          filter_intra_mode_info_uv[uv_tx].use_filter_intra_mode[1];
      if (filter_intra_mode_info_uv[uv_tx].use_filter_intra_mode[1]) {
        mbmi->filter_intra_mode_info.filter_intra_mode[1] =
            filter_intra_mode_info_uv[uv_tx].filter_intra_mode[1];
      }
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_CB4X4
      rate2 = rate_y + intra_mode_cost[mbmi->mode];
      if (!x->skip_chroma_rd)
        rate2 += rate_uv + cpi->intra_uv_mode_cost[mbmi->mode][mbmi->uv_mode];
#else
      rate2 = rate_y + intra_mode_cost[mbmi->mode] + rate_uv +
              cpi->intra_uv_mode_cost[mbmi->mode][mbmi->uv_mode];
#endif  // CONFIG_CB4X4

#if CONFIG_PALETTE
      if (try_palette && mbmi->mode == DC_PRED) {
        rate2 += av1_cost_bit(
            av1_default_palette_y_mode_prob[bsize - BLOCK_8X8][palette_ctx], 0);
      }
#endif  // CONFIG_PALETTE

      if (!xd->lossless[mbmi->segment_id] && bsize >= BLOCK_8X8) {
        // super_block_yrd above includes the cost of the tx_size in the
        // tokenonly rate, but for intra blocks, tx_size is always coded
        // (prediction granularity), so we account for it in the full rate,
        // not the tokenonly rate.
        rate_y -= tx_size_cost(cpi, x, bsize, mbmi->tx_size);
      }
#if CONFIG_EXT_INTRA
      if (is_directional_mode) {
#if CONFIG_INTRA_INTERP
        const int intra_filter_ctx = av1_get_pred_context_intra_interp(xd);
        const int p_angle =
            mode_to_angle_map[mbmi->mode] + mbmi->angle_delta[0] * ANGLE_STEP;
        if (av1_is_intra_filter_switchable(p_angle))
          rate2 += cpi->intra_filter_cost[intra_filter_ctx][mbmi->intra_filter];
#endif  // CONFIG_INTRA_INTERP
        rate2 += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                    MAX_ANGLE_DELTA + mbmi->angle_delta[0]);
      }
      if (mbmi->uv_mode != DC_PRED && mbmi->uv_mode != TM_PRED) {
        rate2 += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1,
                                    MAX_ANGLE_DELTA + mbmi->angle_delta[1]);
      }
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
      if (mbmi->mode == DC_PRED) {
        rate2 +=
            av1_cost_bit(cm->fc->filter_intra_probs[0],
                         mbmi->filter_intra_mode_info.use_filter_intra_mode[0]);
        if (mbmi->filter_intra_mode_info.use_filter_intra_mode[0]) {
          rate2 += write_uniform_cost(
              FILTER_INTRA_MODES,
              mbmi->filter_intra_mode_info.filter_intra_mode[0]);
        }
      }
      if (mbmi->uv_mode == DC_PRED) {
        rate2 +=
            av1_cost_bit(cpi->common.fc->filter_intra_probs[1],
                         mbmi->filter_intra_mode_info.use_filter_intra_mode[1]);
        if (mbmi->filter_intra_mode_info.use_filter_intra_mode[1])
          rate2 += write_uniform_cost(
              FILTER_INTRA_MODES,
              mbmi->filter_intra_mode_info.filter_intra_mode[1]);
      }
#endif  // CONFIG_FILTER_INTRA
      if (mbmi->mode != DC_PRED && mbmi->mode != TM_PRED)
        rate2 += intra_cost_penalty;
      distortion2 = distortion_y + distortion_uv;
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
      if (bsize < BLOCK_8X8) distortion2_y = distortion_y;
#endif
    } else {
      int_mv backup_ref_mv[2];

#if !SUB8X8_COMP_REF
      if (bsize == BLOCK_4X4 && mbmi->ref_frame[1] > INTRA_FRAME) continue;
#endif  // !SUB8X8_COMP_REF

      backup_ref_mv[0] = mbmi_ext->ref_mvs[ref_frame][0];
      if (comp_pred) backup_ref_mv[1] = mbmi_ext->ref_mvs[second_ref_frame][0];
#if CONFIG_EXT_INTER && CONFIG_INTERINTRA
      if (second_ref_frame == INTRA_FRAME) {
        if (best_single_inter_ref != ref_frame) continue;
        mbmi->interintra_mode = intra_to_interintra_mode[best_intra_mode];
// TODO(debargha|geza.lore):
// Should we use ext_intra modes for interintra?
#if CONFIG_EXT_INTRA
        mbmi->angle_delta[0] = 0;
        mbmi->angle_delta[1] = 0;
#if CONFIG_INTRA_INTERP
        mbmi->intra_filter = INTRA_FILTER_LINEAR;
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
        mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
        mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
      }
#endif  // CONFIG_EXT_INTER && CONFIG_INTERINTRA
      mbmi->ref_mv_idx = 0;
      ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);

#if CONFIG_EXT_INTER
      if (comp_pred) {
        if (mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
          int ref_mv_idx = 0;
          // Special case: NEAR_NEWMV and NEW_NEARMV modes use
          // 1 + mbmi->ref_mv_idx (like NEARMV) instead of
          // mbmi->ref_mv_idx (like NEWMV)
          if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV)
            ref_mv_idx = 1;

          if (compound_ref0_mode(mbmi->mode) == NEWMV) {
            int_mv this_mv =
                mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
            clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                         xd->n8_h << MI_SIZE_LOG2, xd);
            mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
          }
          if (compound_ref1_mode(mbmi->mode) == NEWMV) {
            int_mv this_mv =
                mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;
            clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                         xd->n8_h << MI_SIZE_LOG2, xd);
            mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0] = this_mv;
          }
        }
      } else {
#endif  // CONFIG_EXT_INTER
        if (mbmi->mode == NEWMV && mbmi_ext->ref_mv_count[ref_frame_type] > 1) {
          int ref;
          for (ref = 0; ref < 1 + comp_pred; ++ref) {
            int_mv this_mv =
                (ref == 0) ? mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv
                           : mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;
            clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                         xd->n8_h << MI_SIZE_LOG2, xd);
            mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0] = this_mv;
          }
        }
#if CONFIG_EXT_INTER
      }
#endif  // CONFIG_EXT_INTER
      {
        RD_STATS rd_stats, rd_stats_y, rd_stats_uv;
        av1_init_rd_stats(&rd_stats);
        rd_stats.rate = rate2;

        // Point to variables that are maintained between loop iterations
        args.single_newmv = single_newmv;
#if CONFIG_EXT_INTER
        args.single_newmv_rate = single_newmv_rate;
        args.modelled_rd = modelled_rd;
#endif  // CONFIG_EXT_INTER
        this_rd = handle_inter_mode(cpi, x, bsize, &rd_stats, &rd_stats_y,
                                    &rd_stats_uv, &disable_skip, frame_mv,
                                    mi_row, mi_col, &args, best_rd);

        rate2 = rd_stats.rate;
        skippable = rd_stats.skip;
        distortion2 = rd_stats.dist;
        total_sse = rd_stats.sse;
        rate_y = rd_stats_y.rate;
        rate_uv = rd_stats_uv.rate;
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
        if (bsize < BLOCK_8X8) distortion2_y = rd_stats_y.dist;
#endif
      }

// TODO(jingning): This needs some refactoring to improve code quality
// and reduce redundant steps.
#if CONFIG_EXT_INTER
      if ((have_nearmv_in_inter_mode(mbmi->mode) &&
           mbmi_ext->ref_mv_count[ref_frame_type] > 2) ||
          ((mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV) &&
           mbmi_ext->ref_mv_count[ref_frame_type] > 1)) {
#else
      if ((mbmi->mode == NEARMV &&
           mbmi_ext->ref_mv_count[ref_frame_type] > 2) ||
          (mbmi->mode == NEWMV && mbmi_ext->ref_mv_count[ref_frame_type] > 1)) {
#endif
        int_mv backup_mv = frame_mv[NEARMV][ref_frame];
        MB_MODE_INFO backup_mbmi = *mbmi;
        int backup_skip = x->skip;
        int64_t tmp_ref_rd = this_rd;
        int ref_idx;

// TODO(jingning): This should be deprecated shortly.
#if CONFIG_EXT_INTER
        int idx_offset = have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0;
#else
        int idx_offset = (mbmi->mode == NEARMV) ? 1 : 0;
#endif  // CONFIG_EXT_INTER
        int ref_set =
            AOMMIN(2, mbmi_ext->ref_mv_count[ref_frame_type] - 1 - idx_offset);

        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx_offset);
        // Dummy
        int_mv backup_fmv[2];
        backup_fmv[0] = frame_mv[NEWMV][ref_frame];
        if (comp_pred) backup_fmv[1] = frame_mv[NEWMV][second_ref_frame];

        rate2 += (rate2 < INT_MAX ? cpi->drl_mode_cost0[drl_ctx][0] : 0);

        if (this_rd < INT64_MAX) {
          if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv, distortion2) <
              RDCOST(x->rdmult, x->rddiv, 0, total_sse))
            tmp_ref_rd =
                RDCOST(x->rdmult, x->rddiv,
                       rate2 + av1_cost_bit(av1_get_skip_prob(cm, xd), 0),
                       distortion2);
          else
            tmp_ref_rd =
                RDCOST(x->rdmult, x->rddiv,
                       rate2 + av1_cost_bit(av1_get_skip_prob(cm, xd), 1) -
                           rate_y - rate_uv,
                       total_sse);
        }
#if CONFIG_VAR_TX
        for (i = 0; i < MAX_MB_PLANE; ++i)
          memcpy(x->blk_skip_drl[i], x->blk_skip[i],
                 sizeof(uint8_t) * ctx->num_4x4_blk);
#endif  // CONFIG_VAR_TX

        for (ref_idx = 0; ref_idx < ref_set; ++ref_idx) {
          int64_t tmp_alt_rd = INT64_MAX;
          int dummy_disable_skip = 0;
          int ref;
          int_mv cur_mv;
          RD_STATS tmp_rd_stats, tmp_rd_stats_y, tmp_rd_stats_uv;

          av1_invalid_rd_stats(&tmp_rd_stats);
          x->skip = 0;

          mbmi->ref_mv_idx = 1 + ref_idx;

#if CONFIG_EXT_INTER
          if (comp_pred) {
            int ref_mv_idx = mbmi->ref_mv_idx;
            // Special case: NEAR_NEWMV and NEW_NEARMV modes use
            // 1 + mbmi->ref_mv_idx (like NEARMV) instead of
            // mbmi->ref_mv_idx (like NEWMV)
            if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV)
              ref_mv_idx = 1 + mbmi->ref_mv_idx;

            if (compound_ref0_mode(mbmi->mode) == NEWMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
            } else if (compound_ref0_mode(mbmi->mode) == NEARESTMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][0].this_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
            }

            if (compound_ref1_mode(mbmi->mode) == NEWMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0] = this_mv;
            } else if (compound_ref1_mode(mbmi->mode) == NEARESTMV) {
              int_mv this_mv =
                  mbmi_ext->ref_mv_stack[ref_frame_type][0].comp_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0] = this_mv;
            }
          } else {
#endif  // CONFIG_EXT_INTER
            for (ref = 0; ref < 1 + comp_pred; ++ref) {
              int_mv this_mv =
                  (ref == 0)
                      ? mbmi_ext->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                            .this_mv
                      : mbmi_ext->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx]
                            .comp_mv;
              clamp_mv_ref(&this_mv.as_mv, xd->n8_w << MI_SIZE_LOG2,
                           xd->n8_h << MI_SIZE_LOG2, xd);
              mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0] = this_mv;
            }
#if CONFIG_EXT_INTER
          }
#endif

          cur_mv =
              mbmi_ext->ref_mv_stack[ref_frame][mbmi->ref_mv_idx + idx_offset]
                  .this_mv;
          clamp_mv2(&cur_mv.as_mv, xd);

          if (!mv_check_bounds(&x->mv_limits, &cur_mv.as_mv)) {
            int_mv dummy_single_newmv[TOTAL_REFS_PER_FRAME] = { { 0 } };
#if CONFIG_EXT_INTER
            int dummy_single_newmv_rate[TOTAL_REFS_PER_FRAME] = { 0 };
#endif  // CONFIG_EXT_INTER

            frame_mv[NEARMV][ref_frame] = cur_mv;
            av1_init_rd_stats(&tmp_rd_stats);

            // Point to variables that are not maintained between iterations
            args.single_newmv = dummy_single_newmv;
#if CONFIG_EXT_INTER
            args.single_newmv_rate = dummy_single_newmv_rate;
            args.modelled_rd = NULL;
#endif  // CONFIG_EXT_INTER
            tmp_alt_rd = handle_inter_mode(
                cpi, x, bsize, &tmp_rd_stats, &tmp_rd_stats_y, &tmp_rd_stats_uv,
                &dummy_disable_skip, frame_mv, mi_row, mi_col, &args, best_rd);
            // Prevent pointers from escaping local scope
            args.single_newmv = NULL;
#if CONFIG_EXT_INTER
            args.single_newmv_rate = NULL;
#endif  // CONFIG_EXT_INTER
          }

          for (i = 0; i < mbmi->ref_mv_idx; ++i) {
            uint8_t drl1_ctx = 0;
            drl1_ctx = av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type],
                                   i + idx_offset);
            tmp_rd_stats.rate +=
                (tmp_rd_stats.rate < INT_MAX ? cpi->drl_mode_cost0[drl1_ctx][1]
                                             : 0);
          }

          if (mbmi_ext->ref_mv_count[ref_frame_type] >
                  mbmi->ref_mv_idx + idx_offset + 1 &&
              ref_idx < ref_set - 1) {
            uint8_t drl1_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type],
                            mbmi->ref_mv_idx + idx_offset);
            tmp_rd_stats.rate +=
                (tmp_rd_stats.rate < INT_MAX ? cpi->drl_mode_cost0[drl1_ctx][0]
                                             : 0);
          }

          if (tmp_alt_rd < INT64_MAX) {
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
            tmp_alt_rd = RDCOST(x->rdmult, x->rddiv, tmp_rd_stats.rate,
                                tmp_rd_stats.dist);
#else
            if (RDCOST(x->rdmult, x->rddiv,
                       tmp_rd_stats_y.rate + tmp_rd_stats_uv.rate,
                       tmp_rd_stats.dist) <
                RDCOST(x->rdmult, x->rddiv, 0, tmp_rd_stats.sse))
              tmp_alt_rd =
                  RDCOST(x->rdmult, x->rddiv,
                         tmp_rd_stats.rate +
                             av1_cost_bit(av1_get_skip_prob(cm, xd), 0),
                         tmp_rd_stats.dist);
            else
              tmp_alt_rd =
                  RDCOST(x->rdmult, x->rddiv,
                         tmp_rd_stats.rate +
                             av1_cost_bit(av1_get_skip_prob(cm, xd), 1) -
                             tmp_rd_stats_y.rate - tmp_rd_stats_uv.rate,
                         tmp_rd_stats.sse);
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
          }

          if (tmp_ref_rd > tmp_alt_rd) {
            rate2 = tmp_rd_stats.rate;
            disable_skip = dummy_disable_skip;
            distortion2 = tmp_rd_stats.dist;
            skippable = tmp_rd_stats.skip;
            rate_y = tmp_rd_stats_y.rate;
            rate_uv = tmp_rd_stats_uv.rate;
            total_sse = tmp_rd_stats.sse;
            this_rd = tmp_alt_rd;
            tmp_ref_rd = tmp_alt_rd;
            backup_mbmi = *mbmi;
            backup_skip = x->skip;
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
            if (bsize < BLOCK_8X8) {
              total_sse_y = tmp_rd_stats_y.sse;
              distortion2_y = tmp_rd_stats_y.dist;
            }
#endif
#if CONFIG_VAR_TX
            for (i = 0; i < MAX_MB_PLANE; ++i)
              memcpy(x->blk_skip_drl[i], x->blk_skip[i],
                     sizeof(uint8_t) * ctx->num_4x4_blk);
#endif  // CONFIG_VAR_TX
          } else {
            *mbmi = backup_mbmi;
            x->skip = backup_skip;
          }
        }

        frame_mv[NEARMV][ref_frame] = backup_mv;
        frame_mv[NEWMV][ref_frame] = backup_fmv[0];
        if (comp_pred) frame_mv[NEWMV][second_ref_frame] = backup_fmv[1];
#if CONFIG_VAR_TX
        for (i = 0; i < MAX_MB_PLANE; ++i)
          memcpy(x->blk_skip[i], x->blk_skip_drl[i],
                 sizeof(uint8_t) * ctx->num_4x4_blk);
#endif  // CONFIG_VAR_TX
      }
      mbmi_ext->ref_mvs[ref_frame][0] = backup_ref_mv[0];
      if (comp_pred) mbmi_ext->ref_mvs[second_ref_frame][0] = backup_ref_mv[1];

      if (this_rd == INT64_MAX) continue;

#if SUB8X8_COMP_REF
      compmode_cost = av1_cost_bit(comp_mode_p, comp_pred);
#else
      if (mbmi->sb_type != BLOCK_4X4)
        compmode_cost = av1_cost_bit(comp_mode_p, comp_pred);
#endif  // SUB8X8_COMP_REF

      if (cm->reference_mode == REFERENCE_MODE_SELECT) rate2 += compmode_cost;
    }

    // Estimate the reference frame signaling cost and add it
    // to the rolling cost variable.
    if (comp_pred) {
      rate2 += ref_costs_comp[ref_frame];
#if CONFIG_EXT_REFS
      rate2 += ref_costs_comp[second_ref_frame];
#endif  // CONFIG_EXT_REFS
    } else {
      rate2 += ref_costs_single[ref_frame];
    }

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    if (ref_frame == INTRA_FRAME) {
#else
    if (!disable_skip) {
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
      if (skippable) {
        // Back out the coefficient coding costs
        rate2 -= (rate_y + rate_uv);
        rate_y = 0;
        rate_uv = 0;
        // Cost the skip mb case
        rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
      } else if (ref_frame != INTRA_FRAME && !xd->lossless[mbmi->segment_id]) {
        if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv + rate_skip0,
                   distortion2) <
            RDCOST(x->rdmult, x->rddiv, rate_skip1, total_sse)) {
          // Add in the cost of the no skip flag.
          rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
        } else {
          // FIXME(rbultje) make this work for splitmv also
          rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
          distortion2 = total_sse;
          assert(total_sse >= 0);
          rate2 -= (rate_y + rate_uv);
          this_skip2 = 1;
          rate_y = 0;
          rate_uv = 0;
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
          if (bsize < BLOCK_8X8) distortion2_y = total_sse_y;
#endif
        }
      } else {
        // Add in the cost of the no skip flag.
        rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
      }

      // Calculate the final RD estimate for this mode.
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    } else {
      this_skip2 = mbmi->skip;
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
      if (this_skip2) {
        rate_y = 0;
        rate_uv = 0;
      }
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    }

    if (ref_frame == INTRA_FRAME) {
      // Keep record of best intra rd
      if (this_rd < best_intra_rd) {
        best_intra_rd = this_rd;
        best_intra_mode = mbmi->mode;
      }
#if CONFIG_EXT_INTER && CONFIG_INTERINTRA
    } else if (second_ref_frame == NONE_FRAME) {
      if (this_rd < best_single_inter_rd) {
        best_single_inter_rd = this_rd;
        best_single_inter_ref = mbmi->ref_frame[0];
      }
#endif  // CONFIG_EXT_INTER && CONFIG_INTERINTRA
    }

    if (!disable_skip && ref_frame == INTRA_FRAME) {
      for (i = 0; i < REFERENCE_MODES; ++i)
        best_pred_rd[i] = AOMMIN(best_pred_rd[i], this_rd);
    }

    // Did this mode help.. i.e. is it the new best mode
    if (this_rd < best_rd || x->skip) {
      if (!mode_excluded) {
        // Note index of best mode so far
        best_mode_index = mode_index;

        if (ref_frame == INTRA_FRAME) {
          /* required for left and above block mv */
          mbmi->mv[0].as_int = 0;
        } else {
          best_pred_sse = x->pred_sse[ref_frame];
        }

        rd_cost->rate = rate2;
#if CONFIG_SUPERTX
        if (x->skip)
          *returnrate_nocoef = rate2;
        else
          *returnrate_nocoef = rate2 - rate_y - rate_uv;
        *returnrate_nocoef -= av1_cost_bit(
            av1_get_skip_prob(cm, xd), disable_skip || skippable || this_skip2);
        *returnrate_nocoef -= av1_cost_bit(av1_get_intra_inter_prob(cm, xd),
                                           mbmi->ref_frame[0] != INTRA_FRAME);
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
        MODE_INFO *const mi = xd->mi[0];
        const MOTION_MODE motion_allowed = motion_mode_allowed(
#if CONFIG_GLOBAL_MOTION && SEPARATE_GLOBAL_MOTION
            0, xd->global_motion,
#endif  // CONFIG_GLOBAL_MOTION && SEPARATE_GLOBAL_MOTION
            mi);
        if (motion_allowed == WARPED_CAUSAL)
          *returnrate_nocoef -= cpi->motion_mode_cost[bsize][mbmi->motion_mode];
        else if (motion_allowed == OBMC_CAUSAL)
          *returnrate_nocoef -=
              cpi->motion_mode_cost1[bsize][mbmi->motion_mode];
#else
        *returnrate_nocoef -= cpi->motion_mode_cost[bsize][mbmi->motion_mode];
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#endif  // CONFIG_SUPERTX
        rd_cost->dist = distortion2;
        rd_cost->rdcost = this_rd;
        best_rd = this_rd;
        best_mbmode = *mbmi;
        best_skip2 = this_skip2;
        best_mode_skippable = skippable;
        best_rate_y = rate_y + av1_cost_bit(av1_get_skip_prob(cm, xd),
                                            this_skip2 || skippable);
        best_rate_uv = rate_uv;
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
        if (bsize < BLOCK_8X8) rd_cost->dist_y = distortion2_y;
#endif
#if CONFIG_VAR_TX
        for (i = 0; i < MAX_MB_PLANE; ++i)
          memcpy(ctx->blk_skip[i], x->blk_skip[i],
                 sizeof(uint8_t) * ctx->num_4x4_blk);
#endif  // CONFIG_VAR_TX
      }
    }

    /* keep record of best compound/single-only prediction */
    if (!disable_skip && ref_frame != INTRA_FRAME) {
      int64_t single_rd, hybrid_rd, single_rate, hybrid_rate;

      if (cm->reference_mode == REFERENCE_MODE_SELECT) {
        single_rate = rate2 - compmode_cost;
        hybrid_rate = rate2;
      } else {
        single_rate = rate2;
        hybrid_rate = rate2 + compmode_cost;
      }

      single_rd = RDCOST(x->rdmult, x->rddiv, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, x->rddiv, hybrid_rate, distortion2);

      if (!comp_pred) {
        if (single_rd < best_pred_rd[SINGLE_REFERENCE])
          best_pred_rd[SINGLE_REFERENCE] = single_rd;
      } else {
        if (single_rd < best_pred_rd[COMPOUND_REFERENCE])
          best_pred_rd[COMPOUND_REFERENCE] = single_rd;
      }
      if (hybrid_rd < best_pred_rd[REFERENCE_MODE_SELECT])
        best_pred_rd[REFERENCE_MODE_SELECT] = hybrid_rd;
    }

    if (x->skip && !comp_pred) break;
  }

  if (xd->lossless[mbmi->segment_id] == 0 && best_mode_index >= 0 &&
      ((sf->tx_type_search.fast_inter_tx_type_search == 1 &&
        is_inter_mode(best_mbmode.mode)) ||
       (sf->tx_type_search.fast_intra_tx_type_search == 1 &&
        !is_inter_mode(best_mbmode.mode)))) {
    int skip_blk = 0;
    RD_STATS rd_stats_y, rd_stats_uv;

    x->use_default_inter_tx_type = 0;
    x->use_default_intra_tx_type = 0;

    *mbmi = best_mbmode;

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[mbmi->ref_frame[0]][i];
      if (has_second_ref(mbmi))
        xd->plane[i].pre[1] = yv12_mb[mbmi->ref_frame[1]][i];
    }

    if (is_inter_mode(mbmi->mode)) {
      av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
#if CONFIG_MOTION_VAR
      if (mbmi->motion_mode == OBMC_CAUSAL) {
        av1_build_obmc_inter_prediction(
            cm, xd, mi_row, mi_col, args.above_pred_buf, args.above_pred_stride,
            args.left_pred_buf, args.left_pred_stride);
      }
#endif  // CONFIG_MOTION_VAR
      av1_subtract_plane(x, bsize, 0);
#if CONFIG_VAR_TX
      if (cm->tx_mode == TX_MODE_SELECT || xd->lossless[mbmi->segment_id]) {
        select_tx_type_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
      } else {
        int idx, idy;
        super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
        for (idy = 0; idy < xd->n8_h; ++idy)
          for (idx = 0; idx < xd->n8_w; ++idx)
            mbmi->inter_tx_size[idy][idx] = mbmi->tx_size;
        memset(x->blk_skip[0], rd_stats_y.skip,
               sizeof(uint8_t) * xd->n8_h * xd->n8_w * 4);
      }

      inter_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
#else
      super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
      super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
#endif  // CONFIG_VAR_TX
    } else {
      super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
      super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
    }

    if (RDCOST(x->rdmult, x->rddiv, rd_stats_y.rate + rd_stats_uv.rate,
               (rd_stats_y.dist + rd_stats_uv.dist)) >
        RDCOST(x->rdmult, x->rddiv, 0, (rd_stats_y.sse + rd_stats_uv.sse))) {
      skip_blk = 1;
      rd_stats_y.rate = av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
      rd_stats_uv.rate = 0;
      rd_stats_y.dist = rd_stats_y.sse;
      rd_stats_uv.dist = rd_stats_uv.sse;
    } else {
      skip_blk = 0;
      rd_stats_y.rate += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
    }

    if (RDCOST(x->rdmult, x->rddiv, best_rate_y + best_rate_uv, rd_cost->dist) >
        RDCOST(x->rdmult, x->rddiv, rd_stats_y.rate + rd_stats_uv.rate,
               (rd_stats_y.dist + rd_stats_uv.dist))) {
#if CONFIG_VAR_TX
      int idx, idy;
#endif  // CONFIG_VAR_TX
      best_mbmode.tx_type = mbmi->tx_type;
      best_mbmode.tx_size = mbmi->tx_size;
#if CONFIG_VAR_TX
      for (idy = 0; idy < xd->n8_h; ++idy)
        for (idx = 0; idx < xd->n8_w; ++idx)
          best_mbmode.inter_tx_size[idy][idx] = mbmi->inter_tx_size[idy][idx];

      for (i = 0; i < MAX_MB_PLANE; ++i)
        memcpy(ctx->blk_skip[i], x->blk_skip[i],
               sizeof(uint8_t) * ctx->num_4x4_blk);

      best_mbmode.min_tx_size = mbmi->min_tx_size;
#endif  // CONFIG_VAR_TX
      rd_cost->rate +=
          (rd_stats_y.rate + rd_stats_uv.rate - best_rate_y - best_rate_uv);
      rd_cost->dist = rd_stats_y.dist + rd_stats_uv.dist;
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
      if (bsize < BLOCK_8X8) rd_cost->dist_y = rd_stats_y.dist;
#endif
      rd_cost->rdcost =
          RDCOST(x->rdmult, x->rddiv, rd_cost->rate, rd_cost->dist);
      best_skip2 = skip_blk;
    }
  }

#if CONFIG_PALETTE
  // Only try palette mode when the best mode so far is an intra mode.
  if (try_palette && !is_inter_mode(best_mbmode.mode)) {
    int rate2 = 0;
#if CONFIG_SUPERTX
    int best_rate_nocoef;
#endif  // CONFIG_SUPERTX
    int64_t distortion2 = 0, best_rd_palette = best_rd, this_rd,
            best_model_rd_palette = INT64_MAX;
    int skippable = 0, rate_overhead_palette = 0;
    RD_STATS rd_stats_y;
    TX_SIZE uv_tx;
    uint8_t *const best_palette_color_map =
        x->palette_buffer->best_palette_color_map;
    uint8_t *const color_map = xd->plane[0].color_index_map;
    MB_MODE_INFO best_mbmi_palette = best_mbmode;

    mbmi->mode = DC_PRED;
    mbmi->uv_mode = DC_PRED;
    mbmi->ref_frame[0] = INTRA_FRAME;
    mbmi->ref_frame[1] = NONE_FRAME;
    rate_overhead_palette = rd_pick_palette_intra_sby(
        cpi, x, bsize, palette_ctx, intra_mode_cost[DC_PRED],
        &best_mbmi_palette, best_palette_color_map, &best_rd_palette,
        &best_model_rd_palette, NULL, NULL, NULL, NULL);
    if (pmi->palette_size[0] == 0) goto PALETTE_EXIT;
    memcpy(color_map, best_palette_color_map,
           rows * cols * sizeof(best_palette_color_map[0]));
    super_block_yrd(cpi, x, &rd_stats_y, bsize, best_rd);
    if (rd_stats_y.rate == INT_MAX) goto PALETTE_EXIT;
    uv_tx = uv_txsize_lookup[bsize][mbmi->tx_size][xd->plane[1].subsampling_x]
                            [xd->plane[1].subsampling_y];
    if (rate_uv_intra[uv_tx] == INT_MAX) {
      choose_intra_uv_mode(cpi, x, ctx, bsize, uv_tx, &rate_uv_intra[uv_tx],
                           &rate_uv_tokenonly[uv_tx], &dist_uvs[uv_tx],
                           &skip_uvs[uv_tx], &mode_uv[uv_tx]);
      pmi_uv[uv_tx] = *pmi;
#if CONFIG_EXT_INTRA
      uv_angle_delta[uv_tx] = mbmi->angle_delta[1];
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
      filter_intra_mode_info_uv[uv_tx] = mbmi->filter_intra_mode_info;
#endif  // CONFIG_FILTER_INTRA
    }
    mbmi->uv_mode = mode_uv[uv_tx];
    pmi->palette_size[1] = pmi_uv[uv_tx].palette_size[1];
    if (pmi->palette_size[1] > 0) {
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
#if CONFIG_EXT_INTRA
    mbmi->angle_delta[1] = uv_angle_delta[uv_tx];
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
    mbmi->filter_intra_mode_info.use_filter_intra_mode[1] =
        filter_intra_mode_info_uv[uv_tx].use_filter_intra_mode[1];
    if (filter_intra_mode_info_uv[uv_tx].use_filter_intra_mode[1]) {
      mbmi->filter_intra_mode_info.filter_intra_mode[1] =
          filter_intra_mode_info_uv[uv_tx].filter_intra_mode[1];
    }
#endif  // CONFIG_FILTER_INTRA
    skippable = rd_stats_y.skip && skip_uvs[uv_tx];
    distortion2 = rd_stats_y.dist + dist_uvs[uv_tx];
    rate2 = rd_stats_y.rate + rate_overhead_palette + rate_uv_intra[uv_tx];
    rate2 += ref_costs_single[INTRA_FRAME];

    if (skippable) {
      rate2 -= (rd_stats_y.rate + rate_uv_tokenonly[uv_tx]);
#if CONFIG_SUPERTX
      best_rate_nocoef = rate2;
#endif  // CONFIG_SUPERTX
      rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 1);
    } else {
#if CONFIG_SUPERTX
      best_rate_nocoef = rate2 - (rd_stats_y.rate + rate_uv_tokenonly[uv_tx]);
#endif  // CONFIG_SUPERTX
      rate2 += av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
    }
    this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
    if (this_rd < best_rd) {
      best_mode_index = 3;
      mbmi->mv[0].as_int = 0;
      rd_cost->rate = rate2;
#if CONFIG_SUPERTX
      *returnrate_nocoef = best_rate_nocoef;
#endif  // CONFIG_SUPERTX
      rd_cost->dist = distortion2;
      rd_cost->rdcost = this_rd;
      best_rd = this_rd;
      best_mbmode = *mbmi;
      best_skip2 = 0;
      best_mode_skippable = skippable;
    }
  }
PALETTE_EXIT:
#endif  // CONFIG_PALETTE

#if CONFIG_FILTER_INTRA
  // TODO(huisu): filter-intra is turned off in lossless mode for now to
  // avoid a unit test failure
  if (!xd->lossless[mbmi->segment_id] &&
#if CONFIG_PALETTE
      pmi->palette_size[0] == 0 &&
#endif  // CONFIG_PALETTE
      !dc_skipped && best_mode_index >= 0 &&
      best_intra_rd < (best_rd + (best_rd >> 3))) {
    pick_filter_intra_interframe(
        cpi, x, ctx, bsize, mi_row, mi_col, rate_uv_intra, rate_uv_tokenonly,
        dist_uvs, skip_uvs, mode_uv, filter_intra_mode_info_uv,
#if CONFIG_EXT_INTRA
        uv_angle_delta,
#endif  // CONFIG_EXT_INTRA
#if CONFIG_PALETTE
        pmi_uv, palette_ctx,
#endif  // CONFIG_PALETTE
        0, ref_costs_single, &best_rd, &best_intra_rd, &best_intra_mode,
        &best_mode_index, &best_skip2, &best_mode_skippable,
#if CONFIG_SUPERTX
        returnrate_nocoef,
#endif  // CONFIG_SUPERTX
        best_pred_rd, &best_mbmode, rd_cost);
  }
#endif  // CONFIG_FILTER_INTRA

  // The inter modes' rate costs are not calculated precisely in some cases.
  // Therefore, sometimes, NEWMV is chosen instead of NEARESTMV, NEARMV, and
  // ZEROMV. Here, checks are added for those cases, and the mode decisions
  // are corrected.
  if (best_mbmode.mode == NEWMV
#if CONFIG_EXT_INTER
      || best_mbmode.mode == NEW_NEWMV
#endif  // CONFIG_EXT_INTER
      ) {
    const MV_REFERENCE_FRAME refs[2] = { best_mbmode.ref_frame[0],
                                         best_mbmode.ref_frame[1] };
    int comp_pred_mode = refs[1] > INTRA_FRAME;
    int_mv zeromv[2];
    const uint8_t rf_type = av1_ref_frame_type(best_mbmode.ref_frame);
#if CONFIG_GLOBAL_MOTION
    zeromv[0].as_int = gm_get_motion_vector(&cm->global_motion[refs[0]],
                                            cm->allow_high_precision_mv, bsize,
                                            mi_col, mi_row, 0)
                           .as_int;
    zeromv[1].as_int = comp_pred_mode
                           ? gm_get_motion_vector(&cm->global_motion[refs[1]],
                                                  cm->allow_high_precision_mv,
                                                  bsize, mi_col, mi_row, 0)
                                 .as_int
                           : 0;
#else
    zeromv[0].as_int = 0;
    zeromv[1].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION
    if (!comp_pred_mode) {
      int ref_set = (mbmi_ext->ref_mv_count[rf_type] >= 2)
                        ? AOMMIN(2, mbmi_ext->ref_mv_count[rf_type] - 2)
                        : INT_MAX;

      for (i = 0; i <= ref_set && ref_set != INT_MAX; ++i) {
        int_mv cur_mv = mbmi_ext->ref_mv_stack[rf_type][i + 1].this_mv;
        if (cur_mv.as_int == best_mbmode.mv[0].as_int) {
          best_mbmode.mode = NEARMV;
          best_mbmode.ref_mv_idx = i;
        }
      }

      if (frame_mv[NEARESTMV][refs[0]].as_int == best_mbmode.mv[0].as_int)
        best_mbmode.mode = NEARESTMV;
      else if (best_mbmode.mv[0].as_int == zeromv[0].as_int)
        best_mbmode.mode = ZEROMV;
    } else {
      int_mv nearestmv[2];
      int_mv nearmv[2];

#if CONFIG_EXT_INTER
      if (mbmi_ext->ref_mv_count[rf_type] > 1) {
        nearmv[0] = mbmi_ext->ref_mv_stack[rf_type][1].this_mv;
        nearmv[1] = mbmi_ext->ref_mv_stack[rf_type][1].comp_mv;
      } else {
        nearmv[0] = frame_mv[NEARMV][refs[0]];
        nearmv[1] = frame_mv[NEARMV][refs[1]];
      }
#else
      int ref_set = (mbmi_ext->ref_mv_count[rf_type] >= 2)
                        ? AOMMIN(2, mbmi_ext->ref_mv_count[rf_type] - 2)
                        : INT_MAX;

      for (i = 0; i <= ref_set && ref_set != INT_MAX; ++i) {
        nearmv[0] = mbmi_ext->ref_mv_stack[rf_type][i + 1].this_mv;
        nearmv[1] = mbmi_ext->ref_mv_stack[rf_type][i + 1].comp_mv;

        if (nearmv[0].as_int == best_mbmode.mv[0].as_int &&
            nearmv[1].as_int == best_mbmode.mv[1].as_int) {
          best_mbmode.mode = NEARMV;
          best_mbmode.ref_mv_idx = i;
        }
      }
#endif  // CONFIG_EXT_INTER
      if (mbmi_ext->ref_mv_count[rf_type] >= 1) {
        nearestmv[0] = mbmi_ext->ref_mv_stack[rf_type][0].this_mv;
        nearestmv[1] = mbmi_ext->ref_mv_stack[rf_type][0].comp_mv;
      } else {
        nearestmv[0] = frame_mv[NEARESTMV][refs[0]];
        nearestmv[1] = frame_mv[NEARESTMV][refs[1]];
      }

      if (nearestmv[0].as_int == best_mbmode.mv[0].as_int &&
          nearestmv[1].as_int == best_mbmode.mv[1].as_int) {
#if CONFIG_EXT_INTER
        best_mbmode.mode = NEAREST_NEARESTMV;
      } else {
        int ref_set = (mbmi_ext->ref_mv_count[rf_type] >= 2)
                          ? AOMMIN(2, mbmi_ext->ref_mv_count[rf_type] - 2)
                          : INT_MAX;

        for (i = 0; i <= ref_set && ref_set != INT_MAX; ++i) {
          nearmv[0] = mbmi_ext->ref_mv_stack[rf_type][i + 1].this_mv;
          nearmv[1] = mbmi_ext->ref_mv_stack[rf_type][i + 1].comp_mv;

          // Try switching to the NEAR_NEARMV mode
          if (nearmv[0].as_int == best_mbmode.mv[0].as_int &&
              nearmv[1].as_int == best_mbmode.mv[1].as_int) {
            best_mbmode.mode = NEAR_NEARMV;
            best_mbmode.ref_mv_idx = i;
          }
        }

        if (best_mbmode.mode == NEW_NEWMV &&
            best_mbmode.mv[0].as_int == zeromv[0].as_int &&
            best_mbmode.mv[1].as_int == zeromv[1].as_int)
          best_mbmode.mode = ZERO_ZEROMV;
      }
#else
        best_mbmode.mode = NEARESTMV;
      } else if (best_mbmode.mv[0].as_int == zeromv[0].as_int &&
                 best_mbmode.mv[1].as_int == zeromv[1].as_int) {
        best_mbmode.mode = ZEROMV;
      }
#endif  // CONFIG_EXT_INTER
    }
  }

  // Make sure that the ref_mv_idx is only nonzero when we're
  // using a mode which can support ref_mv_idx
  if (best_mbmode.ref_mv_idx != 0 &&
#if CONFIG_EXT_INTER
      !(best_mbmode.mode == NEWMV || best_mbmode.mode == NEW_NEWMV ||
        have_nearmv_in_inter_mode(best_mbmode.mode))) {
#else
      !(best_mbmode.mode == NEARMV || best_mbmode.mode == NEWMV)) {
#endif
    best_mbmode.ref_mv_idx = 0;
  }

  {
    int8_t ref_frame_type = av1_ref_frame_type(best_mbmode.ref_frame);
    int16_t mode_ctx = mbmi_ext->mode_context[ref_frame_type];
    if (mode_ctx & (1 << ALL_ZERO_FLAG_OFFSET)) {
      int_mv zeromv[2];
#if CONFIG_GLOBAL_MOTION
      const MV_REFERENCE_FRAME refs[2] = { best_mbmode.ref_frame[0],
                                           best_mbmode.ref_frame[1] };
      zeromv[0].as_int = gm_get_motion_vector(&cm->global_motion[refs[0]],
                                              cm->allow_high_precision_mv,
                                              bsize, mi_col, mi_row, 0)
                             .as_int;
      zeromv[1].as_int = (refs[1] != NONE_FRAME)
                             ? gm_get_motion_vector(&cm->global_motion[refs[1]],
                                                    cm->allow_high_precision_mv,
                                                    bsize, mi_col, mi_row, 0)
                                   .as_int
                             : 0;
      lower_mv_precision(&zeromv[0].as_mv, cm->allow_high_precision_mv);
      lower_mv_precision(&zeromv[1].as_mv, cm->allow_high_precision_mv);
#else
      zeromv[0].as_int = zeromv[1].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION
      if (best_mbmode.ref_frame[0] > INTRA_FRAME &&
          best_mbmode.mv[0].as_int == zeromv[0].as_int &&
#if CONFIG_EXT_INTER
          (best_mbmode.ref_frame[1] <= INTRA_FRAME)
#else
          (best_mbmode.ref_frame[1] == NONE_FRAME ||
           best_mbmode.mv[1].as_int == zeromv[1].as_int)
#endif  // CONFIG_EXT_INTER
              ) {
        best_mbmode.mode = ZEROMV;
      }
    }
  }

  if (best_mode_index < 0 || best_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

#if CONFIG_DUAL_FILTER
  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == best_mbmode.interp_filter[0]) ||
         !is_inter_block(&best_mbmode));
  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == best_mbmode.interp_filter[1]) ||
         !is_inter_block(&best_mbmode));
  if (best_mbmode.ref_frame[1] > INTRA_FRAME) {
    assert((cm->interp_filter == SWITCHABLE) ||
           (cm->interp_filter == best_mbmode.interp_filter[2]) ||
           !is_inter_block(&best_mbmode));
    assert((cm->interp_filter == SWITCHABLE) ||
           (cm->interp_filter == best_mbmode.interp_filter[3]) ||
           !is_inter_block(&best_mbmode));
  }
#else
  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == best_mbmode.interp_filter) ||
         !is_inter_block(&best_mbmode));
#endif  // CONFIG_DUAL_FILTER

  if (!cpi->rc.is_src_frame_alt_ref)
    av1_update_rd_thresh_fact(cm, tile_data->thresh_freq_fact,
                              sf->adaptive_rd_thresh, bsize, best_mode_index);

  // macroblock modes
  *mbmi = best_mbmode;
  x->skip |= best_skip2;

// Note: this section is needed since the mode may have been forced to
// ZEROMV by the all-zero mode handling of ref-mv.
#if CONFIG_GLOBAL_MOTION
  if (mbmi->mode == ZEROMV
#if CONFIG_EXT_INTER
      || mbmi->mode == ZERO_ZEROMV
#endif  // CONFIG_EXT_INTER
      ) {
#if CONFIG_WARPED_MOTION || CONFIG_MOTION_VAR
    // Correct the motion mode for ZEROMV
    const MOTION_MODE last_motion_mode_allowed = motion_mode_allowed(
#if SEPARATE_GLOBAL_MOTION
        0, xd->global_motion,
#endif  // SEPARATE_GLOBAL_MOTION
        xd->mi[0]);
    if (mbmi->motion_mode > last_motion_mode_allowed)
      mbmi->motion_mode = last_motion_mode_allowed;
#endif  // CONFIG_WARPED_MOTION || CONFIG_MOTION_VAR

    // Correct the interpolation filter for ZEROMV
    if (is_nontrans_global_motion(xd)) {
#if CONFIG_DUAL_FILTER
      mbmi->interp_filter[0] = cm->interp_filter == SWITCHABLE
                                   ? EIGHTTAP_REGULAR
                                   : cm->interp_filter;
      mbmi->interp_filter[1] = cm->interp_filter == SWITCHABLE
                                   ? EIGHTTAP_REGULAR
                                   : cm->interp_filter;
#else
      mbmi->interp_filter = cm->interp_filter == SWITCHABLE ? EIGHTTAP_REGULAR
                                                            : cm->interp_filter;
#endif  // CONFIG_DUAL_FILTER
    }
  }
#endif  // CONFIG_GLOBAL_MOTION

  for (i = 0; i < 1 + has_second_ref(mbmi); ++i) {
    if (mbmi->mode != NEWMV)
      mbmi->pred_mv[i].as_int = mbmi->mv[i].as_int;
    else
      mbmi->pred_mv[i].as_int = mbmi_ext->ref_mvs[mbmi->ref_frame[i]][0].as_int;
  }

  for (i = 0; i < REFERENCE_MODES; ++i) {
    if (best_pred_rd[i] == INT64_MAX)
      best_pred_diff[i] = INT_MIN;
    else
      best_pred_diff[i] = best_rd - best_pred_rd[i];
  }

  x->skip |= best_mode_skippable;

  assert(best_mode_index >= 0);

  store_coding_context(x, ctx, best_mode_index, best_pred_diff,
                       best_mode_skippable);

#if CONFIG_PALETTE
  if (cm->allow_screen_content_tools && pmi->palette_size[1] > 0) {
    restore_uv_color_map(cpi, x);
  }
#endif  // CONFIG_PALETTE
}

void av1_rd_pick_inter_mode_sb_seg_skip(const AV1_COMP *cpi,
                                        TileDataEnc *tile_data, MACROBLOCK *x,
                                        int mi_row, int mi_col,
                                        RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                        PICK_MODE_CONTEXT *ctx,
                                        int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  unsigned char segment_id = mbmi->segment_id;
  const int comp_pred = 0;
  int i;
  int64_t best_pred_diff[REFERENCE_MODES];
  unsigned int ref_costs_single[TOTAL_REFS_PER_FRAME];
  unsigned int ref_costs_comp[TOTAL_REFS_PER_FRAME];
  aom_prob comp_mode_p;
  InterpFilter best_filter = SWITCHABLE;
  int64_t this_rd = INT64_MAX;
  int rate2 = 0;
  const int64_t distortion2 = 0;
  (void)mi_row;
  (void)mi_col;

  estimate_ref_frame_costs(cm, xd, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < TOTAL_REFS_PER_FRAME; ++i) x->pred_sse[i] = INT_MAX;
  for (i = LAST_FRAME; i < TOTAL_REFS_PER_FRAME; ++i)
    x->pred_mv_sad[i] = INT_MAX;

  rd_cost->rate = INT_MAX;

  assert(segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP));

#if CONFIG_PALETTE
  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
#endif  // CONFIG_PALETTE

#if CONFIG_FILTER_INTRA
  mbmi->filter_intra_mode_info.use_filter_intra_mode[0] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra_mode[1] = 0;
#endif  // CONFIG_FILTER_INTRA
  mbmi->mode = ZEROMV;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->uv_mode = DC_PRED;
  mbmi->ref_frame[0] = LAST_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
#if CONFIG_GLOBAL_MOTION
  mbmi->mv[0].as_int =
      gm_get_motion_vector(&cm->global_motion[mbmi->ref_frame[0]],
                           cm->allow_high_precision_mv, bsize, mi_col, mi_row,
                           0)
          .as_int;
#else   // CONFIG_GLOBAL_MOTION
  mbmi->mv[0].as_int = 0;
#endif  // CONFIG_GLOBAL_MOTION
  mbmi->tx_size = max_txsize_lookup[bsize];
  x->skip = 1;

  mbmi->ref_mv_idx = 0;
  mbmi->pred_mv[0].as_int = 0;

  mbmi->motion_mode = SIMPLE_TRANSLATION;
#if CONFIG_MOTION_VAR
  av1_count_overlappable_neighbors(cm, xd, mi_row, mi_col);
#endif
#if CONFIG_WARPED_MOTION
  if (is_motion_variation_allowed_bsize(bsize) && !has_second_ref(mbmi)) {
    int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
    mbmi->num_proj_ref[0] = findSamples(cm, xd, mi_row, mi_col, pts, pts_inref);
  }
#endif

  set_default_interp_filters(mbmi, cm->interp_filter);

  if (cm->interp_filter != SWITCHABLE) {
    best_filter = cm->interp_filter;
  } else {
    best_filter = EIGHTTAP_REGULAR;
    if (av1_is_interp_needed(xd) && av1_is_interp_search_needed(xd) &&
        x->source_variance >= cpi->sf.disable_filter_search_var_thresh) {
      int rs;
      int best_rs = INT_MAX;
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
#if CONFIG_DUAL_FILTER
        int k;
        for (k = 0; k < 4; ++k) mbmi->interp_filter[k] = i;
#else
        mbmi->interp_filter = i;
#endif  // CONFIG_DUAL_FILTER
        rs = av1_get_switchable_rate(cpi, xd);
        if (rs < best_rs) {
          best_rs = rs;
#if CONFIG_DUAL_FILTER
          best_filter = mbmi->interp_filter[0];
#else
          best_filter = mbmi->interp_filter;
#endif  // CONFIG_DUAL_FILTER
        }
      }
    }
  }
// Set the appropriate filter
#if CONFIG_DUAL_FILTER
  for (i = 0; i < 4; ++i) mbmi->interp_filter[i] = best_filter;
#else
  mbmi->interp_filter = best_filter;
#endif  // CONFIG_DUAL_FILTER
  rate2 += av1_get_switchable_rate(cpi, xd);

  if (cm->reference_mode == REFERENCE_MODE_SELECT)
    rate2 += av1_cost_bit(comp_mode_p, comp_pred);

  // Estimate the reference frame signaling cost and add it
  // to the rolling cost variable.
  rate2 += ref_costs_single[LAST_FRAME];
  this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);

  rd_cost->rate = rate2;
  rd_cost->dist = distortion2;
  rd_cost->rdcost = this_rd;
#if CONFIG_DAALA_DIST && CONFIG_CB4X4
  if (bsize < BLOCK_8X8) rd_cost->dist_y = distortion2;
#endif
  if (this_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

#if CONFIG_DUAL_FILTER
  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == mbmi->interp_filter[0]));
#else
  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == mbmi->interp_filter));
#endif  // CONFIG_DUAL_FILTER

  av1_update_rd_thresh_fact(cm, tile_data->thresh_freq_fact,
                            cpi->sf.adaptive_rd_thresh, bsize, THR_ZEROMV);

  av1_zero(best_pred_diff);

  store_coding_context(x, ctx, THR_ZEROMV, best_pred_diff, 0);
}

#if CONFIG_MOTION_VAR
// This function has a structure similar to av1_build_obmc_inter_prediction
//
// The OBMC predictor is computed as:
//
//  PObmc(x,y) =
//    AOM_BLEND_A64(Mh(x),
//                  AOM_BLEND_A64(Mv(y), P(x,y), PAbove(x,y)),
//                  PLeft(x, y))
//
// Scaling up by AOM_BLEND_A64_MAX_ALPHA ** 2 and omitting the intermediate
// rounding, this can be written as:
//
//  AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA * Pobmc(x,y) =
//    Mh(x) * Mv(y) * P(x,y) +
//      Mh(x) * Cv(y) * Pabove(x,y) +
//      AOM_BLEND_A64_MAX_ALPHA * Ch(x) * PLeft(x, y)
//
// Where :
//
//  Cv(y) = AOM_BLEND_A64_MAX_ALPHA - Mv(y)
//  Ch(y) = AOM_BLEND_A64_MAX_ALPHA - Mh(y)
//
// This function computes 'wsrc' and 'mask' as:
//
//  wsrc(x, y) =
//    AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA * src(x, y) -
//      Mh(x) * Cv(y) * Pabove(x,y) +
//      AOM_BLEND_A64_MAX_ALPHA * Ch(x) * PLeft(x, y)
//
//  mask(x, y) = Mh(x) * Mv(y)
//
// These can then be used to efficiently approximate the error for any
// predictor P in the context of the provided neighbouring predictors by
// computing:
//
//  error(x, y) =
//    wsrc(x, y) - mask(x, y) * P(x, y) / (AOM_BLEND_A64_MAX_ALPHA ** 2)
//
static void calc_target_weighted_pred(const AV1_COMMON *cm, const MACROBLOCK *x,
                                      const MACROBLOCKD *xd, int mi_row,
                                      int mi_col, const uint8_t *above,
                                      int above_stride, const uint8_t *left,
                                      int left_stride) {
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int row, col, i;
  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const int bh = xd->n8_h << MI_SIZE_LOG2;
  int32_t *mask_buf = x->mask_buf;
  int32_t *wsrc_buf = x->wsrc_buf;
  const int wsrc_stride = bw;
  const int mask_stride = bw;
  const int src_scale = AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA;
#if CONFIG_HIGHBITDEPTH
  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
#else
  const int is_hbd = 0;
#endif  // CONFIG_HIGHBITDEPTH

  // plane 0 should not be subsampled
  assert(xd->plane[0].subsampling_x == 0);
  assert(xd->plane[0].subsampling_y == 0);

  av1_zero_array(wsrc_buf, bw * bh);
  for (i = 0; i < bw * bh; ++i) mask_buf[i] = AOM_BLEND_A64_MAX_ALPHA;

  // handle above row
  if (xd->up_available) {
    const int overlap = num_4x4_blocks_high_lookup[bsize] * 2;
    const int miw = AOMMIN(xd->n8_w, cm->mi_cols - mi_col);
    const int mi_row_offset = -1;
    const uint8_t *const mask1d = av1_get_obmc_mask(overlap);
    const int neighbor_limit = max_neighbor_obmc[b_width_log2_lookup[bsize]];
    int neighbor_count = 0;

    assert(miw > 0);

    i = 0;
    do {  // for each mi in the above row
      const int mi_col_offset = i;
      const MB_MODE_INFO *above_mbmi =
          &xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride]->mbmi;
#if CONFIG_CHROMA_SUB8X8
      if (above_mbmi->sb_type < BLOCK_8X8)
        above_mbmi =
            &xd->mi[mi_col_offset + 1 + mi_row_offset * xd->mi_stride]->mbmi;
#endif
      const BLOCK_SIZE a_bsize = AOMMAX(above_mbmi->sb_type, BLOCK_8X8);
      const int mi_step = AOMMIN(xd->n8_w, mi_size_wide[a_bsize]);
      const int neighbor_bw = mi_step * MI_SIZE;

      if (is_neighbor_overlappable(above_mbmi)) {
        if (!CONFIG_CB4X4 && (a_bsize == BLOCK_4X4 || a_bsize == BLOCK_4X8))
          neighbor_count += 2;
        else
          neighbor_count++;
        if (neighbor_count > neighbor_limit) break;

        const int tmp_stride = above_stride;
        int32_t *wsrc = wsrc_buf + (i * MI_SIZE);
        int32_t *mask = mask_buf + (i * MI_SIZE);

        if (!is_hbd) {
          const uint8_t *tmp = above;

          for (row = 0; row < overlap; ++row) {
            const uint8_t m0 = mask1d[row];
            const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
            for (col = 0; col < neighbor_bw; ++col) {
              wsrc[col] = m1 * tmp[col];
              mask[col] = m0;
            }
            wsrc += wsrc_stride;
            mask += mask_stride;
            tmp += tmp_stride;
          }
#if CONFIG_HIGHBITDEPTH
        } else {
          const uint16_t *tmp = CONVERT_TO_SHORTPTR(above);

          for (row = 0; row < overlap; ++row) {
            const uint8_t m0 = mask1d[row];
            const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
            for (col = 0; col < neighbor_bw; ++col) {
              wsrc[col] = m1 * tmp[col];
              mask[col] = m0;
            }
            wsrc += wsrc_stride;
            mask += mask_stride;
            tmp += tmp_stride;
          }
#endif  // CONFIG_HIGHBITDEPTH
        }
      }

      above += neighbor_bw;
      i += mi_step;
    } while (i < miw);
  }

  for (i = 0; i < bw * bh; ++i) {
    wsrc_buf[i] *= AOM_BLEND_A64_MAX_ALPHA;
    mask_buf[i] *= AOM_BLEND_A64_MAX_ALPHA;
  }

  // handle left column
  if (xd->left_available) {
    const int overlap = num_4x4_blocks_wide_lookup[bsize] * 2;
    const int mih = AOMMIN(xd->n8_h, cm->mi_rows - mi_row);
    const int mi_col_offset = -1;
    const uint8_t *const mask1d = av1_get_obmc_mask(overlap);
    const int neighbor_limit = max_neighbor_obmc[b_height_log2_lookup[bsize]];
    int neighbor_count = 0;

    assert(mih > 0);

    i = 0;
    do {  // for each mi in the left column
      const int mi_row_offset = i;
      MB_MODE_INFO *left_mbmi =
          &xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride]->mbmi;

#if CONFIG_CHROMA_SUB8X8
      if (left_mbmi->sb_type < BLOCK_8X8)
        left_mbmi =
            &xd->mi[mi_col_offset + (mi_row_offset + 1) * xd->mi_stride]->mbmi;
#endif
      const BLOCK_SIZE l_bsize = AOMMAX(left_mbmi->sb_type, BLOCK_8X8);
      const int mi_step = AOMMIN(xd->n8_h, mi_size_high[l_bsize]);
      const int neighbor_bh = mi_step * MI_SIZE;

      if (is_neighbor_overlappable(left_mbmi)) {
        if (!CONFIG_CB4X4 && (l_bsize == BLOCK_4X4 || l_bsize == BLOCK_8X4))
          neighbor_count += 2;
        else
          neighbor_count++;
        if (neighbor_count > neighbor_limit) break;

        const int tmp_stride = left_stride;
        int32_t *wsrc = wsrc_buf + (i * MI_SIZE * wsrc_stride);
        int32_t *mask = mask_buf + (i * MI_SIZE * mask_stride);

        if (!is_hbd) {
          const uint8_t *tmp = left;

          for (row = 0; row < neighbor_bh; ++row) {
            for (col = 0; col < overlap; ++col) {
              const uint8_t m0 = mask1d[col];
              const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
              wsrc[col] = (wsrc[col] >> AOM_BLEND_A64_ROUND_BITS) * m0 +
                          (tmp[col] << AOM_BLEND_A64_ROUND_BITS) * m1;
              mask[col] = (mask[col] >> AOM_BLEND_A64_ROUND_BITS) * m0;
            }
            wsrc += wsrc_stride;
            mask += mask_stride;
            tmp += tmp_stride;
          }
#if CONFIG_HIGHBITDEPTH
        } else {
          const uint16_t *tmp = CONVERT_TO_SHORTPTR(left);

          for (row = 0; row < neighbor_bh; ++row) {
            for (col = 0; col < overlap; ++col) {
              const uint8_t m0 = mask1d[col];
              const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
              wsrc[col] = (wsrc[col] >> AOM_BLEND_A64_ROUND_BITS) * m0 +
                          (tmp[col] << AOM_BLEND_A64_ROUND_BITS) * m1;
              mask[col] = (mask[col] >> AOM_BLEND_A64_ROUND_BITS) * m0;
            }
            wsrc += wsrc_stride;
            mask += mask_stride;
            tmp += tmp_stride;
          }
#endif  // CONFIG_HIGHBITDEPTH
        }
      }

      left += neighbor_bh * left_stride;
      i += mi_step;
    } while (i < mih);
  }

  if (!is_hbd) {
    const uint8_t *src = x->plane[0].src.buf;

    for (row = 0; row < bh; ++row) {
      for (col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += wsrc_stride;
      src += x->plane[0].src.stride;
    }
#if CONFIG_HIGHBITDEPTH
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(x->plane[0].src.buf);

    for (row = 0; row < bh; ++row) {
      for (col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += wsrc_stride;
      src += x->plane[0].src.stride;
    }
#endif  // CONFIG_HIGHBITDEPTH
  }
}

#if CONFIG_NCOBMC
void av1_check_ncobmc_rd(const struct AV1_COMP *cpi, struct macroblock *x,
                         int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  MB_MODE_INFO backup_mbmi;
  BLOCK_SIZE bsize = mbmi->sb_type;
  int ref, skip_blk, backup_skip = x->skip;
  int64_t rd_causal;
  RD_STATS rd_stats_y, rd_stats_uv;
  int rate_skip0 = av1_cost_bit(av1_get_skip_prob(cm, xd), 0);
  int rate_skip1 = av1_cost_bit(av1_get_skip_prob(cm, xd), 1);

  // Recompute the best causal predictor and rd
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[ref]);
    assert(cfg != NULL);
    av1_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                         &xd->block_refs[ref]->sf);
  }
  av1_setup_dst_planes(x->e_mbd.plane, bsize,
                       get_frame_new_buffer(&cpi->common), mi_row, mi_col);

  av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);

  av1_subtract_plane(x, bsize, 0);
  super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
  super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
  assert(rd_stats_y.rate != INT_MAX && rd_stats_uv.rate != INT_MAX);
  if (rd_stats_y.skip && rd_stats_uv.skip) {
    rd_stats_y.rate = rate_skip1;
    rd_stats_uv.rate = 0;
    rd_stats_y.dist = rd_stats_y.sse;
    rd_stats_uv.dist = rd_stats_uv.sse;
    skip_blk = 0;
  } else if (RDCOST(x->rdmult, x->rddiv,
                    (rd_stats_y.rate + rd_stats_uv.rate + rate_skip0),
                    (rd_stats_y.dist + rd_stats_uv.dist)) >
             RDCOST(x->rdmult, x->rddiv, rate_skip1,
                    (rd_stats_y.sse + rd_stats_uv.sse))) {
    rd_stats_y.rate = rate_skip1;
    rd_stats_uv.rate = 0;
    rd_stats_y.dist = rd_stats_y.sse;
    rd_stats_uv.dist = rd_stats_uv.sse;
    skip_blk = 1;
  } else {
    rd_stats_y.rate += rate_skip0;
    skip_blk = 0;
  }
  backup_skip = skip_blk;
  backup_mbmi = *mbmi;
  rd_causal = RDCOST(x->rdmult, x->rddiv, (rd_stats_y.rate + rd_stats_uv.rate),
                     (rd_stats_y.dist + rd_stats_uv.dist));
  rd_causal += RDCOST(x->rdmult, x->rddiv,
                      av1_cost_bit(cm->fc->motion_mode_prob[bsize][0], 0), 0);

  // Check non-causal mode
  mbmi->motion_mode = OBMC_CAUSAL;
  av1_build_ncobmc_inter_predictors_sb(cm, xd, mi_row, mi_col);

  av1_subtract_plane(x, bsize, 0);
  super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
  super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
  assert(rd_stats_y.rate != INT_MAX && rd_stats_uv.rate != INT_MAX);
  if (rd_stats_y.skip && rd_stats_uv.skip) {
    rd_stats_y.rate = rate_skip1;
    rd_stats_uv.rate = 0;
    rd_stats_y.dist = rd_stats_y.sse;
    rd_stats_uv.dist = rd_stats_uv.sse;
    skip_blk = 0;
  } else if (RDCOST(x->rdmult, x->rddiv,
                    (rd_stats_y.rate + rd_stats_uv.rate + rate_skip0),
                    (rd_stats_y.dist + rd_stats_uv.dist)) >
             RDCOST(x->rdmult, x->rddiv, rate_skip1,
                    (rd_stats_y.sse + rd_stats_uv.sse))) {
    rd_stats_y.rate = rate_skip1;
    rd_stats_uv.rate = 0;
    rd_stats_y.dist = rd_stats_y.sse;
    rd_stats_uv.dist = rd_stats_uv.sse;
    skip_blk = 1;
  } else {
    rd_stats_y.rate += rate_skip0;
    skip_blk = 0;
  }

  if (rd_causal >
      RDCOST(x->rdmult, x->rddiv,
             rd_stats_y.rate + rd_stats_uv.rate +
                 av1_cost_bit(cm->fc->motion_mode_prob[bsize][0], 1),
             (rd_stats_y.dist + rd_stats_uv.dist))) {
    x->skip = skip_blk;
  } else {
    *mbmi = backup_mbmi;
    x->skip = backup_skip;
  }
}
#endif  // CONFIG_NCOBMC
#endif  // CONFIG_MOTION_VAR
