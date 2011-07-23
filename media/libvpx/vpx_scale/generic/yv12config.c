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
/*NOTE:*/

    if (ybf)
    {
        int y_stride = ((width + 2 * border) + 31) & ~31;
        int yplane_size = (height + 2 * border) * y_stride;
        /** There is currently a bunch of code which assumes
          *  uv_stride == y_stride/2, so enforce this here. */
        int uv_width = width >> 1;
        int uv_height = height >> 1;
        int uv_stride = y_stride >> 1;
        int uvplane_size = ((1 + height) / 2 + border) * uv_stride;

        vp8_yv12_de_alloc_frame_buffer(ybf);

        /** Only support allocating buffers that have a height and width that
          *  are multiples of 16, and a border that's a multiple of 32.
          * The border restriction is required to get 16-byte alignment of the
          *  start of the chroma rows without intoducing an arbitrary gap
          *  between planes, which would break the semantics of things like
          *  vpx_img_set_rect(). */
        if ((width & 0xf) | (height & 0xf) | (border & 0x1f))
            return -3;

        ybf->y_width  = width;
        ybf->y_height = height;
        ybf->y_stride = y_stride;

        ybf->uv_width = uv_width;
        ybf->uv_height = uv_height;
        ybf->uv_stride = uv_stride;

        ybf->border = border;
        ybf->frame_size = yplane_size + 2 * uvplane_size;

        /* Added 2 extra lines to framebuffer so that copy12x12 doesn't fail
         * when we have a large motion vector in V on the last v block.
         * Note : We never use these pixels anyway so this doesn't hurt.
         */
        ybf->buffer_alloc = (unsigned char *) vpx_memalign(32,  ybf->frame_size);

        if (ybf->buffer_alloc == NULL)
            return -1;

        ybf->y_buffer = ybf->buffer_alloc + (border * y_stride) + border;
        ybf->u_buffer = ybf->buffer_alloc + yplane_size + (border / 2  * uv_stride) + border / 2;
        ybf->v_buffer = ybf->buffer_alloc + yplane_size + uvplane_size + (border / 2  * uv_stride) + border / 2;
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
