@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This is a modification of armSP_FFT_CToC_FC32_Radix8_fs_unsafe_s.s
@//  to support float instead of SC32.
@//

@//
@// Description:
@// Compute a first stage Radix 8 FFT stage for a N point complex signal
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
#define subFFTNum       r6
#define subFFTSize      r7
@// dest buffer for the next stage (not pSrc for first stage)
#define pPingPongBuf    r5


@//Output Registers


@//Local Scratch Registers

#define grpSize         r3
@// Reuse grpSize as setCount
#define setCount        r3
#define pointStep       r4
#define outPointStep    r4
#define setStep         r8
#define step1           r9
#define step2           r10
#define t0              r11


@// Neon Registers

#define dXr0    D0
#define dXi0    D1
#define dXr1    D2
#define dXi1    D3
#define dXr2    D4
#define dXi2    D5
#define dXr3    D6
#define dXi3    D7
#define dXr4    D8
#define dXi4    D9
#define dXr5    D10
#define dXi5    D11
#define dXr6    D12
#define dXi6    D13
#define dXr7    D14
#define dXi7    D15
#define qX0     Q0
#define qX1     Q1
#define qX2     Q2
#define qX3     Q3
#define qX4     Q4
#define qX5     Q5
#define qX6     Q6
#define qX7     Q7

#define dUr0    D16
#define dUi0    D17
#define dUr2    D18
#define dUi2    D19
#define dUr4    D20
#define dUi4    D21
#define dUr6    D22
#define dUi6    D23
#define dUr1    D24
#define dUi1    D25
#define dUr3    D26
#define dUi3    D27
#define dUr5    D28
#define dUi5    D29
@// reuse dXr7 and dXi7
#define dUr7    D30
#define dUi7    D31
#define qU0     Q8
#define qU1     Q12
#define qU2     Q9
#define qU3     Q13
#define qU4     Q10
#define qU5     Q14
#define qU6     Q11
#define qU7     Q15


#define dVr0    D24
#define dVi0    D25
#define dVr2    D26
#define dVi2    D27
#define dVr4    D28
#define dVi4    D29
#define dVr6    D30
#define dVi6    D31
#define dVr1    D16
#define dVi1    D17
#define dVr3    D18
#define dVi3    D19
#define dVr5    D20
#define dVi5    D21
#define dVr7    D22
#define dVi7    D23
#define qV0     Q12
#define qV1     Q8
#define qV2     Q13
#define qV3     Q9
#define qV4     Q14
#define qV5     Q10
#define qV6     Q15
#define qV7     Q11

#define dYr0    D16
#define dYi0    D17
#define dYr2    D18
#define dYi2    D19
#define dYr4    D20
#define dYi4    D21
#define dYr6    D22
#define dYi6    D23
#define dYr1    D24
#define dYi1    D25
#define dYr3    D26
#define dYi3    D27
#define dYr5    D28
#define dYi5    D29
#define dYr7    D30
#define dYi7    D31
#define qY0     Q8
#define qY1     Q12
#define qY2     Q9
#define qY3     Q13
#define qY4     Q10
#define qY5     Q14
#define qY6     Q11
#define qY7     Q15

#define dT0     D14
#define dT1     D15


        .MACRO FFTSTAGE scaled, inverse, name

        @// Define stack arguments

        @// Update pSubFFTSize and pSubFFTNum regs
        @// subFFTSize = 1 for the first stage
        MOV     subFFTSize,#8
        ADR     t0,ONEBYSQRT2\name

        @// Note: setCount = subFFTNum/8 (reuse the grpSize reg for setCount)
        LSR     grpSize,subFFTNum,#3
        MOV     subFFTNum,grpSize


        @// pT0+1 increments pT0 by 8 bytes
        @// pT0+pointStep = increment of 8*pointStep bytes = grpSize bytes
        @// Note: outPointStep = pointStep for firststage

        MOV     pointStep,grpSize,LSL #3


        @// Calculate the step of input data for the next set
        @//MOV     step1,pointStep,LSL #1             @// step1 = 2*pointStep
        VLD2.F32    {dXr0,dXi0},[pSrc, :128],pointStep     @//  data[0]
        MOV     step1,grpSize,LSL #4

        MOV     step2,pointStep,LSL #3
        VLD2.F32    {dXr1,dXi1},[pSrc, :128],pointStep     @//  data[1]
        SUB     step2,step2,pointStep                 @// step2 = 7*pointStep
        @// setStep = - 7*pointStep+16
        RSB     setStep,step2,#16

        VLD2.F32    {dXr2,dXi2},[pSrc, :128],pointStep     @//  data[2]
        VLD2.F32    {dXr3,dXi3},[pSrc, :128],pointStep     @//  data[3]
        VLD2.F32    {dXr4,dXi4},[pSrc, :128],pointStep     @//  data[4]
        VLD2.F32    {dXr5,dXi5},[pSrc, :128],pointStep     @//  data[5]
        VLD2.F32    {dXr6,dXi6},[pSrc, :128],pointStep     @//  data[6]
        @//  data[7] & update pSrc for the next set
        @//  setStep = -7*pointStep + 16
        VLD2.F32    {dXr7,dXi7},[pSrc, :128],setStep
        @// grp = 0 a special case since all the twiddle factors are 1
        @// Loop on the sets

