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
#include <stdio.h>
#include <limits.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"

#include "av1/common/blockd.h"
#include "av1/common/mvref_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/obmc.h"
#include "av1/encoder/reconinter_enc.h"

static INLINE void calc_subpel_params(
    MACROBLOCKD *xd, const struct scale_factors *const sf, const MV mv,
    int plane, const int pre_x, const int pre_y, int x, int y,
    struct buf_2d *const pre_buf, uint8_t **pre, SubpelParams *subpel_params,
    int bw, int bh) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int is_scaled = av1_is_scaled(sf);
  if (is_scaled) {
    int ssx = pd->subsampling_x;
    int ssy = pd->subsampling_y;
    int orig_pos_y = (pre_y + y) << SUBPEL_BITS;
    orig_pos_y += mv.row * (1 << (1 - ssy));
    int orig_pos_x = (pre_x + x) << SUBPEL_BITS;
    orig_pos_x += mv.col * (1 << (1 - ssx));
    int pos_y = sf->scale_value_y(orig_pos_y, sf);
    int pos_x = sf->scale_value_x(orig_pos_x, sf);
    pos_x += SCALE_EXTRA_OFF;
    pos_y += SCALE_EXTRA_OFF;

    const int top = -AOM_LEFT_TOP_MARGIN_SCALED(ssy);
    const int left = -AOM_LEFT_TOP_MARGIN_SCALED(ssx);
    const int bottom = (pre_buf->height + AOM_INTERP_EXTEND)
                       << SCALE_SUBPEL_BITS;
    const int right = (pre_buf->width + AOM_INTERP_EXTEND) << SCALE_SUBPEL_BITS;
    pos_y = clamp(pos_y, top, bottom);
    pos_x = clamp(pos_x, left, right);

    *pre = pre_buf->buf0 + (pos_y >> SCALE_SUBPEL_BITS) * pre_buf->stride +
           (pos_x >> SCALE_SUBPEL_BITS);
    subpel_params->subpel_x = pos_x & SCALE_SUBPEL_MASK;
    subpel_params->subpel_y = pos_y & SCALE_SUBPEL_MASK;
    subpel_params->xs = sf->x_step_q4;
    subpel_params->ys = sf->y_step_q4;
  } else {
    const MV mv_q4 = clamp_mv_to_umv_border_sb(
        xd, &mv, bw, bh, pd->subsampling_x, pd->subsampling_y);
    subpel_params->xs = subpel_params->ys = SCALE_SUBPEL_SHIFTS;
    subpel_params->subpel_x = (mv_q4.col & SUBPEL_MASK) << SCALE_EXTRA_BITS;
    subpel_params->subpel_y = (mv_q4.row & SUBPEL_MASK) << SCALE_EXTRA_BITS;
    *pre = pre_buf->buf + (y + (mv_q4.row >> SUBPEL_BITS)) * pre_buf->stride +
           (x + (mv_q4.col >> SUBPEL_BITS));
  }
}

