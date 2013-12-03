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
#include "vpx_rtcd.h"
#include "blockd.h"

void vp8_intra4x4_predict_c(unsigned char *Above,
                            unsigned char *yleft, int left_stride,
                            B_PREDICTION_MODE b_mode,
                            unsigned char *dst, int dst_stride,
                            unsigned char top_left)
{
    int i, r, c;

    unsigned char Left[4];
    Left[0] = yleft[0];
    Left[1] = yleft[left_stride];
    Left[2] = yleft[2 * left_stride];
    Left[3] = yleft[3 * left_stride];

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
                dst[c] = expected_dc;
            }

            dst += dst_stride;
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

                dst[c] = pred;
            }

            dst += dst_stride;
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

                dst[c] = ap[c];
            }

            dst += dst_stride;
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
                dst[c] = lp[r];
            }

            dst += dst_stride;
        }
    }
    break;
    case B_LD_PRED:
    {
        unsigned char *ptr = Above;
        dst[0 * dst_stride + 0] = (ptr[0] + ptr[1] * 2 + ptr[2] + 2) >> 2;
        dst[0 * dst_stride + 1] =
            dst[1 * dst_stride + 0] = (ptr[1] + ptr[2] * 2 + ptr[3] + 2) >> 2;
        dst[0 * dst_stride + 2] =
            dst[1 * dst_stride + 1] =
                dst[2 * dst_stride + 0] = (ptr[2] + ptr[3] * 2 + ptr[4] + 2) >> 2;
        dst[0 * dst_stride + 3] =
            dst[1 * dst_stride + 2] =
                dst[2 * dst_stride + 1] =
                    dst[3 * dst_stride + 0] = (ptr[3] + ptr[4] * 2 + ptr[5] + 2) >> 2;
        dst[1 * dst_stride + 3] =
            dst[2 * dst_stride + 2] =
                dst[3 * dst_stride + 1] = (ptr[4] + ptr[5] * 2 + ptr[6] + 2) >> 2;
        dst[2 * dst_stride + 3] =
            dst[3 * dst_stride + 2] = (ptr[5] + ptr[6] * 2 + ptr[7] + 2) >> 2;
        dst[3 * dst_stride + 3] = (ptr[6] + ptr[7] * 2 + ptr[7] + 2) >> 2;

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

        dst[3 * dst_stride + 0] = (pp[0] + pp[1] * 2 + pp[2] + 2) >> 2;
        dst[3 * dst_stride + 1] =
            dst[2 * dst_stride + 0] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        dst[3 * dst_stride + 2] =
            dst[2 * dst_stride + 1] =
                dst[1 * dst_stride + 0] = (pp[2] + pp[3] * 2 + pp[4] + 2) >> 2;
        dst[3 * dst_stride + 3] =
            dst[2 * dst_stride + 2] =
                dst[1 * dst_stride + 1] =
                    dst[0 * dst_stride + 0] = (pp[3] + pp[4] * 2 + pp[5] + 2) >> 2;
        dst[2 * dst_stride + 3] =
            dst[1 * dst_stride + 2] =
                dst[0 * dst_stride + 1] = (pp[4] + pp[5] * 2 + pp[6] + 2) >> 2;
        dst[1 * dst_stride + 3] =
            dst[0 * dst_stride + 2] = (pp[5] + pp[6] * 2 + pp[7] + 2) >> 2;
        dst[0 * dst_stride + 3] = (pp[6] + pp[7] * 2 + pp[8] + 2) >> 2;

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


        dst[3 * dst_stride + 0] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        dst[2 * dst_stride + 0] = (pp[2] + pp[3] * 2 + pp[4] + 2) >> 2;
        dst[3 * dst_stride + 1] =
            dst[1 * dst_stride + 0] = (pp[3] + pp[4] * 2 + pp[5] + 2) >> 2;
        dst[2 * dst_stride + 1] =
            dst[0 * dst_stride + 0] = (pp[4] + pp[5] + 1) >> 1;
        dst[3 * dst_stride + 2] =
            dst[1 * dst_stride + 1] = (pp[4] + pp[5] * 2 + pp[6] + 2) >> 2;
        dst[2 * dst_stride + 2] =
            dst[0 * dst_stride + 1] = (pp[5] + pp[6] + 1) >> 1;
        dst[3 * dst_stride + 3] =
            dst[1 * dst_stride + 2] = (pp[5] + pp[6] * 2 + pp[7] + 2) >> 2;
        dst[2 * dst_stride + 3] =
            dst[0 * dst_stride + 2] = (pp[6] + pp[7] + 1) >> 1;
        dst[1 * dst_stride + 3] = (pp[6] + pp[7] * 2 + pp[8] + 2) >> 2;
        dst[0 * dst_stride + 3] = (pp[7] + pp[8] + 1) >> 1;

    }
    break;
    case B_VL_PRED:
    {

        unsigned char *pp = Above;

        dst[0 * dst_stride + 0] = (pp[0] + pp[1] + 1) >> 1;
        dst[1 * dst_stride + 0] = (pp[0] + pp[1] * 2 + pp[2] + 2) >> 2;
        dst[2 * dst_stride + 0] =
            dst[0 * dst_stride + 1] = (pp[1] + pp[2] + 1) >> 1;
        dst[1 * dst_stride + 1] =
            dst[3 * dst_stride + 0] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        dst[2 * dst_stride + 1] =
            dst[0 * dst_stride + 2] = (pp[2] + pp[3] + 1) >> 1;
        dst[3 * dst_stride + 1] =
            dst[1 * dst_stride + 2] = (pp[2] + pp[3] * 2 + pp[4] + 2) >> 2;
        dst[0 * dst_stride + 3] =
            dst[2 * dst_stride + 2] = (pp[3] + pp[4] + 1) >> 1;
        dst[1 * dst_stride + 3] =
            dst[3 * dst_stride + 2] = (pp[3] + pp[4] * 2 + pp[5] + 2) >> 2;
        dst[2 * dst_stride + 3] = (pp[4] + pp[5] * 2 + pp[6] + 2) >> 2;
        dst[3 * dst_stride + 3] = (pp[5] + pp[6] * 2 + pp[7] + 2) >> 2;
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


        dst[3 * dst_stride + 0] = (pp[0] + pp[1] + 1) >> 1;
        dst[3 * dst_stride + 1] = (pp[0] + pp[1] * 2 + pp[2] + 2) >> 2;
        dst[2 * dst_stride + 0] =
            dst[3 * dst_stride + 2] = (pp[1] + pp[2] + 1) >> 1;
        dst[2 * dst_stride + 1] =
            dst[3 * dst_stride + 3] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        dst[2 * dst_stride + 2] =
            dst[1 * dst_stride + 0] = (pp[2] + pp[3] + 1) >> 1;
        dst[2 * dst_stride + 3] =
            dst[1 * dst_stride + 1] = (pp[2] + pp[3] * 2 + pp[4] + 2) >> 2;
        dst[1 * dst_stride + 2] =
            dst[0 * dst_stride + 0] = (pp[3] + pp[4] + 1) >> 1;
        dst[1 * dst_stride + 3] =
            dst[0 * dst_stride + 1] = (pp[3] + pp[4] * 2 + pp[5] + 2) >> 2;
        dst[0 * dst_stride + 2] = (pp[4] + pp[5] * 2 + pp[6] + 2) >> 2;
        dst[0 * dst_stride + 3] = (pp[5] + pp[6] * 2 + pp[7] + 2) >> 2;
    }
    break;


    case B_HU_PRED:
    {
        unsigned char *pp = Left;
        dst[0 * dst_stride + 0] = (pp[0] + pp[1] + 1) >> 1;
        dst[0 * dst_stride + 1] = (pp[0] + pp[1] * 2 + pp[2] + 2) >> 2;
        dst[0 * dst_stride + 2] =
            dst[1 * dst_stride + 0] = (pp[1] + pp[2] + 1) >> 1;
        dst[0 * dst_stride + 3] =
            dst[1 * dst_stride + 1] = (pp[1] + pp[2] * 2 + pp[3] + 2) >> 2;
        dst[1 * dst_stride + 2] =
            dst[2 * dst_stride + 0] = (pp[2] + pp[3] + 1) >> 1;
        dst[1 * dst_stride + 3] =
            dst[2 * dst_stride + 1] = (pp[2] + pp[3] * 2 + pp[3] + 2) >> 2;
        dst[2 * dst_stride + 2] =
            dst[2 * dst_stride + 3] =
                dst[3 * dst_stride + 0] =
                    dst[3 * dst_stride + 1] =
                        dst[3 * dst_stride + 2] =
                            dst[3 * dst_stride + 3] = pp[3];
    }
    break;

    default:
    break;

    }
}
