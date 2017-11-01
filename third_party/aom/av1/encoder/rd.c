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
#include <stdio.h>

#include "./av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/bitops.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/tokenize.h"

#define RD_THRESH_POW 1.25

// Factor to weigh the rate for switchable interp filters.
#define SWITCHABLE_INTERP_RATE_FACTOR 1

// The baseline rd thresholds for breaking out of the rd loop for
// certain modes are assumed to be based on 8x8 blocks.
// This table is used to correct for block size.
// The factors here are << 2 (2 = x0.5, 32 = x8 etc).
static const uint8_t rd_thresh_block_size_factor[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  2,  2,  2,
#endif
  2,  3,  3,  4, 6,  6,  8, 12, 12, 16, 24, 24, 32,
#if CONFIG_EXT_PARTITION
  48, 48, 64,
#endif  // CONFIG_EXT_PARTITION
  4,  4,  8,  8, 16, 16,
#if CONFIG_EXT_PARTITION
  32, 32
#endif  // CONFIG_EXT_PARTITION
};

#if CONFIG_EXT_TX
static const int use_intra_ext_tx_for_txsize[EXT_TX_SETS_INTRA][EXT_TX_SIZES] =
    {
#if CONFIG_CHROMA_2X2
      { 1, 1, 1, 1, 1 },  // unused
      { 0, 1, 1, 0, 0 },
      { 0, 0, 0, 1, 0 },
#if CONFIG_MRC_TX
      { 0, 0, 0, 0, 1 },
#endif  // CONFIG_MRC_TX
#else   // CONFIG_CHROMA_2X2
      { 1, 1, 1, 1 },  // unused
      { 1, 1, 0, 0 },
      { 0, 0, 1, 0 },
#if CONFIG_MRC_TX
      { 0, 0, 0, 1 },
#endif  // CONFIG_MRC_TX
#endif  // CONFIG_CHROMA_2X2
    };

static const int use_inter_ext_tx_for_txsize[EXT_TX_SETS_INTER][EXT_TX_SIZES] =
    {
#if CONFIG_CHROMA_2X2
      { 1, 1, 1, 1, 1 },  // unused
      { 0, 1, 1, 0, 0 }, { 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 1 },
#if CONFIG_MRC_TX
      { 0, 0, 0, 0, 1 },
#endif  // CONFIG_MRC_TX
#else   // CONFIG_CHROMA_2X2
      { 1, 1, 1, 1 },  // unused
      { 1, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 },
#if CONFIG_MRC_TX
      { 0, 0, 0, 1 },
#endif  // CONFIG_MRC_TX
#endif  // CONFIG_CHROMA_2X2
    };
#endif  // CONFIG_EXT_TX

void av1_fill_mode_rates(AV1_COMMON *const cm, MACROBLOCK *x,
                         FRAME_CONTEXT *fc) {
  int i, j;

  if (cm->frame_type == KEY_FRAME) {
    for (i = 0; i < PARTITION_CONTEXTS_PRIMARY; ++i)
      av1_cost_tokens_from_cdf(x->partition_cost[i], fc->partition_cdf[i],
                               NULL);
#if CONFIG_UNPOISON_PARTITION_CTX
    for (; i < PARTITION_CONTEXTS_PRIMARY + PARTITION_BLOCK_SIZES; ++i) {
      aom_prob p = fc->partition_prob[i][PARTITION_VERT];
      assert(p > 0);
      x->partition_cost[i][PARTITION_NONE] = INT_MAX;
      x->partition_cost[i][PARTITION_HORZ] = INT_MAX;
      x->partition_cost[i][PARTITION_VERT] = av1_cost_bit(p, 0);
      x->partition_cost[i][PARTITION_SPLIT] = av1_cost_bit(p, 1);
    }
    for (; i < PARTITION_CONTEXTS_PRIMARY + 2 * PARTITION_BLOCK_SIZES; ++i) {
      aom_prob p = fc->partition_prob[i][PARTITION_HORZ];
      assert(p > 0);
      x->partition_cost[i][PARTITION_NONE] = INT_MAX;
      x->partition_cost[i][PARTITION_HORZ] = av1_cost_bit(p, 0);
      x->partition_cost[i][PARTITION_VERT] = INT_MAX;
      x->partition_cost[i][PARTITION_SPLIT] = av1_cost_bit(p, 1);
    }
    x->partition_cost[PARTITION_CONTEXTS][PARTITION_NONE] = INT_MAX;
    x->partition_cost[PARTITION_CONTEXTS][PARTITION_HORZ] = INT_MAX;
    x->partition_cost[PARTITION_CONTEXTS][PARTITION_VERT] = INT_MAX;
    x->partition_cost[PARTITION_CONTEXTS][PARTITION_SPLIT] = 0;
#endif  // CONFIG_UNPOISON_PARTITION_CTX
  }

#if CONFIG_KF_CTX
  for (i = 0; i < KF_MODE_CONTEXTS; ++i)
    for (j = 0; j < KF_MODE_CONTEXTS; ++j)
      av1_cost_tokens_from_cdf(x->y_mode_costs[i][j], fc->kf_y_cdf[i][j], NULL);
#else
  for (i = 0; i < INTRA_MODES; ++i)
    for (j = 0; j < INTRA_MODES; ++j)
      av1_cost_tokens_from_cdf(x->y_mode_costs[i][j], fc->kf_y_cdf[i][j], NULL);
#endif

  for (i = 0; i < BLOCK_SIZE_GROUPS; ++i)
    av1_cost_tokens_from_cdf(x->mbmode_cost[i], fc->y_mode_cdf[i], NULL);
  for (i = 0; i < INTRA_MODES; ++i)
    av1_cost_tokens_from_cdf(x->intra_uv_mode_cost[i], fc->uv_mode_cdf[i],
                             NULL);

  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
    av1_cost_tokens_from_cdf(x->switchable_interp_costs[i],
                             fc->switchable_interp_cdf[i], NULL);

  for (i = 0; i < PALETTE_BLOCK_SIZES; ++i) {
    av1_cost_tokens_from_cdf(x->palette_y_size_cost[i],
                             fc->palette_y_size_cdf[i], NULL);
    av1_cost_tokens_from_cdf(x->palette_uv_size_cost[i],
                             fc->palette_uv_size_cdf[i], NULL);
  }

  for (i = 0; i < PALETTE_SIZES; ++i) {
    for (j = 0; j < PALETTE_COLOR_INDEX_CONTEXTS; ++j) {
      av1_cost_tokens_from_cdf(x->palette_y_color_cost[i][j],
                               fc->palette_y_color_index_cdf[i][j], NULL);
      av1_cost_tokens_from_cdf(x->palette_uv_color_cost[i][j],
                               fc->palette_uv_color_index_cdf[i][j], NULL);
    }
  }
#if CONFIG_MRC_TX
  for (i = 0; i < PALETTE_SIZES; ++i) {
    for (j = 0; j < PALETTE_COLOR_INDEX_CONTEXTS; ++j) {
      av1_cost_tokens_from_cdf(x->mrc_mask_inter_cost[i][j],
                               fc->mrc_mask_inter_cdf[i][j], NULL);
      av1_cost_tokens_from_cdf(x->mrc_mask_intra_cost[i][j],
                               fc->mrc_mask_intra_cdf[i][j], NULL);
    }
  }
#endif  // CONFIG_MRC_TX

#if CONFIG_CFL
  int sign_cost[CFL_JOINT_SIGNS];
  av1_cost_tokens_from_cdf(sign_cost, fc->cfl_sign_cdf, NULL);
  for (int joint_sign = 0; joint_sign < CFL_JOINT_SIGNS; joint_sign++) {
    const aom_cdf_prob *cdf_u = fc->cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];
    const aom_cdf_prob *cdf_v = fc->cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];
    int *cost_u = x->cfl_cost[joint_sign][CFL_PRED_U];
    int *cost_v = x->cfl_cost[joint_sign][CFL_PRED_V];
    if (CFL_SIGN_U(joint_sign) == CFL_SIGN_ZERO)
      memset(cost_u, 0, CFL_ALPHABET_SIZE * sizeof(*cost_u));
    else
      av1_cost_tokens_from_cdf(cost_u, cdf_u, NULL);
    if (CFL_SIGN_V(joint_sign) == CFL_SIGN_ZERO)
      memset(cost_v, 0, CFL_ALPHABET_SIZE * sizeof(*cost_v));
    else
      av1_cost_tokens_from_cdf(cost_v, cdf_v, NULL);
    for (int u = 0; u < CFL_ALPHABET_SIZE; u++)
      cost_u[u] += sign_cost[joint_sign];
  }