static INLINE void build_inter_predictors(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                          int plane, const MB_MODE_INFO *mi,
                                          int build_for_obmc, int bw, int bh,
                                          int mi_x, int mi_y) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int is_compound = has_second_ref(mi);
  int ref;
  const int is_intrabc = is_intrabc_block(mi);
  assert(IMPLIES(is_intrabc, !is_compound));
  int is_global[2] = { 0, 0 };
  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[ref]];
    is_global[ref] = is_global_mv_block(mi, wm->wmtype);
  }

  const BLOCK_SIZE bsize = mi->sb_type;
  const int ss_x = pd->subsampling_x;
  const int ss_y = pd->subsampling_y;
  int sub8x8_inter = (block_size_wide[bsize] < 8 && ss_x) ||
                     (block_size_high[bsize] < 8 && ss_y);

  if (is_intrabc) sub8x8_inter = 0;

  // For sub8x8 chroma blocks, we may be covering more than one luma block's
  // worth of pixels. Thus (mi_x, mi_y) may not be the correct coordinates for
  // the top-left corner of the prediction source - the correct top-left corner
  // is at (pre_x, pre_y).
  const int row_start =
      (block_size_high[bsize] == 4) && ss_y && !build_for_obmc ? -1 : 0;
  const int col_start =
      (block_size_wide[bsize] == 4) && ss_x && !build_for_obmc ? -1 : 0;
  const int pre_x = (mi_x + MI_SIZE * col_start) >> ss_x;
  const int pre_y = (mi_y + MI_SIZE * row_start) >> ss_y;

  sub8x8_inter = sub8x8_inter && !build_for_obmc;
  if (sub8x8_inter) {
    for (int row = row_start; row <= 0 && sub8x8_inter; ++row) {
      for (int col = col_start; col <= 0; ++col) {
        const MB_MODE_INFO *this_mbmi = xd->mi[row * xd->mi_stride + col];
        if (!is_inter_block(this_mbmi)) sub8x8_inter = 0;
        if (is_intrabc_block(this_mbmi)) sub8x8_inter = 0;
      }
    }
  }

  if (sub8x8_inter) {
    // block size
    const int b4_w = block_size_wide[bsize] >> ss_x;
    const int b4_h = block_size_high[bsize] >> ss_y;
    const BLOCK_SIZE plane_bsize = scale_chroma_bsize(bsize, ss_x, ss_y);
    const int b8_w = block_size_wide[plane_bsize] >> ss_x;
    const int b8_h = block_size_high[plane_bsize] >> ss_y;
    assert(!is_compound);

    const struct buf_2d orig_pred_buf[2] = { pd->pre[0], pd->pre[1] };

    int row = row_start;
    for (int y = 0; y < b8_h; y += b4_h) {
      int col = col_start;
      for (int x = 0; x < b8_w; x += b4_w) {
        MB_MODE_INFO *this_mbmi = xd->mi[row * xd->mi_stride + col];
        is_compound = has_second_ref(this_mbmi);
        int tmp_dst_stride = 8;
        assert(bw < 8 || bh < 8);
        ConvolveParams conv_params = get_conv_params_no_round(
            0, plane, xd->tmp_conv_dst, tmp_dst_stride, is_compound, xd->bd);
        conv_params.use_jnt_comp_avg = 0;
        struct buf_2d *const dst_buf = &pd->dst;
        uint8_t *dst = dst_buf->buf + dst_buf->stride * y + x;

        ref = 0;
        const RefBuffer *ref_buf =
            &cm->frame_refs[this_mbmi->ref_frame[ref] - LAST_FRAME];

        pd->pre[ref].buf0 =
            (plane == 1) ? ref_buf->buf->u_buffer : ref_buf->buf->v_buffer;
        pd->pre[ref].buf =
            pd->pre[ref].buf0 + scaled_buffer_offset(pre_x, pre_y,
                                                     ref_buf->buf->uv_stride,
                                                     &ref_buf->sf);
        pd->pre[ref].width = ref_buf->buf->uv_crop_width;
        pd->pre[ref].height = ref_buf->buf->uv_crop_height;
        pd->pre[ref].stride = ref_buf->buf->uv_stride;

        const struct scale_factors *const sf =
            is_intrabc ? &cm->sf_identity : &ref_buf->sf;
        struct buf_2d *const pre_buf = is_intrabc ? dst_buf : &pd->pre[ref];

        const MV mv = this_mbmi->mv[ref].as_mv;

        uint8_t *pre;
        SubpelParams subpel_params;
        WarpTypesAllowed warp_types;
        warp_types.global_warp_allowed = is_global[ref];
        warp_types.local_warp_allowed = this_mbmi->motion_mode == WARPED_CAUSAL;

        calc_subpel_params(xd, sf, mv, plane, pre_x, pre_y, x, y, pre_buf, &pre,
                           &subpel_params, bw, bh);
        conv_params.do_average = ref;
        if (is_masked_compound_type(mi->interinter_comp.type)) {
          // masked compound type has its own average mechanism
          conv_params.do_average = 0;
        }

        av1_make_inter_predictor(
            pre, pre_buf->stride, dst, dst_buf->stride, &subpel_params, sf,
            b4_w, b4_h, &conv_params, this_mbmi->interp_filters, &warp_types,
            (mi_x >> pd->subsampling_x) + x, (mi_y >> pd->subsampling_y) + y,
            plane, ref, mi, build_for_obmc, xd, cm->allow_warped_motion);

        ++col;
      }
      ++row;
    }

    for (ref = 0; ref < 2; ++ref) pd->pre[ref] = orig_pred_buf[ref];
    return;
  }

  {
    ConvolveParams conv_params = get_conv_params_no_round(
        0, plane, xd->tmp_conv_dst, MAX_SB_SIZE, is_compound, xd->bd);
    av1_jnt_comp_weight_assign(cm, mi, 0, &conv_params.fwd_offset,
                               &conv_params.bck_offset,
                               &conv_params.use_jnt_comp_avg, is_compound);

    struct buf_2d *const dst_buf = &pd->dst;
    uint8_t *const dst = dst_buf->buf;
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      const struct scale_factors *const sf =
          is_intrabc ? &cm->sf_identity : &xd->block_refs[ref]->sf;
      struct buf_2d *const pre_buf = is_intrabc ? dst_buf : &pd->pre[ref];
      const MV mv = mi->mv[ref].as_mv;

      uint8_t *pre;
      SubpelParams subpel_params;
      calc_subpel_params(xd, sf, mv, plane, pre_x, pre_y, 0, 0, pre_buf, &pre,
                         &subpel_params, bw, bh);

      WarpTypesAllowed warp_types;
      warp_types.global_warp_allowed = is_global[ref];
      warp_types.local_warp_allowed = mi->motion_mode == WARPED_CAUSAL;

      if (ref && is_masked_compound_type(mi->interinter_comp.type)) {
        // masked compound type has its own average mechanism
        conv_params.do_average = 0;
        av1_make_masked_inter_predictor(
            pre, pre_buf->stride, dst, dst_buf->stride, &subpel_params, sf, bw,
            bh, &conv_params, mi->interp_filters, plane, &warp_types,
            mi_x >> pd->subsampling_x, mi_y >> pd->subsampling_y, ref, xd,
            cm->allow_warped_motion);
      } else {
        conv_params.do_average = ref;
        av1_make_inter_predictor(
            pre, pre_buf->stride, dst, dst_buf->stride, &subpel_params, sf, bw,
            bh, &conv_params, mi->interp_filters, &warp_types,
            mi_x >> pd->subsampling_x, mi_y >> pd->subsampling_y, plane, ref,
            mi, build_for_obmc, xd, cm->allow_warped_motion);
      }
    }
  }
}

