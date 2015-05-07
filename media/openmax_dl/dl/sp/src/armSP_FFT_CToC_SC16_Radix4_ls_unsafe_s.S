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
@// File Name:  armSP_FFT_CToC_SC16_Radix4_ls_unsafe_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   7765
@// Last Modified Date:       Thu, 27 Sep 2007
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
    @//IMPORT  armAAC_constTable

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
#define pw1                             r8
#define pw2                             r9
#define pw3                             r10
#define pTmp                            r4


@// Neon Registers

#define dButterfly1Real02               D0.S16
#define dButterfly1Imag02               D1.S16
#define dButterfly1Real13               D2.S16
#define dButterfly1Imag13               D3.S16
#define dButterfly2Real02               D4.S16
#define dButterfly2Imag02               D5.S16
#define dButterfly2Real13               D6.S16
#define dButterfly2Imag13               D7.S16
#define dXr0                            D0.S16
#define dXi0                            D1.S16
#define dXr1                            D2.S16
#define dXi1                            D3.S16
#define dXr2                            D4.S16
#define dXi2                            D5.S16
#define dXr3                            D6.S16
#define dXi3                            D7.S16

#define dW1rS32                         D8.S32
#define dW1iS32                         D9.S32
#define dW2rS32                         D10.S32
#define dW2iS32                         D11.S32
#define dW3rS32                         D12.S32
#define dW3iS32                         D13.S32

#define dW1r                            D8.S16
#define dW1i                            D9.S16
#define dW2r                            D10.S16
#define dW2i                            D11.S16
#define dW3r                            D12.S16
#define dW3i                            D13.S16

#define dTmp0                           D12.S16
#define dTmp1                           D13.S16
#define dTmp1S32                        D13.S32
#define dTmp2S32                        D14.S32
#define dTmp3S32                        D15.S32

#define dYr0                            D18.S16
#define dYi0                            D19.S16
#define dYr1                            D16.S16
#define dYi1                            D17.S16
#define dYr2                            D20.S16
#define dYi2                            D21.S16
#define dYr3                            D14.S16
#define dYi3                            D15.S16
#define qY0                             Q9.S16
#define qY1                             Q8.S16
#define qY2                             Q10.S16
#define qY3                             Q7.S16

#define qX0                             Q0.S16
#define qX1                             Q1.S16
#define qX2                             Q2.S16
#define qX3                             Q3.S16

#define qT0                             Q9.S32
#define qT1                             Q10.S32
#define qT2                             Q7.S32
#define qT3                             Q8.S32

#define dZr0                            D22.S16
#define dZi0                            D23.S16
#define dZr1                            D24.S16
#define dZi1                            D25.S16
#define dZr2                            D26.S16
#define dZi2                            D27.S16
#define dZr3                            D28.S16
#define dZi3                            D29.S16

