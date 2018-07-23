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

#ifndef AV1_COMMON_RECONINTER_H_
#define AV1_COMMON_RECONINTER_H_

#include "av1/common/filter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/convolve.h"
#include "av1/common/warped_motion.h"
#include "aom/aom_integer.h"

// Work out how many pixels off the edge of a reference frame we're allowed
// to go when forming an inter prediction.
// The outermost row/col of each referernce frame is extended by
// (AOM_BORDER_IN_PIXELS >> subsampling) pixels, but we need to keep
// at least AOM_INTERP_EXTEND pixels within that to account for filtering.
//
// We have to break this up into two macros to keep both clang-format and
// tools/lint-hunks.py happy.
#define AOM_LEFT_TOP_MARGIN_PX(subsampling) \
  ((AOM_BORDER_IN_PIXELS >> subsampling) - AOM_INTERP_EXTEND)
#define AOM_LEFT_TOP_MARGIN_SCALED(subsampling) \
  (AOM_LEFT_TOP_MARGIN_PX(subsampling) << SCALE_SUBPEL_BITS)

#ifdef __cplusplus
extern "C" {
#endif

// Set to (1 << 5) if the 32-ary codebooks are used for any bock size
#define MAX_WEDGE_TYPES (1 << 4)

#define MAX_WEDGE_SIZE_LOG2 5  // 32x32
#define MAX_WEDGE_SIZE (1 << MAX_WEDGE_SIZE_LOG2)
#define MAX_WEDGE_SQUARE (MAX_WEDGE_SIZE * MAX_WEDGE_SIZE)

#define WEDGE_WEIGHT_BITS 6

#define WEDGE_NONE -1

// Angles are with respect to horizontal anti-clockwise
typedef enum {
  WEDGE_HORIZONTAL = 0,
  WEDGE_VERTICAL = 1,
  WEDGE_OBLIQUE27 = 2,
  WEDGE_OBLIQUE63 = 3,
  WEDGE_OBLIQUE117 = 4,
  WEDGE_OBLIQUE153 = 5,
  WEDGE_DIRECTIONS
} WedgeDirectionType;

// 3-tuple: {direction, x_offset, y_offset}
typedef struct {
  WedgeDirectionType direction;
  int x_offset;
  int y_offset;
} wedge_code_type;

typedef uint8_t *wedge_masks_type[MAX_WEDGE_TYPES];

typedef struct {
  int bits;
  const wedge_code_type *codebook;
  uint8_t *signflip;
  wedge_masks_type *masks;
} wedge_params_type;

extern const wedge_params_type wedge_params_lookup[BLOCK_SIZES_ALL];

typedef struct SubpelParams {
  int xs;
  int ys;
  int subpel_x;
  int subpel_y;
} SubpelParams;

struct build_prediction_ctxt {
  const AV1_COMMON *cm;
  int mi_row;
  int mi_col;
  uint8_t **tmp_buf;
  int *tmp_width;
  int *tmp_height;
  int *tmp_stride;
  int mb_to_far_edge;
};

static INLINE int has_scale(int xs, int ys) {
  return xs != SCALE_SUBPEL_SHIFTS || ys != SCALE_SUBPEL_SHIFTS;
}

static INLINE void revert_scale_extra_bits(SubpelParams *sp) {
  sp->subpel_x >>= SCALE_EXTRA_BITS;
  sp->subpel_y >>= SCALE_EXTRA_BITS;
  sp->xs >>= SCALE_EXTRA_BITS;
  sp->ys >>= SCALE_EXTRA_BITS;
  assert(sp->subpel_x < SUBPEL_SHIFTS);
  assert(sp->subpel_y < SUBPEL_SHIFTS);
  assert(sp->xs <= SUBPEL_SHIFTS);
  assert(sp->ys <= SUBPEL_SHIFTS);
}

static INLINE void inter_predictor(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride,
                                   const SubpelParams *subpel_params,
                                   const struct scale_factors *sf, int w, int h,
                                   ConvolveParams *conv_params,
                                   InterpFilters interp_filters) {
  assert(conv_params->do_average == 0 || conv_params->do_average == 1);
  assert(sf);
  if (has_scale(subpel_params->xs, subpel_params->ys)) {
    av1_convolve_2d_facade(src, src_stride, dst, dst_stride, w, h,
                           interp_filters, subpel_params->subpel_x,
                           subpel_params->xs, subpel_params->subpel_y,
                           subpel_params->ys, 1, conv_params, sf);
  } else {
    SubpelParams sp = *subpel_params;
    revert_scale_extra_bits(&sp);
    av1_convolve_2d_facade(src, src_stride, dst, dst_stride, w, h,
                           interp_filters, sp.subpel_x, sp.xs, sp.subpel_y,
                           sp.ys, 0, conv_params, sf);
  }
}

static INLINE void highbd_inter_predictor(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
    const SubpelParams *subpel_params, const struct scale_factors *sf, int w,
    int h, ConvolveParams *conv_params, InterpFilters interp_filters, int bd) {
  assert(conv_params->do_average == 0 || conv_params->do_average == 1);
  assert(sf);
  if (has_scale(subpel_params->xs, subpel_params->ys)) {
    av1_highbd_convolve_2d_facade(src, src_stride, dst, dst_stride, w, h,
                                  interp_filters, subpel_params->subpel_x,
                                  subpel_params->xs, subpel_params->subpel_y,
                                  subpel_params->ys, 1, conv_params, sf, bd);
  } else {
    SubpelParams sp = *subpel_params;
    revert_scale_extra_bits(&sp);
    av1_highbd_convolve_2d_facade(src, src_stride, dst, dst_stride, w, h,
                                  interp_filters, sp.subpel_x, sp.xs,
                                  sp.subpel_y, sp.ys, 0, conv_params, sf, bd);
  }
}

void av1_modify_neighbor_predictor_for_obmc(MB_MODE_INFO *mbmi);
int av1_skip_u4x4_pred_in_obmc(BLOCK_SIZE bsize,
                               const struct macroblockd_plane *pd, int dir);

static INLINE int is_interinter_compound_used(COMPOUND_TYPE type,
                                              BLOCK_SIZE sb_type) {
  const int comp_allowed = is_comp_ref_allowed(sb_type);
  switch (type) {
    case COMPOUND_AVERAGE:
    case COMPOUND_DIFFWTD: return comp_allowed;
    case COMPOUND_WEDGE:
      return comp_allowed && wedge_params_lookup[sb_type].bits > 0;
    default: assert(0); return 0;
  }
}

static INLINE int is_any_masked_compound_used(BLOCK_SIZE sb_type) {
  COMPOUND_TYPE comp_type;
  int i;
  if (!is_comp_ref_allowed(sb_type)) return 0;
  for (i = 0; i < COMPOUND_TYPES; i++) {
    comp_type = (COMPOUND_TYPE)i;
    if (is_masked_compound_type(comp_type) &&
        is_interinter_compound_used(comp_type, sb_type))
      return 1;
  }
  return 0;
}

static INLINE int get_wedge_bits_lookup(BLOCK_SIZE sb_type) {
  return wedge_params_lookup[sb_type].bits;
}

static INLINE int get_interinter_wedge_bits(BLOCK_SIZE sb_type) {
  const int wbits = wedge_params_lookup[sb_type].bits;
  return (wbits > 0) ? wbits + 1 : 0;
}

static INLINE int is_interintra_wedge_used(BLOCK_SIZE sb_type) {
  return wedge_params_lookup[sb_type].bits > 0;
}

static INLINE int get_interintra_wedge_bits(BLOCK_SIZE sb_type) {
  return wedge_params_lookup[sb_type].bits;
}

void av1_make_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride, const SubpelParams *subpel_params,
                              const struct scale_factors *sf, int w, int h,
                              ConvolveParams *conv_params,
                              InterpFilters interp_filters,
                              const WarpTypesAllowed *warp_types, int p_col,
                              int p_row, int plane, int ref,
                              const MB_MODE_INFO *mi, int build_for_obmc,
                              const MACROBLOCKD *xd, int can_use_previous);

