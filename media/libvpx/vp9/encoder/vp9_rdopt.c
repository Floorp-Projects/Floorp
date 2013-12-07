/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "vp9/common/vp9_pragmas.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vp9/encoder/vp9_treewriter.h"
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_modecosts.h"
#include "vp9/encoder/vp9_encodeintra.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_reconintra.h"
#include "vp9/common/vp9_findnearmv.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_variance.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vpx_mem/vpx_mem.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_entropy.h"
#include "./vp9_rtcd.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_common.h"

#define INVALID_MV 0x80008000

/* Factor to weigh the rate for switchable interp filters */
#define SWITCHABLE_INTERP_RATE_FACTOR 1

#define LAST_FRAME_MODE_MASK    0xFFEDCD60
#define GOLDEN_FRAME_MODE_MASK  0xFFDA3BB0
#define ALT_REF_MODE_MASK       0xFFC648D0

#define MIN_EARLY_TERM_INDEX    3

const MODE_DEFINITION vp9_mode_order[MAX_MODES] = {
  {NEARESTMV, LAST_FRAME,   NONE},
  {NEARESTMV, ALTREF_FRAME, NONE},
  {NEARESTMV, GOLDEN_FRAME, NONE},

  {DC_PRED,   INTRA_FRAME,  NONE},

  {NEWMV,     LAST_FRAME,   NONE},
  {NEWMV,     ALTREF_FRAME, NONE},
  {NEWMV,     GOLDEN_FRAME, NONE},

  {NEARMV,    LAST_FRAME,   NONE},
  {NEARMV,    ALTREF_FRAME, NONE},
  {NEARESTMV, LAST_FRAME,   ALTREF_FRAME},
  {NEARESTMV, GOLDEN_FRAME, ALTREF_FRAME},

  {TM_PRED,   INTRA_FRAME,  NONE},

  {NEARMV,    LAST_FRAME,   ALTREF_FRAME},
  {NEWMV,     LAST_FRAME,   ALTREF_FRAME},
  {NEARMV,    GOLDEN_FRAME, NONE},
  {NEARMV,    GOLDEN_FRAME, ALTREF_FRAME},
  {NEWMV,     GOLDEN_FRAME, ALTREF_FRAME},

  {ZEROMV,    LAST_FRAME,   NONE},
  {ZEROMV,    GOLDEN_FRAME, NONE},
  {ZEROMV,    ALTREF_FRAME, NONE},
  {ZEROMV,    LAST_FRAME,   ALTREF_FRAME},
  {ZEROMV,    GOLDEN_FRAME, ALTREF_FRAME},

  {H_PRED,    INTRA_FRAME,  NONE},
  {V_PRED,    INTRA_FRAME,  NONE},
  {D135_PRED, INTRA_FRAME,  NONE},
  {D207_PRED, INTRA_FRAME,  NONE},
  {D153_PRED, INTRA_FRAME,  NONE},
  {D63_PRED,  INTRA_FRAME,  NONE},
  {D117_PRED, INTRA_FRAME,  NONE},
  {D45_PRED,  INTRA_FRAME,  NONE},
};

const REF_DEFINITION vp9_ref_order[MAX_REFS] = {
  {LAST_FRAME,   NONE},
  {GOLDEN_FRAME, NONE},
  {ALTREF_FRAME, NONE},
  {LAST_FRAME,   ALTREF_FRAME},
  {GOLDEN_FRAME, ALTREF_FRAME},
  {INTRA_FRAME,  NONE},
};

// The baseline rd thresholds for breaking out of the rd loop for
// certain modes are assumed to be based on 8x8 blocks.
// This table is used to correct for blocks size.
// The factors here are << 2 (2 = x0.5, 32 = x8 etc).
static int rd_thresh_block_size_factor[BLOCK_SIZES] =
  {2, 3, 3, 4, 6, 6, 8, 12, 12, 16, 24, 24, 32};

#define RD_THRESH_MAX_FACT 64
#define RD_THRESH_INC      1
#define RD_THRESH_POW      1.25
#define RD_MULT_EPB_RATIO  64

#define MV_COST_WEIGHT      108
#define MV_COST_WEIGHT_SUB  120

static void fill_token_costs(vp9_coeff_cost *c,
                             vp9_coeff_probs_model (*p)[BLOCK_TYPES]) {
  int i, j, k, l;
  TX_SIZE t;
  for (t = TX_4X4; t <= TX_32X32; t++)
    for (i = 0; i < BLOCK_TYPES; i++)
      for (j = 0; j < REF_TYPES; j++)
        for (k = 0; k < COEF_BANDS; k++)
          for (l = 0; l < PREV_COEF_CONTEXTS; l++) {
            vp9_prob probs[ENTROPY_NODES];
            vp9_model_to_full_probs(p[t][i][j][k][l], probs);
            vp9_cost_tokens((int *)c[t][i][j][k][0][l], probs,
                            vp9_coef_tree);
            vp9_cost_tokens_skip((int *)c[t][i][j][k][1][l], probs,
                                 vp9_coef_tree);
            assert(c[t][i][j][k][0][l][DCT_EOB_TOKEN] ==
                   c[t][i][j][k][1][l][DCT_EOB_TOKEN]);
          }
}

static const int rd_iifactor[32] = {
  4, 4, 3, 2, 1, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
};

// 3* dc_qlookup[Q]*dc_qlookup[Q];

/* values are now correlated to quantizer */
static int sad_per_bit16lut[QINDEX_RANGE];
static int sad_per_bit4lut[QINDEX_RANGE];

void vp9_init_me_luts() {
  int i;

  // Initialize the sad lut tables using a formulaic calculation for now
  // This is to make it easier to resolve the impact of experimental changes
  // to the quantizer tables.
  for (i = 0; i < QINDEX_RANGE; i++) {
    sad_per_bit16lut[i] =
      (int)((0.0418 * vp9_convert_qindex_to_q(i)) + 2.4107);
    sad_per_bit4lut[i] = (int)(0.063 * vp9_convert_qindex_to_q(i) + 2.742);
  }
}

int vp9_compute_rd_mult(VP9_COMP *cpi, int qindex) {
  const int q = vp9_dc_quant(qindex, 0);
  // TODO(debargha): Adjust the function below
  int rdmult = 88 * q * q / 25;
  if (cpi->pass == 2 && (cpi->common.frame_type != KEY_FRAME)) {
    if (cpi->twopass.next_iiratio > 31)
      rdmult += (rdmult * rd_iifactor[31]) >> 4;
    else
      rdmult += (rdmult * rd_iifactor[cpi->twopass.next_iiratio]) >> 4;
  }
  return rdmult;
}

static int compute_rd_thresh_factor(int qindex) {
  int q;
  // TODO(debargha): Adjust the function below
  q = (int)(pow(vp9_dc_quant(qindex, 0) / 4.0, RD_THRESH_POW) * 5.12);
  if (q < 8)
    q = 8;
  return q;
}

void vp9_initialize_me_consts(VP9_COMP *cpi, int qindex) {
  cpi->mb.sadperbit16 = sad_per_bit16lut[qindex];
  cpi->mb.sadperbit4 = sad_per_bit4lut[qindex];
}

static void set_block_thresholds(VP9_COMP *cpi) {
  int i, bsize, segment_id;
  VP9_COMMON *cm = &cpi->common;

  for (segment_id = 0; segment_id < MAX_SEGMENTS; ++segment_id) {
    int q;
    int segment_qindex = vp9_get_qindex(&cm->seg, segment_id, cm->base_qindex);
    segment_qindex = clamp(segment_qindex + cm->y_dc_delta_q, 0, MAXQ);
    q = compute_rd_thresh_factor(segment_qindex);

    for (bsize = 0; bsize < BLOCK_SIZES; ++bsize) {
      // Threshold here seem unecessarily harsh but fine given actual
      // range of values used for cpi->sf.thresh_mult[]
      int thresh_max = INT_MAX / (q * rd_thresh_block_size_factor[bsize]);

      for (i = 0; i < MAX_MODES; ++i) {
        if (cpi->sf.thresh_mult[i] < thresh_max) {
          cpi->rd_threshes[segment_id][bsize][i] =
              cpi->sf.thresh_mult[i] * q *
              rd_thresh_block_size_factor[bsize] / 4;
        } else {
          cpi->rd_threshes[segment_id][bsize][i] = INT_MAX;
        }
      }

      for (i = 0; i < MAX_REFS; ++i) {
        if (cpi->sf.thresh_mult_sub8x8[i] < thresh_max) {
          cpi->rd_thresh_sub8x8[segment_id][bsize][i] =
              cpi->sf.thresh_mult_sub8x8[i] * q *
              rd_thresh_block_size_factor[bsize] / 4;
        } else {
          cpi->rd_thresh_sub8x8[segment_id][bsize][i] = INT_MAX;
        }
      }
    }
  }
}

void vp9_initialize_rd_consts(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int qindex, i;

  vp9_clear_system_state();  // __asm emms;

  // Further tests required to see if optimum is different
  // for key frames, golden frames and arf frames.
  // if (cpi->common.refresh_golden_frame ||
  //     cpi->common.refresh_alt_ref_frame)
  qindex = clamp(cm->base_qindex + cm->y_dc_delta_q, 0, MAXQ);

  cpi->RDDIV = RDDIV_BITS;  // in bits (to multiply D by 128)
  cpi->RDMULT = vp9_compute_rd_mult(cpi, qindex);

  cpi->mb.errorperbit = cpi->RDMULT / RD_MULT_EPB_RATIO;
  cpi->mb.errorperbit += (cpi->mb.errorperbit == 0);

  vp9_set_speed_features(cpi);

  cpi->mb.select_txfm_size = (cpi->sf.tx_size_search_method == USE_LARGESTALL &&
                              cm->frame_type != KEY_FRAME) ?
                             0 : 1;

  set_block_thresholds(cpi);

  fill_token_costs(cpi->mb.token_costs, cm->fc.coef_probs);

  for (i = 0; i < PARTITION_CONTEXTS; i++)
    vp9_cost_tokens(cpi->mb.partition_cost[i], get_partition_probs(cm, i),
                    vp9_partition_tree);

  /*rough estimate for costing*/
  vp9_init_mode_costs(cpi);

  if (!frame_is_intra_only(cm)) {
    vp9_build_nmv_cost_table(
        cpi->mb.nmvjointcost,
        cm->allow_high_precision_mv ? cpi->mb.nmvcost_hp : cpi->mb.nmvcost,
        &cm->fc.nmvc,
        cm->allow_high_precision_mv, 1, 1);

    for (i = 0; i < INTER_MODE_CONTEXTS; i++) {
      MB_PREDICTION_MODE m;

      for (m = NEARESTMV; m < MB_MODE_COUNT; m++)
        cpi->mb.inter_mode_cost[i][INTER_OFFSET(m)] =
            cost_token(vp9_inter_mode_tree,
                       cm->fc.inter_mode_probs[i],
                       &vp9_inter_mode_encodings[INTER_OFFSET(m)]);
    }
  }
}

static INLINE void linear_interpolate2(double x, int ntab, int inv_step,
                                       const double *tab1, const double *tab2,
                                       double *v1, double *v2) {
  double y = x * inv_step;
  int d = (int) y;
  if (d >= ntab - 1) {
    *v1 = tab1[ntab - 1];
    *v2 = tab2[ntab - 1];
  } else {
    double a = y - d;
    *v1 = tab1[d] * (1 - a) + tab1[d + 1] * a;
    *v2 = tab2[d] * (1 - a) + tab2[d + 1] * a;
  }
}

static void model_rd_norm(double x, double *R, double *D) {
  static const int inv_tab_step = 8;
  static const int tab_size = 120;
  // NOTE: The tables below must be of the same size
  //
  // Normalized rate
  // This table models the rate for a Laplacian source
  // source with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expression is:
  // Rn(x) = H(sqrt(r)) + sqrt(r)*[1 + H(r)/(1 - r)],
  // where r = exp(-sqrt(2) * x) and x = qpstep / sqrt(variance),
  // and H(x) is the binary entropy function.
  static const double rate_tab[] = {
    64.00, 4.944, 3.949, 3.372, 2.966, 2.655, 2.403, 2.194,
    2.014, 1.858, 1.720, 1.596, 1.485, 1.384, 1.291, 1.206,
    1.127, 1.054, 0.986, 0.923, 0.863, 0.808, 0.756, 0.708,
    0.662, 0.619, 0.579, 0.541, 0.506, 0.473, 0.442, 0.412,
    0.385, 0.359, 0.335, 0.313, 0.291, 0.272, 0.253, 0.236,
    0.220, 0.204, 0.190, 0.177, 0.165, 0.153, 0.142, 0.132,
    0.123, 0.114, 0.106, 0.099, 0.091, 0.085, 0.079, 0.073,
    0.068, 0.063, 0.058, 0.054, 0.050, 0.047, 0.043, 0.040,
    0.037, 0.034, 0.032, 0.029, 0.027, 0.025, 0.023, 0.022,
    0.020, 0.019, 0.017, 0.016, 0.015, 0.014, 0.013, 0.012,
    0.011, 0.010, 0.009, 0.008, 0.008, 0.007, 0.007, 0.006,
    0.006, 0.005, 0.005, 0.005, 0.004, 0.004, 0.004, 0.003,
    0.003, 0.003, 0.003, 0.002, 0.002, 0.002, 0.002, 0.002,
    0.002, 0.001, 0.001, 0.001, 0.001, 0.001, 0.001, 0.001,
    0.001, 0.001, 0.001, 0.001, 0.001, 0.001, 0.001, 0.000,
  };
  // Normalized distortion
  // This table models the normalized distortion for a Laplacian source
  // source with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expression is:
  // Dn(x) = 1 - 1/sqrt(2) * x / sinh(x/sqrt(2))
  // where x = qpstep / sqrt(variance)
  // Note the actual distortion is Dn * variance.
  static const double dist_tab[] = {
    0.000, 0.001, 0.005, 0.012, 0.021, 0.032, 0.045, 0.061,
    0.079, 0.098, 0.119, 0.142, 0.166, 0.190, 0.216, 0.242,
    0.269, 0.296, 0.324, 0.351, 0.378, 0.405, 0.432, 0.458,
    0.484, 0.509, 0.534, 0.557, 0.580, 0.603, 0.624, 0.645,
    0.664, 0.683, 0.702, 0.719, 0.735, 0.751, 0.766, 0.780,
    0.794, 0.807, 0.819, 0.830, 0.841, 0.851, 0.861, 0.870,
    0.878, 0.886, 0.894, 0.901, 0.907, 0.913, 0.919, 0.925,
    0.930, 0.935, 0.939, 0.943, 0.947, 0.951, 0.954, 0.957,
    0.960, 0.963, 0.966, 0.968, 0.971, 0.973, 0.975, 0.976,
    0.978, 0.980, 0.981, 0.982, 0.984, 0.985, 0.986, 0.987,
    0.988, 0.989, 0.990, 0.990, 0.991, 0.992, 0.992, 0.993,
    0.993, 0.994, 0.994, 0.995, 0.995, 0.996, 0.996, 0.996,
    0.996, 0.997, 0.997, 0.997, 0.997, 0.998, 0.998, 0.998,
    0.998, 0.998, 0.998, 0.999, 0.999, 0.999, 0.999, 0.999,
    0.999, 0.999, 0.999, 0.999, 0.999, 0.999, 0.999, 1.000,
  };
  /*
  assert(sizeof(rate_tab) == tab_size * sizeof(rate_tab[0]);
  assert(sizeof(dist_tab) == tab_size * sizeof(dist_tab[0]);
  assert(sizeof(rate_tab) == sizeof(dist_tab));
  */
  assert(x >= 0.0);
  linear_interpolate2(x, tab_size, inv_tab_step,
                      rate_tab, dist_tab, R, D);
}

static void model_rd_from_var_lapndz(int var, int n, int qstep,
                                     int *rate, int64_t *dist) {
  // This function models the rate and distortion for a Laplacian
  // source with given variance when quantized with a uniform quantizer
  // with given stepsize. The closed form expressions are in:
  // Hang and Chen, "Source Model for transform video coder and its
  // application - Part I: Fundamental Theory", IEEE Trans. Circ.
  // Sys. for Video Tech., April 1997.
  vp9_clear_system_state();
  if (var == 0 || n == 0) {
    *rate = 0;
    *dist = 0;
  } else {
    double D, R;
    double s2 = (double) var / n;
    double x = qstep / sqrt(s2);
    model_rd_norm(x, &R, &D);
    *rate = (int)((n << 8) * R + 0.5);
    *dist = (int)(var * D + 0.5);
  }
  vp9_clear_system_state();
}

static void model_rd_for_sb(VP9_COMP *cpi, BLOCK_SIZE bsize,
                            MACROBLOCK *x, MACROBLOCKD *xd,
                            int *out_rate_sum, int64_t *out_dist_sum) {
  // Note our transform coeffs are 8 times an orthogonal transform.
  // Hence quantizer step is also 8 times. To get effective quantizer
  // we need to divide by 8 before sending to modeling function.
  int i, rate_sum = 0, dist_sum = 0;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    struct macroblock_plane *const p = &x->plane[i];
    struct macroblockd_plane *const pd = &xd->plane[i];
    const BLOCK_SIZE bs = get_plane_block_size(bsize, pd);
    unsigned int sse;
    int rate;
    int64_t dist;
    (void) cpi->fn_ptr[bs].vf(p->src.buf, p->src.stride,
                              pd->dst.buf, pd->dst.stride, &sse);
    // sse works better than var, since there is no dc prediction used
    model_rd_from_var_lapndz(sse, 1 << num_pels_log2_lookup[bs],
                             pd->dequant[1] >> 3, &rate, &dist);

    rate_sum += rate;
    dist_sum += (int)dist;
  }

  *out_rate_sum = rate_sum;
  *out_dist_sum = dist_sum << 4;
}

static void model_rd_for_sb_y_tx(VP9_COMP *cpi, BLOCK_SIZE bsize,
                                 TX_SIZE tx_size,
                                 MACROBLOCK *x, MACROBLOCKD *xd,
                                 int *out_rate_sum, int64_t *out_dist_sum,
                                 int *out_skip) {
  int j, k;
  BLOCK_SIZE bs;
  struct macroblock_plane *const p = &x->plane[0];
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int width = 4 << num_4x4_blocks_wide_lookup[bsize];
  const int height = 4 << num_4x4_blocks_high_lookup[bsize];
  int rate_sum = 0;
  int64_t dist_sum = 0;
  const int t = 4 << tx_size;

  if (tx_size == TX_4X4) {
    bs = BLOCK_4X4;
  } else if (tx_size == TX_8X8) {
    bs = BLOCK_8X8;
  } else if (tx_size == TX_16X16) {
    bs = BLOCK_16X16;
  } else if (tx_size == TX_32X32) {
    bs = BLOCK_32X32;
  } else {
    assert(0);
  }

  *out_skip = 1;
  for (j = 0; j < height; j += t) {
    for (k = 0; k < width; k += t) {
      int rate;
      int64_t dist;
      unsigned int sse;
      cpi->fn_ptr[bs].vf(&p->src.buf[j * p->src.stride + k], p->src.stride,
                         &pd->dst.buf[j * pd->dst.stride + k], pd->dst.stride,
                         &sse);
      // sse works better than var, since there is no dc prediction used
      model_rd_from_var_lapndz(sse, t * t, pd->dequant[1] >> 3, &rate, &dist);
      rate_sum += rate;
      dist_sum += dist;
      *out_skip &= (rate < 1024);
    }
  }

  *out_rate_sum = rate_sum;
  *out_dist_sum = dist_sum << 4;
}

int64_t vp9_block_error_c(int16_t *coeff, int16_t *dqcoeff,
                          intptr_t block_size, int64_t *ssz) {
  int i;
  int64_t error = 0, sqcoeff = 0;

  for (i = 0; i < block_size; i++) {
    int this_diff = coeff[i] - dqcoeff[i];
    error += (unsigned)this_diff * this_diff;
    sqcoeff += (unsigned) coeff[i] * coeff[i];
  }

  *ssz = sqcoeff;
  return error;
}

/* The trailing '0' is a terminator which is used inside cost_coeffs() to
 * decide whether to include cost of a trailing EOB node or not (i.e. we
 * can skip this if the last coefficient in this transform block, e.g. the
 * 16th coefficient in a 4x4 block or the 64th coefficient in a 8x8 block,
 * were non-zero). */
static const int16_t band_counts[TX_SIZES][8] = {
  { 1, 2, 3, 4,  3,   16 - 13, 0 },
  { 1, 2, 3, 4, 11,   64 - 21, 0 },
  { 1, 2, 3, 4, 11,  256 - 21, 0 },
  { 1, 2, 3, 4, 11, 1024 - 21, 0 },
};

static INLINE int cost_coeffs(MACROBLOCK *x,
                              int plane, int block,
                              ENTROPY_CONTEXT *A, ENTROPY_CONTEXT *L,
                              TX_SIZE tx_size,
                              const int16_t *scan, const int16_t *nb) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  struct macroblockd_plane *pd = &xd->plane[plane];
  const PLANE_TYPE type = pd->plane_type;
  const int16_t *band_count = &band_counts[tx_size][1];
  const int eob = pd->eobs[block];
  const int16_t *const qcoeff_ptr = BLOCK_OFFSET(pd->qcoeff, block);
  const int ref = mbmi->ref_frame[0] != INTRA_FRAME;
  unsigned int (*token_costs)[2][PREV_COEF_CONTEXTS][MAX_ENTROPY_TOKENS] =
                   x->token_costs[tx_size][type][ref];
  const ENTROPY_CONTEXT above_ec = !!*A, left_ec = !!*L;
  uint8_t *p_tok = x->token_cache;
  int pt = combine_entropy_contexts(above_ec, left_ec);
  int c, cost;

  // Check for consistency of tx_size with mode info
  assert(type == PLANE_TYPE_Y_WITH_DC ? mbmi->tx_size == tx_size
                                      : get_uv_tx_size(mbmi) == tx_size);

  if (eob == 0) {
    // single eob token
    cost = token_costs[0][0][pt][DCT_EOB_TOKEN];
    c = 0;
  } else {
    int band_left = *band_count++;

    // dc token
    int v = qcoeff_ptr[0];
    int prev_t = vp9_dct_value_tokens_ptr[v].token;
    cost = (*token_costs)[0][pt][prev_t] + vp9_dct_value_cost_ptr[v];
    p_tok[0] = vp9_pt_energy_class[prev_t];
    ++token_costs;

    // ac tokens
    for (c = 1; c < eob; c++) {
      const int rc = scan[c];
      int t;

      v = qcoeff_ptr[rc];
      t = vp9_dct_value_tokens_ptr[v].token;
      pt = get_coef_context(nb, p_tok, c);
      cost += (*token_costs)[!prev_t][pt][t] + vp9_dct_value_cost_ptr[v];
      p_tok[rc] = vp9_pt_energy_class[t];
      prev_t = t;
      if (!--band_left) {
        band_left = *band_count++;
        ++token_costs;
      }
    }

    // eob token
    if (band_left) {
      pt = get_coef_context(nb, p_tok, c);
      cost += (*token_costs)[0][pt][DCT_EOB_TOKEN];
    }
  }

  // is eob first coefficient;
  *A = *L = (c > 0);

  return cost;
}

