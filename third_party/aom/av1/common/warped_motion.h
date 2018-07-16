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

#include "config/aom_config.h"

#include "aom_ports/mem.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/mv.h"
#include "av1/common/convolve.h"

#define MAX_PARAMDIM 9
#define LEAST_SQUARES_SAMPLES_MAX_BITS 3
#define LEAST_SQUARES_SAMPLES_MAX (1 << LEAST_SQUARES_SAMPLES_MAX_BITS)
#define SAMPLES_ARRAY_SIZE (LEAST_SQUARES_SAMPLES_MAX * 2)
#define WARPED_MOTION_DEBUG 0
#define DEFAULT_WMTYPE AFFINE

extern const int16_t warped_filter[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8];

static const uint8_t warp_pad_left[14][16] = {
  { 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 4, 4, 4, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 5, 5, 5, 5, 5, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 6, 6, 6, 6, 6, 6, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 7, 7, 7, 7, 7, 7, 7, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 10, 11, 12, 13, 14, 15 },
  { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 11, 12, 13, 14, 15 },
  { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 12, 13, 14, 15 },
  { 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 13, 14, 15 },
  { 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 14, 15 },
  { 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 15 },
  { 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15 },
};

static const uint8_t warp_pad_right[14][16] = {
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 14 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 12, 12 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 11, 11, 11, 11 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10, 10, 10 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9, 9, 9, 9, 9 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7 },
  { 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
  { 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
  { 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
  { 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
  { 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
  { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
};

void project_points_affine(const int32_t *mat, int *points, int *proj,
                           const int n, const int stride_points,
                           const int stride_proj, const int subsampling_x,
                           const int subsampling_y);

// Returns the error between the result of applying motion 'wm' to the frame
// described by 'ref' and the frame described by 'dst'.
int64_t av1_warp_error(WarpedMotionParams *wm, int use_hbd, int bd,
                       const uint8_t *ref, int width, int height, int stride,
                       uint8_t *dst, int p_col, int p_row, int p_width,
                       int p_height, int p_stride, int subsampling_x,
                       int subsampling_y, int64_t best_error);

// Returns the error between the frame described by 'ref' and the frame
// described by 'dst'.
int64_t av1_frame_error(int use_hbd, int bd, const uint8_t *ref, int stride,
                        uint8_t *dst, int p_width, int p_height, int p_stride);

void av1_warp_plane(WarpedMotionParams *wm, int use_hbd, int bd,
                    const uint8_t *ref, int width, int height, int stride,
                    uint8_t *pred, int p_col, int p_row, int p_width,
                    int p_height, int p_stride, int subsampling_x,
                    int subsampling_y, ConvolveParams *conv_params);

int find_projection(int np, int *pts1, int *pts2, BLOCK_SIZE bsize, int mvy,
                    int mvx, WarpedMotionParams *wm_params, int mi_row,
                    int mi_col);

int get_shear_params(WarpedMotionParams *wm);
#endif  // AV1_COMMON_WARPED_MOTION_H_