#endif  // CONFIG_CFL

  for (i = 0; i < MAX_TX_DEPTH; ++i)
    for (j = 0; j < TX_SIZE_CONTEXTS; ++j)
      av1_cost_tokens_from_cdf(x->tx_size_cost[i][j], fc->tx_size_cdf[i][j],
                               NULL);

#if CONFIG_EXT_TX
#if CONFIG_LGT_FROM_PRED
  if (LGT_FROM_PRED_INTRA) {
    for (i = 0; i < LGT_SIZES; ++i) {
      for (j = 0; j < INTRA_MODES; ++j) {
        x->intra_lgt_cost[i][j][0] = av1_cost_bit(fc->intra_lgt_prob[i][j], 0);
        x->intra_lgt_cost[i][j][1] = av1_cost_bit(fc->intra_lgt_prob[i][j], 1);
      }
    }
  }
  if (LGT_FROM_PRED_INTER) {
    for (i = 0; i < LGT_SIZES; ++i) {
      x->inter_lgt_cost[i][0] = av1_cost_bit(fc->inter_lgt_prob[i], 0);
      x->inter_lgt_cost[i][1] = av1_cost_bit(fc->inter_lgt_prob[i], 1);
    }
  }
#endif  // CONFIG_LGT_FROM_PRED
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    int s;
    for (s = 1; s < EXT_TX_SETS_INTER; ++s) {
      if (use_inter_ext_tx_for_txsize[s][i]) {
        av1_cost_tokens_from_cdf(
            x->inter_tx_type_costs[s][i], fc->inter_ext_tx_cdf[s][i],
            av1_ext_tx_inv[av1_ext_tx_set_idx_to_type[1][s]]);
      }
    }
    for (s = 1; s < EXT_TX_SETS_INTRA; ++s) {
      if (use_intra_ext_tx_for_txsize[s][i]) {
        for (j = 0; j < INTRA_MODES; ++j) {
          av1_cost_tokens_from_cdf(
              x->intra_tx_type_costs[s][i][j], fc->intra_ext_tx_cdf[s][i][j],
              av1_ext_tx_inv[av1_ext_tx_set_idx_to_type[0][s]]);
        }
      }
    }
  }
#else
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    for (j = 0; j < TX_TYPES; ++j)
      av1_cost_tokens_from_cdf(x->intra_tx_type_costs[i][j],
                               fc->intra_ext_tx_cdf[i][j], av1_ext_tx_inv);
  }
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    av1_cost_tokens_from_cdf(x->inter_tx_type_costs[i], fc->inter_ext_tx_cdf[i],
                             av1_ext_tx_inv);
  }
#endif  // CONFIG_EXT_TX
#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
  for (i = 0; i < INTRA_FILTERS + 1; ++i)
    av1_cost_tokens_from_cdf(x->intra_filter_cost[i], fc->intra_filter_cdf[i],
                             NULL);
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_LOOP_RESTORATION
  av1_cost_tokens(x->switchable_restore_cost, fc->switchable_restore_prob,
                  av1_switchable_restore_tree);
#endif  // CONFIG_LOOP_RESTORATION
#if CONFIG_INTRABC
  av1_cost_tokens_from_cdf(x->intrabc_cost, fc->intrabc_cdf, NULL);
#endif  // CONFIG_INTRABC

  if (!frame_is_intra_only(cm)) {
    for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i) {
#if CONFIG_NEW_MULTISYMBOL
      av1_cost_tokens_from_cdf(x->newmv_mode_cost[i], fc->newmv_cdf[i], NULL);
#else
      x->newmv_mode_cost[i][0] = av1_cost_bit(fc->newmv_prob[i], 0);
      x->newmv_mode_cost[i][1] = av1_cost_bit(fc->newmv_prob[i], 1);
#endif
    }

    for (i = 0; i < ZEROMV_MODE_CONTEXTS; ++i) {
#if CONFIG_NEW_MULTISYMBOL
      av1_cost_tokens_from_cdf(x->zeromv_mode_cost[i], fc->zeromv_cdf[i], NULL);
#else
      x->zeromv_mode_cost[i][0] = av1_cost_bit(fc->zeromv_prob[i], 0);
      x->zeromv_mode_cost[i][1] = av1_cost_bit(fc->zeromv_prob[i], 1);
#endif
    }

    for (i = 0; i < REFMV_MODE_CONTEXTS; ++i) {
#if CONFIG_NEW_MULTISYMBOL
      av1_cost_tokens_from_cdf(x->refmv_mode_cost[i], fc->refmv_cdf[i], NULL);
#else
      x->refmv_mode_cost[i][0] = av1_cost_bit(fc->refmv_prob[i], 0);
      x->refmv_mode_cost[i][1] = av1_cost_bit(fc->refmv_prob[i], 1);
#endif
    }

    for (i = 0; i < DRL_MODE_CONTEXTS; ++i) {
#if CONFIG_NEW_MULTISYMBOL
      av1_cost_tokens_from_cdf(x->drl_mode_cost0[i], fc->drl_cdf[i], NULL);
#else
      x->drl_mode_cost0[i][0] = av1_cost_bit(fc->drl_prob[i], 0);
      x->drl_mode_cost0[i][1] = av1_cost_bit(fc->drl_prob[i], 1);
#endif
    }
    for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
      av1_cost_tokens_from_cdf(x->inter_compound_mode_cost[i],
                               fc->inter_compound_mode_cdf[i], NULL);
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
    for (i = 0; i < BLOCK_SIZES_ALL; ++i)
      av1_cost_tokens_from_cdf(x->compound_type_cost[i],
                               fc->compound_type_cdf[i], NULL);
#endif  // CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
#if CONFIG_COMPOUND_SINGLEREF
    for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
      av1_cost_tokens_from_cdf(x->inter_singleref_comp_mode_cost[i],
                               fc->inter_singleref_comp_mode_cdf[i], NULL);
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_INTERINTRA
    for (i = 0; i < BLOCK_SIZE_GROUPS; ++i)
      av1_cost_tokens_from_cdf(x->interintra_mode_cost[i],
                               fc->interintra_mode_cdf[i], NULL);
#endif  // CONFIG_INTERINTRA
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
    for (i = BLOCK_8X8; i < BLOCK_SIZES_ALL; i++) {
      av1_cost_tokens_from_cdf(x->motion_mode_cost[i], fc->motion_mode_cdf[i],
                               NULL);
    }
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
    for (i = BLOCK_8X8; i < BLOCK_SIZES_ALL; i++) {
#if CONFIG_NCOBMC_ADAPT_WEIGHT
      av1_cost_tokens_from_cdf(x->motion_mode_cost2[i], fc->ncobmc_cdf[i],
                               NULL);
#endif
#if CONFIG_NEW_MULTISYMBOL || CONFIG_NCOBMC_ADAPT_WEIGHT
      av1_cost_tokens_from_cdf(x->motion_mode_cost1[i], fc->obmc_cdf[i], NULL);
#else
      x->motion_mode_cost1[i][0] = av1_cost_bit(fc->obmc_prob[i], 0);
      x->motion_mode_cost1[i][1] = av1_cost_bit(fc->obmc_prob[i], 1);
#endif
    }
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#if CONFIG_MOTION_VAR && CONFIG_NCOBMC_ADAPT_WEIGHT
    for (i = ADAPT_OVERLAP_BLOCK_8X8; i < ADAPT_OVERLAP_BLOCKS; ++i) {
      av1_cost_tokens_from_cdf(x->ncobmc_mode_cost[i], fc->ncobmc_mode_cdf[i],
                               NULL);
    }
