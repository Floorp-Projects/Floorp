/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "./vpx_config.h"

#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_common.h"

#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_mcomp.h"

// #define NEW_DIAMOND_SEARCH

void vp9_set_mv_search_range(MACROBLOCK *x, const MV *mv) {
  int col_min = (mv->col >> 3) - MAX_FULL_PEL_VAL + (mv->col & 7 ? 1 : 0);
  int row_min = (mv->row >> 3) - MAX_FULL_PEL_VAL + (mv->row & 7 ? 1 : 0);
  int col_max = (mv->col >> 3) + MAX_FULL_PEL_VAL;
  int row_max = (mv->row >> 3) + MAX_FULL_PEL_VAL;

  col_min = MAX(col_min, (MV_LOW >> 3) + 1);
  row_min = MAX(row_min, (MV_LOW >> 3) + 1);
  col_max = MIN(col_max, (MV_UPP >> 3) - 1);
  row_max = MIN(row_max, (MV_UPP >> 3) - 1);

  // Get intersection of UMV window and valid MV window to reduce # of checks
  // in diamond search.
  if (x->mv_col_min < col_min)
    x->mv_col_min = col_min;
  if (x->mv_col_max > col_max)
    x->mv_col_max = col_max;
  if (x->mv_row_min < row_min)
    x->mv_row_min = row_min;
  if (x->mv_row_max > row_max)
    x->mv_row_max = row_max;
}

int vp9_init_search_range(VP9_COMP *cpi, int size) {
  int sr = 0;

  // Minimum search size no matter what the passed in value.
  size = MAX(16, size);

  while ((size << sr) < MAX_FULL_PEL_VAL)
    sr++;

  sr += cpi->sf.reduce_first_step_size;
  sr = MIN(sr, (cpi->sf.max_step_search_steps - 2));
  return sr;
}

static INLINE int mv_cost(const MV *mv,
                          const int *joint_cost, int *comp_cost[2]) {
  return joint_cost[vp9_get_mv_joint(mv)] +
             comp_cost[0][mv->row] + comp_cost[1][mv->col];
}

int vp9_mv_bit_cost(const MV *mv, const MV *ref,
                    const int *mvjcost, int *mvcost[2], int weight) {
  const MV diff = { mv->row - ref->row,
                    mv->col - ref->col };
  return ROUND_POWER_OF_TWO(mv_cost(&diff, mvjcost, mvcost) * weight, 7);
}

static int mv_err_cost(const MV *mv, const MV *ref,
                       const int *mvjcost, int *mvcost[2],
                       int error_per_bit) {
  if (mvcost) {
    const MV diff = { mv->row - ref->row,
                      mv->col - ref->col };
    return ROUND_POWER_OF_TWO(mv_cost(&diff, mvjcost, mvcost) *
                                  error_per_bit, 13);
  }
  return 0;
}

static int mvsad_err_cost(const MV *mv, const MV *ref,
                          const int *mvjsadcost, int *mvsadcost[2],
                          int error_per_bit) {
  if (mvsadcost) {
    const MV diff = { mv->row - ref->row,
                      mv->col - ref->col };
    return ROUND_POWER_OF_TWO(mv_cost(&diff, mvjsadcost, mvsadcost) *
                                  error_per_bit, 8);
  }
  return 0;
}

void vp9_init_dsmotion_compensation(MACROBLOCK *x, int stride) {
  int len;
  int search_site_count = 0;

  // Generate offsets for 4 search sites per step.
  x->ss[search_site_count].mv.col = 0;
  x->ss[search_site_count].mv.row = 0;
  x->ss[search_site_count].offset = 0;
  search_site_count++;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Compute offsets for search sites.
    x->ss[search_site_count].mv.col = 0;
    x->ss[search_site_count].mv.row = -len;
    x->ss[search_site_count].offset = -len * stride;
    search_site_count++;

    // Compute offsets for search sites.
    x->ss[search_site_count].mv.col = 0;
    x->ss[search_site_count].mv.row = len;
    x->ss[search_site_count].offset = len * stride;
    search_site_count++;

    // Compute offsets for search sites.
    x->ss[search_site_count].mv.col = -len;
    x->ss[search_site_count].mv.row = 0;
    x->ss[search_site_count].offset = -len;
    search_site_count++;

    // Compute offsets for search sites.
    x->ss[search_site_count].mv.col = len;
    x->ss[search_site_count].mv.row = 0;
    x->ss[search_site_count].offset = len;
    search_site_count++;
  }

  x->ss_count = search_site_count;
  x->searches_per_step = 4;
}

void vp9_init3smotion_compensation(MACROBLOCK *x, int stride) {
  int len, ss_count = 1;

  x->ss[0].mv.col = x->ss[0].mv.row = 0;
  x->ss[0].offset = 0;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Generate offsets for 8 search sites per step.
    const MV ss_mvs[8] = {
      {-len,  0  }, {len,  0  }, { 0,   -len}, {0,    len},
      {-len, -len}, {-len, len}, {len,  -len}, {len,  len}
    };
    int i;
    for (i = 0; i < 8; ++i) {
      search_site *const ss = &x->ss[ss_count++];
      ss->mv = ss_mvs[i];
      ss->offset = ss->mv.row * stride + ss->mv.col;
    }
  }

  x->ss_count = ss_count;
  x->searches_per_step = 8;
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

/* estimated cost of a motion vector (r,c) */
#define MVC(r, c)                                       \
    (mvcost ?                                           \
     ((mvjcost[((r) != rr) * 2 + ((c) != rc)] +         \
       mvcost[0][((r) - rr)] + mvcost[1][((c) - rc)]) * \
      error_per_bit + 4096) >> 13 : 0)


// convert motion vector component to offset for svf calc
static INLINE int sp(int x) {
  return (x & 7) << 1;
}

static INLINE const uint8_t *pre(const uint8_t *buf, int stride, int r, int c,
                                 int offset) {
  return &buf[(r >> 3) * stride + (c >> 3) - offset];
}

/* returns subpixel variance error function */
#define DIST(r, c) \
    vfp->svf(pre(y, y_stride, r, c, offset), y_stride, sp(c), sp(r), z, \
             src_stride, &sse)

/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER(v, r, c) \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {              \
    thismse = (DIST(r, c));                                            \
    if ((v = MVC(r, c) + thismse) < besterr) {                         \
      besterr = v;                                                     \
      br = r;                                                          \
      bc = c;                                                          \
      *distortion = thismse;                                           \
      *sse1 = sse;                                                     \
    }                                                                  \
  } else {                                                             \
    v = INT_MAX;                                                       \
  }

#define FIRST_LEVEL_CHECKS                              \
  {                                                     \
    unsigned int left, right, up, down, diag;           \
    CHECK_BETTER(left, tr, tc - hstep);                 \
    CHECK_BETTER(right, tr, tc + hstep);                \
    CHECK_BETTER(up, tr - hstep, tc);                   \
    CHECK_BETTER(down, tr + hstep, tc);                 \
    whichdir = (left < right ? 0 : 1) +                 \
               (up < down ? 0 : 2);                     \
    switch (whichdir) {                                 \
      case 0:                                           \
        CHECK_BETTER(diag, tr - hstep, tc - hstep);     \
        break;                                          \
      case 1:                                           \
        CHECK_BETTER(diag, tr - hstep, tc + hstep);     \
        break;                                          \
      case 2:                                           \
        CHECK_BETTER(diag, tr + hstep, tc - hstep);     \
        break;                                          \
      case 3:                                           \
        CHECK_BETTER(diag, tr + hstep, tc + hstep);     \
        break;                                          \
    }                                                   \
  }

