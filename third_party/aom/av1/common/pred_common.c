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

#include "av1/common/common.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconinter.h"
#if CONFIG_EXT_INTRA
#include "av1/common/reconintra.h"
#endif  // CONFIG_EXT_INTRA
#include "av1/common/seg_common.h"

// Returns a context number for the given MB prediction signal
#if CONFIG_DUAL_FILTER
static InterpFilter get_ref_filter_type(const MODE_INFO *mi,
                                        const MACROBLOCKD *xd, int dir,
                                        MV_REFERENCE_FRAME ref_frame) {
  const MB_MODE_INFO *ref_mbmi = &mi->mbmi;
  int use_subpel[2] = {
    has_subpel_mv_component(mi, xd, dir),
    has_subpel_mv_component(mi, xd, dir + 2),
  };

  return (((ref_mbmi->ref_frame[0] == ref_frame && use_subpel[0]) ||
           (ref_mbmi->ref_frame[1] == ref_frame && use_subpel[1]))
              ? av1_extract_interp_filter(ref_mbmi->interp_filters, dir & 0x01)
              : SWITCHABLE_FILTERS);
}

int av1_get_pred_context_switchable_interp(const MACROBLOCKD *xd, int dir) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int ctx_offset =
      (mbmi->ref_frame[1] > INTRA_FRAME) * INTER_FILTER_COMP_OFFSET;
  MV_REFERENCE_FRAME ref_frame =
      (dir < 2) ? mbmi->ref_frame[0] : mbmi->ref_frame[1];
  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries corresponding to real macroblocks.
  // The prediction flags in these dummy entries are initialized to 0.
  int filter_type_ctx = ctx_offset + (dir & 0x01) * INTER_FILTER_DIR_OFFSET;
  int left_type = SWITCHABLE_FILTERS;
  int above_type = SWITCHABLE_FILTERS;

  if (xd->left_available)
    left_type = get_ref_filter_type(xd->mi[-1], xd, dir, ref_frame);

  if (xd->up_available)
    above_type =
        get_ref_filter_type(xd->mi[-xd->mi_stride], xd, dir, ref_frame);

  if (left_type == above_type) {
    filter_type_ctx += left_type;
  } else if (left_type == SWITCHABLE_FILTERS) {
    assert(above_type != SWITCHABLE_FILTERS);
    filter_type_ctx += above_type;
  } else if (above_type == SWITCHABLE_FILTERS) {
    assert(left_type != SWITCHABLE_FILTERS);
    filter_type_ctx += left_type;
  } else {
    filter_type_ctx += SWITCHABLE_FILTERS;
  }

  return filter_type_ctx;
}
#else
int av1_get_pred_context_switchable_interp(const MACROBLOCKD *xd) {
  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries corresponding to real macroblocks.
  // The prediction flags in these dummy entries are initialized to 0.
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int left_type =
      xd->left_available && is_inter_block(left_mbmi)
          ? av1_extract_interp_filter(left_mbmi->interp_filters, 0)
          : SWITCHABLE_FILTERS;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const int above_type =
      xd->up_available && is_inter_block(above_mbmi)
          ? av1_extract_interp_filter(above_mbmi->interp_filters, 0)
          : SWITCHABLE_FILTERS;

  if (left_type == above_type) {
    return left_type;
  } else if (left_type == SWITCHABLE_FILTERS) {
    assert(above_type != SWITCHABLE_FILTERS);
    return above_type;
  } else if (above_type == SWITCHABLE_FILTERS) {
    assert(left_type != SWITCHABLE_FILTERS);
    return left_type;
  } else {
    return SWITCHABLE_FILTERS;
  }
}
#endif

#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
// Obtain the reference filter type from the above/left neighbor blocks.
static INTRA_FILTER get_ref_intra_filter(const MB_MODE_INFO *ref_mbmi) {
  INTRA_FILTER ref_type = INTRA_FILTERS;

  if (ref_mbmi->sb_type >= BLOCK_8X8) {
    const PREDICTION_MODE mode = ref_mbmi->mode;
    if (is_inter_block(ref_mbmi)) {
      switch (av1_extract_interp_filter(ref_mbmi->interp_filters, 0)) {
        case EIGHTTAP_REGULAR: ref_type = INTRA_FILTER_8TAP; break;
        case EIGHTTAP_SMOOTH: ref_type = INTRA_FILTER_8TAP_SMOOTH; break;
        case MULTITAP_SHARP: ref_type = INTRA_FILTER_8TAP_SHARP; break;
        case BILINEAR: ref_type = INTRA_FILTERS; break;
        default: break;
      }
    } else {
      if (av1_is_directional_mode(mode, ref_mbmi->sb_type)) {
        const int p_angle =
            mode_to_angle_map[mode] + ref_mbmi->angle_delta[0] * ANGLE_STEP;
        if (av1_is_intra_filter_switchable(p_angle)) {
          ref_type = ref_mbmi->intra_filter;
        }
      }
    }
  }
  return ref_type;
}

int av1_get_pred_context_intra_interp(const MACROBLOCKD *xd) {
  int left_type = INTRA_FILTERS, above_type = INTRA_FILTERS;

  if (xd->left_available) left_type = get_ref_intra_filter(xd->left_mbmi);

  if (xd->up_available) above_type = get_ref_intra_filter(xd->above_mbmi);

  if (left_type == above_type)
    return left_type;
  else if (left_type == INTRA_FILTERS && above_type != INTRA_FILTERS)
    return above_type;
  else if (left_type != INTRA_FILTERS && above_type == INTRA_FILTERS)
    return left_type;
  else
    return INTRA_FILTERS;
}
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA

#if CONFIG_PALETTE_DELTA_ENCODING
int av1_get_palette_cache(const MACROBLOCKD *const xd, int plane,
                          uint16_t *cache) {
  const int row = -xd->mb_to_top_edge >> 3;
  // Do not refer to above SB row when on SB boundary.
  const MODE_INFO *const above_mi =
      (row % (1 << MIN_SB_SIZE_LOG2)) ? xd->above_mi : NULL;
  const MODE_INFO *const left_mi = xd->left_mi;
  int above_n = 0, left_n = 0;
  if (above_mi)
    above_n = above_mi->mbmi.palette_mode_info.palette_size[plane != 0];
  if (left_mi)
    left_n = left_mi->mbmi.palette_mode_info.palette_size[plane != 0];
  if (above_n == 0 && left_n == 0) return 0;
  int above_idx = plane * PALETTE_MAX_SIZE;
  int left_idx = plane * PALETTE_MAX_SIZE;
  int n = 0;
  const uint16_t *above_colors =
      above_mi ? above_mi->mbmi.palette_mode_info.palette_colors : NULL;
  const uint16_t *left_colors =
      left_mi ? left_mi->mbmi.palette_mode_info.palette_colors : NULL;
  // Merge the sorted lists of base colors from above and left to get
  // combined sorted color cache.
  while (above_n > 0 && left_n > 0) {
    uint16_t v_above = above_colors[above_idx];
    uint16_t v_left = left_colors[left_idx];
    if (v_left < v_above) {
      if (n == 0 || v_left != cache[n - 1]) cache[n++] = v_left;
      ++left_idx, --left_n;
    } else {
      if (n == 0 || v_above != cache[n - 1]) cache[n++] = v_above;
      ++above_idx, --above_n;
      if (v_left == v_above) ++left_idx, --left_n;
    }
  }
  while (above_n-- > 0) {
    uint16_t val = above_colors[above_idx++];
    if (n == 0 || val != cache[n - 1]) cache[n++] = val;
  }
  while (left_n-- > 0) {
    uint16_t val = left_colors[left_idx++];
    if (n == 0 || val != cache[n - 1]) cache[n++] = val;
  }
  assert(n <= 2 * PALETTE_MAX_SIZE);
  return n;
}
#endif  // CONFIG_PALETTE_DELTA_ENCODING

