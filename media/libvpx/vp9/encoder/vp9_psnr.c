/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "vpx_scale/yv12config.h"

#define MAX_PSNR 100

double vp9_mse2psnr(double samples, double peak, double mse) {
  double psnr;

  if (mse > 0.0)
    psnr = 10.0 * log10(peak * peak * samples / mse);
  else
    psnr = MAX_PSNR;  // Limit to prevent / 0

  if (psnr > MAX_PSNR)
    psnr = MAX_PSNR;

  return psnr;
}
