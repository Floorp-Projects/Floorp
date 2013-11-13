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
@// File Name:  omxSP_FFTFwd_RToCCS_S32_Sfs_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   7810
@// Last Modified Date:       Thu, 04 Oct 2007
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
        
        .extern  armSP_FFTFwd_CToC_SC32_Sfs_Radix2_fs_OutOfPlace_unsafe 
        .extern  armSP_FFTFwd_CToC_SC32_Radix2_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC32_Radix4_fs_OutOfPlace_unsafe 
        .extern  armSP_FFTFwd_CToC_SC32_Radix8_fs_OutOfPlace_unsafe 
        .extern  armSP_FFTFwd_CToC_SC32_Radix4_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC32_Sfs_Radix4_fs_OutOfPlace_unsafe 
        .extern  armSP_FFTFwd_CToC_SC32_Sfs_Radix8_fs_OutOfPlace_unsafe 
        .extern  armSP_FFTFwd_CToC_SC32_Sfs_Radix4_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC32_Sfs_Radix2_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC32_Radix2_OutOfPlace_unsafe   
        
@// Set debugging level        
@//DEBUG_ON    SETL {TRUE}



@// Guarding implementation by the processor name
    
    
    
    @// Guarding implementation by the processor name
    
@// Import symbols required from other files
@// (For example tables)
        .extern  armSP_FFTFwd_CToC_SC32_Radix4_ls_OutOfPlace_unsafe     
        .extern  armSP_FFTFwd_CToC_SC32_Radix2_ls_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC32_Sfs_Radix4_ls_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC32_Sfs_Radix2_ls_OutOfPlace_unsafe      
     
    
@//Input Registers

#define pSrc            r0
#define pDst            r1
#define pFFTSpec        r2
#define scale           r3


@// Output registers
#define result          r0

@//Local Scratch Registers

#define argTwiddle      r1
#define argDst          r2
#define argScale        r4
#define tmpOrder        r4
#define pTwiddle        r4
#define pOut            r5
#define subFFTSize      r7     
#define subFFTNum       r6
#define N               r6
#define order           r14
#define diff            r9
@// Total num of radix stages required to comple the FFT        
#define count           r8
#define x0r             r4    
#define x0i             r5
#define diffMinusOne    r2
#define subFFTSizeTmp   r6
#define step            r3
#define step1           r4
#define twStep          r8
#define zero            r9
#define pTwiddleTmp     r5
#define t0              r10

@// Neon registers

#define dX0       d0.s32
#define dzero     d1.s32
#define dZero     d2.s32
#define dShift    d3.s32
#define dX0r      d2.s32            
#define dX0i      d3.s32
#define dX1r      d4.s32
#define dX1i      d5.s32
#define dT0       d6.s32
#define dT1       d7.s32
#define dT2       d8.s32
#define dT3       d9.s32
#define qT0       q5.s64
#define qT1       q6.s64
#define dW0r      d14.s32
#define dW0i      d15.s32
#define dW1r      d16.s32
#define dW1i      d17.s32
#define dY0r      d14.s32
#define dY0i      d15.s32
#define dY1r      d16.s32
#define dY1i      d17.s32
#define dY0rS64   d14.s64
#define dY0iS64   d15.s64
#define qT2       q9.s64
#define qT3       q10.s64
@// lastThreeelements
#define dX1       d3.s32
#define dW0       d4.s32
#define dW1       d5.s32
#define dY0       d10.s32
#define dY1       d11.s32
#define dY2       d12.s32
#define dY3       d13.s32

    @// Allocate stack memory required by the function

        M_ALLOC4        diffOnStack, 4
        
    @// Write function header
        M_START     omxSP_FFTFwd_RToCCS_S32_Sfs,r11,d15
        
