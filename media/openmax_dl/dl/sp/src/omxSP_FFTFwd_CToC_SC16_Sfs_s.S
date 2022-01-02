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
@// File Name:  omxSP_FFTFwd_CToC_SC16_Sfs_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   6729
@// Last Modified Date:       Tue, 17 Jul 2007
@//
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@//
@//
@//
@// Description:
@// Compute an inverse FFT for a complex signal
@//
@//


@// Include standard headers

#include "dl/api/armCOMM_s.h"
#include "dl/api/omxtypes_s.h"


@// Import symbols required from other files
@// (For example tables)

        .extern  armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Radix4_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Radix4_ls_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Radix8_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Radix4_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Sfs_Radix4_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Sfs_Radix4_ls_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Sfs_Radix8_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Sfs_Radix4_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Radix2_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ls_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe

@// Set debugging level
@//DEBUG_ON    SETL {TRUE}



@// Guarding implementation by the processor name



@// Guarding implementation by the processor name


    .extern  armSP_FFTFwd_CToC_SC16_Radix2_ps_OutOfPlace_unsafe
    .extern  armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe

@//Input Registers

#define pSrc            r0
#define pDst            r1
#define pFFTSpec                r2
#define scale           r3


@// Output registers
#define result          r0

@//Local Scratch Registers
#define argTwiddle              r1
#define argDst          r2
#define argScale                r4
#define pTwiddle                r4
#define tmpOrder                r4
#define pOut            r5
#define subFFTSize              r7
#define subFFTNum               r6
#define N               r6
#define order           r14
#define diff            r9
@// Total num of radix stages required to comple the FFT
#define count           r8
#define x0r             r4
#define x0i             r5
#define diffMinusOne            r2
#define round           r3

@// Neon registers

#define dX0     D0.S16
#define dShift  D1.S16
#define dX0S32  D0.S32



    @// Allocate stack memory required by the function
        M_ALLOC4        diffOnStack, 4

    @// Write function header
        M_START     omxSP_FFTFwd_CToC_SC16_Sfs,r11,d15

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

        CLZ     order,N                             @// N = 2^order
        RSB     order,order,#31
        MOV     subFFTSize,#1
        @//MOV     subFFTNum,N

        CMP     order,#3
        BGT     orderGreaterthan3                   @// order > 3

        CMP     order,#1
        BGE     orderGreaterthan0                   @// order > 0
        M_STR   scale, diffOnStack,LT               @// order = 0
        LDRLT   x0r,[pSrc]
        STRLT   x0r,[pDst]
        MOVLT   pSrc,pDst
        BLT     FFTEnd

orderGreaterthan0:
        @// set the buffers appropriately for various orders
        CMP     order,#2
        MOVNE   argDst,pDst
        MOVEQ   argDst,pOut
        MOVEQ   pOut,pDst                           @// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle

        SUBS     diff,scale,order
        M_STR   diff,diffOnStack
        MOVGT   scale,order
        @// Now scale <= order

        CMP     order,#1
        BGT     orderGreaterthan1
        SUBS    scale,scale,#1
        BLEQ    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe  @// order = 1
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe      @// order = 1
        B       FFTEnd

orderGreaterthan1:
        CMP     order,#2
        MOV     argScale,scale
        BGT     orderGreaterthan2
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe      @// order =2
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1
        BLEQ    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ls_OutOfPlace_unsafe
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe
        B       FFTEnd

orderGreaterthan2:                                                                     @// order =3
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_ps_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1
        BLEQ    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ls_OutOfPlace_unsafe
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe
        B       FFTEnd


orderGreaterthan3:
        @// check scale = 0 or scale = order
        SUBS    diff, scale, order                 @// scale > order
        MOVGT   scale,order
        BGE     specialScaleCase                   @// scale = 0 or scale = order
        CMP     scale,#0
        BEQ     specialScaleCase
        B       generalScaleCase

specialScaleCase:                                           @//  scale = 0 or scale = order  and order > 3

        TST     order, #2                           @// Set input args to fft stages
        MOVNE   argDst,pDst
        MOVEQ   argDst,pOut
        MOVEQ   pOut,pDst                           @// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle

        CMP      diff,#0
        M_STR    diff, diffOnStack
        BGE      scaleEqualsOrder

        @//check for even or odd order
        @// NOTE: The following combination of BL's would work fine eventhough the first
        @// BL would corrupt the flags. This is because the end of the "grpZeroSetLoop" loop inside
        @// armSP_FFTFwd_CToC_SC16_Radix4_fs_OutOfPlace_unsafe sets the Z flag to EQ

        TST     order,#0x00000001
        BLEQ    armSP_FFTFwd_CToC_SC16_Radix4_fs_OutOfPlace_unsafe
        BLNE    armSP_FFTFwd_CToC_SC16_Radix8_fs_OutOfPlace_unsafe

        CMP        subFFTNum,#4
        BLT     FFTEnd

