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

#include <math.h>

#include "av1/common/common.h"
#include "av1/common/entropymode.h"

#include "av1/encoder/cost.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/subexp.h"

#include "aom_dsp/aom_dsp_common.h"

static struct av1_token mv_joint_encodings[MV_JOINTS];
static struct av1_token mv_class_encodings[MV_CLASSES];
static struct av1_token mv_fp_encodings[MV_FP_SIZE];

void av1_entropy_mv_init(void) {
  av1_tokens_from_tree(mv_joint_encodings, av1_mv_joint_tree);
  av1_tokens_from_tree(mv_class_encodings, av1_mv_class_tree);
  av1_tokens_from_tree(mv_fp_encodings, av1_mv_fp_tree);
}

static void encode_mv_component(aom_writer *w, int comp, nmv_component *mvcomp,
                                MvSubpelPrecision precision) {
  int offset;
  const int sign = comp < 0;
  const int mag = sign ? -comp : comp;
  const int mv_class = av1_get_mv_class(mag - 1, &offset);
  const int d = offset >> 3;         // int mv data
  const int fr = (offset >> 1) & 3;  // fractional mv data
  const int hp = offset & 1;         // high precision mv data

  assert(comp != 0);

// Sign
#if CONFIG_NEW_MULTISYMBOL
  aom_write_bit(w, sign);
#else
  aom_write(w, sign, mvcomp->sign);
#endif

  // Class
  aom_write_symbol(w, mv_class, mvcomp->class_cdf, MV_CLASSES);

  // Integer bits
  if (mv_class == MV_CLASS_0) {
#if CONFIG_NEW_MULTISYMBOL
    aom_write_symbol(w, d, mvcomp->class0_cdf, CLASS0_SIZE);
#else
    aom_write(w, d, mvcomp->class0[0]);
#endif
  } else {
    int i;
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
#if CONFIG_NEW_MULTISYMBOL
    for (i = 0; i < n; ++i)
      aom_write_symbol(w, (d >> i) & 1, mvcomp->bits_cdf[(i + 1) / 2], 2);
#else
    for (i = 0; i < n; ++i) aom_write(w, (d >> i) & 1, mvcomp->bits[i]);
#endif
  }
// Fractional bits
#if CONFIG_INTRABC || CONFIG_AMVR
  if (precision > MV_SUBPEL_NONE)
#endif  // CONFIG_INTRABC || CONFIG_AMVR
  {
    aom_write_symbol(
        w, fr,
        mv_class == MV_CLASS_0 ? mvcomp->class0_fp_cdf[d] : mvcomp->fp_cdf,
        MV_FP_SIZE);
  }

  // High precision bit
  if (precision > MV_SUBPEL_LOW_PRECISION)
#if CONFIG_NEW_MULTISYMBOL
    aom_write_symbol(
        w, hp, mv_class == MV_CLASS_0 ? mvcomp->class0_hp_cdf : mvcomp->hp_cdf,
        2);
#else
    aom_write(w, hp, mv_class == MV_CLASS_0 ? mvcomp->class0_hp : mvcomp->hp);
#endif
}

