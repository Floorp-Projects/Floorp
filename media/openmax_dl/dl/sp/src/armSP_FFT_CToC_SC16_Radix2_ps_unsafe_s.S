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
@// File Name:  armSP_FFT_CToC_SC16_Radix2_ps_unsafe_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   6740
@// Last Modified Date:       Wed, 18 Jul 2007
@//
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@//
@//
@//
@// Description:
@// Compute a Radix 2 FFT stage for a N point complex signal
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


@//Input Registers

#define pSrc                            r0
#define pDst                            r2
#define pTwiddle                        r1
#define subFFTNum                       r6
#define subFFTSize                      r7


@//Output Registers


@//Local Scratch Registers

#define outPointStep                    r3
#define grpCount                        r4
#define dstStep                         r5
#define twStep                          r8
#define pTmp                            r4

@// Neon Registers

#define dW1S32                          D0.S32
#define dW2S32                          D1.S32
#define dW1                             D0.S16
#define dW2                             D1.S16

#define dX0                             D2.S16
#define dX1                             D3.S16
#define dX2                             D4.S16
#define dX3                             D5.S16
#define dY0                             D6.S16
#define dY1                             D7.S16
#define dY2                             D8.S16
#define dY3                             D9.S16
#define qT0                             Q5.S32
#define qT1                             Q6.S32


        .MACRO FFTSTAGE scaled, inverse, name

        @// Define stack arguments


        @// Update grpCount and grpSize rightaway inorder to reuse pGrpCount and pGrpSize regs


        LSL     grpCount,subFFTSize,#1


        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount

        @// pOut0+1 increments pOut0 by 8 bytes
        @// pOut0+outPointStep == increment of 4*outPointStep bytes = 2*size bytes
        SMULBB  outPointStep,grpCount,subFFTNum
        MOV     twStep,subFFTNum,LSL #1
        LSR     subFFTNum,subFFTNum,#1                      @//grpSize


        RSB      dstStep,outPointStep,#8


        @// Note: pointStep is 8 in this case: so need of extra reg
        @// Loop on the groups: 2 groups at a time

grpLoop\name:

        VLD1     dW1S32[],[pTwiddle],twStep                @//[wi | wr]
        VLD1     dW2S32[],[pTwiddle],twStep

        @// Process the sets for each grp:  2 sets at a time (no set looping required)

        VLD1    dX0,[pSrc]!            @// point0: of set0,set1 of grp0
        VLD1    dX1,[pSrc]!            @// point1: of set0,set1 of grp0
        VLD1    dX2,[pSrc]!            @// point0: of set0,set1 of grp1
        VLD1    dX3,[pSrc]!            @// point1: of set0,set1 of grp1

        SUBS    grpCount,grpCount,#4              @// decrement the loop counter
        VUZP    dW1,dW2
        VUZP    dX1,dX3

        .ifeqs  "\inverse", "TRUE"
            VMULL   qT0,dX1,dW1
            VMLAL   qT0,dX3,dW2                       @// real part
            VMULL   qT1,dX3,dW1
            VMLSL   qT1,dX1,dW2                       @// imag part

        .ELSE
            VMULL   qT0,dX1,dW1
            VMLSL   qT0,dX3,dW2                       @// real part
            VMULL   qT1,dX3,dW1
            VMLAL   qT1,dX1,dW2                       @// imag part

        .ENDIF

        VRSHRN  dX1,qT0,#15
        VRSHRN  dX3,qT1,#15

        VZIP    dX1,dX3


        .ifeqs "\scaled", "TRUE"

            VHSUB    dY0,dX0,dX1
            VHADD    dY1,dX0,dX1
            VHSUB    dY2,dX2,dX3
            VHADD    dY3,dX2,dX3

        .ELSE

            VSUB    dY0,dX0,dX1
            VADD    dY1,dX0,dX1
            VSUB    dY2,dX2,dX3
            VADD    dY3,dX2,dX3



        .ENDIF

        VST1    dY0,[pDst],outPointStep             @// point0: of set0,set1 of grp0
        VST1    dY1,[pDst],dstStep                  @// dstStep = -outPointStep + 8
        VST1    dY2,[pDst],outPointStep             @// point0: of set0,set1 of grp1
        VST1    dY3,[pDst],dstStep                  @// point1: of set0,set1 of grp1


        BGT     grpLoop\name


        @// Reset and Swap pSrc and pDst for the next stage
        MOV     pTmp,pDst
        SUB     pDst,pSrc,outPointStep,LSL #1       @// pDst -= 2*size; pSrc -= 4*size bytes
        SUB     pSrc,pTmp,outPointStep

        @// Reset pTwiddle for the next stage
        SUB     pTwiddle,pTwiddle,outPointStep      @// pTwiddle -= 2*size bytes

        .endm



        M_START armSP_FFTFwd_CToC_SC16_Radix2_ps_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",FWD
        M_END



        M_START armSP_FFTInv_CToC_SC16_Radix2_ps_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",INV
        M_END



        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","FALSE",FWDSFS
        M_END



        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","TRUE",INVSFS
        M_END



    .END
