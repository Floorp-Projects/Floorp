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
#include "vp9/encoder/vp9_encodemv.h"


#ifdef ENTROPY_STATS
extern unsigned int active_section;
#endif

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
  write_token(w, vp9_mv_class_tree, mvcomp->classes,
              &vp9_mv_class_encodings[mv_class]);

  // Integer bits
  if (mv_class == MV_CLASS_0) {
    write_token(w, vp9_mv_class0_tree, mvcomp->class0,
                &vp9_mv_class0_encodings[d]);
  } else {
    int i;
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
    for (i = 0; i < n; ++i)
      vp9_write(w, (d >> i) & 1, mvcomp->bits[i]);
  }

  // Fractional bits
  write_token(w, vp9_mv_fp_tree,
              mv_class == MV_CLASS_0 ?  mvcomp->class0_fp[d] : mvcomp->fp,
              &vp9_mv_fp_encodings[fr]);

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
  int class0_fp_cost[CLASS0_SIZE][4], fp_cost[4];
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
  const vp9_prob new_p = get_binary_prob(ct[0], ct[1]);
  vp9_prob mod_p = new_p | 1;
  const int cur_b = cost_branch256(ct, *cur_p);
  const int mod_b = cost_branch256(ct, mod_p);
  const int cost = 7 * 256 + (vp9_cost_one(upd_p) - vp9_cost_zero(upd_p));
  if (cur_b - mod_b > cost) {
    *cur_p = mod_p;
    vp9_write(w, 1, upd_p);
    vp9_write_literal(w, mod_p >> 1, 7);
    return 1;
  } else {
    vp9_write(w, 0, upd_p);
    return 0;
  }
}

static void counts_to_nmv_context(
    nmv_context_counts *nmv_count,
    int usehp,
    unsigned int (*branch_ct_joint)[2],
    unsigned int (*branch_ct_sign)[2],
    unsigned int (*branch_ct_classes)[MV_CLASSES - 1][2],
    unsigned int (*branch_ct_class0)[CLASS0_SIZE - 1][2],
    unsigned int (*branch_ct_bits)[MV_OFFSET_BITS][2],
    unsigned int (*branch_ct_class0_fp)[CLASS0_SIZE][4 - 1][2],
    unsigned int (*branch_ct_fp)[4 - 1][2],
    unsigned int (*branch_ct_class0_hp)[2],
    unsigned int (*branch_ct_hp)[2]) {
  int i, j, k;
  vp9_tree_probs_from_distribution(vp9_mv_joint_tree, branch_ct_joint,
                                   nmv_count->joints);
  for (i = 0; i < 2; ++i) {
    const uint32_t s0 = nmv_count->comps[i].sign[0];
    const uint32_t s1 = nmv_count->comps[i].sign[1];

    branch_ct_sign[i][0] = s0;
    branch_ct_sign[i][1] = s1;
    vp9_tree_probs_from_distribution(vp9_mv_class_tree,
                                    branch_ct_classes[i],
                                    nmv_count->comps[i].classes);
    vp9_tree_probs_from_distribution(vp9_mv_class0_tree,
                                     branch_ct_class0[i],
                                     nmv_count->comps[i].class0);
    for (j = 0; j < MV_OFFSET_BITS; ++j) {
      const uint32_t b0 = nmv_count->comps[i].bits[j][0];
      const uint32_t b1 = nmv_count->comps[i].bits[j][1];

      branch_ct_bits[i][j][0] = b0;
      branch_ct_bits[i][j][1] = b1;
    }
  }
  for (i = 0; i < 2; ++i) {
    for (k = 0; k < CLASS0_SIZE; ++k) {
      vp9_tree_probs_from_distribution(vp9_mv_fp_tree,
                                       branch_ct_class0_fp[i][k],
                                       nmv_count->comps[i].class0_fp[k]);
    }
    vp9_tree_probs_from_distribution(vp9_mv_fp_tree,
                                     branch_ct_fp[i],
                                     nmv_count->comps[i].fp);
  }
  if (usehp) {
    for (i = 0; i < 2; ++i) {
      const uint32_t c0_hp0 = nmv_count->comps[i].class0_hp[0];
      const uint32_t c0_hp1 = nmv_count->comps[i].class0_hp[1];
      const uint32_t hp0 = nmv_count->comps[i].hp[0];
      const uint32_t hp1 = nmv_count->comps[i].hp[1];

      branch_ct_class0_hp[i][0] = c0_hp0;
      branch_ct_class0_hp[i][1] = c0_hp1;

      branch_ct_hp[i][0] = hp0;
      branch_ct_hp[i][1] = hp1;
    }
  }
}

