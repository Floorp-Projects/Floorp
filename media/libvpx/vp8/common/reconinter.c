/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#include "recon.h"
#include "subpixel.h"
#include "blockd.h"
#include "reconinter.h"
#if CONFIG_RUNTIME_CPU_DETECT
#include "onyxc_int.h"
#endif

// use this define on systems where unaligned int reads and writes are
// not allowed, i.e. ARM architectures
//#define MUST_BE_ALIGNED


static const int bbb[4] = {0, 2, 8, 10};



void vp8_copy_mem16x16_c(
    unsigned char *src,
    int src_stride,
    unsigned char *dst,
    int dst_stride)
{

    int r;

    for (r = 0; r < 16; r++)
    {
#ifdef MUST_BE_ALIGNED
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
        dst[8] = src[8];
        dst[9] = src[9];
        dst[10] = src[10];
        dst[11] = src[11];
        dst[12] = src[12];
        dst[13] = src[13];
        dst[14] = src[14];
        dst[15] = src[15];

#else
        ((int *)dst)[0] = ((int *)src)[0] ;
        ((int *)dst)[1] = ((int *)src)[1] ;
        ((int *)dst)[2] = ((int *)src)[2] ;
        ((int *)dst)[3] = ((int *)src)[3] ;

#endif
        src += src_stride;
        dst += dst_stride;

    }

}

void vp8_copy_mem8x8_c(
    unsigned char *src,
    int src_stride,
    unsigned char *dst,
    int dst_stride)
{
    int r;

    for (r = 0; r < 8; r++)
    {
#ifdef MUST_BE_ALIGNED
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
#else
        ((int *)dst)[0] = ((int *)src)[0] ;
        ((int *)dst)[1] = ((int *)src)[1] ;
#endif
        src += src_stride;
        dst += dst_stride;

    }

}

void vp8_copy_mem8x4_c(
    unsigned char *src,
    int src_stride,
    unsigned char *dst,
    int dst_stride)
{
    int r;

    for (r = 0; r < 4; r++)
    {
#ifdef MUST_BE_ALIGNED
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
#else
        ((int *)dst)[0] = ((int *)src)[0] ;
        ((int *)dst)[1] = ((int *)src)[1] ;
#endif
        src += src_stride;
        dst += dst_stride;

    }

}



void vp8_build_inter_predictors_b(BLOCKD *d, int pitch, vp8_subpix_fn_t sppf)
{
    int r;
    unsigned char *ptr_base;
    unsigned char *ptr;
    unsigned char *pred_ptr = d->predictor;

    ptr_base = *(d->base_pre);

    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
    {
        ptr = ptr_base + d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);
        sppf(ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, pred_ptr, pitch);
    }
    else
    {
        ptr_base += d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);
        ptr = ptr_base;

        for (r = 0; r < 4; r++)
        {
#ifdef MUST_BE_ALIGNED
            pred_ptr[0]  = ptr[0];
            pred_ptr[1]  = ptr[1];
            pred_ptr[2]  = ptr[2];
            pred_ptr[3]  = ptr[3];
#else
            *(int *)pred_ptr = *(int *)ptr ;
#endif
            pred_ptr     += pitch;
            ptr         += d->pre_stride;
        }
    }
}

void vp8_build_inter_predictors4b(MACROBLOCKD *x, BLOCKD *d, int pitch)
{
    unsigned char *ptr_base;
    unsigned char *ptr;
    unsigned char *pred_ptr = d->predictor;

    ptr_base = *(d->base_pre);
    ptr = ptr_base + d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
    {
        x->subpixel_predict8x8(ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, pred_ptr, pitch);
    }
    else
    {
        RECON_INVOKE(&x->rtcd->recon, copy8x8)(ptr, d->pre_stride, pred_ptr, pitch);
    }
}

void vp8_build_inter_predictors2b(MACROBLOCKD *x, BLOCKD *d, int pitch)
{
    unsigned char *ptr_base;
    unsigned char *ptr;
    unsigned char *pred_ptr = d->predictor;

    ptr_base = *(d->base_pre);
    ptr = ptr_base + d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
    {
        x->subpixel_predict8x4(ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, pred_ptr, pitch);
    }
    else
    {
        RECON_INVOKE(&x->rtcd->recon, copy8x4)(ptr, d->pre_stride, pred_ptr, pitch);
    }
}


