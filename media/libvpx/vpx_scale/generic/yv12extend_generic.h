/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef YV12_EXTEND_GENERIC_H
#define YV12_EXTEND_GENERIC_H

#include "vpx_scale/yv12config.h"

    void vp8_yv12_extend_frame_borders(YV12_BUFFER_CONFIG *ybf);

    /* Copy Y,U,V buffer data from src to dst, filling border of dst as well. */
    void vp8_yv12_copy_frame(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc);

    /* Copy Y buffer data from src_ybc to dst_ybc without filling border data */
    void vp8_yv12_copy_y_c(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc);

#endif /* YV12_EXTEND_GENERIC_H */