static void dist_block(int plane, int block, TX_SIZE tx_size, void *arg) {
  const int ss_txfrm_size = tx_size << 1;
  struct rdcost_block_args* args = arg;
  MACROBLOCK* const x = args->x;
  MACROBLOCKD* const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int64_t this_sse;
  int shift = args->tx_size == TX_32X32 ? 0 : 2;
  int16_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  int16_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  args->dist = vp9_block_error(coeff, dqcoeff, 16 << ss_txfrm_size,
                               &this_sse) >> shift;
  args->sse  = this_sse >> shift;

  if (x->skip_encode &&
      xd->mi_8x8[0]->mbmi.ref_frame[0] == INTRA_FRAME) {
    // TODO(jingning): tune the model to better capture the distortion.
    int64_t p = (pd->dequant[1] * pd->dequant[1] *
                    (1 << ss_txfrm_size)) >> (shift + 2);
    args->dist += (p >> 4);
    args->sse  += p;
  }
}

static void rate_block(int plane, int block, BLOCK_SIZE plane_bsize,
                       TX_SIZE tx_size, void *arg) {
  struct rdcost_block_args* args = arg;

  int x_idx, y_idx;
  txfrm_block_to_raster_xy(plane_bsize, args->tx_size, block, &x_idx, &y_idx);

  args->rate = cost_coeffs(args->x, plane, block, args->t_above + x_idx,
                           args->t_left + y_idx, args->tx_size,
                           args->scan, args->nb);
}

static void block_yrd_txfm(int plane, int block, BLOCK_SIZE plane_bsize,
                           TX_SIZE tx_size, void *arg) {
  struct rdcost_block_args *args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct encode_b_args encode_args = {x, NULL};
  int64_t rd1, rd2, rd;

  if (args->skip)
    return;

  if (!is_inter_block(&xd->mi_8x8[0]->mbmi))
    vp9_encode_block_intra(plane, block, plane_bsize, tx_size, &encode_args);
  else
    vp9_xform_quant(plane, block, plane_bsize, tx_size, &encode_args);

  dist_block(plane, block, tx_size, args);
  rate_block(plane, block, plane_bsize, tx_size, args);
  rd1 = RDCOST(x->rdmult, x->rddiv, args->rate, args->dist);
  rd2 = RDCOST(x->rdmult, x->rddiv, 0, args->sse);

  // TODO(jingning): temporarily enabled only for luma component
  rd = MIN(rd1, rd2);
  if (!xd->lossless && plane == 0)
    x->zcoeff_blk[tx_size][block] = rd1 > rd2 || !xd->plane[plane].eobs[block];

  args->this_rate += args->rate;
  args->this_dist += args->dist;
  args->this_sse  += args->sse;
  args->this_rd += rd;

  if (args->this_rd > args->best_rd) {
    args->skip = 1;
    return;
  }
}

void vp9_get_entropy_contexts(TX_SIZE tx_size,
    ENTROPY_CONTEXT t_above[16], ENTROPY_CONTEXT t_left[16],
    const ENTROPY_CONTEXT *above, const ENTROPY_CONTEXT *left,
    int num_4x4_w, int num_4x4_h) {
  int i;
  switch (tx_size) {
    case TX_4X4:
      vpx_memcpy(t_above, above, sizeof(ENTROPY_CONTEXT) * num_4x4_w);
      vpx_memcpy(t_left, left, sizeof(ENTROPY_CONTEXT) * num_4x4_h);
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
    default:
      assert(!"Invalid transform size.");
  }
}

static void init_rdcost_stack(MACROBLOCK *x, TX_SIZE tx_size,
                              const int num_4x4_w, const int num_4x4_h,
                              const int64_t ref_rdcost,
                              struct rdcost_block_args *arg) {
  vpx_memset(arg, 0, sizeof(struct rdcost_block_args));
  arg->x = x;
  arg->tx_size = tx_size;
  arg->bw = num_4x4_w;
  arg->bh = num_4x4_h;
  arg->best_rd = ref_rdcost;
}

static void txfm_rd_in_plane(MACROBLOCK *x,
                             struct rdcost_block_args *rd_stack,
                             int *rate, int64_t *distortion,
                             int *skippable, int64_t *sse,
                             int64_t ref_best_rd, int plane,
                             BLOCK_SIZE bsize, TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE bs = get_plane_block_size(bsize, pd);
  const int num_4x4_w = num_4x4_blocks_wide_lookup[bs];
  const int num_4x4_h = num_4x4_blocks_high_lookup[bs];

  init_rdcost_stack(x, tx_size, num_4x4_w, num_4x4_h,
                    ref_best_rd, rd_stack);
  if (plane == 0)
    xd->mi_8x8[0]->mbmi.tx_size = tx_size;

  vp9_get_entropy_contexts(tx_size, rd_stack->t_above, rd_stack->t_left,
                           pd->above_context, pd->left_context,
                           num_4x4_w, num_4x4_h);

  get_scan(xd, tx_size, pd->plane_type, 0, &rd_stack->scan, &rd_stack->nb);

  foreach_transformed_block_in_plane(xd, bsize, plane,
                                     block_yrd_txfm, rd_stack);
  if (rd_stack->skip) {
    *rate       = INT_MAX;
    *distortion = INT64_MAX;
    *sse        = INT64_MAX;
    *skippable  = 0;
  } else {
    *distortion = rd_stack->this_dist;
    *rate       = rd_stack->this_rate;
    *sse        = rd_stack->this_sse;
    *skippable  = vp9_is_skippable_in_plane(xd, bsize, plane);
  }
}

static void choose_largest_txfm_size(VP9_COMP *cpi, MACROBLOCK *x,
                                     int *rate, int64_t *distortion,
                                     int *skip, int64_t *sse,
                                     int64_t ref_best_rd,
                                     BLOCK_SIZE bs) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  VP9_COMMON *const cm = &cpi->common;
  const TX_SIZE largest_tx_size = tx_mode_to_biggest_tx_size[cm->tx_mode];
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;

  mbmi->tx_size = MIN(max_tx_size, largest_tx_size);

  txfm_rd_in_plane(x, &cpi->rdcost_stack, rate, distortion, skip,
                   &sse[mbmi->tx_size], ref_best_rd, 0, bs,
                   mbmi->tx_size);
  cpi->tx_stepdown_count[0]++;
}

static void choose_txfm_size_from_rd(VP9_COMP *cpi, MACROBLOCK *x,
                                     int (*r)[2], int *rate,
                                     int64_t *d, int64_t *distortion,
                                     int *s, int *skip,
                                     int64_t tx_cache[TX_MODES],
                                     BLOCK_SIZE bs) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;
  vp9_prob skip_prob = vp9_get_pred_prob_mbskip(cm, xd);
  int64_t rd[TX_SIZES][2];
  int n, m;
  int s0, s1;

  const vp9_prob *tx_probs = get_tx_probs2(max_tx_size, xd, &cm->fc.tx_probs);

  for (n = TX_4X4; n <= max_tx_size; n++) {
    r[n][1] = r[n][0];
    if (r[n][0] == INT_MAX)
      continue;
    for (m = 0; m <= n - (n == max_tx_size); m++) {
      if (m == n)
        r[n][1] += vp9_cost_zero(tx_probs[m]);
      else
        r[n][1] += vp9_cost_one(tx_probs[m]);
    }
  }

  assert(skip_prob > 0);
  s0 = vp9_cost_bit(skip_prob, 0);
  s1 = vp9_cost_bit(skip_prob, 1);

  for (n = TX_4X4; n <= max_tx_size; n++) {
    if (d[n] == INT64_MAX) {
      rd[n][0] = rd[n][1] = INT64_MAX;
      continue;
    }
    if (s[n]) {
      rd[n][0] = rd[n][1] = RDCOST(x->rdmult, x->rddiv, s1, d[n]);
    } else {
      rd[n][0] = RDCOST(x->rdmult, x->rddiv, r[n][0] + s0, d[n]);
      rd[n][1] = RDCOST(x->rdmult, x->rddiv, r[n][1] + s0, d[n]);
    }
  }

  if (max_tx_size == TX_32X32 &&
      (cm->tx_mode == ALLOW_32X32 ||
       (cm->tx_mode == TX_MODE_SELECT &&
        rd[TX_32X32][1] < rd[TX_16X16][1] && rd[TX_32X32][1] < rd[TX_8X8][1] &&
        rd[TX_32X32][1] < rd[TX_4X4][1]))) {
    mbmi->tx_size = TX_32X32;
  } else if (max_tx_size >= TX_16X16 &&
             (cm->tx_mode == ALLOW_16X16 ||
              cm->tx_mode == ALLOW_32X32 ||
              (cm->tx_mode == TX_MODE_SELECT &&
               rd[TX_16X16][1] < rd[TX_8X8][1] &&
               rd[TX_16X16][1] < rd[TX_4X4][1]))) {
    mbmi->tx_size = TX_16X16;
  } else if (cm->tx_mode == ALLOW_8X8 ||
             cm->tx_mode == ALLOW_16X16 ||
             cm->tx_mode == ALLOW_32X32 ||
           (cm->tx_mode == TX_MODE_SELECT && rd[TX_8X8][1] < rd[TX_4X4][1])) {
    mbmi->tx_size = TX_8X8;
  } else {
    mbmi->tx_size = TX_4X4;
  }

  *distortion = d[mbmi->tx_size];
  *rate       = r[mbmi->tx_size][cm->tx_mode == TX_MODE_SELECT];
  *skip       = s[mbmi->tx_size];

  tx_cache[ONLY_4X4] = rd[TX_4X4][0];
  tx_cache[ALLOW_8X8] = rd[TX_8X8][0];
  tx_cache[ALLOW_16X16] = rd[MIN(max_tx_size, TX_16X16)][0];
  tx_cache[ALLOW_32X32] = rd[MIN(max_tx_size, TX_32X32)][0];
  if (max_tx_size == TX_32X32 &&
      rd[TX_32X32][1] < rd[TX_16X16][1] && rd[TX_32X32][1] < rd[TX_8X8][1] &&
      rd[TX_32X32][1] < rd[TX_4X4][1])
    tx_cache[TX_MODE_SELECT] = rd[TX_32X32][1];
  else if (max_tx_size >= TX_16X16 &&
           rd[TX_16X16][1] < rd[TX_8X8][1] && rd[TX_16X16][1] < rd[TX_4X4][1])
    tx_cache[TX_MODE_SELECT] = rd[TX_16X16][1];
  else
    tx_cache[TX_MODE_SELECT] = rd[TX_4X4][1] < rd[TX_8X8][1] ?
                                 rd[TX_4X4][1] : rd[TX_8X8][1];

  if (max_tx_size == TX_32X32 &&
      rd[TX_32X32][1] < rd[TX_16X16][1] &&
      rd[TX_32X32][1] < rd[TX_8X8][1] &&
      rd[TX_32X32][1] < rd[TX_4X4][1]) {
    cpi->tx_stepdown_count[0]++;
  } else if (max_tx_size >= TX_16X16 &&
             rd[TX_16X16][1] < rd[TX_8X8][1] &&
             rd[TX_16X16][1] < rd[TX_4X4][1]) {
    cpi->tx_stepdown_count[max_tx_size - TX_16X16]++;
  } else if (rd[TX_8X8][1] < rd[TX_4X4][1]) {
    cpi->tx_stepdown_count[max_tx_size - TX_8X8]++;
  } else {
    cpi->tx_stepdown_count[max_tx_size - TX_4X4]++;
  }
}

static void choose_txfm_size_from_modelrd(VP9_COMP *cpi, MACROBLOCK *x,
                                          int (*r)[2], int *rate,
                                          int64_t *d, int64_t *distortion,
                                          int *s, int *skip, int64_t *sse,
                                          int64_t ref_best_rd,
                                          BLOCK_SIZE bs) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;
  vp9_prob skip_prob = vp9_get_pred_prob_mbskip(cm, xd);
  int64_t rd[TX_SIZES][2];
  int n, m;
  int s0, s1;
  double scale_rd[TX_SIZES] = {1.73, 1.44, 1.20, 1.00};
  // double scale_r[TX_SIZES] = {2.82, 2.00, 1.41, 1.00};

  const vp9_prob *tx_probs = get_tx_probs2(max_tx_size, xd, &cm->fc.tx_probs);

  // for (n = TX_4X4; n <= max_txfm_size; n++)
  //   r[n][0] = (r[n][0] * scale_r[n]);

  for (n = TX_4X4; n <= max_tx_size; n++) {
    r[n][1] = r[n][0];
    for (m = 0; m <= n - (n == max_tx_size); m++) {
      if (m == n)
        r[n][1] += vp9_cost_zero(tx_probs[m]);
      else
        r[n][1] += vp9_cost_one(tx_probs[m]);
    }
  }

  assert(skip_prob > 0);
  s0 = vp9_cost_bit(skip_prob, 0);
  s1 = vp9_cost_bit(skip_prob, 1);

  for (n = TX_4X4; n <= max_tx_size; n++) {
    if (s[n]) {
      rd[n][0] = rd[n][1] = RDCOST(x->rdmult, x->rddiv, s1, d[n]);
    } else {
      rd[n][0] = RDCOST(x->rdmult, x->rddiv, r[n][0] + s0, d[n]);
      rd[n][1] = RDCOST(x->rdmult, x->rddiv, r[n][1] + s0, d[n]);
    }
  }
  for (n = TX_4X4; n <= max_tx_size; n++) {
    rd[n][0] = (int64_t)(scale_rd[n] * rd[n][0]);
    rd[n][1] = (int64_t)(scale_rd[n] * rd[n][1]);
  }

  if (max_tx_size == TX_32X32 &&
      (cm->tx_mode == ALLOW_32X32 ||
       (cm->tx_mode == TX_MODE_SELECT &&
        rd[TX_32X32][1] <= rd[TX_16X16][1] &&
        rd[TX_32X32][1] <= rd[TX_8X8][1] &&
        rd[TX_32X32][1] <= rd[TX_4X4][1]))) {
    mbmi->tx_size = TX_32X32;
  } else if (max_tx_size >= TX_16X16 &&
             (cm->tx_mode == ALLOW_16X16 ||
              cm->tx_mode == ALLOW_32X32 ||
              (cm->tx_mode == TX_MODE_SELECT &&
               rd[TX_16X16][1] <= rd[TX_8X8][1] &&
               rd[TX_16X16][1] <= rd[TX_4X4][1]))) {
    mbmi->tx_size = TX_16X16;
  } else if (cm->tx_mode == ALLOW_8X8 ||
             cm->tx_mode == ALLOW_16X16 ||
             cm->tx_mode == ALLOW_32X32 ||
           (cm->tx_mode == TX_MODE_SELECT &&
            rd[TX_8X8][1] <= rd[TX_4X4][1])) {
    mbmi->tx_size = TX_8X8;
  } else {
    mbmi->tx_size = TX_4X4;
  }

  // Actually encode using the chosen mode if a model was used, but do not
  // update the r, d costs
  txfm_rd_in_plane(x, &cpi->rdcost_stack, rate, distortion, skip,
                   &sse[mbmi->tx_size], ref_best_rd, 0, bs, mbmi->tx_size);

  if (max_tx_size == TX_32X32 &&
      rd[TX_32X32][1] <= rd[TX_16X16][1] &&
      rd[TX_32X32][1] <= rd[TX_8X8][1] &&
      rd[TX_32X32][1] <= rd[TX_4X4][1]) {
    cpi->tx_stepdown_count[0]++;
  } else if (max_tx_size >= TX_16X16 &&
             rd[TX_16X16][1] <= rd[TX_8X8][1] &&
             rd[TX_16X16][1] <= rd[TX_4X4][1]) {
    cpi->tx_stepdown_count[max_tx_size - TX_16X16]++;
  } else if (rd[TX_8X8][1] <= rd[TX_4X4][1]) {
    cpi->tx_stepdown_count[max_tx_size - TX_8X8]++;
  } else {
    cpi->tx_stepdown_count[max_tx_size - TX_4X4]++;
  }
}

static void super_block_yrd(VP9_COMP *cpi,
                            MACROBLOCK *x, int *rate, int64_t *distortion,
                            int *skip, int64_t *psse, BLOCK_SIZE bs,
                            int64_t txfm_cache[TX_MODES],
                            int64_t ref_best_rd) {
  int r[TX_SIZES][2], s[TX_SIZES];
  int64_t d[TX_SIZES], sse[TX_SIZES];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;
  struct rdcost_block_args *rdcost_stack = &cpi->rdcost_stack;
  const int b_inter_mode = is_inter_block(mbmi);

  assert(bs == mbmi->sb_type);
  if (b_inter_mode)
    vp9_subtract_sby(x, bs);

  if (cpi->sf.tx_size_search_method == USE_LARGESTALL ||
      (cpi->sf.tx_size_search_method != USE_FULL_RD &&
       !b_inter_mode)) {
    vpx_memset(txfm_cache, 0, TX_MODES * sizeof(int64_t));
    choose_largest_txfm_size(cpi, x, rate, distortion, skip, sse,
                             ref_best_rd, bs);
    if (psse)
      *psse = sse[mbmi->tx_size];
    return;
  }

  if (cpi->sf.tx_size_search_method == USE_LARGESTINTRA_MODELINTER &&
      b_inter_mode) {
    if (bs >= BLOCK_32X32)
      model_rd_for_sb_y_tx(cpi, bs, TX_32X32, x, xd,
                           &r[TX_32X32][0], &d[TX_32X32], &s[TX_32X32]);
    if (bs >= BLOCK_16X16)
      model_rd_for_sb_y_tx(cpi, bs, TX_16X16, x, xd,
                           &r[TX_16X16][0], &d[TX_16X16], &s[TX_16X16]);

    model_rd_for_sb_y_tx(cpi, bs, TX_8X8, x, xd,
                         &r[TX_8X8][0], &d[TX_8X8], &s[TX_8X8]);

    model_rd_for_sb_y_tx(cpi, bs, TX_4X4, x, xd,
                         &r[TX_4X4][0], &d[TX_4X4], &s[TX_4X4]);

    choose_txfm_size_from_modelrd(cpi, x, r, rate, d, distortion, s,
                                  skip, sse, ref_best_rd, bs);
  } else {
    if (bs >= BLOCK_32X32)
      txfm_rd_in_plane(x, rdcost_stack, &r[TX_32X32][0], &d[TX_32X32],
                       &s[TX_32X32], &sse[TX_32X32],
                       ref_best_rd, 0, bs, TX_32X32);
    if (bs >= BLOCK_16X16)
      txfm_rd_in_plane(x, rdcost_stack, &r[TX_16X16][0], &d[TX_16X16],
                       &s[TX_16X16], &sse[TX_16X16],
                       ref_best_rd, 0, bs, TX_16X16);
    txfm_rd_in_plane(x, rdcost_stack, &r[TX_8X8][0], &d[TX_8X8], &s[TX_8X8],
                     &sse[TX_8X8], ref_best_rd, 0, bs, TX_8X8);
    txfm_rd_in_plane(x, rdcost_stack, &r[TX_4X4][0], &d[TX_4X4], &s[TX_4X4],
                     &sse[TX_4X4], ref_best_rd, 0, bs, TX_4X4);
    choose_txfm_size_from_rd(cpi, x, r, rate, d, distortion, s,
                             skip, txfm_cache, bs);
  }
  if (psse)
    *psse = sse[mbmi->tx_size];
}

static int conditional_skipintra(MB_PREDICTION_MODE mode,
                                 MB_PREDICTION_MODE best_intra_mode) {
  if (mode == D117_PRED &&
      best_intra_mode != V_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  if (mode == D63_PRED &&
      best_intra_mode != V_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D207_PRED &&
      best_intra_mode != H_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D153_PRED &&
      best_intra_mode != H_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  return 0;
}

static int64_t rd_pick_intra4x4block(VP9_COMP *cpi, MACROBLOCK *x, int ib,
                                     MB_PREDICTION_MODE *best_mode,
                                     int *bmode_costs,
                                     ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l,
                                     int *bestrate, int *bestratey,
                                     int64_t *bestdistortion,
                                     BLOCK_SIZE bsize, int64_t rd_thresh) {
  MB_PREDICTION_MODE mode;
  MACROBLOCKD *xd = &x->e_mbd;
  int64_t best_rd = rd_thresh;
  int rate = 0;
  int64_t distortion;
  struct macroblock_plane *p = &x->plane[0];
  struct macroblockd_plane *pd = &xd->plane[0];
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  uint8_t *src_init = raster_block_offset_uint8(BLOCK_8X8, ib,
                                                p->src.buf, src_stride);
  uint8_t *dst_init = raster_block_offset_uint8(BLOCK_8X8, ib,
                                                pd->dst.buf, dst_stride);
  int16_t *src_diff, *coeff;

  ENTROPY_CONTEXT ta[2], tempa[2];
  ENTROPY_CONTEXT tl[2], templ[2];

  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int idx, idy;
  uint8_t best_dst[8 * 8];

  assert(ib < 4);

  vpx_memcpy(ta, a, sizeof(ta));
  vpx_memcpy(tl, l, sizeof(tl));
  xd->mi_8x8[0]->mbmi.tx_size = TX_4X4;

  for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
    int64_t this_rd;
    int ratey = 0;

    if (!(cpi->sf.intra_y_mode_mask[TX_4X4] & (1 << mode)))
      continue;

    // Only do the oblique modes if the best so far is
    // one of the neighboring directional modes
    if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
      if (conditional_skipintra(mode, *best_mode))
          continue;
    }

    rate = bmode_costs[mode];
    distortion = 0;

    vpx_memcpy(tempa, ta, sizeof(ta));
    vpx_memcpy(templ, tl, sizeof(tl));

    for (idy = 0; idy < num_4x4_blocks_high; ++idy) {
      for (idx = 0; idx < num_4x4_blocks_wide; ++idx) {
        int64_t ssz;
        const int16_t *scan;
        const int16_t *nb;
        uint8_t *src = src_init + idx * 4 + idy * 4 * src_stride;
        uint8_t *dst = dst_init + idx * 4 + idy * 4 * dst_stride;
        const int block = ib + idy * 2 + idx;
        TX_TYPE tx_type;
        xd->mi_8x8[0]->bmi[block].as_mode = mode;
        src_diff = raster_block_offset_int16(BLOCK_8X8, block, p->src_diff);
        coeff = BLOCK_OFFSET(x->plane[0].coeff, block);
        vp9_predict_intra_block(xd, block, 1,
                                TX_4X4, mode,
                                x->skip_encode ? src : dst,
                                x->skip_encode ? src_stride : dst_stride,
                                dst, dst_stride);
        vp9_subtract_block(4, 4, src_diff, 8,
                           src, src_stride,
                           dst, dst_stride);

        tx_type = get_tx_type_4x4(PLANE_TYPE_Y_WITH_DC, xd, block);
        get_scan_nb_4x4(tx_type, &scan, &nb);

        if (tx_type != DCT_DCT)
          vp9_short_fht4x4(src_diff, coeff, 8, tx_type);
        else
          x->fwd_txm4x4(src_diff, coeff, 8);

        vp9_regular_quantize_b_4x4(x, 4, block, scan, get_iscan_4x4(tx_type));

        ratey += cost_coeffs(x, 0, block,
                             tempa + idx, templ + idy, TX_4X4, scan, nb);
        distortion += vp9_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, block),
                                      16, &ssz) >> 2;
        if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
          goto next;

        if (tx_type != DCT_DCT)
          vp9_iht4x4_16_add(BLOCK_OFFSET(pd->dqcoeff, block),
                               dst, pd->dst.stride, tx_type);
        else
          xd->itxm_add(BLOCK_OFFSET(pd->dqcoeff, block), dst, pd->dst.stride,
                       16);
      }
    }

    rate += ratey;
    this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

    if (this_rd < best_rd) {
      *bestrate = rate;
      *bestratey = ratey;
      *bestdistortion = distortion;
      best_rd = this_rd;
      *best_mode = mode;
      vpx_memcpy(a, tempa, sizeof(tempa));
      vpx_memcpy(l, templ, sizeof(templ));
      for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy)
        vpx_memcpy(best_dst + idy * 8, dst_init + idy * dst_stride,
                   num_4x4_blocks_wide * 4);
    }
  next:
    {}
  }

  if (best_rd >= rd_thresh || x->skip_encode)
    return best_rd;

  for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy)
    vpx_memcpy(dst_init + idy * dst_stride, best_dst + idy * 8,
               num_4x4_blocks_wide * 4);

  return best_rd;
}

