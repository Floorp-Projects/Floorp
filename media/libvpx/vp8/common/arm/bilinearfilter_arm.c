/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <math.h>
#include "subpixel.h"

#define BLOCK_HEIGHT_WIDTH 4
#define VP8_FILTER_WEIGHT 128
#define VP8_FILTER_SHIFT  7

static const short bilinear_filters[8][2] =
{
    { 128,   0 },
    { 112,  16 },
    {  96,  32 },
    {  80,  48 },
    {  64,  64 },
    {  48,  80 },
    {  32,  96 },
    {  16, 112 }
};


extern void vp8_filter_block2d_bil_first_pass_armv6
(
    unsigned char *src_ptr,
    unsigned short *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int output_height,
    unsigned int output_width,
    const short *vp8_filter
);

extern void vp8_filter_block2d_bil_second_pass_armv6
(
    unsigned short *src_ptr,
    unsigned char  *output_ptr,
    int output_pitch,
    unsigned int  output_height,
    unsigned int  output_width,
    const short *vp8_filter
);

#if 0
void vp8_filter_block2d_bil_first_pass_6
(
    unsigned char *src_ptr,
    unsigned short *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int output_height,
    unsigned int output_width,
    const short *vp8_filter
)
{
    unsigned int i, j;

    for ( i=0; i<output_height; i++ )
    {
        for ( j=0; j<output_width; j++ )
        {
            /* Apply bilinear filter */
            output_ptr[j] = ( ( (int)src_ptr[0]          * vp8_filter[0]) +
                               ((int)src_ptr[1] * vp8_filter[1]) +
                                (VP8_FILTER_WEIGHT/2) ) >> VP8_FILTER_SHIFT;
            src_ptr++;
        }

        /* Next row... */
        src_ptr    += src_pixels_per_line - output_width;
        output_ptr += output_width;
    }
}

void vp8_filter_block2d_bil_second_pass_6
(
    unsigned short *src_ptr,
    unsigned char  *output_ptr,
    int output_pitch,
    unsigned int  output_height,
    unsigned int  output_width,
    const short *vp8_filter
)
{
    unsigned int  i,j;
    int  Temp;

    for ( i=0; i<output_height; i++ )
    {
        for ( j=0; j<output_width; j++ )
        {
            /* Apply filter */
            Temp =  ((int)src_ptr[0]         * vp8_filter[0]) +
                    ((int)src_ptr[output_width] * vp8_filter[1]) +
                    (VP8_FILTER_WEIGHT/2);
            output_ptr[j] = (unsigned int)(Temp >> VP8_FILTER_SHIFT);
            src_ptr++;
        }

        /* Next row... */
        /*src_ptr    += src_pixels_per_line - output_width;*/
        output_ptr += output_pitch;
    }
}
#endif

void vp8_filter_block2d_bil_armv6
(
    unsigned char *src_ptr,
    unsigned char *output_ptr,
    unsigned int   src_pixels_per_line,
    unsigned int   dst_pitch,
    const short      *HFilter,
    const short      *VFilter,
    int            Width,
    int            Height
)
{

    unsigned short FData[36*16]; /* Temp data bufffer used in filtering */

    /* First filter 1-D horizontally... */
    /* pixel_step = 1; */
    vp8_filter_block2d_bil_first_pass_armv6(src_ptr, FData, src_pixels_per_line, Height + 1, Width, HFilter);

    /* then 1-D vertically... */
    vp8_filter_block2d_bil_second_pass_armv6(FData, output_ptr, dst_pitch, Height, Width, VFilter);
}


void vp8_bilinear_predict4x4_armv6
(
    unsigned char  *src_ptr,
    int   src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int dst_pitch
)
{
    const short  *HFilter;
    const short  *VFilter;

    HFilter = bilinear_filters[xoffset];
    VFilter = bilinear_filters[yoffset];

    vp8_filter_block2d_bil_armv6(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter, 4, 4);
}

void vp8_bilinear_predict8x8_armv6
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

    HFilter = bilinear_filters[xoffset];
    VFilter = bilinear_filters[yoffset];

    vp8_filter_block2d_bil_armv6(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter, 8, 8);
}

void vp8_bilinear_predict8x4_armv6
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

    HFilter = bilinear_filters[xoffset];
    VFilter = bilinear_filters[yoffset];

    vp8_filter_block2d_bil_armv6(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter, 8, 4);
}

void vp8_bilinear_predict16x16_armv6
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

    HFilter = bilinear_filters[xoffset];
    VFilter = bilinear_filters[yoffset];

    vp8_filter_block2d_bil_armv6(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter, 16, 16);
}
