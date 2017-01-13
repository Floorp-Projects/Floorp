/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "onyx_int.h"

void vp8_ssim_parms_16x16_c
(
    unsigned char *s,
    int sp,
    unsigned char *r,
    int rp,
    unsigned long *sum_s,
    unsigned long *sum_r,
    unsigned long *sum_sq_s,
    unsigned long *sum_sq_r,
    unsigned long *sum_sxr
)
{
    int i,j;
    for(i=0;i<16;i++,s+=sp,r+=rp)
     {
         for(j=0;j<16;j++)
         {
             *sum_s += s[j];
             *sum_r += r[j];
             *sum_sq_s += s[j] * s[j];
             *sum_sq_r += r[j] * r[j];
             *sum_sxr += s[j] * r[j];
         }
     }
}
void vp8_ssim_parms_8x8_c
(
    unsigned char *s,
    int sp,
    unsigned char *r,
    int rp,
    unsigned long *sum_s,
    unsigned long *sum_r,
    unsigned long *sum_sq_s,
    unsigned long *sum_sq_r,
    unsigned long *sum_sxr
)
{
    int i,j;
    for(i=0;i<8;i++,s+=sp,r+=rp)
     {
         for(j=0;j<8;j++)
         {
             *sum_s += s[j];
             *sum_r += r[j];
             *sum_sq_s += s[j] * s[j];
             *sum_sq_r += r[j] * r[j];
             *sum_sxr += s[j] * r[j];
         }
     }
}

const static int64_t cc1 =  26634; // (64^2*(.01*255)^2
const static int64_t cc2 = 239708; // (64^2*(.03*255)^2

static double similarity
(
    unsigned long sum_s,
    unsigned long sum_r,
    unsigned long sum_sq_s,
    unsigned long sum_sq_r,
    unsigned long sum_sxr,
    int count
)
{
    int64_t ssim_n, ssim_d;
    int64_t c1, c2;

    //scale the constants by number of pixels
    c1 = (cc1*count*count)>>12;
    c2 = (cc2*count*count)>>12;

    ssim_n = (2*sum_s*sum_r+ c1)*((int64_t) 2*count*sum_sxr-
          (int64_t) 2*sum_s*sum_r+c2);

    ssim_d = (sum_s*sum_s +sum_r*sum_r+c1)*
        ((int64_t)count*sum_sq_s-(int64_t)sum_s*sum_s +
        (int64_t)count*sum_sq_r-(int64_t) sum_r*sum_r +c2) ;

    return ssim_n * 1.0 / ssim_d;
}

static double ssim_16x16(unsigned char *s,int sp, unsigned char *r,int rp)
{
    unsigned long sum_s=0,sum_r=0,sum_sq_s=0,sum_sq_r=0,sum_sxr=0;
    vp8_ssim_parms_16x16(s, sp, r, rp, &sum_s, &sum_r, &sum_sq_s, &sum_sq_r, &sum_sxr);
    return similarity(sum_s, sum_r, sum_sq_s, sum_sq_r, sum_sxr, 256);
}
static double ssim_8x8(unsigned char *s,int sp, unsigned char *r,int rp)
{
    unsigned long sum_s=0,sum_r=0,sum_sq_s=0,sum_sq_r=0,sum_sxr=0;
    vp8_ssim_parms_8x8(s, sp, r, rp, &sum_s, &sum_r, &sum_sq_s, &sum_sq_r, &sum_sxr);
    return similarity(sum_s, sum_r, sum_sq_s, sum_sq_r, sum_sxr, 64);
}