void av1_make_masked_inter_predictor(
    const uint8_t *pre, int pre_stride, uint8_t *dst, int dst_stride,
    const SubpelParams *subpel_params, const struct scale_factors *sf, int w,
    int h, ConvolveParams *conv_params, InterpFilters interp_filters, int plane,
    const WarpTypesAllowed *warp_types, int p_col, int p_row, int ref,
    MACROBLOCKD *xd, int can_use_previous);

// TODO(jkoleszar): yet another mv clamping function :-(
static INLINE MV clamp_mv_to_umv_border_sb(const MACROBLOCKD *xd,
                                           const MV *src_mv, int bw, int bh,
                                           int ss_x, int ss_y) {
  // If the MV points so far into the UMV border that no visible pixels
  // are used for reconstruction, the subpel part of the MV can be
  // discarded and the MV limited to 16 pixels with equivalent results.
  const int spel_left = (AOM_INTERP_EXTEND + bw) << SUBPEL_BITS;
  const int spel_right = spel_left - SUBPEL_SHIFTS;
  const int spel_top = (AOM_INTERP_EXTEND + bh) << SUBPEL_BITS;
  const int spel_bottom = spel_top - SUBPEL_SHIFTS;
  MV clamped_mv = { (int16_t)(src_mv->row * (1 << (1 - ss_y))),
                    (int16_t)(src_mv->col * (1 << (1 - ss_x))) };
  assert(ss_x <= 1);
  assert(ss_y <= 1);

  clamp_mv(&clamped_mv, xd->mb_to_left_edge * (1 << (1 - ss_x)) - spel_left,
           xd->mb_to_right_edge * (1 << (1 - ss_x)) + spel_right,
           xd->mb_to_top_edge * (1 << (1 - ss_y)) - spel_top,
           xd->mb_to_bottom_edge * (1 << (1 - ss_y)) + spel_bottom);

  return clamped_mv;
}

