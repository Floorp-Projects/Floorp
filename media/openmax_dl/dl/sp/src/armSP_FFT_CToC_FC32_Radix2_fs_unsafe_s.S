@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This is a modification of armSP_FFT_CToC_SC32_Radix2_fs_unsafe_s.S
@//  to support float instead of SC32.
@//

@//
@// Description:
@// Compute the first stage of a Radix 2 DIT in-order out-of-place FFT
@// stage for a N point complex signal.
@//
@//


@// Include standard headers

#include "dl/api/armCOMM_s.h"
#include "dl/api/omxtypes_s.h"


@// Import symbols required from other files
@// (For example tables)




@// Set debugging level
@//DEBUG_ON    SETL {TRUE}



@// Guarding implementation by the processor name



@// Guarding implementation by the processor name


@//Input Registers

#define pSrc            r0
#define pDst            r2
#define pTwiddle        r1
#define pPingPongBuf    r5
#define subFFTNum       r6
#define subFFTSize      r7


@//Output Registers


@//Local Scratch Registers

#define pointStep       r3
#define outPointStep    r3
#define grpSize         r4
#define setCount        r4
#define step            r8
#define dstStep         r8

@// Neon Registers

#define dX0     D0
#define dX1     D1
#define dY0     D2
#define dY1     D3


        .MACRO FFTSTAGE scaled, inverse, name

        @// Define stack arguments


        @// update subFFTSize and subFFTNum into RN6 and RN7 for the next stage


        MOV        subFFTSize,#2
        LSR        grpSize,subFFTNum,#1
        MOV        subFFTNum,grpSize


        @// pT0+1 increments pT0 by 8 bytes
        @// pT0+pointStep = increment of 8*pointStep bytes = 4*grpSize bytes
        @// Note: outPointStep = pointStep for firststage
        @// Note: setCount = grpSize/2 (reuse the updated grpSize for setCount)

        MOV        pointStep,grpSize,LSL #3
        RSB        step,pointStep,#8


        @// Loop on the sets for grp zero

grpZeroSetLoop\name :

        VLD1.F32    dX0,[pSrc],pointStep
        VLD1.F32    dX1,[pSrc],step                   @// step = -pointStep + 8
        SUBS    setCount,setCount,#1

        VADD.F32    dY0,dX0,dX1
        VSUB.F32    dY1,dX0,dX1

        VST1.F32    dY0,[pDst],outPointStep
        @// dstStep =  step = -pointStep + 8
        VST1.F32    dY1,[pDst],dstStep

        BGT     grpZeroSetLoop\name


        @// reset pSrc to pDst for the next stage
        SUB     pSrc,pDst,pointStep                     @// pDst -= 2*grpSize
        MOV     pDst,pPingPongBuf

        .endm



        M_START armSP_FFTFwd_CToC_FC32_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",fwd
        M_END



        M_START armSP_FFTInv_CToC_FC32_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",inv
        M_END

	.end
