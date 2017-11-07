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
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
#include "av1/common/warped_motion.h"
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
#include "aom/aom_integer.h"

#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#define WARP_WM_NEIGHBORS_WITH_OBMC 0
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION

#if CONFIG_MOTION_VAR && CONFIG_GLOBAL_MOTION
#define WARP_GM_NEIGHBORS_WITH_OBMC 0
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION

#ifdef __cplusplus
extern "C" {
#endif

static INLINE int has_scale(int xs, int ys) {
  return xs != SCALE_SUBPEL_SHIFTS || ys != SCALE_SUBPEL_SHIFTS;
}

static INLINE void inter_predictor(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride, int subpel_x,
                                   int subpel_y, const struct scale_factors *sf,
                                   int w, int h, ConvolveParams *conv_params,
                                   InterpFilters interp_filters, int xs,
                                   int ys) {
  assert(conv_params->do_average == 0 || conv_params->do_average == 1);
  assert(sf);
  if (has_scale(xs, ys)) {
    // TODO(afergs, debargha): Use a different scale convolve function
    // that uses higher precision for subpel_x, subpel_y, xs, ys
    if (conv_params->round == CONVOLVE_OPT_NO_ROUND) {
#if CONFIG_CONVOLVE_ROUND
      av1_convolve_2d_facade(src, src_stride, dst, dst_stride, w, h,
                             interp_filters, subpel_x, xs, subpel_y, ys, 1,
                             conv_params);
      conv_params->do_post_rounding = 1;
#else
      assert(0);
#endif  // CONFIG_CONVOLVE_ROUND
    } else {
      assert(conv_params->round == CONVOLVE_OPT_ROUND);
      av1_convolve_scale(src, src_stride, dst, dst_stride, w, h, interp_filters,
                         subpel_x, xs, subpel_y, ys, conv_params);
    }
  } else {
    subpel_x >>= SCALE_EXTRA_BITS;
    subpel_y >>= SCALE_EXTRA_BITS;
    xs >>= SCALE_EXTRA_BITS;
    ys >>= SCALE_EXTRA_BITS;
    assert(subpel_x < SUBPEL_SHIFTS);
    assert(subpel_y < SUBPEL_SHIFTS);
    assert(xs <= SUBPEL_SHIFTS);
    assert(ys <= SUBPEL_SHIFTS);
    if (conv_params->round == CONVOLVE_OPT_NO_ROUND) {
#if CONFIG_CONVOLVE_ROUND
      av1_convolve_2d_facade(src, src_stride, dst, dst_stride, w, h,
                             interp_filters, subpel_x, xs, subpel_y, ys, 0,
                             conv_params);
      conv_params->do_post_rounding = 1;
#else
      assert(0);
#endif  // CONFIG_CONVOLVE_ROUND
    } else {
      assert(conv_params->round == CONVOLVE_OPT_ROUND);

      InterpFilterParams filter_params_x, filter_params_y;
      av1_get_convolve_filter_params(interp_filters, 0, &filter_params_x,
                                     &filter_params_y);

      if (w <= 2 || h <= 2) {
        av1_convolve_c(src, src_stride, dst, dst_stride, w, h, interp_filters,
                       subpel_x, xs, subpel_y, ys, conv_params);
      } else if (filter_params_x.taps == SUBPEL_TAPS &&
                 filter_params_y.taps == SUBPEL_TAPS) {
        const int16_t *kernel_x =
            av1_get_interp_filter_subpel_kernel(filter_params_x, subpel_x);
        const int16_t *kernel_y =
            av1_get_interp_filter_subpel_kernel(filter_params_y, subpel_y);
        sf->predict[subpel_x != 0][subpel_y != 0][conv_params->do_average](
            src, src_stride, dst, dst_stride, kernel_x, xs, kernel_y, ys, w, h);
      } else {
        av1_convolve(src, src_stride, dst, dst_stride, w, h, interp_filters,
                     subpel_x, xs, subpel_y, ys, conv_params);
      }
    }
  }
}

#if CONFIG_HIGHBITDEPTH
static INLINE void highbd_inter_predictor(const uint8_t *src, int src_stride,
                                          uint8_t *dst, int dst_stride,
                                          int subpel_x, int subpel_y,
                                          const struct scale_factors *sf, int w,
                                          int h, ConvolveParams *conv_params,
                                          InterpFilters interp_filters, int xs,
                                          int ys, int bd) {
  const int avg = conv_params->do_average;
  assert(avg == 0 || avg == 1);

  if (has_scale(xs, ys)) {
    if (conv_params->round == CONVOLVE_OPT_NO_ROUND) {
#if CONFIG_CONVOLVE_ROUND
      av1_highbd_convolve_2d_facade(src, src_stride, dst, dst_stride, w, h,
                                    interp_filters, subpel_x, xs, subpel_y, ys,
                                    1, conv_params, bd);
      conv_params->do_post_rounding = 1;
#else
      assert(0);
#endif  // CONFIG_CONVOLVE_ROUND
    } else {
      av1_highbd_convolve_scale(src, src_stride, dst, dst_stride, w, h,
                                interp_filters, subpel_x, xs, subpel_y, ys, avg,
                                bd);
    }
  } else {
    subpel_x >>= SCALE_EXTRA_BITS;
    subpel_y >>= SCALE_EXTRA_BITS;
    xs >>= SCALE_EXTRA_BITS;
    ys >>= SCALE_EXTRA_BITS;
    assert(subpel_x < SUBPEL_SHIFTS);
    assert(subpel_y < SUBPEL_SHIFTS);
    assert(xs <= SUBPEL_SHIFTS);
    assert(ys <= SUBPEL_SHIFTS);
    if (conv_params->round == CONVOLVE_OPT_NO_ROUND) {
#if CONFIG_CONVOLVE_ROUND
      av1_highbd_convolve_2d_facade(src, src_stride, dst, dst_stride, w, h,
                                    interp_filters, subpel_x, xs, subpel_y, ys,
                                    0, conv_params, bd);
      conv_params->do_post_rounding = 1;
#else
      assert(0);
#endif  // CONFIG_CONVOLVE_ROUND
    } else {
      InterpFilterParams filter_params_x, filter_params_y;
      av1_get_convolve_filter_params(interp_filters, 0, &filter_params_x,
                                     &filter_params_y);

      if (filter_params_x.taps == SUBPEL_TAPS &&
          filter_params_y.taps == SUBPEL_TAPS && w > 2 && h > 2) {
        const int16_t *kernel_x =
            av1_get_interp_filter_subpel_kernel(filter_params_x, subpel_x);
        const int16_t *kernel_y =
            av1_get_interp_filter_subpel_kernel(filter_params_y, subpel_y);
        sf->highbd_predict[subpel_x != 0][subpel_y != 0][avg](
            src, src_stride, dst, dst_stride, kernel_x, xs, kernel_y, ys, w, h,
            bd);
      } else {
        av1_highbd_convolve(src, src_stride, dst, dst_stride, w, h,
                            interp_filters, subpel_x, xs, subpel_y, ys, avg,
                            bd);
      }
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH

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
  int smoother;
  wedge_masks_type *masks;
} wedge_params_type;

extern const wedge_params_type wedge_params_lookup[BLOCK_SIZES_ALL];

static INLINE int is_interinter_compound_used(COMPOUND_TYPE type,
                                              BLOCK_SIZE sb_type) {
  (void)sb_type;
  switch (type) {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
    case COMPOUND_AVERAGE: return sb_type >= BLOCK_4X4;
#else   // CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
    case COMPOUND_AVERAGE: return 1;
#endif  // CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
#if CONFIG_WEDGE
    case COMPOUND_WEDGE: return wedge_params_lookup[sb_type].bits > 0;
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
    case COMPOUND_SEG:
      return AOMMIN(block_size_wide[sb_type], block_size_high[sb_type]) >= 8;
#endif  // CONFIG_COMPOUND_SEGMENT
    default: assert(0); return 0;
  }
}

static INLINE int is_any_masked_compound_used(BLOCK_SIZE sb_type) {
  COMPOUND_TYPE comp_type;
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  if (sb_type < BLOCK_4X4) return 0;
#endif  // CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  for (comp_type = 0; comp_type < COMPOUND_TYPES; comp_type++) {
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
  (void)sb_type;
  return wedge_params_lookup[sb_type].bits > 0;
}

static INLINE int get_interintra_wedge_bits(BLOCK_SIZE sb_type) {
  return wedge_params_lookup[sb_type].bits;
}

#if CONFIG_COMPOUND_SEGMENT
void build_compound_seg_mask(uint8_t *mask, SEG_MASK_TYPE mask_type,
                             const uint8_t *src0, int src0_stride,
                             const uint8_t *src1, int src1_stride,
                             BLOCK_SIZE sb_type, int h, int w);
#if CONFIG_HIGHBITDEPTH
void build_compound_seg_mask_highbd(uint8_t *mask, SEG_MASK_TYPE mask_type,
                                    const uint8_t *src0, int src0_stride,
                                    const uint8_t *src1, int src1_stride,
                                    BLOCK_SIZE sb_type, int h, int w, int bd);
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_COMPOUND_SEGMENT

void av1_make_masked_inter_predictor(
    const uint8_t *pre, int pre_stride, uint8_t *dst, int dst_stride,
    const int subpel_x, const int subpel_y, const struct scale_factors *sf,
    int w, int h, ConvolveParams *conv_params, InterpFilters interp_filters,
    int xs, int ys,
#if CONFIG_SUPERTX
    int wedge_offset_x, int wedge_offset_y,
#endif  // CONFIG_SUPERTX
    int plane,
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
    const WarpTypesAllowed *warp_types, int p_col, int p_row, int ref,
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
    MACROBLOCKD *xd);

static INLINE int round_mv_comp_q4(int value) {
  return (value < 0 ? value - 2 : value + 2) / 4;
}

static MV mi_mv_pred_q4(const MODE_INFO *mi, int idx) {
  MV res = {
    round_mv_comp_q4(
        mi->bmi[0].as_mv[idx].as_mv.row + mi->bmi[1].as_mv[idx].as_mv.row +
        mi->bmi[2].as_mv[idx].as_mv.row + mi->bmi[3].as_mv[idx].as_mv.row),
    round_mv_comp_q4(
        mi->bmi[0].as_mv[idx].as_mv.col + mi->bmi[1].as_mv[idx].as_mv.col +
        mi->bmi[2].as_mv[idx].as_mv.col + mi->bmi[3].as_mv[idx].as_mv.col)
  };
  return res;
}

static INLINE int round_mv_comp_q2(int value) {
  return (value < 0 ? value - 1 : value + 1) / 2;
}

static MV mi_mv_pred_q2(const MODE_INFO *mi, int idx, int block0, int block1) {
  MV res = { round_mv_comp_q2(mi->bmi[block0].as_mv[idx].as_mv.row +
                              mi->bmi[block1].as_mv[idx].as_mv.row),
             round_mv_comp_q2(mi->bmi[block0].as_mv[idx].as_mv.col +
                              mi->bmi[block1].as_mv[idx].as_mv.col) };
  return res;
}

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
  MV clamped_mv = { src_mv->row * (1 << (1 - ss_y)),
                    src_mv->col * (1 << (1 - ss_x)) };
  assert(ss_x <= 1);
  assert(ss_y <= 1);

  clamp_mv(&clamped_mv, xd->mb_to_left_edge * (1 << (1 - ss_x)) - spel_left,
           xd->mb_to_right_edge * (1 << (1 - ss_x)) + spel_right,
           xd->mb_to_top_edge * (1 << (1 - ss_y)) - spel_top,
           xd->mb_to_bottom_edge * (1 << (1 - ss_y)) + spel_bottom);

  return clamped_mv;
}

static INLINE MV average_split_mvs(const struct macroblockd_plane *pd,
                                   const MODE_INFO *mi, int ref, int block) {
  const int ss_idx = ((pd->subsampling_x > 0) << 1) | (pd->subsampling_y > 0);
  MV res = { 0, 0 };
  switch (ss_idx) {
    case 0: res = mi->bmi[block].as_mv[ref].as_mv; break;
    case 1: res = mi_mv_pred_q2(mi, ref, block, block + 2); break;
    case 2: res = mi_mv_pred_q2(mi, ref, block, block + 1); break;
    case 3: res = mi_mv_pred_q4(mi, ref); break;
    default: assert(ss_idx <= 3 && ss_idx >= 0);
  }
  return res;
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

#if CONFIG_SUPERTX
void av1_build_inter_predictor_sb_sub8x8_extend(const AV1_COMMON *cm,
                                                MACROBLOCKD *xd, int mi_row_ori,
                                                int mi_col_ori, int mi_row,
                                                int mi_col, int plane,
                                                BLOCK_SIZE bsize, int block);

void av1_build_inter_predictor_sb_extend(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         int mi_row_ori, int mi_col_ori,
                                         int mi_row, int mi_col, int plane,
                                         BLOCK_SIZE bsize);
struct macroblockd_plane;
void av1_build_masked_inter_predictor_complex(
    MACROBLOCKD *xd, uint8_t *dst, int dst_stride, const uint8_t *pre,
    int pre_stride, int mi_row, int mi_col, int mi_row_ori, int mi_col_ori,
    BLOCK_SIZE bsize, BLOCK_SIZE top_bsize, PARTITION_TYPE partition,
    int plane);
#endif  // CONFIG_SUPERTX

void av1_build_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, const MV *src_mv,
                               const struct scale_factors *sf, int w, int h,
                               ConvolveParams *conv_params,
                               InterpFilters interp_filters,
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
                               const WarpTypesAllowed *warp_types, int p_col,
                               int p_row, int plane, int ref,
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
                               enum mv_precision precision, int x, int y,
                               const MACROBLOCKD *xd);

#if CONFIG_HIGHBITDEPTH
void av1_highbd_build_inter_predictor(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
    const MV *mv_q3, const struct scale_factors *sf, int w, int h, int do_avg,
    InterpFilters interp_filters,
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
    const WarpTypesAllowed *warp_types, int p_col, int p_row,
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
    int plane, enum mv_precision precision, int x, int y,
    const MACROBLOCKD *xd);
#endif

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
#if CONFIG_CHROMA_SUB8X8
  // Offset the buffer pointer
  if (subsampling_y && (mi_row & 0x01) && (mi_size_high[bsize] == 1))
    mi_row -= 1;
  if (subsampling_x && (mi_col & 0x01) && (mi_size_wide[bsize] == 1))
    mi_col -= 1;
#else
  (void)bsize;
#endif

  const int x = (MI_SIZE * mi_col) >> subsampling_x;
  const int y = (MI_SIZE * mi_row) >> subsampling_y;
  dst->buf = src + scaled_buffer_offset(x, y, stride, scale);
  dst->buf0 = src;
  dst->width = width;
  dst->height = height;
  dst->stride = stride;
}

void av1_setup_dst_planes(struct macroblockd_plane *planes, BLOCK_SIZE bsize,
                          const YV12_BUFFER_CONFIG *src, int mi_row,
                          int mi_col);

void av1_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf);

