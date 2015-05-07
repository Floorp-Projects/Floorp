@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//
@//  This is a modification of armSP_FFT_CToC_SC32_Radix4_fs_unsafe_s.s
@//  to support float instead of SC32.
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

#define pSrc            r0
#define pDst            r2
#define pTwiddle        r1
#define pPingPongBuf    r5
#define subFFTNum       r6
#define subFFTSize      r7


@//Output Registers


@//Local Scratch Registers

#define grpSize         r3
@// Reuse grpSize as setCount
#define setCount        r3
#define pointStep       r4
#define outPointStep    r4
#define setStep         r8
#define step1           r9
#define step3           r10

@// Neon Registers

#define dXr0    D0.F32
#define dXi0    D1.F32
#define dXr1    D2.F32
#define dXi1    D3.F32
#define dXr2    D4.F32
#define dXi2    D5.F32
#define dXr3    D6.F32
#define dXi3    D7.F32
#define dYr0    D8.F32
#define dYi0    D9.F32
#define dYr1    D10.F32
#define dYi1    D11.F32
#define dYr2    D12.F32
#define dYi2    D13.F32
#define dYr3    D14.F32
#define dYi3    D15.F32
#define qX0     Q0.F32
#define qX1     Q1.F32
#define qX2     Q2.F32
#define qX3     Q3.F32
#define qY0     Q4.F32
#define qY1     Q5.F32
#define qY2     Q6.F32
#define qY3     Q7.F32
#define dZr0    D16.F32
#define dZi0    D17.F32
#define dZr1    D18.F32
#define dZi1    D19.F32
#define dZr2    D20.F32
#define dZi2    D21.F32
#define dZr3    D22.F32
#define dZi3    D23.F32
#define qZ0     Q8.F32
#define qZ1     Q9.F32
#define qZ2     Q10.F32
#define qZ3     Q11.F32


        .MACRO FFTSTAGE scaled, inverse, name

        @// Define stack arguments

        @// pT0+1 increments pT0 by 8 bytes
        @// pT0+pointStep = increment of 8*pointStep bytes = 2*grpSize bytes
        @// Note: outPointStep = pointStep for firststage

        MOV     pointStep,subFFTNum,LSL #1


        @// Update pSubFFTSize and pSubFFTNum regs
        VLD2    {dXr0,dXi0},[pSrc, :128],pointStep          @//  data[0]
        @// subFFTSize = 1 for the first stage
        MOV     subFFTSize,#4

        @// Note: setCount = subFFTNum/4 (reuse the grpSize reg for setCount)
        LSR     grpSize,subFFTNum,#2
        VLD2    {dXr1,dXi1},[pSrc, :128],pointStep          @//  data[1]
        MOV     subFFTNum,grpSize


        @// Calculate the step of input data for the next set
        @//MOV     setStep,pointStep,LSL #1
        MOV     setStep,grpSize,LSL #4
        VLD2    {dXr2,dXi2},[pSrc, :128],pointStep          @//  data[2]
        @// setStep = 3*pointStep
        ADD     setStep,setStep,pointStep
        @// setStep = - 3*pointStep+16
        RSB     setStep,setStep,#16

        @//  data[3] & update pSrc for the next set
        VLD2    {dXr3,dXi3},[pSrc, :128],setStep
        @// step1 = 2*pointStep
        MOV     step1,pointStep,LSL #1

        VADD    qY0,qX0,qX2

        @// step3 = -pointStep
        RSB     step3,pointStep,#0

        @// grp = 0 a special case since all the twiddle factors are 1
        @// Loop on the sets : 2 sets at a time

radix4fsGrpZeroSetLoop\name :



        @// Decrement setcount
        SUBS    setCount,setCount,#2


        @// finish first stage of 4 point FFT


        VSUB    qY2,qX0,qX2

        VLD2    {dXr0,dXi0},[pSrc, :128],step1          @//  data[0]
        VADD    qY1,qX1,qX3
        VLD2    {dXr2,dXi2},[pSrc, :128],step3          @//  data[2]
        VSUB    qY3,qX1,qX3


        @// finish second stage of 4 point FFT

        .ifeqs "\inverse", "TRUE"

            VLD2    {dXr1,dXi1},[pSrc, :128],step1          @//  data[1]
            VADD    qZ0,qY0,qY1

            @//  data[3] & update pSrc for the next set, but not if it's the
            @//  last iteration so that we don't read past the end of the 
            @//  input array.
            BEQ     radix4SkipLastUpdateInv\name
            VLD2    {dXr3,dXi3},[pSrc, :128],setStep
radix4SkipLastUpdateInv\name:
            VSUB    dZr3,dYr2,dYi3

            VST2    {dZr0,dZi0},[pDst, :128],outPointStep
            VADD    dZi3,dYi2,dYr3

            VSUB    qZ1,qY0,qY1
            VST2    {dZr3,dZi3},[pDst, :128],outPointStep

            VADD    dZr2,dYr2,dYi3
            VST2    {dZr1,dZi1},[pDst, :128],outPointStep
            VSUB    dZi2,dYi2,dYr3

            VADD    qY0,qX0,qX2                     @// u0 for next iteration
            VST2    {dZr2,dZi2},[pDst, :128],setStep


        .else

            VLD2    {dXr1,dXi1},[pSrc, :128],step1          @//  data[1]
            VADD    qZ0,qY0,qY1

            @//  data[3] & update pSrc for the next set, but not if it's the
            @//  last iteration so that we don't read past the end of the 
            @//  input array.
            BEQ     radix4SkipLastUpdateFwd\name
            VLD2    {dXr3,dXi3},[pSrc, :128],setStep
radix4SkipLastUpdateFwd\name:
            VADD    dZr2,dYr2,dYi3

            VST2    {dZr0,dZi0},[pDst, :128],outPointStep
            VSUB    dZi2,dYi2,dYr3

            VSUB    qZ1,qY0,qY1
            VST2    {dZr2,dZi2},[pDst, :128],outPointStep

            VSUB    dZr3,dYr2,dYi3
            VST2    {dZr1,dZi1},[pDst, :128],outPointStep
            VADD    dZi3,dYi2,dYr3

            VADD    qY0,qX0,qX2                     @// u0 for next iteration
            VST2    {dZr3,dZi3},[pDst, :128],setStep

        .endif

        BGT     radix4fsGrpZeroSetLoop\name

        @// reset pSrc to pDst for the next stage
        SUB     pSrc,pDst,pointStep                     @// pDst -= 2*grpSize
        MOV     pDst,pPingPongBuf


        .endm



        M_START armSP_FFTFwd_CToC_FC32_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",fwd
        M_END



        M_START armSP_FFTInv_CToC_FC32_Radix4_fs_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",inv
        M_END


        .end
