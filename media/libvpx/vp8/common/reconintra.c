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
#include "vp8_rtcd.h"
#include "vpx_mem/vpx_mem.h"
#include "blockd.h"

void vp8_build_intra_predictors_mby_s_c(MACROBLOCKD *x,
                                          unsigned char * yabove_row,
                                          unsigned char * yleft,
                                          int left_stride,
                                          unsigned char * ypred_ptr,
                                          int y_stride)
{
    unsigned char yleft_col[16];
    unsigned char ytop_left = yabove_row[-1];
    int r, c, i;

    for (i = 0; i < 16; i++)
    {
        yleft_col[i] = yleft[i* left_stride];
    }

    /* for Y */
    switch (x->mode_info_context->mbmi.mode)
    {
    case DC_PRED:
    {
        int expected_dc;
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
            ypred_ptr += y_stride;
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
            ypred_ptr += y_stride;
        }
    }
    break;
    case H_PRED:
    {

        for (r = 0; r < 16; r++)
        {

            vpx_memset(ypred_ptr, yleft_col[r], 16);
            ypred_ptr += y_stride;
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

            ypred_ptr += y_stride;
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
}

void vp8_build_intra_predictors_mbuv_s_c(MACROBLOCKD *x,
                                         unsigned char * uabove_row,
                                         unsigned char * vabove_row,
                                         unsigned char * uleft,
                                         unsigned char * vleft,
                                         int left_stride,
                                         unsigned char * upred_ptr,
                                         unsigned char * vpred_ptr,
                                         int pred_stride)
{
    unsigned char uleft_col[8];
    unsigned char utop_left = uabove_row[-1];
    unsigned char vleft_col[8];
    unsigned char vtop_left = vabove_row[-1];

    int i, j;

    for (i = 0; i < 8; i++)
    {
        uleft_col[i] = uleft [i* left_stride];
        vleft_col[i] = vleft [i* left_stride];
    }

    switch (x->mode_info_context->mbmi.uv_mode)
    {
    case DC_PRED:
    {
        int expected_udc;
        int expected_vdc;
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


        /*vpx_memset(upred_ptr,expected_udc,64);*/
        /*vpx_memset(vpred_ptr,expected_vdc,64);*/
        for (i = 0; i < 8; i++)
        {
            vpx_memset(upred_ptr, expected_udc, 8);
            vpx_memset(vpred_ptr, expected_vdc, 8);
            upred_ptr += pred_stride;
            vpred_ptr += pred_stride;
        }
    }
    break;
    case V_PRED:
    {
        for (i = 0; i < 8; i++)
        {
            vpx_memcpy(upred_ptr, uabove_row, 8);
            vpx_memcpy(vpred_ptr, vabove_row, 8);
            upred_ptr += pred_stride;
            vpred_ptr += pred_stride;
        }

    }
    break;
    case H_PRED:
    {
        for (i = 0; i < 8; i++)
        {
            vpx_memset(upred_ptr, uleft_col[i], 8);
            vpx_memset(vpred_ptr, vleft_col[i], 8);
            upred_ptr += pred_stride;
            vpred_ptr += pred_stride;
        }
    }

    break;
    case TM_PRED:
    {
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

            upred_ptr += pred_stride;
            vpred_ptr += pred_stride;
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
}
