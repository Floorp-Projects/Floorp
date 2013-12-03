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
#include "vp8/common/variance.h"
#include "vp8/common/pragmas.h"
#include "vpx_ports/mem.h"
#include "vp8/common/x86/filter_x86.h"

extern void filter_block1d_h6_mmx
(
    const unsigned char *src_ptr,
    unsigned short *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int pixel_step,
    unsigned int output_height,
    unsigned int output_width,
    short *filter
);
extern void filter_block1d_v6_mmx
(
    const short *src_ptr,
    unsigned char *output_ptr,
    unsigned int pixels_per_line,
    unsigned int pixel_step,
    unsigned int output_height,
    unsigned int output_width,
    short *filter
);

extern unsigned int vp8_get_mb_ss_mmx(const short *src_ptr);
extern unsigned int vp8_get8x8var_mmx
(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *SSE,
    int *Sum
);
extern unsigned int vp8_get4x4var_mmx
(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *SSE,
    int *Sum
);
extern void vp8_filter_block2d_bil4x4_var_mmx
(
    const unsigned char *ref_ptr,
    int ref_pixels_per_line,
    const unsigned char *src_ptr,
    int src_pixels_per_line,
    const short *HFilter,
    const short *VFilter,
    int *sum,
    unsigned int *sumsquared
);
extern void vp8_filter_block2d_bil_var_mmx
(
    const unsigned char *ref_ptr,
    int ref_pixels_per_line,
    const unsigned char *src_ptr,
    int src_pixels_per_line,
    unsigned int Height,
    const short *HFilter,
    const short *VFilter,
    int *sum,
    unsigned int *sumsquared
);


unsigned int vp8_variance4x4_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    unsigned int var;
    int avg;

    vp8_get4x4var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &var, &avg) ;
    *sse = var;
    return (var - ((unsigned int)(avg * avg) >> 4));

}

unsigned int vp8_variance8x8_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    unsigned int var;
    int avg;

    vp8_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &var, &avg) ;
    *sse = var;

    return (var - ((unsigned int)(avg * avg) >> 6));

}

unsigned int vp8_mse16x16_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    unsigned int sse0, sse1, sse2, sse3, var;
    int sum0, sum1, sum2, sum3;


    vp8_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &sse0, &sum0) ;
    vp8_get8x8var_mmx(src_ptr + 8, source_stride, ref_ptr + 8, recon_stride, &sse1, &sum1);
    vp8_get8x8var_mmx(src_ptr + 8 * source_stride, source_stride, ref_ptr + 8 * recon_stride, recon_stride, &sse2, &sum2) ;
    vp8_get8x8var_mmx(src_ptr + 8 * source_stride + 8, source_stride, ref_ptr + 8 * recon_stride + 8, recon_stride, &sse3, &sum3);

    var = sse0 + sse1 + sse2 + sse3;
    *sse = var;
    return var;
}


unsigned int vp8_variance16x16_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    unsigned int sse0, sse1, sse2, sse3, var;
    int sum0, sum1, sum2, sum3, avg;


    vp8_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &sse0, &sum0) ;
    vp8_get8x8var_mmx(src_ptr + 8, source_stride, ref_ptr + 8, recon_stride, &sse1, &sum1);
    vp8_get8x8var_mmx(src_ptr + 8 * source_stride, source_stride, ref_ptr + 8 * recon_stride, recon_stride, &sse2, &sum2) ;
    vp8_get8x8var_mmx(src_ptr + 8 * source_stride + 8, source_stride, ref_ptr + 8 * recon_stride + 8, recon_stride, &sse3, &sum3);

    var = sse0 + sse1 + sse2 + sse3;
    avg = sum0 + sum1 + sum2 + sum3;
    *sse = var;
    return (var - ((unsigned int)(avg * avg) >> 8));
}

unsigned int vp8_variance16x8_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    unsigned int sse0, sse1, var;
    int sum0, sum1, avg;

    vp8_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &sse0, &sum0) ;
    vp8_get8x8var_mmx(src_ptr + 8, source_stride, ref_ptr + 8, recon_stride, &sse1, &sum1);

    var = sse0 + sse1;
    avg = sum0 + sum1;
    *sse = var;
    return (var - ((unsigned int)(avg * avg) >> 7));

}


unsigned int vp8_variance8x16_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    unsigned int sse0, sse1, var;
    int sum0, sum1, avg;

    vp8_get8x8var_mmx(src_ptr, source_stride, ref_ptr, recon_stride, &sse0, &sum0) ;
    vp8_get8x8var_mmx(src_ptr + 8 * source_stride, source_stride, ref_ptr + 8 * recon_stride, recon_stride, &sse1, &sum1) ;

    var = sse0 + sse1;
    avg = sum0 + sum1;
    *sse = var;

    return (var - ((unsigned int)(avg * avg) >> 7));

}