// The mode info data structure has a one element border above and to the
// left of the entries corresponding to real macroblocks.
// The prediction flags in these dummy entries are initialized to 0.
// 0 - inter/inter, inter/--, --/inter, --/--
// 1 - intra/inter, inter/intra
// 2 - intra/--, --/intra
// 3 - intra/intra
int av1_get_intra_inter_context(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  if (has_above && has_left) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);
    return left_intra && above_intra ? 3 : left_intra || above_intra;
  } else if (has_above || has_left) {  // one edge available
    return 2 * !is_inter_block(has_above ? above_mbmi : left_mbmi);
  } else {
    return 0;
  }
}

#if CONFIG_COMPOUND_SINGLEREF
// The compound/single mode info data structure has one element border above and
// to the left of the entries corresponding to real macroblocks.
// The prediction flags in these dummy entries are initialized to 0.
int av1_get_inter_mode_context(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  if (has_above && has_left) {  // both edges available
    const int above_inter_comp_mode =
        is_inter_anyref_comp_mode(above_mbmi->mode);
    const int left_inter_comp_mode = is_inter_anyref_comp_mode(left_mbmi->mode);
    if (above_inter_comp_mode && left_inter_comp_mode)
      return 0;
    else if (above_inter_comp_mode || left_inter_comp_mode)
      return 1;
    else if (!is_inter_block(above_mbmi) && !is_inter_block(left_mbmi))
      return 2;
    else
      return 3;
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *const edge_mbmi = has_above ? above_mbmi : left_mbmi;
    if (is_inter_anyref_comp_mode(edge_mbmi->mode))
      return 1;
    else if (!is_inter_block(edge_mbmi))
      return 2;
    else
      return 3;
  } else {  // no edge available
    return 2;
  }
}
#endif  // CONFIG_COMPOUND_SINGLEREF

#if CONFIG_EXT_REFS
#define CHECK_BACKWARD_REFS(ref_frame) \
  (((ref_frame) >= BWDREF_FRAME) && ((ref_frame) <= ALTREF_FRAME))
#define IS_BACKWARD_REF_FRAME(ref_frame) CHECK_BACKWARD_REFS(ref_frame)
#else
#define IS_BACKWARD_REF_FRAME(ref_frame) ((ref_frame) == cm->comp_fixed_ref)
#endif  // CONFIG_EXT_REFS

#define CHECK_GOLDEN_OR_LAST3(ref_frame) \
  (((ref_frame) == GOLDEN_FRAME) || ((ref_frame) == LAST3_FRAME))

int av1_get_reference_mode_context(const AV1_COMMON *cm,
                                   const MACROBLOCKD *xd) {
  int ctx;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

#if CONFIG_EXT_REFS
  (void)cm;
#endif  // CONFIG_EXT_REFS

  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries corresponding to real macroblocks.
  // The prediction flags in these dummy entries are initialized to 0.
  if (has_above && has_left) {  // both edges available
    if (!has_second_ref(above_mbmi) && !has_second_ref(left_mbmi))
      // neither edge uses comp pred (0/1)
      ctx = IS_BACKWARD_REF_FRAME(above_mbmi->ref_frame[0]) ^
            IS_BACKWARD_REF_FRAME(left_mbmi->ref_frame[0]);
    else if (!has_second_ref(above_mbmi))
      // one of two edges uses comp pred (2/3)
      ctx = 2 + (IS_BACKWARD_REF_FRAME(above_mbmi->ref_frame[0]) ||
                 !is_inter_block(above_mbmi));
    else if (!has_second_ref(left_mbmi))
      // one of two edges uses comp pred (2/3)
      ctx = 2 + (IS_BACKWARD_REF_FRAME(left_mbmi->ref_frame[0]) ||
                 !is_inter_block(left_mbmi));
    else  // both edges use comp pred (4)
      ctx = 4;
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = has_above ? above_mbmi : left_mbmi;

    if (!has_second_ref(edge_mbmi))
      // edge does not use comp pred (0/1)
      ctx = IS_BACKWARD_REF_FRAME(edge_mbmi->ref_frame[0]);
    else
      // edge uses comp pred (3)
      ctx = 3;
  } else {  // no edges available (1)
    ctx = 1;
  }
  assert(ctx >= 0 && ctx < COMP_INTER_CONTEXTS);
  return ctx;
}

#if CONFIG_EXT_COMP_REFS
// TODO(zoeliu): To try on the design of 3 contexts, instead of 5:
//               COMP_REF_TYPE_CONTEXTS = 3
int av1_get_comp_reference_type_context(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  if (above_in_image && left_in_image) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter
      const MB_MODE_INFO *inter_mbmi = above_intra ? left_mbmi : above_mbmi;

      if (!has_second_ref(inter_mbmi))  // single pred
        pred_context = 2;
      else  // comp pred
        pred_context = 1 + 2 * has_uni_comp_refs(inter_mbmi);
    } else {  // inter/inter
      const int a_sg = !has_second_ref(above_mbmi);
      const int l_sg = !has_second_ref(left_mbmi);
      const MV_REFERENCE_FRAME frfa = above_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME frfl = left_mbmi->ref_frame[0];

      if (a_sg && l_sg) {  // single/single
        pred_context =
            1 +
            2 * (!(IS_BACKWARD_REF_FRAME(frfa) ^ IS_BACKWARD_REF_FRAME(frfl)));
      } else if (l_sg || a_sg) {  // single/comp
        const int uni_rfc =
            a_sg ? has_uni_comp_refs(left_mbmi) : has_uni_comp_refs(above_mbmi);

        if (!uni_rfc)  // comp bidir
          pred_context = 1;
        else  // comp unidir
          pred_context = 3 + (!(IS_BACKWARD_REF_FRAME(frfa) ^
                                IS_BACKWARD_REF_FRAME(frfl)));
      } else {  // comp/comp
        const int a_uni_rfc = has_uni_comp_refs(above_mbmi);
        const int l_uni_rfc = has_uni_comp_refs(left_mbmi);

        if (!a_uni_rfc && !l_uni_rfc)  // bidir/bidir
          pred_context = 0;
        else if (!a_uni_rfc || !l_uni_rfc)  // unidir/bidir
          pred_context = 2;
        else  // unidir/unidir
          pred_context =
              3 + (!((frfa == BWDREF_FRAME) ^ (frfl == BWDREF_FRAME)));
      }
    }
  } else if (above_in_image || left_in_image) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = above_in_image ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi)) {  // intra
      pred_context = 2;
    } else {                           // inter
      if (!has_second_ref(edge_mbmi))  // single pred
        pred_context = 2;
      else  // comp pred
        pred_context = 4 * has_uni_comp_refs(edge_mbmi);
    }
  } else {  // no edges available
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < COMP_REF_TYPE_CONTEXTS);
  return pred_context;
}

