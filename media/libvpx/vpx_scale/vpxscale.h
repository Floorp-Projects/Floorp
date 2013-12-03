/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPXSCALE_H
#define VPXSCALE_H

#include "vpx_scale/yv12config.h"

extern void vp8_yv12_scale_or_center(YV12_BUFFER_CONFIG *src_yuv_config,
                                     YV12_BUFFER_CONFIG *dst_yuv_config,
                                     int expanded_frame_width,
                                     int expanded_frame_height,
                                     int scaling_mode,
                                     int HScale,
                                     int HRatio,
                                     int VScale,
                                     int VRatio);

extern void vp8_scale_frame(YV12_BUFFER_CONFIG *src,
                            YV12_BUFFER_CONFIG *dst,
                            unsigned char *temp_area,
                            unsigned char temp_height,
                            unsigned int hscale,
                            unsigned int hratio,
                            unsigned int vscale,
                            unsigned int vratio,
                            unsigned int interlaced);

#endif
