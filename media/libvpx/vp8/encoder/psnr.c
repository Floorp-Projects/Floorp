/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_scale/yv12config.h"
#include "math.h"
#include "vp8/common/systemdependent.h" /* for vp8_clear_system_state() */

#define MAX_PSNR 60

double vp8_mse2psnr(double Samples, double Peak, double Mse)
{
    double psnr;

    if ((double)Mse > 0.0)
        psnr = 10.0 * log10(Peak * Peak * Samples / Mse);
    else
        psnr = MAX_PSNR;      // Limit to prevent / 0

    if (psnr > MAX_PSNR)
        psnr = MAX_PSNR;

    return psnr;
}