#endif  // CONFIG_MOTION_VAR && CONFIG_NCOBMC_ADAPT_WEIGHT
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  }
}

// Values are now correlated to quantizer.
static int sad_per_bit16lut_8[QINDEX_RANGE];
static int sad_per_bit4lut_8[QINDEX_RANGE];

#if CONFIG_HIGHBITDEPTH
static int sad_per_bit16lut_10[QINDEX_RANGE];
static int sad_per_bit4lut_10[QINDEX_RANGE];
static int sad_per_bit16lut_12[QINDEX_RANGE];
static int sad_per_bit4lut_12[QINDEX_RANGE];
#endif

static void init_me_luts_bd(int *bit16lut, int *bit4lut, int range,
                            aom_bit_depth_t bit_depth) {
  int i;
  // Initialize the sad lut tables using a formulaic calculation for now.
  // This is to make it easier to resolve the impact of experimental changes
  // to the quantizer tables.
  for (i = 0; i < range; i++) {
    const double q = av1_convert_qindex_to_q(i, bit_depth);
    bit16lut[i] = (int)(0.0418 * q + 2.4107);
    bit4lut[i] = (int)(0.063 * q + 2.742);
  }
}

void av1_init_me_luts(void) {
  init_me_luts_bd(sad_per_bit16lut_8, sad_per_bit4lut_8, QINDEX_RANGE,
                  AOM_BITS_8);
#if CONFIG_HIGHBITDEPTH
  init_me_luts_bd(sad_per_bit16lut_10, sad_per_bit4lut_10, QINDEX_RANGE,
                  AOM_BITS_10);
  init_me_luts_bd(sad_per_bit16lut_12, sad_per_bit4lut_12, QINDEX_RANGE,
                  AOM_BITS_12);
#endif
}

static const int rd_boost_factor[16] = { 64, 32, 32, 32, 24, 16, 12, 12,
                                         8,  8,  4,  4,  2,  2,  1,  0 };
static const int rd_frame_type_factor[FRAME_UPDATE_TYPES] = {
  128, 144, 128, 128, 144,
#if CONFIG_EXT_REFS
  // TODO(zoeliu): To adjust further following factor values.
  128, 128, 128,
  // TODO(weitinglin): We should investigate if the values should be the same
  //                   as the value used by OVERLAY frame
  144,  // INTNL_OVERLAY_UPDATE
  128   // INTNL_ARF_UPDATE
#endif  // CONFIG_EXT_REFS
};

int av1_compute_rd_mult(const AV1_COMP *cpi, int qindex) {
  const int64_t q = av1_dc_quant(qindex, 0, cpi->common.bit_depth);
#if CONFIG_HIGHBITDEPTH
  int64_t rdmult = 0;
  switch (cpi->common.bit_depth) {
    case AOM_BITS_8: rdmult = 88 * q * q / 24; break;
    case AOM_BITS_10: rdmult = ROUND_POWER_OF_TWO(88 * q * q / 24, 4); break;
    case AOM_BITS_12: rdmult = ROUND_POWER_OF_TWO(88 * q * q / 24, 8); break;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
#else
  int64_t rdmult = 88 * q * q / 24;
#endif  // CONFIG_HIGHBITDEPTH
  if (cpi->oxcf.pass == 2 && (cpi->common.frame_type != KEY_FRAME)) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    const FRAME_UPDATE_TYPE frame_type = gf_group->update_type[gf_group->index];
    const int boost_index = AOMMIN(15, (cpi->rc.gfu_boost / 100));

    rdmult = (rdmult * rd_frame_type_factor[frame_type]) >> 7;
    rdmult += ((rdmult * rd_boost_factor[boost_index]) >> 7);
  }
  if (rdmult < 1) rdmult = 1;
  return (int)rdmult;
}

static int compute_rd_thresh_factor(int qindex, aom_bit_depth_t bit_depth) {
  double q;
#if CONFIG_HIGHBITDEPTH
  switch (bit_depth) {
    case AOM_BITS_8: q = av1_dc_quant(qindex, 0, AOM_BITS_8) / 4.0; break;
    case AOM_BITS_10: q = av1_dc_quant(qindex, 0, AOM_BITS_10) / 16.0; break;
    case AOM_BITS_12: q = av1_dc_quant(qindex, 0, AOM_BITS_12) / 64.0; break;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
#else
  (void)bit_depth;
  q = av1_dc_quant(qindex, 0, AOM_BITS_8) / 4.0;
#endif  // CONFIG_HIGHBITDEPTH
  // TODO(debargha): Adjust the function below.
  return AOMMAX((int)(pow(q, RD_THRESH_POW) * 5.12), 8);
}

void av1_initialize_me_consts(const AV1_COMP *cpi, MACROBLOCK *x, int qindex) {
#if CONFIG_HIGHBITDEPTH
  switch (cpi->common.bit_depth) {
    case AOM_BITS_8:
      x->sadperbit16 = sad_per_bit16lut_8[qindex];
      x->sadperbit4 = sad_per_bit4lut_8[qindex];
      break;
    case AOM_BITS_10:
      x->sadperbit16 = sad_per_bit16lut_10[qindex];
      x->sadperbit4 = sad_per_bit4lut_10[qindex];
      break;
    case AOM_BITS_12:
      x->sadperbit16 = sad_per_bit16lut_12[qindex];
      x->sadperbit4 = sad_per_bit4lut_12[qindex];
      break;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
  }
#else
  (void)cpi;
  x->sadperbit16 = sad_per_bit16lut_8[qindex];
  x->sadperbit4 = sad_per_bit4lut_8[qindex];
#endif  // CONFIG_HIGHBITDEPTH
}

static void set_block_thresholds(const AV1_COMMON *cm, RD_OPT *rd) {
  int i, bsize, segment_id;

  for (segment_id = 0; segment_id < MAX_SEGMENTS; ++segment_id) {
    const int qindex =
        clamp(av1_get_qindex(&cm->seg, segment_id, cm->base_qindex) +
                  cm->y_dc_delta_q,
              0, MAXQ);
    const int q = compute_rd_thresh_factor(qindex, cm->bit_depth);

    for (bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
      // Threshold here seems unnecessarily harsh but fine given actual
      // range of values used for cpi->sf.thresh_mult[].
      const int t = q * rd_thresh_block_size_factor[bsize];
      const int thresh_max = INT_MAX / t;

#if CONFIG_CB4X4
      for (i = 0; i < MAX_MODES; ++i)
        rd->threshes[segment_id][bsize][i] = rd->thresh_mult[i] < thresh_max
                                                 ? rd->thresh_mult[i] * t / 4
                                                 : INT_MAX;
#else
      if (bsize >= BLOCK_8X8) {
        for (i = 0; i < MAX_MODES; ++i)
          rd->threshes[segment_id][bsize][i] = rd->thresh_mult[i] < thresh_max
                                                   ? rd->thresh_mult[i] * t / 4
                                                   : INT_MAX;
      } else {
        for (i = 0; i < MAX_REFS; ++i)
          rd->threshes[segment_id][bsize][i] =
              rd->thresh_mult_sub8x8[i] < thresh_max
                  ? rd->thresh_mult_sub8x8[i] * t / 4
                  : INT_MAX;
      }
#endif
    }
  }
}

void av1_set_mvcost(MACROBLOCK *x, MV_REFERENCE_FRAME ref_frame, int ref,
                    int ref_mv_idx) {
  MB_MODE_INFO_EXT *mbmi_ext = x->mbmi_ext;
  int8_t rf_type = av1_ref_frame_type(x->e_mbd.mi[0]->mbmi.ref_frame);
  int nmv_ctx = av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                            mbmi_ext->ref_mv_stack[rf_type], ref, ref_mv_idx);
  (void)ref_frame;
  x->mvcost = x->mv_cost_stack[nmv_ctx];
  x->nmvjointcost = x->nmv_vec_cost[nmv_ctx];
}

