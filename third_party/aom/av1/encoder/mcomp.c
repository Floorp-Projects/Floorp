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

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/common.h"
#include "av1/common/mvref_common.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/rdopt.h"

// #define NEW_DIAMOND_SEARCH

static INLINE const uint8_t *get_buf_from_mv(const struct buf_2d *buf,
                                             const MV *mv) {
  return &buf->buf[mv->row * buf->stride + mv->col];
}

void av1_set_mv_search_range(MvLimits *mv_limits, const MV *mv) {
  int col_min = (mv->col >> 3) - MAX_FULL_PEL_VAL + (mv->col & 7 ? 1 : 0);
  int row_min = (mv->row >> 3) - MAX_FULL_PEL_VAL + (mv->row & 7 ? 1 : 0);
  int col_max = (mv->col >> 3) + MAX_FULL_PEL_VAL;
  int row_max = (mv->row >> 3) + MAX_FULL_PEL_VAL;

  col_min = AOMMAX(col_min, (MV_LOW >> 3) + 1);
  row_min = AOMMAX(row_min, (MV_LOW >> 3) + 1);
  col_max = AOMMIN(col_max, (MV_UPP >> 3) - 1);
  row_max = AOMMIN(row_max, (MV_UPP >> 3) - 1);

  // Get intersection of UMV window and valid MV window to reduce # of checks
  // in diamond search.
  if (mv_limits->col_min < col_min) mv_limits->col_min = col_min;
  if (mv_limits->col_max > col_max) mv_limits->col_max = col_max;
  if (mv_limits->row_min < row_min) mv_limits->row_min = row_min;
  if (mv_limits->row_max > row_max) mv_limits->row_max = row_max;
}

static void set_subpel_mv_search_range(const MvLimits *mv_limits, int *col_min,
                                       int *col_max, int *row_min, int *row_max,
                                       const MV *ref_mv) {
  const int max_mv = MAX_FULL_PEL_VAL * 8;
  const int minc = AOMMAX(mv_limits->col_min * 8, ref_mv->col - max_mv);
  const int maxc = AOMMIN(mv_limits->col_max * 8, ref_mv->col + max_mv);
  const int minr = AOMMAX(mv_limits->row_min * 8, ref_mv->row - max_mv);
  const int maxr = AOMMIN(mv_limits->row_max * 8, ref_mv->row + max_mv);

  *col_min = AOMMAX(MV_LOW + 1, minc);
  *col_max = AOMMIN(MV_UPP - 1, maxc);
  *row_min = AOMMAX(MV_LOW + 1, minr);
  *row_max = AOMMIN(MV_UPP - 1, maxr);
}

int av1_init_search_range(int size) {
  int sr = 0;
  // Minimum search size no matter what the passed in value.
  size = AOMMAX(16, size);

  while ((size << sr) < MAX_FULL_PEL_VAL) sr++;

  sr = AOMMIN(sr, MAX_MVSEARCH_STEPS - 2);
  return sr;
}

static INLINE int mv_cost(const MV *mv, const int *joint_cost,
                          int *const comp_cost[2]) {
  return joint_cost[av1_get_mv_joint(mv)] + comp_cost[0][mv->row] +
         comp_cost[1][mv->col];
}

int av1_mv_bit_cost(const MV *mv, const MV *ref, const int *mvjcost,
                    int *mvcost[2], int weight) {
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  return ROUND_POWER_OF_TWO(mv_cost(&diff, mvjcost, mvcost) * weight, 7);
}

#define PIXEL_TRANSFORM_ERROR_SCALE 4
static int mv_err_cost(const MV *mv, const MV *ref, const int *mvjcost,
                       int *mvcost[2], int error_per_bit) {
  if (mvcost) {
    const MV diff = { mv->row - ref->row, mv->col - ref->col };
    return (int)ROUND_POWER_OF_TWO_64(
        (int64_t)mv_cost(&diff, mvjcost, mvcost) * error_per_bit,
        RDDIV_BITS + AV1_PROB_COST_SHIFT - RD_EPB_SHIFT +
            PIXEL_TRANSFORM_ERROR_SCALE);
  }
  return 0;
}

static int mvsad_err_cost(const MACROBLOCK *x, const MV *mv, const MV *ref,
                          int sad_per_bit) {
  const MV diff = { (mv->row - ref->row) * 8, (mv->col - ref->col) * 8 };
  return ROUND_POWER_OF_TWO(
      (unsigned)mv_cost(&diff, x->nmvjointcost, x->mvcost) * sad_per_bit,
      AV1_PROB_COST_SHIFT);
}

void av1_init_dsmotion_compensation(search_site_config *cfg, int stride) {
  int len, ss_count = 1;

  cfg->ss[0].mv.col = cfg->ss[0].mv.row = 0;
  cfg->ss[0].offset = 0;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Generate offsets for 4 search sites per step.
    const MV ss_mvs[] = { { -len, 0 }, { len, 0 }, { 0, -len }, { 0, len } };
    int i;
    for (i = 0; i < 4; ++i) {
      search_site *const ss = &cfg->ss[ss_count++];
      ss->mv = ss_mvs[i];
      ss->offset = ss->mv.row * stride + ss->mv.col;
    }
  }

  cfg->ss_count = ss_count;
  cfg->searches_per_step = 4;
}

void av1_init3smotion_compensation(search_site_config *cfg, int stride) {
  int len, ss_count = 1;

  cfg->ss[0].mv.col = cfg->ss[0].mv.row = 0;
  cfg->ss[0].offset = 0;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Generate offsets for 8 search sites per step.
    const MV ss_mvs[8] = { { -len, 0 },   { len, 0 },     { 0, -len },
                           { 0, len },    { -len, -len }, { -len, len },
                           { len, -len }, { len, len } };
    int i;
    for (i = 0; i < 8; ++i) {
      search_site *const ss = &cfg->ss[ss_count++];
      ss->mv = ss_mvs[i];
      ss->offset = ss->mv.row * stride + ss->mv.col;
    }
  }

  cfg->ss_count = ss_count;
  cfg->searches_per_step = 8;
}

/*
 * To avoid the penalty for crossing cache-line read, preload the reference
 * area in a small buffer, which is aligned to make sure there won't be crossing
 * cache-line read while reading from this buffer. This reduced the cpu
 * cycles spent on reading ref data in sub-pixel filter functions.
 * TODO: Currently, since sub-pixel search range here is -3 ~ 3, copy 22 rows x
 * 32 cols area that is enough for 16x16 macroblock. Later, for SPLITMV, we
 * could reduce the area.
 */

// convert motion vector component to offset for sv[a]f calc
static INLINE int sp(int x) { return x & 7; }

static INLINE const uint8_t *pre(const uint8_t *buf, int stride, int r, int c) {
  const int offset = (r >> 3) * stride + (c >> 3);
  return buf + offset;
}

/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER(v, r, c)                                                \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {                    \
    MV this_mv = { r, c };                                                   \
    v = mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost, error_per_bit);       \
    if (second_pred == NULL) {                                               \
      thismse = vfp->svf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r),     \
                         src_address, src_stride, &sse);                     \
    } else if (mask) {                                                       \
      thismse = vfp->msvf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r),    \
                          src_address, src_stride, second_pred, mask,        \
                          mask_stride, invert_mask, &sse);                   \
    } else {                                                                 \
      if (xd->jcp_param.use_jnt_comp_avg)                                    \
        thismse = vfp->jsvaf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), \
                             src_address, src_stride, &sse, second_pred,     \
                             &xd->jcp_param);                                \
      else                                                                   \
        thismse = vfp->svaf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r),  \
                            src_address, src_stride, &sse, second_pred);     \
    }                                                                        \
    v += thismse;                                                            \
    if (v < besterr) {                                                       \
      besterr = v;                                                           \
      br = r;                                                                \
      bc = c;                                                                \
      *distortion = thismse;                                                 \
      *sse1 = sse;                                                           \
    }                                                                        \
  } else {                                                                   \
    v = INT_MAX;                                                             \
  }

#define CHECK_BETTER0(v, r, c) CHECK_BETTER(v, r, c)

/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER1(v, r, c)                                             \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {                  \
    MV this_mv = { r, c };                                                 \
    thismse = upsampled_pref_error(                                        \
        xd, cm, mi_row, mi_col, &this_mv, vfp, src_address, src_stride,    \
        pre(y, y_stride, r, c), y_stride, sp(c), sp(r), second_pred, mask, \
        mask_stride, invert_mask, w, h, &sse);                             \
    v = mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost, error_per_bit);     \
    v += thismse;                                                          \
    if (v < besterr) {                                                     \
      besterr = v;                                                         \
      br = r;                                                              \
      bc = c;                                                              \
      *distortion = thismse;                                               \
      *sse1 = sse;                                                         \
    }                                                                      \
  } else {                                                                 \
    v = INT_MAX;                                                           \
  }

#define FIRST_LEVEL_CHECKS                                       \
  {                                                              \
    unsigned int left, right, up, down, diag;                    \
    CHECK_BETTER(left, tr, tc - hstep);                          \
    CHECK_BETTER(right, tr, tc + hstep);                         \
    CHECK_BETTER(up, tr - hstep, tc);                            \
    CHECK_BETTER(down, tr + hstep, tc);                          \
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);     \
    switch (whichdir) {                                          \
      case 0: CHECK_BETTER(diag, tr - hstep, tc - hstep); break; \
      case 1: CHECK_BETTER(diag, tr - hstep, tc + hstep); break; \
      case 2: CHECK_BETTER(diag, tr + hstep, tc - hstep); break; \
      case 3: CHECK_BETTER(diag, tr + hstep, tc + hstep); break; \
    }                                                            \
  }

#define SECOND_LEVEL_CHECKS                                       \
  {                                                               \
    int kr, kc;                                                   \
    unsigned int second;                                          \
    if (tr != br && tc != bc) {                                   \
      kr = br - tr;                                               \
      kc = bc - tc;                                               \
      CHECK_BETTER(second, tr + kr, tc + 2 * kc);                 \
      CHECK_BETTER(second, tr + 2 * kr, tc + kc);                 \
    } else if (tr == br && tc != bc) {                            \
      kc = bc - tc;                                               \
      CHECK_BETTER(second, tr + hstep, tc + 2 * kc);              \
      CHECK_BETTER(second, tr - hstep, tc + 2 * kc);              \
      switch (whichdir) {                                         \
        case 0:                                                   \
        case 1: CHECK_BETTER(second, tr + hstep, tc + kc); break; \
        case 2:                                                   \
        case 3: CHECK_BETTER(second, tr - hstep, tc + kc); break; \
      }                                                           \
    } else if (tr != br && tc == bc) {                            \
      kr = br - tr;                                               \
      CHECK_BETTER(second, tr + 2 * kr, tc + hstep);              \
      CHECK_BETTER(second, tr + 2 * kr, tc - hstep);              \
      switch (whichdir) {                                         \
        case 0:                                                   \
        case 2: CHECK_BETTER(second, tr + kr, tc + hstep); break; \
        case 1:                                                   \
        case 3: CHECK_BETTER(second, tr + kr, tc - hstep); break; \
      }                                                           \
    }                                                             \
  }

// TODO(yunqingwang): SECOND_LEVEL_CHECKS_BEST was a rewrote of
// SECOND_LEVEL_CHECKS, and SECOND_LEVEL_CHECKS should be rewritten
// later in the same way.
#define SECOND_LEVEL_CHECKS_BEST(k)                \
  {                                                \
    unsigned int second;                           \
    int br0 = br;                                  \
    int bc0 = bc;                                  \
    assert(tr == br || tc == bc);                  \
    if (tr == br && tc != bc) {                    \
      kc = bc - tc;                                \
    } else if (tr != br && tc == bc) {             \
      kr = br - tr;                                \
    }                                              \
    CHECK_BETTER##k(second, br0 + kr, bc0);        \
    CHECK_BETTER##k(second, br0, bc0 + kc);        \
    if (br0 != br || bc0 != bc) {                  \
      CHECK_BETTER##k(second, br0 + kr, bc0 + kc); \
    }                                              \
  }

