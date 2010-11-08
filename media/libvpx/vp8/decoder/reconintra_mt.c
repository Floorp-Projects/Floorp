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
#include "reconintra.h"
#include "vpx_mem/vpx_mem.h"
#include "onyxd_int.h"

/* For skip_recon_mb(), add vp8_build_intra_predictors_mby_s(MACROBLOCKD *x) and
 * vp8_build_intra_predictors_mbuv_s(MACROBLOCKD *x).
 */

void vp8mt_build_intra_predictors_mby(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col)
{
#if CONFIG_MULTITHREAD
    unsigned char *yabove_row;    /* = x->dst.y_buffer - x->dst.y_stride; */
    unsigned char *yleft_col;
    unsigned char yleft_buf[16];
    unsigned char ytop_left;      /* = yabove_row[-1]; */
    unsigned char *ypred_ptr = x->predictor;
    int r, c, i;

    if (pbi->common.filter_level)
    {
        yabove_row = pbi->mt_yabove_row[mb_row] + mb_col*16 +32;
        yleft_col = pbi->mt_yleft_col[mb_row];
    } else
    {
        yabove_row = x->dst.y_buffer - x->dst.y_stride;

        for (i = 0; i < 16; i++)
            yleft_buf[i] = x->dst.y_buffer [i* x->dst.y_stride -1];
        yleft_col = yleft_buf;
    }

    ytop_left = yabove_row[-1];

    /* for Y */
    switch (x->mode_info_context->mbmi.mode)
    {
    case DC_PRED:
    {
        int expected_dc;
        int i;
        int shift;
        int average = 0;


        if (x->up_available || x->left_available)
        {
            if (x->up_available)
            {
                for (i = 0; i < 16; i++)
                {
                    average += yabove_row[i];
                }
            }

            if (x->left_available)
            {

                for (i = 0; i < 16; i++)
                {
                    average += yleft_col[i];
                }

            }



            shift = 3 + x->up_available + x->left_available;
            expected_dc = (average + (1 << (shift - 1))) >> shift;
        }
        else
        {
            expected_dc = 128;
        }

        vpx_memset(ypred_ptr, expected_dc, 256);
    }
    break;
    case V_PRED:
    {

        for (r = 0; r < 16; r++)
        {

            ((int *)ypred_ptr)[0] = ((int *)yabove_row)[0];
            ((int *)ypred_ptr)[1] = ((int *)yabove_row)[1];
            ((int *)ypred_ptr)[2] = ((int *)yabove_row)[2];
            ((int *)ypred_ptr)[3] = ((int *)yabove_row)[3];
            ypred_ptr += 16;
        }
    }
    break;
    case H_PRED:
    {

        for (r = 0; r < 16; r++)
        {

            vpx_memset(ypred_ptr, yleft_col[r], 16);
            ypred_ptr += 16;
        }

    }
    break;
    case TM_PRED:
    {

        for (r = 0; r < 16; r++)
        {
            for (c = 0; c < 16; c++)
            {
                int pred =  yleft_col[r] + yabove_row[ c] - ytop_left;

                if (pred < 0)
                    pred = 0;

                if (pred > 255)
                    pred = 255;

                ypred_ptr[c] = pred;
            }

            ypred_ptr += 16;
        }

    }
    break;
    case B_PRED:
    case NEARESTMV:
    case NEARMV:
    case ZEROMV:
    case NEWMV:
    case SPLITMV:
    case MB_MODE_COUNT:
        break;
    }
#else
    (void) pbi;
    (void) x;
    (void) mb_row;
    (void) mb_col;
#endif
}

