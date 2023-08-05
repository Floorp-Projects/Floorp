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

#define dButterfly1Real02       D0
#define dButterfly1Imag02       D1
#define dButterfly1Real13       D2
#define dButterfly1Imag13       D3
#define dButterfly2Real02       D4
#define dButterfly2Imag02       D5
#define dButterfly2Real13       D6
#define dButterfly2Imag13       D7
#define dXr0                    D0
#define dXi0                    D1
#define dXr1                    D2
#define dXi1                    D3
#define dXr2                    D4
#define dXi2                    D5
#define dXr3                    D6
#define dXi3                    D7

#define dYr0                    D16
#define dYi0                    D17
#define dYr1                    D18
#define dYi1                    D19
#define dYr2                    D20
#define dYi2                    D21
#define dYr3                    D22
#define dYi3                    D23

#define dW1r                    D8
#define dW1i                    D9
#define dW2r                    D10
#define dW2i                    D11
#define dW3r                    D12
#define dW3i                    D13
#define qT0                     d14
#define qT1                     d16
#define qT2                     d18
#define qT3                     d20
#define qT4                     d22
#define qT5                     d24

#define dZr0                    D14
#define dZi0                    D15
#define dZr1                    D26
#define dZi1                    D27
#define dZr2                    D28
#define dZi2                    D29
#define dZr3                    D30
#define dZi3                    D31

#define qX0                     Q0
#define qY0                     Q8
#define qY1                     Q9
#define qY2                     Q10
#define qY3                     Q11
#define qZ0                     Q7
#define qZ1                     Q13
#define qZ2                     Q14
#define qZ3                     Q15



        .MACRO FFTSTAGE scaled, inverse , name

        @// Define stack arguments


        @// pOut0+1 increments pOut0 by 8 bytes
        @// pOut0+outPointStep == increment of 8*outPointStep bytes
        MOV     outPointStep,subFFTSize,LSL #3

        @// Update grpCount and grpSize rightaway

        VLD2.F32    {dW1r,dW1i},[pTwiddle, :128]             @// [wi|wr]
        MOV     step16,#16
        LSL     grpCount,subFFTSize,#2

        VLD1.F32    dW2r,[pTwiddle, :64]                     @// [wi|wr]
        MOV     subFFTNum,#1                            @//after the last stage

        VLD1.F32    dW3r,[pTwiddle, :64],step16              @// [wi|wr]
        MOV     stepTwiddle,#0

        VLD1.F32    dW2i,[pTwiddle, :64]!                    @// [wi|wr]
        SUB     grpTwStep,stepTwiddle,#8                @// grpTwStep = -8 to start with

        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
        VLD1.F32    dW3i,[pTwiddle, :64],grpTwStep           @// [wi|wr]
        MOV     dstStep,outPointStep,LSL #1

        @// AC.r AC.i BD.r BD.i
        VLD4.F32     {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]!
        ADD     dstStep,dstStep,outPointStep            @// dstStep = 3*outPointStep
        RSB     dstStep,dstStep,#16                     @// dstStep = - 3*outPointStep+16
        MOV     step24,#24

        @// AC.r AC.i BD.r BD.i
        VLD4.F32     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]!


        @// Process two groups at a time