void vp9_write_nmv_probs(VP9_COMP* const cpi, int usehp, vp9_writer* const bc) {
  int i, j;
  unsigned int branch_ct_joint[MV_JOINTS - 1][2];
  unsigned int branch_ct_sign[2][2];
  unsigned int branch_ct_classes[2][MV_CLASSES - 1][2];
  unsigned int branch_ct_class0[2][CLASS0_SIZE - 1][2];
  unsigned int branch_ct_bits[2][MV_OFFSET_BITS][2];
  unsigned int branch_ct_class0_fp[2][CLASS0_SIZE][4 - 1][2];
  unsigned int branch_ct_fp[2][4 - 1][2];
  unsigned int branch_ct_class0_hp[2][2];
  unsigned int branch_ct_hp[2][2];
  nmv_context *mvc = &cpi->common.fc.nmvc;

  counts_to_nmv_context(&cpi->NMVcount, usehp,
                        branch_ct_joint, branch_ct_sign, branch_ct_classes,
                        branch_ct_class0, branch_ct_bits,
                        branch_ct_class0_fp, branch_ct_fp,
                        branch_ct_class0_hp, branch_ct_hp);

  for (j = 0; j < MV_JOINTS - 1; ++j)
    update_mv(bc, branch_ct_joint[j], &mvc->joints[j], NMV_UPDATE_PROB);

  for (i = 0; i < 2; ++i) {
    update_mv(bc, branch_ct_sign[i], &mvc->comps[i].sign, NMV_UPDATE_PROB);
    for (j = 0; j < MV_CLASSES - 1; ++j)
      update_mv(bc, branch_ct_classes[i][j], &mvc->comps[i].classes[j],
                NMV_UPDATE_PROB);

    for (j = 0; j < CLASS0_SIZE - 1; ++j)
      update_mv(bc, branch_ct_class0[i][j], &mvc->comps[i].class0[j],
                NMV_UPDATE_PROB);

    for (j = 0; j < MV_OFFSET_BITS; ++j)
      update_mv(bc, branch_ct_bits[i][j], &mvc->comps[i].bits[j],
                NMV_UPDATE_PROB);
  }

  for (i = 0; i < 2; ++i) {
    for (j = 0; j < CLASS0_SIZE; ++j) {
      int k;
      for (k = 0; k < 3; ++k)
        update_mv(bc, branch_ct_class0_fp[i][j][k],
                  &mvc->comps[i].class0_fp[j][k], NMV_UPDATE_PROB);
    }

    for (j = 0; j < 3; ++j)
      update_mv(bc, branch_ct_fp[i][j], &mvc->comps[i].fp[j], NMV_UPDATE_PROB);
  }

  if (usehp) {
    for (i = 0; i < 2; ++i) {
      update_mv(bc, branch_ct_class0_hp[i], &mvc->comps[i].class0_hp,
                NMV_UPDATE_PROB);
      update_mv(bc, branch_ct_hp[i], &mvc->comps[i].hp,
                NMV_UPDATE_PROB);
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

  write_token(w, vp9_mv_joint_tree, mvctx->joints, &vp9_mv_joint_encodings[j]);
  if (mv_joint_vertical(j))
    encode_mv_component(w, diff.row, &mvctx->comps[0], usehp);

  if (mv_joint_horizontal(j))
    encode_mv_component(w, diff.col, &mvctx->comps[1], usehp);

  // If auto_mv_step_size is enabled then keep track of the largest
  // motion vector component used.
  if (!cpi->dummy_packing && cpi->sf.auto_mv_step_size) {
    unsigned int maxv = MAX(abs(mv->row), abs(mv->col)) >> 3;
    cpi->max_mv_magnitude = MAX(maxv, cpi->max_mv_magnitude);
  }
}

void vp9_build_nmv_cost_table(int *mvjoint,
                              int *mvcost[2],
                              const nmv_context* const mvctx,
                              int usehp,
                              int mvc_flag_v,
                              int mvc_flag_h) {
  vp9_clear_system_state();
  vp9_cost_tokens(mvjoint, mvctx->joints, vp9_mv_joint_tree);
  if (mvc_flag_v)
    build_nmv_component_cost_table(mvcost[0], &mvctx->comps[0], usehp);
  if (mvc_flag_h)
    build_nmv_component_cost_table(mvcost[1], &mvctx->comps[1], usehp);
}

static void inc_mvs(int_mv mv[2], int_mv ref[2], int is_compound,
                    nmv_context_counts *counts) {
  int i;
  for (i = 0; i < 1 + is_compound; ++i) {
    const MV diff = { mv[i].as_mv.row - ref[i].as_mv.row,
                      mv[i].as_mv.col - ref[i].as_mv.col };
    vp9_inc_mv(&diff, counts);
  }
}

void vp9_update_mv_count(VP9_COMP *cpi, MACROBLOCK *x, int_mv best_ref_mv[2]) {
  MODE_INFO *mi = x->e_mbd.mi_8x8[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const int is_compound = has_second_ref(mbmi);

  if (mbmi->sb_type < BLOCK_8X8) {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[mbmi->sb_type];
    const int num_4x4_h = num_4x4_blocks_high_lookup[mbmi->sb_type];
    int idx, idy;

    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        const int i = idy * 2 + idx;
        if (mi->bmi[i].as_mode == NEWMV)
          inc_mvs(mi->bmi[i].as_mv, best_ref_mv, is_compound, &cpi->NMVcount);
      }
    }
  } else if (mbmi->mode == NEWMV) {
    inc_mvs(mbmi->mv, best_ref_mv, is_compound, &cpi->NMVcount);
  }
}
