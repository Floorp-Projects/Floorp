/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP8_ENCODER_PSNR_H_
#define VP8_ENCODER_PSNR_H_

#ifdef __cplusplus
extern "C" {
#endif

extern double vp8_mse2psnr(double Samples, double Peak, double Mse);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP8_ENCODER_PSNR_H_