#if CONFIG_LV_MAP
#if !LV_MAP_PROB
static void get_rate_cost(aom_prob p, int cost[2]) {
  cost[0] = av1_cost_bit(p, 0);
  cost[1] = av1_cost_bit(p, 1);
}
#endif  // !LV_MAP_PROB

void av1_fill_coeff_costs(MACROBLOCK *x, FRAME_CONTEXT *fc) {
  for (int tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (int plane = 0; plane < PLANE_TYPES; ++plane) {
      LV_MAP_COEFF_COST *pcost = &x->coeff_costs[tx_size][plane];

#if LV_MAP_PROB
      for (int ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->txb_skip_cost[ctx],
                                 fc->txb_skip_cdf[tx_size][ctx], NULL);

      for (int ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->nz_map_cost[ctx],
                                 fc->nz_map_cdf[tx_size][plane][ctx], NULL);

      for (int ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->eob_cost[ctx],
                                 fc->eob_flag_cdf[tx_size][plane][ctx], NULL);

      for (int ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->dc_sign_cost[ctx],
                                 fc->dc_sign_cdf[plane][ctx], NULL);

      for (int layer = 0; layer < NUM_BASE_LEVELS; ++layer)
        for (int ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx)
          av1_cost_tokens_from_cdf(
              pcost->base_cost[layer][ctx],
              fc->coeff_base_cdf[tx_size][plane][layer][ctx], NULL);

#if BR_NODE
      for (int br = 0; br < BASE_RANGE_SETS; ++br)
        for (int ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx)
          av1_cost_tokens_from_cdf(pcost->br_cost[br][ctx],
                                   fc->coeff_br_cdf[tx_size][plane][br][ctx],
                                   NULL);

      for (int ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
        int lps_rate[2];
        av1_cost_tokens_from_cdf(lps_rate,
                                 fc->coeff_lps_cdf[tx_size][plane][ctx], NULL);

        for (int base_range = 0; base_range < COEFF_BASE_RANGE + 1;
             ++base_range) {
          int br_set_idx = base_range < COEFF_BASE_RANGE
                               ? coeff_to_br_index[base_range]
                               : BASE_RANGE_SETS;

          pcost->lps_cost[ctx][base_range] = 0;

          for (int idx = 0; idx < BASE_RANGE_SETS; ++idx) {
            if (idx == br_set_idx) {
              pcost->lps_cost[ctx][base_range] += pcost->br_cost[idx][ctx][1];

              int br_base = br_index_to_coeff[br_set_idx];
              int br_offset = base_range - br_base;
              int extra_bits = (1 << br_extra_bits[idx]) - 1;
              for (int tok = 0; tok < extra_bits; ++tok) {
                if (tok == br_offset) {
                  pcost->lps_cost[ctx][base_range] += lps_rate[1];
                  break;
                } else {
                  pcost->lps_cost[ctx][base_range] += lps_rate[0];
                }
              }
              break;
            } else {
              pcost->lps_cost[ctx][base_range] += pcost->br_cost[idx][ctx][0];
            }
          }
          // load the base range cost
        }
      }
#else   // BR_NODE
      for (int ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->lps_cost[ctx],
                                 fc->coeff_lps_cdf[tx_size][plane][ctx], NULL);
#endif  // BR_NODE
#if CONFIG_CTX1D
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        av1_cost_tokens_from_cdf(pcost->eob_mode_cost[tx_class],
                                 fc->eob_mode_cdf[tx_size][plane][tx_class],
                                 NULL);

      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        for (int ctx = 0; ctx < EMPTY_LINE_CONTEXTS; ++ctx)
          av1_cost_tokens_from_cdf(
              pcost->empty_line_cost[tx_class][ctx],
              fc->empty_line_cdf[tx_size][plane][tx_class][ctx], NULL);

      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        for (int ctx = 0; ctx < HV_EOB_CONTEXTS; ++ctx)
          av1_cost_tokens_from_cdf(
              pcost->hv_eob_cost[tx_class][ctx],
              fc->hv_eob_cdf[tx_size][plane][tx_class][ctx], NULL);
#endif  // CONFIG_CTX1D
#else   // LV_MAP_PROB
      for (int ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx)
        get_rate_cost(fc->txb_skip[tx_size][ctx], pcost->txb_skip_cost[ctx]);

      for (int ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx)
        get_rate_cost(fc->nz_map[tx_size][plane][ctx], pcost->nz_map_cost[ctx]);

      for (int ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx)
        get_rate_cost(fc->eob_flag[tx_size][plane][ctx], pcost->eob_cost[ctx]);

      for (int ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
        get_rate_cost(fc->dc_sign[plane][ctx], pcost->dc_sign_cost[ctx]);

      for (int layer = 0; layer < NUM_BASE_LEVELS; ++layer)
        for (int ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx)
          get_rate_cost(fc->coeff_base[tx_size][plane][layer][ctx],
                        pcost->base_cost[layer][ctx]);

      for (int ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx)
        get_rate_cost(fc->coeff_lps[tx_size][plane][ctx], pcost->lps_cost[ctx]);

#if CONFIG_CTX1D
      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        get_rate_cost(fc->eob_mode[tx_size][plane][tx_class],
                      pcost->eob_mode_cost[tx_class]);

      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        for (int ctx = 0; ctx < EMPTY_LINE_CONTEXTS; ++ctx)
          get_rate_cost(fc->empty_line[tx_size][plane][tx_class][ctx],
                        pcost->empty_line_cost[tx_class][ctx]);

      for (int tx_class = 0; tx_class < TX_CLASSES; ++tx_class)
        for (int ctx = 0; ctx < HV_EOB_CONTEXTS; ++ctx)
          get_rate_cost(fc->hv_eob[tx_size][plane][tx_class][ctx],
                        pcost->hv_eob_cost[tx_class][ctx]);
#endif  // CONFIG_CTX1D
#endif  // LV_MAP_PROB
    }
  }
}
#endif  // CONFIG_LV_MAP

void av1_fill_token_costs_from_cdf(av1_coeff_cost *cost,
                                   coeff_cdf_model (*cdf)[PLANE_TYPES]) {
  for (int tx = 0; tx < TX_SIZES; ++tx) {
    for (int pt = 0; pt < PLANE_TYPES; ++pt) {
      for (int rt = 0; rt < REF_TYPES; ++rt) {
        for (int band = 0; band < COEF_BANDS; ++band) {
          for (int ctx = 0; ctx < BAND_COEFF_CONTEXTS(band); ++ctx) {
            av1_cost_tokens_from_cdf(cost[tx][pt][rt][band][ctx],
                                     cdf[tx][pt][rt][band][ctx], NULL);
          }
        }
      }
    }
  }
}

void av1_initialize_rd_consts(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->td.mb;
  RD_OPT *const rd = &cpi->rd;
  int nmv_ctx;

  aom_clear_system_state();

  rd->RDMULT = av1_compute_rd_mult(cpi, cm->base_qindex + cm->y_dc_delta_q);

  set_error_per_bit(x, rd->RDMULT);

  set_block_thresholds(cm, rd);

  for (nmv_ctx = 0; nmv_ctx < NMV_CONTEXTS; ++nmv_ctx) {
#if CONFIG_AMVR
    if (cm->cur_frame_mv_precision_level) {
      av1_build_nmv_cost_table(x->nmv_vec_cost[nmv_ctx], x->nmvcost[nmv_ctx],
                               &cm->fc->nmvc[nmv_ctx], MV_SUBPEL_NONE);
    } else {
      av1_build_nmv_cost_table(
          x->nmv_vec_cost[nmv_ctx],
          cm->allow_high_precision_mv ? x->nmvcost_hp[nmv_ctx]
                                      : x->nmvcost[nmv_ctx],
          &cm->fc->nmvc[nmv_ctx], cm->allow_high_precision_mv);
    }

#else
    av1_build_nmv_cost_table(
        x->nmv_vec_cost[nmv_ctx],
        cm->allow_high_precision_mv ? x->nmvcost_hp[nmv_ctx]
                                    : x->nmvcost[nmv_ctx],
        &cm->fc->nmvc[nmv_ctx], cm->allow_high_precision_mv);
#endif
  }
  x->mvcost = x->mv_cost_stack[0];
  x->nmvjointcost = x->nmv_vec_cost[0];

#if CONFIG_INTRABC
  if (frame_is_intra_only(cm) && cm->allow_screen_content_tools &&
      cpi->oxcf.pass != 1) {
    av1_build_nmv_cost_table(
        x->nmv_vec_cost[0],
        cm->allow_high_precision_mv ? x->nmvcost_hp[0] : x->nmvcost[0],
        &cm->fc->ndvc, MV_SUBPEL_NONE);
  }
#endif

#if CONFIG_GLOBAL_MOTION
  if (cpi->oxcf.pass != 1) {
    for (int i = 0; i < TRANS_TYPES; ++i)
#if GLOBAL_TRANS_TYPES > 4
      cpi->gmtype_cost[i] = (1 + (i > 0 ? GLOBAL_TYPE_BITS : 0))
                            << AV1_PROB_COST_SHIFT;
#else
      // IDENTITY: 1 bit
      // TRANSLATION: 3 bits
      // ROTZOOM: 2 bits
      // AFFINE: 3 bits
      cpi->gmtype_cost[i] = (1 + (i > 0 ? (i == ROTZOOM ? 1 : 2) : 0))
                            << AV1_PROB_COST_SHIFT;
#endif  // GLOBAL_TRANS_TYPES > 4
  }
#endif  // CONFIG_GLOBAL_MOTION
}

static void model_rd_norm(int xsq_q10, int *r_q10, int *d_q10) {
  // NOTE: The tables below must be of the same size.

  // The functions described below are sampled at the four most significant
  // bits of x^2 + 8 / 256.

  // Normalized rate:
  // This table models the rate for a Laplacian source with given variance
  // when quantized with a uniform quantizer with given stepsize. The
  // closed form expression is:
  // Rn(x) = H(sqrt(r)) + sqrt(r)*[1 + H(r)/(1 - r)],
  // where r = exp(-sqrt(2) * x) and x = qpstep / sqrt(variance),
  // and H(x) is the binary entropy function.
  static const int rate_tab_q10[] = {
    65536, 6086, 5574, 5275, 5063, 4899, 4764, 4651, 4553, 4389, 4255, 4142,
    4044,  3958, 3881, 3811, 3748, 3635, 3538, 3453, 3376, 3307, 3244, 3186,
    3133,  3037, 2952, 2877, 2809, 2747, 2690, 2638, 2589, 2501, 2423, 2353,
    2290,  2232, 2179, 2130, 2084, 2001, 1928, 1862, 1802, 1748, 1698, 1651,
    1608,  1530, 1460, 1398, 1342, 1290, 1243, 1199, 1159, 1086, 1021, 963,
    911,   864,  821,  781,  745,  680,  623,  574,  530,  490,  455,  424,
    395,   345,  304,  269,  239,  213,  190,  171,  154,  126,  104,  87,
    73,    61,   52,   44,   38,   28,   21,   16,   12,   10,   8,    6,
    5,     3,    2,    1,    1,    1,    0,    0,
  };
  // Normalized distortion:
  // This table models the normalized distortion for a Laplacian source
  // with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expression is:
  // Dn(x) = 1 - 1/sqrt(2) * x / sinh(x/sqrt(2))
  // where x = qpstep / sqrt(variance).
  // Note the actual distortion is Dn * variance.
  static const int dist_tab_q10[] = {
    0,    0,    1,    1,    1,    2,    2,    2,    3,    3,    4,    5,
    5,    6,    7,    7,    8,    9,    11,   12,   13,   15,   16,   17,
    18,   21,   24,   26,   29,   31,   34,   36,   39,   44,   49,   54,
    59,   64,   69,   73,   78,   88,   97,   106,  115,  124,  133,  142,
    151,  167,  184,  200,  215,  231,  245,  260,  274,  301,  327,  351,
    375,  397,  418,  439,  458,  495,  528,  559,  587,  613,  637,  659,
    680,  717,  749,  777,  801,  823,  842,  859,  874,  899,  919,  936,
    949,  960,  969,  977,  983,  994,  1001, 1006, 1010, 1013, 1015, 1017,
    1018, 1020, 1022, 1022, 1023, 1023, 1023, 1024,
  };
  static const int xsq_iq_q10[] = {
    0,      4,      8,      12,     16,     20,     24,     28,     32,
    40,     48,     56,     64,     72,     80,     88,     96,     112,
    128,    144,    160,    176,    192,    208,    224,    256,    288,
    320,    352,    384,    416,    448,    480,    544,    608,    672,
    736,    800,    864,    928,    992,    1120,   1248,   1376,   1504,
    1632,   1760,   1888,   2016,   2272,   2528,   2784,   3040,   3296,
    3552,   3808,   4064,   4576,   5088,   5600,   6112,   6624,   7136,
    7648,   8160,   9184,   10208,  11232,  12256,  13280,  14304,  15328,
    16352,  18400,  20448,  22496,  24544,  26592,  28640,  30688,  32736,
    36832,  40928,  45024,  49120,  53216,  57312,  61408,  65504,  73696,
    81888,  90080,  98272,  106464, 114656, 122848, 131040, 147424, 163808,
    180192, 196576, 212960, 229344, 245728,
  };
  const int tmp = (xsq_q10 >> 2) + 8;
  const int k = get_msb(tmp) - 3;
  const int xq = (k << 3) + ((tmp >> k) & 0x7);
  const int one_q10 = 1 << 10;
  const int a_q10 = ((xsq_q10 - xsq_iq_q10[xq]) << 10) >> (2 + k);
  const int b_q10 = one_q10 - a_q10;
  *r_q10 = (rate_tab_q10[xq] * b_q10 + rate_tab_q10[xq + 1] * a_q10) >> 10;
  *d_q10 = (dist_tab_q10[xq] * b_q10 + dist_tab_q10[xq + 1] * a_q10) >> 10;
}

void av1_model_rd_from_var_lapndz(int64_t var, unsigned int n_log2,
                                  unsigned int qstep, int *rate,
                                  int64_t *dist) {
  // This function models the rate and distortion for a Laplacian
  // source with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expressions are in:
  // Hang and Chen, "Source Model for transform video coder and its
  // application - Part I: Fundamental Theory", IEEE Trans. Circ.
  // Sys. for Video Tech., April 1997.
  if (var == 0) {
    *rate = 0;
    *dist = 0;
  } else {
    int d_q10, r_q10;
    static const uint32_t MAX_XSQ_Q10 = 245727;
    const uint64_t xsq_q10_64 =
        (((uint64_t)qstep * qstep << (n_log2 + 10)) + (var >> 1)) / var;
    const int xsq_q10 = (int)AOMMIN(xsq_q10_64, MAX_XSQ_Q10);
    model_rd_norm(xsq_q10, &r_q10, &d_q10);
    *rate = ROUND_POWER_OF_TWO(r_q10 << n_log2, 10 - AV1_PROB_COST_SHIFT);
    *dist = (var * (int64_t)d_q10 + 512) >> 10;
  }
}

static void get_entropy_contexts_plane(
    BLOCK_SIZE plane_bsize, TX_SIZE tx_size, const struct macroblockd_plane *pd,
    ENTROPY_CONTEXT t_above[2 * MAX_MIB_SIZE],
    ENTROPY_CONTEXT t_left[2 * MAX_MIB_SIZE]) {
  const int num_4x4_w = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int num_4x4_h = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  const ENTROPY_CONTEXT *const above = pd->above_context;
  const ENTROPY_CONTEXT *const left = pd->left_context;

#if CONFIG_LV_MAP
  memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
  memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
  return;
#endif  // CONFIG_LV_MAP

  int i;

#if CONFIG_CHROMA_2X2
  switch (tx_size) {
    case TX_2X2:
      memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
      memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
      break;
    case TX_4X4:
      for (i = 0; i < num_4x4_w; i += 2)
        t_above[i] = !!*(const uint16_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 2)
        t_left[i] = !!*(const uint16_t *)&left[i];
      break;
    case TX_8X8:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
    case TX_16X16:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
    case TX_32X32:
      for (i = 0; i < num_4x4_w; i += 16)
        t_above[i] =
            !!(*(const uint64_t *)&above[i] | *(const uint64_t *)&above[i + 8]);
      for (i = 0; i < num_4x4_h; i += 16)
        t_left[i] =
            !!(*(const uint64_t *)&left[i] | *(const uint64_t *)&left[i + 8]);
      break;
#if CONFIG_TX64X64
    case TX_32X64:
      for (i = 0; i < num_4x4_w; i += 16)
        t_above[i] =
            !!(*(const uint64_t *)&above[i] | *(const uint64_t *)&above[i + 8]);
      for (i = 0; i < num_4x4_h; i += 32)
        t_left[i] =
            !!(*(const uint64_t *)&left[i] | *(const uint64_t *)&left[i + 8] |
               *(const uint64_t *)&left[i + 16] |
               *(const uint64_t *)&left[i + 24]);
      break;
    case TX_64X32:
      for (i = 0; i < num_4x4_w; i += 32)
        t_above[i] =
            !!(*(const uint64_t *)&above[i] | *(const uint64_t *)&above[i + 8] |
               *(const uint64_t *)&above[i + 16] |
               *(const uint64_t *)&above[i + 24]);
      for (i = 0; i < num_4x4_h; i += 16)
        t_left[i] =
            !!(*(const uint64_t *)&left[i] | *(const uint64_t *)&left[i + 8]);
      break;
    case TX_64X64:
      for (i = 0; i < num_4x4_w; i += 32)
        t_above[i] =
            !!(*(const uint64_t *)&above[i] | *(const uint64_t *)&above[i + 8] |
               *(const uint64_t *)&above[i + 16] |
               *(const uint64_t *)&above[i + 24]);
      for (i = 0; i < num_4x4_h; i += 32)
        t_left[i] =
            !!(*(const uint64_t *)&left[i] | *(const uint64_t *)&left[i + 8] |
               *(const uint64_t *)&left[i + 16] |
               *(const uint64_t *)&left[i + 24]);
      break;
#endif  // CONFIG_TX64X64
    case TX_4X8:
      for (i = 0; i < num_4x4_w; i += 2)
        t_above[i] = !!*(const uint16_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
    case TX_8X4:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 2)
        t_left[i] = !!*(const uint16_t *)&left[i];
      break;
    case TX_8X16:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
    case TX_16X8:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
    case TX_16X32:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 16)
        t_left[i] =
            !!(*(const uint64_t *)&left[i] | *(const uint64_t *)&left[i + 8]);
      break;
    case TX_32X16:
      for (i = 0; i < num_4x4_w; i += 16)
        t_above[i] =
            !!(*(const uint64_t *)&above[i] | *(const uint64_t *)&above[i + 8]);
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
    case TX_4X16:
      for (i = 0; i < num_4x4_w; i += 2)
        t_above[i] = !!*(const uint16_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
    case TX_16X4:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 2)
        t_left[i] = !!*(const uint16_t *)&left[i];
      break;
    case TX_8X32:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 16)
        t_left[i] =
            !!(*(const uint64_t *)&left[i] | *(const uint64_t *)&left[i + 8]);
      break;
    case TX_32X8:
      for (i = 0; i < num_4x4_w; i += 16)
        t_above[i] =
            !!(*(const uint64_t *)&above[i] | *(const uint64_t *)&above[i + 8]);
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
#endif

    default: assert(0 && "Invalid transform size."); break;
  }
  return;