static void build_inter_predictors_for_planes(const AV1_COMMON *cm,
                                              MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                              int mi_row, int mi_col,
                                              int plane_from, int plane_to) {
  int plane;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const int bw = pd->width;
    const int bh = pd->height;

    if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                             pd->subsampling_y))
      continue;

    build_inter_predictors(cm, xd, plane, xd->mi[0], 0, bw, bh, mi_x, mi_y);
  }
}

void av1_build_inter_predictors_sby(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int mi_row, int mi_col, BUFFER_SET *ctx,
                                    BLOCK_SIZE bsize) {
  av1_build_inter_predictors_sbp(cm, xd, mi_row, mi_col, ctx, bsize, 0);
}

void av1_build_inter_predictors_sbuv(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col, BUFFER_SET *ctx,
                                     BLOCK_SIZE bsize) {
  for (int plane_idx = 1; plane_idx < MAX_MB_PLANE; plane_idx++) {
    av1_build_inter_predictors_sbp(cm, xd, mi_row, mi_col, ctx, bsize,
                                   plane_idx);
  }
}

void av1_build_inter_predictors_sbp(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int mi_row, int mi_col, BUFFER_SET *ctx,
                                    BLOCK_SIZE bsize, int plane_idx) {
  build_inter_predictors_for_planes(cm, xd, bsize, mi_row, mi_col, plane_idx,
                                    plane_idx);

  if (is_interintra_pred(xd->mi[0])) {
    BUFFER_SET default_ctx = { { NULL, NULL, NULL }, { 0, 0, 0 } };
    if (!ctx) {
      default_ctx.plane[plane_idx] = xd->plane[plane_idx].dst.buf;
      default_ctx.stride[plane_idx] = xd->plane[plane_idx].dst.stride;
      ctx = &default_ctx;
    }
    av1_build_interintra_predictors_sbp(cm, xd, xd->plane[plane_idx].dst.buf,
                                        xd->plane[plane_idx].dst.stride, ctx,
                                        plane_idx, bsize);
  }
}