// Detect if the block have sub-pixel level motion vectors
// per component.
#define CHECK_SUBPEL 0
static INLINE int has_subpel_mv_component(const MODE_INFO *const mi,
                                          const MACROBLOCKD *const xd,
                                          int dir) {
#if CHECK_SUBPEL
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int plane;
  int ref = (dir >> 1);
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif

  if (bsize >= BLOCK_8X8 || unify_bsize) {
    if (dir & 0x01) {
      if (mbmi->mv[ref].as_mv.col & SUBPEL_MASK) return 1;
    } else {
      if (mbmi->mv[ref].as_mv.row & SUBPEL_MASK) return 1;
    }
  } else {
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      const PARTITION_TYPE bp = BLOCK_8X8 - bsize;
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const int have_vsplit = bp != PARTITION_HORZ;
      const int have_hsplit = bp != PARTITION_VERT;
      const int num_4x4_w = 2 >> ((!have_vsplit) | pd->subsampling_x);
      const int num_4x4_h = 2 >> ((!have_hsplit) | pd->subsampling_y);

      int x, y;
      for (y = 0; y < num_4x4_h; ++y) {
        for (x = 0; x < num_4x4_w; ++x) {
          const MV mv = average_split_mvs(pd, mi, ref, y * 2 + x);
          if (dir & 0x01) {
            if (mv.col & SUBPEL_MASK) return 1;
          } else {
            if (mv.row & SUBPEL_MASK) return 1;
          }
        }
      }
    }
  }

  return 0;