static int64_t rd_pick_intra_sub_8x8_y_mode(VP9_COMP * const cpi,
                                            MACROBLOCK * const mb,
                                            int * const rate,
                                            int * const rate_y,
                                            int64_t * const distortion,
                                            int64_t best_rd) {
  int i, j;
  MACROBLOCKD *const xd = &mb->e_mbd;
  MODE_INFO *const mic = xd->mi_8x8[0];
  const MODE_INFO *above_mi = xd->mi_8x8[-xd->mode_info_stride];
  const MODE_INFO *left_mi = xd->left_available ? xd->mi_8x8[-1] : NULL;
  const BLOCK_SIZE bsize = xd->mi_8x8[0]->mbmi.sb_type;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int idx, idy;
  int cost = 0;
  int64_t total_distortion = 0;
  int tot_rate_y = 0;
  int64_t total_rd = 0;
  ENTROPY_CONTEXT t_above[4], t_left[4];
  int *bmode_costs;

  vpx_memcpy(t_above, xd->plane[0].above_context, sizeof(t_above));
  vpx_memcpy(t_left, xd->plane[0].left_context, sizeof(t_left));

  bmode_costs = mb->mbmode_cost;

  // Pick modes for each sub-block (of size 4x4, 4x8, or 8x4) in an 8x8 block.
  for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
    for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
      MB_PREDICTION_MODE best_mode = DC_PRED;
      int r = INT_MAX, ry = INT_MAX;
      int64_t d = INT64_MAX, this_rd = INT64_MAX;
      i = idy * 2 + idx;
      if (cpi->common.frame_type == KEY_FRAME) {
        const MB_PREDICTION_MODE A = above_block_mode(mic, above_mi, i);
        const MB_PREDICTION_MODE L = left_block_mode(mic, left_mi, i);

        bmode_costs  = mb->y_mode_costs[A][L];
      }

      this_rd = rd_pick_intra4x4block(cpi, mb, i, &best_mode, bmode_costs,
                                      t_above + idx, t_left + idy, &r, &ry, &d,
                                      bsize, best_rd - total_rd);
      if (this_rd >= best_rd - total_rd)
        return INT64_MAX;

      total_rd += this_rd;
      cost += r;
      total_distortion += d;
      tot_rate_y += ry;

      mic->bmi[i].as_mode = best_mode;
      for (j = 1; j < num_4x4_blocks_high; ++j)
        mic->bmi[i + j * 2].as_mode = best_mode;
      for (j = 1; j < num_4x4_blocks_wide; ++j)
        mic->bmi[i + j].as_mode = best_mode;

      if (total_rd >= best_rd)
        return INT64_MAX;
    }
  }

  *rate = cost;
  *rate_y = tot_rate_y;
  *distortion = total_distortion;
  mic->mbmi.mode = mic->bmi[3].as_mode;

  return RDCOST(mb->rdmult, mb->rddiv, cost, total_distortion);
}

static int64_t rd_pick_intra_sby_mode(VP9_COMP *cpi, MACROBLOCK *x,
                                      int *rate, int *rate_tokenonly,
                                      int64_t *distortion, int *skippable,
                                      BLOCK_SIZE bsize,
                                      int64_t tx_cache[TX_MODES],
                                      int64_t best_rd) {
  MB_PREDICTION_MODE mode;
  MB_PREDICTION_MODE mode_selected = DC_PRED;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi_8x8[0];
  int this_rate, this_rate_tokenonly, s;
  int64_t this_distortion, this_rd;
  TX_SIZE best_tx = TX_4X4;
  int i;
  int *bmode_costs = x->mbmode_cost;

  if (cpi->sf.tx_size_search_method == USE_FULL_RD)
    for (i = 0; i < TX_MODES; i++)
      tx_cache[i] = INT64_MAX;

  /* Y Search for intra prediction mode */
  for (mode = DC_PRED; mode <= TM_PRED; mode++) {
    int64_t local_tx_cache[TX_MODES];
    MODE_INFO *above_mi = xd->mi_8x8[-xd->mode_info_stride];
    MODE_INFO *left_mi = xd->left_available ? xd->mi_8x8[-1] : NULL;

    if (!(cpi->sf.intra_y_mode_mask[max_txsize_lookup[bsize]] & (1 << mode)))
      continue;

    if (cpi->common.frame_type == KEY_FRAME) {
      const MB_PREDICTION_MODE A = above_block_mode(mic, above_mi, 0);
      const MB_PREDICTION_MODE L = left_block_mode(mic, left_mi, 0);

      bmode_costs = x->y_mode_costs[A][L];
    }
    mic->mbmi.mode = mode;

    super_block_yrd(cpi, x, &this_rate_tokenonly, &this_distortion, &s, NULL,
                    bsize, local_tx_cache, best_rd);

    if (this_rate_tokenonly == INT_MAX)
      continue;

    this_rate = this_rate_tokenonly + bmode_costs[mode];
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);

    if (this_rd < best_rd) {
      mode_selected   = mode;
      best_rd         = this_rd;
      best_tx         = mic->mbmi.tx_size;
      *rate           = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion     = this_distortion;
      *skippable      = s;
    }

    if (cpi->sf.tx_size_search_method == USE_FULL_RD && this_rd < INT64_MAX) {
      for (i = 0; i < TX_MODES && local_tx_cache[i] < INT64_MAX; i++) {
        const int64_t adj_rd = this_rd + local_tx_cache[i] -
            local_tx_cache[cpi->common.tx_mode];
        if (adj_rd < tx_cache[i]) {
          tx_cache[i] = adj_rd;
        }
      }
    }
  }

  mic->mbmi.mode = mode_selected;
  mic->mbmi.tx_size = best_tx;

  return best_rd;
}

static void super_block_uvrd(VP9_COMP *const cpi, MACROBLOCK *x,
                             int *rate, int64_t *distortion, int *skippable,
                             int64_t *sse, BLOCK_SIZE bsize,
                             int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;
  TX_SIZE uv_txfm_size = get_uv_tx_size(mbmi);
  int plane;
  int pnrate = 0, pnskip = 1;
  int64_t pndist = 0, pnsse = 0;

  if (ref_best_rd < 0)
    goto term;

  if (is_inter_block(mbmi))
    vp9_subtract_sbuv(x, bsize);

  *rate = 0;
  *distortion = 0;
  *sse = 0;
  *skippable = 1;

  for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
    txfm_rd_in_plane(x, &cpi->rdcost_stack, &pnrate, &pndist, &pnskip, &pnsse,
                     ref_best_rd, plane, bsize, uv_txfm_size);
    if (pnrate == INT_MAX)
      goto term;
    *rate += pnrate;
    *distortion += pndist;
    *sse += pnsse;
    *skippable &= pnskip;
  }
  return;

  term:
  *rate = INT_MAX;
  *distortion = INT64_MAX;
  *sse = INT64_MAX;
  *skippable = 0;
  return;
}

static int64_t rd_pick_intra_sbuv_mode(VP9_COMP *cpi, MACROBLOCK *x,
                                       PICK_MODE_CONTEXT *ctx,
                                       int *rate, int *rate_tokenonly,
                                       int64_t *distortion, int *skippable,
                                       BLOCK_SIZE bsize) {
  MB_PREDICTION_MODE mode;
  MB_PREDICTION_MODE mode_selected = DC_PRED;
  int64_t best_rd = INT64_MAX, this_rd;
  int this_rate_tokenonly, this_rate, s;
  int64_t this_distortion, this_sse;

  // int mode_mask = (bsize <= BLOCK_8X8)
  //                ? ALL_INTRA_MODES : cpi->sf.intra_uv_mode_mask;

  for (mode = DC_PRED; mode <= TM_PRED; mode ++) {
    // if (!(mode_mask & (1 << mode)))
    if (!(cpi->sf.intra_uv_mode_mask[max_uv_txsize_lookup[bsize]]
          & (1 << mode)))
      continue;

    x->e_mbd.mi_8x8[0]->mbmi.uv_mode = mode;

    super_block_uvrd(cpi, x, &this_rate_tokenonly,
                     &this_distortion, &s, &this_sse, bsize, best_rd);
    if (this_rate_tokenonly == INT_MAX)
      continue;
    this_rate = this_rate_tokenonly +
                x->intra_uv_mode_cost[cpi->common.frame_type][mode];
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);

    if (this_rd < best_rd) {
      mode_selected   = mode;
      best_rd         = this_rd;
      *rate           = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion     = this_distortion;
      *skippable      = s;
      if (!x->select_txfm_size) {
        int i;
        struct macroblock_plane *const p = x->plane;
        struct macroblockd_plane *const pd = x->e_mbd.plane;
        for (i = 1; i < MAX_MB_PLANE; ++i) {
          p[i].coeff    = ctx->coeff_pbuf[i][2];
          pd[i].qcoeff  = ctx->qcoeff_pbuf[i][2];
          pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][2];
          pd[i].eobs    = ctx->eobs_pbuf[i][2];

          ctx->coeff_pbuf[i][2]   = ctx->coeff_pbuf[i][0];
          ctx->qcoeff_pbuf[i][2]  = ctx->qcoeff_pbuf[i][0];
          ctx->dqcoeff_pbuf[i][2] = ctx->dqcoeff_pbuf[i][0];
          ctx->eobs_pbuf[i][2]    = ctx->eobs_pbuf[i][0];

          ctx->coeff_pbuf[i][0]   = p[i].coeff;
          ctx->qcoeff_pbuf[i][0]  = pd[i].qcoeff;
          ctx->dqcoeff_pbuf[i][0] = pd[i].dqcoeff;
          ctx->eobs_pbuf[i][0]    = pd[i].eobs;
        }
      }
    }
  }

  x->e_mbd.mi_8x8[0]->mbmi.uv_mode = mode_selected;

  return best_rd;
}

static int64_t rd_sbuv_dcpred(VP9_COMP *cpi, MACROBLOCK *x,
                              int *rate, int *rate_tokenonly,
                              int64_t *distortion, int *skippable,
                              BLOCK_SIZE bsize) {
  int64_t this_rd;
  int64_t this_sse;

  x->e_mbd.mi_8x8[0]->mbmi.uv_mode = DC_PRED;
  super_block_uvrd(cpi, x, rate_tokenonly, distortion,
                   skippable, &this_sse, bsize, INT64_MAX);
  *rate = *rate_tokenonly +
          x->intra_uv_mode_cost[cpi->common.frame_type][DC_PRED];
  this_rd = RDCOST(x->rdmult, x->rddiv, *rate, *distortion);

  return this_rd;
}

static void choose_intra_uv_mode(VP9_COMP *cpi, PICK_MODE_CONTEXT *ctx,
                                 BLOCK_SIZE bsize, int *rate_uv,
                                 int *rate_uv_tokenonly,
                                 int64_t *dist_uv, int *skip_uv,
                                 MB_PREDICTION_MODE *mode_uv) {
  MACROBLOCK *const x = &cpi->mb;

  // Use an estimated rd for uv_intra based on DC_PRED if the
  // appropriate speed flag is set.
  if (cpi->sf.use_uv_intra_rd_estimate) {
    rd_sbuv_dcpred(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                   bsize < BLOCK_8X8 ? BLOCK_8X8 : bsize);
  // Else do a proper rd search for each possible transform size that may
  // be considered in the main rd loop.
  } else {
    rd_pick_intra_sbuv_mode(cpi, x, ctx,
                            rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                            bsize < BLOCK_8X8 ? BLOCK_8X8 : bsize);
  }
  *mode_uv = x->e_mbd.mi_8x8[0]->mbmi.uv_mode;
}

static int cost_mv_ref(VP9_COMP *cpi, MB_PREDICTION_MODE mode,
                       int mode_context) {
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int segment_id = xd->mi_8x8[0]->mbmi.segment_id;

  // Don't account for mode here if segment skip is enabled.
  if (!vp9_segfeature_active(&cpi->common.seg, segment_id, SEG_LVL_SKIP)) {
    assert(is_inter_mode(mode));
    return x->inter_mode_cost[mode_context][INTER_OFFSET(mode)];
  } else {
    return 0;
  }
}

void vp9_set_mbmode_and_mvs(MACROBLOCK *x, MB_PREDICTION_MODE mb, int_mv *mv) {
  x->e_mbd.mi_8x8[0]->mbmi.mode = mb;
  x->e_mbd.mi_8x8[0]->mbmi.mv[0].as_int = mv->as_int;
}

static void joint_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                BLOCK_SIZE bsize,
                                int_mv *frame_mv,
                                int mi_row, int mi_col,
                                int_mv single_newmv[MAX_REF_FRAMES],
                                int *rate_mv);

static int labels2mode(MACROBLOCK *x, int i,
                       MB_PREDICTION_MODE this_mode,
                       int_mv *this_mv, int_mv *this_second_mv,
                       int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES],
                       int_mv seg_mvs[MAX_REF_FRAMES],
                       int_mv *best_ref_mv,
                       int_mv *second_best_ref_mv,
                       int *mvjcost, int *mvcost[2], VP9_COMP *cpi) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi_8x8[0];
  MB_MODE_INFO *mbmi = &mic->mbmi;
  int cost = 0, thismvcost = 0;
  int idx, idy;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[mbmi->sb_type];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[mbmi->sb_type];
  const int has_second_rf = has_second_ref(mbmi);

  /* We have to be careful retrieving previously-encoded motion vectors.
   Ones from this macroblock have to be pulled from the BLOCKD array
   as they have not yet made it to the bmi array in our MB_MODE_INFO. */
  MB_PREDICTION_MODE m;

  // the only time we should do costing for new motion vector or mode
  // is when we are on a new label  (jbb May 08, 2007)
  switch (m = this_mode) {
    case NEWMV:
      this_mv->as_int = seg_mvs[mbmi->ref_frame[0]].as_int;
      thismvcost  = vp9_mv_bit_cost(&this_mv->as_mv, &best_ref_mv->as_mv,
                                    mvjcost, mvcost, MV_COST_WEIGHT_SUB);
      if (has_second_rf) {
        this_second_mv->as_int = seg_mvs[mbmi->ref_frame[1]].as_int;
        thismvcost += vp9_mv_bit_cost(&this_second_mv->as_mv,
                                      &second_best_ref_mv->as_mv,
                                      mvjcost, mvcost, MV_COST_WEIGHT_SUB);
      }
      break;
    case NEARESTMV:
      this_mv->as_int = frame_mv[NEARESTMV][mbmi->ref_frame[0]].as_int;
      if (has_second_rf)
        this_second_mv->as_int =
            frame_mv[NEARESTMV][mbmi->ref_frame[1]].as_int;
      break;
    case NEARMV:
      this_mv->as_int = frame_mv[NEARMV][mbmi->ref_frame[0]].as_int;
      if (has_second_rf)
        this_second_mv->as_int =
            frame_mv[NEARMV][mbmi->ref_frame[1]].as_int;
      break;
    case ZEROMV:
      this_mv->as_int = 0;
      if (has_second_rf)
        this_second_mv->as_int = 0;
      break;
    default:
      break;
  }

  cost = cost_mv_ref(cpi, this_mode,
                     mbmi->mode_context[mbmi->ref_frame[0]]);

  mic->bmi[i].as_mv[0].as_int = this_mv->as_int;
  if (has_second_rf)
    mic->bmi[i].as_mv[1].as_int = this_second_mv->as_int;

  mic->bmi[i].as_mode = m;

  for (idy = 0; idy < num_4x4_blocks_high; ++idy)
    for (idx = 0; idx < num_4x4_blocks_wide; ++idx)
      vpx_memcpy(&mic->bmi[i + idy * 2 + idx],
                 &mic->bmi[i], sizeof(mic->bmi[i]));

  cost += thismvcost;
  return cost;
}

static int64_t encode_inter_mb_segment(VP9_COMP *cpi,
                                       MACROBLOCK *x,
                                       int64_t best_yrd,
                                       int i,
                                       int *labelyrate,
                                       int64_t *distortion, int64_t *sse,
                                       ENTROPY_CONTEXT *ta,
                                       ENTROPY_CONTEXT *tl) {
  int k;
  MACROBLOCKD *xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[0];
  struct macroblock_plane *const p = &x->plane[0];
  MODE_INFO *const mi = xd->mi_8x8[0];
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;
  const int width = plane_block_width(bsize, pd);
  const int height = plane_block_height(bsize, pd);
  int idx, idy;

  uint8_t *const src = raster_block_offset_uint8(BLOCK_8X8, i,
                                                 p->src.buf, p->src.stride);
  uint8_t *const dst = raster_block_offset_uint8(BLOCK_8X8, i,
                                                 pd->dst.buf, pd->dst.stride);
  int64_t thisdistortion = 0, thissse = 0;
  int thisrate = 0, ref;
  const int is_compound = has_second_ref(&mi->mbmi);
  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const uint8_t *pre = raster_block_offset_uint8(BLOCK_8X8, i,
                                     pd->pre[ref].buf, pd->pre[ref].stride);
    vp9_build_inter_predictor(pre, pd->pre[ref].stride,
                              dst, pd->dst.stride,
                              &mi->bmi[i].as_mv[ref].as_mv,
                              &xd->scale_factor[ref],
                              width, height, ref, &xd->subpix, MV_PRECISION_Q3);
  }

  vp9_subtract_block(height, width,
                     raster_block_offset_int16(BLOCK_8X8, i, p->src_diff), 8,
                     src, p->src.stride,
                     dst, pd->dst.stride);

  k = i;
  for (idy = 0; idy < height / 4; ++idy) {
    for (idx = 0; idx < width / 4; ++idx) {
      int64_t ssz, rd, rd1, rd2;
      int16_t* coeff;

      k += (idy * 2 + idx);
      coeff = BLOCK_OFFSET(p->coeff, k);
      x->fwd_txm4x4(raster_block_offset_int16(BLOCK_8X8, k, p->src_diff),
                    coeff, 8);
      vp9_regular_quantize_b_4x4(x, 4, k, get_scan_4x4(DCT_DCT),
                                 get_iscan_4x4(DCT_DCT));
      thisdistortion += vp9_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, k),
                                        16, &ssz);
      thissse += ssz;
      thisrate += cost_coeffs(x, 0, k,
                              ta + (k & 1),
                              tl + (k >> 1), TX_4X4,
                              vp9_default_scan_4x4,
                              vp9_default_scan_4x4_neighbors);
      rd1 = RDCOST(x->rdmult, x->rddiv, thisrate, thisdistortion >> 2);
      rd2 = RDCOST(x->rdmult, x->rddiv, 0, thissse >> 2);
      rd = MIN(rd1, rd2);
      if (rd >= best_yrd)
        return INT64_MAX;
    }
  }

  *distortion = thisdistortion >> 2;
  *labelyrate = thisrate;
  *sse = thissse >> 2;

  return RDCOST(x->rdmult, x->rddiv, *labelyrate, *distortion);
}

typedef struct {
  int eobs;
  int brate;
  int byrate;
  int64_t bdist;
  int64_t bsse;
  int64_t brdcost;
  int_mv mvs[2];
  ENTROPY_CONTEXT ta[2];
  ENTROPY_CONTEXT tl[2];
} SEG_RDSTAT;

typedef struct {
  int_mv *ref_mv, *second_ref_mv;
  int_mv mvp;

  int64_t segment_rd;
  int r;
  int64_t d;
  int64_t sse;
  int segment_yrate;
  MB_PREDICTION_MODE modes[4];
  SEG_RDSTAT rdstat[4][INTER_MODES];
  int mvthresh;
} BEST_SEG_INFO;

static INLINE int mv_check_bounds(MACROBLOCK *x, int_mv *mv) {
  int r = 0;
  r |= (mv->as_mv.row >> 3) < x->mv_row_min;
  r |= (mv->as_mv.row >> 3) > x->mv_row_max;
  r |= (mv->as_mv.col >> 3) < x->mv_col_min;
  r |= (mv->as_mv.col >> 3) > x->mv_col_max;
  return r;
}

static INLINE void mi_buf_shift(MACROBLOCK *x, int i) {
  MB_MODE_INFO *const mbmi = &x->e_mbd.mi_8x8[0]->mbmi;
  struct macroblock_plane *const p = &x->plane[0];
  struct macroblockd_plane *const pd = &x->e_mbd.plane[0];

  p->src.buf = raster_block_offset_uint8(BLOCK_8X8, i, p->src.buf,
                                         p->src.stride);
  assert(((intptr_t)pd->pre[0].buf & 0x7) == 0);
  pd->pre[0].buf = raster_block_offset_uint8(BLOCK_8X8, i, pd->pre[0].buf,
                                             pd->pre[0].stride);
  if (has_second_ref(mbmi))
    pd->pre[1].buf = raster_block_offset_uint8(BLOCK_8X8, i, pd->pre[1].buf,
                                               pd->pre[1].stride);
}

static INLINE void mi_buf_restore(MACROBLOCK *x, struct buf_2d orig_src,
                                  struct buf_2d orig_pre[2]) {
  MB_MODE_INFO *mbmi = &x->e_mbd.mi_8x8[0]->mbmi;
  x->plane[0].src = orig_src;
  x->e_mbd.plane[0].pre[0] = orig_pre[0];
  if (has_second_ref(mbmi))
    x->e_mbd.plane[0].pre[1] = orig_pre[1];
}

