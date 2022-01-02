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

#ifndef AOM_AV1_ENCODER_RANSAC_H_
#define AOM_AV1_ENCODER_RANSAC_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>

#include "av1/common/warped_motion.h"

typedef int (*RansacFunc)(int *matched_points, int npoints,
                          int *num_inliers_by_motion, double *params_by_motion,
                          int num_motions);

/* Each of these functions fits a motion model from a set of
   corresponding points in 2 frames using RANSAC. */
int ransac_affine(int *matched_points, int npoints, int *num_inliers_by_motion,
                  double *params_by_motion, int num_motions);
int ransac_rotzoom(int *matched_points, int npoints, int *num_inliers_by_motion,
                   double *params_by_motion, int num_motions);
int ransac_translation(int *matched_points, int npoints,
                       int *num_inliers_by_motion, double *params_by_motion,
                       int num_motions);
#endif  // AOM_AV1_ENCODER_RANSAC_H_