#define SETUP_SUBPEL_SEARCH                                             \
  const uint8_t *const src_address = x->plane[0].src.buf;               \
  const int src_stride = x->plane[0].src.stride;                        \
  const MACROBLOCKD *xd = &x->e_mbd;                                    \
  unsigned int besterr = INT_MAX;                                       \
  unsigned int sse;                                                     \
  unsigned int whichdir;                                                \
  int thismse;                                                          \
  MV *bestmv = &x->best_mv.as_mv;                                       \
  const unsigned int halfiters = iters_per_step;                        \
  const unsigned int quarteriters = iters_per_step;                     \
  const unsigned int eighthiters = iters_per_step;                      \
  const int y_stride = xd->plane[0].pre[0].stride;                      \
  const int offset = bestmv->row * y_stride + bestmv->col;              \
  const uint8_t *const y = xd->plane[0].pre[0].buf;                     \
                                                                        \
  int br = bestmv->row * 8;                                             \
  int bc = bestmv->col * 8;                                             \
  int hstep = 4;                                                        \
  int minc, maxc, minr, maxr;                                           \
  int tr = br;                                                          \
  int tc = bc;                                                          \
                                                                        \
  set_subpel_mv_search_range(&x->mv_limits, &minc, &maxc, &minr, &maxr, \
                             ref_mv);                                   \
                                                                        \
  bestmv->row *= 8;                                                     \
  bestmv->col *= 8;

static unsigned int setup_center_error(
    const MACROBLOCKD *xd, const MV *bestmv, const MV *ref_mv,
    int error_per_bit, const aom_variance_fn_ptr_t *vfp,
    const uint8_t *const src, const int src_stride, const uint8_t *const y,
    int y_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, int invert_mask, int w, int h, int offset, int *mvjcost,
    int *mvcost[2], unsigned int *sse1, int *distortion) {
  unsigned int besterr;
  if (second_pred != NULL) {
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      DECLARE_ALIGNED(16, uint16_t, comp_pred16[MAX_SB_SQUARE]);
      if (mask) {
        aom_highbd_comp_mask_pred(comp_pred16, second_pred, w, h, y + offset,
                                  y_stride, mask, mask_stride, invert_mask);
      } else {
        if (xd->jcp_param.use_jnt_comp_avg)
          aom_highbd_jnt_comp_avg_pred(comp_pred16, second_pred, w, h,
                                       y + offset, y_stride, &xd->jcp_param);
        else
          aom_highbd_comp_avg_pred(comp_pred16, second_pred, w, h, y + offset,
                                   y_stride);
      }
      besterr =
          vfp->vf(CONVERT_TO_BYTEPTR(comp_pred16), w, src, src_stride, sse1);
    } else {
      DECLARE_ALIGNED(16, uint8_t, comp_pred[MAX_SB_SQUARE]);
      if (mask) {
        aom_comp_mask_pred(comp_pred, second_pred, w, h, y + offset, y_stride,
                           mask, mask_stride, invert_mask);
      } else {
        if (xd->jcp_param.use_jnt_comp_avg)
          aom_jnt_comp_avg_pred(comp_pred, second_pred, w, h, y + offset,
                                y_stride, &xd->jcp_param);
        else
          aom_comp_avg_pred(comp_pred, second_pred, w, h, y + offset, y_stride);
      }
      besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
    }
  } else {
    besterr = vfp->vf(y + offset, y_stride, src, src_stride, sse1);
  }
  *distortion = besterr;
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);
  return besterr;
}

static INLINE int divide_and_round(int n, int d) {
  return ((n < 0) ^ (d < 0)) ? ((n - d / 2) / d) : ((n + d / 2) / d);
}

static INLINE int is_cost_list_wellbehaved(int *cost_list) {
  return cost_list[0] < cost_list[1] && cost_list[0] < cost_list[2] &&
         cost_list[0] < cost_list[3] && cost_list[0] < cost_list[4];
}

// Returns surface minima estimate at given precision in 1/2^n bits.
// Assume a model for the cost surface: S = A(x - x0)^2 + B(y - y0)^2 + C
// For a given set of costs S0, S1, S2, S3, S4 at points
// (y, x) = (0, 0), (0, -1), (1, 0), (0, 1) and (-1, 0) respectively,
// the solution for the location of the minima (x0, y0) is given by:
// x0 = 1/2 (S1 - S3)/(S1 + S3 - 2*S0),
// y0 = 1/2 (S4 - S2)/(S4 + S2 - 2*S0).
// The code below is an integerized version of that.
static void get_cost_surf_min(int *cost_list, int *ir, int *ic, int bits) {
  *ic = divide_and_round((cost_list[1] - cost_list[3]) * (1 << (bits - 1)),
                         (cost_list[1] - 2 * cost_list[0] + cost_list[3]));
  *ir = divide_and_round((cost_list[4] - cost_list[2]) * (1 << (bits - 1)),
                         (cost_list[4] - 2 * cost_list[0] + cost_list[2]));
}

int av1_find_best_sub_pixel_tree_pruned_evenmore(
    MACROBLOCK *x, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *ref_mv, int allow_hp, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, int forced_stop, int iters_per_step,
    int *cost_list, int *mvjcost, int *mvcost[2], int *distortion,
    unsigned int *sse1, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, int invert_mask, int w, int h,
    int use_accurate_subpel_search) {
  SETUP_SUBPEL_SEARCH;
  besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp,
                               src_address, src_stride, y, y_stride,
                               second_pred, mask, mask_stride, invert_mask, w,
                               h, offset, mvjcost, mvcost, sse1, distortion);
  (void)halfiters;
  (void)quarteriters;
  (void)eighthiters;
  (void)whichdir;
  (void)allow_hp;
  (void)forced_stop;
  (void)hstep;
  (void)use_accurate_subpel_search;
  (void)cm;
  (void)mi_row;
  (void)mi_col;

  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX && is_cost_list_wellbehaved(cost_list)) {
    int ir, ic;
    unsigned int minpt;
    get_cost_surf_min(cost_list, &ir, &ic, 2);
    if (ir != 0 || ic != 0) {
      CHECK_BETTER(minpt, tr + 2 * ir, tc + 2 * ic);
    }
  } else {
    FIRST_LEVEL_CHECKS;
    if (halfiters > 1) {
      SECOND_LEVEL_CHECKS;
    }

    tr = br;
    tc = bc;

    // Each subsequent iteration checks at least one point in common with
    // the last iteration could be 2 ( if diag selected) 1/4 pel
    // Note forced_stop: 0 - full, 1 - qtr only, 2 - half only
    if (forced_stop != 2) {
      hstep >>= 1;
      FIRST_LEVEL_CHECKS;
      if (quarteriters > 1) {
        SECOND_LEVEL_CHECKS;
      }
    }
  }

  tr = br;
  tc = bc;

  if (allow_hp && forced_stop == 0) {
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (eighthiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

int av1_find_best_sub_pixel_tree_pruned_more(
    MACROBLOCK *x, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *ref_mv, int allow_hp, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, int forced_stop, int iters_per_step,
    int *cost_list, int *mvjcost, int *mvcost[2], int *distortion,
    unsigned int *sse1, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, int invert_mask, int w, int h,
    int use_accurate_subpel_search) {
  SETUP_SUBPEL_SEARCH;
  (void)use_accurate_subpel_search;
  (void)cm;
  (void)mi_row;
  (void)mi_col;

  besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp,
                               src_address, src_stride, y, y_stride,
                               second_pred, mask, mask_stride, invert_mask, w,
                               h, offset, mvjcost, mvcost, sse1, distortion);
  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX && is_cost_list_wellbehaved(cost_list)) {
    unsigned int minpt;
    int ir, ic;
    get_cost_surf_min(cost_list, &ir, &ic, 1);
    if (ir != 0 || ic != 0) {
      CHECK_BETTER(minpt, tr + ir * hstep, tc + ic * hstep);
    }
  } else {
    FIRST_LEVEL_CHECKS;
    if (halfiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel

  // Note forced_stop: 0 - full, 1 - qtr only, 2 - half only
  if (forced_stop != 2) {
    tr = br;
    tc = bc;
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (quarteriters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }

  if (allow_hp && forced_stop == 0) {
    tr = br;
    tc = bc;
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (eighthiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }
  // These lines insure static analysis doesn't warn that
  // tr and tc aren't used after the above point.
  (void)tr;
  (void)tc;

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

int av1_find_best_sub_pixel_tree_pruned(
    MACROBLOCK *x, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *ref_mv, int allow_hp, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, int forced_stop, int iters_per_step,
    int *cost_list, int *mvjcost, int *mvcost[2], int *distortion,
    unsigned int *sse1, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, int invert_mask, int w, int h,
    int use_accurate_subpel_search) {
  SETUP_SUBPEL_SEARCH;
  (void)use_accurate_subpel_search;
  (void)cm;
  (void)mi_row;
  (void)mi_col;

  besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp,
                               src_address, src_stride, y, y_stride,
                               second_pred, mask, mask_stride, invert_mask, w,
                               h, offset, mvjcost, mvcost, sse1, distortion);
  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX) {
    unsigned int left, right, up, down, diag;
    whichdir = (cost_list[1] < cost_list[3] ? 0 : 1) +
               (cost_list[2] < cost_list[4] ? 0 : 2);
    switch (whichdir) {
      case 0:
        CHECK_BETTER(left, tr, tc - hstep);
        CHECK_BETTER(down, tr + hstep, tc);
        CHECK_BETTER(diag, tr + hstep, tc - hstep);
        break;
      case 1:
        CHECK_BETTER(right, tr, tc + hstep);
        CHECK_BETTER(down, tr + hstep, tc);
        CHECK_BETTER(diag, tr + hstep, tc + hstep);
        break;
      case 2:
        CHECK_BETTER(left, tr, tc - hstep);
        CHECK_BETTER(up, tr - hstep, tc);
        CHECK_BETTER(diag, tr - hstep, tc - hstep);
        break;
      case 3:
        CHECK_BETTER(right, tr, tc + hstep);
        CHECK_BETTER(up, tr - hstep, tc);
        CHECK_BETTER(diag, tr - hstep, tc + hstep);
        break;
    }
  } else {
    FIRST_LEVEL_CHECKS;
    if (halfiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
  }

  tr = br;
  tc = bc;

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel

  // Note forced_stop: 0 - full, 1 - qtr only, 2 - half only
  if (forced_stop != 2) {
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (quarteriters > 1) {
      SECOND_LEVEL_CHECKS;
    }
    tr = br;
    tc = bc;
  }

  if (allow_hp && forced_stop == 0) {
    hstep >>= 1;
    FIRST_LEVEL_CHECKS;
    if (eighthiters > 1) {
      SECOND_LEVEL_CHECKS;
    }
    tr = br;
    tc = bc;
  }
  // These lines insure static analysis doesn't warn that
  // tr and tc aren't used after the above point.
  (void)tr;
  (void)tc;

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

/* clang-format off */
static const MV search_step_table[12] = {
  // left, right, up, down
  { 0, -4 }, { 0, 4 }, { -4, 0 }, { 4, 0 },
  { 0, -2 }, { 0, 2 }, { -2, 0 }, { 2, 0 },
  { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 }
};
/* clang-format on */

static int upsampled_pref_error(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                int mi_row, int mi_col, const MV *const mv,
                                const aom_variance_fn_ptr_t *vfp,
                                const uint8_t *const src, const int src_stride,
                                const uint8_t *const y, int y_stride,
                                int subpel_x_q3, int subpel_y_q3,
                                const uint8_t *second_pred, const uint8_t *mask,
                                int mask_stride, int invert_mask, int w, int h,
                                unsigned int *sse) {
  unsigned int besterr;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    DECLARE_ALIGNED(16, uint16_t, pred16[MAX_SB_SQUARE]);
    if (second_pred != NULL) {
      if (mask) {
        aom_highbd_comp_mask_upsampled_pred(
            xd, cm, mi_row, mi_col, mv, pred16, second_pred, w, h, subpel_x_q3,
            subpel_y_q3, y, y_stride, mask, mask_stride, invert_mask, xd->bd);
      } else {
        if (xd->jcp_param.use_jnt_comp_avg)
          aom_highbd_jnt_comp_avg_upsampled_pred(
              xd, cm, mi_row, mi_col, mv, pred16, second_pred, w, h,
              subpel_x_q3, subpel_y_q3, y, y_stride, xd->bd, &xd->jcp_param);
        else
          aom_highbd_comp_avg_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred16,
                                             second_pred, w, h, subpel_x_q3,
                                             subpel_y_q3, y, y_stride, xd->bd);
      }
    } else {
      aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred16, w, h,
                                subpel_x_q3, subpel_y_q3, y, y_stride, xd->bd);
    }

    besterr = vfp->vf(CONVERT_TO_BYTEPTR(pred16), w, src, src_stride, sse);
  } else {
    DECLARE_ALIGNED(16, uint8_t, pred[MAX_SB_SQUARE]);
    if (second_pred != NULL) {
      if (mask) {
        aom_comp_mask_upsampled_pred(
            xd, cm, mi_row, mi_col, mv, pred, second_pred, w, h, subpel_x_q3,
            subpel_y_q3, y, y_stride, mask, mask_stride, invert_mask);
      } else {
        if (xd->jcp_param.use_jnt_comp_avg)
          aom_jnt_comp_avg_upsampled_pred(
              xd, cm, mi_row, mi_col, mv, pred, second_pred, w, h, subpel_x_q3,
              subpel_y_q3, y, y_stride, &xd->jcp_param);
        else
          aom_comp_avg_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred,
                                      second_pred, w, h, subpel_x_q3,
                                      subpel_y_q3, y, y_stride);
      }
    } else {
      aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred, w, h, subpel_x_q3,
                         subpel_y_q3, y, y_stride);
    }

    besterr = vfp->vf(pred, w, src, src_stride, sse);
  }
  return besterr;
}