radix4lsGrpLoop\name :

        VZIP.F32    dW2r,dW2i
        ADD     stepTwiddle,stepTwiddle,#16
        VZIP.F32    dW3r,dW3i
        ADD     grpTwStep,stepTwiddle,#4
        VUZP.F32     dButterfly1Real13, dButterfly2Real13   @// B.r D.r
        SUB     twStep,stepTwiddle,#16                  @// -16+stepTwiddle
        VUZP.F32     dButterfly1Imag13, dButterfly2Imag13   @// B.i D.i
        MOV     grpTwStep,grpTwStep,LSL #1
        VUZP.F32     dButterfly1Real02, dButterfly2Real02   @// A.r C.r
        RSB     grpTwStep,grpTwStep,#0                  @// -8-2*stepTwiddle


        VUZP.F32     dButterfly1Imag02, dButterfly2Imag02   @// A.i C.i


        @// grpCount is multiplied by 4
        SUBS    grpCount,grpCount,#8

        .ifeqs  "\inverse", "TRUE"
            VMUL.F32   dZr1,dW1r,dXr1
            VMLA.F32   dZr1,dW1i,dXi1                       @// real part
            VMUL.F32   dZi1,dW1r,dXi1
            VMLS.F32   dZi1,dW1i,dXr1                       @// imag part

        .else

            VMUL.F32   dZr1,dW1r,dXr1
            VMLS.F32   dZr1,dW1i,dXi1                       @// real part
            VMUL.F32   dZi1,dW1r,dXi1
            VMLA.F32   dZi1,dW1i,dXr1                       @// imag part

        .endif

        VLD2.F32    {dW1r,dW1i},[pTwiddle, :128],stepTwiddle      @// [wi|wr]

        .ifeqs  "\inverse", "TRUE"
            VMUL.F32   dZr2,dW2r,dXr2
            VMLA.F32   dZr2,dW2i,dXi2                       @// real part
            VMUL.F32   dZi2,dW2r,dXi2
            VLD1.F32   dW2r,[pTwiddle, :64],step16           @// [wi|wr]
            VMLS.F32   dZi2,dW2i,dXr2                       @// imag part

        .else

            VMUL.F32   dZr2,dW2r,dXr2
            VMLS.F32   dZr2,dW2i,dXi2                       @// real part
            VMUL.F32   dZi2,dW2r,dXi2
            VLD1.F32    dW2r,[pTwiddle, :64],step16          @// [wi|wr]
            VMLA.F32   dZi2,dW2i,dXr2                       @// imag part

        .endif


        VLD1.F32    dW2i,[pTwiddle, :64],twStep              @// [wi|wr]

        @// move qX0 so as to load for the next iteration
        VMOV     qZ0,qX0

        .ifeqs  "\inverse", "TRUE"
            VMUL.F32   dZr3,dW3r,dXr3
            VMLA.F32   dZr3,dW3i,dXi3                       @// real part
            VMUL.F32   dZi3,dW3r,dXi3
            VLD1.F32    dW3r,[pTwiddle, :64],step24
            VMLS.F32   dZi3,dW3i,dXr3                       @// imag part

        .else

            VMUL.F32   dZr3,dW3r,dXr3
            VMLS.F32   dZr3,dW3i,dXi3                       @// real part
            VMUL.F32   dZi3,dW3r,dXi3
            VLD1.F32    dW3r,[pTwiddle, :64],step24
            VMLA.F32   dZi3,dW3i,dXr3                       @// imag part

        .endif

        VLD1.F32    dW3i,[pTwiddle, :64],grpTwStep           @// [wi|wr]

        @// Don't do the load on the last iteration so we don't read past the end
        @// of pSrc.
        addeq   pSrc, pSrc, #64
        beq     radix4lsSkipRead\name
        @// AC.r AC.i BD.r BD.i
        VLD4.F32     {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]!

        @// AC.r AC.i BD.r BD.i
        VLD4.F32     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]!
radix4lsSkipRead\name:

        @// finish first stage of 4 point FFT

        VADD.F32    qY0,qZ0,qZ2
        VSUB.F32    qY2,qZ0,qZ2
        VADD.F32    qY1,qZ1,qZ3
        VSUB.F32    qY3,qZ1,qZ3


        @// finish second stage of 4 point FFT

        .ifeqs  "\inverse", "TRUE"

            VSUB.F32    qZ0,qY2,qY1

            VADD.F32    dZr3,dYr0,dYi3
            VST2.F32    {dZr0,dZi0},[pDst, :128],outPointStep
            VSUB.F32    dZi3,dYi0,dYr3

            VADD.F32    qZ2,qY2,qY1
            VST2.F32    {dZr3,dZi3},[pDst, :128],outPointStep

            VSUB.F32    dZr1,dYr0,dYi3
            VST2.F32    {dZr2,dZi2},[pDst, :128],outPointStep
            VADD.F32    dZi1,dYi0,dYr3

            @// dstStep = -outPointStep + 16
            VST2.F32    {dZr1,dZi1},[pDst, :128],dstStep


        .else

            VSUB.F32    qZ0,qY2,qY1

            VSUB.F32    dZr1,dYr0,dYi3
            VST2.F32    {dZr0,dZi0},[pDst, :128],outPointStep
            VADD.F32    dZi1,dYi0,dYr3

            VADD.F32    qZ2,qY2,qY1
            VST2.F32    {dZr1,dZi1},[pDst, :128],outPointStep

            VADD.F32    dZr3,dYr0,dYi3
            VST2.F32    {dZr2,dZi2},[pDst, :128],outPointStep
            VSUB.F32    dZi3,dYi0,dYr3

            @// dstStep = -outPointStep + 16
            VST2.F32    {dZr3,dZi3},[pDst, :128],dstStep


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
