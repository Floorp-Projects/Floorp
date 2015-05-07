@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This is a modification of armSP_FFT_CToC_SC32_Radix4_ls_unsafe_s.s
@//  to support float instead of SC32.
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


@// Import symbols required from other files
@// (For example tables)
    @//IMPORT  armAAC_constTable

@//Input Registers

#define pSrc            r0
#define pDst            r2
#define pTwiddle        r1
#define subFFTNum       r6
#define subFFTSize      r7



@//Output Registers


@//Local Scratch Registers

#define outPointStep    r3
#define grpCount        r4
#define dstStep         r5
#define grpTwStep       r8
#define stepTwiddle     r9
#define twStep          r10
#define pTmp            r4
#define step16          r11
#define step24          r12


@// Neon Registers

#define dButterfly1Real02       D0.F32
#define dButterfly1Imag02       D1.F32
#define dButterfly1Real13       D2.F32
#define dButterfly1Imag13       D3.F32
#define dButterfly2Real02       D4.F32
#define dButterfly2Imag02       D5.F32
#define dButterfly2Real13       D6.F32
#define dButterfly2Imag13       D7.F32
#define dXr0                    D0.F32
#define dXi0                    D1.F32
#define dXr1                    D2.F32
#define dXi1                    D3.F32
#define dXr2                    D4.F32
#define dXi2                    D5.F32
#define dXr3                    D6.F32
#define dXi3                    D7.F32

#define dYr0                    D16.F32
#define dYi0                    D17.F32
#define dYr1                    D18.F32
#define dYi1                    D19.F32
#define dYr2                    D20.F32
#define dYi2                    D21.F32
#define dYr3                    D22.F32
#define dYi3                    D23.F32

#define dW1r                    D8.F32
#define dW1i                    D9.F32
#define dW2r                    D10.F32
#define dW2i                    D11.F32
#define dW3r                    D12.F32
#define dW3i                    D13.F32
#define qT0                     d14.f32
#define qT1                     d16.F32
#define qT2                     d18.F32
#define qT3                     d20.f32
#define qT4                     d22.f32
#define qT5                     d24.f32

#define dZr0                    D14.F32
#define dZi0                    D15.F32
#define dZr1                    D26.F32
#define dZi1                    D27.F32
#define dZr2                    D28.F32
#define dZi2                    D29.F32
#define dZr3                    D30.F32
#define dZi3                    D31.F32

#define qX0                     Q0.F32
#define qY0                     Q8.F32
#define qY1                     Q9.F32
#define qY2                     Q10.F32
#define qY3                     Q11.F32
#define qZ0                     Q7.F32
#define qZ1                     Q13.F32
#define qZ2                     Q14.F32
#define qZ3                     Q15.F32



        .MACRO FFTSTAGE scaled, inverse , name

        @// Define stack arguments


        @// pOut0+1 increments pOut0 by 8 bytes
        @// pOut0+outPointStep == increment of 8*outPointStep bytes
        MOV     outPointStep,subFFTSize,LSL #3

        @// Update grpCount and grpSize rightaway

        VLD2    {dW1r,dW1i},[pTwiddle, :128]             @// [wi|wr]
        MOV     step16,#16
        LSL     grpCount,subFFTSize,#2

        VLD1    dW2r,[pTwiddle, :64]                     @// [wi|wr]
        MOV     subFFTNum,#1                            @//after the last stage

        VLD1    dW3r,[pTwiddle, :64],step16              @// [wi|wr]
        MOV     stepTwiddle,#0

        VLD1    dW2i,[pTwiddle, :64]!                    @// [wi|wr]
        SUB     grpTwStep,stepTwiddle,#8                @// grpTwStep = -8 to start with

        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
        VLD1    dW3i,[pTwiddle, :64],grpTwStep           @// [wi|wr]
        MOV     dstStep,outPointStep,LSL #1

        @// AC.r AC.i BD.r BD.i
        VLD4     {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]!
        ADD     dstStep,dstStep,outPointStep            @// dstStep = 3*outPointStep
        RSB     dstStep,dstStep,#16                     @// dstStep = - 3*outPointStep+16
        MOV     step24,#24

        @// AC.r AC.i BD.r BD.i
        VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]!


        @// Process two groups at a time