#endif  // CONFIG_CHROMA_2X2

  switch (tx_size) {
    case TX_4X4:
      memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
      memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
      break;
    case TX_8X8:
      for (i = 0; i < num_4x4_w; i += 2)
        t_above[i] = !!*(const uint16_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 2)
        t_left[i] = !!*(const uint16_t *)&left[i];
      break;
    case TX_16X16:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
    case TX_32X32:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
#if CONFIG_TX64X64
    case TX_32X64:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 16)
        t_left[i] =
            !!(*(const uint64_t *)&left[i] | *(const uint64_t *)&left[i + 8]);
      break;
    case TX_64X32:
      for (i = 0; i < num_4x4_w; i += 16)
        t_above[i] =
            !!(*(const uint64_t *)&above[i] | *(const uint64_t *)&above[i + 8]);
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
    case TX_64X64:
      for (i = 0; i < num_4x4_w; i += 16)
        t_above[i] =
            !!(*(const uint64_t *)&above[i] | *(const uint64_t *)&above[i + 8]);
      for (i = 0; i < num_4x4_h; i += 16)
        t_left[i] =
            !!(*(const uint64_t *)&left[i] | *(const uint64_t *)&left[i + 8]);
      break;
#endif  // CONFIG_TX64X64
    case TX_4X8:
      memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
      for (i = 0; i < num_4x4_h; i += 2)
        t_left[i] = !!*(const uint16_t *)&left[i];
      break;
    case TX_8X4:
      for (i = 0; i < num_4x4_w; i += 2)
        t_above[i] = !!*(const uint16_t *)&above[i];
      memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
      break;
    case TX_8X16:
      for (i = 0; i < num_4x4_w; i += 2)
        t_above[i] = !!*(const uint16_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
    case TX_16X8:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 2)
        t_left[i] = !!*(const uint16_t *)&left[i];
      break;
    case TX_16X32:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
    case TX_32X16:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
    case TX_4X16:
      memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
      for (i = 0; i < num_4x4_h; i += 4)
        t_left[i] = !!*(const uint32_t *)&left[i];
      break;
    case TX_16X4:
      for (i = 0; i < num_4x4_w; i += 4)
        t_above[i] = !!*(const uint32_t *)&above[i];
      memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
      break;
    case TX_8X32:
      for (i = 0; i < num_4x4_w; i += 2)
        t_above[i] = !!*(const uint16_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 8)
        t_left[i] = !!*(const uint64_t *)&left[i];
      break;
    case TX_32X8:
      for (i = 0; i < num_4x4_w; i += 8)
        t_above[i] = !!*(const uint64_t *)&above[i];
      for (i = 0; i < num_4x4_h; i += 2)
        t_left[i] = !!*(const uint16_t *)&left[i];
      break;