#define SECOND_LEVEL_CHECKS                             \
  {                                                     \
    int kr, kc;                                         \
    unsigned int second;                                \
    if (tr != br && tc != bc) {                         \
      kr = br - tr;                                     \
      kc = bc - tc;                                     \
      CHECK_BETTER(second, tr + kr, tc + 2 * kc);       \
      CHECK_BETTER(second, tr + 2 * kr, tc + kc);       \
    } else if (tr == br && tc != bc) {                  \
      kc = bc - tc;                                     \
      CHECK_BETTER(second, tr + hstep, tc + 2 * kc);    \
      CHECK_BETTER(second, tr - hstep, tc + 2 * kc);    \
      switch (whichdir) {                               \
        case 0:                                         \
        case 1:                                         \
          CHECK_BETTER(second, tr + hstep, tc + kc);    \
          break;                                        \
        case 2:                                         \
        case 3:                                         \
          CHECK_BETTER(second, tr - hstep, tc + kc);    \
          break;                                        \
      }                                                 \
    } else if (tr != br && tc == bc) {                  \
      kr = br - tr;                                     \
      CHECK_BETTER(second, tr + 2 * kr, tc + hstep);    \
      CHECK_BETTER(second, tr + 2 * kr, tc - hstep);    \
      switch (whichdir) {                               \
        case 0:                                         \
        case 2:                                         \
          CHECK_BETTER(second, tr + kr, tc + hstep);    \
          break;                                        \
        case 1:                                         \
        case 3:                                         \
          CHECK_BETTER(second, tr + kr, tc - hstep);    \
          break;                                        \
      }                                                 \
    }                                                   \
  }

int vp9_find_best_sub_pixel_tree(const MACROBLOCK *x,
                                 MV *bestmv, const MV *ref_mv,
                                 int allow_hp,
                                 int error_per_bit,
                                 const vp9_variance_fn_ptr_t *vfp,
                                 int forced_stop,
                                 int iters_per_step,
                                 int *mvjcost, int *mvcost[2],
                                 int *distortion,
                                 unsigned int *sse1) {
  const uint8_t *z = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const MACROBLOCKD *xd = &x->e_mbd;
  unsigned int besterr = INT_MAX;
  unsigned int sse;
  unsigned int whichdir;
  int thismse;
  unsigned int halfiters = iters_per_step;
  unsigned int quarteriters = iters_per_step;
  unsigned int eighthiters = iters_per_step;

  const int y_stride = xd->plane[0].pre[0].stride;
  const int offset = bestmv->row * y_stride + bestmv->col;
  const uint8_t *y = xd->plane[0].pre[0].buf + offset;

  int rr = ref_mv->row;
  int rc = ref_mv->col;
  int br = bestmv->row * 8;
  int bc = bestmv->col * 8;
  int hstep = 4;
  const int minc = MAX(x->mv_col_min * 8, ref_mv->col - MV_MAX);
  const int maxc = MIN(x->mv_col_max * 8, ref_mv->col + MV_MAX);
  const int minr = MAX(x->mv_row_min * 8, ref_mv->row - MV_MAX);
  const int maxr = MIN(x->mv_row_max * 8, ref_mv->row + MV_MAX);

  int tr = br;
  int tc = bc;

  // central mv
  bestmv->row *= 8;
  bestmv->col *= 8;

  // calculate central point error
  besterr = vfp->vf(y, y_stride, z, src_stride, sse1);
  *distortion = besterr;
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);

  // 1/2 pel
  FIRST_LEVEL_CHECKS;
  if (halfiters > 1) {
    SECOND_LEVEL_CHECKS;
  }
  tr = br;
  tc = bc;

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

  if (allow_hp && vp9_use_mv_hp(ref_mv) && forced_stop == 0) {
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
  (void) tr;
  (void) tc;

  bestmv->row = br;
  bestmv->col = bc;

  if ((abs(bestmv->col - ref_mv->col) > (MAX_FULL_PEL_VAL << 3)) ||
      (abs(bestmv->row - ref_mv->row) > (MAX_FULL_PEL_VAL << 3)))
    return INT_MAX;

  return besterr;
}

#undef DIST
/* returns subpixel variance error function */
#define DIST(r, c) \
    vfp->svaf(pre(y, y_stride, r, c, offset), y_stride, sp(c), sp(r), \
              z, src_stride, &sse, second_pred)

int vp9_find_best_sub_pixel_comp_tree(const MACROBLOCK *x,
                                      MV *bestmv, const MV *ref_mv,
                                      int allow_hp,
                                      int error_per_bit,
                                      const vp9_variance_fn_ptr_t *vfp,
                                      int forced_stop,
                                      int iters_per_step,
                                      int *mvjcost, int *mvcost[2],
                                      int *distortion,
                                      unsigned int *sse1,
                                      const uint8_t *second_pred,
                                      int w, int h) {
  const uint8_t *z = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const MACROBLOCKD *xd = &x->e_mbd;
  unsigned int besterr = INT_MAX;
  unsigned int sse;
  unsigned int whichdir;
  int thismse;
  unsigned int halfiters = iters_per_step;
  unsigned int quarteriters = iters_per_step;
  unsigned int eighthiters = iters_per_step;

  DECLARE_ALIGNED_ARRAY(16, uint8_t, comp_pred, 64 * 64);
  const int y_stride = xd->plane[0].pre[0].stride;
  const int offset = bestmv->row * y_stride + bestmv->col;
  const uint8_t *y = xd->plane[0].pre[0].buf + offset;

  int rr = ref_mv->row;
  int rc = ref_mv->col;
  int br = bestmv->row * 8;
  int bc = bestmv->col * 8;
  int hstep = 4;
  const int minc = MAX(x->mv_col_min * 8, ref_mv->col - MV_MAX);
  const int maxc = MIN(x->mv_col_max * 8, ref_mv->col + MV_MAX);
  const int minr = MAX(x->mv_row_min * 8, ref_mv->row - MV_MAX);
  const int maxr = MIN(x->mv_row_max * 8, ref_mv->row + MV_MAX);

  int tr = br;
  int tc = bc;

  // central mv
  bestmv->row *= 8;
  bestmv->col *= 8;

  // calculate central point error
  // TODO(yunqingwang): central pointer error was already calculated in full-
  // pixel search, and can be passed in this function.
  comp_avg_pred(comp_pred, second_pred, w, h, y, y_stride);
  besterr = vfp->vf(comp_pred, w, z, src_stride, sse1);
  *distortion = besterr;
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);

  // Each subsequent iteration checks at least one point in
  // common with the last iteration could be 2 ( if diag selected)
  // 1/2 pel
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
    tr = br;
    tc = bc;
  }

  if (allow_hp && vp9_use_mv_hp(ref_mv) && forced_stop == 0) {
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
  (void) tr;
  (void) tc;

  bestmv->row = br;
  bestmv->col = bc;

  if ((abs(bestmv->col - ref_mv->col) > (MAX_FULL_PEL_VAL << 3)) ||
      (abs(bestmv->row - ref_mv->row) > (MAX_FULL_PEL_VAL << 3)))
    return INT_MAX;

  return besterr;
}

#undef MVC
#undef PRE
#undef DIST
#undef CHECK_BETTER

static INLINE int check_bounds(const MACROBLOCK *x, int row, int col,
                               int range) {
  return ((row - range) >= x->mv_row_min) &
         ((row + range) <= x->mv_row_max) &
         ((col - range) >= x->mv_col_min) &
         ((col + range) <= x->mv_col_max);
}