void vp8_build_inter_predictors_mbuv(MACROBLOCKD *x)
{
    int i;

    if (x->mode_info_context->mbmi.ref_frame != INTRA_FRAME &&
        x->mode_info_context->mbmi.mode != SPLITMV)
    {
        unsigned char *uptr, *vptr;
        unsigned char *upred_ptr = &x->predictor[256];
        unsigned char *vpred_ptr = &x->predictor[320];

        int mv_row = x->block[16].bmi.mv.as_mv.row;
        int mv_col = x->block[16].bmi.mv.as_mv.col;
        int offset;
        int pre_stride = x->block[16].pre_stride;

        offset = (mv_row >> 3) * pre_stride + (mv_col >> 3);
        uptr = x->pre.u_buffer + offset;
        vptr = x->pre.v_buffer + offset;

        if ((mv_row | mv_col) & 7)
        {
            x->subpixel_predict8x8(uptr, pre_stride, mv_col & 7, mv_row & 7, upred_ptr, 8);
            x->subpixel_predict8x8(vptr, pre_stride, mv_col & 7, mv_row & 7, vpred_ptr, 8);
        }
        else
        {
            RECON_INVOKE(&x->rtcd->recon, copy8x8)(uptr, pre_stride, upred_ptr, 8);
            RECON_INVOKE(&x->rtcd->recon, copy8x8)(vptr, pre_stride, vpred_ptr, 8);
        }
    }
    else
    {
        for (i = 16; i < 24; i += 2)
        {
            BLOCKD *d0 = &x->block[i];
            BLOCKD *d1 = &x->block[i+1];

            if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                vp8_build_inter_predictors2b(x, d0, 8);
            else
            {
                vp8_build_inter_predictors_b(d0, 8, x->subpixel_predict);
                vp8_build_inter_predictors_b(d1, 8, x->subpixel_predict);
            }
        }
    }
}

//encoder only
void vp8_build_inter_predictors_mby(MACROBLOCKD *x)
{

  if (x->mode_info_context->mbmi.ref_frame != INTRA_FRAME &&
      x->mode_info_context->mbmi.mode != SPLITMV)
    {
        unsigned char *ptr_base;
        unsigned char *ptr;
        unsigned char *pred_ptr = x->predictor;
        int mv_row = x->mode_info_context->mbmi.mv.as_mv.row;
        int mv_col = x->mode_info_context->mbmi.mv.as_mv.col;
        int pre_stride = x->block[0].pre_stride;

        ptr_base = x->pre.y_buffer;
        ptr = ptr_base + (mv_row >> 3) * pre_stride + (mv_col >> 3);

        if ((mv_row | mv_col) & 7)
        {
            x->subpixel_predict16x16(ptr, pre_stride, mv_col & 7, mv_row & 7, pred_ptr, 16);
        }
        else
        {
            RECON_INVOKE(&x->rtcd->recon, copy16x16)(ptr, pre_stride, pred_ptr, 16);
        }
    }
    else
    {
        int i;

        if (x->mode_info_context->mbmi.partitioning < 3)
        {
            for (i = 0; i < 4; i++)
            {
                BLOCKD *d = &x->block[bbb[i]];
                vp8_build_inter_predictors4b(x, d, 16);
            }

        }
        else
        {
            for (i = 0; i < 16; i += 2)
            {
                BLOCKD *d0 = &x->block[i];
                BLOCKD *d1 = &x->block[i+1];

                if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                    vp8_build_inter_predictors2b(x, d0, 16);
                else
                {
                    vp8_build_inter_predictors_b(d0, 16, x->subpixel_predict);
                    vp8_build_inter_predictors_b(d1, 16, x->subpixel_predict);
                }

            }
        }
    }
}