#endif
    default: assert(0 && "Invalid transform size."); break;
  }
}

void av1_get_entropy_contexts(BLOCK_SIZE bsize, TX_SIZE tx_size,
                              const struct macroblockd_plane *pd,
                              ENTROPY_CONTEXT t_above[2 * MAX_MIB_SIZE],
                              ENTROPY_CONTEXT t_left[2 * MAX_MIB_SIZE]) {
#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#else
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#endif
  get_entropy_contexts_plane(plane_bsize, tx_size, pd, t_above, t_left);
}

void av1_mv_pred(const AV1_COMP *cpi, MACROBLOCK *x, uint8_t *ref_y_buffer,
                 int ref_y_stride, int ref_frame, BLOCK_SIZE block_size) {
  int i;
  int zero_seen = 0;
  int best_index = 0;
  int best_sad = INT_MAX;
  int this_sad = INT_MAX;
  int max_mv = 0;
  uint8_t *src_y_ptr = x->plane[0].src.buf;
  uint8_t *ref_y_ptr;
  MV pred_mv[MAX_MV_REF_CANDIDATES + 1];
  int num_mv_refs = 0;

  pred_mv[num_mv_refs++] = x->mbmi_ext->ref_mvs[ref_frame][0].as_mv;
  if (x->mbmi_ext->ref_mvs[ref_frame][0].as_int !=
      x->mbmi_ext->ref_mvs[ref_frame][1].as_int) {
    pred_mv[num_mv_refs++] = x->mbmi_ext->ref_mvs[ref_frame][1].as_mv;
  }
  if (cpi->sf.adaptive_motion_search && block_size < x->max_partition_size)
    pred_mv[num_mv_refs++] = x->pred_mv[ref_frame];

  assert(num_mv_refs <= (int)(sizeof(pred_mv) / sizeof(pred_mv[0])));

  // Get the sad for each candidate reference mv.
  for (i = 0; i < num_mv_refs; ++i) {
    const MV *this_mv = &pred_mv[i];
    int fp_row, fp_col;
    fp_row = (this_mv->row + 3 + (this_mv->row >= 0)) >> 3;
    fp_col = (this_mv->col + 3 + (this_mv->col >= 0)) >> 3;
    max_mv = AOMMAX(max_mv, AOMMAX(abs(this_mv->row), abs(this_mv->col)) >> 3);

    if (fp_row == 0 && fp_col == 0 && zero_seen) continue;
    zero_seen |= (fp_row == 0 && fp_col == 0);

    ref_y_ptr = &ref_y_buffer[ref_y_stride * fp_row + fp_col];
    // Find sad for current vector.
    this_sad = cpi->fn_ptr[block_size].sdf(src_y_ptr, x->plane[0].src.stride,
                                           ref_y_ptr, ref_y_stride);
    // Note if it is the best so far.
    if (this_sad < best_sad) {
      best_sad = this_sad;
      best_index = i;
    }
  }

  // Note the index of the mv that worked best in the reference list.
  x->mv_best_ref_index[ref_frame] = best_index;
  x->max_mv_context[ref_frame] = max_mv;
  x->pred_mv_sad[ref_frame] = best_sad;
}