static void build_nmv_component_cost_table(int *mvcost,
                                           const nmv_component *const mvcomp,
                                           MvSubpelPrecision precision) {
  int i, v;
  int sign_cost[2], class_cost[MV_CLASSES], class0_cost[CLASS0_SIZE];
  int bits_cost[MV_OFFSET_BITS][2];
  int class0_fp_cost[CLASS0_SIZE][MV_FP_SIZE], fp_cost[MV_FP_SIZE];
  int class0_hp_cost[2], hp_cost[2];

  sign_cost[0] = av1_cost_zero(mvcomp->sign);
  sign_cost[1] = av1_cost_one(mvcomp->sign);
  av1_cost_tokens(class_cost, mvcomp->classes, av1_mv_class_tree);
  av1_cost_tokens(class0_cost, mvcomp->class0, av1_mv_class0_tree);
  for (i = 0; i < MV_OFFSET_BITS; ++i) {
    bits_cost[i][0] = av1_cost_zero(mvcomp->bits[i]);
    bits_cost[i][1] = av1_cost_one(mvcomp->bits[i]);
  }

  for (i = 0; i < CLASS0_SIZE; ++i)
    av1_cost_tokens(class0_fp_cost[i], mvcomp->class0_fp[i], av1_mv_fp_tree);
  av1_cost_tokens(fp_cost, mvcomp->fp, av1_mv_fp_tree);

  if (precision > MV_SUBPEL_LOW_PRECISION) {
    class0_hp_cost[0] = av1_cost_zero(mvcomp->class0_hp);
    class0_hp_cost[1] = av1_cost_one(mvcomp->class0_hp);
    hp_cost[0] = av1_cost_zero(mvcomp->hp);
    hp_cost[1] = av1_cost_one(mvcomp->hp);
  }
  mvcost[0] = 0;
  for (v = 1; v <= MV_MAX; ++v) {
    int z, c, o, d, e, f, cost = 0;
    z = v - 1;
    c = av1_get_mv_class(z, &o);
    cost += class_cost[c];
    d = (o >> 3);     /* int mv data */
    f = (o >> 1) & 3; /* fractional pel mv data */
    e = (o & 1);      /* high precision mv data */
    if (c == MV_CLASS_0) {
      cost += class0_cost[d];
    } else {
      const int b = c + CLASS0_BITS - 1; /* number of bits */
      for (i = 0; i < b; ++i) cost += bits_cost[i][((d >> i) & 1)];
    }
#if CONFIG_INTRABC || CONFIG_AMVR
    if (precision > MV_SUBPEL_NONE)
#endif  // CONFIG_INTRABC || CONFIG_AMVR
    {
      if (c == MV_CLASS_0) {
        cost += class0_fp_cost[d][f];
      } else {
        cost += fp_cost[f];
      }
      if (precision > MV_SUBPEL_LOW_PRECISION) {
        if (c == MV_CLASS_0) {
          cost += class0_hp_cost[e];
        } else {
          cost += hp_cost[e];
        }
      }
    }
    mvcost[v] = cost + sign_cost[0];
    mvcost[-v] = cost + sign_cost[1];
  }
}

#if !CONFIG_NEW_MULTISYMBOL
static void update_mv(aom_writer *w, const unsigned int ct[2], aom_prob *cur_p,
                      aom_prob upd_p) {
  (void)upd_p;
  // Just use the default maximum number of tile groups to avoid passing in the
  // actual
  // number
  av1_cond_prob_diff_update(w, cur_p, ct, DEFAULT_MAX_NUM_TG);
}

void av1_write_nmv_probs(AV1_COMMON *cm, int usehp, aom_writer *w,
                         nmv_context_counts *const nmv_counts) {
  int i;
  int nmv_ctx = 0;
#if CONFIG_AMVR
  if (cm->cur_frame_mv_precision_level) {
    return;
  }
#endif
  for (nmv_ctx = 0; nmv_ctx < NMV_CONTEXTS; ++nmv_ctx) {
    nmv_context *const mvc = &cm->fc->nmvc[nmv_ctx];
    nmv_context_counts *const counts = &nmv_counts[nmv_ctx];

    if (usehp) {
      for (i = 0; i < 2; ++i) {
        update_mv(w, counts->comps[i].class0_hp, &mvc->comps[i].class0_hp,
                  MV_UPDATE_PROB);
        update_mv(w, counts->comps[i].hp, &mvc->comps[i].hp, MV_UPDATE_PROB);
      }
    }
  }
}
#endif

void av1_encode_mv(AV1_COMP *cpi, aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx, int usehp) {
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  const MV_JOINT_TYPE j = av1_get_mv_joint(&diff);
#if CONFIG_AMVR
  if (cpi->common.cur_frame_mv_precision_level) {
    usehp = MV_SUBPEL_NONE;
  }
#endif
  aom_write_symbol(w, j, mvctx->joint_cdf, MV_JOINTS);
  if (mv_joint_vertical(j))
    encode_mv_component(w, diff.row, &mvctx->comps[0], usehp);

  if (mv_joint_horizontal(j))
    encode_mv_component(w, diff.col, &mvctx->comps[1], usehp);

  // If auto_mv_step_size is enabled then keep track of the largest
  // motion vector component used.
  if (cpi->sf.mv.auto_mv_step_size) {
    unsigned int maxv = AOMMAX(abs(mv->row), abs(mv->col)) >> 3;
    cpi->max_mv_magnitude = AOMMAX(maxv, cpi->max_mv_magnitude);
  }
}