static unsigned int upsampled_setup_center_error(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *bestmv, const MV *ref_mv, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, const uint8_t *const src,
    const int src_stride, const uint8_t *const y, int y_stride,
    const uint8_t *second_pred, const uint8_t *mask, int mask_stride,
    int invert_mask, int w, int h, int offset, int *mvjcost, int *mvcost[2],
    unsigned int *sse1, int *distortion) {
  unsigned int besterr = upsampled_pref_error(
      xd, cm, mi_row, mi_col, bestmv, vfp, src, src_stride, y + offset,
      y_stride, 0, 0, second_pred, mask, mask_stride, invert_mask, w, h, sse1);
  *distortion = besterr;
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);
  return besterr;
}

// when use_accurate_subpel_search == 0
static INLINE unsigned int estimate_upsampled_pref_error(
    MACROBLOCKD *xd, const aom_variance_fn_ptr_t *vfp, const uint8_t *const src,
    const int src_stride, const uint8_t *const pre, int y_stride,
    int subpel_x_q3, int subpel_y_q3, const uint8_t *second_pred,
    const uint8_t *mask, int mask_stride, int invert_mask, unsigned int *sse) {
  if (second_pred == NULL) {
    return vfp->svf(pre, y_stride, subpel_x_q3, subpel_y_q3, src, src_stride,
                    sse);
  } else if (mask) {
    return vfp->msvf(pre, y_stride, subpel_x_q3, subpel_y_q3, src, src_stride,
                     second_pred, mask, mask_stride, invert_mask, sse);
  } else {
    if (xd->jcp_param.use_jnt_comp_avg)
      return vfp->jsvaf(pre, y_stride, subpel_x_q3, subpel_y_q3, src,
                        src_stride, sse, second_pred, &xd->jcp_param);
    else
      return vfp->svaf(pre, y_stride, subpel_x_q3, subpel_y_q3, src, src_stride,
                       sse, second_pred);
  }
}

int av1_find_best_sub_pixel_tree(
    MACROBLOCK *x, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *ref_mv, int allow_hp, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, int forced_stop, int iters_per_step,
    int *cost_list, int *mvjcost, int *mvcost[2], int *distortion,
    unsigned int *sse1, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, int invert_mask, int w, int h,
    int use_accurate_subpel_search) {
  const uint8_t *const src_address = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  MACROBLOCKD *xd = &x->e_mbd;
  unsigned int besterr = INT_MAX;
  unsigned int sse;
  unsigned int thismse;
  const int y_stride = xd->plane[0].pre[0].stride;
  MV *bestmv = &x->best_mv.as_mv;
  const int offset = bestmv->row * y_stride + bestmv->col;
  const uint8_t *const y = xd->plane[0].pre[0].buf;

  int br = bestmv->row * 8;
  int bc = bestmv->col * 8;
  int hstep = 4;
  int iter, round = 3 - forced_stop;
  int tr = br;
  int tc = bc;
  const MV *search_step = search_step_table;
  int idx, best_idx = -1;
  unsigned int cost_array[5];
  int kr, kc;
  int minc, maxc, minr, maxr;

  set_subpel_mv_search_range(&x->mv_limits, &minc, &maxc, &minr, &maxr, ref_mv);

  if (!allow_hp)
    if (round == 3) round = 2;

  bestmv->row *= 8;
  bestmv->col *= 8;

  if (use_accurate_subpel_search)
    besterr = upsampled_setup_center_error(
        xd, cm, mi_row, mi_col, bestmv, ref_mv, error_per_bit, vfp, src_address,
        src_stride, y, y_stride, second_pred, mask, mask_stride, invert_mask, w,
        h, offset, mvjcost, mvcost, sse1, distortion);
  else
    besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp,
                                 src_address, src_stride, y, y_stride,
                                 second_pred, mask, mask_stride, invert_mask, w,
                                 h, offset, mvjcost, mvcost, sse1, distortion);

  (void)cost_list;  // to silence compiler warning

  for (iter = 0; iter < round; ++iter) {
    // Check vertical and horizontal sub-pixel positions.
    for (idx = 0; idx < 4; ++idx) {
      tr = br + search_step[idx].row;
      tc = bc + search_step[idx].col;
      if (tc >= minc && tc <= maxc && tr >= minr && tr <= maxr) {
        MV this_mv = { tr, tc };

        if (use_accurate_subpel_search) {
          thismse = upsampled_pref_error(
              xd, cm, mi_row, mi_col, &this_mv, vfp, src_address, src_stride,
              pre(y, y_stride, tr, tc), y_stride, sp(tc), sp(tr), second_pred,
              mask, mask_stride, invert_mask, w, h, &sse);
        } else {
          thismse = estimate_upsampled_pref_error(
              xd, vfp, src_address, src_stride, pre(y, y_stride, tr, tc),
              y_stride, sp(tc), sp(tr), second_pred, mask, mask_stride,
              invert_mask, &sse);
        }

        cost_array[idx] = thismse + mv_err_cost(&this_mv, ref_mv, mvjcost,
                                                mvcost, error_per_bit);

        if (cost_array[idx] < besterr) {
          best_idx = idx;
          besterr = cost_array[idx];
          *distortion = thismse;
          *sse1 = sse;
        }
      } else {
        cost_array[idx] = INT_MAX;
      }
    }

    // Check diagonal sub-pixel position
    kc = (cost_array[0] <= cost_array[1] ? -hstep : hstep);
    kr = (cost_array[2] <= cost_array[3] ? -hstep : hstep);

    tc = bc + kc;
    tr = br + kr;
    if (tc >= minc && tc <= maxc && tr >= minr && tr <= maxr) {
      MV this_mv = { tr, tc };

      if (use_accurate_subpel_search) {
        thismse = upsampled_pref_error(
            xd, cm, mi_row, mi_col, &this_mv, vfp, src_address, src_stride,
            pre(y, y_stride, tr, tc), y_stride, sp(tc), sp(tr), second_pred,
            mask, mask_stride, invert_mask, w, h, &sse);
      } else {
        thismse = estimate_upsampled_pref_error(
            xd, vfp, src_address, src_stride, pre(y, y_stride, tr, tc),
            y_stride, sp(tc), sp(tr), second_pred, mask, mask_stride,
            invert_mask, &sse);
      }

      cost_array[4] = thismse + mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost,
                                            error_per_bit);

      if (cost_array[4] < besterr) {
        best_idx = 4;
        besterr = cost_array[4];
        *distortion = thismse;
        *sse1 = sse;
      }
    } else {
      cost_array[idx] = INT_MAX;
    }

    if (best_idx < 4 && best_idx >= 0) {
      br += search_step[best_idx].row;
      bc += search_step[best_idx].col;
    } else if (best_idx == 4) {
      br = tr;
      bc = tc;
    }

    if (iters_per_step > 1 && best_idx != -1) {
      if (use_accurate_subpel_search) {
        SECOND_LEVEL_CHECKS_BEST(1);
      } else {
        SECOND_LEVEL_CHECKS_BEST(0);
      }
    }

    search_step += 4;
    hstep >>= 1;
    best_idx = -1;
  }

  // These lines insure static analysis doesn't warn that
  // tr and tc aren't used after the above point.
  (void)tr;
  (void)tc;

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

#undef PRE
#undef CHECK_BETTER

unsigned int av1_compute_motion_cost(const AV1_COMP *cpi, MACROBLOCK *const x,
                                     BLOCK_SIZE bsize, int mi_row, int mi_col,
                                     const MV *this_mv) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const uint8_t *const src = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  uint8_t *const dst = xd->plane[0].dst.buf;
  const int dst_stride = xd->plane[0].dst.stride;
  const aom_variance_fn_ptr_t *vfp = &cpi->fn_ptr[bsize];
  const int_mv ref_mv = av1_get_ref_mv(x, 0);
  unsigned int mse;
  unsigned int sse;

  av1_build_inter_predictors_sby(cm, xd, mi_row, mi_col, NULL, bsize);
  mse = vfp->vf(dst, dst_stride, src, src_stride, &sse);
  mse += mv_err_cost(this_mv, &ref_mv.as_mv, x->nmvjointcost, x->mvcost,
                     x->errorperbit);
  return mse;
}

// Refine MV in a small range
unsigned int av1_refine_warped_mv(const AV1_COMP *cpi, MACROBLOCK *const x,
                                  BLOCK_SIZE bsize, int mi_row, int mi_col,
                                  int *pts0, int *pts_inref0,
                                  int total_samples) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const MV neighbors[8] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
                            { 0, -2 }, { 2, 0 }, { 0, 2 }, { -2, 0 } };
  const int_mv ref_mv = av1_get_ref_mv(x, 0);
  int16_t br = mbmi->mv[0].as_mv.row;
  int16_t bc = mbmi->mv[0].as_mv.col;
  int16_t *tr = &mbmi->mv[0].as_mv.row;
  int16_t *tc = &mbmi->mv[0].as_mv.col;
  WarpedMotionParams best_wm_params = mbmi->wm_params[0];
  int best_num_proj_ref = mbmi->num_proj_ref[0];
  unsigned int bestmse;
  int minc, maxc, minr, maxr;
  const int start = cm->allow_high_precision_mv ? 0 : 4;
  int ite;

  set_subpel_mv_search_range(&x->mv_limits, &minc, &maxc, &minr, &maxr,
                             &ref_mv.as_mv);

  // Calculate the center position's error
  assert(bc >= minc && bc <= maxc && br >= minr && br <= maxr);
  bestmse = av1_compute_motion_cost(cpi, x, bsize, mi_row, mi_col,
                                    &mbmi->mv[0].as_mv);

  // MV search
  for (ite = 0; ite < 2; ++ite) {
    int best_idx = -1;
    int idx;

    for (idx = start; idx < start + 4; ++idx) {
      unsigned int thismse;

      *tr = br + neighbors[idx].row;
      *tc = bc + neighbors[idx].col;

      if (*tc >= minc && *tc <= maxc && *tr >= minr && *tr <= maxr) {
        MV this_mv = { *tr, *tc };
        int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];

        memcpy(pts, pts0, total_samples * 2 * sizeof(*pts0));
        memcpy(pts_inref, pts_inref0, total_samples * 2 * sizeof(*pts_inref0));
        if (total_samples > 1)
          mbmi->num_proj_ref[0] =
              selectSamples(&this_mv, pts, pts_inref, total_samples, bsize);

        if (!find_projection(mbmi->num_proj_ref[0], pts, pts_inref, bsize, *tr,
                             *tc, &mbmi->wm_params[0], mi_row, mi_col)) {
          thismse =
              av1_compute_motion_cost(cpi, x, bsize, mi_row, mi_col, &this_mv);

          if (thismse < bestmse) {
            best_idx = idx;
            best_wm_params = mbmi->wm_params[0];
            best_num_proj_ref = mbmi->num_proj_ref[0];
            bestmse = thismse;
          }
        }
      }
    }

    if (best_idx == -1) break;

    if (best_idx >= 0) {
      br += neighbors[best_idx].row;
      bc += neighbors[best_idx].col;
    }
  }

  *tr = br;
  *tc = bc;
  mbmi->wm_params[0] = best_wm_params;
  mbmi->num_proj_ref[0] = best_num_proj_ref;
  return bestmse;
}

