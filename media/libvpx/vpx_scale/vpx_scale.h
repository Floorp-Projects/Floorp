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

extern void vpx_scale_frame(YV12_BUFFER_CONFIG *src,
                            YV12_BUFFER_CONFIG *dst,
                            unsigned char *temp_area,
                            unsigned char temp_height,
                            unsigned int hscale,
                            unsigned int hratio,
                            unsigned int vscale,
                            unsigned int vratio,
                            unsigned int interlaced);

#endif