static void rd_check_segment_txsize(VP9_COMP *cpi, MACROBLOCK *x,
                                    const TileInfo *const tile,
                                    BEST_SEG_INFO *bsi_buf, int filter_idx,
                                    int_mv seg_mvs[4][MAX_REF_FRAMES],
                                    int mi_row, int mi_col) {
  int i, br = 0, idx, idy;
  int64_t bd = 0, block_sse = 0;
  MB_PREDICTION_MODE this_mode;
  MODE_INFO *mi = x->e_mbd.mi_8x8[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  struct macroblockd_plane *const pd = &x->e_mbd.plane[0];
  const int label_count = 4;
  int64_t this_segment_rd = 0;
  int label_mv_thresh;
  int segmentyrate = 0;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  vp9_variance_fn_ptr_t *v_fn_ptr;
  ENTROPY_CONTEXT t_above[2], t_left[2];
  BEST_SEG_INFO *bsi = bsi_buf + filter_idx;
  int mode_idx;
  int subpelmv = 1, have_ref = 0;
  const int has_second_rf = has_second_ref(mbmi);

  vpx_memcpy(t_above, pd->above_context, sizeof(t_above));
  vpx_memcpy(t_left, pd->left_context, sizeof(t_left));

  v_fn_ptr = &cpi->fn_ptr[bsize];

  // 64 makes this threshold really big effectively
  // making it so that we very rarely check mvs on
  // segments.   setting this to 1 would make mv thresh
  // roughly equal to what it is for macroblocks
  label_mv_thresh = 1 * bsi->mvthresh / label_count;

  // Segmentation method overheads
  for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
    for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
      // TODO(jingning,rbultje): rewrite the rate-distortion optimization
      // loop for 4x4/4x8/8x4 block coding. to be replaced with new rd loop
      int_mv mode_mv[MB_MODE_COUNT], second_mode_mv[MB_MODE_COUNT];
      int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
      MB_PREDICTION_MODE mode_selected = ZEROMV;
      int64_t best_rd = INT64_MAX;
      i = idy * 2 + idx;

      frame_mv[ZEROMV][mbmi->ref_frame[0]].as_int = 0;
      vp9_append_sub8x8_mvs_for_idx(&cpi->common, &x->e_mbd, tile,
                                    &frame_mv[NEARESTMV][mbmi->ref_frame[0]],
                                    &frame_mv[NEARMV][mbmi->ref_frame[0]],
                                    i, 0, mi_row, mi_col);
      if (has_second_rf) {
        frame_mv[ZEROMV][mbmi->ref_frame[1]].as_int = 0;
        vp9_append_sub8x8_mvs_for_idx(&cpi->common, &x->e_mbd, tile,
                                      &frame_mv[NEARESTMV][mbmi->ref_frame[1]],
                                      &frame_mv[NEARMV][mbmi->ref_frame[1]],
                                      i, 1, mi_row, mi_col);
      }
      // search for the best motion vector on this segment
      for (this_mode = NEARESTMV; this_mode <= NEWMV; ++this_mode) {
        const struct buf_2d orig_src = x->plane[0].src;
        struct buf_2d orig_pre[2];

        mode_idx = INTER_OFFSET(this_mode);
        bsi->rdstat[i][mode_idx].brdcost = INT64_MAX;

        // if we're near/nearest and mv == 0,0, compare to zeromv
        if ((this_mode == NEARMV || this_mode == NEARESTMV ||
             this_mode == ZEROMV) &&
            frame_mv[this_mode][mbmi->ref_frame[0]].as_int == 0 &&
            (!has_second_rf ||
             frame_mv[this_mode][mbmi->ref_frame[1]].as_int == 0)) {
          int rfc = mbmi->mode_context[mbmi->ref_frame[0]];
          int c1 = cost_mv_ref(cpi, NEARMV, rfc);
          int c2 = cost_mv_ref(cpi, NEARESTMV, rfc);
          int c3 = cost_mv_ref(cpi, ZEROMV, rfc);

          if (this_mode == NEARMV) {
            if (c1 > c3)
              continue;
          } else if (this_mode == NEARESTMV) {
            if (c2 > c3)
              continue;
          } else {
            assert(this_mode == ZEROMV);
            if (!has_second_rf) {
              if ((c3 >= c2 &&
                   frame_mv[NEARESTMV][mbmi->ref_frame[0]].as_int == 0) ||
                  (c3 >= c1 &&
                   frame_mv[NEARMV][mbmi->ref_frame[0]].as_int == 0))
                continue;
            } else {
              if ((c3 >= c2 &&
                   frame_mv[NEARESTMV][mbmi->ref_frame[0]].as_int == 0 &&
                   frame_mv[NEARESTMV][mbmi->ref_frame[1]].as_int == 0) ||
                  (c3 >= c1 &&
                   frame_mv[NEARMV][mbmi->ref_frame[0]].as_int == 0 &&
                   frame_mv[NEARMV][mbmi->ref_frame[1]].as_int == 0))
                continue;
            }
          }
        }

        vpx_memcpy(orig_pre, pd->pre, sizeof(orig_pre));
        vpx_memcpy(bsi->rdstat[i][mode_idx].ta, t_above,
                   sizeof(bsi->rdstat[i][mode_idx].ta));
        vpx_memcpy(bsi->rdstat[i][mode_idx].tl, t_left,
                   sizeof(bsi->rdstat[i][mode_idx].tl));

        // motion search for newmv (single predictor case only)
        if (!has_second_rf && this_mode == NEWMV &&
            seg_mvs[i][mbmi->ref_frame[0]].as_int == INVALID_MV) {
          int step_param = 0;
          int further_steps;
          int thissme, bestsme = INT_MAX;
          int sadpb = x->sadperbit4;
          int_mv mvp_full;
          int max_mv;

          /* Is the best so far sufficiently good that we cant justify doing
           * and new motion search. */
          if (best_rd < label_mv_thresh)
            break;

          if (cpi->compressor_speed) {
            // use previous block's result as next block's MV predictor.
            if (i > 0) {
              bsi->mvp.as_int = mi->bmi[i - 1].as_mv[0].as_int;
              if (i == 2)
                bsi->mvp.as_int = mi->bmi[i - 2].as_mv[0].as_int;
            }
          }
          if (i == 0)
            max_mv = x->max_mv_context[mbmi->ref_frame[0]];
          else
            max_mv = MAX(abs(bsi->mvp.as_mv.row), abs(bsi->mvp.as_mv.col)) >> 3;

          if (cpi->sf.auto_mv_step_size && cpi->common.show_frame) {
            // Take wtd average of the step_params based on the last frame's
            // max mv magnitude and the best ref mvs of the current block for
            // the given reference.
            step_param = (vp9_init_search_range(cpi, max_mv) +
                          cpi->mv_step_param) >> 1;
          } else {
            step_param = cpi->mv_step_param;
          }

          mvp_full.as_mv.row = bsi->mvp.as_mv.row >> 3;
          mvp_full.as_mv.col = bsi->mvp.as_mv.col >> 3;

          if (cpi->sf.adaptive_motion_search && cpi->common.show_frame) {
            mvp_full.as_mv.row = x->pred_mv[mbmi->ref_frame[0]].as_mv.row >> 3;
            mvp_full.as_mv.col = x->pred_mv[mbmi->ref_frame[0]].as_mv.col >> 3;
            step_param = MAX(step_param, 8);
          }

          further_steps = (MAX_MVSEARCH_STEPS - 1) - step_param;
          // adjust src pointer for this block
          mi_buf_shift(x, i);
          if (cpi->sf.search_method == HEX) {
            bestsme = vp9_hex_search(x, &mvp_full.as_mv,
                                     step_param,
                                     sadpb, 1, v_fn_ptr, 1,
                                     &bsi->ref_mv->as_mv,
                                     &mode_mv[NEWMV].as_mv);
          } else if (cpi->sf.search_method == SQUARE) {
            bestsme = vp9_square_search(x, &mvp_full.as_mv,
                                        step_param,
                                        sadpb, 1, v_fn_ptr, 1,
                                        &bsi->ref_mv->as_mv,
                                        &mode_mv[NEWMV].as_mv);
          } else if (cpi->sf.search_method == BIGDIA) {
            bestsme = vp9_bigdia_search(x, &mvp_full.as_mv,
                                        step_param,
                                        sadpb, 1, v_fn_ptr, 1,
                                        &bsi->ref_mv->as_mv,
                                        &mode_mv[NEWMV].as_mv);
          } else {
            bestsme = vp9_full_pixel_diamond(cpi, x, &mvp_full, step_param,
                                             sadpb, further_steps, 0, v_fn_ptr,
                                             bsi->ref_mv, &mode_mv[NEWMV]);
          }

          // Should we do a full search (best quality only)
          if (cpi->compressor_speed == 0) {
            /* Check if mvp_full is within the range. */
            clamp_mv(&mvp_full.as_mv, x->mv_col_min, x->mv_col_max,
                     x->mv_row_min, x->mv_row_max);

            thissme = cpi->full_search_sad(x, &mvp_full,
                                           sadpb, 16, v_fn_ptr,
                                           x->nmvjointcost, x->mvcost,
                                           bsi->ref_mv, i);

            if (thissme < bestsme) {
              bestsme = thissme;
              mode_mv[NEWMV].as_int = mi->bmi[i].as_mv[0].as_int;
            } else {
              /* The full search result is actually worse so re-instate the
               * previous best vector */
              mi->bmi[i].as_mv[0].as_int = mode_mv[NEWMV].as_int;
            }
          }

          if (bestsme < INT_MAX) {
            int distortion;
            unsigned int sse;
            cpi->find_fractional_mv_step(x,
                                         &mode_mv[NEWMV].as_mv,
                                         &bsi->ref_mv->as_mv,
                                         cpi->common.allow_high_precision_mv,
                                         x->errorperbit, v_fn_ptr,
                                         0, cpi->sf.subpel_iters_per_step,
                                         x->nmvjointcost, x->mvcost,
                                         &distortion, &sse);

            // save motion search result for use in compound prediction
            seg_mvs[i][mbmi->ref_frame[0]].as_int = mode_mv[NEWMV].as_int;
          }

          if (cpi->sf.adaptive_motion_search)
            x->pred_mv[mbmi->ref_frame[0]].as_int = mode_mv[NEWMV].as_int;

          // restore src pointers
          mi_buf_restore(x, orig_src, orig_pre);
        }

        if (has_second_rf) {
          if (seg_mvs[i][mbmi->ref_frame[1]].as_int == INVALID_MV ||
              seg_mvs[i][mbmi->ref_frame[0]].as_int == INVALID_MV)
            continue;
        }

        if (has_second_rf && this_mode == NEWMV &&
            mbmi->interp_filter == EIGHTTAP) {
          // adjust src pointers
          mi_buf_shift(x, i);
          if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
            int rate_mv;
            joint_motion_search(cpi, x, bsize, frame_mv[this_mode],
                                mi_row, mi_col, seg_mvs[i],
                                &rate_mv);
            seg_mvs[i][mbmi->ref_frame[0]].as_int =
                frame_mv[this_mode][mbmi->ref_frame[0]].as_int;
            seg_mvs[i][mbmi->ref_frame[1]].as_int =
                frame_mv[this_mode][mbmi->ref_frame[1]].as_int;
          }
          // restore src pointers
          mi_buf_restore(x, orig_src, orig_pre);
        }

        bsi->rdstat[i][mode_idx].brate =
            labels2mode(x, i, this_mode, &mode_mv[this_mode],
                        &second_mode_mv[this_mode], frame_mv, seg_mvs[i],
                        bsi->ref_mv, bsi->second_ref_mv, x->nmvjointcost,
                        x->mvcost, cpi);


        bsi->rdstat[i][mode_idx].mvs[0].as_int = mode_mv[this_mode].as_int;
        if (num_4x4_blocks_wide > 1)
          bsi->rdstat[i + 1][mode_idx].mvs[0].as_int =
              mode_mv[this_mode].as_int;
        if (num_4x4_blocks_high > 1)
          bsi->rdstat[i + 2][mode_idx].mvs[0].as_int =
              mode_mv[this_mode].as_int;
        if (has_second_rf) {
          bsi->rdstat[i][mode_idx].mvs[1].as_int =
              second_mode_mv[this_mode].as_int;
          if (num_4x4_blocks_wide > 1)
            bsi->rdstat[i + 1][mode_idx].mvs[1].as_int =
                second_mode_mv[this_mode].as_int;
          if (num_4x4_blocks_high > 1)
            bsi->rdstat[i + 2][mode_idx].mvs[1].as_int =
                second_mode_mv[this_mode].as_int;
        }

        // Trap vectors that reach beyond the UMV borders
        if (mv_check_bounds(x, &mode_mv[this_mode]))
          continue;
        if (has_second_rf &&
            mv_check_bounds(x, &second_mode_mv[this_mode]))
          continue;

        if (filter_idx > 0) {
          BEST_SEG_INFO *ref_bsi = bsi_buf;
          subpelmv = (mode_mv[this_mode].as_mv.row & 0x0f) ||
                     (mode_mv[this_mode].as_mv.col & 0x0f);
          have_ref = mode_mv[this_mode].as_int ==
                     ref_bsi->rdstat[i][mode_idx].mvs[0].as_int;
          if (has_second_rf) {
            subpelmv |= (second_mode_mv[this_mode].as_mv.row & 0x0f) ||
                        (second_mode_mv[this_mode].as_mv.col & 0x0f);
            have_ref  &= second_mode_mv[this_mode].as_int ==
                         ref_bsi->rdstat[i][mode_idx].mvs[1].as_int;
          }

          if (filter_idx > 1 && !subpelmv && !have_ref) {
            ref_bsi = bsi_buf + 1;
            have_ref = mode_mv[this_mode].as_int ==
                       ref_bsi->rdstat[i][mode_idx].mvs[0].as_int;
            if (has_second_rf) {
              have_ref  &= second_mode_mv[this_mode].as_int ==
                           ref_bsi->rdstat[i][mode_idx].mvs[1].as_int;
            }
          }

          if (!subpelmv && have_ref &&
              ref_bsi->rdstat[i][mode_idx].brdcost < INT64_MAX) {
            vpx_memcpy(&bsi->rdstat[i][mode_idx], &ref_bsi->rdstat[i][mode_idx],
                       sizeof(SEG_RDSTAT));
            if (num_4x4_blocks_wide > 1)
              bsi->rdstat[i + 1][mode_idx].eobs =
                  ref_bsi->rdstat[i + 1][mode_idx].eobs;
            if (num_4x4_blocks_high > 1)
              bsi->rdstat[i + 2][mode_idx].eobs =
                  ref_bsi->rdstat[i + 2][mode_idx].eobs;

            if (bsi->rdstat[i][mode_idx].brdcost < best_rd) {
              mode_selected = this_mode;
              best_rd = bsi->rdstat[i][mode_idx].brdcost;
            }
            continue;
          }
        }

        bsi->rdstat[i][mode_idx].brdcost =
            encode_inter_mb_segment(cpi, x,
                                    bsi->segment_rd - this_segment_rd, i,
                                    &bsi->rdstat[i][mode_idx].byrate,
                                    &bsi->rdstat[i][mode_idx].bdist,
                                    &bsi->rdstat[i][mode_idx].bsse,
                                    bsi->rdstat[i][mode_idx].ta,
                                    bsi->rdstat[i][mode_idx].tl);
        if (bsi->rdstat[i][mode_idx].brdcost < INT64_MAX) {
          bsi->rdstat[i][mode_idx].brdcost += RDCOST(x->rdmult, x->rddiv,
                                            bsi->rdstat[i][mode_idx].brate, 0);
          bsi->rdstat[i][mode_idx].brate += bsi->rdstat[i][mode_idx].byrate;
          bsi->rdstat[i][mode_idx].eobs = pd->eobs[i];
          if (num_4x4_blocks_wide > 1)
            bsi->rdstat[i + 1][mode_idx].eobs = pd->eobs[i + 1];
          if (num_4x4_blocks_high > 1)
            bsi->rdstat[i + 2][mode_idx].eobs = pd->eobs[i + 2];
        }

        if (bsi->rdstat[i][mode_idx].brdcost < best_rd) {
          mode_selected = this_mode;
          best_rd = bsi->rdstat[i][mode_idx].brdcost;
        }
      } /*for each 4x4 mode*/

      if (best_rd == INT64_MAX) {
        int iy, midx;
        for (iy = i + 1; iy < 4; ++iy)
          for (midx = 0; midx < INTER_MODES; ++midx)
            bsi->rdstat[iy][midx].brdcost = INT64_MAX;
        bsi->segment_rd = INT64_MAX;
        return;
      }

      mode_idx = INTER_OFFSET(mode_selected);
      vpx_memcpy(t_above, bsi->rdstat[i][mode_idx].ta, sizeof(t_above));
      vpx_memcpy(t_left, bsi->rdstat[i][mode_idx].tl, sizeof(t_left));

      labels2mode(x, i, mode_selected, &mode_mv[mode_selected],
                  &second_mode_mv[mode_selected], frame_mv, seg_mvs[i],
                  bsi->ref_mv, bsi->second_ref_mv, x->nmvjointcost,
                  x->mvcost, cpi);

      br += bsi->rdstat[i][mode_idx].brate;
      bd += bsi->rdstat[i][mode_idx].bdist;
      block_sse += bsi->rdstat[i][mode_idx].bsse;
      segmentyrate += bsi->rdstat[i][mode_idx].byrate;
      this_segment_rd += bsi->rdstat[i][mode_idx].brdcost;

      if (this_segment_rd > bsi->segment_rd) {
        int iy, midx;
        for (iy = i + 1; iy < 4; ++iy)
          for (midx = 0; midx < INTER_MODES; ++midx)
            bsi->rdstat[iy][midx].brdcost = INT64_MAX;
        bsi->segment_rd = INT64_MAX;
        return;
      }
    }
  } /* for each label */

  bsi->r = br;
  bsi->d = bd;
  bsi->segment_yrate = segmentyrate;
  bsi->segment_rd = this_segment_rd;
  bsi->sse = block_sse;

  // update the coding decisions
  for (i = 0; i < 4; ++i)
    bsi->modes[i] = mi->bmi[i].as_mode;
}

static int64_t rd_pick_best_mbsegmentation(VP9_COMP *cpi, MACROBLOCK *x,
                                           const TileInfo *const tile,
                                           int_mv *best_ref_mv,
                                           int_mv *second_best_ref_mv,
                                           int64_t best_rd,
                                           int *returntotrate,
                                           int *returnyrate,
                                           int64_t *returndistortion,
                                           int *skippable, int64_t *psse,
                                           int mvthresh,
                                           int_mv seg_mvs[4][MAX_REF_FRAMES],
                                           BEST_SEG_INFO *bsi_buf,
                                           int filter_idx,
                                           int mi_row, int mi_col) {
  int i;
  BEST_SEG_INFO *bsi = bsi_buf + filter_idx;
  MACROBLOCKD *xd = &x->e_mbd;
  MODE_INFO *mi = xd->mi_8x8[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  int mode_idx;

  vp9_zero(*bsi);

  bsi->segment_rd = best_rd;
  bsi->ref_mv = best_ref_mv;
  bsi->second_ref_mv = second_best_ref_mv;
  bsi->mvp.as_int = best_ref_mv->as_int;
  bsi->mvthresh = mvthresh;

  for (i = 0; i < 4; i++)
    bsi->modes[i] = ZEROMV;

  rd_check_segment_txsize(cpi, x, tile, bsi_buf, filter_idx, seg_mvs,
                          mi_row, mi_col);

  if (bsi->segment_rd > best_rd)
    return INT64_MAX;
  /* set it to the best */
  for (i = 0; i < 4; i++) {
    mode_idx = INTER_OFFSET(bsi->modes[i]);
    mi->bmi[i].as_mv[0].as_int = bsi->rdstat[i][mode_idx].mvs[0].as_int;
    if (has_second_ref(mbmi))
      mi->bmi[i].as_mv[1].as_int = bsi->rdstat[i][mode_idx].mvs[1].as_int;
    xd->plane[0].eobs[i] = bsi->rdstat[i][mode_idx].eobs;
    mi->bmi[i].as_mode = bsi->modes[i];
  }

  /*
   * used to set mbmi->mv.as_int
   */
  *returntotrate = bsi->r;
  *returndistortion = bsi->d;
  *returnyrate = bsi->segment_yrate;
  *skippable = vp9_is_skippable_in_plane(&x->e_mbd, BLOCK_8X8, 0);
  *psse = bsi->sse;
  mbmi->mode = bsi->modes[3];

  return bsi->segment_rd;
}

static void mv_pred(VP9_COMP *cpi, MACROBLOCK *x,
                    uint8_t *ref_y_buffer, int ref_y_stride,
                    int ref_frame, BLOCK_SIZE block_size ) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  int_mv this_mv;
  int i;
  int zero_seen = 0;
  int best_index = 0;
  int best_sad = INT_MAX;
  int this_sad = INT_MAX;
  unsigned int max_mv = 0;

  uint8_t *src_y_ptr = x->plane[0].src.buf;
  uint8_t *ref_y_ptr;
  int row_offset, col_offset;
  int num_mv_refs = MAX_MV_REF_CANDIDATES +
                    (cpi->sf.adaptive_motion_search &&
                     cpi->common.show_frame &&
                     block_size < cpi->sf.max_partition_size);

  // Get the sad for each candidate reference mv
  for (i = 0; i < num_mv_refs; i++) {
    this_mv.as_int = (i < MAX_MV_REF_CANDIDATES) ?
        mbmi->ref_mvs[ref_frame][i].as_int : x->pred_mv[ref_frame].as_int;

    max_mv = MAX(max_mv,
                 MAX(abs(this_mv.as_mv.row), abs(this_mv.as_mv.col)) >> 3);
    // The list is at an end if we see 0 for a second time.
    if (!this_mv.as_int && zero_seen)
      break;
    zero_seen = zero_seen || !this_mv.as_int;

    row_offset = this_mv.as_mv.row >> 3;
    col_offset = this_mv.as_mv.col >> 3;
    ref_y_ptr = ref_y_buffer + (ref_y_stride * row_offset) + col_offset;

    // Find sad for current vector.
    this_sad = cpi->fn_ptr[block_size].sdf(src_y_ptr, x->plane[0].src.stride,
                                           ref_y_ptr, ref_y_stride,
                                           0x7fffffff);

    // Note if it is the best so far.
    if (this_sad < best_sad) {
      best_sad = this_sad;
      best_index = i;
    }
  }

  // Note the index of the mv that worked best in the reference list.
  x->mv_best_ref_index[ref_frame] = best_index;
  x->max_mv_context[ref_frame] = max_mv;
}

static void estimate_ref_frame_costs(VP9_COMP *cpi, int segment_id,
                                     unsigned int *ref_costs_single,
                                     unsigned int *ref_costs_comp,
                                     vp9_prob *comp_mode_p) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  int seg_ref_active = vp9_segfeature_active(&cm->seg, segment_id,
                                             SEG_LVL_REF_FRAME);
  if (seg_ref_active) {
    vpx_memset(ref_costs_single, 0, MAX_REF_FRAMES * sizeof(*ref_costs_single));
    vpx_memset(ref_costs_comp,   0, MAX_REF_FRAMES * sizeof(*ref_costs_comp));
    *comp_mode_p = 128;
  } else {
    vp9_prob intra_inter_p = vp9_get_pred_prob_intra_inter(cm, xd);
    vp9_prob comp_inter_p = 128;

    if (cm->comp_pred_mode == HYBRID_PREDICTION) {
      comp_inter_p = vp9_get_pred_prob_comp_inter_inter(cm, xd);
      *comp_mode_p = comp_inter_p;
    } else {
      *comp_mode_p = 128;
    }

    ref_costs_single[INTRA_FRAME] = vp9_cost_bit(intra_inter_p, 0);

    if (cm->comp_pred_mode != COMP_PREDICTION_ONLY) {
      vp9_prob ref_single_p1 = vp9_get_pred_prob_single_ref_p1(cm, xd);
      vp9_prob ref_single_p2 = vp9_get_pred_prob_single_ref_p2(cm, xd);
      unsigned int base_cost = vp9_cost_bit(intra_inter_p, 1);

      if (cm->comp_pred_mode == HYBRID_PREDICTION)
        base_cost += vp9_cost_bit(comp_inter_p, 0);

      ref_costs_single[LAST_FRAME] = ref_costs_single[GOLDEN_FRAME] =
          ref_costs_single[ALTREF_FRAME] = base_cost;
      ref_costs_single[LAST_FRAME]   += vp9_cost_bit(ref_single_p1, 0);
      ref_costs_single[GOLDEN_FRAME] += vp9_cost_bit(ref_single_p1, 1);
      ref_costs_single[ALTREF_FRAME] += vp9_cost_bit(ref_single_p1, 1);
      ref_costs_single[GOLDEN_FRAME] += vp9_cost_bit(ref_single_p2, 0);
      ref_costs_single[ALTREF_FRAME] += vp9_cost_bit(ref_single_p2, 1);
    } else {
      ref_costs_single[LAST_FRAME]   = 512;
      ref_costs_single[GOLDEN_FRAME] = 512;
      ref_costs_single[ALTREF_FRAME] = 512;
    }
    if (cm->comp_pred_mode != SINGLE_PREDICTION_ONLY) {
      vp9_prob ref_comp_p = vp9_get_pred_prob_comp_ref_p(cm, xd);
      unsigned int base_cost = vp9_cost_bit(intra_inter_p, 1);

      if (cm->comp_pred_mode == HYBRID_PREDICTION)
        base_cost += vp9_cost_bit(comp_inter_p, 1);

      ref_costs_comp[LAST_FRAME]   = base_cost + vp9_cost_bit(ref_comp_p, 0);
      ref_costs_comp[GOLDEN_FRAME] = base_cost + vp9_cost_bit(ref_comp_p, 1);
    } else {
      ref_costs_comp[LAST_FRAME]   = 512;
      ref_costs_comp[GOLDEN_FRAME] = 512;
    }
  }
}

