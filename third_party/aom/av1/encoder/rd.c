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

#include "config/av1_rtcd.h"

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
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/tokenize.h"

#define RD_THRESH_POW 1.25

// The baseline rd thresholds for breaking out of the rd loop for
// certain modes are assumed to be based on 8x8 blocks.
// This table is used to correct for block size.
// The factors here are << 2 (2 = x0.5, 32 = x8 etc).
static const uint8_t rd_thresh_block_size_factor[BLOCK_SIZES_ALL] = {
  2, 3, 3, 4, 6, 6, 8, 12, 12, 16, 24, 24, 32, 48, 48, 64, 4, 4, 8, 8, 16, 16
};

static const int use_intra_ext_tx_for_txsize[EXT_TX_SETS_INTRA][EXT_TX_SIZES] =
    {
      { 1, 1, 1, 1 },  // unused
      { 1, 1, 0, 0 },
      { 0, 0, 1, 0 },
    };

static const int use_inter_ext_tx_for_txsize[EXT_TX_SETS_INTER][EXT_TX_SIZES] =
    {
      { 1, 1, 1, 1 },  // unused
      { 1, 1, 0, 0 },
      { 0, 0, 1, 0 },
      { 0, 0, 0, 1 },
    };

static const int av1_ext_tx_set_idx_to_type[2][AOMMAX(EXT_TX_SETS_INTRA,
                                                      EXT_TX_SETS_INTER)] = {
  {
      // Intra
      EXT_TX_SET_DCTONLY,
      EXT_TX_SET_DTT4_IDTX_1DDCT,
      EXT_TX_SET_DTT4_IDTX,
  },
  {
      // Inter
      EXT_TX_SET_DCTONLY,
      EXT_TX_SET_ALL16,
      EXT_TX_SET_DTT9_IDTX_1DDCT,
      EXT_TX_SET_DCT_IDTX,
  },
};

void av1_fill_mode_rates(AV1_COMMON *const cm, MACROBLOCK *x,
                         FRAME_CONTEXT *fc) {
  int i, j;

  for (i = 0; i < PARTITION_CONTEXTS; ++i)
    av1_cost_tokens_from_cdf(x->partition_cost[i], fc->partition_cdf[i], NULL);

  if (cm->skip_mode_flag) {
    for (i = 0; i < SKIP_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->skip_mode_cost[i], fc->skip_mode_cdfs[i],
                               NULL);
    }
  }

  for (i = 0; i < SKIP_CONTEXTS; ++i) {
    av1_cost_tokens_from_cdf(x->skip_cost[i], fc->skip_cdfs[i], NULL);
  }

  for (i = 0; i < KF_MODE_CONTEXTS; ++i)
    for (j = 0; j < KF_MODE_CONTEXTS; ++j)
      av1_cost_tokens_from_cdf(x->y_mode_costs[i][j], fc->kf_y_cdf[i][j], NULL);

  for (i = 0; i < BLOCK_SIZE_GROUPS; ++i)
    av1_cost_tokens_from_cdf(x->mbmode_cost[i], fc->y_mode_cdf[i], NULL);
  for (i = 0; i < CFL_ALLOWED_TYPES; ++i)
    for (j = 0; j < INTRA_MODES; ++j)
      av1_cost_tokens_from_cdf(x->intra_uv_mode_cost[i][j],
                               fc->uv_mode_cdf[i][j], NULL);

  av1_cost_tokens_from_cdf(x->filter_intra_mode_cost, fc->filter_intra_mode_cdf,
                           NULL);
  for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
    if (av1_filter_intra_allowed_bsize(cm, i))
      av1_cost_tokens_from_cdf(x->filter_intra_cost[i],
                               fc->filter_intra_cdfs[i], NULL);
  }

  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
    av1_cost_tokens_from_cdf(x->switchable_interp_costs[i],
                             fc->switchable_interp_cdf[i], NULL);

  for (i = 0; i < PALATTE_BSIZE_CTXS; ++i) {
    av1_cost_tokens_from_cdf(x->palette_y_size_cost[i],
                             fc->palette_y_size_cdf[i], NULL);
    av1_cost_tokens_from_cdf(x->palette_uv_size_cost[i],
                             fc->palette_uv_size_cdf[i], NULL);
    for (j = 0; j < PALETTE_Y_MODE_CONTEXTS; ++j) {
      av1_cost_tokens_from_cdf(x->palette_y_mode_cost[i][j],
                               fc->palette_y_mode_cdf[i][j], NULL);
    }
  }

  for (i = 0; i < PALETTE_UV_MODE_CONTEXTS; ++i) {
    av1_cost_tokens_from_cdf(x->palette_uv_mode_cost[i],
                             fc->palette_uv_mode_cdf[i], NULL);
  }

  for (i = 0; i < PALETTE_SIZES; ++i) {
    for (j = 0; j < PALETTE_COLOR_INDEX_CONTEXTS; ++j) {
      av1_cost_tokens_from_cdf(x->palette_y_color_cost[i][j],
                               fc->palette_y_color_index_cdf[i][j], NULL);
      av1_cost_tokens_from_cdf(x->palette_uv_color_cost[i][j],
                               fc->palette_uv_color_index_cdf[i][j], NULL);
    }
  }

  int sign_cost[CFL_JOINT_SIGNS];
  av1_cost_tokens_from_cdf(sign_cost, fc->cfl_sign_cdf, NULL);
  for (int joint_sign = 0; joint_sign < CFL_JOINT_SIGNS; joint_sign++) {
    int *cost_u = x->cfl_cost[joint_sign][CFL_PRED_U];
    int *cost_v = x->cfl_cost[joint_sign][CFL_PRED_V];
    if (CFL_SIGN_U(joint_sign) == CFL_SIGN_ZERO) {
      memset(cost_u, 0, CFL_ALPHABET_SIZE * sizeof(*cost_u));
    } else {
      const aom_cdf_prob *cdf_u = fc->cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];
      av1_cost_tokens_from_cdf(cost_u, cdf_u, NULL);
    }
    if (CFL_SIGN_V(joint_sign) == CFL_SIGN_ZERO) {
      memset(cost_v, 0, CFL_ALPHABET_SIZE * sizeof(*cost_v));
    } else {
      const aom_cdf_prob *cdf_v = fc->cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];
      av1_cost_tokens_from_cdf(cost_v, cdf_v, NULL);
    }
    for (int u = 0; u < CFL_ALPHABET_SIZE; u++)
      cost_u[u] += sign_cost[joint_sign];
  }

  for (i = 0; i < MAX_TX_CATS; ++i)
    for (j = 0; j < TX_SIZE_CONTEXTS; ++j)
      av1_cost_tokens_from_cdf(x->tx_size_cost[i][j], fc->tx_size_cdf[i][j],
                               NULL);

  for (i = 0; i < TXFM_PARTITION_CONTEXTS; ++i) {
    av1_cost_tokens_from_cdf(x->txfm_partition_cost[i],
                             fc->txfm_partition_cdf[i], NULL);
  }

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
  for (i = 0; i < DIRECTIONAL_MODES; ++i) {
    av1_cost_tokens_from_cdf(x->angle_delta_cost[i], fc->angle_delta_cdf[i],
                             NULL);
  }
  av1_cost_tokens_from_cdf(x->switchable_restore_cost,
                           fc->switchable_restore_cdf, NULL);
  av1_cost_tokens_from_cdf(x->wiener_restore_cost, fc->wiener_restore_cdf,
                           NULL);
  av1_cost_tokens_from_cdf(x->sgrproj_restore_cost, fc->sgrproj_restore_cdf,
                           NULL);
  av1_cost_tokens_from_cdf(x->intrabc_cost, fc->intrabc_cdf, NULL);

  if (!frame_is_intra_only(cm)) {
    for (i = 0; i < COMP_INTER_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->comp_inter_cost[i], fc->comp_inter_cdf[i],
                               NULL);
    }

    for (i = 0; i < REF_CONTEXTS; ++i) {
      for (j = 0; j < SINGLE_REFS - 1; ++j) {
        av1_cost_tokens_from_cdf(x->single_ref_cost[i][j],
                                 fc->single_ref_cdf[i][j], NULL);
      }
    }

    for (i = 0; i < COMP_REF_TYPE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->comp_ref_type_cost[i],
                               fc->comp_ref_type_cdf[i], NULL);
    }

    for (i = 0; i < UNI_COMP_REF_CONTEXTS; ++i) {
      for (j = 0; j < UNIDIR_COMP_REFS - 1; ++j) {
        av1_cost_tokens_from_cdf(x->uni_comp_ref_cost[i][j],
                                 fc->uni_comp_ref_cdf[i][j], NULL);
      }
    }

    for (i = 0; i < REF_CONTEXTS; ++i) {
      for (j = 0; j < FWD_REFS - 1; ++j) {
        av1_cost_tokens_from_cdf(x->comp_ref_cost[i][j], fc->comp_ref_cdf[i][j],
                                 NULL);
      }
    }

    for (i = 0; i < REF_CONTEXTS; ++i) {
      for (j = 0; j < BWD_REFS - 1; ++j) {
        av1_cost_tokens_from_cdf(x->comp_bwdref_cost[i][j],
                                 fc->comp_bwdref_cdf[i][j], NULL);
      }
    }

    for (i = 0; i < INTRA_INTER_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->intra_inter_cost[i], fc->intra_inter_cdf[i],
                               NULL);
    }

    for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->newmv_mode_cost[i], fc->newmv_cdf[i], NULL);
    }

    for (i = 0; i < GLOBALMV_MODE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->zeromv_mode_cost[i], fc->zeromv_cdf[i], NULL);
    }

    for (i = 0; i < REFMV_MODE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->refmv_mode_cost[i], fc->refmv_cdf[i], NULL);
    }

    for (i = 0; i < DRL_MODE_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->drl_mode_cost0[i], fc->drl_cdf[i], NULL);
    }
    for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
      av1_cost_tokens_from_cdf(x->inter_compound_mode_cost[i],
                               fc->inter_compound_mode_cdf[i], NULL);
    for (i = 0; i < BLOCK_SIZES_ALL; ++i)
      av1_cost_tokens_from_cdf(x->compound_type_cost[i],
                               fc->compound_type_cdf[i], NULL);
    for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
      if (get_interinter_wedge_bits(i)) {
        av1_cost_tokens_from_cdf(x->wedge_idx_cost[i], fc->wedge_idx_cdf[i],
                                 NULL);
      }
    }
    for (i = 0; i < BLOCK_SIZE_GROUPS; ++i) {
      av1_cost_tokens_from_cdf(x->interintra_cost[i], fc->interintra_cdf[i],
                               NULL);
      av1_cost_tokens_from_cdf(x->interintra_mode_cost[i],
                               fc->interintra_mode_cdf[i], NULL);
    }
    for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
      av1_cost_tokens_from_cdf(x->wedge_interintra_cost[i],
                               fc->wedge_interintra_cdf[i], NULL);
    }
    for (i = BLOCK_8X8; i < BLOCK_SIZES_ALL; i++) {
      av1_cost_tokens_from_cdf(x->motion_mode_cost[i], fc->motion_mode_cdf[i],
                               NULL);
    }
    for (i = BLOCK_8X8; i < BLOCK_SIZES_ALL; i++) {
      av1_cost_tokens_from_cdf(x->motion_mode_cost1[i], fc->obmc_cdf[i], NULL);
    }
    for (i = 0; i < COMP_INDEX_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->comp_idx_cost[i], fc->compound_index_cdf[i],
                               NULL);
    }
    for (i = 0; i < COMP_GROUP_IDX_CONTEXTS; ++i) {
      av1_cost_tokens_from_cdf(x->comp_group_idx_cost[i],
                               fc->comp_group_idx_cdf[i], NULL);
    }
  }
}

