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

#ifndef AOM_AV1_ENCODER_RECONINTER_ENC_H_
#define AOM_AV1_ENCODER_RECONINTER_ENC_H_

#include "aom/aom_integer.h"
#include "av1/common/filter.h"
#include "av1/common/blockd.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/convolve.h"
#include "av1/common/warped_motion.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_build_inter_predictors_sby(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int mi_row, int mi_col, BUFFER_SET *ctx,
                                    BLOCK_SIZE bsize);

void av1_build_inter_predictors_sbuv(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col, BUFFER_SET *ctx,
                                     BLOCK_SIZE bsize);

void av1_build_inter_predictors_sbp(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int mi_row, int mi_col, BUFFER_SET *ctx,
                                    BLOCK_SIZE bsize, int plane_idx);

void av1_build_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                   int mi_row, int mi_col, BUFFER_SET *ctx,
                                   BLOCK_SIZE bsize);

void av1_build_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, const MV *src_mv,
                               const struct scale_factors *sf, int w, int h,
                               ConvolveParams *conv_params,
                               InterpFilters interp_filters,
                               const WarpTypesAllowed *warp_types, int p_col,
                               int p_row, int plane, int ref,
                               enum mv_precision precision, int x, int y,
                               const MACROBLOCKD *xd, int can_use_previous);

// Detect if the block have sub-pixel level motion vectors
// per component.
#define CHECK_SUBPEL 0
static INLINE int has_subpel_mv_component(const MB_MODE_INFO *const mbmi,
                                          const MACROBLOCKD *const xd,
                                          int dir) {
#if CHECK_SUBPEL
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int plane;
  int ref = (dir >> 1);

  if (dir & 0x01) {
    if (mbmi->mv[ref].as_mv.col & SUBPEL_MASK) return 1;
  } else {
    if (mbmi->mv[ref].as_mv.row & SUBPEL_MASK) return 1;
  }

  return 0;
#else
  (void)mbmi;
  (void)xd;
  (void)dir;
  return 1;
#endif
}

static INLINE int av1_is_interp_search_needed(const MACROBLOCKD *const xd) {
  MB_MODE_INFO *const mi = xd->mi[0];
  const int is_compound = has_second_ref(mi);
  int ref;
  for (ref = 0; ref < 1 + is_compound; ++ref) {
    int row_col;
    for (row_col = 0; row_col < 2; ++row_col) {
      const int dir = (ref << 1) + row_col;
      if (has_subpel_mv_component(mi, xd, dir)) {
        return 1;
      }
    }
  }
  return 0;
}

void av1_build_prediction_by_above_preds(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         int mi_row, int mi_col,
                                         uint8_t *tmp_buf[MAX_MB_PLANE],
                                         int tmp_width[MAX_MB_PLANE],
                                         int tmp_height[MAX_MB_PLANE],
                                         int tmp_stride[MAX_MB_PLANE]);

void av1_build_prediction_by_left_preds(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col,
                                        uint8_t *tmp_buf[MAX_MB_PLANE],
                                        int tmp_width[MAX_MB_PLANE],
                                        int tmp_height[MAX_MB_PLANE],
                                        int tmp_stride[MAX_MB_PLANE]);

void av1_build_obmc_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col);

void av1_build_inter_predictors_for_planes_single_buf(
    MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane_from, int plane_to, int mi_row,
    int mi_col, int ref, uint8_t *ext_dst[3], int ext_dst_stride[3],
    int can_use_previous);

void av1_build_wedge_inter_predictor_from_buf(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                              int plane_from, int plane_to,
                                              uint8_t *ext_dst0[3],
                                              int ext_dst_stride0[3],
                                              uint8_t *ext_dst1[3],
                                              int ext_dst_stride1[3]);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_RECONINTER_ENC_H_