static INLINE int check_bounds(const MvLimits *mv_limits, int row, int col,
                               int range) {
  return ((row - range) >= mv_limits->row_min) &
         ((row + range) <= mv_limits->row_max) &
         ((col - range) >= mv_limits->col_min) &
         ((col + range) <= mv_limits->col_max);
}

static INLINE int is_mv_in(const MvLimits *mv_limits, const MV *mv) {
  return (mv->col >= mv_limits->col_min) && (mv->col <= mv_limits->col_max) &&
         (mv->row >= mv_limits->row_min) && (mv->row <= mv_limits->row_max);
}

#define CHECK_BETTER                                                      \
  {                                                                       \
    if (thissad < bestsad) {                                              \
      if (use_mvcost)                                                     \
        thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit); \
      if (thissad < bestsad) {                                            \
        bestsad = thissad;                                                \
        best_site = i;                                                    \
      }                                                                   \
    }                                                                     \
  }

#define MAX_PATTERN_SCALES 11
#define MAX_PATTERN_CANDIDATES 8  // max number of canddiates per scale
#define PATTERN_CANDIDATES_REF 3  // number of refinement candidates

// Calculate and return a sad+mvcost list around an integer best pel.
static INLINE void calc_int_cost_list(const MACROBLOCK *x,
                                      const MV *const ref_mv, int sadpb,
                                      const aom_variance_fn_ptr_t *fn_ptr,
                                      const MV *best_mv, int *cost_list) {
  static const MV neighbors[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &x->e_mbd.plane[0].pre[0];
  const MV fcenter_mv = { ref_mv->row >> 3, ref_mv->col >> 3 };
  const int br = best_mv->row;
  const int bc = best_mv->col;
  int i;
  unsigned int sse;
  const MV this_mv = { br, bc };

  cost_list[0] =
      fn_ptr->vf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv),
                 in_what->stride, &sse) +
      mvsad_err_cost(x, &this_mv, &fcenter_mv, sadpb);
  if (check_bounds(&x->mv_limits, br, bc, 1)) {
    for (i = 0; i < 4; i++) {
      const MV neighbor_mv = { br + neighbors[i].row, bc + neighbors[i].col };
      cost_list[i + 1] = fn_ptr->vf(what->buf, what->stride,
                                    get_buf_from_mv(in_what, &neighbor_mv),
                                    in_what->stride, &sse) +
                         mv_err_cost(&neighbor_mv, &fcenter_mv, x->nmvjointcost,
                                     x->mvcost, x->errorperbit);
    }
  } else {
    for (i = 0; i < 4; i++) {
      const MV neighbor_mv = { br + neighbors[i].row, bc + neighbors[i].col };
      if (!is_mv_in(&x->mv_limits, &neighbor_mv))
        cost_list[i + 1] = INT_MAX;
      else
        cost_list[i + 1] =
            fn_ptr->vf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &neighbor_mv), in_what->stride,
                       &sse) +
            mv_err_cost(&neighbor_mv, &fcenter_mv, x->nmvjointcost, x->mvcost,
                        x->errorperbit);
    }
  }
}

static INLINE void calc_int_sad_list(const MACROBLOCK *x,
                                     const MV *const ref_mv, int sadpb,
                                     const aom_variance_fn_ptr_t *fn_ptr,
                                     const MV *best_mv, int *cost_list,
                                     const int use_mvcost, const int bestsad) {
  static const MV neighbors[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &x->e_mbd.plane[0].pre[0];
  const MV fcenter_mv = { ref_mv->row >> 3, ref_mv->col >> 3 };
  int i;
  const int br = best_mv->row;
  const int bc = best_mv->col;

  if (cost_list[0] == INT_MAX) {
    cost_list[0] = bestsad;
    if (check_bounds(&x->mv_limits, br, bc, 1)) {
      for (i = 0; i < 4; i++) {
        const MV this_mv = { br + neighbors[i].row, bc + neighbors[i].col };
        cost_list[i + 1] =
            fn_ptr->sdf(what->buf, what->stride,
                        get_buf_from_mv(in_what, &this_mv), in_what->stride);
      }
    } else {
      for (i = 0; i < 4; i++) {
        const MV this_mv = { br + neighbors[i].row, bc + neighbors[i].col };
        if (!is_mv_in(&x->mv_limits, &this_mv))
          cost_list[i + 1] = INT_MAX;
        else
          cost_list[i + 1] =
              fn_ptr->sdf(what->buf, what->stride,
                          get_buf_from_mv(in_what, &this_mv), in_what->stride);
      }
    }
  } else {
    if (use_mvcost) {
      for (i = 0; i < 4; i++) {
        const MV this_mv = { br + neighbors[i].row, bc + neighbors[i].col };
        if (cost_list[i + 1] != INT_MAX) {
          cost_list[i + 1] += mvsad_err_cost(x, &this_mv, &fcenter_mv, sadpb);
        }
      }
    }
  }
}

// Generic pattern search function that searches over multiple scales.
// Each scale can have a different number of candidates and shape of
// candidates as indicated in the num_candidates and candidates arrays
// passed into this function
//
static int pattern_search(
    MACROBLOCK *x, MV *start_mv, int search_param, int sad_per_bit,
    int do_init_search, int *cost_list, const aom_variance_fn_ptr_t *vfp,
    int use_mvcost, const MV *center_mv,
    const int num_candidates[MAX_PATTERN_SCALES],
    const MV candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES]) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  static const int search_param_to_steps[MAX_MVSEARCH_STEPS] = {
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  int i, s, t;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const int last_is_4 = num_candidates[0] == 4;
  int br, bc;
  int bestsad = INT_MAX;
  int thissad;
  int k = -1;
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  assert(search_param < MAX_MVSEARCH_STEPS);
  int best_init_s = search_param_to_steps[search_param];
  // adjust ref_mv to make sure it is within MV range
  clamp_mv(start_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  br = start_mv->row;
  bc = start_mv->col;
  if (cost_list != NULL) {
    cost_list[0] = cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] =
        INT_MAX;
  }

  // Work out the start point for the search
  bestsad = vfp->sdf(what->buf, what->stride,
                     get_buf_from_mv(in_what, start_mv), in_what->stride) +
            mvsad_err_cost(x, start_mv, &fcenter_mv, sad_per_bit);

  // Search all possible scales upto the search param around the center point
  // pick the scale of the point that is best as the starting scale of
  // further steps around it.
  if (do_init_search) {
    s = best_init_s;
    best_init_s = -1;
    for (t = 0; t <= s; ++t) {
      int best_site = -1;
      if (check_bounds(&x->mv_limits, br, bc, 1 << t)) {
        for (i = 0; i < num_candidates[t]; i++) {
          const MV this_mv = { br + candidates[t][i].row,
                               bc + candidates[t][i].col };
          thissad =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
          CHECK_BETTER
        }
      } else {
        for (i = 0; i < num_candidates[t]; i++) {
          const MV this_mv = { br + candidates[t][i].row,
                               bc + candidates[t][i].col };
          if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
          thissad =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
          CHECK_BETTER
        }
      }
      if (best_site == -1) {
        continue;
      } else {
        best_init_s = t;
        k = best_site;
      }
    }
    if (best_init_s != -1) {
      br += candidates[best_init_s][k].row;
      bc += candidates[best_init_s][k].col;
    }
  }

  // If the center point is still the best, just skip this and move to
  // the refinement step.
  if (best_init_s != -1) {
    const int last_s = (last_is_4 && cost_list != NULL);
    int best_site = -1;
    s = best_init_s;

    for (; s >= last_s; s--) {
      // No need to search all points the 1st time if initial search was used
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site == -1) {
          continue;
        } else {
          br += candidates[s][best_site].row;
          bc += candidates[s][best_site].col;
          k = best_site;
        }
      }

      do {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;

        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += candidates[s][k].row;
          bc += candidates[s][k].col;
        }
      } while (best_site != -1);
    }

    // Note: If we enter the if below, then cost_list must be non-NULL.
    if (s == 0) {
      cost_list[0] = bestsad;
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            cost_list[i + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = { br + candidates[s][i].row,
                                 bc + candidates[s][i].col };
            if (!is_mv_in(&x->mv_limits, &this_mv)) continue;
            cost_list[i + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          br += candidates[s][best_site].row;
          bc += candidates[s][best_site].col;
          k = best_site;
        }
      }
      while (best_site != -1) {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;
        cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] = INT_MAX;
        cost_list[((k + 2) % 4) + 1] = cost_list[0];
        cost_list[0] = bestsad;

        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            cost_list[next_chkpts_indices[i] + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            if (!is_mv_in(&x->mv_limits, &this_mv)) {
              cost_list[next_chkpts_indices[i] + 1] = INT_MAX;
              continue;
            }
            cost_list[next_chkpts_indices[i] + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += candidates[s][k].row;
          bc += candidates[s][k].col;
        }
      }
    }
  }

  // Returns the one-away integer pel cost/sad around the best as follows:
  // cost_list[0]: cost/sad at the best integer pel
  // cost_list[1]: cost/sad at delta {0, -1} (left)   from the best integer pel
  // cost_list[2]: cost/sad at delta { 1, 0} (bottom) from the best integer pel
  // cost_list[3]: cost/sad at delta { 0, 1} (right)  from the best integer pel
  // cost_list[4]: cost/sad at delta {-1, 0} (top)    from the best integer pel
  if (cost_list) {
    const MV best_int_mv = { br, bc };
    if (last_is_4) {
      calc_int_sad_list(x, center_mv, sad_per_bit, vfp, &best_int_mv, cost_list,
                        use_mvcost, bestsad);
    } else {
      calc_int_cost_list(x, center_mv, sad_per_bit, vfp, &best_int_mv,
                         cost_list);
    }
  }
  x->best_mv.as_mv.row = br;
  x->best_mv.as_mv.col = bc;
  return bestsad;
}

int av1_get_mvpred_var(const MACROBLOCK *x, const MV *best_mv,
                       const MV *center_mv, const aom_variance_fn_ptr_t *vfp,
                       int use_mvcost) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = { best_mv->row * 8, best_mv->col * 8 };
  unsigned int unused;

  return vfp->vf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv),
                 in_what->stride, &unused) +
         (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost,
                                   x->errorperbit)
                     : 0);
}

int av1_get_mvpred_av_var(const MACROBLOCK *x, const MV *best_mv,
                          const MV *center_mv, const uint8_t *second_pred,
                          const aom_variance_fn_ptr_t *vfp, int use_mvcost) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = { best_mv->row * 8, best_mv->col * 8 };
  unsigned int unused;

  if (xd->jcp_param.use_jnt_comp_avg)
    return vfp->jsvaf(get_buf_from_mv(in_what, best_mv), in_what->stride, 0, 0,
                      what->buf, what->stride, &unused, second_pred,
                      &xd->jcp_param) +
           (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost,
                                     x->errorperbit)
                       : 0);
  else
    return vfp->svaf(get_buf_from_mv(in_what, best_mv), in_what->stride, 0, 0,
                     what->buf, what->stride, &unused, second_pred) +
           (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost,
                                     x->errorperbit)
                       : 0);
}

int av1_get_mvpred_mask_var(const MACROBLOCK *x, const MV *best_mv,
                            const MV *center_mv, const uint8_t *second_pred,
                            const uint8_t *mask, int mask_stride,
                            int invert_mask, const aom_variance_fn_ptr_t *vfp,
                            int use_mvcost) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = { best_mv->row * 8, best_mv->col * 8 };
  unsigned int unused;

  return vfp->msvf(what->buf, what->stride, 0, 0,
                   get_buf_from_mv(in_what, best_mv), in_what->stride,
                   second_pred, mask, mask_stride, invert_mask, &unused) +
         (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost,
                                   x->errorperbit)
                     : 0);
}