static void store_coding_context(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
                         int mode_index,
                         int_mv *ref_mv,
                         int_mv *second_ref_mv,
                         int64_t comp_pred_diff[NB_PREDICTION_TYPES],
                         int64_t tx_size_diff[TX_MODES],
                         int64_t best_filter_diff[SWITCHABLE_FILTER_CONTEXTS]) {
  MACROBLOCKD *const xd = &x->e_mbd;

  // Take a snapshot of the coding context so it can be
  // restored if we decide to encode this way
  ctx->skip = x->skip;
  ctx->best_mode_index = mode_index;
  ctx->mic = *xd->mi_8x8[0];

  ctx->best_ref_mv.as_int = ref_mv->as_int;
  ctx->second_best_ref_mv.as_int = second_ref_mv->as_int;

  ctx->single_pred_diff = (int)comp_pred_diff[SINGLE_PREDICTION_ONLY];
  ctx->comp_pred_diff   = (int)comp_pred_diff[COMP_PREDICTION_ONLY];
  ctx->hybrid_pred_diff = (int)comp_pred_diff[HYBRID_PREDICTION];

  vpx_memcpy(ctx->tx_rd_diff, tx_size_diff, sizeof(ctx->tx_rd_diff));
  vpx_memcpy(ctx->best_filter_diff, best_filter_diff,
             sizeof(*best_filter_diff) * SWITCHABLE_FILTER_CONTEXTS);
}

static void setup_pred_block(const MACROBLOCKD *xd,
                             struct buf_2d dst[MAX_MB_PLANE],
                             const YV12_BUFFER_CONFIG *src,
                             int mi_row, int mi_col,
                             const struct scale_factors *scale,
                             const struct scale_factors *scale_uv) {
  int i;

  dst[0].buf = src->y_buffer;
  dst[0].stride = src->y_stride;
  dst[1].buf = src->u_buffer;
  dst[2].buf = src->v_buffer;
  dst[1].stride = dst[2].stride = src->uv_stride;
#if CONFIG_ALPHA
  dst[3].buf = src->alpha_buffer;
  dst[3].stride = src->alpha_stride;
#endif

  // TODO(jkoleszar): Make scale factors per-plane data
  for (i = 0; i < MAX_MB_PLANE; i++) {
    setup_pred_plane(dst + i, dst[i].buf, dst[i].stride, mi_row, mi_col,
                     i ? scale_uv : scale,
                     xd->plane[i].subsampling_x, xd->plane[i].subsampling_y);
  }
}

static void setup_buffer_inter(VP9_COMP *cpi, MACROBLOCK *x,
                               const TileInfo *const tile,
                               int idx, MV_REFERENCE_FRAME frame_type,
                               BLOCK_SIZE block_size,
                               int mi_row, int mi_col,
                               int_mv frame_nearest_mv[MAX_REF_FRAMES],
                               int_mv frame_near_mv[MAX_REF_FRAMES],
                               struct buf_2d yv12_mb[4][MAX_MB_PLANE],
                               struct scale_factors scale[MAX_REF_FRAMES]) {
  VP9_COMMON *cm = &cpi->common;
  YV12_BUFFER_CONFIG *yv12 = &cm->yv12_fb[cpi->common.ref_frame_map[idx]];
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;

  // set up scaling factors
  scale[frame_type] = cpi->common.active_ref_scale[frame_type - 1];

  scale[frame_type].sfc->set_scaled_offsets(&scale[frame_type],
                                            mi_row * MI_SIZE, mi_col * MI_SIZE);

  // TODO(jkoleszar): Is the UV buffer ever used here? If so, need to make this
  // use the UV scaling factors.
  setup_pred_block(xd, yv12_mb[frame_type], yv12, mi_row, mi_col,
                   &scale[frame_type], &scale[frame_type]);

  // Gets an initial list of candidate vectors from neighbours and orders them
  vp9_find_mv_refs(cm, xd, tile, xd->mi_8x8[0],
                   xd->last_mi,
                   frame_type,
                   mbmi->ref_mvs[frame_type], mi_row, mi_col);

  // Candidate refinement carried out at encoder and decoder
  vp9_find_best_ref_mvs(xd, cm->allow_high_precision_mv,
                        mbmi->ref_mvs[frame_type],
                        &frame_nearest_mv[frame_type],
                        &frame_near_mv[frame_type]);

  // Further refinement that is encode side only to test the top few candidates
  // in full and choose the best as the centre point for subsequent searches.
  // The current implementation doesn't support scaling.
  if (!vp9_is_scaled(scale[frame_type].sfc) && block_size >= BLOCK_8X8)
    mv_pred(cpi, x, yv12_mb[frame_type][0].buf, yv12->y_stride,
            frame_type, block_size);
}

static YV12_BUFFER_CONFIG *get_scaled_ref_frame(VP9_COMP *cpi, int ref_frame) {
  YV12_BUFFER_CONFIG *scaled_ref_frame = NULL;
  int fb = get_ref_frame_idx(cpi, ref_frame);
  int fb_scale = get_scale_ref_frame_idx(cpi, ref_frame);
  if (cpi->scaled_ref_idx[fb_scale] != cpi->common.ref_frame_map[fb])
    scaled_ref_frame = &cpi->common.yv12_fb[cpi->scaled_ref_idx[fb_scale]];
  return scaled_ref_frame;
}

static INLINE int get_switchable_rate(const MACROBLOCK *x) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi_8x8[0]->mbmi;
  const int ctx = vp9_get_pred_context_switchable_interp(xd);
  return SWITCHABLE_INTERP_RATE_FACTOR *
             x->switchable_interp_costs[ctx][mbmi->interp_filter];
}

static void single_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                 const TileInfo *const tile,
                                 BLOCK_SIZE bsize,
                                 int mi_row, int mi_col,
                                 int_mv *tmp_mv, int *rate_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  VP9_COMMON *cm = &cpi->common;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  struct buf_2d backup_yv12[MAX_MB_PLANE] = {{0}};
  int bestsme = INT_MAX;
  int further_steps, step_param;
  int sadpb = x->sadperbit16;
  int_mv mvp_full;
  int ref = mbmi->ref_frame[0];
  int_mv ref_mv = mbmi->ref_mvs[ref][0];
  const BLOCK_SIZE block_size = get_plane_block_size(bsize, &xd->plane[0]);

  int tmp_col_min = x->mv_col_min;
  int tmp_col_max = x->mv_col_max;
  int tmp_row_min = x->mv_row_min;
  int tmp_row_max = x->mv_row_max;

  YV12_BUFFER_CONFIG *scaled_ref_frame = get_scaled_ref_frame(cpi, ref);

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[0];

    setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  vp9_clamp_mv_min_max(x, &ref_mv.as_mv);

  // Adjust search parameters based on small partitions' result.
  if (x->fast_ms) {
    // && abs(mvp_full.as_mv.row - x->pred_mv.as_mv.row) < 24 &&
    // abs(mvp_full.as_mv.col - x->pred_mv.as_mv.col) < 24) {
    // adjust search range
    step_param = 6;
    if (x->fast_ms > 1)
      step_param = 8;

    // Get prediction MV.
    mvp_full.as_int = x->pred_mv[ref].as_int;

    // Adjust MV sign if needed.
    if (cm->ref_frame_sign_bias[ref]) {
      mvp_full.as_mv.col *= -1;
      mvp_full.as_mv.row *= -1;
    }
  } else {
    // Work out the size of the first step in the mv step search.
    // 0 here is maximum length first step. 1 is MAX >> 1 etc.
    if (cpi->sf.auto_mv_step_size && cpi->common.show_frame) {
      // Take wtd average of the step_params based on the last frame's
      // max mv magnitude and that based on the best ref mvs of the current
      // block for the given reference.
      step_param = (vp9_init_search_range(cpi, x->max_mv_context[ref]) +
                    cpi->mv_step_param) >> 1;
    } else {
      step_param = cpi->mv_step_param;
    }
  }

  if (cpi->sf.adaptive_motion_search && bsize < BLOCK_64X64 &&
      cpi->common.show_frame) {
    int boffset = 2 * (b_width_log2(BLOCK_64X64) - MIN(b_height_log2(bsize),
                                                       b_width_log2(bsize)));
    step_param = MAX(step_param, boffset);
  }

  mvp_full.as_int = x->mv_best_ref_index[ref] < MAX_MV_REF_CANDIDATES ?
      mbmi->ref_mvs[ref][x->mv_best_ref_index[ref]].as_int :
      x->pred_mv[ref].as_int;

  mvp_full.as_mv.col >>= 3;
  mvp_full.as_mv.row >>= 3;

  // Further step/diamond searches as necessary
  further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;

  if (cpi->sf.search_method == HEX) {
    bestsme = vp9_hex_search(x, &mvp_full.as_mv,
                             step_param,
                             sadpb, 1,
                             &cpi->fn_ptr[block_size], 1,
                             &ref_mv.as_mv, &tmp_mv->as_mv);
  } else if (cpi->sf.search_method == SQUARE) {
    bestsme = vp9_square_search(x, &mvp_full.as_mv,
                                step_param,
                                sadpb, 1,
                                &cpi->fn_ptr[block_size], 1,
                                &ref_mv.as_mv, &tmp_mv->as_mv);
  } else if (cpi->sf.search_method == BIGDIA) {
    bestsme = vp9_bigdia_search(x, &mvp_full.as_mv,
                                step_param,
                                sadpb, 1,
                                &cpi->fn_ptr[block_size], 1,
                                &ref_mv.as_mv, &tmp_mv->as_mv);
  } else {
    bestsme = vp9_full_pixel_diamond(cpi, x, &mvp_full, step_param,
                                     sadpb, further_steps, 1,
                                     &cpi->fn_ptr[block_size],
                                     &ref_mv, tmp_mv);
  }

  x->mv_col_min = tmp_col_min;
  x->mv_col_max = tmp_col_max;
  x->mv_row_min = tmp_row_min;
  x->mv_row_max = tmp_row_max;

  if (bestsme < INT_MAX) {
    int dis;  /* TODO: use dis in distortion calculation later. */
    unsigned int sse;
    cpi->find_fractional_mv_step(x, &tmp_mv->as_mv, &ref_mv.as_mv,
                                 cm->allow_high_precision_mv,
                                 x->errorperbit,
                                 &cpi->fn_ptr[block_size],
                                 0, cpi->sf.subpel_iters_per_step,
                                 x->nmvjointcost, x->mvcost,
                                 &dis, &sse);
  }
  *rate_mv = vp9_mv_bit_cost(&tmp_mv->as_mv, &ref_mv.as_mv,
                             x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);

  if (cpi->sf.adaptive_motion_search && cpi->common.show_frame)
    x->pred_mv[ref].as_int = tmp_mv->as_int;

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[0] = backup_yv12[i];
  }
}

static void joint_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                BLOCK_SIZE bsize,
                                int_mv *frame_mv,
                                int mi_row, int mi_col,
                                int_mv single_newmv[MAX_REF_FRAMES],
                                int *rate_mv) {
  int pw = 4 << b_width_log2(bsize), ph = 4 << b_height_log2(bsize);
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  const int refs[2] = { mbmi->ref_frame[0],
                        mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1] };
  int_mv ref_mv[2];
  const BLOCK_SIZE block_size = get_plane_block_size(bsize, &xd->plane[0]);
  int ite, ref;
  // Prediction buffer from second frame.
  uint8_t *second_pred = vpx_memalign(16, pw * ph * sizeof(uint8_t));

  // Do joint motion search in compound mode to get more accurate mv.
  struct buf_2d backup_yv12[2][MAX_MB_PLANE];
  struct buf_2d scaled_first_yv12 = xd->plane[0].pre[0];
  int last_besterr[2] = {INT_MAX, INT_MAX};
  YV12_BUFFER_CONFIG *const scaled_ref_frame[2] = {
    get_scaled_ref_frame(cpi, mbmi->ref_frame[0]),
    get_scaled_ref_frame(cpi, mbmi->ref_frame[1])
  };

  for (ref = 0; ref < 2; ++ref) {
    ref_mv[ref] = mbmi->ref_mvs[refs[ref]][0];

    if (scaled_ref_frame[ref]) {
      int i;
      // Swap out the reference frame for a version that's been scaled to
      // match the resolution of the current frame, allowing the existing
      // motion search code to be used without additional modifications.
      for (i = 0; i < MAX_MB_PLANE; i++)
        backup_yv12[ref][i] = xd->plane[i].pre[ref];
      setup_pre_planes(xd, ref, scaled_ref_frame[ref], mi_row, mi_col, NULL);
    }

    xd->scale_factor[ref].sfc->set_scaled_offsets(&xd->scale_factor[ref],
                                                  mi_row, mi_col);
    frame_mv[refs[ref]].as_int = single_newmv[refs[ref]].as_int;
  }

  // Allow joint search multiple times iteratively for each ref frame
  // and break out the search loop if it couldn't find better mv.
  for (ite = 0; ite < 4; ite++) {
    struct buf_2d ref_yv12[2];
    int bestsme = INT_MAX;
    int sadpb = x->sadperbit16;
    int_mv tmp_mv;
    int search_range = 3;

    int tmp_col_min = x->mv_col_min;
    int tmp_col_max = x->mv_col_max;
    int tmp_row_min = x->mv_row_min;
    int tmp_row_max = x->mv_row_max;
    int id = ite % 2;

    // Initialized here because of compiler problem in Visual Studio.
    ref_yv12[0] = xd->plane[0].pre[0];
    ref_yv12[1] = xd->plane[0].pre[1];

    // Get pred block from second frame.
    vp9_build_inter_predictor(ref_yv12[!id].buf,
                              ref_yv12[!id].stride,
                              second_pred, pw,
                              &frame_mv[refs[!id]].as_mv,
                              &xd->scale_factor[!id],
                              pw, ph, 0,
                              &xd->subpix, MV_PRECISION_Q3);

    // Compound motion search on first ref frame.
    if (id)
      xd->plane[0].pre[0] = ref_yv12[id];
    vp9_clamp_mv_min_max(x, &ref_mv[id].as_mv);

    // Use mv result from single mode as mvp.
    tmp_mv.as_int = frame_mv[refs[id]].as_int;

    tmp_mv.as_mv.col >>= 3;
    tmp_mv.as_mv.row >>= 3;

    // Small-range full-pixel motion search
    bestsme = vp9_refining_search_8p_c(x, &tmp_mv, sadpb,
                                       search_range,
                                       &cpi->fn_ptr[block_size],
                                       x->nmvjointcost, x->mvcost,
                                       &ref_mv[id], second_pred,
                                       pw, ph);

    x->mv_col_min = tmp_col_min;
    x->mv_col_max = tmp_col_max;
    x->mv_row_min = tmp_row_min;
    x->mv_row_max = tmp_row_max;

    if (bestsme < INT_MAX) {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;

      bestsme = cpi->find_fractional_mv_step_comp(
          x, &tmp_mv.as_mv,
          &ref_mv[id].as_mv,
          cpi->common.allow_high_precision_mv,
          x->errorperbit,
          &cpi->fn_ptr[block_size],
          0, cpi->sf.subpel_iters_per_step,
          x->nmvjointcost, x->mvcost,
          &dis, &sse, second_pred,
          pw, ph);
    }

    if (id)
      xd->plane[0].pre[0] = scaled_first_yv12;

    if (bestsme < last_besterr[id]) {
      frame_mv[refs[id]].as_int = tmp_mv.as_int;
      last_besterr[id] = bestsme;
    } else {
      break;
    }
  }

  *rate_mv = 0;

  for (ref = 0; ref < 2; ++ref) {
    if (scaled_ref_frame[ref]) {
      // restore the predictor
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[ref] = backup_yv12[ref][i];
    }

    *rate_mv += vp9_mv_bit_cost(&frame_mv[refs[ref]].as_mv,
                                &mbmi->ref_mvs[refs[ref]][0].as_mv,
                                x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
  }

  vpx_free(second_pred);
}