// Returns a context number for the given MB prediction signal
//
// Signal the uni-directional compound reference frame pair as either
// (BWDREF, ALTREF), or (LAST, LAST2) / (LAST, LAST3) / (LAST, GOLDEN),
// conditioning on the pair is known as uni-directional.
//
// 3 contexts: Voting is used to compare the count of forward references with
//             that of backward references from the spatial neighbors.
int av1_get_pred_context_uni_comp_ref_p(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  // Count of forward references (L, L2, L3, or G)
  int frf_count = 0;
  // Count of backward references (B or A)
  int brf_count = 0;

  if (above_in_image && is_inter_block(above_mbmi)) {
    if (above_mbmi->ref_frame[0] <= GOLDEN_FRAME)
      ++frf_count;
    else
      ++brf_count;
    if (has_second_ref(above_mbmi)) {
      if (above_mbmi->ref_frame[1] <= GOLDEN_FRAME)
        ++frf_count;
      else
        ++brf_count;
    }
  }

  if (left_in_image && is_inter_block(left_mbmi)) {
    if (left_mbmi->ref_frame[0] <= GOLDEN_FRAME)
      ++frf_count;
    else
      ++brf_count;
    if (has_second_ref(left_mbmi)) {
      if (left_mbmi->ref_frame[1] <= GOLDEN_FRAME)
        ++frf_count;
      else
        ++brf_count;
    }
  }

  pred_context =
      (frf_count == brf_count) ? 1 : ((frf_count < brf_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < UNI_COMP_REF_CONTEXTS);
  return pred_context;
}

// Returns a context number for the given MB prediction signal
//
// Signal the uni-directional compound reference frame pair as
// either (LAST, LAST2), or (LAST, LAST3) / (LAST, GOLDEN),
// conditioning on the pair is known as one of the above three.
//
// 3 contexts: Voting is used to compare the count of LAST2_FRAME with the
//             total count of LAST3/GOLDEN from the spatial neighbors.
int av1_get_pred_context_uni_comp_ref_p1(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  // Count of LAST2
  int last2_count = 0;
  // Count of LAST3 or GOLDEN
  int last3_or_gld_count = 0;

  if (above_in_image && is_inter_block(above_mbmi)) {
    last2_count = (above_mbmi->ref_frame[0] == LAST2_FRAME) ? last2_count + 1
                                                            : last2_count;
    last3_or_gld_count = CHECK_GOLDEN_OR_LAST3(above_mbmi->ref_frame[0])
                             ? last3_or_gld_count + 1
                             : last3_or_gld_count;
    if (has_second_ref(above_mbmi)) {
      last2_count = (above_mbmi->ref_frame[1] == LAST2_FRAME) ? last2_count + 1
                                                              : last2_count;
      last3_or_gld_count = CHECK_GOLDEN_OR_LAST3(above_mbmi->ref_frame[1])
                               ? last3_or_gld_count + 1
                               : last3_or_gld_count;
    }
  }

  if (left_in_image && is_inter_block(left_mbmi)) {
    last2_count = (left_mbmi->ref_frame[0] == LAST2_FRAME) ? last2_count + 1
                                                           : last2_count;
    last3_or_gld_count = CHECK_GOLDEN_OR_LAST3(left_mbmi->ref_frame[0])
                             ? last3_or_gld_count + 1
                             : last3_or_gld_count;
    if (has_second_ref(left_mbmi)) {
      last2_count = (left_mbmi->ref_frame[1] == LAST2_FRAME) ? last2_count + 1
                                                             : last2_count;
      last3_or_gld_count = CHECK_GOLDEN_OR_LAST3(left_mbmi->ref_frame[1])
                               ? last3_or_gld_count + 1
                               : last3_or_gld_count;
    }
  }

  pred_context = (last2_count == last3_or_gld_count)
                     ? 1
                     : ((last2_count < last3_or_gld_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < UNI_COMP_REF_CONTEXTS);
  return pred_context;
}

// Returns a context number for the given MB prediction signal
//
// Signal the uni-directional compound reference frame pair as
// either (LAST, LAST3) or (LAST, GOLDEN),
// conditioning on the pair is known as one of the above two.
//
// 3 contexts: Voting is used to compare the count of LAST3_FRAME with the
//             total count of GOLDEN_FRAME from the spatial neighbors.
int av1_get_pred_context_uni_comp_ref_p2(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  // Count of LAST3
  int last3_count = 0;
  // Count of GOLDEN
  int gld_count = 0;

  if (above_in_image && is_inter_block(above_mbmi)) {
    last3_count = (above_mbmi->ref_frame[0] == LAST3_FRAME) ? last3_count + 1
                                                            : last3_count;
    gld_count =
        (above_mbmi->ref_frame[0] == GOLDEN_FRAME) ? gld_count + 1 : gld_count;
    if (has_second_ref(above_mbmi)) {
      last3_count = (above_mbmi->ref_frame[1] == LAST3_FRAME) ? last3_count + 1
                                                              : last3_count;
      gld_count = (above_mbmi->ref_frame[1] == GOLDEN_FRAME) ? gld_count + 1
                                                             : gld_count;
    }
  }

  if (left_in_image && is_inter_block(left_mbmi)) {
    last3_count = (left_mbmi->ref_frame[0] == LAST3_FRAME) ? last3_count + 1
                                                           : last3_count;
    gld_count =
        (left_mbmi->ref_frame[0] == GOLDEN_FRAME) ? gld_count + 1 : gld_count;
    if (has_second_ref(left_mbmi)) {
      last3_count = (left_mbmi->ref_frame[1] == LAST3_FRAME) ? last3_count + 1
                                                             : last3_count;
      gld_count =
          (left_mbmi->ref_frame[1] == GOLDEN_FRAME) ? gld_count + 1 : gld_count;
    }
  }

  pred_context =
      (last3_count == gld_count) ? 1 : ((last3_count < gld_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < UNI_COMP_REF_CONTEXTS);
  return pred_context;
}
#endif  // CONFIG_EXT_COMP_REFS

#if CONFIG_EXT_REFS

// TODO(zoeliu): Future work will be conducted to optimize the context design
//               for the coding of the reference frames.

#define CHECK_LAST_OR_LAST2(ref_frame) \
  ((ref_frame == LAST_FRAME) || (ref_frame == LAST2_FRAME))

// Returns a context number for the given MB prediction signal
// Signal the first reference frame for a compound mode be either
// GOLDEN/LAST3, or LAST/LAST2.
//
// NOTE(zoeliu): The probability of ref_frame[0] is either
//               GOLDEN_FRAME or LAST3_FRAME.
int av1_get_pred_context_comp_ref_p(const AV1_COMMON *cm,
                                    const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

// Note:
// The mode info data structure has a one element border above and to the
// left of the entries correpsonding to real macroblocks.
// The prediction flags in these dummy entries are initialised to 0.
#if CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS
  // Code seems to assume that signbias of cm->comp_bwd_ref[0] is always 1
  const int bwd_ref_sign_idx = 1;
#else
  const int bwd_ref_sign_idx = cm->ref_frame_sign_bias[cm->comp_bwd_ref[0]];
#endif  // CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS
  const int fwd_ref_sign_idx = !bwd_ref_sign_idx;

  (void)cm;

  if (above_in_image && left_in_image) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra (2)
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;

      if (!has_second_ref(edge_mbmi))  // single pred (1/3)
        pred_context =
            1 + 2 * (!CHECK_GOLDEN_OR_LAST3(edge_mbmi->ref_frame[0]));
      else  // comp pred (1/3)
        pred_context = 1 +
                       2 * (!CHECK_GOLDEN_OR_LAST3(
                               edge_mbmi->ref_frame[fwd_ref_sign_idx]));
    } else {  // inter/inter
      const int l_sg = !has_second_ref(left_mbmi);
      const int a_sg = !has_second_ref(above_mbmi);
      const MV_REFERENCE_FRAME frfa =
          a_sg ? above_mbmi->ref_frame[0]
               : above_mbmi->ref_frame[fwd_ref_sign_idx];
      const MV_REFERENCE_FRAME frfl =
          l_sg ? left_mbmi->ref_frame[0]
               : left_mbmi->ref_frame[fwd_ref_sign_idx];

      if (frfa == frfl && CHECK_GOLDEN_OR_LAST3(frfa)) {
        pred_context = 0;
      } else if (l_sg && a_sg) {  // single/single
        if ((CHECK_BACKWARD_REFS(frfa) && CHECK_LAST_OR_LAST2(frfl)) ||
            (CHECK_BACKWARD_REFS(frfl) && CHECK_LAST_OR_LAST2(frfa))) {
          pred_context = 4;
        } else if (CHECK_GOLDEN_OR_LAST3(frfa) || CHECK_GOLDEN_OR_LAST3(frfl)) {
          pred_context = 1;
        } else {
          pred_context = 3;
        }
      } else if (l_sg || a_sg) {  // single/comp
        const MV_REFERENCE_FRAME frfc = l_sg ? frfa : frfl;
        const MV_REFERENCE_FRAME rfs = a_sg ? frfa : frfl;

        if (CHECK_GOLDEN_OR_LAST3(frfc) && !CHECK_GOLDEN_OR_LAST3(rfs))
          pred_context = 1;
        else if (CHECK_GOLDEN_OR_LAST3(rfs) && !CHECK_GOLDEN_OR_LAST3(frfc))
          pred_context = 2;
        else
          pred_context = 4;
      } else {  // comp/comp
        if ((CHECK_LAST_OR_LAST2(frfa) && CHECK_LAST_OR_LAST2(frfl))) {
          pred_context = 4;
        } else {
// NOTE(zoeliu): Following assert may be removed once confirmed.
#if !USE_UNI_COMP_REFS
          // TODO(zoeliu): To further study the UNIDIR scenario
          assert(CHECK_GOLDEN_OR_LAST3(frfa) || CHECK_GOLDEN_OR_LAST3(frfl));
#endif  // !USE_UNI_COMP_REFS
          pred_context = 2;
        }
      }
    }
  } else if (above_in_image || left_in_image) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = above_in_image ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi)) {
      pred_context = 2;
    } else {
      if (has_second_ref(edge_mbmi))
        pred_context =
            4 *
            (!CHECK_GOLDEN_OR_LAST3(edge_mbmi->ref_frame[fwd_ref_sign_idx]));
      else
        pred_context = 3 * (!CHECK_GOLDEN_OR_LAST3(edge_mbmi->ref_frame[0]));
    }
  } else {  // no edges available (2)
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);

  return pred_context;
}