void av1_build_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                   int mi_row, int mi_col, BUFFER_SET *ctx,
                                   BLOCK_SIZE bsize) {
  const int num_planes = av1_num_planes(cm);
  av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, ctx, bsize);
  if (num_planes > 1)
    av1_build_inter_predictors_sbuv(cm, xd, mi_row, mi_col, ctx, bsize);
}

// TODO(sarahparker):
// av1_build_inter_predictor should be combined with
// av1_make_inter_predictor
void av1_build_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, const MV *src_mv,
                               const struct scale_factors *sf, int w, int h,
                               ConvolveParams *conv_params,
                               InterpFilters interp_filters,
                               const WarpTypesAllowed *warp_types, int p_col,
                               int p_row, int plane, int ref,
                               enum mv_precision precision, int x, int y,
                               const MACROBLOCKD *xd, int can_use_previous) {
  const int is_q4 = precision == MV_PRECISION_Q4;
  const MV mv_q4 = { is_q4 ? src_mv->row : src_mv->row * 2,
                     is_q4 ? src_mv->col : src_mv->col * 2 };
  MV32 mv = av1_scale_mv(&mv_q4, x, y, sf);
  mv.col += SCALE_EXTRA_OFF;
  mv.row += SCALE_EXTRA_OFF;

  const SubpelParams subpel_params = { sf->x_step_q4, sf->y_step_q4,
                                       mv.col & SCALE_SUBPEL_MASK,
                                       mv.row & SCALE_SUBPEL_MASK };
  src += (mv.row >> SCALE_SUBPEL_BITS) * src_stride +
         (mv.col >> SCALE_SUBPEL_BITS);

  av1_make_inter_predictor(src, src_stride, dst, dst_stride, &subpel_params, sf,
                           w, h, conv_params, interp_filters, warp_types, p_col,
                           p_row, plane, ref, xd->mi[0], 0, xd,
                           can_use_previous);
}

static INLINE void build_prediction_by_above_pred(
    MACROBLOCKD *xd, int rel_mi_col, uint8_t above_mi_width,
    MB_MODE_INFO *above_mbmi, void *fun_ctxt, const int num_planes) {
  struct build_prediction_ctxt *ctxt = (struct build_prediction_ctxt *)fun_ctxt;
  const int above_mi_col = ctxt->mi_col + rel_mi_col;
  int mi_x, mi_y;
  MB_MODE_INFO backup_mbmi = *above_mbmi;

  av1_setup_build_prediction_by_above_pred(xd, rel_mi_col, above_mi_width,
                                           above_mbmi, ctxt, num_planes);
  mi_x = above_mi_col << MI_SIZE_LOG2;
  mi_y = ctxt->mi_row << MI_SIZE_LOG2;

  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;

  for (int j = 0; j < num_planes; ++j) {
    const struct macroblockd_plane *pd = &xd->plane[j];
    int bw = (above_mi_width * MI_SIZE) >> pd->subsampling_x;
    int bh = clamp(block_size_high[bsize] >> (pd->subsampling_y + 1), 4,
                   block_size_high[BLOCK_64X64] >> (pd->subsampling_y + 1));

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, 0)) continue;
    build_inter_predictors(ctxt->cm, xd, j, above_mbmi, 1, bw, bh, mi_x, mi_y);
  }
  *above_mbmi = backup_mbmi;
}

