/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "extend.h"
#include "vpx_mem/vpx_mem.h"


static void extend_plane_borders
(
    unsigned char *s, /* source */
    int sp,           /* pitch */
    int h,            /* height */
    int w,            /* width */
    int et,           /* extend top border */
    int el,           /* extend left border */
    int eb,           /* extend bottom border */
    int er            /* extend right border */
)
{

    int i;
    unsigned char *src_ptr1, *src_ptr2;
    unsigned char *dest_ptr1, *dest_ptr2;
    int linesize;

    /* copy the left and right most columns out */
    src_ptr1 = s;
    src_ptr2 = s + w - 1;
    dest_ptr1 = s - el;
    dest_ptr2 = s + w;

    for (i = 0; i < h - 0 + 1; i++)
    {
        /* Some linkers will complain if we call vpx_memset with el set to a
         * constant 0.
         */
        if (el)
            vpx_memset(dest_ptr1, src_ptr1[0], el);
        vpx_memset(dest_ptr2, src_ptr2[0], er);
        src_ptr1  += sp;
        src_ptr2  += sp;
        dest_ptr1 += sp;
        dest_ptr2 += sp;
    }

    /* Now copy the top and bottom source lines into each line of the respective borders */
    src_ptr1 = s - el;
    src_ptr2 = s + sp * (h - 1) - el;
    dest_ptr1 = s + sp * (-et) - el;
    dest_ptr2 = s + sp * (h) - el;
    linesize = el + er + w + 1;

    for (i = 0; i < (int)et; i++)
    {
        vpx_memcpy(dest_ptr1, src_ptr1, linesize);
        dest_ptr1 += sp;
    }

    for (i = 0; i < (int)eb; i++)
    {
        vpx_memcpy(dest_ptr2, src_ptr2, linesize);
        dest_ptr2 += sp;
    }
}


void vp8_extend_to_multiple_of16(YV12_BUFFER_CONFIG *ybf, int width, int height)
{
    int er = 0xf & (16 - (width & 0xf));
    int eb = 0xf & (16 - (height & 0xf));

    /* check for non multiples of 16 */
    if (er != 0 || eb != 0)
    {
        extend_plane_borders(ybf->y_buffer, ybf->y_stride, height, width, 0, 0, eb, er);

        /* adjust for uv */
        height = (height + 1) >> 1;
        width  = (width  + 1) >> 1;
        er = 0x7 & (8 - (width  & 0x7));
        eb = 0x7 & (8 - (height & 0x7));

        if (er || eb)
        {
            extend_plane_borders(ybf->u_buffer, ybf->uv_stride, height, width, 0, 0, eb, er);
            extend_plane_borders(ybf->v_buffer, ybf->uv_stride, height, width, 0, 0, eb, er);
        }
    }
}

/* note the extension is only for the last row, for intra prediction purpose */
void vp8_extend_mb_row(YV12_BUFFER_CONFIG *ybf, unsigned char *YPtr, unsigned char *UPtr, unsigned char *VPtr)
{
    int i;

    YPtr += ybf->y_stride * 14;
    UPtr += ybf->uv_stride * 6;
    VPtr += ybf->uv_stride * 6;

    for (i = 0; i < 4; i++)
    {
        YPtr[i] = YPtr[-1];
        UPtr[i] = UPtr[-1];
        VPtr[i] = VPtr[-1];
    }

    YPtr += ybf->y_stride;
    UPtr += ybf->uv_stride;
    VPtr += ybf->uv_stride;

    for (i = 0; i < 4; i++)
    {
        YPtr[i] = YPtr[-1];
        UPtr[i] = UPtr[-1];
        VPtr[i] = VPtr[-1];
    }
}
