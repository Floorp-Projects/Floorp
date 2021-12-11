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
@// File Name:  armSP_FFT_CToC_SC16_Radix2_fs_unsafe_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   6693
@// Last Modified Date:       Tue, 10 Jul 2007
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



@// Guarding implementation by the processor name


@//Input Registers

#define pSrc                            r0
#define pDst                            r2
#define pTwiddle                        r1
#define pPingPongBuf                    r5
#define subFFTNum                       r6
#define subFFTSize                      r7


@//Output Registers


@//Local Scratch Registers

#define pointStep                       r3
#define outPointStep                    r3
#define grpSize                         r4
#define setCount                        r4
#define step                            r8
#define dstStep                         r8

@// Neon Registers

#define dX0                             D0.S16
#define dX1                             D1.S16
#define dY0                             D2.S16
#define dY1                             D3.S16
#define dX0S32                          D0.S32
#define dX1S32                          D1.S32
#define dY0S32                          D2.S32
#define dY1S32                          D3.S32


        .MACRO FFTSTAGE scaled, inverse, name

        @// Define stack arguments


        @// update subFFTSize and subFFTNum into RN6 and RN7 for the next stage


        MOV        subFFTSize,#2
        LSR        grpSize,subFFTNum,#1
        MOV        subFFTNum,grpSize


        @// pT0+1 increments pT0 by 8 bytes
        @// pT0+pointStep = increment of 4*pointStep bytes = 2*grpSize bytes
        @// Note: outPointStep = pointStep for firststage
        @// Note: setCount = grpSize/2 (reuse the updated grpSize for setCount)

        MOV        pointStep,grpSize,LSL #2
        RSB        step,pointStep,#4


        @// Loop on the sets for grp zero: 1 set at a time

grpZeroSetLoop\name:

        VLD1    {dX0S32[0]},[pSrc],pointStep
        VLD1    {dX1S32[0]},[pSrc],step                   @// step = -pointStep + 4
        SUBS    setCount,setCount,#1              @// decrement the loop counter

        .ifeqs "\scaled", "TRUE"

            VHADD    dY0,dX0,dX1
            VHSUB    dY1,dX0,dX1

        .ELSE

            VADD    dY0,dX0,dX1
            VSUB    dY1,dX0,dX1


        .ENDIF

        VST1    {dY0S32[0]},[pDst],outPointStep
        VST1    {dY1S32[0]},[pDst],dstStep                  @// dstStep =  step = -pointStep + 4

        BGT     grpZeroSetLoop\name


        @// reset pSrc to pDst for the next stage
        SUB     pSrc,pDst,pointStep                     @// pDst -= 2*grpSize
        MOV     pDst,pPingPongBuf

        .endm



        M_START armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",FWD
        M_END



        M_START armSP_FFTInv_CToC_SC16_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",INV
        M_END



        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","FALSE",FWDSFS
        M_END



        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","TRUE",INVSFS
        M_END




    .END
