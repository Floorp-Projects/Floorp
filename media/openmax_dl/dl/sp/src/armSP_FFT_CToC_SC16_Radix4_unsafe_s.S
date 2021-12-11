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
@// File Name:  armSP_FFT_CToC_SC16_Radix4_unsafe_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   7761
@// Last Modified Date:       Wed, 26 Sep 2007
@//
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@//
@//
@//
@// Description:
@// Compute a Radix 4 FFT stage for a N point complex signal
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


@// Import symbols required from other files
@// (For example tables)


@//Input Registers

#define pSrc                            r0
#define pDst                            r2
#define pTwiddle                        r1
#define subFFTNum                       r6
#define subFFTSize                      r7



@//Output Registers


@//Local Scratch Registers

#define grpCount                        r3
#define pointStep                       r4
#define outPointStep                    r5
#define stepTwiddle                     r12
#define setCount                        r14
#define srcStep                         r8
#define setStep                         r9
#define dstStep                         r10
#define twStep                          r11
#define t1                              r3

@// Neon Registers

#define dW1                             D0.S16
#define dW2                             D1.S16
#define dW3                             D2.S16

#define dXr0                            D4.S16
#define dXi0                            D5.S16
#define dXr1                            D6.S16
#define dXi1                            D7.S16
#define dXr2                            D8.S16
#define dXi2                            D9.S16
#define dXr3                            D10.S16
#define dXi3                            D11.S16
#define dYr0                            D12.S16
#define dYi0                            D13.S16
#define dYr1                            D14.S16
#define dYi1                            D15.S16
#define dYr2                            D16.S16
#define dYi2                            D17.S16
#define dYr3                            D18.S16
#define dYi3                            D19.S16
#define qT0                             Q8.S32
#define qT1                             Q9.S32
#define qT2                             Q6.S32
#define qT3                             Q7.S32

#define dZr0                            D20.S16
#define dZi0                            D21.S16
#define dZr1                            D22.S16
#define dZi1                            D23.S16
#define dZr2                            D24.S16
#define dZi2                            D25.S16
#define dZr3                            D26.S16
#define dZi3                            D27.S16
#define qY0                             Q6.S16
#define qY1                             Q7.S16
#define qY2                             Q8.S16
#define qY3                             Q9.S16
#define qX0                             Q2.S16
#define qZ0                             Q10.S16
#define qZ1                             Q11.S16
#define qZ2                             Q12.S16
#define qZ3                             Q13.S16


        .MACRO FFTSTAGE scaled, inverse , name

        @// Define stack arguments


        @// Update grpCount and grpSize rightaway inorder to reuse pGrpCount and pGrpSize regs

        LSL     grpCount,subFFTSize,#2
        LSR     subFFTNum,subFFTNum,#2
        MOV     subFFTSize,grpCount


        @// pOut0+1 increments pOut0 by 4 bytes
        @// pOut0+outPointStep == increment of 4*outPointStep bytes = size bytes

        MOV     stepTwiddle,#0
        SMULBB  outPointStep,grpCount,subFFTNum

        @// pT0+1 increments pT0 by 4 bytes
        @// pT0+pointStep = increment of 4*pointStep bytes = grpSize bytes

        LSL     pointStep,subFFTNum,#2                      @// 2*grpSize

        VLD1     dW1,[pTwiddle, :64]                             @//[wi | wr]
        MOV     srcStep,pointStep,LSL #1                    @// srcStep = 2*pointStep
        VLD1     dW2,[pTwiddle, :64]                             @//[wi | wr]
        ADD     setStep,srcStep,pointStep                   @// setStep = 3*pointStep
        SUB     srcStep,srcStep,#16                         @// srcStep = 2*pointStep-16
        VLD1     dW3,[pTwiddle, :64]
        @//RSB     setStep,setStep,#16                      @// setStep = - 3*pointStep+16
        RSB     setStep,setStep,#0                          @// setStep = - 3*pointStep

        MOV     dstStep,outPointStep,LSL #1
        ADD     dstStep,dstStep,outPointStep                @// dstStep = 3*outPointStep
        RSB     dstStep,dstStep,#16                         @// dstStep = - 3*outPointStep+16



