/*
  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_entropymv.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_seg_common.h"

#include "vp9/decoder/vp9_decodemv.h"
#include "vp9/decoder/vp9_decodeframe.h"
#include "vp9/decoder/vp9_reader.h"

static PREDICTION_MODE read_intra_mode(vp9_reader *r, const vp9_prob *p) {
  return (PREDICTION_MODE)vp9_read_tree(r, vp9_intra_mode_tree, p);
}

static PREDICTION_MODE read_intra_mode_y(VP9_COMMON *cm, vp9_reader *r,
                                            int size_group) {
  const PREDICTION_MODE y_mode =
      read_intra_mode(r, cm->fc.y_mode_prob[size_group]);
  if (!cm->frame_parallel_decoding_mode)
    ++cm->counts.y_mode[size_group][y_mode];
  return y_mode;
}

static PREDICTION_MODE read_intra_mode_uv(VP9_COMMON *cm, vp9_reader *r,
                                          PREDICTION_MODE y_mode) {
  const PREDICTION_MODE uv_mode = read_intra_mode(r,
                                         cm->fc.uv_mode_prob[y_mode]);
  if (!cm->frame_parallel_decoding_mode)
    ++cm->counts.uv_mode[y_mode][uv_mode];
  return uv_mode;
}

static PREDICTION_MODE read_inter_mode(VP9_COMMON *cm, vp9_reader *r, int ctx) {
  const int mode = vp9_read_tree(r, vp9_inter_mode_tree,
                                 cm->fc.inter_mode_probs[ctx]);
  if (!cm->frame_parallel_decoding_mode)
    ++cm->counts.inter_mode[ctx][mode];

  return NEARESTMV + mode;
}

static int read_segment_id(vp9_reader *r, const struct segmentation *seg) {
  return vp9_read_tree(r, vp9_segment_tree, seg->tree_probs);
}

static TX_SIZE read_selected_tx_size(VP9_COMMON *cm, MACROBLOCKD *xd,
                                     TX_SIZE max_tx_size, vp9_reader *r) {
  const int ctx = vp9_get_tx_size_context(xd);
  const vp9_prob *tx_probs = get_tx_probs(max_tx_size, ctx, &cm->fc.tx_probs);
  int tx_size = vp9_read(r, tx_probs[0]);
  if (tx_size != TX_4X4 && max_tx_size >= TX_16X16) {
    tx_size += vp9_read(r, tx_probs[1]);
    if (tx_size != TX_8X8 && max_tx_size >= TX_32X32)
      tx_size += vp9_read(r, tx_probs[2]);
  }

  if (!cm->frame_parallel_decoding_mode)
    ++get_tx_counts(max_tx_size, ctx, &cm->counts.tx)[tx_size];
  return (TX_SIZE)tx_size;
}

static TX_SIZE read_tx_size(VP9_COMMON *cm, MACROBLOCKD *xd, TX_MODE tx_mode,
                            BLOCK_SIZE bsize, int allow_select, vp9_reader *r) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
  if (allow_select && tx_mode == TX_MODE_SELECT && bsize >= BLOCK_8X8)
    return read_selected_tx_size(cm, xd, max_tx_size, r);
  else
    return MIN(max_tx_size, tx_mode_to_biggest_tx_size[tx_mode]);
}

static void set_segment_id(VP9_COMMON *cm, BLOCK_SIZE bsize,
                           int mi_row, int mi_col, int segment_id) {
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = num_8x8_blocks_wide_lookup[bsize];
  const int bh = num_8x8_blocks_high_lookup[bsize];
  const int xmis = MIN(cm->mi_cols - mi_col, bw);
  const int ymis = MIN(cm->mi_rows - mi_row, bh);
  int x, y;

  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);

  for (y = 0; y < ymis; y++)
    for (x = 0; x < xmis; x++)
      cm->last_frame_seg_map[mi_offset + y * cm->mi_cols + x] = segment_id;
}

static int read_intra_segment_id(VP9_COMMON *const cm, MACROBLOCKD *const xd,
                                 int mi_row, int mi_col,
                                 vp9_reader *r) {
  struct segmentation *const seg = &cm->seg;
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int segment_id;

  if (!seg->enabled)
    return 0;  // Default for disabled segmentation

  if (!seg->update_map)
    return 0;

  segment_id = read_segment_id(r, seg);
  set_segment_id(cm, bsize, mi_row, mi_col, segment_id);
  return segment_id;
}

static int read_inter_segment_id(VP9_COMMON *const cm, MACROBLOCKD *const xd,
                                 int mi_row, int mi_col, vp9_reader *r) {
  struct segmentation *const seg = &cm->seg;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int predicted_segment_id, segment_id;

  if (!seg->enabled)
    return 0;  // Default for disabled segmentation

  predicted_segment_id = vp9_get_segment_id(cm, cm->last_frame_seg_map,
                                            bsize, mi_row, mi_col);
  if (!seg->update_map)
    return predicted_segment_id;

  if (seg->temporal_update) {
    const vp9_prob pred_prob = vp9_get_pred_prob_seg_id(seg, xd);
    mbmi->seg_id_predicted = vp9_read(r, pred_prob);
    segment_id = mbmi->seg_id_predicted ? predicted_segment_id
                                        : read_segment_id(r, seg);
  } else {
    segment_id = read_segment_id(r, seg);
  }
  set_segment_id(cm, bsize, mi_row, mi_col, segment_id);
  return segment_id;
}

static int read_skip(VP9_COMMON *cm, const MACROBLOCKD *xd,
                     int segment_id, vp9_reader *r) {
  if (vp9_segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int ctx = vp9_get_skip_context(xd);
    const int skip = vp9_read(r, cm->fc.skip_probs[ctx]);
    if (!cm->frame_parallel_decoding_mode)
      ++cm->counts.skip[ctx][skip];
    return skip;
  }
}

static void read_intra_frame_mode_info(VP9_COMMON *const cm,
                                       MACROBLOCKD *const xd,
                                       int mi_row, int mi_col, vp9_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MODE_INFO *above_mi = xd->mi[-cm->mi_stride];
  const MODE_INFO *left_mi  = xd->left_available ? xd->mi[-1] : NULL;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int i;

  mbmi->segment_id = read_intra_segment_id(cm, xd, mi_row, mi_col, r);
  mbmi->skip = read_skip(cm, xd, mbmi->segment_id, r);
  mbmi->tx_size = read_tx_size(cm, xd, cm->tx_mode, bsize, 1, r);
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE;

  switch (bsize) {
    case BLOCK_4X4:
      for (i = 0; i < 4; ++i)
        mi->bmi[i].as_mode =
            read_intra_mode(r, get_y_mode_probs(mi, above_mi, left_mi, i));
      mbmi->mode = mi->bmi[3].as_mode;
      break;
    case BLOCK_4X8:
      mi->bmi[0].as_mode = mi->bmi[2].as_mode =
          read_intra_mode(r, get_y_mode_probs(mi, above_mi, left_mi, 0));
      mi->bmi[1].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode(r, get_y_mode_probs(mi, above_mi, left_mi, 1));
      break;
    case BLOCK_8X4:
      mi->bmi[0].as_mode = mi->bmi[1].as_mode =
          read_intra_mode(r, get_y_mode_probs(mi, above_mi, left_mi, 0));
      mi->bmi[2].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode(r, get_y_mode_probs(mi, above_mi, left_mi, 2));
      break;
    default:
      mbmi->mode = read_intra_mode(r,
                                   get_y_mode_probs(mi, above_mi, left_mi, 0));
  }

  mbmi->uv_mode = read_intra_mode(r, vp9_kf_uv_mode_prob[mbmi->mode]);
}

static int read_mv_component(vp9_reader *r,
                             const nmv_component *mvcomp, int usehp) {
  int mag, d, fr, hp;
  const int sign = vp9_read(r, mvcomp->sign);
  const int mv_class = vp9_read_tree(r, vp9_mv_class_tree, mvcomp->classes);
  const int class0 = mv_class == MV_CLASS_0;

  // Integer part
  if (class0) {
    d = vp9_read_tree(r, vp9_mv_class0_tree, mvcomp->class0);
  } else {
    int i;
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits

    d = 0;
    for (i = 0; i < n; ++i)
      d |= vp9_read(r, mvcomp->bits[i]) << i;
  }

  // Fractional part
  fr = vp9_read_tree(r, vp9_mv_fp_tree, class0 ? mvcomp->class0_fp[d]
                                               : mvcomp->fp);


  // High precision part (if hp is not used, the default value of the hp is 1)
  hp = usehp ? vp9_read(r, class0 ? mvcomp->class0_hp : mvcomp->hp)
             : 1;

  // Result
  mag = vp9_get_mv_mag(mv_class, (d << 3) | (fr << 1) | hp) + 1;
  return sign ? -mag : mag;
}

static INLINE void read_mv(vp9_reader *r, MV *mv, const MV *ref,
                           const nmv_context *ctx,
                           nmv_context_counts *counts, int allow_hp) {
  const MV_JOINT_TYPE joint_type =
      (MV_JOINT_TYPE)vp9_read_tree(r, vp9_mv_joint_tree, ctx->joints);
  const int use_hp = allow_hp && vp9_use_mv_hp(ref);
  MV diff = {0, 0};

  if (mv_joint_vertical(joint_type))
    diff.row = read_mv_component(r, &ctx->comps[0], use_hp);

  if (mv_joint_horizontal(joint_type))
    diff.col = read_mv_component(r, &ctx->comps[1], use_hp);

  vp9_inc_mv(&diff, counts);

  mv->row = ref->row + diff.row;
  mv->col = ref->col + diff.col;
}

static REFERENCE_MODE read_block_reference_mode(VP9_COMMON *cm,
                                                const MACROBLOCKD *xd,
                                                vp9_reader *r) {
  if (cm->reference_mode == REFERENCE_MODE_SELECT) {
    const int ctx = vp9_get_reference_mode_context(cm, xd);
    const REFERENCE_MODE mode =
        (REFERENCE_MODE)vp9_read(r, cm->fc.comp_inter_prob[ctx]);
    if (!cm->frame_parallel_decoding_mode)
      ++cm->counts.comp_inter[ctx][mode];
    return mode;  // SINGLE_REFERENCE or COMPOUND_REFERENCE
  } else {
    return cm->reference_mode;
  }
}

// Read the referncence frame
static void read_ref_frames(VP9_COMMON *const cm, MACROBLOCKD *const xd,
                            vp9_reader *r,
                            int segment_id, MV_REFERENCE_FRAME ref_frame[2]) {
  FRAME_CONTEXT *const fc = &cm->fc;
  FRAME_COUNTS *const counts = &cm->counts;

  if (vp9_segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    ref_frame[0] = (MV_REFERENCE_FRAME)vp9_get_segdata(&cm->seg, segment_id,
                                                       SEG_LVL_REF_FRAME);
    ref_frame[1] = NONE;
  } else {
    const REFERENCE_MODE mode = read_block_reference_mode(cm, xd, r);
    // FIXME(rbultje) I'm pretty sure this breaks segmentation ref frame coding
    if (mode == COMPOUND_REFERENCE) {
      const int idx = cm->ref_frame_sign_bias[cm->comp_fixed_ref];
      const int ctx = vp9_get_pred_context_comp_ref_p(cm, xd);
      const int bit = vp9_read(r, fc->comp_ref_prob[ctx]);
      if (!cm->frame_parallel_decoding_mode)
        ++counts->comp_ref[ctx][bit];
      ref_frame[idx] = cm->comp_fixed_ref;
      ref_frame[!idx] = cm->comp_var_ref[bit];
    } else if (mode == SINGLE_REFERENCE) {
      const int ctx0 = vp9_get_pred_context_single_ref_p1(xd);
      const int bit0 = vp9_read(r, fc->single_ref_prob[ctx0][0]);
      if (!cm->frame_parallel_decoding_mode)
        ++counts->single_ref[ctx0][0][bit0];
      if (bit0) {
        const int ctx1 = vp9_get_pred_context_single_ref_p2(xd);
        const int bit1 = vp9_read(r, fc->single_ref_prob[ctx1][1]);
        if (!cm->frame_parallel_decoding_mode)
          ++counts->single_ref[ctx1][1][bit1];
        ref_frame[0] = bit1 ? ALTREF_FRAME : GOLDEN_FRAME;
      } else {
        ref_frame[0] = LAST_FRAME;
      }

      ref_frame[1] = NONE;
    } else {
      assert(0 && "Invalid prediction mode.");
    }
  }
}


static INLINE INTERP_FILTER read_switchable_interp_filter(
    VP9_COMMON *const cm, MACROBLOCKD *const xd, vp9_reader *r) {
  const int ctx = vp9_get_pred_context_switchable_interp(xd);
  const INTERP_FILTER type =
      (INTERP_FILTER)vp9_read_tree(r, vp9_switchable_interp_tree,
                                   cm->fc.switchable_interp_prob[ctx]);
  if (!cm->frame_parallel_decoding_mode)
    ++cm->counts.switchable_interp[ctx][type];
  return type;
}

static void read_intra_block_mode_info(VP9_COMMON *const cm, MODE_INFO *mi,
                                       vp9_reader *r) {
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;
  int i;

  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE;

  switch (bsize) {
    case BLOCK_4X4:
      for (i = 0; i < 4; ++i)
        mi->bmi[i].as_mode = read_intra_mode_y(cm, r, 0);
      mbmi->mode = mi->bmi[3].as_mode;
      break;
    case BLOCK_4X8:
      mi->bmi[0].as_mode = mi->bmi[2].as_mode = read_intra_mode_y(cm, r, 0);
      mi->bmi[1].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode_y(cm, r, 0);
      break;
    case BLOCK_8X4:
      mi->bmi[0].as_mode = mi->bmi[1].as_mode = read_intra_mode_y(cm, r, 0);
      mi->bmi[2].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode_y(cm, r, 0);
      break;
    default:
      mbmi->mode = read_intra_mode_y(cm, r, size_group_lookup[bsize]);
  }

  mbmi->uv_mode = read_intra_mode_uv(cm, r, mbmi->mode);
}

static INLINE int is_mv_valid(const MV *mv) {
  return mv->row > MV_LOW && mv->row < MV_UPP &&
         mv->col > MV_LOW && mv->col < MV_UPP;
}

static INLINE int assign_mv(VP9_COMMON *cm, PREDICTION_MODE mode,
                            int_mv mv[2], int_mv ref_mv[2],
                            int_mv nearest_mv[2], int_mv near_mv[2],
                            int is_compound, int allow_hp, vp9_reader *r) {
  int i;
  int ret = 1;

  switch (mode) {
    case NEWMV: {
      nmv_context_counts *const mv_counts = cm->frame_parallel_decoding_mode ?
                                            NULL : &cm->counts.mv;
      for (i = 0; i < 1 + is_compound; ++i) {
        read_mv(r, &mv[i].as_mv, &ref_mv[i].as_mv, &cm->fc.nmvc, mv_counts,
                allow_hp);
        ret = ret && is_mv_valid(&mv[i].as_mv);
      }
      break;
    }
    case NEARESTMV: {
      mv[0].as_int = nearest_mv[0].as_int;
      if (is_compound)
        mv[1].as_int = nearest_mv[1].as_int;
      break;
    }
    case NEARMV: {
      mv[0].as_int = near_mv[0].as_int;
      if (is_compound)
        mv[1].as_int = near_mv[1].as_int;
      break;
    }
    case ZEROMV: {
      mv[0].as_int = 0;
      if (is_compound)
        mv[1].as_int = 0;
      break;
    }
    default: {
      return 0;
    }
  }
  return ret;
}

static int read_is_inter_block(VP9_COMMON *const cm, MACROBLOCKD *const xd,
                               int segment_id, vp9_reader *r) {
  if (vp9_segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    return vp9_get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME) !=
           INTRA_FRAME;
  } else {
    const int ctx = vp9_get_intra_inter_context(xd);
    const int is_inter = vp9_read(r, cm->fc.intra_inter_prob[ctx]);
    if (!cm->frame_parallel_decoding_mode)
      ++cm->counts.intra_inter[ctx][is_inter];
    return is_inter;
  }
}

static void read_inter_block_mode_info(VP9_COMMON *const cm,
                                       MACROBLOCKD *const xd,
                                       const TileInfo *const tile,
                                       MODE_INFO *const mi,
                                       int mi_row, int mi_col, vp9_reader *r) {
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;

  int_mv nearestmv[2], nearmv[2];
  int inter_mode_ctx, ref, is_compound;

  read_ref_frames(cm, xd, r, mbmi->segment_id, mbmi->ref_frame);
  is_compound = has_second_ref(mbmi);

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
    const int ref_idx = frame - LAST_FRAME;
    if (cm->frame_refs[ref_idx].sf.x_scale_fp == REF_INVALID_SCALE ||
        cm->frame_refs[ref_idx].sf.y_scale_fp == REF_INVALID_SCALE )
      vpx_internal_error(&cm->error, VPX_CODEC_UNSUP_BITSTREAM,
                         "Reference frame has invalid dimensions");
    vp9_find_mv_refs(cm, xd, tile, mi, frame, mbmi->ref_mvs[frame],
                     mi_row, mi_col);
  }

  inter_mode_ctx = mbmi->mode_context[mbmi->ref_frame[0]];

  if (vp9_segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    mbmi->mode = ZEROMV;
    if (bsize < BLOCK_8X8) {
        vpx_internal_error(&cm->error, VPX_CODEC_UNSUP_BITSTREAM,
                           "Invalid usage of segement feature on small blocks");
        return;
    }
  } else {
    if (bsize >= BLOCK_8X8)
      mbmi->mode = read_inter_mode(cm, r, inter_mode_ctx);
  }

  if (bsize < BLOCK_8X8 || mbmi->mode != ZEROMV) {
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      vp9_find_best_ref_mvs(xd, allow_hp, mbmi->ref_mvs[mbmi->ref_frame[ref]],
                            &nearestmv[ref], &nearmv[ref]);
    }
  }

  mbmi->interp_filter = (cm->interp_filter == SWITCHABLE)
                      ? read_switchable_interp_filter(cm, xd, r)
                      : cm->interp_filter;

  if (bsize < BLOCK_8X8) {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];  // 1 or 2
    const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];  // 1 or 2
    int idx, idy;
    PREDICTION_MODE b_mode;
    int_mv nearest_sub8x8[2], near_sub8x8[2];
    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        int_mv block[2];
        const int j = idy * 2 + idx;
        b_mode = read_inter_mode(cm, r, inter_mode_ctx);

        if (b_mode == NEARESTMV || b_mode == NEARMV)
          for (ref = 0; ref < 1 + is_compound; ++ref)
            vp9_append_sub8x8_mvs_for_idx(cm, xd, tile, j, ref, mi_row, mi_col,
                                          &nearest_sub8x8[ref],
                                          &near_sub8x8[ref]);

        if (!assign_mv(cm, b_mode, block, nearestmv,
                       nearest_sub8x8, near_sub8x8,
                       is_compound, allow_hp, r)) {
          xd->corrupted |= 1;
          break;
        };

        mi->bmi[j].as_mv[0].as_int = block[0].as_int;
        if (is_compound)
          mi->bmi[j].as_mv[1].as_int = block[1].as_int;

        if (num_4x4_h == 2)
          mi->bmi[j + 2] = mi->bmi[j];
        if (num_4x4_w == 2)
          mi->bmi[j + 1] = mi->bmi[j];
      }
    }

    mi->mbmi.mode = b_mode;

    mbmi->mv[0].as_int = mi->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = mi->bmi[3].as_mv[1].as_int;
  } else {
    xd->corrupted |= !assign_mv(cm, mbmi->mode, mbmi->mv, nearestmv,
                                nearestmv, nearmv, is_compound, allow_hp, r);
  }
}

static void read_inter_frame_mode_info(VP9_COMMON *const cm,
                                       MACROBLOCKD *const xd,
                                       const TileInfo *const tile,
                                       int mi_row, int mi_col, vp9_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  int inter_block;

  mbmi->mv[0].as_int = 0;
  mbmi->mv[1].as_int = 0;
  mbmi->segment_id = read_inter_segment_id(cm, xd, mi_row, mi_col, r);
  mbmi->skip = read_skip(cm, xd, mbmi->segment_id, r);
  inter_block = read_is_inter_block(cm, xd, mbmi->segment_id, r);
  mbmi->tx_size = read_tx_size(cm, xd, cm->tx_mode, mbmi->sb_type,
                               !mbmi->skip || !inter_block, r);

  if (inter_block)
    read_inter_block_mode_info(cm, xd, tile, mi, mi_row, mi_col, r);
  else
    read_intra_block_mode_info(cm, mi, r);
}

void vp9_read_mode_info(VP9_COMMON *cm, MACROBLOCKD *xd,
                        const TileInfo *const tile,
                        int mi_row, int mi_col, vp9_reader *r) {
  if (frame_is_intra_only(cm))
    read_intra_frame_mode_info(cm, xd, mi_row, mi_col, r);
  else
    read_inter_frame_mode_info(cm, xd, tile, mi_row, mi_col, r);
}