int av1_hex_search(MACROBLOCK *x, MV *start_mv, int search_param,
                   int sad_per_bit, int do_init_search, int *cost_list,
                   const aom_variance_fn_ptr_t *vfp, int use_mvcost,
                   const MV *center_mv) {
  // First scale has 8-closest points, the rest have 6 points in hex shape
  // at increasing scales
  static const int hex_num_candidates[MAX_PATTERN_SCALES] = { 8, 6, 6, 6, 6, 6,
                                                              6, 6, 6, 6, 6 };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV hex_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
    { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 },
      { -1, 0 } },
    { { -1, -2 }, { 1, -2 }, { 2, 0 }, { 1, 2 }, { -1, 2 }, { -2, 0 } },
    { { -2, -4 }, { 2, -4 }, { 4, 0 }, { 2, 4 }, { -2, 4 }, { -4, 0 } },
    { { -4, -8 }, { 4, -8 }, { 8, 0 }, { 4, 8 }, { -4, 8 }, { -8, 0 } },
    { { -8, -16 }, { 8, -16 }, { 16, 0 }, { 8, 16 }, { -8, 16 }, { -16, 0 } },
    { { -16, -32 }, { 16, -32 }, { 32, 0 }, { 16, 32 }, { -16, 32 },
      { -32, 0 } },
    { { -32, -64 }, { 32, -64 }, { 64, 0 }, { 32, 64 }, { -32, 64 },
      { -64, 0 } },
    { { -64, -128 }, { 64, -128 }, { 128, 0 }, { 64, 128 }, { -64, 128 },
      { -128, 0 } },
    { { -128, -256 }, { 128, -256 }, { 256, 0 }, { 128, 256 }, { -128, 256 },
      { -256, 0 } },
    { { -256, -512 }, { 256, -512 }, { 512, 0 }, { 256, 512 }, { -256, 512 },
      { -512, 0 } },
    { { -512, -1024 }, { 512, -1024 }, { 1024, 0 }, { 512, 1024 },
      { -512, 1024 }, { -1024, 0 } },
  };
  /* clang-format on */
  return pattern_search(x, start_mv, search_param, sad_per_bit, do_init_search,
                        cost_list, vfp, use_mvcost, center_mv,
                        hex_num_candidates, hex_candidates);
}

static int bigdia_search(MACROBLOCK *x, MV *start_mv, int search_param,
                         int sad_per_bit, int do_init_search, int *cost_list,
                         const aom_variance_fn_ptr_t *vfp, int use_mvcost,
                         const MV *center_mv) {
  // First scale has 4-closest points, the rest have 8 points in diamond
  // shape at increasing scales
  static const int bigdia_num_candidates[MAX_PATTERN_SCALES] = {
    4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV
      bigdia_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
        { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } },
        { { -1, -1 }, { 0, -2 }, { 1, -1 }, { 2, 0 }, { 1, 1 }, { 0, 2 },
          { -1, 1 }, { -2, 0 } },
        { { -2, -2 }, { 0, -4 }, { 2, -2 }, { 4, 0 }, { 2, 2 }, { 0, 4 },
          { -2, 2 }, { -4, 0 } },
        { { -4, -4 }, { 0, -8 }, { 4, -4 }, { 8, 0 }, { 4, 4 }, { 0, 8 },
          { -4, 4 }, { -8, 0 } },
        { { -8, -8 }, { 0, -16 }, { 8, -8 }, { 16, 0 }, { 8, 8 }, { 0, 16 },
          { -8, 8 }, { -16, 0 } },
        { { -16, -16 }, { 0, -32 }, { 16, -16 }, { 32, 0 }, { 16, 16 },
          { 0, 32 }, { -16, 16 }, { -32, 0 } },
        { { -32, -32 }, { 0, -64 }, { 32, -32 }, { 64, 0 }, { 32, 32 },
          { 0, 64 }, { -32, 32 }, { -64, 0 } },
        { { -64, -64 }, { 0, -128 }, { 64, -64 }, { 128, 0 }, { 64, 64 },
          { 0, 128 }, { -64, 64 }, { -128, 0 } },
        { { -128, -128 }, { 0, -256 }, { 128, -128 }, { 256, 0 }, { 128, 128 },
          { 0, 256 }, { -128, 128 }, { -256, 0 } },
        { { -256, -256 }, { 0, -512 }, { 256, -256 }, { 512, 0 }, { 256, 256 },
          { 0, 512 }, { -256, 256 }, { -512, 0 } },
        { { -512, -512 }, { 0, -1024 }, { 512, -512 }, { 1024, 0 },
          { 512, 512 }, { 0, 1024 }, { -512, 512 }, { -1024, 0 } },
      };
  /* clang-format on */
  return pattern_search(x, start_mv, search_param, sad_per_bit, do_init_search,
                        cost_list, vfp, use_mvcost, center_mv,
                        bigdia_num_candidates, bigdia_candidates);
}

static int square_search(MACROBLOCK *x, MV *start_mv, int search_param,
                         int sad_per_bit, int do_init_search, int *cost_list,
                         const aom_variance_fn_ptr_t *vfp, int use_mvcost,
                         const MV *center_mv) {
  // All scales have 8 closest points in square shape
  static const int square_num_candidates[MAX_PATTERN_SCALES] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV
      square_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
        { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 },
          { -1, 1 }, { -1, 0 } },
        { { -2, -2 }, { 0, -2 }, { 2, -2 }, { 2, 0 }, { 2, 2 }, { 0, 2 },
          { -2, 2 }, { -2, 0 } },
        { { -4, -4 }, { 0, -4 }, { 4, -4 }, { 4, 0 }, { 4, 4 }, { 0, 4 },
          { -4, 4 }, { -4, 0 } },
        { { -8, -8 }, { 0, -8 }, { 8, -8 }, { 8, 0 }, { 8, 8 }, { 0, 8 },
          { -8, 8 }, { -8, 0 } },
        { { -16, -16 }, { 0, -16 }, { 16, -16 }, { 16, 0 }, { 16, 16 },
          { 0, 16 }, { -16, 16 }, { -16, 0 } },
        { { -32, -32 }, { 0, -32 }, { 32, -32 }, { 32, 0 }, { 32, 32 },
          { 0, 32 }, { -32, 32 }, { -32, 0 } },
        { { -64, -64 }, { 0, -64 }, { 64, -64 }, { 64, 0 }, { 64, 64 },
          { 0, 64 }, { -64, 64 }, { -64, 0 } },
        { { -128, -128 }, { 0, -128 }, { 128, -128 }, { 128, 0 }, { 128, 128 },
          { 0, 128 }, { -128, 128 }, { -128, 0 } },
        { { -256, -256 }, { 0, -256 }, { 256, -256 }, { 256, 0 }, { 256, 256 },
          { 0, 256 }, { -256, 256 }, { -256, 0 } },
        { { -512, -512 }, { 0, -512 }, { 512, -512 }, { 512, 0 }, { 512, 512 },
          { 0, 512 }, { -512, 512 }, { -512, 0 } },
        { { -1024, -1024 }, { 0, -1024 }, { 1024, -1024 }, { 1024, 0 },
          { 1024, 1024 }, { 0, 1024 }, { -1024, 1024 }, { -1024, 0 } },
      };
  /* clang-format on */
  return pattern_search(x, start_mv, search_param, sad_per_bit, do_init_search,
                        cost_list, vfp, use_mvcost, center_mv,
                        square_num_candidates, square_candidates);
}

static int fast_hex_search(MACROBLOCK *x, MV *ref_mv, int search_param,
                           int sad_per_bit,
                           int do_init_search,  // must be zero for fast_hex
                           int *cost_list, const aom_variance_fn_ptr_t *vfp,
                           int use_mvcost, const MV *center_mv) {
  return av1_hex_search(x, ref_mv, AOMMAX(MAX_MVSEARCH_STEPS - 2, search_param),
                        sad_per_bit, do_init_search, cost_list, vfp, use_mvcost,
                        center_mv);
}

static int fast_dia_search(MACROBLOCK *x, MV *ref_mv, int search_param,
                           int sad_per_bit, int do_init_search, int *cost_list,
                           const aom_variance_fn_ptr_t *vfp, int use_mvcost,
                           const MV *center_mv) {
  return bigdia_search(x, ref_mv, AOMMAX(MAX_MVSEARCH_STEPS - 2, search_param),
                       sad_per_bit, do_init_search, cost_list, vfp, use_mvcost,
                       center_mv);
}

#undef CHECK_BETTER

// Exhuastive motion search around a given centre position with a given
// step size.
static int exhuastive_mesh_search(MACROBLOCK *x, MV *ref_mv, MV *best_mv,
                                  int range, int step, int sad_per_bit,
                                  const aom_variance_fn_ptr_t *fn_ptr,
                                  const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  MV fcenter_mv = { center_mv->row, center_mv->col };
  unsigned int best_sad = INT_MAX;
  int r, c, i;
  int start_col, end_col, start_row, end_row;
  int col_step = (step > 1) ? step : 4;

  assert(step >= 1);

  clamp_mv(&fcenter_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  *best_mv = fcenter_mv;
  best_sad =
      fn_ptr->sdf(what->buf, what->stride,
                  get_buf_from_mv(in_what, &fcenter_mv), in_what->stride) +
      mvsad_err_cost(x, &fcenter_mv, ref_mv, sad_per_bit);
  start_row = AOMMAX(-range, x->mv_limits.row_min - fcenter_mv.row);
  start_col = AOMMAX(-range, x->mv_limits.col_min - fcenter_mv.col);
  end_row = AOMMIN(range, x->mv_limits.row_max - fcenter_mv.row);
  end_col = AOMMIN(range, x->mv_limits.col_max - fcenter_mv.col);

  for (r = start_row; r <= end_row; r += step) {
    for (c = start_col; c <= end_col; c += col_step) {
      // Step > 1 means we are not checking every location in this pass.
      if (step > 1) {
        const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c };
        unsigned int sad =
            fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &mv),
                        in_what->stride);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            x->second_best_mv.as_mv = *best_mv;
            *best_mv = mv;
          }
        }
      } else {
        // 4 sads in a single call if we are checking every location
        if (c + 3 <= end_col) {
          unsigned int sads[4];
          const uint8_t *addrs[4];
          for (i = 0; i < 4; ++i) {
            const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
            addrs[i] = get_buf_from_mv(in_what, &mv);
          }
          fn_ptr->sdx4df(what->buf, what->stride, addrs, in_what->stride, sads);

          for (i = 0; i < 4; ++i) {
            if (sads[i] < best_sad) {
              const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
              const unsigned int sad =
                  sads[i] + mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
              if (sad < best_sad) {
                best_sad = sad;
                x->second_best_mv.as_mv = *best_mv;
                *best_mv = mv;
              }
            }
          }
        } else {
          for (i = 0; i < end_col - c; ++i) {
            const MV mv = { fcenter_mv.row + r, fcenter_mv.col + c + i };
            unsigned int sad =
                fn_ptr->sdf(what->buf, what->stride,
                            get_buf_from_mv(in_what, &mv), in_what->stride);
            if (sad < best_sad) {
              sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
              if (sad < best_sad) {
                best_sad = sad;
                x->second_best_mv.as_mv = *best_mv;
                *best_mv = mv;
              }
            }
          }
        }
      }
    }
  }

  return best_sad;
}

