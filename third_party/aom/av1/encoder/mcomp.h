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

#ifndef AV1_ENCODER_MCOMP_H_
#define AV1_ENCODER_MCOMP_H_

#include "av1/encoder/block.h"
#include "aom_dsp/variance.h"

#ifdef __cplusplus
extern "C" {
#endif

// The maximum number of steps in a step search given the largest
// allowed initial step
#define MAX_MVSEARCH_STEPS 11
// Max full pel mv specified in the unit of full pixel
// Enable the use of motion vector in range [-1023, 1023].
#define MAX_FULL_PEL_VAL ((1 << (MAX_MVSEARCH_STEPS - 1)) - 1)
// Maximum size of the first step in full pel units
#define MAX_FIRST_STEP (1 << (MAX_MVSEARCH_STEPS - 1))
// Allowed motion vector pixel distance outside image border
// for Block_16x16
#define BORDER_MV_PIXELS_B16 (16 + AOM_INTERP_EXTEND)

// motion search site
typedef struct search_site {
  MV mv;
  int offset;
} search_site;

typedef struct search_site_config {
  search_site ss[8 * MAX_MVSEARCH_STEPS + 1];
  int ss_count;
  int searches_per_step;
} search_site_config;

void av1_init_dsmotion_compensation(search_site_config *cfg, int stride);
void av1_init3smotion_compensation(search_site_config *cfg, int stride);

void av1_set_mv_search_range(MvLimits *mv_limits, const MV *mv);

int av1_mv_bit_cost(const MV *mv, const MV *ref, const int *mvjcost,
                    int *mvcost[2], int weight);

// Utility to compute variance + MV rate cost for a given MV
int av1_get_mvpred_var(const MACROBLOCK *x, const MV *best_mv,
                       const MV *center_mv, const aom_variance_fn_ptr_t *vfp,
                       int use_mvcost);
int av1_get_mvpred_av_var(const MACROBLOCK *x, const MV *best_mv,
                          const MV *center_mv, const uint8_t *second_pred,
                          const aom_variance_fn_ptr_t *vfp, int use_mvcost);
int av1_get_mvpred_mask_var(const MACROBLOCK *x, const MV *best_mv,
                            const MV *center_mv, const uint8_t *second_pred,
                            const uint8_t *mask, int mask_stride,
                            int invert_mask, const aom_variance_fn_ptr_t *vfp,
                            int use_mvcost);

struct AV1_COMP;
struct SPEED_FEATURES;

int av1_init_search_range(int size);

int av1_refining_search_sad(struct macroblock *x, MV *ref_mv, int sad_per_bit,
                            int distance, const aom_variance_fn_ptr_t *fn_ptr,
                            const MV *center_mv);

// Runs sequence of diamond searches in smaller steps for RD.
int av1_full_pixel_diamond(const struct AV1_COMP *cpi, MACROBLOCK *x,
                           MV *mvp_full, int step_param, int sadpb,
                           int further_steps, int do_refine, int *cost_list,
                           const aom_variance_fn_ptr_t *fn_ptr,
                           const MV *ref_mv, MV *dst_mv);

int av1_hex_search(MACROBLOCK *x, MV *start_mv, int search_param,
                   int sad_per_bit, int do_init_search, int *cost_list,
                   const aom_variance_fn_ptr_t *vfp, int use_mvcost,
                   const MV *center_mv);

typedef int(fractional_mv_step_fp)(
    MACROBLOCK *x, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *ref_mv, int allow_hp, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp,
    int forced_stop,  // 0 - full, 1 - qtr only, 2 - half only
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    int *distortion, unsigned int *sse1, const uint8_t *second_pred,
    const uint8_t *mask, int mask_stride, int invert_mask, int w, int h,
    int use_accurate_subpel_search);

extern fractional_mv_step_fp av1_find_best_sub_pixel_tree;
extern fractional_mv_step_fp av1_find_best_sub_pixel_tree_pruned;
extern fractional_mv_step_fp av1_find_best_sub_pixel_tree_pruned_more;
extern fractional_mv_step_fp av1_find_best_sub_pixel_tree_pruned_evenmore;
extern fractional_mv_step_fp av1_return_max_sub_pixel_mv;
extern fractional_mv_step_fp av1_return_min_sub_pixel_mv;

typedef int (*av1_full_search_fn_t)(const MACROBLOCK *x, const MV *ref_mv,
                                    int sad_per_bit, int distance,
                                    const aom_variance_fn_ptr_t *fn_ptr,
                                    const MV *center_mv, MV *best_mv);

typedef int (*av1_diamond_search_fn_t)(
    MACROBLOCK *x, const search_site_config *cfg, MV *ref_mv, MV *best_mv,
    int search_param, int sad_per_bit, int *num00,
    const aom_variance_fn_ptr_t *fn_ptr, const MV *center_mv);

int av1_refining_search_8p_c(MACROBLOCK *x, int error_per_bit, int search_range,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const uint8_t *mask, int mask_stride,
                             int invert_mask, const MV *center_mv,
                             const uint8_t *second_pred);

int av1_full_pixel_search(const struct AV1_COMP *cpi, MACROBLOCK *x,
                          BLOCK_SIZE bsize, MV *mvp_full, int step_param,
                          int error_per_bit, int *cost_list, const MV *ref_mv,
                          int var_max, int rd, int x_pos, int y_pos, int intra);

int av1_obmc_full_pixel_diamond(const struct AV1_COMP *cpi, MACROBLOCK *x,
                                MV *mvp_full, int step_param, int sadpb,
                                int further_steps, int do_refine,
                                const aom_variance_fn_ptr_t *fn_ptr,
                                const MV *ref_mv, MV *dst_mv, int is_second);
int av1_find_best_obmc_sub_pixel_tree_up(
    MACROBLOCK *x, const AV1_COMMON *const cm, int mi_row, int mi_col,
    MV *bestmv, const MV *ref_mv, int allow_hp, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, int forced_stop, int iters_per_step,
    int *mvjcost, int *mvcost[2], int *distortion, unsigned int *sse1,
    int is_second, int use_accurate_subpel_search);

unsigned int av1_compute_motion_cost(const struct AV1_COMP *cpi,
                                     MACROBLOCK *const x, BLOCK_SIZE bsize,
                                     int mi_row, int mi_col, const MV *this_mv);
unsigned int av1_refine_warped_mv(const struct AV1_COMP *cpi,
                                  MACROBLOCK *const x, BLOCK_SIZE bsize,
                                  int mi_row, int mi_col, int *pts0,
                                  int *pts_inref0, int total_samples);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_MCOMP_H_
