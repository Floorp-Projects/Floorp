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

#ifndef AV1_COMMON_WARPED_MOTION_H_
#define AV1_COMMON_WARPED_MOTION_H_

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include "./aom_config.h"
#include "aom_ports/mem.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/mv.h"
#include "av1/common/convolve.h"

#define MAX_PARAMDIM 9
#if CONFIG_WARPED_MOTION
#define LEAST_SQUARES_SAMPLES_MAX_BITS 3
#define LEAST_SQUARES_SAMPLES_MAX (1 << LEAST_SQUARES_SAMPLES_MAX_BITS)

#if WARPED_MOTION_SORT_SAMPLES
// Search 1 row on the top and 1 column on the left, 1 upper-left block,
// 1 upper-right block.
#define SAMPLES_ARRAY_SIZE ((MAX_MIB_SIZE * 2 + 2) * 2)
#else
#define SAMPLES_ARRAY_SIZE (LEAST_SQUARES_SAMPLES_MAX * 2)
#endif  // WARPED_MOTION_SORT_SAMPLES

#define DEFAULT_WMTYPE AFFINE
#endif  // CONFIG_WARPED_MOTION

extern const int16_t warped_filter[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8];

typedef void (*ProjectPointsFunc)(const int32_t *mat, int *points, int *proj,
                                  const int n, const int stride_points,
                                  const int stride_proj,
                                  const int subsampling_x,
                                  const int subsampling_y);

void project_points_translation(const int32_t *mat, int *points, int *proj,
                                const int n, const int stride_points,
                                const int stride_proj, const int subsampling_x,
                                const int subsampling_y);

void project_points_rotzoom(const int32_t *mat, int *points, int *proj,
                            const int n, const int stride_points,
                            const int stride_proj, const int subsampling_x,
                            const int subsampling_y);

void project_points_affine(const int32_t *mat, int *points, int *proj,
                           const int n, const int stride_points,
                           const int stride_proj, const int subsampling_x,
                           const int subsampling_y);

void project_points_hortrapezoid(const int32_t *mat, int *points, int *proj,
                                 const int n, const int stride_points,
                                 const int stride_proj, const int subsampling_x,
                                 const int subsampling_y);
void project_points_vertrapezoid(const int32_t *mat, int *points, int *proj,
                                 const int n, const int stride_points,
                                 const int stride_proj, const int subsampling_x,
                                 const int subsampling_y);
void project_points_homography(const int32_t *mat, int *points, int *proj,
                               const int n, const int stride_points,
                               const int stride_proj, const int subsampling_x,
                               const int subsampling_y);

// Returns the error between the result of applying motion 'wm' to the frame
// described by 'ref' and the frame described by 'dst'.
int64_t av1_warp_error(WarpedMotionParams *wm,
#if CONFIG_HIGHBITDEPTH
                       int use_hbd, int bd,
#endif  // CONFIG_HIGHBITDEPTH
                       const uint8_t *ref, int width, int height, int stride,
                       uint8_t *dst, int p_col, int p_row, int p_width,
                       int p_height, int p_stride, int subsampling_x,
                       int subsampling_y, int x_scale, int y_scale,
                       int64_t best_error);

// Returns the error between the frame described by 'ref' and the frame
// described by 'dst'.
int64_t av1_frame_error(
#if CONFIG_HIGHBITDEPTH
    int use_hbd, int bd,
#endif  // CONFIG_HIGHBITDEPTH
    const uint8_t *ref, int stride, uint8_t *dst, int p_width, int p_height,
    int p_stride);

void av1_warp_plane(WarpedMotionParams *wm,
#if CONFIG_HIGHBITDEPTH
                    int use_hbd, int bd,
#endif  // CONFIG_HIGHBITDEPTH
                    const uint8_t *ref, int width, int height, int stride,
                    uint8_t *pred, int p_col, int p_row, int p_width,
                    int p_height, int p_stride, int subsampling_x,
                    int subsampling_y, int x_scale, int y_scale,
                    ConvolveParams *conv_params);

int find_projection(int np, int *pts1, int *pts2, BLOCK_SIZE bsize, int mvy,
                    int mvx, WarpedMotionParams *wm_params, int mi_row,
                    int mi_col);

int get_shear_params(WarpedMotionParams *wm);
#endif  // AV1_COMMON_WARPED_MOTION_H_