// Returns a context number for the given MB prediction signal
// Signal the first reference frame for a compound mode be LAST,
// conditioning on that it is known either LAST/LAST2.
//
// NOTE(zoeliu): The probability of ref_frame[0] is LAST_FRAME,
// conditioning on it is either LAST_FRAME or LAST2_FRAME.
int av1_get_pred_context_comp_ref_p1(const AV1_COMMON *cm,
                                     const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

// Note:
// The mode info data structure has a one element border above and to the
// left of the entries correpsonding to real macroblocks.
// The prediction flags in these dummy entries are initialised to 0.
#if CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS
  // Code seems to assume that signbias of cm->comp_bwd_ref[0] is always 1
  const int bwd_ref_sign_idx = 1;
#else
  const int bwd_ref_sign_idx = cm->ref_frame_sign_bias[cm->comp_bwd_ref[0]];
#endif  //  CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS
  const int fwd_ref_sign_idx = !bwd_ref_sign_idx;

  (void)cm;

  if (above_in_image && left_in_image) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra (2)
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;

      if (!has_second_ref(edge_mbmi))  // single pred (1/3)
        pred_context = 1 + 2 * (edge_mbmi->ref_frame[0] != LAST_FRAME);
      else  // comp pred (1/3)
        pred_context =
            1 + 2 * (edge_mbmi->ref_frame[fwd_ref_sign_idx] != LAST_FRAME);
    } else {  // inter/inter
      const int l_sg = !has_second_ref(left_mbmi);
      const int a_sg = !has_second_ref(above_mbmi);
      const MV_REFERENCE_FRAME frfa =
          a_sg ? above_mbmi->ref_frame[0]
               : above_mbmi->ref_frame[fwd_ref_sign_idx];
      const MV_REFERENCE_FRAME frfl =
          l_sg ? left_mbmi->ref_frame[0]
               : left_mbmi->ref_frame[fwd_ref_sign_idx];

      if (frfa == frfl && frfa == LAST_FRAME)
        pred_context = 0;
      else if (l_sg && a_sg) {  // single/single
        if (frfa == LAST_FRAME || frfl == LAST_FRAME)
          pred_context = 1;
        else if (CHECK_GOLDEN_OR_LAST3(frfa) || CHECK_GOLDEN_OR_LAST3(frfl))
          pred_context = 2 + (frfa != frfl);
        else if (frfa == frfl ||
                 (CHECK_BACKWARD_REFS(frfa) && CHECK_BACKWARD_REFS(frfl)))
          pred_context = 3;
        else
          pred_context = 4;
      } else if (l_sg || a_sg) {  // single/comp
        const MV_REFERENCE_FRAME frfc = l_sg ? frfa : frfl;
        const MV_REFERENCE_FRAME rfs = a_sg ? frfa : frfl;

        if (frfc == LAST_FRAME && rfs != LAST_FRAME)
          pred_context = 1;
        else if (rfs == LAST_FRAME && frfc != LAST_FRAME)
          pred_context = 2;
        else
          pred_context =
              3 + (frfc == LAST2_FRAME || CHECK_GOLDEN_OR_LAST3(rfs));
      } else {  // comp/comp
        if (frfa == LAST_FRAME || frfl == LAST_FRAME)
          pred_context = 2;
        else
          pred_context =
              3 + (CHECK_GOLDEN_OR_LAST3(frfa) || CHECK_GOLDEN_OR_LAST3(frfl));
      }
    }
  } else if (above_in_image || left_in_image) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = above_in_image ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi)) {
      pred_context = 2;
    } else {
      if (has_second_ref(edge_mbmi)) {
        pred_context =
            4 * (edge_mbmi->ref_frame[fwd_ref_sign_idx] != LAST_FRAME);
      } else {
        if (edge_mbmi->ref_frame[0] == LAST_FRAME)
          pred_context = 0;
        else
          pred_context = 2 + CHECK_GOLDEN_OR_LAST3(edge_mbmi->ref_frame[0]);
      }
    }
  } else {  // no edges available (2)
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);

  return pred_context;
}