void vp8_build_inter_predictors_mb(MACROBLOCKD *x)
{

    if (x->mode_info_context->mbmi.ref_frame != INTRA_FRAME &&
        x->mode_info_context->mbmi.mode != SPLITMV)
    {
        int offset;
        unsigned char *ptr_base;
        unsigned char *ptr;
        unsigned char *uptr, *vptr;
        unsigned char *pred_ptr = x->predictor;
        unsigned char *upred_ptr = &x->predictor[256];
        unsigned char *vpred_ptr = &x->predictor[320];

        int mv_row = x->mode_info_context->mbmi.mv.as_mv.row;
        int mv_col = x->mode_info_context->mbmi.mv.as_mv.col;
        int pre_stride = x->block[0].pre_stride;

        ptr_base = x->pre.y_buffer;
        ptr = ptr_base + (mv_row >> 3) * pre_stride + (mv_col >> 3);

        if ((mv_row | mv_col) & 7)
        {
            x->subpixel_predict16x16(ptr, pre_stride, mv_col & 7, mv_row & 7, pred_ptr, 16);
        }
        else
        {
            RECON_INVOKE(&x->rtcd->recon, copy16x16)(ptr, pre_stride, pred_ptr, 16);
        }

        mv_row = x->block[16].bmi.mv.as_mv.row;
        mv_col = x->block[16].bmi.mv.as_mv.col;
        pre_stride >>= 1;
        offset = (mv_row >> 3) * pre_stride + (mv_col >> 3);
        uptr = x->pre.u_buffer + offset;
        vptr = x->pre.v_buffer + offset;

        if ((mv_row | mv_col) & 7)
        {
            x->subpixel_predict8x8(uptr, pre_stride, mv_col & 7, mv_row & 7, upred_ptr, 8);
            x->subpixel_predict8x8(vptr, pre_stride, mv_col & 7, mv_row & 7, vpred_ptr, 8);
        }
        else
        {
            RECON_INVOKE(&x->rtcd->recon, copy8x8)(uptr, pre_stride, upred_ptr, 8);
            RECON_INVOKE(&x->rtcd->recon, copy8x8)(vptr, pre_stride, vpred_ptr, 8);
        }
    }
    else
    {
        int i;

        if (x->mode_info_context->mbmi.partitioning < 3)
        {
            for (i = 0; i < 4; i++)
            {
                BLOCKD *d = &x->block[bbb[i]];
                vp8_build_inter_predictors4b(x, d, 16);
            }
        }
        else
        {
            for (i = 0; i < 16; i += 2)
            {
                BLOCKD *d0 = &x->block[i];
                BLOCKD *d1 = &x->block[i+1];

                if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                    vp8_build_inter_predictors2b(x, d0, 16);
                else
                {
                    vp8_build_inter_predictors_b(d0, 16, x->subpixel_predict);
                    vp8_build_inter_predictors_b(d1, 16, x->subpixel_predict);
                }

            }

        }

        for (i = 16; i < 24; i += 2)
        {
            BLOCKD *d0 = &x->block[i];
            BLOCKD *d1 = &x->block[i+1];

            if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                vp8_build_inter_predictors2b(x, d0, 8);
            else
            {
                vp8_build_inter_predictors_b(d0, 8, x->subpixel_predict);
                vp8_build_inter_predictors_b(d1, 8, x->subpixel_predict);
            }

        }

    }
}

void vp8_build_uvmvs(MACROBLOCKD *x, int fullpixel)
{
    int i, j;

    if (x->mode_info_context->mbmi.mode == SPLITMV)
    {
        for (i = 0; i < 2; i++)
        {
            for (j = 0; j < 2; j++)
            {
                int yoffset = i * 8 + j * 2;
                int uoffset = 16 + i * 2 + j;
                int voffset = 20 + i * 2 + j;

                int temp;

                temp = x->block[yoffset  ].bmi.mv.as_mv.row
                       + x->block[yoffset+1].bmi.mv.as_mv.row
                       + x->block[yoffset+4].bmi.mv.as_mv.row
                       + x->block[yoffset+5].bmi.mv.as_mv.row;

                if (temp < 0) temp -= 4;
                else temp += 4;

                x->block[uoffset].bmi.mv.as_mv.row = temp / 8;

                if (fullpixel)
                    x->block[uoffset].bmi.mv.as_mv.row = (temp / 8) & 0xfffffff8;

                temp = x->block[yoffset  ].bmi.mv.as_mv.col
                       + x->block[yoffset+1].bmi.mv.as_mv.col
                       + x->block[yoffset+4].bmi.mv.as_mv.col
                       + x->block[yoffset+5].bmi.mv.as_mv.col;

                if (temp < 0) temp -= 4;
                else temp += 4;

                x->block[uoffset].bmi.mv.as_mv.col = temp / 8;

                if (fullpixel)
                    x->block[uoffset].bmi.mv.as_mv.col = (temp / 8) & 0xfffffff8;

                x->block[voffset].bmi.mv.as_mv.row = x->block[uoffset].bmi.mv.as_mv.row ;
                x->block[voffset].bmi.mv.as_mv.col = x->block[uoffset].bmi.mv.as_mv.col ;
            }
        }
    }
    else
    {
        int mvrow = x->mode_info_context->mbmi.mv.as_mv.row;
        int mvcol = x->mode_info_context->mbmi.mv.as_mv.col;

        if (mvrow < 0)
            mvrow -= 1;
        else
            mvrow += 1;

        if (mvcol < 0)
            mvcol -= 1;
        else
            mvcol += 1;

        mvrow /= 2;
        mvcol /= 2;

        for (i = 0; i < 8; i++)
        {
            x->block[ 16 + i].bmi.mv.as_mv.row = mvrow;
            x->block[ 16 + i].bmi.mv.as_mv.col = mvcol;

            if (fullpixel)
            {
                x->block[ 16 + i].bmi.mv.as_mv.row = mvrow & 0xfffffff8;
                x->block[ 16 + i].bmi.mv.as_mv.col = mvcol & 0xfffffff8;
            }
        }
    }
}