unscaledRadix4Loop:
        BEQ        lastStageUnscaledRadix4
        BL        armSP_FFTFwd_CToC_SC16_Radix4_OutOfPlace_unsafe
         CMP        subFFTNum,#4
         B        unscaledRadix4Loop

lastStageUnscaledRadix4:
        BL      armSP_FFTFwd_CToC_SC16_Radix4_ls_OutOfPlace_unsafe
        B        FFTEnd

scaleEqualsOrder:
        @//check for even or odd order
        @// NOTE: The following combination of BL's would work fine eventhough the first
        @// BL would corrupt the flags. This is because the end of the "grpZeroSetLoop" loop inside
        @// armSP_FFTFwd_CToC_SC32_Radix4_fs_OutOfPlace_unsafe sets the Z flag to EQ

        TST     order,#0x00000001
        BLEQ    armSP_FFTFwd_CToC_SC16_Sfs_Radix4_fs_OutOfPlace_unsafe
        BLNE    armSP_FFTFwd_CToC_SC16_Sfs_Radix8_fs_OutOfPlace_unsafe

        CMP        subFFTNum,#4
        BLT     FFTEnd

scaledRadix4Loop:
        BEQ        lastStageScaledRadix4
        BL        armSP_FFTFwd_CToC_SC16_Sfs_Radix4_OutOfPlace_unsafe
         CMP        subFFTNum,#4
         B        scaledRadix4Loop

lastStageScaledRadix4:
        BL      armSP_FFTFwd_CToC_SC16_Sfs_Radix4_ls_OutOfPlace_unsafe
        B        FFTEnd



generalScaleCase:                                               @// 0 < scale < order and order > 3
        @// Determine the correct destination buffer
        SUB     diff,order,scale
        TST     diff,#0x01
        ADDEQ   count,scale,diff,LSR #1         @// count = scale + (order - scale)/2
        MOVNE   count,order
        TST     count,#0x01                     @// Is count even or odd ?

        MOVNE   argDst,pDst                     @// Set input args to fft stages
        MOVEQ   argDst,pOut
        MOVEQ   pOut,pDst                       @// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle

        CMP     diff,#1
        M_STR   diff, diffOnStack
        BEQ     scaleps                         @// scaling including a radix2_ps stage

        MOV     argScale,scale                  @// Put scale in RN4 so as to save and restore
        BL      armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe     @// scaled first stage
        SUBS    argScale,argScale,#1

scaledRadix2Loop:
        BLGT    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1            @// save and restore scale (RN4) in the scaled stages
        BGT     scaledRadix2Loop
        B       outScale

scaleps:
        SUB     argScale,scale,#1                   @// order>3 and diff=1 => scale >= 3
        BL      armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe     @// scaled first stage
        SUBS    argScale,argScale,#1

scaledRadix2psLoop:
        BEQ     scaledRadix2psStage
        BLGT    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1            @// save and restore scale (RN4) in the scaled stages
        BGE     scaledRadix2psLoop

scaledRadix2psStage:
        BL      armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe
        B       generalLastStageUnscaledRadix2


outScale:
        M_LDR   diff, diffOnStack
        @//check for even or odd order
        TST     diff,#0x00000001
        BEQ     generalUnscaledRadix4Loop
        B       unscaledRadix2Loop

generalUnscaledRadix4Loop:
        CMP        subFFTNum,#4
         BEQ        generalLastStageUnscaledRadix4
         BL        armSP_FFTFwd_CToC_SC16_Radix4_OutOfPlace_unsafe
         B        generalUnscaledRadix4Loop

generalLastStageUnscaledRadix4:
        BL      armSP_FFTFwd_CToC_SC16_Radix4_ls_OutOfPlace_unsafe
        B        End

unscaledRadix2Loop:
        CMP        subFFTNum,#4
         BEQ        generalLastTwoStagesUnscaledRadix2
         BL        armSP_FFTFwd_CToC_SC16_Radix2_OutOfPlace_unsafe
         B        unscaledRadix2Loop

generalLastTwoStagesUnscaledRadix2:
        BL      armSP_FFTFwd_CToC_SC16_Radix2_ps_OutOfPlace_unsafe
generalLastStageUnscaledRadix2:
        BL      armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe
        B        End


FFTEnd:                                               @// Does only the scaling

        M_LDR   diff, diffOnStack
        CMP     diff,#0
        BLE     End

        RSB     diff,diff,#0                        @// to use VRSHL for right shift by a variable
        VDUP    dShift,diff

scaleFFTData:                                           @// N = subFFTSize  ; dataptr = pDst  ; scale = diff
        VLD1    {dX0S32[0]},[pSrc]                        @// pSrc contains pDst pointer
        SUBS    subFFTSize,subFFTSize,#1
        VRSHL   dX0,dShift
        VST1    {dX0S32[0]},[pSrc]!

        BGT     scaleFFTData



End:
        @// Set return value
        MOV     result, #OMX_Sts_NoErr

        @// Write function tail
        M_END

    .END