static INLINE int is_mv_in(const MACROBLOCK *x, const MV *mv) {
  return (mv->col >= x->mv_col_min) && (mv->col <= x->mv_col_max) &&
         (mv->row >= x->mv_row_min) && (mv->row <= x->mv_row_max);
}

#define CHECK_BETTER \
  {\
    if (thissad < bestsad) {\
      if (use_mvcost) \
        thissad += mvsad_err_cost(&this_mv, &fcenter_mv, \
                                  mvjsadcost, mvsadcost, sad_per_bit);\
      if (thissad < bestsad) {\
        bestsad = thissad;\
        best_site = i;\
      }\
    }\
  }

#define MAX_PATTERN_SCALES         11
#define MAX_PATTERN_CANDIDATES      8  // max number of canddiates per scale
#define PATTERN_CANDIDATES_REF      3  // number of refinement candidates

// Generic pattern search function that searches over multiple scales.
// Each scale can have a different number of candidates and shape of
// candidates as indicated in the num_candidates and candidates arrays
// passed into this function
static int vp9_pattern_search(const MACROBLOCK *x,
                              MV *ref_mv,
                              int search_param,
                              int sad_per_bit,
                              int do_init_search,
                              int do_refine,
                              const vp9_variance_fn_ptr_t *vfp,
                              int use_mvcost,
                              const MV *center_mv, MV *best_mv,
                              const int num_candidates[MAX_PATTERN_SCALES],
                              const MV candidates[MAX_PATTERN_SCALES]
                                                 [MAX_PATTERN_CANDIDATES]) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  static const int search_param_to_steps[MAX_MVSEARCH_STEPS] = {
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  int i, j, s, t;
  const uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  int br, bc;
  MV this_mv;
  int bestsad = INT_MAX;
  int thissad;
  const uint8_t *base_offset;
  const uint8_t *this_offset;
  int k = -1;
  int best_site = -1;
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  int best_init_s = search_param_to_steps[search_param];
  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  // adjust ref_mv to make sure it is within MV range
  clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
  br = ref_mv->row;
  bc = ref_mv->col;

  // Work out the start point for the search
  base_offset = xd->plane[0].pre[0].buf;
  this_offset = base_offset + (br * in_what_stride) + bc;
  this_mv.row = br;
  this_mv.col = bc;
  bestsad = vfp->sdf(what, what_stride, this_offset, in_what_stride, 0x7fffffff)
                + mvsad_err_cost(&this_mv, &fcenter_mv,
                                 mvjsadcost, mvsadcost, sad_per_bit);

  // Search all possible scales upto the search param around the center point
  // pick the scale of the point that is best as the starting scale of
  // further steps around it.
  if (do_init_search) {
    s = best_init_s;
    best_init_s = -1;
    for (t = 0; t <= s; ++t) {
      best_site = -1;
      if (check_bounds(x, br, bc, 1 << t)) {
        for (i = 0; i < num_candidates[t]; i++) {
          this_mv.row = br + candidates[t][i].row;
          this_mv.col = bc + candidates[t][i].col;
          this_offset = base_offset + (this_mv.row * in_what_stride) +
                                       this_mv.col;
          thissad = vfp->sdf(what, what_stride, this_offset, in_what_stride,
                             bestsad);
          CHECK_BETTER
        }
      } else {
        for (i = 0; i < num_candidates[t]; i++) {
          this_mv.row = br + candidates[t][i].row;
          this_mv.col = bc + candidates[t][i].col;
          if (!is_mv_in(x, &this_mv))
            continue;
          this_offset = base_offset + (this_mv.row * in_what_stride) +
                                       this_mv.col;
          thissad = vfp->sdf(what, what_stride, this_offset, in_what_stride,
                             bestsad);
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
    s = best_init_s;
    best_site = -1;
    do {
      // No need to search all 6 points the 1st time if initial search was used
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(x, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            this_mv.row = br + candidates[s][i].row;
            this_mv.col = bc + candidates[s][i].col;
            this_offset = base_offset + (this_mv.row * in_what_stride) +
                                         this_mv.col;
            thissad = vfp->sdf(what, what_stride, this_offset, in_what_stride,
                               bestsad);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            this_mv.row = br + candidates[s][i].row;
            this_mv.col = bc + candidates[s][i].col;
            if (!is_mv_in(x, &this_mv))
              continue;
            this_offset = base_offset + (this_mv.row * in_what_stride) +
                                         this_mv.col;
            thissad = vfp->sdf(what, what_stride, this_offset, in_what_stride,
                               bestsad);
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

        if (check_bounds(x, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            this_mv.row = br + candidates[s][next_chkpts_indices[i]].row;
            this_mv.col = bc + candidates[s][next_chkpts_indices[i]].col;
            this_offset = base_offset + (this_mv.row * (in_what_stride)) +
                                         this_mv.col;
            thissad = vfp->sdf(what, what_stride, this_offset, in_what_stride,
                               bestsad);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            this_mv.row = br + candidates[s][next_chkpts_indices[i]].row;
            this_mv.col = bc + candidates[s][next_chkpts_indices[i]].col;
            if (!is_mv_in(x, &this_mv))
              continue;
            this_offset = base_offset + (this_mv.row * (in_what_stride)) +
                                         this_mv.col;
            thissad = vfp->sdf(what, what_stride, this_offset, in_what_stride,
                               bestsad);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += candidates[s][k].row;
          bc += candidates[s][k].col;
        }
      } while (best_site != -1);
    } while (s--);
  }

  // Check 4 1-away neighbors if do_refine is true.
  // For most well-designed schemes do_refine will not be necessary.
  if (do_refine) {
    static const MV neighbors[4] = { {0, -1}, { -1, 0}, {1, 0}, {0, 1} };
    for (j = 0; j < 16; j++) {
      best_site = -1;
      if (check_bounds(x, br, bc, 1)) {
        for (i = 0; i < 4; i++) {
          this_mv.row = br + neighbors[i].row;
          this_mv.col = bc + neighbors[i].col;
          this_offset = base_offset + this_mv.row * in_what_stride +
                            this_mv.col;
          thissad = vfp->sdf(what, what_stride, this_offset, in_what_stride,
                             bestsad);
          CHECK_BETTER
        }
      } else {
        for (i = 0; i < 4; i++) {
          this_mv.row = br + neighbors[i].row;
          this_mv.col = bc + neighbors[i].col;
          if (!is_mv_in(x, &this_mv))
            continue;
          this_offset = base_offset + this_mv.row * in_what_stride +
                            this_mv.col;
          thissad = vfp->sdf(what, what_stride, this_offset, in_what_stride,
                             bestsad);
          CHECK_BETTER
        }
      }

      if (best_site == -1) {
        break;
      } else {
        br += neighbors[best_site].row;
        bc += neighbors[best_site].col;
      }
    }
  }

  best_mv->row = br;
  best_mv->col = bc;

  this_offset = base_offset + (best_mv->row * in_what_stride) +
                               best_mv->col;
  this_mv.row = best_mv->row * 8;
  this_mv.col = best_mv->col * 8;
  if (bestsad == INT_MAX)
    return INT_MAX;

  return vfp->vf(what, what_stride, this_offset, in_what_stride,
                 (unsigned int *)&bestsad) +
         use_mvcost ? mv_err_cost(&this_mv, center_mv,
                                  x->nmvjointcost, x->mvcost, x->errorperbit)
                    : 0;
}


int vp9_hex_search(const MACROBLOCK *x,
                   MV *ref_mv,
                   int search_param,
                   int sad_per_bit,
                   int do_init_search,
                   const vp9_variance_fn_ptr_t *vfp,
                   int use_mvcost,
                   const MV *center_mv, MV *best_mv) {
  // First scale has 8-closest points, the rest have 6 points in hex shape
  // at increasing scales
  static const int hex_num_candidates[MAX_PATTERN_SCALES] = {
    8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
  };
  // Note that the largest candidate step at each scale is 2^scale
  static const MV hex_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
    {{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, { 0, 1}, { -1, 1}, {-1, 0}},
    {{-1, -2}, {1, -2}, {2, 0}, {1, 2}, { -1, 2}, { -2, 0}},
    {{-2, -4}, {2, -4}, {4, 0}, {2, 4}, { -2, 4}, { -4, 0}},
    {{-4, -8}, {4, -8}, {8, 0}, {4, 8}, { -4, 8}, { -8, 0}},
    {{-8, -16}, {8, -16}, {16, 0}, {8, 16}, { -8, 16}, { -16, 0}},
    {{-16, -32}, {16, -32}, {32, 0}, {16, 32}, { -16, 32}, { -32, 0}},
    {{-32, -64}, {32, -64}, {64, 0}, {32, 64}, { -32, 64}, { -64, 0}},
    {{-64, -128}, {64, -128}, {128, 0}, {64, 128}, { -64, 128}, { -128, 0}},
    {{-128, -256}, {128, -256}, {256, 0}, {128, 256}, { -128, 256}, { -256, 0}},
    {{-256, -512}, {256, -512}, {512, 0}, {256, 512}, { -256, 512}, { -512, 0}},
    {{-512, -1024}, {512, -1024}, {1024, 0}, {512, 1024}, { -512, 1024},
      { -1024, 0}},
  };
  return vp9_pattern_search(x, ref_mv, search_param, sad_per_bit,
                            do_init_search, 0, vfp, use_mvcost,
                            center_mv, best_mv,
                            hex_num_candidates, hex_candidates);
}

int vp9_bigdia_search(const MACROBLOCK *x,
                      MV *ref_mv,
                      int search_param,
                      int sad_per_bit,
                      int do_init_search,
                      const vp9_variance_fn_ptr_t *vfp,
                      int use_mvcost,
                      const MV *center_mv,
                      MV *best_mv) {
  // First scale has 4-closest points, the rest have 8 points in diamond
  // shape at increasing scales
  static const int bigdia_num_candidates[MAX_PATTERN_SCALES] = {
    4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  // Note that the largest candidate step at each scale is 2^scale
  static const MV bigdia_candidates[MAX_PATTERN_SCALES]
                                   [MAX_PATTERN_CANDIDATES] = {
    {{0, -1}, {1, 0}, { 0, 1}, {-1, 0}},
    {{-1, -1}, {0, -2}, {1, -1}, {2, 0}, {1, 1}, {0, 2}, {-1, 1}, {-2, 0}},
    {{-2, -2}, {0, -4}, {2, -2}, {4, 0}, {2, 2}, {0, 4}, {-2, 2}, {-4, 0}},
    {{-4, -4}, {0, -8}, {4, -4}, {8, 0}, {4, 4}, {0, 8}, {-4, 4}, {-8, 0}},
    {{-8, -8}, {0, -16}, {8, -8}, {16, 0}, {8, 8}, {0, 16}, {-8, 8}, {-16, 0}},
    {{-16, -16}, {0, -32}, {16, -16}, {32, 0}, {16, 16}, {0, 32},
      {-16, 16}, {-32, 0}},
    {{-32, -32}, {0, -64}, {32, -32}, {64, 0}, {32, 32}, {0, 64},
      {-32, 32}, {-64, 0}},
    {{-64, -64}, {0, -128}, {64, -64}, {128, 0}, {64, 64}, {0, 128},
      {-64, 64}, {-128, 0}},
    {{-128, -128}, {0, -256}, {128, -128}, {256, 0}, {128, 128}, {0, 256},
      {-128, 128}, {-256, 0}},
    {{-256, -256}, {0, -512}, {256, -256}, {512, 0}, {256, 256}, {0, 512},
      {-256, 256}, {-512, 0}},
    {{-512, -512}, {0, -1024}, {512, -512}, {1024, 0}, {512, 512}, {0, 1024},
      {-512, 512}, {-1024, 0}},
  };
  return vp9_pattern_search(x, ref_mv, search_param, sad_per_bit,
                            do_init_search, 0, vfp, use_mvcost,
                            center_mv, best_mv,
                            bigdia_num_candidates, bigdia_candidates);
}

int vp9_square_search(const MACROBLOCK *x,
                      MV *ref_mv,
                      int search_param,
                      int sad_per_bit,
                      int do_init_search,
                      const vp9_variance_fn_ptr_t *vfp,
                      int use_mvcost,
                      const MV *center_mv,
                      MV *best_mv) {
  // All scales have 8 closest points in square shape
  static const int square_num_candidates[MAX_PATTERN_SCALES] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  // Note that the largest candidate step at each scale is 2^scale
  static const MV square_candidates[MAX_PATTERN_SCALES]
                                   [MAX_PATTERN_CANDIDATES] = {
    {{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}},
    {{-2, -2}, {0, -2}, {2, -2}, {2, 0}, {2, 2}, {0, 2}, {-2, 2}, {-2, 0}},
    {{-4, -4}, {0, -4}, {4, -4}, {4, 0}, {4, 4}, {0, 4}, {-4, 4}, {-4, 0}},
    {{-8, -8}, {0, -8}, {8, -8}, {8, 0}, {8, 8}, {0, 8}, {-8, 8}, {-8, 0}},
    {{-16, -16}, {0, -16}, {16, -16}, {16, 0}, {16, 16}, {0, 16},
      {-16, 16}, {-16, 0}},
    {{-32, -32}, {0, -32}, {32, -32}, {32, 0}, {32, 32}, {0, 32},
      {-32, 32}, {-32, 0}},
    {{-64, -64}, {0, -64}, {64, -64}, {64, 0}, {64, 64}, {0, 64},
      {-64, 64}, {-64, 0}},
    {{-128, -128}, {0, -128}, {128, -128}, {128, 0}, {128, 128}, {0, 128},
      {-128, 128}, {-128, 0}},
    {{-256, -256}, {0, -256}, {256, -256}, {256, 0}, {256, 256}, {0, 256},
      {-256, 256}, {-256, 0}},
    {{-512, -512}, {0, -512}, {512, -512}, {512, 0}, {512, 512}, {0, 512},
      {-512, 512}, {-512, 0}},
    {{-1024, -1024}, {0, -1024}, {1024, -1024}, {1024, 0}, {1024, 1024},
      {0, 1024}, {-1024, 1024}, {-1024, 0}},
  };
  return vp9_pattern_search(x, ref_mv, search_param, sad_per_bit,
                            do_init_search, 0, vfp, use_mvcost,
                            center_mv, best_mv,
                            square_num_candidates, square_candidates);
};

#undef CHECK_BETTER

int vp9_full_range_search_c(const MACROBLOCK *x, MV *ref_mv, MV *best_mv,
                            int search_param, int sad_per_bit, int *num00,
                            const vp9_variance_fn_ptr_t *fn_ptr,
                            int *mvjcost, int *mvcost[2],
                            const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *best_address;

  MV this_mv;

  int bestsad = INT_MAX;
  int ref_row, ref_col;

  int thissad;
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};

  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  int tr, tc;
  int best_tr = 0;
  int best_tc = 0;
  int range = 64;

  int start_col, end_col;
  int start_row, end_row;
  int i;

  clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
  ref_row = ref_mv->row;
  ref_col = ref_mv->col;
  *num00 = 11;
  best_mv->row = ref_row;
  best_mv->col = ref_col;

  // Work out the start point for the search
  in_what = xd->plane[0].pre[0].buf + ref_row * in_what_stride + ref_col;
  best_address = in_what;

  // Check the starting position
  bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride, 0x7fffffff)
                + mvsad_err_cost(best_mv, &fcenter_mv,
                                 mvjsadcost, mvsadcost, sad_per_bit);

  start_row = MAX(-range, x->mv_row_min - ref_row);
  start_col = MAX(-range, x->mv_col_min - ref_col);
  end_row = MIN(range, x->mv_row_max - ref_row);
  end_col = MIN(range, x->mv_col_max - ref_col);

  for (tr = start_row; tr <= end_row; ++tr) {
    for (tc = start_col; tc <= end_col; tc += 4) {
      if ((tc + 3) <= end_col) {
        unsigned int sad_array[4];
        unsigned char const *addr_ref[4];
        for (i = 0; i < 4; ++i)
          addr_ref[i] = in_what + tr * in_what_stride + tc + i;

        fn_ptr->sdx4df(what, what_stride, addr_ref, in_what_stride, sad_array);

        for (i = 0; i < 4; ++i) {
          if (sad_array[i] < bestsad) {
            this_mv.row = ref_row + tr;
            this_mv.col = ref_col + tc + i;
            thissad = sad_array[i] +
                      mvsad_err_cost(&this_mv, &fcenter_mv,
                                      mvjsadcost, mvsadcost, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_tr = tr;
              best_tc = tc + i;
            }
          }
        }
      } else {
        for (i = 0; i < end_col - tc; ++i) {
          const uint8_t *check_here = in_what + tr * in_what_stride + tc + i;
          thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                                bestsad);

          if (thissad < bestsad) {
            this_mv.row = ref_row + tr;
            this_mv.col = ref_col + tc + i;
            thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                      mvjsadcost, mvsadcost, sad_per_bit);

            if (thissad < bestsad) {
              bestsad = thissad;
              best_tr = tr;
              best_tc = tc + i;
            }
          }
        }
      }
    }
  }

  best_mv->row += best_tr;
  best_mv->col += best_tc;

  this_mv.row = best_mv->row * 8;
  this_mv.col = best_mv->col * 8;

  if (bestsad == INT_MAX)
    return INT_MAX;

  return fn_ptr->vf(what, what_stride, best_address, in_what_stride,
                    (unsigned int *)(&thissad)) +
                       mv_err_cost(&this_mv, center_mv,
                                   mvjcost, mvcost, x->errorperbit);
}

int vp9_diamond_search_sad_c(const MACROBLOCK *x,
                             MV *ref_mv, MV *best_mv,
                             int search_param, int sad_per_bit, int *num00,
                             const vp9_variance_fn_ptr_t *fn_ptr,
                             int *mvjcost, int *mvcost[2],
                             const MV *center_mv) {
  int i, j, step;

  const MACROBLOCKD *const xd = &x->e_mbd;
  const uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *best_address;

  MV this_mv;

  int bestsad = INT_MAX;
  int best_site = 0;
  int last_site = 0;

  int ref_row, ref_col;
  int this_row_offset, this_col_offset;

  // search_param determines the length of the initial step and hence the number
  // of iterations
  // 0 = initial step (MAX_FIRST_STEP) pel : 1 = (MAX_FIRST_STEP/2) pel, 2 =
  // (MAX_FIRST_STEP/4) pel... etc.
  const search_site *const ss = &x->ss[search_param * x->searches_per_step];
  const int tot_steps = (x->ss_count / x->searches_per_step) - search_param;

  int thissad;
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};

  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
  ref_row = ref_mv->row;
  ref_col = ref_mv->col;
  *num00 = 0;
  best_mv->row = ref_row;
  best_mv->col = ref_col;

  // Work out the start point for the search
  in_what = xd->plane[0].pre[0].buf + ref_row * in_what_stride + ref_col;
  best_address = in_what;

  // Check the starting position
  bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride, 0x7fffffff)
                + mvsad_err_cost(best_mv, &fcenter_mv,
                                 mvjsadcost, mvsadcost, sad_per_bit);

  i = 1;

  for (step = 0; step < tot_steps; step++) {
    for (j = 0; j < x->searches_per_step; j++) {
      // Trap illegal vectors
      this_row_offset = best_mv->row + ss[i].mv.row;
      this_col_offset = best_mv->col + ss[i].mv.col;

      if ((this_col_offset > x->mv_col_min) &&
          (this_col_offset < x->mv_col_max) &&
          (this_row_offset > x->mv_row_min) &&
          (this_row_offset < x->mv_row_max)) {
        const uint8_t *const check_here = ss[i].offset + best_address;
        thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                              bestsad);

        if (thissad < bestsad) {
          this_mv.row = this_row_offset;
          this_mv.col = this_col_offset;
          thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                    mvjsadcost, mvsadcost, sad_per_bit);

          if (thissad < bestsad) {
            bestsad = thissad;
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
        this_row_offset = best_mv->row + ss[best_site].mv.row;
        this_col_offset = best_mv->col + ss[best_site].mv.col;
        if ((this_col_offset > x->mv_col_min) &&
            (this_col_offset < x->mv_col_max) &&
            (this_row_offset > x->mv_row_min) &&
            (this_row_offset < x->mv_row_max)) {
          check_here = ss[best_site].offset + best_address;
          thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                                bestsad);
          if (thissad < bestsad) {
            this_mv.row = this_row_offset;
            this_mv.col = this_col_offset;
            thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                      mvjsadcost, mvsadcost, sad_per_bit);
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
      };
#endif
    } else if (best_address == in_what) {
      (*num00)++;
    }
  }

  this_mv.row = best_mv->row * 8;
  this_mv.col = best_mv->col * 8;

  if (bestsad == INT_MAX)
    return INT_MAX;

  return fn_ptr->vf(what, what_stride, best_address, in_what_stride,
                    (unsigned int *)(&thissad)) +
                       mv_err_cost(&this_mv, center_mv,
                                   mvjcost, mvcost, x->errorperbit);
}

