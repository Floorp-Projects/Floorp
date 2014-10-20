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

#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_mcomp.h"

// #define NEW_DIAMOND_SEARCH

static INLINE const uint8_t *get_buf_from_mv(const struct buf_2d *buf,
                                             const MV *mv) {
  return &buf->buf[mv->row * buf->stride + mv->col];
}

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

int vp9_init_search_range(int size) {
  int sr = 0;
  // Minimum search size no matter what the passed in value.
  size = MAX(16, size);

  while ((size << sr) < MAX_FULL_PEL_VAL)
    sr++;

  sr = MIN(sr, MAX_MVSEARCH_STEPS - 2);
  return sr;
}

static INLINE int mv_cost(const MV *mv,
                          const int *joint_cost, int *const comp_cost[2]) {
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

static int mvsad_err_cost(const MACROBLOCK *x, const MV *mv, const MV *ref,
                          int error_per_bit) {
  if (x->nmvsadcost) {
    const MV diff = { mv->row - ref->row,
                      mv->col - ref->col };
    return ROUND_POWER_OF_TWO(mv_cost(&diff, x->nmvjointsadcost,
                                      x->nmvsadcost) * error_per_bit, 8);
  }
  return 0;
}

void vp9_init_dsmotion_compensation(search_site_config *cfg, int stride) {
  int len, ss_count = 1;

  cfg->ss[0].mv.col = cfg->ss[0].mv.row = 0;
  cfg->ss[0].offset = 0;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Generate offsets for 4 search sites per step.
    const MV ss_mvs[] = {{-len, 0}, {len, 0}, {0, -len}, {0, len}};
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

void vp9_init3smotion_compensation(search_site_config *cfg, int stride) {
  int len, ss_count = 1;

  cfg->ss[0].mv.col = cfg->ss[0].mv.row = 0;
  cfg->ss[0].offset = 0;

  for (len = MAX_FIRST_STEP; len > 0; len /= 2) {
    // Generate offsets for 8 search sites per step.
    const MV ss_mvs[8] = {
      {-len,  0  }, {len,  0  }, { 0,   -len}, {0,    len},
      {-len, -len}, {-len, len}, {len,  -len}, {len,  len}
    };
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

static INLINE const uint8_t *pre(const uint8_t *buf, int stride, int r, int c) {
  return &buf[(r >> 3) * stride + (c >> 3)];
}

/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER(v, r, c) \
  if (c >= minc && c <= maxc && r >= minr && r <= maxr) {              \
    if (second_pred == NULL)                                           \
      thismse = vfp->svf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), z, \
                             src_stride, &sse);                        \
    else                                                               \
      thismse = vfp->svaf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), \
                              z, src_stride, &sse, second_pred);       \
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

#define SETUP_SUBPEL_SEARCH                                                \
  const uint8_t *const z = x->plane[0].src.buf;                            \
  const int src_stride = x->plane[0].src.stride;                           \
  const MACROBLOCKD *xd = &x->e_mbd;                                       \
  unsigned int besterr = INT_MAX;                                          \
  unsigned int sse;                                                        \
  unsigned int whichdir;                                                   \
  int thismse;                                                             \
  const unsigned int halfiters = iters_per_step;                           \
  const unsigned int quarteriters = iters_per_step;                        \
  const unsigned int eighthiters = iters_per_step;                         \
  const int y_stride = xd->plane[0].pre[0].stride;                         \
  const int offset = bestmv->row * y_stride + bestmv->col;                 \
  const uint8_t *const y = xd->plane[0].pre[0].buf;                        \
                                                                           \
  int rr = ref_mv->row;                                                    \
  int rc = ref_mv->col;                                                    \
  int br = bestmv->row * 8;                                                \
  int bc = bestmv->col * 8;                                                \
  int hstep = 4;                                                           \
  const int minc = MAX(x->mv_col_min * 8, ref_mv->col - MV_MAX);           \
  const int maxc = MIN(x->mv_col_max * 8, ref_mv->col + MV_MAX);           \
  const int minr = MAX(x->mv_row_min * 8, ref_mv->row - MV_MAX);           \
  const int maxr = MIN(x->mv_row_max * 8, ref_mv->row + MV_MAX);           \
  int tr = br;                                                             \
  int tc = bc;                                                             \
                                                                           \
  bestmv->row *= 8;                                                        \
  bestmv->col *= 8;                                                        \
  if (second_pred != NULL) {                                               \
    DECLARE_ALIGNED_ARRAY(16, uint8_t, comp_pred, 64 * 64);                \
    vp9_comp_avg_pred(comp_pred, second_pred, w, h, y + offset, y_stride); \
    besterr = vfp->vf(comp_pred, w, z, src_stride, sse1);                  \
  } else {                                                                 \
    besterr = vfp->vf(y + offset, y_stride, z, src_stride, sse1);          \
  }                                                                        \
  *distortion = besterr;                                                   \
  besterr += mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit);