int av1_diamond_search_sad_c(MACROBLOCK *x, const search_site_config *cfg,
                             MV *ref_mv, MV *best_mv, int search_param,
                             int sad_per_bit, int *num00,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const MV *center_mv) {
  int i, j, step;

  const MACROBLOCKD *const xd = &x->e_mbd;
  uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *best_address;

  unsigned int bestsad = INT_MAX;
  int best_site = 0;
  int last_site = 0;

  int ref_row;
  int ref_col;

  // search_param determines the length of the initial step and hence the number
  // of iterations.
  // 0 = initial step (MAX_FIRST_STEP) pel
  // 1 = (MAX_FIRST_STEP/2) pel,
  // 2 = (MAX_FIRST_STEP/4) pel...
  const search_site *ss = &cfg->ss[search_param * cfg->searches_per_step];
  const int tot_steps = (cfg->ss_count / cfg->searches_per_step) - search_param;

  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  clamp_mv(ref_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  ref_row = ref_mv->row;
  ref_col = ref_mv->col;
  *num00 = 0;
  best_mv->row = ref_row;
  best_mv->col = ref_col;

  // Work out the start point for the search
  in_what = xd->plane[0].pre[0].buf + ref_row * in_what_stride + ref_col;
  best_address = in_what;

  // Check the starting position
  bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride) +
            mvsad_err_cost(x, best_mv, &fcenter_mv, sad_per_bit);

  i = 1;

  for (step = 0; step < tot_steps; step++) {
    int all_in = 1, t;

    // All_in is true if every one of the points we are checking are within
    // the bounds of the image.
    all_in &= ((best_mv->row + ss[i].mv.row) > x->mv_limits.row_min);
    all_in &= ((best_mv->row + ss[i + 1].mv.row) < x->mv_limits.row_max);
    all_in &= ((best_mv->col + ss[i + 2].mv.col) > x->mv_limits.col_min);
    all_in &= ((best_mv->col + ss[i + 3].mv.col) < x->mv_limits.col_max);

    // If all the pixels are within the bounds we don't check whether the
    // search point is valid in this loop,  otherwise we check each point
    // for validity..
    if (all_in) {
      unsigned int sad_array[4];

      for (j = 0; j < cfg->searches_per_step; j += 4) {
        unsigned char const *block_offset[4];

        for (t = 0; t < 4; t++)
          block_offset[t] = ss[i + t].offset + best_address;

        fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride,
                       sad_array);

        for (t = 0; t < 4; t++, i++) {
          if (sad_array[t] < bestsad) {
            const MV this_mv = { best_mv->row + ss[i].mv.row,
                                 best_mv->col + ss[i].mv.col };
            sad_array[t] +=
                mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (sad_array[t] < bestsad) {
              bestsad = sad_array[t];
              best_site = i;
            }
          }
        }
      }
    } else {
      for (j = 0; j < cfg->searches_per_step; j++) {
        // Trap illegal vectors
        const MV this_mv = { best_mv->row + ss[i].mv.row,
                             best_mv->col + ss[i].mv.col };

        if (is_mv_in(&x->mv_limits, &this_mv)) {
          const uint8_t *const check_here = ss[i].offset + best_address;
          unsigned int thissad =
              fn_ptr->sdf(what, what_stride, check_here, in_what_stride);

          if (thissad < bestsad) {
            thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = i;
            }
          }
        }
        i++;
      }
    }
    if (best_site != last_site) {
      x->second_best_mv.as_mv = *best_mv;
      best_mv->row += ss[best_site].mv.row;
      best_mv->col += ss[best_site].mv.col;
      best_address += ss[best_site].offset;
      last_site = best_site;
#if defined(NEW_DIAMOND_SEARCH)
      while (1) {
        const MV this_mv = { best_mv->row + ss[best_site].mv.row,
                             best_mv->col + ss[best_site].mv.col };
        if (is_mv_in(&x->mv_limits, &this_mv)) {
          const uint8_t *const check_here = ss[best_site].offset + best_address;
          unsigned int thissad =
              fn_ptr->sdf(what, what_stride, check_here, in_what_stride);
          if (thissad < bestsad) {
            thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_mv->row += ss[best_site].mv.row;
              best_mv->col += ss[best_site].mv.col;
              best_address += ss[best_site].offset;
              continue;
            }
          }
        }
        break;
      }
#endif
    } else if (best_address == in_what) {
      (*num00)++;
    }
  }
  return bestsad;
}

/* do_refine: If last step (1-away) of n-step search doesn't pick the center
              point as the best match, we will do a final 1-away diamond
              refining search  */
static int full_pixel_diamond(const AV1_COMP *const cpi, MACROBLOCK *x,
                              MV *mvp_full, int step_param, int sadpb,
                              int further_steps, int do_refine, int *cost_list,
                              const aom_variance_fn_ptr_t *fn_ptr,
                              const MV *ref_mv) {
  MV temp_mv;
  int thissme, n, num00 = 0;
  int bestsme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                        step_param, sadpb, &n, fn_ptr, ref_mv);
  if (bestsme < INT_MAX)
    bestsme = av1_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);
  x->best_mv.as_mv = temp_mv;

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  if (n > further_steps) do_refine = 0;

  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      thissme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                        step_param + n, sadpb, &num00, fn_ptr,
                                        ref_mv);
      if (thissme < INT_MAX)
        thissme = av1_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);

      // check to see if refining search is needed.
      if (num00 > further_steps - n) do_refine = 0;

      if (thissme < bestsme) {
        bestsme = thissme;
        x->best_mv.as_mv = temp_mv;
      }
    }
  }

  // final 1-away diamond refining search
  if (do_refine) {
    const int search_range = 8;
    MV best_mv = x->best_mv.as_mv;
    thissme = av1_refining_search_sad(x, &best_mv, sadpb, search_range, fn_ptr,
                                      ref_mv);
    if (thissme < INT_MAX)
      thissme = av1_get_mvpred_var(x, &best_mv, ref_mv, fn_ptr, 1);
    if (thissme < bestsme) {
      bestsme = thissme;
      x->best_mv.as_mv = best_mv;
    }
  }

  // Return cost list.
  if (cost_list) {
    calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, &x->best_mv.as_mv, cost_list);
  }
  return bestsme;
}

#define MIN_RANGE 7
#define MAX_RANGE 256
#define MIN_INTERVAL 1
// Runs an limited range exhaustive mesh search using a pattern set
// according to the encode speed profile.
static int full_pixel_exhaustive(const AV1_COMP *const cpi, MACROBLOCK *x,
                                 const MV *centre_mv_full, int sadpb,
                                 int *cost_list,
                                 const aom_variance_fn_ptr_t *fn_ptr,
                                 const MV *ref_mv, MV *dst_mv) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  MV temp_mv = { centre_mv_full->row, centre_mv_full->col };
  MV f_ref_mv = { ref_mv->row >> 3, ref_mv->col >> 3 };
  int bestsme;
  int i;
  int interval = sf->mesh_patterns[0].interval;
  int range = sf->mesh_patterns[0].range;
  int baseline_interval_divisor;

  // Keep track of number of exhaustive calls (this frame in this thread).
  ++(*x->ex_search_count_ptr);

  // Trap illegal values for interval and range for this function.
  if ((range < MIN_RANGE) || (range > MAX_RANGE) || (interval < MIN_INTERVAL) ||
      (interval > range))
    return INT_MAX;

  baseline_interval_divisor = range / interval;

  // Check size of proposed first range against magnitude of the centre
  // value used as a starting point.
  range = AOMMAX(range, (5 * AOMMAX(abs(temp_mv.row), abs(temp_mv.col))) / 4);
  range = AOMMIN(range, MAX_RANGE);
  interval = AOMMAX(interval, range / baseline_interval_divisor);

  // initial search
  bestsme = exhuastive_mesh_search(x, &f_ref_mv, &temp_mv, range, interval,
                                   sadpb, fn_ptr, &temp_mv);

  if ((interval > MIN_INTERVAL) && (range > MIN_RANGE)) {
    // Progressive searches with range and step size decreasing each time
    // till we reach a step size of 1. Then break out.
    for (i = 1; i < MAX_MESH_STEP; ++i) {
      // First pass with coarser step and longer range
      bestsme = exhuastive_mesh_search(
          x, &f_ref_mv, &temp_mv, sf->mesh_patterns[i].range,
          sf->mesh_patterns[i].interval, sadpb, fn_ptr, &temp_mv);

      if (sf->mesh_patterns[i].interval == 1) break;
    }
  }

  if (bestsme < INT_MAX)
    bestsme = av1_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);
  *dst_mv = temp_mv;

  // Return cost list.
  if (cost_list) {
    calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, dst_mv, cost_list);
  }
  return bestsme;
}