#else
  (void)mi;
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
  (void)xd;
#if CONFIG_WARPED_MOTION
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  if (mbmi->motion_mode == WARPED_CAUSAL) return 0;
#endif  // CONFIG_WARPED_MOTION
#if CONFIG_GLOBAL_MOTION
  if (is_nontrans_global_motion(xd)) return 0;
#endif  // CONFIG_GLOBAL_MOTION
  return 1;
}

static INLINE int av1_is_interp_search_needed(const MACROBLOCKD *const xd) {
  MODE_INFO *const mi = xd->mi[0];
  const int is_compound = has_second_ref(&mi->mbmi);
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

#if CONFIG_MOTION_VAR
const uint8_t *av1_get_obmc_mask(int length);
void av1_count_overlappable_neighbors(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                      int mi_row, int mi_col);
void av1_build_obmc_inter_prediction(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col,
                                     uint8_t *above[MAX_MB_PLANE],
                                     int above_stride[MAX_MB_PLANE],
                                     uint8_t *left[MAX_MB_PLANE],
                                     int left_stride[MAX_MB_PLANE]);
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
#if CONFIG_NCOBMC
void av1_build_ncobmc_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                          int mi_row, int mi_col);
#endif
#endif  // CONFIG_MOTION_VAR

#define MASK_MASTER_SIZE ((MAX_WEDGE_SIZE) << 1)
#define MASK_MASTER_STRIDE (MASK_MASTER_SIZE)