int vp9_find_best_sub_pixel_tree_pruned(const MACROBLOCK *x,
                                        MV *bestmv, const MV *ref_mv,
                                        int allow_hp,
                                        int error_per_bit,
                                        const vp9_variance_fn_ptr_t *vfp,
                                        int forced_stop,
                                        int iters_per_step,
                                        int *sad_list,
                                        int *mvjcost, int *mvcost[2],
                                        int *distortion,
                                        unsigned int *sse1,
                                        const uint8_t *second_pred,
                                        int w, int h) {
  SETUP_SUBPEL_SEARCH;

  if (sad_list &&
      sad_list[0] != INT_MAX && sad_list[1] != INT_MAX &&
      sad_list[2] != INT_MAX && sad_list[3] != INT_MAX &&
      sad_list[4] != INT_MAX) {
    unsigned int left, right, up, down, diag;
    whichdir = (sad_list[1] < sad_list[3] ? 0 : 1) +
               (sad_list[2] < sad_list[4] ? 0 : 2);
    switch (whichdir) {
      case 0:
        CHECK_BETTER(left, tr, tc - hstep);
        CHECK_BETTER(up, tr - hstep, tc);
        CHECK_BETTER(diag, tr - hstep, tc - hstep);
        break;
      case 1:
        CHECK_BETTER(right, tr, tc + hstep);
        CHECK_BETTER(up, tr - hstep, tc);
        CHECK_BETTER(diag, tr - hstep, tc + hstep);
        break;
      case 2:
        CHECK_BETTER(left, tr, tc - hstep);
        CHECK_BETTER(down, tr + hstep, tc);
        CHECK_BETTER(diag, tr + hstep, tc - hstep);
        break;
      case 3:
        CHECK_BETTER(right, tr, tc + hstep);
        CHECK_BETTER(down, tr + hstep, tc);
        CHECK_BETTER(diag, tr + hstep, tc + hstep);
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

int vp9_find_best_sub_pixel_tree(const MACROBLOCK *x,
                                 MV *bestmv, const MV *ref_mv,
                                 int allow_hp,
                                 int error_per_bit,
                                 const vp9_variance_fn_ptr_t *vfp,
                                 int forced_stop,
                                 int iters_per_step,
                                 int *sad_list,
                                 int *mvjcost, int *mvcost[2],
                                 int *distortion,
                                 unsigned int *sse1,
                                 const uint8_t *second_pred,
                                 int w, int h) {
  SETUP_SUBPEL_SEARCH;
  (void) sad_list;  // to silence compiler warning

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
        thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);\
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
//
static int vp9_pattern_search(const MACROBLOCK *x,
                              MV *ref_mv,
                              int search_param,
                              int sad_per_bit,
                              int do_init_search,
                              int *sad_list,
                              const vp9_variance_fn_ptr_t *vfp,
                              int use_mvcost,
                              const MV *center_mv,
                              MV *best_mv,
                              const int num_candidates[MAX_PATTERN_SCALES],
                              const MV candidates[MAX_PATTERN_SCALES]
                                                 [MAX_PATTERN_CANDIDATES]) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  static const int search_param_to_steps[MAX_MVSEARCH_STEPS] = {
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  int i, s, t;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  int br, bc;
  int bestsad = INT_MAX;
  int thissad;
  int k = -1;
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  int best_init_s = search_param_to_steps[search_param];
  // adjust ref_mv to make sure it is within MV range
  clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
  br = ref_mv->row;
  bc = ref_mv->col;

  // Work out the start point for the search
  bestsad = vfp->sdf(what->buf, what->stride,
                     get_buf_from_mv(in_what, ref_mv), in_what->stride) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);

  // Search all possible scales upto the search param around the center point
  // pick the scale of the point that is best as the starting scale of
  // further steps around it.
  if (do_init_search) {
    s = best_init_s;
    best_init_s = -1;
    for (t = 0; t <= s; ++t) {
      int best_site = -1;
      if (check_bounds(x, br, bc, 1 << t)) {
        for (i = 0; i < num_candidates[t]; i++) {
          const MV this_mv = {br + candidates[t][i].row,
                              bc + candidates[t][i].col};
          thissad = vfp->sdf(what->buf, what->stride,
                             get_buf_from_mv(in_what, &this_mv),
                             in_what->stride);
          CHECK_BETTER
        }
      } else {
        for (i = 0; i < num_candidates[t]; i++) {
          const MV this_mv = {br + candidates[t][i].row,
                              bc + candidates[t][i].col};
          if (!is_mv_in(x, &this_mv))
            continue;
          thissad = vfp->sdf(what->buf, what->stride,
                             get_buf_from_mv(in_what, &this_mv),
                             in_what->stride);
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
    int best_site = -1;
    s = best_init_s;

    do {
      // No need to search all 6 points the 1st time if initial search was used
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(x, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = {br + candidates[s][i].row,
                                bc + candidates[s][i].col};
            thissad = vfp->sdf(what->buf, what->stride,
                               get_buf_from_mv(in_what, &this_mv),
                               in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const MV this_mv = {br + candidates[s][i].row,
                                bc + candidates[s][i].col};
            if (!is_mv_in(x, &this_mv))
              continue;
            thissad = vfp->sdf(what->buf, what->stride,
                               get_buf_from_mv(in_what, &this_mv),
                               in_what->stride);
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
            const MV this_mv = {br + candidates[s][next_chkpts_indices[i]].row,
                                bc + candidates[s][next_chkpts_indices[i]].col};
            thissad = vfp->sdf(what->buf, what->stride,
                               get_buf_from_mv(in_what, &this_mv),
                               in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const MV this_mv = {br + candidates[s][next_chkpts_indices[i]].row,
                                bc + candidates[s][next_chkpts_indices[i]].col};
            if (!is_mv_in(x, &this_mv))
              continue;
            thissad = vfp->sdf(what->buf, what->stride,
                               get_buf_from_mv(in_what, &this_mv),
                               in_what->stride);
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

  // Returns the one-away integer pel sad values around the best as follows:
  // sad_list[0]: sad at the best integer pel
  // sad_list[1]: sad at delta {0, -1} (left)   from the best integer pel
  // sad_list[2]: sad at delta {-1, 0} (top)    from the best integer pel
  // sad_list[3]: sad at delta { 0, 1} (right)  from the best integer pel
  // sad_list[4]: sad at delta { 1, 0} (bottom) from the best integer pel
  if (sad_list) {
    static const MV neighbors[4] = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}};
    sad_list[0] = bestsad;
    if (check_bounds(x, br, bc, 1)) {
      for (i = 0; i < 4; i++) {
        const MV this_mv = {br + neighbors[i].row,
                            bc + neighbors[i].col};
        sad_list[i + 1] = vfp->sdf(what->buf, what->stride,
                                   get_buf_from_mv(in_what, &this_mv),
                                   in_what->stride);
      }
    } else {
      for (i = 0; i < 4; i++) {
        const MV this_mv = {br + neighbors[i].row,
                            bc + neighbors[i].col};
        if (!is_mv_in(x, &this_mv))
          sad_list[i + 1] = INT_MAX;
        else
          sad_list[i + 1] = vfp->sdf(what->buf, what->stride,
                                     get_buf_from_mv(in_what, &this_mv),
                                     in_what->stride);
      }
    }
  }
  best_mv->row = br;
  best_mv->col = bc;
  return bestsad;
}

int vp9_get_mvpred_var(const MACROBLOCK *x,
                       const MV *best_mv, const MV *center_mv,
                       const vp9_variance_fn_ptr_t *vfp,
                       int use_mvcost) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = {best_mv->row * 8, best_mv->col * 8};
  unsigned int unused;

  return vfp->vf(what->buf, what->stride,
                 get_buf_from_mv(in_what, best_mv), in_what->stride, &unused) +
      (use_mvcost ?  mv_err_cost(&mv, center_mv, x->nmvjointcost,
                                 x->mvcost, x->errorperbit) : 0);
}

int vp9_get_mvpred_av_var(const MACROBLOCK *x,
                          const MV *best_mv, const MV *center_mv,
                          const uint8_t *second_pred,
                          const vp9_variance_fn_ptr_t *vfp,
                          int use_mvcost) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = {best_mv->row * 8, best_mv->col * 8};
  unsigned int unused;

  return vfp->svaf(get_buf_from_mv(in_what, best_mv), in_what->stride, 0, 0,
                   what->buf, what->stride, &unused, second_pred) +
      (use_mvcost ?  mv_err_cost(&mv, center_mv, x->nmvjointcost,
                                 x->mvcost, x->errorperbit) : 0);
}

int vp9_hex_search(const MACROBLOCK *x,
                   MV *ref_mv,
                   int search_param,
                   int sad_per_bit,
                   int do_init_search,
                   int *sad_list,
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
                            do_init_search, sad_list, vfp, use_mvcost,
                            center_mv, best_mv,
                            hex_num_candidates, hex_candidates);
}

int vp9_bigdia_search(const MACROBLOCK *x,
                      MV *ref_mv,
                      int search_param,
                      int sad_per_bit,
                      int do_init_search,
                      int *sad_list,
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
                            do_init_search, sad_list, vfp, use_mvcost,
                            center_mv, best_mv,
                            bigdia_num_candidates, bigdia_candidates);
}

int vp9_square_search(const MACROBLOCK *x,
                      MV *ref_mv,
                      int search_param,
                      int sad_per_bit,
                      int do_init_search,
                      int *sad_list,
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
                            do_init_search, sad_list, vfp, use_mvcost,
                            center_mv, best_mv,
                            square_num_candidates, square_candidates);
}

int vp9_fast_hex_search(const MACROBLOCK *x,
                        MV *ref_mv,
                        int search_param,
                        int sad_per_bit,
                        int do_init_search,  // must be zero for fast_hex
                        int *sad_list,
                        const vp9_variance_fn_ptr_t *vfp,
                        int use_mvcost,
                        const MV *center_mv,
                        MV *best_mv) {
  return vp9_hex_search(x, ref_mv, MAX(MAX_MVSEARCH_STEPS - 2, search_param),
                        sad_per_bit, do_init_search, sad_list, vfp, use_mvcost,
                        center_mv, best_mv);
}

int vp9_fast_dia_search(const MACROBLOCK *x,
                        MV *ref_mv,
                        int search_param,
                        int sad_per_bit,
                        int do_init_search,
                        int *sad_list,
                        const vp9_variance_fn_ptr_t *vfp,
                        int use_mvcost,
                        const MV *center_mv,
                        MV *best_mv) {
  return vp9_bigdia_search(x, ref_mv, MAX(MAX_MVSEARCH_STEPS - 2, search_param),
                           sad_per_bit, do_init_search, sad_list, vfp,
                           use_mvcost, center_mv, best_mv);
}

#undef CHECK_BETTER

int vp9_full_range_search_c(const MACROBLOCK *x,
                            const search_site_config *cfg,
                            MV *ref_mv, MV *best_mv,
                            int search_param, int sad_per_bit, int *num00,
                            const vp9_variance_fn_ptr_t *fn_ptr,
                            const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const int range = 64;
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  unsigned int best_sad = INT_MAX;
  int r, c, i;
  int start_col, end_col, start_row, end_row;

  // The cfg and search_param parameters are not used in this search variant
  (void)cfg;
  (void)search_param;

  clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
  *best_mv = *ref_mv;
  *num00 = 11;
  best_sad = fn_ptr->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, ref_mv), in_what->stride) +
                 mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);
  start_row = MAX(-range, x->mv_row_min - ref_mv->row);
  start_col = MAX(-range, x->mv_col_min - ref_mv->col);
  end_row = MIN(range, x->mv_row_max - ref_mv->row);
  end_col = MIN(range, x->mv_col_max - ref_mv->col);

  for (r = start_row; r <= end_row; ++r) {
    for (c = start_col; c <= end_col; c += 4) {
      if (c + 3 <= end_col) {
        unsigned int sads[4];
        const uint8_t *addrs[4];
        for (i = 0; i < 4; ++i) {
          const MV mv = {ref_mv->row + r, ref_mv->col + c + i};
          addrs[i] = get_buf_from_mv(in_what, &mv);
        }

        fn_ptr->sdx4df(what->buf, what->stride, addrs, in_what->stride, sads);

        for (i = 0; i < 4; ++i) {
          if (sads[i] < best_sad) {
            const MV mv = {ref_mv->row + r, ref_mv->col + c + i};
            const unsigned int sad = sads[i] +
                mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              *best_mv = mv;
            }
          }
        }
      } else {
        for (i = 0; i < end_col - c; ++i) {
          const MV mv = {ref_mv->row + r, ref_mv->col + c + i};
          unsigned int sad = fn_ptr->sdf(what->buf, what->stride,
              get_buf_from_mv(in_what, &mv), in_what->stride);
          if (sad < best_sad) {
            sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              *best_mv = mv;
            }
          }
        }
      }
    }
  }

  return best_sad;
}