#if CONFIG_INTRABC
void av1_encode_dv(aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx) {
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  const MV_JOINT_TYPE j = av1_get_mv_joint(&diff);

  aom_write_symbol(w, j, mvctx->joint_cdf, MV_JOINTS);
  if (mv_joint_vertical(j))
    encode_mv_component(w, diff.row, &mvctx->comps[0], MV_SUBPEL_NONE);

  if (mv_joint_horizontal(j))
    encode_mv_component(w, diff.col, &mvctx->comps[1], MV_SUBPEL_NONE);
}
#endif  // CONFIG_INTRABC

void av1_build_nmv_cost_table(int *mvjoint, int *mvcost[2],
                              const nmv_context *ctx,
                              MvSubpelPrecision precision) {
  av1_cost_tokens(mvjoint, ctx->joints, av1_mv_joint_tree);
  build_nmv_component_cost_table(mvcost[0], &ctx->comps[0], precision);
  build_nmv_component_cost_table(mvcost[1], &ctx->comps[1], precision);
}

static void inc_mvs(const MB_MODE_INFO *mbmi, const MB_MODE_INFO_EXT *mbmi_ext,
                    const int_mv mvs[2], const int_mv pred_mvs[2],
                    nmv_context_counts *nmv_counts
#if CONFIG_AMVR
                    ,
                    MvSubpelPrecision precision
#endif
                    ) {
  int i;
  PREDICTION_MODE mode = mbmi->mode;

  if (mode == NEWMV || mode == NEW_NEWMV) {
    for (i = 0; i < 1 + has_second_ref(mbmi); ++i) {
      const MV *ref = &mbmi_ext->ref_mvs[mbmi->ref_frame[i]][0].as_mv;
      const MV diff = { mvs[i].as_mv.row - ref->row,
                        mvs[i].as_mv.col - ref->col };
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx =
          av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                      mbmi_ext->ref_mv_stack[rf_type], i, mbmi->ref_mv_idx);
      nmv_context_counts *counts = &nmv_counts[nmv_ctx];
      (void)pred_mvs;
#if CONFIG_AMVR
      av1_inc_mv(&diff, counts, precision);
#else
      av1_inc_mv(&diff, counts, 1);
#endif
    }
  } else if (mode == NEAREST_NEWMV || mode == NEAR_NEWMV) {
    const MV *ref = &mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0].as_mv;
    const MV diff = { mvs[1].as_mv.row - ref->row,
                      mvs[1].as_mv.col - ref->col };
    int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
    int nmv_ctx =
        av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                    mbmi_ext->ref_mv_stack[rf_type], 1, mbmi->ref_mv_idx);
    nmv_context_counts *counts = &nmv_counts[nmv_ctx];
#if CONFIG_AMVR
    av1_inc_mv(&diff, counts, precision);
#else
    av1_inc_mv(&diff, counts, 1);
#endif
  } else if (mode == NEW_NEARESTMV || mode == NEW_NEARMV) {
    const MV *ref = &mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0].as_mv;
    const MV diff = { mvs[0].as_mv.row - ref->row,
                      mvs[0].as_mv.col - ref->col };
    int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
    int nmv_ctx =
        av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                    mbmi_ext->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
    nmv_context_counts *counts = &nmv_counts[nmv_ctx];
#if CONFIG_AMVR
    av1_inc_mv(&diff, counts, precision);
#else
    av1_inc_mv(&diff, counts, 1);
#endif
#if CONFIG_COMPOUND_SINGLEREF
  } else {
    assert(  // mode == SR_NEAREST_NEWMV ||
        mode == SR_NEAR_NEWMV || mode == SR_ZERO_NEWMV || mode == SR_NEW_NEWMV);
    const MV *ref = &mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0].as_mv;
    int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
    int nmv_ctx =
        av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                    mbmi_ext->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
    nmv_context_counts *counts = &nmv_counts[nmv_ctx];
    (void)pred_mvs;
    MV diff;
    if (mode == SR_NEW_NEWMV) {
      diff.row = mvs[0].as_mv.row - ref->row;
      diff.col = mvs[0].as_mv.col - ref->col;
      av1_inc_mv(&diff, counts, 1);
    }
    diff.row = mvs[1].as_mv.row - ref->row;
    diff.col = mvs[1].as_mv.col - ref->col;
    av1_inc_mv(&diff, counts, 1);