int vp9_diamond_search_sadx4(const MACROBLOCK *x,
                             MV *ref_mv, MV *best_mv, int search_param,
                             int sad_per_bit, int *num00,
                             const vp9_variance_fn_ptr_t *fn_ptr,
                             int *mvjcost, int *mvcost[2],
                             const MV *center_mv) {
  int i, j, step;

  const MACROBLOCKD *const xd = &x->e_mbd;
  uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *best_address;

  MV this_mv;

  unsigned int bestsad = INT_MAX;
  int best_site = 0;
  int last_site = 0;

  int ref_row;
  int ref_col;
  int this_row_offset;
  int this_col_offset;

  // search_param determines the length of the initial step and hence the number
  // of iterations.
  // 0 = initial step (MAX_FIRST_STEP) pel
  // 1 = (MAX_FIRST_STEP/2) pel,
  // 2 = (MAX_FIRST_STEP/4) pel...
  const search_site *ss = &x->ss[search_param * x->searches_per_step];
  const int tot_steps = (x->ss_count / x->searches_per_step) - search_param;

  unsigned int thissad;
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};

  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
  ref_row = ref_mv->row;
  ref_col = ref_mv->col;
  *num00 = 0;
  best_mv->row = ref_row;
  best_mv->col = ref_col;

  // Work out the start point for the search
  in_what = xd->plane[0].pre[0].buf + ref_row * in_what_stride + ref_col;
  best_address = in_what;

  // Check the starting position
  bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride, 0x7fffffff)
                + mvsad_err_cost(best_mv, &fcenter_mv,
                                 mvjsadcost, mvsadcost, sad_per_bit);

  i = 1;

  for (step = 0; step < tot_steps; step++) {
    int all_in = 1, t;

    // All_in is true if every one of the points we are checking are within
    // the bounds of the image.
    all_in &= ((best_mv->row + ss[i].mv.row) > x->mv_row_min);
    all_in &= ((best_mv->row + ss[i + 1].mv.row) < x->mv_row_max);
    all_in &= ((best_mv->col + ss[i + 2].mv.col) > x->mv_col_min);
    all_in &= ((best_mv->col + ss[i + 3].mv.col) < x->mv_col_max);

    // If all the pixels are within the bounds we don't check whether the
    // search point is valid in this loop,  otherwise we check each point
    // for validity..
    if (all_in) {
      unsigned int sad_array[4];

      for (j = 0; j < x->searches_per_step; j += 4) {
        unsigned char const *block_offset[4];

        for (t = 0; t < 4; t++)
          block_offset[t] = ss[i + t].offset + best_address;

        fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride,
                       sad_array);

        for (t = 0; t < 4; t++, i++) {
          if (sad_array[t] < bestsad) {
            this_mv.row = best_mv->row + ss[i].mv.row;
            this_mv.col = best_mv->col + ss[i].mv.col;
            sad_array[t] += mvsad_err_cost(&this_mv, &fcenter_mv,
                                           mvjsadcost, mvsadcost, sad_per_bit);

            if (sad_array[t] < bestsad) {
              bestsad = sad_array[t];
              best_site = i;
            }
          }
        }
      }
    } else {
      for (j = 0; j < x->searches_per_step; j++) {
        // Trap illegal vectors
        this_row_offset = best_mv->row + ss[i].mv.row;
        this_col_offset = best_mv->col + ss[i].mv.col;

        if ((this_col_offset > x->mv_col_min) &&
            (this_col_offset < x->mv_col_max) &&
            (this_row_offset > x->mv_row_min) &&
            (this_row_offset < x->mv_row_max)) {
          const uint8_t *const check_here = ss[i].offset + best_address;
          thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                                bestsad);

          if (thissad < bestsad) {
            this_mv.row = this_row_offset;
            this_mv.col = this_col_offset;
            thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                      mvjsadcost, mvsadcost, sad_per_bit);

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
      best_mv->row += ss[best_site].mv.row;
      best_mv->col += ss[best_site].mv.col;
      best_address += ss[best_site].offset;
      last_site = best_site;
#if defined(NEW_DIAMOND_SEARCH)
      while (1) {
        this_row_offset = best_mv->row + ss[best_site].mv.row;
        this_col_offset = best_mv->col + ss[best_site].mv.col;
        if ((this_col_offset > x->mv_col_min) &&
            (this_col_offset < x->mv_col_max) &&
            (this_row_offset > x->mv_row_min) &&
            (this_row_offset < x->mv_row_max)) {
          check_here = ss[best_site].offset + best_address;
          thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                                bestsad);
          if (thissad < bestsad) {
            this_mv.row = this_row_offset;
            this_mv.col = this_col_offset;
            thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                      mvjsadcost, mvsadcost, sad_per_bit);
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
      };
#endif
    } else if (best_address == in_what) {
      (*num00)++;
    }
  }

  this_mv.row = best_mv->row * 8;
  this_mv.col = best_mv->col * 8;

  if (bestsad == INT_MAX)
    return INT_MAX;

  return fn_ptr->vf(what, what_stride, best_address, in_what_stride,
                    (unsigned int *)(&thissad)) +
                    mv_err_cost(&this_mv, center_mv,
                                mvjcost, mvcost, x->errorperbit);
}