// Values are now correlated to quantizer.
static int sad_per_bit16lut_8[QINDEX_RANGE];
static int sad_per_bit4lut_8[QINDEX_RANGE];
static int sad_per_bit16lut_10[QINDEX_RANGE];
static int sad_per_bit4lut_10[QINDEX_RANGE];
static int sad_per_bit16lut_12[QINDEX_RANGE];
static int sad_per_bit4lut_12[QINDEX_RANGE];

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
  init_me_luts_bd(sad_per_bit16lut_10, sad_per_bit4lut_10, QINDEX_RANGE,
                  AOM_BITS_10);
  init_me_luts_bd(sad_per_bit16lut_12, sad_per_bit4lut_12, QINDEX_RANGE,
                  AOM_BITS_12);
}

static const int rd_boost_factor[16] = { 64, 32, 32, 32, 24, 16, 12, 12,
                                         8,  8,  4,  4,  2,  2,  1,  0 };
static const int rd_frame_type_factor[FRAME_UPDATE_TYPES] = {
  128, 144, 128, 128, 144,
  // TODO(zoeliu): To adjust further following factor values.
  128, 128, 128,
  // TODO(weitinglin): We should investigate if the values should be the same
  //                   as the value used by OVERLAY frame
  144,  // INTNL_OVERLAY_UPDATE
  128   // INTNL_ARF_UPDATE
};