int vp9_diamond_search_sad_c(const MACROBLOCK *x,
                             const search_site_config *cfg,
                             MV *ref_mv, MV *best_mv, int search_param,
                             int sad_per_bit, int *num00,
                             const vp9_variance_fn_ptr_t *fn_ptr,
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

  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
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
  bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride)
                + mvsad_err_cost(x, best_mv, &fcenter_mv, sad_per_bit);

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

      for (j = 0; j < cfg->searches_per_step; j += 4) {
        unsigned char const *block_offset[4];

        for (t = 0; t < 4; t++)
          block_offset[t] = ss[i + t].offset + best_address;

        fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride,
                       sad_array);

        for (t = 0; t < 4; t++, i++) {
          if (sad_array[t] < bestsad) {
            const MV this_mv = {best_mv->row + ss[i].mv.row,
                                best_mv->col + ss[i].mv.col};
            sad_array[t] += mvsad_err_cost(x, &this_mv, &fcenter_mv,
                                           sad_per_bit);
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
        const MV this_mv = {best_mv->row + ss[i].mv.row,
                            best_mv->col + ss[i].mv.col};

        if (is_mv_in(x, &this_mv)) {
          const uint8_t *const check_here = ss[i].offset + best_address;
          unsigned int thissad = fn_ptr->sdf(what, what_stride, check_here,
                                             in_what_stride);

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
      best_mv->row += ss[best_site].mv.row;
      best_mv->col += ss[best_site].mv.col;
      best_address += ss[best_site].offset;
      last_site = best_site;
#if defined(NEW_DIAMOND_SEARCH)
      while (1) {
        const MV this_mv = {best_mv->row + ss[best_site].mv.row,
                            best_mv->col + ss[best_site].mv.col};
        if (is_mv_in(x, &this_mv)) {
          const uint8_t *const check_here = ss[best_site].offset + best_address;
          unsigned int thissad = fn_ptr->sdf(what, what_stride, check_here,
                                             in_what_stride);
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
      };
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

int vp9_full_pixel_diamond(const VP9_COMP *cpi, MACROBLOCK *x,
                           MV *mvp_full, int step_param,
                           int sadpb, int further_steps, int do_refine,
                           const vp9_variance_fn_ptr_t *fn_ptr,
                           const MV *ref_mv, MV *dst_mv) {
  MV temp_mv;
  int thissme, n, num00 = 0;
  int bestsme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                        step_param, sadpb, &n,
                                        fn_ptr, ref_mv);
  if (bestsme < INT_MAX)
    bestsme = vp9_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);
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
      thissme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv,
                                        step_param + n, sadpb, &num00,
                                        fn_ptr, ref_mv);
      if (thissme < INT_MAX)
        thissme = vp9_get_mvpred_var(x, &temp_mv, ref_mv, fn_ptr, 1);

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
                                       fn_ptr, ref_mv);
    if (thissme < INT_MAX)
      thissme = vp9_get_mvpred_var(x, &best_mv, ref_mv, fn_ptr, 1);
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
                          const MV *center_mv, MV *best_mv) {
  int r, c;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const int row_min = MAX(ref_mv->row - distance, x->mv_row_min);
  const int row_max = MIN(ref_mv->row + distance, x->mv_row_max);
  const int col_min = MAX(ref_mv->col - distance, x->mv_col_min);
  const int col_max = MIN(ref_mv->col + distance, x->mv_col_max);
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  int best_sad = fn_ptr->sdf(what->buf, what->stride,
      get_buf_from_mv(in_what, ref_mv), in_what->stride) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);
  *best_mv = *ref_mv;

  for (r = row_min; r < row_max; ++r) {
    for (c = col_min; c < col_max; ++c) {
      const MV mv = {r, c};
      const int sad = fn_ptr->sdf(what->buf, what->stride,
          get_buf_from_mv(in_what, &mv), in_what->stride) +
              mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
      if (sad < best_sad) {
        best_sad = sad;
        *best_mv = mv;
      }
    }
  }
  return best_sad;
}

