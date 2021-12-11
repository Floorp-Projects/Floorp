@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Copyright 2016, Mozilla Foundation and contributors
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This is a modification of omxSP_FFTInv_CCSToR_S32_Sfs_s.s
@//  to support float instead of SC32.
@//
@//  It is further modified to produce an "unscaled" version, which
@//  actually multiplies by two for consistency with the other FFT functions
@//  in use.
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

        .extern  armSP_FFTInv_CToC_FC32_Radix2_fs_OutOfPlace_unsafe
        .extern  armSP_FFTInv_CToC_FC32_Radix4_fs_OutOfPlace_unsafe
        .extern  armSP_FFTInv_CToC_FC32_Radix8_fs_OutOfPlace_unsafe
        .extern  armSP_FFTInv_CToC_FC32_Radix4_OutOfPlace_unsafe
        .extern  armSP_FFTInv_CToC_FC32_Radix2_OutOfPlace_unsafe
        .extern  armSP_FFTInv_CCSToR_F32_preTwiddleRadix2_unsafe


@// Set debugging level
@//DEBUG_ON    SETL {TRUE}



@// Guarding implementation by the processor name



      @// Guarding implementation by the processor name

@// Import symbols required from other files
@// (For example tables)
        .extern  armSP_FFTInv_CToC_FC32_Radix4_ls_OutOfPlace_unsafe
        .extern  armSP_FFTInv_CToC_FC32_Radix2_ls_OutOfPlace_unsafe


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
#define round           r3

#define pOut1           r2
#define size            r7
#define step            r8
#define step1           r9
#define twStep          r10
#define pTwiddleTmp     r11
#define argTwiddle1     r12
#define zero            r14

@// Neon registers

#define dX0     D0.F32
#define dShift  D1.F32
#define dX1     D1.F32
#define dY0     D2.F32
#define dY1     D3.F32
#define dX0r    D0.F32
#define dX0i    D1.F32
#define dX1r    D2.F32
#define dX1i    D3.F32
#define dW0r    D4.F32
#define dW0i    D5.F32
#define dW1r    D6.F32
#define dW1i    D7.F32
#define dT0     D8.F32
#define dT1     D9.F32
#define dT2     D10.F32
#define dT3     D11.F32
#define qT0     d12.F32
#define qT1     d14.F32
#define qT2     d16.F32
#define qT3     d18.F32
#define dY0r    D4.F32
#define dY0i    D5.F32
#define dY1r    D6.F32
#define dY1i    D7.F32
#define dzero   D20.F32

#define dY2     D4.F32
#define dY3     D5.F32
#define dW0     D6.F32
#define dW1     D7.F32
#define dW0Tmp  D10.F32
#define dW1Neg  D11.F32

#define sN      S0.S32
#define fN      S1.F32
@// two must be the same as dScale[0]!
#define dScale  D2.F32
#define two S4.F32


    @// Allocate stack memory required by the function
        M_ALLOC4        complexFFTSize, 4

    @// Write function header
        M_START     omxSP_FFTInv_CCSToR_F32_Sfs_unscaled,r11,d15

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
        VST1    dX0[0],[pDst]

        B       End

sizeGreaterThanOne:

        @// Call the preTwiddle Radix2 stage before doing the compledIFFT


        BL    armSP_FFTInv_CCSToR_F32_preTwiddleRadix2_unsafe


complexIFFT:

        ASR     N,N,#1                             @// N/2 point complex IFFT
        M_STR   N, complexFFTSize                  @ Save N for scaling later
        ADD     pSrc,pOut,N,LSL #3                 @// set pSrc as pOut1

        CLZ     order,N                             @// N = 2^order
        RSB     order,order,#31
        MOV     subFFTSize,#1
        @//MOV     subFFTNum,N

        CMP     order,#3
        BGT     orderGreaterthan3                   @// order > 3

        CMP     order,#1
        BGE     orderGreaterthan0                   @// order > 0

        VLD1    dX0,[pSrc]
        VST1    dX0,[pDst]
        MOV     pSrc,pDst
        BLT     FFTEnd

orderGreaterthan0:
        @// set the buffers appropriately for various orders
        CMP     order,#2
        MOVNE   argDst,pDst
        MOVEQ   argDst,pOut
        @// Pass the first stage destination in RN5
        MOVEQ   pOut,pDst
        MOV     argTwiddle,pTwiddle

        BGE     orderGreaterthan1
        BLLT    armSP_FFTInv_CToC_FC32_Radix2_fs_OutOfPlace_unsafe  @// order = 1
        B       FFTEnd

orderGreaterthan1:
        MOV     tmpOrder,order                          @// tmpOrder = RN 4
        BL      armSP_FFTInv_CToC_FC32_Radix2_fs_OutOfPlace_unsafe
        CMP     tmpOrder,#2
        BLGT    armSP_FFTInv_CToC_FC32_Radix2_OutOfPlace_unsafe
        BL      armSP_FFTInv_CToC_FC32_Radix2_ls_OutOfPlace_unsafe
        B       FFTEnd


orderGreaterthan3:
specialScaleCase:

        @// Set input args to fft stages
        TST     order, #2
        MOVNE   argDst,pDst
        MOVEQ   argDst,pOut
        @// Pass the first stage destination in RN5
        MOVEQ   pOut,pDst
        MOV     argTwiddle,pTwiddle

        @//check for even or odd order
        @// NOTE: The following combination of BL's would work fine even though
        @// the first BL would corrupt the flags. This is because the end of
        @// the "grpZeroSetLoop" loop inside
        @// armSP_FFTInv_CToC_FC32_Radix4_fs_OutOfPlace_unsafe sets the Z flag
        @// to EQ

        TST     order,#0x00000001
        BLEQ    armSP_FFTInv_CToC_FC32_Radix4_fs_OutOfPlace_unsafe
        BLNE    armSP_FFTInv_CToC_FC32_Radix8_fs_OutOfPlace_unsafe

        CMP        subFFTNum,#4
        BLT     FFTEnd


unscaledRadix4Loop:
        BEQ        lastStageUnscaledRadix4
         BL        armSP_FFTInv_CToC_FC32_Radix4_OutOfPlace_unsafe
         CMP        subFFTNum,#4
         B        unscaledRadix4Loop

lastStageUnscaledRadix4:
        BL      armSP_FFTInv_CToC_FC32_Radix4_ls_OutOfPlace_unsafe
        B        FFTEnd

FFTEnd:                                               @// Does only the scaling
        @ Scale inverse FFT result by 2 for consistency with other FFTs
        VMOV    two, 2.0                   @ two = dScale[0]

        @// N = subFFTSize  ; dataptr = pDst
scaleFFTData:
        VLD1    {dX0},[pSrc]            @// pSrc contains pDst pointer
        SUBS    subFFTSize,subFFTSize,#1
        VMUL    dX0, dX0, dScale[0]
        VST1    {dX0},[pSrc]!

        BGT     scaleFFTData


End:
        @// Set return value
        MOV     result, #OMX_Sts_NoErr

        @// Write function tail
        M_END



        .end