int av1_compute_rd_mult(const AV1_COMP *cpi, int qindex) {
  const int64_t q =
      av1_dc_quant_Q3(qindex, 0, cpi->common.seq_params.bit_depth);
  int64_t rdmult = 0;
  switch (cpi->common.seq_params.bit_depth) {
    case AOM_BITS_8: rdmult = 88 * q * q / 24; break;
    case AOM_BITS_10: rdmult = ROUND_POWER_OF_TWO(88 * q * q / 24, 4); break;
    case AOM_BITS_12: rdmult = ROUND_POWER_OF_TWO(88 * q * q / 24, 8); break;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
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
  switch (bit_depth) {
    case AOM_BITS_8: q = av1_dc_quant_Q3(qindex, 0, AOM_BITS_8) / 4.0; break;
    case AOM_BITS_10: q = av1_dc_quant_Q3(qindex, 0, AOM_BITS_10) / 16.0; break;
    case AOM_BITS_12: q = av1_dc_quant_Q3(qindex, 0, AOM_BITS_12) / 64.0; break;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
  // TODO(debargha): Adjust the function below.
  return AOMMAX((int)(pow(q, RD_THRESH_POW) * 5.12), 8);
}

void av1_initialize_me_consts(const AV1_COMP *cpi, MACROBLOCK *x, int qindex) {
  switch (cpi->common.seq_params.bit_depth) {
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
}

static void set_block_thresholds(const AV1_COMMON *cm, RD_OPT *rd) {
  int i, bsize, segment_id;

  for (segment_id = 0; segment_id < MAX_SEGMENTS; ++segment_id) {
    const int qindex =
        clamp(av1_get_qindex(&cm->seg, segment_id, cm->base_qindex) +
                  cm->y_dc_delta_q,
              0, MAXQ);
    const int q = compute_rd_thresh_factor(qindex, cm->seq_params.bit_depth);

    for (bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
      // Threshold here seems unnecessarily harsh but fine given actual
      // range of values used for cpi->sf.thresh_mult[].
      const int t = q * rd_thresh_block_size_factor[bsize];
      const int thresh_max = INT_MAX / t;

      for (i = 0; i < MAX_MODES; ++i)
        rd->threshes[segment_id][bsize][i] = rd->thresh_mult[i] < thresh_max
                                                 ? rd->thresh_mult[i] * t / 4
                                                 : INT_MAX;
    }
  }
}

void av1_set_mvcost(MACROBLOCK *x, int ref, int ref_mv_idx) {
  (void)ref;
  (void)ref_mv_idx;
  x->mvcost = x->mv_cost_stack;
  x->nmvjointcost = x->nmv_vec_cost;
}

void av1_fill_coeff_costs(MACROBLOCK *x, FRAME_CONTEXT *fc,
                          const int num_planes) {
  const int nplanes = AOMMIN(num_planes, PLANE_TYPES);
  for (int eob_multi_size = 0; eob_multi_size < 7; ++eob_multi_size) {
    for (int plane = 0; plane < nplanes; ++plane) {
      LV_MAP_EOB_COST *pcost = &x->eob_costs[eob_multi_size][plane];

      for (int ctx = 0; ctx < 2; ++ctx) {
        aom_cdf_prob *pcdf;
        switch (eob_multi_size) {
          case 0: pcdf = fc->eob_flag_cdf16[plane][ctx]; break;
          case 1: pcdf = fc->eob_flag_cdf32[plane][ctx]; break;
          case 2: pcdf = fc->eob_flag_cdf64[plane][ctx]; break;
          case 3: pcdf = fc->eob_flag_cdf128[plane][ctx]; break;
          case 4: pcdf = fc->eob_flag_cdf256[plane][ctx]; break;
          case 5: pcdf = fc->eob_flag_cdf512[plane][ctx]; break;
          case 6:
          default: pcdf = fc->eob_flag_cdf1024[plane][ctx]; break;
        }
        av1_cost_tokens_from_cdf(pcost->eob_cost[ctx], pcdf, NULL);
      }
    }
  }
  for (int tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (int plane = 0; plane < nplanes; ++plane) {
      LV_MAP_COEFF_COST *pcost = &x->coeff_costs[tx_size][plane];

      for (int ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->txb_skip_cost[ctx],
                                 fc->txb_skip_cdf[tx_size][ctx], NULL);

      for (int ctx = 0; ctx < SIG_COEF_CONTEXTS_EOB; ++ctx)
        av1_cost_tokens_from_cdf(pcost->base_eob_cost[ctx],
                                 fc->coeff_base_eob_cdf[tx_size][plane][ctx],
                                 NULL);
      for (int ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->base_cost[ctx],
                                 fc->coeff_base_cdf[tx_size][plane][ctx], NULL);

      for (int ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->eob_extra_cost[ctx],
                                 fc->eob_extra_cdf[tx_size][plane][ctx], NULL);

      for (int ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(pcost->dc_sign_cost[ctx],
                                 fc->dc_sign_cdf[plane][ctx], NULL);

      for (int ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
        int br_rate[BR_CDF_SIZE];
        int prev_cost = 0;
        int i, j;
        av1_cost_tokens_from_cdf(br_rate, fc->coeff_br_cdf[tx_size][plane][ctx],
                                 NULL);
        // printf("br_rate: ");
        // for(j = 0; j < BR_CDF_SIZE; j++)
        //  printf("%4d ", br_rate[j]);
        // printf("\n");
        for (i = 0; i < COEFF_BASE_RANGE; i += BR_CDF_SIZE - 1) {
          for (j = 0; j < BR_CDF_SIZE - 1; j++) {
            pcost->lps_cost[ctx][i + j] = prev_cost + br_rate[j];
          }
          prev_cost += br_rate[j];
        }
        pcost->lps_cost[ctx][i] = prev_cost;
        // printf("lps_cost: %d %d %2d : ", tx_size, plane, ctx);
        // for (i = 0; i <= COEFF_BASE_RANGE; i++)
        //  printf("%5d ", pcost->lps_cost[ctx][i]);
        // printf("\n");
      }
    }
  }
}

void av1_initialize_rd_consts(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->td.mb;
  RD_OPT *const rd = &cpi->rd;

  aom_clear_system_state();

  rd->RDMULT = av1_compute_rd_mult(cpi, cm->base_qindex + cm->y_dc_delta_q);

  set_error_per_bit(x, rd->RDMULT);

  set_block_thresholds(cm, rd);

  if (cm->cur_frame_force_integer_mv) {
    av1_build_nmv_cost_table(x->nmv_vec_cost, x->nmvcost, &cm->fc->nmvc,
                             MV_SUBPEL_NONE);
  } else {
    av1_build_nmv_cost_table(
        x->nmv_vec_cost,
        cm->allow_high_precision_mv ? x->nmvcost_hp : x->nmvcost, &cm->fc->nmvc,
        cm->allow_high_precision_mv);
  }

  x->mvcost = x->mv_cost_stack;
  x->nmvjointcost = x->nmv_vec_cost;

  if (frame_is_intra_only(cm) && cm->allow_screen_content_tools &&
      cpi->oxcf.pass != 1) {
    int *dvcost[2] = { &cpi->dv_cost[0][MV_MAX], &cpi->dv_cost[1][MV_MAX] };
    av1_build_nmv_cost_table(cpi->dv_joint_cost, dvcost, &cm->fc->ndvc,
                             MV_SUBPEL_NONE);
  }

  if (cpi->oxcf.pass != 1) {
    for (int i = 0; i < TRANS_TYPES; ++i)
      // IDENTITY: 1 bit
      // TRANSLATION: 3 bits
      // ROTZOOM: 2 bits
      // AFFINE: 3 bits
      cpi->gmtype_cost[i] = (1 + (i > 0 ? (i == ROTZOOM ? 1 : 2) : 0))
                            << AV1_PROB_COST_SHIFT;
  }
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

static double interp_cubic(const double *p, double x) {
  return p[1] + 0.5 * x *
                    (p[2] - p[0] +
                     x * (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3] +
                          x * (3.0 * (p[1] - p[2]) + p[3] - p[0])));
}

static double interp_bicubic(const double *p, int p_stride, double x,
                             double y) {
  double q[4];
  q[0] = interp_cubic(p, x);
  q[1] = interp_cubic(p + p_stride, x);
  q[2] = interp_cubic(p + 2 * p_stride, x);
  q[3] = interp_cubic(p + 3 * p_stride, x);
  return interp_cubic(q, y);
}

static const double interp_rgrid_surf[65 * 18] = {
  0.104019,    0.245714,    0.293686,    0.358635,    0.382167,    0.412446,
  0.419955,    0.421388,    0.426672,    0.427990,    0.428531,    0.456868,
  0.569880,    0.638822,    1.016319,    2.143453,    3.565229,    4.720880,
  0.124618,    0.294211,    0.352023,    0.429991,    0.458206,    0.494510,
  0.503513,    0.505232,    0.511566,    0.513234,    0.519365,    0.570225,
  0.697373,    0.840624,    1.462198,    3.289054,    6.256517,    6.852788,
  0.118630,    0.269669,    0.346620,    0.430999,    0.459385,    0.495783,
  0.504808,    0.506532,    0.512884,    0.514988,    0.543437,    0.662772,
  0.795876,    1.313596,    2.403841,    4.163098,    7.440589,    8.616275,
  0.093329,    0.168205,    0.321320,    0.430607,    0.459385,    0.495783,
  0.504813,    0.506548,    0.512975,    0.520662,    0.571659,    0.701841,
  1.010727,    2.138851,    3.460626,    6.317955,    10.098127,   14.418553,
  0.087021,    0.142905,    0.315011,    0.430509,    0.459385,    0.495787,
  0.505075,    0.507599,    0.513584,    0.543182,    0.669941,    0.825620,
  1.362800,    2.572187,    4.205047,    7.498399,    12.303118,   16.641735,
  0.086923,    0.142513,    0.314913,    0.430508,    0.459385,    0.495803,
  0.506126,    0.511816,    0.514810,    0.549705,    0.725350,    1.127334,
  2.168597,    3.463686,    6.318605,    10.162284,   18.556041,   19.847042,
  0.086923,    0.142513,    0.314913,    0.430506,    0.459376,    0.495805,
  0.506388,    0.512954,    0.520772,    0.580215,    0.810474,    1.391548,
  2.579442,    4.205160,    7.498399,    12.381597,   21.703618,   24.015457,
  0.086923,    0.142513,    0.314911,    0.430353,    0.458765,    0.495652,
  0.506391,    0.513406,    0.544098,    0.702950,    1.121860,    2.168961,
  3.463798,    6.318607,    10.162284,   18.685361,   28.188192,   37.638872,
  0.086923,    0.142513,    0.314901,    0.429742,    0.456313,    0.495045,
  0.506484,    0.519195,    0.580104,    0.810126,    1.391462,    2.579441,
  4.205160,    7.498399,    12.381597,   21.848607,   33.367199,   42.623190,
  0.086923,    0.142513,    0.314899,    0.429589,    0.455706,    0.495155,
  0.507882,    0.542426,    0.702360,    1.119921,    2.168478,    3.463791,
  6.318607,    10.162284,   18.685361,   28.345760,   47.802028,   49.163533,
  0.086924,    0.142548,    0.315086,    0.429842,    0.455870,    0.496336,
  0.512412,    0.556953,    0.773373,    1.266396,    2.548277,    4.204676,
  7.498399,    12.381597,   21.848607,   33.548250,   54.301011,   56.262859,
  0.087067,    0.144957,    0.327436,    0.446616,    0.466362,    0.505706,
  0.522077,    0.610747,    0.972543,    1.666916,    3.338812,    6.316669,
  10.162284,   18.685361,   28.345760,   48.065311,   66.145302,   78.396020,
  0.094295,    0.164235,    0.393722,    0.534219,    0.530922,    0.579308,
  0.603889,    0.760870,    1.229961,    2.423214,    4.173513,    7.497916,
  12.381597,   21.848607,   33.548250,   54.589585,   74.875848,   86.468182,
  0.124096,    0.213005,    0.497188,    0.665176,    0.685973,    0.800200,
  0.911394,    1.077971,    1.677290,    3.332129,    6.314960,    10.162257,
  18.685361,   28.345760,   48.065311,   66.453506,   98.275189,   96.862588,
  0.140999,    0.270140,    0.658212,    0.867661,    0.970183,    1.149516,
  1.480599,    1.664833,    2.421893,    3.857981,    7.418830,    12.380371,
  21.848607,   33.548250,   54.589585,   75.188867,   106.657971,  99.762997,
  0.178353,    0.398001,    0.988462,    1.241473,    1.340967,    1.713568,
  2.335030,    2.701432,    3.348532,    5.077158,    9.829903,    18.676528,
  28.345700,   48.065311,   66.453506,   98.588283,   117.057193,  101.130722,
  0.281079,    0.548300,    1.395825,    1.780770,    2.000508,    2.702964,
  3.638454,    4.573843,    5.051641,    7.079129,    11.293332,   21.594861,
  33.544335,   54.589585,   75.188867,   106.971065,  119.957601,  101.466632,
  0.476762,    0.842189,    2.019678,    2.723895,    3.188467,    4.011610,
  5.545111,    7.508984,    8.176339,    9.774504,    14.720782,   27.334416,
  48.049609,   66.453506,   98.588283,   117.370357,  121.329855,  101.509242,
  0.993999,    1.520111,    3.013605,    4.203530,    4.982992,    6.074944,
  8.583581,    11.818375,   14.192544,   14.937517,   21.258160,   33.305953,
  54.585735,   75.188867,   106.971135,  120.279824,  121.976055,  102.690130,
  1.776487,    2.613655,    4.356487,    6.161726,    7.622196,    9.464193,
  13.077233,   18.051656,   23.221051,   24.080068,   30.085038,   48.345269,
  66.457698,   98.588353,   117.379415,  121.976128,  124.356210,  107.713202,
  3.191085,    4.495201,    5.686033,    8.365566,    11.275339,   14.706437,
  20.300969,   28.152237,   35.688355,   39.341382,   41.030743,   55.752262,
  75.211764,   106.980285,  120.608403,  124.680746,  130.222528,  112.260098,
  6.136611,    7.305215,    7.272532,    10.646713,   15.630815,   22.383168,
  31.349131,   42.419822,   52.301680,   58.983454,   58.915405,   69.161305,
  98.992460,   117.713855,  124.344836,  130.623638,  138.442401,  127.846670,
  11.707980,   13.490761,   11.640845,   14.176132,   22.131124,   33.776462,
  47.365711,   61.603834,   75.281056,   83.463985,   85.510533,   86.026513,
  108.787480,  123.031136,  130.607284,  138.954406,  160.867784,  158.958882,
  27.062874,   32.195139,   24.147297,   22.114632,   35.580506,   52.551674,
  71.652956,   88.606776,   102.107193,  110.703186,  114.398733,  111.118539,
  121.503578,  132.455924,  139.490806,  161.412674,  193.563210,  172.203945,
  35.625692,   47.953028,   42.639820,   42.276254,   58.815664,   84.977282,
  110.656412,  126.168446,  134.658126,  140.604482,  144.006012,  141.702382,
  140.125323,  153.122630,  164.748041,  194.156197,  206.854650,  174.013079,
  49.516447,   65.335381,   71.738306,   81.872819,   98.400740,   136.840488,
  163.775802,  169.440078,  172.747876,  171.222919,  171.679604,  172.173550,
  168.200129,  187.617133,  199.683394,  207.768200,  210.062520,  175.478356,
  60.341673,   92.487135,   119.907299,  136.068010,  144.778950,  189.443534,
  220.120077,  219.641635,  214.616503,  205.894657,  198.453924,  200.013069,
  195.938103,  206.118661,  210.447375,  212.061379,  216.078218,  181.162805,
  78.422159,   112.242899,  158.416312,  181.404320,  193.188690,  229.296967,
  270.461799,  275.168977,  256.511701,  244.706786,  231.344608,  226.065087,
  222.248618,  218.662324,  217.966722,  218.248574,  218.818588,  182.740573,
  88.713664,   123.594164,  172.928179,  213.781414,  245.800351,  252.063414,
  313.283141,  331.703831,  305.866639,  285.177142,  269.759635,  251.988739,
  245.998388,  232.688076,  230.588702,  230.882657,  230.319053,  192.120741,
  102.540561,  152.905927,  189.137131,  241.806756,  273.868497,  284.258017,
  339.689853,  373.561104,  362.657463,  326.291984,  311.922687,  290.460189,
  276.774381,  273.012072,  277.751792,  279.123748,  278.820447,  233.813798,
  132.983118,  176.307242,  197.415684,  243.307787,  280.893995,  332.922370,
  340.329043,  404.530166,  419.475405,  375.775209,  351.300889,  340.042759,
  315.683832,  306.123530,  306.359319,  306.733063,  307.609556,  261.647847,
  149.579109,  185.925581,  207.937033,  245.159084,  301.890957,  350.040480,
  352.250771,  418.742329,  458.112686,  430.125208,  386.460441,  380.346839,
  354.679150,  337.305620,  334.504124,  335.889932,  341.060725,  286.898578,
  153.576812,  202.105624,  219.366967,  248.524506,  314.255692,  350.607526,
  390.567688,  408.629209,  488.000213,  480.563823,  432.461799,  410.412624,
  398.607371,  400.188740,  402.780916,  408.853470,  430.449735,  363.777088,
  161.353129,  214.848904,  231.549852,  258.536466,  313.163177,  368.140577,
  412.136393,  413.409032,  499.838438,  519.571063,  485.833867,  444.562715,
  435.738129,  442.358549,  450.166531,  453.208524,  458.424358,  385.823139,
  175.109034,  227.608058,  250.069563,  286.101747,  312.256740,  378.421485,
  413.344147,  435.058646,  476.960941,  542.448886,  530.189154,  495.408402,
  475.326752,  465.017144,  464.694045,  465.144689,  466.905382,  398.669138,
  184.750180,  240.766694,  283.240772,  305.480150,  322.409001,  374.526162,
  427.141326,  452.840323,  472.604139,  545.366105,  567.676694,  541.666203,
  509.591873,  492.044219,  492.778569,  493.765684,  493.235693,  413.684325,
  194.728357,  254.928927,  289.991157,  300.193195,  324.194589,  371.563147,
  439.226438,  468.295088,  495.654854,  533.506353,  587.476353,  578.298989,
  548.041942,  527.393885,  538.965146,  545.070442,  544.295454,  454.012211,
  205.195287,  283.135677,  297.921431,  319.295927,  355.621830,  392.466463,
  446.696167,  485.053519,  516.426615,  532.264584,  588.481600,  615.906737,
  589.319634,  555.754316,  558.389367,  569.094521,  569.779764,  475.384946,
  218.552054,  298.511016,  319.188338,  351.781666,  372.789510,  412.827434,
  464.569387,  506.270203,  533.049810,  553.347364,  580.644599,  632.759854,
  622.235843,  569.960552,  580.799340,  586.553714,  579.488366,  491.826482,
  244.803348,  299.790203,  324.187975,  363.280782,  403.710443,  441.724083,
  492.732682,  534.722691,  552.193622,  575.112647,  586.097705,  635.224970,
  644.642944,  606.017786,  640.321218,  642.316989,  616.397020,  548.300111,
  256.957358,  318.638991,  355.063346,  389.889307,  433.607315,  468.209001,
  515.178157,  573.556591,  578.113115,  587.246475,  601.762801,  638.454644,
  656.574853,  641.184609,  676.908189,  684.198162,  678.387412,  574.805864,
  251.211502,  323.448532,  364.227424,  411.792704,  462.226488,  503.572288,
  549.299249,  599.124071,  601.227977,  597.118176,  613.247552,  633.278532,
  658.074755,  664.930719,  685.731531,  693.632845,  693.076350,  578.326477,
  267.695377,  354.273736,  389.976833,  438.518178,  493.332686,  544.343027,
  588.895829,  620.206193,  628.327410,  606.067827,  620.998532,  657.985256,
  683.936059,  691.345257,  693.894723,  695.175306,  693.618786,  578.517148,
  274.290725,  363.465288,  411.808596,  463.369805,  515.310226,  581.009306,
  613.070738,  636.638714,  647.333929,  629.867603,  644.646319,  687.796202,
  702.859596,  713.495479,  704.068069,  704.991807,  704.188594,  587.283658,
  302.538449,  389.174737,  438.518422,  493.398902,  547.662399,  601.981814,
  624.773046,  641.629484,  644.699451,  645.848784,  668.033340,  703.643523,
  707.422408,  717.329600,  726.298973,  744.127507,  745.365167,  617.954068,
  310.328188,  410.984766,  463.369805,  515.315010,  581.309832,  613.787792,
  634.988538,  654.145284,  662.632978,  668.413496,  706.494057,  750.545471,
  730.724808,  730.002100,  743.625262,  750.801609,  745.308457,  606.505800,
  329.948756,  437.600191,  493.398902,  547.661910,  601.917884,  622.557745,
  633.244395,  644.055898,  648.224221,  665.062911,  763.555733,  812.391078,
  769.063582,  744.865168,  727.579796,  724.950408,  722.179707,  598.564510,
  350.848328,  462.437458,  515.315010,  581.309823,  613.779123,  634.465309,
  652.056257,  662.179143,  671.466297,  726.881256,  819.824030,  880.232789,
  810.371672,  754.246481,  725.053473,  724.253390,  723.503395,  603.394909,
  373.704088,  492.408266,  547.661910,  601.917884,  622.557620,  633.236320,
  644.023513,  648.232514,  666.381639,  785.498283,  929.441612,  999.772800,
  890.339033,  775.852504,  731.840181,  726.905100,  725.251844,  604.899901,
  394.473422,  514.261306,  581.309823,  613.779123,  634.465309,  652.056257,
  662.179143,  671.466557,  727.134512,  835.764144,  981.747089,  1018.462934,
  939.686967,  811.276731,  739.398459,  727.365647,  725.285425,  604.923525,
  419.976505,  546.538939,  601.917884,  622.557620,  633.236320,  644.023513,
  648.232514,  666.381639,  785.545191,  932.841398,  1036.609617, 1026.945092,
  963.822765,  840.827315,  755.532423,  730.241865,  725.366847,  604.924155,
  437.281359,  580.116337,  613.779123,  634.465309,  652.056257,  662.179143,
  671.466557,  727.134512,  835.764859,  981.996194,  1031.896881, 1002.544732,
  881.157178,  828.151494,  799.340975,  751.314325,  728.316587,  605.005504,
  464.713920,  600.649281,  622.557620,  633.236320,  644.023513,  648.232514,
  666.381639,  785.545191,  932.841398,  1036.735329, 1035.037004, 995.478339,
  858.093733,  823.471976,  819.881754,  798.749289,  749.440463,  607.955244,
  495.880237,  612.473139,  634.465309,  652.056257,  662.179143,  671.466557,
  727.134512,  835.764859,  981.996194,  1032.339788, 1031.105117, 995.303259,
  857.733663,  823.435877,  822.822791,  819.873050,  796.882480,  629.038445,
  510.391280,  621.158273,  633.236320,  644.023513,  648.232514,  666.381639,
  785.545191,  932.841398,  1036.735329, 1035.566013, 1029.599350, 994.926093,
  857.645648,  823.435143,  822.904139,  822.822791,  817.965681,  673.856962,
  514.588176,  632.947715,  652.056257,  662.179143,  671.466557,  727.134512,
  835.764859,  981.996194,  1032.339788, 1031.547475, 1023.835377, 972.158629,
  851.968626,  823.347128,  822.904770,  822.904139,  820.752301,  684.418900,
  520.013294,  631.668183,  644.023513,  648.232514,  666.381639,  785.545191,
  932.841398,  1036.735329, 1035.567378, 1029.776746, 1001.044108, 880.853721,
  829.201546,  822.994150,  822.904770,  822.904770,  820.792975,  684.582020,
  531.253628,  650.479606,  662.179143,  671.466557,  727.134512,  835.764859,
  981.996194,  1032.339788, 1031.636855, 1029.601779, 995.366703,  858.086641,
  823.524524,  822.906135,  822.904770,  822.904770,  820.792975,  684.582020,
  528.531744,  642.424501,  648.232514,  666.381639,  785.545191,  932.841398,
  1036.735329, 1035.567378, 1030.219103, 1029.576226, 995.278687,  857.733663,
  823.436508,  822.904770,  822.904770,  822.904770,  820.792975,  684.582020,
  545.401164,  660.550678,  671.508859,  727.304161,  835.807162,  981.996850,
  1032.339788, 1031.636855, 1030.130788, 1029.487827, 994.925709,  857.645648,
  823.435143,  822.904770,  822.904770,  822.904770,  820.792975,  684.582020,
  537.684760,  646.650947,  669.110131,  796.487512,  935.569890,  1036.777631,
  1035.567378, 1030.219103, 1030.018584, 1023.810805, 972.158629,  851.968626,
  823.347128,  822.904770,  822.904770,  822.904770,  820.792975,  684.582020,
  552.408370,  670.001885,  738.246482,  879.690154,  992.939171,  1032.509436,
  1031.636855, 1030.132153, 1029.665223, 1001.043724, 880.853721,  829.201546,
  822.994150,  822.904770,  822.904770,  822.904770,  820.792975,  684.582020,
  539.835902,  667.496388,  799.216004,  946.512211,  1039.506123, 1035.609680,
  1030.219103, 1030.107964, 1029.577207, 995.366703,  858.086641,  823.524524,
  822.906135,  822.904770,  822.904770,  822.904770,  820.792975,  684.582020,
  558.362529,  734.277451,  877.197218,  990.478243,  1029.908393, 1028.993978,
  1027.488620, 1027.464048, 1026.933674, 992.724534,  855.532488,  821.323349,
  820.792975,  820.792975,  820.792975,  820.792975,  818.686600,  682.825198,
  453.127195,  649.075095,  780.278390,  867.165890,  862.469711,  857.067460,
  856.956321,  856.955937,  856.513579,  827.981461,  713.556496,  685.024378,
  684.582020,  684.582020,  684.582020,  684.582020,  682.825198,  569.510056,
};

static const double interp_dgrid_surf[65 * 18] = {
  10.650434, 12.204694, 12.040917, 11.843008, 11.845578, 12.051535, 12.103583,
  12.136780, 12.266709, 12.299107, 12.299673, 12.303120, 12.316337, 12.293431,
  12.092165, 11.602421, 11.141559, 8.864495,  12.770003, 14.634889, 14.437149,
  14.199413, 14.202487, 14.449423, 14.511827, 14.551629, 14.707410, 14.746265,
  14.747610, 14.753705, 14.762194, 14.699395, 14.390525, 13.690970, 12.874168,
  10.367121, 12.832328, 14.790730, 14.503765, 14.236403, 14.239028, 14.486600,
  14.549164, 14.589069, 14.745250, 14.784258, 14.788320, 14.801930, 14.762798,
  14.499088, 14.021544, 13.469684, 12.661560, 10.108384, 12.950520, 15.264726,
  14.621957, 14.238236, 14.239028, 14.486601, 14.549264, 14.589469, 14.745361,
  14.784949, 14.791572, 14.798652, 14.660251, 14.119394, 13.651131, 12.935657,
  12.176082, 9.228999,  12.979992, 15.382918, 14.651428, 14.238693, 14.239028,
  14.486701, 14.555710, 14.615321, 14.751849, 14.787700, 14.797104, 14.743189,
  14.475057, 13.944406, 13.450468, 12.687876, 11.824993, 8.906683,  12.980449,
  15.384750, 14.651885, 14.238700, 14.239028, 14.487102, 14.581562, 14.718998,
  14.777721, 14.788445, 14.778661, 14.582790, 14.099785, 13.649637, 12.935359,
  12.201859, 10.891931, 8.482221,  12.980449, 15.384750, 14.651886, 14.238801,
  14.239434, 14.487303, 14.588010, 14.744860, 14.784773, 14.786094, 14.735647,
  14.455704, 13.939591, 13.450393, 12.687876, 11.849334, 10.476658, 8.043672,
  12.980449, 15.384750, 14.651987, 14.245320, 14.265579, 14.493824, 14.588211,
  14.745312, 14.787263, 14.775934, 14.582036, 14.099475, 13.649563, 12.935358,
  12.201859, 10.911285, 9.730570,  6.696921,  12.980449, 15.384750, 14.652393,
  14.271466, 14.370434, 14.520069, 14.589027, 14.746028, 14.785482, 14.735605,
  14.455693, 13.939590, 13.450393, 12.687876, 11.849334, 10.494514, 9.195398,
  6.215460,  12.980449, 15.384750, 14.652494, 14.277985, 14.396679, 14.533035,
  14.615021, 14.754825, 14.775610, 14.582796, 14.099664, 13.649565, 12.935358,
  12.201859, 10.911285, 9.747361,  7.779960,  5.617541,  12.980448, 15.384731,
  14.652415, 14.278078, 14.397578, 14.559053, 14.718657, 14.776398, 14.747044,
  14.504690, 13.951810, 13.450583, 12.687876, 11.849334, 10.494514, 9.210817,
  7.210003,  5.164575,  12.980446, 15.383448, 14.647073, 14.277541, 14.403813,
  14.569546, 14.744956, 14.765103, 14.629073, 14.296161, 13.698573, 12.936118,
  12.201859, 10.911285, 9.747361,  7.790897,  6.322998,  3.931551,  12.981550,
  15.376916, 14.615597, 14.274820, 14.437479, 14.575942, 14.707492, 14.734111,
  14.515975, 14.000806, 13.462803, 12.688066, 11.849334, 10.494514, 9.210817,
  7.219566,  5.781392,  3.486081,  12.991899, 15.376201, 14.579444, 14.296898,
  14.473361, 14.522910, 14.491600, 14.543267, 14.288580, 13.700311, 12.936579,
  12.201867, 10.911285, 9.747361,  7.790897,  6.331506,  4.480348,  2.923138,
  13.019848, 15.383477, 14.582260, 14.385262, 14.452673, 14.436019, 14.238174,
  14.255993, 13.977481, 13.532342, 12.705591, 11.849605, 10.494514, 9.210817,
  7.219566,  5.789642,  4.018194,  2.766222,  13.028558, 15.315782, 14.439141,
  14.326286, 14.452429, 14.311731, 14.033235, 13.922587, 13.665868, 13.207897,
  12.274375, 10.912967, 9.747371,  7.790897,  6.331506,  4.488594,  3.454993,
  2.692682,  12.992752, 15.321471, 14.409573, 14.236340, 14.322969, 14.049072,
  13.764823, 13.479242, 13.250105, 12.759133, 12.019174, 10.532951, 9.211409,
  7.219566,  5.789642,  4.026440,  3.298077,  2.674624,  12.945493, 15.276596,
  14.315745, 14.026198, 14.085774, 13.844563, 13.447576, 12.964935, 12.735525,
  12.288592, 11.511693, 9.900227,  7.793270,  6.331506,  4.488594,  3.463236,
  3.224318,  2.672433,  12.757570, 15.056661, 14.095011, 13.722362, 13.812624,
  13.608480, 13.021206, 12.367627, 11.937931, 11.581049, 10.599552, 9.247860,
  7.220151,  5.789642,  4.026437,  3.305882,  3.191260,  2.615317,  12.581293,
  14.824658, 13.909074, 13.496158, 13.491402, 13.221550, 12.514140, 11.677229,
  10.936895, 10.619912, 9.634779,  7.763570,  6.331082,  4.488590,  3.462798,
  3.216460,  3.076315,  2.373499,  12.283499, 14.455760, 13.890593, 13.427587,
  13.183783, 12.763833, 11.861006, 10.740618, 9.820756,  9.354945,  8.669862,
  7.123268,  5.787860,  4.025994,  3.290000,  3.084410,  2.810905,  2.222916,
  12.010893, 14.300919, 13.986624, 13.484026, 13.025385, 12.224281, 11.064265,
  9.631040,  8.594396,  8.003736,  7.561587,  6.274418,  4.466637,  3.446574,
  3.102467,  2.816989,  2.598688,  1.951541,  11.581477, 13.831132, 13.632027,
  13.380414, 12.807880, 11.665651, 10.218236, 8.562237,  7.222614,  6.611808,
  6.261676,  5.402793,  3.938544,  3.174375,  2.818166,  2.602758,  2.213911,
  1.434763,  11.050735, 12.893449, 12.363152, 12.712829, 12.012961, 10.887854,
  9.109699,  7.421701,  5.965603,  5.272129,  4.991435,  4.423000,  3.369988,
  2.800371,  2.593901,  2.217431,  1.670917,  1.215265,  10.641194, 11.766277,
  10.777082, 10.972917, 10.689298, 9.701545,  7.719947,  6.145654,  4.872442,
  4.099600,  3.880934,  3.514159,  2.786474,  2.368963,  2.162376,  1.673670,
  1.450770,  1.185424,  10.071964, 11.107701, 9.172361,  8.551313,  8.412080,
  7.641397,  6.174246,  4.853916,  3.904549,  3.246810,  2.959903,  2.785066,
  2.240001,  1.793166,  1.585520,  1.449824,  1.405368,  1.168856,  9.213182,
  9.173278,  7.219231,  6.242951,  5.626013,  5.768007,  4.908666,  3.809589,
  3.115109,  2.617899,  2.274793,  2.172960,  1.838597,  1.505915,  1.414333,
  1.392666,  1.338173,  1.105611,  7.365015,  7.471370,  5.622346,  4.520127,
  3.936272,  4.208822,  3.623024,  2.977794,  2.450003,  2.097261,  1.824090,
  1.643270,  1.473525,  1.351388,  1.327504,  1.323865,  1.307894,  1.088234,
  6.198210,  6.580712,  4.682511,  3.416952,  2.941929,  2.766637,  2.650686,
  2.315439,  1.925838,  1.659784,  1.464419,  1.252806,  1.162722,  1.197518,
  1.199875,  1.197365,  1.194040,  0.995797,  5.402507,  5.055466,  3.728724,
  2.624359,  2.165810,  1.943189,  1.918190,  1.738078,  1.516328,  1.290520,
  1.155793,  1.015962,  0.881900,  0.807203,  0.754242,  0.743378,  0.740288,
  0.614158,  3.937867,  3.862507,  2.884664,  2.088147,  1.648496,  1.473584,
  1.340123,  1.291769,  1.165381,  1.000224,  0.893316,  0.821333,  0.691363,
  0.610501,  0.586766,  0.583762,  0.577840,  0.468733,  3.104660,  3.181078,
  2.420208,  1.747442,  1.297956,  1.109835,  0.970385,  0.943229,  0.876923,
  0.777584,  0.678183,  0.628623,  0.553745,  0.523430,  0.519490,  0.514394,
  0.492259,  0.403172,  2.593833,  2.533720,  2.010452,  1.480944,  1.060302,
  0.846383,  0.738703,  0.673144,  0.658010,  0.592449,  0.518236,  0.470335,
  0.425088,  0.393168,  0.378116,  0.355846,  0.275469,  0.213128,  2.176988,
  2.089575,  1.671284,  1.225008,  0.895382,  0.672008,  0.566241,  0.496746,
  0.488005,  0.449874,  0.400899,  0.354002,  0.318150,  0.281533,  0.238545,
  0.224159,  0.202399,  0.160681,  1.874679,  1.769165,  1.430124,  1.068727,
  0.780272,  0.557801,  0.441643,  0.377256,  0.352957,  0.338452,  0.304965,
  0.273172,  0.240052,  0.208724,  0.193431,  0.190845,  0.185025,  0.138166,
  1.590226,  1.502830,  1.193127,  0.917885,  0.670432,  0.474546,  0.355420,
  0.292305,  0.259035,  0.249937,  0.232079,  0.208943,  0.181936,  0.160038,
  0.152257,  0.151235,  0.149583,  0.120747,  1.331730,  1.255907,  1.012871,
  0.778422,  0.578977,  0.412432,  0.293155,  0.231824,  0.197187,  0.183921,
  0.174876,  0.157252,  0.140263,  0.127050,  0.110244,  0.105041,  0.104323,
  0.086944,  1.153994,  1.118771,  0.822355,  0.612321,  0.478249,  0.348222,
  0.247408,  0.186141,  0.152714,  0.135445,  0.129810,  0.119994,  0.115619,
  0.131626,  0.095612,  0.079343,  0.077502,  0.064550,  0.946317,  0.925894,
  0.677969,  0.499906,  0.397101,  0.297931,  0.214467,  0.152333,  0.120731,
  0.102686,  0.095062,  0.090361,  0.122319,  0.240194,  0.112687,  0.070690,
  0.070461,  0.054194,  0.824155,  0.787241,  0.581856,  0.419228,  0.313167,
  0.245582,  0.183500,  0.128101,  0.096577,  0.080267,  0.071022,  0.066851,
  0.085754,  0.154163,  0.075884,  0.052401,  0.054270,  0.026656,  0.716310,
  0.671378,  0.489580,  0.349569,  0.256155,  0.206343,  0.157853,  0.111950,
  0.079271,  0.062518,  0.053441,  0.049660,  0.051400,  0.063778,  0.039993,
  0.029133,  0.023382,  0.013725,  0.614125,  0.579096,  0.417126,  0.299465,
  0.217849,  0.165515,  0.129040,  0.093127,  0.065612,  0.049543,  0.041429,
  0.036850,  0.034416,  0.033989,  0.024216,  0.017377,  0.014833,  0.011987,
  0.520407,  0.487239,  0.349473,  0.251741,  0.184897,  0.135813,  0.107098,
  0.073607,  0.053938,  0.040531,  0.032931,  0.028876,  0.025759,  0.022168,
  0.016739,  0.014638,  0.014333,  0.011947,  0.449954,  0.415124,  0.299452,
  0.216942,  0.158874,  0.115334,  0.088821,  0.060105,  0.042610,  0.032566,
  0.026903,  0.023123,  0.019913,  0.016835,  0.014306,  0.013625,  0.013535,
  0.011284,  0.377618,  0.347773,  0.251741,  0.184839,  0.132857,  0.095439,
  0.070462,  0.052244,  0.036078,  0.026025,  0.021518,  0.018487,  0.015361,
  0.012905,  0.011470,  0.010569,  0.010283,  0.008297,  0.319953,  0.297976,
  0.216942,  0.158842,  0.113280,  0.080426,  0.057367,  0.041987,  0.030135,
  0.022295,  0.017901,  0.015121,  0.012224,  0.010035,  0.009353,  0.009108,
  0.008695,  0.006139,  0.267864,  0.250502,  0.184839,  0.132851,  0.095039,
  0.068220,  0.049135,  0.035315,  0.025144,  0.018237,  0.013857,  0.012094,
  0.009715,  0.007743,  0.006937,  0.006446,  0.006243,  0.004929,  0.230449,
  0.215895,  0.158842,  0.113280,  0.080417,  0.057174,  0.041304,  0.029959,
  0.021866,  0.015673,  0.012133,  0.010083,  0.007801,  0.006053,  0.005401,
  0.003834,  0.003429,  0.002851,  0.193984,  0.183963,  0.132851,  0.095039,
  0.068220,  0.049133,  0.035305,  0.025140,  0.018150,  0.013175,  0.010422,
  0.008491,  0.006397,  0.004567,  0.003494,  0.002933,  0.002825,  0.002355,
  0.167298,  0.158088,  0.113280,  0.080417,  0.057174,  0.041304,  0.029959,
  0.021866,  0.015669,  0.011955,  0.009257,  0.007051,  0.005543,  0.003905,
  0.002984,  0.002825,  0.002814,  0.002347,  0.143228,  0.132220,  0.095039,
  0.068220,  0.049133,  0.035305,  0.025140,  0.018150,  0.013174,  0.010394,
  0.008403,  0.006661,  0.005378,  0.003545,  0.002876,  0.002818,  0.002814,
  0.002347,  0.122934,  0.112735,  0.080417,  0.057174,  0.041304,  0.029959,
  0.021866,  0.015669,  0.011955,  0.009258,  0.007182,  0.006012,  0.003762,
  0.002866,  0.002739,  0.002788,  0.002810,  0.002347,  0.101934,  0.094569,
  0.068220,  0.049133,  0.035305,  0.025140,  0.018150,  0.013174,  0.010394,
  0.008405,  0.006797,  0.005845,  0.003333,  0.002703,  0.002695,  0.002723,
  0.002781,  0.002343,  0.086702,  0.080014,  0.057174,  0.041304,  0.029959,
  0.021866,  0.015669,  0.011955,  0.009258,  0.007190,  0.006533,  0.005839,
  0.003326,  0.002700,  0.002690,  0.002694,  0.002716,  0.002314,  0.073040,
  0.067886,  0.049133,  0.035305,  0.025140,  0.018150,  0.013174,  0.010394,
  0.008405,  0.006807,  0.006468,  0.005831,  0.003325,  0.002700,  0.002690,
  0.002690,  0.002687,  0.002253,  0.061685,  0.056890,  0.041304,  0.029959,
  0.021866,  0.015669,  0.011955,  0.009258,  0.007190,  0.006542,  0.006360,
  0.005416,  0.003221,  0.002698,  0.002690,  0.002690,  0.002683,  0.002238,
  0.052465,  0.048894,  0.035305,  0.025140,  0.018150,  0.013174,  0.010394,
  0.008405,  0.006807,  0.006472,  0.005943,  0.003748,  0.002805,  0.002692,
  0.002690,  0.002690,  0.002683,  0.002238,  0.043838,  0.041101,  0.029959,
  0.021866,  0.015669,  0.011955,  0.009258,  0.007190,  0.006543,  0.006465,
  0.005839,  0.003333,  0.002702,  0.002690,  0.002690,  0.002690,  0.002683,
  0.002238,  0.037824,  0.035133,  0.025140,  0.018150,  0.013174,  0.010394,
  0.008405,  0.006807,  0.006480,  0.006464,  0.005838,  0.003326,  0.002700,
  0.002690,  0.002690,  0.002690,  0.002683,  0.002238,  0.031865,  0.029815,
  0.021866,  0.015668,  0.011955,  0.009258,  0.007190,  0.006543,  0.006475,
  0.006462,  0.005831,  0.003325,  0.002700,  0.002690,  0.002690,  0.002690,
  0.002683,  0.002238,  0.027150,  0.025016,  0.018128,  0.013083,  0.010371,
  0.008405,  0.006807,  0.006480,  0.006472,  0.006359,  0.005416,  0.003221,
  0.002698,  0.002690,  0.002690,  0.002690,  0.002683,  0.002238,  0.023094,
  0.021760,  0.015577,  0.011590,  0.009167,  0.007188,  0.006543,  0.006475,
  0.006466,  0.005943,  0.003748,  0.002805,  0.002692,  0.002690,  0.002690,
  0.002690,  0.002683,  0.002238,  0.019269,  0.018038,  0.013060,  0.010280,
  0.008382,  0.006806,  0.006480,  0.006474,  0.006464,  0.005839,  0.003333,
  0.002702,  0.002690,  0.002690,  0.002690,  0.002690,  0.002683,  0.002238,
  0.016874,  0.015472,  0.011566,  0.009148,  0.007171,  0.006527,  0.006458,
  0.006457,  0.006447,  0.005823,  0.003318,  0.002693,  0.002683,  0.002683,
  0.002683,  0.002683,  0.002676,  0.002232,  0.011968,  0.011056,  0.008762,
  0.007219,  0.005717,  0.005391,  0.005386,  0.005386,  0.005377,  0.004856,
  0.002767,  0.002246,  0.002238,  0.002238,  0.002238,  0.002238,  0.002232,
  0.001862,
};

void av1_model_rd_surffit(double xm, double yl, double *rate_f,
                          double *dist_f) {
  const double x_start = -0.5;
  const double x_end = 16.5;
  const double x_step = 1;
  const double y_start = -15.5;
  const double y_end = 16.5;
  const double y_step = 0.5;
  const double epsilon = 1e-6;
  const int stride = (int)rint((x_end - x_start) / x_step) + 1;
  (void)y_end;

  xm = AOMMAX(xm, x_start + x_step + epsilon);
  xm = AOMMIN(xm, x_end - x_step - epsilon);
  yl = AOMMAX(yl, y_start + y_step + epsilon);
  yl = AOMMIN(yl, y_end - y_step - epsilon);

  const double y = (yl - y_start) / y_step;
  const double x = (xm - x_start) / x_step;

  const int yi = (int)floor(y);
  const int xi = (int)floor(x);
  assert(xi > 0);
  assert(yi > 0);

  const double yo = y - yi;
  const double xo = x - xi;
  const double *prate = &interp_rgrid_surf[(yi - 1) * stride + (xi - 1)];
  const double *pdist = &interp_dgrid_surf[(yi - 1) * stride + (xi - 1)];
  *rate_f = interp_bicubic(prate, stride, xo, yo);
  *dist_f = interp_bicubic(pdist, stride, xo, yo);
}

static const double interp_rgrid_curv[65] = {
  0.000000,    0.000000,    0.000000,    0.000000,    0.000000,     0.000000,
  0.000000,    0.000000,    0.000000,    0.000000,    0.000000,     0.000000,
  0.000000,    0.000000,    0.000000,    0.000000,    0.000000,     4.759876,
  8.132086,    13.651828,   21.908271,   33.522054,   48.782376,    71.530983,
  106.728649,  151.942795,  199.893011,  242.850965,  283.933923,   322.154203,
  360.684608,  394.801656,  426.879017,  460.234313,  484.103987,   508.261495,
  536.486763,  558.196737,  586.285894,  614.764511,  634.166333,   647.706472,
  658.211478,  681.360407,  701.052141,  727.007310,  768.663973,   804.407660,
  884.627751,  1065.658131, 1238.875214, 1440.185176, 1678.377931,  1962.243390,
  2300.571467, 2702.152072, 3175.775119, 3730.230519, 4374.308184,  5116.798028,
  5966.489961, 6932.173897, 8022.639747, 9246.677424, 10613.076839,
};

static const double interp_dgrid_curv[65] = {
  14.604855, 14.604855, 14.604855, 14.604855, 14.604855, 14.604855, 14.604855,
  14.604855, 14.604855, 14.604855, 14.604855, 14.604855, 14.555776, 14.533692,
  14.439920, 14.257791, 13.977230, 13.623229, 13.064884, 12.355411, 11.560773,
  10.728960, 9.861975,  8.643612,  6.916021,  5.154769,  3.734940,  2.680051,
  1.925506,  1.408410,  1.042223,  0.767641,  0.565392,  0.420116,  0.310427,
  0.231711,  0.172999,  0.128293,  0.094992,  0.072171,  0.052972,  0.039354,
  0.029555,  0.022857,  0.016832,  0.013297,  0.000000,  0.000000,  0.000000,
  0.000000,  0.000000,  0.000000,  0.000000,  0.000000,  0.000000,  0.000000,
  0.000000,  0.000000,  0.000000,  0.000000,  0.000000,  0.000000,  0.000000,
  0.000000,  0.000000,
};

void av1_model_rd_curvfit(double xqr, double *rate_f, double *distbysse_f) {
  const double x_start = -15.5;
  const double x_end = 16.5;
  const double x_step = 0.5;
  const double epsilon = 1e-6;
  (void)x_end;

  xqr = AOMMAX(xqr, x_start + x_step + epsilon);
  xqr = AOMMIN(xqr, x_end - x_step - epsilon);
  const double x = (xqr - x_start) / x_step;
  const int xi = (int)floor(x);
  const double xo = x - xi;

  assert(xi > 0);

  const double *prate = &interp_rgrid_curv[(xi - 1)];
  const double *pdist = &interp_dgrid_curv[(xi - 1)];
  *rate_f = interp_cubic(prate, xo);
  *distbysse_f = interp_cubic(pdist, xo);
}

static void get_entropy_contexts_plane(BLOCK_SIZE plane_bsize,
                                       const struct macroblockd_plane *pd,
                                       ENTROPY_CONTEXT t_above[MAX_MIB_SIZE],
                                       ENTROPY_CONTEXT t_left[MAX_MIB_SIZE]) {
  const int num_4x4_w = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int num_4x4_h = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  const ENTROPY_CONTEXT *const above = pd->above_context;
  const ENTROPY_CONTEXT *const left = pd->left_context;

  memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
  memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
}

void av1_get_entropy_contexts(BLOCK_SIZE bsize,
                              const struct macroblockd_plane *pd,
                              ENTROPY_CONTEXT t_above[MAX_MIB_SIZE],
                              ENTROPY_CONTEXT t_left[MAX_MIB_SIZE]) {
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
  get_entropy_contexts_plane(plane_bsize, pd, t_above, t_left);
}

void av1_mv_pred(const AV1_COMP *cpi, MACROBLOCK *x, uint8_t *ref_y_buffer,
                 int ref_y_stride, int ref_frame, BLOCK_SIZE block_size) {
  int i;
  int zero_seen = 0;
  int best_sad = INT_MAX;
  int this_sad = INT_MAX;
  int max_mv = 0;
  uint8_t *src_y_ptr = x->plane[0].src.buf;
  uint8_t *ref_y_ptr;
  MV pred_mv[MAX_MV_REF_CANDIDATES + 1];
  int num_mv_refs = 0;
  const MV_REFERENCE_FRAME ref_frames[2] = { ref_frame, NONE_FRAME };
  const int_mv ref_mv =
      av1_get_ref_mv_from_stack(0, ref_frames, 0, x->mbmi_ext);
  const int_mv ref_mv1 =
      av1_get_ref_mv_from_stack(0, ref_frames, 1, x->mbmi_ext);

  pred_mv[num_mv_refs++] = ref_mv.as_mv;
  if (ref_mv.as_int != ref_mv1.as_int) {
    pred_mv[num_mv_refs++] = ref_mv1.as_mv;
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
    }
  }

  // Note the index of the mv that worked best in the reference list.
  x->max_mv_context[ref_frame] = max_mv;
  x->pred_mv_sad[ref_frame] = best_sad;
}

void av1_setup_pred_block(const MACROBLOCKD *xd,
                          struct buf_2d dst[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *scale,
                          const struct scale_factors *scale_uv,
                          const int num_planes) {
  int i;

  dst[0].buf = src->y_buffer;
  dst[0].stride = src->y_stride;
  dst[1].buf = src->u_buffer;
  dst[2].buf = src->v_buffer;
  dst[1].stride = dst[2].stride = src->uv_stride;

  for (i = 0; i < num_planes; ++i) {
    setup_pred_plane(dst + i, xd->mi[0]->sb_type, dst[i].buf,
                     i ? src->uv_crop_width : src->y_crop_width,
                     i ? src->uv_crop_height : src->y_crop_height,
                     dst[i].stride, mi_row, mi_col, i ? scale_uv : scale,
                     xd->plane[i].subsampling_x, xd->plane[i].subsampling_y);
  }
}

int av1_raster_block_offset(BLOCK_SIZE plane_bsize, int raster_block,
                            int stride) {
  const int bw = mi_size_wide_log2[plane_bsize];
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

int av1_get_switchable_rate(const AV1_COMMON *const cm, MACROBLOCK *x,
                            const MACROBLOCKD *xd) {
  if (cm->interp_filter == SWITCHABLE) {
    const MB_MODE_INFO *const mbmi = xd->mi[0];
    int inter_filter_cost = 0;
    int dir;

    for (dir = 0; dir < 2; ++dir) {
      const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
      const InterpFilter filter =
          av1_extract_interp_filter(mbmi->interp_filters, dir);
      inter_filter_cost += x->switchable_interp_costs[ctx][filter];
    }
    return SWITCHABLE_INTERP_RATE_FACTOR * inter_filter_cost;
  } else {
    return 0;
  }
}

void av1_set_rd_speed_thresholds(AV1_COMP *cpi) {
  int i;
  RD_OPT *const rd = &cpi->rd;
  SPEED_FEATURES *const sf = &cpi->sf;

  // Set baseline threshold values.
  for (i = 0; i < MAX_MODES; ++i) rd->thresh_mult[i] = cpi->oxcf.mode == 0;

  if (sf->adaptive_rd_thresh) {
    rd->thresh_mult[THR_NEARESTMV] = 300;
    rd->thresh_mult[THR_NEARESTL2] = 300;
    rd->thresh_mult[THR_NEARESTL3] = 300;
    rd->thresh_mult[THR_NEARESTB] = 300;
    rd->thresh_mult[THR_NEARESTA2] = 300;
    rd->thresh_mult[THR_NEARESTA] = 300;
    rd->thresh_mult[THR_NEARESTG] = 300;
  } else {
    rd->thresh_mult[THR_NEARESTMV] = 0;
    rd->thresh_mult[THR_NEARESTL2] = 0;
    rd->thresh_mult[THR_NEARESTL3] = 0;
    rd->thresh_mult[THR_NEARESTB] = 0;
    rd->thresh_mult[THR_NEARESTA2] = 0;
    rd->thresh_mult[THR_NEARESTA] = 0;
    rd->thresh_mult[THR_NEARESTG] = 0;
  }

  rd->thresh_mult[THR_NEWMV] += 1000;
  rd->thresh_mult[THR_NEWL2] += 1000;
  rd->thresh_mult[THR_NEWL3] += 1000;
  rd->thresh_mult[THR_NEWB] += 1000;
  rd->thresh_mult[THR_NEWA2] = 1000;
  rd->thresh_mult[THR_NEWA] += 1000;
  rd->thresh_mult[THR_NEWG] += 1000;

  rd->thresh_mult[THR_NEARMV] += 1000;
  rd->thresh_mult[THR_NEARL2] += 1000;
  rd->thresh_mult[THR_NEARL3] += 1000;
  rd->thresh_mult[THR_NEARB] += 1000;
  rd->thresh_mult[THR_NEARA2] = 1000;
  rd->thresh_mult[THR_NEARA] += 1000;
  rd->thresh_mult[THR_NEARG] += 1000;

  rd->thresh_mult[THR_GLOBALMV] += 2000;
  rd->thresh_mult[THR_GLOBALL2] += 2000;
  rd->thresh_mult[THR_GLOBALL3] += 2000;
  rd->thresh_mult[THR_GLOBALB] += 2000;
  rd->thresh_mult[THR_GLOBALA2] = 2000;
  rd->thresh_mult[THR_GLOBALG] += 2000;
  rd->thresh_mult[THR_GLOBALA] += 2000;

  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLA] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2A] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3A] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGA] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLB] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2B] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3B] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGB] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLA2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL2A2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTL3A2] += 1000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTGA2] += 1000;

  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLL2] += 2000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLL3] += 2000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTLG] += 2000;
  rd->thresh_mult[THR_COMP_NEAREST_NEARESTBA] += 2000;

  rd->thresh_mult[THR_COMP_NEAR_NEARLA] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLA] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLA] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLA] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLA] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL2A] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2A] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2A] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2A] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL2A] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3A] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3A] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3A] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3A] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3A] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL3A] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARGA] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGA] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGA] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGA] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGA] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALGA] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLB] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLB] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLB] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLB] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLB] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL2B] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2B] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2B] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2B] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL2B] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3B] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3B] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3B] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3B] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3B] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL3B] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARGB] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGB] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGB] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGB] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGB] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALGB] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLA2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLA2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLA2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWLA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARLA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWLA2] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLA2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL2A2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL2A2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL2A2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL2A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL2A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL2A2] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL2A2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARL3A2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWL3A2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTL3A2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWL3A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARL3A2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWL3A2] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALL3A2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARGA2] += 1200;
  rd->thresh_mult[THR_COMP_NEAREST_NEWGA2] += 1500;
  rd->thresh_mult[THR_COMP_NEW_NEARESTGA2] += 1500;
  rd->thresh_mult[THR_COMP_NEAR_NEWGA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEARGA2] += 1700;
  rd->thresh_mult[THR_COMP_NEW_NEWGA2] += 2000;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALGA2] += 2500;

  rd->thresh_mult[THR_COMP_NEAR_NEARLL2] += 1600;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLL2] += 2000;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLL2] += 2000;
  rd->thresh_mult[THR_COMP_NEAR_NEWLL2] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEARLL2] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEWLL2] += 2400;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLL2] += 3200;

  rd->thresh_mult[THR_COMP_NEAR_NEARLL3] += 1600;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLL3] += 2000;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLL3] += 2000;
  rd->thresh_mult[THR_COMP_NEAR_NEWLL3] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEARLL3] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEWLL3] += 2400;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLL3] += 3200;

  rd->thresh_mult[THR_COMP_NEAR_NEARLG] += 1600;
  rd->thresh_mult[THR_COMP_NEAREST_NEWLG] += 2000;
  rd->thresh_mult[THR_COMP_NEW_NEARESTLG] += 2000;
  rd->thresh_mult[THR_COMP_NEAR_NEWLG] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEARLG] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEWLG] += 2400;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALLG] += 3200;

  rd->thresh_mult[THR_COMP_NEAR_NEARBA] += 1600;
  rd->thresh_mult[THR_COMP_NEAREST_NEWBA] += 2000;
  rd->thresh_mult[THR_COMP_NEW_NEARESTBA] += 2000;
  rd->thresh_mult[THR_COMP_NEAR_NEWBA] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEARBA] += 2200;
  rd->thresh_mult[THR_COMP_NEW_NEWBA] += 2400;
  rd->thresh_mult[THR_COMP_GLOBAL_GLOBALBA] += 3200;

  rd->thresh_mult[THR_DC] += 1000;
  rd->thresh_mult[THR_PAETH] += 1000;
  rd->thresh_mult[THR_SMOOTH] += 2000;
  rd->thresh_mult[THR_SMOOTH_V] += 2000;
  rd->thresh_mult[THR_SMOOTH_H] += 2000;
  rd->thresh_mult[THR_H_PRED] += 2000;
  rd->thresh_mult[THR_V_PRED] += 2000;
  rd->thresh_mult[THR_D135_PRED] += 2500;
  rd->thresh_mult[THR_D203_PRED] += 2500;
  rd->thresh_mult[THR_D157_PRED] += 2500;
  rd->thresh_mult[THR_D67_PRED] += 2500;
  rd->thresh_mult[THR_D113_PRED] += 2500;
  rd->thresh_mult[THR_D45_PRED] += 2500;
}

