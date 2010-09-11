/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef YV12_EXTEND_H
#define YV12_EXTEND_H

#include "vpx_scale/yv12config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void vp8_yv12_extend_frame_borders(YV12_BUFFER_CONFIG *ybf);

    /* Copy Y,U,V buffer data from src to dst, filling border of dst as well. */

    void vp8_yv12_copy_frame(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc);
    void vp8_yv12_copy_frame_yonly(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc);

#ifdef __cplusplus
}
#endif

#endif
