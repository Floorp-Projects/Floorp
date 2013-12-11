/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_RECONINTER_H_
#define VP9_COMMON_VP9_RECONINTER_H_

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_onyxc_int.h"

struct subpix_fn_table;
void vp9_build_inter_predictors_sby(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize);

void vp9_build_inter_predictors_sbuv(MACROBLOCKD *xd, int mi_row, int mi_col,
                                     BLOCK_SIZE bsize);

void vp9_build_inter_predictors_sb(MACROBLOCKD *xd, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize);

void vp9_setup_interp_filters(MACROBLOCKD *xd,
                              INTERPOLATION_TYPE filter,
                              VP9_COMMON *cm);

void vp9_build_inter_predictor(const uint8_t *src, int src_stride,
                               uint8_t *dst, int dst_stride,
                               const MV *mv_q3,
                               const struct scale_factors *scale,
                               int w, int h, int do_avg,
                               const struct subpix_fn_table *subpix,
                               enum mv_precision precision);

static int scaled_buffer_offset(int x_offset, int y_offset, int stride,
                                const struct scale_factors *scale) {
  const int x = scale ? scale->sfc->scale_value_x(x_offset, scale->sfc) :
      x_offset;
  const int y = scale ? scale->sfc->scale_value_y(y_offset, scale->sfc) :
      y_offset;
  return y * stride + x;
}

static void setup_pred_plane(struct buf_2d *dst,
                             uint8_t *src, int stride,
                             int mi_row, int mi_col,
                             const struct scale_factors *scale,
                             int subsampling_x, int subsampling_y) {
  const int x = (MI_SIZE * mi_col) >> subsampling_x;
  const int y = (MI_SIZE * mi_row) >> subsampling_y;
  dst->buf = src + scaled_buffer_offset(x, y, stride, scale);
  dst->stride = stride;
}

// TODO(jkoleszar): audit all uses of this that don't set mb_row, mb_col
static void setup_dst_planes(MACROBLOCKD *xd,
                             const YV12_BUFFER_CONFIG *src,
                             int mi_row, int mi_col) {
  uint8_t *buffers[4] = {src->y_buffer, src->u_buffer, src->v_buffer,
                         src->alpha_buffer};
  int strides[4] = {src->y_stride, src->uv_stride, src->uv_stride,
                    src->alpha_stride};
  int i;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    struct macroblockd_plane *pd = &xd->plane[i];
    setup_pred_plane(&pd->dst, buffers[i], strides[i], mi_row, mi_col, NULL,
                     pd->subsampling_x, pd->subsampling_y);
  }
}

static void setup_pre_planes(MACROBLOCKD *xd, int i,
                             const YV12_BUFFER_CONFIG *src,
                             int mi_row, int mi_col,
                             const struct scale_factors *sf) {
  if (src) {
    int j;
    uint8_t* buffers[4] = {src->y_buffer, src->u_buffer, src->v_buffer,
                           src->alpha_buffer};
    int strides[4] = {src->y_stride, src->uv_stride, src->uv_stride,
                      src->alpha_stride};

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      struct macroblockd_plane *pd = &xd->plane[j];
      setup_pred_plane(&pd->pre[i], buffers[j], strides[j],
                     mi_row, mi_col, sf, pd->subsampling_x, pd->subsampling_y);
    }
  }
}

static void set_scale_factors(MACROBLOCKD *xd, int ref0, int ref1,
                              struct scale_factors sf[MAX_REF_FRAMES]) {
  xd->scale_factor[0] = sf[ref0 >= 0 ? ref0 : 0];
  xd->scale_factor[1] = sf[ref1 >= 0 ? ref1 : 0];
}

void vp9_setup_scale_factors(VP9_COMMON *cm, int i);

#endif  // VP9_COMMON_VP9_RECONINTER_H_
