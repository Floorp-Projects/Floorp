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

// metrics used for calculating ssim, ssim2, dssim, and ssimc
typedef struct {
  // source sum ( over 8x8 region )
  uint64_t sum_s;

  // reference sum (over 8x8 region )
  uint64_t sum_r;

  // source sum squared ( over 8x8 region )
  uint64_t sum_sq_s;

  // reference sum squared (over 8x8 region )
  uint64_t sum_sq_r;

  // sum of source times reference (over 8x8 region)
  uint64_t sum_sxr;

  // calculated ssim score between source and reference
  double ssim;
} Ssimv;

// metrics collected on a frame basis
typedef struct {
  // ssim consistency error metric ( see code for explanation )
  double ssimc;

  // standard ssim
  double ssim;

  // revised ssim ( see code for explanation)
  double ssim2;

  // ssim restated as an error metric like sse
  double dssim;

  // dssim converted to decibels
  double dssimd;

  // ssimc converted to decibels
  double ssimcd;
} Metrics;

double vp9_get_ssim_metrics(uint8_t *img1, int img1_pitch, uint8_t *img2,
                      int img2_pitch, int width, int height, Ssimv *sv2,
                      Metrics *m, int do_inconsistency);

double vp9_calc_ssim(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest,
                     double *weight);

double vp9_calc_ssimg(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest,
                      double *ssim_y, double *ssim_u, double *ssim_v);

double vp9_calc_fastssim(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest,
                         double *ssim_y, double *ssim_u, double *ssim_v);

double vp9_psnrhvs(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest,
                   double *ssim_y, double *ssim_u, double *ssim_v);

#if CONFIG_VP9_HIGHBITDEPTH
double vp9_highbd_calc_ssim(YV12_BUFFER_CONFIG *source,
                            YV12_BUFFER_CONFIG *dest,
                            double *weight,
                            unsigned int bd);

double vp9_highbd_calc_ssimg(YV12_BUFFER_CONFIG *source,
                             YV12_BUFFER_CONFIG *dest,
                             double *ssim_y,
                             double *ssim_u,
                             double *ssim_v,
                             unsigned int bd);
#endif  // CONFIG_VP9_HIGHBITDEPTH

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_SSIM_H_