int vp9_full_search_sadx3(const MACROBLOCK *x, const MV *ref_mv,
                          int sad_per_bit, int distance,
                          const vp9_variance_fn_ptr_t *fn_ptr,
                          const MV *center_mv, MV *best_mv) {
  int r;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const int row_min = MAX(ref_mv->row - distance, x->mv_row_min);
  const int row_max = MIN(ref_mv->row + distance, x->mv_row_max);
  const int col_min = MAX(ref_mv->col - distance, x->mv_col_min);
  const int col_max = MIN(ref_mv->col + distance, x->mv_col_max);
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  unsigned int best_sad = fn_ptr->sdf(what->buf, what->stride,
      get_buf_from_mv(in_what, ref_mv), in_what->stride) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);
  *best_mv = *ref_mv;

  for (r = row_min; r < row_max; ++r) {
    int c = col_min;
    const uint8_t *check_here = &in_what->buf[r * in_what->stride + c];

    if (fn_ptr->sdx3f != NULL) {
      while ((c + 2) < col_max) {
        int i;
        unsigned int sads[3];

        fn_ptr->sdx3f(what->buf, what->stride, check_here, in_what->stride,
                      sads);

        for (i = 0; i < 3; ++i) {
          unsigned int sad = sads[i];
          if (sad < best_sad) {
            const MV mv = {r, c};
            sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              *best_mv = mv;
            }
          }
          ++check_here;
          ++c;
        }
      }
    }

    while (c < col_max) {
      unsigned int sad = fn_ptr->sdf(what->buf, what->stride,
                                     check_here, in_what->stride);
      if (sad < best_sad) {
        const MV mv = {r, c};
        sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
        if (sad < best_sad) {
          best_sad = sad;
          *best_mv = mv;
        }
      }
      ++check_here;
      ++c;
    }
  }

  return best_sad;
}

