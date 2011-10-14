/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vp8/common/onyxc_int.h"
#include "vp8/encoder/onyx_int.h"
#include "vp8/encoder/quantize.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_scale/yv12extend.h"
#include "vpx_scale/vpxscale.h"
#include "vp8/common/alloccommon.h"

extern void vp8_memcpy_neon(unsigned char *dst_ptr, unsigned char *src_ptr, int sz);


void
vpxyv12_copy_partial_frame_neon(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc, int Fraction)
{
    unsigned char *src_y, *dst_y;
    int yheight;
    int ystride;
    int border;
    int yoffset;
    int linestocopy;

    border   = src_ybc->border;
    yheight  = src_ybc->y_height;
    ystride  = src_ybc->y_stride;

    linestocopy = (yheight >> (Fraction + 4));

    if (linestocopy < 1)
        linestocopy = 1;

    linestocopy <<= 4;

    yoffset  = ystride * ((yheight >> 5) * 16 - 8);
    src_y = src_ybc->y_buffer + yoffset;
    dst_y = dst_ybc->y_buffer + yoffset;

    //vpx_memcpy (dst_y, src_y, ystride * (linestocopy +16));
    vp8_memcpy_neon((unsigned char *)dst_y, (unsigned char *)src_y, (int)(ystride *(linestocopy + 16)));
}