/* do_refine: If last step (1-away) of n-step search doesn't pick the center
              point as the best match, we will do a final 1-away diamond
              refining search  */

int vp9_full_pixel_diamond(VP9_COMP *cpi, MACROBLOCK *x,
                           MV *mvp_full, int step_param,
                           int sadpb, int further_steps, int do_refine,
                           const vp9_variance_fn_ptr_t *fn_ptr,
                           const MV *ref_mv, MV *dst_mv) {
  MV temp_mv;
  int thissme, n, num00 = 0;
  int bestsme = cpi->diamond_search_sad(x, mvp_full, &temp_mv,
                                        step_param, sadpb, &n,
                                        fn_ptr, x->nmvjointcost,
                                        x->mvcost, ref_mv);
  *dst_mv = temp_mv;

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  if (n > further_steps)
    do_refine = 0;

  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      thissme = cpi->diamond_search_sad(x, mvp_full, &temp_mv,
                                        step_param + n, sadpb, &num00,
                                        fn_ptr, x->nmvjointcost, x->mvcost,
                                        ref_mv);

      // check to see if refining search is needed.
      if (num00 > further_steps - n)
        do_refine = 0;

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
    thissme = cpi->refining_search_sad(x, &best_mv, sadpb, search_range,
                                       fn_ptr, x->nmvjointcost, x->mvcost,
                                       ref_mv);
    if (thissme < bestsme) {
      bestsme = thissme;
      *dst_mv = best_mv;
    }
  }

  return bestsme;
}