void vp8mt_build_intra_predictors_mby_s(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col)
{
#if CONFIG_MULTITHREAD
    unsigned char *yabove_row;    /* = x->dst.y_buffer - x->dst.y_stride; */
    unsigned char *yleft_col;
    unsigned char yleft_buf[16];
    unsigned char ytop_left;      /* = yabove_row[-1]; */
    unsigned char *ypred_ptr = x->predictor;
    int r, c, i;

    int y_stride = x->dst.y_stride;
    ypred_ptr = x->dst.y_buffer; /*x->predictor;*/

    if (pbi->common.filter_level)
    {
        yabove_row = pbi->mt_yabove_row[mb_row] + mb_col*16 +32;
        yleft_col = pbi->mt_yleft_col[mb_row];
    } else
    {
        yabove_row = x->dst.y_buffer - x->dst.y_stride;

        for (i = 0; i < 16; i++)
            yleft_buf[i] = x->dst.y_buffer [i* x->dst.y_stride -1];
        yleft_col = yleft_buf;
    }

    ytop_left = yabove_row[-1];

    /* for Y */
    switch (x->mode_info_context->mbmi.mode)
    {
    case DC_PRED:
    {
        int expected_dc;
        int i;
        int shift;
        int average = 0;


        if (x->up_available || x->left_available)
        {
            if (x->up_available)
            {
                for (i = 0; i < 16; i++)
                {
                    average += yabove_row[i];
                }
            }

            if (x->left_available)
            {

                for (i = 0; i < 16; i++)
                {
                    average += yleft_col[i];
                }

            }



            shift = 3 + x->up_available + x->left_available;
            expected_dc = (average + (1 << (shift - 1))) >> shift;
        }
        else
        {
            expected_dc = 128;
        }

        /*vpx_memset(ypred_ptr, expected_dc, 256);*/
        for (r = 0; r < 16; r++)
        {
            vpx_memset(ypred_ptr, expected_dc, 16);
            ypred_ptr += y_stride; /*16;*/
        }
    }
    break;
    case V_PRED:
    {

        for (r = 0; r < 16; r++)
        {

            ((int *)ypred_ptr)[0] = ((int *)yabove_row)[0];
            ((int *)ypred_ptr)[1] = ((int *)yabove_row)[1];
            ((int *)ypred_ptr)[2] = ((int *)yabove_row)[2];
            ((int *)ypred_ptr)[3] = ((int *)yabove_row)[3];
            ypred_ptr += y_stride; /*16;*/
        }
    }
    break;
    case H_PRED:
    {

        for (r = 0; r < 16; r++)
        {

            vpx_memset(ypred_ptr, yleft_col[r], 16);
            ypred_ptr += y_stride;  /*16;*/
        }

    }
    break;
    case TM_PRED:
    {

        for (r = 0; r < 16; r++)
        {
            for (c = 0; c < 16; c++)
            {
                int pred =  yleft_col[r] + yabove_row[ c] - ytop_left;

                if (pred < 0)
                    pred = 0;

                if (pred > 255)
                    pred = 255;

                ypred_ptr[c] = pred;
            }

            ypred_ptr += y_stride;  /*16;*/
        }

    }
    break;
    case B_PRED:
    case NEARESTMV:
    case NEARMV:
    case ZEROMV:
    case NEWMV:
    case SPLITMV:
    case MB_MODE_COUNT:
        break;
    }
#else
    (void) pbi;
    (void) x;
    (void) mb_row;
    (void) mb_col;
#endif
}

