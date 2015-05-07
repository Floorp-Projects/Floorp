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
 * File Name:  omxSP_FFTGetBufSize_R_S16S32.c
 * OpenMAX DL: v1.0.2
 * Last Modified Revision:   7777
 * Last Modified Date:       Thu, 27 Sep 2007
 * 
 * (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
 * 
 * 
 * Description:
 * Computes the size of the specification structure required.
 */

#include "dl/api/armOMX.h"
#include "dl/api/omxtypes.h"
#include "dl/sp/api/armSP.h"
#include "dl/sp/api/omxSP.h"

/**
 * Function: omxSP_FFTGetBufSize_R_S16S32
 *
 * Description:
 * Computes the size of the specification structure required for the length
 * 2^order real FFT and IFFT functions.
 *
 * Remarks:
 * This function is used in conjunction with the 16-bit functions
 * <FFTFwd_RToCCS_S16_S32_Sfs> and <FFTInv_CCSToR_S32_S16_Sfs>.
 *
 * Parameters:
 * [in]  order       base-2 logarithm of the length; valid in the range
 *			   [0,12].
 * [out] pSize	   pointer to the number of bytes required for the
 *			   specification structure.
 *
 * Return Value:
 * Standard omxError result. See enumeration for possible result codes.
 *
 */

OMXResult omxSP_FFTGetBufSize_R_S16S32(
     OMX_INT order,     
     OMX_INT *pSize
 )
{
    OMX_INT     NBy2,N,twiddleSize;
    
    
    /* Check for order zero */
    if (order == 0)
    {
        *pSize = sizeof(ARMsFFTSpec_R_SC32)  
                 + sizeof(OMX_S32) * (2); /* Extra size 'N' is used in FFTInv_CCSToR_S32S16_Sfs as a temporary buf */   
        
        return OMX_Sts_NoErr;
    }
    
    NBy2 = 1 << (order - 1);
    N = NBy2<<1;
    twiddleSize = 5*N/8;            /* 3/4(N/2) + N/4 */
    
    /* 2 pointers to store bitreversed array and twiddle factor array */
    *pSize = sizeof(ARMsFFTSpec_R_SC32)
        /* Twiddle factors  */
           + sizeof(OMX_SC32) * twiddleSize
        /* Ping Pong buffer for doing the N/2 point complex FFT  */      
           + sizeof(OMX_S32) * (N<<1)  /* Extra size 'N' is used in FFTInv_CCSToR_S32S16_Sfs as a temporary buf */
           + 62 ;  /* Extra bytes to get 32 byte alignment of ptwiddle and pBuf */ 
           
           
    return OMX_Sts_NoErr;
}

/*****************************************************************************
 *                              END OF FILE
 *****************************************************************************/