int vp9_full_search_sad_c(const MACROBLOCK *x, const MV *ref_mv,
                          int sad_per_bit, int distance,
                          const vp9_variance_fn_ptr_t *fn_ptr,
                          int *mvjcost, int *mvcost[2],
                          const MV *center_mv, int block) {
  int r, c;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const uint8_t *const what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *const in_what = xd->plane[0].pre[0].buf;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const int row_min = MAX(ref_mv->row - distance, x->mv_row_min);
  const int row_max = MIN(ref_mv->row + distance, x->mv_row_max);
  const int col_min = MAX(ref_mv->col - distance, x->mv_col_min);
  const int col_max = MIN(ref_mv->col + distance, x->mv_col_max);
  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  const uint8_t *best_address = &in_what[ref_mv->row * in_what_stride +
                                         ref_mv->col];
  int best_sad = fn_ptr->sdf(what, what_stride, best_address, in_what_stride,
                             0x7fffffff) +
      mvsad_err_cost(ref_mv, &fcenter_mv, mvjsadcost, mvsadcost, sad_per_bit);
  MV *best_mv = &xd->mi_8x8[0]->bmi[block].as_mv[0].as_mv;
  *best_mv = *ref_mv;

  for (r = row_min; r < row_max; ++r) {
    for (c = col_min; c < col_max; ++c) {
      const MV this_mv = {r, c};
      const uint8_t *check_here = &in_what[r * in_what_stride + c];
      const int sad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                                  best_sad) +
          mvsad_err_cost(&this_mv, &fcenter_mv,
                         mvjsadcost, mvsadcost, sad_per_bit);

      if (sad < best_sad) {
        best_sad = sad;
        *best_mv = this_mv;
        best_address = check_here;
      }
    }
  }

  if (best_sad < INT_MAX) {
    unsigned int unused;
    const MV mv = {best_mv->row * 8, best_mv->col * 8};
    return fn_ptr->vf(what, what_stride, best_address, in_what_stride, &unused)
                + mv_err_cost(&mv, center_mv, mvjcost, mvcost, x->errorperbit);
  } else {
    return INT_MAX;
  }
}