static int64_t handle_inter_mode(VP9_COMP *cpi, MACROBLOCK *x,
                                 const TileInfo *const tile,
                                 BLOCK_SIZE bsize,
                                 int64_t txfm_cache[],
                                 int *rate2, int64_t *distortion,
                                 int *skippable,
                                 int *rate_y, int64_t *distortion_y,
                                 int *rate_uv, int64_t *distortion_uv,
                                 int *mode_excluded, int *disable_skip,
                                 INTERPOLATION_TYPE *best_filter,
                                 int_mv (*mode_mv)[MAX_REF_FRAMES],
                                 int mi_row, int mi_col,
                                 int_mv single_newmv[MAX_REF_FRAMES],
                                 int64_t *psse,
                                 const int64_t ref_best_rd) {
  VP9_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  const int is_comp_pred = has_second_ref(mbmi);
  const int num_refs = is_comp_pred ? 2 : 1;
  const int this_mode = mbmi->mode;
  int_mv *frame_mv = mode_mv[this_mode];
  int i;
  int refs[2] = { mbmi->ref_frame[0],
    (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  int_mv cur_mv[2];
  int64_t this_rd = 0;
  DECLARE_ALIGNED_ARRAY(16, uint8_t, tmp_buf, MAX_MB_PLANE * 64 * 64);
  int pred_exists = 0;
  int intpel_mv;
  int64_t rd, best_rd = INT64_MAX;
  int best_needs_copy = 0;
  uint8_t *orig_dst[MAX_MB_PLANE];
  int orig_dst_stride[MAX_MB_PLANE];
  int rs = 0;

  if (is_comp_pred) {
    if (frame_mv[refs[0]].as_int == INVALID_MV ||
        frame_mv[refs[1]].as_int == INVALID_MV)
      return INT64_MAX;
  }

  if (this_mode == NEWMV) {
    int rate_mv;
    if (is_comp_pred) {
      // Initialize mv using single prediction mode result.
      frame_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;
      frame_mv[refs[1]].as_int = single_newmv[refs[1]].as_int;

      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        joint_motion_search(cpi, x, bsize, frame_mv,
                            mi_row, mi_col, single_newmv, &rate_mv);
      } else {
        rate_mv  = vp9_mv_bit_cost(&frame_mv[refs[0]].as_mv,
                                   &mbmi->ref_mvs[refs[0]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
        rate_mv += vp9_mv_bit_cost(&frame_mv[refs[1]].as_mv,
                                   &mbmi->ref_mvs[refs[1]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
      *rate2 += rate_mv;
    } else {
      int_mv tmp_mv;
      single_motion_search(cpi, x, tile, bsize, mi_row, mi_col,
                           &tmp_mv, &rate_mv);
      *rate2 += rate_mv;
      frame_mv[refs[0]].as_int =
          xd->mi_8x8[0]->bmi[0].as_mv[0].as_int = tmp_mv.as_int;
      single_newmv[refs[0]].as_int = tmp_mv.as_int;
    }
  }

  // if we're near/nearest and mv == 0,0, compare to zeromv
  if ((this_mode == NEARMV || this_mode == NEARESTMV || this_mode == ZEROMV) &&
      frame_mv[refs[0]].as_int == 0 &&
      !vp9_segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP) &&
      (num_refs == 1 || frame_mv[refs[1]].as_int == 0)) {
    int rfc = mbmi->mode_context[mbmi->ref_frame[0]];
    int c1 = cost_mv_ref(cpi, NEARMV, rfc);
    int c2 = cost_mv_ref(cpi, NEARESTMV, rfc);
    int c3 = cost_mv_ref(cpi, ZEROMV, rfc);

    if (this_mode == NEARMV) {
      if (c1 > c3)
        return INT64_MAX;
    } else if (this_mode == NEARESTMV) {
      if (c2 > c3)
        return INT64_MAX;
    } else {
      assert(this_mode == ZEROMV);
      if (num_refs == 1) {
        if ((c3 >= c2 &&
             mode_mv[NEARESTMV][mbmi->ref_frame[0]].as_int == 0) ||
            (c3 >= c1 &&
             mode_mv[NEARMV][mbmi->ref_frame[0]].as_int == 0))
          return INT64_MAX;
      } else {
        if ((c3 >= c2 &&
             mode_mv[NEARESTMV][mbmi->ref_frame[0]].as_int == 0 &&
             mode_mv[NEARESTMV][mbmi->ref_frame[1]].as_int == 0) ||
            (c3 >= c1 &&
             mode_mv[NEARMV][mbmi->ref_frame[0]].as_int == 0 &&
             mode_mv[NEARMV][mbmi->ref_frame[1]].as_int == 0))
          return INT64_MAX;
      }
    }
  }

  for (i = 0; i < num_refs; ++i) {
    cur_mv[i] = frame_mv[refs[i]];
    // Clip "next_nearest" so that it does not extend to far out of image
    if (this_mode != NEWMV)
      clamp_mv2(&cur_mv[i].as_mv, xd);

    if (mv_check_bounds(x, &cur_mv[i]))
      return INT64_MAX;
    mbmi->mv[i].as_int = cur_mv[i].as_int;
  }

  // do first prediction into the destination buffer. Do the next
  // prediction into a temporary buffer. Then keep track of which one
  // of these currently holds the best predictor, and use the other
  // one for future predictions. In the end, copy from tmp_buf to
  // dst if necessary.
  for (i = 0; i < MAX_MB_PLANE; i++) {
    orig_dst[i] = xd->plane[i].dst.buf;
    orig_dst_stride[i] = xd->plane[i].dst.stride;
  }

  /* We don't include the cost of the second reference here, because there
   * are only three options: Last/Golden, ARF/Last or Golden/ARF, or in other
   * words if you present them in that order, the second one is always known
   * if the first is known */
  *rate2 += cost_mv_ref(cpi, this_mode,
                        mbmi->mode_context[mbmi->ref_frame[0]]);

  if (!(*mode_excluded)) {
    if (is_comp_pred) {
      *mode_excluded = (cpi->common.comp_pred_mode == SINGLE_PREDICTION_ONLY);
    } else {
      *mode_excluded = (cpi->common.comp_pred_mode == COMP_PREDICTION_ONLY);
    }
  }

  pred_exists = 0;
  // Are all MVs integer pel for Y and UV
  intpel_mv = (mbmi->mv[0].as_mv.row & 15) == 0 &&
      (mbmi->mv[0].as_mv.col & 15) == 0;
  if (is_comp_pred)
    intpel_mv &= (mbmi->mv[1].as_mv.row & 15) == 0 &&
        (mbmi->mv[1].as_mv.col & 15) == 0;
  // Search for best switchable filter by checking the variance of
  // pred error irrespective of whether the filter will be used
  if (cm->mcomp_filter_type != BILINEAR) {
    *best_filter = EIGHTTAP;
    if (x->source_variance <
        cpi->sf.disable_filter_search_var_thresh) {
      *best_filter = EIGHTTAP;
      vp9_zero(cpi->rd_filter_cache);
    } else {
      int i, newbest;
      int tmp_rate_sum = 0;
      int64_t tmp_dist_sum = 0;

      cpi->rd_filter_cache[SWITCHABLE_FILTERS] = INT64_MAX;
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        int j;
        int64_t rs_rd;
        mbmi->interp_filter = i;
        vp9_setup_interp_filters(xd, mbmi->interp_filter, cm);
        rs = get_switchable_rate(x);
        rs_rd = RDCOST(x->rdmult, x->rddiv, rs, 0);

        if (i > 0 && intpel_mv) {
          cpi->rd_filter_cache[i] = RDCOST(x->rdmult, x->rddiv,
                                           tmp_rate_sum, tmp_dist_sum);
          cpi->rd_filter_cache[SWITCHABLE_FILTERS] =
              MIN(cpi->rd_filter_cache[SWITCHABLE_FILTERS],
                  cpi->rd_filter_cache[i] + rs_rd);
          rd = cpi->rd_filter_cache[i];
          if (cm->mcomp_filter_type == SWITCHABLE)
            rd += rs_rd;
        } else {
          int rate_sum = 0;
          int64_t dist_sum = 0;
          if ((cm->mcomp_filter_type == SWITCHABLE &&
               (!i || best_needs_copy)) ||
              (cm->mcomp_filter_type != SWITCHABLE &&
               (cm->mcomp_filter_type == mbmi->interp_filter ||
                (i == 0 && intpel_mv)))) {
            for (j = 0; j < MAX_MB_PLANE; j++) {
              xd->plane[j].dst.buf = orig_dst[j];
              xd->plane[j].dst.stride = orig_dst_stride[j];
            }
          } else {
            for (j = 0; j < MAX_MB_PLANE; j++) {
              xd->plane[j].dst.buf = tmp_buf + j * 64 * 64;
              xd->plane[j].dst.stride = 64;
            }
          }
          vp9_build_inter_predictors_sb(xd, mi_row, mi_col, bsize);
          model_rd_for_sb(cpi, bsize, x, xd, &rate_sum, &dist_sum);
          cpi->rd_filter_cache[i] = RDCOST(x->rdmult, x->rddiv,
                                           rate_sum, dist_sum);
          cpi->rd_filter_cache[SWITCHABLE_FILTERS] =
              MIN(cpi->rd_filter_cache[SWITCHABLE_FILTERS],
                  cpi->rd_filter_cache[i] + rs_rd);
          rd = cpi->rd_filter_cache[i];
          if (cm->mcomp_filter_type == SWITCHABLE)
            rd += rs_rd;
          if (i == 0 && intpel_mv) {
            tmp_rate_sum = rate_sum;
            tmp_dist_sum = dist_sum;
          }
        }
        if (i == 0 && cpi->sf.use_rd_breakout && ref_best_rd < INT64_MAX) {
          if (rd / 2 > ref_best_rd) {
            for (i = 0; i < MAX_MB_PLANE; i++) {
              xd->plane[i].dst.buf = orig_dst[i];
              xd->plane[i].dst.stride = orig_dst_stride[i];
            }
            return INT64_MAX;
          }
        }
        newbest = i == 0 || rd < best_rd;

        if (newbest) {
          best_rd = rd;
          *best_filter = mbmi->interp_filter;
          if (cm->mcomp_filter_type == SWITCHABLE && i && !intpel_mv)
            best_needs_copy = !best_needs_copy;
        }

        if ((cm->mcomp_filter_type == SWITCHABLE && newbest) ||
            (cm->mcomp_filter_type != SWITCHABLE &&
             cm->mcomp_filter_type == mbmi->interp_filter)) {
          pred_exists = 1;
        }
      }

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = orig_dst[i];
        xd->plane[i].dst.stride = orig_dst_stride[i];
      }
    }
  }
  // Set the appropriate filter
  mbmi->interp_filter = cm->mcomp_filter_type != SWITCHABLE ?
      cm->mcomp_filter_type : *best_filter;
  vp9_setup_interp_filters(xd, mbmi->interp_filter, cm);
  rs = cm->mcomp_filter_type == SWITCHABLE ? get_switchable_rate(x) : 0;

  if (pred_exists) {
    if (best_needs_copy) {
      // again temporarily set the buffers to local memory to prevent a memcpy
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = tmp_buf + i * 64 * 64;
        xd->plane[i].dst.stride = 64;
      }
    }
  } else {
    // Handles the special case when a filter that is not in the
    // switchable list (ex. bilinear, 6-tap) is indicated at the frame level
    vp9_build_inter_predictors_sb(xd, mi_row, mi_col, bsize);
  }


  if (cpi->sf.use_rd_breakout && ref_best_rd < INT64_MAX) {
    int tmp_rate;
    int64_t tmp_dist;
    model_rd_for_sb(cpi, bsize, x, xd, &tmp_rate, &tmp_dist);
    rd = RDCOST(x->rdmult, x->rddiv, rs + tmp_rate, tmp_dist);
    // if current pred_error modeled rd is substantially more than the best
    // so far, do not bother doing full rd
    if (rd / 2 > ref_best_rd) {
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = orig_dst[i];
        xd->plane[i].dst.stride = orig_dst_stride[i];
      }
      return INT64_MAX;
    }
  }

  if (cpi->common.mcomp_filter_type == SWITCHABLE)
    *rate2 += get_switchable_rate(x);

  if (!is_comp_pred && cpi->enable_encode_breakout) {
    if (cpi->active_map_enabled && x->active_ptr[0] == 0)
      x->skip = 1;
    else if (x->encode_breakout) {
      const BLOCK_SIZE y_size = get_plane_block_size(bsize, &xd->plane[0]);
      const BLOCK_SIZE uv_size = get_plane_block_size(bsize, &xd->plane[1]);
      unsigned int var, sse;
      // Skipping threshold for ac.
      unsigned int thresh_ac;
      // The encode_breakout input
      unsigned int encode_breakout = x->encode_breakout << 4;
      unsigned int max_thresh = 36000;

      // Use extreme low threshold for static frames to limit skipping.
      if (cpi->enable_encode_breakout == 2)
        max_thresh = 128;

      // Calculate threshold according to dequant value.
      thresh_ac = (xd->plane[0].dequant[1] * xd->plane[0].dequant[1]) / 9;

      // Use encode_breakout input if it is bigger than internal threshold.
      if (thresh_ac < encode_breakout)
        thresh_ac = encode_breakout;

      // Set a maximum for threshold to avoid big PSNR loss in low bitrate case.
      if (thresh_ac > max_thresh)
        thresh_ac = max_thresh;

      var = cpi->fn_ptr[y_size].vf(x->plane[0].src.buf, x->plane[0].src.stride,
                                   xd->plane[0].dst.buf,
                                   xd->plane[0].dst.stride, &sse);

      // Adjust threshold according to partition size.
      thresh_ac >>= 8 - (b_width_log2_lookup[bsize] +
          b_height_log2_lookup[bsize]);

      // Y skipping condition checking
      if (sse < thresh_ac || sse == 0) {
        // Skipping threshold for dc
        unsigned int thresh_dc;

        thresh_dc = (xd->plane[0].dequant[0] * xd->plane[0].dequant[0] >> 6);

        // dc skipping checking
        if ((sse - var) < thresh_dc || sse == var) {
          unsigned int sse_u, sse_v;
          unsigned int var_u, var_v;

          var_u = cpi->fn_ptr[uv_size].vf(x->plane[1].src.buf,
                                          x->plane[1].src.stride,
                                          xd->plane[1].dst.buf,
                                          xd->plane[1].dst.stride, &sse_u);

          // U skipping condition checking
          if ((sse_u * 4 < thresh_ac || sse_u == 0) &&
              (sse_u - var_u < thresh_dc || sse_u == var_u)) {
            var_v = cpi->fn_ptr[uv_size].vf(x->plane[2].src.buf,
                                            x->plane[2].src.stride,
                                            xd->plane[2].dst.buf,
                                            xd->plane[2].dst.stride, &sse_v);

            // V skipping condition checking
            if ((sse_v * 4 < thresh_ac || sse_v == 0) &&
                (sse_v - var_v < thresh_dc || sse_v == var_v)) {
              x->skip = 1;

              // The cost of skip bit needs to be added.
              *rate2 += vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd), 1);

              // Scaling factor for SSE from spatial domain to frequency domain
              // is 16. Adjust distortion accordingly.
              *distortion_uv = (sse_u + sse_v) << 4;
              *distortion = (sse << 4) + *distortion_uv;

              *disable_skip = 1;
              this_rd = RDCOST(x->rdmult, x->rddiv, *rate2, *distortion);
            }
          }
        }
      }
    }
  }

  if (!x->skip) {
    int skippable_y, skippable_uv;
    int64_t sseuv = INT64_MAX;
    int64_t rdcosty = INT64_MAX;

    // Y cost and distortion
    super_block_yrd(cpi, x, rate_y, distortion_y, &skippable_y, psse,
                    bsize, txfm_cache, ref_best_rd);

    if (*rate_y == INT_MAX) {
      *rate2 = INT_MAX;
      *distortion = INT64_MAX;
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = orig_dst[i];
        xd->plane[i].dst.stride = orig_dst_stride[i];
      }
      return INT64_MAX;
    }

    *rate2 += *rate_y;
    *distortion += *distortion_y;

    rdcosty = RDCOST(x->rdmult, x->rddiv, *rate2, *distortion);
    rdcosty = MIN(rdcosty, RDCOST(x->rdmult, x->rddiv, 0, *psse));

    super_block_uvrd(cpi, x, rate_uv, distortion_uv, &skippable_uv, &sseuv,
                     bsize, ref_best_rd - rdcosty);
    if (*rate_uv == INT_MAX) {
      *rate2 = INT_MAX;
      *distortion = INT64_MAX;
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = orig_dst[i];
        xd->plane[i].dst.stride = orig_dst_stride[i];
      }
      return INT64_MAX;
    }

    *psse += sseuv;
    *rate2 += *rate_uv;
    *distortion += *distortion_uv;
    *skippable = skippable_y && skippable_uv;
  }

  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].dst.buf = orig_dst[i];
    xd->plane[i].dst.stride = orig_dst_stride[i];
  }

  return this_rd;  // if 0, this will be re-calculated by caller
}

static void swap_block_ptr(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
                           int max_plane) {
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = x->e_mbd.plane;
  int i;

  for (i = 0; i < max_plane; ++i) {
    p[i].coeff    = ctx->coeff_pbuf[i][1];
    pd[i].qcoeff  = ctx->qcoeff_pbuf[i][1];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][1];
    pd[i].eobs    = ctx->eobs_pbuf[i][1];

    ctx->coeff_pbuf[i][1]   = ctx->coeff_pbuf[i][0];
    ctx->qcoeff_pbuf[i][1]  = ctx->qcoeff_pbuf[i][0];
    ctx->dqcoeff_pbuf[i][1] = ctx->dqcoeff_pbuf[i][0];
    ctx->eobs_pbuf[i][1]    = ctx->eobs_pbuf[i][0];

    ctx->coeff_pbuf[i][0]   = p[i].coeff;
    ctx->qcoeff_pbuf[i][0]  = pd[i].qcoeff;
    ctx->dqcoeff_pbuf[i][0] = pd[i].dqcoeff;
    ctx->eobs_pbuf[i][0]    = pd[i].eobs;
  }
}

void vp9_rd_pick_intra_mode_sb(VP9_COMP *cpi, MACROBLOCK *x,
                               int *returnrate, int64_t *returndist,
                               BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  int rate_y = 0, rate_uv = 0, rate_y_tokenonly = 0, rate_uv_tokenonly = 0;
  int y_skip = 0, uv_skip = 0;
  int64_t dist_y = 0, dist_uv = 0, tx_cache[TX_MODES] = { 0 };
  x->skip_encode = 0;
  ctx->skip = 0;
  xd->mi_8x8[0]->mbmi.ref_frame[0] = INTRA_FRAME;
  if (bsize >= BLOCK_8X8) {
    if (rd_pick_intra_sby_mode(cpi, x, &rate_y, &rate_y_tokenonly,
                               &dist_y, &y_skip, bsize, tx_cache,
                               best_rd) >= best_rd) {
      *returnrate = INT_MAX;
      return;
    }
    rd_pick_intra_sbuv_mode(cpi, x, ctx, &rate_uv, &rate_uv_tokenonly,
                            &dist_uv, &uv_skip, bsize);
  } else {
    y_skip = 0;
    if (rd_pick_intra_sub_8x8_y_mode(cpi, x, &rate_y, &rate_y_tokenonly,
                                     &dist_y, best_rd) >= best_rd) {
      *returnrate = INT_MAX;
      return;
    }
    rd_pick_intra_sbuv_mode(cpi, x, ctx, &rate_uv, &rate_uv_tokenonly,
                            &dist_uv, &uv_skip, BLOCK_8X8);
  }

  if (y_skip && uv_skip) {
    *returnrate = rate_y + rate_uv - rate_y_tokenonly - rate_uv_tokenonly +
                  vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd), 1);
    *returndist = dist_y + dist_uv;
    vp9_zero(ctx->tx_rd_diff);
  } else {
    int i;
    *returnrate = rate_y + rate_uv +
        vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd), 0);
    *returndist = dist_y + dist_uv;
    if (cpi->sf.tx_size_search_method == USE_FULL_RD)
      for (i = 0; i < TX_MODES; i++) {
        if (tx_cache[i] < INT64_MAX && tx_cache[cm->tx_mode] < INT64_MAX)
          ctx->tx_rd_diff[i] = tx_cache[i] - tx_cache[cm->tx_mode];
        else
          ctx->tx_rd_diff[i] = 0;
      }
  }

  ctx->mic = *xd->mi_8x8[0];
}

int64_t vp9_rd_pick_inter_mode_sb(VP9_COMP *cpi, MACROBLOCK *x,
                                  const TileInfo *const tile,
                                  int mi_row, int mi_col,
                                  int *returnrate,
                                  int64_t *returndistortion,
                                  BLOCK_SIZE bsize,
                                  PICK_MODE_CONTEXT *ctx,
                                  int64_t best_rd_so_far) {
  VP9_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  const struct segmentation *seg = &cm->seg;
  const BLOCK_SIZE block_size = get_plane_block_size(bsize, &xd->plane[0]);
  MB_PREDICTION_MODE this_mode;
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int comp_pred, i;
  int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
  struct buf_2d yv12_mb[4][MAX_MB_PLANE];
  int_mv single_newmv[MAX_REF_FRAMES] = { { 0 } };
  static const int flag_list[4] = { 0, VP9_LAST_FLAG, VP9_GOLD_FLAG,
                                    VP9_ALT_FLAG };
  int idx_list[4] = {0,
                     cpi->lst_fb_idx,
                     cpi->gld_fb_idx,
                     cpi->alt_fb_idx};
  int64_t best_rd = best_rd_so_far;
  int64_t best_tx_rd[TX_MODES];
  int64_t best_tx_diff[TX_MODES];
  int64_t best_pred_diff[NB_PREDICTION_TYPES];
  int64_t best_pred_rd[NB_PREDICTION_TYPES];
  int64_t best_filter_rd[SWITCHABLE_FILTER_CONTEXTS];
  int64_t best_filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  MB_MODE_INFO best_mbmode = { 0 };
  int j;
  int mode_index, best_mode_index = 0;
  unsigned int ref_costs_single[MAX_REF_FRAMES], ref_costs_comp[MAX_REF_FRAMES];
  vp9_prob comp_mode_p;
  int64_t best_intra_rd = INT64_MAX;
  int64_t best_inter_rd = INT64_MAX;
  MB_PREDICTION_MODE best_intra_mode = DC_PRED;
  MV_REFERENCE_FRAME best_inter_ref_frame = LAST_FRAME;
  INTERPOLATION_TYPE tmp_best_filter = SWITCHABLE;
  int rate_uv_intra[TX_SIZES], rate_uv_tokenonly[TX_SIZES];
  int64_t dist_uv[TX_SIZES];
  int skip_uv[TX_SIZES];
  MB_PREDICTION_MODE mode_uv[TX_SIZES];
  struct scale_factors scale_factor[4];
  unsigned int ref_frame_mask = 0;
  unsigned int mode_mask = 0;
  int64_t mode_distortions[MB_MODE_COUNT] = {-1};
  int64_t frame_distortions[MAX_REF_FRAMES] = {-1};
  int intra_cost_penalty = 20 * vp9_dc_quant(cm->base_qindex, cm->y_dc_delta_q);
  const int bws = num_8x8_blocks_wide_lookup[bsize] / 2;
  const int bhs = num_8x8_blocks_high_lookup[bsize] / 2;
  int best_skip2 = 0;

  x->skip_encode = cpi->sf.skip_encode_frame && x->q_index < QIDX_SKIP_THRESH;

  // Everywhere the flag is set the error is much higher than its neighbors.
  ctx->frames_with_high_error = 0;
  ctx->modes_with_high_error = 0;

  estimate_ref_frame_costs(cpi, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < NB_PREDICTION_TYPES; ++i)
    best_pred_rd[i] = INT64_MAX;
  for (i = 0; i < TX_MODES; i++)
    best_tx_rd[i] = INT64_MAX;
  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
    best_filter_rd[i] = INT64_MAX;
  for (i = 0; i < TX_SIZES; i++)
    rate_uv_intra[i] = INT_MAX;

  *returnrate = INT_MAX;

  // Create a mask set to 1 for each reference frame used by a smaller
  // resolution.
  if (cpi->sf.use_avoid_tested_higherror) {
    switch (block_size) {
      case BLOCK_64X64:
        for (i = 0; i < 4; i++) {
          for (j = 0; j < 4; j++) {
            ref_frame_mask |= x->mb_context[i][j].frames_with_high_error;
            mode_mask |= x->mb_context[i][j].modes_with_high_error;
          }
        }
        for (i = 0; i < 4; i++) {
          ref_frame_mask |= x->sb32_context[i].frames_with_high_error;
          mode_mask |= x->sb32_context[i].modes_with_high_error;
        }
        break;
      case BLOCK_32X32:
        for (i = 0; i < 4; i++) {
          ref_frame_mask |=
              x->mb_context[x->sb_index][i].frames_with_high_error;
          mode_mask |= x->mb_context[x->sb_index][i].modes_with_high_error;
        }
        break;
      default:
        // Until we handle all block sizes set it to present;
        ref_frame_mask = 0;
        mode_mask = 0;
        break;
    }
    ref_frame_mask = ~ref_frame_mask;
    mode_mask = ~mode_mask;
  }

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      setup_buffer_inter(cpi, x, tile, idx_list[ref_frame], ref_frame,
                         block_size, mi_row, mi_col,
                         frame_mv[NEARESTMV], frame_mv[NEARMV],
                         yv12_mb, scale_factor);
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[ZEROMV][ref_frame].as_int = 0;
  }

  for (mode_index = 0; mode_index < MAX_MODES; ++mode_index) {
    int mode_excluded = 0;
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int compmode_cost = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0, distortion_y = 0, distortion_uv = 0;
    int skippable = 0;
    int64_t tx_cache[TX_MODES];
    int i;
    int this_skip2 = 0;
    int64_t total_sse = INT_MAX;
    int early_term = 0;

    for (i = 0; i < TX_MODES; ++i)
      tx_cache[i] = INT64_MAX;

    x->skip = 0;
    this_mode = vp9_mode_order[mode_index].mode;
    ref_frame = vp9_mode_order[mode_index].ref_frame;
    second_ref_frame = vp9_mode_order[mode_index].second_ref_frame;

    // Look at the reference frame of the best mode so far and set the
    // skip mask to look at a subset of the remaining modes.
    if (mode_index > cpi->sf.mode_skip_start) {
      if (mode_index == (cpi->sf.mode_skip_start + 1)) {
        switch (vp9_mode_order[best_mode_index].ref_frame) {
          case INTRA_FRAME:
            cpi->mode_skip_mask = 0;
            break;
          case LAST_FRAME:
            cpi->mode_skip_mask = LAST_FRAME_MODE_MASK;
            break;
          case GOLDEN_FRAME:
            cpi->mode_skip_mask = GOLDEN_FRAME_MODE_MASK;
            break;
          case ALTREF_FRAME:
            cpi->mode_skip_mask = ALT_REF_MODE_MASK;
            break;
          case NONE:
          case MAX_REF_FRAMES:
            assert(!"Invalid Reference frame");
        }
      }
      if (cpi->mode_skip_mask & ((int64_t)1 << mode_index))
        continue;
    }

    // Skip if the current reference frame has been masked off
    if (cpi->sf.reference_masking && !cpi->set_ref_frame_mask &&
        (cpi->ref_frame_mask & (1 << ref_frame)))
      continue;

    // Test best rd so far against threshold for trying this mode.
    if ((best_rd < ((int64_t)cpi->rd_threshes[segment_id][bsize][mode_index] *
                     cpi->rd_thresh_freq_fact[bsize][mode_index] >> 5)) ||
        cpi->rd_threshes[segment_id][bsize][mode_index] == INT_MAX)
      continue;

    // Do not allow compound prediction if the segment level reference
    // frame feature is in use as in this case there can only be one reference.
    if ((second_ref_frame > INTRA_FRAME) &&
         vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME))
      continue;

    // Skip some checking based on small partitions' result.
    if (x->fast_ms > 1 && !ref_frame)
      continue;
    if (x->fast_ms > 2 && ref_frame != x->subblock_ref)
      continue;

    if (cpi->sf.use_avoid_tested_higherror && bsize >= BLOCK_8X8) {
      if (!(ref_frame_mask & (1 << ref_frame))) {
        continue;
      }
      if (!(mode_mask & (1 << this_mode))) {
        continue;
      }
      if (second_ref_frame != NONE
          && !(ref_frame_mask & (1 << second_ref_frame))) {
        continue;
      }
    }

    mbmi->ref_frame[0] = ref_frame;
    mbmi->ref_frame[1] = second_ref_frame;

    if (!(ref_frame == INTRA_FRAME
        || (cpi->ref_frame_flags & flag_list[ref_frame]))) {
      continue;
    }
    if (!(second_ref_frame == NONE
        || (cpi->ref_frame_flags & flag_list[second_ref_frame]))) {
      continue;
    }

    comp_pred = second_ref_frame > INTRA_FRAME;
    if (comp_pred) {
      if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_COMP_BESTINTRA)
        if (vp9_mode_order[best_mode_index].ref_frame == INTRA_FRAME)
          continue;
      if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_COMP_REFMISMATCH)
        if (ref_frame != best_inter_ref_frame &&
            second_ref_frame != best_inter_ref_frame)
          continue;
    }

    set_scale_factors(xd, ref_frame, second_ref_frame, scale_factor);
    mbmi->uv_mode = DC_PRED;

    // Evaluate all sub-pel filters irrespective of whether we can use
    // them for this frame.
    mbmi->interp_filter = cm->mcomp_filter_type;
    vp9_setup_interp_filters(xd, mbmi->interp_filter, cm);

    if (comp_pred) {
      if (!(cpi->ref_frame_flags & flag_list[second_ref_frame]))
        continue;
      set_scale_factors(xd, ref_frame, second_ref_frame, scale_factor);

      mode_excluded = mode_excluded
                         ? mode_excluded
                         : cm->comp_pred_mode == SINGLE_PREDICTION_ONLY;
    } else {
      if (ref_frame != INTRA_FRAME && second_ref_frame != INTRA_FRAME) {
        mode_excluded =
            mode_excluded ?
                mode_excluded : cm->comp_pred_mode == COMP_PREDICTION_ONLY;
      }
    }

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred)
        xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

    // If the segment reference frame feature is enabled....
    // then do nothing if the current ref frame is not allowed..
    if (vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
        vp9_get_segdata(seg, segment_id, SEG_LVL_REF_FRAME) !=
            (int)ref_frame) {
      continue;
    // If the segment skip feature is enabled....
    // then do nothing if the current mode is not allowed..
    } else if (vp9_segfeature_active(seg, segment_id, SEG_LVL_SKIP) &&
               (this_mode != ZEROMV && ref_frame != INTRA_FRAME)) {
      continue;
    // Disable this drop out case if the ref frame
    // segment level feature is enabled for this segment. This is to
    // prevent the possibility that we end up unable to pick any mode.
    } else if (!vp9_segfeature_active(seg, segment_id,
                                      SEG_LVL_REF_FRAME)) {
      // Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
      // unless ARNR filtering is enabled in which case we want
      // an unfiltered alternative. We allow near/nearest as well
      // because they may result in zero-zero MVs but be cheaper.
      if (cpi->is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0)) {
        if ((this_mode != ZEROMV &&
             !(this_mode == NEARMV &&
               frame_mv[NEARMV][ALTREF_FRAME].as_int == 0) &&
             !(this_mode == NEARESTMV &&
               frame_mv[NEARESTMV][ALTREF_FRAME].as_int == 0)) ||
            ref_frame != ALTREF_FRAME) {
          continue;
        }
      }
    }
    // TODO(JBB): This is to make up for the fact that we don't have sad
    // functions that work when the block size reads outside the umv.  We
    // should fix this either by making the motion search just work on
    // a representative block in the boundary ( first ) and then implement a
    // function that does sads when inside the border..
    if (((mi_row + bhs) > cm->mi_rows || (mi_col + bws) > cm->mi_cols) &&
        this_mode == NEWMV) {
      continue;
    }