void av1_build_inter_predictors_sby(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int mi_row, int mi_col, BUFFER_SET *ctx,
                                    BLOCK_SIZE bsize);

void av1_build_inter_predictors_sbuv(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col, BUFFER_SET *ctx,
                                     BLOCK_SIZE bsize);

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

void av1_highbd_build_inter_predictor(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
    const MV *mv_q3, const struct scale_factors *sf, int w, int h, int do_avg,
    InterpFilters interp_filters, const WarpTypesAllowed *warp_types, int p_col,
    int p_row, int plane, enum mv_precision precision, int x, int y,
    const MACROBLOCKD *xd, int can_use_previous);

static INLINE int scaled_buffer_offset(int x_offset, int y_offset, int stride,
                                       const struct scale_factors *sf) {
  const int x =
      sf ? sf->scale_value_x(x_offset, sf) >> SCALE_EXTRA_BITS : x_offset;
  const int y =
      sf ? sf->scale_value_y(y_offset, sf) >> SCALE_EXTRA_BITS : y_offset;
  return y * stride + x;
}

static INLINE void setup_pred_plane(struct buf_2d *dst, BLOCK_SIZE bsize,
                                    uint8_t *src, int width, int height,
                                    int stride, int mi_row, int mi_col,
                                    const struct scale_factors *scale,
                                    int subsampling_x, int subsampling_y) {
  // Offset the buffer pointer
  if (subsampling_y && (mi_row & 0x01) && (mi_size_high[bsize] == 1))
    mi_row -= 1;
  if (subsampling_x && (mi_col & 0x01) && (mi_size_wide[bsize] == 1))
    mi_col -= 1;

  const int x = (MI_SIZE * mi_col) >> subsampling_x;
  const int y = (MI_SIZE * mi_row) >> subsampling_y;
  dst->buf = src + scaled_buffer_offset(x, y, stride, scale);
  dst->buf0 = src;
  dst->width = width;
  dst->height = height;
  dst->stride = stride;
}