radix8fsGrpZeroSetLoop\name :

        @// Decrement setcount
        SUBS    setCount,setCount,#2


        @// finish first stage of 8 point FFT

        VADD.F32    qU0,qX0,qX4
        VADD.F32    qU2,qX1,qX5
        VADD.F32    qU4,qX2,qX6
        VADD.F32    qU6,qX3,qX7

        @// finish second stage of 8 point FFT

        VADD.F32    qV0,qU0,qU4
        VSUB.F32    qV2,qU0,qU4
        VADD.F32    qV4,qU2,qU6
        VSUB.F32    qV6,qU2,qU6

        @// finish third stage of 8 point FFT

        VADD.F32    qY0,qV0,qV4
        VSUB.F32    qY4,qV0,qV4
        VST2.F32    {dYr0,dYi0},[pDst, :128],step1         @// store y0

        .ifeqs  "\inverse", "TRUE"

            VSUB.F32    dYr2,dVr2,dVi6
            VADD.F32    dYi2,dVi2,dVr6

            VADD.F32    dYr6,dVr2,dVi6
            VST2.F32    {dYr2,dYi2},[pDst, :128],step1     @// store y2
            VSUB.F32    dYi6,dVi2,dVr6

            VSUB.F32    qU1,qX0,qX4
            VST2.F32    {dYr4,dYi4},[pDst, :128],step1     @// store y4

            VSUB.F32    qU3,qX1,qX5
            VSUB.F32    qU5,qX2,qX6
            VST2.F32    {dYr6,dYi6},[pDst, :128],step1     @// store y6

        .ELSE

            VADD.F32    dYr6,dVr2,dVi6
            VSUB.F32    dYi6,dVi2,dVr6

            VSUB.F32    dYr2,dVr2,dVi6
            VST2.F32    {dYr6,dYi6},[pDst, :128],step1     @// store y2
            VADD.F32    dYi2,dVi2,dVr6


            VSUB.F32    qU1,qX0,qX4
            VST2.F32    {dYr4,dYi4},[pDst, :128],step1     @// store y4
            VSUB.F32    qU3,qX1,qX5
            VSUB.F32    qU5,qX2,qX6
            VST2.F32    {dYr2,dYi2},[pDst, :128],step1     @// store y6


        .ENDIF

        @// finish first stage of 8 point FFT

        VSUB.F32    qU7,qX3,qX7
        VLD1.F32    dT0[0], [t0]

        @// finish second stage of 8 point FFT

        VSUB.F32    dVr1,dUr1,dUi5
        @//  data[0] for next iteration
        VLD2.F32    {dXr0,dXi0},[pSrc, :128],pointStep
        VADD.F32    dVi1,dUi1,dUr5
        VADD.F32    dVr3,dUr1,dUi5
        VLD2.F32    {dXr1,dXi1},[pSrc, :128],pointStep     @//  data[1]
        VSUB.F32    dVi3,dUi1,dUr5

        VSUB.F32    dVr5,dUr3,dUi7
        VLD2.F32    {dXr2,dXi2},[pSrc, :128],pointStep     @//  data[2]
        VADD.F32    dVi5,dUi3,dUr7
        VADD.F32    dVr7,dUr3,dUi7
        VLD2.F32    {dXr3,dXi3},[pSrc, :128],pointStep     @//  data[3]
        VSUB.F32    dVi7,dUi3,dUr7

        @// finish third stage of 8 point FFT

        .ifeqs  "\inverse", "TRUE"

            @// calculate a*v5
            VMUL.F32    dT1,dVr5,dT0[0]                   @// use dVi0 for dT1

            VLD2.F32    {dXr4,dXi4},[pSrc, :128],pointStep @//  data[4]
            VMUL.F32    dVi5,dVi5,dT0[0]

            VLD2.F32    {dXr5,dXi5},[pSrc, :128],pointStep @//  data[5]
            VSUB.F32    dVr5,dT1,dVi5                     @// a * V5
            VADD.F32    dVi5,dT1,dVi5

            VLD2.F32    {dXr6,dXi6},[pSrc, :128],pointStep @//  data[6]

            @// calculate  b*v7
            VMUL.F32    dT1,dVr7,dT0[0]
            VMUL.F32    dVi7,dVi7,dT0[0]

            VADD.F32    qY1,qV1,qV5
            VSUB.F32    qY5,qV1,qV5


            VADD.F32    dVr7,dT1,dVi7                     @// b * V7
            VSUB.F32    dVi7,dVi7,dT1
            SUB     pDst, pDst, step2                 @// set pDst to y1

            @// On the last iteration,  this will read past the end of pSrc, 
            @// so skip this read.
            BEQ     radix8SkipLastUpdateInv\name
            VLD2.F32    {dXr7,dXi7},[pSrc, :128],setStep   @//  data[7]