radix4lsGrpLoop\name :

        VZIP    dW2r,dW2i
        ADD     stepTwiddle,stepTwiddle,#16
        VZIP    dW3r,dW3i
        ADD     grpTwStep,stepTwiddle,#4
        VUZP     dButterfly1Real13, dButterfly2Real13   @// B.r D.r
        SUB     twStep,stepTwiddle,#16                  @// -16+stepTwiddle
        VUZP     dButterfly1Imag13, dButterfly2Imag13   @// B.i D.i
        MOV     grpTwStep,grpTwStep,LSL #1
        VUZP     dButterfly1Real02, dButterfly2Real02   @// A.r C.r
        RSB     grpTwStep,grpTwStep,#0                  @// -8-2*stepTwiddle


        VUZP     dButterfly1Imag02, dButterfly2Imag02   @// A.i C.i


        @// grpCount is multiplied by 4
        SUBS    grpCount,grpCount,#8

        .ifeqs  "\inverse", "TRUE"
            VMUL   dZr1,dW1r,dXr1
            VMLA   dZr1,dW1i,dXi1                       @// real part
            VMUL   dZi1,dW1r,dXi1
            VMLS   dZi1,dW1i,dXr1                       @// imag part

        .else

            VMUL   dZr1,dW1r,dXr1
            VMLS   dZr1,dW1i,dXi1                       @// real part
            VMUL   dZi1,dW1r,dXi1
            VMLA   dZi1,dW1i,dXr1                       @// imag part

        .endif

        VLD2    {dW1r,dW1i},[pTwiddle, :128],stepTwiddle      @// [wi|wr]

        .ifeqs  "\inverse", "TRUE"
            VMUL   dZr2,dW2r,dXr2
            VMLA   dZr2,dW2i,dXi2                       @// real part
            VMUL   dZi2,dW2r,dXi2
            VLD1   dW2r,[pTwiddle, :64],step16           @// [wi|wr]
            VMLS   dZi2,dW2i,dXr2                       @// imag part

        .else

            VMUL   dZr2,dW2r,dXr2
            VMLS   dZr2,dW2i,dXi2                       @// real part
            VMUL   dZi2,dW2r,dXi2
            VLD1    dW2r,[pTwiddle, :64],step16          @// [wi|wr]
            VMLA   dZi2,dW2i,dXr2                       @// imag part

        .endif


        VLD1    dW2i,[pTwiddle, :64],twStep              @// [wi|wr]

        @// move qX0 so as to load for the next iteration
        VMOV     qZ0,qX0

        .ifeqs  "\inverse", "TRUE"
            VMUL   dZr3,dW3r,dXr3
            VMLA   dZr3,dW3i,dXi3                       @// real part
            VMUL   dZi3,dW3r,dXi3
            VLD1    dW3r,[pTwiddle, :64],step24
            VMLS   dZi3,dW3i,dXr3                       @// imag part

        .else

            VMUL   dZr3,dW3r,dXr3
            VMLS   dZr3,dW3i,dXi3                       @// real part
            VMUL   dZi3,dW3r,dXi3
            VLD1    dW3r,[pTwiddle, :64],step24
            VMLA   dZi3,dW3i,dXr3                       @// imag part

        .endif

        VLD1    dW3i,[pTwiddle, :64],grpTwStep           @// [wi|wr]

        @// Don't do the load on the last iteration so we don't read past the end
        @// of pSrc.
        addeq   pSrc, pSrc, #64
        beq     radix4lsSkipRead\name
        @// AC.r AC.i BD.r BD.i
        VLD4     {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]!

        @// AC.r AC.i BD.r BD.i
        VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]!
radix4lsSkipRead\name:

        @// finish first stage of 4 point FFT

        VADD    qY0,qZ0,qZ2
        VSUB    qY2,qZ0,qZ2
        VADD    qY1,qZ1,qZ3
        VSUB    qY3,qZ1,qZ3


        @// finish second stage of 4 point FFT

        .ifeqs  "\inverse", "TRUE"

            VSUB    qZ0,qY2,qY1

            VADD    dZr3,dYr0,dYi3
            VST2    {dZr0,dZi0},[pDst, :128],outPointStep
            VSUB    dZi3,dYi0,dYr3

            VADD    qZ2,qY2,qY1
            VST2    {dZr3,dZi3},[pDst, :128],outPointStep

            VSUB    dZr1,dYr0,dYi3
            VST2    {dZr2,dZi2},[pDst, :128],outPointStep
            VADD    dZi1,dYi0,dYr3

            @// dstStep = -outPointStep + 16
            VST2    {dZr1,dZi1},[pDst, :128],dstStep


        .else

            VSUB    qZ0,qY2,qY1

            VSUB    dZr1,dYr0,dYi3
            VST2    {dZr0,dZi0},[pDst, :128],outPointStep
            VADD    dZi1,dYi0,dYr3

            VADD    qZ2,qY2,qY1
            VST2    {dZr1,dZi1},[pDst, :128],outPointStep

            VADD    dZr3,dYr0,dYi3
            VST2    {dZr2,dZi2},[pDst, :128],outPointStep
            VSUB    dZi3,dYi0,dYr3

            @// dstStep = -outPointStep + 16
            VST2    {dZr3,dZi3},[pDst, :128],dstStep


        .endif

        BGT     radix4lsGrpLoop\name


        @// Reset and Swap pSrc and pDst for the next stage
        MOV     pTmp,pDst
        @// Extra increment done in final iteration of the loop
        SUB     pSrc,pSrc,#64
        @// pDst -= 4*size; pSrc -= 8*size bytes
        SUB     pDst,pSrc,outPointStep,LSL #2
        SUB     pSrc,pTmp,outPointStep
        SUB     pTwiddle,pTwiddle,subFFTSize,LSL #1
        @// Extra increment done in final iteration of the loop
        SUB     pTwiddle,pTwiddle,#16

        .endm


        M_START armSP_FFTFwd_CToC_FC32_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",fwd
        M_END


        M_START armSP_FFTInv_CToC_FC32_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",inv
        M_END


        .end