int vp9_full_search_sadx3(const MACROBLOCK *x, const MV *ref_mv,
                          int sad_per_bit, int distance,
                          const vp9_variance_fn_ptr_t *fn_ptr,
                          int *mvjcost, int *mvcost[2],
                          const MV *center_mv, int n) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const uint8_t *const what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *const in_what = xd->plane[0].pre[0].buf;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  MV *best_mv = &xd->mi_8x8[0]->bmi[n].as_mv[0].as_mv;
  MV this_mv;
  unsigned int bestsad = INT_MAX;
  int r, c;
  unsigned int thissad;
  int ref_row = ref_mv->row;
  int ref_col = ref_mv->col;

  // Apply further limits to prevent us looking using vectors that stretch
  // beyond the UMV border
  const int row_min = MAX(ref_row - distance, x->mv_row_min);
  const int row_max = MIN(ref_row + distance, x->mv_row_max);
  const int col_min = MAX(ref_col - distance, x->mv_col_min);
  const int col_max = MIN(ref_col + distance, x->mv_col_max);
  unsigned int sad_array[3];
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  // Work out the mid point for the search
  const uint8_t *bestaddress = &in_what[ref_row * in_what_stride + ref_col];

  best_mv->row = ref_row;
  best_mv->col = ref_col;

  // Baseline value at the centre
  bestsad = fn_ptr->sdf(what, what_stride,
                        bestaddress, in_what_stride, 0x7fffffff)
            + mvsad_err_cost(best_mv, &fcenter_mv,
                             mvjsadcost, mvsadcost, sad_per_bit);

  for (r = row_min; r < row_max; r++) {
    const uint8_t *check_here = &in_what[r * in_what_stride + col_min];
    this_mv.row = r;
    c = col_min;

    while ((c + 2) < col_max && fn_ptr->sdx3f != NULL) {
      int i;

      fn_ptr->sdx3f(what, what_stride, check_here, in_what_stride, sad_array);

      for (i = 0; i < 3; i++) {
        thissad = sad_array[i];

        if (thissad < bestsad) {
          this_mv.col = c;
          thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                    mvjsadcost, mvsadcost, sad_per_bit);

          if (thissad < bestsad) {
            bestsad = thissad;
            best_mv->row = r;
            best_mv->col = c;
            bestaddress = check_here;
          }
        }

        check_here++;
        c++;
      }
    }

    while (c < col_max) {
      thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                            bestsad);

      if (thissad < bestsad) {
        this_mv.col = c;
        thissad  += mvsad_err_cost(&this_mv, &fcenter_mv,
                                   mvjsadcost, mvsadcost, sad_per_bit);

        if (thissad < bestsad) {
          bestsad = thissad;
          best_mv->row = r;
          best_mv->col = c;
          bestaddress = check_here;
        }
      }

      check_here++;
      c++;
    }
  }

  this_mv.row = best_mv->row * 8;
  this_mv.col = best_mv->col * 8;

  if (bestsad < INT_MAX)
    return fn_ptr->vf(what, what_stride, bestaddress, in_what_stride,
                      (unsigned int *)(&thissad)) +
                      mv_err_cost(&this_mv, center_mv,
                                  mvjcost, mvcost, x->errorperbit);
  else
    return INT_MAX;
}

int vp9_full_search_sadx8(const MACROBLOCK *x, const MV *ref_mv,
                          int sad_per_bit, int distance,
                          const vp9_variance_fn_ptr_t *fn_ptr,
                          int *mvjcost, int *mvcost[2],
                          const MV *center_mv, int n) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const uint8_t *const what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *const in_what = xd->plane[0].pre[0].buf;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  MV *best_mv = &xd->mi_8x8[0]->bmi[n].as_mv[0].as_mv;
  MV this_mv;
  unsigned int bestsad = INT_MAX;
  int r, c;
  unsigned int thissad;
  int ref_row = ref_mv->row;
  int ref_col = ref_mv->col;

  // Apply further limits to prevent us looking using vectors that stretch
  // beyond the UMV border
  const int row_min = MAX(ref_row - distance, x->mv_row_min);
  const int row_max = MIN(ref_row + distance, x->mv_row_max);
  const int col_min = MAX(ref_col - distance, x->mv_col_min);
  const int col_max = MIN(ref_col + distance, x->mv_col_max);
  DECLARE_ALIGNED_ARRAY(16, uint32_t, sad_array8, 8);
  unsigned int sad_array[3];
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};

  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  // Work out the mid point for the search
  const uint8_t *bestaddress = &in_what[ref_row * in_what_stride + ref_col];

  best_mv->row = ref_row;
  best_mv->col = ref_col;

  // Baseline value at the center
  bestsad = fn_ptr->sdf(what, what_stride,
                        bestaddress, in_what_stride, 0x7fffffff)
            + mvsad_err_cost(best_mv, &fcenter_mv,
                             mvjsadcost, mvsadcost, sad_per_bit);

  for (r = row_min; r < row_max; r++) {
    const uint8_t *check_here = &in_what[r * in_what_stride + col_min];
    this_mv.row = r;
    c = col_min;

    while ((c + 7) < col_max) {
      int i;

      fn_ptr->sdx8f(what, what_stride, check_here, in_what_stride, sad_array8);

      for (i = 0; i < 8; i++) {
        thissad = (unsigned int)sad_array8[i];

        if (thissad < bestsad) {
          this_mv.col = c;
          thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                    mvjsadcost, mvsadcost, sad_per_bit);

          if (thissad < bestsad) {
            bestsad = thissad;
            best_mv->row = r;
            best_mv->col = c;
            bestaddress = check_here;
          }
        }

        check_here++;
        c++;
      }
    }

    while ((c + 2) < col_max && fn_ptr->sdx3f != NULL) {
      int i;

      fn_ptr->sdx3f(what, what_stride, check_here, in_what_stride, sad_array);

      for (i = 0; i < 3; i++) {
        thissad = sad_array[i];

        if (thissad < bestsad) {
          this_mv.col = c;
          thissad  += mvsad_err_cost(&this_mv, &fcenter_mv,
                                     mvjsadcost, mvsadcost, sad_per_bit);

          if (thissad < bestsad) {
            bestsad = thissad;
            best_mv->row = r;
            best_mv->col = c;
            bestaddress = check_here;
          }
        }

        check_here++;
        c++;
      }
    }

    while (c < col_max) {
      thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                            bestsad);

      if (thissad < bestsad) {
        this_mv.col = c;
        thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                  mvjsadcost, mvsadcost, sad_per_bit);

        if (thissad < bestsad) {
          bestsad = thissad;
          best_mv->row = r;
          best_mv->col = c;
          bestaddress = check_here;
        }
      }

      check_here++;
      c++;
    }
  }

  this_mv.row = best_mv->row * 8;
  this_mv.col = best_mv->col * 8;

  if (bestsad < INT_MAX)
    return fn_ptr->vf(what, what_stride, bestaddress, in_what_stride,
                      (unsigned int *)(&thissad)) +
                      mv_err_cost(&this_mv, center_mv,
                                  mvjcost, mvcost, x->errorperbit);
  else
    return INT_MAX;
}