radix8SkipLastUpdateInv\name:

            VSUB.F32    dYr3,dVr3,dVr7
            VSUB.F32    dYi3,dVi3,dVi7
            VST2.F32    {dYr1,dYi1},[pDst, :128],step1     @// store y1
            VADD.F32    dYr7,dVr3,dVr7
            VADD.F32    dYi7,dVi3,dVi7


            VST2.F32    {dYr3,dYi3},[pDst, :128],step1     @// store y3
            VST2.F32    {dYr5,dYi5},[pDst, :128],step1     @// store y5
            VST2.F32    {dYr7,dYi7},[pDst, :128]           @// store y7
            ADD pDst, pDst, #16

        .ELSE

            @// calculate  b*v7
            VMUL.F32    dT1,dVr7,dT0[0]
            VLD2.F32    {dXr4,dXi4},[pSrc, :128],pointStep @//  data[4]
            VMUL.F32    dVi7,dVi7,dT0[0]

            VLD2.F32    {dXr5,dXi5},[pSrc, :128],pointStep @//  data[5]
            VADD.F32    dVr7,dT1,dVi7                     @// b * V7
            VSUB.F32    dVi7,dVi7,dT1

            VLD2.F32    {dXr6,dXi6},[pSrc, :128],pointStep @//  data[6]

            @// calculate a*v5
            VMUL.F32    dT1,dVr5,dT0[0]                   @// use dVi0 for dT1
            VMUL.F32    dVi5,dVi5,dT0[0]

            VADD.F32    dYr7,dVr3,dVr7
            VADD.F32    dYi7,dVi3,dVi7
            SUB     pDst, pDst, step2                 @// set pDst to y1

            VSUB.F32    dVr5,dT1,dVi5                     @// a * V5
            VADD.F32    dVi5,dT1,dVi5

            @// On the last iteration,  this will read past the end of pSrc, 
            @// so skip this read.
            BEQ     radix8SkipLastUpdateFwd\name
            VLD2.F32    {dXr7,dXi7},[pSrc, :128],setStep   @//  data[7]
radix8SkipLastUpdateFwd\name:

            VSUB.F32    qY5,qV1,qV5

            VSUB.F32    dYr3,dVr3,dVr7
            VST2.F32    {dYr7,dYi7},[pDst, :128],step1     @// store y1
            VSUB.F32    dYi3,dVi3,dVi7
            VADD.F32    qY1,qV1,qV5


            VST2.F32    {dYr5,dYi5},[pDst, :128],step1     @// store y3
            VST2.F32    {dYr3,dYi3},[pDst, :128],step1     @// store y5
            VST2.F32    {dYr1,dYi1},[pDst, :128]!          @// store y7

        .ENDIF


        @// update pDst for the next set
        SUB     pDst, pDst, step2
        BGT     radix8fsGrpZeroSetLoop\name


        @// reset pSrc to pDst for the next stage
        SUB     pSrc,pDst,pointStep                   @// pDst -= 2*grpSize
        MOV     pDst,pPingPongBuf



        .endm


        @// Allocate stack memory required by the function


        M_START armSP_FFTFwd_CToC_FC32_Radix8_fs_OutOfPlace_unsafe,r4
            FFTSTAGE "FALSE","FALSE",FWD
        M_END
ONEBYSQRT2FWD:     .float  0.7071067811865476e0

        M_START armSP_FFTInv_CToC_FC32_Radix8_fs_OutOfPlace_unsafe,r4
            FFTSTAGE "FALSE","TRUE",INV
        M_END
ONEBYSQRT2INV:     .float  0.7071067811865476e0


        .end