void av1_setup_pred_block(const MACROBLOCKD *xd,
                          struct buf_2d dst[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *scale,
                          const struct scale_factors *scale_uv) {
  int i;

  dst[0].buf = src->y_buffer;
  dst[0].stride = src->y_stride;
  dst[1].buf = src->u_buffer;
  dst[2].buf = src->v_buffer;
  dst[1].stride = dst[2].stride = src->uv_stride;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    setup_pred_plane(dst + i, xd->mi[0]->mbmi.sb_type, dst[i].buf,
                     i ? src->uv_crop_width : src->y_crop_width,
                     i ? src->uv_crop_height : src->y_crop_height,
                     dst[i].stride, mi_row, mi_col, i ? scale_uv : scale,
                     xd->plane[i].subsampling_x, xd->plane[i].subsampling_y);
  }
}

int av1_raster_block_offset(BLOCK_SIZE plane_bsize, int raster_block,
                            int stride) {
  const int bw = b_width_log2_lookup[plane_bsize];
  const int y = 4 * (raster_block >> bw);
  const int x = 4 * (raster_block & ((1 << bw) - 1));
  return y * stride + x;
}

int16_t *av1_raster_block_offset_int16(BLOCK_SIZE plane_bsize, int raster_block,
                                       int16_t *base) {
  const int stride = block_size_wide[plane_bsize];
  return base + av1_raster_block_offset(plane_bsize, raster_block, stride);
}

YV12_BUFFER_CONFIG *av1_get_scaled_ref_frame(const AV1_COMP *cpi,
                                             int ref_frame) {
  const AV1_COMMON *const cm = &cpi->common;
  const int scaled_idx = cpi->scaled_ref_idx[ref_frame - 1];
  const int ref_idx = get_ref_frame_buf_idx(cpi, ref_frame);
  return (scaled_idx != ref_idx && scaled_idx != INVALID_IDX)
             ? &cm->buffer_pool->frame_bufs[scaled_idx].buf
             : NULL;
}

#if CONFIG_DUAL_FILTER
int av1_get_switchable_rate(const AV1_COMMON *const cm, MACROBLOCK *x,
                            const MACROBLOCKD *xd) {
  if (cm->interp_filter == SWITCHABLE) {
    const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
    int inter_filter_cost = 0;
    int dir;

    for (dir = 0; dir < 2; ++dir) {
      if (has_subpel_mv_component(xd->mi[0], xd, dir) ||
          (mbmi->ref_frame[1] > INTRA_FRAME &&
           has_subpel_mv_component(xd->mi[0], xd, dir + 2))) {
        const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
        const InterpFilter filter =
            av1_extract_interp_filter(mbmi->interp_filters, dir);
        inter_filter_cost += x->switchable_interp_costs[ctx][filter];
      }
    }
    return SWITCHABLE_INTERP_RATE_FACTOR * inter_filter_cost;
  } else {
    return 0;
  }
}
#else
int av1_get_switchable_rate(const AV1_COMMON *const cm, MACROBLOCK *x,
                            const MACROBLOCKD *xd) {
  if (cm->interp_filter == SWITCHABLE) {
    const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
    const int ctx = av1_get_pred_context_switchable_interp(xd);
    const InterpFilter filter =
        av1_extract_interp_filter(mbmi->interp_filters, 0);
    return SWITCHABLE_INTERP_RATE_FACTOR *
           x->switchable_interp_costs[ctx][filter];
  }
  return 0;
}
#endif

void av1_set_rd_speed_thresholds(AV1_COMP *cpi) {
  int i;
  RD_OPT *const rd = &cpi->rd;
  SPEED_FEATURES *const sf = &cpi->sf;

  // Set baseline threshold values.
  for (i = 0; i < MAX_MODES; ++i) rd->thresh_mult[i] = cpi->oxcf.mode == 0;

  if (sf->adaptive_rd_thresh) {
    rd->thresh_mult[THR_NEARESTMV] = 300;
#if CONFIG_EXT_REFS
    rd->thresh_mult[THR_NEARESTL2] = 300;
    rd->thresh_mult[THR_NEARESTL3] = 300;
    rd->thresh_mult[THR_NEARESTB] = 300;
    rd->thresh_mult[THR_NEARESTA2] = 300;
#endif  // CONFIG_EXT_REFS
    rd->thresh_mult[THR_NEARESTA] = 300;
    rd->thresh_mult[THR_NEARESTG] = 300;
  } else {
    rd->thresh_mult[THR_NEARESTMV] = 0;
#if CONFIG_EXT_REFS
    rd->thresh_mult[THR_NEARESTL2] = 0;
    rd->thresh_mult[THR_NEARESTL3] = 0;
    rd->thresh_mult[THR_NEARESTB] = 0;
    rd->thresh_mult[THR_NEARESTA2] = 0;
#endif  // CONFIG_EXT_REFS
    rd->thresh_mult[THR_NEARESTA] = 0;
    rd->thresh_mult[THR_NEARESTG] = 0;
  }

  rd->thresh_mult[THR_DC] += 1000;

  rd->thresh_mult[THR_NEWMV] += 1000;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_NEWL2] += 1000;
  rd->thresh_mult[THR_NEWL3] += 1000;
  rd->thresh_mult[THR_NEWB] += 1000;
  rd->thresh_mult[THR_NEWA2] = 1000;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_NEWA] += 1000;
  rd->thresh_mult[THR_NEWG] += 1000;

  rd->thresh_mult[THR_NEARMV] += 1000;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_NEARL2] += 1000;
  rd->thresh_mult[THR_NEARL3] += 1000;
  rd->thresh_mult[THR_NEARB] += 1000;
  rd->thresh_mult[THR_NEARA2] = 1000;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_NEARA] += 1000;
  rd->thresh_mult[THR_NEARG] += 1000;

  rd->thresh_mult[THR_ZEROMV] += 2000;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_ZEROL2] += 2000;
  rd->thresh_mult[THR_ZEROL3] += 2000;
  rd->thresh_mult[THR_ZEROB] += 2000;
  rd->thresh_mult[THR_ZEROA2] = 2000;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_ZEROG] += 2000;
  rd->thresh_mult[THR_ZEROA] += 2000;

  rd->thresh_mult[THR_TM] += 1000;