void av1_init_wedge_masks();

static INLINE const uint8_t *av1_get_contiguous_soft_mask(int wedge_index,
                                                          int wedge_sign,
                                                          BLOCK_SIZE sb_type) {
  return wedge_params_lookup[sb_type].masks[wedge_sign][wedge_index];
}

const uint8_t *av1_get_soft_mask(int wedge_index, int wedge_sign,
                                 BLOCK_SIZE sb_type, int wedge_offset_x,
                                 int wedge_offset_y);

const uint8_t *av1_get_compound_type_mask_inverse(
    const INTERINTER_COMPOUND_DATA *const comp_data,
#if CONFIG_COMPOUND_SEGMENT
    uint8_t *mask_buffer, int h, int w, int stride,
#endif
    BLOCK_SIZE sb_type);

const uint8_t *av1_get_compound_type_mask(
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type);
#if CONFIG_INTERINTRA
void av1_build_interintra_predictors(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     uint8_t *ypred, uint8_t *upred,
                                     uint8_t *vpred, int ystride, int ustride,
                                     int vstride, BUFFER_SET *ctx,
                                     BLOCK_SIZE bsize);
void av1_build_interintra_predictors_sby(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         uint8_t *ypred, int ystride,
                                         BUFFER_SET *ctx, BLOCK_SIZE bsize);
