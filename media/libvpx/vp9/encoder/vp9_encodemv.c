/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_systemdependent.h"

#include "vp9/encoder/vp9_cost.h"
#include "vp9/encoder/vp9_encodemv.h"

static struct vp9_token mv_joint_encodings[MV_JOINTS];
static struct vp9_token mv_class_encodings[MV_CLASSES];
static struct vp9_token mv_fp_encodings[MV_FP_SIZE];
static struct vp9_token mv_class0_encodings[CLASS0_SIZE];

void vp9_entropy_mv_init() {
  vp9_tokens_from_tree(mv_joint_encodings, vp9_mv_joint_tree);
  vp9_tokens_from_tree(mv_class_encodings, vp9_mv_class_tree);
  vp9_tokens_from_tree(mv_class0_encodings, vp9_mv_class0_tree);
  vp9_tokens_from_tree(mv_fp_encodings, vp9_mv_fp_tree);
}

static void encode_mv_component(vp9_writer* w, int comp,
                                const nmv_component* mvcomp, int usehp) {
  int offset;
  const int sign = comp < 0;
  const int mag = sign ? -comp : comp;
  const int mv_class = vp9_get_mv_class(mag - 1, &offset);
  const int d = offset >> 3;                // int mv data
  const int fr = (offset >> 1) & 3;         // fractional mv data
  const int hp = offset & 1;                // high precision mv data

  assert(comp != 0);

  // Sign
  vp9_write(w, sign, mvcomp->sign);

  // Class
  vp9_write_token(w, vp9_mv_class_tree, mvcomp->classes,
                  &mv_class_encodings[mv_class]);

  // Integer bits
  if (mv_class == MV_CLASS_0) {
    vp9_write_token(w, vp9_mv_class0_tree, mvcomp->class0,
                    &mv_class0_encodings[d]);
  } else {
    int i;
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
    for (i = 0; i < n; ++i)
      vp9_write(w, (d >> i) & 1, mvcomp->bits[i]);
  }

  // Fractional bits
  vp9_write_token(w, vp9_mv_fp_tree,
                  mv_class == MV_CLASS_0 ?  mvcomp->class0_fp[d] : mvcomp->fp,
                  &mv_fp_encodings[fr]);

  // High precision bit
  if (usehp)
    vp9_write(w, hp,
              mv_class == MV_CLASS_0 ? mvcomp->class0_hp : mvcomp->hp);
}


static void build_nmv_component_cost_table(int *mvcost,
                                           const nmv_component* const mvcomp,
                                           int usehp) {
  int i, v;
  int sign_cost[2], class_cost[MV_CLASSES], class0_cost[CLASS0_SIZE];
  int bits_cost[MV_OFFSET_BITS][2];
  int class0_fp_cost[CLASS0_SIZE][MV_FP_SIZE], fp_cost[MV_FP_SIZE];
  int class0_hp_cost[2], hp_cost[2];

  sign_cost[0] = vp9_cost_zero(mvcomp->sign);
  sign_cost[1] = vp9_cost_one(mvcomp->sign);
  vp9_cost_tokens(class_cost, mvcomp->classes, vp9_mv_class_tree);
  vp9_cost_tokens(class0_cost, mvcomp->class0, vp9_mv_class0_tree);
  for (i = 0; i < MV_OFFSET_BITS; ++i) {
    bits_cost[i][0] = vp9_cost_zero(mvcomp->bits[i]);
    bits_cost[i][1] = vp9_cost_one(mvcomp->bits[i]);
  }

  for (i = 0; i < CLASS0_SIZE; ++i)
    vp9_cost_tokens(class0_fp_cost[i], mvcomp->class0_fp[i], vp9_mv_fp_tree);
  vp9_cost_tokens(fp_cost, mvcomp->fp, vp9_mv_fp_tree);

  if (usehp) {
    class0_hp_cost[0] = vp9_cost_zero(mvcomp->class0_hp);
    class0_hp_cost[1] = vp9_cost_one(mvcomp->class0_hp);
    hp_cost[0] = vp9_cost_zero(mvcomp->hp);
    hp_cost[1] = vp9_cost_one(mvcomp->hp);
  }
  mvcost[0] = 0;
  for (v = 1; v <= MV_MAX; ++v) {
    int z, c, o, d, e, f, cost = 0;
    z = v - 1;
    c = vp9_get_mv_class(z, &o);
    cost += class_cost[c];
    d = (o >> 3);               /* int mv data */
    f = (o >> 1) & 3;           /* fractional pel mv data */
    e = (o & 1);                /* high precision mv data */
    if (c == MV_CLASS_0) {
      cost += class0_cost[d];
    } else {
      int i, b;
      b = c + CLASS0_BITS - 1;  /* number of bits */
      for (i = 0; i < b; ++i)
        cost += bits_cost[i][((d >> i) & 1)];
    }
    if (c == MV_CLASS_0) {
      cost += class0_fp_cost[d][f];
    } else {
      cost += fp_cost[f];
    }
    if (usehp) {
      if (c == MV_CLASS_0) {
        cost += class0_hp_cost[e];
      } else {
        cost += hp_cost[e];
      }
    }
    mvcost[v] = cost + sign_cost[0];
    mvcost[-v] = cost + sign_cost[1];
  }
}

