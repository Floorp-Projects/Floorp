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

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/cfl.h"
#include "av1/common/common.h"
#include "av1/common/common_data.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/idct.h"
#include "av1/common/mvref_common.h"
#include "av1/common/obmc.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/scan.h"
#include "av1/common/seg_common.h"
#include "av1/common/txb_common.h"
#include "av1/common/warped_motion.h"

#include "av1/encoder/aq_variance.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ml.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/pustats.h"
#include "av1/encoder/random.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/tokenize.h"
#include "av1/encoder/tx_prune_model_weights.h"

#define DNN_BASED_RD_INTERP_FILTER 0

// Set this macro as 1 to collect data about tx size selection.
#define COLLECT_TX_SIZE_DATA 0

#if COLLECT_TX_SIZE_DATA
static const char av1_tx_size_data_output_file[] = "tx_size_data.txt";
#endif

#define DUAL_FILTER_SET_SIZE (SWITCHABLE_FILTERS * SWITCHABLE_FILTERS)
static const InterpFilters filter_sets[DUAL_FILTER_SET_SIZE] = {
  0x00000000, 0x00010000, 0x00020000,  // y = 0
  0x00000001, 0x00010001, 0x00020001,  // y = 1
  0x00000002, 0x00010002, 0x00020002,  // y = 2
};

#define SECOND_REF_FRAME_MASK                                         \
  ((1 << ALTREF_FRAME) | (1 << ALTREF2_FRAME) | (1 << BWDREF_FRAME) | \
   (1 << GOLDEN_FRAME) | (1 << LAST2_FRAME) | 0x01)

#define ANGLE_SKIP_THRESH 10

static const double ADST_FLIP_SVM[8] = {
  /* vertical */
  -6.6623, -2.8062, -3.2531, 3.1671,
  /* horizontal */
  -7.7051, -3.2234, -3.6193, 3.4533
};

typedef struct {
  PREDICTION_MODE mode;
  MV_REFERENCE_FRAME ref_frame[2];
} MODE_DEFINITION;

typedef struct {
  MV_REFERENCE_FRAME ref_frame[2];
} REF_DEFINITION;

typedef enum {
  FTXS_NONE = 0,
  FTXS_DCT_AND_1D_DCT_ONLY = 1 << 0,
  FTXS_DISABLE_TRELLIS_OPT = 1 << 1,
  FTXS_USE_TRANSFORM_DOMAIN = 1 << 2
} FAST_TX_SEARCH_MODE;

struct rdcost_block_args {
  const AV1_COMP *cpi;
  MACROBLOCK *x;
  ENTROPY_CONTEXT t_above[MAX_MIB_SIZE];
  ENTROPY_CONTEXT t_left[MAX_MIB_SIZE];
  RD_STATS rd_stats;
  int64_t this_rd;
  int64_t best_rd;
  int exit_early;
  int use_fast_coef_costing;
  FAST_TX_SEARCH_MODE ftxs_mode;
};

#define LAST_NEW_MV_INDEX 6
static const MODE_DEFINITION av1_mode_order[MAX_MODES] = {
  { NEARESTMV, { LAST_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARESTMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEARESTMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, NONE_FRAME } },

  { DC_PRED, { INTRA_FRAME, NONE_FRAME } },

  { NEWMV, { LAST_FRAME, NONE_FRAME } },
  { NEWMV, { LAST2_FRAME, NONE_FRAME } },
  { NEWMV, { LAST3_FRAME, NONE_FRAME } },
  { NEWMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEWMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEWMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEWMV, { GOLDEN_FRAME, NONE_FRAME } },

  { NEARMV, { LAST_FRAME, NONE_FRAME } },
  { NEARMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEARMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEARMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARMV, { GOLDEN_FRAME, NONE_FRAME } },

  { GLOBALMV, { LAST_FRAME, NONE_FRAME } },
  { GLOBALMV, { LAST2_FRAME, NONE_FRAME } },
  { GLOBALMV, { LAST3_FRAME, NONE_FRAME } },
  { GLOBALMV, { BWDREF_FRAME, NONE_FRAME } },
  { GLOBALMV, { ALTREF2_FRAME, NONE_FRAME } },
  { GLOBALMV, { GOLDEN_FRAME, NONE_FRAME } },
  { GLOBALMV, { ALTREF_FRAME, NONE_FRAME } },

  // TODO(zoeliu): May need to reconsider the order on the modes to check

  { NEAREST_NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, ALTREF2_FRAME } },

  { NEAREST_NEARESTMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAREST_NEARESTMV, { BWDREF_FRAME, ALTREF_FRAME } },

  { PAETH_PRED, { INTRA_FRAME, NONE_FRAME } },

  { SMOOTH_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_H_PRED, { INTRA_FRAME, NONE_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, ALTREF2_FRAME } },

  { H_PRED, { INTRA_FRAME, NONE_FRAME } },
  { V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D135_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D203_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D157_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D67_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D113_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D45_PRED, { INTRA_FRAME, NONE_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, LAST2_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, LAST3_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, GOLDEN_FRAME } },

  { NEAR_NEARMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { BWDREF_FRAME, ALTREF_FRAME } },
};

static const int16_t intra_to_mode_idx[INTRA_MODE_NUM] = {
  7,    // DC_PRED,
  134,  // V_PRED,
  133,  // H_PRED,
  140,  // D45_PRED,
  135,  // D135_PRED,
  139,  // D113_PRED,
  137,  // D157_PRED,
  136,  // D203_PRED,
  138,  // D67_PRED,
  46,   // SMOOTH_PRED,
  47,   // SMOOTH_V_PRED,
  48,   // SMOOTH_H_PRED,
  45,   // PAETH_PRED,
};

/* clang-format off */
static const int16_t single_inter_to_mode_idx[SINGLE_INTER_MODE_NUM]
                                             [REF_FRAMES] = {
  // NEARESTMV,
  { -1, 0, 1, 2, 6, 3, 4, 5, },
  // NEARMV,
  { -1, 15, 16, 17, 21, 18, 19, 20, },
  // GLOBALMV,
  { -1, 22, 23, 24, 27, 25, 26, 28, },
  // NEWMV,
  { -1, 8, 9, 10, 14, 11, 12, 13, },
};
/* clang-format on */

/* clang-format off */
static const int16_t comp_inter_to_mode_idx[COMP_INTER_MODE_NUM][REF_FRAMES]
                                     [REF_FRAMES] = {
  // NEAREST_NEARESTMV,
  {
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, 41, 42, 43, 33, 37, 29, },
      { -1, -1, -1, -1, -1, 34, 38, 30, },
      { -1, -1, -1, -1, -1, 35, 39, 31, },
      { -1, -1, -1, -1, -1, 36, 40, 32, },
      { -1, -1, -1, -1, -1, -1, -1, 44, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
  },
  // NEAR_NEARMV,
  {
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, 141, 148, 155, 77, 105, 49, },
      { -1, -1, -1, -1, -1, 84, 112, 56, },
      { -1, -1, -1, -1, -1, 91, 119, 63, },
      { -1, -1, -1, -1, -1, 98, 126, 70, },
      { -1, -1, -1, -1, -1, -1, -1, 162, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
  },
  // NEAREST_NEWMV,
  {
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, 143, 150, 157, 79, 107, 51, },
      { -1, -1, -1, -1, -1, 86, 114, 58, },
      { -1, -1, -1, -1, -1, 93, 121, 65, },
      { -1, -1, -1, -1, -1, 100, 128, 72, },
      { -1, -1, -1, -1, -1, -1, -1, 164, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
  },
  // NEW_NEARESTMV,
  {
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, 142, 149, 156, 78, 106, 50, },
      { -1, -1, -1, -1, -1, 85, 113, 57, },
      { -1, -1, -1, -1, -1, 92, 120, 64, },
      { -1, -1, -1, -1, -1, 99, 127, 71, },
      { -1, -1, -1, -1, -1, -1, -1, 163, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
  },
  // NEAR_NEWMV,
  {
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, 145, 152, 159, 81, 109, 53, },
      { -1, -1, -1, -1, -1, 88, 116, 60, },
      { -1, -1, -1, -1, -1, 95, 123, 67, },
      { -1, -1, -1, -1, -1, 102, 130, 74, },
      { -1, -1, -1, -1, -1, -1, -1, 166, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
  },
  // NEW_NEARMV,
  {
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, 144, 151, 158, 80, 108, 52, },
      { -1, -1, -1, -1, -1, 87, 115, 59, },
      { -1, -1, -1, -1, -1, 94, 122, 66, },
      { -1, -1, -1, -1, -1, 101, 129, 73, },
      { -1, -1, -1, -1, -1, -1, -1, 165, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
  },
  // GLOBAL_GLOBALMV,
  {
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, 147, 154, 161, 83, 111, 55, },
      { -1, -1, -1, -1, -1, 90, 118, 62, },
      { -1, -1, -1, -1, -1, 97, 125, 69, },
      { -1, -1, -1, -1, -1, 104, 132, 76, },
      { -1, -1, -1, -1, -1, -1, -1, 168, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
  },
  // NEW_NEWMV,
  {
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, 146, 153, 160, 82, 110, 54, },
      { -1, -1, -1, -1, -1, 89, 117, 61, },
      { -1, -1, -1, -1, -1, 96, 124, 68, },
      { -1, -1, -1, -1, -1, 103, 131, 75, },
      { -1, -1, -1, -1, -1, -1, -1, 167, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
      { -1, -1, -1, -1, -1, -1, -1, -1, },
  },
};
/* clang-format on */

static int get_prediction_mode_idx(PREDICTION_MODE this_mode,
                                   MV_REFERENCE_FRAME ref_frame,
                                   MV_REFERENCE_FRAME second_ref_frame) {
  if (this_mode < INTRA_MODE_END) {
    assert(ref_frame == INTRA_FRAME);
    assert(second_ref_frame == NONE_FRAME);
    return intra_to_mode_idx[this_mode - INTRA_MODE_START];
  }
  if (this_mode >= SINGLE_INTER_MODE_START &&
      this_mode < SINGLE_INTER_MODE_END) {
    assert((ref_frame > INTRA_FRAME) && (ref_frame <= ALTREF_FRAME));
    assert(second_ref_frame == NONE_FRAME);
    return single_inter_to_mode_idx[this_mode - SINGLE_INTER_MODE_START]
                                   [ref_frame];
  }
  if (this_mode >= COMP_INTER_MODE_START && this_mode < COMP_INTER_MODE_END) {
    assert((ref_frame > INTRA_FRAME) && (ref_frame <= ALTREF_FRAME));
    assert((second_ref_frame > INTRA_FRAME) &&
           (second_ref_frame <= ALTREF_FRAME));
    return comp_inter_to_mode_idx[this_mode - COMP_INTER_MODE_START][ref_frame]
                                 [second_ref_frame];
  }
  assert(0);
  return -1;
}

static const PREDICTION_MODE intra_rd_search_mode_order[INTRA_MODES] = {
  DC_PRED,       H_PRED,        V_PRED,    SMOOTH_PRED, PAETH_PRED,
  SMOOTH_V_PRED, SMOOTH_H_PRED, D135_PRED, D203_PRED,   D157_PRED,
  D67_PRED,      D113_PRED,     D45_PRED,
};

static const UV_PREDICTION_MODE uv_rd_search_mode_order[UV_INTRA_MODES] = {
  UV_DC_PRED,     UV_CFL_PRED,   UV_H_PRED,        UV_V_PRED,
  UV_SMOOTH_PRED, UV_PAETH_PRED, UV_SMOOTH_V_PRED, UV_SMOOTH_H_PRED,
  UV_D135_PRED,   UV_D203_PRED,  UV_D157_PRED,     UV_D67_PRED,
  UV_D113_PRED,   UV_D45_PRED,
};

typedef struct InterModeSearchState {
  int64_t best_rd;
  MB_MODE_INFO best_mbmode;
  int best_rate_y;
  int best_rate_uv;
  int best_mode_skippable;
  int best_skip2;
  int best_mode_index;
  int skip_intra_modes;
  int num_available_refs;
  int64_t dist_refs[REF_FRAMES];
  int dist_order_refs[REF_FRAMES];
  int64_t mode_threshold[MAX_MODES];
  PREDICTION_MODE best_intra_mode;
  int64_t best_intra_rd;
  int angle_stats_ready;
  uint8_t directional_mode_skip_mask[INTRA_MODES];
  unsigned int best_pred_sse;
  int rate_uv_intra[TX_SIZES_ALL];
  int rate_uv_tokenonly[TX_SIZES_ALL];
  int64_t dist_uvs[TX_SIZES_ALL];
  int skip_uvs[TX_SIZES_ALL];
  UV_PREDICTION_MODE mode_uv[TX_SIZES_ALL];
  PALETTE_MODE_INFO pmi_uv[TX_SIZES_ALL];
  int8_t uv_angle_delta[TX_SIZES_ALL];
  int64_t best_pred_rd[REFERENCE_MODES];
  int64_t best_pred_diff[REFERENCE_MODES];
  // Save a set of single_newmv for each checked ref_mv.
  int_mv single_newmv[MAX_REF_MV_SERCH][REF_FRAMES];
  int single_newmv_rate[MAX_REF_MV_SERCH][REF_FRAMES];
  int single_newmv_valid[MAX_REF_MV_SERCH][REF_FRAMES];
  int64_t modelled_rd[MB_MODE_COUNT][REF_FRAMES];
} InterModeSearchState;

#if CONFIG_COLLECT_INTER_MODE_RD_STATS

typedef struct InterModeRdModel {
  int ready;
  double a;
  double b;
  double dist_mean;
  int skip_count;
  int non_skip_count;
  int fp_skip_count;
  int bracket_idx;
} InterModeRdModel;

InterModeRdModel inter_mode_rd_models[BLOCK_SIZES_ALL];

#define INTER_MODE_RD_DATA_OVERALL_SIZE 6400
static int inter_mode_data_idx[4];
static int64_t inter_mode_data_sse[4][INTER_MODE_RD_DATA_OVERALL_SIZE];
static int64_t inter_mode_data_dist[4][INTER_MODE_RD_DATA_OVERALL_SIZE];
static int inter_mode_data_residue_cost[4][INTER_MODE_RD_DATA_OVERALL_SIZE];
static int inter_mode_data_all_cost[4][INTER_MODE_RD_DATA_OVERALL_SIZE];
static int64_t inter_mode_data_ref_best_rd[4][INTER_MODE_RD_DATA_OVERALL_SIZE];

int inter_mode_data_block_idx(BLOCK_SIZE bsize) {
  if (bsize == BLOCK_8X8) return 1;
  if (bsize == BLOCK_16X16) return 2;
  if (bsize == BLOCK_32X32) return 3;
  return -1;
}

void av1_inter_mode_data_init() {
  for (int i = 0; i < BLOCK_SIZES_ALL; ++i) {
    const int block_idx = inter_mode_data_block_idx(i);
    if (block_idx != -1) inter_mode_data_idx[block_idx] = 0;
    InterModeRdModel *md = &inter_mode_rd_models[i];
    md->ready = 0;
    md->skip_count = 0;
    md->non_skip_count = 0;
    md->fp_skip_count = 0;
    md->bracket_idx = 0;
  }
}

void av1_inter_mode_data_show(const AV1_COMMON *cm) {
  printf("frame_offset %d\n", cm->frame_offset);
  for (int i = 0; i < BLOCK_SIZES_ALL; ++i) {
    const int block_idx = inter_mode_data_block_idx(i);
    if (block_idx != -1) inter_mode_data_idx[block_idx] = 0;
    InterModeRdModel *md = &inter_mode_rd_models[i];
    if (md->ready) {
      printf("bsize %d non_skip_count %d skip_count %d fp_skip_count %d\n", i,
             md->non_skip_count, md->skip_count, md->fp_skip_count);
    }
  }
}

static int64_t get_est_rd(BLOCK_SIZE bsize, int rdmult, int64_t sse,
                          int curr_cost) {
  aom_clear_system_state();
  InterModeRdModel *md = &inter_mode_rd_models[bsize];
  if (md->ready) {
    const double est_ld = md->a * sse + md->b;
    const double est_residue_cost = (sse - md->dist_mean) / est_ld;
    const int64_t est_cost = (int64_t)round(est_residue_cost) + curr_cost;
    const int64_t int64_dist_mean = (int64_t)round(md->dist_mean);
    const int64_t est_rd = RDCOST(rdmult, est_cost, int64_dist_mean);
    return est_rd;
  }
  return 0;
}

#define DATA_BRACKETS 7
static const int data_num_threshold[DATA_BRACKETS] = {
  200, 400, 800, 1600, 3200, 6400, INT32_MAX
};

void av1_inter_mode_data_fit(int rdmult) {
  aom_clear_system_state();
  for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
    const int block_idx = inter_mode_data_block_idx(bsize);
    InterModeRdModel *md = &inter_mode_rd_models[bsize];
    if (block_idx == -1) continue;
    int data_num = inter_mode_data_idx[block_idx];
    if (data_num < data_num_threshold[md->bracket_idx]) {
      continue;
    }
    double my = 0;
    double mx = 0;
    double dx = 0;
    double dxy = 0;
    double dist_mean = 0;
    const int train_num = data_num;
    for (int i = 0; i < train_num; ++i) {
      const double sse = (double)inter_mode_data_sse[block_idx][i];
      const double dist = (double)inter_mode_data_dist[block_idx][i];
      const double residue_cost = inter_mode_data_residue_cost[block_idx][i];
      const double ld = (sse - dist) / residue_cost;
      dist_mean += dist;
      my += ld;
      mx += sse;
      dx += sse * sse;
      dxy += sse * ld;
    }
    dist_mean = dist_mean / data_num;
    my = my / train_num;
    mx = mx / train_num;
    dx = sqrt(dx / train_num);
    dxy = dxy / train_num;

    md->dist_mean = dist_mean;
    md->a = (dxy - mx * my) / (dx * dx - mx * mx);
    md->b = my - md->a * mx;
    ++md->bracket_idx;
    md->ready = 1;
    assert(md->bracket_idx < DATA_BRACKETS);

    (void)rdmult;
#if 0
    int skip_count = 0;
    int fp_skip_count = 0;
    double avg_error = 0;
    const int test_num = data_num;
    for (int i = 0; i < data_num; ++i) {
      const int64_t sse = inter_mode_data_sse[block_idx][i];
      const int64_t dist = inter_mode_data_dist[block_idx][i];
      const int64_t residue_cost = inter_mode_data_residue_cost[block_idx][i];
      const int64_t all_cost = inter_mode_data_all_cost[block_idx][i];
      const int64_t est_rd =
          get_est_rd(bsize, rdmult, sse, all_cost - residue_cost);
      const int64_t real_rd = RDCOST(rdmult, all_cost, dist);
      const int64_t ref_best_rd = inter_mode_data_ref_best_rd[block_idx][i];
      if (est_rd > ref_best_rd) {
        ++skip_count;
        if (real_rd < ref_best_rd) {
          ++fp_skip_count;
        }
      }
      avg_error += abs(est_rd - real_rd) * 100. / real_rd;
    }
    avg_error /= test_num;
    printf("test_num %d bsize %d avg_error %f skip_count %d fp_skip_count %d\n",
           test_num, bsize, avg_error, skip_count, fp_skip_count);
#endif
  }
}

static void inter_mode_data_push(BLOCK_SIZE bsize, int64_t sse, int64_t dist,
                                 int residue_cost, int all_cost,
                                 int64_t ref_best_rd) {
  if (residue_cost == 0 || sse == dist) return;
  const int block_idx = inter_mode_data_block_idx(bsize);
  if (block_idx == -1) return;
  if (inter_mode_data_idx[block_idx] < INTER_MODE_RD_DATA_OVERALL_SIZE) {
    const int data_idx = inter_mode_data_idx[block_idx];
    inter_mode_data_sse[block_idx][data_idx] = sse;
    inter_mode_data_dist[block_idx][data_idx] = dist;
    inter_mode_data_residue_cost[block_idx][data_idx] = residue_cost;
    inter_mode_data_all_cost[block_idx][data_idx] = all_cost;
    inter_mode_data_ref_best_rd[block_idx][data_idx] = ref_best_rd;
    ++inter_mode_data_idx[block_idx];
  }
}
#endif  // CONFIG_COLLECT_INTER_MODE_RD_STATS

static INLINE int write_uniform_cost(int n, int v) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  if (l == 0) return 0;
  if (v < m)
    return av1_cost_literal(l - 1);
  else
    return av1_cost_literal(l);
}

// Similar to store_cfl_required(), but for use during the RDO process,
// where we haven't yet determined whether this block uses CfL.
static INLINE CFL_ALLOWED_TYPE store_cfl_required_rdo(const AV1_COMMON *cm,
                                                      const MACROBLOCK *x) {
  const MACROBLOCKD *xd = &x->e_mbd;

  if (cm->seq_params.monochrome || x->skip_chroma_rd) return CFL_DISALLOWED;

  if (!xd->cfl.is_chroma_reference) {
    // For non-chroma-reference blocks, we should always store the luma pixels,
    // in case the corresponding chroma-reference block uses CfL.
    // Note that this can only happen for block sizes which are <8 on
    // their shortest side, as otherwise they would be chroma reference
    // blocks.
    return CFL_ALLOWED;
  }

  // For chroma reference blocks, we should store data in the encoder iff we're
  // allowed to try out CfL.
  return is_cfl_allowed(xd);
}

// constants for prune 1 and prune 2 decision boundaries
#define FAST_EXT_TX_CORR_MID 0.0
#define FAST_EXT_TX_EDST_MID 0.1
#define FAST_EXT_TX_CORR_MARGIN 0.5
#define FAST_EXT_TX_EDST_MARGIN 0.3

static int inter_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                           RD_STATS *rd_stats, BLOCK_SIZE bsize,
                           int64_t ref_best_rd, FAST_TX_SEARCH_MODE ftxs_mode);

static unsigned pixel_dist_visible_only(
    const AV1_COMP *const cpi, const MACROBLOCK *x, const uint8_t *src,
    const int src_stride, const uint8_t *dst, const int dst_stride,
    const BLOCK_SIZE tx_bsize, int txb_rows, int txb_cols, int visible_rows,
    int visible_cols) {
  unsigned sse;

  if (txb_rows == visible_rows && txb_cols == visible_cols) {
    cpi->fn_ptr[tx_bsize].vf(src, src_stride, dst, dst_stride, &sse);
    return sse;
  }
  const MACROBLOCKD *xd = &x->e_mbd;

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    uint64_t sse64 = aom_highbd_sse_odd_size(src, src_stride, dst, dst_stride,
                                             visible_cols, visible_rows);
    return (unsigned int)ROUND_POWER_OF_TWO(sse64, (xd->bd - 8) * 2);
  }
  sse = aom_sse_odd_size(src, src_stride, dst, dst_stride, visible_cols,
                         visible_rows);
  return sse;
}

#if CONFIG_DIST_8X8
static uint64_t cdef_dist_8x8_16bit(uint16_t *dst, int dstride, uint16_t *src,
                                    int sstride, int coeff_shift) {
  uint64_t svar = 0;
  uint64_t dvar = 0;
  uint64_t sum_s = 0;
  uint64_t sum_d = 0;
  uint64_t sum_s2 = 0;
  uint64_t sum_d2 = 0;
  uint64_t sum_sd = 0;
  uint64_t dist = 0;

  int i, j;
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      sum_s += src[i * sstride + j];
      sum_d += dst[i * dstride + j];
      sum_s2 += src[i * sstride + j] * src[i * sstride + j];
      sum_d2 += dst[i * dstride + j] * dst[i * dstride + j];
      sum_sd += src[i * sstride + j] * dst[i * dstride + j];
    }
  }
  /* Compute the variance -- the calculation cannot go negative. */
  svar = sum_s2 - ((sum_s * sum_s + 32) >> 6);
  dvar = sum_d2 - ((sum_d * sum_d + 32) >> 6);

  // Tuning of jm's original dering distortion metric used in CDEF tool,
  // suggested by jm
  const uint64_t a = 4;
  const uint64_t b = 2;
  const uint64_t c1 = (400 * a << 2 * coeff_shift);
  const uint64_t c2 = (b * 20000 * a * a << 4 * coeff_shift);

  dist = (uint64_t)floor(.5 + (sum_d2 + sum_s2 - 2 * sum_sd) * .5 *
                                  (svar + dvar + c1) /
                                  (sqrt(svar * (double)dvar + c2)));

  // Calibrate dist to have similar rate for the same QP with MSE only
  // distortion (as in master branch)
  dist = (uint64_t)((float)dist * 0.75);

  return dist;
}

static int od_compute_var_4x4(uint16_t *x, int stride) {
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

  return (s2 - (sum * sum >> 4)) >> 4;
}

/* OD_DIST_LP_MID controls the frequency weighting filter used for computing
   the distortion. For a value X, the filter is [1 X 1]/(X + 2) and
   is applied both horizontally and vertically. For X=5, the filter is
   a good approximation for the OD_QM8_Q4_HVS quantization matrix. */
#define OD_DIST_LP_MID (5)
#define OD_DIST_LP_NORM (OD_DIST_LP_MID + 2)

static double od_compute_dist_8x8(int use_activity_masking, uint16_t *x,
                                  uint16_t *y, od_coeff *e_lp, int stride) {
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
     masking is used, since the harmonic mean appeared slightly worse with
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
static double od_compute_dist_common(int activity_masking, uint16_t *x,
                                     uint16_t *y, int bsize_w, int bsize_h,
                                     int qindex, od_coeff *tmp,
                                     od_coeff *e_lp) {
  int i, j;
  double sum = 0;
  const int mid = OD_DIST_LP_MID;

  for (j = 0; j < bsize_w; j++) {
    e_lp[j] = mid * tmp[j] + 2 * tmp[bsize_w + j];
    e_lp[(bsize_h - 1) * bsize_w + j] = mid * tmp[(bsize_h - 1) * bsize_w + j] +
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
      sum += od_compute_dist_8x8(activity_masking, &x[i * bsize_w + j],
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
               : qindex <= 43 ? 1.5 + (2.0 - 1.5) * (qindex - 43) / (16 - 43)
                              : 1.5 + (1.4 - 1.5) * (qindex - 43) / (128 - 43);
  }

  return sum;
}

static double od_compute_dist(uint16_t *x, uint16_t *y, int bsize_w,
                              int bsize_h, int qindex) {
  assert(bsize_w >= 8 && bsize_h >= 8);

  int activity_masking = 0;

  int i, j;
  DECLARE_ALIGNED(16, od_coeff, e[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, tmp[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, e_lp[MAX_SB_SQUARE]);
  for (i = 0; i < bsize_h; i++) {
    for (j = 0; j < bsize_w; j++) {
      e[i * bsize_w + j] = x[i * bsize_w + j] - y[i * bsize_w + j];
    }
  }
  int mid = OD_DIST_LP_MID;
  for (i = 0; i < bsize_h; i++) {
    tmp[i * bsize_w] = mid * e[i * bsize_w] + 2 * e[i * bsize_w + 1];
    tmp[i * bsize_w + bsize_w - 1] =
        mid * e[i * bsize_w + bsize_w - 1] + 2 * e[i * bsize_w + bsize_w - 2];
    for (j = 1; j < bsize_w - 1; j++) {
      tmp[i * bsize_w + j] = mid * e[i * bsize_w + j] + e[i * bsize_w + j - 1] +
                             e[i * bsize_w + j + 1];
    }
  }
  return od_compute_dist_common(activity_masking, x, y, bsize_w, bsize_h,
                                qindex, tmp, e_lp);
}

static double od_compute_dist_diff(uint16_t *x, int16_t *e, int bsize_w,
                                   int bsize_h, int qindex) {
  assert(bsize_w >= 8 && bsize_h >= 8);

  int activity_masking = 0;

  DECLARE_ALIGNED(16, uint16_t, y[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, tmp[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, od_coeff, e_lp[MAX_SB_SQUARE]);
  int i, j;
  for (i = 0; i < bsize_h; i++) {
    for (j = 0; j < bsize_w; j++) {
      y[i * bsize_w + j] = x[i * bsize_w + j] - e[i * bsize_w + j];
    }
  }
  int mid = OD_DIST_LP_MID;
  for (i = 0; i < bsize_h; i++) {
    tmp[i * bsize_w] = mid * e[i * bsize_w] + 2 * e[i * bsize_w + 1];
    tmp[i * bsize_w + bsize_w - 1] =
        mid * e[i * bsize_w + bsize_w - 1] + 2 * e[i * bsize_w + bsize_w - 2];
    for (j = 1; j < bsize_w - 1; j++) {
      tmp[i * bsize_w + j] = mid * e[i * bsize_w + j] + e[i * bsize_w + j - 1] +
                             e[i * bsize_w + j + 1];
    }
  }
  return od_compute_dist_common(activity_masking, x, y, bsize_w, bsize_h,
                                qindex, tmp, e_lp);
}

int64_t av1_dist_8x8(const AV1_COMP *const cpi, const MACROBLOCK *x,
                     const uint8_t *src, int src_stride, const uint8_t *dst,
                     int dst_stride, const BLOCK_SIZE tx_bsize, int bsw,
                     int bsh, int visible_w, int visible_h, int qindex) {
  int64_t d = 0;
  int i, j;
  const MACROBLOCKD *xd = &x->e_mbd;

  DECLARE_ALIGNED(16, uint16_t, orig[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint16_t, rec[MAX_SB_SQUARE]);

  assert(bsw >= 8);
  assert(bsh >= 8);
  assert((bsw & 0x07) == 0);
  assert((bsh & 0x07) == 0);

  if (x->tune_metric == AOM_TUNE_CDEF_DIST ||
      x->tune_metric == AOM_TUNE_DAALA_DIST) {
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          orig[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];

      if ((bsw == visible_w) && (bsh == visible_h)) {
        for (j = 0; j < bsh; j++)
          for (i = 0; i < bsw; i++)
            rec[j * bsw + i] = CONVERT_TO_SHORTPTR(dst)[j * dst_stride + i];
      } else {
        for (j = 0; j < visible_h; j++)
          for (i = 0; i < visible_w; i++)
            rec[j * bsw + i] = CONVERT_TO_SHORTPTR(dst)[j * dst_stride + i];

        if (visible_w < bsw) {
          for (j = 0; j < bsh; j++)
            for (i = visible_w; i < bsw; i++)
              rec[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];
        }

        if (visible_h < bsh) {
          for (j = visible_h; j < bsh; j++)
            for (i = 0; i < bsw; i++)
              rec[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];
        }
      }
    } else {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++) orig[j * bsw + i] = src[j * src_stride + i];

      if ((bsw == visible_w) && (bsh == visible_h)) {
        for (j = 0; j < bsh; j++)
          for (i = 0; i < bsw; i++) rec[j * bsw + i] = dst[j * dst_stride + i];
      } else {
        for (j = 0; j < visible_h; j++)
          for (i = 0; i < visible_w; i++)
            rec[j * bsw + i] = dst[j * dst_stride + i];

        if (visible_w < bsw) {
          for (j = 0; j < bsh; j++)
            for (i = visible_w; i < bsw; i++)
              rec[j * bsw + i] = src[j * src_stride + i];
        }

        if (visible_h < bsh) {
          for (j = visible_h; j < bsh; j++)
            for (i = 0; i < bsw; i++)
              rec[j * bsw + i] = src[j * src_stride + i];
        }
      }
    }
  }

  if (x->tune_metric == AOM_TUNE_DAALA_DIST) {
    d = (int64_t)od_compute_dist(orig, rec, bsw, bsh, qindex);
  } else if (x->tune_metric == AOM_TUNE_CDEF_DIST) {
    int coeff_shift = AOMMAX(xd->bd - 8, 0);

    for (i = 0; i < bsh; i += 8) {
      for (j = 0; j < bsw; j += 8) {
        d += cdef_dist_8x8_16bit(&rec[i * bsw + j], bsw, &orig[i * bsw + j],
                                 bsw, coeff_shift);
      }
    }
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      d = ((uint64_t)d) >> 2 * coeff_shift;
  } else {
    // Otherwise, MSE by default
    d = pixel_dist_visible_only(cpi, x, src, src_stride, dst, dst_stride,
                                tx_bsize, bsh, bsw, visible_h, visible_w);
  }

  return d;
}

static int64_t dist_8x8_diff(const MACROBLOCK *x, const uint8_t *src,
                             int src_stride, const int16_t *diff,
                             int diff_stride, int bsw, int bsh, int visible_w,
                             int visible_h, int qindex) {
  int64_t d = 0;
  int i, j;
  const MACROBLOCKD *xd = &x->e_mbd;

  DECLARE_ALIGNED(16, uint16_t, orig[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, int16_t, diff16[MAX_SB_SQUARE]);

  assert(bsw >= 8);
  assert(bsh >= 8);
  assert((bsw & 0x07) == 0);
  assert((bsh & 0x07) == 0);

  if (x->tune_metric == AOM_TUNE_CDEF_DIST ||
      x->tune_metric == AOM_TUNE_DAALA_DIST) {
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          orig[j * bsw + i] = CONVERT_TO_SHORTPTR(src)[j * src_stride + i];
    } else {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++) orig[j * bsw + i] = src[j * src_stride + i];
    }

    if ((bsw == visible_w) && (bsh == visible_h)) {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          diff16[j * bsw + i] = diff[j * diff_stride + i];
    } else {
      for (j = 0; j < visible_h; j++)
        for (i = 0; i < visible_w; i++)
          diff16[j * bsw + i] = diff[j * diff_stride + i];

      if (visible_w < bsw) {
        for (j = 0; j < bsh; j++)
          for (i = visible_w; i < bsw; i++) diff16[j * bsw + i] = 0;
      }

      if (visible_h < bsh) {
        for (j = visible_h; j < bsh; j++)
          for (i = 0; i < bsw; i++) diff16[j * bsw + i] = 0;
      }
    }
  }

  if (x->tune_metric == AOM_TUNE_DAALA_DIST) {
    d = (int64_t)od_compute_dist_diff(orig, diff16, bsw, bsh, qindex);
  } else if (x->tune_metric == AOM_TUNE_CDEF_DIST) {
    int coeff_shift = AOMMAX(xd->bd - 8, 0);
    DECLARE_ALIGNED(16, uint16_t, dst16[MAX_SB_SQUARE]);

    for (i = 0; i < bsh; i++) {
      for (j = 0; j < bsw; j++) {
        dst16[i * bsw + j] = orig[i * bsw + j] - diff16[i * bsw + j];
      }
    }

    for (i = 0; i < bsh; i += 8) {
      for (j = 0; j < bsw; j += 8) {
        d += cdef_dist_8x8_16bit(&dst16[i * bsw + j], bsw, &orig[i * bsw + j],
                                 bsw, coeff_shift);
      }
    }
    // Don't scale 'd' for HBD since it will be done by caller side for diff
    // input
  } else {
    // Otherwise, MSE by default
    d = aom_sum_squares_2d_i16(diff, diff_stride, visible_w, visible_h);
  }

  return d;
}
#endif  // CONFIG_DIST_8X8

static void get_energy_distribution_fine(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                                         const uint8_t *src, int src_stride,
                                         const uint8_t *dst, int dst_stride,
                                         int need_4th, double *hordist,
                                         double *verdist) {
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  unsigned int esq[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  if (bsize < BLOCK_16X16 || (bsize >= BLOCK_4X16 && bsize <= BLOCK_32X8)) {
    // Special cases: calculate 'esq' values manually, as we don't have 'vf'
    // functions for the 16 (very small) sub-blocks of this block.
    const int w_shift = (bw == 4) ? 0 : (bw == 8) ? 1 : (bw == 16) ? 2 : 3;
    const int h_shift = (bh == 4) ? 0 : (bh == 8) ? 1 : (bh == 16) ? 2 : 3;
    assert(bw <= 32);
    assert(bh <= 32);
    assert(((bw - 1) >> w_shift) + (((bh - 1) >> h_shift) << 2) == 15);
    if (cpi->common.seq_params.use_highbitdepth) {
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
      for (int i = 0; i < bh; ++i)
        for (int j = 0; j < bw; ++j) {
          const int index = (j >> w_shift) + ((i >> h_shift) << 2);
          esq[index] += (src[j + i * src_stride] - dst[j + i * dst_stride]) *
                        (src[j + i * src_stride] - dst[j + i * dst_stride]);
        }
    }
  } else {  // Calculate 'esq' values using 'vf' functions on the 16 sub-blocks.
    const int f_index =
        (bsize < BLOCK_SIZES) ? bsize - BLOCK_16X16 : bsize - BLOCK_8X16;
    assert(f_index >= 0 && f_index < BLOCK_SIZES_ALL);
    const BLOCK_SIZE subsize = (BLOCK_SIZE)f_index;
    assert(block_size_wide[bsize] == 4 * block_size_wide[subsize]);
    assert(block_size_high[bsize] == 4 * block_size_high[subsize]);
    cpi->fn_ptr[subsize].vf(src, src_stride, dst, dst_stride, &esq[0]);
    cpi->fn_ptr[subsize].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[1]);
    cpi->fn_ptr[subsize].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[2]);
    cpi->fn_ptr[subsize].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[3]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[subsize].vf(src, src_stride, dst, dst_stride, &esq[4]);
    cpi->fn_ptr[subsize].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[5]);
    cpi->fn_ptr[subsize].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[6]);
    cpi->fn_ptr[subsize].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[7]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[subsize].vf(src, src_stride, dst, dst_stride, &esq[8]);
    cpi->fn_ptr[subsize].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[9]);
    cpi->fn_ptr[subsize].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[10]);
    cpi->fn_ptr[subsize].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                            dst_stride, &esq[11]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->fn_ptr[subsize].vf(src, src_stride, dst, dst_stride, &esq[12]);
    cpi->fn_ptr[subsize].vf(src + bw / 4, src_stride, dst + bw / 4, dst_stride,
                            &esq[13]);
    cpi->fn_ptr[subsize].vf(src + bw / 2, src_stride, dst + bw / 2, dst_stride,
                            &esq[14]);
    cpi->fn_ptr[subsize].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
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
    if (need_4th) {
      hordist[3] = ((double)esq[3] + esq[7] + esq[11] + esq[15]) * e_recip;
    }
    verdist[0] = ((double)esq[0] + esq[1] + esq[2] + esq[3]) * e_recip;
    verdist[1] = ((double)esq[4] + esq[5] + esq[6] + esq[7]) * e_recip;
    verdist[2] = ((double)esq[8] + esq[9] + esq[10] + esq[11]) * e_recip;
    if (need_4th) {
      verdist[3] = ((double)esq[12] + esq[13] + esq[14] + esq[15]) * e_recip;
    }
  } else {
    hordist[0] = verdist[0] = 0.25;
    hordist[1] = verdist[1] = 0.25;
    hordist[2] = verdist[2] = 0.25;
    if (need_4th) {
      hordist[3] = verdist[3] = 0.25;
    }
  }
}

static int adst_vs_flipadst(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                            const uint8_t *src, int src_stride,
                            const uint8_t *dst, int dst_stride) {
  int prune_bitmask = 0;
  double svm_proj_h = 0, svm_proj_v = 0;
  double hdist[3] = { 0, 0, 0 }, vdist[3] = { 0, 0, 0 };
  get_energy_distribution_fine(cpi, bsize, src, src_stride, dst, dst_stride, 0,
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

static int dct_vs_idtx(const int16_t *diff, int stride, int w, int h) {
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
    const int bw = block_size_wide[bsize];
    const int bh = block_size_high[bsize];
    prune |= dct_vs_idtx(p->src_diff, bw, bw, bh);
  }

  return prune;
}

// Performance drop: 0.3%, Speed improvement: 5%
static int prune_one_for_sby(const AV1_COMP *cpi, BLOCK_SIZE bsize,
                             const MACROBLOCK *x, const MACROBLOCKD *xd) {
  const struct macroblock_plane *const p = &x->plane[0];
  const struct macroblockd_plane *const pd = &xd->plane[0];
  return adst_vs_flipadst(cpi, bsize, p->src.buf, p->src.stride, pd->dst.buf,
                          pd->dst.stride);
}

// 1D Transforms used in inter set, this needs to be changed if
// ext_tx_used_inter is changed
static const int ext_tx_used_inter_1D[EXT_TX_SETS_INTER][TX_TYPES_1D] = {
  { 1, 0, 0, 0 },
  { 1, 1, 1, 1 },
  { 1, 1, 1, 1 },
  { 1, 0, 0, 1 },
};

static void get_energy_distribution_finer(const int16_t *diff, int stride,
                                          int bw, int bh, float *hordist,
                                          float *verdist) {
  // First compute downscaled block energy values (esq); downscale factors
  // are defined by w_shift and h_shift.
  unsigned int esq[256];
  const int w_shift = bw <= 8 ? 0 : 1;
  const int h_shift = bh <= 8 ? 0 : 1;
  const int esq_w = bw >> w_shift;
  const int esq_h = bh >> h_shift;
  const int esq_sz = esq_w * esq_h;
  int i, j;
  memset(esq, 0, esq_sz * sizeof(esq[0]));
  if (w_shift) {
    for (i = 0; i < bh; i++) {
      unsigned int *cur_esq_row = esq + (i >> h_shift) * esq_w;
      const int16_t *cur_diff_row = diff + i * stride;
      for (j = 0; j < bw; j += 2) {
        cur_esq_row[j >> 1] += (cur_diff_row[j] * cur_diff_row[j] +
                                cur_diff_row[j + 1] * cur_diff_row[j + 1]);
      }
    }
  } else {
    for (i = 0; i < bh; i++) {
      unsigned int *cur_esq_row = esq + (i >> h_shift) * esq_w;
      const int16_t *cur_diff_row = diff + i * stride;
      for (j = 0; j < bw; j++) {
        cur_esq_row[j] += cur_diff_row[j] * cur_diff_row[j];
      }
    }
  }

  uint64_t total = 0;
  for (i = 0; i < esq_sz; i++) total += esq[i];

  // Output hordist and verdist arrays are normalized 1D projections of esq
  if (total == 0) {
    float hor_val = 1.0f / esq_w;
    for (j = 0; j < esq_w - 1; j++) hordist[j] = hor_val;
    float ver_val = 1.0f / esq_h;
    for (i = 0; i < esq_h - 1; i++) verdist[i] = ver_val;
    return;
  }

  const float e_recip = 1.0f / (float)total;
  memset(hordist, 0, (esq_w - 1) * sizeof(hordist[0]));
  memset(verdist, 0, (esq_h - 1) * sizeof(verdist[0]));
  const unsigned int *cur_esq_row;
  for (i = 0; i < esq_h - 1; i++) {
    cur_esq_row = esq + i * esq_w;
    for (j = 0; j < esq_w - 1; j++) {
      hordist[j] += (float)cur_esq_row[j];
      verdist[i] += (float)cur_esq_row[j];
    }
    verdist[i] += (float)cur_esq_row[j];
  }
  cur_esq_row = esq + i * esq_w;
  for (j = 0; j < esq_w - 1; j++) hordist[j] += (float)cur_esq_row[j];

  for (j = 0; j < esq_w - 1; j++) hordist[j] *= e_recip;
  for (i = 0; i < esq_h - 1; i++) verdist[i] *= e_recip;
}

// Similar to get_horver_correlation, but also takes into account first
// row/column, when computing horizontal/vertical correlation.
static void get_horver_correlation_full(const int16_t *diff, int stride, int w,
                                        int h, float *hcorr, float *vcorr) {
  const float num_hor = (float)(h * (w - 1));
  const float num_ver = (float)((h - 1) * w);
  int i, j;

  // The following notation is used:
  // x - current pixel
  // y - left neighbor pixel
  // z - top neighbor pixel
  int64_t xy_sum = 0, xz_sum = 0;
  int64_t xhor_sum = 0, xver_sum = 0, y_sum = 0, z_sum = 0;
  int64_t x2hor_sum = 0, x2ver_sum = 0, y2_sum = 0, z2_sum = 0;

  int16_t x, y, z;
  for (j = 1; j < w; ++j) {
    x = diff[j];
    y = diff[j - 1];
    xy_sum += x * y;
    xhor_sum += x;
    y_sum += y;
    x2hor_sum += x * x;
    y2_sum += y * y;
  }
  for (i = 1; i < h; ++i) {
    x = diff[i * stride];
    z = diff[(i - 1) * stride];
    xz_sum += x * z;
    xver_sum += x;
    z_sum += z;
    x2ver_sum += x * x;
    z2_sum += z * z;
    for (j = 1; j < w; ++j) {
      x = diff[i * stride + j];
      y = diff[i * stride + j - 1];
      z = diff[(i - 1) * stride + j];
      xy_sum += x * y;
      xz_sum += x * z;
      xhor_sum += x;
      xver_sum += x;
      y_sum += y;
      z_sum += z;
      x2hor_sum += x * x;
      x2ver_sum += x * x;
      y2_sum += y * y;
      z2_sum += z * z;
    }
  }
  const float xhor_var_n = x2hor_sum - (xhor_sum * xhor_sum) / num_hor;
  const float y_var_n = y2_sum - (y_sum * y_sum) / num_hor;
  const float xy_var_n = xy_sum - (xhor_sum * y_sum) / num_hor;
  const float xver_var_n = x2ver_sum - (xver_sum * xver_sum) / num_ver;
  const float z_var_n = z2_sum - (z_sum * z_sum) / num_ver;
  const float xz_var_n = xz_sum - (xver_sum * z_sum) / num_ver;

  *hcorr = *vcorr = 1;
  if (xhor_var_n > 0 && y_var_n > 0) {
    *hcorr = xy_var_n / sqrtf(xhor_var_n * y_var_n);
    *hcorr = *hcorr < 0 ? 0 : *hcorr;
  }
  if (xver_var_n > 0 && z_var_n > 0) {
    *vcorr = xz_var_n / sqrtf(xver_var_n * z_var_n);
    *vcorr = *vcorr < 0 ? 0 : *vcorr;
  }
}

// Transforms raw scores into a probability distribution across 16 TX types
static void score_2D_transform_pow8(float *scores_2D, float shift) {
  float sum = 0.0f;
  int i;

  for (i = 0; i < 16; i++) {
    float v, v2, v4;
    v = AOMMAX(scores_2D[i] + shift, 0.0f);
    v2 = v * v;
    v4 = v2 * v2;
    scores_2D[i] = v4 * v4;
    sum += scores_2D[i];
  }
  for (i = 0; i < 16; i++) scores_2D[i] /= sum;
}

// These thresholds were calibrated to provide a certain number of TX types
// pruned by the model on average, i.e. selecting a threshold with index i
// will lead to pruning i+1 TX types on average
static const float *prune_2D_adaptive_thresholds[] = {
  // TX_4X4
  (float[]){ 0.02014f, 0.02722f, 0.03430f, 0.04114f, 0.04724f, 0.05212f,
             0.05627f, 0.06018f, 0.06409f, 0.06824f, 0.07312f, 0.07849f,
             0.08606f, 0.09827f },
  // TX_8X8
  (float[]){ 0.00745f, 0.01355f, 0.02039f, 0.02795f, 0.03625f, 0.04407f,
             0.05042f, 0.05579f, 0.06067f, 0.06604f, 0.07239f, 0.08093f,
             0.09363f, 0.11682f },
  // TX_16X16
  (float[]){ 0.01404f, 0.02820f, 0.04211f, 0.05164f, 0.05798f, 0.06335f,
             0.06897f, 0.07629f, 0.08875f, 0.11169f },
  // TX_32X32
  NULL,
  // TX_64X64
  NULL,
  // TX_4X8
  (float[]){ 0.01282f, 0.02087f, 0.02844f, 0.03601f, 0.04285f, 0.04871f,
             0.05359f, 0.05823f, 0.06287f, 0.06799f, 0.07361f, 0.08093f,
             0.09119f, 0.10828f },
  // TX_8X4
  (float[]){ 0.01184f, 0.01941f, 0.02722f, 0.03503f, 0.04187f, 0.04822f,
             0.05359f, 0.05823f, 0.06287f, 0.06799f, 0.07361f, 0.08093f,
             0.09167f, 0.10974f },
  // TX_8X16
  (float[]){ 0.00525f, 0.01135f, 0.01819f, 0.02576f, 0.03357f, 0.04114f,
             0.04773f, 0.05383f, 0.05920f, 0.06506f, 0.07190f, 0.08118f,
             0.09509f, 0.12097f },
  // TX_16X8
  (float[]){ 0.00525f, 0.01160f, 0.01819f, 0.02527f, 0.03308f, 0.04065f,
             0.04773f, 0.05383f, 0.05969f, 0.06531f, 0.07214f, 0.08118f,
             0.09485f, 0.12048f },
  // TX_16X32
  (float[]){ 0.01257f, 0.02576f, 0.03723f, 0.04578f, 0.05212f, 0.05798f,
             0.06506f, 0.07385f, 0.08606f, 0.10925f },
  // TX_32X16
  (float[]){ 0.01233f, 0.02527f, 0.03699f, 0.04602f, 0.05286f, 0.05896f,
             0.06531f, 0.07336f, 0.08582f, 0.11072f },
  // TX_32X64
  NULL,
  // TX_64X32
  NULL,
  // TX_4X16
  NULL,
  // TX_16X4
  NULL,
  // TX_8X32
  NULL,
  // TX_32X8
  NULL,
  // TX_16X64
  NULL,
  // TX_64X16
  NULL,
};

static uint16_t prune_tx_2D(MACROBLOCK *x, BLOCK_SIZE bsize, TX_SIZE tx_size,
                            int blk_row, int blk_col, TxSetType tx_set_type,
                            TX_TYPE_PRUNE_MODE prune_mode) {
  static const int tx_type_table_2D[16] = {
    DCT_DCT,      DCT_ADST,      DCT_FLIPADST,      V_DCT,
    ADST_DCT,     ADST_ADST,     ADST_FLIPADST,     V_ADST,
    FLIPADST_DCT, FLIPADST_ADST, FLIPADST_FLIPADST, V_FLIPADST,
    H_DCT,        H_ADST,        H_FLIPADST,        IDTX
  };
  if (tx_set_type != EXT_TX_SET_ALL16 &&
      tx_set_type != EXT_TX_SET_DTT9_IDTX_1DDCT)
    return 0;
  const NN_CONFIG *nn_config_hor = av1_tx_type_nnconfig_map_hor[tx_size];
  const NN_CONFIG *nn_config_ver = av1_tx_type_nnconfig_map_ver[tx_size];
  if (!nn_config_hor || !nn_config_ver) return 0;  // Model not established yet.

  aom_clear_system_state();
  float hfeatures[16], vfeatures[16];
  float hscores[4], vscores[4];
  float scores_2D[16];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  const int hfeatures_num = bw <= 8 ? bw : bw / 2;
  const int vfeatures_num = bh <= 8 ? bh : bh / 2;
  assert(hfeatures_num <= 16);
  assert(vfeatures_num <= 16);

  const struct macroblock_plane *const p = &x->plane[0];
  const int diff_stride = block_size_wide[bsize];
  const int16_t *diff = p->src_diff + 4 * blk_row * diff_stride + 4 * blk_col;
  get_energy_distribution_finer(diff, diff_stride, bw, bh, hfeatures,
                                vfeatures);
  get_horver_correlation_full(diff, diff_stride, bw, bh,
                              &hfeatures[hfeatures_num - 1],
                              &vfeatures[vfeatures_num - 1]);
  av1_nn_predict(hfeatures, nn_config_hor, hscores);
  av1_nn_predict(vfeatures, nn_config_ver, vscores);

  float score_2D_average = 0.0f;
  for (int i = 0; i < 4; i++) {
    float *cur_scores_2D = scores_2D + i * 4;
    cur_scores_2D[0] = vscores[i] * hscores[0];
    cur_scores_2D[1] = vscores[i] * hscores[1];
    cur_scores_2D[2] = vscores[i] * hscores[2];
    cur_scores_2D[3] = vscores[i] * hscores[3];
    score_2D_average += cur_scores_2D[0] + cur_scores_2D[1] + cur_scores_2D[2] +
                        cur_scores_2D[3];
  }
  score_2D_average /= 16;
  score_2D_transform_pow8(scores_2D, (20 - score_2D_average));

  // Always keep the TX type with the highest score, prune all others with
  // score below score_thresh.
  int max_score_i = 0;
  float max_score = 0.0f;
  for (int i = 0; i < 16; i++) {
    if (scores_2D[i] > max_score &&
        av1_ext_tx_used[tx_set_type][tx_type_table_2D[i]]) {
      max_score = scores_2D[i];
      max_score_i = i;
    }
  }

  int pruning_aggressiveness = 0;
  if (prune_mode == PRUNE_2D_ACCURATE) {
    if (tx_set_type == EXT_TX_SET_ALL16)
      pruning_aggressiveness = 6;
    else if (tx_set_type == EXT_TX_SET_DTT9_IDTX_1DDCT)
      pruning_aggressiveness = 4;
  } else if (prune_mode == PRUNE_2D_FAST) {
    if (tx_set_type == EXT_TX_SET_ALL16)
      pruning_aggressiveness = 10;
    else if (tx_set_type == EXT_TX_SET_DTT9_IDTX_1DDCT)
      pruning_aggressiveness = 7;
  }
  const float score_thresh =
      prune_2D_adaptive_thresholds[tx_size][pruning_aggressiveness - 1];

  uint16_t prune_bitmask = 0;
  for (int i = 0; i < 16; i++) {
    if (scores_2D[i] < score_thresh && i != max_score_i)
      prune_bitmask |= (1 << tx_type_table_2D[i]);
  }
  return prune_bitmask;
}

// ((prune >> vtx_tab[tx_type]) & 1)
static const uint16_t prune_v_mask[] = {
  0x0000, 0x0425, 0x108a, 0x14af, 0x4150, 0x4575, 0x51da, 0x55ff,
  0xaa00, 0xae25, 0xba8a, 0xbeaf, 0xeb50, 0xef75, 0xfbda, 0xffff,
};

// ((prune >> (htx_tab[tx_type] + 8)) & 1)
static const uint16_t prune_h_mask[] = {
  0x0000, 0x0813, 0x210c, 0x291f, 0x80e0, 0x88f3, 0xa1ec, 0xa9ff,
  0x5600, 0x5e13, 0x770c, 0x7f1f, 0xd6e0, 0xdef3, 0xf7ec, 0xffff,
};

static INLINE uint16_t gen_tx_search_prune_mask(int tx_search_prune) {
  uint8_t prune_v = tx_search_prune & 0x0F;
  uint8_t prune_h = (tx_search_prune >> 8) & 0x0F;
  return (prune_v_mask[prune_v] & prune_h_mask[prune_h]);
}

static void prune_tx(const AV1_COMP *cpi, BLOCK_SIZE bsize, MACROBLOCK *x,
                     const MACROBLOCKD *const xd, int tx_set_type) {
  x->tx_search_prune[tx_set_type] = 0;
  x->tx_split_prune_flag = 0;
  const MB_MODE_INFO *mbmi = xd->mi[0];
  if (!is_inter_block(mbmi) || cpi->sf.tx_type_search.prune_mode == NO_PRUNE ||
      x->use_default_inter_tx_type || xd->lossless[mbmi->segment_id] ||
      x->cb_partition_scan)
    return;
  int tx_set = ext_tx_set_index[1][tx_set_type];
  assert(tx_set >= 0);
  const int *tx_set_1D = ext_tx_used_inter_1D[tx_set];
  int prune = 0;
  switch (cpi->sf.tx_type_search.prune_mode) {
    case NO_PRUNE: return;
    case PRUNE_ONE:
      if (!(tx_set_1D[FLIPADST_1D] & tx_set_1D[ADST_1D])) return;
      prune = prune_one_for_sby(cpi, bsize, x, xd);
      x->tx_search_prune[tx_set_type] = gen_tx_search_prune_mask(prune);
      break;
    case PRUNE_TWO:
      if (!(tx_set_1D[FLIPADST_1D] & tx_set_1D[ADST_1D])) {
        if (!(tx_set_1D[DCT_1D] & tx_set_1D[IDTX_1D])) return;
        prune = prune_two_for_sby(cpi, bsize, x, xd, 0, 1);
      } else if (!(tx_set_1D[DCT_1D] & tx_set_1D[IDTX_1D])) {
        prune = prune_two_for_sby(cpi, bsize, x, xd, 1, 0);
      } else {
        prune = prune_two_for_sby(cpi, bsize, x, xd, 1, 1);
      }
      x->tx_search_prune[tx_set_type] = gen_tx_search_prune_mask(prune);
      break;
    case PRUNE_2D_ACCURATE:
    case PRUNE_2D_FAST: break;
    default: assert(0);
  }
}

static void model_rd_from_sse(const AV1_COMP *const cpi,
                              const MACROBLOCKD *const xd, BLOCK_SIZE bsize,
                              int plane, int64_t sse, int *rate,
                              int64_t *dist) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int dequant_shift =
      (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd - 5 : 3;

  // Fast approximate the modelling function.
  if (cpi->sf.simple_model_rd_from_var) {
    const int64_t square_error = sse;
    int quantizer = (pd->dequant_Q3[1] >> dequant_shift);
    if (quantizer < 120)
      *rate = (int)((square_error * (280 - quantizer)) >>
                    (16 - AV1_PROB_COST_SHIFT));
    else
      *rate = 0;
    *dist = (square_error * quantizer) >> 8;
  } else {
    av1_model_rd_from_var_lapndz(sse, num_pels_log2_lookup[bsize],
                                 pd->dequant_Q3[1] >> dequant_shift, rate,
                                 dist);
  }
  *dist <<= 4;
}

#if CONFIG_COLLECT_INTER_MODE_RD_STATS
static int64_t get_sse(const AV1_COMP *cpi, const MACROBLOCK *x) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = xd->mi[0];
  int64_t total_sse = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblock_plane *const p = &x->plane[plane];
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE bs = get_plane_block_size(mbmi->sb_type, pd->subsampling_x,
                                               pd->subsampling_y);
    unsigned int sse;

    if (x->skip_chroma_rd && plane) continue;

    cpi->fn_ptr[bs].vf(p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride,
                       &sse);
    total_sse += sse;
  }
  total_sse <<= 4;
  return total_sse;
}
#endif

static void model_rd_for_sb(const AV1_COMP *const cpi, BLOCK_SIZE bsize,
                            MACROBLOCK *x, MACROBLOCKD *xd, int plane_from,
                            int plane_to, int *out_rate_sum,
                            int64_t *out_dist_sum, int *skip_txfm_sb,
                            int64_t *skip_sse_sb, int *plane_rate,
                            int64_t *plane_sse, int64_t *plane_dist) {
  // Note our transform coeffs are 8 times an orthogonal transform.
  // Hence quantizer step is also 8 times. To get effective quantizer
  // we need to divide by 8 before sending to modeling function.
  int plane;
  const int ref = xd->mi[0]->ref_frame[0];

  int64_t rate_sum = 0;
  int64_t dist_sum = 0;
  int64_t total_sse = 0;

  x->pred_sse[ref] = 0;

  for (plane = plane_from; plane <= plane_to; ++plane) {
    struct macroblock_plane *const p = &x->plane[plane];
    struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];
    int64_t sse;
    int rate;
    int64_t dist;

    if (x->skip_chroma_rd && plane) continue;

    // TODO(geza): Write direct sse functions that do not compute
    // variance as well.
    sse = aom_sum_squares_2d_i16(p->src_diff, bw, bw, bh);
    sse = ROUND_POWER_OF_TWO(sse, (xd->bd - 8) * 2);

    if (plane == 0) x->pred_sse[ref] = (unsigned int)AOMMIN(sse, UINT_MAX);

    total_sse += sse;

    model_rd_from_sse(cpi, xd, plane_bsize, plane, sse, &rate, &dist);

    rate_sum += rate;
    dist_sum += dist;
    if (plane_rate) plane_rate[plane] = rate;
    if (plane_sse) plane_sse[plane] = sse;
    if (plane_dist) plane_dist[plane] = dist;
  }

  if (skip_txfm_sb) *skip_txfm_sb = total_sse == 0;
  if (skip_sse_sb) *skip_sse_sb = total_sse << 4;
  *out_rate_sum = (int)rate_sum;
  *out_dist_sum = dist_sum;
}

static void check_block_skip(const AV1_COMP *const cpi, BLOCK_SIZE bsize,
                             MACROBLOCK *x, MACROBLOCKD *xd, int plane_from,
                             int plane_to, int *skip_txfm_sb) {
  *skip_txfm_sb = 1;
  for (int plane = plane_from; plane <= plane_to; ++plane) {
    struct macroblock_plane *const p = &x->plane[plane];
    struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE bs =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    unsigned int sse;

    if (x->skip_chroma_rd && plane) continue;

    // Since fast HBD variance functions scale down sse by 4 bit, we first use
    // fast vf implementation to rule out blocks with non-zero scaled sse. Then,
    // only if the source is HBD and the scaled sse is 0, accurate sse
    // computation is applied to determine if the sse is really 0. This step is
    // necessary for HBD lossless coding.
    cpi->fn_ptr[bs].vf(p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride,
                       &sse);
    if (sse) {
      *skip_txfm_sb = 0;
      return;
    } else if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint64_t sse64 = aom_highbd_sse_odd_size(
          p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride,
          block_size_wide[bs], block_size_high[bs]);

      if (sse64) {
        *skip_txfm_sb = 0;
        return;
      }
    }
  }
  return;
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

// Get transform block visible dimensions cropped to the MI units.
static void get_txb_dimensions(const MACROBLOCKD *xd, int plane,
                               BLOCK_SIZE plane_bsize, int blk_row, int blk_col,
                               BLOCK_SIZE tx_bsize, int *width, int *height,
                               int *visible_width, int *visible_height) {
  assert(tx_bsize <= plane_bsize);
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

// Compute the pixel domain distortion from src and dst on all visible 4x4s in
// the
// transform block.
static unsigned pixel_dist(const AV1_COMP *const cpi, const MACROBLOCK *x,
                           int plane, const uint8_t *src, const int src_stride,
                           const uint8_t *dst, const int dst_stride,
                           int blk_row, int blk_col,
                           const BLOCK_SIZE plane_bsize,
                           const BLOCK_SIZE tx_bsize) {
  int txb_rows, txb_cols, visible_rows, visible_cols;
  const MACROBLOCKD *xd = &x->e_mbd;

  get_txb_dimensions(xd, plane, plane_bsize, blk_row, blk_col, tx_bsize,
                     &txb_cols, &txb_rows, &visible_cols, &visible_rows);
  assert(visible_rows > 0);
  assert(visible_cols > 0);

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && plane == 0 && txb_cols >= 8 && txb_rows >= 8)
    return (unsigned)av1_dist_8x8(cpi, x, src, src_stride, dst, dst_stride,
                                  tx_bsize, txb_cols, txb_rows, visible_cols,
                                  visible_rows, x->qindex);
#endif  // CONFIG_DIST_8X8

  unsigned sse = pixel_dist_visible_only(cpi, x, src, src_stride, dst,
                                         dst_stride, tx_bsize, txb_rows,
                                         txb_cols, visible_rows, visible_cols);

  return sse;
}

// Compute the pixel domain distortion from diff on all visible 4x4s in the
// transform block.
static INLINE int64_t pixel_diff_dist(const MACROBLOCK *x, int plane,
                                      int blk_row, int blk_col,
                                      const BLOCK_SIZE plane_bsize,
                                      const BLOCK_SIZE tx_bsize,
                                      int force_sse) {
  int visible_rows, visible_cols;
  const MACROBLOCKD *xd = &x->e_mbd;
  get_txb_dimensions(xd, plane, plane_bsize, blk_row, blk_col, tx_bsize, NULL,
                     NULL, &visible_cols, &visible_rows);
  const int diff_stride = block_size_wide[plane_bsize];
  const int16_t *diff = x->plane[plane].src_diff;
#if CONFIG_DIST_8X8
  int txb_height = block_size_high[tx_bsize];
  int txb_width = block_size_wide[tx_bsize];
  if (!force_sse && x->using_dist_8x8 && plane == 0 && txb_width >= 8 &&
      txb_height >= 8) {
    const int src_stride = x->plane[plane].src.stride;
    const int src_idx = (blk_row * src_stride + blk_col)
                        << tx_size_wide_log2[0];
    const int diff_idx = (blk_row * diff_stride + blk_col)
                         << tx_size_wide_log2[0];
    const uint8_t *src = &x->plane[plane].src.buf[src_idx];
    return dist_8x8_diff(x, src, src_stride, diff + diff_idx, diff_stride,
                         txb_width, txb_height, visible_cols, visible_rows,
                         x->qindex);
  }
#endif
  diff += ((blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]);
  return aom_sum_squares_2d_i16(diff, diff_stride, visible_cols, visible_rows);
}

int av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                     int *val_count) {
  const int max_pix_val = 1 << 8;
  memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int this_val = src[r * stride + c];
      assert(this_val < max_pix_val);
      ++val_count[this_val];
    }
  }
  int n = 0;
  for (int i = 0; i < max_pix_val; ++i) {
    if (val_count[i]) ++n;
  }
  return n;
}

int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth, int *val_count) {
  assert(bit_depth <= 12);
  const int max_pix_val = 1 << bit_depth;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int this_val = src[r * stride + c];
      assert(this_val < max_pix_val);
      if (this_val >= max_pix_val) return 0;
      ++val_count[this_val];
    }
  }
  int n = 0;
  for (int i = 0; i < max_pix_val; ++i) {
    if (val_count[i]) ++n;
  }
  return n;
}

static void inverse_transform_block_facade(MACROBLOCKD *xd, int plane,
                                           int block, int blk_row, int blk_col,
                                           int eob, int reduced_tx_set) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
  const TX_TYPE tx_type = av1_get_tx_type(plane_type, xd, blk_row, blk_col,
                                          tx_size, reduced_tx_set);
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  av1_inverse_transform_block(xd, dqcoeff, plane, tx_type, tx_size, dst,
                              dst_stride, eob, reduced_tx_set);
}

static int find_tx_size_rd_info(TXB_RD_RECORD *cur_record, const uint32_t hash);

static uint32_t get_intra_txb_hash(MACROBLOCK *x, int plane, int blk_row,
                                   int blk_col, BLOCK_SIZE plane_bsize,
                                   TX_SIZE tx_size) {
  int16_t tmp_data[64 * 64];
  const int diff_stride = block_size_wide[plane_bsize];
  const int16_t *diff = x->plane[plane].src_diff;
  const int16_t *cur_diff_row = diff + 4 * blk_row * diff_stride + 4 * blk_col;
  const int txb_w = tx_size_wide[tx_size];
  const int txb_h = tx_size_high[tx_size];
  uint8_t *hash_data = (uint8_t *)cur_diff_row;
  if (txb_w != diff_stride) {
    int16_t *cur_hash_row = tmp_data;
    for (int i = 0; i < txb_h; i++) {
      memcpy(cur_hash_row, cur_diff_row, sizeof(*diff) * txb_w);
      cur_hash_row += txb_w;
      cur_diff_row += diff_stride;
    }
    hash_data = (uint8_t *)tmp_data;
  }
  CRC32C *crc = &x->mb_rd_record.crc_calculator;
  const uint32_t hash = av1_get_crc32c_value(crc, hash_data, 2 * txb_w * txb_h);
  return (hash << 5) + tx_size;
}

static INLINE void dist_block_tx_domain(MACROBLOCK *x, int plane, int block,
                                        TX_SIZE tx_size, int64_t *out_dist,
                                        int64_t *out_sse) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  // Transform domain distortion computation is more efficient as it does
  // not involve an inverse transform, but it is less accurate.
  const int buffer_length = av1_get_max_eob(tx_size);
  int64_t this_sse;
  // TX-domain results need to shift down to Q2/D10 to match pixel
  // domain distortion values which are in Q2^2
  int shift = (MAX_TX_SCALE - av1_get_tx_scale(tx_size)) * 2;
  tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    *out_dist = av1_highbd_block_error(coeff, dqcoeff, buffer_length, &this_sse,
                                       xd->bd);
  else
    *out_dist = av1_block_error(coeff, dqcoeff, buffer_length, &this_sse);

  *out_dist = RIGHT_SIGNED_SHIFT(*out_dist, shift);
  *out_sse = RIGHT_SIGNED_SHIFT(this_sse, shift);
}

static INLINE int64_t dist_block_px_domain(const AV1_COMP *cpi, MACROBLOCK *x,
                                           int plane, BLOCK_SIZE plane_bsize,
                                           int block, int blk_row, int blk_col,
                                           TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const uint16_t eob = p->eobs[block];
  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  const int bsw = block_size_wide[tx_bsize];
  const int bsh = block_size_high[tx_bsize];
  const int src_stride = x->plane[plane].src.stride;
  const int dst_stride = xd->plane[plane].dst.stride;
  // Scale the transform block index to pixel unit.
  const int src_idx = (blk_row * src_stride + blk_col) << tx_size_wide_log2[0];
  const int dst_idx = (blk_row * dst_stride + blk_col) << tx_size_wide_log2[0];
  const uint8_t *src = &x->plane[plane].src.buf[src_idx];
  const uint8_t *dst = &xd->plane[plane].dst.buf[dst_idx];
  const tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);

  assert(cpi != NULL);
  assert(tx_size_wide_log2[0] == tx_size_high_log2[0]);

  uint8_t *recon;
  DECLARE_ALIGNED(16, uint16_t, recon16[MAX_TX_SQUARE]);

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    recon = CONVERT_TO_BYTEPTR(recon16);
    av1_highbd_convolve_2d_copy_sr(CONVERT_TO_SHORTPTR(dst), dst_stride,
                                   CONVERT_TO_SHORTPTR(recon), MAX_TX_SIZE, bsw,
                                   bsh, NULL, NULL, 0, 0, NULL, xd->bd);
  } else {
    recon = (uint8_t *)recon16;
    av1_convolve_2d_copy_sr(dst, dst_stride, recon, MAX_TX_SIZE, bsw, bsh, NULL,
                            NULL, 0, 0, NULL);
  }

  const PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type = av1_get_tx_type(plane_type, xd, blk_row, blk_col, tx_size,
                                    cpi->common.reduced_tx_set_used);
  av1_inverse_transform_block(xd, dqcoeff, plane, tx_type, tx_size, recon,
                              MAX_TX_SIZE, eob,
                              cpi->common.reduced_tx_set_used);
#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && plane == 0 && (bsw < 8 || bsh < 8)) {
    // Save decoded pixels for inter block in pd->pred to avoid
    // block_8x8_rd_txfm_daala_dist() need to produce them
    // by calling av1_inverse_transform_block() again.
    const int pred_stride = block_size_wide[plane_bsize];
    const int pred_idx = (blk_row * pred_stride + blk_col)
                         << tx_size_wide_log2[0];
    int16_t *pred = &x->pred_luma[pred_idx];
    int i, j;

    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          pred[j * pred_stride + i] =
              CONVERT_TO_SHORTPTR(recon)[j * MAX_TX_SIZE + i];
    } else {
      for (j = 0; j < bsh; j++)
        for (i = 0; i < bsw; i++)
          pred[j * pred_stride + i] = recon[j * MAX_TX_SIZE + i];
    }
  }
#endif  // CONFIG_DIST_8X8
  return 16 * pixel_dist(cpi, x, plane, src, src_stride, recon, MAX_TX_SIZE,
                         blk_row, blk_col, plane_bsize, tx_bsize);
}

static double get_mean(const int16_t *diff, int stride, int w, int h) {
  double sum = 0.0;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      sum += diff[j * stride + i];
    }
  }
  assert(w > 0 && h > 0);
  return sum / (w * h);
}

static double get_sse_norm(const int16_t *diff, int stride, int w, int h) {
  double sum = 0.0;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      const int err = diff[j * stride + i];
      sum += err * err;
    }
  }
  assert(w > 0 && h > 0);
  return sum / (w * h);
}

static double get_sad_norm(const int16_t *diff, int stride, int w, int h) {
  double sum = 0.0;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      sum += abs(diff[j * stride + i]);
    }
  }
  assert(w > 0 && h > 0);
  return sum / (w * h);
}

static void get_2x2_normalized_sses_and_sads(
    const AV1_COMP *const cpi, BLOCK_SIZE tx_bsize, const uint8_t *const src,
    int src_stride, const uint8_t *const dst, int dst_stride,
    const int16_t *const src_diff, int diff_stride, double *const sse_norm_arr,
    double *const sad_norm_arr) {
  const BLOCK_SIZE tx_bsize_half =
      get_partition_subsize(tx_bsize, PARTITION_SPLIT);
  if (tx_bsize_half == BLOCK_INVALID) {  // manually calculate stats
    const int half_width = block_size_wide[tx_bsize] / 2;
    const int half_height = block_size_high[tx_bsize] / 2;
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 2; ++col) {
        const int16_t *const this_src_diff =
            src_diff + row * half_height * diff_stride + col * half_width;
        if (sse_norm_arr) {
          sse_norm_arr[row * 2 + col] =
              get_sse_norm(this_src_diff, diff_stride, half_width, half_height);
        }
        if (sad_norm_arr) {
          sad_norm_arr[row * 2 + col] =
              get_sad_norm(this_src_diff, diff_stride, half_width, half_height);
        }
      }
    }
  } else {  // use function pointers to calculate stats
    const int half_width = block_size_wide[tx_bsize_half];
    const int half_height = block_size_high[tx_bsize_half];
    const int num_samples_half = half_width * half_height;
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 2; ++col) {
        const uint8_t *const this_src =
            src + row * half_height * src_stride + col * half_width;
        const uint8_t *const this_dst =
            dst + row * half_height * dst_stride + col * half_width;

        if (sse_norm_arr) {
          unsigned int this_sse;
          cpi->fn_ptr[tx_bsize_half].vf(this_src, src_stride, this_dst,
                                        dst_stride, &this_sse);
          sse_norm_arr[row * 2 + col] = (double)this_sse / num_samples_half;
        }

        if (sad_norm_arr) {
          const unsigned int this_sad = cpi->fn_ptr[tx_bsize_half].sdf(
              this_src, src_stride, this_dst, dst_stride);
          sad_norm_arr[row * 2 + col] = (double)this_sad / num_samples_half;
        }
      }
    }
  }
}

#if CONFIG_COLLECT_RD_STATS
  // NOTE: CONFIG_COLLECT_RD_STATS has 3 possible values
  // 0: Do not collect any RD stats
  // 1: Collect RD stats for transform units
  // 2: Collect RD stats for partition units

#if CONFIG_COLLECT_RD_STATS == 1
static void PrintTransformUnitStats(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    const RD_STATS *const rd_stats, int blk_row,
                                    int blk_col, BLOCK_SIZE plane_bsize,
                                    TX_SIZE tx_size, TX_TYPE tx_type,
                                    int64_t rd) {
  if (rd_stats->rate == INT_MAX || rd_stats->dist == INT64_MAX) return;

  // Generate small sample to restrict output size.
  static unsigned int seed = 21743;
  if (lcg_rand16(&seed) % 100 > 0) return;

  const char output_file[] = "tu_stats.txt";
  FILE *fout = fopen(output_file, "a");
  if (!fout) return;

  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  const MACROBLOCKD *const xd = &x->e_mbd;
  const int plane = 0;
  struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int txw = tx_size_wide[tx_size];
  const int txh = tx_size_high[tx_size];
  const int dequant_shift =
      (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd - 5 : 3;
  const int q_step = pd->dequant_Q3[1] >> dequant_shift;
  const double num_samples = txw * txh;

  const double rate_norm = (double)rd_stats->rate / num_samples;
  const double dist_norm = (double)rd_stats->dist / num_samples;

  fprintf(fout, "%g %g", rate_norm, dist_norm);

  const int src_stride = p->src.stride;
  const uint8_t *const src =
      &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
  const int dst_stride = pd->dst.stride;
  const uint8_t *const dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  unsigned int sse;
  cpi->fn_ptr[tx_bsize].vf(src, src_stride, dst, dst_stride, &sse);
  const double sse_norm = (double)sse / num_samples;

  const unsigned int sad =
      cpi->fn_ptr[tx_bsize].sdf(src, src_stride, dst, dst_stride);
  const double sad_norm = (double)sad / num_samples;

  fprintf(fout, " %g %g", sse_norm, sad_norm);

  const int diff_stride = block_size_wide[plane_bsize];
  const int16_t *const src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];

  double sse_norm_arr[4], sad_norm_arr[4];
  get_2x2_normalized_sses_and_sads(cpi, tx_bsize, src, src_stride, dst,
                                   dst_stride, src_diff, diff_stride,
                                   sse_norm_arr, sad_norm_arr);
  for (int i = 0; i < 4; ++i) {
    fprintf(fout, " %g", sse_norm_arr[i]);
  }
  for (int i = 0; i < 4; ++i) {
    fprintf(fout, " %g", sad_norm_arr[i]);
  }

  const TX_TYPE_1D tx_type_1d_row = htx_tab[tx_type];
  const TX_TYPE_1D tx_type_1d_col = vtx_tab[tx_type];

  fprintf(fout, " %d %d %d %d %d", q_step, tx_size_wide[tx_size],
          tx_size_high[tx_size], tx_type_1d_row, tx_type_1d_col);

  int model_rate;
  int64_t model_dist;
  model_rd_from_sse(cpi, xd, tx_bsize, plane, sse, &model_rate, &model_dist);
  const double model_rate_norm = (double)model_rate / num_samples;
  const double model_dist_norm = (double)model_dist / num_samples;
  fprintf(fout, " %g %g", model_rate_norm, model_dist_norm);

  const double mean = get_mean(src_diff, diff_stride, txw, txh);
  double hor_corr, vert_corr;
  get_horver_correlation(src_diff, diff_stride, txw, txh, &hor_corr,
                         &vert_corr);
  fprintf(fout, " %g %g %g", mean, hor_corr, vert_corr);

  double hdist[4] = { 0 }, vdist[4] = { 0 };
  get_energy_distribution_fine(cpi, tx_bsize, src, src_stride, dst, dst_stride,
                               1, hdist, vdist);
  fprintf(fout, " %g %g %g %g %g %g %g %g", hdist[0], hdist[1], hdist[2],
          hdist[3], vdist[0], vdist[1], vdist[2], vdist[3]);

  fprintf(fout, " %d %" PRId64, x->rdmult, rd);

  fprintf(fout, "\n");
  fclose(fout);
}
#endif  // CONFIG_COLLECT_RD_STATS == 1

#if CONFIG_COLLECT_RD_STATS == 2
static void PrintPredictionUnitStats(const AV1_COMP *const cpi, MACROBLOCK *x,
                                     const RD_STATS *const rd_stats,
                                     BLOCK_SIZE plane_bsize) {
  if (rd_stats->invalid_rate) return;
  if (rd_stats->rate == INT_MAX || rd_stats->dist == INT64_MAX) return;

  // Generate small sample to restrict output size.
  static unsigned int seed = 95014;
  if (lcg_rand16(&seed) % 100 > 0) return;

  const char output_file[] = "pu_stats.txt";
  FILE *fout = fopen(output_file, "a");
  if (!fout) return;

  const MACROBLOCKD *const xd = &x->e_mbd;
  const int plane = 0;
  struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int diff_stride = block_size_wide[plane_bsize];
  int bw, bh;
  get_txb_dimensions(xd, plane, plane_bsize, 0, 0, plane_bsize, NULL, NULL, &bw,
                     &bh);
  const int num_samples = bw * bh;
  const int dequant_shift =
      (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd - 5 : 3;
  const int q_step = pd->dequant_Q3[1] >> dequant_shift;

  const double rate_norm = (double)rd_stats->rate / num_samples;
  const double dist_norm = (double)rd_stats->dist / num_samples;

  fprintf(fout, "%g %g", rate_norm, dist_norm);

  const int src_stride = p->src.stride;
  const uint8_t *const src = p->src.buf;
  const int dst_stride = pd->dst.stride;
  const uint8_t *const dst = pd->dst.buf;
  const int16_t *const src_diff = p->src_diff;
  const int shift = (xd->bd - 8);

  int64_t sse = aom_sum_squares_2d_i16(src_diff, diff_stride, bw, bh);
  sse = ROUND_POWER_OF_TWO(sse, shift * 2);
  const double sse_norm = (double)sse / num_samples;

  const unsigned int sad =
      cpi->fn_ptr[plane_bsize].sdf(src, src_stride, dst, dst_stride);
  const double sad_norm =
      (double)sad / (1 << num_pels_log2_lookup[plane_bsize]);

  fprintf(fout, " %g %g", sse_norm, sad_norm);

  double sse_norm_arr[4], sad_norm_arr[4];
  get_2x2_normalized_sses_and_sads(cpi, plane_bsize, src, src_stride, dst,
                                   dst_stride, src_diff, diff_stride,
                                   sse_norm_arr, sad_norm_arr);
  if (shift) {
    for (int k = 0; k < 4; ++k) sse_norm_arr[k] /= (1 << (2 * shift));
    for (int k = 0; k < 4; ++k) sad_norm_arr[k] /= (1 << shift);
  }
  for (int i = 0; i < 4; ++i) {
    fprintf(fout, " %g", sse_norm_arr[i]);
  }
  for (int i = 0; i < 4; ++i) {
    fprintf(fout, " %g", sad_norm_arr[i]);
  }

  fprintf(fout, " %d %d %d", q_step, bw, bh);

  int model_rate;
  int64_t model_dist;
  model_rd_from_sse(cpi, xd, plane_bsize, plane, sse, &model_rate, &model_dist);
  const double model_rate_norm = (double)model_rate / num_samples;
  const double model_dist_norm = (double)model_dist / num_samples;
  fprintf(fout, " %g %g", model_rate_norm, model_dist_norm);

  double mean = get_mean(src_diff, diff_stride, bw, bh);
  mean /= (1 << shift);
  double hor_corr, vert_corr;
  get_horver_correlation(src_diff, diff_stride, bw, bh, &hor_corr, &vert_corr);
  fprintf(fout, " %g %g %g", mean, hor_corr, vert_corr);

  double hdist[4] = { 0 }, vdist[4] = { 0 };
  get_energy_distribution_fine(cpi, plane_bsize, src, src_stride, dst,
                               dst_stride, 1, hdist, vdist);
  fprintf(fout, " %g %g %g %g %g %g %g %g", hdist[0], hdist[1], hdist[2],
          hdist[3], vdist[0], vdist[1], vdist[2], vdist[3]);

  fprintf(fout, "\n");
  fclose(fout);
}
#endif  // CONFIG_COLLECT_RD_STATS == 2
#endif  // CONFIG_COLLECT_RD_STATS

static void model_rd_with_dnn(const AV1_COMP *const cpi, MACROBLOCK *const x,
                              BLOCK_SIZE plane_bsize, int plane, int64_t *rsse,
                              int *rate, int64_t *dist) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int log_numpels = num_pels_log2_lookup[plane_bsize];

  const struct macroblock_plane *const p = &x->plane[plane];
  int bw, bh;
  const int diff_stride = block_size_wide[plane_bsize];
  get_txb_dimensions(xd, plane, plane_bsize, 0, 0, plane_bsize, NULL, NULL, &bw,
                     &bh);
  const int num_samples = bw * bh;
  const int dequant_shift =
      (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? xd->bd - 5 : 3;
  const int q_step = pd->dequant_Q3[1] >> dequant_shift;

  const int src_stride = p->src.stride;
  const uint8_t *const src = p->src.buf;
  const int dst_stride = pd->dst.stride;
  const uint8_t *const dst = pd->dst.buf;
  const int16_t *const src_diff = p->src_diff;
  const int shift = (xd->bd - 8);
  int64_t sse = aom_sum_squares_2d_i16(p->src_diff, diff_stride, bw, bh);
  sse = ROUND_POWER_OF_TWO(sse, shift * 2);
  const double sse_norm = (double)sse / num_samples;

  if (sse == 0) {
    if (rate) *rate = 0;
    if (dist) *dist = 0;
    if (rsse) *rsse = sse;
    return;
  }
  if (plane) {
    int model_rate;
    int64_t model_dist;
    model_rd_from_sse(cpi, xd, plane_bsize, plane, sse, &model_rate,
                      &model_dist);
    if (rate) *rate = model_rate;
    if (dist) *dist = model_dist;
    if (rsse) *rsse = sse;
    return;
  }

  double sse_norm_arr[4];
  get_2x2_normalized_sses_and_sads(cpi, plane_bsize, src, src_stride, dst,
                                   dst_stride, src_diff, diff_stride,
                                   sse_norm_arr, NULL);
  double mean = get_mean(src_diff, bw, bw, bh);
  if (shift) {
    for (int k = 0; k < 4; ++k) sse_norm_arr[k] /= (1 << (2 * shift));
    mean /= (1 << shift);
  }
  const double variance = sse_norm - mean * mean;
  assert(variance >= 0.0);
  const double q_sqr = (double)(q_step * q_step);
  const double q_sqr_by_sse_norm = q_sqr / (sse_norm + 1.0);
  double hor_corr, vert_corr;
  get_horver_correlation(src_diff, diff_stride, bw, bh, &hor_corr, &vert_corr);

  float features[11];
  features[0] = (float)hor_corr;
  features[1] = (float)log_numpels;
  features[2] = (float)q_sqr;
  features[3] = (float)q_sqr_by_sse_norm;
  features[4] = (float)sse_norm_arr[0];
  features[5] = (float)sse_norm_arr[1];
  features[6] = (float)sse_norm_arr[2];
  features[7] = (float)sse_norm_arr[3];
  features[8] = (float)sse_norm;
  features[9] = (float)variance;
  features[10] = (float)vert_corr;

  float rate_f, dist_by_sse_norm_f;
  av1_nn_predict(features, &av1_pustats_dist_nnconfig, &dist_by_sse_norm_f);
  av1_nn_predict(features, &av1_pustats_rate_nnconfig, &rate_f);
  const float dist_f = (float)((double)dist_by_sse_norm_f * (1.0 + sse_norm));
  int rate_i = (int)(AOMMAX(0.0, rate_f * num_samples) + 0.5);
  int64_t dist_i = (int64_t)(AOMMAX(0.0, dist_f * num_samples) + 0.5);

  // Check if skip is better
  if (RDCOST(x->rdmult, rate_i, dist_i) >= RDCOST(x->rdmult, 0, (sse << 4))) {
    dist_i = sse << 4;
    rate_i = 0;
  } else if (rate_i == 0) {
    dist_i = sse << 4;
  }

  if (rate) *rate = rate_i;
  if (dist) *dist = dist_i;
  if (rsse) *rsse = sse;
  return;
}

void model_rd_for_sb_with_dnn(const AV1_COMP *const cpi, BLOCK_SIZE bsize,
                              MACROBLOCK *x, MACROBLOCKD *xd, int plane_from,
                              int plane_to, int *out_rate_sum,
                              int64_t *out_dist_sum, int *skip_txfm_sb,
                              int64_t *skip_sse_sb, int *plane_rate,
                              int64_t *plane_sse, int64_t *plane_dist) {
  // Note our transform coeffs are 8 times an orthogonal transform.
  // Hence quantizer step is also 8 times. To get effective quantizer
  // we need to divide by 8 before sending to modeling function.
  const int ref = xd->mi[0]->ref_frame[0];

  int64_t rate_sum = 0;
  int64_t dist_sum = 0;
  int64_t total_sse = 0;

  x->pred_sse[ref] = 0;

  for (int plane = plane_from; plane <= plane_to; ++plane) {
    struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    int64_t sse;
    int rate;
    int64_t dist;

    if (x->skip_chroma_rd && plane) continue;

    model_rd_with_dnn(cpi, x, plane_bsize, plane, &sse, &rate, &dist);

    if (plane == 0) x->pred_sse[ref] = (unsigned int)AOMMIN(sse, UINT_MAX);

    total_sse += sse;
    rate_sum += rate;
    dist_sum += dist;

    if (plane_rate) plane_rate[plane] = rate;
    if (plane_sse) plane_sse[plane] = sse;
    if (plane_dist) plane_dist[plane] = dist;
  }

  if (skip_txfm_sb) *skip_txfm_sb = total_sse == 0;
  if (skip_sse_sb) *skip_sse_sb = total_sse << 4;
  *out_rate_sum = (int)rate_sum;
  *out_dist_sum = dist_sum;
}

static int64_t search_txk_type(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                               int block, int blk_row, int blk_col,
                               BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                               const TXB_CTX *const txb_ctx,
                               FAST_TX_SEARCH_MODE ftxs_mode,
                               int use_fast_coef_costing, int64_t ref_best_rd,
                               RD_STATS *best_rd_stats) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  int64_t best_rd = INT64_MAX;
  uint16_t best_eob = 0;
  TX_TYPE best_tx_type = DCT_DCT;
  TX_TYPE last_tx_type = TX_TYPES;
  const int fast_tx_search = ftxs_mode & FTXS_DCT_AND_1D_DCT_ONLY;
  // The buffer used to swap dqcoeff in macroblockd_plane so we can keep dqcoeff
  // of the best tx_type
  DECLARE_ALIGNED(32, tran_low_t, this_dqcoeff[MAX_SB_SQUARE]);
  tran_low_t *orig_dqcoeff = pd->dqcoeff;
  tran_low_t *best_dqcoeff = this_dqcoeff;
  const int txk_type_idx =
      av1_get_txk_type_index(plane_bsize, blk_row, blk_col);
  av1_invalid_rd_stats(best_rd_stats);

  TXB_RD_INFO *intra_txb_rd_info = NULL;
  uint16_t cur_joint_ctx = 0;
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  const int within_border =
      mi_row >= xd->tile.mi_row_start &&
      (mi_row + mi_size_high[plane_bsize] < xd->tile.mi_row_end) &&
      mi_col >= xd->tile.mi_col_start &&
      (mi_col + mi_size_wide[plane_bsize] < xd->tile.mi_col_end);
  if (within_border && cpi->sf.use_intra_txb_hash && frame_is_intra_only(cm) &&
      !is_inter && plane == 0 &&
      tx_size_wide[tx_size] == tx_size_high[tx_size]) {
    const uint32_t intra_hash =
        get_intra_txb_hash(x, plane, blk_row, blk_col, plane_bsize, tx_size);
    const int intra_hash_idx =
        find_tx_size_rd_info(&x->txb_rd_record_intra, intra_hash);
    intra_txb_rd_info = &x->txb_rd_record_intra.tx_rd_info[intra_hash_idx];

    cur_joint_ctx = (txb_ctx->dc_sign_ctx << 8) + txb_ctx->txb_skip_ctx;
    if (intra_hash_idx > 0 &&
        intra_txb_rd_info->entropy_context == cur_joint_ctx &&
        x->txb_rd_record_intra.tx_rd_info[intra_hash_idx].valid) {
      mbmi->txk_type[txk_type_idx] = intra_txb_rd_info->tx_type;
      const TX_TYPE ref_tx_type =
          av1_get_tx_type(get_plane_type(plane), &x->e_mbd, blk_row, blk_col,
                          tx_size, cpi->common.reduced_tx_set_used);
      if (ref_tx_type == intra_txb_rd_info->tx_type) {
        best_rd_stats->rate = intra_txb_rd_info->rate;
        best_rd_stats->dist = intra_txb_rd_info->dist;
        best_rd_stats->sse = intra_txb_rd_info->sse;
        best_rd_stats->skip = intra_txb_rd_info->eob == 0;
        x->plane[plane].eobs[block] = intra_txb_rd_info->eob;
        x->plane[plane].txb_entropy_ctx[block] =
            intra_txb_rd_info->txb_entropy_ctx;
        best_rd = RDCOST(x->rdmult, best_rd_stats->rate, best_rd_stats->dist);
        best_eob = intra_txb_rd_info->eob;
        best_tx_type = intra_txb_rd_info->tx_type;
        update_txk_array(mbmi->txk_type, plane_bsize, blk_row, blk_col, tx_size,
                         best_tx_type);
        goto RECON_INTRA;
      }
    }
  }

  int rate_cost = 0;
  TX_TYPE txk_start = DCT_DCT;
  TX_TYPE txk_end = TX_TYPES - 1;
  if ((!is_inter && x->use_default_intra_tx_type) ||
      (is_inter && x->use_default_inter_tx_type)) {
    txk_start = txk_end = get_default_tx_type(0, xd, tx_size);
  } else if (x->rd_model == LOW_TXFM_RD || x->cb_partition_scan) {
    if (plane == 0) txk_end = DCT_DCT;
  }

  uint8_t best_txb_ctx = 0;
  const TxSetType tx_set_type =
      av1_get_ext_tx_set_type(tx_size, is_inter, cm->reduced_tx_set_used);

  TX_TYPE uv_tx_type = DCT_DCT;
  if (plane) {
    // tx_type of PLANE_TYPE_UV should be the same as PLANE_TYPE_Y
    uv_tx_type = txk_start = txk_end =
        av1_get_tx_type(get_plane_type(plane), xd, blk_row, blk_col, tx_size,
                        cm->reduced_tx_set_used);
  }
  const uint16_t ext_tx_used_flag = av1_ext_tx_used_flag[tx_set_type];
  if (xd->lossless[mbmi->segment_id] || txsize_sqr_up_map[tx_size] > TX_32X32 ||
      ext_tx_used_flag == 0x0001) {
    txk_start = txk_end = DCT_DCT;
  }
  uint16_t allowed_tx_mask = 0;  // 1: allow; 0: skip.
  if (txk_start == txk_end) {
    allowed_tx_mask = 1 << txk_start;
    allowed_tx_mask &= ext_tx_used_flag;
  } else if (fast_tx_search) {
    allowed_tx_mask = 0x0c01;  // V_DCT, H_DCT, DCT_DCT
    allowed_tx_mask &= ext_tx_used_flag;
  } else {
    assert(plane == 0);
    allowed_tx_mask = ext_tx_used_flag;
    // !fast_tx_search && txk_end != txk_start && plane == 0
    const int do_prune = cpi->sf.tx_type_search.prune_mode > NO_PRUNE;
    if (do_prune && is_inter) {
      if (cpi->sf.tx_type_search.prune_mode >= PRUNE_2D_ACCURATE) {
        const uint16_t prune =
            prune_tx_2D(x, plane_bsize, tx_size, blk_row, blk_col, tx_set_type,
                        cpi->sf.tx_type_search.prune_mode);
        allowed_tx_mask &= (~prune);
      } else {
        allowed_tx_mask &= (~x->tx_search_prune[tx_set_type]);
      }
    }
  }
  // Need to have at least one transform type allowed.
  if (allowed_tx_mask == 0) {
    txk_start = txk_end = (plane ? uv_tx_type : DCT_DCT);
    allowed_tx_mask = (1 << txk_start);
  }

  int use_transform_domain_distortion =
      (cpi->sf.use_transform_domain_distortion > 0) &&
      // Any 64-pt transforms only preserves half the coefficients.
      // Therefore transform domain distortion is not valid for these
      // transform sizes.
      txsize_sqr_up_map[tx_size] != TX_64X64;
#if CONFIG_DIST_8X8
  if (x->using_dist_8x8) use_transform_domain_distortion = 0;
#endif

  int calc_pixel_domain_distortion_final =
      cpi->sf.use_transform_domain_distortion == 1 &&
      use_transform_domain_distortion && x->rd_model != LOW_TXFM_RD &&
      !x->cb_partition_scan;
  if (calc_pixel_domain_distortion_final &&
      (txk_start == txk_end || allowed_tx_mask == 0x0001))
    calc_pixel_domain_distortion_final = use_transform_domain_distortion = 0;

  const uint16_t *eobs_ptr = x->plane[plane].eobs;

  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  int64_t block_sse =
      pixel_diff_dist(x, plane, blk_row, blk_col, plane_bsize, tx_bsize, 1);
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    block_sse = ROUND_POWER_OF_TWO(block_sse, (xd->bd - 8) * 2);
  block_sse *= 16;

  for (TX_TYPE tx_type = txk_start; tx_type <= txk_end; ++tx_type) {
    if (!(allowed_tx_mask & (1 << tx_type))) continue;
    if (plane == 0) mbmi->txk_type[txk_type_idx] = tx_type;
    RD_STATS this_rd_stats;
    av1_invalid_rd_stats(&this_rd_stats);

    if (!cpi->optimize_seg_arr[mbmi->segment_id]) {
      av1_xform_quant(
          cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size, tx_type,
          USE_B_QUANT_NO_TRELLIS ? AV1_XFORM_QUANT_B : AV1_XFORM_QUANT_FP);
      rate_cost = av1_cost_coeffs(cm, x, plane, block, tx_size, tx_type,
                                  txb_ctx, use_fast_coef_costing);
    } else {
      av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize,
                      tx_size, tx_type, AV1_XFORM_QUANT_FP);
      if (cpi->sf.optimize_b_precheck && best_rd < INT64_MAX &&
          eobs_ptr[block] >= 4) {
        // Calculate distortion quickly in transform domain.
        dist_block_tx_domain(x, plane, block, tx_size, &this_rd_stats.dist,
                             &this_rd_stats.sse);

        const int64_t best_rd_ = AOMMIN(best_rd, ref_best_rd);
        const int64_t dist_cost_estimate =
            RDCOST(x->rdmult, 0, AOMMIN(this_rd_stats.dist, this_rd_stats.sse));
        if (dist_cost_estimate - (dist_cost_estimate >> 3) > best_rd_) continue;

        rate_cost = av1_cost_coeffs(cm, x, plane, block, tx_size, tx_type,
                                    txb_ctx, use_fast_coef_costing);
        const int64_t rd_estimate =
            AOMMIN(RDCOST(x->rdmult, rate_cost, this_rd_stats.dist),
                   RDCOST(x->rdmult, 0, this_rd_stats.sse));
        if (rd_estimate - (rd_estimate >> 3) > best_rd_) continue;
      }
      av1_optimize_b(cpi, x, plane, block, tx_size, tx_type, txb_ctx, 1,
                     &rate_cost);
    }
    if (eobs_ptr[block] == 0) {
      // When eob is 0, pixel domain distortion is more efficient and accurate.
      this_rd_stats.dist = this_rd_stats.sse = block_sse;
    } else if (use_transform_domain_distortion) {
      dist_block_tx_domain(x, plane, block, tx_size, &this_rd_stats.dist,
                           &this_rd_stats.sse);
    } else {
      this_rd_stats.dist = dist_block_px_domain(
          cpi, x, plane, plane_bsize, block, blk_row, blk_col, tx_size);
      this_rd_stats.sse = block_sse;
    }

    this_rd_stats.rate = rate_cost;

    const int64_t rd =
        RDCOST(x->rdmult, this_rd_stats.rate, this_rd_stats.dist);

    if (rd < best_rd) {
      best_rd = rd;
      *best_rd_stats = this_rd_stats;
      best_tx_type = tx_type;
      best_txb_ctx = x->plane[plane].txb_entropy_ctx[block];
      best_eob = x->plane[plane].eobs[block];
      last_tx_type = best_tx_type;

      // Swap qcoeff and dqcoeff buffers
      tran_low_t *const tmp_dqcoeff = best_dqcoeff;
      best_dqcoeff = pd->dqcoeff;
      pd->dqcoeff = tmp_dqcoeff;
    }

#if CONFIG_COLLECT_RD_STATS == 1
    if (plane == 0) {
      PrintTransformUnitStats(cpi, x, &this_rd_stats, blk_row, blk_col,
                              plane_bsize, tx_size, tx_type, rd);
    }
#endif  // CONFIG_COLLECT_RD_STATS == 1

    if (cpi->sf.adaptive_txb_search_level) {
      if ((best_rd - (best_rd >> cpi->sf.adaptive_txb_search_level)) >
          ref_best_rd) {
        break;
      }
    }

    // Skip transform type search when we found the block has been quantized to
    // all zero and at the same time, it has better rdcost than doing transform.
    if (cpi->sf.tx_type_search.skip_tx_search && !best_eob) break;
  }

  assert(best_rd != INT64_MAX);

  best_rd_stats->skip = best_eob == 0;
  if (best_eob == 0) best_tx_type = DCT_DCT;
  if (plane == 0) {
    update_txk_array(mbmi->txk_type, plane_bsize, blk_row, blk_col, tx_size,
                     best_tx_type);
  }
  x->plane[plane].txb_entropy_ctx[block] = best_txb_ctx;
  x->plane[plane].eobs[block] = best_eob;

  pd->dqcoeff = best_dqcoeff;

  if (calc_pixel_domain_distortion_final && best_eob) {
    best_rd_stats->dist = dist_block_px_domain(
        cpi, x, plane, plane_bsize, block, blk_row, blk_col, tx_size);
    best_rd_stats->sse = block_sse;
  }

  if (intra_txb_rd_info != NULL) {
    intra_txb_rd_info->valid = 1;
    intra_txb_rd_info->entropy_context = cur_joint_ctx;
    intra_txb_rd_info->rate = best_rd_stats->rate;
    intra_txb_rd_info->dist = best_rd_stats->dist;
    intra_txb_rd_info->sse = best_rd_stats->sse;
    intra_txb_rd_info->eob = best_eob;
    intra_txb_rd_info->txb_entropy_ctx = best_txb_ctx;
    if (plane == 0) intra_txb_rd_info->tx_type = best_tx_type;
  }

RECON_INTRA:
  if (!is_inter && best_eob &&
      (blk_row + tx_size_high_unit[tx_size] < mi_size_high[plane_bsize] ||
       blk_col + tx_size_wide_unit[tx_size] < mi_size_wide[plane_bsize])) {
    // intra mode needs decoded result such that the next transform block
    // can use it for prediction.
    // if the last search tx_type is the best tx_type, we don't need to
    // do this again
    if (best_tx_type != last_tx_type) {
      if (!cpi->optimize_seg_arr[mbmi->segment_id]) {
        av1_xform_quant(
            cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
            best_tx_type,
            USE_B_QUANT_NO_TRELLIS ? AV1_XFORM_QUANT_B : AV1_XFORM_QUANT_FP);
      } else {
        av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize,
                        tx_size, best_tx_type, AV1_XFORM_QUANT_FP);
        av1_optimize_b(cpi, x, plane, block, tx_size, best_tx_type, txb_ctx, 1,
                       &rate_cost);
      }
    }

    inverse_transform_block_facade(xd, plane, block, blk_row, blk_col,
                                   x->plane[plane].eobs[block],
                                   cm->reduced_tx_set_used);

    // This may happen because of hash collision. The eob stored in the hash
    // table is non-zero, but the real eob is zero. We need to make sure tx_type
    // is DCT_DCT in this case.
    if (plane == 0 && x->plane[plane].eobs[block] == 0 &&
        best_tx_type != DCT_DCT) {
      update_txk_array(mbmi->txk_type, plane_bsize, blk_row, blk_col, tx_size,
                       DCT_DCT);
    }
  }
  pd->dqcoeff = orig_dqcoeff;

  return best_rd;
}

static void block_rd_txfm(int plane, int block, int blk_row, int blk_col,
                          BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
  struct rdcost_block_args *args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const AV1_COMP *cpi = args->cpi;
  ENTROPY_CONTEXT *a = args->t_above + blk_col;
  ENTROPY_CONTEXT *l = args->t_left + blk_row;
  const AV1_COMMON *cm = &cpi->common;
  int64_t rd1, rd2, rd;
  RD_STATS this_rd_stats;

#if CONFIG_DIST_8X8
  // If sub8x8 tx, 8x8 or larger partition, and luma channel,
  // dist-8x8 disables early skip, because the distortion metrics for
  // sub8x8 tx (MSE) and reference distortion from 8x8 or larger partition
  // (new distortion metric) are different.
  // Exception is: dist-8x8 is enabled but still MSE is used,
  // i.e. "--tune=" encoder option is not used.
  int bw = block_size_wide[plane_bsize];
  int bh = block_size_high[plane_bsize];
  int disable_early_skip =
      x->using_dist_8x8 && plane == AOM_PLANE_Y && bw >= 8 && bh >= 8 &&
      (tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4) &&
      x->tune_metric != AOM_TUNE_PSNR;
#endif  // CONFIG_DIST_8X8

  av1_init_rd_stats(&this_rd_stats);

  if (args->exit_early) return;

  if (!is_inter_block(mbmi)) {
    av1_predict_intra_block_facade(cm, xd, plane, blk_col, blk_row, tx_size);
    av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size);
  }
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, a, l, &txb_ctx);
  search_txk_type(cpi, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  &txb_ctx, args->ftxs_mode, args->use_fast_coef_costing,
                  args->best_rd - args->this_rd, &this_rd_stats);

  if (plane == AOM_PLANE_Y && xd->cfl.store_y) {
    assert(!is_inter_block(mbmi) || plane_bsize < BLOCK_8X8);
    cfl_store_tx(xd, blk_row, blk_col, tx_size, plane_bsize);
  }

#if CONFIG_RD_DEBUG
  av1_update_txb_coeff_cost(&this_rd_stats, plane, tx_size, blk_row, blk_col,
                            this_rd_stats.rate);
#endif  // CONFIG_RD_DEBUG
  av1_set_txb_context(x, plane, block, tx_size, a, l);

  if (plane == 0) {
    x->blk_skip[blk_row *
                    (block_size_wide[plane_bsize] >> tx_size_wide_log2[0]) +
                blk_col] = (x->plane[plane].eobs[block] == 0);
  }

  rd1 = RDCOST(x->rdmult, this_rd_stats.rate, this_rd_stats.dist);
  rd2 = RDCOST(x->rdmult, 0, this_rd_stats.sse);

  // TODO(jingning): temporarily enabled only for luma component
  rd = AOMMIN(rd1, rd2);

  this_rd_stats.skip &= !x->plane[plane].eobs[block];

  av1_merge_rd_stats(&args->rd_stats, &this_rd_stats);

  args->this_rd += rd;

#if CONFIG_DIST_8X8
  if (!disable_early_skip)
#endif
    if (args->this_rd > args->best_rd) {
      args->exit_early = 1;
      return;
    }
}

#if CONFIG_DIST_8X8
static void dist_8x8_sub8x8_txfm_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize,
                                    struct rdcost_block_args *args) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[0];
  const struct macroblock_plane *const p = &x->plane[0];
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  const uint8_t *src = &p->src.buf[0];
  const uint8_t *dst = &pd->dst.buf[0];
  const int16_t *pred = &x->pred_luma[0];
  int bw = block_size_wide[bsize];
  int bh = block_size_high[bsize];
  int visible_w = bw;
  int visible_h = bh;

  int i, j;
  int64_t rd, rd1, rd2;
  int64_t sse = INT64_MAX, dist = INT64_MAX;
  int qindex = x->qindex;

  assert((bw & 0x07) == 0);
  assert((bh & 0x07) == 0);

  get_txb_dimensions(xd, 0, bsize, 0, 0, bsize, &bw, &bh, &visible_w,
                     &visible_h);

  const int diff_stride = block_size_wide[bsize];
  const int16_t *diff = p->src_diff;
  sse = dist_8x8_diff(x, src, src_stride, diff, diff_stride, bw, bh, visible_w,
                      visible_h, qindex);
  sse = ROUND_POWER_OF_TWO(sse, (xd->bd - 8) * 2);
  sse *= 16;

  if (!is_inter_block(mbmi)) {
    dist = av1_dist_8x8(cpi, x, src, src_stride, dst, dst_stride, bsize, bw, bh,
                        visible_w, visible_h, qindex);
    dist *= 16;
  } else {
    // For inter mode, the decoded pixels are provided in x->pred_luma,
    // while the predicted pixels are in dst.
    uint8_t *pred8;
    DECLARE_ALIGNED(16, uint16_t, pred16[MAX_SB_SQUARE]);

    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      pred8 = CONVERT_TO_BYTEPTR(pred16);
    else
      pred8 = (uint8_t *)pred16;

    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      for (j = 0; j < bh; j++)
        for (i = 0; i < bw; i++)
          CONVERT_TO_SHORTPTR(pred8)[j * bw + i] = pred[j * bw + i];
    } else {
      for (j = 0; j < bh; j++)
        for (i = 0; i < bw; i++) pred8[j * bw + i] = (uint8_t)pred[j * bw + i];
    }

    dist = av1_dist_8x8(cpi, x, src, src_stride, pred8, bw, bsize, bw, bh,
                        visible_w, visible_h, qindex);
    dist *= 16;
  }

#ifdef DEBUG_DIST_8X8
  if (x->tune_metric == AOM_TUNE_PSNR && xd->bd == 8) {
    assert(args->rd_stats.sse == sse);
    assert(args->rd_stats.dist == dist);
  }
#endif  // DEBUG_DIST_8X8

  args->rd_stats.sse = sse;
  args->rd_stats.dist = dist;

  rd1 = RDCOST(x->rdmult, args->rd_stats.rate, args->rd_stats.dist);
  rd2 = RDCOST(x->rdmult, 0, args->rd_stats.sse);
  rd = AOMMIN(rd1, rd2);

  args->rd_stats.rdcost = rd;
  args->this_rd = rd;

  if (args->this_rd > args->best_rd) args->exit_early = 1;
}
#endif  // CONFIG_DIST_8X8

static void txfm_rd_in_plane(MACROBLOCK *x, const AV1_COMP *cpi,
                             RD_STATS *rd_stats, int64_t ref_best_rd, int plane,
                             BLOCK_SIZE bsize, TX_SIZE tx_size,
                             int use_fast_coef_casting,
                             FAST_TX_SEARCH_MODE ftxs_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  struct rdcost_block_args args;
  av1_zero(args);
  args.x = x;
  args.cpi = cpi;
  args.best_rd = ref_best_rd;
  args.use_fast_coef_costing = use_fast_coef_casting;
  args.ftxs_mode = ftxs_mode;
  av1_init_rd_stats(&args.rd_stats);

  if (plane == 0) xd->mi[0]->tx_size = tx_size;

  av1_get_entropy_contexts(bsize, pd, args.t_above, args.t_left);

  av1_foreach_transformed_block_in_plane(xd, bsize, plane, block_rd_txfm,
                                         &args);
#if CONFIG_DIST_8X8
  int bw = block_size_wide[bsize];
  int bh = block_size_high[bsize];

  if (x->using_dist_8x8 && !args.exit_early && plane == 0 && bw >= 8 &&
      bh >= 8 && (tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4))
    dist_8x8_sub8x8_txfm_rd(cpi, x, bsize, &args);
#endif

  if (args.exit_early) {
    av1_invalid_rd_stats(rd_stats);
  } else {
    *rd_stats = args.rd_stats;
  }
}

static int tx_size_cost(const AV1_COMMON *const cm, const MACROBLOCK *const x,
                        BLOCK_SIZE bsize, TX_SIZE tx_size) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];

  if (cm->tx_mode == TX_MODE_SELECT && block_signals_txsize(mbmi->sb_type)) {
    const int32_t tx_size_cat = bsize_to_tx_size_cat(bsize);
    const int depth = tx_size_to_depth(tx_size, bsize);
    const int tx_size_ctx = get_tx_size_context(xd);
    int r_tx_size = x->tx_size_cost[tx_size_cat][tx_size_ctx][depth];
    return r_tx_size;
  } else {
    return 0;
  }
}

static int64_t txfm_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                        RD_STATS *rd_stats, int64_t ref_best_rd, BLOCK_SIZE bs,
                        TX_SIZE tx_size, FAST_TX_SEARCH_MODE ftxs_mode) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int64_t rd = INT64_MAX;
  const int skip_ctx = av1_get_skip_context(xd);
  int s0, s1;
  const int is_inter = is_inter_block(mbmi);
  const int tx_select =
      cm->tx_mode == TX_MODE_SELECT && block_signals_txsize(mbmi->sb_type);
  int ctx = txfm_partition_context(
      xd->above_txfm_context, xd->left_txfm_context, mbmi->sb_type, tx_size);
  const int r_tx_size = is_inter ? x->txfm_partition_cost[ctx][0]
                                 : tx_size_cost(cm, x, bs, tx_size);

  assert(IMPLIES(is_rect_tx(tx_size), is_rect_tx_allowed_bsize(bs)));

  s0 = x->skip_cost[skip_ctx][0];
  s1 = x->skip_cost[skip_ctx][1];

  mbmi->tx_size = tx_size;
  txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, AOM_PLANE_Y, bs, tx_size,
                   cpi->sf.use_fast_coef_costing, ftxs_mode);
  if (rd_stats->rate == INT_MAX) return INT64_MAX;

  if (rd_stats->skip) {
    if (is_inter) {
      rd = RDCOST(x->rdmult, s1, rd_stats->sse);
    } else {
      rd = RDCOST(x->rdmult, s1 + r_tx_size * tx_select, rd_stats->sse);
    }
  } else {
    rd = RDCOST(x->rdmult, rd_stats->rate + s0 + r_tx_size * tx_select,
                rd_stats->dist);
  }

  if (tx_select) rd_stats->rate += r_tx_size;

  if (is_inter && !xd->lossless[xd->mi[0]->segment_id] && !(rd_stats->skip))
    rd = AOMMIN(rd, RDCOST(x->rdmult, s1, rd_stats->sse));

  return rd;
}

static int64_t estimate_yrd_for_sb(const AV1_COMP *const cpi, BLOCK_SIZE bs,
                                   MACROBLOCK *x, int *r, int64_t *d, int *s,
                                   int64_t *sse, int64_t ref_best_rd) {
  RD_STATS rd_stats;
  av1_subtract_plane(x, bs, 0);
  x->rd_model = LOW_TXFM_RD;
  int64_t rd = txfm_yrd(cpi, x, &rd_stats, ref_best_rd, bs,
                        max_txsize_rect_lookup[bs], FTXS_NONE);
  x->rd_model = FULL_TXFM_RD;
  *r = rd_stats.rate;
  *d = rd_stats.dist;
  *s = rd_stats.skip;
  *sse = rd_stats.sse;
  return rd;
}

static void choose_largest_tx_size(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   RD_STATS *rd_stats, int64_t ref_best_rd,
                                   BLOCK_SIZE bs) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  mbmi->tx_size = tx_size_from_tx_mode(bs, cm->tx_mode);
  const TxSetType tx_set_type =
      av1_get_ext_tx_set_type(mbmi->tx_size, is_inter, cm->reduced_tx_set_used);
  prune_tx(cpi, bs, x, xd, tx_set_type);
  txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, AOM_PLANE_Y, bs,
                   mbmi->tx_size, cpi->sf.use_fast_coef_costing, FTXS_NONE);
  // Reset the pruning flags.
  av1_zero(x->tx_search_prune);
  x->tx_split_prune_flag = 0;
}

static void choose_smallest_tx_size(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    RD_STATS *rd_stats, int64_t ref_best_rd,
                                    BLOCK_SIZE bs) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];

  mbmi->tx_size = TX_4X4;
  txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, bs, mbmi->tx_size,
                   cpi->sf.use_fast_coef_costing, FTXS_NONE);
}

static INLINE int bsize_to_num_blk(BLOCK_SIZE bsize) {
  int num_blk = 1 << (num_pels_log2_lookup[bsize] - 2 * tx_size_wide_log2[0]);
  return num_blk;
}

static int get_search_init_depth(int mi_width, int mi_height, int is_inter,
                                 const SPEED_FEATURES *sf) {
  if (sf->tx_size_search_method == USE_LARGESTALL) return MAX_VARTX_DEPTH;

  if (sf->tx_size_search_lgr_block) {
    if (mi_width > mi_size_wide[BLOCK_64X64] ||
        mi_height > mi_size_high[BLOCK_64X64])
      return MAX_VARTX_DEPTH;
  }

  if (is_inter) {
    return (mi_height != mi_width) ? sf->inter_tx_size_search_init_depth_rect
                                   : sf->inter_tx_size_search_init_depth_sqr;
  } else {
    return (mi_height != mi_width) ? sf->intra_tx_size_search_init_depth_rect
                                   : sf->intra_tx_size_search_init_depth_sqr;
  }
}

static void choose_tx_size_type_from_rd(const AV1_COMP *const cpi,
                                        MACROBLOCK *x, RD_STATS *rd_stats,
                                        int64_t ref_best_rd, BLOCK_SIZE bs) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int64_t rd = INT64_MAX;
  int n;
  int start_tx;
  int depth;
  int64_t best_rd = INT64_MAX;
  const TX_SIZE max_rect_tx_size = max_txsize_rect_lookup[bs];
  TX_SIZE best_tx_size = max_rect_tx_size;
  TX_TYPE best_txk_type[TXK_TYPE_BUF_LEN];
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const int n4 = bsize_to_num_blk(bs);
  const int tx_select = cm->tx_mode == TX_MODE_SELECT;

  av1_invalid_rd_stats(rd_stats);

  if (tx_select) {
    start_tx = max_rect_tx_size;
    depth = get_search_init_depth(mi_size_wide[bs], mi_size_high[bs],
                                  is_inter_block(mbmi), &cpi->sf);
  } else {
    const TX_SIZE chosen_tx_size = tx_size_from_tx_mode(bs, cm->tx_mode);
    start_tx = chosen_tx_size;
    depth = MAX_TX_DEPTH;
  }

  prune_tx(cpi, bs, x, xd, EXT_TX_SET_ALL16);

  for (n = start_tx; depth <= MAX_TX_DEPTH; depth++, n = sub_tx_size_map[n]) {
    RD_STATS this_rd_stats;
    if (mbmi->ref_mv_idx > 0) x->rd_model = LOW_TXFM_RD;
    rd = txfm_yrd(cpi, x, &this_rd_stats, ref_best_rd, bs, n, FTXS_NONE);
    x->rd_model = FULL_TXFM_RD;

    if (rd < best_rd) {
      memcpy(best_txk_type, mbmi->txk_type,
             sizeof(best_txk_type[0]) * TXK_TYPE_BUF_LEN);
      memcpy(best_blk_skip, x->blk_skip, sizeof(best_blk_skip[0]) * n4);
      best_tx_size = n;
      best_rd = rd;
      *rd_stats = this_rd_stats;
    }
    if (n == TX_4X4) break;
  }
  mbmi->tx_size = best_tx_size;
  memcpy(mbmi->txk_type, best_txk_type,
         sizeof(best_txk_type[0]) * TXK_TYPE_BUF_LEN);
  memcpy(x->blk_skip, best_blk_skip, sizeof(best_blk_skip[0]) * n4);

  // Reset the pruning flags.
  av1_zero(x->tx_search_prune);
  x->tx_split_prune_flag = 0;
}

static void super_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bs,
                            int64_t ref_best_rd) {
  MACROBLOCKD *xd = &x->e_mbd;
  av1_init_rd_stats(rd_stats);

  assert(bs == xd->mi[0]->sb_type);

  if (xd->lossless[xd->mi[0]->segment_id]) {
    choose_smallest_tx_size(cpi, x, rd_stats, ref_best_rd, bs);
  } else if (cpi->sf.tx_size_search_method == USE_LARGESTALL) {
    choose_largest_tx_size(cpi, x, rd_stats, ref_best_rd, bs);
  } else {
    choose_tx_size_type_from_rd(cpi, x, rd_stats, ref_best_rd, bs);
  }
}

// Return the rate cost for luma prediction mode info. of intra blocks.
static int intra_mode_info_cost_y(const AV1_COMP *cpi, const MACROBLOCK *x,
                                  const MB_MODE_INFO *mbmi, BLOCK_SIZE bsize,
                                  int mode_cost) {
  int total_rate = mode_cost;
  const int use_palette = mbmi->palette_mode_info.palette_size[0] > 0;
  const int use_filter_intra = mbmi->filter_intra_mode_info.use_filter_intra;
  const int use_intrabc = mbmi->use_intrabc;
  // Can only activate one mode.
  assert(((mbmi->mode != DC_PRED) + use_palette + use_intrabc +
          use_filter_intra) <= 1);
  const int try_palette =
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  if (try_palette && mbmi->mode == DC_PRED) {
    const MACROBLOCKD *xd = &x->e_mbd;
    const int bsize_ctx = av1_get_palette_bsize_ctx(bsize);
    const int mode_ctx = av1_get_palette_mode_ctx(xd);
    total_rate += x->palette_y_mode_cost[bsize_ctx][mode_ctx][use_palette];
    if (use_palette) {
      const uint8_t *const color_map = xd->plane[0].color_index_map;
      int block_width, block_height, rows, cols;
      av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                               &cols);
      const int plt_size = mbmi->palette_mode_info.palette_size[0];
      int palette_mode_cost =
          x->palette_y_size_cost[bsize_ctx][plt_size - PALETTE_MIN_SIZE] +
          write_uniform_cost(plt_size, color_map[0]);
      uint16_t color_cache[2 * PALETTE_MAX_SIZE];
      const int n_cache = av1_get_palette_cache(xd, 0, color_cache);
      palette_mode_cost +=
          av1_palette_color_cost_y(&mbmi->palette_mode_info, color_cache,
                                   n_cache, cpi->common.seq_params.bit_depth);
      palette_mode_cost +=
          av1_cost_color_map(x, 0, bsize, mbmi->tx_size, PALETTE_MAP);
      total_rate += palette_mode_cost;
    }
  }
  if (av1_filter_intra_allowed(&cpi->common, mbmi)) {
    total_rate += x->filter_intra_cost[mbmi->sb_type][use_filter_intra];
    if (use_filter_intra) {
      total_rate += x->filter_intra_mode_cost[mbmi->filter_intra_mode_info
                                                  .filter_intra_mode];
    }
  }
  if (av1_is_directional_mode(mbmi->mode)) {
    if (av1_use_angle_delta(bsize)) {
      total_rate += x->angle_delta_cost[mbmi->mode - V_PRED]
                                       [MAX_ANGLE_DELTA +
                                        mbmi->angle_delta[PLANE_TYPE_Y]];
    }
  }
  if (av1_allow_intrabc(&cpi->common))
    total_rate += x->intrabc_cost[use_intrabc];
  return total_rate;
}

// Return the rate cost for chroma prediction mode info. of intra blocks.
static int intra_mode_info_cost_uv(const AV1_COMP *cpi, const MACROBLOCK *x,
                                   const MB_MODE_INFO *mbmi, BLOCK_SIZE bsize,
                                   int mode_cost) {
  int total_rate = mode_cost;
  const int use_palette = mbmi->palette_mode_info.palette_size[1] > 0;
  const UV_PREDICTION_MODE mode = mbmi->uv_mode;
  // Can only activate one mode.
  assert(((mode != UV_DC_PRED) + use_palette + mbmi->use_intrabc) <= 1);

  const int try_palette =
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  if (try_palette && mode == UV_DC_PRED) {
    const PALETTE_MODE_INFO *pmi = &mbmi->palette_mode_info;
    total_rate +=
        x->palette_uv_mode_cost[pmi->palette_size[0] > 0][use_palette];
    if (use_palette) {
      const int bsize_ctx = av1_get_palette_bsize_ctx(bsize);
      const int plt_size = pmi->palette_size[1];
      const MACROBLOCKD *xd = &x->e_mbd;
      const uint8_t *const color_map = xd->plane[1].color_index_map;
      int palette_mode_cost =
          x->palette_uv_size_cost[bsize_ctx][plt_size - PALETTE_MIN_SIZE] +
          write_uniform_cost(plt_size, color_map[0]);
      uint16_t color_cache[2 * PALETTE_MAX_SIZE];
      const int n_cache = av1_get_palette_cache(xd, 1, color_cache);
      palette_mode_cost += av1_palette_color_cost_uv(
          pmi, color_cache, n_cache, cpi->common.seq_params.bit_depth);
      palette_mode_cost +=
          av1_cost_color_map(x, 1, bsize, mbmi->tx_size, PALETTE_MAP);
      total_rate += palette_mode_cost;
    }
  }
  if (av1_is_directional_mode(get_uv_mode(mode))) {
    if (av1_use_angle_delta(bsize)) {
      total_rate +=
          x->angle_delta_cost[mode - V_PRED][mbmi->angle_delta[PLANE_TYPE_UV] +
                                             MAX_ANGLE_DELTA];
    }
  }
  return total_rate;
}

static int conditional_skipintra(PREDICTION_MODE mode,
                                 PREDICTION_MODE best_intra_mode) {
  if (mode == D113_PRED && best_intra_mode != V_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  if (mode == D67_PRED && best_intra_mode != V_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D203_PRED && best_intra_mode != H_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D157_PRED && best_intra_mode != H_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  return 0;
}

// Model based RD estimation for luma intra blocks.
static int64_t intra_model_yrd(const AV1_COMP *const cpi, MACROBLOCK *const x,
                               BLOCK_SIZE bsize, int mode_cost) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  RD_STATS this_rd_stats;
  int row, col;
  int64_t temp_sse, this_rd;
  TX_SIZE tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode);
  const int stepr = tx_size_high_unit[tx_size];
  const int stepc = tx_size_wide_unit[tx_size];
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  mbmi->tx_size = tx_size;
  // Prediction.
  for (row = 0; row < max_blocks_high; row += stepr) {
    for (col = 0; col < max_blocks_wide; col += stepc) {
      av1_predict_intra_block_facade(cm, xd, 0, col, row, tx_size);
    }
  }
  // RD estimation.
  av1_subtract_plane(x, bsize, 0);
  model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &this_rd_stats.rate,
                  &this_rd_stats.dist, &this_rd_stats.skip, &temp_sse, NULL,
                  NULL, NULL);
  if (av1_is_directional_mode(mbmi->mode) && av1_use_angle_delta(bsize)) {
    mode_cost +=
        x->angle_delta_cost[mbmi->mode - V_PRED]
                           [MAX_ANGLE_DELTA + mbmi->angle_delta[PLANE_TYPE_Y]];
  }
  if (mbmi->mode == DC_PRED &&
      av1_filter_intra_allowed_bsize(cm, mbmi->sb_type)) {
    if (mbmi->filter_intra_mode_info.use_filter_intra) {
      const int mode = mbmi->filter_intra_mode_info.filter_intra_mode;
      mode_cost += x->filter_intra_cost[mbmi->sb_type][1] +
                   x->filter_intra_mode_cost[mode];
    } else {
      mode_cost += x->filter_intra_cost[mbmi->sb_type][0];
    }
  }
  this_rd =
      RDCOST(x->rdmult, this_rd_stats.rate + mode_cost, this_rd_stats.dist);
  return this_rd;
}

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

// Bias toward using colors in the cache.
// TODO(huisu): Try other schemes to improve compression.
static void optimize_palette_colors(uint16_t *color_cache, int n_cache,
                                    int n_colors, int stride, int *centroids) {
  if (n_cache <= 0) return;
  for (int i = 0; i < n_colors * stride; i += stride) {
    int min_diff = abs(centroids[i] - (int)color_cache[0]);
    int idx = 0;
    for (int j = 1; j < n_cache; ++j) {
      const int this_diff = abs(centroids[i] - color_cache[j]);
      if (this_diff < min_diff) {
        min_diff = this_diff;
        idx = j;
      }
    }
    if (min_diff <= 1) centroids[i] = color_cache[idx];
  }
}

// Given the base colors as specified in centroids[], calculate the RD cost
// of palette mode.
static void palette_rd_y(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int *data, int *centroids, int n,
    uint16_t *color_cache, int n_cache, MB_MODE_INFO *best_mbmi,
    uint8_t *best_palette_color_map, int64_t *best_rd, int64_t *best_model_rd,
    int *rate, int *rate_tokenonly, int *rate_overhead, int64_t *distortion,
    int *skippable, PICK_MODE_CONTEXT *ctx, uint8_t *blk_skip) {
  optimize_palette_colors(color_cache, n_cache, n, 1, centroids);
  int k = av1_remove_duplicates(centroids, n);
  if (k < PALETTE_MIN_SIZE) {
    // Too few unique colors to create a palette. And DC_PRED will work
    // well for that case anyway. So skip.
    return;
  }
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  if (cpi->common.seq_params.use_highbitdepth)
    for (int i = 0; i < k; ++i)
      pmi->palette_colors[i] = clip_pixel_highbd(
          (int)centroids[i], cpi->common.seq_params.bit_depth);
  else
    for (int i = 0; i < k; ++i)
      pmi->palette_colors[i] = clip_pixel(centroids[i]);
  pmi->palette_size[0] = k;
  MACROBLOCKD *const xd = &x->e_mbd;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  int block_width, block_height, rows, cols;
  av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                           &cols);
  av1_calc_indices(data, centroids, color_map, rows * cols, k, 1);
  extend_palette_color_map(color_map, cols, rows, block_width, block_height);
  const int palette_mode_cost =
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, dc_mode_cost);
  int64_t this_model_rd = intra_model_yrd(cpi, x, bsize, palette_mode_cost);
  if (*best_model_rd != INT64_MAX &&
      this_model_rd > *best_model_rd + (*best_model_rd >> 1))
    return;
  if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
  RD_STATS tokenonly_rd_stats;
  super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
  if (tokenonly_rd_stats.rate == INT_MAX) return;
  int this_rate = tokenonly_rd_stats.rate + palette_mode_cost;
  int64_t this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->sb_type)) {
    tokenonly_rd_stats.rate -=
        tx_size_cost(&cpi->common, x, bsize, mbmi->tx_size);
  }
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    memcpy(best_palette_color_map, color_map,
           block_width * block_height * sizeof(color_map[0]));
    *best_mbmi = *mbmi;
    memcpy(blk_skip, x->blk_skip, sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
    *rate_overhead = this_rate - tokenonly_rd_stats.rate;
    if (rate) *rate = this_rate;
    if (rate_tokenonly) *rate_tokenonly = tokenonly_rd_stats.rate;
    if (distortion) *distortion = tokenonly_rd_stats.dist;
    if (skippable) *skippable = tokenonly_rd_stats.skip;
  }
}

static int rd_pick_palette_intra_sby(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int dc_mode_cost, MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map,
    int64_t *best_rd, int64_t *best_model_rd, int *rate, int *rate_tokenonly,
    int64_t *distortion, int *skippable, PICK_MODE_CONTEXT *ctx,
    uint8_t *best_blk_skip) {
  int rate_overhead = 0;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  assert(av1_allow_palette(cpi->common.allow_screen_content_tools, bsize));
  const SequenceHeader *const seq_params = &cpi->common.seq_params;
  int colors, n;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *const src = x->plane[0].src.buf;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  int block_width, block_height, rows, cols;
  av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                           &cols);

  int count_buf[1 << 12];  // Maximum (1 << 12) color levels.
  if (seq_params->use_highbitdepth)
    colors = av1_count_colors_highbd(src, src_stride, rows, cols,
                                     seq_params->bit_depth, count_buf);
  else
    colors = av1_count_colors(src, src_stride, rows, cols, count_buf);
  mbmi->filter_intra_mode_info.use_filter_intra = 0;

  if (colors > 1 && colors <= 64) {
    int r, c, i;
    const int max_itr = 50;
    int *const data = x->palette_buffer->kmeans_data_buf;
    int centroids[PALETTE_MAX_SIZE];
    int lb, ub, val;
    uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
    if (seq_params->use_highbitdepth)
      lb = ub = src16[0];
    else
      lb = ub = src[0];

    if (seq_params->use_highbitdepth) {
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
    }

    mbmi->mode = DC_PRED;
    mbmi->filter_intra_mode_info.use_filter_intra = 0;

    uint16_t color_cache[2 * PALETTE_MAX_SIZE];
    const int n_cache = av1_get_palette_cache(xd, 0, color_cache);

    // Find the dominant colors, stored in top_colors[].
    int top_colors[PALETTE_MAX_SIZE] = { 0 };
    for (i = 0; i < AOMMIN(colors, PALETTE_MAX_SIZE); ++i) {
      int max_count = 0;
      for (int j = 0; j < (1 << seq_params->bit_depth); ++j) {
        if (count_buf[j] > max_count) {
          max_count = count_buf[j];
          top_colors[i] = j;
        }
      }
      assert(max_count > 0);
      count_buf[top_colors[i]] = 0;
    }

    // Try the dominant colors directly.
    // TODO(huisu@google.com): Try to avoid duplicate computation in cases
    // where the dominant colors and the k-means results are similar.
    for (n = AOMMIN(colors, PALETTE_MAX_SIZE); n >= 2; --n) {
      for (i = 0; i < n; ++i) centroids[i] = top_colors[i];
      palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, n,
                   color_cache, n_cache, best_mbmi, best_palette_color_map,
                   best_rd, best_model_rd, rate, rate_tokenonly, &rate_overhead,
                   distortion, skippable, ctx, best_blk_skip);
    }

    // K-means clustering.
    for (n = AOMMIN(colors, PALETTE_MAX_SIZE); n >= 2; --n) {
      if (colors == PALETTE_MIN_SIZE) {
        // Special case: These colors automatically become the centroids.
        assert(colors == n);
        assert(colors == 2);
        centroids[0] = lb;
        centroids[1] = ub;
      } else {
        for (i = 0; i < n; ++i) {
          centroids[i] = lb + (2 * i + 1) * (ub - lb) / n / 2;
        }
        av1_k_means(data, centroids, color_map, rows * cols, n, 1, max_itr);
      }
      palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, n,
                   color_cache, n_cache, best_mbmi, best_palette_color_map,
                   best_rd, best_model_rd, rate, rate_tokenonly, &rate_overhead,
                   distortion, skippable, ctx, best_blk_skip);
    }
  }

  if (best_mbmi->palette_mode_info.palette_size[0] > 0) {
    memcpy(color_map, best_palette_color_map,
           block_width * block_height * sizeof(best_palette_color_map[0]));
  }
  *mbmi = *best_mbmi;
  return rate_overhead;
}

// Return 1 if an filter intra mode is selected; return 0 otherwise.
static int rd_pick_filter_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, int mode_cost,
                                    int64_t *best_rd, int64_t *best_model_rd,
                                    PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  int filter_intra_selected_flag = 0;
  FILTER_INTRA_MODE mode;
  TX_SIZE best_tx_size = TX_8X8;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  TX_TYPE best_txk_type[TXK_TYPE_BUF_LEN];
  (void)ctx;
  av1_zero(filter_intra_mode_info);
  mbmi->filter_intra_mode_info.use_filter_intra = 1;
  mbmi->mode = DC_PRED;
  mbmi->palette_mode_info.palette_size[0] = 0;

  for (mode = 0; mode < FILTER_INTRA_MODES; ++mode) {
    int64_t this_rd, this_model_rd;
    RD_STATS tokenonly_rd_stats;
    mbmi->filter_intra_mode_info.filter_intra_mode = mode;
    this_model_rd = intra_model_yrd(cpi, x, bsize, mode_cost);
    if (*best_model_rd != INT64_MAX &&
        this_model_rd > *best_model_rd + (*best_model_rd >> 1))
      continue;
    if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
    super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) continue;
    const int this_rate =
        tokenonly_rd_stats.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost);
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    if (this_rd < *best_rd) {
      *best_rd = this_rd;
      best_tx_size = mbmi->tx_size;
      filter_intra_mode_info = mbmi->filter_intra_mode_info;
      memcpy(best_txk_type, mbmi->txk_type,
             sizeof(best_txk_type[0]) * TXK_TYPE_BUF_LEN);
      memcpy(ctx->blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
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
    mbmi->filter_intra_mode_info = filter_intra_mode_info;
    memcpy(mbmi->txk_type, best_txk_type,
           sizeof(best_txk_type[0]) * TXK_TYPE_BUF_LEN);
    return 1;
  } else {
    return 0;
  }
}

// Run RD calculation with given luma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t calc_rd_given_intra_angle(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int mode_cost,
    int64_t best_rd_in, int8_t angle_delta, int max_angle_delta, int *rate,
    RD_STATS *rd_stats, int *best_angle_delta, TX_SIZE *best_tx_size,
    int64_t *best_rd, int64_t *best_model_rd, TX_TYPE *best_txk_type,
    uint8_t *best_blk_skip) {
  int this_rate;
  RD_STATS tokenonly_rd_stats;
  int64_t this_rd, this_model_rd;
  MB_MODE_INFO *mbmi = x->e_mbd.mi[0];
  const int n4 = bsize_to_num_blk(bsize);
  assert(!is_inter_block(mbmi));

  mbmi->angle_delta[PLANE_TYPE_Y] = angle_delta;
  this_model_rd = intra_model_yrd(cpi, x, bsize, mode_cost);
  if (*best_model_rd != INT64_MAX &&
      this_model_rd > *best_model_rd + (*best_model_rd >> 1))
    return INT64_MAX;
  if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
  super_block_yrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in);
  if (tokenonly_rd_stats.rate == INT_MAX) return INT64_MAX;

  this_rate =
      tokenonly_rd_stats.rate + mode_cost +
      x->angle_delta_cost[mbmi->mode - V_PRED]
                         [max_angle_delta + mbmi->angle_delta[PLANE_TYPE_Y]];
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

  if (this_rd < *best_rd) {
    memcpy(best_txk_type, mbmi->txk_type,
           sizeof(*best_txk_type) * TXK_TYPE_BUF_LEN);
    memcpy(best_blk_skip, x->blk_skip, sizeof(best_blk_skip[0]) * n4);
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[PLANE_TYPE_Y];
    *best_tx_size = mbmi->tx_size;
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
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  int i, angle_delta, best_angle_delta = 0;
  int first_try = 1;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];
  TX_SIZE best_tx_size = mbmi->tx_size;
  const int n4 = bsize_to_num_blk(bsize);
  TX_TYPE best_txk_type[TXK_TYPE_BUF_LEN];
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];

  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> (first_try ? 3 : 5)));
      this_rd = calc_rd_given_intra_angle(
          cpi, x, bsize, mode_cost, best_rd_in, (1 - 2 * i) * angle_delta,
          MAX_ANGLE_DELTA, rate, rd_stats, &best_angle_delta, &best_tx_size,
          &best_rd, best_model_rd, best_txk_type, best_blk_skip);
      rd_cost[2 * angle_delta + i] = this_rd;
      if (first_try && this_rd == INT64_MAX) return best_rd;
      first_try = 0;
      if (angle_delta == 0) {
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
        calc_rd_given_intra_angle(
            cpi, x, bsize, mode_cost, best_rd, (1 - 2 * i) * angle_delta,
            MAX_ANGLE_DELTA, rate, rd_stats, &best_angle_delta, &best_tx_size,
            &best_rd, best_model_rd, best_txk_type, best_blk_skip);
      }
    }
  }

  mbmi->tx_size = best_tx_size;
  mbmi->angle_delta[PLANE_TYPE_Y] = best_angle_delta;
  memcpy(mbmi->txk_type, best_txk_type,
         sizeof(*best_txk_type) * TXK_TYPE_BUF_LEN);
  memcpy(x->blk_skip, best_blk_skip, sizeof(best_blk_skip[0]) * n4);
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
  0,
};
/* clang-format on */

static void angle_estimation(const uint8_t *src, int src_stride, int rows,
                             int cols, BLOCK_SIZE bsize,
                             uint8_t *directional_mode_skip_mask) {
  memset(directional_mode_skip_mask, 0,
         INTRA_MODES * sizeof(*directional_mode_skip_mask));
  // Check if angle_delta is used
  if (!av1_use_angle_delta(bsize)) return;
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
    if (av1_is_directional_mode(i)) {
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

static void highbd_angle_estimation(const uint8_t *src8, int src_stride,
                                    int rows, int cols, BLOCK_SIZE bsize,
                                    uint8_t *directional_mode_skip_mask) {
  memset(directional_mode_skip_mask, 0,
         INTRA_MODES * sizeof(*directional_mode_skip_mask));
  // Check if angle_delta is used
  if (!av1_use_angle_delta(bsize)) return;
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
    if (av1_is_directional_mode(i)) {
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

// Given selected prediction mode, search for the best tx type and size.
static void intra_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, const int *bmode_costs,
                            int64_t *best_rd, int *rate, int *rate_tokenonly,
                            int64_t *distortion, int *skippable,
                            MB_MODE_INFO *best_mbmi, PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  RD_STATS rd_stats;
  super_block_yrd(cpi, x, &rd_stats, bsize, *best_rd);
  if (rd_stats.rate == INT_MAX) return;
  int this_rate_tokenonly = rd_stats.rate;
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->sb_type)) {
    // super_block_yrd above includes the cost of the tx_size in the
    // tokenonly rate, but for intra blocks, tx_size is always coded
    // (prediction granularity), so we account for it in the full rate,
    // not the tokenonly rate.
    this_rate_tokenonly -= tx_size_cost(&cpi->common, x, bsize, mbmi->tx_size);
  }
  const int this_rate =
      rd_stats.rate +
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode]);
  const int64_t this_rd = RDCOST(x->rdmult, this_rate, rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_mbmi = *mbmi;
    *best_rd = this_rd;
    *rate = this_rate;
    *rate_tokenonly = this_rate_tokenonly;
    *distortion = rd_stats.dist;
    *skippable = rd_stats.skip;
    memcpy(ctx->blk_skip, x->blk_skip,
           sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
  }
}

// This function is used only for intra_only frames
static int64_t rd_pick_intra_sby_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      int *rate, int *rate_tokenonly,
                                      int64_t *distortion, int *skippable,
                                      BLOCK_SIZE bsize, int64_t best_rd,
                                      PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  int64_t best_model_rd = INT64_MAX;
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  int is_directional_mode;
  uint8_t directional_mode_skip_mask[INTRA_MODES];
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *src = x->plane[0].src.buf;
  int beat_best_rd = 0;
  const int *bmode_costs;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  uint8_t *best_palette_color_map =
      try_palette ? x->palette_buffer->best_palette_color_map : NULL;
  const MB_MODE_INFO *above_mi = xd->above_mbmi;
  const MB_MODE_INFO *left_mi = xd->left_mbmi;
  const PREDICTION_MODE A = av1_above_block_mode(above_mi);
  const PREDICTION_MODE L = av1_left_block_mode(left_mi);
  const int above_ctx = intra_mode_context[A];
  const int left_ctx = intra_mode_context[L];
  bmode_costs = x->y_mode_costs[above_ctx][left_ctx];

  mbmi->angle_delta[PLANE_TYPE_Y] = 0;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    highbd_angle_estimation(src, src_stride, rows, cols, bsize,
                            directional_mode_skip_mask);
  else
    angle_estimation(src, src_stride, rows, cols, bsize,
                     directional_mode_skip_mask);
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  pmi->palette_size[0] = 0;

  if (cpi->sf.tx_type_search.fast_intra_tx_type_search)
    x->use_default_intra_tx_type = 1;
  else
    x->use_default_intra_tx_type = 0;

  MB_MODE_INFO best_mbmi = *mbmi;
  /* Y Search for intra prediction mode */
  for (int mode_idx = DC_PRED; mode_idx < INTRA_MODES; ++mode_idx) {
    RD_STATS this_rd_stats;
    int this_rate, this_rate_tokenonly, s;
    int64_t this_distortion, this_rd, this_model_rd;
    mbmi->mode = intra_rd_search_mode_order[mode_idx];
    mbmi->angle_delta[PLANE_TYPE_Y] = 0;
    this_model_rd = intra_model_yrd(cpi, x, bsize, bmode_costs[mbmi->mode]);
    if (best_model_rd != INT64_MAX &&
        this_model_rd > best_model_rd + (best_model_rd >> 1))
      continue;
    if (this_model_rd < best_model_rd) best_model_rd = this_model_rd;
    is_directional_mode = av1_is_directional_mode(mbmi->mode);
    if (is_directional_mode && directional_mode_skip_mask[mbmi->mode]) continue;
    if (is_directional_mode && av1_use_angle_delta(bsize)) {
      this_rd_stats.rate = INT_MAX;
      rd_pick_intra_angle_sby(cpi, x, &this_rate, &this_rd_stats, bsize,
                              bmode_costs[mbmi->mode], best_rd, &best_model_rd);
    } else {
      super_block_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
    }
    this_rate_tokenonly = this_rd_stats.rate;
    this_distortion = this_rd_stats.dist;
    s = this_rd_stats.skip;

    if (this_rate_tokenonly == INT_MAX) continue;

    if (!xd->lossless[mbmi->segment_id] &&
        block_signals_txsize(mbmi->sb_type)) {
      // super_block_yrd above includes the cost of the tx_size in the
      // tokenonly rate, but for intra blocks, tx_size is always coded
      // (prediction granularity), so we account for it in the full rate,
      // not the tokenonly rate.
      this_rate_tokenonly -=
          tx_size_cost(&cpi->common, x, bsize, mbmi->tx_size);
    }
    this_rate =
        this_rd_stats.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode]);
    this_rd = RDCOST(x->rdmult, this_rate, this_distortion);
    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      beat_best_rd = 1;
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_distortion;
      *skippable = s;
      memcpy(ctx->blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
    }
  }

  if (try_palette) {
    rd_pick_palette_intra_sby(cpi, x, bsize, bmode_costs[DC_PRED], &best_mbmi,
                              best_palette_color_map, &best_rd, &best_model_rd,
                              rate, rate_tokenonly, distortion, skippable, ctx,
                              ctx->blk_skip);
  }

  if (beat_best_rd && av1_filter_intra_allowed_bsize(&cpi->common, bsize)) {
    if (rd_pick_filter_intra_sby(cpi, x, rate, rate_tokenonly, distortion,
                                 skippable, bsize, bmode_costs[DC_PRED],
                                 &best_rd, &best_model_rd, ctx)) {
      best_mbmi = *mbmi;
    }
  }

  // If previous searches use only the default tx type, do an extra search for
  // the best tx type.
  if (x->use_default_intra_tx_type) {
    *mbmi = best_mbmi;
    x->use_default_intra_tx_type = 0;
    intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate, rate_tokenonly,
                    distortion, skippable, &best_mbmi, ctx);
  }

  *mbmi = best_mbmi;
  return best_rd;
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int super_block_uvrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bsize,
                            int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_U];
  const TX_SIZE uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
  int plane;
  int is_cost_valid = 1;
  av1_init_rd_stats(rd_stats);

  if (ref_best_rd < 0) is_cost_valid = 0;

  if (x->skip_chroma_rd) return is_cost_valid;

  bsize = scale_chroma_bsize(bsize, pd->subsampling_x, pd->subsampling_y);

  if (is_inter_block(mbmi) && is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane)
      av1_subtract_plane(x, bsize, plane);
  }

  if (is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
      RD_STATS pn_rd_stats;
      txfm_rd_in_plane(x, cpi, &pn_rd_stats, ref_best_rd, plane, bsize,
                       uv_tx_size, cpi->sf.use_fast_coef_costing, FTXS_NONE);
      if (pn_rd_stats.rate == INT_MAX) {
        is_cost_valid = 0;
        break;
      }
      av1_merge_rd_stats(rd_stats, &pn_rd_stats);
      if (RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) > ref_best_rd &&
          RDCOST(x->rdmult, 0, rd_stats->sse) > ref_best_rd) {
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

static void tx_block_rd_b(const AV1_COMP *cpi, MACROBLOCK *x, TX_SIZE tx_size,
                          int blk_row, int blk_col, int plane, int block,
                          int plane_bsize, const ENTROPY_CONTEXT *a,
                          const ENTROPY_CONTEXT *l, RD_STATS *rd_stats,
                          FAST_TX_SEARCH_MODE ftxs_mode, int64_t ref_rdcost,
                          TXB_RD_INFO *rd_info_array) {
  const struct macroblock_plane *const p = &x->plane[plane];
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, a, l, &txb_ctx);
  const uint16_t cur_joint_ctx =
      (txb_ctx.dc_sign_ctx << 8) + txb_ctx.txb_skip_ctx;

  const int txk_type_idx =
      av1_get_txk_type_index(plane_bsize, blk_row, blk_col);
  // Look up RD and terminate early in case when we've already processed exactly
  // the same residual with exactly the same entropy context.
  if (rd_info_array != NULL && rd_info_array->valid &&
      rd_info_array->entropy_context == cur_joint_ctx) {
    if (plane == 0)
      x->e_mbd.mi[0]->txk_type[txk_type_idx] = rd_info_array->tx_type;
    const TX_TYPE ref_tx_type =
        av1_get_tx_type(get_plane_type(plane), &x->e_mbd, blk_row, blk_col,
                        tx_size, cpi->common.reduced_tx_set_used);
    if (ref_tx_type == rd_info_array->tx_type) {
      rd_stats->rate += rd_info_array->rate;
      rd_stats->dist += rd_info_array->dist;
      rd_stats->sse += rd_info_array->sse;
      rd_stats->skip &= rd_info_array->eob == 0;
      p->eobs[block] = rd_info_array->eob;
      p->txb_entropy_ctx[block] = rd_info_array->txb_entropy_ctx;
      return;
    }
  }

  RD_STATS this_rd_stats;
  search_txk_type(cpi, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  &txb_ctx, ftxs_mode, 0, ref_rdcost, &this_rd_stats);

  av1_merge_rd_stats(rd_stats, &this_rd_stats);

  // Save RD results for possible reuse in future.
  if (rd_info_array != NULL) {
    rd_info_array->valid = 1;
    rd_info_array->entropy_context = cur_joint_ctx;
    rd_info_array->rate = this_rd_stats.rate;
    rd_info_array->dist = this_rd_stats.dist;
    rd_info_array->sse = this_rd_stats.sse;
    rd_info_array->eob = p->eobs[block];
    rd_info_array->txb_entropy_ctx = p->txb_entropy_ctx[block];
    if (plane == 0) {
      rd_info_array->tx_type = x->e_mbd.mi[0]->txk_type[txk_type_idx];
    }
  }
}

static void get_mean_and_dev(const int16_t *data, int stride, int bw, int bh,
                             float *mean, float *dev) {
  int x_sum = 0;
  uint64_t x2_sum = 0;
  for (int i = 0; i < bh; ++i) {
    for (int j = 0; j < bw; ++j) {
      const int val = data[j];
      x_sum += val;
      x2_sum += val * val;
    }
    data += stride;
  }

  const int num = bw * bh;
  const float e_x = (float)x_sum / num;
  const float e_x2 = (float)((double)x2_sum / num);
  const float diff = e_x2 - e_x * e_x;
  *dev = (diff > 0) ? sqrtf(diff) : 0;
  *mean = e_x;
}

static void get_mean_and_dev_float(const float *data, int stride, int bw,
                                   int bh, float *mean, float *dev) {
  float x_sum = 0;
  float x2_sum = 0;
  for (int i = 0; i < bh; ++i) {
    for (int j = 0; j < bw; ++j) {
      const float val = data[j];
      x_sum += val;
      x2_sum += val * val;
    }
    data += stride;
  }

  const int num = bw * bh;
  const float e_x = x_sum / num;
  const float e_x2 = x2_sum / num;
  const float diff = e_x2 - e_x * e_x;
  *dev = (diff > 0) ? sqrtf(diff) : 0;
  *mean = e_x;
}

// Feature used by the model to predict tx split: the mean and standard
// deviation values of the block and sub-blocks.
static void get_mean_dev_features(const int16_t *data, int stride, int bw,
                                  int bh, int levels, float *feature) {
  int feature_idx = 0;
  int width = bw;
  int height = bh;
  const int16_t *const data_ptr = &data[0];
  for (int lv = 0; lv < levels; ++lv) {
    if (width < 2 || height < 2) break;
    float mean_buf[16];
    float dev_buf[16];
    int blk_idx = 0;
    for (int row = 0; row < bh; row += height) {
      for (int col = 0; col < bw; col += width) {
        float mean, dev;
        get_mean_and_dev(data_ptr + row * stride + col, stride, width, height,
                         &mean, &dev);
        feature[feature_idx++] = mean;
        feature[feature_idx++] = dev;
        mean_buf[blk_idx] = mean;
        dev_buf[blk_idx++] = dev;
      }
    }
    if (blk_idx > 1) {
      float mean, dev;
      // Deviation of means.
      get_mean_and_dev_float(mean_buf, 1, 1, blk_idx, &mean, &dev);
      feature[feature_idx++] = dev;
      // Mean of deviations.
      get_mean_and_dev_float(dev_buf, 1, 1, blk_idx, &mean, &dev);
      feature[feature_idx++] = mean;
    }
    // Reduce the block size when proceeding to the next level.
    if (height == width) {
      height = height >> 1;
      width = width >> 1;
    } else if (height > width) {
      height = height >> 1;
    } else {
      width = width >> 1;
    }
  }
}

static int ml_predict_tx_split(MACROBLOCK *x, BLOCK_SIZE bsize, int blk_row,
                               int blk_col, TX_SIZE tx_size) {
  const NN_CONFIG *nn_config = av1_tx_split_nnconfig_map[tx_size];
  if (!nn_config) return -1;

  const int diff_stride = block_size_wide[bsize];
  const int16_t *diff =
      x->plane[0].src_diff + 4 * blk_row * diff_stride + 4 * blk_col;
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  aom_clear_system_state();

  float features[64] = { 0.0f };
  get_mean_dev_features(diff, diff_stride, bw, bh, 2, features);

  float score = 0.0f;
  av1_nn_predict(features, nn_config, &score);
  if (score > 8.0f) return 100;
  if (score < -8.0f) return 0;
  score = 1.0f / (1.0f + (float)exp(-score));
  return (int)(score * 100);
}

typedef struct {
  int64_t rd;
  int txb_entropy_ctx;
  TX_TYPE tx_type;
} TxCandidateInfo;

static void try_tx_block_no_split(
    const AV1_COMP *cpi, MACROBLOCK *x, int blk_row, int blk_col, int block,
    TX_SIZE tx_size, int depth, BLOCK_SIZE plane_bsize,
    const ENTROPY_CONTEXT *ta, const ENTROPY_CONTEXT *tl,
    int txfm_partition_ctx, RD_STATS *rd_stats, int64_t ref_best_rd,
    FAST_TX_SEARCH_MODE ftxs_mode, TXB_RD_INFO_NODE *rd_info_node,
    TxCandidateInfo *no_split) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  struct macroblock_plane *const p = &x->plane[0];
  const int bw = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];

  no_split->rd = INT64_MAX;
  no_split->txb_entropy_ctx = 0;
  no_split->tx_type = TX_TYPES;

  const ENTROPY_CONTEXT *const pta = ta + blk_col;
  const ENTROPY_CONTEXT *const ptl = tl + blk_row;

  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, 0, pta, ptl, &txb_ctx);
  const int zero_blk_rate = x->coeff_costs[txs_ctx][PLANE_TYPE_Y]
                                .txb_skip_cost[txb_ctx.txb_skip_ctx][1];

  rd_stats->ref_rdcost = ref_best_rd;
  rd_stats->zero_rate = zero_blk_rate;
  const int index = av1_get_txb_size_index(plane_bsize, blk_row, blk_col);
  mbmi->inter_tx_size[index] = tx_size;
  tx_block_rd_b(cpi, x, tx_size, blk_row, blk_col, 0, block, plane_bsize, pta,
                ptl, rd_stats, ftxs_mode, ref_best_rd,
                rd_info_node != NULL ? rd_info_node->rd_info_array : NULL);
  assert(rd_stats->rate < INT_MAX);

  if ((RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) >=
           RDCOST(x->rdmult, zero_blk_rate, rd_stats->sse) ||
       rd_stats->skip == 1) &&
      !xd->lossless[mbmi->segment_id]) {
#if CONFIG_RD_DEBUG
    av1_update_txb_coeff_cost(rd_stats, plane, tx_size, blk_row, blk_col,
                              zero_blk_rate - rd_stats->rate);
#endif  // CONFIG_RD_DEBUG
    rd_stats->rate = zero_blk_rate;
    rd_stats->dist = rd_stats->sse;
    rd_stats->skip = 1;
    x->blk_skip[blk_row * bw + blk_col] = 1;
    p->eobs[block] = 0;
    update_txk_array(mbmi->txk_type, plane_bsize, blk_row, blk_col, tx_size,
                     DCT_DCT);
  } else {
    x->blk_skip[blk_row * bw + blk_col] = 0;
    rd_stats->skip = 0;
  }

  if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
    rd_stats->rate += x->txfm_partition_cost[txfm_partition_ctx][0];

  no_split->rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
  no_split->txb_entropy_ctx = p->txb_entropy_ctx[block];
  const int txk_type_idx =
      av1_get_txk_type_index(plane_bsize, blk_row, blk_col);
  no_split->tx_type = mbmi->txk_type[txk_type_idx];
}

static void select_tx_block(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                            int blk_col, int block, TX_SIZE tx_size, int depth,
                            BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *ta,
                            ENTROPY_CONTEXT *tl, TXFM_CONTEXT *tx_above,
                            TXFM_CONTEXT *tx_left, RD_STATS *rd_stats,
                            int64_t ref_best_rd, int *is_cost_valid,
                            FAST_TX_SEARCH_MODE ftxs_mode,
                            TXB_RD_INFO_NODE *rd_info_node);

static void try_tx_block_split(
    const AV1_COMP *cpi, MACROBLOCK *x, int blk_row, int blk_col, int block,
    TX_SIZE tx_size, int depth, BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *ta,
    ENTROPY_CONTEXT *tl, TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
    int txfm_partition_ctx, int64_t no_split_rd, int64_t ref_best_rd,
    FAST_TX_SEARCH_MODE ftxs_mode, TXB_RD_INFO_NODE *rd_info_node,
    RD_STATS *split_rd_stats, int64_t *split_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const int max_blocks_high = max_block_high(xd, plane_bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, 0);
  struct macroblock_plane *const p = &x->plane[0];
  const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
  const int bsw = tx_size_wide_unit[sub_txs];
  const int bsh = tx_size_high_unit[sub_txs];
  const int sub_step = bsw * bsh;
  RD_STATS this_rd_stats;
  int this_cost_valid = 1;
  int64_t tmp_rd = 0;
#if CONFIG_DIST_8X8
  int sub8x8_eob[4] = { 0, 0, 0, 0 };
  struct macroblockd_plane *const pd = &xd->plane[0];
#endif
  split_rd_stats->rate = x->txfm_partition_cost[txfm_partition_ctx][1];

  assert(tx_size < TX_SIZES_ALL);

  int blk_idx = 0;
  for (int r = 0; r < tx_size_high_unit[tx_size]; r += bsh) {
    for (int c = 0; c < tx_size_wide_unit[tx_size]; c += bsw, ++blk_idx) {
      const int offsetr = blk_row + r;
      const int offsetc = blk_col + c;
      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;
      assert(blk_idx < 4);
      select_tx_block(
          cpi, x, offsetr, offsetc, block, sub_txs, depth + 1, plane_bsize, ta,
          tl, tx_above, tx_left, &this_rd_stats, ref_best_rd - tmp_rd,
          &this_cost_valid, ftxs_mode,
          (rd_info_node != NULL) ? rd_info_node->children[blk_idx] : NULL);

#if CONFIG_DIST_8X8
      if (!x->using_dist_8x8)
#endif
        if (!this_cost_valid) goto LOOP_EXIT;
#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && tx_size == TX_8X8) {
        sub8x8_eob[2 * (r / bsh) + (c / bsw)] = p->eobs[block];
      }
#endif  // CONFIG_DIST_8X8
      av1_merge_rd_stats(split_rd_stats, &this_rd_stats);

      tmp_rd = RDCOST(x->rdmult, split_rd_stats->rate, split_rd_stats->dist);
#if CONFIG_DIST_8X8
      if (!x->using_dist_8x8)
#endif
        if (no_split_rd < tmp_rd) {
          this_cost_valid = 0;
          goto LOOP_EXIT;
        }
      block += sub_step;
    }
  }

LOOP_EXIT : {}

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && this_cost_valid && tx_size == TX_8X8) {
    const int src_stride = p->src.stride;
    const int dst_stride = pd->dst.stride;

    const uint8_t *src =
        &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
    const uint8_t *dst =
        &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];

    int64_t dist_8x8;
    const int qindex = x->qindex;
    const int pred_stride = block_size_wide[plane_bsize];
    const int pred_idx = (blk_row * pred_stride + blk_col)
                         << tx_size_wide_log2[0];
    const int16_t *pred = &x->pred_luma[pred_idx];
    int i, j;
    int row, col;

    uint8_t *pred8;
    DECLARE_ALIGNED(16, uint16_t, pred8_16[8 * 8]);

    dist_8x8 = av1_dist_8x8(cpi, x, src, src_stride, dst, dst_stride, BLOCK_8X8,
                            8, 8, 8, 8, qindex) *
               16;

#ifdef DEBUG_DIST_8X8
    if (x->tune_metric == AOM_TUNE_PSNR && xd->bd == 8)
      assert(sum_rd_stats.sse == dist_8x8);
#endif  // DEBUG_DIST_8X8

    split_rd_stats->sse = dist_8x8;

    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      pred8 = CONVERT_TO_BYTEPTR(pred8_16);
    else
      pred8 = (uint8_t *)pred8_16;

    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      for (row = 0; row < 2; ++row) {
        for (col = 0; col < 2; ++col) {
          int idx = row * 2 + col;
          int eob = sub8x8_eob[idx];

          if (eob > 0) {
            for (j = 0; j < 4; j++)
              for (i = 0; i < 4; i++)
                CONVERT_TO_SHORTPTR(pred8)
                [(row * 4 + j) * 8 + 4 * col + i] =
                    pred[(row * 4 + j) * pred_stride + 4 * col + i];
          } else {
            for (j = 0; j < 4; j++)
              for (i = 0; i < 4; i++)
                CONVERT_TO_SHORTPTR(pred8)
                [(row * 4 + j) * 8 + 4 * col + i] = CONVERT_TO_SHORTPTR(
                    dst)[(row * 4 + j) * dst_stride + 4 * col + i];
          }
        }
      }
    } else {
      for (row = 0; row < 2; ++row) {
        for (col = 0; col < 2; ++col) {
          int idx = row * 2 + col;
          int eob = sub8x8_eob[idx];

          if (eob > 0) {
            for (j = 0; j < 4; j++)
              for (i = 0; i < 4; i++)
                pred8[(row * 4 + j) * 8 + 4 * col + i] =
                    (uint8_t)pred[(row * 4 + j) * pred_stride + 4 * col + i];
          } else {
            for (j = 0; j < 4; j++)
              for (i = 0; i < 4; i++)
                pred8[(row * 4 + j) * 8 + 4 * col + i] =
                    dst[(row * 4 + j) * dst_stride + 4 * col + i];
          }
        }
      }
    }
    dist_8x8 = av1_dist_8x8(cpi, x, src, src_stride, pred8, 8, BLOCK_8X8, 8, 8,
                            8, 8, qindex) *
               16;

#ifdef DEBUG_DIST_8X8
    if (x->tune_metric == AOM_TUNE_PSNR && xd->bd == 8)
      assert(sum_rd_stats.dist == dist_8x8);
#endif  // DEBUG_DIST_8X8

    split_rd_stats->dist = dist_8x8;
    tmp_rd = RDCOST(x->rdmult, split_rd_stats->rate, split_rd_stats->dist);
  }
#endif  // CONFIG_DIST_8X8
  if (this_cost_valid) *split_rd = tmp_rd;
}

// Search for the best tx partition/type for a given luma block.
static void select_tx_block(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                            int blk_col, int block, TX_SIZE tx_size, int depth,
                            BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *ta,
                            ENTROPY_CONTEXT *tl, TXFM_CONTEXT *tx_above,
                            TXFM_CONTEXT *tx_left, RD_STATS *rd_stats,
                            int64_t ref_best_rd, int *is_cost_valid,
                            FAST_TX_SEARCH_MODE ftxs_mode,
                            TXB_RD_INFO_NODE *rd_info_node) {
  assert(tx_size < TX_SIZES_ALL);
  av1_init_rd_stats(rd_stats);
  if (ref_best_rd < 0) {
    *is_cost_valid = 0;
    return;
  }

  MACROBLOCKD *const xd = &x->e_mbd;
  const int max_blocks_high = max_block_high(xd, plane_bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, 0);
  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  const int bw = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int ctx = txfm_partition_context(tx_above + blk_col, tx_left + blk_row,
                                         mbmi->sb_type, tx_size);
  struct macroblock_plane *const p = &x->plane[0];

  const int try_no_split = 1;
  int try_split = tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH;

  TxCandidateInfo no_split = { INT64_MAX, 0, TX_TYPES };

  // TX no split
  if (try_no_split) {
    try_tx_block_no_split(cpi, x, blk_row, blk_col, block, tx_size, depth,
                          plane_bsize, ta, tl, ctx, rd_stats, ref_best_rd,
                          ftxs_mode, rd_info_node, &no_split);

    if (cpi->sf.adaptive_txb_search_level &&
        (no_split.rd -
         (no_split.rd >> (1 + cpi->sf.adaptive_txb_search_level))) >
            ref_best_rd) {
      *is_cost_valid = 0;
      return;
    }

    if (cpi->sf.txb_split_cap) {
      if (p->eobs[block] == 0) try_split = 0;
    }
  }

  if (x->e_mbd.bd == 8 && !x->cb_partition_scan && try_split) {
    const int threshold = cpi->sf.tx_type_search.ml_tx_split_thresh;
    if (threshold >= 0) {
      const int split_score =
          ml_predict_tx_split(x, plane_bsize, blk_row, blk_col, tx_size);
      if (split_score >= 0 && split_score < threshold) try_split = 0;
    }
  }

#if COLLECT_TX_SIZE_DATA
  // Do not skip tx_split when collecting tx size data.
  try_split = 1;
#endif

  // TX split
  int64_t split_rd = INT64_MAX;
  RD_STATS split_rd_stats;
  av1_init_rd_stats(&split_rd_stats);
  if (try_split) {
    try_tx_block_split(cpi, x, blk_row, blk_col, block, tx_size, depth,
                       plane_bsize, ta, tl, tx_above, tx_left, ctx, no_split.rd,
                       AOMMIN(no_split.rd, ref_best_rd), ftxs_mode,
                       rd_info_node, &split_rd_stats, &split_rd);
  }

#if COLLECT_TX_SIZE_DATA
  do {
    if (tx_size <= TX_4X4 || depth >= MAX_VARTX_DEPTH) break;

#if 0
    // Randomly select blocks to collect data to reduce output file size.
    const int rnd_val = rand() % 2;
    if (rnd_val) break;
#endif

    const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
    const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
    const int within_border =
        mi_row >= xd->tile.mi_row_start &&
        (mi_row + mi_size_high[plane_bsize] < xd->tile.mi_row_end) &&
        mi_col >= xd->tile.mi_col_start &&
        (mi_col + mi_size_wide[plane_bsize] < xd->tile.mi_col_end);
    if (!within_border) break;

    FILE *fp = fopen(av1_tx_size_data_output_file, "a");
    if (!fp) break;

    // Split decision, RD cost, block type(inter/intra), q-index, rdmult,
    // and block size.
    const int split_selected = sum_rd < this_rd;
    const int is_inter = 1;
    const int txb_w = tx_size_wide[tx_size];
    const int txb_h = tx_size_high[tx_size];
    fprintf(fp, "%d,%lld,%lld,%d,%d,%d,%d,%d,", split_selected,
            (long long)this_rd, (long long)sum_rd, cpi->common.base_qindex,
            x->rdmult, is_inter, txb_w, txb_h);

    // Residue signal.
    const int diff_stride = block_size_wide[plane_bsize];
    const int16_t *src_diff =
        &p->src_diff[(blk_row * diff_stride + blk_col) * 4];
    for (int r = 0; r < txb_h; ++r) {
      for (int c = 0; c < txb_w; ++c) {
        fprintf(fp, "%d,", src_diff[c]);
      }
      src_diff += diff_stride;
    }
    fprintf(fp, "\n");

    fclose(fp);
  } while (0);
#endif  // COLLECT_TX_SIZE_DATA

  if (no_split.rd < split_rd) {
    ENTROPY_CONTEXT *pta = ta + blk_col;
    ENTROPY_CONTEXT *ptl = tl + blk_row;
    const TX_SIZE tx_size_selected = tx_size;
    p->txb_entropy_ctx[block] = no_split.txb_entropy_ctx;
    av1_set_txb_context(x, 0, block, tx_size_selected, pta, ptl);
    txfm_partition_update(tx_above + blk_col, tx_left + blk_row, tx_size,
                          tx_size);
    for (int idy = 0; idy < tx_size_high_unit[tx_size]; ++idy) {
      for (int idx = 0; idx < tx_size_wide_unit[tx_size]; ++idx) {
        const int index =
            av1_get_txb_size_index(plane_bsize, blk_row + idy, blk_col + idx);
        mbmi->inter_tx_size[index] = tx_size_selected;
      }
    }
    mbmi->tx_size = tx_size_selected;
    update_txk_array(mbmi->txk_type, plane_bsize, blk_row, blk_col, tx_size,
                     no_split.tx_type);
    x->blk_skip[blk_row * bw + blk_col] = rd_stats->skip;
  } else {
    *rd_stats = split_rd_stats;
    if (split_rd == INT64_MAX) *is_cost_valid = 0;
  }
}

static void select_inter_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                                   RD_STATS *rd_stats, BLOCK_SIZE bsize,
                                   int64_t ref_best_rd,
                                   FAST_TX_SEARCH_MODE ftxs_mode,
                                   TXB_RD_INFO_NODE *rd_info_tree) {
  MACROBLOCKD *const xd = &x->e_mbd;
  int is_cost_valid = 1;
  int64_t this_rd = 0;

  if (ref_best_rd < 0) is_cost_valid = 0;

  av1_init_rd_stats(rd_stats);

  if (is_cost_valid) {
    const struct macroblockd_plane *const pd = &xd->plane[0];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    const int mi_width = mi_size_wide[plane_bsize];
    const int mi_height = mi_size_high[plane_bsize];
    const TX_SIZE max_tx_size = max_txsize_rect_lookup[plane_bsize];
    const int bh = tx_size_high_unit[max_tx_size];
    const int bw = tx_size_wide_unit[max_tx_size];
    int idx, idy;
    int block = 0;
    int step = tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
    ENTROPY_CONTEXT ctxa[MAX_MIB_SIZE];
    ENTROPY_CONTEXT ctxl[MAX_MIB_SIZE];
    TXFM_CONTEXT tx_above[MAX_MIB_SIZE];
    TXFM_CONTEXT tx_left[MAX_MIB_SIZE];

    RD_STATS pn_rd_stats;
    const int init_depth =
        get_search_init_depth(mi_width, mi_height, 1, &cpi->sf);
    av1_init_rd_stats(&pn_rd_stats);

    av1_get_entropy_contexts(bsize, pd, ctxa, ctxl);
    memcpy(tx_above, xd->above_txfm_context, sizeof(TXFM_CONTEXT) * mi_width);
    memcpy(tx_left, xd->left_txfm_context, sizeof(TXFM_CONTEXT) * mi_height);

    for (idy = 0; idy < mi_height; idy += bh) {
      for (idx = 0; idx < mi_width; idx += bw) {
        select_tx_block(cpi, x, idy, idx, block, max_tx_size, init_depth,
                        plane_bsize, ctxa, ctxl, tx_above, tx_left,
                        &pn_rd_stats, ref_best_rd - this_rd, &is_cost_valid,
                        ftxs_mode, rd_info_tree);
        if (!is_cost_valid || pn_rd_stats.rate == INT_MAX) {
          av1_invalid_rd_stats(rd_stats);
          return;
        }
        av1_merge_rd_stats(rd_stats, &pn_rd_stats);
        this_rd +=
            AOMMIN(RDCOST(x->rdmult, pn_rd_stats.rate, pn_rd_stats.dist),
                   RDCOST(x->rdmult, pn_rd_stats.zero_rate, pn_rd_stats.sse));
        block += step;
        if (rd_info_tree != NULL) rd_info_tree += 1;
      }
    }
  }

  const int skip_ctx = av1_get_skip_context(xd);
  const int s0 = x->skip_cost[skip_ctx][0];
  const int s1 = x->skip_cost[skip_ctx][1];
  int64_t skip_rd = RDCOST(x->rdmult, s1, rd_stats->sse);
  this_rd = RDCOST(x->rdmult, rd_stats->rate + s0, rd_stats->dist);
  if (skip_rd <= this_rd) {
    this_rd = skip_rd;
    rd_stats->rate = 0;
    rd_stats->dist = rd_stats->sse;
    rd_stats->skip = 1;
  } else {
    rd_stats->skip = 0;
  }
  if (this_rd > ref_best_rd) is_cost_valid = 0;

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }
}

static int64_t select_tx_size_fix_type(const AV1_COMP *cpi, MACROBLOCK *x,
                                       RD_STATS *rd_stats, BLOCK_SIZE bsize,
                                       int64_t ref_best_rd,
                                       TXB_RD_INFO_NODE *rd_info_tree) {
  const int fast_tx_search = cpi->sf.tx_size_search_method > USE_FULL_RD;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  const int skip_ctx = av1_get_skip_context(xd);
  int s0 = x->skip_cost[skip_ctx][0];
  int s1 = x->skip_cost[skip_ctx][1];
  int64_t rd;

  // TODO(debargha): enable this as a speed feature where the
  // select_inter_block_yrd() function above will use a simplified search
  // such as not using full optimize, but the inter_block_yrd() function
  // will use more complex search given that the transform partitions have
  // already been decided.

  int64_t rd_thresh = ref_best_rd;
  if (fast_tx_search && rd_thresh < INT64_MAX) {
    if (INT64_MAX - rd_thresh > (rd_thresh >> 3)) rd_thresh += (rd_thresh >> 3);
  }
  assert(rd_thresh > 0);

  FAST_TX_SEARCH_MODE ftxs_mode =
      fast_tx_search ? FTXS_DCT_AND_1D_DCT_ONLY : FTXS_NONE;
  select_inter_block_yrd(cpi, x, rd_stats, bsize, rd_thresh, ftxs_mode,
                         rd_info_tree);
  if (rd_stats->rate == INT_MAX) return INT64_MAX;

  // If fast_tx_search is true, only DCT and 1D DCT were tested in
  // select_inter_block_yrd() above. Do a better search for tx type with
  // tx sizes already decided.
  if (fast_tx_search) {
    if (!inter_block_yrd(cpi, x, rd_stats, bsize, ref_best_rd, FTXS_NONE))
      return INT64_MAX;
  }

  if (rd_stats->skip)
    rd = RDCOST(x->rdmult, s1, rd_stats->sse);
  else
    rd = RDCOST(x->rdmult, rd_stats->rate + s0, rd_stats->dist);

  if (is_inter && !xd->lossless[xd->mi[0]->segment_id] && !(rd_stats->skip))
    rd = AOMMIN(rd, RDCOST(x->rdmult, s1, rd_stats->sse));

  return rd;
}

// Finds rd cost for a y block, given the transform size partitions
static void tx_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                         int blk_col, int block, TX_SIZE tx_size,
                         BLOCK_SIZE plane_bsize, int depth,
                         ENTROPY_CONTEXT *above_ctx, ENTROPY_CONTEXT *left_ctx,
                         TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
                         int64_t ref_best_rd, RD_STATS *rd_stats,
                         FAST_TX_SEARCH_MODE ftxs_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int max_blocks_high = max_block_high(xd, plane_bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, 0);

  assert(tx_size < TX_SIZES_ALL);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[av1_get_txb_size_index(
      plane_bsize, blk_row, blk_col)];

  int ctx = txfm_partition_context(tx_above + blk_col, tx_left + blk_row,
                                   mbmi->sb_type, tx_size);

  av1_init_rd_stats(rd_stats);
  if (tx_size == plane_tx_size) {
    ENTROPY_CONTEXT *ta = above_ctx + blk_col;
    ENTROPY_CONTEXT *tl = left_ctx + blk_row;
    const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
    TXB_CTX txb_ctx;
    get_txb_ctx(plane_bsize, tx_size, 0, ta, tl, &txb_ctx);

    const int zero_blk_rate = x->coeff_costs[txs_ctx][get_plane_type(0)]
                                  .txb_skip_cost[txb_ctx.txb_skip_ctx][1];
    rd_stats->zero_rate = zero_blk_rate;
    rd_stats->ref_rdcost = ref_best_rd;
    tx_block_rd_b(cpi, x, tx_size, blk_row, blk_col, 0, block, plane_bsize, ta,
                  tl, rd_stats, ftxs_mode, ref_best_rd, NULL);
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    if (RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) >=
            RDCOST(x->rdmult, zero_blk_rate, rd_stats->sse) ||
        rd_stats->skip == 1) {
      rd_stats->rate = zero_blk_rate;
      rd_stats->dist = rd_stats->sse;
      rd_stats->skip = 1;
      x->blk_skip[blk_row * mi_width + blk_col] = 1;
      x->plane[0].eobs[block] = 0;
      x->plane[0].txb_entropy_ctx[block] = 0;
      update_txk_array(mbmi->txk_type, plane_bsize, blk_row, blk_col, tx_size,
                       DCT_DCT);
    } else {
      rd_stats->skip = 0;
      x->blk_skip[blk_row * mi_width + blk_col] = 0;
    }
    if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
      rd_stats->rate += x->txfm_partition_cost[ctx][0];
    av1_set_txb_context(x, 0, block, tx_size, ta, tl);
    txfm_partition_update(tx_above + blk_col, tx_left + blk_row, tx_size,
                          tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    const int step = bsh * bsw;
    RD_STATS pn_rd_stats;
    int64_t this_rd = 0;
    assert(bsw > 0 && bsh > 0);

    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        const int offsetr = blk_row + row;
        const int offsetc = blk_col + col;

        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

        av1_init_rd_stats(&pn_rd_stats);
        tx_block_yrd(cpi, x, offsetr, offsetc, block, sub_txs, plane_bsize,
                     depth + 1, above_ctx, left_ctx, tx_above, tx_left,
                     ref_best_rd - this_rd, &pn_rd_stats, ftxs_mode);
        if (pn_rd_stats.rate == INT_MAX) {
          av1_invalid_rd_stats(rd_stats);
          return;
        }
        av1_merge_rd_stats(rd_stats, &pn_rd_stats);
        this_rd += RDCOST(x->rdmult, pn_rd_stats.rate, pn_rd_stats.dist);
        block += step;
      }
    }

    if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
      rd_stats->rate += x->txfm_partition_cost[ctx][1];
  }
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int inter_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                           RD_STATS *rd_stats, BLOCK_SIZE bsize,
                           int64_t ref_best_rd, FAST_TX_SEARCH_MODE ftxs_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  int is_cost_valid = 1;
  int64_t this_rd = 0;

  if (ref_best_rd < 0) is_cost_valid = 0;

  av1_init_rd_stats(rd_stats);

  if (is_cost_valid) {
    const struct macroblockd_plane *const pd = &xd->plane[0];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    const int mi_width = mi_size_wide[plane_bsize];
    const int mi_height = mi_size_high[plane_bsize];
    const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, 0);
    const int bh = tx_size_high_unit[max_tx_size];
    const int bw = tx_size_wide_unit[max_tx_size];
    const int init_depth =
        get_search_init_depth(mi_width, mi_height, 1, &cpi->sf);
    int idx, idy;
    int block = 0;
    int step = tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
    ENTROPY_CONTEXT ctxa[MAX_MIB_SIZE];
    ENTROPY_CONTEXT ctxl[MAX_MIB_SIZE];
    TXFM_CONTEXT tx_above[MAX_MIB_SIZE];
    TXFM_CONTEXT tx_left[MAX_MIB_SIZE];
    RD_STATS pn_rd_stats;

    av1_get_entropy_contexts(bsize, pd, ctxa, ctxl);
    memcpy(tx_above, xd->above_txfm_context, sizeof(TXFM_CONTEXT) * mi_width);
    memcpy(tx_left, xd->left_txfm_context, sizeof(TXFM_CONTEXT) * mi_height);

    for (idy = 0; idy < mi_height; idy += bh) {
      for (idx = 0; idx < mi_width; idx += bw) {
        av1_init_rd_stats(&pn_rd_stats);
        tx_block_yrd(cpi, x, idy, idx, block, max_tx_size, plane_bsize,
                     init_depth, ctxa, ctxl, tx_above, tx_left,
                     ref_best_rd - this_rd, &pn_rd_stats, ftxs_mode);
        if (pn_rd_stats.rate == INT_MAX) {
          av1_invalid_rd_stats(rd_stats);
          return 0;
        }
        av1_merge_rd_stats(rd_stats, &pn_rd_stats);
        this_rd +=
            AOMMIN(RDCOST(x->rdmult, pn_rd_stats.rate, pn_rd_stats.dist),
                   RDCOST(x->rdmult, pn_rd_stats.zero_rate, pn_rd_stats.sse));
        block += step;
      }
    }
  }

  const int skip_ctx = av1_get_skip_context(xd);
  const int s0 = x->skip_cost[skip_ctx][0];
  const int s1 = x->skip_cost[skip_ctx][1];
  int64_t skip_rd = RDCOST(x->rdmult, s1, rd_stats->sse);
  this_rd = RDCOST(x->rdmult, rd_stats->rate + s0, rd_stats->dist);
  if (skip_rd < this_rd) {
    this_rd = skip_rd;
    rd_stats->rate = 0;
    rd_stats->dist = rd_stats->sse;
    rd_stats->skip = 1;
  }
  if (this_rd > ref_best_rd) is_cost_valid = 0;

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }
  return is_cost_valid;
}

static INLINE uint32_t get_block_residue_hash(MACROBLOCK *x, BLOCK_SIZE bsize) {
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  const int16_t *diff = x->plane[0].src_diff;
  const uint32_t hash = av1_get_crc32c_value(&x->mb_rd_record.crc_calculator,
                                             (uint8_t *)diff, 2 * rows * cols);
  return (hash << 5) + bsize;
}

static void save_tx_rd_info(int n4, uint32_t hash, const MACROBLOCK *const x,
                            const RD_STATS *const rd_stats,
                            MB_RD_RECORD *tx_rd_record) {
  int index;
  if (tx_rd_record->num < RD_RECORD_BUFFER_LEN) {
    index =
        (tx_rd_record->index_start + tx_rd_record->num) % RD_RECORD_BUFFER_LEN;
    ++tx_rd_record->num;
  } else {
    index = tx_rd_record->index_start;
    tx_rd_record->index_start =
        (tx_rd_record->index_start + 1) % RD_RECORD_BUFFER_LEN;
  }
  MB_RD_INFO *const tx_rd_info = &tx_rd_record->tx_rd_info[index];
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  tx_rd_info->hash_value = hash;
  tx_rd_info->tx_size = mbmi->tx_size;
  memcpy(tx_rd_info->blk_skip, x->blk_skip,
         sizeof(tx_rd_info->blk_skip[0]) * n4);
  av1_copy(tx_rd_info->inter_tx_size, mbmi->inter_tx_size);
  av1_copy(tx_rd_info->txk_type, mbmi->txk_type);
  tx_rd_info->rd_stats = *rd_stats;
}

static void fetch_tx_rd_info(int n4, const MB_RD_INFO *const tx_rd_info,
                             RD_STATS *const rd_stats, MACROBLOCK *const x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  mbmi->tx_size = tx_rd_info->tx_size;
  memcpy(x->blk_skip, tx_rd_info->blk_skip,
         sizeof(tx_rd_info->blk_skip[0]) * n4);
  av1_copy(mbmi->inter_tx_size, tx_rd_info->inter_tx_size);
  av1_copy(mbmi->txk_type, tx_rd_info->txk_type);
  *rd_stats = tx_rd_info->rd_stats;
}

static int find_tx_size_rd_info(TXB_RD_RECORD *cur_record,
                                const uint32_t hash) {
  // Linear search through the circular buffer to find matching hash.
  int index;
  for (int i = cur_record->num - 1; i >= 0; i--) {
    index = (cur_record->index_start + i) % TX_SIZE_RD_RECORD_BUFFER_LEN;
    if (cur_record->hash_vals[index] == hash) return index;
  }

  // If not found - add new RD info into the buffer and return its index
  if (cur_record->num < TX_SIZE_RD_RECORD_BUFFER_LEN) {
    index = (cur_record->index_start + cur_record->num) %
            TX_SIZE_RD_RECORD_BUFFER_LEN;
    cur_record->num++;
  } else {
    index = cur_record->index_start;
    cur_record->index_start =
        (cur_record->index_start + 1) % TX_SIZE_RD_RECORD_BUFFER_LEN;
  }

  cur_record->hash_vals[index] = hash;
  av1_zero(cur_record->tx_rd_info[index]);
  return index;
}

// Go through all TX blocks that could be used in TX size search, compute
// residual hash values for them and find matching RD info that stores previous
// RD search results for these TX blocks. The idea is to prevent repeated
// rate/distortion computations that happen because of the combination of
// partition and TX size search. The resulting RD info records are returned in
// the form of a quadtree for easier access in actual TX size search.
static int find_tx_size_rd_records(MACROBLOCK *x, BLOCK_SIZE bsize, int mi_row,
                                   int mi_col, TXB_RD_INFO_NODE *dst_rd_info) {
  TXB_RD_RECORD *rd_records_table[4] = { x->txb_rd_record_8X8,
                                         x->txb_rd_record_16X16,
                                         x->txb_rd_record_32X32,
                                         x->txb_rd_record_64X64 };
  const TX_SIZE max_square_tx_size = max_txsize_lookup[bsize];
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];

  // Hashing is performed only for square TX sizes larger than TX_4X4
  if (max_square_tx_size < TX_8X8) return 0;

  const int bw_mi = mi_size_wide[bsize];
  const int diff_stride = bw;
  const struct macroblock_plane *const p = &x->plane[0];
  const int16_t *diff = &p->src_diff[0];

  // Coordinates of the top-left corner of current block within the superblock
  // measured in pixels:
  const int mi_row_in_sb = (mi_row % MAX_MIB_SIZE) << MI_SIZE_LOG2;
  const int mi_col_in_sb = (mi_col % MAX_MIB_SIZE) << MI_SIZE_LOG2;
  int cur_rd_info_idx = 0;
  int cur_tx_depth = 0;
  uint8_t parent_idx_buf[MAX_MIB_SIZE * MAX_MIB_SIZE] = { 0 };
  uint8_t child_idx_buf[MAX_MIB_SIZE * MAX_MIB_SIZE] = { 0 };
  TX_SIZE cur_tx_size = max_txsize_rect_lookup[bsize];
  while (cur_tx_depth <= MAX_VARTX_DEPTH) {
    const int cur_tx_bw = tx_size_wide[cur_tx_size];
    const int cur_tx_bh = tx_size_high[cur_tx_size];
    if (cur_tx_bw < 8 || cur_tx_bh < 8) break;
    const TX_SIZE next_tx_size = sub_tx_size_map[cur_tx_size];
    for (int row = 0; row < bh; row += cur_tx_bh) {
      for (int col = 0; col < bw; col += cur_tx_bw) {
        if (cur_tx_bw != cur_tx_bh) {
          // Use dummy nodes for all rectangular transforms within the
          // TX size search tree.
          dst_rd_info[cur_rd_info_idx].rd_info_array = NULL;
        } else {
          // Get spatial location of this TX block within the superblock
          // (measured in cur_tx_bsize units).
          const int row_in_sb = (mi_row_in_sb + row) / cur_tx_bh;
          const int col_in_sb = (mi_col_in_sb + col) / cur_tx_bw;

          int16_t hash_data[MAX_SB_SQUARE];
          int16_t *cur_hash_row = hash_data;
          const int16_t *cur_diff_row = diff + row * diff_stride + col;
          for (int i = 0; i < cur_tx_bh; i++) {
            memcpy(cur_hash_row, cur_diff_row, sizeof(*hash_data) * cur_tx_bw);
            cur_hash_row += cur_tx_bw;
            cur_diff_row += diff_stride;
          }
          const int hash = av1_get_crc32c_value(&x->mb_rd_record.crc_calculator,
                                                (uint8_t *)hash_data,
                                                2 * cur_tx_bw * cur_tx_bh);

          // Find corresponding RD info based on the hash value.
          const int rd_record_idx =
              row_in_sb * (MAX_MIB_SIZE >> (cur_tx_size + 1 - TX_8X8)) +
              col_in_sb;

          int idx = find_tx_size_rd_info(
              &rd_records_table[cur_tx_size - TX_8X8][rd_record_idx], hash);
          dst_rd_info[cur_rd_info_idx].rd_info_array =
              &rd_records_table[cur_tx_size - TX_8X8][rd_record_idx]
                   .tx_rd_info[idx];
        }

        // Update the output quadtree RD info structure.
        av1_zero(dst_rd_info[cur_rd_info_idx].children);
        const int this_mi_row = row / MI_SIZE;
        const int this_mi_col = col / MI_SIZE;
        if (cur_tx_depth > 0) {  // Set up child pointers.
          const int mi_index = this_mi_row * bw_mi + this_mi_col;
          const int child_idx = child_idx_buf[mi_index];
          assert(child_idx < 4);
          dst_rd_info[parent_idx_buf[mi_index]].children[child_idx] =
              &dst_rd_info[cur_rd_info_idx];
        }
        if (cur_tx_depth < MAX_VARTX_DEPTH) {  // Set up parent and child idx.
          const int tx_bh_mi = cur_tx_bh / MI_SIZE;
          const int tx_bw_mi = cur_tx_bw / MI_SIZE;
          for (int i = this_mi_row; i < this_mi_row + tx_bh_mi; ++i) {
            memset(parent_idx_buf + i * bw_mi + this_mi_col, cur_rd_info_idx,
                   tx_bw_mi);
          }
          int child_idx = 0;
          const int next_tx_bh_mi = tx_size_wide_unit[next_tx_size];
          const int next_tx_bw_mi = tx_size_wide_unit[next_tx_size];
          for (int i = this_mi_row; i < this_mi_row + tx_bh_mi;
               i += next_tx_bh_mi) {
            for (int j = this_mi_col; j < this_mi_col + tx_bw_mi;
                 j += next_tx_bw_mi) {
              assert(child_idx < 4);
              child_idx_buf[i * bw_mi + j] = child_idx++;
            }
          }
        }
        ++cur_rd_info_idx;
      }
    }
    cur_tx_size = next_tx_size;
    ++cur_tx_depth;
  }
  return 1;
}

// origin_threshold * 128 / 100
static const uint32_t skip_pred_threshold[3][BLOCK_SIZES_ALL] = {
  {
      64, 64, 64, 70, 60, 60, 68, 68, 68, 68, 68,
      68, 68, 68, 68, 68, 64, 64, 70, 70, 68, 68,
  },
  {
      88, 88, 88, 86, 87, 87, 68, 68, 68, 68, 68,
      68, 68, 68, 68, 68, 88, 88, 86, 86, 68, 68,
  },
  {
      90, 93, 93, 90, 93, 93, 74, 74, 74, 74, 74,
      74, 74, 74, 74, 74, 90, 90, 90, 90, 74, 74,
  },
};

// lookup table for predict_skip_flag
// int max_tx_size = max_txsize_rect_lookup[bsize];
// if (tx_size_high[max_tx_size] > 16 || tx_size_wide[max_tx_size] > 16)
//   max_tx_size = AOMMIN(max_txsize_lookup[bsize], TX_16X16);
static const TX_SIZE max_predict_sf_tx_size[BLOCK_SIZES_ALL] = {
  TX_4X4,   TX_4X8,   TX_8X4,   TX_8X8,   TX_8X16,  TX_16X8,
  TX_16X16, TX_16X16, TX_16X16, TX_16X16, TX_16X16, TX_16X16,
  TX_16X16, TX_16X16, TX_16X16, TX_16X16, TX_4X16,  TX_16X4,
  TX_8X8,   TX_8X8,   TX_16X16, TX_16X16,
};

// Uses simple features on top of DCT coefficients to quickly predict
// whether optimal RD decision is to skip encoding the residual.
// The sse value is stored in dist.
static int predict_skip_flag(MACROBLOCK *x, BLOCK_SIZE bsize, int64_t *dist,
                             int reduced_tx_set) {
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const MACROBLOCKD *xd = &x->e_mbd;
  const int16_t dc_q = av1_dc_quant_QTX(x->qindex, 0, xd->bd);

  *dist = pixel_diff_dist(x, 0, 0, 0, bsize, bsize, 1);
  const int64_t mse = *dist / bw / bh;
  // Normalized quantizer takes the transform upscaling factor (8 for tx size
  // smaller than 32) into account.
  const int16_t normalized_dc_q = dc_q >> 3;
  const int64_t mse_thresh = (int64_t)normalized_dc_q * normalized_dc_q / 8;
  // Predict not to skip when mse is larger than threshold.
  if (mse > mse_thresh) return 0;

  const int max_tx_size = max_predict_sf_tx_size[bsize];
  const int tx_h = tx_size_high[max_tx_size];
  const int tx_w = tx_size_wide[max_tx_size];
  DECLARE_ALIGNED(32, tran_low_t, coefs[32 * 32]);
  TxfmParam param;
  param.tx_type = DCT_DCT;
  param.tx_size = max_tx_size;
  param.bd = xd->bd;
  param.is_hbd = get_bitdepth_data_path_index(xd);
  param.lossless = 0;
  param.tx_set_type = av1_get_ext_tx_set_type(
      param.tx_size, is_inter_block(xd->mi[0]), reduced_tx_set);
  const int bd_idx = (xd->bd == 8) ? 0 : ((xd->bd == 10) ? 1 : 2);
  const uint32_t max_qcoef_thresh = skip_pred_threshold[bd_idx][bsize];
  const int16_t *src_diff = x->plane[0].src_diff;
  const int n_coeff = tx_w * tx_h;
  const int16_t ac_q = av1_ac_quant_QTX(x->qindex, 0, xd->bd);
  const uint32_t dc_thresh = max_qcoef_thresh * dc_q;
  const uint32_t ac_thresh = max_qcoef_thresh * ac_q;
  for (int row = 0; row < bh; row += tx_h) {
    for (int col = 0; col < bw; col += tx_w) {
      av1_fwd_txfm(src_diff + col, coefs, bw, &param);
      // Operating on TX domain, not pixels; we want the QTX quantizers
      const uint32_t dc_coef = (((uint32_t)abs(coefs[0])) << 7);
      if (dc_coef >= dc_thresh) return 0;
      for (int i = 1; i < n_coeff; ++i) {
        const uint32_t ac_coef = (((uint32_t)abs(coefs[i])) << 7);
        if (ac_coef >= ac_thresh) return 0;
      }
    }
    src_diff += tx_h * bw;
  }
  return 1;
}

// Used to set proper context for early termination with skip = 1.
static void set_skip_flag(MACROBLOCK *x, RD_STATS *rd_stats, int bsize,
                          int64_t dist) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int n4 = bsize_to_num_blk(bsize);
  const TX_SIZE tx_size = max_txsize_rect_lookup[bsize];
  memset(mbmi->txk_type, DCT_DCT, sizeof(mbmi->txk_type[0]) * TXK_TYPE_BUF_LEN);
  memset(mbmi->inter_tx_size, tx_size, sizeof(mbmi->inter_tx_size));
  mbmi->tx_size = tx_size;
  memset(x->blk_skip, 1, sizeof(x->blk_skip[0]) * n4);
  rd_stats->skip = 1;
  rd_stats->rate = 0;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    dist = ROUND_POWER_OF_TWO(dist, (xd->bd - 8) * 2);
  rd_stats->dist = rd_stats->sse = (dist << 4);
}

static void select_tx_type_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                               RD_STATS *rd_stats, BLOCK_SIZE bsize, int mi_row,
                               int mi_col, int64_t ref_best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int64_t rd = INT64_MAX;
  int64_t best_rd = INT64_MAX;
  const int is_inter = is_inter_block(mbmi);
  const int n4 = bsize_to_num_blk(bsize);
  // Get the tx_size 1 level down
  const TX_SIZE min_tx_size = sub_tx_size_map[max_txsize_rect_lookup[bsize]];
  const TxSetType tx_set_type =
      av1_get_ext_tx_set_type(min_tx_size, is_inter, cm->reduced_tx_set_used);
  const int within_border =
      mi_row >= xd->tile.mi_row_start &&
      (mi_row + mi_size_high[bsize] < xd->tile.mi_row_end) &&
      mi_col >= xd->tile.mi_col_start &&
      (mi_col + mi_size_wide[bsize] < xd->tile.mi_col_end);

  av1_invalid_rd_stats(rd_stats);

  if (cpi->sf.model_based_prune_tx_search_level && ref_best_rd != INT64_MAX) {
    int model_rate;
    int64_t model_dist;
    int model_skip;
    model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &model_rate, &model_dist,
                    &model_skip, NULL, NULL, NULL, NULL);
    const int64_t model_rd = RDCOST(x->rdmult, model_rate, model_dist);
    // If the modeled rd is a lot worse than the best so far, breakout.
    // TODO(debargha, urvang): Improve the model and make the check below
    // tighter.
    assert(cpi->sf.model_based_prune_tx_search_level >= 0 &&
           cpi->sf.model_based_prune_tx_search_level <= 2);
    if (!model_skip &&
        model_rd / (5 - cpi->sf.model_based_prune_tx_search_level) >
            ref_best_rd)
      return;
  }

  const uint32_t hash = get_block_residue_hash(x, bsize);
  MB_RD_RECORD *mb_rd_record = &x->mb_rd_record;

  if (ref_best_rd != INT64_MAX && within_border && cpi->sf.use_mb_rd_hash) {
    for (int i = 0; i < mb_rd_record->num; ++i) {
      const int index = (mb_rd_record->index_start + i) % RD_RECORD_BUFFER_LEN;
      // If there is a match in the tx_rd_record, fetch the RD decision and
      // terminate early.
      if (mb_rd_record->tx_rd_info[index].hash_value == hash) {
        MB_RD_INFO *tx_rd_info = &mb_rd_record->tx_rd_info[index];
        fetch_tx_rd_info(n4, tx_rd_info, rd_stats, x);
        return;
      }
    }
  }

  // If we predict that skip is the optimal RD decision - set the respective
  // context and terminate early.
  int64_t dist;
  if (is_inter && cpi->sf.tx_type_search.use_skip_flag_prediction &&
      predict_skip_flag(x, bsize, &dist, cm->reduced_tx_set_used)) {
    set_skip_flag(x, rd_stats, bsize, dist);
    // Save the RD search results into tx_rd_record.
    if (within_border) save_tx_rd_info(n4, hash, x, rd_stats, mb_rd_record);
    return;
  }

  // Precompute residual hashes and find existing or add new RD records to
  // store and reuse rate and distortion values to speed up TX size search.
  TXB_RD_INFO_NODE matched_rd_info[16 + 64 + 256];
  int found_rd_info = 0;
  if (ref_best_rd != INT64_MAX && within_border && cpi->sf.use_inter_txb_hash) {
    found_rd_info =
        find_tx_size_rd_records(x, bsize, mi_row, mi_col, matched_rd_info);
  }

  prune_tx(cpi, bsize, x, xd, tx_set_type);

  int found = 0;

  RD_STATS this_rd_stats;
  av1_init_rd_stats(&this_rd_stats);

  rd = select_tx_size_fix_type(cpi, x, &this_rd_stats, bsize, ref_best_rd,
                               found_rd_info ? matched_rd_info : NULL);
  assert(IMPLIES(this_rd_stats.skip && !this_rd_stats.invalid_rate,
                 this_rd_stats.rate == 0));

  ref_best_rd = AOMMIN(rd, ref_best_rd);
  if (rd < best_rd) {
    *rd_stats = this_rd_stats;
    found = 1;
  }

  // Reset the pruning flags.
  av1_zero(x->tx_search_prune);
  x->tx_split_prune_flag = 0;

  // We should always find at least one candidate unless ref_best_rd is less
  // than INT64_MAX (in which case, all the calls to select_tx_size_fix_type
  // might have failed to find something better)
  assert(IMPLIES(!found, ref_best_rd != INT64_MAX));
  if (!found) return;

  // Save the RD search results into tx_rd_record.
  if (within_border && cpi->sf.use_mb_rd_hash)
    save_tx_rd_info(n4, hash, x, rd_stats, mb_rd_record);
}

static void tx_block_uvrd(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                          int blk_col, int plane, int block, TX_SIZE tx_size,
                          BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *above_ctx,
                          ENTROPY_CONTEXT *left_ctx, RD_STATS *rd_stats,
                          FAST_TX_SEARCH_MODE ftxs_mode) {
  assert(plane > 0);
  assert(tx_size < TX_SIZES_ALL);
  MACROBLOCKD *const xd = &x->e_mbd;
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  ENTROPY_CONTEXT *ta = above_ctx + blk_col;
  ENTROPY_CONTEXT *tl = left_ctx + blk_row;
  tx_block_rd_b(cpi, x, tx_size, blk_row, blk_col, plane, block, plane_bsize,
                ta, tl, rd_stats, ftxs_mode, INT64_MAX, NULL);
  av1_set_txb_context(x, plane, block, tx_size, ta, tl);
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int inter_block_uvrd(const AV1_COMP *cpi, MACROBLOCK *x,
                            RD_STATS *rd_stats, BLOCK_SIZE bsize,
                            int64_t ref_best_rd,
                            FAST_TX_SEARCH_MODE ftxs_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int plane;
  int is_cost_valid = 1;
  int64_t this_rd = 0;

  if (ref_best_rd < 0) is_cost_valid = 0;

  av1_init_rd_stats(rd_stats);

  if (x->skip_chroma_rd) return is_cost_valid;
  const BLOCK_SIZE bsizec = scale_chroma_bsize(
      bsize, xd->plane[1].subsampling_x, xd->plane[1].subsampling_y);

  if (is_inter_block(mbmi) && is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane)
      av1_subtract_plane(x, bsizec, plane);
  }

  if (is_cost_valid) {
    for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const BLOCK_SIZE plane_bsize =
          get_plane_block_size(bsizec, pd->subsampling_x, pd->subsampling_y);
      const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
      const int mi_height =
          block_size_high[plane_bsize] >> tx_size_high_log2[0];
      const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, plane);
      const int bh = tx_size_high_unit[max_tx_size];
      const int bw = tx_size_wide_unit[max_tx_size];
      int idx, idy;
      int block = 0;
      const int step = bh * bw;
      ENTROPY_CONTEXT ta[MAX_MIB_SIZE];
      ENTROPY_CONTEXT tl[MAX_MIB_SIZE];
      RD_STATS pn_rd_stats;
      av1_init_rd_stats(&pn_rd_stats);
      av1_get_entropy_contexts(bsizec, pd, ta, tl);

      for (idy = 0; idy < mi_height; idy += bh) {
        for (idx = 0; idx < mi_width; idx += bw) {
          tx_block_uvrd(cpi, x, idy, idx, plane, block, max_tx_size,
                        plane_bsize, ta, tl, &pn_rd_stats, ftxs_mode);
          block += step;
        }
      }

      if (pn_rd_stats.rate == INT_MAX) {
        is_cost_valid = 0;
        break;
      }

      av1_merge_rd_stats(rd_stats, &pn_rd_stats);

      this_rd = AOMMIN(RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist),
                       RDCOST(x->rdmult, rd_stats->zero_rate, rd_stats->sse));

      if (this_rd > ref_best_rd) {
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

static void rd_pick_palette_intra_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int dc_mode_cost,
                                       uint8_t *best_palette_color_map,
                                       MB_MODE_INFO *const best_mbmi,
                                       int64_t *best_rd, int *rate,
                                       int *rate_tokenonly, int64_t *distortion,
                                       int *skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  assert(
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type));
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const SequenceHeader *const seq_params = &cpi->common.seq_params;
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

  mbmi->uv_mode = UV_DC_PRED;

  int count_buf[1 << 12];  // Maximum (1 << 12) color levels.
  if (seq_params->use_highbitdepth) {
    colors_u = av1_count_colors_highbd(src_u, src_stride, rows, cols,
                                       seq_params->bit_depth, count_buf);
    colors_v = av1_count_colors_highbd(src_v, src_stride, rows, cols,
                                       seq_params->bit_depth, count_buf);
  } else {
    colors_u = av1_count_colors(src_u, src_stride, rows, cols, count_buf);
    colors_v = av1_count_colors(src_v, src_stride, rows, cols, count_buf);
  }

  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 1, color_cache);

  colors = colors_u > colors_v ? colors_u : colors_v;
  if (colors > 1 && colors <= 64) {
    int r, c, n, i, j;
    const int max_itr = 50;
    int lb_u, ub_u, val_u;
    int lb_v, ub_v, val_v;
    int *const data = x->palette_buffer->kmeans_data_buf;
    int centroids[2 * PALETTE_MAX_SIZE];

    uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src_u);
    uint16_t *src_v16 = CONVERT_TO_SHORTPTR(src_v);
    if (seq_params->use_highbitdepth) {
      lb_u = src_u16[0];
      ub_u = src_u16[0];
      lb_v = src_v16[0];
      ub_v = src_v16[0];
    } else {
      lb_u = src_u[0];
      ub_u = src_u[0];
      lb_v = src_v[0];
      ub_v = src_v[0];
    }

    for (r = 0; r < rows; ++r) {
      for (c = 0; c < cols; ++c) {
        if (seq_params->use_highbitdepth) {
          val_u = src_u16[r * src_stride + c];
          val_v = src_v16[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        } else {
          val_u = src_u[r * src_stride + c];
          val_v = src_v[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        }
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
      optimize_palette_colors(color_cache, n_cache, n, 2, centroids);
      // Sort the U channel colors in ascending order.
      for (i = 0; i < 2 * (n - 1); i += 2) {
        int min_idx = i;
        int min_val = centroids[i];
        for (j = i + 2; j < 2 * n; j += 2)
          if (centroids[j] < min_val) min_val = centroids[j], min_idx = j;
        if (min_idx != i) {
          int temp_u = centroids[i], temp_v = centroids[i + 1];
          centroids[i] = centroids[min_idx];
          centroids[i + 1] = centroids[min_idx + 1];
          centroids[min_idx] = temp_u, centroids[min_idx + 1] = temp_v;
        }
      }
      av1_calc_indices(data, centroids, color_map, rows * cols, n, 2);
      extend_palette_color_map(color_map, cols, rows, plane_block_width,
                               plane_block_height);
      pmi->palette_size[1] = n;
      for (i = 1; i < 3; ++i) {
        for (j = 0; j < n; ++j) {
          if (seq_params->use_highbitdepth)
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] = clip_pixel_highbd(
                (int)centroids[j * 2 + i - 1], seq_params->bit_depth);
          else
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] =
                clip_pixel((int)centroids[j * 2 + i - 1]);
        }
      }

      super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
      if (tokenonly_rd_stats.rate == INT_MAX) continue;
      this_rate = tokenonly_rd_stats.rate +
                  intra_mode_info_cost_uv(cpi, x, mbmi, bsize, dc_mode_cost);
      this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
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
           plane_block_width * plane_block_height *
               sizeof(best_palette_color_map[0]));
  }
}

// Run RD calculation with given chroma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t pick_intra_angle_routine_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int rate_overhead, int64_t best_rd_in, int *rate, RD_STATS *rd_stats,
    int *best_angle_delta, int64_t *best_rd) {
  MB_MODE_INFO *mbmi = x->e_mbd.mi[0];
  assert(!is_inter_block(mbmi));
  int this_rate;
  int64_t this_rd;
  RD_STATS tokenonly_rd_stats;

  if (!super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in))
    return INT64_MAX;
  this_rate = tokenonly_rd_stats.rate +
              intra_mode_info_cost_uv(cpi, x, mbmi, bsize, rate_overhead);
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
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
  MB_MODE_INFO *mbmi = xd->mi[0];
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
      mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
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
        mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
        pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                      rate, rd_stats, &best_angle_delta,
                                      &best_rd);
      }
    }
  }

  mbmi->angle_delta[PLANE_TYPE_UV] = best_angle_delta;
  return rd_stats->rate != INT_MAX;
}

#define PLANE_SIGN_TO_JOINT_SIGN(plane, a, b) \
  (plane == CFL_PRED_U ? a * CFL_SIGNS + b - 1 : b * CFL_SIGNS + a - 1)
static int cfl_rd_pick_alpha(MACROBLOCK *const x, const AV1_COMP *const cpi,
                             TX_SIZE tx_size, int64_t best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];

  const BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_DEBUG
  assert(is_cfl_allowed(xd));
  const int ssx = xd->plane[AOM_PLANE_U].subsampling_x;
  const int ssy = xd->plane[AOM_PLANE_U].subsampling_y;
  const BLOCK_SIZE plane_bsize = get_plane_block_size(mbmi->sb_type, ssx, ssy);
  (void)plane_bsize;
  assert(plane_bsize < BLOCK_SIZES_ALL);
  if (!xd->lossless[mbmi->segment_id]) {
    assert(block_size_wide[plane_bsize] == tx_size_wide[tx_size]);
    assert(block_size_high[plane_bsize] == tx_size_high[tx_size]);
  }
#endif  // CONFIG_DEBUG

  xd->cfl.use_dc_pred_cache = 1;
  const int64_t mode_rd =
      RDCOST(x->rdmult,
             x->intra_uv_mode_cost[CFL_ALLOWED][mbmi->mode][UV_CFL_PRED], 0);
  int64_t best_rd_uv[CFL_JOINT_SIGNS][CFL_PRED_PLANES];
  int best_c[CFL_JOINT_SIGNS][CFL_PRED_PLANES];
#if CONFIG_DEBUG
  int best_rate_uv[CFL_JOINT_SIGNS][CFL_PRED_PLANES];
#endif  // CONFIG_DEBUG

  for (int plane = 0; plane < CFL_PRED_PLANES; plane++) {
    RD_STATS rd_stats;
    av1_init_rd_stats(&rd_stats);
    for (int joint_sign = 0; joint_sign < CFL_JOINT_SIGNS; joint_sign++) {
      best_rd_uv[joint_sign][plane] = INT64_MAX;
      best_c[joint_sign][plane] = 0;
    }
    // Collect RD stats for an alpha value of zero in this plane.
    // Skip i == CFL_SIGN_ZERO as (0, 0) is invalid.
    for (int i = CFL_SIGN_NEG; i < CFL_SIGNS; i++) {
      const int joint_sign = PLANE_SIGN_TO_JOINT_SIGN(plane, CFL_SIGN_ZERO, i);
      if (i == CFL_SIGN_NEG) {
        mbmi->cfl_alpha_idx = 0;
        mbmi->cfl_alpha_signs = joint_sign;
        txfm_rd_in_plane(x, cpi, &rd_stats, best_rd, plane + 1, bsize, tx_size,
                         cpi->sf.use_fast_coef_costing, FTXS_NONE);
        if (rd_stats.rate == INT_MAX) break;
      }
      const int alpha_rate = x->cfl_cost[joint_sign][plane][0];
      best_rd_uv[joint_sign][plane] =
          RDCOST(x->rdmult, rd_stats.rate + alpha_rate, rd_stats.dist);
#if CONFIG_DEBUG
      best_rate_uv[joint_sign][plane] = rd_stats.rate;
#endif  // CONFIG_DEBUG
    }
  }

  int best_joint_sign = -1;

  for (int plane = 0; plane < CFL_PRED_PLANES; plane++) {
    for (int pn_sign = CFL_SIGN_NEG; pn_sign < CFL_SIGNS; pn_sign++) {
      int progress = 0;
      for (int c = 0; c < CFL_ALPHABET_SIZE; c++) {
        int flag = 0;
        RD_STATS rd_stats;
        if (c > 2 && progress < c) break;
        av1_init_rd_stats(&rd_stats);
        for (int i = 0; i < CFL_SIGNS; i++) {
          const int joint_sign = PLANE_SIGN_TO_JOINT_SIGN(plane, pn_sign, i);
          if (i == 0) {
            mbmi->cfl_alpha_idx = (c << CFL_ALPHABET_SIZE_LOG2) + c;
            mbmi->cfl_alpha_signs = joint_sign;
            txfm_rd_in_plane(x, cpi, &rd_stats, best_rd, plane + 1, bsize,
                             tx_size, cpi->sf.use_fast_coef_costing, FTXS_NONE);
            if (rd_stats.rate == INT_MAX) break;
          }
          const int alpha_rate = x->cfl_cost[joint_sign][plane][c];
          int64_t this_rd =
              RDCOST(x->rdmult, rd_stats.rate + alpha_rate, rd_stats.dist);
          if (this_rd >= best_rd_uv[joint_sign][plane]) continue;
          best_rd_uv[joint_sign][plane] = this_rd;
          best_c[joint_sign][plane] = c;
#if CONFIG_DEBUG
          best_rate_uv[joint_sign][plane] = rd_stats.rate;
#endif  // CONFIG_DEBUG
          flag = 2;
          if (best_rd_uv[joint_sign][!plane] == INT64_MAX) continue;
          this_rd += mode_rd + best_rd_uv[joint_sign][!plane];
          if (this_rd >= best_rd) continue;
          best_rd = this_rd;
          best_joint_sign = joint_sign;
        }
        progress += flag;
      }
    }
  }

  int best_rate_overhead = INT_MAX;
  int ind = 0;
  if (best_joint_sign >= 0) {
    const int u = best_c[best_joint_sign][CFL_PRED_U];
    const int v = best_c[best_joint_sign][CFL_PRED_V];
    ind = (u << CFL_ALPHABET_SIZE_LOG2) + v;
    best_rate_overhead = x->cfl_cost[best_joint_sign][CFL_PRED_U][u] +
                         x->cfl_cost[best_joint_sign][CFL_PRED_V][v];
#if CONFIG_DEBUG
    xd->cfl.rate = x->intra_uv_mode_cost[CFL_ALLOWED][mbmi->mode][UV_CFL_PRED] +
                   best_rate_overhead +
                   best_rate_uv[best_joint_sign][CFL_PRED_U] +
                   best_rate_uv[best_joint_sign][CFL_PRED_V];
#endif  // CONFIG_DEBUG
  } else {
    best_joint_sign = 0;
  }

  mbmi->cfl_alpha_idx = ind;
  mbmi->cfl_alpha_signs = best_joint_sign;
  xd->cfl.use_dc_pred_cache = 0;
  xd->cfl.dc_pred_is_cached[0] = 0;
  xd->cfl.dc_pred_is_cached[1] = 0;
  return best_rate_overhead;
}

static void init_sbuv_mode(MB_MODE_INFO *const mbmi) {
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->palette_mode_info.palette_size[1] = 0;
}

static int64_t rd_pick_intra_sbuv_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, int *rate_tokenonly,
                                       int64_t *distortion, int *skippable,
                                       BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  MB_MODE_INFO best_mbmi = *mbmi;
  int64_t best_rd = INT64_MAX, this_rd;

  for (int mode_idx = 0; mode_idx < UV_INTRA_MODES; ++mode_idx) {
    int this_rate;
    RD_STATS tokenonly_rd_stats;
    UV_PREDICTION_MODE mode = uv_rd_search_mode_order[mode_idx];
    const int is_directional_mode = av1_is_directional_mode(get_uv_mode(mode));
    if (!(cpi->sf.intra_uv_mode_mask[txsize_sqr_up_map[max_tx_size]] &
          (1 << mode)))
      continue;

    mbmi->uv_mode = mode;
    int cfl_alpha_rate = 0;
    if (mode == UV_CFL_PRED) {
      if (!is_cfl_allowed(xd)) continue;
      assert(!is_directional_mode);
      const TX_SIZE uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
      cfl_alpha_rate = cfl_rd_pick_alpha(x, cpi, uv_tx_size, best_rd);
      if (cfl_alpha_rate == INT_MAX) continue;
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = 0;
    if (is_directional_mode && av1_use_angle_delta(mbmi->sb_type)) {
      const int rate_overhead =
          x->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode];
      if (!rd_pick_intra_angle_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                    &this_rate, &tokenonly_rd_stats))
        continue;
    } else {
      if (!super_block_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd)) {
        continue;
      }
    }
    const int mode_cost =
        x->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode] +
        cfl_alpha_rate;
    this_rate = tokenonly_rd_stats.rate +
                intra_mode_info_cost_uv(cpi, x, mbmi, bsize, mode_cost);
    if (mode == UV_CFL_PRED) {
      assert(is_cfl_allowed(xd));
#if CONFIG_DEBUG
      if (!xd->lossless[mbmi->segment_id])
        assert(xd->cfl.rate == tokenonly_rd_stats.rate + mode_cost);
#endif  // CONFIG_DEBUG
    }
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip;
    }
  }

  const int try_palette =
      av1_allow_palette(cpi->common.allow_screen_content_tools, mbmi->sb_type);
  if (try_palette) {
    uint8_t *best_palette_color_map = x->palette_buffer->best_palette_color_map;
    rd_pick_palette_intra_sbuv(
        cpi, x,
        x->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][UV_DC_PRED],
        best_palette_color_map, &best_mbmi, &best_rd, rate, rate_tokenonly,
        distortion, skippable);
  }

  *mbmi = best_mbmi;
  // Make sure we actually chose a mode
  assert(best_rd < INT64_MAX);
  return best_rd;
}

static void choose_intra_uv_mode(const AV1_COMP *const cpi, MACROBLOCK *const x,
                                 BLOCK_SIZE bsize, TX_SIZE max_tx_size,
                                 int *rate_uv, int *rate_uv_tokenonly,
                                 int64_t *dist_uv, int *skip_uv,
                                 UV_PREDICTION_MODE *mode_uv) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  // Use an estimated rd for uv_intra based on DC_PRED if the
  // appropriate speed flag is set.
  init_sbuv_mode(mbmi);
  if (x->skip_chroma_rd) {
    *rate_uv = 0;
    *rate_uv_tokenonly = 0;
    *dist_uv = 0;
    *skip_uv = 1;
    *mode_uv = UV_DC_PRED;
    return;
  }
  xd->cfl.is_chroma_reference =
      is_chroma_reference(mi_row, mi_col, bsize, cm->seq_params.subsampling_x,
                          cm->seq_params.subsampling_y);
  bsize = scale_chroma_bsize(bsize, xd->plane[AOM_PLANE_U].subsampling_x,
                             xd->plane[AOM_PLANE_U].subsampling_y);
  // Only store reconstructed luma when there's chroma RDO. When there's no
  // chroma RDO, the reconstructed luma will be stored in encode_superblock().
  xd->cfl.store_y = store_cfl_required_rdo(cm, x);
  if (xd->cfl.store_y) {
    // Restore reconstructed luma values.
    av1_encode_intra_block_plane(cpi, x, mbmi->sb_type, AOM_PLANE_Y,
                                 cpi->optimize_seg_arr[mbmi->segment_id],
                                 mi_row, mi_col);
    xd->cfl.store_y = 0;
  }
  rd_pick_intra_sbuv_mode(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                          bsize, max_tx_size);
  *mode_uv = mbmi->uv_mode;
}

static int cost_mv_ref(const MACROBLOCK *const x, PREDICTION_MODE mode,
                       int16_t mode_context) {
  if (is_inter_compound_mode(mode)) {
    return x
        ->inter_compound_mode_cost[mode_context][INTER_COMPOUND_OFFSET(mode)];
  }

  int mode_cost = 0;
  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;

  assert(is_inter_mode(mode));

  if (mode == NEWMV) {
    mode_cost = x->newmv_mode_cost[mode_ctx][0];
    return mode_cost;
  } else {
    mode_cost = x->newmv_mode_cost[mode_ctx][1];
    mode_ctx = (mode_context >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;

    if (mode == GLOBALMV) {
      mode_cost += x->zeromv_mode_cost[mode_ctx][0];
      return mode_cost;
    } else {
      mode_cost += x->zeromv_mode_cost[mode_ctx][1];
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;
      mode_cost += x->refmv_mode_cost[mode_ctx][mode != NEARESTMV];
      return mode_cost;
    }
  }
}

static int get_interinter_compound_mask_rate(const MACROBLOCK *const x,
                                             const MB_MODE_INFO *const mbmi) {
  switch (mbmi->interinter_comp.type) {
    case COMPOUND_AVERAGE: return 0;
    case COMPOUND_WEDGE:
      return get_interinter_wedge_bits(mbmi->sb_type) > 0
                 ? av1_cost_literal(1) +
                       x->wedge_idx_cost[mbmi->sb_type]
                                        [mbmi->interinter_comp.wedge_index]
                 : 0;
    case COMPOUND_DIFFWTD: return av1_cost_literal(1);
    default: assert(0); return 0;
  }
}

typedef struct {
  int eobs;
  int brate;
  int byrate;
  int64_t bdist;
  int64_t bsse;
  int64_t brdcost;
  int_mv mvs[2];
  int_mv pred_mv[2];
  int_mv ref_mv[2];

  ENTROPY_CONTEXT ta[2];
  ENTROPY_CONTEXT tl[2];
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
  SEG_RDSTAT rdstat[4][INTER_MODES + INTER_COMPOUND_MODES];
  int mvthresh;
} BEST_SEG_INFO;

static INLINE int mv_check_bounds(const MvLimits *mv_limits, const MV *mv) {
  return (mv->row >> 3) < mv_limits->row_min ||
         (mv->row >> 3) > mv_limits->row_max ||
         (mv->col >> 3) < mv_limits->col_min ||
         (mv->col >> 3) > mv_limits->col_max;
}

static INLINE int get_single_mode(int this_mode, int ref_idx,
                                  int is_comp_pred) {
  int single_mode;
  if (is_comp_pred) {
    single_mode =
        ref_idx ? compound_ref1_mode(this_mode) : compound_ref0_mode(this_mode);
  } else {
    single_mode = this_mode;
  }
  return single_mode;
}

/* If the current mode shares the same mv with other modes with higher prority,
 * skip this mode. This priority order is nearest > global > near. */
static int skip_repeated_mv(const AV1_COMMON *const cm,
                            const MACROBLOCK *const x, int this_mode,
                            const MV_REFERENCE_FRAME ref_frames[2]) {
  const int is_comp_pred = ref_frames[1] > INTRA_FRAME;
  const uint8_t ref_frame_type = av1_ref_frame_type(ref_frames);
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  if (!is_comp_pred) {
    if (this_mode == NEARMV) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] == 0) {
        // NEARMV has the same motion vector as NEARESTMV
        return 1;
      }
      if (mbmi_ext->ref_mv_count[ref_frame_type] == 1 &&
          cm->global_motion[ref_frames[0]].wmtype <= TRANSLATION) {
        // NEARMV has the same motion vector as GLOBALMV
        return 1;
      }
    }
    if (this_mode == GLOBALMV) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] == 0 &&
          cm->global_motion[ref_frames[0]].wmtype <= TRANSLATION) {
        // GLOBALMV has the same motion vector as NEARESTMV
        return 1;
      }
    }
  } else {
    for (int i = 0; i < 2; ++i) {
      const int single_mode = get_single_mode(this_mode, i, is_comp_pred);
      if (single_mode == NEARMV) {
        if (mbmi_ext->ref_mv_count[ref_frame_type] == 0) {
          // NEARMV has the same motion vector as NEARESTMV in compound mode
          return 1;
        }
      }
    }
    if (this_mode == NEAR_NEARMV) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] == 1 &&
          cm->global_motion[ref_frames[0]].wmtype <= TRANSLATION &&
          cm->global_motion[ref_frames[1]].wmtype <= TRANSLATION) {
        // NEAR_NEARMV has the same motion vector as GLOBAL_GLOBALMV
        return 1;
      }
    }
    if (this_mode == GLOBAL_GLOBALMV) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] == 0 &&
          cm->global_motion[ref_frames[0]].wmtype <= TRANSLATION &&
          cm->global_motion[ref_frames[1]].wmtype <= TRANSLATION) {
        // GLOBAL_GLOBALMV has the same motion vector as NEARST_NEARSTMV
        return 1;
      }
    }
  }
  return 0;
}

static void joint_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                BLOCK_SIZE bsize, int_mv *cur_mv, int mi_row,
                                int mi_col, int_mv *ref_mv_sub8x8[2],
                                const uint8_t *mask, int mask_stride,
                                int *rate_mv, const int block) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));
  const int refs[2] = { mbmi->ref_frame[0], mbmi->ref_frame[1] };
  int_mv ref_mv[2];
  int ite, ref;
  // ic and ir are the 4x4 coordinates of the sub8x8 at index "block"
  const int ic = block & 1;
  const int ir = (block - ic) >> 1;
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int p_col = ((mi_col * MI_SIZE) >> pd->subsampling_x) + 4 * ic;
  const int p_row = ((mi_row * MI_SIZE) >> pd->subsampling_y) + 4 * ir;
  int is_global[2];
  for (ref = 0; ref < 2; ++ref) {
    const WarpedMotionParams *const wm =
        &xd->global_motion[xd->mi[0]->ref_frame[ref]];
    is_global[ref] = is_global_mv_block(xd->mi[0], wm->wmtype);
  }

  // Do joint motion search in compound mode to get more accurate mv.
  struct buf_2d backup_yv12[2][MAX_MB_PLANE];
  int last_besterr[2] = { INT_MAX, INT_MAX };
  const YV12_BUFFER_CONFIG *const scaled_ref_frame[2] = {
    av1_get_scaled_ref_frame(cpi, refs[0]),
    av1_get_scaled_ref_frame(cpi, refs[1])
  };

  // Prediction buffer from second frame.
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[MAX_SB_SQUARE]);
  uint8_t *second_pred;
  (void)ref_mv_sub8x8;

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
    ConvolveParams conv_params = get_conv_params(!id, 0, plane, xd->bd);
    conv_params.use_jnt_comp_avg = 0;
    WarpTypesAllowed warp_types;
    warp_types.global_warp_allowed = is_global[!id];
    warp_types.local_warp_allowed = mbmi->motion_mode == WARPED_CAUSAL;

    for (ref = 0; ref < 2; ++ref) {
      ref_mv[ref] = av1_get_ref_mv(x, ref);
      // Swap out the reference frame for a version that's been scaled to
      // match the resolution of the current frame, allowing the existing
      // motion search code to be used without additional modifications.
      if (scaled_ref_frame[ref]) {
        int i;
        for (i = 0; i < num_planes; i++)
          backup_yv12[ref][i] = xd->plane[i].pre[ref];
        av1_setup_pre_planes(xd, ref, scaled_ref_frame[ref], mi_row, mi_col,
                             NULL, num_planes);
      }
    }

    assert(IMPLIES(scaled_ref_frame[0] != NULL,
                   cm->width == scaled_ref_frame[0]->y_crop_width &&
                       cm->height == scaled_ref_frame[0]->y_crop_height));
    assert(IMPLIES(scaled_ref_frame[1] != NULL,
                   cm->width == scaled_ref_frame[1]->y_crop_width &&
                       cm->height == scaled_ref_frame[1]->y_crop_height));

    // Initialize based on (possibly scaled) prediction buffers.
    ref_yv12[0] = xd->plane[plane].pre[0];
    ref_yv12[1] = xd->plane[plane].pre[1];

    // Get the prediction block from the 'other' reference frame.
    InterpFilters interp_filters = EIGHTTAP_REGULAR;

    // Since we have scaled the reference frames to match the size of the
    // current frame we must use a unit scaling factor during mode selection.
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
      av1_highbd_build_inter_predictor(
          ref_yv12[!id].buf, ref_yv12[!id].stride, second_pred, pw,
          &cur_mv[!id].as_mv, &cm->sf_identity, pw, ph, 0, interp_filters,
          &warp_types, p_col, p_row, plane, MV_PRECISION_Q3, mi_col * MI_SIZE,
          mi_row * MI_SIZE, xd, cm->allow_warped_motion);
    } else {
      second_pred = (uint8_t *)second_pred_alloc_16;
      av1_build_inter_predictor(ref_yv12[!id].buf, ref_yv12[!id].stride,
                                second_pred, pw, &cur_mv[!id].as_mv,
                                &cm->sf_identity, pw, ph, &conv_params,
                                interp_filters, &warp_types, p_col, p_row,
                                plane, !id, MV_PRECISION_Q3, mi_col * MI_SIZE,
                                mi_row * MI_SIZE, xd, cm->allow_warped_motion);
    }

    const int order_idx = id != 0;
    av1_jnt_comp_weight_assign(cm, mbmi, order_idx, &xd->jcp_param.fwd_offset,
                               &xd->jcp_param.bck_offset,
                               &xd->jcp_param.use_jnt_comp_avg, 1);

    // Do full-pixel compound motion search on the current reference frame.
    if (id) xd->plane[plane].pre[0] = ref_yv12[id];
    av1_set_mv_search_range(&x->mv_limits, &ref_mv[id].as_mv);

    // Use the mv result from the single mode as mv predictor.
    // Use the mv result from the single mode as mv predictor.
    *best_mv = cur_mv[id].as_mv;

    best_mv->col >>= 3;
    best_mv->row >>= 3;

    av1_set_mvcost(
        x, id,
        mbmi->ref_mv_idx + (have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0));

    // Small-range full-pixel motion search.
    bestsme = av1_refining_search_8p_c(x, sadpb, search_range,
                                       &cpi->fn_ptr[bsize], mask, mask_stride,
                                       id, &ref_mv[id].as_mv, second_pred);
    if (bestsme < INT_MAX) {
      if (mask)
        bestsme = av1_get_mvpred_mask_var(x, best_mv, &ref_mv[id].as_mv,
                                          second_pred, mask, mask_stride, id,
                                          &cpi->fn_ptr[bsize], 1);
      else
        bestsme = av1_get_mvpred_av_var(x, best_mv, &ref_mv[id].as_mv,
                                        second_pred, &cpi->fn_ptr[bsize], 1);
    }

    x->mv_limits = tmp_mv_limits;

    // Restore the pointer to the first (possibly scaled) prediction buffer.
    if (id) xd->plane[plane].pre[0] = ref_yv12[0];

    for (ref = 0; ref < 2; ++ref) {
      if (scaled_ref_frame[ref]) {
        // Swap back the original buffers for subpel motion search.
        for (int i = 0; i < num_planes; i++) {
          xd->plane[i].pre[ref] = backup_yv12[ref][i];
        }
        // Re-initialize based on unscaled prediction buffers.
        ref_yv12[ref] = xd->plane[plane].pre[ref];
      }
    }

    // Do sub-pixel compound motion search on the current reference frame.
    if (id) xd->plane[plane].pre[0] = ref_yv12[id];

    if (cpi->common.cur_frame_force_integer_mv) {
      x->best_mv.as_mv.row *= 8;
      x->best_mv.as_mv.col *= 8;
    }
    if (bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0) {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;
      bestsme = cpi->find_fractional_mv_step(
          x, cm, mi_row, mi_col, &ref_mv[id].as_mv,
          cpi->common.allow_high_precision_mv, x->errorperbit,
          &cpi->fn_ptr[bsize], 0, cpi->sf.mv.subpel_iters_per_step, NULL,
          x->nmvjointcost, x->mvcost, &dis, &sse, second_pred, mask,
          mask_stride, id, pw, ph, cpi->sf.use_accurate_subpel_search);
    }

    // Restore the pointer to the first prediction buffer.
    if (id) xd->plane[plane].pre[0] = ref_yv12[0];

    if (bestsme < last_besterr[id]) {
      cur_mv[id].as_mv = *best_mv;
      last_besterr[id] = bestsme;
    } else {
      break;
    }
  }

  *rate_mv = 0;

  for (ref = 0; ref < 2; ++ref) {
    av1_set_mvcost(
        x, ref,
        mbmi->ref_mv_idx + (have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0));

    const int_mv curr_ref_mv = av1_get_ref_mv(x, ref);
    *rate_mv += av1_mv_bit_cost(&cur_mv[ref].as_mv, &curr_ref_mv.as_mv,
                                x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
  }
}

static void estimate_ref_frame_costs(
    const AV1_COMMON *cm, const MACROBLOCKD *xd, const MACROBLOCK *x,
    int segment_id, unsigned int *ref_costs_single,
    unsigned int (*ref_costs_comp)[REF_FRAMES]) {
  int seg_ref_active =
      segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME);
  if (seg_ref_active) {
    memset(ref_costs_single, 0, REF_FRAMES * sizeof(*ref_costs_single));
    int ref_frame;
    for (ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame)
      memset(ref_costs_comp[ref_frame], 0,
             REF_FRAMES * sizeof((*ref_costs_comp)[0]));
  } else {
    int intra_inter_ctx = av1_get_intra_inter_context(xd);
    ref_costs_single[INTRA_FRAME] = x->intra_inter_cost[intra_inter_ctx][0];
    unsigned int base_cost = x->intra_inter_cost[intra_inter_ctx][1];

    for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i)
      ref_costs_single[i] = base_cost;

    const int ctx_p1 = av1_get_pred_context_single_ref_p1(xd);
    const int ctx_p2 = av1_get_pred_context_single_ref_p2(xd);
    const int ctx_p3 = av1_get_pred_context_single_ref_p3(xd);
    const int ctx_p4 = av1_get_pred_context_single_ref_p4(xd);
    const int ctx_p5 = av1_get_pred_context_single_ref_p5(xd);
    const int ctx_p6 = av1_get_pred_context_single_ref_p6(xd);

    // Determine cost of a single ref frame, where frame types are represented
    // by a tree:
    // Level 0: add cost whether this ref is a forward or backward ref
    ref_costs_single[LAST_FRAME] += x->single_ref_cost[ctx_p1][0][0];
    ref_costs_single[LAST2_FRAME] += x->single_ref_cost[ctx_p1][0][0];
    ref_costs_single[LAST3_FRAME] += x->single_ref_cost[ctx_p1][0][0];
    ref_costs_single[GOLDEN_FRAME] += x->single_ref_cost[ctx_p1][0][0];
    ref_costs_single[BWDREF_FRAME] += x->single_ref_cost[ctx_p1][0][1];
    ref_costs_single[ALTREF2_FRAME] += x->single_ref_cost[ctx_p1][0][1];
    ref_costs_single[ALTREF_FRAME] += x->single_ref_cost[ctx_p1][0][1];

    // Level 1: if this ref is forward ref,
    // add cost whether it is last/last2 or last3/golden
    ref_costs_single[LAST_FRAME] += x->single_ref_cost[ctx_p3][2][0];
    ref_costs_single[LAST2_FRAME] += x->single_ref_cost[ctx_p3][2][0];
    ref_costs_single[LAST3_FRAME] += x->single_ref_cost[ctx_p3][2][1];
    ref_costs_single[GOLDEN_FRAME] += x->single_ref_cost[ctx_p3][2][1];

    // Level 1: if this ref is backward ref
    // then add cost whether this ref is altref or backward ref
    ref_costs_single[BWDREF_FRAME] += x->single_ref_cost[ctx_p2][1][0];
    ref_costs_single[ALTREF2_FRAME] += x->single_ref_cost[ctx_p2][1][0];
    ref_costs_single[ALTREF_FRAME] += x->single_ref_cost[ctx_p2][1][1];

    // Level 2: further add cost whether this ref is last or last2
    ref_costs_single[LAST_FRAME] += x->single_ref_cost[ctx_p4][3][0];
    ref_costs_single[LAST2_FRAME] += x->single_ref_cost[ctx_p4][3][1];

    // Level 2: last3 or golden
    ref_costs_single[LAST3_FRAME] += x->single_ref_cost[ctx_p5][4][0];
    ref_costs_single[GOLDEN_FRAME] += x->single_ref_cost[ctx_p5][4][1];

    // Level 2: bwdref or altref2
    ref_costs_single[BWDREF_FRAME] += x->single_ref_cost[ctx_p6][5][0];
    ref_costs_single[ALTREF2_FRAME] += x->single_ref_cost[ctx_p6][5][1];

    if (cm->reference_mode != SINGLE_REFERENCE) {
      // Similar to single ref, determine cost of compound ref frames.
      // cost_compound_refs = cost_first_ref + cost_second_ref
      const int bwdref_comp_ctx_p = av1_get_pred_context_comp_bwdref_p(xd);
      const int bwdref_comp_ctx_p1 = av1_get_pred_context_comp_bwdref_p1(xd);
      const int ref_comp_ctx_p = av1_get_pred_context_comp_ref_p(xd);
      const int ref_comp_ctx_p1 = av1_get_pred_context_comp_ref_p1(xd);
      const int ref_comp_ctx_p2 = av1_get_pred_context_comp_ref_p2(xd);

      const int comp_ref_type_ctx = av1_get_comp_reference_type_context(xd);
      unsigned int ref_bicomp_costs[REF_FRAMES] = { 0 };

      ref_bicomp_costs[LAST_FRAME] = ref_bicomp_costs[LAST2_FRAME] =
          ref_bicomp_costs[LAST3_FRAME] = ref_bicomp_costs[GOLDEN_FRAME] =
              base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][1];
      ref_bicomp_costs[BWDREF_FRAME] = ref_bicomp_costs[ALTREF2_FRAME] = 0;
      ref_bicomp_costs[ALTREF_FRAME] = 0;

      // cost of first ref frame
      ref_bicomp_costs[LAST_FRAME] += x->comp_ref_cost[ref_comp_ctx_p][0][0];
      ref_bicomp_costs[LAST2_FRAME] += x->comp_ref_cost[ref_comp_ctx_p][0][0];
      ref_bicomp_costs[LAST3_FRAME] += x->comp_ref_cost[ref_comp_ctx_p][0][1];
      ref_bicomp_costs[GOLDEN_FRAME] += x->comp_ref_cost[ref_comp_ctx_p][0][1];

      ref_bicomp_costs[LAST_FRAME] += x->comp_ref_cost[ref_comp_ctx_p1][1][0];
      ref_bicomp_costs[LAST2_FRAME] += x->comp_ref_cost[ref_comp_ctx_p1][1][1];

      ref_bicomp_costs[LAST3_FRAME] += x->comp_ref_cost[ref_comp_ctx_p2][2][0];
      ref_bicomp_costs[GOLDEN_FRAME] += x->comp_ref_cost[ref_comp_ctx_p2][2][1];

      // cost of second ref frame
      ref_bicomp_costs[BWDREF_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p][0][0];
      ref_bicomp_costs[ALTREF2_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p][0][0];
      ref_bicomp_costs[ALTREF_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p][0][1];

      ref_bicomp_costs[BWDREF_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p1][1][0];
      ref_bicomp_costs[ALTREF2_FRAME] +=
          x->comp_bwdref_cost[bwdref_comp_ctx_p1][1][1];

      // cost: if one ref frame is forward ref, the other ref is backward ref
      int ref0, ref1;
      for (ref0 = LAST_FRAME; ref0 <= GOLDEN_FRAME; ++ref0) {
        for (ref1 = BWDREF_FRAME; ref1 <= ALTREF_FRAME; ++ref1) {
          ref_costs_comp[ref0][ref1] =
              ref_bicomp_costs[ref0] + ref_bicomp_costs[ref1];
        }
      }

      // cost: if both ref frames are the same side.
      const int uni_comp_ref_ctx_p = av1_get_pred_context_uni_comp_ref_p(xd);
      const int uni_comp_ref_ctx_p1 = av1_get_pred_context_uni_comp_ref_p1(xd);
      const int uni_comp_ref_ctx_p2 = av1_get_pred_context_uni_comp_ref_p2(xd);
      ref_costs_comp[LAST_FRAME][LAST2_FRAME] =
          base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p][0][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p1][1][0];
      ref_costs_comp[LAST_FRAME][LAST3_FRAME] =
          base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p][0][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p1][1][1] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p2][2][0];
      ref_costs_comp[LAST_FRAME][GOLDEN_FRAME] =
          base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p][0][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p1][1][1] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p2][2][1];
      ref_costs_comp[BWDREF_FRAME][ALTREF_FRAME] =
          base_cost + x->comp_ref_type_cost[comp_ref_type_ctx][0] +
          x->uni_comp_ref_cost[uni_comp_ref_ctx_p][0][1];
    } else {
      int ref0, ref1;
      for (ref0 = LAST_FRAME; ref0 <= GOLDEN_FRAME; ++ref0) {
        for (ref1 = BWDREF_FRAME; ref1 <= ALTREF_FRAME; ++ref1)
          ref_costs_comp[ref0][ref1] = 512;
      }
      ref_costs_comp[LAST_FRAME][LAST2_FRAME] = 512;
      ref_costs_comp[LAST_FRAME][LAST3_FRAME] = 512;
      ref_costs_comp[LAST_FRAME][GOLDEN_FRAME] = 512;
      ref_costs_comp[BWDREF_FRAME][ALTREF_FRAME] = 512;
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

static void setup_buffer_ref_mvs_inter(
    const AV1_COMP *const cpi, MACROBLOCK *x, MV_REFERENCE_FRAME ref_frame,
    BLOCK_SIZE block_size, int mi_row, int mi_col,
    struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE]) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_buffer(cpi, ref_frame);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const struct scale_factors *const sf = &cm->frame_refs[ref_frame - 1].sf;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;

  assert(yv12 != NULL);

  // TODO(jkoleszar): Is the UV buffer ever used here? If so, need to make this
  // use the UV scaling factors.
  av1_setup_pred_block(xd, yv12_mb[ref_frame], yv12, mi_row, mi_col, sf, sf,
                       num_planes);

  // Gets an initial list of candidate vectors from neighbours and orders them
  av1_find_mv_refs(cm, xd, mbmi, ref_frame, mbmi_ext->ref_mv_count,
                   mbmi_ext->ref_mv_stack, NULL, mbmi_ext->global_mvs, mi_row,
                   mi_col, mbmi_ext->mode_context);

  // Further refinement that is encode side only to test the top few candidates
  // in full and choose the best as the centre point for subsequent searches.
  // The current implementation doesn't support scaling.
  (void)block_size;
  av1_mv_pred(cpi, x, yv12_mb[ref_frame][0].buf, yv12->y_stride, ref_frame,
              block_size);
}

static void single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize, int mi_row, int mi_col,
                                 int ref_idx, int *rate_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MB_MODE_INFO *mbmi = xd->mi[0];
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0, 0, 0, 0 } };
  int bestsme = INT_MAX;
  int step_param;
  int sadpb = x->sadperbit16;
  MV mvp_full;
  int ref = mbmi->ref_frame[ref_idx];
  MV ref_mv = av1_get_ref_mv(x, ref_idx).as_mv;

  MvLimits tmp_mv_limits = x->mv_limits;
  int cost_list[5];

  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);

  if (scaled_ref_frame) {
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // full-pixel motion search code to be used without additional
    // modifications.
    for (int i = 0; i < num_planes; i++) {
      backup_yv12[i] = xd->plane[i].pre[ref_idx];
    }
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL,
                         num_planes);
  }

  av1_set_mvcost(
      x, ref_idx,
      mbmi->ref_mv_idx + (have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0));

  // Work out the size of the first step in the mv step search.
  // 0 here is maximum length first step. 1 is AOMMAX >> 1 etc.
  if (cpi->sf.mv.auto_mv_step_size && cm->show_frame) {
    // Take the weighted average of the step_params based on the last frame's
    // max mv magnitude and that based on the best ref mvs of the current
    // block for the given reference.
    step_param =
        (av1_init_search_range(x->max_mv_context[ref]) + cpi->mv_step_param) /
        2;
  } else {
    step_param = cpi->mv_step_param;
  }

  if (cpi->sf.adaptive_motion_search && bsize < cm->seq_params.sb_size) {
    int boffset =
        2 * (mi_size_wide_log2[cm->seq_params.sb_size] -
             AOMMIN(mi_size_high_log2[bsize], mi_size_wide_log2[bsize]));
    step_param = AOMMAX(step_param, boffset);
  }

  if (cpi->sf.adaptive_motion_search) {
    int bwl = mi_size_wide_log2[bsize];
    int bhl = mi_size_high_log2[bsize];
    int tlevel = x->pred_mv_sad[ref] >> (bwl + bhl + 4);

    if (tlevel < 5) {
      step_param += 2;
      step_param = AOMMIN(step_param, MAX_MVSEARCH_STEPS - 1);
    }

    // prev_mv_sad is not setup for dynamically scaled frames.
    if (cpi->oxcf.resize_mode != RESIZE_RANDOM) {
      int i;
      for (i = LAST_FRAME; i <= ALTREF_FRAME && cm->show_frame; ++i) {
        if ((x->pred_mv_sad[ref] >> 3) > x->pred_mv_sad[i]) {
          x->pred_mv[ref].row = 0;
          x->pred_mv[ref].col = 0;
          x->best_mv.as_int = INVALID_MV;

          if (scaled_ref_frame) {
            // Swap back the original buffers before returning.
            for (int j = 0; j < num_planes; ++j)
              xd->plane[j].pre[ref_idx] = backup_yv12[j];
          }
          return;
        }
      }
    }
  }

  // Note: MV limits are modified here. Always restore the original values
  // after full-pixel motion search.
  av1_set_mv_search_range(&x->mv_limits, &ref_mv);

  if (mbmi->motion_mode != SIMPLE_TRANSLATION)
    mvp_full = mbmi->mv[0].as_mv;
  else
    mvp_full = ref_mv;

  mvp_full.col >>= 3;
  mvp_full.row >>= 3;

  x->best_mv.as_int = x->second_best_mv.as_int = INVALID_MV;

  switch (mbmi->motion_mode) {
    case SIMPLE_TRANSLATION:
      bestsme = av1_full_pixel_search(cpi, x, bsize, &mvp_full, step_param,
                                      sadpb, cond_cost_list(cpi, cost_list),
                                      &ref_mv, INT_MAX, 1, (MI_SIZE * mi_col),
                                      (MI_SIZE * mi_row), 0);
      break;
    case OBMC_CAUSAL:
      bestsme = av1_obmc_full_pixel_diamond(
          cpi, x, &mvp_full, step_param, sadpb,
          MAX_MVSEARCH_STEPS - 1 - step_param, 1, &cpi->fn_ptr[bsize], &ref_mv,
          &(x->best_mv.as_mv), 0);
      break;
    default: assert(0 && "Invalid motion mode!\n");
  }

  if (scaled_ref_frame) {
    // Swap back the original buffers for subpel motion search.
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
    }
  }

  x->mv_limits = tmp_mv_limits;

  if (cpi->common.cur_frame_force_integer_mv) {
    x->best_mv.as_mv.row *= 8;
    x->best_mv.as_mv.col *= 8;
  }
  const int use_fractional_mv =
      bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0;
  if (use_fractional_mv) {
    int dis; /* TODO: use dis in distortion calculation later. */
    switch (mbmi->motion_mode) {
      case SIMPLE_TRANSLATION:
        if (cpi->sf.use_accurate_subpel_search) {
          int best_mv_var;
          const int try_second = x->second_best_mv.as_int != INVALID_MV &&
                                 x->second_best_mv.as_int != x->best_mv.as_int;
          const int pw = block_size_wide[bsize];
          const int ph = block_size_high[bsize];

          best_mv_var = cpi->find_fractional_mv_step(
              x, cm, mi_row, mi_col, &ref_mv, cm->allow_high_precision_mv,
              x->errorperbit, &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
              cpi->sf.mv.subpel_iters_per_step, cond_cost_list(cpi, cost_list),
              x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], NULL, NULL,
              0, 0, pw, ph, 1);

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
                  x, cm, mi_row, mi_col, &ref_mv, cm->allow_high_precision_mv,
                  x->errorperbit, &cpi->fn_ptr[bsize],
                  cpi->sf.mv.subpel_force_stop,
                  cpi->sf.mv.subpel_iters_per_step,
                  cond_cost_list(cpi, cost_list), x->nmvjointcost, x->mvcost,
                  &dis, &x->pred_sse[ref], NULL, NULL, 0, 0, pw, ph, 1);
              if (this_var < best_mv_var) best_mv = x->best_mv.as_mv;
              x->best_mv.as_mv = best_mv;
            }
          }
        } else {
          cpi->find_fractional_mv_step(
              x, cm, mi_row, mi_col, &ref_mv, cm->allow_high_precision_mv,
              x->errorperbit, &cpi->fn_ptr[bsize], cpi->sf.mv.subpel_force_stop,
              cpi->sf.mv.subpel_iters_per_step, cond_cost_list(cpi, cost_list),
              x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], NULL, NULL,
              0, 0, 0, 0, 0);
        }
        break;
      case OBMC_CAUSAL:
        av1_find_best_obmc_sub_pixel_tree_up(
            x, cm, mi_row, mi_col, &x->best_mv.as_mv, &ref_mv,
            cm->allow_high_precision_mv, x->errorperbit, &cpi->fn_ptr[bsize],
            cpi->sf.mv.subpel_force_stop, cpi->sf.mv.subpel_iters_per_step,
            x->nmvjointcost, x->mvcost, &dis, &x->pred_sse[ref], 0,
            cpi->sf.use_accurate_subpel_search);
        break;
      default: assert(0 && "Invalid motion mode!\n");
    }
  }
  *rate_mv = av1_mv_bit_cost(&x->best_mv.as_mv, &ref_mv, x->nmvjointcost,
                             x->mvcost, MV_COST_WEIGHT);

  if (cpi->sf.adaptive_motion_search && mbmi->motion_mode == SIMPLE_TRANSLATION)
    x->pred_mv[ref] = x->best_mv.as_mv;
}

static INLINE void restore_dst_buf(MACROBLOCKD *xd, BUFFER_SET dst,
                                   const int num_planes) {
  int i;
  for (i = 0; i < num_planes; i++) {
    xd->plane[i].dst.buf = dst.plane[i];
    xd->plane[i].dst.stride = dst.stride[i];
  }
}

static void build_second_inter_pred(const AV1_COMP *cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, const MV *other_mv,
                                    int mi_row, int mi_col, const int block,
                                    int ref_idx, uint8_t *second_pred) {
  const AV1_COMMON *const cm = &cpi->common;
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int other_ref = mbmi->ref_frame[!ref_idx];
  struct macroblockd_plane *const pd = &xd->plane[0];
  // ic and ir are the 4x4 coordinates of the sub8x8 at index "block"
  const int ic = block & 1;
  const int ir = (block - ic) >> 1;
  const int p_col = ((mi_col * MI_SIZE) >> pd->subsampling_x) + 4 * ic;
  const int p_row = ((mi_row * MI_SIZE) >> pd->subsampling_y) + 4 * ir;
  const WarpedMotionParams *const wm = &xd->global_motion[other_ref];
  int is_global = is_global_mv_block(xd->mi[0], wm->wmtype);

  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));

  const int plane = 0;
  struct buf_2d ref_yv12 = xd->plane[plane].pre[!ref_idx];

  struct scale_factors sf;
  av1_setup_scale_factors_for_frame(&sf, ref_yv12.width, ref_yv12.height,
                                    cm->width, cm->height);

  ConvolveParams conv_params = get_conv_params(!ref_idx, 0, plane, xd->bd);
  WarpTypesAllowed warp_types;
  warp_types.global_warp_allowed = is_global;
  warp_types.local_warp_allowed = mbmi->motion_mode == WARPED_CAUSAL;

  // Get the prediction block from the 'other' reference frame.
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    av1_highbd_build_inter_predictor(
        ref_yv12.buf, ref_yv12.stride, second_pred, pw, other_mv, &sf, pw, ph,
        0, mbmi->interp_filters, &warp_types, p_col, p_row, plane,
        MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd,
        cm->allow_warped_motion);
  } else {
    av1_build_inter_predictor(
        ref_yv12.buf, ref_yv12.stride, second_pred, pw, other_mv, &sf, pw, ph,
        &conv_params, mbmi->interp_filters, &warp_types, p_col, p_row, plane,
        !ref_idx, MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE, xd,
        cm->allow_warped_motion);
  }

  av1_jnt_comp_weight_assign(cm, mbmi, 0, &xd->jcp_param.fwd_offset,
                             &xd->jcp_param.bck_offset,
                             &xd->jcp_param.use_jnt_comp_avg, 1);
}

// Search for the best mv for one component of a compound,
// given that the other component is fixed.
static void compound_single_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                          BLOCK_SIZE bsize, MV *this_mv,
                                          int mi_row, int mi_col,
                                          const uint8_t *second_pred,
                                          const uint8_t *mask, int mask_stride,
                                          int *rate_mv, int ref_idx) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int ref = mbmi->ref_frame[ref_idx];
  const int_mv ref_mv = av1_get_ref_mv(x, ref_idx);
  struct macroblockd_plane *const pd = &xd->plane[0];

  struct buf_2d backup_yv12[MAX_MB_PLANE];
  const YV12_BUFFER_CONFIG *const scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);

  // Check that this is either an interinter or an interintra block
  assert(has_second_ref(mbmi) || (ref_idx == 0 && is_interintra_mode(mbmi)));

  // Store the first prediction buffer.
  struct buf_2d orig_yv12;
  if (ref_idx) {
    orig_yv12 = pd->pre[0];
    pd->pre[0] = pd->pre[ref_idx];
  }

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // full-pixel motion search code to be used without additional
    // modifications.
    for (i = 0; i < num_planes; i++) backup_yv12[i] = xd->plane[i].pre[ref_idx];
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL,
                         num_planes);
  }

  int bestsme = INT_MAX;
  int sadpb = x->sadperbit16;
  MV *const best_mv = &x->best_mv.as_mv;
  int search_range = 3;

  MvLimits tmp_mv_limits = x->mv_limits;

  // Do compound motion search on the current reference frame.
  av1_set_mv_search_range(&x->mv_limits, &ref_mv.as_mv);

  // Use the mv result from the single mode as mv predictor.
  *best_mv = *this_mv;

  best_mv->col >>= 3;
  best_mv->row >>= 3;

  av1_set_mvcost(
      x, ref_idx,
      mbmi->ref_mv_idx + (have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0));

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

  if (scaled_ref_frame) {
    // Swap back the original buffers for subpel motion search.
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
    }
  }

  if (cpi->common.cur_frame_force_integer_mv) {
    x->best_mv.as_mv.row *= 8;
    x->best_mv.as_mv.col *= 8;
  }
  const int use_fractional_mv =
      bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0;
  if (use_fractional_mv) {
    int dis; /* TODO: use dis in distortion calculation later. */
    unsigned int sse;
    bestsme = cpi->find_fractional_mv_step(
        x, cm, mi_row, mi_col, &ref_mv.as_mv,
        cpi->common.allow_high_precision_mv, x->errorperbit,
        &cpi->fn_ptr[bsize], 0, cpi->sf.mv.subpel_iters_per_step, NULL,
        x->nmvjointcost, x->mvcost, &dis, &sse, second_pred, mask, mask_stride,
        ref_idx, pw, ph, cpi->sf.use_accurate_subpel_search);
  }

  // Restore the pointer to the first unscaled prediction buffer.
  if (ref_idx) pd->pre[0] = orig_yv12;

  if (bestsme < INT_MAX) *this_mv = *best_mv;

  *rate_mv = 0;

  av1_set_mvcost(
      x, ref_idx,
      mbmi->ref_mv_idx + (have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0));
  *rate_mv += av1_mv_bit_cost(this_mv, &ref_mv.as_mv, x->nmvjointcost,
                              x->mvcost, MV_COST_WEIGHT);
}

// Wrapper for compound_single_motion_search, for the common case
// where the second prediction is also an inter mode.
static void compound_single_motion_search_interinter(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int_mv *cur_mv,
    int mi_row, int mi_col, const uint8_t *mask, int mask_stride, int *rate_mv,
    const int block, int ref_idx) {
  MACROBLOCKD *xd = &x->e_mbd;
  // This function should only ever be called for compound modes
  assert(has_second_ref(xd->mi[0]));

  // Prediction buffer from second frame.
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[MAX_SB_SQUARE]);
  uint8_t *second_pred;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
  else
    second_pred = (uint8_t *)second_pred_alloc_16;

  MV *this_mv = &cur_mv[ref_idx].as_mv;
  const MV *other_mv = &cur_mv[!ref_idx].as_mv;

  build_second_inter_pred(cpi, x, bsize, other_mv, mi_row, mi_col, block,
                          ref_idx, second_pred);

  compound_single_motion_search(cpi, x, bsize, this_mv, mi_row, mi_col,
                                second_pred, mask, mask_stride, rate_mv,
                                ref_idx);
}

static void do_masked_motion_search_indexed(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE bsize,
    int mi_row, int mi_col, int_mv *tmp_mv, int *rate_mv, int which) {
  // NOTE: which values: 0 - 0 only, 1 - 1 only, 2 - both
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  BLOCK_SIZE sb_type = mbmi->sb_type;
  const uint8_t *mask;
  const int mask_stride = block_size_wide[bsize];

  mask = av1_get_compound_type_mask(comp_data, sb_type);

  tmp_mv[0].as_int = cur_mv[0].as_int;
  tmp_mv[1].as_int = cur_mv[1].as_int;
  if (which == 0 || which == 1) {
    compound_single_motion_search_interinter(cpi, x, bsize, tmp_mv, mi_row,
                                             mi_col, mask, mask_stride, rate_mv,
                                             0, which);
  } else if (which == 2) {
    joint_motion_search(cpi, x, bsize, tmp_mv, mi_row, mi_col, NULL, mask,
                        mask_stride, rate_mv, 0);
  }
}

#define USE_DISCOUNT_NEWMV_TEST 0
#if USE_DISCOUNT_NEWMV_TEST
// In some situations we want to discount the apparent cost of a new motion
// vector. Where there is a subtle motion field and especially where there is
// low spatial complexity then it can be hard to cover the cost of a new motion
// vector in a single block, even if that motion vector reduces distortion.
// However, once established that vector may be usable through the nearest and
// near mv modes to reduce distortion in subsequent blocks and also improve
// visual quality.
#define NEW_MV_DISCOUNT_FACTOR 8
static INLINE void get_this_mv(int_mv *this_mv, int this_mode, int ref_idx,
                               int ref_mv_idx,
                               const MV_REFERENCE_FRAME *ref_frame,
                               const MB_MODE_INFO_EXT *mbmi_ext);
static int discount_newmv_test(const AV1_COMP *const cpi, const MACROBLOCK *x,
                               int this_mode, int_mv this_mv) {
  if (this_mode == NEWMV && this_mv.as_int != 0 &&
      !cpi->rc.is_src_frame_alt_ref) {
    // Only discount new_mv when nearst_mv and all near_mv are zero, and the
    // new_mv is not equal to global_mv
    const AV1_COMMON *const cm = &cpi->common;
    const MACROBLOCKD *const xd = &x->e_mbd;
    const MB_MODE_INFO *const mbmi = xd->mi[0];
    const MV_REFERENCE_FRAME tmp_ref_frames[2] = { mbmi->ref_frame[0],
                                                   NONE_FRAME };
    const uint8_t ref_frame_type = av1_ref_frame_type(tmp_ref_frames);
    int_mv nearest_mv;
    get_this_mv(&nearest_mv, NEARESTMV, 0, 0, tmp_ref_frames, x->mbmi_ext);
    int ret = nearest_mv.as_int == 0;
    for (int ref_mv_idx = 0;
         ref_mv_idx < x->mbmi_ext->ref_mv_count[ref_frame_type]; ++ref_mv_idx) {
      int_mv near_mv;
      get_this_mv(&near_mv, NEARMV, 0, ref_mv_idx, tmp_ref_frames, x->mbmi_ext);
      ret &= near_mv.as_int == 0;
    }
    if (cm->global_motion[tmp_ref_frames[0]].wmtype <= TRANSLATION) {
      int_mv global_mv;
      get_this_mv(&global_mv, GLOBALMV, 0, 0, tmp_ref_frames, x->mbmi_ext);
      ret &= global_mv.as_int != this_mv.as_int;
    }
    return ret;
  }
  return 0;
}
#endif

#define LEFT_TOP_MARGIN ((AOM_BORDER_IN_PIXELS - AOM_INTERP_EXTEND) << 3)
#define RIGHT_BOTTOM_MARGIN ((AOM_BORDER_IN_PIXELS - AOM_INTERP_EXTEND) << 3)

// TODO(jingning): this mv clamping function should be block size dependent.
static INLINE void clamp_mv2(MV *mv, const MACROBLOCKD *xd) {
  clamp_mv(mv, xd->mb_to_left_edge - LEFT_TOP_MARGIN,
           xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN,
           xd->mb_to_top_edge - LEFT_TOP_MARGIN,
           xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN);
}

static int estimate_wedge_sign(const AV1_COMP *cpi, const MACROBLOCK *x,
                               const BLOCK_SIZE bsize, const uint8_t *pred0,
                               int stride0, const uint8_t *pred1, int stride1) {
  static const BLOCK_SIZE split_qtr[BLOCK_SIZES_ALL] = {
    //                            4X4
    BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X4,
    // 8X16,       16X8,          16X16
    BLOCK_4X8, BLOCK_8X4, BLOCK_8X8,
    // 16X32,      32X16,         32X32
    BLOCK_8X16, BLOCK_16X8, BLOCK_16X16,
    // 32X64,      64X32,         64X64
    BLOCK_16X32, BLOCK_32X16, BLOCK_32X32,
    // 64x128,     128x64,        128x128
    BLOCK_32X64, BLOCK_64X32, BLOCK_64X64,
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X16,
    // 32X8,       16X64,         64X16
    BLOCK_16X4, BLOCK_8X32, BLOCK_32X8
  };
  const struct macroblock_plane *const p = &x->plane[0];
  const uint8_t *src = p->src.buf;
  int src_stride = p->src.stride;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  uint32_t esq[2][4];
  int64_t tl, br;

  const BLOCK_SIZE f_index = split_qtr[bsize];
  assert(f_index != BLOCK_INVALID);

  if (x->e_mbd.cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    pred0 = CONVERT_TO_BYTEPTR(pred0);
    pred1 = CONVERT_TO_BYTEPTR(pred1);
  }

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

  tl = ((int64_t)esq[0][0] + esq[0][1] + esq[0][2]) -
       ((int64_t)esq[1][0] + esq[1][1] + esq[1][2]);
  br = ((int64_t)esq[1][3] + esq[1][1] + esq[1][2]) -
       ((int64_t)esq[0][3] + esq[0][1] + esq[0][2]);
  return (tl + br > 0);
}

// Choose the best wedge index and sign
static int64_t pick_wedge(const AV1_COMP *const cpi, const MACROBLOCK *const x,
                          const BLOCK_SIZE bsize, const uint8_t *const p0,
                          const int16_t *const residual1,
                          const int16_t *const diff10,
                          int *const best_wedge_sign,
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
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;

  DECLARE_ALIGNED(32, int16_t, residual0[MAX_SB_SQUARE]);  // src - pred0
  if (hbd) {
    aom_highbd_subtract_block(bh, bw, residual0, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
  } else {
    aom_subtract_block(bh, bw, residual0, bw, src->buf, src->stride, p0, bw);
  }

  int64_t sign_limit = ((int64_t)aom_sum_squares_i16(residual0, N) -
                        (int64_t)aom_sum_squares_i16(residual1, N)) *
                       (1 << WEDGE_WEIGHT_BITS) / 2;
  int16_t *ds = residual0;
  if (N < 64)
    av1_wedge_compute_delta_squares_c(ds, residual0, residual1, N);
  else
    av1_wedge_compute_delta_squares(ds, residual0, residual1, N);

  for (wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    mask = av1_get_contiguous_soft_mask(wedge_index, 0, bsize);

    // TODO(jingning): Make sse2 functions support N = 16 case
    if (N < 64)
      wedge_sign = av1_wedge_sign_from_residuals_c(ds, mask, N, sign_limit);
    else
      wedge_sign = av1_wedge_sign_from_residuals(ds, mask, N, sign_limit);

    mask = av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
    if (N < 64)
      sse = av1_wedge_sse_from_residuals_c(residual1, diff10, mask, N);
    else
      sse = av1_wedge_sse_from_residuals(residual1, diff10, mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rate += x->wedge_idx_cost[bsize][wedge_index];
    rd = RDCOST(x->rdmult, rate, dist);

    if (rd < best_rd) {
      *best_wedge_index = wedge_index;
      *best_wedge_sign = wedge_sign;
      best_rd = rd;
    }
  }

  return best_rd -
         RDCOST(x->rdmult, x->wedge_idx_cost[bsize][*best_wedge_index], 0);
}

// Choose the best wedge index the specified sign
static int64_t pick_wedge_fixed_sign(const AV1_COMP *const cpi,
                                     const MACROBLOCK *const x,
                                     const BLOCK_SIZE bsize,
                                     const int16_t *const residual1,
                                     const int16_t *const diff10,
                                     const int wedge_sign,
                                     int *const best_wedge_index) {
  const MACROBLOCKD *const xd = &x->e_mbd;

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
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
  for (wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    mask = av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
    if (N < 64)
      sse = av1_wedge_sse_from_residuals_c(residual1, diff10, mask, N);
    else
      sse = av1_wedge_sse_from_residuals(residual1, diff10, mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rate += x->wedge_idx_cost[bsize][wedge_index];
    rd = RDCOST(x->rdmult, rate, dist);

    if (rd < best_rd) {
      *best_wedge_index = wedge_index;
      best_rd = rd;
    }
  }
  return best_rd -
         RDCOST(x->rdmult, x->wedge_idx_cost[bsize][*best_wedge_index], 0);
}

static int64_t pick_interinter_wedge(
    const AV1_COMP *const cpi, MACROBLOCK *const x, const BLOCK_SIZE bsize,
    const uint8_t *const p0, const uint8_t *const p1,
    const int16_t *const residual1, const int16_t *const diff10) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int bw = block_size_wide[bsize];

  int64_t rd;
  int wedge_index = -1;
  int wedge_sign = 0;

  assert(is_interinter_compound_used(COMPOUND_WEDGE, bsize));
  assert(cpi->common.seq_params.enable_masked_compound);

  if (cpi->sf.fast_wedge_sign_estimate) {
    wedge_sign = estimate_wedge_sign(cpi, x, bsize, p0, bw, p1, bw);
    rd = pick_wedge_fixed_sign(cpi, x, bsize, residual1, diff10, wedge_sign,
                               &wedge_index);
  } else {
    rd = pick_wedge(cpi, x, bsize, p0, residual1, diff10, &wedge_sign,
                    &wedge_index);
  }

  mbmi->interinter_comp.wedge_sign = wedge_sign;
  mbmi->interinter_comp.wedge_index = wedge_index;
  return rd;
}

static int64_t pick_interinter_seg(const AV1_COMP *const cpi,
                                   MACROBLOCK *const x, const BLOCK_SIZE bsize,
                                   const uint8_t *const p0,
                                   const uint8_t *const p1,
                                   const int16_t *const residual1,
                                   const int16_t *const diff10) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  int rate;
  uint64_t sse;
  int64_t dist;
  int64_t rd0;
  DIFFWTD_MASK_TYPE cur_mask_type;
  int64_t best_rd = INT64_MAX;
  DIFFWTD_MASK_TYPE best_mask_type = 0;
  const int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
  // try each mask type and its inverse
  for (cur_mask_type = 0; cur_mask_type < DIFFWTD_MASK_TYPES; cur_mask_type++) {
    // build mask and inverse
    if (hbd)
      av1_build_compound_diffwtd_mask_highbd(
          xd->seg_mask, cur_mask_type, CONVERT_TO_BYTEPTR(p0), bw,
          CONVERT_TO_BYTEPTR(p1), bw, bh, bw, xd->bd);
    else
      av1_build_compound_diffwtd_mask(xd->seg_mask, cur_mask_type, p0, bw, p1,
                                      bw, bh, bw);

    // compute rd for mask
    sse = av1_wedge_sse_from_residuals(residual1, diff10, xd->seg_mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_from_sse(cpi, xd, bsize, 0, sse, &rate, &dist);
    rd0 = RDCOST(x->rdmult, rate, dist);

    if (rd0 < best_rd) {
      best_mask_type = cur_mask_type;
      best_rd = rd0;
    }
  }

  // make final mask
  mbmi->interinter_comp.mask_type = best_mask_type;
  if (hbd)
    av1_build_compound_diffwtd_mask_highbd(
        xd->seg_mask, mbmi->interinter_comp.mask_type, CONVERT_TO_BYTEPTR(p0),
        bw, CONVERT_TO_BYTEPTR(p1), bw, bh, bw, xd->bd);
  else
    av1_build_compound_diffwtd_mask(
        xd->seg_mask, mbmi->interinter_comp.mask_type, p0, bw, p1, bw, bh, bw);

  return best_rd;
}

static int64_t pick_interintra_wedge(const AV1_COMP *const cpi,
                                     const MACROBLOCK *const x,
                                     const BLOCK_SIZE bsize,
                                     const uint8_t *const p0,
                                     const uint8_t *const p1) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(is_interintra_wedge_used(bsize));
  assert(cpi->common.seq_params.enable_interintra_compound);

  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  DECLARE_ALIGNED(32, int16_t, residual1[MAX_SB_SQUARE]);  // src - pred1
  DECLARE_ALIGNED(32, int16_t, diff10[MAX_SB_SQUARE]);     // pred1 - pred0
  if (get_bitdepth_data_path_index(xd)) {
    aom_highbd_subtract_block(bh, bw, residual1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p1), bw, xd->bd);
    aom_highbd_subtract_block(bh, bw, diff10, bw, CONVERT_TO_BYTEPTR(p1), bw,
                              CONVERT_TO_BYTEPTR(p0), bw, xd->bd);
  } else {
    aom_subtract_block(bh, bw, residual1, bw, src->buf, src->stride, p1, bw);
    aom_subtract_block(bh, bw, diff10, bw, p1, bw, p0, bw);
  }
  int wedge_index = -1;
  int64_t rd =
      pick_wedge_fixed_sign(cpi, x, bsize, residual1, diff10, 0, &wedge_index);

  mbmi->interintra_wedge_sign = 0;
  mbmi->interintra_wedge_index = wedge_index;
  return rd;
}

static int64_t pick_interinter_mask(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    const BLOCK_SIZE bsize,
                                    const uint8_t *const p0,
                                    const uint8_t *const p1,
                                    const int16_t *const residual1,
                                    const int16_t *const diff10) {
  const COMPOUND_TYPE compound_type = x->e_mbd.mi[0]->interinter_comp.type;
  switch (compound_type) {
    case COMPOUND_WEDGE:
      return pick_interinter_wedge(cpi, x, bsize, p0, p1, residual1, diff10);
    case COMPOUND_DIFFWTD:
      return pick_interinter_seg(cpi, x, bsize, p0, p1, residual1, diff10);
    default: assert(0); return 0;
  }
}

static int interinter_compound_motion_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const BLOCK_SIZE bsize, const int this_mode, int mi_row, int mi_col) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int_mv tmp_mv[2];
  int tmp_rate_mv = 0;
  mbmi->interinter_comp.seg_mask = xd->seg_mask;
  const INTERINTER_COMPOUND_DATA *compound_data = &mbmi->interinter_comp;

  if (this_mode == NEW_NEWMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 2);
    mbmi->mv[0].as_int = tmp_mv[0].as_int;
    mbmi->mv[1].as_int = tmp_mv[1].as_int;
  } else if (this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 0);
    mbmi->mv[0].as_int = tmp_mv[0].as_int;
  } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, compound_data, bsize,
                                    mi_row, mi_col, tmp_mv, &tmp_rate_mv, 1);
    mbmi->mv[1].as_int = tmp_mv[1].as_int;
  }
  return tmp_rate_mv;
}

static int64_t build_and_cost_compound_type(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const BLOCK_SIZE bsize, const int this_mode, int *rs2, int rate_mv,
    BUFFER_SET *ctx, int *out_rate_mv, uint8_t **preds0, uint8_t **preds1,
    int16_t *residual1, int16_t *diff10, int *strides, int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int rate_sum;
  int64_t dist_sum;
  int64_t best_rd_cur = INT64_MAX;
  int64_t rd = INT64_MAX;
  int tmp_skip_txfm_sb;
  int64_t tmp_skip_sse_sb;
  const COMPOUND_TYPE compound_type = mbmi->interinter_comp.type;

  best_rd_cur =
      pick_interinter_mask(cpi, x, bsize, *preds0, *preds1, residual1, diff10);
  *rs2 += get_interinter_compound_mask_rate(x, mbmi);
  best_rd_cur += RDCOST(x->rdmult, *rs2 + rate_mv, 0);

  if (have_newmv_in_inter_mode(this_mode) &&
      use_masked_motion_search(compound_type)) {
    *out_rate_mv = interinter_compound_motion_search(cpi, x, cur_mv, bsize,
                                                     this_mode, mi_row, mi_col);
    av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, ctx, bsize);
    av1_subtract_plane(x, bsize, 0);
    model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                    &tmp_skip_txfm_sb, &tmp_skip_sse_sb, NULL, NULL, NULL);
    rd = RDCOST(x->rdmult, *rs2 + *out_rate_mv + rate_sum, dist_sum);
    if (rd >= best_rd_cur) {
      mbmi->mv[0].as_int = cur_mv[0].as_int;
      mbmi->mv[1].as_int = cur_mv[1].as_int;
      *out_rate_mv = rate_mv;
      av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0, preds0, strides,
                                               preds1, strides);
    }
    rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                             &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
    if (rd != INT64_MAX)
      rd = RDCOST(x->rdmult, *rs2 + *out_rate_mv + rate_sum, dist_sum);
    best_rd_cur = rd;

  } else {
    av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0, preds0, strides,
                                             preds1, strides);
    rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                             &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
    if (rd != INT64_MAX)
      rd = RDCOST(x->rdmult, *rs2 + rate_mv + rate_sum, dist_sum);
    best_rd_cur = rd;
  }
  return best_rd_cur;
}

typedef struct {
  // OBMC secondary prediction buffers and respective strides
  uint8_t *above_pred_buf[MAX_MB_PLANE];
  int above_pred_stride[MAX_MB_PLANE];
  uint8_t *left_pred_buf[MAX_MB_PLANE];
  int left_pred_stride[MAX_MB_PLANE];
  int_mv (*single_newmv)[REF_FRAMES];
  // Pointer to array of motion vectors to use for each ref and their rates
  // Should point to first of 2 arrays in 2D array
  int (*single_newmv_rate)[REF_FRAMES];
  int (*single_newmv_valid)[REF_FRAMES];
  // Pointer to array of predicted rate-distortion
  // Should point to first of 2 arrays in 2D array
  int64_t (*modelled_rd)[REF_FRAMES];
  InterpFilter single_filter[MB_MODE_COUNT][REF_FRAMES];
  int ref_frame_cost;
  int single_comp_cost;
} HandleInterModeArgs;

static INLINE int clamp_and_check_mv(int_mv *out_mv, int_mv in_mv,
                                     const AV1_COMMON *cm,
                                     const MACROBLOCK *x) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  *out_mv = in_mv;
  lower_mv_precision(&out_mv->as_mv, cm->allow_high_precision_mv,
                     cm->cur_frame_force_integer_mv);
  clamp_mv2(&out_mv->as_mv, xd);
  return !mv_check_bounds(&x->mv_limits, &out_mv->as_mv);
}

static int64_t handle_newmv(const AV1_COMP *const cpi, MACROBLOCK *const x,
                            const BLOCK_SIZE bsize, int_mv *cur_mv,
                            const int mi_row, const int mi_col,
                            int *const rate_mv,
                            HandleInterModeArgs *const args) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;
  const int refs[2] = { mbmi->ref_frame[0],
                        mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1] };
  const int ref_mv_idx = mbmi->ref_mv_idx;
  int i;

  (void)args;

  if (is_comp_pred) {
    if (this_mode == NEW_NEWMV) {
      cur_mv[0].as_int = args->single_newmv[ref_mv_idx][refs[0]].as_int;
      cur_mv[1].as_int = args->single_newmv[ref_mv_idx][refs[1]].as_int;

      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        joint_motion_search(cpi, x, bsize, cur_mv, mi_row, mi_col, NULL, NULL,
                            0, rate_mv, 0);
      } else {
        *rate_mv = 0;
        for (i = 0; i < 2; ++i) {
          const int_mv ref_mv = av1_get_ref_mv(x, i);
          av1_set_mvcost(x, i, mbmi->ref_mv_idx);
          *rate_mv +=
              av1_mv_bit_cost(&cur_mv[i].as_mv, &ref_mv.as_mv, x->nmvjointcost,
                              x->mvcost, MV_COST_WEIGHT);
        }
      }
    } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV) {
      cur_mv[1].as_int = args->single_newmv[ref_mv_idx][refs[1]].as_int;
      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        compound_single_motion_search_interinter(
            cpi, x, bsize, cur_mv, mi_row, mi_col, NULL, 0, rate_mv, 0, 1);
      } else {
        av1_set_mvcost(x, 1,
                       mbmi->ref_mv_idx + (this_mode == NEAR_NEWMV ? 1 : 0));
        const int_mv ref_mv = av1_get_ref_mv(x, 1);
        *rate_mv = av1_mv_bit_cost(&cur_mv[1].as_mv, &ref_mv.as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
    } else {
      assert(this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV);
      cur_mv[0].as_int = args->single_newmv[ref_mv_idx][refs[0]].as_int;
      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        compound_single_motion_search_interinter(
            cpi, x, bsize, cur_mv, mi_row, mi_col, NULL, 0, rate_mv, 0, 0);
      } else {
        const int_mv ref_mv = av1_get_ref_mv(x, 0);
        av1_set_mvcost(x, 0,
                       mbmi->ref_mv_idx + (this_mode == NEW_NEARMV ? 1 : 0));
        *rate_mv = av1_mv_bit_cost(&cur_mv[0].as_mv, &ref_mv.as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
    }
  } else {
    single_motion_search(cpi, x, bsize, mi_row, mi_col, 0, rate_mv);
    if (x->best_mv.as_int == INVALID_MV) return INT64_MAX;

    args->single_newmv[ref_mv_idx][refs[0]] = x->best_mv;
    args->single_newmv_rate[ref_mv_idx][refs[0]] = *rate_mv;
    args->single_newmv_valid[ref_mv_idx][refs[0]] = 1;

    cur_mv[0].as_int = x->best_mv.as_int;

#if USE_DISCOUNT_NEWMV_TEST
    // Estimate the rate implications of a new mv but discount this
    // under certain circumstances where we want to help initiate a weak
    // motion field, where the distortion gain for a single block may not
    // be enough to overcome the cost of a new mv.
    if (discount_newmv_test(cpi, x, this_mode, x->best_mv)) {
      *rate_mv = AOMMAX(*rate_mv / NEW_MV_DISCOUNT_FACTOR, 1);
    }
#endif
  }

  return 0;
}

static INLINE void swap_dst_buf(MACROBLOCKD *xd, const BUFFER_SET *dst_bufs[2],
                                int num_planes) {
  const BUFFER_SET *buf0 = dst_bufs[0];
  dst_bufs[0] = dst_bufs[1];
  dst_bufs[1] = buf0;
  restore_dst_buf(xd, *dst_bufs[0], num_planes);
}

static INLINE int get_switchable_rate(MACROBLOCK *const x,
                                      const InterpFilters filters,
                                      const int ctx[2]) {
  int inter_filter_cost;
  const InterpFilter filter0 = av1_extract_interp_filter(filters, 0);
  const InterpFilter filter1 = av1_extract_interp_filter(filters, 1);
  inter_filter_cost = x->switchable_interp_costs[ctx[0]][filter0];
  inter_filter_cost += x->switchable_interp_costs[ctx[1]][filter1];
  return SWITCHABLE_INTERP_RATE_FACTOR * inter_filter_cost;
}

// calculate the rdcost of given interpolation_filter
static INLINE int64_t interpolation_filter_rd(
    MACROBLOCK *const x, const AV1_COMP *const cpi, BLOCK_SIZE bsize,
    int mi_row, int mi_col, BUFFER_SET *const orig_dst, int64_t *const rd,
    int *const switchable_rate, int *const skip_txfm_sb,
    int64_t *const skip_sse_sb, const BUFFER_SET *dst_bufs[2], int filter_idx,
    const int switchable_ctx[2], const int skip_pred, int *rate,
    int64_t *dist) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int tmp_rate, tmp_skip_sb = 0;
  int64_t tmp_dist, tmp_skip_sse = INT64_MAX;

  const InterpFilters last_best = mbmi->interp_filters;
  mbmi->interp_filters = filter_sets[filter_idx];
  const int tmp_rs =
      get_switchable_rate(x, mbmi->interp_filters, switchable_ctx);

  if (!skip_pred) {
    av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, orig_dst, bsize);
    av1_subtract_plane(x, bsize, 0);
#if DNN_BASED_RD_INTERP_FILTER
    model_rd_for_sb_with_dnn(cpi, bsize, x, xd, 0, 0, &tmp_rate, &tmp_dist,
                             &tmp_skip_sb, &tmp_skip_sse, NULL, NULL, NULL);
#else
    model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &tmp_rate, &tmp_dist, &tmp_skip_sb,
                    &tmp_skip_sse, NULL, NULL, NULL);
#endif
    if (num_planes > 1) {
      int64_t tmp_y_rd = RDCOST(x->rdmult, tmp_rs + tmp_rate, tmp_dist);
      if (tmp_y_rd > *rd) {
        mbmi->interp_filters = last_best;
        return 0;
      }
      int tmp_rate_uv, tmp_skip_sb_uv;
      int64_t tmp_dist_uv, tmp_skip_sse_uv;
      av1_build_inter_predictors_sbuv(cm, xd, mi_row, mi_col, orig_dst, bsize);
      for (int plane = 1; plane < num_planes; ++plane)
        av1_subtract_plane(x, bsize, plane);
#if DNN_BASED_RD_INTERP_FILTER
      model_rd_for_sb_with_dnn(cpi, bsize, x, xd, 1, num_planes - 1,
                               &tmp_rate_uv, &tmp_dist_uv, &tmp_skip_sb_uv,
                               &tmp_skip_sse_uv, NULL, NULL, NULL);
#else
      model_rd_for_sb(cpi, bsize, x, xd, 1, num_planes - 1, &tmp_rate_uv,
                      &tmp_dist_uv, &tmp_skip_sb_uv, &tmp_skip_sse_uv, NULL,
                      NULL, NULL);
#endif
      tmp_rate += tmp_rate_uv;
      tmp_skip_sb &= tmp_skip_sb_uv;
      tmp_dist += tmp_dist_uv;
      tmp_skip_sse += tmp_skip_sse_uv;
    }
  } else {
    tmp_rate = *rate;
    tmp_dist = *dist;
  }
  int64_t tmp_rd = RDCOST(x->rdmult, tmp_rs + tmp_rate, tmp_dist);
  if (tmp_rd < *rd) {
    *rd = tmp_rd;
    *switchable_rate = tmp_rs;
    *skip_txfm_sb = tmp_skip_sb;
    *skip_sse_sb = tmp_skip_sse;
    *rate = tmp_rate;
    *dist = tmp_dist;
    if (!skip_pred) {
      swap_dst_buf(xd, dst_bufs, num_planes);
    }
    return 1;
  }
  mbmi->interp_filters = last_best;
  return 0;
}

// Find the best rd filter in horizontal direction
static INLINE int find_best_horiz_interp_filter_rd(
    MACROBLOCK *const x, const AV1_COMP *const cpi, BLOCK_SIZE bsize,
    int mi_row, int mi_col, BUFFER_SET *const orig_dst, int64_t *const rd,
    int *const switchable_rate, int *const skip_txfm_sb,
    int64_t *const skip_sse_sb, const BUFFER_SET *dst_bufs[2],
    const int switchable_ctx[2], const int skip_hor, int *rate, int64_t *dist,
    int best_dual_mode) {
  int i;
  const int bw = block_size_wide[bsize];
  assert(best_dual_mode == 0);
  if ((bw <= 4) && (!skip_hor)) {
    int skip_pred = 1;
    // Process the filters in reverse order to enable reusing rate and
    // distortion (calcuated during EIGHTTAP_REGULAR) for MULTITAP_SHARP
    for (i = (SWITCHABLE_FILTERS - 1); i >= 1; --i) {
      if (interpolation_filter_rd(x, cpi, bsize, mi_row, mi_col, orig_dst, rd,
                                  switchable_rate, skip_txfm_sb, skip_sse_sb,
                                  dst_bufs, i, switchable_ctx, skip_pred, rate,
                                  dist)) {
        best_dual_mode = i;
      }
      skip_pred = 0;
    }
  } else {
    for (i = 1; i < SWITCHABLE_FILTERS; ++i) {
      if (interpolation_filter_rd(x, cpi, bsize, mi_row, mi_col, orig_dst, rd,
                                  switchable_rate, skip_txfm_sb, skip_sse_sb,
                                  dst_bufs, i, switchable_ctx, skip_hor, rate,
                                  dist)) {
        best_dual_mode = i;
      }
    }
  }
  return best_dual_mode;
}

// Find the best rd filter in vertical direction
static INLINE void find_best_vert_interp_filter_rd(
    MACROBLOCK *const x, const AV1_COMP *const cpi, BLOCK_SIZE bsize,
    int mi_row, int mi_col, BUFFER_SET *const orig_dst, int64_t *const rd,
    int *const switchable_rate, int *const skip_txfm_sb,
    int64_t *const skip_sse_sb, const BUFFER_SET *dst_bufs[2],
    const int switchable_ctx[2], const int skip_ver, int *rate, int64_t *dist,
    int best_dual_mode, int filter_set_size) {
  int i;
  const int bh = block_size_high[bsize];
  if ((bh <= 4) && (!skip_ver)) {
    int skip_pred = 1;
    // Process the filters in reverse order to enable reusing rate and
    // distortion (calcuated during EIGHTTAP_REGULAR) for MULTITAP_SHARP
    assert(filter_set_size == DUAL_FILTER_SET_SIZE);
    for (i = (filter_set_size - SWITCHABLE_FILTERS + best_dual_mode);
         i >= (best_dual_mode + SWITCHABLE_FILTERS); i -= SWITCHABLE_FILTERS) {
      interpolation_filter_rd(x, cpi, bsize, mi_row, mi_col, orig_dst, rd,
                              switchable_rate, skip_txfm_sb, skip_sse_sb,
                              dst_bufs, i, switchable_ctx, skip_pred, rate,
                              dist);
      skip_pred = 0;
    }
  } else {
    for (i = best_dual_mode + SWITCHABLE_FILTERS; i < filter_set_size;
         i += SWITCHABLE_FILTERS) {
      interpolation_filter_rd(x, cpi, bsize, mi_row, mi_col, orig_dst, rd,
                              switchable_rate, skip_txfm_sb, skip_sse_sb,
                              dst_bufs, i, switchable_ctx, skip_ver, rate,
                              dist);
    }
  }
}

// check if there is saved result match with this search
static INLINE int is_interp_filter_match(const INTERPOLATION_FILTER_STATS *st,
                                         MB_MODE_INFO *const mi) {
  for (int i = 0; i < 2; ++i) {
    if ((st->ref_frames[i] != mi->ref_frame[i]) ||
        (st->mv[i].as_int != mi->mv[i].as_int)) {
      return 0;
    }
  }
  return 1;
}

static INLINE int find_interp_filter_in_stats(MACROBLOCK *x,
                                              MB_MODE_INFO *const mbmi) {
  const int comp_idx = mbmi->compound_idx;
  const int offset = x->interp_filter_stats_idx[comp_idx];
  for (int j = 0; j < offset; ++j) {
    const INTERPOLATION_FILTER_STATS *st = &x->interp_filter_stats[comp_idx][j];
    if (is_interp_filter_match(st, mbmi)) {
      mbmi->interp_filters = st->filters;
      return j;
    }
  }
  return -1;  // no match result found
}

static INLINE void save_interp_filter_search_stat(MACROBLOCK *x,
                                                  MB_MODE_INFO *const mbmi) {
  const int comp_idx = mbmi->compound_idx;
  const int offset = x->interp_filter_stats_idx[comp_idx];
  if (offset < MAX_INTERP_FILTER_STATS) {
    INTERPOLATION_FILTER_STATS stat = {
      mbmi->interp_filters,
      { mbmi->mv[0], mbmi->mv[1] },
      { mbmi->ref_frame[0], mbmi->ref_frame[1] },
    };
    x->interp_filter_stats[comp_idx][offset] = stat;
    x->interp_filter_stats_idx[comp_idx]++;
  }
}

static int64_t interpolation_filter_search(
    MACROBLOCK *const x, const AV1_COMP *const cpi, BLOCK_SIZE bsize,
    int mi_row, int mi_col, const BUFFER_SET *const tmp_dst,
    BUFFER_SET *const orig_dst, InterpFilter (*const single_filter)[REF_FRAMES],
    int64_t *const rd, int *const switchable_rate, int *const skip_txfm_sb,
    int64_t *const skip_sse_sb) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int need_search =
      av1_is_interp_needed(xd) && av1_is_interp_search_needed(xd);
  int i, tmp_rate;
  int64_t tmp_dist;

  (void)single_filter;
  int match_found = -1;
  const InterpFilter assign_filter = cm->interp_filter;
  if (cpi->sf.skip_repeat_interpolation_filter_search && need_search) {
    match_found = find_interp_filter_in_stats(x, mbmi);
  }
  if (!need_search || match_found == -1) {
    set_default_interp_filters(mbmi, assign_filter);
  }
  int switchable_ctx[2];
  switchable_ctx[0] = av1_get_pred_context_switchable_interp(xd, 0);
  switchable_ctx[1] = av1_get_pred_context_switchable_interp(xd, 1);
  *switchable_rate =
      get_switchable_rate(x, mbmi->interp_filters, switchable_ctx);
  av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
  for (int plane = 0; plane < num_planes; ++plane)
    av1_subtract_plane(x, bsize, plane);
#if DNN_BASED_RD_INTERP_FILTER
  model_rd_for_sb_with_dnn(cpi, bsize, x, xd, 0, num_planes - 1, &tmp_rate,
                           &tmp_dist, skip_txfm_sb, skip_sse_sb, NULL, NULL,
                           NULL);
#else
  model_rd_for_sb(cpi, bsize, x, xd, 0, num_planes - 1, &tmp_rate, &tmp_dist,
                  skip_txfm_sb, skip_sse_sb, NULL, NULL, NULL);
#endif  // DNN_BASED_RD_INTERP_FILTER
  *rd = RDCOST(x->rdmult, *switchable_rate + tmp_rate, tmp_dist);

  if (assign_filter != SWITCHABLE || match_found != -1) {
    return 0;
  }
  if (!need_search) {
    assert(mbmi->interp_filters ==
           av1_broadcast_interp_filter(EIGHTTAP_REGULAR));
    return 0;
  }
  int skip_hor = 1;
  int skip_ver = 1;
  const int is_compound = has_second_ref(mbmi);
  for (int k = 0; k < num_planes - 1; ++k) {
    struct macroblockd_plane *const pd = &xd->plane[k];
    const int bw = pd->width;
    const int bh = pd->height;
    for (int j = 0; j < 1 + is_compound; ++j) {
      const MV mv = mbmi->mv[j].as_mv;
      const MV mv_q4 = clamp_mv_to_umv_border_sb(
          xd, &mv, bw, bh, pd->subsampling_x, pd->subsampling_y);
      const int sub_x = (mv_q4.col & SUBPEL_MASK) << SCALE_EXTRA_BITS;
      const int sub_y = (mv_q4.row & SUBPEL_MASK) << SCALE_EXTRA_BITS;
      skip_hor &= (sub_x == 0);
      skip_ver &= (sub_y == 0);
    }
  }
  // do interp_filter search
  const int filter_set_size = DUAL_FILTER_SET_SIZE;
  restore_dst_buf(xd, *tmp_dst, num_planes);
  const BUFFER_SET *dst_bufs[2] = { tmp_dst, orig_dst };
  if (cpi->sf.use_fast_interpolation_filter_search &&
      cm->seq_params.enable_dual_filter) {
    // default to (R,R): EIGHTTAP_REGULARxEIGHTTAP_REGULAR
    int best_dual_mode = 0;
    // Find best of {R}x{R,Sm,Sh}
    // EIGHTTAP_REGULAR mode is calculated beforehand
    best_dual_mode = find_best_horiz_interp_filter_rd(
        x, cpi, bsize, mi_row, mi_col, orig_dst, rd, switchable_rate,
        skip_txfm_sb, skip_sse_sb, dst_bufs, switchable_ctx, skip_hor,
        &tmp_rate, &tmp_dist, best_dual_mode);

    // From best of horizontal EIGHTTAP_REGULAR modes, check vertical modes
    find_best_vert_interp_filter_rd(
        x, cpi, bsize, mi_row, mi_col, orig_dst, rd, switchable_rate,
        skip_txfm_sb, skip_sse_sb, dst_bufs, switchable_ctx, skip_ver,
        &tmp_rate, &tmp_dist, best_dual_mode, filter_set_size);
  } else {
    // EIGHTTAP_REGULAR mode is calculated beforehand
    for (i = 1; i < filter_set_size; ++i) {
      if (cm->seq_params.enable_dual_filter == 0) {
        const int16_t filter_y = filter_sets[i] & 0xffff;
        const int16_t filter_x = filter_sets[i] >> 16;
        if (filter_x != filter_y) continue;
      }
      interpolation_filter_rd(x, cpi, bsize, mi_row, mi_col, orig_dst, rd,
                              switchable_rate, skip_txfm_sb, skip_sse_sb,
                              dst_bufs, i, switchable_ctx, 0, &tmp_rate,
                              &tmp_dist);
    }
  }
  swap_dst_buf(xd, dst_bufs, num_planes);
  // save search results
  if (cpi->sf.skip_repeat_interpolation_filter_search) {
    assert(match_found == -1);
    save_interp_filter_search_stat(x, mbmi);
  }
  return 0;
}

// TODO(afergs): Refactor the MBMI references in here - there's four
// TODO(afergs): Refactor optional args - add them to a struct or remove
static int64_t motion_mode_rd(const AV1_COMP *const cpi, MACROBLOCK *const x,
                              BLOCK_SIZE bsize, RD_STATS *rd_stats,
                              RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv,
                              int *disable_skip, int mi_row, int mi_col,
                              HandleInterModeArgs *const args,
                              int64_t ref_best_rd, const int *refs, int rate_mv,
                              BUFFER_SET *orig_dst
#if CONFIG_COLLECT_INTER_MODE_RD_STATS
                              ,
                              int64_t *best_est_rd
#endif
) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_comp_pred = has_second_ref(mbmi);
  const PREDICTION_MODE this_mode = mbmi->mode;
  int rate2_nocoeff = 0, best_xskip, best_disable_skip = 0;
  RD_STATS best_rd_stats, best_rd_stats_y, best_rd_stats_uv;
  MB_MODE_INFO base_mbmi, best_mbmi;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  int interintra_allowed = cm->seq_params.enable_interintra_compound &&
                           is_interintra_allowed(mbmi) && mbmi->compound_idx;
  int pts0[SAMPLES_ARRAY_SIZE], pts_inref0[SAMPLES_ARRAY_SIZE];
  int total_samples;

  (void)rate_mv;

  av1_invalid_rd_stats(&best_rd_stats);

  aom_clear_system_state();
  mbmi->num_proj_ref[0] = findSamples(cm, xd, mi_row, mi_col, pts0, pts_inref0);
  total_samples = mbmi->num_proj_ref[0];
  rate2_nocoeff = rd_stats->rate;
  base_mbmi = *mbmi;
  MOTION_MODE last_motion_mode_allowed =
      cm->switchable_motion_mode
          ? motion_mode_allowed(xd->global_motion, xd, mbmi,
                                cm->allow_warped_motion)
          : SIMPLE_TRANSLATION;
  assert(mbmi->ref_frame[1] != INTRA_FRAME);
  const MV_REFERENCE_FRAME ref_frame_1 = mbmi->ref_frame[1];

  int64_t best_rd = INT64_MAX;

  for (int mode_index = (int)SIMPLE_TRANSLATION;
       mode_index <= (int)last_motion_mode_allowed + interintra_allowed;
       mode_index++) {
    int64_t tmp_rd = INT64_MAX;
    int tmp_rate2 = rate2_nocoeff;
    int is_interintra_mode = mode_index > (int)last_motion_mode_allowed;
    int skip_txfm_sb = 0;

    *mbmi = base_mbmi;
    if (is_interintra_mode) {
      mbmi->motion_mode = SIMPLE_TRANSLATION;
    } else {
      mbmi->motion_mode = (MOTION_MODE)mode_index;
      assert(mbmi->ref_frame[1] != INTRA_FRAME);
    }

    if (mbmi->motion_mode == SIMPLE_TRANSLATION && !is_interintra_mode) {
      // SIMPLE_TRANSLATION mode: no need to recalculate.
      // The prediction is calculated before motion_mode_rd() is called in
      // handle_inter_mode()
    } else if (mbmi->motion_mode == OBMC_CAUSAL) {
      mbmi->motion_mode = OBMC_CAUSAL;
      if (!is_comp_pred && have_newmv_in_inter_mode(this_mode)) {
        int tmp_rate_mv = 0;

        single_motion_search(cpi, x, bsize, mi_row, mi_col, 0, &tmp_rate_mv);
        mbmi->mv[0].as_int = x->best_mv.as_int;
#if USE_DISCOUNT_NEWMV_TEST
        if (discount_newmv_test(cpi, x, this_mode, mbmi->mv[0])) {
          tmp_rate_mv = AOMMAX((tmp_rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
        }
#endif
        tmp_rate2 = rate2_nocoeff - rate_mv + tmp_rate_mv;
      }
      av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
      av1_build_obmc_inter_prediction(
          cm, xd, mi_row, mi_col, args->above_pred_buf, args->above_pred_stride,
          args->left_pred_buf, args->left_pred_stride);
    } else if (mbmi->motion_mode == WARPED_CAUSAL) {
      int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
      mbmi->motion_mode = WARPED_CAUSAL;
      mbmi->wm_params[0].wmtype = DEFAULT_WMTYPE;
      mbmi->interp_filters = av1_broadcast_interp_filter(
          av1_unswitchable_filter(cm->interp_filter));

      memcpy(pts, pts0, total_samples * 2 * sizeof(*pts0));
      memcpy(pts_inref, pts_inref0, total_samples * 2 * sizeof(*pts_inref0));
      // Select the samples according to motion vector difference
      if (mbmi->num_proj_ref[0] > 1) {
        mbmi->num_proj_ref[0] = selectSamples(
            &mbmi->mv[0].as_mv, pts, pts_inref, mbmi->num_proj_ref[0], bsize);
      }

      if (!find_projection(mbmi->num_proj_ref[0], pts, pts_inref, bsize,
                           mbmi->mv[0].as_mv.row, mbmi->mv[0].as_mv.col,
                           &mbmi->wm_params[0], mi_row, mi_col)) {
        // Refine MV for NEWMV mode
        if (!is_comp_pred && have_newmv_in_inter_mode(this_mode)) {
          int tmp_rate_mv = 0;
          const int_mv mv0 = mbmi->mv[0];
          const WarpedMotionParams wm_params0 = mbmi->wm_params[0];
          int num_proj_ref0 = mbmi->num_proj_ref[0];

          // Refine MV in a small range.
          av1_refine_warped_mv(cpi, x, bsize, mi_row, mi_col, pts0, pts_inref0,
                               total_samples);

          // Keep the refined MV and WM parameters.
          if (mv0.as_int != mbmi->mv[0].as_int) {
            const int ref = refs[0];
            const int_mv ref_mv = av1_get_ref_mv(x, 0);
            tmp_rate_mv =
                av1_mv_bit_cost(&mbmi->mv[0].as_mv, &ref_mv.as_mv,
                                x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);

            if (cpi->sf.adaptive_motion_search)
              x->pred_mv[ref] = mbmi->mv[0].as_mv;

#if USE_DISCOUNT_NEWMV_TEST
            if (discount_newmv_test(cpi, x, this_mode, mbmi->mv[0])) {
              tmp_rate_mv = AOMMAX((tmp_rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
            }
#endif
            tmp_rate2 = rate2_nocoeff - rate_mv + tmp_rate_mv;
          } else {
            // Restore the old MV and WM parameters.
            mbmi->mv[0] = mv0;
            mbmi->wm_params[0] = wm_params0;
            mbmi->num_proj_ref[0] = num_proj_ref0;
          }
        }

        av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
      } else {
        continue;
      }
    } else if (is_interintra_mode) {
      INTERINTRA_MODE best_interintra_mode = II_DC_PRED;
      int64_t rd, best_interintra_rd = INT64_MAX;
      int rmode, rate_sum;
      int64_t dist_sum;
      int j;
      int tmp_rate_mv = 0;
      int tmp_skip_txfm_sb;
      int bw = block_size_wide[bsize];
      int64_t tmp_skip_sse_sb;
      DECLARE_ALIGNED(16, uint8_t, intrapred_[2 * MAX_INTERINTRA_SB_SQUARE]);
      DECLARE_ALIGNED(16, uint8_t, tmp_buf_[2 * MAX_INTERINTRA_SB_SQUARE]);
      uint8_t *tmp_buf, *intrapred;
      const int *const interintra_mode_cost =
          x->interintra_mode_cost[size_group_lookup[bsize]];

      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        tmp_buf = CONVERT_TO_BYTEPTR(tmp_buf_);
        intrapred = CONVERT_TO_BYTEPTR(intrapred_);
      } else {
        tmp_buf = tmp_buf_;
        intrapred = intrapred_;
      }
      const int_mv mv0 = mbmi->mv[0];

      mbmi->ref_frame[1] = NONE_FRAME;
      xd->plane[0].dst.buf = tmp_buf;
      xd->plane[0].dst.stride = bw;
      av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, NULL, bsize);

      restore_dst_buf(xd, *orig_dst, num_planes);
      mbmi->ref_frame[1] = INTRA_FRAME;
      mbmi->use_wedge_interintra = 0;
      for (j = 0; j < INTERINTRA_MODES; ++j) {
        mbmi->interintra_mode = (INTERINTRA_MODE)j;
        rmode = interintra_mode_cost[mbmi->interintra_mode];
        av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                                  intrapred, bw);
        av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
        av1_subtract_plane(x, bsize, 0);
        model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                        &tmp_skip_txfm_sb, &tmp_skip_sse_sb, NULL, NULL, NULL);
        rd = RDCOST(x->rdmult, tmp_rate_mv + rate_sum + rmode, dist_sum);
        if (rd < best_interintra_rd) {
          best_interintra_rd = rd;
          best_interintra_mode = mbmi->interintra_mode;
        }
      }
      mbmi->interintra_mode = best_interintra_mode;
      rmode = interintra_mode_cost[mbmi->interintra_mode];
      av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                                intrapred, bw);
      av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
      rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                               &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
      if (rd != INT64_MAX)
        rd = RDCOST(x->rdmult, rate_mv + rmode + rate_sum, dist_sum);
      best_interintra_rd = rd;

      if (ref_best_rd < INT64_MAX && (best_interintra_rd >> 1) > ref_best_rd) {
        // restore ref_frame[1]
        mbmi->ref_frame[1] = ref_frame_1;
        continue;
      }

      if (is_interintra_wedge_used(bsize)) {
        int64_t best_interintra_rd_nowedge = INT64_MAX;
        int64_t best_interintra_rd_wedge = INT64_MAX;
        int_mv tmp_mv;
        InterpFilters backup_interp_filters = mbmi->interp_filters;
        int rwedge = x->wedge_interintra_cost[bsize][0];
        if (rd != INT64_MAX)
          rd = RDCOST(x->rdmult, rate_mv + rmode + rate_sum + rwedge, dist_sum);
        best_interintra_rd_nowedge = rd;

        // Disable wedge search if source variance is small
        if (x->source_variance > cpi->sf.disable_wedge_search_var_thresh) {
          mbmi->use_wedge_interintra = 1;

          rwedge = av1_cost_literal(get_interintra_wedge_bits(bsize)) +
                   x->wedge_interintra_cost[bsize][1];

          best_interintra_rd_wedge =
              pick_interintra_wedge(cpi, x, bsize, intrapred_, tmp_buf_);

          best_interintra_rd_wedge +=
              RDCOST(x->rdmult, rmode + rate_mv + rwedge, 0);
          // Refine motion vector.
          if (have_newmv_in_inter_mode(mbmi->mode)) {
            // get negative of mask
            const uint8_t *mask = av1_get_contiguous_soft_mask(
                mbmi->interintra_wedge_index, 1, bsize);
            tmp_mv = av1_get_ref_mv(x, 0);
            compound_single_motion_search(cpi, x, bsize, &tmp_mv.as_mv, mi_row,
                                          mi_col, intrapred, mask, bw,
                                          &tmp_rate_mv, 0);
            mbmi->mv[0].as_int = tmp_mv.as_int;
            av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, orig_dst,
                                           bsize);
            av1_subtract_plane(x, bsize, 0);
            model_rd_for_sb(cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                            &tmp_skip_txfm_sb, &tmp_skip_sse_sb, NULL, NULL,
                            NULL);
            rd = RDCOST(x->rdmult, tmp_rate_mv + rmode + rate_sum + rwedge,
                        dist_sum);
            if (rd >= best_interintra_rd_wedge) {
              tmp_mv.as_int = mv0.as_int;
              tmp_rate_mv = rate_mv;
              mbmi->interp_filters = backup_interp_filters;
              av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
            }
          } else {
            tmp_mv.as_int = mv0.as_int;
            tmp_rate_mv = rate_mv;
            av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
          }
          // Evaluate closer to true rd
          rd = estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                                   &tmp_skip_txfm_sb, &tmp_skip_sse_sb,
                                   INT64_MAX);
          if (rd != INT64_MAX)
            rd = RDCOST(x->rdmult, rmode + tmp_rate_mv + rwedge + rate_sum,
                        dist_sum);
          best_interintra_rd_wedge = rd;
          if (best_interintra_rd_wedge < best_interintra_rd_nowedge) {
            mbmi->use_wedge_interintra = 1;
            mbmi->mv[0].as_int = tmp_mv.as_int;
            tmp_rate2 += tmp_rate_mv - rate_mv;
          } else {
            mbmi->use_wedge_interintra = 0;
            mbmi->mv[0].as_int = mv0.as_int;
            mbmi->interp_filters = backup_interp_filters;
          }
        } else {
          mbmi->use_wedge_interintra = 0;
        }
      }  // if (is_interintra_wedge_used(bsize))
      restore_dst_buf(xd, *orig_dst, num_planes);
      av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);
    }

    if (!cpi->common.all_lossless)
      check_block_skip(cpi, bsize, x, xd, 0, num_planes - 1, &skip_txfm_sb);

    x->skip = 0;

    rd_stats->dist = 0;
    rd_stats->sse = 0;
    rd_stats->skip = 1;
    rd_stats->rate = tmp_rate2;
    if (av1_is_interp_needed(xd))
      rd_stats->rate += av1_get_switchable_rate(cm, x, xd);
    if (interintra_allowed) {
      rd_stats->rate += x->interintra_cost[size_group_lookup[bsize]]
                                          [mbmi->ref_frame[1] == INTRA_FRAME];
      if (mbmi->ref_frame[1] == INTRA_FRAME) {
        rd_stats->rate += x->interintra_mode_cost[size_group_lookup[bsize]]
                                                 [mbmi->interintra_mode];
        if (is_interintra_wedge_used(bsize)) {
          rd_stats->rate +=
              x->wedge_interintra_cost[bsize][mbmi->use_wedge_interintra];
          if (mbmi->use_wedge_interintra) {
            rd_stats->rate +=
                av1_cost_literal(get_interintra_wedge_bits(bsize));
          }
        }
      }
    }
    if ((last_motion_mode_allowed > SIMPLE_TRANSLATION) &&
        (mbmi->ref_frame[1] != INTRA_FRAME)) {
      if (last_motion_mode_allowed == WARPED_CAUSAL) {
        rd_stats->rate += x->motion_mode_cost[bsize][mbmi->motion_mode];
      } else {
        rd_stats->rate += x->motion_mode_cost1[bsize][mbmi->motion_mode];
      }
    }
    if (!skip_txfm_sb) {
#if CONFIG_COLLECT_INTER_MODE_RD_STATS
      int64_t est_rd = 0;
      int est_skip = 0;
      if (cpi->sf.inter_mode_rd_model_estimation) {
        InterModeRdModel *md = &inter_mode_rd_models[mbmi->sb_type];
        if (md->ready) {
          const int64_t curr_sse = get_sse(cpi, x);
          est_rd =
              get_est_rd(mbmi->sb_type, x->rdmult, curr_sse, rd_stats->rate);
          est_skip = est_rd * 0.8 > *best_est_rd;
#if INTER_MODE_RD_TEST
          if (est_rd < *best_est_rd) {
            *best_est_rd = est_rd;
          }
#else   // INTER_MODE_RD_TEST
          if (est_skip) {
            ++md->skip_count;
            mbmi->ref_frame[1] = ref_frame_1;
            continue;
          } else {
            if (est_rd < *best_est_rd) {
              *best_est_rd = est_rd;
            }
            ++md->non_skip_count;
          }
#endif  // INTER_MODE_RD_TEST
        }
      }
#endif  // CONFIG_COLLECT_INTER_MODE_RD_STATS

      int64_t rdcosty = INT64_MAX;
      int is_cost_valid_uv = 0;

      // cost and distortion
      av1_subtract_plane(x, bsize, 0);
      if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id]) {
        // Motion mode
        select_tx_type_yrd(cpi, x, rd_stats_y, bsize, mi_row, mi_col,
                           ref_best_rd);
#if CONFIG_COLLECT_RD_STATS == 2
        PrintPredictionUnitStats(cpi, x, rd_stats_y, bsize);
#endif  // CONFIG_COLLECT_RD_STATS == 2
      } else {
        super_block_yrd(cpi, x, rd_stats_y, bsize, ref_best_rd);
        memset(mbmi->inter_tx_size, mbmi->tx_size, sizeof(mbmi->inter_tx_size));
        memset(x->blk_skip, rd_stats_y->skip,
               sizeof(x->blk_skip[0]) * xd->n8_h * xd->n8_w);
      }

      if (rd_stats_y->rate == INT_MAX) {
        av1_invalid_rd_stats(rd_stats);
        if (mbmi->motion_mode != SIMPLE_TRANSLATION ||
            mbmi->ref_frame[1] == INTRA_FRAME) {
          mbmi->ref_frame[1] = ref_frame_1;
          continue;
        } else {
          restore_dst_buf(xd, *orig_dst, num_planes);
          mbmi->ref_frame[1] = ref_frame_1;
          return INT64_MAX;
        }
      }

      av1_merge_rd_stats(rd_stats, rd_stats_y);

      rdcosty = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
      rdcosty = AOMMIN(rdcosty, RDCOST(x->rdmult, 0, rd_stats->sse));
      if (num_planes > 1) {
        /* clang-format off */
        is_cost_valid_uv =
            inter_block_uvrd(cpi, x, rd_stats_uv, bsize, ref_best_rd - rdcosty,
                             FTXS_NONE);
        if (!is_cost_valid_uv) {
          mbmi->ref_frame[1] = ref_frame_1;
          continue;
        }
        /* clang-format on */
        av1_merge_rd_stats(rd_stats, rd_stats_uv);
      } else {
        av1_init_rd_stats(rd_stats_uv);
      }
#if CONFIG_RD_DEBUG
      // record transform block coefficient cost
      // TODO(angiebird): So far rd_debug tool only detects discrepancy of
      // coefficient cost. Therefore, it is fine to copy rd_stats into mbmi
      // here because we already collect the coefficient cost. Move this part to
      // other place when we need to compare non-coefficient cost.
      mbmi->rd_stats = *rd_stats;
#endif  // CONFIG_RD_DEBUG
      const int skip_ctx = av1_get_skip_context(xd);
      if (rd_stats->skip) {
        rd_stats->rate -= rd_stats_uv->rate + rd_stats_y->rate;
        rd_stats_y->rate = 0;
        rd_stats_uv->rate = 0;
        rd_stats->rate += x->skip_cost[skip_ctx][1];
        mbmi->skip = 0;
        // here mbmi->skip temporarily plays a role as what this_skip2 does
      } else if (!xd->lossless[mbmi->segment_id] &&
                 (RDCOST(x->rdmult,
                         rd_stats_y->rate + rd_stats_uv->rate +
                             x->skip_cost[skip_ctx][0],
                         rd_stats->dist) >= RDCOST(x->rdmult,
                                                   x->skip_cost[skip_ctx][1],
                                                   rd_stats->sse))) {
        rd_stats->rate -= rd_stats_uv->rate + rd_stats_y->rate;
        rd_stats->rate += x->skip_cost[skip_ctx][1];
        rd_stats->dist = rd_stats->sse;
        rd_stats_y->rate = 0;
        rd_stats_uv->rate = 0;
        mbmi->skip = 1;
      } else {
        rd_stats->rate += x->skip_cost[skip_ctx][0];
        mbmi->skip = 0;
      }
      *disable_skip = 0;
#if CONFIG_COLLECT_INTER_MODE_RD_STATS
      if (cpi->sf.inter_mode_rd_model_estimation && cm->tile_cols == 1 &&
          cm->tile_rows == 1) {
#if INTER_MODE_RD_TEST
        if (md->ready) {
          int64_t real_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
          if (est_skip) {
            ++md->skip_count;
            if (real_rd < ref_best_rd) {
              ++md->fp_skip_count;
            }
            // int fp_skip = real_rd < ref_best_rd;
            // printf("est_skip %d fp_skip %d est_rd %ld best_est_rd %ld real_rd
            // %ld ref_best_rd %ld\n",
            //        est_skip, fp_skip, est_rd, *best_est_rd, real_rd,
            //        ref_best_rd);
          } else {
            ++md->non_skip_count;
          }
        }
#endif  // INTER_MODE_RD_TEST
        inter_mode_data_push(mbmi->sb_type, rd_stats->sse, rd_stats->dist,
                             rd_stats_y->rate + rd_stats_uv->rate +
                                 x->skip_cost[skip_ctx][mbmi->skip],
                             rd_stats->rate, ref_best_rd);
      }
#endif  // CONFIG_COLLECT_INTER_MODE_RD_STATS
      int64_t curr_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
      if (curr_rd < ref_best_rd) {
        ref_best_rd = curr_rd;
      }
    } else {
      x->skip = 1;
      *disable_skip = 1;
      mbmi->tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode);

      // The cost of skip bit needs to be added.
      mbmi->skip = 0;
      rd_stats->rate += x->skip_cost[av1_get_skip_context(xd)][1];

      rd_stats->dist = 0;
      rd_stats->sse = 0;
      rd_stats_y->rate = 0;
      rd_stats_uv->rate = 0;
      rd_stats->skip = 1;
    }

    if (this_mode == GLOBALMV || this_mode == GLOBAL_GLOBALMV) {
      if (is_nontrans_global_motion(xd, xd->mi[0])) {
        mbmi->interp_filters = av1_broadcast_interp_filter(
            av1_unswitchable_filter(cm->interp_filter));
      }
    }

    tmp_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
    if ((mbmi->motion_mode == SIMPLE_TRANSLATION &&
         mbmi->ref_frame[1] != INTRA_FRAME) ||
        (tmp_rd < best_rd)) {
      best_mbmi = *mbmi;
      best_rd = tmp_rd;
      best_rd_stats = *rd_stats;
      best_rd_stats_y = *rd_stats_y;
      if (num_planes > 1) best_rd_stats_uv = *rd_stats_uv;
      memcpy(best_blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * xd->n8_h * xd->n8_w);
      best_xskip = x->skip;
      best_disable_skip = *disable_skip;
      if (best_xskip) break;
    }
  }
  mbmi->ref_frame[1] = ref_frame_1;

  if (best_rd == INT64_MAX) {
    av1_invalid_rd_stats(rd_stats);
    restore_dst_buf(xd, *orig_dst, num_planes);
    return INT64_MAX;
  }
  *mbmi = best_mbmi;
  *rd_stats = best_rd_stats;
  *rd_stats_y = best_rd_stats_y;
  if (num_planes > 1) *rd_stats_uv = best_rd_stats_uv;
  memcpy(x->blk_skip, best_blk_skip,
         sizeof(x->blk_skip[0]) * xd->n8_h * xd->n8_w);
  x->skip = best_xskip;
  *disable_skip = best_disable_skip;

  restore_dst_buf(xd, *orig_dst, num_planes);
  return 0;
}

static int64_t skip_mode_rd(RD_STATS *rd_stats, const AV1_COMP *const cpi,
                            MACROBLOCK *const x, BLOCK_SIZE bsize, int mi_row,
                            int mi_col, BUFFER_SET *const orig_dst) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, orig_dst, bsize);

  int64_t total_sse = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblock_plane *const p = &x->plane[plane];
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];

    av1_subtract_plane(x, bsize, plane);
    int64_t sse = aom_sum_squares_2d_i16(p->src_diff, bw, bw, bh);
    sse = sse << 4;
    total_sse += sse;
  }
  const int skip_mode_ctx = av1_get_skip_mode_context(xd);
  rd_stats->dist = rd_stats->sse = total_sse;
  rd_stats->rate = x->skip_mode_cost[skip_mode_ctx][1];
  rd_stats->rdcost = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);

  restore_dst_buf(xd, *orig_dst, num_planes);
  return 0;
}

#ifndef NDEBUG
static INLINE int is_single_inter_mode(int this_mode) {
  return this_mode >= SINGLE_INTER_MODE_START &&
         this_mode < SINGLE_INTER_MODE_END;
}
#endif

static INLINE int get_ref_mv_offset(int single_mode, uint8_t ref_mv_idx) {
  assert(is_single_inter_mode(single_mode));
  int ref_mv_offset;
  if (single_mode == NEARESTMV) {
    ref_mv_offset = 0;
  } else if (single_mode == NEARMV) {
    ref_mv_offset = ref_mv_idx + 1;
  } else {
    ref_mv_offset = -1;
  }
  return ref_mv_offset;
}

static INLINE void get_this_mv(int_mv *this_mv, int this_mode, int ref_idx,
                               int ref_mv_idx,
                               const MV_REFERENCE_FRAME *ref_frame,
                               const MB_MODE_INFO_EXT *mbmi_ext) {
  const uint8_t ref_frame_type = av1_ref_frame_type(ref_frame);
  const int is_comp_pred = ref_frame[1] > INTRA_FRAME;
  const int single_mode = get_single_mode(this_mode, ref_idx, is_comp_pred);
  assert(is_single_inter_mode(single_mode));
  if (single_mode == NEWMV) {
    this_mv->as_int = INVALID_MV;
  } else if (single_mode == GLOBALMV) {
    *this_mv = mbmi_ext->global_mvs[ref_frame[ref_idx]];
  } else {
    assert(single_mode == NEARMV || single_mode == NEARESTMV);
    const int ref_mv_offset = get_ref_mv_offset(single_mode, ref_mv_idx);
    if (ref_mv_offset < mbmi_ext->ref_mv_count[ref_frame_type]) {
      assert(ref_mv_offset >= 0);
      if (ref_idx == 0) {
        *this_mv =
            mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_offset].this_mv;
      } else {
        *this_mv =
            mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_offset].comp_mv;
      }
    } else {
      *this_mv = mbmi_ext->global_mvs[ref_frame[ref_idx]];
    }
  }
}

// This function update the non-new mv for the current prediction mode
static INLINE int build_cur_mv(int_mv *cur_mv, int this_mode,
                               const AV1_COMMON *cm, const MACROBLOCK *x) {
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_comp_pred = has_second_ref(mbmi);
  int ret = 1;
  for (int i = 0; i < is_comp_pred + 1; ++i) {
    int_mv this_mv;
    get_this_mv(&this_mv, this_mode, i, mbmi->ref_mv_idx, mbmi->ref_frame,
                x->mbmi_ext);
    const int single_mode = get_single_mode(this_mode, i, is_comp_pred);
    if (single_mode == NEWMV) {
      cur_mv[i] = this_mv;
    } else {
      ret &= clamp_and_check_mv(cur_mv + i, this_mv, cm, x);
    }
  }
  return ret;
}

static INLINE int get_drl_cost(const MB_MODE_INFO *mbmi,
                               const MB_MODE_INFO_EXT *mbmi_ext,
                               int (*drl_mode_cost0)[2],
                               int8_t ref_frame_type) {
  int cost = 0;
  if (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV) {
    for (int idx = 0; idx < 2; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
        cost += drl_mode_cost0[drl_ctx][mbmi->ref_mv_idx != idx];
        if (mbmi->ref_mv_idx == idx) return cost;
      }
    }
    return cost;
  }

  if (have_nearmv_in_inter_mode(mbmi->mode)) {
    for (int idx = 1; idx < 3; ++idx) {
      if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx =
            av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
        cost += drl_mode_cost0[drl_ctx][mbmi->ref_mv_idx != (idx - 1)];
        if (mbmi->ref_mv_idx == (idx - 1)) return cost;
      }
    }
    return cost;
  }
  return cost;
}

static INLINE int compound_type_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   BLOCK_SIZE bsize, int mi_col, int mi_row,
                                   int_mv *cur_mv, int masked_compound_used,
                                   BUFFER_SET *orig_dst, BUFFER_SET *tmp_dst,
                                   int *rate_mv, int64_t *rd,
                                   RD_STATS *rd_stats, int64_t ref_best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int this_mode = mbmi->mode;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  int rate_sum, rs2;
  int64_t dist_sum;

  int_mv best_mv[2];
  int best_tmp_rate_mv = *rate_mv;
  int tmp_skip_txfm_sb;
  int64_t tmp_skip_sse_sb;
  INTERINTER_COMPOUND_DATA best_compound_data;
  best_compound_data.type = COMPOUND_AVERAGE;
  DECLARE_ALIGNED(16, uint8_t, pred0[2 * MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, pred1[2 * MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, residual1[MAX_SB_SQUARE]);  // src - pred1
  DECLARE_ALIGNED(32, int16_t, diff10[MAX_SB_SQUARE]);     // pred1 - pred0
  uint8_t tmp_best_mask_buf[2 * MAX_SB_SQUARE];
  uint8_t *preds0[1] = { pred0 };
  uint8_t *preds1[1] = { pred1 };
  int strides[1] = { bw };
  int tmp_rate_mv;
  const int num_pix = 1 << num_pels_log2_lookup[bsize];
  const int mask_len = 2 * num_pix * sizeof(uint8_t);
  COMPOUND_TYPE cur_type;
  int best_compmode_interinter_cost = 0;
  int can_use_previous = cm->allow_warped_motion;

  best_mv[0].as_int = cur_mv[0].as_int;
  best_mv[1].as_int = cur_mv[1].as_int;
  *rd = INT64_MAX;
  if (masked_compound_used) {
    // get inter predictors to use for masked compound modes
    av1_build_inter_predictors_for_planes_single_buf(
        xd, bsize, 0, 0, mi_row, mi_col, 0, preds0, strides, can_use_previous);
    av1_build_inter_predictors_for_planes_single_buf(
        xd, bsize, 0, 0, mi_row, mi_col, 1, preds1, strides, can_use_previous);
    const struct buf_2d *const src = &x->plane[0].src;
    if (get_bitdepth_data_path_index(xd)) {
      aom_highbd_subtract_block(bh, bw, residual1, bw, src->buf, src->stride,
                                CONVERT_TO_BYTEPTR(pred1), bw, xd->bd);
      aom_highbd_subtract_block(bh, bw, diff10, bw, CONVERT_TO_BYTEPTR(pred1),
                                bw, CONVERT_TO_BYTEPTR(pred0), bw, xd->bd);
    } else {
      aom_subtract_block(bh, bw, residual1, bw, src->buf, src->stride, pred1,
                         bw);
      aom_subtract_block(bh, bw, diff10, bw, pred1, bw, pred0, bw);
    }
  }
  const int orig_is_best = xd->plane[0].dst.buf == orig_dst->plane[0];
  const BUFFER_SET *backup_buf = orig_is_best ? tmp_dst : orig_dst;
  const BUFFER_SET *best_buf = orig_is_best ? orig_dst : tmp_dst;
  for (cur_type = COMPOUND_AVERAGE; cur_type < COMPOUND_TYPES; cur_type++) {
    if (cur_type != COMPOUND_AVERAGE && !masked_compound_used) break;
    if (!is_interinter_compound_used(cur_type, bsize)) continue;
    tmp_rate_mv = *rate_mv;
    int64_t best_rd_cur = INT64_MAX;
    mbmi->interinter_comp.type = cur_type;
    int masked_type_cost = 0;

    const int comp_group_idx_ctx = get_comp_group_idx_context(xd);
    const int comp_index_ctx = get_comp_index_context(cm, xd);
    mbmi->compound_idx = 1;
    if (cur_type == COMPOUND_AVERAGE) {
      mbmi->comp_group_idx = 0;
      if (masked_compound_used) {
        masked_type_cost += x->comp_group_idx_cost[comp_group_idx_ctx][0];
      }
      masked_type_cost += x->comp_idx_cost[comp_index_ctx][1];
      rs2 = masked_type_cost;
      // No need to call av1_build_inter_predictors_sby here
      // 1. COMPOUND_AVERAGE is always the first candidate
      // 2. av1_build_inter_predictors_sby has been called by
      // interpolation_filter_search
      int64_t est_rd =
          estimate_yrd_for_sb(cpi, bsize, x, &rate_sum, &dist_sum,
                              &tmp_skip_txfm_sb, &tmp_skip_sse_sb, INT64_MAX);
      // use spare buffer for following compound type try
      restore_dst_buf(xd, *backup_buf, 1);
      if (est_rd != INT64_MAX)
        best_rd_cur = RDCOST(x->rdmult, rs2 + *rate_mv + rate_sum, dist_sum);
    } else {
      mbmi->comp_group_idx = 1;
      masked_type_cost += x->comp_group_idx_cost[comp_group_idx_ctx][1];
      masked_type_cost += x->compound_type_cost[bsize][cur_type - 1];
      rs2 = masked_type_cost;
      if (x->source_variance > cpi->sf.disable_wedge_search_var_thresh &&
          *rd / 3 < ref_best_rd) {
        best_rd_cur = build_and_cost_compound_type(
            cpi, x, cur_mv, bsize, this_mode, &rs2, *rate_mv, orig_dst,
            &tmp_rate_mv, preds0, preds1, residual1, diff10, strides, mi_row,
            mi_col);
      }
    }
    if (best_rd_cur < *rd) {
      *rd = best_rd_cur;
      best_compound_data = mbmi->interinter_comp;
      if (masked_compound_used && cur_type != COMPOUND_TYPES - 1) {
        memcpy(tmp_best_mask_buf, xd->seg_mask, mask_len);
      }
      best_compmode_interinter_cost = rs2;
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
  if (mbmi->interinter_comp.type != best_compound_data.type) {
    mbmi->comp_group_idx =
        (best_compound_data.type == COMPOUND_AVERAGE) ? 0 : 1;
    mbmi->interinter_comp = best_compound_data;
    memcpy(xd->seg_mask, tmp_best_mask_buf, mask_len);
  }
  if (have_newmv_in_inter_mode(this_mode)) {
    mbmi->mv[0].as_int = best_mv[0].as_int;
    mbmi->mv[1].as_int = best_mv[1].as_int;
    if (use_masked_motion_search(mbmi->interinter_comp.type)) {
      rd_stats->rate += best_tmp_rate_mv - *rate_mv;
      *rate_mv = best_tmp_rate_mv;
    }
  }
  restore_dst_buf(xd, *best_buf, 1);
  return best_compmode_interinter_cost;
}

static int64_t handle_inter_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize, RD_STATS *rd_stats,
                                 RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv,
                                 int *disable_skip, int mi_row, int mi_col,
                                 HandleInterModeArgs *args, int64_t ref_best_rd
#if CONFIG_COLLECT_INTER_MODE_RD_STATS
                                 ,
                                 int64_t *best_est_rd
#endif
) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const int is_comp_pred = has_second_ref(mbmi);
  const int this_mode = mbmi->mode;
  int i;
  int refs[2] = { mbmi->ref_frame[0],
                  (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  int rate_mv = 0;
  DECLARE_ALIGNED(32, uint8_t, tmp_buf_[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
  uint8_t *tmp_buf = get_buf_by_bd(xd, tmp_buf_);
  int64_t rd = INT64_MAX;
  BUFFER_SET orig_dst, tmp_dst;

  int skip_txfm_sb = 0;
  int64_t skip_sse_sb = INT64_MAX;
  int16_t mode_ctx;
  const int masked_compound_used = is_any_masked_compound_used(bsize) &&
                                   cm->seq_params.enable_masked_compound;
  int64_t ret_val = INT64_MAX;
  const int8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
  RD_STATS best_rd_stats, best_rd_stats_y, best_rd_stats_uv;
  int64_t best_rd = INT64_MAX;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  MB_MODE_INFO best_mbmi = *mbmi;
  int best_disable_skip;
  int best_xskip;
  int plane_rate[MAX_MB_PLANE] = { 0 };
  int64_t plane_sse[MAX_MB_PLANE] = { 0 };
  int64_t plane_dist[MAX_MB_PLANE] = { 0 };
  int64_t newmv_ret_val = INT64_MAX;
  int_mv backup_mv[2] = { { 0 } };
  int backup_rate_mv = 0;

  int comp_idx;
  const int search_jnt_comp = is_comp_pred & cm->seq_params.enable_jnt_comp &
                              (mbmi->mode != GLOBAL_GLOBALMV);

  const int has_drl = (have_nearmv_in_inter_mode(mbmi->mode) &&
                       mbmi_ext->ref_mv_count[ref_frame_type] > 2) ||
                      ((mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV) &&
                       mbmi_ext->ref_mv_count[ref_frame_type] > 1);

  // TODO(jingning): This should be deprecated shortly.
  const int idx_offset = have_nearmv_in_inter_mode(mbmi->mode) ? 1 : 0;
  const int ref_set =
      has_drl ? AOMMIN(MAX_REF_MV_SERCH,
                       mbmi_ext->ref_mv_count[ref_frame_type] - idx_offset)
              : 1;

  for (int ref_mv_idx = 0; ref_mv_idx < ref_set; ++ref_mv_idx) {
    if (cpi->sf.reduce_inter_modes && ref_mv_idx > 0) {
      if (mbmi->ref_frame[0] == LAST2_FRAME ||
          mbmi->ref_frame[0] == LAST3_FRAME ||
          mbmi->ref_frame[1] == LAST2_FRAME ||
          mbmi->ref_frame[1] == LAST3_FRAME) {
        if (mbmi_ext->ref_mv_stack[ref_frame_type][ref_mv_idx + idx_offset]
                .weight < REF_CAT_LEVEL) {
          continue;
        }
      }
    }

    av1_init_rd_stats(rd_stats);

    mbmi->interinter_comp.type = COMPOUND_AVERAGE;
    mbmi->comp_group_idx = 0;
    mbmi->compound_idx = 1;
    if (mbmi->ref_frame[1] == INTRA_FRAME) mbmi->ref_frame[1] = NONE_FRAME;

    mode_ctx =
        av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame);

    mbmi->num_proj_ref[0] = 0;
    mbmi->num_proj_ref[1] = 0;
    mbmi->motion_mode = SIMPLE_TRANSLATION;
    mbmi->ref_mv_idx = ref_mv_idx;

    if (is_comp_pred) {
      for (int ref_idx = 0; ref_idx < is_comp_pred + 1; ++ref_idx) {
        const int single_mode =
            get_single_mode(this_mode, ref_idx, is_comp_pred);
        if (single_mode == NEWMV &&
            args->single_newmv[mbmi->ref_mv_idx][mbmi->ref_frame[ref_idx]]
                    .as_int == INVALID_MV)
          continue;
      }
    }

    rd_stats->rate += args->ref_frame_cost + args->single_comp_cost;
    rd_stats->rate +=
        get_drl_cost(mbmi, mbmi_ext, x->drl_mode_cost0, ref_frame_type);

    const RD_STATS backup_rd_stats = *rd_stats;
    const MB_MODE_INFO backup_mbmi = *mbmi;
    int64_t best_rd2 = INT64_MAX;

    // If !search_jnt_comp, we need to force mbmi->compound_idx = 1.
    for (comp_idx = 1; comp_idx >= !search_jnt_comp; --comp_idx) {
      int rs = 0;
      int compmode_interinter_cost = 0;
      *rd_stats = backup_rd_stats;
      *mbmi = backup_mbmi;
      mbmi->compound_idx = comp_idx;

      if (is_comp_pred && comp_idx == 0) {
        mbmi->comp_group_idx = 0;
        mbmi->compound_idx = 0;

        const int comp_group_idx_ctx = get_comp_group_idx_context(xd);
        const int comp_index_ctx = get_comp_index_context(cm, xd);
        if (masked_compound_used) {
          compmode_interinter_cost +=
              x->comp_group_idx_cost[comp_group_idx_ctx][0];
        }
        compmode_interinter_cost += x->comp_idx_cost[comp_index_ctx][0];
      }

      int_mv cur_mv[2];
      if (!build_cur_mv(cur_mv, this_mode, cm, x)) {
        continue;
      }
      if (have_newmv_in_inter_mode(this_mode)) {
        if (comp_idx == 0) {
          cur_mv[0] = backup_mv[0];
          cur_mv[1] = backup_mv[1];
          rate_mv = backup_rate_mv;
        }

        // when jnt_comp_skip_mv_search flag is on, new mv will be searched once
        if (!(search_jnt_comp && cpi->sf.jnt_comp_skip_mv_search &&
              comp_idx == 0)) {
          newmv_ret_val = handle_newmv(cpi, x, bsize, cur_mv, mi_row, mi_col,
                                       &rate_mv, args);

          // Store cur_mv and rate_mv so that they can be restored in the next
          // iteration of the loop
          backup_mv[0] = cur_mv[0];
          backup_mv[1] = cur_mv[1];
          backup_rate_mv = rate_mv;
        }

        if (newmv_ret_val != 0) {
          continue;
        } else {
          rd_stats->rate += rate_mv;
        }
      }
      for (i = 0; i < is_comp_pred + 1; ++i) {
        mbmi->mv[i].as_int = cur_mv[i].as_int;
      }

      // Initialise tmp_dst and orig_dst buffers to prevent "may be used
      // uninitialized" warnings in GCC when the stream is monochrome.
      memset(tmp_dst.plane, 0, sizeof(tmp_dst.plane));
      memset(tmp_dst.stride, 0, sizeof(tmp_dst.stride));
      memset(orig_dst.plane, 0, sizeof(tmp_dst.plane));
      memset(orig_dst.stride, 0, sizeof(tmp_dst.stride));

      // do first prediction into the destination buffer. Do the next
      // prediction into a temporary buffer. Then keep track of which one
      // of these currently holds the best predictor, and use the other
      // one for future predictions. In the end, copy from tmp_buf to
      // dst if necessary.
      for (i = 0; i < num_planes; i++) {
        tmp_dst.plane[i] = tmp_buf + i * MAX_SB_SQUARE;
        tmp_dst.stride[i] = MAX_SB_SIZE;
      }
      for (i = 0; i < num_planes; i++) {
        orig_dst.plane[i] = xd->plane[i].dst.buf;
        orig_dst.stride[i] = xd->plane[i].dst.stride;
      }

      const int ref_mv_cost = cost_mv_ref(x, this_mode, mode_ctx);
#if USE_DISCOUNT_NEWMV_TEST
      // We don't include the cost of the second reference here, because there
      // are only three options: Last/Golden, ARF/Last or Golden/ARF, or in
      // other words if you present them in that order, the second one is always
      // known if the first is known.
      //
      // Under some circumstances we discount the cost of new mv mode to
      // encourage initiation of a motion field.
      if (discount_newmv_test(cpi, x, this_mode, mbmi->mv[0])) {
        // discount_newmv_test only applies discount on NEWMV mode.
        assert(this_mode == NEWMV);
        rd_stats->rate += AOMMIN(cost_mv_ref(x, this_mode, mode_ctx),
                                 cost_mv_ref(x, NEARESTMV, mode_ctx));
      } else {
        rd_stats->rate += ref_mv_cost;
      }
#else
      rd_stats->rate += ref_mv_cost;
#endif

      if (RDCOST(x->rdmult, rd_stats->rate, 0) > ref_best_rd &&
          mbmi->mode != NEARESTMV && mbmi->mode != NEAREST_NEARESTMV) {
        continue;
      }

      ret_val = interpolation_filter_search(
          x, cpi, bsize, mi_row, mi_col, &tmp_dst, &orig_dst,
          args->single_filter, &rd, &rs, &skip_txfm_sb, &skip_sse_sb);
      if (ret_val != 0) {
        restore_dst_buf(xd, orig_dst, num_planes);
        continue;
      } else if (cpi->sf.model_based_post_interp_filter_breakout &&
                 ref_best_rd != INT64_MAX && (rd / 6 > ref_best_rd)) {
        restore_dst_buf(xd, orig_dst, num_planes);
        if ((rd >> 4) > ref_best_rd) break;
        continue;
      }

      if (is_comp_pred && comp_idx) {
        int64_t best_rd_compound;
        compmode_interinter_cost = compound_type_rd(
            cpi, x, bsize, mi_col, mi_row, cur_mv, masked_compound_used,
            &orig_dst, &tmp_dst, &rate_mv, &best_rd_compound, rd_stats,
            ref_best_rd);
        if (ref_best_rd < INT64_MAX && best_rd_compound / 3 > ref_best_rd) {
          restore_dst_buf(xd, orig_dst, num_planes);
          continue;
        }
        if (mbmi->interinter_comp.type != COMPOUND_AVERAGE) {
          int tmp_rate;
          int64_t tmp_dist;
          av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, &orig_dst,
                                        bsize);
          for (int plane = 0; plane < num_planes; ++plane)
            av1_subtract_plane(x, bsize, plane);
          model_rd_for_sb(cpi, bsize, x, xd, 0, num_planes - 1, &tmp_rate,
                          &tmp_dist, &skip_txfm_sb, &skip_sse_sb, plane_rate,
                          plane_sse, plane_dist);
          rd = RDCOST(x->rdmult, rs + tmp_rate, tmp_dist);
        }
      }

      if (search_jnt_comp) {
        // if 1/2 model rd is larger than best_rd in jnt_comp mode,
        // use jnt_comp mode, save additional search
        if ((rd >> 1) > best_rd) {
          restore_dst_buf(xd, orig_dst, num_planes);
          continue;
        }
      }

      if (!is_comp_pred)
        args->single_filter[this_mode][refs[0]] =
            av1_extract_interp_filter(mbmi->interp_filters, 0);

      if (args->modelled_rd != NULL) {
        if (is_comp_pred) {
          const int mode0 = compound_ref0_mode(this_mode);
          const int mode1 = compound_ref1_mode(this_mode);
          const int64_t mrd = AOMMIN(args->modelled_rd[mode0][refs[0]],
                                     args->modelled_rd[mode1][refs[1]]);
          if (rd / 4 * 3 > mrd && ref_best_rd < INT64_MAX) {
            restore_dst_buf(xd, orig_dst, num_planes);
            continue;
          }
        } else {
          args->modelled_rd[this_mode][refs[0]] = rd;
        }
      }

      if (cpi->sf.use_rd_breakout && ref_best_rd < INT64_MAX) {
        // if current pred_error modeled rd is substantially more than the best
        // so far, do not bother doing full rd
        if (rd / 2 > ref_best_rd) {
          restore_dst_buf(xd, orig_dst, num_planes);
          continue;
        }
      }

      rd_stats->rate += compmode_interinter_cost;

      if (search_jnt_comp && cpi->sf.jnt_comp_fast_tx_search && comp_idx == 0) {
        // TODO(chengchen): this speed feature introduces big loss.
        // Need better estimation of rate distortion.
        rd_stats->rate += rs;
        rd_stats->rate += plane_rate[0] + plane_rate[1] + plane_rate[2];
        rd_stats_y->rate = plane_rate[0];
        rd_stats_uv->rate = plane_rate[1] + plane_rate[2];
        rd_stats->sse = plane_sse[0] + plane_sse[1] + plane_sse[2];
        rd_stats_y->sse = plane_sse[0];
        rd_stats_uv->sse = plane_sse[1] + plane_sse[2];
        rd_stats->dist = plane_dist[0] + plane_dist[1] + plane_dist[2];
        rd_stats_y->dist = plane_dist[0];
        rd_stats_uv->dist = plane_dist[1] + plane_dist[2];
      } else {
#if CONFIG_COLLECT_INTER_MODE_RD_STATS
        ret_val =
            motion_mode_rd(cpi, x, bsize, rd_stats, rd_stats_y, rd_stats_uv,
                           disable_skip, mi_row, mi_col, args, ref_best_rd,
                           refs, rate_mv, &orig_dst, best_est_rd);
#else
        ret_val = motion_mode_rd(cpi, x, bsize, rd_stats, rd_stats_y,
                                 rd_stats_uv, disable_skip, mi_row, mi_col,
                                 args, ref_best_rd, refs, rate_mv, &orig_dst);
#endif
      }
      if (ret_val != INT64_MAX) {
        int64_t tmp_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
        if (tmp_rd < best_rd) {
          best_rd_stats = *rd_stats;
          best_rd_stats_y = *rd_stats_y;
          best_rd_stats_uv = *rd_stats_uv;
          best_rd = tmp_rd;
          best_mbmi = *mbmi;
          best_disable_skip = *disable_skip;
          best_xskip = x->skip;
          memcpy(best_blk_skip, x->blk_skip,
                 sizeof(best_blk_skip[0]) * xd->n8_h * xd->n8_w);
        }

        if (tmp_rd < best_rd2) {
          best_rd2 = tmp_rd;
        }

        if (tmp_rd < ref_best_rd) {
          ref_best_rd = tmp_rd;
        }
      }
      restore_dst_buf(xd, orig_dst, num_planes);
    }

    args->modelled_rd = NULL;
  }

  if (best_rd == INT64_MAX) return INT64_MAX;

  // re-instate status of the best choice
  *rd_stats = best_rd_stats;
  *rd_stats_y = best_rd_stats_y;
  *rd_stats_uv = best_rd_stats_uv;
  *mbmi = best_mbmi;
  *disable_skip = best_disable_skip;
  x->skip = best_xskip;
  assert(IMPLIES(mbmi->comp_group_idx == 1,
                 mbmi->interinter_comp.type != COMPOUND_AVERAGE));
  memcpy(x->blk_skip, best_blk_skip,
         sizeof(best_blk_skip[0]) * xd->n8_h * xd->n8_w);

  return RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
}

static int64_t rd_pick_intrabc_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x,
                                       RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                       int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  if (!av1_allow_intrabc(cm)) return INT64_MAX;
  const int num_planes = av1_num_planes(cm);

  MACROBLOCKD *const xd = &x->e_mbd;
  const TileInfo *tile = &xd->tile;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int mi_row = -xd->mb_to_top_edge / (8 * MI_SIZE);
  const int mi_col = -xd->mb_to_left_edge / (8 * MI_SIZE);
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int sb_row = mi_row >> cm->seq_params.mib_size_log2;
  const int sb_col = mi_col >> cm->seq_params.mib_size_log2;

  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  MV_REFERENCE_FRAME ref_frame = INTRA_FRAME;
  av1_find_mv_refs(cm, xd, mbmi, ref_frame, mbmi_ext->ref_mv_count,
                   mbmi_ext->ref_mv_stack, NULL, mbmi_ext->global_mvs, mi_row,
                   mi_col, mbmi_ext->mode_context);

  int_mv nearestmv, nearmv;
  av1_find_best_ref_mvs_from_stack(0, mbmi_ext, ref_frame, &nearestmv, &nearmv,
                                   0);

  if (nearestmv.as_int == INVALID_MV) {
    nearestmv.as_int = 0;
  }
  if (nearmv.as_int == INVALID_MV) {
    nearmv.as_int = 0;
  }

  int_mv dv_ref = nearestmv.as_int == 0 ? nearmv : nearestmv;
  if (dv_ref.as_int == 0)
    av1_find_ref_dv(&dv_ref, tile, cm->seq_params.mib_size, mi_row, mi_col);
  // Ref DV should not have sub-pel.
  assert((dv_ref.as_mv.col & 7) == 0);
  assert((dv_ref.as_mv.row & 7) == 0);
  mbmi_ext->ref_mv_stack[INTRA_FRAME][0].this_mv = dv_ref;

  struct buf_2d yv12_mb[MAX_MB_PLANE];
  av1_setup_pred_block(xd, yv12_mb, xd->cur_buf, mi_row, mi_col, NULL, NULL,
                       num_planes);
  for (int i = 0; i < num_planes; ++i) {
    xd->plane[i].pre[0] = yv12_mb[i];
  }

  enum IntrabcMotionDirection {
    IBC_MOTION_ABOVE,
    IBC_MOTION_LEFT,
    IBC_MOTION_DIRECTIONS
  };

  MB_MODE_INFO best_mbmi = *mbmi;
  RD_STATS best_rdcost = *rd_cost;
  int best_skip = x->skip;

  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE] = { 0 };
  for (enum IntrabcMotionDirection dir = IBC_MOTION_ABOVE;
       dir < IBC_MOTION_DIRECTIONS; ++dir) {
    const MvLimits tmp_mv_limits = x->mv_limits;
    switch (dir) {
      case IBC_MOTION_ABOVE:
        x->mv_limits.col_min = (tile->mi_col_start - mi_col) * MI_SIZE;
        x->mv_limits.col_max = (tile->mi_col_end - mi_col) * MI_SIZE - w;
        x->mv_limits.row_min = (tile->mi_row_start - mi_row) * MI_SIZE;
        x->mv_limits.row_max =
            (sb_row * cm->seq_params.mib_size - mi_row) * MI_SIZE - h;
        break;
      case IBC_MOTION_LEFT:
        x->mv_limits.col_min = (tile->mi_col_start - mi_col) * MI_SIZE;
        x->mv_limits.col_max =
            (sb_col * cm->seq_params.mib_size - mi_col) * MI_SIZE - w;
        // TODO(aconverse@google.com): Minimize the overlap between above and
        // left areas.
        x->mv_limits.row_min = (tile->mi_row_start - mi_row) * MI_SIZE;
        int bottom_coded_mi_edge =
            AOMMIN((sb_row + 1) * cm->seq_params.mib_size, tile->mi_row_end);
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
    int bestsme = av1_full_pixel_search(
        cpi, x, bsize, &mvp_full, step_param, sadpb,
        cond_cost_list(cpi, cost_list), &dv_ref.as_mv, INT_MAX, 1,
        (MI_SIZE * mi_col), (MI_SIZE * mi_row), 1);

    x->mv_limits = tmp_mv_limits;
    if (bestsme == INT_MAX) continue;
    mvp_full = x->best_mv.as_mv;
    MV dv = { .row = mvp_full.row * 8, .col = mvp_full.col * 8 };
    if (mv_check_bounds(&x->mv_limits, &dv)) continue;
    if (!av1_is_dv_valid(dv, cm, xd, mi_row, mi_col, bsize,
                         cm->seq_params.mib_size_log2))
      continue;

    // DV should not have sub-pel.
    assert((dv.col & 7) == 0);
    assert((dv.row & 7) == 0);
    memset(&mbmi->palette_mode_info, 0, sizeof(mbmi->palette_mode_info));
    mbmi->filter_intra_mode_info.use_filter_intra = 0;
    mbmi->use_intrabc = 1;
    mbmi->mode = DC_PRED;
    mbmi->uv_mode = UV_DC_PRED;
    mbmi->motion_mode = SIMPLE_TRANSLATION;
    mbmi->mv[0].as_mv = dv;
    mbmi->interp_filters = av1_broadcast_interp_filter(BILINEAR);
    mbmi->skip = 0;
    x->skip = 0;
    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);

    int *dvcost[2] = { (int *)&cpi->dv_cost[0][MV_MAX],
                       (int *)&cpi->dv_cost[1][MV_MAX] };
    // TODO(aconverse@google.com): The full motion field defining discount
    // in MV_COST_WEIGHT is too large. Explore other values.
    int rate_mv = av1_mv_bit_cost(&dv, &dv_ref.as_mv, cpi->dv_joint_cost,
                                  dvcost, MV_COST_WEIGHT_SUB);
    const int rate_mode = x->intrabc_cost[1];
    RD_STATS rd_stats, rd_stats_uv;
    av1_subtract_plane(x, bsize, 0);
    if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id]) {
      // Intrabc
      select_tx_type_yrd(cpi, x, &rd_stats, bsize, mi_row, mi_col, INT64_MAX);
    } else {
      super_block_yrd(cpi, x, &rd_stats, bsize, INT64_MAX);
      memset(mbmi->inter_tx_size, mbmi->tx_size, sizeof(mbmi->inter_tx_size));
      memset(x->blk_skip, rd_stats.skip,
             sizeof(x->blk_skip[0]) * xd->n8_h * xd->n8_w);
    }
    if (num_planes > 1) {
      super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
      av1_merge_rd_stats(&rd_stats, &rd_stats_uv);
    }
#if CONFIG_RD_DEBUG
    mbmi->rd_stats = rd_stats;
#endif

    const int skip_ctx = av1_get_skip_context(xd);

    RD_STATS rdc_noskip;
    av1_init_rd_stats(&rdc_noskip);
    rdc_noskip.rate =
        rate_mode + rate_mv + rd_stats.rate + x->skip_cost[skip_ctx][0];
    rdc_noskip.dist = rd_stats.dist;
    rdc_noskip.rdcost = RDCOST(x->rdmult, rdc_noskip.rate, rdc_noskip.dist);
    if (rdc_noskip.rdcost < best_rd) {
      best_rd = rdc_noskip.rdcost;
      best_mbmi = *mbmi;
      best_skip = x->skip;
      best_rdcost = rdc_noskip;
      memcpy(best_blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * xd->n8_h * xd->n8_w);
    }

    if (!xd->lossless[mbmi->segment_id]) {
      x->skip = 1;
      mbmi->skip = 1;
      RD_STATS rdc_skip;
      av1_init_rd_stats(&rdc_skip);
      rdc_skip.rate = rate_mode + rate_mv + x->skip_cost[skip_ctx][1];
      rdc_skip.dist = rd_stats.sse;
      rdc_skip.rdcost = RDCOST(x->rdmult, rdc_skip.rate, rdc_skip.dist);
      if (rdc_skip.rdcost < best_rd) {
        best_rd = rdc_skip.rdcost;
        best_mbmi = *mbmi;
        best_skip = x->skip;
        best_rdcost = rdc_skip;
        memcpy(best_blk_skip, x->blk_skip,
               sizeof(x->blk_skip[0]) * xd->n8_h * xd->n8_w);
      }
    }
  }
  *mbmi = best_mbmi;
  *rd_cost = best_rdcost;
  x->skip = best_skip;
  memcpy(x->blk_skip, best_blk_skip,
         sizeof(x->blk_skip[0]) * xd->n8_h * xd->n8_w);
  return best_rd;
}

void av1_rd_pick_intra_mode_sb(const AV1_COMP *cpi, MACROBLOCK *x, int mi_row,
                               int mi_col, RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int num_planes = av1_num_planes(cm);
  int rate_y = 0, rate_uv = 0, rate_y_tokenonly = 0, rate_uv_tokenonly = 0;
  int y_skip = 0, uv_skip = 0;
  int64_t dist_y = 0, dist_uv = 0;
  TX_SIZE max_uv_tx_size;

  ctx->skip = 0;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  mbmi->use_intrabc = 0;
  mbmi->mv[0].as_int = 0;

  const int64_t intra_yrd =
      rd_pick_intra_sby_mode(cpi, x, &rate_y, &rate_y_tokenonly, &dist_y,
                             &y_skip, bsize, best_rd, ctx);

  if (intra_yrd < best_rd) {
    // Only store reconstructed luma when there's chroma RDO. When there's no
    // chroma RDO, the reconstructed luma will be stored in encode_superblock().
    xd->cfl.is_chroma_reference =
        is_chroma_reference(mi_row, mi_col, bsize, cm->seq_params.subsampling_x,
                            cm->seq_params.subsampling_y);
    xd->cfl.store_y = store_cfl_required_rdo(cm, x);
    if (xd->cfl.store_y) {
      // Restore reconstructed luma values.
      memcpy(x->blk_skip, ctx->blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
      av1_encode_intra_block_plane(cpi, x, bsize, AOM_PLANE_Y,
                                   cpi->optimize_seg_arr[mbmi->segment_id],
                                   mi_row, mi_col);
      xd->cfl.store_y = 0;
    }
    if (num_planes > 1) {
      max_uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
      init_sbuv_mode(mbmi);
      if (!x->skip_chroma_rd)
        rd_pick_intra_sbuv_mode(cpi, x, &rate_uv, &rate_uv_tokenonly, &dist_uv,
                                &uv_skip, bsize, max_uv_tx_size);
    }

    if (y_skip && (uv_skip || x->skip_chroma_rd)) {
      rd_cost->rate = rate_y + rate_uv - rate_y_tokenonly - rate_uv_tokenonly +
                      x->skip_cost[av1_get_skip_context(xd)][1];
      rd_cost->dist = dist_y + dist_uv;
    } else {
      rd_cost->rate =
          rate_y + rate_uv + x->skip_cost[av1_get_skip_context(xd)][0];
      rd_cost->dist = dist_y + dist_uv;
    }
    rd_cost->rdcost = RDCOST(x->rdmult, rd_cost->rate, rd_cost->dist);
  } else {
    rd_cost->rate = INT_MAX;
  }

  if (rd_cost->rate != INT_MAX && rd_cost->rdcost < best_rd)
    best_rd = rd_cost->rdcost;
  if (rd_pick_intrabc_mode_sb(cpi, x, rd_cost, bsize, best_rd) < best_rd) {
    ctx->skip = x->skip;
    memcpy(ctx->blk_skip, x->blk_skip,
           sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
    assert(rd_cost->rate != INT_MAX);
  }
  if (rd_cost->rate == INT_MAX) return;

  ctx->mic = *xd->mi[0];
  ctx->mbmi_ext = *x->mbmi_ext;
}

static void restore_uv_color_map(const AV1_COMP *const cpi, MACROBLOCK *x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  int *const data = x->palette_buffer->kmeans_data_buf;
  int centroids[2 * PALETTE_MAX_SIZE];
  uint8_t *const color_map = xd->plane[1].color_index_map;
  int r, c;
  const uint16_t *const src_u16 = CONVERT_TO_SHORTPTR(src_u);
  const uint16_t *const src_v16 = CONVERT_TO_SHORTPTR(src_v);
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(bsize, 1, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);

  for (r = 0; r < rows; ++r) {
    for (c = 0; c < cols; ++c) {
      if (cpi->common.seq_params.use_highbitdepth) {
        data[(r * cols + c) * 2] = src_u16[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v16[r * src_stride + c];
      } else {
        data[(r * cols + c) * 2] = src_u[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v[r * src_stride + c];
      }
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

static void calc_target_weighted_pred(const AV1_COMMON *cm, const MACROBLOCK *x,
                                      const MACROBLOCKD *xd, int mi_row,
                                      int mi_col, const uint8_t *above,
                                      int above_stride, const uint8_t *left,
                                      int left_stride);

static const int ref_frame_flag_list[REF_FRAMES] = { 0,
                                                     AOM_LAST_FLAG,
                                                     AOM_LAST2_FLAG,
                                                     AOM_LAST3_FLAG,
                                                     AOM_GOLD_FLAG,
                                                     AOM_BWD_FLAG,
                                                     AOM_ALT2_FLAG,
                                                     AOM_ALT_FLAG };

static void rd_pick_skip_mode(RD_STATS *rd_cost,
                              InterModeSearchState *search_state,
                              const AV1_COMP *const cpi, MACROBLOCK *const x,
                              BLOCK_SIZE bsize, int mi_row, int mi_col,
                              struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE]) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];

  x->compound_idx = 1;  // COMPOUND_AVERAGE
  RD_STATS skip_mode_rd_stats;
  av1_invalid_rd_stats(&skip_mode_rd_stats);

  if (cm->ref_frame_idx_0 == INVALID_IDX ||
      cm->ref_frame_idx_1 == INVALID_IDX) {
    return;
  }

  const MV_REFERENCE_FRAME ref_frame = LAST_FRAME + cm->ref_frame_idx_0;
  const MV_REFERENCE_FRAME second_ref_frame = LAST_FRAME + cm->ref_frame_idx_1;
  const PREDICTION_MODE this_mode = NEAREST_NEARESTMV;
  const int mode_index =
      get_prediction_mode_idx(this_mode, ref_frame, second_ref_frame);

  if (mode_index == -1) {
    return;
  }

  mbmi->mode = this_mode;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = ref_frame;
  mbmi->ref_frame[1] = second_ref_frame;

  assert(this_mode == NEAREST_NEARESTMV);
  if (!build_cur_mv(mbmi->mv, this_mode, cm, x)) {
    return;
  }

  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  mbmi->interintra_mode = (INTERINTRA_MODE)(II_DC_PRED - 1);
  mbmi->comp_group_idx = 0;
  mbmi->compound_idx = x->compound_idx;
  mbmi->interinter_comp.type = COMPOUND_AVERAGE;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->ref_mv_idx = 0;
  mbmi->skip_mode = mbmi->skip = 1;

  set_default_interp_filters(mbmi, cm->interp_filter);

  set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
  for (int i = 0; i < num_planes; i++) {
    xd->plane[i].pre[0] = yv12_mb[mbmi->ref_frame[0]][i];
    xd->plane[i].pre[1] = yv12_mb[mbmi->ref_frame[1]][i];
  }

  BUFFER_SET orig_dst;
  for (int i = 0; i < num_planes; i++) {
    orig_dst.plane[i] = xd->plane[i].dst.buf;
    orig_dst.stride[i] = xd->plane[i].dst.stride;
  }

  // Obtain the rdcost for skip_mode.
  skip_mode_rd(&skip_mode_rd_stats, cpi, x, bsize, mi_row, mi_col, &orig_dst);

  // Compare the use of skip_mode with the best intra/inter mode obtained.
  const int skip_mode_ctx = av1_get_skip_mode_context(xd);
  const int64_t best_intra_inter_mode_cost =
      (rd_cost->dist < INT64_MAX && rd_cost->rate < INT32_MAX)
          ? RDCOST(x->rdmult,
                   rd_cost->rate + x->skip_mode_cost[skip_mode_ctx][0],
                   rd_cost->dist)
          : INT64_MAX;

  if (skip_mode_rd_stats.rdcost <= best_intra_inter_mode_cost) {
    assert(mode_index != -1);
    search_state->best_mbmode.skip_mode = 1;
    search_state->best_mbmode = *mbmi;

    search_state->best_mbmode.skip_mode = search_state->best_mbmode.skip = 1;
    search_state->best_mbmode.mode = NEAREST_NEARESTMV;
    search_state->best_mbmode.ref_frame[0] = mbmi->ref_frame[0];
    search_state->best_mbmode.ref_frame[1] = mbmi->ref_frame[1];
    search_state->best_mbmode.mv[0].as_int = mbmi->mv[0].as_int;
    search_state->best_mbmode.mv[1].as_int = mbmi->mv[1].as_int;
    search_state->best_mbmode.ref_mv_idx = 0;

    // Set up tx_size related variables for skip-specific loop filtering.
    search_state->best_mbmode.tx_size =
        block_signals_txsize(bsize) ? tx_size_from_tx_mode(bsize, cm->tx_mode)
                                    : max_txsize_rect_lookup[bsize];
    memset(search_state->best_mbmode.inter_tx_size,
           search_state->best_mbmode.tx_size,
           sizeof(search_state->best_mbmode.inter_tx_size));
    set_txfm_ctxs(search_state->best_mbmode.tx_size, xd->n8_w, xd->n8_h,
                  search_state->best_mbmode.skip && is_inter_block(mbmi), xd);

    // Set up color-related variables for skip mode.
    search_state->best_mbmode.uv_mode = UV_DC_PRED;
    search_state->best_mbmode.palette_mode_info.palette_size[0] = 0;
    search_state->best_mbmode.palette_mode_info.palette_size[1] = 0;

    search_state->best_mbmode.comp_group_idx = 0;
    search_state->best_mbmode.compound_idx = x->compound_idx;
    search_state->best_mbmode.interinter_comp.type = COMPOUND_AVERAGE;
    search_state->best_mbmode.motion_mode = SIMPLE_TRANSLATION;

    search_state->best_mbmode.interintra_mode =
        (INTERINTRA_MODE)(II_DC_PRED - 1);
    search_state->best_mbmode.filter_intra_mode_info.use_filter_intra = 0;

    set_default_interp_filters(&search_state->best_mbmode, cm->interp_filter);

    search_state->best_mode_index = mode_index;

    // Update rd_cost
    rd_cost->rate = skip_mode_rd_stats.rate;
    rd_cost->dist = rd_cost->sse = skip_mode_rd_stats.dist;
    rd_cost->rdcost = skip_mode_rd_stats.rdcost;

    search_state->best_rd = rd_cost->rdcost;
    search_state->best_skip2 = 1;
    search_state->best_mode_skippable = (skip_mode_rd_stats.sse == 0);

    x->skip = 1;
  }
}

// speed feature: fast intra/inter transform type search
// Used for speed >= 2
// When this speed feature is on, in rd mode search, only DCT is used.
// After the mode is determined, this function is called, to select
// transform types and get accurate rdcost.
static void sf_refine_fast_tx_type_search(
    const AV1_COMP *cpi, MACROBLOCK *x, int mi_row, int mi_col,
    RD_STATS *rd_cost, BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
    int best_mode_index, MB_MODE_INFO *best_mbmode,
    struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE], int best_rate_y,
    int best_rate_uv, int *best_skip2) {
  const AV1_COMMON *const cm = &cpi->common;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int num_planes = av1_num_planes(cm);

  if (xd->lossless[mbmi->segment_id] == 0 && best_mode_index >= 0 &&
      ((sf->tx_type_search.fast_inter_tx_type_search == 1 &&
        is_inter_mode(best_mbmode->mode)) ||
       (sf->tx_type_search.fast_intra_tx_type_search == 1 &&
        !is_inter_mode(best_mbmode->mode)))) {
    int skip_blk = 0;
    RD_STATS rd_stats_y, rd_stats_uv;

    x->use_default_inter_tx_type = 0;
    x->use_default_intra_tx_type = 0;

    *mbmi = *best_mbmode;

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);

    // Select prediction reference frames.
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[0] = yv12_mb[mbmi->ref_frame[0]][i];
      if (has_second_ref(mbmi))
        xd->plane[i].pre[1] = yv12_mb[mbmi->ref_frame[1]][i];
    }

    if (is_inter_mode(mbmi->mode)) {
      av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
      if (mbmi->motion_mode == OBMC_CAUSAL)
        av1_build_obmc_inter_predictors_sb(cm, xd, mi_row, mi_col);

      av1_subtract_plane(x, bsize, 0);
      if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id]) {
        // av1_rd_pick_inter_mode_sb
        select_tx_type_yrd(cpi, x, &rd_stats_y, bsize, mi_row, mi_col,
                           INT64_MAX);
        assert(rd_stats_y.rate != INT_MAX);
      } else {
        super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
        memset(mbmi->inter_tx_size, mbmi->tx_size, sizeof(mbmi->inter_tx_size));
        memset(x->blk_skip, rd_stats_y.skip,
               sizeof(x->blk_skip[0]) * xd->n8_h * xd->n8_w);
      }
      if (num_planes > 1) {
        inter_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX, FTXS_NONE);
      } else {
        av1_init_rd_stats(&rd_stats_uv);
      }
    } else {
      super_block_yrd(cpi, x, &rd_stats_y, bsize, INT64_MAX);
      if (num_planes > 1) {
        super_block_uvrd(cpi, x, &rd_stats_uv, bsize, INT64_MAX);
      } else {
        av1_init_rd_stats(&rd_stats_uv);
      }
    }

    if (RDCOST(x->rdmult, rd_stats_y.rate + rd_stats_uv.rate,
               (rd_stats_y.dist + rd_stats_uv.dist)) >
        RDCOST(x->rdmult, 0, (rd_stats_y.sse + rd_stats_uv.sse))) {
      skip_blk = 1;
      rd_stats_y.rate = x->skip_cost[av1_get_skip_context(xd)][1];
      rd_stats_uv.rate = 0;
      rd_stats_y.dist = rd_stats_y.sse;
      rd_stats_uv.dist = rd_stats_uv.sse;
    } else {
      skip_blk = 0;
      rd_stats_y.rate += x->skip_cost[av1_get_skip_context(xd)][0];
    }

    if (RDCOST(x->rdmult, best_rate_y + best_rate_uv, rd_cost->dist) >
        RDCOST(x->rdmult, rd_stats_y.rate + rd_stats_uv.rate,
               (rd_stats_y.dist + rd_stats_uv.dist))) {
      best_mbmode->tx_size = mbmi->tx_size;
      av1_copy(best_mbmode->inter_tx_size, mbmi->inter_tx_size);
      memcpy(ctx->blk_skip, x->blk_skip,
             sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
      av1_copy(best_mbmode->txk_type, mbmi->txk_type);
      rd_cost->rate +=
          (rd_stats_y.rate + rd_stats_uv.rate - best_rate_y - best_rate_uv);
      rd_cost->dist = rd_stats_y.dist + rd_stats_uv.dist;
      rd_cost->rdcost = RDCOST(x->rdmult, rd_cost->rate, rd_cost->dist);
      *best_skip2 = skip_blk;
    }
  }
}

// Please add/modify parameter setting in this function, making it consistent
// and easy to read and maintain.
static void set_params_rd_pick_inter_mode(
    const AV1_COMP *cpi, MACROBLOCK *x, HandleInterModeArgs *args,
    BLOCK_SIZE bsize, int mi_row, int mi_col, uint16_t ref_frame_skip_mask[2],
    uint32_t mode_skip_mask[REF_FRAMES],
    unsigned int ref_costs_single[REF_FRAMES],
    unsigned int ref_costs_comp[REF_FRAMES][REF_FRAMES],
    struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE]) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const struct segmentation *const seg = &cm->seg;
  const SPEED_FEATURES *const sf = &cpi->sf;
  unsigned char segment_id = mbmi->segment_id;
  int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE >> 1, MAX_SB_SIZE >> 1,
                                   MAX_SB_SIZE >> 1 };
  int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE >> 1, MAX_SB_SIZE >> 1,
                                    MAX_SB_SIZE >> 1 };
  int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

  for (int i = 0; i < MB_MODE_COUNT; ++i)
    for (int k = 0; k < REF_FRAMES; ++k) args->single_filter[i][k] = SWITCHABLE;

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    args->above_pred_buf[0] = CONVERT_TO_BYTEPTR(x->above_pred_buf);
    args->above_pred_buf[1] =
        CONVERT_TO_BYTEPTR(x->above_pred_buf + (MAX_SB_SQUARE >> 1) * len);
    args->above_pred_buf[2] =
        CONVERT_TO_BYTEPTR(x->above_pred_buf + MAX_SB_SQUARE * len);
    args->left_pred_buf[0] = CONVERT_TO_BYTEPTR(x->left_pred_buf);
    args->left_pred_buf[1] =
        CONVERT_TO_BYTEPTR(x->left_pred_buf + (MAX_SB_SQUARE >> 1) * len);
    args->left_pred_buf[2] =
        CONVERT_TO_BYTEPTR(x->left_pred_buf + MAX_SB_SQUARE * len);
  } else {
    args->above_pred_buf[0] = x->above_pred_buf;
    args->above_pred_buf[1] = x->above_pred_buf + (MAX_SB_SQUARE >> 1);
    args->above_pred_buf[2] = x->above_pred_buf + MAX_SB_SQUARE;
    args->left_pred_buf[0] = x->left_pred_buf;
    args->left_pred_buf[1] = x->left_pred_buf + (MAX_SB_SQUARE >> 1);
    args->left_pred_buf[2] = x->left_pred_buf + MAX_SB_SQUARE;
  }

  av1_collect_neighbors_ref_counts(xd);

  estimate_ref_frame_costs(cm, xd, x, segment_id, ref_costs_single,
                           ref_costs_comp);

  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    x->pred_mv_sad[ref_frame] = INT_MAX;
    x->mbmi_ext->mode_context[ref_frame] = 0;
    x->mbmi_ext->compound_mode_context[ref_frame] = 0;
    if (cpi->ref_frame_flags & ref_frame_flag_list[ref_frame]) {
      assert(get_ref_frame_buffer(cpi, ref_frame) != NULL);
      setup_buffer_ref_mvs_inter(cpi, x, ref_frame, bsize, mi_row, mi_col,
                                 yv12_mb);
    }
  }

  // TODO(zoeliu@google.com): To further optimize the obtaining of motion vector
  // references for compound prediction, as not every pair of reference frames
  // woud be examined for the RD evaluation.
  for (; ref_frame < MODE_CTX_REF_FRAMES; ++ref_frame) {
    x->mbmi_ext->mode_context[ref_frame] = 0;
    av1_find_mv_refs(cm, xd, mbmi, ref_frame, mbmi_ext->ref_mv_count,
                     mbmi_ext->ref_mv_stack, NULL, mbmi_ext->global_mvs, mi_row,
                     mi_col, mbmi_ext->mode_context);
  }

  av1_count_overlappable_neighbors(cm, xd, mi_row, mi_col);

  if (check_num_overlappable_neighbors(mbmi) &&
      is_motion_variation_allowed_bsize(bsize)) {
    av1_build_prediction_by_above_preds(cm, xd, mi_row, mi_col,
                                        args->above_pred_buf, dst_width1,
                                        dst_height1, args->above_pred_stride);
    av1_build_prediction_by_left_preds(cm, xd, mi_row, mi_col,
                                       args->left_pred_buf, dst_width2,
                                       dst_height2, args->left_pred_stride);
    av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                         mi_col, 0, num_planes);
    calc_target_weighted_pred(
        cm, x, xd, mi_row, mi_col, args->above_pred_buf[0],
        args->above_pred_stride[0], args->left_pred_buf[0],
        args->left_pred_stride[0]);
  }

  int min_pred_mv_sad = INT_MAX;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame)
    min_pred_mv_sad = AOMMIN(min_pred_mv_sad, x->pred_mv_sad[ref_frame]);

  for (int i = 0; i < 2; ++i) {
    ref_frame_skip_mask[i] = 0;
  }
  memset(mode_skip_mask, 0, REF_FRAMES * sizeof(*mode_skip_mask));
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (!(cpi->ref_frame_flags & ref_frame_flag_list[ref_frame])) {
      // Skip checking missing references in both single and compound reference
      // modes. Note that a mode will be skipped iff both reference frames
      // are masked out.
      ref_frame_skip_mask[0] |= (1 << ref_frame);
      ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
    } else {
      // Skip fixed mv modes for poor references
      if ((x->pred_mv_sad[ref_frame] >> 2) > min_pred_mv_sad) {
        mode_skip_mask[ref_frame] |= INTER_NEAREST_NEAR_ZERO;
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
    // Only consider GLOBALMV/ALTREF_FRAME for alt ref frame,
    // unless ARNR filtering is enabled in which case we want
    // an unfiltered alternative. We allow near/nearest as well
    // because they may result in zero-zero MVs but be cheaper.
    if (cpi->rc.is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0)) {
      ref_frame_skip_mask[0] = (1 << LAST_FRAME) | (1 << LAST2_FRAME) |
                               (1 << LAST3_FRAME) | (1 << BWDREF_FRAME) |
                               (1 << ALTREF2_FRAME) | (1 << GOLDEN_FRAME);
      ref_frame_skip_mask[1] = SECOND_REF_FRAME_MASK;
      // TODO(zoeliu): To further explore whether following needs to be done for
      //               BWDREF_FRAME as well.
      mode_skip_mask[ALTREF_FRAME] = ~INTER_NEAREST_NEAR_ZERO;
      const MV_REFERENCE_FRAME tmp_ref_frames[2] = { ALTREF_FRAME, NONE_FRAME };
      int_mv near_mv, nearest_mv, global_mv;
      get_this_mv(&nearest_mv, NEARESTMV, 0, 0, tmp_ref_frames, x->mbmi_ext);
      get_this_mv(&near_mv, NEARMV, 0, 0, tmp_ref_frames, x->mbmi_ext);
      get_this_mv(&global_mv, GLOBALMV, 0, 0, tmp_ref_frames, x->mbmi_ext);

      if (near_mv.as_int != global_mv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARMV);
      if (nearest_mv.as_int != global_mv.as_int)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARESTMV);
    }
  }

  if (cpi->rc.is_src_frame_alt_ref) {
    if (sf->alt_ref_search_fp) {
      assert(cpi->ref_frame_flags & ref_frame_flag_list[ALTREF_FRAME]);
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

  if (cpi->sf.tx_type_search.fast_intra_tx_type_search)
    x->use_default_intra_tx_type = 1;
  else
    x->use_default_intra_tx_type = 0;

  if (cpi->sf.tx_type_search.fast_inter_tx_type_search)
    x->use_default_inter_tx_type = 1;
  else
    x->use_default_inter_tx_type = 0;
  if (cpi->sf.skip_repeat_interpolation_filter_search) {
    x->interp_filter_stats_idx[0] = 0;
    x->interp_filter_stats_idx[1] = 0;
  }
}

static void search_palette_mode(const AV1_COMP *cpi, MACROBLOCK *x,
                                RD_STATS *rd_cost, PICK_MODE_CONTEXT *ctx,
                                BLOCK_SIZE bsize, MB_MODE_INFO *const mbmi,
                                PALETTE_MODE_INFO *const pmi,
                                unsigned int *ref_costs_single,
                                InterModeSearchState *search_state) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  int rate2 = 0;
  int64_t distortion2 = 0, best_rd_palette = search_state->best_rd, this_rd,
          best_model_rd_palette = INT64_MAX;
  int skippable = 0, rate_overhead_palette = 0;
  RD_STATS rd_stats_y;
  TX_SIZE uv_tx = TX_4X4;
  uint8_t *const best_palette_color_map =
      x->palette_buffer->best_palette_color_map;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  MB_MODE_INFO best_mbmi_palette = *mbmi;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const int *const intra_mode_cost = x->mbmode_cost[size_group_lookup[bsize]];
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];

  mbmi->mode = DC_PRED;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  rate_overhead_palette = rd_pick_palette_intra_sby(
      cpi, x, bsize, intra_mode_cost[DC_PRED], &best_mbmi_palette,
      best_palette_color_map, &best_rd_palette, &best_model_rd_palette, NULL,
      NULL, NULL, NULL, ctx, best_blk_skip);
  if (pmi->palette_size[0] == 0) return;

  memcpy(x->blk_skip, best_blk_skip,
         sizeof(best_blk_skip[0]) * bsize_to_num_blk(bsize));

  memcpy(color_map, best_palette_color_map,
         rows * cols * sizeof(best_palette_color_map[0]));
  super_block_yrd(cpi, x, &rd_stats_y, bsize, search_state->best_rd);
  if (rd_stats_y.rate == INT_MAX) return;

  skippable = rd_stats_y.skip;
  distortion2 = rd_stats_y.dist;
  rate2 = rd_stats_y.rate + rate_overhead_palette;
  rate2 += ref_costs_single[INTRA_FRAME];
  if (num_planes > 1) {
    uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
    if (search_state->rate_uv_intra[uv_tx] == INT_MAX) {
      choose_intra_uv_mode(
          cpi, x, bsize, uv_tx, &search_state->rate_uv_intra[uv_tx],
          &search_state->rate_uv_tokenonly[uv_tx],
          &search_state->dist_uvs[uv_tx], &search_state->skip_uvs[uv_tx],
          &search_state->mode_uv[uv_tx]);
      search_state->pmi_uv[uv_tx] = *pmi;
      search_state->uv_angle_delta[uv_tx] = mbmi->angle_delta[PLANE_TYPE_UV];
    }
    mbmi->uv_mode = search_state->mode_uv[uv_tx];
    pmi->palette_size[1] = search_state->pmi_uv[uv_tx].palette_size[1];
    if (pmi->palette_size[1] > 0) {
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             search_state->pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = search_state->uv_angle_delta[uv_tx];
    skippable = skippable && search_state->skip_uvs[uv_tx];
    distortion2 += search_state->dist_uvs[uv_tx];
    rate2 += search_state->rate_uv_intra[uv_tx];
  }

  if (skippable) {
    rate2 -= rd_stats_y.rate;
    if (num_planes > 1) rate2 -= search_state->rate_uv_tokenonly[uv_tx];
    rate2 += x->skip_cost[av1_get_skip_context(xd)][1];
  } else {
    rate2 += x->skip_cost[av1_get_skip_context(xd)][0];
  }
  this_rd = RDCOST(x->rdmult, rate2, distortion2);
  if (this_rd < search_state->best_rd) {
    search_state->best_mode_index = 3;
    mbmi->mv[0].as_int = 0;
    rd_cost->rate = rate2;
    rd_cost->dist = distortion2;
    rd_cost->rdcost = this_rd;
    search_state->best_rd = this_rd;
    search_state->best_mbmode = *mbmi;
    search_state->best_skip2 = 0;
    search_state->best_mode_skippable = skippable;
    memcpy(ctx->blk_skip, x->blk_skip,
           sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
  }
}

static void init_inter_mode_search_state(InterModeSearchState *search_state,
                                         const AV1_COMP *cpi,
                                         const TileDataEnc *tile_data,
                                         const MACROBLOCK *x, BLOCK_SIZE bsize,
                                         int64_t best_rd_so_far) {
  search_state->best_rd = best_rd_so_far;

  av1_zero(search_state->best_mbmode);

  search_state->best_rate_y = INT_MAX;

  search_state->best_rate_uv = INT_MAX;

  search_state->best_mode_skippable = 0;

  search_state->best_skip2 = 0;

  search_state->best_mode_index = -1;

  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const unsigned char segment_id = mbmi->segment_id;

  search_state->skip_intra_modes = 0;

  search_state->num_available_refs = 0;
  memset(search_state->dist_refs, -1, sizeof(search_state->dist_refs));
  memset(search_state->dist_order_refs, -1,
         sizeof(search_state->dist_order_refs));

  for (int i = 0; i <= LAST_NEW_MV_INDEX; ++i)
    search_state->mode_threshold[i] = 0;
  const int *const rd_threshes = cpi->rd.threshes[segment_id][bsize];
  for (int i = LAST_NEW_MV_INDEX + 1; i < MAX_MODES; ++i)
    search_state->mode_threshold[i] =
        ((int64_t)rd_threshes[i] * tile_data->thresh_freq_fact[bsize][i]) >> 5;

  search_state->best_intra_mode = DC_PRED;
  search_state->best_intra_rd = INT64_MAX;

  search_state->angle_stats_ready = 0;

  search_state->best_pred_sse = UINT_MAX;

  for (int i = 0; i < TX_SIZES_ALL; i++)
    search_state->rate_uv_intra[i] = INT_MAX;

  av1_zero(search_state->pmi_uv);

  for (int i = 0; i < REFERENCE_MODES; ++i)
    search_state->best_pred_rd[i] = INT64_MAX;

  av1_zero(search_state->single_newmv);
  av1_zero(search_state->single_newmv_rate);
  av1_zero(search_state->single_newmv_valid);
  for (int i = 0; i < MB_MODE_COUNT; ++i)
    for (int ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame)
      search_state->modelled_rd[i][ref_frame] = INT64_MAX;
}

static int inter_mode_search_order_independent_skip(
    const AV1_COMP *cpi, const MACROBLOCK *x, BLOCK_SIZE bsize, int mode_index,
    int mi_row, int mi_col, uint32_t *mode_skip_mask,
    uint16_t *ref_frame_skip_mask) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  const AV1_COMMON *const cm = &cpi->common;
  const struct segmentation *const seg = &cm->seg;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const unsigned char segment_id = mbmi->segment_id;
  const MV_REFERENCE_FRAME *ref_frame = av1_mode_order[mode_index].ref_frame;
  const PREDICTION_MODE this_mode = av1_mode_order[mode_index].mode;

  if (cpi->sf.mode_pruning_based_on_two_pass_partition_search &&
      !x->cb_partition_scan) {
    const int mi_width = mi_size_wide[bsize];
    const int mi_height = mi_size_high[bsize];
    int found = 0;
    // Search in the stats table to see if the ref frames have been used in the
    // first pass of partition search.
    for (int row = mi_row; row < mi_row + mi_width && !found;
         row += FIRST_PARTITION_PASS_SAMPLE_REGION) {
      for (int col = mi_col; col < mi_col + mi_height && !found;
           col += FIRST_PARTITION_PASS_SAMPLE_REGION) {
        const int index = av1_first_partition_pass_stats_index(row, col);
        const FIRST_PARTITION_PASS_STATS *const stats =
            &x->first_partition_pass_stats[index];
        if (stats->ref0_counts[ref_frame[0]] &&
            (ref_frame[1] < 0 || stats->ref1_counts[ref_frame[1]])) {
          found = 1;
          break;
        }
      }
    }
    if (!found) return 1;
  }

  if (ref_frame[0] > INTRA_FRAME && ref_frame[1] == INTRA_FRAME) {
    // Mode must by compatible
    if (!is_interintra_allowed_mode(this_mode)) return 1;
    if (!is_interintra_allowed_bsize(bsize)) return 1;
  }

  // This is only used in motion vector unit test.
  if (cpi->oxcf.motion_vector_unit_test && ref_frame[0] == INTRA_FRAME)
    return 1;

  if (ref_frame[0] == INTRA_FRAME) {
    if (this_mode != DC_PRED) {
      // Disable intra modes other than DC_PRED for blocks with low variance
      // Threshold for intra skipping based on source variance
      // TODO(debargha): Specialize the threshold for super block sizes
      const unsigned int skip_intra_var_thresh = 64;
      if ((sf->mode_search_skip_flags & FLAG_SKIP_INTRA_LOWVAR) &&
          x->source_variance < skip_intra_var_thresh)
        return 1;
    }
  } else {
    if (!is_comp_ref_allowed(bsize) && ref_frame[1] > INTRA_FRAME) return 1;
  }

  const int comp_pred = ref_frame[1] > INTRA_FRAME;
  if (comp_pred) {
    if (!cpi->allow_comp_inter_inter) return 1;

    if (cm->reference_mode == SINGLE_REFERENCE) return 1;

    // Skip compound inter modes if ARF is not available.
    if (!(cpi->ref_frame_flags & ref_frame_flag_list[ref_frame[1]])) return 1;

    // Do not allow compound prediction if the segment level reference frame
    // feature is in use as in this case there can only be one reference.
    if (segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) return 1;
  }

  if (sf->selective_ref_frame) {
    if (sf->selective_ref_frame >= 2 || x->cb_partition_scan) {
      if (ref_frame[0] == ALTREF2_FRAME || ref_frame[1] == ALTREF2_FRAME)
        if (get_relative_dist(
                cm, cm->cur_frame->ref_frame_offset[ALTREF2_FRAME - LAST_FRAME],
                cm->frame_offset) < 0)
          return 1;
      if (ref_frame[0] == BWDREF_FRAME || ref_frame[1] == BWDREF_FRAME)
        if (get_relative_dist(
                cm, cm->cur_frame->ref_frame_offset[BWDREF_FRAME - LAST_FRAME],
                cm->frame_offset) < 0)
          return 1;
    }
    if (ref_frame[0] == LAST3_FRAME || ref_frame[1] == LAST3_FRAME)
      if (get_relative_dist(
              cm, cm->cur_frame->ref_frame_offset[LAST3_FRAME - LAST_FRAME],
              cm->cur_frame->ref_frame_offset[GOLDEN_FRAME - LAST_FRAME]) <= 0)
        return 1;
    if (ref_frame[0] == LAST2_FRAME || ref_frame[1] == LAST2_FRAME)
      if (get_relative_dist(
              cm, cm->cur_frame->ref_frame_offset[LAST2_FRAME - LAST_FRAME],
              cm->cur_frame->ref_frame_offset[GOLDEN_FRAME - LAST_FRAME]) <= 0)
        return 1;
  }

  // One-sided compound is used only when all reference frames are one-sided.
  if (sf->selective_ref_frame && comp_pred && !cpi->all_one_sided_refs) {
    unsigned int ref_offsets[2];
    for (int i = 0; i < 2; ++i) {
      const int buf_idx = cm->frame_refs[ref_frame[i] - LAST_FRAME].idx;
      assert(buf_idx >= 0);
      ref_offsets[i] = cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset;
    }
    if ((get_relative_dist(cm, ref_offsets[0], cm->frame_offset) <= 0 &&
         get_relative_dist(cm, ref_offsets[1], cm->frame_offset) <= 0) ||
        (get_relative_dist(cm, ref_offsets[0], cm->frame_offset) > 0 &&
         get_relative_dist(cm, ref_offsets[1], cm->frame_offset) > 0))
      return 1;
  }

  if (mode_skip_mask[ref_frame[0]] & (1 << this_mode)) {
    return 1;
  }

  if ((ref_frame_skip_mask[0] & (1 << ref_frame[0])) &&
      (ref_frame_skip_mask[1] & (1 << AOMMAX(0, ref_frame[1])))) {
    return 1;
  }

  if (skip_repeated_mv(cm, x, this_mode, ref_frame)) {
    return 1;
  }
  return 0;
}

static INLINE void init_mbmi(MB_MODE_INFO *mbmi, int mode_index,
                             const AV1_COMMON *cm) {
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  PREDICTION_MODE this_mode = av1_mode_order[mode_index].mode;
  mbmi->ref_mv_idx = 0;
  mbmi->mode = this_mode;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = av1_mode_order[mode_index].ref_frame[0];
  mbmi->ref_frame[1] = av1_mode_order[mode_index].ref_frame[1];
  pmi->palette_size[0] = 0;
  pmi->palette_size[1] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  mbmi->mv[0].as_int = mbmi->mv[1].as_int = 0;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->interintra_mode = (INTERINTRA_MODE)(II_DC_PRED - 1);
  set_default_interp_filters(mbmi, cm->interp_filter);
}

static int handle_intra_mode(InterModeSearchState *search_state,
                             const AV1_COMP *cpi, MACROBLOCK *x,
                             BLOCK_SIZE bsize, int ref_frame_cost,
                             const PICK_MODE_CONTEXT *ctx, int disable_skip,
                             RD_STATS *rd_stats, RD_STATS *rd_stats_y,
                             RD_STATS *rd_stats_uv) {
  const AV1_COMMON *cm = &cpi->common;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->ref_frame[0] == INTRA_FRAME);
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      av1_allow_palette(cm->allow_screen_content_tools, mbmi->sb_type);
  const int *const intra_mode_cost = x->mbmode_cost[size_group_lookup[bsize]];
  const int intra_cost_penalty = av1_get_intra_cost_penalty(
      cm->base_qindex, cm->y_dc_delta_q, cm->seq_params.bit_depth);
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  const int num_planes = av1_num_planes(cm);
  av1_init_rd_stats(rd_stats);
  av1_init_rd_stats(rd_stats_y);
  av1_init_rd_stats(rd_stats_uv);
  TX_SIZE uv_tx;
  int is_directional_mode = av1_is_directional_mode(mbmi->mode);
  if (is_directional_mode && av1_use_angle_delta(bsize)) {
    int rate_dummy;
    int64_t model_rd = INT64_MAX;
    if (!search_state->angle_stats_ready) {
      const int src_stride = x->plane[0].src.stride;
      const uint8_t *src = x->plane[0].src.buf;
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
        highbd_angle_estimation(src, src_stride, rows, cols, bsize,
                                search_state->directional_mode_skip_mask);
      else
        angle_estimation(src, src_stride, rows, cols, bsize,
                         search_state->directional_mode_skip_mask);
      search_state->angle_stats_ready = 1;
    }
    if (search_state->directional_mode_skip_mask[mbmi->mode]) return 0;
    rd_stats_y->rate = INT_MAX;
    rd_pick_intra_angle_sby(cpi, x, &rate_dummy, rd_stats_y, bsize,
                            intra_mode_cost[mbmi->mode], search_state->best_rd,
                            &model_rd);
  } else {
    mbmi->angle_delta[PLANE_TYPE_Y] = 0;
    super_block_yrd(cpi, x, rd_stats_y, bsize, search_state->best_rd);
  }
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  memcpy(best_blk_skip, x->blk_skip,
         sizeof(best_blk_skip[0]) * ctx->num_4x4_blk);

  if (mbmi->mode == DC_PRED && av1_filter_intra_allowed_bsize(cm, bsize)) {
    RD_STATS rd_stats_y_fi;
    int filter_intra_selected_flag = 0;
    TX_SIZE best_tx_size = mbmi->tx_size;
    TX_TYPE best_txk_type[TXK_TYPE_BUF_LEN];
    memcpy(best_txk_type, mbmi->txk_type,
           sizeof(*best_txk_type) * TXK_TYPE_BUF_LEN);
    FILTER_INTRA_MODE best_fi_mode = FILTER_DC_PRED;
    int64_t best_rd_tmp = INT64_MAX;
    if (rd_stats_y->rate != INT_MAX) {
      best_rd_tmp = RDCOST(x->rdmult,
                           rd_stats_y->rate + x->filter_intra_cost[bsize][0] +
                               intra_mode_cost[mbmi->mode],
                           rd_stats_y->dist);
    }

    mbmi->filter_intra_mode_info.use_filter_intra = 1;
    for (FILTER_INTRA_MODE fi_mode = FILTER_DC_PRED;
         fi_mode < FILTER_INTRA_MODES; ++fi_mode) {
      int64_t this_rd_tmp;
      mbmi->filter_intra_mode_info.filter_intra_mode = fi_mode;

      super_block_yrd(cpi, x, &rd_stats_y_fi, bsize, search_state->best_rd);
      if (rd_stats_y_fi.rate == INT_MAX) {
        continue;
      }
      const int this_rate_tmp =
          rd_stats_y_fi.rate +
          intra_mode_info_cost_y(cpi, x, mbmi, bsize,
                                 intra_mode_cost[mbmi->mode]);
      this_rd_tmp = RDCOST(x->rdmult, this_rate_tmp, rd_stats_y_fi.dist);

      if (this_rd_tmp < best_rd_tmp) {
        best_tx_size = mbmi->tx_size;
        memcpy(best_txk_type, mbmi->txk_type,
               sizeof(*best_txk_type) * TXK_TYPE_BUF_LEN);
        memcpy(best_blk_skip, x->blk_skip,
               sizeof(best_blk_skip[0]) * ctx->num_4x4_blk);
        best_fi_mode = fi_mode;
        *rd_stats_y = rd_stats_y_fi;
        filter_intra_selected_flag = 1;
        best_rd_tmp = this_rd_tmp;
      }
    }

    mbmi->tx_size = best_tx_size;
    memcpy(mbmi->txk_type, best_txk_type,
           sizeof(*best_txk_type) * TXK_TYPE_BUF_LEN);
    memcpy(x->blk_skip, best_blk_skip,
           sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);

    if (filter_intra_selected_flag) {
      mbmi->filter_intra_mode_info.use_filter_intra = 1;
      mbmi->filter_intra_mode_info.filter_intra_mode = best_fi_mode;
    } else {
      mbmi->filter_intra_mode_info.use_filter_intra = 0;
    }
  }

  if (rd_stats_y->rate == INT_MAX) return 0;

  if (num_planes > 1) {
    uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
    if (search_state->rate_uv_intra[uv_tx] == INT_MAX) {
      choose_intra_uv_mode(
          cpi, x, bsize, uv_tx, &search_state->rate_uv_intra[uv_tx],
          &search_state->rate_uv_tokenonly[uv_tx],
          &search_state->dist_uvs[uv_tx], &search_state->skip_uvs[uv_tx],
          &search_state->mode_uv[uv_tx]);
      if (try_palette) search_state->pmi_uv[uv_tx] = *pmi;
      search_state->uv_angle_delta[uv_tx] = mbmi->angle_delta[PLANE_TYPE_UV];
    }

    rd_stats_uv->rate = search_state->rate_uv_tokenonly[uv_tx];
    rd_stats_uv->dist = search_state->dist_uvs[uv_tx];
    rd_stats_uv->skip = search_state->skip_uvs[uv_tx];
    rd_stats->skip = rd_stats_y->skip && rd_stats_uv->skip;
    mbmi->uv_mode = search_state->mode_uv[uv_tx];
    if (try_palette) {
      pmi->palette_size[1] = search_state->pmi_uv[uv_tx].palette_size[1];
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             search_state->pmi_uv[uv_tx].palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = search_state->uv_angle_delta[uv_tx];
  }

  rd_stats->rate =
      rd_stats_y->rate +
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, intra_mode_cost[mbmi->mode]);
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(bsize)) {
    // super_block_yrd above includes the cost of the tx_size in the
    // tokenonly rate, but for intra blocks, tx_size is always coded
    // (prediction granularity), so we account for it in the full rate,
    // not the tokenonly rate.
    rd_stats_y->rate -= tx_size_cost(cm, x, bsize, mbmi->tx_size);
  }
  if (num_planes > 1 && !x->skip_chroma_rd) {
    const int uv_mode_cost =
        x->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mbmi->uv_mode];
    rd_stats->rate +=
        rd_stats_uv->rate +
        intra_mode_info_cost_uv(cpi, x, mbmi, bsize, uv_mode_cost);
  }
  if (mbmi->mode != DC_PRED && mbmi->mode != PAETH_PRED)
    rd_stats->rate += intra_cost_penalty;
  rd_stats->dist = rd_stats_y->dist + rd_stats_uv->dist;

  // Estimate the reference frame signaling cost and add it
  // to the rolling cost variable.
  rd_stats->rate += ref_frame_cost;
  if (rd_stats->skip) {
    // Back out the coefficient coding costs
    rd_stats->rate -= (rd_stats_y->rate + rd_stats_uv->rate);
    rd_stats_y->rate = 0;
    rd_stats_uv->rate = 0;
    // Cost the skip mb case
    rd_stats->rate += x->skip_cost[av1_get_skip_context(xd)][1];
  } else {
    // Add in the cost of the no skip flag.
    rd_stats->rate += x->skip_cost[av1_get_skip_context(xd)][0];
  }
  // Calculate the final RD estimate for this mode.
  int64_t this_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);

  // Keep record of best intra rd
  if (this_rd < search_state->best_intra_rd) {
    search_state->best_intra_rd = this_rd;
    search_state->best_intra_mode = mbmi->mode;
  }

  if (sf->skip_intra_in_interframe) {
    if (search_state->best_rd < (INT64_MAX / 2) &&
        this_rd > (search_state->best_rd + (search_state->best_rd >> 1)))
      search_state->skip_intra_modes = 1;
  }

  if (!disable_skip) {
    for (int i = 0; i < REFERENCE_MODES; ++i)
      search_state->best_pred_rd[i] =
          AOMMIN(search_state->best_pred_rd[i], this_rd);
  }
  return 1;
}

void av1_rd_pick_inter_mode_sb(const AV1_COMP *cpi, TileDataEnc *tile_data,
                               MACROBLOCK *x, int mi_row, int mi_col,
                               RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int try_palette =
      av1_allow_palette(cm->allow_screen_content_tools, mbmi->sb_type);
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const struct segmentation *const seg = &cm->seg;
  PREDICTION_MODE this_mode;
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int i, k;
  struct buf_2d yv12_mb[REF_FRAMES][MAX_MB_PLANE];
  unsigned int ref_costs_single[REF_FRAMES];
  unsigned int ref_costs_comp[REF_FRAMES][REF_FRAMES];
  int *comp_inter_cost = x->comp_inter_cost[av1_get_reference_mode_context(xd)];
  int *mode_map = tile_data->mode_map[bsize];
  uint32_t mode_skip_mask[REF_FRAMES];
  uint16_t ref_frame_skip_mask[2];

  InterModeSearchState search_state;
  init_inter_mode_search_state(&search_state, cpi, tile_data, x, bsize,
                               best_rd_so_far);

  HandleInterModeArgs args = {
    { NULL },  { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE },
    { NULL },  { MAX_SB_SIZE >> 1, MAX_SB_SIZE >> 1, MAX_SB_SIZE >> 1 },
    NULL,      NULL,
    NULL,      NULL,
    { { 0 } }, INT_MAX,
    INT_MAX
  };
  for (i = 0; i < REF_FRAMES; ++i) x->pred_sse[i] = INT_MAX;

  av1_invalid_rd_stats(rd_cost);

  // init params, set frame modes, speed features
  set_params_rd_pick_inter_mode(cpi, x, &args, bsize, mi_row, mi_col,
                                ref_frame_skip_mask, mode_skip_mask,
                                ref_costs_single, ref_costs_comp, yv12_mb);

#if CONFIG_COLLECT_INTER_MODE_RD_STATS
  int64_t best_est_rd = INT64_MAX;
#endif

  for (int midx = 0; midx < MAX_MODES; ++midx) {
    int mode_index = mode_map[midx];
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0;
    int skippable = 0;
    int this_skip2 = 0;

    this_mode = av1_mode_order[mode_index].mode;
    ref_frame = av1_mode_order[mode_index].ref_frame[0];
    second_ref_frame = av1_mode_order[mode_index].ref_frame[1];

    init_mbmi(mbmi, mode_index, cm);

    x->skip = 0;
    set_ref_ptrs(cm, xd, ref_frame, second_ref_frame);

    if (inter_mode_search_order_independent_skip(cpi, x, bsize, mode_index,
                                                 mi_row, mi_col, mode_skip_mask,
                                                 ref_frame_skip_mask))
      continue;

    if (ref_frame == INTRA_FRAME) {
      if (sf->skip_intra_in_interframe && search_state.skip_intra_modes)
        continue;
    }

    if (sf->drop_ref) {
      if (ref_frame > INTRA_FRAME && second_ref_frame > INTRA_FRAME) {
        if (search_state.num_available_refs > 2) {
          if ((ref_frame == search_state.dist_order_refs[0] &&
               second_ref_frame == search_state.dist_order_refs[1]) ||
              (ref_frame == search_state.dist_order_refs[1] &&
               second_ref_frame == search_state.dist_order_refs[0]))
            continue;
        }
      }
    }

    if (search_state.best_rd < search_state.mode_threshold[mode_index])
      continue;

    const int comp_pred = second_ref_frame > INTRA_FRAME;
    const int ref_frame_cost = comp_pred
                                   ? ref_costs_comp[ref_frame][second_ref_frame]
                                   : ref_costs_single[ref_frame];
    const int compmode_cost =
        is_comp_ref_allowed(mbmi->sb_type) ? comp_inter_cost[comp_pred] : 0;
    const int real_compmode_cost =
        cm->reference_mode == REFERENCE_MODE_SELECT ? compmode_cost : 0;

    if (comp_pred) {
      if ((sf->mode_search_skip_flags & FLAG_SKIP_COMP_BESTINTRA) &&
          search_state.best_mode_index >= 0 &&
          search_state.best_mbmode.ref_frame[0] == INTRA_FRAME)
        continue;
    }

    if (ref_frame == INTRA_FRAME) {
      if (sf->adaptive_mode_search)
        if ((x->source_variance << num_pels_log2_lookup[bsize]) >
            search_state.best_pred_sse)
          continue;

      if (this_mode != DC_PRED) {
        // Only search the oblique modes if the best so far is
        // one of the neighboring directional modes
        if ((sf->mode_search_skip_flags & FLAG_SKIP_INTRA_BESTINTER) &&
            (this_mode >= D45_PRED && this_mode <= PAETH_PRED)) {
          if (search_state.best_mode_index >= 0 &&
              search_state.best_mbmode.ref_frame[0] > INTRA_FRAME)
            continue;
        }
        if (sf->mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
          if (conditional_skipintra(this_mode, search_state.best_intra_mode))
            continue;
        }
      }
    }

    // Select prediction reference frames.
    for (i = 0; i < num_planes; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred) xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

    if (ref_frame == INTRA_FRAME) {
      RD_STATS intra_rd_stats, intra_rd_stats_y, intra_rd_stats_uv;
      const int ret = handle_intra_mode(
          &search_state, cpi, x, bsize, ref_frame_cost, ctx, disable_skip,
          &intra_rd_stats, &intra_rd_stats_y, &intra_rd_stats_uv);
      if (!ret) {
        continue;
      }
      rate2 = intra_rd_stats.rate;
      distortion2 = intra_rd_stats.dist;
      this_rd = RDCOST(x->rdmult, rate2, distortion2);
      skippable = intra_rd_stats.skip;
      rate_y = intra_rd_stats_y.rate;
    } else {
      mbmi->angle_delta[PLANE_TYPE_Y] = 0;
      mbmi->angle_delta[PLANE_TYPE_UV] = 0;
      mbmi->filter_intra_mode_info.use_filter_intra = 0;
      mbmi->ref_mv_idx = 0;
      int64_t ref_best_rd = search_state.best_rd;
      {
        RD_STATS rd_stats, rd_stats_y, rd_stats_uv;
        av1_init_rd_stats(&rd_stats);
        rd_stats.rate = rate2;

        // Point to variables that are maintained between loop iterations
        args.single_newmv = search_state.single_newmv;
        args.single_newmv_rate = search_state.single_newmv_rate;
        args.single_newmv_valid = search_state.single_newmv_valid;
        args.modelled_rd = search_state.modelled_rd;
        args.single_comp_cost = real_compmode_cost;
        args.ref_frame_cost = ref_frame_cost;
#if CONFIG_COLLECT_INTER_MODE_RD_STATS
        this_rd = handle_inter_mode(cpi, x, bsize, &rd_stats, &rd_stats_y,
                                    &rd_stats_uv, &disable_skip, mi_row, mi_col,
                                    &args, ref_best_rd, &best_est_rd);
#else
        this_rd = handle_inter_mode(cpi, x, bsize, &rd_stats, &rd_stats_y,
                                    &rd_stats_uv, &disable_skip, mi_row, mi_col,
                                    &args, ref_best_rd);
#endif
        rate2 = rd_stats.rate;
        skippable = rd_stats.skip;
        distortion2 = rd_stats.dist;
        rate_y = rd_stats_y.rate;
        rate_uv = rd_stats_uv.rate;
      }

      if (this_rd == INT64_MAX) continue;

      this_skip2 = mbmi->skip;
      this_rd = RDCOST(x->rdmult, rate2, distortion2);
      if (this_skip2) {
        rate_y = 0;
        rate_uv = 0;
      }
    }

    // Did this mode help.. i.e. is it the new best mode
    if (this_rd < search_state.best_rd || x->skip) {
      int mode_excluded = 0;
      if (comp_pred) {
        mode_excluded = cm->reference_mode == SINGLE_REFERENCE;
      }
      if (!mode_excluded) {
        // Note index of best mode so far
        search_state.best_mode_index = mode_index;

        if (ref_frame == INTRA_FRAME) {
          /* required for left and above block mv */
          mbmi->mv[0].as_int = 0;
        } else {
          search_state.best_pred_sse = x->pred_sse[ref_frame];
        }

        rd_cost->rate = rate2;
        rd_cost->dist = distortion2;
        rd_cost->rdcost = this_rd;
        search_state.best_rd = this_rd;
        search_state.best_mbmode = *mbmi;
        search_state.best_skip2 = this_skip2;
        search_state.best_mode_skippable = skippable;
        search_state.best_rate_y =
            rate_y +
            x->skip_cost[av1_get_skip_context(xd)][this_skip2 || skippable];
        search_state.best_rate_uv = rate_uv;
        memcpy(ctx->blk_skip, x->blk_skip,
               sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);
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

      single_rd = RDCOST(x->rdmult, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, hybrid_rate, distortion2);

      if (!comp_pred) {
        if (single_rd < search_state.best_pred_rd[SINGLE_REFERENCE])
          search_state.best_pred_rd[SINGLE_REFERENCE] = single_rd;
      } else {
        if (single_rd < search_state.best_pred_rd[COMPOUND_REFERENCE])
          search_state.best_pred_rd[COMPOUND_REFERENCE] = single_rd;
      }
      if (hybrid_rd < search_state.best_pred_rd[REFERENCE_MODE_SELECT])
        search_state.best_pred_rd[REFERENCE_MODE_SELECT] = hybrid_rd;
    }

    if (sf->drop_ref) {
      if (second_ref_frame == NONE_FRAME) {
        const int idx = ref_frame - LAST_FRAME;
        if (idx && distortion2 > search_state.dist_refs[idx]) {
          search_state.dist_refs[idx] = distortion2;
          search_state.dist_order_refs[idx] = ref_frame;
        }

        // Reach the last single ref prediction mode
        if (ref_frame == ALTREF_FRAME && this_mode == GLOBALMV) {
          // bubble sort dist_refs and the order index
          for (i = 0; i < REF_FRAMES; ++i) {
            for (k = i + 1; k < REF_FRAMES; ++k) {
              if (search_state.dist_refs[i] < search_state.dist_refs[k]) {
                int64_t tmp_dist = search_state.dist_refs[i];
                search_state.dist_refs[i] = search_state.dist_refs[k];
                search_state.dist_refs[k] = tmp_dist;

                int tmp_idx = search_state.dist_order_refs[i];
                search_state.dist_order_refs[i] =
                    search_state.dist_order_refs[k];
                search_state.dist_order_refs[k] = tmp_idx;
              }
            }
          }

          for (i = 0; i < REF_FRAMES; ++i) {
            if (search_state.dist_refs[i] == -1) break;
            search_state.num_available_refs = i;
          }
          search_state.num_available_refs++;
        }
      }
    }

    if (x->skip && !comp_pred) break;
  }

  // In effect only when speed >= 2.
  sf_refine_fast_tx_type_search(
      cpi, x, mi_row, mi_col, rd_cost, bsize, ctx, search_state.best_mode_index,
      &search_state.best_mbmode, yv12_mb, search_state.best_rate_y,
      search_state.best_rate_uv, &search_state.best_skip2);

  // Only try palette mode when the best mode so far is an intra mode.
  if (try_palette && !is_inter_mode(search_state.best_mbmode.mode)) {
    search_palette_mode(cpi, x, rd_cost, ctx, bsize, mbmi, pmi,
                        ref_costs_single, &search_state);
  }

  search_state.best_mbmode.skip_mode = 0;
  if (cm->skip_mode_flag &&
      !segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
      is_comp_ref_allowed(bsize)) {
    rd_pick_skip_mode(rd_cost, &search_state, cpi, x, bsize, mi_row, mi_col,
                      yv12_mb);
  }

  // Make sure that the ref_mv_idx is only nonzero when we're
  // using a mode which can support ref_mv_idx
  if (search_state.best_mbmode.ref_mv_idx != 0 &&
      !(search_state.best_mbmode.mode == NEWMV ||
        search_state.best_mbmode.mode == NEW_NEWMV ||
        have_nearmv_in_inter_mode(search_state.best_mbmode.mode))) {
    search_state.best_mbmode.ref_mv_idx = 0;
  }

  if (search_state.best_mode_index < 0 ||
      search_state.best_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert(
      (cm->interp_filter == SWITCHABLE) ||
      (cm->interp_filter ==
       av1_extract_interp_filter(search_state.best_mbmode.interp_filters, 0)) ||
      !is_inter_block(&search_state.best_mbmode));
  assert(
      (cm->interp_filter == SWITCHABLE) ||
      (cm->interp_filter ==
       av1_extract_interp_filter(search_state.best_mbmode.interp_filters, 1)) ||
      !is_inter_block(&search_state.best_mbmode));

  if (!cpi->rc.is_src_frame_alt_ref)
    av1_update_rd_thresh_fact(cm, tile_data->thresh_freq_fact,
                              sf->adaptive_rd_thresh, bsize,
                              search_state.best_mode_index);

  // macroblock modes
  *mbmi = search_state.best_mbmode;
  x->skip |= search_state.best_skip2;

  // Note: this section is needed since the mode may have been forced to
  // GLOBALMV by the all-zero mode handling of ref-mv.
  if (mbmi->mode == GLOBALMV || mbmi->mode == GLOBAL_GLOBALMV) {
    // Correct the interp filters for GLOBALMV
    if (is_nontrans_global_motion(xd, xd->mi[0])) {
      assert(mbmi->interp_filters ==
             av1_broadcast_interp_filter(
                 av1_unswitchable_filter(cm->interp_filter)));
    }
  }

  for (i = 0; i < REFERENCE_MODES; ++i) {
    if (search_state.best_pred_rd[i] == INT64_MAX)
      search_state.best_pred_diff[i] = INT_MIN;
    else
      search_state.best_pred_diff[i] =
          search_state.best_rd - search_state.best_pred_rd[i];
  }

  x->skip |= search_state.best_mode_skippable;

  assert(search_state.best_mode_index >= 0);

  store_coding_context(x, ctx, search_state.best_mode_index,
                       search_state.best_pred_diff,
                       search_state.best_mode_skippable);

  if (pmi->palette_size[1] > 0) {
    assert(try_palette);
    restore_uv_color_map(cpi, x);
  }
}

void av1_rd_pick_inter_mode_sb_seg_skip(const AV1_COMP *cpi,
                                        TileDataEnc *tile_data, MACROBLOCK *x,
                                        int mi_row, int mi_col,
                                        RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                        PICK_MODE_CONTEXT *ctx,
                                        int64_t best_rd_so_far) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  unsigned char segment_id = mbmi->segment_id;
  const int comp_pred = 0;
  int i;
  int64_t best_pred_diff[REFERENCE_MODES];
  unsigned int ref_costs_single[REF_FRAMES];
  unsigned int ref_costs_comp[REF_FRAMES][REF_FRAMES];
  int *comp_inter_cost = x->comp_inter_cost[av1_get_reference_mode_context(xd)];
  InterpFilter best_filter = SWITCHABLE;
  int64_t this_rd = INT64_MAX;
  int rate2 = 0;
  const int64_t distortion2 = 0;
  (void)mi_row;
  (void)mi_col;

  av1_collect_neighbors_ref_counts(xd);

  estimate_ref_frame_costs(cm, xd, x, segment_id, ref_costs_single,
                           ref_costs_comp);

  for (i = 0; i < REF_FRAMES; ++i) x->pred_sse[i] = INT_MAX;
  for (i = LAST_FRAME; i < REF_FRAMES; ++i) x->pred_mv_sad[i] = INT_MAX;

  rd_cost->rate = INT_MAX;

  assert(segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP));

  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  mbmi->mode = GLOBALMV;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->uv_mode = UV_DC_PRED;
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME))
    mbmi->ref_frame[0] = get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME);
  else
    mbmi->ref_frame[0] = LAST_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  mbmi->mv[0].as_int =
      gm_get_motion_vector(&cm->global_motion[mbmi->ref_frame[0]],
                           cm->allow_high_precision_mv, bsize, mi_col, mi_row,
                           cm->cur_frame_force_integer_mv)
          .as_int;
  mbmi->tx_size = max_txsize_lookup[bsize];
  x->skip = 1;

  mbmi->ref_mv_idx = 0;

  mbmi->motion_mode = SIMPLE_TRANSLATION;
  av1_count_overlappable_neighbors(cm, xd, mi_row, mi_col);
  if (is_motion_variation_allowed_bsize(bsize) && !has_second_ref(mbmi)) {
    int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
    mbmi->num_proj_ref[0] = findSamples(cm, xd, mi_row, mi_col, pts, pts_inref);
    // Select the samples according to motion vector difference
    if (mbmi->num_proj_ref[0] > 1)
      mbmi->num_proj_ref[0] = selectSamples(&mbmi->mv[0].as_mv, pts, pts_inref,
                                            mbmi->num_proj_ref[0], bsize);
  }

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
        mbmi->interp_filters = av1_broadcast_interp_filter(i);
        rs = av1_get_switchable_rate(cm, x, xd);
        if (rs < best_rs) {
          best_rs = rs;
          best_filter = av1_extract_interp_filter(mbmi->interp_filters, 0);
        }
      }
    }
  }
  // Set the appropriate filter
  mbmi->interp_filters = av1_broadcast_interp_filter(best_filter);
  rate2 += av1_get_switchable_rate(cm, x, xd);

  if (cm->reference_mode == REFERENCE_MODE_SELECT)
    rate2 += comp_inter_cost[comp_pred];

  // Estimate the reference frame signaling cost and add it
  // to the rolling cost variable.
  rate2 += ref_costs_single[LAST_FRAME];
  this_rd = RDCOST(x->rdmult, rate2, distortion2);

  rd_cost->rate = rate2;
  rd_cost->dist = distortion2;
  rd_cost->rdcost = this_rd;

  if (this_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter ==
          av1_extract_interp_filter(mbmi->interp_filters, 0)));

  av1_update_rd_thresh_fact(cm, tile_data->thresh_freq_fact,
                            cpi->sf.adaptive_rd_thresh, bsize, THR_GLOBALMV);

  av1_zero(best_pred_diff);

  store_coding_context(x, ctx, THR_GLOBALMV, best_pred_diff, 0);
}

struct calc_target_weighted_pred_ctxt {
  const MACROBLOCK *x;
  const uint8_t *tmp;
  int tmp_stride;
  int overlap;
};

static INLINE void calc_target_weighted_pred_above(
    MACROBLOCKD *xd, int rel_mi_col, uint8_t nb_mi_width, MB_MODE_INFO *nb_mi,
    void *fun_ctxt, const int num_planes) {
  (void)nb_mi;
  (void)num_planes;

  struct calc_target_weighted_pred_ctxt *ctxt =
      (struct calc_target_weighted_pred_ctxt *)fun_ctxt;

  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const uint8_t *const mask1d = av1_get_obmc_mask(ctxt->overlap);

  int32_t *wsrc = ctxt->x->wsrc_buf + (rel_mi_col * MI_SIZE);
  int32_t *mask = ctxt->x->mask_buf + (rel_mi_col * MI_SIZE);
  const uint8_t *tmp = ctxt->tmp + rel_mi_col * MI_SIZE;
  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;

  if (!is_hbd) {
    for (int row = 0; row < ctxt->overlap; ++row) {
      const uint8_t m0 = mask1d[row];
      const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
      for (int col = 0; col < nb_mi_width * MI_SIZE; ++col) {
        wsrc[col] = m1 * tmp[col];
        mask[col] = m0;
      }
      wsrc += bw;
      mask += bw;
      tmp += ctxt->tmp_stride;
    }
  } else {
    const uint16_t *tmp16 = CONVERT_TO_SHORTPTR(tmp);

    for (int row = 0; row < ctxt->overlap; ++row) {
      const uint8_t m0 = mask1d[row];
      const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
      for (int col = 0; col < nb_mi_width * MI_SIZE; ++col) {
        wsrc[col] = m1 * tmp16[col];
        mask[col] = m0;
      }
      wsrc += bw;
      mask += bw;
      tmp16 += ctxt->tmp_stride;
    }
  }
}

static INLINE void calc_target_weighted_pred_left(
    MACROBLOCKD *xd, int rel_mi_row, uint8_t nb_mi_height, MB_MODE_INFO *nb_mi,
    void *fun_ctxt, const int num_planes) {
  (void)nb_mi;
  (void)num_planes;

  struct calc_target_weighted_pred_ctxt *ctxt =
      (struct calc_target_weighted_pred_ctxt *)fun_ctxt;

  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const uint8_t *const mask1d = av1_get_obmc_mask(ctxt->overlap);

  int32_t *wsrc = ctxt->x->wsrc_buf + (rel_mi_row * MI_SIZE * bw);
  int32_t *mask = ctxt->x->mask_buf + (rel_mi_row * MI_SIZE * bw);
  const uint8_t *tmp = ctxt->tmp + (rel_mi_row * MI_SIZE * ctxt->tmp_stride);
  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;

  if (!is_hbd) {
    for (int row = 0; row < nb_mi_height * MI_SIZE; ++row) {
      for (int col = 0; col < ctxt->overlap; ++col) {
        const uint8_t m0 = mask1d[col];
        const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
        wsrc[col] = (wsrc[col] >> AOM_BLEND_A64_ROUND_BITS) * m0 +
                    (tmp[col] << AOM_BLEND_A64_ROUND_BITS) * m1;
        mask[col] = (mask[col] >> AOM_BLEND_A64_ROUND_BITS) * m0;
      }
      wsrc += bw;
      mask += bw;
      tmp += ctxt->tmp_stride;
    }
  } else {
    const uint16_t *tmp16 = CONVERT_TO_SHORTPTR(tmp);

    for (int row = 0; row < nb_mi_height * MI_SIZE; ++row) {
      for (int col = 0; col < ctxt->overlap; ++col) {
        const uint8_t m0 = mask1d[col];
        const uint8_t m1 = AOM_BLEND_A64_MAX_ALPHA - m0;
        wsrc[col] = (wsrc[col] >> AOM_BLEND_A64_ROUND_BITS) * m0 +
                    (tmp16[col] << AOM_BLEND_A64_ROUND_BITS) * m1;
        mask[col] = (mask[col] >> AOM_BLEND_A64_ROUND_BITS) * m0;
      }
      wsrc += bw;
      mask += bw;
      tmp16 += ctxt->tmp_stride;
    }
  }
}

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
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const int bh = xd->n8_h << MI_SIZE_LOG2;
  int32_t *mask_buf = x->mask_buf;
  int32_t *wsrc_buf = x->wsrc_buf;

  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
  const int src_scale = AOM_BLEND_A64_MAX_ALPHA * AOM_BLEND_A64_MAX_ALPHA;

  // plane 0 should not be subsampled
  assert(xd->plane[0].subsampling_x == 0);
  assert(xd->plane[0].subsampling_y == 0);

  av1_zero_array(wsrc_buf, bw * bh);
  for (int i = 0; i < bw * bh; ++i) mask_buf[i] = AOM_BLEND_A64_MAX_ALPHA;

  // handle above row
  if (xd->up_available) {
    const int overlap =
        AOMMIN(block_size_high[bsize], block_size_high[BLOCK_64X64]) >> 1;
    struct calc_target_weighted_pred_ctxt ctxt = { x, above, above_stride,
                                                   overlap };
    foreach_overlappable_nb_above(cm, (MACROBLOCKD *)xd, mi_col,
                                  max_neighbor_obmc[mi_size_wide_log2[bsize]],
                                  calc_target_weighted_pred_above, &ctxt);
  }

  for (int i = 0; i < bw * bh; ++i) {
    wsrc_buf[i] *= AOM_BLEND_A64_MAX_ALPHA;
    mask_buf[i] *= AOM_BLEND_A64_MAX_ALPHA;
  }

  // handle left column
  if (xd->left_available) {
    const int overlap =
        AOMMIN(block_size_wide[bsize], block_size_wide[BLOCK_64X64]) >> 1;
    struct calc_target_weighted_pred_ctxt ctxt = { x, left, left_stride,
                                                   overlap };
    foreach_overlappable_nb_left(cm, (MACROBLOCKD *)xd, mi_row,
                                 max_neighbor_obmc[mi_size_high_log2[bsize]],
                                 calc_target_weighted_pred_left, &ctxt);
  }

  if (!is_hbd) {
    const uint8_t *src = x->plane[0].src.buf;

    for (int row = 0; row < bh; ++row) {
      for (int col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += bw;
      src += x->plane[0].src.stride;
    }
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(x->plane[0].src.buf);

    for (int row = 0; row < bh; ++row) {
      for (int col = 0; col < bw; ++col) {
        wsrc_buf[col] = src[col] * src_scale - wsrc_buf[col];
      }
      wsrc_buf += bw;
      src += x->plane[0].src.stride;
    }
  }
}