@ Structure offsets for the FFTSpec             
        .set    ARMsFFTSpec_N, 0
        .set    ARMsFFTSpec_pBitRev, 4
        .set    ARMsFFTSpec_pTwiddle, 8
        .set    ARMsFFTSpec_pBuf, 12

        @// Define stack arguments
        
        @// Read the size from structure and take log
        LDR     N, [pFFTSpec, #ARMsFFTSpec_N]
        
        @// Read other structure parameters
        LDR     pTwiddle, [pFFTSpec, #ARMsFFTSpec_pTwiddle]
        LDR     pOut, [pFFTSpec, #ARMsFFTSpec_pBuf]
        
        @//  N=1 Treat seperately  
        CMP     N,#1
        BGT     sizeGreaterThanOne
        VLD1    dX0[0],[pSrc]
        RSB     scale,scale,#0                        @// to use VRSHL for right shift by a variable
        MOV     zero,#0
        VMOV    dShift[0],scale
        VMOV    dzero[0],zero
        VRSHL   dX0,dShift
        VMOV    dZero[0],zero
        VST3    {dX0[0],dzero[0],dZero[0]},[pDst]
                
        B       End
        
                
        
sizeGreaterThanOne:
        @// Do a N/2 point complex FFT including the scaling
              
        MOV     N,N,ASR #1                          @// N/2 point complex FFT
                               
        CLZ     order,N                             @// N = 2^order 
        RSB     order,order,#31     
        MOV     subFFTSize,#1
        @//MOV     subFFTNum,N
        
        CMP     order,#3
        BGT     orderGreaterthan3                   @// order > 3
                
        CMP     order,#1
        BGE     orderGreaterthan0                   @// order > 0
        M_STR   scale, diffOnStack,LT               @// order = 0
        VLD1    dX0,[pSrc]
        VST1    dX0,[pOut]
        MOV     pSrc,pOut
        MOV     argDst,pDst
        BLT     FFTEnd
        
orderGreaterthan0:
        @// set the buffers appropriately for various orders
        CMP     order,#2
        MOVEQ   argDst,pDst        
        MOVNE   argDst,pOut
        MOVNE   pOut,pDst                           @// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle
        
        SUBS     diff,scale,order
        M_STR   diff,diffOnStack
        MOVGT   scale,order
        @// Now scale <= order
        
        CMP     order,#1
        BGT     orderGreaterthan1
        SUBS    scale,scale,#1
        BLEQ    armSP_FFTFwd_CToC_SC32_Sfs_Radix2_fs_OutOfPlace_unsafe  @// order = 1
        BLLT    armSP_FFTFwd_CToC_SC32_Radix2_fs_OutOfPlace_unsafe      @// order = 1
        B       FFTEnd

orderGreaterthan1:
        CMP     order,#2
        MOV     argScale,scale
        BGT     orderGreaterthan2
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC32_Sfs_Radix2_fs_OutOfPlace_unsafe      @// order =2          
        BLLT    armSP_FFTFwd_CToC_SC32_Radix2_fs_OutOfPlace_unsafe  
        SUBS    argScale,argScale,#1
        BLEQ    armSP_FFTFwd_CToC_SC32_Sfs_Radix2_ls_OutOfPlace_unsafe  
        BLLT    armSP_FFTFwd_CToC_SC32_Radix2_ls_OutOfPlace_unsafe  
        B       FFTEnd
        
orderGreaterthan2:@// order =3        
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC32_Sfs_Radix2_fs_OutOfPlace_unsafe      
        BLLT    armSP_FFTFwd_CToC_SC32_Radix2_fs_OutOfPlace_unsafe  
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC32_Sfs_Radix2_OutOfPlace_unsafe  
        BLLT    armSP_FFTFwd_CToC_SC32_Radix2_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1
        BLEQ    armSP_FFTFwd_CToC_SC32_Sfs_Radix2_ls_OutOfPlace_unsafe      
        BLLT    armSP_FFTFwd_CToC_SC32_Radix2_ls_OutOfPlace_unsafe    
        B       FFTEnd

        

orderGreaterthan3:
        @// check scale = 0 or scale = order
        SUBS    diff, scale, order                 @// scale > order 
        MOVGT   scale,order     
        BGE     specialScaleCase                   @// scale = 0 or scale = order 
        CMP     scale,#0
        BEQ     specialScaleCase
        B       generalScaleCase
        
specialScaleCase:@//  scale = 0 or scale = order  and order >= 2     
        
        TST     order, #2                           @// Set input args to fft stages
        MOVEQ   argDst,pDst        
        MOVNE   argDst,pOut
        MOVNE   pOut,pDst                           @// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle  

        CMP      diff,#0
        M_STR    diff, diffOnStack
        BGE      scaleEqualsOrder  
       
        @//check for even or odd order
        @// NOTE: The following combination of BL's would work fine eventhough the first
        @// BL would corrupt the flags. This is because the end of the "grpZeroSetLoop" loop inside 
        @// armSP_FFTFwd_CToC_SC32_Radix4_fs_OutOfPlace_unsafe sets the Z flag to EQ
        
        TST     order,#0x00000001
        BLEQ    armSP_FFTFwd_CToC_SC32_Radix4_fs_OutOfPlace_unsafe 
        BLNE    armSP_FFTFwd_CToC_SC32_Radix8_fs_OutOfPlace_unsafe
        
        CMP        subFFTNum,#4
        BLT     FFTEnd
         

unscaledRadix4Loop:
        BEQ        lastStageUnscaledRadix4
         BL        armSP_FFTFwd_CToC_SC32_Radix4_OutOfPlace_unsafe
         CMP        subFFTNum,#4
         B        unscaledRadix4Loop
         
lastStageUnscaledRadix4:
        BL      armSP_FFTFwd_CToC_SC32_Radix4_ls_OutOfPlace_unsafe 
        B        FFTEnd        
         

scaleEqualsOrder:
        @//check for even or odd order
        @// NOTE: The following combination of BL's would work fine eventhough the first
        @// BL would corrupt the flags. This is because the end of the "grpZeroSetLoop" loop inside 
        @// armSP_FFTFwd_CToC_SC32_Radix4_fs_OutOfPlace_unsafe sets the Z flag to EQ
                
        TST     order,#0x00000001
        BLEQ    armSP_FFTFwd_CToC_SC32_Sfs_Radix4_fs_OutOfPlace_unsafe 
        BLNE    armSP_FFTFwd_CToC_SC32_Sfs_Radix8_fs_OutOfPlace_unsafe 
        
        CMP        subFFTNum,#4
        BLT     FFTEnd
        

scaledRadix4Loop:
        BEQ        lastStageScaledRadix4
         BL        armSP_FFTFwd_CToC_SC32_Sfs_Radix4_OutOfPlace_unsafe
         CMP        subFFTNum,#4
         B        scaledRadix4Loop         
         
lastStageScaledRadix4:
        BL      armSP_FFTFwd_CToC_SC32_Sfs_Radix4_ls_OutOfPlace_unsafe 
        B        FFTEnd        
        
generalScaleCase:@// 0 < scale < order and order >= 2
        @// Determine the correct destination buffer
        SUB     diff,order,scale
        TST     diff,#0x01
        ADDEQ   count, scale,diff,lsr #1         @// count = scale + (order - scale)/2
        MOVNE   count, order
        TST     count, #0x01                     @// Is count even or odd ?
        
        MOVEQ   argDst,pDst                     @// Set input args to fft stages
        MOVNE   argDst,pOut
        MOVNE   pOut,pDst                       @// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle  

        M_STR   diff, diffOnStack    
        
        MOV     argScale,scale                  @// Put scale in RN4 so as to save and restore
        BL      armSP_FFTFwd_CToC_SC32_Sfs_Radix2_fs_OutOfPlace_unsafe     @// scaled first stage
        SUBS    argScale,argScale,#1
        
scaledRadix2Loop:
        BLGT    armSP_FFTFwd_CToC_SC32_Sfs_Radix2_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1            @// save and restore scale (RN4) in the scaled stages
        BGT     scaledRadix2Loop
        
        
        M_LDR   diff, diffOnStack  
        @//check for even or odd order
        TST     diff,#0x00000001
        BEQ     generalUnscaledRadix4Loop
        B       unscaledRadix2Loop

generalUnscaledRadix4Loop:
        CMP        subFFTNum,#4
         BEQ        generalLastStageUnscaledRadix4
         BL        armSP_FFTFwd_CToC_SC32_Radix4_OutOfPlace_unsafe
         B        generalUnscaledRadix4Loop 
         
generalLastStageUnscaledRadix4:
        BL      armSP_FFTFwd_CToC_SC32_Radix4_ls_OutOfPlace_unsafe 
        B        finalComplexToRealFixup             
             

unscaledRadix2Loop:
        CMP        subFFTNum,#2
         BEQ        generalLastStageUnscaledRadix2
         BL        armSP_FFTFwd_CToC_SC32_Radix2_OutOfPlace_unsafe
         B        unscaledRadix2Loop        

generalLastStageUnscaledRadix2:
        BL      armSP_FFTFwd_CToC_SC32_Radix2_ls_OutOfPlace_unsafe 
        B        finalComplexToRealFixup             

       
FFTEnd:@// Does only the scaling
        
        M_LDR   diff, diffOnStack  
        CMP     diff,#0
        BLE     finalComplexToRealFixup
        
        RSB     diff,diff,#0                        @// to use VRSHL for right shift by a variable
        VDUP    dShift,diff
        
        @// save subFFTSize and use tmpsubfftsize in the folowwing loop
        MOV    subFFTSizeTmp,subFFTSize                 @// subFFTSizeTmp same reg as subFFTNum      
        
scaleFFTData:@// N = subFFTSize  ; dataptr = pDst  ; scale = diff
        VLD1    {dX0},[pSrc]            @// pSrc contains pDst pointer
        SUBS    subFFTSizeTmp,subFFTSizeTmp,#1
        VRSHL   dX0,dShift
        VST1    {dX0},[pSrc]!
                
        BGT     scaleFFTData
        
        SUB     pSrc,pSrc,subFFTSize,LSL #3             @// reset pSrc for final fixup
        
        @//  change the logic so that output after scaling is in pOut and not in pDst
        @//  finally store from pOut to pDst
        @//  change branch "End" to branch "finalComplexToRealFixup" in the above
        @//  chk the code below for multiplication by j factor

finalComplexToRealFixup:
        
               
        @// F(0) = 1/2[Z(0) + Z'(0)] - j [Z(0) - Z'(0)]
        @// 1/2[(a+jb) + (a-jb)] - j  [(a+jb) - (a-jb)]
        @// 1/2[2a+j0] - j [0+j2b]
        @// (a+b, 0)
        
        @// F(N/2) = 1/2[Z(0) + Z'(0)] + j [Z(0) - Z'(0)]
        @// 1/2[(a+jb) + (a-jb)] + j  [(a+jb) - (a-jb)]
        @// 1/2[2a+j0] + j [0+j2b]
        @// (a-b, 0)
       
        @// F(0) and F(N/2)
        VLD2    {dX0r[0],dX0i[0]},[pSrc]!
        MOV     zero,#0
        VMOV    dX0r[1],zero
        MOV     step,subFFTSize,LSL #3                  @// step = N/2 * 8 bytes
        VMOV    dX0i[1],zero
        SUB     twStep,step,subFFTSize,LSL #1           @// twStep = 3N/8 * 8 bytes pointing to W^1
        
        VADD    dY0r,dX0r,dX0i                          @// F(0) = ((Z0.r+Z0.i) , 0)
        MOV     step1,subFFTSize,LSL #2                 @// step1 = N/2 * 4 bytes
        VSUB    dY0i,dX0r,dX0i                            @// F(N/2) = ((Z0.r-Z0.i) , 0)
        SUBS    subFFTSize,subFFTSize,#2
        
        VST1    dY0r,[argDst],step
        ADD     pTwiddleTmp,argTwiddle,#8                @// W^2
        VST1    dY0i,[argDst]!
        ADD     argTwiddle,argTwiddle,twStep             @// W^1 
        
        VDUP    dzero,zero
        SUB     argDst,argDst,step
        
        BLT     End
        BEQ     lastElement
        SUB     step,step,#24
        SUB     step1,step1,#8                         @// (N/4-1)*8 bytes
        
        @// F(k) = 1/2[Z(k) +  Z'(N/2-k)] -j*W^(k) [Z(k) -  Z'(N/2-k)]
        @// Note: W^k is stored as negative values in the table
        @// Process 4 elements at a time. E.g: F(1),F(2) and F(N/2-2),F(N/2-1) since both of them
        @// require Z(1),Z(2) and Z(N/2-2),Z(N/2-1)
        
        
evenOddButterflyLoop:
        
        
        VLD1    dW0r,[argTwiddle],step1
        VLD1    dW1r,[argTwiddle]!
        
        VLD2    {dX0r,dX0i},[pSrc],step
        SUB     argTwiddle,argTwiddle,step1
        VLD2    {dX1r,dX1i},[pSrc]!
        
        
        
        SUB     step1,step1,#8                          @// (N/4-2)*8 bytes
        VLD1    dW0i,[pTwiddleTmp],step1
        VLD1    dW1i,[pTwiddleTmp]!
        SUB     pSrc,pSrc,step
        
        SUB     pTwiddleTmp,pTwiddleTmp,step1
        VREV64  dX1r,dX1r
        VREV64  dX1i,dX1i
        SUBS    subFFTSize,subFFTSize,#4
        
                
        
        VSUB    dT2,dX0r,dX1r                            @// a-c
        SUB     step1,step1,#8
        VADD    dT3,dX0i,dX1i                            @// b+d
        VADD    dT0,dX0r,dX1r                           @// a+c
        VSUB    dT1,dX0i,dX1i                            @// b-d
        VHADD   dT0,dT0,dzero
        VHADD   dT1,dT1,dzero
        
        VZIP    dW1r,dW1i
        vzip    dW0r,dW0i
        
                                
        VMULL   qT0,dW1r,dT2
        VMLAL   qT0,dW1i,dT3
        VMULL   qT1,dW1r,dT3
        VMLSL   qT1,dW1i,dT2
                    
        VMULL   qT2,dW0r,dT2
        VMLSL   qT2,dW0i,dT3
        VMULL   qT3,dW0r,dT3
        VMLAL   qT3,dW0i,dT2
        
                
        VRSHRN  dX1r,qT0,#32
        VRSHRN  dX1i,qT1,#32
        
        VSUB    dY1r,dT0,dX1i                           @// F(N/2 -1)
        VADD    dY1i,dT1,dX1r
        VNEG    dY1i,dY1i
        
        VREV64  dY1r,dY1r
        VREV64  dY1i,dY1i
        
                            
        VRSHRN  dX0r,qT2,#32
        VRSHRN  dX0i,qT3,#32
        
        
        VSUB    dY0r,dT0,dX0i                           @// F(1)
        VADD    dY0i,dT1,dX0r
        
        
        VST2    {dY0r,dY0i},[argDst],step
        VST2    {dY1r,dY1i},[argDst]!
        SUB     argDst,argDst,step
        SUB     step,step,#32                            @// (N/2-4)*8 bytes
        
        
        BGT     evenOddButterflyLoop
        
        SUB     pSrc,pSrc,#8                @// set both the ptrs to the last element
        SUB     argDst,argDst,#8
        
        
                                       
        @// Last element can be expanded as follows
        @// 1/2[Z(k) + Z'(k)] + j w^k [Z(k) - Z'(k)]
        @// 1/2[(a+jb) + (a-jb)] + j w^k [(a+jb) - (a-jb)]
        @// 1/2[2a+j0] + j (c+jd) [0+j2b]
        @// (a-bc, -bd)
        @// Since (c,d) = (0,1) for the last element, result is just (a,-b)
        
lastElement:
        VLD1    dX0r,[pSrc]
        
        VST1    dX0r[0],[argDst]!
        VNEG    dX0r,dX0r
        VST1    dX0r[1],[argDst]!
        
        
        
        
                
                       
End:
        @// Set return value
        MOV     result, #OMX_Sts_NoErr       

        @// Write function tail
        M_END

        .end
        