int vp9_full_search_sadx8(const MACROBLOCK *x, const MV *ref_mv,
                          int sad_per_bit, int distance,
                          const vp9_variance_fn_ptr_t *fn_ptr,
                          const MV *center_mv, MV *best_mv) {
  int r;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const int row_min = MAX(ref_mv->row - distance, x->mv_row_min);
  const int row_max = MIN(ref_mv->row + distance, x->mv_row_max);
  const int col_min = MAX(ref_mv->col - distance, x->mv_col_min);
  const int col_max = MIN(ref_mv->col + distance, x->mv_col_max);
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  unsigned int best_sad = fn_ptr->sdf(what->buf, what->stride,
      get_buf_from_mv(in_what, ref_mv), in_what->stride) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);
  *best_mv = *ref_mv;

  for (r = row_min; r < row_max; ++r) {
    int c = col_min;
    const uint8_t *check_here = &in_what->buf[r * in_what->stride + c];

    if (fn_ptr->sdx8f != NULL) {
      while ((c + 7) < col_max) {
        int i;
        unsigned int sads[8];

        fn_ptr->sdx8f(what->buf, what->stride, check_here, in_what->stride,
                      sads);

        for (i = 0; i < 8; ++i) {
          unsigned int sad = sads[i];
          if (sad < best_sad) {
            const MV mv = {r, c};
            sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              *best_mv = mv;
            }
          }
          ++check_here;
          ++c;
        }
      }
    }

    if (fn_ptr->sdx3f != NULL) {
      while ((c + 2) < col_max) {
        int i;
        unsigned int sads[3];

        fn_ptr->sdx3f(what->buf, what->stride, check_here, in_what->stride,
                      sads);

        for (i = 0; i < 3; ++i) {
          unsigned int sad = sads[i];
          if (sad < best_sad) {
            const MV mv = {r, c};
            sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
            if (sad < best_sad) {
              best_sad = sad;
              *best_mv = mv;
            }
          }
          ++check_here;
          ++c;
        }
      }
    }

    while (c < col_max) {
      unsigned int sad = fn_ptr->sdf(what->buf, what->stride,
                                     check_here, in_what->stride);
      if (sad < best_sad) {
        const MV mv = {r, c};
        sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
        if (sad < best_sad) {
          best_sad = sad;
          *best_mv = mv;
        }
      }
      ++check_here;
      ++c;
    }
  }

  return best_sad;
}

