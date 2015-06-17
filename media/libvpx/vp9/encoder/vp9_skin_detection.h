/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_SKIN_MAP_H_
#define VP9_ENCODER_VP9_SKIN_MAP_H_

#include "vp9/common/vp9_blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

struct VP9_COMP;

// #define OUTPUT_YUV_SKINMAP

int vp9_skin_pixel(const uint8_t y, const uint8_t cb, const uint8_t cr);

#ifdef OUTPUT_YUV_SKINMAP
// For viewing skin map on input source.
void vp9_compute_skin_map(VP9_COMP *const cpi, FILE *yuv_skinmap_file);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_SKIN_MAP_H_