int av1_refining_search_sad(MACROBLOCK *x, MV *ref_mv, int error_per_bit,
                            int search_range,
                            const aom_variance_fn_ptr_t *fn_ptr,
                            const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MV neighbors[4] = { { -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  const uint8_t *best_address = get_buf_from_mv(in_what, ref_mv);
  unsigned int best_sad =
      fn_ptr->sdf(what->buf, what->stride, best_address, in_what->stride) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);
  int i, j;

  for (i = 0; i < search_range; i++) {
    int best_site = -1;
    const int all_in = ((ref_mv->row - 1) > x->mv_limits.row_min) &
                       ((ref_mv->row + 1) < x->mv_limits.row_max) &
                       ((ref_mv->col - 1) > x->mv_limits.col_min) &
                       ((ref_mv->col + 1) < x->mv_limits.col_max);

    if (all_in) {
      unsigned int sads[4];
      const uint8_t *const positions[4] = { best_address - in_what->stride,
                                            best_address - 1, best_address + 1,
                                            best_address + in_what->stride };

      fn_ptr->sdx4df(what->buf, what->stride, positions, in_what->stride, sads);

      for (j = 0; j < 4; ++j) {
        if (sads[j] < best_sad) {
          const MV mv = { ref_mv->row + neighbors[j].row,
                          ref_mv->col + neighbors[j].col };
          sads[j] += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
          if (sads[j] < best_sad) {
            best_sad = sads[j];
            best_site = j;
          }
        }
      }
    } else {
      for (j = 0; j < 4; ++j) {
        const MV mv = { ref_mv->row + neighbors[j].row,
                        ref_mv->col + neighbors[j].col };

        if (is_mv_in(&x->mv_limits, &mv)) {
          unsigned int sad =
              fn_ptr->sdf(what->buf, what->stride,
                          get_buf_from_mv(in_what, &mv), in_what->stride);
          if (sad < best_sad) {
            sad += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              best_site = j;
            }
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      x->second_best_mv.as_mv = *ref_mv;
      ref_mv->row += neighbors[best_site].row;
      ref_mv->col += neighbors[best_site].col;
      best_address = get_buf_from_mv(in_what, ref_mv);
    }
  }

  return best_sad;
}

// This function is called when we do joint motion search in comp_inter_inter
// mode, or when searching for one component of an ext-inter compound mode.
int av1_refining_search_8p_c(MACROBLOCK *x, int error_per_bit, int search_range,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const uint8_t *mask, int mask_stride,
                             int invert_mask, const MV *center_mv,
                             const uint8_t *second_pred) {
  const MV neighbors[8] = { { -1, 0 },  { 0, -1 }, { 0, 1 },  { 1, 0 },
                            { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  MV *best_mv = &x->best_mv.as_mv;
  unsigned int best_sad = INT_MAX;
  int i, j;

  clamp_mv(best_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  if (mask) {
    best_sad = fn_ptr->msdf(what->buf, what->stride,
                            get_buf_from_mv(in_what, best_mv), in_what->stride,
                            second_pred, mask, mask_stride, invert_mask) +
               mvsad_err_cost(x, best_mv, &fcenter_mv, error_per_bit);
  } else {
    if (xd->jcp_param.use_jnt_comp_avg)
      best_sad = fn_ptr->jsdaf(what->buf, what->stride,
                               get_buf_from_mv(in_what, best_mv),
                               in_what->stride, second_pred, &xd->jcp_param) +
                 mvsad_err_cost(x, best_mv, &fcenter_mv, error_per_bit);
    else
      best_sad = fn_ptr->sdaf(what->buf, what->stride,
                              get_buf_from_mv(in_what, best_mv),
                              in_what->stride, second_pred) +
                 mvsad_err_cost(x, best_mv, &fcenter_mv, error_per_bit);
  }

  for (i = 0; i < search_range; ++i) {
    int best_site = -1;

    for (j = 0; j < 8; ++j) {
      const MV mv = { best_mv->row + neighbors[j].row,
                      best_mv->col + neighbors[j].col };

      if (is_mv_in(&x->mv_limits, &mv)) {
        unsigned int sad;
        if (mask) {
          sad = fn_ptr->msdf(what->buf, what->stride,
                             get_buf_from_mv(in_what, &mv), in_what->stride,
                             second_pred, mask, mask_stride, invert_mask);
        } else {
          if (xd->jcp_param.use_jnt_comp_avg)
            sad = fn_ptr->jsdaf(what->buf, what->stride,
                                get_buf_from_mv(in_what, &mv), in_what->stride,
                                second_pred, &xd->jcp_param);
          else
            sad = fn_ptr->sdaf(what->buf, what->stride,
                               get_buf_from_mv(in_what, &mv), in_what->stride,
                               second_pred);
        }
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            best_site = j;
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      best_mv->row += neighbors[best_site].row;
      best_mv->col += neighbors[best_site].col;
    }
  }
  return best_sad;
}

#define MIN_EX_SEARCH_LIMIT 128
static int is_exhaustive_allowed(const AV1_COMP *const cpi, MACROBLOCK *x) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  const int max_ex =
      AOMMAX(MIN_EX_SEARCH_LIMIT,
             (*x->m_search_count_ptr * sf->max_exaustive_pct) / 100);

  return sf->allow_exhaustive_searches &&
         (sf->exhaustive_searches_thresh < INT_MAX) &&
         (*x->ex_search_count_ptr <= max_ex) && !cpi->rc.is_src_frame_alt_ref;
}

int av1_full_pixel_search(const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
                          MV *mvp_full, int step_param, int error_per_bit,
                          int *cost_list, const MV *ref_mv, int var_max, int rd,
                          int x_pos, int y_pos, int intra) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  const SEARCH_METHODS method = sf->mv.search_method;
  const aom_variance_fn_ptr_t *fn_ptr = &cpi->fn_ptr[bsize];
  int var = 0;

  if (cost_list) {
    cost_list[0] = INT_MAX;
    cost_list[1] = INT_MAX;
    cost_list[2] = INT_MAX;
    cost_list[3] = INT_MAX;
    cost_list[4] = INT_MAX;
  }

  // Keep track of number of searches (this frame in this thread).
  ++(*x->m_search_count_ptr);

  switch (method) {
    case FAST_DIAMOND:
      var = fast_dia_search(x, mvp_full, step_param, error_per_bit, 0,
                            cost_list, fn_ptr, 1, ref_mv);
      break;
    case FAST_HEX:
      var = fast_hex_search(x, mvp_full, step_param, error_per_bit, 0,
                            cost_list, fn_ptr, 1, ref_mv);
      break;
    case HEX:
      var = av1_hex_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
                           fn_ptr, 1, ref_mv);
      break;
    case SQUARE:
      var = square_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
                          fn_ptr, 1, ref_mv);
      break;
    case BIGDIA:
      var = bigdia_search(x, mvp_full, step_param, error_per_bit, 1, cost_list,
                          fn_ptr, 1, ref_mv);
      break;
    case NSTEP:
      var = full_pixel_diamond(cpi, x, mvp_full, step_param, error_per_bit,
                               MAX_MVSEARCH_STEPS - 1 - step_param, 1,
                               cost_list, fn_ptr, ref_mv);

      // Should we allow a follow on exhaustive search?
      if (is_exhaustive_allowed(cpi, x)) {
        int exhuastive_thr = sf->exhaustive_searches_thresh;
        exhuastive_thr >>=
            10 - (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);

        // Threshold variance for an exhaustive full search.
        if (var > exhuastive_thr) {
          int var_ex;
          MV tmp_mv_ex;
          var_ex =
              full_pixel_exhaustive(cpi, x, &x->best_mv.as_mv, error_per_bit,
                                    cost_list, fn_ptr, ref_mv, &tmp_mv_ex);

          if (var_ex < var) {
            var = var_ex;
            x->best_mv.as_mv = tmp_mv_ex;
          }
        }
      }
      break;
    default: assert(0 && "Invalid search method.");
  }

  if (method != NSTEP && rd && var < var_max)
    var = av1_get_mvpred_var(x, &x->best_mv.as_mv, ref_mv, fn_ptr, 1);

  do {
    if (!av1_use_hash_me(&cpi->common)) break;

    // already single ME
    // get block size and original buffer of current block
    const int block_height = block_size_high[bsize];
    const int block_width = block_size_wide[bsize];
    if (block_height == block_width && x_pos >= 0 && y_pos >= 0) {
      if (block_width == 4 || block_width == 8 || block_width == 16 ||
          block_width == 32 || block_width == 64 || block_width == 128) {
        uint8_t *what = x->plane[0].src.buf;
        const int what_stride = x->plane[0].src.stride;
        uint32_t hash_value1, hash_value2;
        MV best_hash_mv;
        int best_hash_cost = INT_MAX;

        // for the hashMap
        hash_table *ref_frame_hash =
            intra
                ? &cpi->common.cur_frame->hash_table
                : av1_get_ref_frame_hash_map(cpi, x->e_mbd.mi[0]->ref_frame[0]);

        av1_get_block_hash_value(
            what, what_stride, block_width, &hash_value1, &hash_value2,
            x->e_mbd.cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);

        const int count = av1_hash_table_count(ref_frame_hash, hash_value1);
        // for intra, at lest one matching can be found, itself.
        if (count <= (intra ? 1 : 0)) {
          break;
        }

        Iterator iterator =
            av1_hash_get_first_iterator(ref_frame_hash, hash_value1);
        for (int i = 0; i < count; i++, iterator_increment(&iterator)) {
          block_hash ref_block_hash = *(block_hash *)(iterator_get(&iterator));
          if (hash_value2 == ref_block_hash.hash_value2) {
            // For intra, make sure the prediction is from valid area.
            if (intra) {
              const int mi_col = x_pos / MI_SIZE;
              const int mi_row = y_pos / MI_SIZE;
              const MV dv = { 8 * (ref_block_hash.y - y_pos),
                              8 * (ref_block_hash.x - x_pos) };
              if (!av1_is_dv_valid(dv, &cpi->common, &x->e_mbd, mi_row, mi_col,
                                   bsize, cpi->common.seq_params.mib_size_log2))
                continue;
            }
            MV hash_mv;
            hash_mv.col = ref_block_hash.x - x_pos;
            hash_mv.row = ref_block_hash.y - y_pos;
            if (!is_mv_in(&x->mv_limits, &hash_mv)) continue;
            const int refCost =
                av1_get_mvpred_var(x, &hash_mv, ref_mv, fn_ptr, 1);
            if (refCost < best_hash_cost) {
              best_hash_cost = refCost;
              best_hash_mv = hash_mv;
            }
          }
        }
        if (best_hash_cost < var) {
          x->second_best_mv = x->best_mv;
          x->best_mv.as_mv = best_hash_mv;
          var = best_hash_cost;
        }
      }
    }
  } while (0);

  return var;
}

/* returns subpixel variance error function */
#define DIST(r, c) \
  vfp->osvf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), z, mask, &sse)

/* checks if (r, c) has better score than previous best */
#define MVC(r, c)                                                              \
  (unsigned int)(mvcost                                                        \
                     ? ((mvjcost[((r) != rr) * 2 + ((c) != rc)] +              \
                         mvcost[0][((r)-rr)] + (int64_t)mvcost[1][((c)-rc)]) * \
                            error_per_bit +                                    \
                        4096) >>                                               \
                           13                                                  \
                     : 0)

#define CHECK_BETTER(v, r, c)                             \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) { \
    thismse = (DIST(r, c));                               \
    if ((v = MVC(r, c) + thismse) < besterr) {            \
      besterr = v;                                        \
      br = r;                                             \
      bc = c;                                             \
      *distortion = thismse;                              \
      *sse1 = sse;                                        \
    }                                                     \
  } else {                                                \
    v = INT_MAX;                                          \
  }

#undef CHECK_BETTER0
#define CHECK_BETTER0(v, r, c) CHECK_BETTER(v, r, c)

#undef CHECK_BETTER1
#define CHECK_BETTER1(v, r, c)                                                \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {                     \
    MV this_mv = { r, c };                                                    \
    thismse = upsampled_obmc_pref_error(xd, cm, mi_row, mi_col, &this_mv,     \
                                        mask, vfp, z, pre(y, y_stride, r, c), \
                                        y_stride, sp(c), sp(r), w, h, &sse);  \
    if ((v = MVC(r, c) + thismse) < besterr) {                                \
      besterr = v;                                                            \
      br = r;                                                                 \
      bc = c;                                                                 \
      *distortion = thismse;                                                  \
      *sse1 = sse;                                                            \
    }                                                                         \
  } else {                                                                    \
    v = INT_MAX;                                                              \
  }

static unsigned int setup_obmc_center_error(
    const int32_t *mask, const MV *bestmv, const MV *ref_mv, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, const int32_t *const wsrc,
    const uint8_t *const y, int y_stride, int offset, int *mvjcost,
    int *mvcost[2], unsigned int *sse1, int *distortion) {
  unsigned int besterr;
  besterr = vfp->ovf(y + offset, y_stride, wsrc, mask, sse1);
  *distortion = besterr;
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);
  return besterr;
}

static int upsampled_obmc_pref_error(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *const mv, const int32_t *mask, const aom_variance_fn_ptr_t *vfp,
    const int32_t *const wsrc, const uint8_t *const y, int y_stride,
    int subpel_x_q3, int subpel_y_q3, int w, int h, unsigned int *sse) {
  unsigned int besterr;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    DECLARE_ALIGNED(16, uint16_t, pred16[MAX_SB_SQUARE]);
    aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred16, w, h,
                              subpel_x_q3, subpel_y_q3, y, y_stride, xd->bd);

    besterr = vfp->ovf(CONVERT_TO_BYTEPTR(pred16), w, wsrc, mask, sse);
  } else {
    DECLARE_ALIGNED(16, uint8_t, pred[MAX_SB_SQUARE]);
    aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred, w, h, subpel_x_q3,
                       subpel_y_q3, y, y_stride);

    besterr = vfp->ovf(pred, w, wsrc, mask, sse);
  }
  return besterr;
}

static unsigned int upsampled_setup_obmc_center_error(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const int32_t *mask, const MV *bestmv, const MV *ref_mv, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, const int32_t *const wsrc,
    const uint8_t *const y, int y_stride, int w, int h, int offset,
    int *mvjcost, int *mvcost[2], unsigned int *sse1, int *distortion) {
  unsigned int besterr =
      upsampled_obmc_pref_error(xd, cm, mi_row, mi_col, bestmv, mask, vfp, wsrc,
                                y + offset, y_stride, 0, 0, w, h, sse1);
  *distortion = besterr;
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);
  return besterr;
}

int av1_find_best_obmc_sub_pixel_tree_up(
    MACROBLOCK *x, const AV1_COMMON *const cm, int mi_row, int mi_col,
    MV *bestmv, const MV *ref_mv, int allow_hp, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, int forced_stop, int iters_per_step,
    int *mvjcost, int *mvcost[2], int *distortion, unsigned int *sse1,
    int is_second, int use_accurate_subpel_search) {
  const int32_t *wsrc = x->wsrc_buf;
  const int32_t *mask = x->mask_buf;
  const int *const z = wsrc;
  const int *const src_address = z;
  MACROBLOCKD *xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[0];
  MB_MODE_INFO *mbmi = xd->mi[0];
  unsigned int besterr = INT_MAX;
  unsigned int sse;
  unsigned int thismse;

  int rr = ref_mv->row;
  int rc = ref_mv->col;
  int br = bestmv->row * 8;
  int bc = bestmv->col * 8;
  int hstep = 4;
  int iter;
  int round = 3 - forced_stop;
  int tr = br;
  int tc = bc;
  const MV *search_step = search_step_table;
  int idx, best_idx = -1;
  unsigned int cost_array[5];
  int kr, kc;
  const int w = block_size_wide[mbmi->sb_type];
  const int h = block_size_high[mbmi->sb_type];
  int offset;
  int y_stride;
  const uint8_t *y;

  int minc, maxc, minr, maxr;

  set_subpel_mv_search_range(&x->mv_limits, &minc, &maxc, &minr, &maxr, ref_mv);

  y = pd->pre[is_second].buf;
  y_stride = pd->pre[is_second].stride;
  offset = bestmv->row * y_stride + bestmv->col;

  if (!allow_hp)
    if (round == 3) round = 2;

  bestmv->row *= 8;
  bestmv->col *= 8;
  // use_accurate_subpel_search can be 0 or 1
  if (use_accurate_subpel_search)
    besterr = upsampled_setup_obmc_center_error(
        xd, cm, mi_row, mi_col, mask, bestmv, ref_mv, error_per_bit, vfp, z, y,
        y_stride, w, h, offset, mvjcost, mvcost, sse1, distortion);
  else
    besterr = setup_obmc_center_error(mask, bestmv, ref_mv, error_per_bit, vfp,
                                      z, y, y_stride, offset, mvjcost, mvcost,
                                      sse1, distortion);

  for (iter = 0; iter < round; ++iter) {
    // Check vertical and horizontal sub-pixel positions.
    for (idx = 0; idx < 4; ++idx) {
      tr = br + search_step[idx].row;
      tc = bc + search_step[idx].col;
      if (tc >= minc && tc <= maxc && tr >= minr && tr <= maxr) {
        MV this_mv = { tr, tc };
        if (use_accurate_subpel_search) {
          thismse = upsampled_obmc_pref_error(
              xd, cm, mi_row, mi_col, &this_mv, mask, vfp, src_address,
              pre(y, y_stride, tr, tc), y_stride, sp(tc), sp(tr), w, h, &sse);
        } else {
          thismse = vfp->osvf(pre(y, y_stride, tr, tc), y_stride, sp(tc),
                              sp(tr), src_address, mask, &sse);
        }

        cost_array[idx] = thismse + mv_err_cost(&this_mv, ref_mv, mvjcost,
                                                mvcost, error_per_bit);
        if (cost_array[idx] < besterr) {
          best_idx = idx;
          besterr = cost_array[idx];
          *distortion = thismse;
          *sse1 = sse;
        }
      } else {
        cost_array[idx] = INT_MAX;
      }
    }

    // Check diagonal sub-pixel position
    kc = (cost_array[0] <= cost_array[1] ? -hstep : hstep);
    kr = (cost_array[2] <= cost_array[3] ? -hstep : hstep);

    tc = bc + kc;
    tr = br + kr;
    if (tc >= minc && tc <= maxc && tr >= minr && tr <= maxr) {
      MV this_mv = { tr, tc };

      if (use_accurate_subpel_search) {
        thismse = upsampled_obmc_pref_error(
            xd, cm, mi_row, mi_col, &this_mv, mask, vfp, src_address,
            pre(y, y_stride, tr, tc), y_stride, sp(tc), sp(tr), w, h, &sse);
      } else {
        thismse = vfp->osvf(pre(y, y_stride, tr, tc), y_stride, sp(tc), sp(tr),
                            src_address, mask, &sse);
      }

      cost_array[4] = thismse + mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost,
                                            error_per_bit);

      if (cost_array[4] < besterr) {
        best_idx = 4;
        besterr = cost_array[4];
        *distortion = thismse;
        *sse1 = sse;
      }
    } else {
      cost_array[idx] = INT_MAX;
    }

    if (best_idx < 4 && best_idx >= 0) {
      br += search_step[best_idx].row;
      bc += search_step[best_idx].col;
    } else if (best_idx == 4) {
      br = tr;
      bc = tc;
    }

    if (iters_per_step > 1 && best_idx != -1) {
      if (use_accurate_subpel_search) {
        SECOND_LEVEL_CHECKS_BEST(1);
      } else {
        SECOND_LEVEL_CHECKS_BEST(0);
      }
    }

    tr = br;
    tc = bc;

    search_step += 4;
    hstep >>= 1;
    best_idx = -1;
  }

  // These lines insure static analysis doesn't warn that
  // tr and tc aren't used after the above point.
  (void)tr;
  (void)tc;

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