#define qZ0                             Q11.S16
#define qZ1                             Q12.S16
#define qZ2                             Q13.S16
#define qZ3                             Q14.S16


        .MACRO FFTSTAGE scaled, inverse , name

        @// Define stack arguments

        MOV     pw2,pTwiddle
        VLD4 {dW2r,dW2i,dTmp0,dTmp1},[pw2, :256]!

        MOV     pw3,pTwiddle
        MOV     pw1,pTwiddle
        @// pOut0+1 increments pOut0 by 8 bytes
        @// pOut0+outPointStep == increment of 4*outPointStep bytes
        MOV     outPointStep,subFFTSize,LSL #2

        VLD3 {dW3rS32,dTmp1S32,dTmp2S32},[pw3, :64]!
        MOV     subFFTNum,#1                            @//after the last stage
        LSL     grpCount,subFFTSize,#2


        @// Update grpCount and grpSize rightaway
        VLD3 {dW3iS32,dTmp2S32,dTmp3S32},[pw3, :64]!

        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
        MOV     dstStep,outPointStep,LSL #1

        VLD2 {dW1r,dW1i}, [pw1, :128]!


        ADD     dstStep,dstStep,outPointStep                @// dstStep = 3*outPointStep
        RSB     dstStep,dstStep,#16                         @// dstStep = - 3*outPointStep+16

        VLD4     {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i
        VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i

        @// Process 4 groups at a time

grpLoop\name:


        @// Rearrange the third twiddle
        VUZP    dW3r,dW3i
        SUBS    grpCount,grpCount,#16                    @// grpCount is multiplied by 4


        VUZP     dButterfly1Real13, dButterfly2Real13        @// B.r D.r
        VUZP     dButterfly1Imag13, dButterfly2Imag13        @// B.i D.i
        VUZP     dButterfly1Real02, dButterfly2Real02        @// A.r C.r
        VUZP     dButterfly1Imag02, dButterfly2Imag02        @// A.i C.i


        .ifeqs  "\inverse", "TRUE"
            VMULL   qT0,dXr1,dW1r
            VMLAL   qT0,dXi1,dW1i                       @// real part
            VMULL   qT1,dXi1,dW1r
            VMLSL   qT1,dXr1,dW1i                       @// imag part

        .ELSE
            VMULL   qT0,dXr1,dW1r
            VMLSL   qT0,dXi1,dW1i                       @// real part
            VMULL   qT1,dXi1,dW1r
            VMLAL   qT1,dXr1,dW1i                       @// imag part

        .ENDIF

        @// Load the first twiddle for 4 groups : w^1
        @// w^1 twiddle (i+0,i+1,i+2,i+3)       for group 0,1,2,3

        VLD2 {dW1r,dW1i}, [pw1, :128]!

        .ifeqs  "\inverse", "TRUE"
            VMULL   qT2,dXr2,dW2r
            VMLAL   qT2,dXi2,dW2i                       @// real part
            VMULL   qT3,dXi2,dW2r
            VMLSL   qT3,dXr2,dW2i                       @// imag part

        .ELSE
            VMULL   qT2,dXr2,dW2r
            VMLSL   qT2,dXi2,dW2i                       @// real part
            VMULL   qT3,dXi2,dW2r
            VMLAL   qT3,dXr2,dW2i                       @// imag part

        .ENDIF

        VRSHRN  dZr1,qT0,#15
        VRSHRN  dZi1,qT1,#15



        .ifeqs  "\inverse", "TRUE"
            VMULL   qT0,dXr3,dW3r
            VMLAL   qT0,dXi3,dW3i                       @// real part
            VMULL   qT1,dXi3,dW3r
            VMLSL   qT1,dXr3,dW3i                       @// imag part

        .ELSE
            VMULL   qT0,dXr3,dW3r
            VMLSL   qT0,dXi3,dW3i                       @// real part
            VMULL   qT1,dXi3,dW3r
            VMLAL   qT1,dXr3,dW3i                       @// imag part

        .ENDIF

        @// Load the second twiddle for 4 groups : w^2
        @// w^2 twiddle (2i+0,2i+2,2i+4,2i+6)   for group 0,1,2,3
        VLD4 {dW2r,dW2i,dTmp0,dTmp1},[pw2, :256]!


        VRSHRN  dZr2,qT2,#15
        VRSHRN  dZi2,qT3,#15

        @// Load the third twiddle for 4 groups : w^3
        @// w^3 twiddle (3i+0,3i+3,3i+6,3i+9)   for group 0,1,2,3

        VLD3 {dW3rS32,dTmp1S32,dTmp2S32},[pw3, :64]!

        VRSHRN  dZr3,qT0,#15
        VRSHRN  dZi3,qT1,#15

        VLD3 {dW3iS32,dTmp2S32,dTmp3S32},[pw3, :64]!

        .ifeqs "\scaled", "TRUE"

            @// finish first stage of 4 point FFT

            VHADD    qY0,qX0,qZ2
            VHSUB    qY2,qX0,qZ2
            VHADD    qY1,qZ1,qZ3
            VLD4    {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i

            VHSUB    qY3,qZ1,qZ3

            @// finish second stage of 4 point FFT

            VHSUB    qZ0,qY2,qY1
            VHADD    qZ2,qY2,qY1
            VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i


            .ifeqs "\inverse", "TRUE"

                VHADD    dZr3,dYr0,dYi3                          @// y3 = u0-ju3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VHSUB    dZi3,dYi0,dYr3

                VHSUB    dZr1,dYr0,dYi3                          @// y1 = u0+ju3
                VHADD    dZi1,dYi0,dYr3
                VST2    {dZr3,dZi3},[pDst, :128],outPointStep
                VST2    {dZr2,dZi2},[pDst, :128],outPointStep
                VST2    {dZr1,dZi1},[pDst, :128],dstStep              @// dstStep = -3*outPointStep + 16

            .ELSE

                VHSUB    dZr1,dYr0,dYi3                          @// y1 = u0+ju3
                VHADD    dZi1,dYi0,dYr3

                VHADD    dZr3,dYr0,dYi3                          @// y3 = u0-ju3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VHSUB    dZi3,dYi0,dYr3
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VST2    {dZr2,dZi2},[pDst, :128],outPointStep
                VST2    {dZr3,dZi3},[pDst, :128],dstStep              @// dstStep = -3*outPointStep + 16

            .ENDIF

        .ELSE

            @// finish first stage of 4 point FFT

            VADD    qY0,qX0,qZ2
            VSUB    qY2,qX0,qZ2
            VADD    qY1,qZ1,qZ3
            VLD4    {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i

            VSUB    qY3,qZ1,qZ3

            @// finish second stage of 4 point FFT

            VSUB    qZ0,qY2,qY1
            VADD    qZ2,qY2,qY1
            VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i


            .ifeqs "\inverse", "TRUE"

                VADD    dZr3,dYr0,dYi3                          @// y3 = u0-ju3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VSUB    dZi3,dYi0,dYr3

                VSUB    dZr1,dYr0,dYi3                          @// y1 = u0+ju3
                VADD    dZi1,dYi0,dYr3
                VST2    {dZr3,dZi3},[pDst, :128],outPointStep
                VST2    {dZr2,dZi2},[pDst, :128],outPointStep
                VST2    {dZr1,dZi1},[pDst, :128],dstStep              @// dstStep = -3*outPointStep + 16

            .ELSE

                VSUB    dZr1,dYr0,dYi3                          @// y1 = u0+ju3
                VADD    dZi1,dYi0,dYr3

                VADD    dZr3,dYr0,dYi3                          @// y3 = u0-ju3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VSUB    dZi3,dYi0,dYr3
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
                VST2    {dZr2,dZi2},[pDst, :128],outPointStep
                VST2    {dZr3,dZi3},[pDst, :128],dstStep              @// dstStep = -3*outPointStep + 16

            .ENDIF




        .ENDIF

        BGT     grpLoop\name


        @// Reset and Swap pSrc and pDst for the next stage
        MOV     pTmp,pDst
        SUB     pSrc,pSrc,#64                       @// Extra increment currently done in the loop
        SUB     pDst,pSrc,outPointStep,LSL #2       @// pDst -= size; pSrc -= 4*size bytes
        SUB     pSrc,pTmp,outPointStep

        .endm


        M_START armSP_FFTFwd_CToC_SC16_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",FWD
        M_END


        M_START armSP_FFTInv_CToC_SC16_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",INV
        M_END


        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","FALSE",FWDSFS
        M_END


        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","TRUE",INVSFS
        M_END






    .END
