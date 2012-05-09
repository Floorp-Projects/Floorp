/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_config.h"
#include <math.h>
#include "vp8/common/filter.h"
#include "vp8/common/subpixel.h"
#include "vpx_ports/mem.h"

extern void vp8_filter_block2d_first_pass_armv6
(
    unsigned char *src_ptr,
    short         *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int output_width,
    unsigned int output_height,
    const short *vp8_filter
);

// 8x8
extern void vp8_filter_block2d_first_pass_8x8_armv6
(
    unsigned char *src_ptr,
    short         *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int output_width,
    unsigned int output_height,
    const short *vp8_filter
);

// 16x16
extern void vp8_filter_block2d_first_pass_16x16_armv6
(
    unsigned char *src_ptr,
    short         *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int output_width,
    unsigned int output_height,
    const short *vp8_filter
);

extern void vp8_filter_block2d_second_pass_armv6
(
    short         *src_ptr,
    unsigned char *output_ptr,
    unsigned int output_pitch,
    unsigned int cnt,
    const short *vp8_filter
);

extern void vp8_filter4_block2d_second_pass_armv6
(
    short         *src_ptr,
    unsigned char *output_ptr,
    unsigned int output_pitch,
    unsigned int cnt,
    const short *vp8_filter
);

extern void vp8_filter_block2d_first_pass_only_armv6
(
    unsigned char *src_ptr,
    unsigned char *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int cnt,
    unsigned int output_pitch,
    const short *vp8_filter
);


extern void vp8_filter_block2d_second_pass_only_armv6
(
    unsigned char *src_ptr,
    unsigned char *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int cnt,
    unsigned int output_pitch,
    const short *vp8_filter
);

#if HAVE_ARMV6
void vp8_sixtap_predict_armv6
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const short  *HFilter;
    const short  *VFilter;
    DECLARE_ALIGNED_ARRAY(4, short, FData, 12*4); /* Temp data buffer used in filtering */


    HFilter = vp8_sub_pel_filters[xoffset];   /* 6 tap */
    VFilter = vp8_sub_pel_filters[yoffset];   /* 6 tap */

    /* Vfilter is null. First pass only */
    if (xoffset && !yoffset)
    {
        /*vp8_filter_block2d_first_pass_armv6 ( src_ptr, FData+2, src_pixels_per_line, 4, 4, HFilter );
        vp8_filter_block2d_second_pass_armv6 ( FData+2, dst_ptr, dst_pitch, 4, VFilter );*/

        vp8_filter_block2d_first_pass_only_armv6(src_ptr, dst_ptr, src_pixels_per_line, 4, dst_pitch, HFilter);
    }
    /* Hfilter is null. Second pass only */
    else if (!xoffset && yoffset)
    {
        vp8_filter_block2d_second_pass_only_armv6(src_ptr, dst_ptr, src_pixels_per_line, 4, dst_pitch, VFilter);
    }
    else
    {
        /* Vfilter is a 4 tap filter */
        if (yoffset & 0x1)
        {
            vp8_filter_block2d_first_pass_armv6(src_ptr - src_pixels_per_line, FData + 1, src_pixels_per_line, 4, 7, HFilter);
            vp8_filter4_block2d_second_pass_armv6(FData + 2, dst_ptr, dst_pitch, 4, VFilter);
        }
        /* Vfilter is 6 tap filter */
        else
        {
            vp8_filter_block2d_first_pass_armv6(src_ptr - (2 * src_pixels_per_line), FData, src_pixels_per_line, 4, 9, HFilter);
            vp8_filter_block2d_second_pass_armv6(FData + 2, dst_ptr, dst_pitch, 4, VFilter);
        }
    }
}

void vp8_sixtap_predict8x8_armv6
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const short  *HFilter;
    const short  *VFilter;
    DECLARE_ALIGNED_ARRAY(4, short, FData, 16*8); /* Temp data buffer used in filtering */

    HFilter = vp8_sub_pel_filters[xoffset];   /* 6 tap */
    VFilter = vp8_sub_pel_filters[yoffset];   /* 6 tap */

    if (xoffset && !yoffset)
    {
        vp8_filter_block2d_first_pass_only_armv6(src_ptr, dst_ptr, src_pixels_per_line, 8, dst_pitch, HFilter);
    }
    /* Hfilter is null. Second pass only */
    else if (!xoffset && yoffset)
    {
        vp8_filter_block2d_second_pass_only_armv6(src_ptr, dst_ptr, src_pixels_per_line, 8, dst_pitch, VFilter);
    }
    else
    {
        if (yoffset & 0x1)
        {
            vp8_filter_block2d_first_pass_8x8_armv6(src_ptr - src_pixels_per_line, FData + 1, src_pixels_per_line, 8, 11, HFilter);
            vp8_filter4_block2d_second_pass_armv6(FData + 2, dst_ptr, dst_pitch, 8, VFilter);
        }
        else
        {
            vp8_filter_block2d_first_pass_8x8_armv6(src_ptr - (2 * src_pixels_per_line), FData, src_pixels_per_line, 8, 13, HFilter);
            vp8_filter_block2d_second_pass_armv6(FData + 2, dst_ptr, dst_pitch, 8, VFilter);
        }
    }
}


void vp8_sixtap_predict16x16_armv6
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const short  *HFilter;
    const short  *VFilter;
    DECLARE_ALIGNED_ARRAY(4, short, FData, 24*16);    /* Temp data buffer used in filtering */

    HFilter = vp8_sub_pel_filters[xoffset];   /* 6 tap */
    VFilter = vp8_sub_pel_filters[yoffset];   /* 6 tap */

    if (xoffset && !yoffset)
    {
        vp8_filter_block2d_first_pass_only_armv6(src_ptr, dst_ptr, src_pixels_per_line, 16, dst_pitch, HFilter);
    }
    /* Hfilter is null. Second pass only */
    else if (!xoffset && yoffset)
    {
        vp8_filter_block2d_second_pass_only_armv6(src_ptr, dst_ptr, src_pixels_per_line, 16, dst_pitch, VFilter);
    }
    else
    {
        if (yoffset & 0x1)
        {
            vp8_filter_block2d_first_pass_16x16_armv6(src_ptr - src_pixels_per_line, FData + 1, src_pixels_per_line, 16, 19, HFilter);
            vp8_filter4_block2d_second_pass_armv6(FData + 2, dst_ptr, dst_pitch, 16, VFilter);
        }
        else
        {
            vp8_filter_block2d_first_pass_16x16_armv6(src_ptr - (2 * src_pixels_per_line), FData, src_pixels_per_line, 16, 21, HFilter);
            vp8_filter_block2d_second_pass_armv6(FData + 2, dst_ptr, dst_pitch, 16, VFilter);
        }
    }

}
#endif
