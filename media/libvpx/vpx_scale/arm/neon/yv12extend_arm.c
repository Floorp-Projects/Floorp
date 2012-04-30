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
#include "vpx_mem/vpx_mem.h"
#include "vpx_scale/vpxscale.h"

extern void vp8_yv12_copy_frame_func_neon(YV12_BUFFER_CONFIG *src_ybc,
                                          YV12_BUFFER_CONFIG *dst_ybc);

void vp8_yv12_copy_frame_neon(YV12_BUFFER_CONFIG *src_ybc,
                              YV12_BUFFER_CONFIG *dst_ybc)
{
    vp8_yv12_copy_frame_func_neon(src_ybc, dst_ybc);

    vp8_yv12_extend_frame_borders_neon(dst_ybc);
}
