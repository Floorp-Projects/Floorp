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
 * File Name:  omxSP_FFTGetBufSize_C_SC16.c
 * OpenMAX DL: v1.0.2
 * Last Modified Revision:   9468
 * Last Modified Date:       Thu, 03 Jan 2008
 * 
 * (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
 * 
 * 
 * Description:
 * Compute the size of the specification structure required
 */

#include "dl/api/armOMX.h"
#include "dl/api/omxtypes.h"
#include "dl/sp/api/armSP.h"
#include "dl/sp/api/omxSP.h"


/**
 * Function:  omxSP_FFTGetBufSize_C_SC16   (2.2.4.1.6)
 *
 * Description:
 * These functions compute the size of the specification structure 
 * required for the length 2^order complex FFT and IFFT functions. The function 
 * <FFTGetBufSize_C_SC16> is used in conjunction with the 16-bit functions 
 * <FFTFwd_CToC_SC16_Sfs> and <FFTInv_CToC_SC16_Sfs>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [0,12] 
 *
 * Output Arguments:
 *   
 *   pSize - pointer to the number of bytes required for the specification 
 *            structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    
 *
 */



OMXResult omxSP_FFTGetBufSize_C_SC16(
     OMX_INT order,
     OMX_INT *pSize)
{
    
    OMX_INT     N,twiddleSize;
    
    /* Check for order zero */
    if (order == 0)
    {
        *pSize = sizeof(ARMsFFTSpec_SC16);   
        return OMX_Sts_NoErr;
    }

    
    N = 1 << order;
    
    /*The max size of the twiddle table needed is 3N/4 for a radix-4 stage*/
    twiddleSize = 3*N/4;

    /* 2 pointers to store bitreversed array and twiddle factor array */
    *pSize = sizeof(ARMsFFTSpec_SC16)
        /* Twiddle factors  */
           + sizeof(OMX_SC16) * twiddleSize
        /* Ping Pong buffer   */   
           + sizeof(OMX_SC16) * N
           + 62 ;  /* Extra bytes to get 32 byte alignment of ptwiddle and pBuf */
        
    return OMX_Sts_NoErr;
}

/*****************************************************************************
 *                              END OF FILE
 *****************************************************************************/