int vp9_refining_search_sad_c(const MACROBLOCK *x,
                              MV *ref_mv, int error_per_bit,
                              int search_range,
                              const vp9_variance_fn_ptr_t *fn_ptr,
                              const MV *center_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MV neighbors[4] = {{ -1, 0}, {0, -1}, {0, 1}, {1, 0}};
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  const uint8_t *best_address = get_buf_from_mv(in_what, ref_mv);
  unsigned int best_sad = fn_ptr->sdf(what->buf, what->stride, best_address,
                                    in_what->stride) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);
  int i, j;

  for (i = 0; i < search_range; i++) {
    int best_site = -1;
    const int all_in = ((ref_mv->row - 1) > x->mv_row_min) &
                       ((ref_mv->row + 1) < x->mv_row_max) &
                       ((ref_mv->col - 1) > x->mv_col_min) &
                       ((ref_mv->col + 1) < x->mv_col_max);

    if (all_in) {
      unsigned int sads[4];
      const uint8_t *const positions[4] = {
        best_address - in_what->stride,
        best_address - 1,
        best_address + 1,
        best_address + in_what->stride
      };

      fn_ptr->sdx4df(what->buf, what->stride, positions, in_what->stride, sads);

      for (j = 0; j < 4; ++j) {
        if (sads[j] < best_sad) {
          const MV mv = {ref_mv->row + neighbors[j].row,
                         ref_mv->col + neighbors[j].col};
          sads[j] += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
          if (sads[j] < best_sad) {
            best_sad = sads[j];
            best_site = j;
          }
        }
      }
    } else {
      for (j = 0; j < 4; ++j) {
        const MV mv = {ref_mv->row + neighbors[j].row,
                       ref_mv->col + neighbors[j].col};

        if (is_mv_in(x, &mv)) {
          unsigned int sad = fn_ptr->sdf(what->buf, what->stride,
                                         get_buf_from_mv(in_what, &mv),
                                         in_what->stride);
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
      ref_mv->row += neighbors[best_site].row;
      ref_mv->col += neighbors[best_site].col;
      best_address = get_buf_from_mv(in_what, ref_mv);
    }
  }

  return best_sad;
}

// This function is called when we do joint motion search in comp_inter_inter
// mode.
int vp9_refining_search_8p_c(const MACROBLOCK *x,
                             MV *ref_mv, int error_per_bit,
                             int search_range,
                             const vp9_variance_fn_ptr_t *fn_ptr,
                             const MV *center_mv,
                             const uint8_t *second_pred) {
  const MV neighbors[8] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0},
                           {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV fcenter_mv = {center_mv->row >> 3, center_mv->col >> 3};
  unsigned int best_sad = fn_ptr->sdaf(what->buf, what->stride,
      get_buf_from_mv(in_what, ref_mv), in_what->stride, second_pred) +
      mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);
  int i, j;

  for (i = 0; i < search_range; ++i) {
    int best_site = -1;

    for (j = 0; j < 8; ++j) {
      const MV mv = {ref_mv->row + neighbors[j].row,
                     ref_mv->col + neighbors[j].col};

      if (is_mv_in(x, &mv)) {
        unsigned int sad = fn_ptr->sdaf(what->buf, what->stride,
            get_buf_from_mv(in_what, &mv), in_what->stride, second_pred);
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

int vp9_full_pixel_search(VP9_COMP *cpi, MACROBLOCK *x,
                          BLOCK_SIZE bsize, MV *mvp_full,
                          int step_param, int error_per_bit,
                          int *sad_list,
                          const MV *ref_mv, MV *tmp_mv,
                          int var_max, int rd) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  const SEARCH_METHODS method = sf->mv.search_method;
  vp9_variance_fn_ptr_t *fn_ptr = &cpi->fn_ptr[bsize];
  int var = 0;
  if (sad_list) {
    sad_list[0] = INT_MAX;
    sad_list[1] = INT_MAX;
    sad_list[2] = INT_MAX;
    sad_list[3] = INT_MAX;
    sad_list[4] = INT_MAX;
  }

  switch (method) {
    case FAST_DIAMOND:
      var = vp9_fast_dia_search(x, mvp_full, step_param, error_per_bit, 0,
                                sad_list, fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case FAST_HEX:
      var = vp9_fast_hex_search(x, mvp_full, step_param, error_per_bit, 0,
                                sad_list, fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case HEX:
      var = vp9_hex_search(x, mvp_full, step_param, error_per_bit, 1,
                           sad_list, fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case SQUARE:
      var = vp9_square_search(x, mvp_full, step_param, error_per_bit, 1,
                              sad_list, fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case BIGDIA:
      var = vp9_bigdia_search(x, mvp_full, step_param, error_per_bit, 1,
                              sad_list, fn_ptr, 1, ref_mv, tmp_mv);
      break;
    case NSTEP:
      var = vp9_full_pixel_diamond(cpi, x, mvp_full, step_param, error_per_bit,
                                   MAX_MVSEARCH_STEPS - 1 - step_param,
                                   1, fn_ptr, ref_mv, tmp_mv);
      break;
    default:
      assert(!"Invalid search method.");
  }

  if (method != NSTEP && rd && var < var_max)
    var = vp9_get_mvpred_var(x, tmp_mv, ref_mv, fn_ptr, 1);

  return var;
}