void av1_build_interintra_predictors_sbc(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         uint8_t *upred, int ustride,
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
#endif  // CONFIG_INTERINTRA
// Encoder only
void av1_build_inter_predictors_for_planes_single_buf(
    MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane_from, int plane_to, int mi_row,
    int mi_col, int ref, uint8_t *ext_dst[3], int ext_dst_stride[3]);
void av1_build_wedge_inter_predictor_from_buf(
    MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane_from, int plane_to,
#if CONFIG_SUPERTX
    int wedge_offset_x, int wedge_offset_y,
#endif  // CONFIG_SUPERTX
    uint8_t *ext_dst0[3], int ext_dst_stride0[3], uint8_t *ext_dst1[3],
    int ext_dst_stride1[3]);

#if CONFIG_NCOBMC_ADAPT_WEIGHT
#define ASSIGN_ALIGNED_PTRS(p, a, s) \
  p[0] = a;                          \
  p[1] = a + s;                      \
  p[2] = a + 2 * s;

#define ASSIGN_ALIGNED_PTRS_HBD(p, a, s, l) \
  p[0] = CONVERT_TO_BYTEPTR(a);             \
  p[1] = CONVERT_TO_BYTEPTR(a + s * l);     \
  p[2] = CONVERT_TO_BYTEPTR(a + 2 * s * l);

void alloc_ncobmc_pred_buffer(MACROBLOCKD *const xd);
void free_ncobmc_pred_buffer(MACROBLOCKD *const xd);
void set_sb_mi_boundaries(const AV1_COMMON *const cm, MACROBLOCKD *const xd,
                          const int mi_row, const int mi_col);

void reset_xd_boundary(MACROBLOCKD *xd, int mi_row, int bh, int mi_col, int bw,
                       int mi_rows, int mi_cols);

void get_pred_from_intrpl_buf(MACROBLOCKD *xd, int mi_row, int mi_col,
                              BLOCK_SIZE bsize, int plane);

void build_ncobmc_intrpl_pred(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                              int plane, int pxl_row, int pxl_col,
                              BLOCK_SIZE bsize, uint8_t *preds[][MAX_MB_PLANE],
                              int ps[MAX_MB_PLANE],  // pred buffer strides
                              int mode);

void av1_get_ext_blk_preds(const AV1_COMMON *cm, MACROBLOCKD *xd, int bsize,
                           int mi_row, int mi_col,
                           uint8_t *dst_buf[][MAX_MB_PLANE],
                           int dst_stride[MAX_MB_PLANE]);

void av1_get_ori_blk_pred(const AV1_COMMON *cm, MACROBLOCKD *xd, int bsize,
                          int mi_row, int mi_col,
                          uint8_t *dst_buf[MAX_MB_PLANE],
                          int dst_stride[MAX_MB_PLANE]);
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_RECONINTER_H_
