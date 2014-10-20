/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_SSIM_H_
#define VP9_ENCODER_VP9_SSIM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vpx_scale/yv12config.h"

double vp9_calc_ssim(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest,
                     double *weight);

double vp9_calc_ssimg(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest,
                      double *ssim_y, double *ssim_u, double *ssim_v);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_SSIM_H_
