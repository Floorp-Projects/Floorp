/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  This file was originally licensed as follows. It has been
 *  relicensed with permission from the copyright holders.
 */

/**
 *
 * File Name:  omxSP_FFTInit_C_SC16.c
 * OpenMAX DL: v1.0.2
 * Last Modified Revision:   15322
 * Last Modified Date:       Wed, 15 Oct 2008
 *
 * (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
 *
 *
 * Description:
 * Initializes the specification structures required
 */

#include "dl/api/armOMX.h"
#include "dl/api/omxtypes.h"
#include "dl/sp/api/armSP.h"
#include "dl/sp/api/omxSP.h"


/**
 * Function: omxSP_FFTInit_C_SC16
 *
 * Description:
 * These functions initialize the specification structures required for the
 * complex FFT and IFFT functions.
 *
 * Remarks:
 * Desired block length is specified as an input. The function is used to
 * initialize the specification structures for functions <FFTFwd_CToC_SC16_Sfs>
 * and <FFTInv_CToC_SC16_Sfs>. Memory for the specification structure *pFFTSpec
 * must be allocated prior to calling this function. The space required for
 * *pFFTSpec, in bytes, can be determined using <FFTGetBufSize_C_SC16>.
 *
 * Parameters:
 * [in]  order          base-2 logarithm of the desired block length;
 *                              valid in the range [0,12].
 * [out] pFFTSpec               pointer to initialized specification structure.
 *
 * Return Value:
 * Standard omxError result. See enumeration for possible result codes.
 *
 */

OMXResult omxSP_FFTInit_C_SC16(
     OMXFFTSpec_C_SC16* pFFTSpec,
     OMX_INT order
 )
 {
    OMX_INT     i,j;
    OMX_SC16    *pTwiddle, *pBuf;
    OMX_U16     *pBitRev;
    OMX_INT     Nby2,N,M,diff,step;
    OMX_U32             pTmp;
    ARMsFFTSpec_SC16 *pFFTStruct = 0;
    OMX_S16     x,y,xNeg;
    OMX_S32     xS32,yS32;


    pFFTStruct = (ARMsFFTSpec_SC16 *) pFFTSpec;

    /* if order zero no init is needed */
    if (order == 0)
    {
        pFFTStruct->N = 1;
        return OMX_Sts_NoErr;
    }

    /* Do the initializations */
    Nby2 = 1 << (order - 1);
    N = Nby2 << 1;
    M = N>>3;

    pBitRev = NULL ;

    pTwiddle = (OMX_SC16 *)
        (sizeof(ARMsFFTSpec_SC16) + (OMX_S8*) pFFTSpec);

    /* Align to 32 byte boundary */
    pTmp = ((OMX_U32)pTwiddle)&31;              /* (OMX_U32)pTwiddle % 32 */
    if(pTmp != 0)
        pTwiddle = (OMX_SC16*) ((OMX_S8*)pTwiddle + (32-pTmp));

    pBuf = (OMX_SC16 *)
        (sizeof(OMX_SC16) * (3*N/4) + (OMX_S8*) pTwiddle);

    /* Align to 32 byte boundary */
    pTmp = ((OMX_U32)pBuf)&31;                 /* (OMX_U32)pBuf % 32 */
    if(pTmp != 0)
        pBuf = (OMX_SC16*) ((OMX_S8*)pBuf + (32-pTmp));



    /*
     * Filling Twiddle factors :
     * The original twiddle table "armSP_FFT_S16TwiddleTable" is of size (MaxSize/8 + 1)
     * Rest of the values i.e., upto MaxSize are calculated using the symmetries of sin and cos
     * The max size of the twiddle table needed is 3N/4 for a radix-4 stage
     *
     * W = (-2 * PI) / N
     * N = 1 << order
     * W = -PI >> (order - 1)
     */



    diff = 12 - order;
    step = 1<<diff;             /* step into the twiddle table for the current order */

    xS32 = armSP_FFT_S32TwiddleTable[0];
    yS32 = armSP_FFT_S32TwiddleTable[1];
    x = (xS32+0x8000)>>16;
    y = (yS32+0x8000)>>16;

    xNeg = 0x7FFF;

    if(order >=3)
    {
            /* i = 0 case */
            pTwiddle[0].Re = x;
            pTwiddle[0].Im = y;
            pTwiddle[2*M].Re = -y;
            pTwiddle[2*M].Im = xNeg;
            pTwiddle[4*M].Re = xNeg;
            pTwiddle[4*M].Im = y;


        for (i=1; i<=M; i++)
          {
            j = i*step;

            xS32 = armSP_FFT_S32TwiddleTable[2*j];
            yS32 = armSP_FFT_S32TwiddleTable[2*j+1];
            x = (xS32+0x8000)>>16;
            y = (yS32+0x8000)>>16;

            pTwiddle[i].Re = x;
            pTwiddle[i].Im = y;
            pTwiddle[2*M-i].Re = -y;
            pTwiddle[2*M-i].Im = -x;
            pTwiddle[2*M+i].Re = y;
            pTwiddle[2*M+i].Im = -x;
            pTwiddle[4*M-i].Re = -x;
            pTwiddle[4*M-i].Im = y;
            pTwiddle[4*M+i].Re = -x;
            pTwiddle[4*M+i].Im = -y;
            pTwiddle[6*M-i].Re = y;
            pTwiddle[6*M-i].Im = x;
        }


    }
    else
    {
        if (order == 2)
        {
            pTwiddle[0].Re = x;
            pTwiddle[0].Im = y;
            pTwiddle[1].Re = -y;
            pTwiddle[1].Im = xNeg;
            pTwiddle[2].Re = xNeg;
            pTwiddle[2].Im = y;

        }
        if (order == 1)
        {
            pTwiddle[0].Re = x;
            pTwiddle[0].Im = y;

        }


    }


    /* Update the structure */
    pFFTStruct->N = N;
    pFFTStruct->pTwiddle = pTwiddle;
    pFFTStruct->pBitRev = pBitRev;
    pFFTStruct->pBuf = pBuf;

    return OMX_Sts_NoErr;
}

/*****************************************************************************
 *                              END OF FILE
 *****************************************************************************/

