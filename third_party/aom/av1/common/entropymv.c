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

#include "av1/common/onyxc_int.h"
#include "av1/common/entropymv.h"

// Integer pel reference mv threshold for use of high-precision 1/8 mv
#define COMPANDED_MVREF_THRESH 8

const aom_tree_index av1_mv_joint_tree[TREE_SIZE(MV_JOINTS)] = {
  -MV_JOINT_ZERO, 2, -MV_JOINT_HNZVZ, 4, -MV_JOINT_HZVNZ, -MV_JOINT_HNZVNZ
};

/* clang-format off */
const aom_tree_index av1_mv_class_tree[TREE_SIZE(MV_CLASSES)] = {
  -MV_CLASS_0, 2,
  -MV_CLASS_1, 4,
  6, 8,
  -MV_CLASS_2, -MV_CLASS_3,
  10, 12,
  -MV_CLASS_4, -MV_CLASS_5,
  -MV_CLASS_6, 14,
  16, 18,
  -MV_CLASS_7, -MV_CLASS_8,
  -MV_CLASS_9, -MV_CLASS_10,
};
/* clang-format on */

const aom_tree_index av1_mv_class0_tree[TREE_SIZE(CLASS0_SIZE)] = {
  -0, -1,
};

const aom_tree_index av1_mv_fp_tree[TREE_SIZE(MV_FP_SIZE)] = { -0, 2,  -1,
                                                               4,  -2, -3 };

static const nmv_context default_nmv_context = {
  { 32, 64, 96 },  // joints
  { AOM_ICDF(4096), AOM_ICDF(11264), AOM_ICDF(19328), AOM_ICDF(32768),
    0 },  // joint_cdf
  { {
        // Vertical component
        128,                                                   // sign
        { 224, 144, 192, 168, 192, 176, 192, 198, 198, 245 },  // class
        { AOM_ICDF(28672), AOM_ICDF(30976), AOM_ICDF(31858), AOM_ICDF(32320),
          AOM_ICDF(32551), AOM_ICDF(32656), AOM_ICDF(32740), AOM_ICDF(32757),
          AOM_ICDF(32762), AOM_ICDF(32767), AOM_ICDF(32768), 0 },  // class_cdf
        { 216 },                                                   // class0
        { 136, 140, 148, 160, 176, 192, 224, 234, 234, 240 },      // bits
        { { 128, 128, 64 }, { 96, 112, 64 } },                     // class0_fp
        { 64, 96, 64 },                                            // fp
        { { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(26624), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(12288), AOM_ICDF(21248), AOM_ICDF(24128), AOM_ICDF(32768),
            0 } },  // class0_fp_cdf
        { AOM_ICDF(8192), AOM_ICDF(17408), AOM_ICDF(21248), AOM_ICDF(32768),
          0 },  // fp_cdf
        160,    // class0_hp bit
        128,    // hp
#if CONFIG_NEW_MULTISYMBOL
        { AOM_ICDF(160 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(216 * 128), AOM_ICDF(32768), 0 },
        { { AOM_ICDF(128 * 196), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 198), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 208), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 224), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 245), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 240), AOM_ICDF(32768), 0 } },  // bits_cdf
#endif
    },
    {
        // Horizontal component
        128,                                                   // sign
        { 216, 128, 176, 160, 176, 176, 192, 198, 198, 208 },  // class
        { AOM_ICDF(28672), AOM_ICDF(30976), AOM_ICDF(31858), AOM_ICDF(32320),
          AOM_ICDF(32551), AOM_ICDF(32656), AOM_ICDF(32740), AOM_ICDF(32757),
          AOM_ICDF(32762), AOM_ICDF(32767), AOM_ICDF(32768), 0 },  // class_cdf
        { 208 },                                                   // class0
        { 136, 140, 148, 160, 176, 192, 224, 234, 234, 240 },      // bits
        { { 128, 128, 64 }, { 96, 112, 64 } },                     // class0_fp
        { 64, 96, 64 },                                            // fp
        { { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(26624), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(12288), AOM_ICDF(21248), AOM_ICDF(24128), AOM_ICDF(32768),
            0 } },  // class0_fp_cdf
        { AOM_ICDF(8192), AOM_ICDF(17408), AOM_ICDF(21248), AOM_ICDF(32768),
          0 },  // fp_cdf
        160,    // class0_hp bit
        128,    // hp
#if CONFIG_NEW_MULTISYMBOL
        { AOM_ICDF(160 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(216 * 128), AOM_ICDF(32768), 0 },
        { { AOM_ICDF(128 * 196), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 198), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 208), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 224), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 245), AOM_ICDF(32768), 0 },
          { AOM_ICDF(128 * 240), AOM_ICDF(32768), 0 } },  // bits_cdf
#endif
    } },
};

static const uint8_t log_in_base_2[] = {
  0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10
};

