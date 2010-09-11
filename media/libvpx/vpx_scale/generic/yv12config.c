/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_scale/yv12config.h"
#include "vpx_mem/vpx_mem.h"

/****************************************************************************
*  Exports
****************************************************************************/

/****************************************************************************
 *
 ****************************************************************************/
int
vp8_yv12_de_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf)
{
    if (ybf)
    {
        if (ybf->buffer_alloc)
        {
            duck_free(ybf->buffer_alloc);
        }

        ybf->buffer_alloc = 0;
    }
    else
    {
        return -1;
    }

    return 0;
}

/****************************************************************************
 *
 ****************************************************************************/
int
vp8_yv12_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height, int border)
{
//NOTE:

    int yplane_size = (height + 2 * border) * (width + 2 * border);
    int uvplane_size = ((1 + height) / 2 + border) * ((1 + width) / 2 + border);

    if (ybf)
    {
        vp8_yv12_de_alloc_frame_buffer(ybf);

        ybf->y_width  = width;
        ybf->y_height = height;
        ybf->y_stride = width + 2 * border;

        ybf->uv_width = (1 + width) / 2;
        ybf->uv_height = (1 + height) / 2;
        ybf->uv_stride = ybf->uv_width + border;

        ybf->border = border;
        ybf->frame_size = yplane_size + 2 * uvplane_size;

        // Added 2 extra lines to framebuffer so that copy12x12 doesn't fail
        // when we have a large motion vector in V on the last v block.
        // Note : We never use these pixels anyway so this doesn't hurt.
        ybf->buffer_alloc = (unsigned char *) duck_memalign(32,  ybf->frame_size + (ybf->y_stride * 2) + 32, 0);

        if (ybf->buffer_alloc == NULL)
            return -1;

        ybf->y_buffer = ybf->buffer_alloc + (border * ybf->y_stride) + border;

        if (yplane_size & 0xf)
            yplane_size += 16 - (yplane_size & 0xf);

        ybf->u_buffer = ybf->buffer_alloc + yplane_size + (border / 2  * ybf->uv_stride) + border / 2;
        ybf->v_buffer = ybf->buffer_alloc + yplane_size + uvplane_size + (border / 2  * ybf->uv_stride) + border / 2;
    }
    else
    {
        return -2;
    }

    return 0;
}

/****************************************************************************
 *
 ****************************************************************************/
int
vp8_yv12_black_frame_buffer(YV12_BUFFER_CONFIG *ybf)
{
    if (ybf)
    {
        if (ybf->buffer_alloc)
        {
            duck_memset(ybf->y_buffer, 0x0, ybf->y_stride * ybf->y_height);
            duck_memset(ybf->u_buffer, 0x80, ybf->uv_stride * ybf->uv_height);
            duck_memset(ybf->v_buffer, 0x80, ybf->uv_stride * ybf->uv_height);
        }

        return 0;
    }

    return -1;
}
