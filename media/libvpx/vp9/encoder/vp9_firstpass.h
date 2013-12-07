/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_FIRSTPASS_H_
#define VP9_ENCODER_VP9_FIRSTPASS_H_
#include "vp9/encoder/vp9_onyx_int.h"

void vp9_init_first_pass(VP9_COMP *cpi);
void vp9_first_pass(VP9_COMP *cpi);
void vp9_end_first_pass(VP9_COMP *cpi);

void vp9_init_second_pass(VP9_COMP *cpi);
void vp9_second_pass(VP9_COMP *cpi);
void vp9_end_second_pass(VP9_COMP *cpi);

#endif  // VP9_ENCODER_VP9_FIRSTPASS_H_
