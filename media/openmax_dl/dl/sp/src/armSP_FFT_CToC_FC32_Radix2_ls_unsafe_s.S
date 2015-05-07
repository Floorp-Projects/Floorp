@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This is a modification of armSP_FFT_CToC_SC32_Radix2_ls_unsafe_s.S
@//  to support float instead of SC32.
@//

@//
@// Description:
@// Compute the last stage of a Radix 2 DIT in-order out-of-place FFT
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
#define pTmp            r4

@// Neon Registers

#define dWr     d0.f32
#define dWi     d1.f32
#define dXr0    d2.f32
#define dXi0    d3.f32
#define dXr1    d4.f32
#define dXi1    d5.f32
#define dYr0    d6.f32
#define dYi0    d7.f32
#define dYr1    d8.f32
#define dYi1    d9.f32
#define qT0     d10.f32
#define qT1     d12.f32

        .MACRO FFTSTAGE scaled, inverse, name


        MOV     outPointStep,subFFTSize,LSL #3
        @// Update grpCount and grpSize rightaway

        MOV     subFFTNum,#1                            @//after the last stage
        LSL     grpCount,subFFTSize,#1

        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount

        RSB      dstStep,outPointStep,#16


        @// Loop on 2 grps at a time for the last stage

radix2lsGrpLoop\name :
        @ dWr = [pTwiddle[0].Re, pTwiddle[1].Re]
        @ dWi = [pTwiddle[0].Im, pTwiddle[1].Im]
        VLD2    {dWr,dWi},[pTwiddle, :64]!

        @ dXr0 = [pSrc[0].Re, pSrc[2].Re]
        @ dXi0 = [pSrc[0].Im, pSrc[2].Im]
        @ dXr1 = [pSrc[1].Re, pSrc[3].Re]
        @ dXi1 = [pSrc[1].Im, pSrc[3].Im]
        VLD4    {dXr0,dXi0,dXr1,dXi1},[pSrc, :128]!
        SUBS    grpCount,grpCount,#4                   @// grpCount is multiplied by 2

        .ifeqs  "\inverse", "TRUE"
            VMUL   qT0,dWr,dXr1
            VMLA   qT0,dWi,dXi1                       @// real part
            VMUL   qT1,dWr,dXi1
            VMLS   qT1,dWi,dXr1                       @// imag part

        .else

            VMUL   qT0,dWr,dXr1
            VMLS   qT0,dWi,dXi1                       @// real part
            VMUL   qT1,dWr,dXi1
            VMLA   qT1,dWi,dXr1                       @// imag part

        .endif

        VSUB    dYr0,dXr0,qT0
        VSUB    dYi0,dXi0,qT1
        VADD    dYr1,dXr0,qT0
        VADD    dYi1,dXi0,qT1

        VST2    {dYr0,dYi0},[pDst],outPointStep
        VST2    {dYr1,dYi1},[pDst],dstStep                  @// dstStep =  step = -outPointStep + 16

        BGT     radix2lsGrpLoop\name


        @// Reset and Swap pSrc and pDst for the next stage
        MOV     pTmp,pDst
        SUB     pDst,pSrc,outPointStep,LSL #1       @// pDst -= 4*size; pSrc -= 8*size bytes
        SUB     pSrc,pTmp,outPointStep

        @// Reset pTwiddle for the next stage
        SUB     pTwiddle,pTwiddle,outPointStep      @// pTwiddle -= 4*size bytes

        .endm



        M_START armSP_FFTFwd_CToC_FC32_Radix2_ls_OutOfPlace_unsafe,r4,""
        FFTSTAGE "FALSE","FALSE",fwd
        M_END



        M_START armSP_FFTInv_CToC_FC32_Radix2_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",inv
        M_END

	.end