// Returns a context number for the given MB prediction signal
// Signal the first reference frame for a compound mode be GOLDEN,
// conditioning on that it is known either GOLDEN or LAST3.
//
// NOTE(zoeliu): The probability of ref_frame[0] is GOLDEN_FRAME,
// conditioning on it is either GOLDEN or LAST3.
int av1_get_pred_context_comp_ref_p2(const AV1_COMMON *cm,
                                     const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

// Note:
// The mode info data structure has a one element border above and to the
// left of the entries correpsonding to real macroblocks.
// The prediction flags in these dummy entries are initialised to 0.
#if CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS
  const int bwd_ref_sign_idx = 1;
#else
  const int bwd_ref_sign_idx = cm->ref_frame_sign_bias[cm->comp_bwd_ref[0]];
#endif  // CONFIG_ONE_SIDED_COMPOUND || CONFIG_FRAME_SIGN_BIAS
  const int fwd_ref_sign_idx = !bwd_ref_sign_idx;

  (void)cm;

  if (above_in_image && left_in_image) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra (2)
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;

      if (!has_second_ref(edge_mbmi))  // single pred (1/3)
        pred_context = 1 + 2 * (edge_mbmi->ref_frame[0] != GOLDEN_FRAME);
      else  // comp pred (1/3)
        pred_context =
            1 + 2 * (edge_mbmi->ref_frame[fwd_ref_sign_idx] != GOLDEN_FRAME);
    } else {  // inter/inter
      const int l_sg = !has_second_ref(left_mbmi);
      const int a_sg = !has_second_ref(above_mbmi);
      const MV_REFERENCE_FRAME frfa =
          a_sg ? above_mbmi->ref_frame[0]
               : above_mbmi->ref_frame[fwd_ref_sign_idx];
      const MV_REFERENCE_FRAME frfl =
          l_sg ? left_mbmi->ref_frame[0]
               : left_mbmi->ref_frame[fwd_ref_sign_idx];

      if (frfa == frfl && frfa == GOLDEN_FRAME)
        pred_context = 0;
      else if (l_sg && a_sg) {  // single/single
        if (frfa == GOLDEN_FRAME || frfl == GOLDEN_FRAME)
          pred_context = 1;
        else if (CHECK_LAST_OR_LAST2(frfa) || CHECK_LAST_OR_LAST2(frfl))
          pred_context = 2 + (frfa != frfl);
        else if (frfa == frfl ||
                 (CHECK_BACKWARD_REFS(frfa) && CHECK_BACKWARD_REFS(frfl)))
          pred_context = 3;
        else
          pred_context = 4;
      } else if (l_sg || a_sg) {  // single/comp
        const MV_REFERENCE_FRAME frfc = l_sg ? frfa : frfl;
        const MV_REFERENCE_FRAME rfs = a_sg ? frfa : frfl;

        if (frfc == GOLDEN_FRAME && rfs != GOLDEN_FRAME)
          pred_context = 1;
        else if (rfs == GOLDEN_FRAME && frfc != GOLDEN_FRAME)
          pred_context = 2;
        else
          pred_context = 3 + (frfc == LAST3_FRAME || CHECK_LAST_OR_LAST2(rfs));
      } else {  // comp/comp
        if (frfa == GOLDEN_FRAME || frfl == GOLDEN_FRAME)
          pred_context = 2;
        else
          pred_context =
              3 + (CHECK_LAST_OR_LAST2(frfa) || CHECK_LAST_OR_LAST2(frfl));
      }
    }
  } else if (above_in_image || left_in_image) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = above_in_image ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi)) {
      pred_context = 2;
    } else {
      if (has_second_ref(edge_mbmi)) {
        pred_context =
            4 * (edge_mbmi->ref_frame[fwd_ref_sign_idx] != GOLDEN_FRAME);
      } else {
        if (edge_mbmi->ref_frame[0] == GOLDEN_FRAME)
          pred_context = 0;
        else
          pred_context = 2 + CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[0]);
      }
    }
  } else {  // no edges available (2)
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);

  return pred_context;
}

