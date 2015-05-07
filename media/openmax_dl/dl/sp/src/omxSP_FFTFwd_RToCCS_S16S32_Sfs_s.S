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
@// File Name:  omxSP_FFTFwd_RToCCS_S16S32_Sfs_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   7403
@// Last Modified Date:       Mon, 17 Sep 2007
@// 
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@// 
@// 
@//
@// Description:
@// Compute FFT for a real signal
@// 

        
@// Include standard headers

#include "dl/api/armCOMM_s.h"
#include "dl/api/omxtypes_s.h"
        
        
@// Import symbols required from other files
@// (For example tables)
        .extern  omxSP_FFTFwd_RToCCS_S32_Sfs
        
        
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
        
#define pTmpDst         r4
#define pTmpSrc         r5
#define N               r6
#define order           r7
#define pOut            r8

@// Neon registers

#define dX0             D0.S16
#define qY0             Q1.S32
#define dY0S32          D2.S32
#define qX0             Q1.S32
#define dY1S32          D3.S32
#define dX0S32          D0.S32




    @// Allocate stack memory required by the function
        
    @// Write function header
        M_START     omxSP_FFTFwd_RToCCS_S16S32_Sfs,r11,d15

@ Structure offsets for the FFTSpec             
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
        
        
        @//  N=1 Treat seperately  
        CMP     N,#1
        BGT     sizeGreaterThanOne
        VLD1    dX0[0],[pSrc]
        VMOVL   qY0,dX0
        VST1    dY0S32[0],[pDst]
        
        MOV     pSrc,pDst
        B       realS32FFT
        
sizeGreaterThanOne:        
        MOV     N,N,ASR #1
        
        CLZ     order,N                             @// N = 2^order 
        RSB     order,order,#31
        
        TST     order,#1
        MOVEQ   pTmpDst,pOut
        MOVNE   pTmpDst,pDst
        MOV     pTmpSrc,pTmpDst
        
        CMP     N,#1
        BGT     copyS16ToS32
        VLD1    dX0S32[0],[pSrc]
        VMOVL   qX0,dX0
        VST1    dY0S32,[pTmpDst]
        B       setpSrc

        
copyS16ToS32:               
        
        VLD1    dX0,[pSrc]!
        SUBS    N,N,#2
        VMOVL   qX0,dX0
        VST1    {dY0S32,dY1S32},[pTmpDst]!
        BGT     copyS16ToS32
 
setpSrc:                
        MOV     pSrc,pTmpSrc
        
        
              
realS32FFT:             
        BL      omxSP_FFTFwd_RToCCS_S32_Sfs        
        
                
                       
End:                            
        @// Set return value
        MOV     result, #OMX_Sts_NoErr       

        @// Write function tail
        M_END
        .end
                