#ifdef MODE_TEST_HIT_STATS
    // TEST/DEBUG CODE
    // Keep a rcord of the number of test hits at each size
    cpi->mode_test_hits[bsize]++;
#endif


    if (ref_frame == INTRA_FRAME) {
      TX_SIZE uv_tx;
      // Disable intra modes other than DC_PRED for blocks with low variance
      // Threshold for intra skipping based on source variance
      // TODO(debargha): Specialize the threshold for super block sizes
      static const unsigned int skip_intra_var_thresh[BLOCK_SIZES] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      };
      if ((cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_LOWVAR) &&
          this_mode != DC_PRED &&
          x->source_variance < skip_intra_var_thresh[mbmi->sb_type])
        continue;
      // Only search the oblique modes if the best so far is
      // one of the neighboring directional modes
      if ((cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_BESTINTER) &&
          (this_mode >= D45_PRED && this_mode <= TM_PRED)) {
        if (vp9_mode_order[best_mode_index].ref_frame > INTRA_FRAME)
          continue;
      }
      mbmi->mode = this_mode;
      if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
        if (conditional_skipintra(mbmi->mode, best_intra_mode))
            continue;
      }

      super_block_yrd(cpi, x, &rate_y, &distortion_y, &skippable, NULL,
                      bsize, tx_cache, best_rd);

      if (rate_y == INT_MAX)
        continue;

      uv_tx = MIN(mbmi->tx_size, max_uv_txsize_lookup[bsize]);
      if (rate_uv_intra[uv_tx] == INT_MAX) {
        choose_intra_uv_mode(cpi, ctx, bsize, &rate_uv_intra[uv_tx],
                             &rate_uv_tokenonly[uv_tx],
                             &dist_uv[uv_tx], &skip_uv[uv_tx],
                             &mode_uv[uv_tx]);
      }

      rate_uv = rate_uv_tokenonly[uv_tx];
      distortion_uv = dist_uv[uv_tx];
      skippable = skippable && skip_uv[uv_tx];
      mbmi->uv_mode = mode_uv[uv_tx];

      rate2 = rate_y + x->mbmode_cost[mbmi->mode] + rate_uv_intra[uv_tx];
      if (this_mode != DC_PRED && this_mode != TM_PRED)
        rate2 += intra_cost_penalty;
      distortion2 = distortion_y + distortion_uv;
    } else {
      mbmi->mode = this_mode;
      compmode_cost = vp9_cost_bit(comp_mode_p, second_ref_frame > INTRA_FRAME);
      this_rd = handle_inter_mode(cpi, x, tile, bsize,
                                  tx_cache,
                                  &rate2, &distortion2, &skippable,
                                  &rate_y, &distortion_y,
                                  &rate_uv, &distortion_uv,
                                  &mode_excluded, &disable_skip,
                                  &tmp_best_filter, frame_mv,
                                  mi_row, mi_col,
                                  single_newmv, &total_sse, best_rd);
      if (this_rd == INT64_MAX)
        continue;
    }

    if (cm->comp_pred_mode == HYBRID_PREDICTION) {
      rate2 += compmode_cost;
    }

    // Estimate the reference frame signaling cost and add it
    // to the rolling cost variable.
    if (second_ref_frame > INTRA_FRAME) {
      rate2 += ref_costs_comp[ref_frame];
    } else {
      rate2 += ref_costs_single[ref_frame];
    }

    if (!disable_skip) {
      // Test for the condition where skip block will be activated
      // because there are no non zero coefficients and make any
      // necessary adjustment for rate. Ignore if skip is coded at
      // segment level as the cost wont have been added in.
      // Is Mb level skip allowed (i.e. not coded at segment level).
      const int mb_skip_allowed = !vp9_segfeature_active(seg, segment_id,
                                                         SEG_LVL_SKIP);

      if (skippable) {
        // Back out the coefficient coding costs
        rate2 -= (rate_y + rate_uv);
        // for best yrd calculation
        rate_uv = 0;

        if (mb_skip_allowed) {
          int prob_skip_cost;

          // Cost the skip mb case
          vp9_prob skip_prob =
            vp9_get_pred_prob_mbskip(cm, xd);

          if (skip_prob) {
            prob_skip_cost = vp9_cost_bit(skip_prob, 1);
            rate2 += prob_skip_cost;
          }
        }
      } else if (mb_skip_allowed && ref_frame != INTRA_FRAME && !xd->lossless) {
        if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv, distortion2) <
            RDCOST(x->rdmult, x->rddiv, 0, total_sse)) {
          // Add in the cost of the no skip flag.
          int prob_skip_cost = vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd),
                                            0);
          rate2 += prob_skip_cost;
        } else {
          // FIXME(rbultje) make this work for splitmv also
          int prob_skip_cost = vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd),
                                            1);
          rate2 += prob_skip_cost;
          distortion2 = total_sse;
          assert(total_sse >= 0);
          rate2 -= (rate_y + rate_uv);
          rate_y = 0;
          rate_uv = 0;
          this_skip2 = 1;
        }
      } else if (mb_skip_allowed) {
        // Add in the cost of the no skip flag.
        int prob_skip_cost = vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd),
                                          0);
        rate2 += prob_skip_cost;
      }

      // Calculate the final RD estimate for this mode.
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
    }

    // Keep record of best intra rd
    if (!is_inter_block(&xd->mi_8x8[0]->mbmi) &&
        this_rd < best_intra_rd) {
      best_intra_rd = this_rd;
      best_intra_mode = xd->mi_8x8[0]->mbmi.mode;
    }

    // Keep record of best inter rd with single reference
    if (is_inter_block(&xd->mi_8x8[0]->mbmi) &&
        !has_second_ref(&xd->mi_8x8[0]->mbmi) &&
        !mode_excluded && this_rd < best_inter_rd) {
      best_inter_rd = this_rd;
      best_inter_ref_frame = ref_frame;
    }

    if (!disable_skip && ref_frame == INTRA_FRAME) {
      for (i = 0; i < NB_PREDICTION_TYPES; ++i)
        best_pred_rd[i] = MIN(best_pred_rd[i], this_rd);
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
        best_filter_rd[i] = MIN(best_filter_rd[i], this_rd);
    }

    // Store the respective mode distortions for later use.
    if (mode_distortions[this_mode] == -1
        || distortion2 < mode_distortions[this_mode]) {
      mode_distortions[this_mode] = distortion2;
    }
    if (frame_distortions[ref_frame] == -1
        || distortion2 < frame_distortions[ref_frame]) {
      frame_distortions[ref_frame] = distortion2;
    }

    // Did this mode help.. i.e. is it the new best mode
    if (this_rd < best_rd || x->skip) {
      int max_plane = MAX_MB_PLANE;
      if (!mode_excluded) {
        // Note index of best mode so far
        best_mode_index = mode_index;

        if (ref_frame == INTRA_FRAME) {
          /* required for left and above block mv */
          mbmi->mv[0].as_int = 0;
          max_plane = 1;
        }

        *returnrate = rate2;
        *returndistortion = distortion2;
        best_rd = this_rd;
        best_mbmode = *mbmi;
        best_skip2 = this_skip2;
        if (!x->select_txfm_size)
          swap_block_ptr(x, ctx, max_plane);
        vpx_memcpy(ctx->zcoeff_blk, x->zcoeff_blk[mbmi->tx_size],
                   sizeof(uint8_t) * ctx->num_4x4_blk);

        // TODO(debargha): enhance this test with a better distortion prediction
        // based on qp, activity mask and history
        if ((cpi->sf.mode_search_skip_flags & FLAG_EARLY_TERMINATE) &&
            (mode_index > MIN_EARLY_TERM_INDEX)) {
          const int qstep = xd->plane[0].dequant[1];
          // TODO(debargha): Enhance this by specializing for each mode_index
          int scale = 4;
          if (x->source_variance < UINT_MAX) {
            const int var_adjust = (x->source_variance < 16);
            scale -= var_adjust;
          }
          if (ref_frame > INTRA_FRAME &&
              distortion2 * scale < qstep * qstep) {
            early_term = 1;
          }
        }
      }
    }

    /* keep record of best compound/single-only prediction */
    if (!disable_skip && ref_frame != INTRA_FRAME) {
      int single_rd, hybrid_rd, single_rate, hybrid_rate;

      if (cm->comp_pred_mode == HYBRID_PREDICTION) {
        single_rate = rate2 - compmode_cost;
        hybrid_rate = rate2;
      } else {
        single_rate = rate2;
        hybrid_rate = rate2 + compmode_cost;
      }

      single_rd = RDCOST(x->rdmult, x->rddiv, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, x->rddiv, hybrid_rate, distortion2);

      if (second_ref_frame <= INTRA_FRAME &&
          single_rd < best_pred_rd[SINGLE_PREDICTION_ONLY]) {
        best_pred_rd[SINGLE_PREDICTION_ONLY] = single_rd;
      } else if (second_ref_frame > INTRA_FRAME &&
                 single_rd < best_pred_rd[COMP_PREDICTION_ONLY]) {
        best_pred_rd[COMP_PREDICTION_ONLY] = single_rd;
      }
      if (hybrid_rd < best_pred_rd[HYBRID_PREDICTION])
        best_pred_rd[HYBRID_PREDICTION] = hybrid_rd;
    }

    /* keep record of best filter type */
    if (!mode_excluded && !disable_skip && ref_frame != INTRA_FRAME &&
        cm->mcomp_filter_type != BILINEAR) {
      int64_t ref = cpi->rd_filter_cache[cm->mcomp_filter_type == SWITCHABLE ?
                              SWITCHABLE_FILTERS : cm->mcomp_filter_type];
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
        int64_t adj_rd;
        // In cases of poor prediction, filter_cache[] can contain really big
        // values, which actually are bigger than this_rd itself. This can
        // cause negative best_filter_rd[] values, which is obviously silly.
        // Therefore, if filter_cache < ref, we do an adjusted calculation.
        if (cpi->rd_filter_cache[i] >= ref) {
          adj_rd = this_rd + cpi->rd_filter_cache[i] - ref;
        } else {
          // FIXME(rbultje) do this for comppsred also
          //
          // To prevent out-of-range computation in
          //    adj_rd = cpi->rd_filter_cache[i] * this_rd / ref
          // cpi->rd_filter_cache[i] / ref is converted to a 256 based ratio.
          int tmp = cpi->rd_filter_cache[i] * 256 / ref;
          adj_rd = (this_rd * tmp) >> 8;
        }
        best_filter_rd[i] = MIN(best_filter_rd[i], adj_rd);
      }
    }

    /* keep record of best txfm size */
    if (bsize < BLOCK_32X32) {
      if (bsize < BLOCK_16X16)
        tx_cache[ALLOW_16X16] = tx_cache[ALLOW_8X8];

      tx_cache[ALLOW_32X32] = tx_cache[ALLOW_16X16];
    }
    if (!mode_excluded && this_rd != INT64_MAX) {
      for (i = 0; i < TX_MODES && tx_cache[i] < INT64_MAX; i++) {
        int64_t adj_rd = INT64_MAX;
        adj_rd = this_rd + tx_cache[i] - tx_cache[cm->tx_mode];

        if (adj_rd < best_tx_rd[i])
          best_tx_rd[i] = adj_rd;
      }
    }

    if (early_term)
      break;

    if (x->skip && !comp_pred)
      break;
  }

  if (best_rd >= best_rd_so_far)
    return INT64_MAX;

  // If we used an estimate for the uv intra rd in the loop above...
  if (cpi->sf.use_uv_intra_rd_estimate) {
    // Do Intra UV best rd mode selection if best mode choice above was intra.
    if (vp9_mode_order[best_mode_index].ref_frame == INTRA_FRAME) {
      TX_SIZE uv_tx_size = get_uv_tx_size(mbmi);
      rd_pick_intra_sbuv_mode(cpi, x, ctx, &rate_uv_intra[uv_tx_size],
                              &rate_uv_tokenonly[uv_tx_size],
                              &dist_uv[uv_tx_size],
                              &skip_uv[uv_tx_size],
                              bsize < BLOCK_8X8 ? BLOCK_8X8 : bsize);
    }
  }

  // If we are using reference masking and the set mask flag is set then
  // create the reference frame mask.
  if (cpi->sf.reference_masking && cpi->set_ref_frame_mask)
    cpi->ref_frame_mask = ~(1 << vp9_mode_order[best_mode_index].ref_frame);

  // Flag all modes that have a distortion thats > 2x the best we found at
  // this level.
  for (mode_index = 0; mode_index < MB_MODE_COUNT; ++mode_index) {
    if (mode_index == NEARESTMV || mode_index == NEARMV || mode_index == NEWMV)
      continue;

    if (mode_distortions[mode_index] > 2 * *returndistortion) {
      ctx->modes_with_high_error |= (1 << mode_index);
    }
  }

  // Flag all ref frames that have a distortion thats > 2x the best we found at
  // this level.
  for (ref_frame = INTRA_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
    if (frame_distortions[ref_frame] > 2 * *returndistortion) {
      ctx->frames_with_high_error |= (1 << ref_frame);
    }
  }

  assert((cm->mcomp_filter_type == SWITCHABLE) ||
         (cm->mcomp_filter_type == best_mbmode.interp_filter) ||
         (best_mbmode.ref_frame[0] == INTRA_FRAME));

  // Updating rd_thresh_freq_fact[] here means that the different
  // partition/block sizes are handled independently based on the best
  // choice for the current partition. It may well be better to keep a scaled
  // best rd so far value and update rd_thresh_freq_fact based on the mode/size
  // combination that wins out.
  if (cpi->sf.adaptive_rd_thresh) {
    for (mode_index = 0; mode_index < MAX_MODES; ++mode_index) {
      if (mode_index == best_mode_index) {
        cpi->rd_thresh_freq_fact[bsize][mode_index] -=
          (cpi->rd_thresh_freq_fact[bsize][mode_index] >> 3);
      } else {
        cpi->rd_thresh_freq_fact[bsize][mode_index] += RD_THRESH_INC;
        if (cpi->rd_thresh_freq_fact[bsize][mode_index] >
            (cpi->sf.adaptive_rd_thresh * RD_THRESH_MAX_FACT)) {
          cpi->rd_thresh_freq_fact[bsize][mode_index] =
            cpi->sf.adaptive_rd_thresh * RD_THRESH_MAX_FACT;
        }
      }
    }
  }

  // macroblock modes
  *mbmi = best_mbmode;
  x->skip |= best_skip2;

  for (i = 0; i < NB_PREDICTION_TYPES; ++i) {
    if (best_pred_rd[i] == INT64_MAX)
      best_pred_diff[i] = INT_MIN;
    else
      best_pred_diff[i] = best_rd - best_pred_rd[i];
  }

  if (!x->skip) {
    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
      if (best_filter_rd[i] == INT64_MAX)
        best_filter_diff[i] = 0;
      else
        best_filter_diff[i] = best_rd - best_filter_rd[i];
    }
    if (cm->mcomp_filter_type == SWITCHABLE)
      assert(best_filter_diff[SWITCHABLE_FILTERS] == 0);
  } else {
    vp9_zero(best_filter_diff);
  }

  if (!x->skip) {
    for (i = 0; i < TX_MODES; i++) {
      if (best_tx_rd[i] == INT64_MAX)
        best_tx_diff[i] = 0;
      else
        best_tx_diff[i] = best_rd - best_tx_rd[i];
    }
  } else {
    vp9_zero(best_tx_diff);
  }

  set_scale_factors(xd, mbmi->ref_frame[0], mbmi->ref_frame[1],
                    scale_factor);
  store_coding_context(x, ctx, best_mode_index,
                       &mbmi->ref_mvs[mbmi->ref_frame[0]][0],
                       &mbmi->ref_mvs[mbmi->ref_frame[1] < 0 ? 0 :
                                      mbmi->ref_frame[1]][0],
                       best_pred_diff, best_tx_diff, best_filter_diff);

  return best_rd;
}


int64_t vp9_rd_pick_inter_mode_sub8x8(VP9_COMP *cpi, MACROBLOCK *x,
                                      const TileInfo *const tile,
                                      int mi_row, int mi_col,
                                      int *returnrate,
                                      int64_t *returndistortion,
                                      BLOCK_SIZE bsize,
                                      PICK_MODE_CONTEXT *ctx,
                                      int64_t best_rd_so_far) {
  VP9_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  const struct segmentation *seg = &cm->seg;
  const BLOCK_SIZE block_size = get_plane_block_size(bsize, &xd->plane[0]);
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int comp_pred, i;
  int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
  struct buf_2d yv12_mb[4][MAX_MB_PLANE];
  static const int flag_list[4] = { 0, VP9_LAST_FLAG, VP9_GOLD_FLAG,
                                    VP9_ALT_FLAG };
  int idx_list[4] = {0,
                     cpi->lst_fb_idx,
                     cpi->gld_fb_idx,
                     cpi->alt_fb_idx};
  int64_t best_rd = best_rd_so_far;
  int64_t best_yrd = best_rd_so_far;  // FIXME(rbultje) more precise
  int64_t best_tx_rd[TX_MODES];
  int64_t best_tx_diff[TX_MODES];
  int64_t best_pred_diff[NB_PREDICTION_TYPES];
  int64_t best_pred_rd[NB_PREDICTION_TYPES];
  int64_t best_filter_rd[SWITCHABLE_FILTER_CONTEXTS];
  int64_t best_filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  MB_MODE_INFO best_mbmode = { 0 };
  int mode_index, best_mode_index = 0;
  unsigned int ref_costs_single[MAX_REF_FRAMES], ref_costs_comp[MAX_REF_FRAMES];
  vp9_prob comp_mode_p;
  int64_t best_inter_rd = INT64_MAX;
  MV_REFERENCE_FRAME best_inter_ref_frame = LAST_FRAME;
  INTERPOLATION_TYPE tmp_best_filter = SWITCHABLE;
  int rate_uv_intra[TX_SIZES], rate_uv_tokenonly[TX_SIZES];
  int64_t dist_uv[TX_SIZES];
  int skip_uv[TX_SIZES];
  MB_PREDICTION_MODE mode_uv[TX_SIZES] = { 0 };
  struct scale_factors scale_factor[4];
  unsigned int ref_frame_mask = 0;
  unsigned int mode_mask = 0;
  int intra_cost_penalty = 20 * vp9_dc_quant(cpi->common.base_qindex,
                                             cpi->common.y_dc_delta_q);
  int_mv seg_mvs[4][MAX_REF_FRAMES];
  b_mode_info best_bmodes[4];
  int best_skip2 = 0;

  x->skip_encode = cpi->sf.skip_encode_frame && x->q_index < QIDX_SKIP_THRESH;
  vpx_memset(x->zcoeff_blk[TX_4X4], 0, 4);

  for (i = 0; i < 4; i++) {
    int j;
    for (j = 0; j < MAX_REF_FRAMES; j++)
      seg_mvs[i][j].as_int = INVALID_MV;
  }

  estimate_ref_frame_costs(cpi, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < NB_PREDICTION_TYPES; ++i)
    best_pred_rd[i] = INT64_MAX;
  for (i = 0; i < TX_MODES; i++)
    best_tx_rd[i] = INT64_MAX;
  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
    best_filter_rd[i] = INT64_MAX;
  for (i = 0; i < TX_SIZES; i++)
    rate_uv_intra[i] = INT_MAX;

  *returnrate = INT_MAX;

  // Create a mask set to 1 for each reference frame used by a smaller
  // resolution.
  if (cpi->sf.use_avoid_tested_higherror) {
    ref_frame_mask = 0;
    mode_mask = 0;
    ref_frame_mask = ~ref_frame_mask;
    mode_mask = ~mode_mask;
  }

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      setup_buffer_inter(cpi, x, tile, idx_list[ref_frame], ref_frame,
                         block_size, mi_row, mi_col,
                         frame_mv[NEARESTMV], frame_mv[NEARMV],
                         yv12_mb, scale_factor);
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[ZEROMV][ref_frame].as_int = 0;
  }

  for (mode_index = 0; mode_index < MAX_REFS; ++mode_index) {
    int mode_excluded = 0;
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int compmode_cost = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0, distortion_y = 0, distortion_uv = 0;
    int skippable = 0;
    int64_t tx_cache[TX_MODES];
    int i;
    int this_skip2 = 0;
    int64_t total_sse = INT_MAX;
    int early_term = 0;

    for (i = 0; i < TX_MODES; ++i)
      tx_cache[i] = INT64_MAX;

    x->skip = 0;
    ref_frame = vp9_ref_order[mode_index].ref_frame;
    second_ref_frame = vp9_ref_order[mode_index].second_ref_frame;

    // Look at the reference frame of the best mode so far and set the
    // skip mask to look at a subset of the remaining modes.
    if (mode_index > 2 && cpi->sf.mode_skip_start < MAX_MODES) {
      if (mode_index == 3) {
        switch (vp9_ref_order[best_mode_index].ref_frame) {
          case INTRA_FRAME:
            cpi->mode_skip_mask = 0;
            break;
          case LAST_FRAME:
            cpi->mode_skip_mask = 0x0010;
            break;
          case GOLDEN_FRAME:
            cpi->mode_skip_mask = 0x0008;
            break;
          case ALTREF_FRAME:
            cpi->mode_skip_mask = 0x0000;
            break;
          case NONE:
          case MAX_REF_FRAMES:
            assert(!"Invalid Reference frame");
        }
      }
      if (cpi->mode_skip_mask & ((int64_t)1 << mode_index))
        continue;
    }

    // Skip if the current reference frame has been masked off
    if (cpi->sf.reference_masking && !cpi->set_ref_frame_mask &&
        (cpi->ref_frame_mask & (1 << ref_frame)))
      continue;

    // Test best rd so far against threshold for trying this mode.
    if ((best_rd <
         ((int64_t)cpi->rd_thresh_sub8x8[segment_id][bsize][mode_index] *
          cpi->rd_thresh_freq_sub8x8[bsize][mode_index] >> 5)) ||
        cpi->rd_thresh_sub8x8[segment_id][bsize][mode_index] == INT_MAX)
      continue;

    // Do not allow compound prediction if the segment level reference
    // frame feature is in use as in this case there can only be one reference.
    if ((second_ref_frame > INTRA_FRAME) &&
         vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME))
      continue;

    mbmi->ref_frame[0] = ref_frame;
    mbmi->ref_frame[1] = second_ref_frame;

    if (!(ref_frame == INTRA_FRAME
        || (cpi->ref_frame_flags & flag_list[ref_frame]))) {
      continue;
    }
    if (!(second_ref_frame == NONE
        || (cpi->ref_frame_flags & flag_list[second_ref_frame]))) {
      continue;
    }

    comp_pred = second_ref_frame > INTRA_FRAME;
    if (comp_pred) {
      if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_COMP_BESTINTRA)
        if (vp9_ref_order[best_mode_index].ref_frame == INTRA_FRAME)
          continue;
      if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_COMP_REFMISMATCH)
        if (ref_frame != best_inter_ref_frame &&
            second_ref_frame != best_inter_ref_frame)
          continue;
    }

    // TODO(jingning, jkoleszar): scaling reference frame not supported for
    // sub8x8 blocks.
    if (ref_frame > 0 &&
        vp9_is_scaled(scale_factor[ref_frame].sfc))
      continue;

    if (second_ref_frame > 0 &&
        vp9_is_scaled(scale_factor[second_ref_frame].sfc))
      continue;

    set_scale_factors(xd, ref_frame, second_ref_frame, scale_factor);
    mbmi->uv_mode = DC_PRED;

    // Evaluate all sub-pel filters irrespective of whether we can use
    // them for this frame.
    mbmi->interp_filter = cm->mcomp_filter_type;
    vp9_setup_interp_filters(xd, mbmi->interp_filter, &cpi->common);

    if (comp_pred) {
      if (!(cpi->ref_frame_flags & flag_list[second_ref_frame]))
        continue;
      set_scale_factors(xd, ref_frame, second_ref_frame, scale_factor);

      mode_excluded = mode_excluded
                         ? mode_excluded
                         : cm->comp_pred_mode == SINGLE_PREDICTION_ONLY;
    } else {
      if (ref_frame != INTRA_FRAME && second_ref_frame != INTRA_FRAME) {
        mode_excluded =
            mode_excluded ?
                mode_excluded : cm->comp_pred_mode == COMP_PREDICTION_ONLY;
      }
    }

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred)
        xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

    // If the segment reference frame feature is enabled....
    // then do nothing if the current ref frame is not allowed..
    if (vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
        vp9_get_segdata(seg, segment_id, SEG_LVL_REF_FRAME) !=
            (int)ref_frame) {
      continue;
    // If the segment skip feature is enabled....
    // then do nothing if the current mode is not allowed..
    } else if (vp9_segfeature_active(seg, segment_id, SEG_LVL_SKIP) &&
               ref_frame != INTRA_FRAME) {
      continue;
    // Disable this drop out case if the ref frame
    // segment level feature is enabled for this segment. This is to
    // prevent the possibility that we end up unable to pick any mode.
    } else if (!vp9_segfeature_active(seg, segment_id,
                                      SEG_LVL_REF_FRAME)) {
      // Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
      // unless ARNR filtering is enabled in which case we want
      // an unfiltered alternative. We allow near/nearest as well
      // because they may result in zero-zero MVs but be cheaper.
      if (cpi->is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0))
        continue;
    }