// Obtain contexts to signal a reference frame be either BWDREF/ALTREF2, or
// ALTREF.
int av1_get_pred_context_brfarf2_or_arf(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  // Counts of BWDREF, ALTREF2, or ALTREF frames (B, A2, or A)
  int bwdref_counts[ALTREF_FRAME - BWDREF_FRAME + 1] = { 0 };

  if (above_in_image && is_inter_block(above_mbmi)) {
    if (above_mbmi->ref_frame[0] >= BWDREF_FRAME)
      ++bwdref_counts[above_mbmi->ref_frame[0] - BWDREF_FRAME];
    if (has_second_ref(above_mbmi)) {
      if (above_mbmi->ref_frame[1] >= BWDREF_FRAME)
        ++bwdref_counts[above_mbmi->ref_frame[1] - BWDREF_FRAME];
    }
  }

  if (left_in_image && is_inter_block(left_mbmi)) {
    if (left_mbmi->ref_frame[0] >= BWDREF_FRAME)
      ++bwdref_counts[left_mbmi->ref_frame[0] - BWDREF_FRAME];
    if (has_second_ref(left_mbmi)) {
      if (left_mbmi->ref_frame[1] >= BWDREF_FRAME)
        ++bwdref_counts[left_mbmi->ref_frame[1] - BWDREF_FRAME];
    }
  }

  const int brfarf2_count = bwdref_counts[BWDREF_FRAME - BWDREF_FRAME] +
                            bwdref_counts[ALTREF2_FRAME - BWDREF_FRAME];
  const int arf_count = bwdref_counts[ALTREF_FRAME - BWDREF_FRAME];
  const int pred_context =
      (brfarf2_count == arf_count) ? 1 : ((brfarf2_count < arf_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// Obtain contexts to signal a reference frame be either BWDREF or ALTREF2.
int av1_get_pred_context_brf_or_arf2(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  // Count of BWDREF frames (B)
  int brf_count = 0;
  // Count of ALTREF2 frames (A2)
  int arf2_count = 0;

  if (above_in_image && is_inter_block(above_mbmi)) {
    if (above_mbmi->ref_frame[0] == BWDREF_FRAME)
      ++brf_count;
    else if (above_mbmi->ref_frame[0] == ALTREF2_FRAME)
      ++arf2_count;
    if (has_second_ref(above_mbmi)) {
      if (above_mbmi->ref_frame[1] == BWDREF_FRAME)
        ++brf_count;
      else if (above_mbmi->ref_frame[1] == ALTREF2_FRAME)
        ++arf2_count;
    }
  }

  if (left_in_image && is_inter_block(left_mbmi)) {
    if (left_mbmi->ref_frame[0] == BWDREF_FRAME)
      ++brf_count;
    else if (left_mbmi->ref_frame[0] == ALTREF2_FRAME)
      ++arf2_count;
    if (has_second_ref(left_mbmi)) {
      if (left_mbmi->ref_frame[1] == BWDREF_FRAME)
        ++brf_count;
      else if (left_mbmi->ref_frame[1] == ALTREF2_FRAME)
        ++arf2_count;
    }
  }

  const int pred_context =
      (brf_count == arf2_count) ? 1 : ((brf_count < arf2_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// Signal the 2nd reference frame for a compound mode be either
// ALTREF, or ALTREF2/BWDREF.
int av1_get_pred_context_comp_bwdref_p(const AV1_COMMON *cm,
                                       const MACROBLOCKD *xd) {
  (void)cm;
  return av1_get_pred_context_brfarf2_or_arf(xd);
}

// Signal the 2nd reference frame for a compound mode be either
// ALTREF2 or BWDREF.
int av1_get_pred_context_comp_bwdref_p1(const AV1_COMMON *cm,
                                        const MACROBLOCKD *xd) {
  (void)cm;
  return av1_get_pred_context_brf_or_arf2(xd);
}

#else  // !CONFIG_EXT_REFS

// Returns a context number for the given MB prediction signal
int av1_get_pred_context_comp_ref_p(const AV1_COMMON *cm,
                                    const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries corresponding to real macroblocks.
  // The prediction flags in these dummy entries are initialized to 0.
  const int fix_ref_idx = cm->ref_frame_sign_bias[cm->comp_fixed_ref];
  const int var_ref_idx = !fix_ref_idx;

  if (above_in_image && left_in_image) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra (2)
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;

      if (!has_second_ref(edge_mbmi))  // single pred (1/3)
        pred_context = 1 + 2 * (edge_mbmi->ref_frame[0] != cm->comp_var_ref[1]);
      else  // comp pred (1/3)
        pred_context =
            1 + 2 * (edge_mbmi->ref_frame[var_ref_idx] != cm->comp_var_ref[1]);
    } else {  // inter/inter
      const int l_sg = !has_second_ref(left_mbmi);
      const int a_sg = !has_second_ref(above_mbmi);
      const MV_REFERENCE_FRAME vrfa =
          a_sg ? above_mbmi->ref_frame[0] : above_mbmi->ref_frame[var_ref_idx];
      const MV_REFERENCE_FRAME vrfl =
          l_sg ? left_mbmi->ref_frame[0] : left_mbmi->ref_frame[var_ref_idx];

      if (vrfa == vrfl && cm->comp_var_ref[1] == vrfa) {
        pred_context = 0;
      } else if (l_sg && a_sg) {  // single/single
        if ((vrfa == cm->comp_fixed_ref && vrfl == cm->comp_var_ref[0]) ||
            (vrfl == cm->comp_fixed_ref && vrfa == cm->comp_var_ref[0]))
          pred_context = 4;
        else if (vrfa == vrfl)
          pred_context = 3;
        else
          pred_context = 1;
      } else if (l_sg || a_sg) {  // single/comp
        const MV_REFERENCE_FRAME vrfc = l_sg ? vrfa : vrfl;
        const MV_REFERENCE_FRAME rfs = a_sg ? vrfa : vrfl;
        if (vrfc == cm->comp_var_ref[1] && rfs != cm->comp_var_ref[1])
          pred_context = 1;
        else if (rfs == cm->comp_var_ref[1] && vrfc != cm->comp_var_ref[1])
          pred_context = 2;
        else
          pred_context = 4;
      } else if (vrfa == vrfl) {  // comp/comp
        pred_context = 4;
      } else {
        pred_context = 2;
      }
    }
  } else if (above_in_image || left_in_image) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = above_in_image ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi)) {
      pred_context = 2;
    } else {
      if (has_second_ref(edge_mbmi))
        pred_context =
            4 * (edge_mbmi->ref_frame[var_ref_idx] != cm->comp_var_ref[1]);
      else
        pred_context = 3 * (edge_mbmi->ref_frame[0] != cm->comp_var_ref[1]);
    }
  } else {  // no edges available (2)
    pred_context = 2;
  }
  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);

  return pred_context;
}

#endif  // CONFIG_EXT_REFS

#if CONFIG_EXT_REFS