unsigned int vp8_sub_pixel_variance4x4_mmx
(
    const unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    const unsigned char *dst_ptr,
    int dst_pixels_per_line,
    unsigned int *sse)

{
    int xsum;
    unsigned int xxsum;
    vp8_filter_block2d_bil4x4_var_mmx(
        src_ptr, src_pixels_per_line,
        dst_ptr, dst_pixels_per_line,
        vp8_bilinear_filters_x86_4[xoffset], vp8_bilinear_filters_x86_4[yoffset],
        &xsum, &xxsum
    );
    *sse = xxsum;
    return (xxsum - ((unsigned int)(xsum * xsum) >> 4));
}


unsigned int vp8_sub_pixel_variance8x8_mmx
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

    int xsum;
    unsigned int xxsum;
    vp8_filter_block2d_bil_var_mmx(
        src_ptr, src_pixels_per_line,
        dst_ptr, dst_pixels_per_line, 8,
        vp8_bilinear_filters_x86_4[xoffset], vp8_bilinear_filters_x86_4[yoffset],
        &xsum, &xxsum
    );
    *sse = xxsum;
    return (xxsum - ((unsigned int)(xsum * xsum) >> 6));
}

unsigned int vp8_sub_pixel_variance16x16_mmx
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

    int xsum0, xsum1;
    unsigned int xxsum0, xxsum1;


    vp8_filter_block2d_bil_var_mmx(
        src_ptr, src_pixels_per_line,
        dst_ptr, dst_pixels_per_line, 16,
        vp8_bilinear_filters_x86_4[xoffset], vp8_bilinear_filters_x86_4[yoffset],
        &xsum0, &xxsum0
    );


    vp8_filter_block2d_bil_var_mmx(
        src_ptr + 8, src_pixels_per_line,
        dst_ptr + 8, dst_pixels_per_line, 16,
        vp8_bilinear_filters_x86_4[xoffset], vp8_bilinear_filters_x86_4[yoffset],
        &xsum1, &xxsum1
    );

    xsum0 += xsum1;
    xxsum0 += xxsum1;

    *sse = xxsum0;
    return (xxsum0 - ((unsigned int)(xsum0 * xsum0) >> 8));


}

unsigned int vp8_sub_pixel_mse16x16_mmx(
    const unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    const unsigned char *dst_ptr,
    int dst_pixels_per_line,
    unsigned int *sse
)
{
    vp8_sub_pixel_variance16x16_mmx(src_ptr, src_pixels_per_line, xoffset, yoffset, dst_ptr, dst_pixels_per_line, sse);
    return *sse;
}

unsigned int vp8_sub_pixel_variance16x8_mmx
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
    int xsum0, xsum1;
    unsigned int xxsum0, xxsum1;


    vp8_filter_block2d_bil_var_mmx(
        src_ptr, src_pixels_per_line,
        dst_ptr, dst_pixels_per_line, 8,
        vp8_bilinear_filters_x86_4[xoffset], vp8_bilinear_filters_x86_4[yoffset],
        &xsum0, &xxsum0
    );


    vp8_filter_block2d_bil_var_mmx(
        src_ptr + 8, src_pixels_per_line,
        dst_ptr + 8, dst_pixels_per_line, 8,
        vp8_bilinear_filters_x86_4[xoffset], vp8_bilinear_filters_x86_4[yoffset],
        &xsum1, &xxsum1
    );

    xsum0 += xsum1;
    xxsum0 += xxsum1;

    *sse = xxsum0;
    return (xxsum0 - ((unsigned int)(xsum0 * xsum0) >> 7));
}

unsigned int vp8_sub_pixel_variance8x16_mmx
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
    int xsum;
    unsigned int xxsum;
    vp8_filter_block2d_bil_var_mmx(
        src_ptr, src_pixels_per_line,
        dst_ptr, dst_pixels_per_line, 16,
        vp8_bilinear_filters_x86_4[xoffset], vp8_bilinear_filters_x86_4[yoffset],
        &xsum, &xxsum
    );
    *sse = xxsum;
    return (xxsum - ((unsigned int)(xsum * xsum) >> 7));
}


unsigned int vp8_variance_halfpixvar16x16_h_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    return vp8_sub_pixel_variance16x16_mmx(src_ptr, source_stride, 4, 0,
                                           ref_ptr, recon_stride, sse);
}


unsigned int vp8_variance_halfpixvar16x16_v_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    return vp8_sub_pixel_variance16x16_mmx(src_ptr, source_stride, 0, 4,
                                           ref_ptr, recon_stride, sse);
}


unsigned int vp8_variance_halfpixvar16x16_hv_mmx(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    unsigned int *sse)
{
    return vp8_sub_pixel_variance16x16_mmx(src_ptr, source_stride, 4, 4,
                                           ref_ptr, recon_stride, sse);
}
