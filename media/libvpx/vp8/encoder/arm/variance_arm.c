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
#include "vp8/encoder/variance.h"
#include "vp8/common/filter.h"
#include "vp8/common/arm/bilinearfilter_arm.h"

#if HAVE_ARMV6

unsigned int vp8_sub_pixel_variance8x8_armv6
(
    const unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    const unsigned char *dst_ptr,
    int dst_pixels_per_line,
    unsigned int *sse
)
{
    unsigned short first_pass[10*8];
    unsigned char  second_pass[8*8];
    const short *HFilter, *VFilter;

    HFilter = vp8_bilinear_filters[xoffset];
    VFilter = vp8_bilinear_filters[yoffset];

    vp8_filter_block2d_bil_first_pass_armv6(src_ptr, first_pass,
                                            src_pixels_per_line,
                                            9, 8, HFilter);
    vp8_filter_block2d_bil_second_pass_armv6(first_pass, second_pass,
                                             8, 8, 8, VFilter);

    return vp8_variance8x8_armv6(second_pass, 8, dst_ptr,
                                   dst_pixels_per_line, sse);
}

unsigned int vp8_sub_pixel_variance16x16_armv6
(
    const unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    const unsigned char *dst_ptr,
    int dst_pixels_per_line,
    unsigned int *sse
)
{
    unsigned short first_pass[36*16];
    unsigned char  second_pass[20*16];
    const short *HFilter, *VFilter;
    unsigned int var;

    if (xoffset == 4 && yoffset == 0)
    {
        var = vp8_variance_halfpixvar16x16_h_armv6(src_ptr, src_pixels_per_line,
                                                   dst_ptr, dst_pixels_per_line, sse);
    }
    else if (xoffset == 0 && yoffset == 4)
    {
        var = vp8_variance_halfpixvar16x16_v_armv6(src_ptr, src_pixels_per_line,
                                                   dst_ptr, dst_pixels_per_line, sse);
    }
    else if (xoffset == 4 && yoffset == 4)
    {
        var = vp8_variance_halfpixvar16x16_hv_armv6(src_ptr, src_pixels_per_line,
                                                   dst_ptr, dst_pixels_per_line, sse);
    }
    else
    {
        HFilter = vp8_bilinear_filters[xoffset];
        VFilter = vp8_bilinear_filters[yoffset];

        vp8_filter_block2d_bil_first_pass_armv6(src_ptr, first_pass,
                                                src_pixels_per_line,
                                                17, 16, HFilter);
        vp8_filter_block2d_bil_second_pass_armv6(first_pass, second_pass,
                                                 16, 16, 16, VFilter);

        var = vp8_variance16x16_armv6(second_pass, 16, dst_ptr,
                                       dst_pixels_per_line, sse);
    }
    return var;
}

#endif /* HAVE_ARMV6 */


#if HAVE_ARMV7

unsigned int vp8_sub_pixel_variance16x16_neon
(
    const unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    const unsigned char *dst_ptr,
    int dst_pixels_per_line,
    unsigned int *sse
)
{
  if (xoffset == 4 && yoffset == 0)
    return vp8_variance_halfpixvar16x16_h_neon(src_ptr, src_pixels_per_line, dst_ptr, dst_pixels_per_line, sse);
  else if (xoffset == 0 && yoffset == 4)
    return vp8_variance_halfpixvar16x16_v_neon(src_ptr, src_pixels_per_line, dst_ptr, dst_pixels_per_line, sse);
  else if (xoffset == 4 && yoffset == 4)
    return vp8_variance_halfpixvar16x16_hv_neon(src_ptr, src_pixels_per_line, dst_ptr, dst_pixels_per_line, sse);
  else
    return vp8_sub_pixel_variance16x16_neon_func(src_ptr, src_pixels_per_line, xoffset, yoffset, dst_ptr, dst_pixels_per_line, sse);
}

#endif
