/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_DCT_H_
#define VP9_ENCODER_VP9_DCT_H_

void vp9_fht4x4(TX_TYPE tx_type, const int16_t *input, int16_t *output,
                int stride);

void vp9_fht8x8(TX_TYPE tx_type, const int16_t *input, int16_t *output,
                int stride);

void vp9_fht16x16(TX_TYPE tx_type, const int16_t *input, int16_t *output,
                  int stride);

#endif  // VP9_ENCODER_VP9_DCT_H_
