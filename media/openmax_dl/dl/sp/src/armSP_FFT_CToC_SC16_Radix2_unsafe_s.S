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
@// File Name:  armSP_FFT_CToC_SC16_Radix2_unsafe_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   5892
@// Last Modified Date:       Thu, 07 Jun 2007
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
#define subFFTNum                       r6
#define subFFTSize                      r7


@//Output Registers


@//Local Scratch Registers

#define outPointStep                    r3
#define pointStep                       r4
#define grpCount                        r5
#define setCount                        r8
#define step                            r10
#define dstStep                         r11
#define pTmp                            r9

@// Neon Registers

#define dW                              D0.S16
#define dX0                             D2.S16
#define dX1                             D3.S16
#define dX2                             D4.S16
#define dX3                             D5.S16
#define dY0                             D6.S16
#define dY1                             D7.S16
#define dY2                             D8.S16
#define dY3                             D9.S16
#define qT0                             Q3.S32
#define qT1                             Q4.S32



        .MACRO FFTSTAGE scaled, inverse, name

        @// Define stack arguments


        @// Update grpCount and grpSize rightaway inorder to reuse pGrpCount and pGrpSize regs

        LSR     subFFTNum,subFFTNum,#1                      @//grpSize
        LSL     grpCount,subFFTSize,#1


        @// pT0+1 increments pT0 by 8 bytes
        @// pT0+pointStep = increment of 4*pointStep bytes = 2*grpSize bytes
        MOV     pointStep,subFFTNum,LSL #1

        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount

        @// pOut0+1 increments pOut0 by 8 bytes
        @// pOut0+outPointStep == increment of 4*outPointStep bytes = 2*size bytes
        SMULBB  outPointStep,grpCount,pointStep
        LSL     pointStep,pointStep,#1


        RSB      step,pointStep,#16
        RSB      dstStep,outPointStep,#16

        @// Loop on the groups

grpLoop\name:

        VLD1     dW,[pTwiddle],pointStep                @//[wi | wr]
        MOV      setCount,pointStep,LSR #2


        @// Loop on the sets: 4 at a time


setLoop\name:


        VLD2    {dX0,dX1},[pSrc],pointStep            @// point0: dX0-real part dX1-img part
        VLD2    {dX2,dX3},[pSrc],step                 @// point1: dX2-real part dX3-img part

        SUBS    setCount,setCount,#4

        .ifeqs  "\inverse", "TRUE"
            VMULL   qT0,dX2,dW[0]
            VMLAL   qT0,dX3,dW[1]                       @// real part
            VMULL   qT1,dX3,dW[0]
            VMLSL   qT1,dX2,dW[1]                       @// imag part

        .ELSE

            VMULL   qT0,dX2,dW[0]
            VMLSL   qT0,dX3,dW[1]                       @// real part
            VMULL   qT1,dX3,dW[0]
            VMLAL   qT1,dX2,dW[1]                       @// imag part

        .ENDIF

        VRSHRN  dX2,qT0,#15
        VRSHRN  dX3,qT1,#15

        .ifeqs "\scaled", "TRUE"
            VHSUB    dY0,dX0,dX2
            VHSUB    dY1,dX1,dX3
            VHADD    dY2,dX0,dX2
            VHADD    dY3,dX1,dX3

        .ELSE
            VSUB    dY0,dX0,dX2
            VSUB    dY1,dX1,dX3
            VADD    dY2,dX0,dX2
            VADD    dY3,dX1,dX3

        .ENDIF

        VST2    {dY0,dY1},[pDst],outPointStep
        VST2    {dY2,dY3},[pDst],dstStep              @// dstStep = -outPointStep + 16

        BGT     setLoop\name

        SUBS    grpCount,grpCount,#2
        ADD     pSrc,pSrc,pointStep
        BGT     grpLoop\name


        @// Reset and Swap pSrc and pDst for the next stage
        MOV     pTmp,pDst
        SUB     pDst,pSrc,outPointStep,LSL #1       @// pDst -= 2*size; pSrc -= 4*size bytes
        SUB     pSrc,pTmp,outPointStep

        @// Reset pTwiddle for the next stage
        SUB     pTwiddle,pTwiddle,outPointStep      @// pTwiddle -= 2*size bytes


        .endm



        M_START armSP_FFTFwd_CToC_SC16_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",FWD
        M_END



        M_START armSP_FFTInv_CToC_SC16_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",INV
        M_END



        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","FALSE",FWDSFS
        M_END



        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","TRUE",INVSFS
        M_END





    .END