grpLoop\name:

        VLD2    {dXr0,dXi0},[pSrc, :128],pointStep          @//  data[0]
        ADD      stepTwiddle,stepTwiddle,pointStep
        VLD2    {dXr1,dXi1},[pSrc, :128],pointStep          @//  data[1]
        ADD      pTwiddle,pTwiddle,stepTwiddle               @// set pTwiddle to the first point
        VLD2    {dXr2,dXi2},[pSrc, :128],pointStep          @//  data[2]
        MOV      twStep,stepTwiddle,LSL #2
        VLD2    {dXr3,dXi3},[pSrc, :128],setStep            @//  data[3] & reset pSrc

        SUB      twStep,stepTwiddle,twStep                   @// twStep = -3*stepTwiddle


        MOV      setCount,pointStep,LSR #2
        ADD     pSrc,pSrc,#16                         @// set pSrc to data[0] of the next set
        ADD     pSrc,pSrc,pointStep                   @// increment to data[1] of the next set

        @// Loop on the sets : 4 at a time

setLoop\name:

        SUBS    setCount,setCount,#4                    @// decrement the loop counter

        .ifeqs  "\inverse", "TRUE"
            VMULL   qT0,dXr1,dW1[0]
            VMLAL   qT0,dXi1,dW1[1]                       @// real part
            VMULL   qT1,dXi1,dW1[0]
            VMLSL   qT1,dXr1,dW1[1]                       @// imag part

        .ELSE
            VMULL   qT0,dXr1,dW1[0]
            VMLSL   qT0,dXi1,dW1[1]                       @// real part
            VMULL   qT1,dXi1,dW1[0]
            VMLAL   qT1,dXr1,dW1[1]                       @// imag part

        .ENDIF

        VLD2    {dXr1,dXi1},[pSrc, :128],pointStep          @//  data[1]

        .ifeqs  "\inverse", "TRUE"
            VMULL   qT2,dXr2,dW2[0]
            VMLAL   qT2,dXi2,dW2[1]                       @// real part
            VMULL   qT3,dXi2,dW2[0]
            VMLSL   qT3,dXr2,dW2[1]                       @// imag part

        .ELSE
            VMULL   qT2,dXr2,dW2[0]
            VMLSL   qT2,dXi2,dW2[1]                       @// real part
            VMULL   qT3,dXi2,dW2[0]
            VMLAL   qT3,dXr2,dW2[1]                       @// imag part

        .ENDIF

        VRSHRN  dZr1,qT0,#15
        VRSHRN  dZi1,qT1,#15


        VLD2    {dXr2,dXi2},[pSrc, :128],pointStep          @//  data[2]

        .ifeqs  "\inverse", "TRUE"
            VMULL   qT0,dXr3,dW3[0]
            VMLAL   qT0,dXi3,dW3[1]                       @// real part
            VMULL   qT1,dXi3,dW3[0]
            VMLSL   qT1,dXr3,dW3[1]                       @// imag part

        .ELSE
            VMULL   qT0,dXr3,dW3[0]
            VMLSL   qT0,dXi3,dW3[1]                       @// real part
            VMULL   qT1,dXi3,dW3[0]
            VMLAL   qT1,dXr3,dW3[1]                       @// imag part

        .ENDIF

        VRSHRN  dZr2,qT2,#15
        VRSHRN  dZi2,qT3,#15


        VRSHRN  dZr3,qT0,#15
        VRSHRN  dZi3,qT1,#15
        VLD2    {dXr3,dXi3},[pSrc, :128],setStep            @//  data[3] & update pSrc for the next set


        .ifeqs "\scaled", "TRUE"

            @// finish first stage of 4 point FFT
            VHADD    qY0,qX0,qZ2
            VHSUB    qY2,qX0,qZ2

            VLD2    {dXr0,dXi0},[pSrc, :128]!          @//  data[0]
            VHADD    qY1,qZ1,qZ3
            VHSUB    qY3,qZ1,qZ3


            @// finish second stage of 4 point FFT

            .ifeqs  "\inverse", "TRUE"

                VHSUB    qZ0,qY2,qY1

                VHADD    dZr2,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VHSUB    dZi2,dYi0,dYr3

                VHADD    qZ1,qY2,qY1
                VST2    {dZr2,dZi2},[pDst, :128],outPointStep

                VHSUB    dZr3,dYr0,dYi3
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VHADD    dZi3,dYi0,dYr3
                VST2    {dZr3,dZi3},[pDst, :128],dstStep


            .ELSE

                VHSUB    qZ0,qY2,qY1

                VHSUB    dZr3,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VHADD    dZi3,dYi0,dYr3

                VHADD    qZ1,qY2,qY1
                VST2    {dZr3,dZi3},[pDst, :128],outPointStep

                VHADD    dZr2,dYr0,dYi3
                VHSUB    dZi2,dYi0,dYr3
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VST2    {dZr2,dZi2},[pDst, :128],dstStep


            .ENDIF


        .ELSE

            @// finish first stage of 4 point FFT
            VADD    qY0,qX0,qZ2
            VSUB    qY2,qX0,qZ2

            VLD2    {dXr0,dXi0},[pSrc]!          @//  data[0]
            VADD    qY1,qZ1,qZ3
            VSUB    qY3,qZ1,qZ3


            @// finish second stage of 4 point FFT


            .ifeqs  "\inverse", "TRUE"

                VSUB    qZ0,qY2,qY1

                VADD    dZr2,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VSUB    dZi2,dYi0,dYr3

                VADD    qZ1,qY2,qY1
                VST2    {dZr2,dZi2},[pDst, :128],outPointStep

                VSUB    dZr3,dYr0,dYi3
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VADD    dZi3,dYi0,dYr3
                VST2    {dZr3,dZi3},[pDst, :128],dstStep


            .ELSE

                VSUB    qZ0,qY2,qY1

                VSUB    dZr3,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VADD    dZi3,dYi0,dYr3

                VADD    qZ1,qY2,qY1
                VST2    {dZr3,dZi3},[pDst, :128],outPointStep

                VADD    dZr2,dYr0,dYi3
                VSUB    dZi2,dYi0,dYr3
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VST2    {dZr2,dZi2},[pDst, :128],dstStep


            .ENDIF



        .ENDIF

        ADD     pSrc,pSrc,pointStep                         @// increment to data[1] of the next set
        BGT     setLoop\name

        VLD1     dW1,[pTwiddle, :64],stepTwiddle                 @//[wi | wr]
        SUBS    grpCount,grpCount,#4                        @// subtract 4 since grpCount multiplied by 4
        VLD1     dW2,[pTwiddle, :64],stepTwiddle                 @//[wi | wr]
        ADD     pSrc,pSrc,srcStep                           @// increment pSrc for the next grp
        VLD1     dW3,[pTwiddle, :64],twStep                      @//[wi | wr]



        BGT     grpLoop\name


        @// Reset and Swap pSrc and pDst for the next stage
        MOV     t1,pDst
        SUB     pDst,pSrc,outPointStep,LSL #2           @// pDst -= size; pSrc -= 4*size bytes
        SUB     pSrc,t1,outPointStep


        .endm


        M_START armSP_FFTFwd_CToC_SC16_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE "FALSE","FALSE",FWD
        M_END


        M_START armSP_FFTInv_CToC_SC16_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE "FALSE","TRUE",INV
        M_END


        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE "TRUE","FALSE",FWDSFS
        M_END


        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE "TRUE","TRUE",INVSFS
        M_END





    .END
