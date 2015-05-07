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
 * File Name:  omxSP_FFTInit_R_S16S32.c
 * OpenMAX DL: v1.0.2
 * Last Modified Revision:   7777
 * Last Modified Date:       Thu, 27 Sep 2007
 * 
 * (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
 * 
 * 
 * Description: 
 * Initialize the real forward-FFT specification information struct.
 */

#include "dl/api/armOMX.h"
#include "dl/api/omxtypes.h"
#include "dl/sp/api/armSP.h"
#include "dl/sp/api/omxSP.h"



/**
 * Function: omxSP_FFTInit_R_S16_S32
 *
 * Description:
 * Initialize the real forward-FFT specification information struct.
 *
 * Remarks:
 * This function is used to initialize the specification structures
 * for functions <ippsFFTFwd_RToCCS_S16_S32_Sfs> and
 * <ippsFFTInv_CCSToR_S32_S16_Sfs>. Memory for *pFFTSpec must be
 * allocated prior to calling this function. The number of bytes
 * required for *pFFTSpec can be determined using
 * <FFTGetBufSize_R_S16_S32>.
 *
 * Parameters:
 * [in]  order       base-2 logarithm of the desired block length;
 *			   valid in the range [0,12].
 * [out] pFFTFwdSpec pointer to the initialized specification structure.
 *
 * Return Value:
 * Standard omxError result. See enumeration for possible result codes.
 *
 */

OMXResult omxSP_FFTInit_R_S16S32(
     OMXFFTSpec_R_S16S32* pFFTSpec,
     OMX_INT order
)
{
    OMX_INT     i,j;
    OMX_SC32    *pTwiddle,*pTwiddle1,*pTwiddle2,*pTwiddle3,*pTwiddle4;
    OMX_S32     *pBuf;
    OMX_U16     *pBitRev;
    OMX_U32     pTmp;
    OMX_INT     Nby2,N,M,diff, step; 
    OMX_S32     x,y,xNeg;
    ARMsFFTSpec_R_SC32 *pFFTStruct = 0;

   
    pFFTStruct = (ARMsFFTSpec_R_SC32 *) pFFTSpec;

    /* if order zero no init is needed */
    if (order == 0)
    {
        pFFTStruct->N = 1;
        pFFTStruct->pTwiddle = NULL;
        pFFTStruct->pBuf = (OMX_S32 *)
               (sizeof(ARMsFFTSpec_R_SC32) + (OMX_S8*) pFFTSpec);
        
        return OMX_Sts_NoErr;
    }

    /* Do the initializations */
    Nby2 = 1 << (order - 1);
    N = Nby2 << 1;
                    
    
    
    pBitRev = NULL ;                /* optimized implementations don't use bitreversal */
    
    pTwiddle = (OMX_SC32 *) 
        (sizeof(ARMsFFTSpec_R_SC32) + (OMX_S8*) pFFTSpec);
    
    /* Align to 32 byte boundary */
    pTmp = ((OMX_U32)pTwiddle)&31;              /* (OMX_U32)pTwiddle % 32 */
    if(pTmp != 0)
        pTwiddle = (OMX_SC32*) ((OMX_S8*)pTwiddle + (32-pTmp));                    
        
        
    pBuf = (OMX_S32*)        
        (sizeof(OMX_SC32) * (5*N/8) + (OMX_S8*) pTwiddle);
        
    /* Align to 32 byte boundary */
    pTmp = ((OMX_U32)pBuf)&31;                 /* (OMX_U32)pBuf % 32 */
    if(pTmp != 0)
        pBuf = (OMX_S32*) ((OMX_S8*)pBuf + (32-pTmp));                        
                    
        
    
    
    /* 
     * Filling Twiddle factors : exp^(-j*2*PI*k/ (N/2) ) ; k=0,1,2,...,3/4(N/2)
     * N/2 point complex FFT is used to compute N point real FFT
     * The original twiddle table "armSP_FFT_S32TwiddleTable" is of size (MaxSize/8 + 1)
     * Rest of the values i.e., upto MaxSize are calculated using the symmetries of sin and cos
     * The max size of the twiddle table needed is 3/4(N/2) for a radix-4 stage
     *
     * W = (-2 * PI) / N 
     * N = 1 << order
     * W = -PI >> (order - 1)
     */
    
    M = Nby2>>3;
    diff = 12 - (order-1);
    step = 1<<diff;             /* step into the twiddle table for the current order */
    
    x = armSP_FFT_S32TwiddleTable[0];
    y = armSP_FFT_S32TwiddleTable[1];
    xNeg = 0x7FFFFFFF;
    
    if((order-1) >=3)    
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
            
            x = armSP_FFT_S32TwiddleTable[2*j];
            y = armSP_FFT_S32TwiddleTable[2*j+1];
            
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
        if ((order-1) == 2)
        {
            pTwiddle[0].Re = x;
            pTwiddle[0].Im = y;
            pTwiddle[1].Re = -y;
            pTwiddle[1].Im = xNeg;
            pTwiddle[2].Re = xNeg;
            pTwiddle[2].Im = y;
        
        }
        if ((order-1) == 1)
        {
            pTwiddle[0].Re = x;
            pTwiddle[0].Im = y;
        
        }        
        
    
    }
    
    
    /*
     * Now fill the last N/4 values : exp^(-j*2*PI*k/N) ;  k=1,3,5,...,N/2-1 
     * These are used for the final twiddle fix-up for converting complex to real FFT
     */
     
    M = N>>3;
    diff = 12 - order;
    step = 1<<diff;
    
    pTwiddle1 = pTwiddle + 3*N/8;
    pTwiddle4 = pTwiddle1 + (N/4-1);
    pTwiddle3 = pTwiddle1 + N/8;
    pTwiddle2 = pTwiddle1 + (N/8-1);
    
    x = armSP_FFT_S32TwiddleTable[0];
    y = armSP_FFT_S32TwiddleTable[1];
    xNeg = 0x7FFFFFFF;
    
    if((order) >=3)    
    {
                        
    
        for (i=1; i<=M; i+=2 )
          {
            j = i*step;
            
            x = armSP_FFT_S32TwiddleTable[2*j];
            y = armSP_FFT_S32TwiddleTable[2*j+1];
            
            pTwiddle1[0].Re = x;
            pTwiddle1[0].Im = y;
            pTwiddle1 += 1;
            pTwiddle2[0].Re = -y;
            pTwiddle2[0].Im = -x;
            pTwiddle2 -= 1;
            pTwiddle3[0].Re = y;
            pTwiddle3[0].Im = -x;
            pTwiddle3 += 1;
            pTwiddle4[0].Re = -x;
            pTwiddle4[0].Im = y;
            pTwiddle4 -= 1;
            
        }
        
     
    }
    else
    {
        if (order == 2)
        {
            
            pTwiddle1[0].Re = -y;
            pTwiddle1[0].Im = xNeg;
            
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

