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

/****************************************************************************
*  Exports
****************************************************************************/

/****************************************************************************
 *
 ****************************************************************************/
void
vp8_yv12_extend_frame_borders(YV12_BUFFER_CONFIG *ybf)
{
    int i;
    unsigned char *src_ptr1, *src_ptr2;
    unsigned char *dest_ptr1, *dest_ptr2;

    unsigned int Border;
    int plane_stride;
    int plane_height;
    int plane_width;

    /***********/
    /* Y Plane */
    /***********/
    Border = ybf->border;
    plane_stride = ybf->y_stride;
    plane_height = ybf->y_height;
    plane_width = ybf->y_width;

    /* copy the left and right most columns out */
    src_ptr1 = ybf->y_buffer;
    src_ptr2 = src_ptr1 + plane_width - 1;
    dest_ptr1 = src_ptr1 - Border;
    dest_ptr2 = src_ptr2 + 1;

    for (i = 0; i < plane_height; i++)
    {
        vpx_memset(dest_ptr1, src_ptr1[0], Border);
        vpx_memset(dest_ptr2, src_ptr2[0], Border);
        src_ptr1  += plane_stride;
        src_ptr2  += plane_stride;
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }

    /* Now copy the top and bottom source lines into each line of the respective borders */
    src_ptr1 = ybf->y_buffer - Border;
    src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride;
    dest_ptr1 = src_ptr1 - (Border * plane_stride);
    dest_ptr2 = src_ptr2 + plane_stride;

    for (i = 0; i < (int)Border; i++)
    {
        vpx_memcpy(dest_ptr1, src_ptr1, plane_stride);
        vpx_memcpy(dest_ptr2, src_ptr2, plane_stride);
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }


    /***********/
    /* U Plane */
    /***********/
    plane_stride = ybf->uv_stride;
    plane_height = ybf->uv_height;
    plane_width = ybf->uv_width;
    Border /= 2;

    /* copy the left and right most columns out */
    src_ptr1 = ybf->u_buffer;
    src_ptr2 = src_ptr1 + plane_width - 1;
    dest_ptr1 = src_ptr1 - Border;
    dest_ptr2 = src_ptr2 + 1;

    for (i = 0; i < plane_height; i++)
    {
        vpx_memset(dest_ptr1, src_ptr1[0], Border);
        vpx_memset(dest_ptr2, src_ptr2[0], Border);
        src_ptr1  += plane_stride;
        src_ptr2  += plane_stride;
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }

    /* Now copy the top and bottom source lines into each line of the respective borders */
    src_ptr1 = ybf->u_buffer - Border;
    src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride;
    dest_ptr1 = src_ptr1 - (Border * plane_stride);
    dest_ptr2 = src_ptr2 + plane_stride;

    for (i = 0; i < (int)(Border); i++)
    {
        vpx_memcpy(dest_ptr1, src_ptr1, plane_stride);
        vpx_memcpy(dest_ptr2, src_ptr2, plane_stride);
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }

    /***********/
    /* V Plane */
    /***********/

    /* copy the left and right most columns out */
    src_ptr1 = ybf->v_buffer;
    src_ptr2 = src_ptr1 + plane_width - 1;
    dest_ptr1 = src_ptr1 - Border;
    dest_ptr2 = src_ptr2 + 1;

    for (i = 0; i < plane_height; i++)
    {
        vpx_memset(dest_ptr1, src_ptr1[0], Border);
        vpx_memset(dest_ptr2, src_ptr2[0], Border);
        src_ptr1  += plane_stride;
        src_ptr2  += plane_stride;
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }

    /* Now copy the top and bottom source lines into each line of the respective borders */
    src_ptr1 = ybf->v_buffer - Border;
    src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride;
    dest_ptr1 = src_ptr1 - (Border * plane_stride);
    dest_ptr2 = src_ptr2 + plane_stride;

    for (i = 0; i < (int)(Border); i++)
    {
        vpx_memcpy(dest_ptr1, src_ptr1, plane_stride);
        vpx_memcpy(dest_ptr2, src_ptr2, plane_stride);
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }
}


static void
extend_frame_borders_yonly(YV12_BUFFER_CONFIG *ybf)
{
    int i;
    unsigned char *src_ptr1, *src_ptr2;
    unsigned char *dest_ptr1, *dest_ptr2;

    unsigned int Border;
    int plane_stride;
    int plane_height;
    int plane_width;

    /***********/
    /* Y Plane */
    /***********/
    Border = ybf->border;
    plane_stride = ybf->y_stride;
    plane_height = ybf->y_height;
    plane_width = ybf->y_width;

    /* copy the left and right most columns out */
    src_ptr1 = ybf->y_buffer;
    src_ptr2 = src_ptr1 + plane_width - 1;
    dest_ptr1 = src_ptr1 - Border;
    dest_ptr2 = src_ptr2 + 1;

    for (i = 0; i < plane_height; i++)
    {
        vpx_memset(dest_ptr1, src_ptr1[0], Border);
        vpx_memset(dest_ptr2, src_ptr2[0], Border);
        src_ptr1  += plane_stride;
        src_ptr2  += plane_stride;
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }

    /* Now copy the top and bottom source lines into each line of the respective borders */
    src_ptr1 = ybf->y_buffer - Border;
    src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride;
    dest_ptr1 = src_ptr1 - (Border * plane_stride);
    dest_ptr2 = src_ptr2 + plane_stride;

    for (i = 0; i < (int)Border; i++)
    {
        vpx_memcpy(dest_ptr1, src_ptr1, plane_stride);
        vpx_memcpy(dest_ptr2, src_ptr2, plane_stride);
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }

    plane_stride /= 2;
    plane_height /= 2;
    plane_width /= 2;
    Border /= 2;

}



/****************************************************************************
 *
 *  ROUTINE       : vp8_yv12_copy_frame
 *
 *  INPUTS        :
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies the source image into the destination image and
 *                  updates the destination's UMV borders.
 *
 *  SPECIAL NOTES : The frames are assumed to be identical in size.
 *
 ****************************************************************************/
void
vp8_yv12_copy_frame(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc)
{
    int row;
    unsigned char *source, *dest;

    source = src_ybc->y_buffer;
    dest = dst_ybc->y_buffer;

    for (row = 0; row < src_ybc->y_height; row++)
    {
        vpx_memcpy(dest, source, src_ybc->y_width);
        source += src_ybc->y_stride;
        dest   += dst_ybc->y_stride;
    }

    source = src_ybc->u_buffer;
    dest = dst_ybc->u_buffer;

    for (row = 0; row < src_ybc->uv_height; row++)
    {
        vpx_memcpy(dest, source, src_ybc->uv_width);
        source += src_ybc->uv_stride;
        dest   += dst_ybc->uv_stride;
    }

    source = src_ybc->v_buffer;
    dest = dst_ybc->v_buffer;

    for (row = 0; row < src_ybc->uv_height; row++)
    {
        vpx_memcpy(dest, source, src_ybc->uv_width);
        source += src_ybc->uv_stride;
        dest   += dst_ybc->uv_stride;
    }

    vp8_yv12_extend_frame_borders_ptr(dst_ybc);
}

void
vp8_yv12_copy_frame_yonly(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc)
{
    int row;
    unsigned char *source, *dest;


    source = src_ybc->y_buffer;
    dest = dst_ybc->y_buffer;

    for (row = 0; row < src_ybc->y_height; row++)
    {
        vpx_memcpy(dest, source, src_ybc->y_width);
        source += src_ybc->y_stride;
        dest   += dst_ybc->y_stride;
    }

    extend_frame_borders_yonly(dst_ybc);
}