// The following functions are wriiten for skip_recon_mb() to call. Since there is no recon in this
// situation, we can write the result directly to dst buffer instead of writing it to predictor
// buffer and then copying it to dst buffer.
static void vp8_build_inter_predictors_b_s(BLOCKD *d, unsigned char *dst_ptr, vp8_subpix_fn_t sppf)
{
    int r;
    unsigned char *ptr_base;
    unsigned char *ptr;
    //unsigned char *pred_ptr = d->predictor;
    int dst_stride = d->dst_stride;
    int pre_stride = d->pre_stride;

    ptr_base = *(d->base_pre);

    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
    {
        ptr = ptr_base + d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);
        sppf(ptr, pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, dst_ptr, dst_stride);
    }
    else
    {
        ptr_base += d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);
        ptr = ptr_base;

        for (r = 0; r < 4; r++)
        {
#ifdef MUST_BE_ALIGNED
            dst_ptr[0]   = ptr[0];
            dst_ptr[1]   = ptr[1];
            dst_ptr[2]   = ptr[2];
            dst_ptr[3]   = ptr[3];
#else
            *(int *)dst_ptr = *(int *)ptr ;
#endif
            dst_ptr      += dst_stride;
            ptr         += pre_stride;
        }
    }
}



void vp8_build_inter_predictors_mb_s(MACROBLOCKD *x)
{
    //unsigned char *pred_ptr = x->block[0].predictor;
    //unsigned char *dst_ptr = *(x->block[0].base_dst) + x->block[0].dst;
    unsigned char *pred_ptr = x->predictor;
    unsigned char *dst_ptr = x->dst.y_buffer;

    if (x->mode_info_context->mbmi.mode != SPLITMV)
    {
        int offset;
        unsigned char *ptr_base;
        unsigned char *ptr;
        unsigned char *uptr, *vptr;
        //unsigned char *pred_ptr = x->predictor;
        //unsigned char *upred_ptr = &x->predictor[256];
        //unsigned char *vpred_ptr = &x->predictor[320];
        unsigned char *udst_ptr = x->dst.u_buffer;
        unsigned char *vdst_ptr = x->dst.v_buffer;

        int mv_row = x->mode_info_context->mbmi.mv.as_mv.row;
        int mv_col = x->mode_info_context->mbmi.mv.as_mv.col;
        int pre_stride = x->dst.y_stride; //x->block[0].pre_stride;

        ptr_base = x->pre.y_buffer;
        ptr = ptr_base + (mv_row >> 3) * pre_stride + (mv_col >> 3);

        if ((mv_row | mv_col) & 7)
        {
            x->subpixel_predict16x16(ptr, pre_stride, mv_col & 7, mv_row & 7, dst_ptr, x->dst.y_stride); //x->block[0].dst_stride);
        }
        else
        {
            RECON_INVOKE(&x->rtcd->recon, copy16x16)(ptr, pre_stride, dst_ptr, x->dst.y_stride); //x->block[0].dst_stride);
        }

        mv_row = x->block[16].bmi.mv.as_mv.row;
        mv_col = x->block[16].bmi.mv.as_mv.col;
        pre_stride >>= 1;
        offset = (mv_row >> 3) * pre_stride + (mv_col >> 3);
        uptr = x->pre.u_buffer + offset;
        vptr = x->pre.v_buffer + offset;

        if ((mv_row | mv_col) & 7)
        {
            x->subpixel_predict8x8(uptr, pre_stride, mv_col & 7, mv_row & 7, udst_ptr, x->dst.uv_stride);
            x->subpixel_predict8x8(vptr, pre_stride, mv_col & 7, mv_row & 7, vdst_ptr, x->dst.uv_stride);
        }
        else
        {
            RECON_INVOKE(&x->rtcd->recon, copy8x8)(uptr, pre_stride, udst_ptr, x->dst.uv_stride);
            RECON_INVOKE(&x->rtcd->recon, copy8x8)(vptr, pre_stride, vdst_ptr, x->dst.uv_stride);
        }
    }
    else
    {
        //note: this whole ELSE part is not executed at all. So, no way to test the correctness of my modification. Later,
        //if sth is wrong, go back to what it is in build_inter_predictors_mb.
        int i;

        if (x->mode_info_context->mbmi.partitioning < 3)
        {
            for (i = 0; i < 4; i++)
            {
                BLOCKD *d = &x->block[bbb[i]];
                //vp8_build_inter_predictors4b(x, d, 16);

                {
                    unsigned char *ptr_base;
                    unsigned char *ptr;
                    unsigned char *pred_ptr = d->predictor;

                    ptr_base = *(d->base_pre);
                    ptr = ptr_base + d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

                    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
                    {
                        x->subpixel_predict8x8(ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, dst_ptr, x->dst.y_stride); //x->block[0].dst_stride);
                    }
                    else
                    {
                        RECON_INVOKE(&x->rtcd->recon, copy8x8)(ptr, d->pre_stride, dst_ptr, x->dst.y_stride); //x->block[0].dst_stride);
                    }
                }
            }
        }
        else
        {
            for (i = 0; i < 16; i += 2)
            {
                BLOCKD *d0 = &x->block[i];
                BLOCKD *d1 = &x->block[i+1];

                if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                {
                    //vp8_build_inter_predictors2b(x, d0, 16);
                    unsigned char *ptr_base;
                    unsigned char *ptr;
                    unsigned char *pred_ptr = d0->predictor;

                    ptr_base = *(d0->base_pre);
                    ptr = ptr_base + d0->pre + (d0->bmi.mv.as_mv.row >> 3) * d0->pre_stride + (d0->bmi.mv.as_mv.col >> 3);

                    if (d0->bmi.mv.as_mv.row & 7 || d0->bmi.mv.as_mv.col & 7)
                    {
                        x->subpixel_predict8x4(ptr, d0->pre_stride, d0->bmi.mv.as_mv.col & 7, d0->bmi.mv.as_mv.row & 7, dst_ptr, x->dst.y_stride);
                    }
                    else
                    {
                        RECON_INVOKE(&x->rtcd->recon, copy8x4)(ptr, d0->pre_stride, dst_ptr, x->dst.y_stride);
                    }
                }
                else
                {
                    vp8_build_inter_predictors_b_s(d0, dst_ptr, x->subpixel_predict);
                    vp8_build_inter_predictors_b_s(d1, dst_ptr, x->subpixel_predict);
                }
            }
        }

        for (i = 16; i < 24; i += 2)
        {
            BLOCKD *d0 = &x->block[i];
            BLOCKD *d1 = &x->block[i+1];

            if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
            {
                //vp8_build_inter_predictors2b(x, d0, 8);
                unsigned char *ptr_base;
                unsigned char *ptr;
                unsigned char *pred_ptr = d0->predictor;

                ptr_base = *(d0->base_pre);
                ptr = ptr_base + d0->pre + (d0->bmi.mv.as_mv.row >> 3) * d0->pre_stride + (d0->bmi.mv.as_mv.col >> 3);

                if (d0->bmi.mv.as_mv.row & 7 || d0->bmi.mv.as_mv.col & 7)
                {
                    x->subpixel_predict8x4(ptr, d0->pre_stride, d0->bmi.mv.as_mv.col & 7, d0->bmi.mv.as_mv.row & 7, dst_ptr, x->dst.y_stride);
                }
                else
                {
                    RECON_INVOKE(&x->rtcd->recon, copy8x4)(ptr, d0->pre_stride, dst_ptr, x->dst.y_stride);
                }
            }
            else
            {
                vp8_build_inter_predictors_b_s(d0, dst_ptr, x->subpixel_predict);
                vp8_build_inter_predictors_b_s(d1, dst_ptr, x->subpixel_predict);
            }
        }
    }
}