void av1_setup_dst_planes(struct macroblockd_plane *planes, BLOCK_SIZE bsize,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const int plane_start, const int plane_end);

void av1_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf, const int num_planes);

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

static INLINE void set_default_interp_filters(
    MB_MODE_INFO *const mbmi, InterpFilter frame_interp_filter) {
  mbmi->interp_filters =
      av1_broadcast_interp_filter(av1_unswitchable_filter(frame_interp_filter));
}

static INLINE int av1_is_interp_needed(const MACROBLOCKD *const xd) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  if (mbmi->skip_mode) return 0;
  if (mbmi->motion_mode == WARPED_CAUSAL) return 0;
  if (is_nontrans_global_motion(xd, xd->mi[0])) return 0;
  return 1;
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
void av1_setup_build_prediction_by_above_pred(
    MACROBLOCKD *xd, int rel_mi_col, uint8_t above_mi_width,
    MB_MODE_INFO *above_mbmi, struct build_prediction_ctxt *ctxt,
    const int num_planes);
void av1_setup_build_prediction_by_left_pred(MACROBLOCKD *xd, int rel_mi_row,
                                             uint8_t left_mi_height,
                                             MB_MODE_INFO *left_mbmi,
                                             struct build_prediction_ctxt *ctxt,
                                             const int num_planes);
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
void av1_build_obmc_inter_prediction(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col,
                                     uint8_t *above[MAX_MB_PLANE],
                                     int above_stride[MAX_MB_PLANE],
                                     uint8_t *left[MAX_MB_PLANE],
                                     int left_stride[MAX_MB_PLANE]);

const uint8_t *av1_get_obmc_mask(int length);
void av1_count_overlappable_neighbors(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                      int mi_row, int mi_col);
void av1_build_obmc_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col);

#define MASK_MASTER_SIZE ((MAX_WEDGE_SIZE) << 1)
#define MASK_MASTER_STRIDE (MASK_MASTER_SIZE)

void av1_init_wedge_masks();

static INLINE const uint8_t *av1_get_contiguous_soft_mask(int wedge_index,
                                                          int wedge_sign,
                                                          BLOCK_SIZE sb_type) {
  return wedge_params_lookup[sb_type].masks[wedge_sign][wedge_index];
}

const uint8_t *av1_get_compound_type_mask(
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type);

void av1_build_interintra_predictors(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     uint8_t *ypred, uint8_t *upred,
                                     uint8_t *vpred, int ystride, int ustride,
                                     int vstride, BUFFER_SET *ctx,
                                     BLOCK_SIZE bsize);

// build interintra_predictors for one plane
void av1_build_interintra_predictors_sbp(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         uint8_t *pred, int stride,
                                         BUFFER_SET *ctx, int plane,
                                         BLOCK_SIZE bsize);

void av1_build_interintra_predictors_sbuv(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                          uint8_t *upred, uint8_t *vpred,
                                          int ustride, int vstride,
                                          BUFFER_SET *ctx, BLOCK_SIZE bsize);

void av1_build_intra_predictors_for_interintra(
    const AV1_COMMON *cm, MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane,
    BUFFER_SET *ctx, uint8_t *intra_pred, int intra_stride);

void av1_combine_interintra(MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane,
                            const uint8_t *inter_pred, int inter_stride,
                            const uint8_t *intra_pred, int intra_stride);

// Encoder only
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

void av1_jnt_comp_weight_assign(const AV1_COMMON *cm, const MB_MODE_INFO *mbmi,
                                int order_idx, int *fwd_offset, int *bck_offset,
                                int *use_jnt_comp_avg, int is_compound);
int av1_allow_warp(const MB_MODE_INFO *const mbmi,
                   const WarpTypesAllowed *const warp_types,
                   const WarpedMotionParams *const gm_params,
                   int build_for_obmc, int x_scale, int y_scale,
                   WarpedMotionParams *final_warp_params);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_RECONINTER_H_