void av1_build_prediction_by_above_preds(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         int mi_row, int mi_col,
                                         uint8_t *tmp_buf[MAX_MB_PLANE],
                                         int tmp_width[MAX_MB_PLANE],
                                         int tmp_height[MAX_MB_PLANE],
                                         int tmp_stride[MAX_MB_PLANE]) {
  if (!xd->up_available) return;

  // Adjust mb_to_bottom_edge to have the correct value for the OBMC
  // prediction block. This is half the height of the original block,
  // except for 128-wide blocks, where we only use a height of 32.
  int this_height = xd->n4_h * MI_SIZE;
  int pred_height = AOMMIN(this_height / 2, 32);
  xd->mb_to_bottom_edge += (this_height - pred_height) * 8;

  struct build_prediction_ctxt ctxt = { cm,         mi_row,
                                        mi_col,     tmp_buf,
                                        tmp_width,  tmp_height,
                                        tmp_stride, xd->mb_to_right_edge };
  BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  foreach_overlappable_nb_above(cm, xd, mi_col,
                                max_neighbor_obmc[mi_size_wide_log2[bsize]],
                                build_prediction_by_above_pred, &ctxt);

  xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
  xd->mb_to_right_edge = ctxt.mb_to_far_edge;
  xd->mb_to_bottom_edge -= (this_height - pred_height) * 8;
}

static INLINE void build_prediction_by_left_pred(
    MACROBLOCKD *xd, int rel_mi_row, uint8_t left_mi_height,
    MB_MODE_INFO *left_mbmi, void *fun_ctxt, const int num_planes) {
  struct build_prediction_ctxt *ctxt = (struct build_prediction_ctxt *)fun_ctxt;
  const int left_mi_row = ctxt->mi_row + rel_mi_row;
  int mi_x, mi_y;
  MB_MODE_INFO backup_mbmi = *left_mbmi;

  av1_setup_build_prediction_by_left_pred(xd, rel_mi_row, left_mi_height,
                                          left_mbmi, ctxt, num_planes);
  mi_x = ctxt->mi_col << MI_SIZE_LOG2;
  mi_y = left_mi_row << MI_SIZE_LOG2;
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;

  for (int j = 0; j < num_planes; ++j) {
    const struct macroblockd_plane *pd = &xd->plane[j];
    int bw = clamp(block_size_wide[bsize] >> (pd->subsampling_x + 1), 4,
                   block_size_wide[BLOCK_64X64] >> (pd->subsampling_x + 1));
    int bh = (left_mi_height << MI_SIZE_LOG2) >> pd->subsampling_y;

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, 1)) continue;
    build_inter_predictors(ctxt->cm, xd, j, left_mbmi, 1, bw, bh, mi_x, mi_y);
  }
  *left_mbmi = backup_mbmi;
}

void av1_build_prediction_by_left_preds(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col,
                                        uint8_t *tmp_buf[MAX_MB_PLANE],
                                        int tmp_width[MAX_MB_PLANE],
                                        int tmp_height[MAX_MB_PLANE],
                                        int tmp_stride[MAX_MB_PLANE]) {
  if (!xd->left_available) return;

  // Adjust mb_to_right_edge to have the correct value for the OBMC
  // prediction block. This is half the width of the original block,
  // except for 128-wide blocks, where we only use a width of 32.
  int this_width = xd->n4_w * MI_SIZE;
  int pred_width = AOMMIN(this_width / 2, 32);
  xd->mb_to_right_edge += (this_width - pred_width) * 8;

  struct build_prediction_ctxt ctxt = { cm,         mi_row,
                                        mi_col,     tmp_buf,
                                        tmp_width,  tmp_height,
                                        tmp_stride, xd->mb_to_bottom_edge };
  BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  foreach_overlappable_nb_left(cm, xd, mi_row,
                               max_neighbor_obmc[mi_size_high_log2[bsize]],
                               build_prediction_by_left_pred, &ctxt);

  xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
  xd->mb_to_right_edge -= (this_width - pred_width) * 8;
  xd->mb_to_bottom_edge = ctxt.mb_to_far_edge;
}