#ifdef MODE_TEST_HIT_STATS
    // TEST/DEBUG CODE
    // Keep a rcord of the number of test hits at each size
    cpi->mode_test_hits[bsize]++;
#endif

    if (ref_frame == INTRA_FRAME) {
      int rate;
      mbmi->tx_size = TX_4X4;
      if (rd_pick_intra_sub_8x8_y_mode(cpi, x, &rate, &rate_y,
                                       &distortion_y, best_rd) >= best_rd)
        continue;
      rate2 += rate;
      rate2 += intra_cost_penalty;
      distortion2 += distortion_y;

      if (rate_uv_intra[TX_4X4] == INT_MAX) {
        choose_intra_uv_mode(cpi, ctx, bsize, &rate_uv_intra[TX_4X4],
                             &rate_uv_tokenonly[TX_4X4],
                             &dist_uv[TX_4X4], &skip_uv[TX_4X4],
                             &mode_uv[TX_4X4]);
      }
      rate2 += rate_uv_intra[TX_4X4];
      rate_uv = rate_uv_tokenonly[TX_4X4];
      distortion2 += dist_uv[TX_4X4];
      distortion_uv = dist_uv[TX_4X4];
      mbmi->uv_mode = mode_uv[TX_4X4];
      tx_cache[ONLY_4X4] = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
      for (i = 0; i < TX_MODES; ++i)
        tx_cache[i] = tx_cache[ONLY_4X4];
    } else {
      int rate;
      int64_t distortion;
      int64_t this_rd_thresh;
      int64_t tmp_rd, tmp_best_rd = INT64_MAX, tmp_best_rdu = INT64_MAX;
      int tmp_best_rate = INT_MAX, tmp_best_ratey = INT_MAX;
      int64_t tmp_best_distortion = INT_MAX, tmp_best_sse, uv_sse;
      int tmp_best_skippable = 0;
      int switchable_filter_index;
      int_mv *second_ref = comp_pred ?
                             &mbmi->ref_mvs[second_ref_frame][0] : NULL;
      b_mode_info tmp_best_bmodes[16];
      MB_MODE_INFO tmp_best_mbmode;
      BEST_SEG_INFO bsi[SWITCHABLE_FILTERS];
      int pred_exists = 0;
      int uv_skippable;

      this_rd_thresh = (ref_frame == LAST_FRAME) ?
          cpi->rd_thresh_sub8x8[segment_id][bsize][THR_LAST] :
          cpi->rd_thresh_sub8x8[segment_id][bsize][THR_ALTR];
      this_rd_thresh = (ref_frame == GOLDEN_FRAME) ?
          cpi->rd_thresh_sub8x8[segment_id][bsize][THR_GOLD] : this_rd_thresh;
      xd->mi_8x8[0]->mbmi.tx_size = TX_4X4;

      cpi->rd_filter_cache[SWITCHABLE_FILTERS] = INT64_MAX;
      if (cm->mcomp_filter_type != BILINEAR) {
        tmp_best_filter = EIGHTTAP;
        if (x->source_variance <
            cpi->sf.disable_filter_search_var_thresh) {
          tmp_best_filter = EIGHTTAP;
          vp9_zero(cpi->rd_filter_cache);
        } else {
          for (switchable_filter_index = 0;
               switchable_filter_index < SWITCHABLE_FILTERS;
               ++switchable_filter_index) {
            int newbest, rs;
            int64_t rs_rd;
            mbmi->interp_filter = switchable_filter_index;
            vp9_setup_interp_filters(xd, mbmi->interp_filter, &cpi->common);

            tmp_rd = rd_pick_best_mbsegmentation(cpi, x, tile,
                                                 &mbmi->ref_mvs[ref_frame][0],
                                                 second_ref,
                                                 best_yrd,
                                                 &rate, &rate_y, &distortion,
                                                 &skippable, &total_sse,
                                                 (int)this_rd_thresh, seg_mvs,
                                                 bsi, switchable_filter_index,
                                                 mi_row, mi_col);

            if (tmp_rd == INT64_MAX)
              continue;
            cpi->rd_filter_cache[switchable_filter_index] = tmp_rd;
            rs = get_switchable_rate(x);
            rs_rd = RDCOST(x->rdmult, x->rddiv, rs, 0);
            cpi->rd_filter_cache[SWITCHABLE_FILTERS] =
                MIN(cpi->rd_filter_cache[SWITCHABLE_FILTERS],
                    tmp_rd + rs_rd);
            if (cm->mcomp_filter_type == SWITCHABLE)
              tmp_rd += rs_rd;

            newbest = (tmp_rd < tmp_best_rd);
            if (newbest) {
              tmp_best_filter = mbmi->interp_filter;
              tmp_best_rd = tmp_rd;
            }
            if ((newbest && cm->mcomp_filter_type == SWITCHABLE) ||
                (mbmi->interp_filter == cm->mcomp_filter_type &&
                 cm->mcomp_filter_type != SWITCHABLE)) {
              tmp_best_rdu = tmp_rd;
              tmp_best_rate = rate;
              tmp_best_ratey = rate_y;
              tmp_best_distortion = distortion;
              tmp_best_sse = total_sse;
              tmp_best_skippable = skippable;
              tmp_best_mbmode = *mbmi;
              for (i = 0; i < 4; i++) {
                tmp_best_bmodes[i] = xd->mi_8x8[0]->bmi[i];
                x->zcoeff_blk[TX_4X4][i] = !xd->plane[0].eobs[i];
              }
              pred_exists = 1;
              if (switchable_filter_index == 0 &&
                  cpi->sf.use_rd_breakout &&
                  best_rd < INT64_MAX) {
                if (tmp_best_rdu / 2 > best_rd) {
                  // skip searching the other filters if the first is
                  // already substantially larger than the best so far
                  tmp_best_filter = mbmi->interp_filter;
                  tmp_best_rdu = INT64_MAX;
                  break;
                }
              }
            }
          }  // switchable_filter_index loop
        }
      }

      if (tmp_best_rdu == INT64_MAX)
        continue;

      mbmi->interp_filter = (cm->mcomp_filter_type == SWITCHABLE ?
                             tmp_best_filter : cm->mcomp_filter_type);
      vp9_setup_interp_filters(xd, mbmi->interp_filter, &cpi->common);
      if (!pred_exists) {
        // Handles the special case when a filter that is not in the
        // switchable list (bilinear, 6-tap) is indicated at the frame level
        tmp_rd = rd_pick_best_mbsegmentation(cpi, x, tile,
                     &mbmi->ref_mvs[ref_frame][0],
                     second_ref,
                     best_yrd,
                     &rate, &rate_y, &distortion,
                     &skippable, &total_sse,
                     (int)this_rd_thresh, seg_mvs,
                     bsi, 0,
                     mi_row, mi_col);
        if (tmp_rd == INT64_MAX)
          continue;
      } else {
        if (cpi->common.mcomp_filter_type == SWITCHABLE) {
          int rs = get_switchable_rate(x);
          tmp_best_rdu -= RDCOST(x->rdmult, x->rddiv, rs, 0);
        }
        tmp_rd = tmp_best_rdu;
        total_sse = tmp_best_sse;
        rate = tmp_best_rate;
        rate_y = tmp_best_ratey;
        distortion = tmp_best_distortion;
        skippable = tmp_best_skippable;
        *mbmi = tmp_best_mbmode;
        for (i = 0; i < 4; i++)
          xd->mi_8x8[0]->bmi[i] = tmp_best_bmodes[i];
      }

      rate2 += rate;
      distortion2 += distortion;

      if (cpi->common.mcomp_filter_type == SWITCHABLE)
        rate2 += get_switchable_rate(x);

      if (!mode_excluded) {
        if (comp_pred)
          mode_excluded = cpi->common.comp_pred_mode == SINGLE_PREDICTION_ONLY;
        else
          mode_excluded = cpi->common.comp_pred_mode == COMP_PREDICTION_ONLY;
      }
      compmode_cost = vp9_cost_bit(comp_mode_p, comp_pred);

      tmp_best_rdu = best_rd -
          MIN(RDCOST(x->rdmult, x->rddiv, rate2, distortion2),
              RDCOST(x->rdmult, x->rddiv, 0, total_sse));

      if (tmp_best_rdu > 0) {
        // If even the 'Y' rd value of split is higher than best so far
        // then dont bother looking at UV
        vp9_build_inter_predictors_sbuv(&x->e_mbd, mi_row, mi_col,
                                        BLOCK_8X8);
        super_block_uvrd(cpi, x, &rate_uv, &distortion_uv, &uv_skippable,
                         &uv_sse, BLOCK_8X8, tmp_best_rdu);
        if (rate_uv == INT_MAX)
          continue;
        rate2 += rate_uv;
        distortion2 += distortion_uv;
        skippable = skippable && uv_skippable;
        total_sse += uv_sse;

        tx_cache[ONLY_4X4] = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
        for (i = 0; i < TX_MODES; ++i)
          tx_cache[i] = tx_cache[ONLY_4X4];
      }
    }

    if (cpi->common.comp_pred_mode == HYBRID_PREDICTION) {
      rate2 += compmode_cost;
    }

    // Estimate the reference frame signaling cost and add it
    // to the rolling cost variable.
    if (second_ref_frame > INTRA_FRAME) {
      rate2 += ref_costs_comp[ref_frame];
    } else {
      rate2 += ref_costs_single[ref_frame];
    }

    if (!disable_skip) {
      // Test for the condition where skip block will be activated
      // because there are no non zero coefficients and make any
      // necessary adjustment for rate. Ignore if skip is coded at
      // segment level as the cost wont have been added in.
      // Is Mb level skip allowed (i.e. not coded at segment level).
      const int mb_skip_allowed = !vp9_segfeature_active(seg, segment_id,
                                                         SEG_LVL_SKIP);

      if (mb_skip_allowed && ref_frame != INTRA_FRAME && !xd->lossless) {
        if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv, distortion2) <
            RDCOST(x->rdmult, x->rddiv, 0, total_sse)) {
          // Add in the cost of the no skip flag.
          int prob_skip_cost = vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd),
                                            0);
          rate2 += prob_skip_cost;
        } else {
          // FIXME(rbultje) make this work for splitmv also
          int prob_skip_cost = vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd),
                                            1);
          rate2 += prob_skip_cost;
          distortion2 = total_sse;
          assert(total_sse >= 0);
          rate2 -= (rate_y + rate_uv);
          rate_y = 0;
          rate_uv = 0;
          this_skip2 = 1;
        }
      } else if (mb_skip_allowed) {
        // Add in the cost of the no skip flag.
        int prob_skip_cost = vp9_cost_bit(vp9_get_pred_prob_mbskip(cm, xd),
                                          0);
        rate2 += prob_skip_cost;
      }

      // Calculate the final RD estimate for this mode.
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
    }

    // Keep record of best inter rd with single reference
    if (xd->mi_8x8[0]->mbmi.ref_frame[0] > INTRA_FRAME &&
        xd->mi_8x8[0]->mbmi.ref_frame[1] == NONE &&
        !mode_excluded &&
        this_rd < best_inter_rd) {
      best_inter_rd = this_rd;
      best_inter_ref_frame = ref_frame;
    }

    if (!disable_skip && ref_frame == INTRA_FRAME) {
      for (i = 0; i < NB_PREDICTION_TYPES; ++i)
        best_pred_rd[i] = MIN(best_pred_rd[i], this_rd);
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
        best_filter_rd[i] = MIN(best_filter_rd[i], this_rd);
    }

    // Did this mode help.. i.e. is it the new best mode
    if (this_rd < best_rd || x->skip) {
      if (!mode_excluded) {
        int max_plane = MAX_MB_PLANE;
        // Note index of best mode so far
        best_mode_index = mode_index;

        if (ref_frame == INTRA_FRAME) {
          /* required for left and above block mv */
          mbmi->mv[0].as_int = 0;
          max_plane = 1;
        }

        *returnrate = rate2;
        *returndistortion = distortion2;
        best_rd = this_rd;
        best_yrd = best_rd -
                   RDCOST(x->rdmult, x->rddiv, rate_uv, distortion_uv);
        best_mbmode = *mbmi;
        best_skip2 = this_skip2;
        if (!x->select_txfm_size)
          swap_block_ptr(x, ctx, max_plane);
        vpx_memcpy(ctx->zcoeff_blk, x->zcoeff_blk[mbmi->tx_size],
                   sizeof(uint8_t) * ctx->num_4x4_blk);

        for (i = 0; i < 4; i++)
          best_bmodes[i] = xd->mi_8x8[0]->bmi[i];

        // TODO(debargha): enhance this test with a better distortion prediction
        // based on qp, activity mask and history
        if ((cpi->sf.mode_search_skip_flags & FLAG_EARLY_TERMINATE) &&
            (mode_index > MIN_EARLY_TERM_INDEX)) {
          const int qstep = xd->plane[0].dequant[1];
          // TODO(debargha): Enhance this by specializing for each mode_index
          int scale = 4;
          if (x->source_variance < UINT_MAX) {
            const int var_adjust = (x->source_variance < 16);
            scale -= var_adjust;
          }
          if (ref_frame > INTRA_FRAME &&
              distortion2 * scale < qstep * qstep) {
            early_term = 1;
          }
        }
      }
    }

    /* keep record of best compound/single-only prediction */
    if (!disable_skip && ref_frame != INTRA_FRAME) {
      int single_rd, hybrid_rd, single_rate, hybrid_rate;

      if (cpi->common.comp_pred_mode == HYBRID_PREDICTION) {
        single_rate = rate2 - compmode_cost;
        hybrid_rate = rate2;
      } else {
        single_rate = rate2;
        hybrid_rate = rate2 + compmode_cost;
      }

      single_rd = RDCOST(x->rdmult, x->rddiv, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, x->rddiv, hybrid_rate, distortion2);

      if (second_ref_frame <= INTRA_FRAME &&
          single_rd < best_pred_rd[SINGLE_PREDICTION_ONLY]) {
        best_pred_rd[SINGLE_PREDICTION_ONLY] = single_rd;
      } else if (second_ref_frame > INTRA_FRAME &&
                 single_rd < best_pred_rd[COMP_PREDICTION_ONLY]) {
        best_pred_rd[COMP_PREDICTION_ONLY] = single_rd;
      }
      if (hybrid_rd < best_pred_rd[HYBRID_PREDICTION])
        best_pred_rd[HYBRID_PREDICTION] = hybrid_rd;
    }

    /* keep record of best filter type */
    if (!mode_excluded && !disable_skip && ref_frame != INTRA_FRAME &&
        cm->mcomp_filter_type != BILINEAR) {
      int64_t ref = cpi->rd_filter_cache[cm->mcomp_filter_type == SWITCHABLE ?
                              SWITCHABLE_FILTERS : cm->mcomp_filter_type];
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
        int64_t adj_rd;
        // In cases of poor prediction, filter_cache[] can contain really big
        // values, which actually are bigger than this_rd itself. This can
        // cause negative best_filter_rd[] values, which is obviously silly.
        // Therefore, if filter_cache < ref, we do an adjusted calculation.
        if (cpi->rd_filter_cache[i] >= ref)
          adj_rd = this_rd + cpi->rd_filter_cache[i] - ref;
        else  // FIXME(rbultje) do this for comppred also
          adj_rd = this_rd - (ref - cpi->rd_filter_cache[i]) * this_rd / ref;
        best_filter_rd[i] = MIN(best_filter_rd[i], adj_rd);
      }
    }

    /* keep record of best txfm size */
    if (bsize < BLOCK_32X32) {
      if (bsize < BLOCK_16X16) {
        tx_cache[ALLOW_8X8] = tx_cache[ONLY_4X4];
        tx_cache[ALLOW_16X16] = tx_cache[ALLOW_8X8];
      }
      tx_cache[ALLOW_32X32] = tx_cache[ALLOW_16X16];
    }
    if (!mode_excluded && this_rd != INT64_MAX) {
      for (i = 0; i < TX_MODES && tx_cache[i] < INT64_MAX; i++) {
        int64_t adj_rd = INT64_MAX;
        if (ref_frame > INTRA_FRAME)
          adj_rd = this_rd + tx_cache[i] - tx_cache[cm->tx_mode];
        else
          adj_rd = this_rd;

        if (adj_rd < best_tx_rd[i])
          best_tx_rd[i] = adj_rd;
      }
    }

    if (early_term)
      break;

    if (x->skip && !comp_pred)
      break;
  }

  if (best_rd >= best_rd_so_far)
    return INT64_MAX;

  // If we used an estimate for the uv intra rd in the loop above...
  if (cpi->sf.use_uv_intra_rd_estimate) {
    // Do Intra UV best rd mode selection if best mode choice above was intra.
    if (vp9_ref_order[best_mode_index].ref_frame == INTRA_FRAME) {
      TX_SIZE uv_tx_size = get_uv_tx_size(mbmi);
      rd_pick_intra_sbuv_mode(cpi, x, ctx, &rate_uv_intra[uv_tx_size],
                              &rate_uv_tokenonly[uv_tx_size],
                              &dist_uv[uv_tx_size],
                              &skip_uv[uv_tx_size],
                              BLOCK_8X8);
    }
  }

  // If we are using reference masking and the set mask flag is set then
  // create the reference frame mask.
  if (cpi->sf.reference_masking && cpi->set_ref_frame_mask)
    cpi->ref_frame_mask = ~(1 << vp9_ref_order[best_mode_index].ref_frame);

  if (best_rd == INT64_MAX && bsize < BLOCK_8X8) {
    *returnrate = INT_MAX;
    *returndistortion = INT_MAX;
    return best_rd;
  }

  assert((cm->mcomp_filter_type == SWITCHABLE) ||
         (cm->mcomp_filter_type == best_mbmode.interp_filter) ||
         (best_mbmode.ref_frame[0] == INTRA_FRAME));

  // Updating rd_thresh_freq_fact[] here means that the different
  // partition/block sizes are handled independently based on the best
  // choice for the current partition. It may well be better to keep a scaled
  // best rd so far value and update rd_thresh_freq_fact based on the mode/size
  // combination that wins out.
  if (cpi->sf.adaptive_rd_thresh) {
    for (mode_index = 0; mode_index < MAX_REFS; ++mode_index) {
      if (mode_index == best_mode_index) {
        cpi->rd_thresh_freq_sub8x8[bsize][mode_index] -=
          (cpi->rd_thresh_freq_sub8x8[bsize][mode_index] >> 3);
      } else {
        cpi->rd_thresh_freq_sub8x8[bsize][mode_index] += RD_THRESH_INC;
        if (cpi->rd_thresh_freq_sub8x8[bsize][mode_index] >
            (cpi->sf.adaptive_rd_thresh * RD_THRESH_MAX_FACT)) {
          cpi->rd_thresh_freq_sub8x8[bsize][mode_index] =
            cpi->sf.adaptive_rd_thresh * RD_THRESH_MAX_FACT;
        }
      }
    }
  }

  // macroblock modes
  *mbmi = best_mbmode;
  x->skip |= best_skip2;
  if (best_mbmode.ref_frame[0] == INTRA_FRAME) {
    for (i = 0; i < 4; i++)
      xd->mi_8x8[0]->bmi[i].as_mode = best_bmodes[i].as_mode;
  } else {
    for (i = 0; i < 4; ++i)
      vpx_memcpy(&xd->mi_8x8[0]->bmi[i], &best_bmodes[i], sizeof(b_mode_info));

    mbmi->mv[0].as_int = xd->mi_8x8[0]->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = xd->mi_8x8[0]->bmi[3].as_mv[1].as_int;
  }

  for (i = 0; i < NB_PREDICTION_TYPES; ++i) {
    if (best_pred_rd[i] == INT64_MAX)
      best_pred_diff[i] = INT_MIN;
    else
      best_pred_diff[i] = best_rd - best_pred_rd[i];
  }

  if (!x->skip) {
    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
      if (best_filter_rd[i] == INT64_MAX)
        best_filter_diff[i] = 0;
      else
        best_filter_diff[i] = best_rd - best_filter_rd[i];
    }
    if (cm->mcomp_filter_type == SWITCHABLE)
      assert(best_filter_diff[SWITCHABLE_FILTERS] == 0);
  } else {
    vp9_zero(best_filter_diff);
  }

  if (!x->skip) {
    for (i = 0; i < TX_MODES; i++) {
      if (best_tx_rd[i] == INT64_MAX)
        best_tx_diff[i] = 0;
      else
        best_tx_diff[i] = best_rd - best_tx_rd[i];
    }
  } else {
    vp9_zero(best_tx_diff);
  }

  set_scale_factors(xd, mbmi->ref_frame[0], mbmi->ref_frame[1],
                    scale_factor);
  store_coding_context(x, ctx, best_mode_index,
                       &mbmi->ref_mvs[mbmi->ref_frame[0]][0],
                       &mbmi->ref_mvs[mbmi->ref_frame[1] < 0 ? 0 :
                                      mbmi->ref_frame[1]][0],
                       best_pred_diff, best_tx_diff, best_filter_diff);

  return best_rd;
}
