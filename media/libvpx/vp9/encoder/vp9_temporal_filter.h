/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_TEMPORAL_FILTER_H_
#define VP9_ENCODER_VP9_TEMPORAL_FILTER_H_

void vp9_temporal_filter_prepare(VP9_COMP *cpi, int distance);
void configure_arnr_filter(VP9_COMP *cpi, const unsigned int this_frame,
                           const int group_boost);

#endif  // VP9_ENCODER_VP9_TEMPORAL_FILTER_H_