void av1_set_rd_speed_thresholds_sub8x8(AV1_COMP *cpi) {
  static const int thresh_mult[MAX_REFS] = { 2500, 2500, 2500, 2500, 2500,
                                             2500, 2500, 4500, 4500, 4500,
                                             4500, 4500, 4500, 4500, 4500,
                                             4500, 4500, 4500, 4500, 2500 };
  RD_OPT *const rd = &cpi->rd;
  memcpy(rd->thresh_mult_sub8x8, thresh_mult, sizeof(thresh_mult));
}

void av1_update_rd_thresh_fact(const AV1_COMMON *const cm,
                               int (*factor_buf)[MAX_MODES], int rd_thresh,
                               int bsize, int best_mode_index) {
  if (rd_thresh > 0) {
    const int top_mode = MAX_MODES;
    int mode;
    for (mode = 0; mode < top_mode; ++mode) {
      const BLOCK_SIZE min_size = AOMMAX(bsize - 1, BLOCK_4X4);
      const BLOCK_SIZE max_size =
          AOMMIN(bsize + 2, (int)cm->seq_params.sb_size);
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
  const int q = av1_dc_quant_Q3(qindex, qdelta, bit_depth);
  switch (bit_depth) {
    case AOM_BITS_8: return 20 * q;
    case AOM_BITS_10: return 5 * q;
    case AOM_BITS_12: return ROUND_POWER_OF_TWO(5 * q, 2);
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
}