void vp8mt_build_intra_predictors_mbuv(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col)
{
#if CONFIG_MULTITHREAD
    unsigned char *uabove_row;   /* = x->dst.u_buffer - x->dst.uv_stride; */
    unsigned char *uleft_col;    /*[16];*/
    unsigned char uleft_buf[8];
    unsigned char utop_left;     /* = uabove_row[-1]; */
    unsigned char *vabove_row;   /* = x->dst.v_buffer - x->dst.uv_stride; */
    unsigned char *vleft_col;    /*[20];*/
    unsigned char vleft_buf[8];
    unsigned char vtop_left;     /* = vabove_row[-1]; */
    unsigned char *upred_ptr = &x->predictor[256];
    unsigned char *vpred_ptr = &x->predictor[320];
    int i, j;

    if (pbi->common.filter_level)
    {
        uabove_row = pbi->mt_uabove_row[mb_row] + mb_col*8 +16;
        vabove_row = pbi->mt_vabove_row[mb_row] + mb_col*8 +16;
        uleft_col = pbi->mt_uleft_col[mb_row];
        vleft_col = pbi->mt_vleft_col[mb_row];
    } else
    {
        uabove_row = x->dst.u_buffer - x->dst.uv_stride;
        vabove_row = x->dst.v_buffer - x->dst.uv_stride;

        for (i = 0; i < 8; i++)
        {
            uleft_buf[i] = x->dst.u_buffer [i* x->dst.uv_stride -1];
            vleft_buf[i] = x->dst.v_buffer [i* x->dst.uv_stride -1];
        }
        uleft_col = uleft_buf;
        vleft_col = vleft_buf;
    }
    utop_left = uabove_row[-1];
    vtop_left = vabove_row[-1];

    switch (x->mode_info_context->mbmi.uv_mode)
    {
    case DC_PRED:
    {
        int expected_udc;
        int expected_vdc;
        int i;
        int shift;
        int Uaverage = 0;
        int Vaverage = 0;

        if (x->up_available)
        {
            for (i = 0; i < 8; i++)
            {
                Uaverage += uabove_row[i];
                Vaverage += vabove_row[i];
            }
        }

        if (x->left_available)
        {
            for (i = 0; i < 8; i++)
            {
                Uaverage += uleft_col[i];
                Vaverage += vleft_col[i];
            }
        }

        if (!x->up_available && !x->left_available)
        {
            expected_udc = 128;
            expected_vdc = 128;
        }
        else
        {
            shift = 2 + x->up_available + x->left_available;
            expected_udc = (Uaverage + (1 << (shift - 1))) >> shift;
            expected_vdc = (Vaverage + (1 << (shift - 1))) >> shift;
        }


        vpx_memset(upred_ptr, expected_udc, 64);
        vpx_memset(vpred_ptr, expected_vdc, 64);


    }
    break;
    case V_PRED:
    {
        int i;

        for (i = 0; i < 8; i++)
        {
            vpx_memcpy(upred_ptr, uabove_row, 8);
            vpx_memcpy(vpred_ptr, vabove_row, 8);
            upred_ptr += 8;
            vpred_ptr += 8;
        }

    }
    break;
    case H_PRED:
    {
        int i;

        for (i = 0; i < 8; i++)
        {
            vpx_memset(upred_ptr, uleft_col[i], 8);
            vpx_memset(vpred_ptr, vleft_col[i], 8);
            upred_ptr += 8;
            vpred_ptr += 8;
        }
    }

    break;
    case TM_PRED:
    {
        int i;

        for (i = 0; i < 8; i++)
        {
            for (j = 0; j < 8; j++)
            {
                int predu = uleft_col[i] + uabove_row[j] - utop_left;
                int predv = vleft_col[i] + vabove_row[j] - vtop_left;

                if (predu < 0)
                    predu = 0;

                if (predu > 255)
                    predu = 255;

                if (predv < 0)
                    predv = 0;

                if (predv > 255)
                    predv = 255;

                upred_ptr[j] = predu;
                vpred_ptr[j] = predv;
            }

            upred_ptr += 8;
            vpred_ptr += 8;
        }

    }
    break;
    case B_PRED:
    case NEARESTMV:
    case NEARMV:
    case ZEROMV:
    case NEWMV:
    case SPLITMV:
    case MB_MODE_COUNT:
        break;
    }
#else
    (void) pbi;
    (void) x;
    (void) mb_row;
    (void) mb_col;
#endif
}