// For the bit to signal whether the single reference is a forward reference
// frame or a backward reference frame.
int av1_get_pred_context_single_ref_p1(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries correpsonding to real macroblocks.
  // The prediction flags in these dummy entries are initialised to 0.
  if (has_above && has_left) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter or inter/intra
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;

      if (!has_second_ref(edge_mbmi))  // single
        pred_context = 4 * (!CHECK_BACKWARD_REFS(edge_mbmi->ref_frame[0]));
      else  // comp
        pred_context = 2;
    } else {  // inter/inter
      const int above_has_second = has_second_ref(above_mbmi);
      const int left_has_second = has_second_ref(left_mbmi);

      const MV_REFERENCE_FRAME above0 = above_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME left0 = left_mbmi->ref_frame[0];

      if (above_has_second && left_has_second) {  // comp/comp
        pred_context = 2;
      } else if (above_has_second || left_has_second) {  // single/comp
        const MV_REFERENCE_FRAME rfs = !above_has_second ? above0 : left0;

        pred_context = (!CHECK_BACKWARD_REFS(rfs)) ? 4 : 1;
      } else {  // single/single
        pred_context = 2 * (!CHECK_BACKWARD_REFS(above0)) +
                       2 * (!CHECK_BACKWARD_REFS(left0));
      }
    }
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = has_above ? above_mbmi : left_mbmi;
    if (!is_inter_block(edge_mbmi)) {  // intra
      pred_context = 2;
    } else {                           // inter
      if (!has_second_ref(edge_mbmi))  // single
        pred_context = 4 * (!CHECK_BACKWARD_REFS(edge_mbmi->ref_frame[0]));
      else  // comp
        pred_context = 2;
    }
  } else {  // no edges available
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// For the bit to signal whether the single reference is ALTREF_FRAME or
// non-ALTREF backward reference frame, knowing that it shall be either of
// these 2 choices.
int av1_get_pred_context_single_ref_p2(const MACROBLOCKD *xd) {
  return av1_get_pred_context_brfarf2_or_arf(xd);
}

// For the bit to signal whether the single reference is LAST3/GOLDEN or
// LAST2/LAST, knowing that it shall be either of these 2 choices.
int av1_get_pred_context_single_ref_p3(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries correpsonding to real macroblocks.
  // The prediction flags in these dummy entries are initialised to 0.
  if (has_above && has_left) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter or inter/intra
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;
      if (!has_second_ref(edge_mbmi)) {  // single
        if (CHECK_BACKWARD_REFS(edge_mbmi->ref_frame[0]))
          pred_context = 3;
        else
          pred_context = 4 * CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[0]);
      } else {  // comp
        pred_context = 1 +
                       2 * (CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[0]) ||
                            CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[1]));
      }
    } else {  // inter/inter
      const int above_has_second = has_second_ref(above_mbmi);
      const int left_has_second = has_second_ref(left_mbmi);
      const MV_REFERENCE_FRAME above0 = above_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME above1 = above_mbmi->ref_frame[1];
      const MV_REFERENCE_FRAME left0 = left_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME left1 = left_mbmi->ref_frame[1];

      if (above_has_second && left_has_second) {  // comp/comp
        if (above0 == left0 && above1 == left1)
          pred_context =
              3 * (CHECK_LAST_OR_LAST2(above0) || CHECK_LAST_OR_LAST2(above1) ||
                   CHECK_LAST_OR_LAST2(left0) || CHECK_LAST_OR_LAST2(left1));
        else
          pred_context = 2;
      } else if (above_has_second || left_has_second) {  // single/comp
        const MV_REFERENCE_FRAME rfs = !above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf1 = above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf2 = above_has_second ? above1 : left1;

        if (CHECK_LAST_OR_LAST2(rfs))
          pred_context =
              3 + (CHECK_LAST_OR_LAST2(crf1) || CHECK_LAST_OR_LAST2(crf2));
        else if (CHECK_GOLDEN_OR_LAST3(rfs))
          pred_context =
              (CHECK_LAST_OR_LAST2(crf1) || CHECK_LAST_OR_LAST2(crf2));
        else
          pred_context =
              1 + 2 * (CHECK_LAST_OR_LAST2(crf1) || CHECK_LAST_OR_LAST2(crf2));
      } else {  // single/single
        if (CHECK_BACKWARD_REFS(above0) && CHECK_BACKWARD_REFS(left0)) {
          pred_context = 2 + (above0 == left0);
        } else if (CHECK_BACKWARD_REFS(above0) || CHECK_BACKWARD_REFS(left0)) {
          const MV_REFERENCE_FRAME edge0 =
              CHECK_BACKWARD_REFS(above0) ? left0 : above0;
          pred_context = 4 * CHECK_LAST_OR_LAST2(edge0);
        } else {
          pred_context =
              2 * CHECK_LAST_OR_LAST2(above0) + 2 * CHECK_LAST_OR_LAST2(left0);
        }
      }
    }
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = has_above ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi) ||
        (CHECK_BACKWARD_REFS(edge_mbmi->ref_frame[0]) &&
         !has_second_ref(edge_mbmi)))
      pred_context = 2;
    else if (!has_second_ref(edge_mbmi))  // single
      pred_context = 4 * (CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[0]));
    else  // comp
      pred_context = 3 * (CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[0]) ||
                          CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[1]));
  } else {  // no edges available (2)
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// For the bit to signal whether the single reference is LAST2_FRAME or
// LAST_FRAME, knowing that it shall be either of these 2 choices.
//
// NOTE(zoeliu): The probability of ref_frame[0] is LAST2_FRAME, conditioning
// on it is either LAST2_FRAME/LAST_FRAME.
int av1_get_pred_context_single_ref_p4(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries correpsonding to real macroblocks.
  // The prediction flags in these dummy entries are initialised to 0.
  if (has_above && has_left) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter or inter/intra
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;
      if (!has_second_ref(edge_mbmi)) {  // single
        if (!CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[0]))
          pred_context = 3;
        else
          pred_context = 4 * (edge_mbmi->ref_frame[0] == LAST_FRAME);
      } else {  // comp
        pred_context = 1 +
                       2 * (edge_mbmi->ref_frame[0] == LAST_FRAME ||
                            edge_mbmi->ref_frame[1] == LAST_FRAME);
      }
    } else {  // inter/inter
      const int above_has_second = has_second_ref(above_mbmi);
      const int left_has_second = has_second_ref(left_mbmi);
      const MV_REFERENCE_FRAME above0 = above_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME above1 = above_mbmi->ref_frame[1];
      const MV_REFERENCE_FRAME left0 = left_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME left1 = left_mbmi->ref_frame[1];

      if (above_has_second && left_has_second) {  // comp/comp
        if (above0 == left0 && above1 == left1)
          pred_context = 3 * (above0 == LAST_FRAME || above1 == LAST_FRAME ||
                              left0 == LAST_FRAME || left1 == LAST_FRAME);
        else
          pred_context = 2;
      } else if (above_has_second || left_has_second) {  // single/comp
        const MV_REFERENCE_FRAME rfs = !above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf1 = above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf2 = above_has_second ? above1 : left1;

        if (rfs == LAST_FRAME)
          pred_context = 3 + (crf1 == LAST_FRAME || crf2 == LAST_FRAME);
        else if (rfs == LAST2_FRAME)
          pred_context = (crf1 == LAST_FRAME || crf2 == LAST_FRAME);
        else
          pred_context = 1 + 2 * (crf1 == LAST_FRAME || crf2 == LAST_FRAME);
      } else {  // single/single
        if (!CHECK_LAST_OR_LAST2(above0) && !CHECK_LAST_OR_LAST2(left0)) {
          pred_context = 2 + (above0 == left0);
        } else if (!CHECK_LAST_OR_LAST2(above0) ||
                   !CHECK_LAST_OR_LAST2(left0)) {
          const MV_REFERENCE_FRAME edge0 =
              !CHECK_LAST_OR_LAST2(above0) ? left0 : above0;
          pred_context = 4 * (edge0 == LAST_FRAME);
        } else {
          pred_context = 2 * (above0 == LAST_FRAME) + 2 * (left0 == LAST_FRAME);
        }
      }
    }
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = has_above ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi) ||
        (!CHECK_LAST_OR_LAST2(edge_mbmi->ref_frame[0]) &&
         !has_second_ref(edge_mbmi)))
      pred_context = 2;
    else if (!has_second_ref(edge_mbmi))  // single
      pred_context = 4 * (edge_mbmi->ref_frame[0] == LAST_FRAME);
    else  // comp
      pred_context = 3 * (edge_mbmi->ref_frame[0] == LAST_FRAME ||
                          edge_mbmi->ref_frame[1] == LAST_FRAME);
  } else {  // no edges available (2)
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// For the bit to signal whether the single reference is GOLDEN_FRAME or
// LAST3_FRAME, knowing that it shall be either of these 2 choices.
//
// NOTE(zoeliu): The probability of ref_frame[0] is GOLDEN_FRAME, conditioning
// on it is either GOLDEN_FRAME/LAST3_FRAME.
int av1_get_pred_context_single_ref_p5(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries correpsonding to real macroblocks.
  // The prediction flags in these dummy entries are initialised to 0.
  if (has_above && has_left) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter or inter/intra
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;
      if (!has_second_ref(edge_mbmi)) {  // single
        if (!CHECK_GOLDEN_OR_LAST3(edge_mbmi->ref_frame[0]))
          pred_context = 3;
        else
          pred_context = 4 * (edge_mbmi->ref_frame[0] == LAST3_FRAME);
      } else {  // comp
        pred_context = 1 +
                       2 * (edge_mbmi->ref_frame[0] == LAST3_FRAME ||
                            edge_mbmi->ref_frame[1] == LAST3_FRAME);
      }
    } else {  // inter/inter
      const int above_has_second = has_second_ref(above_mbmi);
      const int left_has_second = has_second_ref(left_mbmi);
      const MV_REFERENCE_FRAME above0 = above_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME above1 = above_mbmi->ref_frame[1];
      const MV_REFERENCE_FRAME left0 = left_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME left1 = left_mbmi->ref_frame[1];

      if (above_has_second && left_has_second) {  // comp/comp
        if (above0 == left0 && above1 == left1)
          pred_context = 3 * (above0 == LAST3_FRAME || above1 == LAST3_FRAME ||
                              left0 == LAST3_FRAME || left1 == LAST3_FRAME);
        else
          pred_context = 2;
      } else if (above_has_second || left_has_second) {  // single/comp
        const MV_REFERENCE_FRAME rfs = !above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf1 = above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf2 = above_has_second ? above1 : left1;

        if (rfs == LAST3_FRAME)
          pred_context = 3 + (crf1 == LAST3_FRAME || crf2 == LAST3_FRAME);
        else if (rfs == GOLDEN_FRAME)
          pred_context = (crf1 == LAST3_FRAME || crf2 == LAST3_FRAME);
        else
          pred_context = 1 + 2 * (crf1 == LAST3_FRAME || crf2 == LAST3_FRAME);
      } else {  // single/single
        if (!CHECK_GOLDEN_OR_LAST3(above0) && !CHECK_GOLDEN_OR_LAST3(left0)) {
          pred_context = 2 + (above0 == left0);
        } else if (!CHECK_GOLDEN_OR_LAST3(above0) ||
                   !CHECK_GOLDEN_OR_LAST3(left0)) {
          const MV_REFERENCE_FRAME edge0 =
              !CHECK_GOLDEN_OR_LAST3(above0) ? left0 : above0;
          pred_context = 4 * (edge0 == LAST3_FRAME);
        } else {
          pred_context =
              2 * (above0 == LAST3_FRAME) + 2 * (left0 == LAST3_FRAME);
        }
      }
    }
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = has_above ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi) ||
        (!CHECK_GOLDEN_OR_LAST3(edge_mbmi->ref_frame[0]) &&
         !has_second_ref(edge_mbmi)))
      pred_context = 2;
    else if (!has_second_ref(edge_mbmi))  // single
      pred_context = 4 * (edge_mbmi->ref_frame[0] == LAST3_FRAME);
    else  // comp
      pred_context = 3 * (edge_mbmi->ref_frame[0] == LAST3_FRAME ||
                          edge_mbmi->ref_frame[1] == LAST3_FRAME);
  } else {  // no edges available (2)
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// For the bit to signal whether the single reference is ALTREF2_FRAME or
// BWDREF_FRAME, knowing that it shall be either of these 2 choices.
int av1_get_pred_context_single_ref_p6(const MACROBLOCKD *xd) {
  return av1_get_pred_context_brf_or_arf2(xd);
}

#else  // !CONFIG_EXT_REFS

int av1_get_pred_context_single_ref_p1(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;
  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries corresponding to real macroblocks.
  // The prediction flags in these dummy entries are initialized to 0.
  if (has_above && has_left) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter or inter/intra
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;
      if (!has_second_ref(edge_mbmi))
        pred_context = 4 * (edge_mbmi->ref_frame[0] == LAST_FRAME);
      else
        pred_context = 1 + (edge_mbmi->ref_frame[0] == LAST_FRAME ||
                            edge_mbmi->ref_frame[1] == LAST_FRAME);
    } else {  // inter/inter
      const int above_has_second = has_second_ref(above_mbmi);
      const int left_has_second = has_second_ref(left_mbmi);
      const MV_REFERENCE_FRAME above0 = above_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME above1 = above_mbmi->ref_frame[1];
      const MV_REFERENCE_FRAME left0 = left_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME left1 = left_mbmi->ref_frame[1];

      if (above_has_second && left_has_second) {
        pred_context = 1 + (above0 == LAST_FRAME || above1 == LAST_FRAME ||
                            left0 == LAST_FRAME || left1 == LAST_FRAME);
      } else if (above_has_second || left_has_second) {
        const MV_REFERENCE_FRAME rfs = !above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf1 = above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf2 = above_has_second ? above1 : left1;

        if (rfs == LAST_FRAME)
          pred_context = 3 + (crf1 == LAST_FRAME || crf2 == LAST_FRAME);
        else
          pred_context = (crf1 == LAST_FRAME || crf2 == LAST_FRAME);
      } else {
        pred_context = 2 * (above0 == LAST_FRAME) + 2 * (left0 == LAST_FRAME);
      }
    }
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = has_above ? above_mbmi : left_mbmi;
    if (!is_inter_block(edge_mbmi)) {  // intra
      pred_context = 2;
    } else {  // inter
      if (!has_second_ref(edge_mbmi))
        pred_context = 4 * (edge_mbmi->ref_frame[0] == LAST_FRAME);
      else
        pred_context = 1 + (edge_mbmi->ref_frame[0] == LAST_FRAME ||
                            edge_mbmi->ref_frame[1] == LAST_FRAME);
    }
  } else {  // no edges available
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

int av1_get_pred_context_single_ref_p2(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries corresponding to real macroblocks.
  // The prediction flags in these dummy entries are initialized to 0.
  if (has_above && has_left) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter or inter/intra
      const MB_MODE_INFO *edge_mbmi = above_intra ? left_mbmi : above_mbmi;
      if (!has_second_ref(edge_mbmi)) {
        if (edge_mbmi->ref_frame[0] == LAST_FRAME)
          pred_context = 3;
        else
          pred_context = 4 * (edge_mbmi->ref_frame[0] == GOLDEN_FRAME);
      } else {
        pred_context = 1 +
                       2 * (edge_mbmi->ref_frame[0] == GOLDEN_FRAME ||
                            edge_mbmi->ref_frame[1] == GOLDEN_FRAME);
      }
    } else {  // inter/inter
      const int above_has_second = has_second_ref(above_mbmi);
      const int left_has_second = has_second_ref(left_mbmi);
      const MV_REFERENCE_FRAME above0 = above_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME above1 = above_mbmi->ref_frame[1];
      const MV_REFERENCE_FRAME left0 = left_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME left1 = left_mbmi->ref_frame[1];

      if (above_has_second && left_has_second) {
        if (above0 == left0 && above1 == left1)
          pred_context =
              3 * (above0 == GOLDEN_FRAME || above1 == GOLDEN_FRAME ||
                   left0 == GOLDEN_FRAME || left1 == GOLDEN_FRAME);
        else
          pred_context = 2;
      } else if (above_has_second || left_has_second) {
        const MV_REFERENCE_FRAME rfs = !above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf1 = above_has_second ? above0 : left0;
        const MV_REFERENCE_FRAME crf2 = above_has_second ? above1 : left1;

        if (rfs == GOLDEN_FRAME)
          pred_context = 3 + (crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME);
        else if (rfs != GOLDEN_FRAME && rfs != LAST_FRAME)
          pred_context = crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME;
        else
          pred_context = 1 + 2 * (crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME);
      } else {
        if (above0 == LAST_FRAME && left0 == LAST_FRAME) {
          pred_context = 3;
        } else if (above0 == LAST_FRAME || left0 == LAST_FRAME) {
          const MV_REFERENCE_FRAME edge0 =
              (above0 == LAST_FRAME) ? left0 : above0;
          pred_context = 4 * (edge0 == GOLDEN_FRAME);
        } else {
          pred_context =
              2 * (above0 == GOLDEN_FRAME) + 2 * (left0 == GOLDEN_FRAME);
        }
      }
    }
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = has_above ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi) ||
        (edge_mbmi->ref_frame[0] == LAST_FRAME && !has_second_ref(edge_mbmi)))
      pred_context = 2;
    else if (!has_second_ref(edge_mbmi))
      pred_context = 4 * (edge_mbmi->ref_frame[0] == GOLDEN_FRAME);
    else
      pred_context = 3 * (edge_mbmi->ref_frame[0] == GOLDEN_FRAME ||
                          edge_mbmi->ref_frame[1] == GOLDEN_FRAME);
  } else {  // no edges available (2)
    pred_context = 2;
  }
  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

#endif  // CONFIG_EXT_REFS
