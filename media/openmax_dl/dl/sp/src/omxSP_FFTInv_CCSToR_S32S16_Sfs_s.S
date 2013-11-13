@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This file was originally licensed as follows. It has been
@//  relicensed with permission from the copyright holders.
@//

@// 
@// File Name:  omxSP_FFTInv_CCSToR_S32S16_Sfs_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   7098
@// Last Modified Date:       Thu, 16 Aug 2007
@// 
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@// 
@// 
@//
@// Description:
@// Compute an inverse FFT for a complex signal
@// 


        
@// Include standard headers

#include "dl/api/armCOMM_s.h"
#include "dl/api/omxtypes_s.h"
        
        
@// Import symbols required from other files
@// (For example tables)
        
        .extern  omxSP_FFTInv_CCSToR_S32_Sfs
        
        
@// Set debugging level        
@//DEBUG_ON    SETL {TRUE}



@// Guarding implementation by the processor name
    
    
    
      @// Guarding implementation by the processor name
    
    
@// Import symbols required from other files
@// (For example tables)
     
    
@//Input Registers

#define pSrc            r0
#define pDst            r1
#define pFFTSpec        r2
#define scale           r3


@// Output registers
#define result          r0


#define N               r6
#define pOut            r5
#define pTmpDst         r4


@// Neon registers

#define dX0     D0.S32
#define dX01    D1.S32  
#define qX0     Q0.S32
#define dY0     D2.S16
#define dY0S32  D2.S32



    @// Allocate stack memory required by the function
        
    @// Write function header
        M_START     omxSP_FFTInv_CCSToR_S32S16_Sfs,r11,d15
        
        .set    ARMsFFTSpec_N, 0
        .set    ARMsFFTSpec_pBitRev, 4
        .set    ARMsFFTSpec_pTwiddle, 8
        .set    ARMsFFTSpec_pBuf, 12

        @// Define stack arguments
        
        @// Read the size from structure and take log
        LDR     N, [pFFTSpec, #ARMsFFTSpec_N]
        
        @// Read other structure parameters
        @//LDR     pTwiddle, [pFFTSpec, #ARMsFFTSpec_pTwiddle]
        LDR     pOut, [pFFTSpec, #ARMsFFTSpec_pBuf]
        
        
        MOV     pTmpDst,pDst
        ADD     pDst,pOut,N, LSL #2
        
        
        BL      omxSP_FFTInv_CCSToR_S32_Sfs
        
        ADD     pDst,pOut,N, LSL #2
        
        CMP     N,#2
        BGT     copyLoop
        BEQ     copyS32ToS16
        VLD1    dX0[0],[pDst]
        VQMOVN  dY0,qX0
        VST1    dY0[0],[pTmpDst]
        
        B       End
        
copyS32ToS16:   
        
        VLD1    dX0,[pDst]
        VQMOVN  dY0,qX0
        VST1    dY0S32[0],[pTmpDst]
        B       End

copyLoop:               
              
        VLD1    {dX0,dX01},[pDst]!
        SUBS    N,N,#4
        VQMOVN  dY0,qX0
        VST1    dY0,[pTmpDst]!
        
        BGT     copyLoop
                
                       
End:                            
        @// Set return value
        MOV     result, #OMX_Sts_NoErr       

        @// Write function tail
        M_END
        
    .end