#if CONFIG_COMPOUND_SINGLEREF
  rd->thresh_mult[THR_SR_NEAREST_NEARMV] += 1200;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_NEAREST_NEARL2] += 1200;
  rd->thresh_mult[THR_SR_NEAREST_NEARL3] += 1200;
  rd->thresh_mult[THR_SR_NEAREST_NEARB] += 1200;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_NEAREST_NEARA] += 1200;
  rd->thresh_mult[THR_SR_NEAREST_NEARG] += 1200;

  /*
  rd->thresh_mult[THR_SR_NEAREST_NEWMV] += 1200;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_NEAREST_NEWL2] += 1200;
  rd->thresh_mult[THR_SR_NEAREST_NEWL3] += 1200;
  rd->thresh_mult[THR_SR_NEAREST_NEWB] += 1200;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_NEAREST_NEWA] += 1200;
  rd->thresh_mult[THR_SR_NEAREST_NEWG] += 1200;*/

  rd->thresh_mult[THR_SR_NEAR_NEWMV] += 1500;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_NEAR_NEWL2] += 1500;
  rd->thresh_mult[THR_SR_NEAR_NEWL3] += 1500;
  rd->thresh_mult[THR_SR_NEAR_NEWB] += 1500;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_NEAR_NEWA] += 1500;
  rd->thresh_mult[THR_SR_NEAR_NEWG] += 1500;

  rd->thresh_mult[THR_SR_ZERO_NEWMV] += 2000;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_ZERO_NEWL2] += 2000;
  rd->thresh_mult[THR_SR_ZERO_NEWL3] += 2000;
  rd->thresh_mult[THR_SR_ZERO_NEWB] += 2000;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_ZERO_NEWA] += 2000;
  rd->thresh_mult[THR_SR_ZERO_NEWG] += 2000;

  rd->thresh_mult[THR_SR_NEW_NEWMV] += 1700;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_NEW_NEWL2] += 1700;
  rd->thresh_mult[THR_SR_NEW_NEWL3] += 1700;
  rd->thresh_mult[THR_SR_NEW_NEWB] += 1700;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_SR_NEW_NEWA] += 1700;
  rd->thresh_mult[THR_SR_NEW_NEWG] += 1700;
#endif  // CONFIG_COMPOUND_SINGLEREF

  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLA] += 1000;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2A] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3A] += 1000;
#endif  // CONFIG_EXT_REFS
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGA] += 1000;
#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLB] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2B] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3B] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGB] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLA2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2A2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3A2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGA2] += 1000;

#if CONFIG_EXT_COMP_REFS
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLL2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLL3] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLG] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTBA] += 1000;
#endif  // CONFIG_EXT_COMP_REFS
#endif  // CONFIG_EXT_REFS

  rd->thresh_mult[THR_COMP_NEAR_NEARLA] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLA] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLA] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLA] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROLA] += 2500;

#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_COMP_NEAR_NEARL2A] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2A] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2A] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2A] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROL2A] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3A] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3A] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3A] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3A] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROL3A] += 2500;
#endif  // CONFIG_EXT_REFS

  rd->thresh_mult[THR_COMP_NEAR_NEARGA] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGA] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGA] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGA] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROGA] += 2500;

#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_COMP_NEAR_NEARLB] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLB] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLB] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLB] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROLB] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL2B] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2B] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2B] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2B] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROL2B] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3B] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3B] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3B] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3B] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROL3B] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARGB] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGB] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGB] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGB] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROGB] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLA2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLA2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLA2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLA2] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROLA2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL2A2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2A2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2A2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2A2] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROL2A2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3A2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3A2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3A2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3A2] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROL3A2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARGA2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGA2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGA2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGA2] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROGA2] += 2500;

#if CONFIG_EXT_COMP_REFS
  rd->thresh_mult[THR_COMP_NEAR_NEARLL2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLL2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLL2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLL2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLL2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLL2] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROLL2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLL3] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLL3] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLL3] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLL3] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLL3] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLL3] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROLL3] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLG] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLG] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLG] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLG] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLG] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLG] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROLG] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARBA] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWBA] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTBA] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWBA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARBA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWBA] += 2000;
  rd->thresh_mult[THR_COMP_ZERO_ZEROBA] += 2500;
#endif  // CONFIG_EXT_COMP_REFS
#endif  // CONFIG_EXT_REFS

  rd->thresh_mult[THR_H_PRED] += 2000;
  rd->thresh_mult[THR_V_PRED] += 2000;
  rd->thresh_mult[THR_D135_PRED] += 2500;
  rd->thresh_mult[THR_D207_PRED] += 2500;
  rd->thresh_mult[THR_D153_PRED] += 2500;
  rd->thresh_mult[THR_D63_PRED] += 2500;
  rd->thresh_mult[THR_D117_PRED] += 2500;
  rd->thresh_mult[THR_D45_PRED] += 2500;

  rd->thresh_mult[THR_COMP_INTERINTRA_ZEROL] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARESTL] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARL] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEWL] += 2000;

#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_COMP_INTERINTRA_ZEROL2] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARESTL2] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARL2] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEWL2] += 2000;

  rd->thresh_mult[THR_COMP_INTERINTRA_ZEROL3] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARESTL3] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARL3] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEWL3] += 2000;
#endif  // CONFIG_EXT_REFS

  rd->thresh_mult[THR_COMP_INTERINTRA_ZEROG] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARESTG] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARG] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEWG] += 2000;

#if CONFIG_EXT_REFS
  rd->thresh_mult[THR_COMP_INTERINTRA_ZEROB] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARESTB] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARB] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEWB] += 2000;

  rd->thresh_mult[THR_COMP_INTERINTRA_ZEROA2] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARESTA2] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARA2] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEWA2] += 2000;
#endif  // CONFIG_EXT_REFS

  rd->thresh_mult[THR_COMP_INTERINTRA_ZEROA] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARESTA] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEARA] += 1500;
  rd->thresh_mult[THR_COMP_INTERINTRA_NEWA] += 2000;
}

void av1_set_rd_speed_thresholds_sub8x8(AV1_COMP *cpi) {
  static const int thresh_mult[MAX_REFS] = {
#if CONFIG_EXT_REFS
    2500,
    2500,
    2500,
    2500,
    2500,
    2500,
    2500,
    4500,
    4500,
    4500,
    4500,
    4500,
    4500,
    4500,
    4500,
    4500,
    4500,
    4500,
    4500,
    2500
#else  // !CONFIG_EXT_REFS
    2500,
    2500,
    2500,
    4500,
    4500,
    2500
#endif  // CONFIG_EXT_REFS
  };
  RD_OPT *const rd = &cpi->rd;
  memcpy(rd->thresh_mult_sub8x8, thresh_mult, sizeof(thresh_mult));
}

void av1_update_rd_thresh_fact(const AV1_COMMON *const cm,
                               int (*factor_buf)[MAX_MODES], int rd_thresh,
                               int bsize, int best_mode_index) {
  if (rd_thresh > 0) {
#if CONFIG_CB4X4
    const int top_mode = MAX_MODES;
#else
    const int top_mode = bsize < BLOCK_8X8 ? MAX_REFS : MAX_MODES;
#endif
    int mode;
    for (mode = 0; mode < top_mode; ++mode) {
      const BLOCK_SIZE min_size = AOMMAX(bsize - 1, BLOCK_4X4);
      const BLOCK_SIZE max_size = AOMMIN(bsize + 2, (int)cm->sb_size);
      BLOCK_SIZE bs;
      for (bs = min_size; bs <= max_size; ++bs) {
        int *const fact = &factor_buf[bs][mode];
        if (mode == best_mode_index) {
          *fact -= (*fact >> 4);
        } else {
          *fact = AOMMIN(*fact + RD_THRESH_INC, rd_thresh * RD_THRESH_MAX_FACT);
        }
      }
    }
  }
}

int av1_get_intra_cost_penalty(int qindex, int qdelta,
                               aom_bit_depth_t bit_depth) {
  const int q = av1_dc_quant(qindex, qdelta, bit_depth);
#if CONFIG_HIGHBITDEPTH
  switch (bit_depth) {
    case AOM_BITS_8: return 20 * q;
    case AOM_BITS_10: return 5 * q;
    case AOM_BITS_12: return ROUND_POWER_OF_TWO(5 * q, 2);
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
#else
  return 20 * q;
#endif  // CONFIG_HIGHBITDEPTH
}
