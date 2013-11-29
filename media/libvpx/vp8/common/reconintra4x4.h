/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_RECONINTRA4x4_H
#define __INC_RECONINTRA4x4_H
#include "vp8/common/blockd.h"

static void intra_prediction_down_copy(MACROBLOCKD *xd,
                                             unsigned char *above_right_src)
{
    int dst_stride = xd->dst.y_stride;
    unsigned char *above_right_dst = xd->dst.y_buffer - dst_stride + 16;

    unsigned int *src_ptr = (unsigned int *)above_right_src;
    unsigned int *dst_ptr0 = (unsigned int *)(above_right_dst + 4 * dst_stride);
    unsigned int *dst_ptr1 = (unsigned int *)(above_right_dst + 8 * dst_stride);
    unsigned int *dst_ptr2 = (unsigned int *)(above_right_dst + 12 * dst_stride);

    *dst_ptr0 = *src_ptr;
    *dst_ptr1 = *src_ptr;
    *dst_ptr2 = *src_ptr;
}

#endif