void av1_build_obmc_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col) {
  const int num_planes = av1_num_planes(cm);
  uint8_t *dst_buf1[MAX_MB_PLANE], *dst_buf2[MAX_MB_PLANE];
  int dst_stride1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_stride2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    dst_buf1[0] = CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[0]);
    dst_buf1[1] =
        CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[0] + MAX_SB_SQUARE * len);
    dst_buf1[2] =
        CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[0] + MAX_SB_SQUARE * 2 * len);
    dst_buf2[0] = CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[1]);
    dst_buf2[1] =
        CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[1] + MAX_SB_SQUARE * len);
    dst_buf2[2] =
        CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[1] + MAX_SB_SQUARE * 2 * len);
  } else {
    dst_buf1[0] = xd->tmp_obmc_bufs[0];
    dst_buf1[1] = xd->tmp_obmc_bufs[0] + MAX_SB_SQUARE;
    dst_buf1[2] = xd->tmp_obmc_bufs[0] + MAX_SB_SQUARE * 2;
    dst_buf2[0] = xd->tmp_obmc_bufs[1];
    dst_buf2[1] = xd->tmp_obmc_bufs[1] + MAX_SB_SQUARE;
    dst_buf2[2] = xd->tmp_obmc_bufs[1] + MAX_SB_SQUARE * 2;
  }
  av1_build_prediction_by_above_preds(cm, xd, mi_row, mi_col, dst_buf1,
                                      dst_width1, dst_height1, dst_stride1);
  av1_build_prediction_by_left_preds(cm, xd, mi_row, mi_col, dst_buf2,
                                     dst_width2, dst_height2, dst_stride2);
  av1_setup_dst_planes(xd->plane, xd->mi[0]->sb_type, get_frame_new_buffer(cm),
                       mi_row, mi_col, 0, num_planes);
  av1_build_obmc_inter_prediction(cm, xd, mi_row, mi_col, dst_buf1, dst_stride1,
                                  dst_buf2, dst_stride2);
}

// Builds the inter-predictor for the single ref case
// for use in the encoder to search the wedges efficiently.
static void build_inter_predictors_single_buf(MACROBLOCKD *xd, int plane,
                                              int bw, int bh, int x, int y,
                                              int w, int h, int mi_x, int mi_y,
                                              int ref, uint8_t *const ext_dst,
                                              int ext_dst_stride,
                                              int can_use_previous) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const MB_MODE_INFO *mi = xd->mi[0];

  const struct scale_factors *const sf = &xd->block_refs[ref]->sf;
  struct buf_2d *const pre_buf = &pd->pre[ref];
  uint8_t *const dst = get_buf_by_bd(xd, ext_dst) + ext_dst_stride * y + x;
  const MV mv = mi->mv[ref].as_mv;

  ConvolveParams conv_params = get_conv_params(0, plane, xd->bd);
  WarpTypesAllowed warp_types;
  const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[ref]];
  warp_types.global_warp_allowed = is_global_mv_block(mi, wm->wmtype);
  warp_types.local_warp_allowed = mi->motion_mode == WARPED_CAUSAL;
  const int pre_x = (mi_x) >> pd->subsampling_x;
  const int pre_y = (mi_y) >> pd->subsampling_y;
  uint8_t *pre;
  SubpelParams subpel_params;
  calc_subpel_params(xd, sf, mv, plane, pre_x, pre_y, x, y, pre_buf, &pre,
                     &subpel_params, bw, bh);

  av1_make_inter_predictor(pre, pre_buf->stride, dst, ext_dst_stride,
                           &subpel_params, sf, w, h, &conv_params,
                           mi->interp_filters, &warp_types, pre_x + x,
                           pre_y + y, plane, ref, mi, 0, xd, can_use_previous);
}

void av1_build_inter_predictors_for_planes_single_buf(
    MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane_from, int plane_to, int mi_row,
    int mi_col, int ref, uint8_t *ext_dst[3], int ext_dst_stride[3],
    int can_use_previous) {
  int plane;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const BLOCK_SIZE plane_bsize = get_plane_block_size(
        bsize, xd->plane[plane].subsampling_x, xd->plane[plane].subsampling_y);
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];
    build_inter_predictors_single_buf(xd, plane, bw, bh, 0, 0, bw, bh, mi_x,
                                      mi_y, ref, ext_dst[plane],
                                      ext_dst_stride[plane], can_use_previous);
  }
}

static void build_masked_compound(
    uint8_t *dst, int dst_stride, const uint8_t *src0, int src0_stride,
    const uint8_t *src1, int src1_stride,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type, int h,
    int w) {
  // Derive subsampling from h and w passed in. May be refactored to
  // pass in subsampling factors directly.
  const int subh = (2 << mi_size_high_log2[sb_type]) == h;
  const int subw = (2 << mi_size_wide_log2[sb_type]) == w;
  const uint8_t *mask = av1_get_compound_type_mask(comp_data, sb_type);
  aom_blend_a64_mask(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                     mask, block_size_wide[sb_type], w, h, subw, subh);
}