void vp8mt_build_intra_predictors_mbuv_s(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col)
{
#if CONFIG_MULTITHREAD
    unsigned char *uabove_row;  /* = x->dst.u_buffer - x->dst.uv_stride; */
    unsigned char *uleft_col;   /*[16];*/
    unsigned char uleft_buf[8];
    unsigned char utop_left;    /* = uabove_row[-1]; */
    unsigned char *vabove_row;  /* = x->dst.v_buffer - x->dst.uv_stride; */
    unsigned char *vleft_col;   /*[20];*/
    unsigned char vleft_buf[8];
    unsigned char vtop_left;    /* = vabove_row[-1]; */
    unsigned char *upred_ptr = x->dst.u_buffer; /*&x->predictor[256];*/
    unsigned char *vpred_ptr = x->dst.v_buffer; /*&x->predictor[320];*/
    int uv_stride = x->dst.uv_stride;
    int i, j;

    if (pbi->common.filter_level)
    {
        uabove_row = pbi->mt_uabove_row[mb_row] + mb_col*8 +16;
        vabove_row = pbi->mt_vabove_row[mb_row] + mb_col*8 +16;
        uleft_col = pbi->mt_uleft_col[mb_row];
        vleft_col = pbi->mt_vleft_col[mb_row];
    } else
    {
        uabove_row = x->dst.u_buffer - x->dst.uv_stride;
        vabove_row = x->dst.v_buffer - x->dst.uv_stride;

        for (i = 0; i < 8; i++)
        {
            uleft_buf[i] = x->dst.u_buffer [i* x->dst.uv_stride -1];
            vleft_buf[i] = x->dst.v_buffer [i* x->dst.uv_stride -1];
        }
        uleft_col = uleft_buf;
        vleft_col = vleft_buf;
    }
    utop_left = uabove_row[-1];
    vtop_left = vabove_row[-1];

    switch (x->mode_info_context->mbmi.uv_mode)
    {
    case DC_PRED:
    {
        int expected_udc;
        int expected_vdc;
        int i;
        int shift;
        int Uaverage = 0;
        int Vaverage = 0;

        if (x->up_available)
        {
            for (i = 0; i < 8; i++)
            {
                Uaverage += uabove_row[i];
                Vaverage += vabove_row[i];
            }
        }

        if (x->left_available)
        {
            for (i = 0; i < 8; i++)
            {
                Uaverage += uleft_col[i];
                Vaverage += vleft_col[i];
            }
        }

        if (!x->up_available && !x->left_available)
        {
            expected_udc = 128;
            expected_vdc = 128;
        }
        else
        {
            shift = 2 + x->up_available + x->left_available;
            expected_udc = (Uaverage + (1 << (shift - 1))) >> shift;
            expected_vdc = (Vaverage + (1 << (shift - 1))) >> shift;
        }


        /*vpx_memset(upred_ptr,expected_udc,64);
        vpx_memset(vpred_ptr,expected_vdc,64);*/
        for (i = 0; i < 8; i++)
        {
            vpx_memset(upred_ptr, expected_udc, 8);
            vpx_memset(vpred_ptr, expected_vdc, 8);
            upred_ptr += uv_stride; /*8;*/
            vpred_ptr += uv_stride; /*8;*/
        }
    }
    break;
    case V_PRED:
    {
        int i;

        for (i = 0; i < 8; i++)
        {
            vpx_memcpy(upred_ptr, uabove_row, 8);
            vpx_memcpy(vpred_ptr, vabove_row, 8);
            upred_ptr += uv_stride; /*8;*/
            vpred_ptr += uv_stride; /*8;*/
        }

    }
    break;
    case H_PRED:
    {
        int i;

        for (i = 0; i < 8; i++)
        {
            vpx_memset(upred_ptr, uleft_col[i], 8);
            vpx_memset(vpred_ptr, vleft_col[i], 8);
            upred_ptr += uv_stride; /*8;*/
            vpred_ptr += uv_stride; /*8;*/
        }
    }

    break;
    case TM_PRED:
    {
        int i;

        for (i = 0; i < 8; i++)
        {
            for (j = 0; j < 8; j++)
            {
                int predu = uleft_col[i] + uabove_row[j] - utop_left;
                int predv = vleft_col[i] + vabove_row[j] - vtop_left;

                if (predu < 0)
                    predu = 0;

                if (predu > 255)
                    predu = 255;

                if (predv < 0)
                    predv = 0;

                if (predv > 255)
                    predv = 255;

                upred_ptr[j] = predu;
                vpred_ptr[j] = predv;
            }

            upred_ptr += uv_stride; /*8;*/
            vpred_ptr += uv_stride; /*8;*/
        }

    }
    break;
    case B_PRED:
    case NEARESTMV:
    case NEARMV:
    case ZEROMV:
    case NEWMV:
    case SPLITMV:
    case MB_MODE_COUNT:
        break;
    }
#else
    (void) pbi;
    (void) x;
    (void) mb_row;
    (void) mb_col;
#endif
}