static INLINE int mv_class_base(MV_CLASS_TYPE c) {
  return c ? CLASS0_SIZE << (c + 2) : 0;
}

MV_CLASS_TYPE av1_get_mv_class(int z, int *offset) {
  const MV_CLASS_TYPE c = (z >= CLASS0_SIZE * 4096)
                              ? MV_CLASS_10
                              : (MV_CLASS_TYPE)log_in_base_2[z >> 3];
  if (offset) *offset = z - mv_class_base(c);
  return c;
}

static void inc_mv_component(int v, nmv_component_counts *comp_counts, int incr,
                             MvSubpelPrecision precision) {
  int s, z, c, o, d, e, f;
  assert(v != 0); /* should not be zero */
  s = v < 0;
  comp_counts->sign[s] += incr;
  z = (s ? -v : v) - 1; /* magnitude - 1 */

  c = av1_get_mv_class(z, &o);
  comp_counts->classes[c] += incr;

  d = (o >> 3);     /* int mv data */
  f = (o >> 1) & 3; /* fractional pel mv data */
  e = (o & 1);      /* high precision mv data */

  if (c == MV_CLASS_0) {
    comp_counts->class0[d] += incr;
#if CONFIG_INTRABC || CONFIG_AMVR
    if (precision > MV_SUBPEL_NONE)
#endif
      comp_counts->class0_fp[d][f] += incr;
    if (precision > MV_SUBPEL_LOW_PRECISION) comp_counts->class0_hp[e] += incr;
  } else {
    int i;
    int b = c + CLASS0_BITS - 1;  // number of bits
    for (i = 0; i < b; ++i) comp_counts->bits[i][((d >> i) & 1)] += incr;
#if CONFIG_INTRABC || CONFIG_AMVR
    if (precision > MV_SUBPEL_NONE)
#endif
      comp_counts->fp[f] += incr;
    if (precision > MV_SUBPEL_LOW_PRECISION) comp_counts->hp[e] += incr;
  }
}

void av1_inc_mv(const MV *mv, nmv_context_counts *counts,
                MvSubpelPrecision precision) {
  if (counts != NULL) {
    const MV_JOINT_TYPE j = av1_get_mv_joint(mv);
    ++counts->joints[j];

    if (mv_joint_vertical(j))
      inc_mv_component(mv->row, &counts->comps[0], 1, precision);

    if (mv_joint_horizontal(j))
      inc_mv_component(mv->col, &counts->comps[1], 1, precision);
  }
}

void av1_adapt_mv_probs(AV1_COMMON *cm, int allow_hp) {
  int i, j;
  int idx;
  for (idx = 0; idx < NMV_CONTEXTS; ++idx) {
    nmv_context *nmvc = &cm->fc->nmvc[idx];
    const nmv_context *pre_nmvc = &cm->pre_fc->nmvc[idx];
    const nmv_context_counts *counts = &cm->counts.mv[idx];
    aom_tree_merge_probs(av1_mv_joint_tree, pre_nmvc->joints, counts->joints,
                         nmvc->joints);
    for (i = 0; i < 2; ++i) {
      nmv_component *comp = &nmvc->comps[i];
      const nmv_component *pre_comp = &pre_nmvc->comps[i];
      const nmv_component_counts *c = &counts->comps[i];

      comp->sign = av1_mode_mv_merge_probs(pre_comp->sign, c->sign);
      aom_tree_merge_probs(av1_mv_class_tree, pre_comp->classes, c->classes,
                           comp->classes);
      aom_tree_merge_probs(av1_mv_class0_tree, pre_comp->class0, c->class0,
                           comp->class0);

      for (j = 0; j < MV_OFFSET_BITS; ++j)
        comp->bits[j] = av1_mode_mv_merge_probs(pre_comp->bits[j], c->bits[j]);
#if CONFIG_AMVR
      if (cm->cur_frame_mv_precision_level == 0) {
#endif
        for (j = 0; j < CLASS0_SIZE; ++j)
          aom_tree_merge_probs(av1_mv_fp_tree, pre_comp->class0_fp[j],
                               c->class0_fp[j], comp->class0_fp[j]);

        aom_tree_merge_probs(av1_mv_fp_tree, pre_comp->fp, c->fp, comp->fp);

        if (allow_hp) {
          comp->class0_hp =
              av1_mode_mv_merge_probs(pre_comp->class0_hp, c->class0_hp);
          comp->hp = av1_mode_mv_merge_probs(pre_comp->hp, c->hp);
        }
#if CONFIG_AMVR
      }
#endif
    }
  }
}

void av1_init_mv_probs(AV1_COMMON *cm) {
  int i;
  for (i = 0; i < NMV_CONTEXTS; ++i) {
    // NB: this sets CDFs too
    cm->fc->nmvc[i] = default_nmv_context;
  }
#if CONFIG_INTRABC
  cm->fc->ndvc = default_nmv_context;
#endif  // CONFIG_INTRABC
}