static void build_masked_compound_highbd(
    uint8_t *dst_8, int dst_stride, const uint8_t *src0_8, int src0_stride,
    const uint8_t *src1_8, int src1_stride,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type, int h,
    int w, int bd) {
  // Derive subsampling from h and w passed in. May be refactored to
  // pass in subsampling factors directly.
  const int subh = (2 << mi_size_high_log2[sb_type]) == h;
  const int subw = (2 << mi_size_wide_log2[sb_type]) == w;
  const uint8_t *mask = av1_get_compound_type_mask(comp_data, sb_type);
  // const uint8_t *mask =
  //     av1_get_contiguous_soft_mask(wedge_index, wedge_sign, sb_type);
  aom_highbd_blend_a64_mask(dst_8, dst_stride, src0_8, src0_stride, src1_8,
                            src1_stride, mask, block_size_wide[sb_type], w, h,
                            subw, subh, bd);
}

static void build_wedge_inter_predictor_from_buf(
    MACROBLOCKD *xd, int plane, int x, int y, int w, int h, uint8_t *ext_dst0,
    int ext_dst_stride0, uint8_t *ext_dst1, int ext_dst_stride1) {
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_compound = has_second_ref(mbmi);
  MACROBLOCKD_PLANE *const pd = &xd->plane[plane];
  struct buf_2d *const dst_buf = &pd->dst;
  uint8_t *const dst = dst_buf->buf + dst_buf->stride * y + x;
  mbmi->interinter_comp.seg_mask = xd->seg_mask;
  const INTERINTER_COMPOUND_DATA *comp_data = &mbmi->interinter_comp;

  if (is_compound && is_masked_compound_type(comp_data->type)) {
    if (!plane && comp_data->type == COMPOUND_DIFFWTD) {
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
        av1_build_compound_diffwtd_mask_highbd(
            comp_data->seg_mask, comp_data->mask_type,
            CONVERT_TO_BYTEPTR(ext_dst0), ext_dst_stride0,
            CONVERT_TO_BYTEPTR(ext_dst1), ext_dst_stride1, h, w, xd->bd);
      else
        av1_build_compound_diffwtd_mask(
            comp_data->seg_mask, comp_data->mask_type, ext_dst0,
            ext_dst_stride0, ext_dst1, ext_dst_stride1, h, w);
    }

    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      build_masked_compound_highbd(
          dst, dst_buf->stride, CONVERT_TO_BYTEPTR(ext_dst0), ext_dst_stride0,
          CONVERT_TO_BYTEPTR(ext_dst1), ext_dst_stride1, comp_data,
          mbmi->sb_type, h, w, xd->bd);
    else
      build_masked_compound(dst, dst_buf->stride, ext_dst0, ext_dst_stride0,
                            ext_dst1, ext_dst_stride1, comp_data, mbmi->sb_type,
                            h, w);
  } else {
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      aom_highbd_convolve_copy(CONVERT_TO_BYTEPTR(ext_dst0), ext_dst_stride0,
                               dst, dst_buf->stride, NULL, 0, NULL, 0, w, h,
                               xd->bd);
    else
      aom_convolve_copy(ext_dst0, ext_dst_stride0, dst, dst_buf->stride, NULL,
                        0, NULL, 0, w, h);
  }
}

void av1_build_wedge_inter_predictor_from_buf(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                              int plane_from, int plane_to,
                                              uint8_t *ext_dst0[3],
                                              int ext_dst_stride0[3],
                                              uint8_t *ext_dst1[3],
                                              int ext_dst_stride1[3]) {
  int plane;
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const BLOCK_SIZE plane_bsize = get_plane_block_size(
        bsize, xd->plane[plane].subsampling_x, xd->plane[plane].subsampling_y);
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];
    build_wedge_inter_predictor_from_buf(
        xd, plane, 0, 0, bw, bh, ext_dst0[plane], ext_dst_stride0[plane],
        ext_dst1[plane], ext_dst_stride1[plane]);
  }
}