void vp8mt_predict_intra4x4(VP8D_COMP *pbi,
                          MACROBLOCKD *xd,
                          int b_mode,
                          unsigned char *predictor,
                          int mb_row,
                          int mb_col,
                          int num)
{
#if CONFIG_MULTITHREAD
    int i, r, c;

    unsigned char *Above;   /* = *(x->base_dst) + x->dst - x->dst_stride; */
    unsigned char Left[4];
    unsigned char top_left; /* = Above[-1]; */

    BLOCKD *x = &xd->block[num];

    /*Caution: For some b_mode, it needs 8 pixels (4 above + 4 above-right).*/
    if (num < 4 && pbi->common.filter_level)
        Above = pbi->mt_yabove_row[mb_row] + mb_col*16 + num*4 + 32;
    else
        Above = *(x->base_dst) + x->dst - x->dst_stride;

    if (num%4==0 && pbi->common.filter_level)
    {
        for (i=0; i<4; i++)
            Left[i] = pbi->mt_yleft_col[mb_row][num + i];
    }else
    {
        Left[0] = (*(x->base_dst))[x->dst - 1];
        Left[1] = (*(x->base_dst))[x->dst - 1 + x->dst_stride];
        Left[2] = (*(x->base_dst))[x->dst - 1 + 2 * x->dst_stride];
        Left[3] = (*(x->base_dst))[x->dst - 1 + 3 * x->dst_stride];
    }

    if ((num==4 || num==8 || num==12) && pbi->common.filter_level)
        top_left = pbi->mt_yleft_col[mb_row][num-1];
    else
        top_left = Above[-1];

     switch (b_mode)
    {
    case B_DC_PRED:
    {
        int expected_dc = 0;

        for (i = 0; i < 4; i++)
        {
            expected_dc += Above[i];
            expected_dc += Left[i];
        }

        expected_dc = (expected_dc + 4) >> 3;

        for (r = 0; r < 4; r++)
        {
            for (c = 0; c < 4; c++)
            {
                predictor[c] = expected_dc;
            }

            predictor += 16;
        }
    }
    break;
    case B_TM_PRED:
    {
        /* prediction similar to true_motion prediction */
        for (r = 0; r < 4; r++)
        {
            for (c = 0; c < 4; c++)
            {
                int pred = Above[c] - top_left + Left[r];

                if (pred < 0)
                    pred = 0;

                if (pred > 255)
                    pred = 255;

                predictor[c] = pred;
            }

            predictor += 16;
        }
    }
    break;

    case B_VE_PRED:
    {

        unsigned int ap[4];
        ap[0] = (top_left  + 2 * Above[0] + Above[1] + 2) >> 2;
        ap[1] = (Above[0] + 2 * Above[1] + Above[2] + 2) >> 2;
        ap[2] = (Above[1] + 2 * Above[2] + Above[3] + 2) >> 2;
        ap[3] = (Above[2] + 2 * Above[3] + Above[4] + 2) >> 2;

        for (r = 0; r < 4; r++)
        {
            for (c = 0; c < 4; c++)
            {

                predictor[c] = ap[c];
            }

            predictor += 16;
        }

    }
    break;


    case B_HE_PRED:
    {

        unsigned int lp[4];
        lp[0] = (top_left + 2 * Left[0] + Left[1] + 2) >> 2;
        lp[1] = (Left[0] + 2 * Left[1] + Left[2] + 2) >> 2;
        lp[2] = (Left[1] + 2 * Left[2] + Left[3] + 2) >> 2;
        lp[3] = (Left[2] + 2 * Left[3] + Left[3] + 2) >> 2;

        for (r = 0; r < 4; r++)
        {
            for (c = 0; c < 4; c++)
            {
                predictor[c] = lp[r];
            }

            predictor += 16;
        }
    }
    break;
    case B_LD_PRED:
    {
        unsigned char *ptr = Above;
        predictor[0 * 16 + 0] = (ptr[0] + ptr[1] * 2 + ptr[2] + 2) >> 2;
        predictor[0 * 16 + 1] =
            predictor[1 * 16 + 0] = (ptr[1] + ptr[2] * 2 + ptr[3] + 2) >> 2;
        predictor[0 * 16 + 2] =
            predictor[1 * 16 + 1] =
                predictor[2 * 16 + 0] = (ptr[2] + ptr[3] * 2 + ptr[4] + 2) >> 2;
        predictor[0 * 16 + 3] =
            predictor[1 * 16 + 2] =
                predictor[2 * 16 + 1] =
                    predictor[3 * 16 + 0] = (ptr[3] + ptr[4] * 2 + ptr[5] + 2) >> 2;
        predictor[1 * 16 + 3] =
            predictor[2 * 16 + 2] =
                predictor[3 * 16 + 1] = (ptr[4] + ptr[5] * 2 + ptr[6] + 2) >> 2;
        predictor[2 * 16 + 3] =
            predictor[3 * 16 + 2] = (ptr[5] + ptr[6] * 2 + ptr[7] + 2) >> 2;
        predictor[3 * 16 + 3] = (ptr[6] + ptr[7] * 2 + ptr[7] + 2) >> 2;

    }
    break;
    case B_RD_PRED:
    {

        unsigned char pp[9];

        pp[0] = Left[3];
        pp[1] = Left[2];
        pp[2] = Left[1];
        pp[3] = Left[0];
        pp[4] = top_left;
        pp[5] = Above[0];
        pp[6] = Above[1];
        pp[7] = Above[2];
        pp[8] = Above[3];

        predictor[3 * 16 + 0] = (pp[0] + pp[1] * 2 + pp[2] + 2) >> 2;
        predictor[3 * 16 + 1] =
            predictor[2 * 16 + 0] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        predictor[3 * 16 + 2] =
            predictor[2 * 16 + 1] =
                predictor[1 * 16 + 0] = (pp[2] + pp[3] * 2 + pp[4] + 2) >> 2;
        predictor[3 * 16 + 3] =
            predictor[2 * 16 + 2] =
                predictor[1 * 16 + 1] =
                    predictor[0 * 16 + 0] = (pp[3] + pp[4] * 2 + pp[5] + 2) >> 2;
        predictor[2 * 16 + 3] =
            predictor[1 * 16 + 2] =
                predictor[0 * 16 + 1] = (pp[4] + pp[5] * 2 + pp[6] + 2) >> 2;
        predictor[1 * 16 + 3] =
            predictor[0 * 16 + 2] = (pp[5] + pp[6] * 2 + pp[7] + 2) >> 2;
        predictor[0 * 16 + 3] = (pp[6] + pp[7] * 2 + pp[8] + 2) >> 2;

    }
    break;
    case B_VR_PRED:
    {

        unsigned char pp[9];

        pp[0] = Left[3];
        pp[1] = Left[2];
        pp[2] = Left[1];
        pp[3] = Left[0];
        pp[4] = top_left;
        pp[5] = Above[0];
        pp[6] = Above[1];
        pp[7] = Above[2];
        pp[8] = Above[3];


        predictor[3 * 16 + 0] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        predictor[2 * 16 + 0] = (pp[2] + pp[3] * 2 + pp[4] + 2) >> 2;
        predictor[3 * 16 + 1] =
            predictor[1 * 16 + 0] = (pp[3] + pp[4] * 2 + pp[5] + 2) >> 2;
        predictor[2 * 16 + 1] =
            predictor[0 * 16 + 0] = (pp[4] + pp[5] + 1) >> 1;
        predictor[3 * 16 + 2] =
            predictor[1 * 16 + 1] = (pp[4] + pp[5] * 2 + pp[6] + 2) >> 2;
        predictor[2 * 16 + 2] =
            predictor[0 * 16 + 1] = (pp[5] + pp[6] + 1) >> 1;
        predictor[3 * 16 + 3] =
            predictor[1 * 16 + 2] = (pp[5] + pp[6] * 2 + pp[7] + 2) >> 2;
        predictor[2 * 16 + 3] =
            predictor[0 * 16 + 2] = (pp[6] + pp[7] + 1) >> 1;
        predictor[1 * 16 + 3] = (pp[6] + pp[7] * 2 + pp[8] + 2) >> 2;
        predictor[0 * 16 + 3] = (pp[7] + pp[8] + 1) >> 1;

    }
    break;
    case B_VL_PRED:
    {

        unsigned char *pp = Above;

        predictor[0 * 16 + 0] = (pp[0] + pp[1] + 1) >> 1;
        predictor[1 * 16 + 0] = (pp[0] + pp[1] * 2 + pp[2] + 2) >> 2;
        predictor[2 * 16 + 0] =
            predictor[0 * 16 + 1] = (pp[1] + pp[2] + 1) >> 1;
        predictor[1 * 16 + 1] =
            predictor[3 * 16 + 0] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        predictor[2 * 16 + 1] =
            predictor[0 * 16 + 2] = (pp[2] + pp[3] + 1) >> 1;
        predictor[3 * 16 + 1] =
            predictor[1 * 16 + 2] = (pp[2] + pp[3] * 2 + pp[4] + 2) >> 2;
        predictor[0 * 16 + 3] =
            predictor[2 * 16 + 2] = (pp[3] + pp[4] + 1) >> 1;
        predictor[1 * 16 + 3] =
            predictor[3 * 16 + 2] = (pp[3] + pp[4] * 2 + pp[5] + 2) >> 2;
        predictor[2 * 16 + 3] = (pp[4] + pp[5] * 2 + pp[6] + 2) >> 2;
        predictor[3 * 16 + 3] = (pp[5] + pp[6] * 2 + pp[7] + 2) >> 2;
    }
    break;

    case B_HD_PRED:
    {
        unsigned char pp[9];
        pp[0] = Left[3];
        pp[1] = Left[2];
        pp[2] = Left[1];
        pp[3] = Left[0];
        pp[4] = top_left;
        pp[5] = Above[0];
        pp[6] = Above[1];
        pp[7] = Above[2];
        pp[8] = Above[3];


        predictor[3 * 16 + 0] = (pp[0] + pp[1] + 1) >> 1;
        predictor[3 * 16 + 1] = (pp[0] + pp[1] * 2 + pp[2] + 2) >> 2;
        predictor[2 * 16 + 0] =
            predictor[3 * 16 + 2] = (pp[1] + pp[2] + 1) >> 1;
        predictor[2 * 16 + 1] =
            predictor[3 * 16 + 3] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        predictor[2 * 16 + 2] =
            predictor[1 * 16 + 0] = (pp[2] + pp[3] + 1) >> 1;
        predictor[2 * 16 + 3] =
            predictor[1 * 16 + 1] = (pp[2] + pp[3] * 2 + pp[4] + 2) >> 2;
        predictor[1 * 16 + 2] =
            predictor[0 * 16 + 0] = (pp[3] + pp[4] + 1) >> 1;
        predictor[1 * 16 + 3] =
            predictor[0 * 16 + 1] = (pp[3] + pp[4] * 2 + pp[5] + 2) >> 2;
        predictor[0 * 16 + 2] = (pp[4] + pp[5] * 2 + pp[6] + 2) >> 2;
        predictor[0 * 16 + 3] = (pp[5] + pp[6] * 2 + pp[7] + 2) >> 2;
    }
    break;


    case B_HU_PRED:
    {
        unsigned char *pp = Left;
        predictor[0 * 16 + 0] = (pp[0] + pp[1] + 1) >> 1;
        predictor[0 * 16 + 1] = (pp[0] + pp[1] * 2 + pp[2] + 2) >> 2;
        predictor[0 * 16 + 2] =
            predictor[1 * 16 + 0] = (pp[1] + pp[2] + 1) >> 1;
        predictor[0 * 16 + 3] =
            predictor[1 * 16 + 1] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        predictor[1 * 16 + 2] =
            predictor[2 * 16 + 0] = (pp[2] + pp[3] + 1) >> 1;
        predictor[1 * 16 + 3] =
            predictor[2 * 16 + 1] = (pp[2] + pp[3] * 2 + pp[3] + 2) >> 2;
        predictor[2 * 16 + 2] =
            predictor[2 * 16 + 3] =
                predictor[3 * 16 + 0] =
                    predictor[3 * 16 + 1] =
                        predictor[3 * 16 + 2] =
                            predictor[3 * 16 + 3] = pp[3];
    }
    break;


    }
#else
    (void) pbi;
    (void) xd;
    (void) b_mode;
    (void) predictor;
    (void) mb_row;
    (void) mb_col;
    (void) num;
#endif
}

