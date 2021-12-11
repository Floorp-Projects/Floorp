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
@// File Name:  armSP_FFT_CToC_SC16_Radix4_fs_unsafe_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   7761
@// Last Modified Date:       Wed, 26 Sep 2007
@//
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@//
@//
@//
@// Description:
@// Compute a first stage Radix 4 FFT stage for a N point complex signal
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

#define grpSize                         r3
@// Reuse grpSize as setCount
#define setCount                        r3
#define pointStep                       r4
#define outPointStep                    r4
#define setStep                         r8
#define step1                           r9
#define step3                           r10

@// Neon Registers

#define dXr0                            D0.S16
#define dXi0                            D1.S16
#define dXr1                            D2.S16
#define dXi1                            D3.S16
#define dXr2                            D4.S16
#define dXi2                            D5.S16
#define dXr3                            D6.S16
#define dXi3                            D7.S16
#define dYr0                            D8.S16
#define dYi0                            D9.S16
#define dYr1                            D10.S16
#define dYi1                            D11.S16
#define dYr2                            D12.S16
#define dYi2                            D13.S16
#define dYr3                            D14.S16
#define dYi3                            D15.S16
#define dZr0                            D16.S16
#define dZi0                            D17.S16
#define dZr1                            D18.S16
#define dZi1                            D19.S16
#define dZr2                            D20.S16
#define dZi2                            D21.S16
#define dZr3                            D22.S16
#define dZi3                            D23.S16
#define qY0                             Q4.S16
#define qY2                             Q6.S16
#define qX0                             Q0.S16
#define qX2                             Q2.S16

#define qY1                             Q5.S16
#define qY3                             Q7.S16
#define qX1                             Q1.S16
#define qX3                             Q3.S16
#define qZ0                             Q8.S16
#define qZ1                             Q9.S16


        .MACRO FFTSTAGE scaled, inverse, name

        @// Define stack arguments

        MOV     pointStep,subFFTNum
        @// Update pSubFFTSize and pSubFFTNum regs


        VLD2    {dXr0,dXi0},[pSrc, :128],pointStep          @//  data[0]
        @// Note: setCount = subFFTNum/4 (reuse the grpSize reg for setCount)
        LSR     grpSize,subFFTNum,#2
        MOV     subFFTNum,grpSize


        @// pT0+1 increments pT0 by 4 bytes
        @// pT0+pointStep = increment of 4*pointStep bytes = grpSize bytes
        @// Note: outPointStep = pointStep for firststage
        VLD2    {dXr1,dXi1},[pSrc, :128],pointStep          @//  data[1]


        @// Calculate the step of input data for the next set
        @//MOV     setStep,pointStep,LSL #1
        MOV     setStep,grpSize,LSL #3
        VLD2    {dXr2,dXi2},[pSrc, :128],pointStep          @//  data[2]
        MOV     step1,setStep
        ADD     setStep,setStep,pointStep             @// setStep = 3*pointStep
        RSB     setStep,setStep,#16                   @// setStep = - 3*pointStep+16


        VLD2    {dXr3,dXi3},[pSrc, :128],setStep            @//  data[3]
        MOV     subFFTSize,#4                         @// subFFTSize = 1 for the first stage


        .ifeqs  "\scaled", "TRUE"
            VHADD    qY0,qX0,qX2             @// u0
        .ELSE
            VADD   qY0,qX0,qX2               @// u0
        .ENDIF
        RSB     step3,pointStep,#0

        @// grp = 0 a special case since all the twiddle factors are 1
        @// Loop on the sets: 4 sets at a time