int vp9_refining_search_sad_c(const MACROBLOCK *x,
                              MV *ref_mv, int error_per_bit,
                              int search_range,
                              const vp9_variance_fn_ptr_t *fn_ptr,
                              int *mvjcost, int *mvcost[2],
                              const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MV neighbors[4] = {{ -1, 0}, {0, -1}, {0, 1}, {1, 0}};
  int i, j;

  const int what_stride = x->plane[0].src.stride;
  const uint8_t *const what = x->plane[0].src.buf;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *const in_what = xd->plane[0].pre[0].buf;
  const uint8_t *best_address = &in_what[ref_mv->row * in_what_stride +
                                             ref_mv->col];
  unsigned int thissad;

  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  MV this_mv;

  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  unsigned int bestsad = fn_ptr->sdf(what, what_stride, best_address,
                                     in_what_stride, 0x7fffffff) +
      mvsad_err_cost(ref_mv, &fcenter_mv, mvjsadcost, mvsadcost, error_per_bit);

  for (i = 0; i < search_range; i++) {
    int best_site = -1;

    for (j = 0; j < 4; j++) {
      this_mv.row = ref_mv->row + neighbors[j].row;
      this_mv.col = ref_mv->col + neighbors[j].col;

      if (is_mv_in(x, &this_mv)) {
        const uint8_t *check_here = &in_what[this_mv.row * in_what_stride +
                                                this_mv.col];
        thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                              bestsad);

        if (thissad < bestsad) {
          thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                    mvjsadcost, mvsadcost, error_per_bit);

          if (thissad < bestsad) {
            bestsad = thissad;
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
      best_address = &in_what[ref_mv->row * in_what_stride + ref_mv->col];
    }
  }

  this_mv.row = ref_mv->row * 8;
  this_mv.col = ref_mv->col * 8;

  if (bestsad < INT_MAX)
    return fn_ptr->vf(what, what_stride, best_address, in_what_stride,
                      (unsigned int *)(&thissad)) +
        mv_err_cost(&this_mv, center_mv, mvjcost, mvcost, x->errorperbit);
  else
    return INT_MAX;
}

int vp9_refining_search_sadx4(const MACROBLOCK *x,
                              MV *ref_mv, int error_per_bit,
                              int search_range,
                              const vp9_variance_fn_ptr_t *fn_ptr,
                              int *mvjcost, int *mvcost[2],
                              const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  MV neighbors[4] = {{ -1, 0}, {0, -1}, {0, 1}, {1, 0}};
  int i, j;
  int this_row_offset, this_col_offset;

  const int what_stride = x->plane[0].src.stride;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *what = x->plane[0].src.buf;
  const uint8_t *best_address = xd->plane[0].pre[0].buf +
                          (ref_mv->row * xd->plane[0].pre[0].stride) +
                          ref_mv->col;
  unsigned int thissad;
  MV this_mv;

  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};

  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  unsigned int bestsad = fn_ptr->sdf(what, what_stride, best_address,
                                    in_what_stride, 0x7fffffff) +
      mvsad_err_cost(ref_mv, &fcenter_mv, mvjsadcost, mvsadcost, error_per_bit);

  for (i = 0; i < search_range; i++) {
    int best_site = -1;
    int all_in = ((ref_mv->row - 1) > x->mv_row_min) &
                 ((ref_mv->row + 1) < x->mv_row_max) &
                 ((ref_mv->col - 1) > x->mv_col_min) &
                 ((ref_mv->col + 1) < x->mv_col_max);

    if (all_in) {
      unsigned int sad_array[4];
      uint8_t const *block_offset[4] = {
        best_address - in_what_stride,
        best_address - 1,
        best_address + 1,
        best_address + in_what_stride
      };

      fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride,
                     sad_array);

      for (j = 0; j < 4; j++) {
        if (sad_array[j] < bestsad) {
          this_mv.row = ref_mv->row + neighbors[j].row;
          this_mv.col = ref_mv->col + neighbors[j].col;
          sad_array[j] += mvsad_err_cost(&this_mv, &fcenter_mv,
                                         mvjsadcost, mvsadcost, error_per_bit);

          if (sad_array[j] < bestsad) {
            bestsad = sad_array[j];
            best_site = j;
          }
        }
      }
    } else {
      for (j = 0; j < 4; j++) {
        this_row_offset = ref_mv->row + neighbors[j].row;
        this_col_offset = ref_mv->col + neighbors[j].col;

        if ((this_col_offset > x->mv_col_min) &&
            (this_col_offset < x->mv_col_max) &&
            (this_row_offset > x->mv_row_min) &&
            (this_row_offset < x->mv_row_max)) {
          const uint8_t *check_here = neighbors[j].row * in_what_stride +
                                      neighbors[j].col + best_address;
          thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride,
                                bestsad);

          if (thissad < bestsad) {
            this_mv.row = this_row_offset;
            this_mv.col = this_col_offset;
            thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                      mvjsadcost, mvsadcost, error_per_bit);

            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = j;
            }
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      ref_mv->row += neighbors[best_site].row;
      ref_mv->col += neighbors[best_site].col;
      best_address += (neighbors[best_site].row) * in_what_stride +
                      neighbors[best_site].col;
    }
  }

  this_mv.row = ref_mv->row * 8;
  this_mv.col = ref_mv->col * 8;

  if (bestsad < INT_MAX)
    return fn_ptr->vf(what, what_stride, best_address, in_what_stride,
                      (unsigned int *)(&thissad)) +
                      mv_err_cost(&this_mv, center_mv,
                                  mvjcost, mvcost, x->errorperbit);
  else
    return INT_MAX;
}

// This function is called when we do joint motion search in comp_inter_inter
// mode.
int vp9_refining_search_8p_c(const MACROBLOCK *x,
                             MV *ref_mv, int error_per_bit,
                             int search_range,
                             const vp9_variance_fn_ptr_t *fn_ptr,
                             int *mvjcost, int *mvcost[2],
                             const MV *center_mv,
                             const uint8_t *second_pred, int w, int h) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MV neighbors[8] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0},
                           {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  int i, j;

  const uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what = xd->plane[0].pre[0].buf;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *best_address = &in_what[ref_mv->row * in_what_stride +
                                             ref_mv->col];
  unsigned int thissad;
  MV this_mv;
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};

  const int *mvjsadcost = x->nmvjointsadcost;
  int *mvsadcost[2] = {x->nmvsadcost[0], x->nmvsadcost[1]};

  /* Get compound pred by averaging two pred blocks. */
  unsigned int bestsad = fn_ptr->sdaf(what, what_stride,
                                      best_address, in_what_stride,
                                      second_pred, 0x7fffffff) +
      mvsad_err_cost(ref_mv, &fcenter_mv, mvjsadcost, mvsadcost, error_per_bit);

  for (i = 0; i < search_range; ++i) {
    int best_site = -1;

    for (j = 0; j < 8; j++) {
      this_mv.row = ref_mv->row + neighbors[j].row;
      this_mv.col = ref_mv->col + neighbors[j].col;

      if (is_mv_in(x, &this_mv)) {
        const uint8_t *check_here = &in_what[this_mv.row * in_what_stride +
                                                this_mv.col];

        thissad = fn_ptr->sdaf(what, what_stride, check_here, in_what_stride,
                               second_pred, bestsad);
        if (thissad < bestsad) {
          thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                    mvjsadcost, mvsadcost, error_per_bit);
          if (thissad < bestsad) {
            bestsad = thissad;
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
      best_address = &in_what[ref_mv->row * in_what_stride + ref_mv->col];
    }
  }

  this_mv.row = ref_mv->row * 8;
  this_mv.col = ref_mv->col * 8;

  if (bestsad < INT_MAX) {
    // FIXME(rbultje, yunqing): add full-pixel averaging variance functions
    // so we don't have to use the subpixel with xoff=0,yoff=0 here.
    return fn_ptr->svaf(best_address, in_what_stride, 0, 0, what, what_stride,
                        (unsigned int *)(&thissad), second_pred) +
                        mv_err_cost(&this_mv, center_mv,
                                    mvjcost, mvcost, x->errorperbit);
  } else {
    return INT_MAX;
  }
}