#undef DIST
#undef MVC
#undef CHECK_BETTER

static int get_obmc_mvpred_var(const MACROBLOCK *x, const int32_t *wsrc,
                               const int32_t *mask, const MV *best_mv,
                               const MV *center_mv,
                               const aom_variance_fn_ptr_t *vfp, int use_mvcost,
                               int is_second) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const in_what = &xd->plane[0].pre[is_second];
  const MV mv = { best_mv->row * 8, best_mv->col * 8 };
  unsigned int unused;

  return vfp->ovf(get_buf_from_mv(in_what, best_mv), in_what->stride, wsrc,
                  mask, &unused) +
         (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost,
                                   x->errorperbit)
                     : 0);
}

int obmc_refining_search_sad(const MACROBLOCK *x, const int32_t *wsrc,
                             const int32_t *mask, MV *ref_mv, int error_per_bit,
                             int search_range,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const MV *center_mv, int is_second) {
  const MV neighbors[4] = { { -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, 0 } };
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const in_what = &xd->plane[0].pre[is_second];
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  unsigned int best_sad = fn_ptr->osdf(get_buf_from_mv(in_what, ref_mv),
                                       in_what->stride, wsrc, mask) +
                          mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);
  int i, j;

  for (i = 0; i < search_range; i++) {
    int best_site = -1;

    for (j = 0; j < 4; j++) {
      const MV mv = { ref_mv->row + neighbors[j].row,
                      ref_mv->col + neighbors[j].col };
      if (is_mv_in(&x->mv_limits, &mv)) {
        unsigned int sad = fn_ptr->osdf(get_buf_from_mv(in_what, &mv),
                                        in_what->stride, wsrc, mask);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            best_site = j;
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      ref_mv->row += neighbors[best_site].row;
      ref_mv->col += neighbors[best_site].col;
    }
  }
  return best_sad;
}

int obmc_diamond_search_sad(const MACROBLOCK *x, const search_site_config *cfg,
                            const int32_t *wsrc, const int32_t *mask,
                            MV *ref_mv, MV *best_mv, int search_param,
                            int sad_per_bit, int *num00,
                            const aom_variance_fn_ptr_t *fn_ptr,
                            const MV *center_mv, int is_second) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const in_what = &xd->plane[0].pre[is_second];
  // search_param determines the length of the initial step and hence the number
  // of iterations
  // 0 = initial step (MAX_FIRST_STEP) pel : 1 = (MAX_FIRST_STEP/2) pel, 2 =
  // (MAX_FIRST_STEP/4) pel... etc.
  const search_site *const ss = &cfg->ss[search_param * cfg->searches_per_step];
  const int tot_steps = (cfg->ss_count / cfg->searches_per_step) - search_param;
  const MV fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
  const uint8_t *best_address, *in_what_ref;
  int best_sad = INT_MAX;
  int best_site = 0;
  int last_site = 0;
  int i, j, step;

  clamp_mv(ref_mv, x->mv_limits.col_min, x->mv_limits.col_max,
           x->mv_limits.row_min, x->mv_limits.row_max);
  in_what_ref = in_what->buf + ref_mv->row * in_what->stride + ref_mv->col;
  best_address = in_what_ref;
  *num00 = 0;
  *best_mv = *ref_mv;

  // Check the starting position
  best_sad = fn_ptr->osdf(best_address, in_what->stride, wsrc, mask) +
             mvsad_err_cost(x, best_mv, &fcenter_mv, sad_per_bit);

  i = 1;

  for (step = 0; step < tot_steps; step++) {
    for (j = 0; j < cfg->searches_per_step; j++) {
      const MV mv = { best_mv->row + ss[i].mv.row,
                      best_mv->col + ss[i].mv.col };
      if (is_mv_in(&x->mv_limits, &mv)) {
        int sad = fn_ptr->osdf(best_address + ss[i].offset, in_what->stride,
                               wsrc, mask);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            best_site = i;
          }
        }
      }

      i++;
    }

    if (best_site != last_site) {
      best_mv->row += ss[best_site].mv.row;
      best_mv->col += ss[best_site].mv.col;
      best_address += ss[best_site].offset;
      last_site = best_site;
#if defined(NEW_DIAMOND_SEARCH)
      while (1) {
        const MV this_mv = { best_mv->row + ss[best_site].mv.row,
                             best_mv->col + ss[best_site].mv.col };
        if (is_mv_in(&x->mv_limits, &this_mv)) {
          int sad = fn_ptr->osdf(best_address + ss[best_site].offset,
                                 in_what->stride, wsrc, mask);
          if (sad < best_sad) {
            sad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              best_mv->row += ss[best_site].mv.row;
              best_mv->col += ss[best_site].mv.col;
              best_address += ss[best_site].offset;
              continue;
            }
          }
        }
        break;
      }
#endif
    } else if (best_address == in_what_ref) {
      (*num00)++;
    }
  }
  return best_sad;
}

int av1_obmc_full_pixel_diamond(const AV1_COMP *cpi, MACROBLOCK *x,
                                MV *mvp_full, int step_param, int sadpb,
                                int further_steps, int do_refine,
                                const aom_variance_fn_ptr_t *fn_ptr,
                                const MV *ref_mv, MV *dst_mv, int is_second) {
  const int32_t *wsrc = x->wsrc_buf;
  const int32_t *mask = x->mask_buf;
  MV temp_mv;
  int thissme, n, num00 = 0;
  int bestsme =
      obmc_diamond_search_sad(x, &cpi->ss_cfg, wsrc, mask, mvp_full, &temp_mv,
                              step_param, sadpb, &n, fn_ptr, ref_mv, is_second);
  if (bestsme < INT_MAX)
    bestsme = get_obmc_mvpred_var(x, wsrc, mask, &temp_mv, ref_mv, fn_ptr, 1,
                                  is_second);
  *dst_mv = temp_mv;

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  if (n > further_steps) do_refine = 0;

  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      thissme = obmc_diamond_search_sad(x, &cpi->ss_cfg, wsrc, mask, mvp_full,
                                        &temp_mv, step_param + n, sadpb, &num00,
                                        fn_ptr, ref_mv, is_second);
      if (thissme < INT_MAX)
        thissme = get_obmc_mvpred_var(x, wsrc, mask, &temp_mv, ref_mv, fn_ptr,
                                      1, is_second);

      // check to see if refining search is needed.
      if (num00 > further_steps - n) do_refine = 0;

      if (thissme < bestsme) {
        bestsme = thissme;
        *dst_mv = temp_mv;
      }
    }
  }

  // final 1-away diamond refining search
  if (do_refine) {
    const int search_range = 8;
    MV best_mv = *dst_mv;
    thissme = obmc_refining_search_sad(x, wsrc, mask, &best_mv, sadpb,
                                       search_range, fn_ptr, ref_mv, is_second);
    if (thissme < INT_MAX)
      thissme = get_obmc_mvpred_var(x, wsrc, mask, &best_mv, ref_mv, fn_ptr, 1,
                                    is_second);
    if (thissme < bestsme) {
      bestsme = thissme;
      *dst_mv = best_mv;
    }
  }
  return bestsme;
}

// Note(yunqingwang): The following 2 functions are only used in the motion
// vector unit test, which return extreme motion vectors allowed by the MV
// limits.
#define COMMON_MV_TEST              \
  SETUP_SUBPEL_SEARCH;              \
                                    \
  (void)error_per_bit;              \
  (void)vfp;                        \
  (void)src_address;                \
  (void)src_stride;                 \
  (void)y;                          \
  (void)y_stride;                   \
  (void)second_pred;                \
  (void)w;                          \
  (void)h;                          \
  (void)use_accurate_subpel_search; \
  (void)offset;                     \
  (void)mvjcost;                    \
  (void)mvcost;                     \
  (void)sse1;                       \
  (void)distortion;                 \
                                    \
  (void)halfiters;                  \
  (void)quarteriters;               \
  (void)eighthiters;                \
  (void)whichdir;                   \
  (void)forced_stop;                \
  (void)hstep;                      \
                                    \
  (void)tr;                         \
  (void)tc;                         \
  (void)sse;                        \
  (void)thismse;                    \
  (void)cost_list;
// Return the maximum MV.
int av1_return_max_sub_pixel_mv(MACROBLOCK *x, const AV1_COMMON *const cm,
                                int mi_row, int mi_col, const MV *ref_mv,
                                int allow_hp, int error_per_bit,
                                const aom_variance_fn_ptr_t *vfp,
                                int forced_stop, int iters_per_step,
                                int *cost_list, int *mvjcost, int *mvcost[2],
                                int *distortion, unsigned int *sse1,
                                const uint8_t *second_pred, const uint8_t *mask,
                                int mask_stride, int invert_mask, int w, int h,
                                int use_accurate_subpel_search) {
  COMMON_MV_TEST;
  (void)mask;
  (void)mask_stride;
  (void)invert_mask;
  (void)minr;
  (void)minc;

  (void)cm;
  (void)mi_row;
  (void)mi_col;

  bestmv->row = maxr;
  bestmv->col = maxc;
  besterr = 0;
  // In the sub-pel motion search, if hp is not used, then the last bit of mv
  // has to be 0.
  lower_mv_precision(bestmv, allow_hp, 0);
  return besterr;
}
// Return the minimum MV.
int av1_return_min_sub_pixel_mv(MACROBLOCK *x, const AV1_COMMON *const cm,
                                int mi_row, int mi_col, const MV *ref_mv,
                                int allow_hp, int error_per_bit,
                                const aom_variance_fn_ptr_t *vfp,
                                int forced_stop, int iters_per_step,
                                int *cost_list, int *mvjcost, int *mvcost[2],
                                int *distortion, unsigned int *sse1,
                                const uint8_t *second_pred, const uint8_t *mask,
                                int mask_stride, int invert_mask, int w, int h,
                                int use_accurate_subpel_search) {
  COMMON_MV_TEST;
  (void)maxr;
  (void)maxc;
  (void)mask;
  (void)mask_stride;
  (void)invert_mask;

  (void)cm;
  (void)mi_row;
  (void)mi_col;

  bestmv->row = minr;
  bestmv->col = minc;
  besterr = 0;
  // In the sub-pel motion search, if hp is not used, then the last bit of mv
  // has to be 0.
  lower_mv_precision(bestmv, allow_hp, 0);
  return besterr;
}