grpZeroSetLoop\name:


        .ifeqs "\scaled", "TRUE"

            @// finish first stage of 4 point FFT

            VHSUB    qY2,qX0,qX2             @// u1
            SUBS    setCount,setCount,#4                    @// decrement the set loop counter

            VLD2    {dXr0,dXi0},[pSrc, :128],step1          @//  data[0]
            VHADD    qY1,qX1,qX3             @// u2
            VLD2    {dXr2,dXi2},[pSrc, :128],step3
            VHSUB    qY3,qX1,qX3             @// u3



            @// finish second stage of 4 point FFT

            VLD2    {dXr1,dXi1},[pSrc, :128],step1          @//  data[1]
            VHADD    qZ0,qY0,qY1             @// y0

            VLD2    {dXr3,dXi3},[pSrc, :128],setStep


            .ifeqs  "\inverse", "TRUE"

                VHSUB    dZr3,dYr2,dYi3                  @// y3
                VHADD    dZi3,dYi2,dYr3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep

                VHSUB    qZ1,qY0,qY1                     @// y2
                VST2    {dZr3,dZi3},[pDst, :128],outPointStep

                VHADD    dZr2,dYr2,dYi3                  @// y1
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VHSUB    dZi2,dYi2,dYr3

                VHADD    qY0,qX0,qX2                     @// u0 (next loop)
                VST2    {dZr2,dZi2},[pDst, :128],setStep


            .ELSE

                VHADD    dZr2,dYr2,dYi3                  @// y1
                VHSUB    dZi2,dYi2,dYr3

                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VHSUB    qZ1,qY0,qY1                     @// y2

                VST2    {dZr2,dZi2},[pDst, :128],outPointStep
                VHSUB    dZr3,dYr2,dYi3                  @// y3
                VHADD    dZi3,dYi2,dYr3
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VHADD    qY0,qX0,qX2                     @// u0 (next loop)
                VST2    {dZr3,dZi3},[pDst, :128],setStep

            .ENDIF


        .ELSE

            @// finish first stage of 4 point FFT

            VSUB    qY2,qX0,qX2             @// u1
            SUBS    setCount,setCount,#4                    @// decrement the set loop counter

            VLD2    {dXr0,dXi0},[pSrc, :128],step1          @//  data[0]
            VADD    qY1,qX1,qX3             @// u2
            VLD2    {dXr2,dXi2},[pSrc, :128],step3
            VSUB    qY3,qX1,qX3             @// u3



            @// finish second stage of 4 point FFT

            VLD2    {dXr1,dXi1},[pSrc, :128],step1          @//  data[1]
            VADD    qZ0,qY0,qY1             @// y0

            VLD2    {dXr3,dXi3},[pSrc, :128],setStep


            .ifeqs  "\inverse", "TRUE"

                VSUB    dZr3,dYr2,dYi3                  @// y3
                VADD    dZi3,dYi2,dYr3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep

                VSUB    qZ1,qY0,qY1                     @// y2
                VST2    {dZr3,dZi3},[pDst, :128],outPointStep

                VADD    dZr2,dYr2,dYi3                  @// y1
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VSUB    dZi2,dYi2,dYr3

                VADD    qY0,qX0,qX2                     @// u0 (next loop)
                VST2    {dZr2,dZi2},[pDst, :128],setStep


            .ELSE

                VADD    dZr2,dYr2,dYi3                  @// y1
                VSUB    dZi2,dYi2,dYr3

                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VSUB    qZ1,qY0,qY1                     @// y2

                VST2    {dZr2,dZi2},[pDst, :128],outPointStep
                VSUB    dZr3,dYr2,dYi3                  @// y3
                VADD    dZi3,dYi2,dYr3
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VADD    qY0,qX0,qX2                     @// u0 (next loop)
                VST2    {dZr3,dZi3},[pDst, :128],setStep

            .ENDIF


        .ENDIF

        BGT     grpZeroSetLoop\name


        @// reset pSrc to pDst for the next stage
        SUB     pSrc,pDst,pointStep                     @// pDst -= grpSize
        MOV     pDst,pPingPongBuf


        .endm



        M_START armSP_FFTFwd_CToC_SC16_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",FWD
        M_END



        M_START armSP_FFTInv_CToC_SC16_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",INV
        M_END


        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","FALSE",FWDSFS
        M_END


        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","TRUE",INVSFS
        M_END





    .END
