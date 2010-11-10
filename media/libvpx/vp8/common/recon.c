/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#include "recon.h"
#include "blockd.h"

void vp8_recon_b_c
(
    unsigned char *pred_ptr,
    short *diff_ptr,
    unsigned char *dst_ptr,
    int stride
)
{
    int r, c;

    for (r = 0; r < 4; r++)
    {
        for (c = 0; c < 4; c++)
        {
            int a = diff_ptr[c] + pred_ptr[c] ;

            if (a < 0)
                a = 0;

            if (a > 255)
                a = 255;

            dst_ptr[c] = (unsigned char) a ;
        }

        dst_ptr += stride;
        diff_ptr += 16;
        pred_ptr += 16;
    }
}

void vp8_recon4b_c
(
    unsigned char *pred_ptr,
    short *diff_ptr,
    unsigned char *dst_ptr,
    int stride
)
{
    int r, c;

    for (r = 0; r < 4; r++)
    {
        for (c = 0; c < 16; c++)
        {
            int a = diff_ptr[c] + pred_ptr[c] ;

            if (a < 0)
                a = 0;

            if (a > 255)
                a = 255;

            dst_ptr[c] = (unsigned char) a ;
        }

        dst_ptr += stride;
        diff_ptr += 16;
        pred_ptr += 16;
    }
}

void vp8_recon2b_c
(
    unsigned char *pred_ptr,
    short *diff_ptr,
    unsigned char *dst_ptr,
    int stride
)
{
    int r, c;

    for (r = 0; r < 4; r++)
    {
        for (c = 0; c < 8; c++)
        {
            int a = diff_ptr[c] + pred_ptr[c] ;

            if (a < 0)
                a = 0;

            if (a > 255)
                a = 255;

            dst_ptr[c] = (unsigned char) a ;
        }

        dst_ptr += stride;
        diff_ptr += 8;
        pred_ptr += 8;
    }
}

void vp8_recon_mby_c(const vp8_recon_rtcd_vtable_t *rtcd, MACROBLOCKD *x)
{
#if ARCH_ARM
    BLOCKD *b = &x->block[0];
    RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);

    /*b = &x->block[4];*/
    b += 4;
    RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);

    /*b = &x->block[8];*/
    b += 4;
    RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);

    /*b = &x->block[12];*/
    b += 4;
    RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
#else
    int i;

    for (i = 0; i < 16; i += 4)
    {
        BLOCKD *b = &x->block[i];

        RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    }
#endif
}

void vp8_recon_mb_c(const vp8_recon_rtcd_vtable_t *rtcd, MACROBLOCKD *x)
{
#if ARCH_ARM
    BLOCKD *b = &x->block[0];

    RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    b += 4;
    RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    b += 4;
    RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    b += 4;
    RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    b += 4;

    /*b = &x->block[16];*/

    RECON_INVOKE(rtcd, recon2)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    b++;
    b++;
    RECON_INVOKE(rtcd, recon2)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    b++;
    b++;
    RECON_INVOKE(rtcd, recon2)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    b++;
    b++;
    RECON_INVOKE(rtcd, recon2)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
#else
    int i;

    for (i = 0; i < 16; i += 4)
    {
        BLOCKD *b = &x->block[i];

        RECON_INVOKE(rtcd, recon4)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    }

    for (i = 16; i < 24; i += 2)
    {
        BLOCKD *b = &x->block[i];

        RECON_INVOKE(rtcd, recon2)(b->predictor, b->diff, *(b->base_dst) + b->dst, b->dst_stride);
    }
#endif
}