// TODO: (jbb) tried to scale this function such that we may be able to use it
// for distortion metric in mode selection code ( provided we do a reconstruction)
long dssim(unsigned char *s,int sp, unsigned char *r,int rp)
{
    unsigned long sum_s=0,sum_r=0,sum_sq_s=0,sum_sq_r=0,sum_sxr=0;
    int64_t ssim3;
    int64_t ssim_n1,ssim_n2;
    int64_t ssim_d1,ssim_d2;
    int64_t ssim_t1,ssim_t2;
    int64_t c1, c2;

    // normalize by 256/64
    c1 = cc1*16;
    c2 = cc2*16;

    vp8_ssim_parms_16x16(s, sp, r, rp, &sum_s, &sum_r, &sum_sq_s, &sum_sq_r, &sum_sxr);
    ssim_n1 = (2*sum_s*sum_r+ c1);

    ssim_n2 =((int64_t) 2*256*sum_sxr-(int64_t) 2*sum_s*sum_r+c2);

    ssim_d1 =((int64_t)sum_s*sum_s +(int64_t)sum_r*sum_r+c1);

    ssim_d2 = (256 * (int64_t) sum_sq_s-(int64_t) sum_s*sum_s +
                    (int64_t) 256*sum_sq_r-(int64_t) sum_r*sum_r +c2) ;

    ssim_t1 = 256 - 256 * ssim_n1 / ssim_d1;
    ssim_t2 = 256 - 256 * ssim_n2 / ssim_d2;

    ssim3 = 256 *ssim_t1 * ssim_t2;
    if(ssim3 <0 )
        ssim3=0;
    return (long)( ssim3  );
}

// We are using a 8x8 moving window with starting location of each 8x8 window
// on the 4x4 pixel grid. Such arrangement allows the windows to overlap
// block boundaries to penalize blocking artifacts.
double vp8_ssim2
(
    unsigned char *img1,
    unsigned char *img2,
    int stride_img1,
    int stride_img2,
    int width,
    int height
)
{
    int i,j;
    int samples =0;
    double ssim_total=0;

    // sample point start with each 4x4 location
    for(i=0; i < height-8; i+=4, img1 += stride_img1*4, img2 += stride_img2*4)
    {
        for(j=0; j < width-8; j+=4 )
        {
            double v = ssim_8x8(img1+j, stride_img1, img2+j, stride_img2);
            ssim_total += v;
            samples++;
        }
    }
    ssim_total /= samples;
    return ssim_total;
}
double vp8_calc_ssim
(
    YV12_BUFFER_CONFIG *source,
    YV12_BUFFER_CONFIG *dest,
    int lumamask,
    double *weight
)
{
    double a, b, c;
    double ssimv;

    a = vp8_ssim2(source->y_buffer, dest->y_buffer,
                 source->y_stride, dest->y_stride, source->y_width,
                 source->y_height);

    b = vp8_ssim2(source->u_buffer, dest->u_buffer,
                 source->uv_stride, dest->uv_stride, source->uv_width,
                 source->uv_height);

    c = vp8_ssim2(source->v_buffer, dest->v_buffer,
                 source->uv_stride, dest->uv_stride, source->uv_width,
                 source->uv_height);

    ssimv = a * .8 + .1 * (b + c);

    *weight = 1;

    return ssimv;
}

double vp8_calc_ssimg
(
    YV12_BUFFER_CONFIG *source,
    YV12_BUFFER_CONFIG *dest,
    double *ssim_y,
    double *ssim_u,
    double *ssim_v
)
{
    double ssim_all = 0;
    double a, b, c;

    a = vp8_ssim2(source->y_buffer, dest->y_buffer,
                 source->y_stride, dest->y_stride, source->y_width,
                 source->y_height);

    b = vp8_ssim2(source->u_buffer, dest->u_buffer,
                 source->uv_stride, dest->uv_stride, source->uv_width,
                 source->uv_height);

    c = vp8_ssim2(source->v_buffer, dest->v_buffer,
                 source->uv_stride, dest->uv_stride, source->uv_width,
                 source->uv_height);
    *ssim_y = a;
    *ssim_u = b;
    *ssim_v = c;
    ssim_all = (a * 4 + b + c) /6;

    return ssim_all;
}