static int update_mv(vp9_writer *w, const unsigned int ct[2], vp9_prob *cur_p,
                     vp9_prob upd_p) {
  const vp9_prob new_p = get_binary_prob(ct[0], ct[1]) | 1;
  const int update = cost_branch256(ct, *cur_p) + vp9_cost_zero(upd_p) >
                     cost_branch256(ct, new_p) + vp9_cost_one(upd_p) + 7 * 256;
  vp9_write(w, update, upd_p);
  if (update) {
    *cur_p = new_p;
    vp9_write_literal(w, new_p >> 1, 7);
  }
  return update;
}

static void write_mv_update(const vp9_tree_index *tree,
                            vp9_prob probs[/*n - 1*/],
                            const unsigned int counts[/*n - 1*/],
                            int n, vp9_writer *w) {
  int i;
  unsigned int branch_ct[32][2];

  // Assuming max number of probabilities <= 32
  assert(n <= 32);

  vp9_tree_probs_from_distribution(tree, branch_ct, counts);
  for (i = 0; i < n - 1; ++i)
    update_mv(w, branch_ct[i], &probs[i], MV_UPDATE_PROB);
}

void vp9_write_nmv_probs(VP9_COMMON *cm, int usehp, vp9_writer *w) {
  int i, j;
  nmv_context *const mvc = &cm->fc.nmvc;
  nmv_context_counts *const counts = &cm->counts.mv;

  write_mv_update(vp9_mv_joint_tree, mvc->joints, counts->joints, MV_JOINTS, w);

  for (i = 0; i < 2; ++i) {
    nmv_component *comp = &mvc->comps[i];
    nmv_component_counts *comp_counts = &counts->comps[i];

    update_mv(w, comp_counts->sign, &comp->sign, MV_UPDATE_PROB);
    write_mv_update(vp9_mv_class_tree, comp->classes, comp_counts->classes,
                    MV_CLASSES, w);
    write_mv_update(vp9_mv_class0_tree, comp->class0, comp_counts->class0,
                    CLASS0_SIZE, w);
    for (j = 0; j < MV_OFFSET_BITS; ++j)
      update_mv(w, comp_counts->bits[j], &comp->bits[j], MV_UPDATE_PROB);
  }

  for (i = 0; i < 2; ++i) {
    for (j = 0; j < CLASS0_SIZE; ++j)
      write_mv_update(vp9_mv_fp_tree, mvc->comps[i].class0_fp[j],
                      counts->comps[i].class0_fp[j], MV_FP_SIZE, w);

    write_mv_update(vp9_mv_fp_tree, mvc->comps[i].fp, counts->comps[i].fp,
                    MV_FP_SIZE, w);
  }

  if (usehp) {
    for (i = 0; i < 2; ++i) {
      update_mv(w, counts->comps[i].class0_hp, &mvc->comps[i].class0_hp,
                MV_UPDATE_PROB);
      update_mv(w, counts->comps[i].hp, &mvc->comps[i].hp, MV_UPDATE_PROB);
    }
  }
}

void vp9_encode_mv(VP9_COMP* cpi, vp9_writer* w,
                   const MV* mv, const MV* ref,
                   const nmv_context* mvctx, int usehp) {
  const MV diff = {mv->row - ref->row,
                   mv->col - ref->col};
  const MV_JOINT_TYPE j = vp9_get_mv_joint(&diff);
  usehp = usehp && vp9_use_mv_hp(ref);

  vp9_write_token(w, vp9_mv_joint_tree, mvctx->joints, &mv_joint_encodings[j]);
  if (mv_joint_vertical(j))
    encode_mv_component(w, diff.row, &mvctx->comps[0], usehp);

  if (mv_joint_horizontal(j))
    encode_mv_component(w, diff.col, &mvctx->comps[1], usehp);

  // If auto_mv_step_size is enabled then keep track of the largest
  // motion vector component used.
  if (cpi->sf.mv.auto_mv_step_size) {
    unsigned int maxv = MAX(abs(mv->row), abs(mv->col)) >> 3;
    cpi->max_mv_magnitude = MAX(maxv, cpi->max_mv_magnitude);
  }
}

void vp9_build_nmv_cost_table(int *mvjoint, int *mvcost[2],
                              const nmv_context* ctx, int usehp) {
  vp9_cost_tokens(mvjoint, ctx->joints, vp9_mv_joint_tree);
  build_nmv_component_cost_table(mvcost[0], &ctx->comps[0], usehp);
  build_nmv_component_cost_table(mvcost[1], &ctx->comps[1], usehp);
}

static void inc_mvs(const MB_MODE_INFO *mbmi, const int_mv mvs[2],
                    nmv_context_counts *counts) {
  int i;

  for (i = 0; i < 1 + has_second_ref(mbmi); ++i) {
    const MV *ref = &mbmi->ref_mvs[mbmi->ref_frame[i]][0].as_mv;
    const MV diff = {mvs[i].as_mv.row - ref->row,
                     mvs[i].as_mv.col - ref->col};
    vp9_inc_mv(&diff, counts);
  }
}

void vp9_update_mv_count(VP9_COMMON *cm, const MACROBLOCKD *xd) {
  const MODE_INFO *mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;

  if (mbmi->sb_type < BLOCK_8X8) {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[mbmi->sb_type];
    const int num_4x4_h = num_4x4_blocks_high_lookup[mbmi->sb_type];
    int idx, idy;

    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        const int i = idy * 2 + idx;
        if (mi->bmi[i].as_mode == NEWMV)
          inc_mvs(mbmi, mi->bmi[i].as_mv, &cm->counts.mv);
      }
    }
  } else {
    if (mbmi->mode == NEWMV)
      inc_mvs(mbmi, mbmi->mv, &cm->counts.mv);
  }
}