#endif  // CONFIG_COMPOUND_SINGLEREF
  }
}

static void inc_mvs_sub8x8(const MODE_INFO *mi, int block, const int_mv mvs[2],
                           const MB_MODE_INFO_EXT *mbmi_ext,
                           nmv_context_counts *nmv_counts
#if CONFIG_AMVR
                           ,
                           MvSubpelPrecision precision
#endif
                           ) {
  int i;
  PREDICTION_MODE mode = mi->bmi[block].as_mode;
  const MB_MODE_INFO *mbmi = &mi->mbmi;

  if (mode == NEWMV || mode == NEW_NEWMV) {
    for (i = 0; i < 1 + has_second_ref(&mi->mbmi); ++i) {
      const MV *ref = &mi->bmi[block].ref_mv[i].as_mv;
      const MV diff = { mvs[i].as_mv.row - ref->row,
                        mvs[i].as_mv.col - ref->col };
      int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
      int nmv_ctx =
          av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                      mbmi_ext->ref_mv_stack[rf_type], i, mbmi->ref_mv_idx);
      nmv_context_counts *counts = &nmv_counts[nmv_ctx];
#if CONFIG_AMVR
      av1_inc_mv(&diff, counts, precision);
#else
      av1_inc_mv(&diff, counts, 1);
#endif
    }
  } else if (mode == NEAREST_NEWMV || mode == NEAR_NEWMV) {
    const MV *ref = &mi->bmi[block].ref_mv[1].as_mv;
    const MV diff = { mvs[1].as_mv.row - ref->row,
                      mvs[1].as_mv.col - ref->col };
    int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
    int nmv_ctx =
        av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                    mbmi_ext->ref_mv_stack[rf_type], 1, mbmi->ref_mv_idx);
    nmv_context_counts *counts = &nmv_counts[nmv_ctx];
#if CONFIG_AMVR
    av1_inc_mv(&diff, counts, precision);
#else
    av1_inc_mv(&diff, counts, 1);
#endif
  } else if (mode == NEW_NEARESTMV || mode == NEW_NEARMV) {
    const MV *ref = &mi->bmi[block].ref_mv[0].as_mv;
    const MV diff = { mvs[0].as_mv.row - ref->row,
                      mvs[0].as_mv.col - ref->col };
    int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
    int nmv_ctx =
        av1_nmv_ctx(mbmi_ext->ref_mv_count[rf_type],
                    mbmi_ext->ref_mv_stack[rf_type], 0, mbmi->ref_mv_idx);
    nmv_context_counts *counts = &nmv_counts[nmv_ctx];
#if CONFIG_AMVR
    av1_inc_mv(&diff, counts, precision);
#else
    av1_inc_mv(&diff, counts, 1);
#endif
  }
}

void av1_update_mv_count(ThreadData *td) {
  const MACROBLOCKD *xd = &td->mb.e_mbd;
  const MODE_INFO *mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MB_MODE_INFO_EXT *mbmi_ext = td->mb.mbmi_ext;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
#if CONFIG_AMVR
  MvSubpelPrecision precision = 1;
  if (xd->cur_frame_mv_precision_level) {
    precision = MV_SUBPEL_NONE;
  }
#endif

  if (mbmi->sb_type < BLOCK_8X8 && !unify_bsize) {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[mbmi->sb_type];
    const int num_4x4_h = num_4x4_blocks_high_lookup[mbmi->sb_type];
    int idx, idy;

    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        const int i = idy * 2 + idx;

        if (have_newmv_in_inter_mode(mi->bmi[i].as_mode))

#if CONFIG_AMVR
          inc_mvs_sub8x8(mi, i, mi->bmi[i].as_mv, mbmi_ext, td->counts->mv,
                         precision);
#else
          inc_mvs_sub8x8(mi, i, mi->bmi[i].as_mv, mbmi_ext, td->counts->mv);
#endif
      }
    }
  } else {
    if (have_newmv_in_inter_mode(mbmi->mode))

#if CONFIG_AMVR
      inc_mvs(mbmi, mbmi_ext, mbmi->mv, mbmi->pred_mv, td->counts->mv,
              precision);
#else
      inc_mvs(mbmi, mbmi_ext, mbmi->mv, mbmi->pred_mv, td->counts->mv);
#endif
  }
}