/* copy 4 bytes from the above right down so that the 4x4 prediction modes using pixels above and
 * to the right prediction have filled in pixels to use.
 */
void vp8mt_intra_prediction_down_copy(VP8D_COMP *pbi, MACROBLOCKD *x, int mb_row, int mb_col)
{
#if CONFIG_MULTITHREAD
    unsigned char *above_right;   /* = *(x->block[0].base_dst) + x->block[0].dst - x->block[0].dst_stride + 16; */
    unsigned int *src_ptr;
    unsigned int *dst_ptr0;
    unsigned int *dst_ptr1;
    unsigned int *dst_ptr2;

    if (pbi->common.filter_level)
        above_right = pbi->mt_yabove_row[mb_row] + mb_col*16 + 32 +16;
    else
        above_right = *(x->block[0].base_dst) + x->block[0].dst - x->block[0].dst_stride + 16;

    src_ptr = (unsigned int *)above_right;
    /*dst_ptr0 = (unsigned int *)(above_right + 4 * x->block[0].dst_stride);
    dst_ptr1 = (unsigned int *)(above_right + 8 * x->block[0].dst_stride);
    dst_ptr2 = (unsigned int *)(above_right + 12 * x->block[0].dst_stride);*/
    dst_ptr0 = (unsigned int *)(*(x->block[0].base_dst) + x->block[0].dst + 16 + 3 * x->block[0].dst_stride);
    dst_ptr1 = (unsigned int *)(*(x->block[0].base_dst) + x->block[0].dst + 16 + 7 * x->block[0].dst_stride);
    dst_ptr2 = (unsigned int *)(*(x->block[0].base_dst) + x->block[0].dst + 16 + 11 * x->block[0].dst_stride);
    *dst_ptr0 = *src_ptr;
    *dst_ptr1 = *src_ptr;
    *dst_ptr2 = *src_ptr;
#else
    (void) pbi;
    (void) x;
    (void) mb_row;
    (void) mb_col;
#endif
}
