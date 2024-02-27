@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This is a modification of armSP_FFT_CToC_SC32_Radix2_unsafe_s.s
@//  to support float instead of SC32.
@//

@// Description:
@// Compute a Radix 2 DIT in-order out-of-place FFT stage for an N point
@// complex signal.  This handles the general stage, not the first or last
@// stage.
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


@//Output Registers


@//Local Scratch Registers

#define outPointStep    r3
#define pointStep       r4
#define grpCount        r5
#define setCount        r8
@//const           RN  9
#define step            r10
#define dstStep         r11
#define pTable          r9
#define pTmp            r9

@// Neon Registers

#define dW      D0
#define dX0     D2
#define dX1     D3
#define dX2     D4
#define dX3     D5
#define dY0     D6
#define dY1     D7
#define dY2     D8
#define dY3     D9
#define qT0     D10
#define qT1     D11


        .MACRO FFTSTAGE scaled, inverse, name

        @// Define stack arguments


        @// Update grpCount and grpSize rightaway inorder to reuse pGrpCount
        @// and pGrpSize regs

        LSR     subFFTNum,subFFTNum,#1                      @//grpSize
        LSL     grpCount,subFFTSize,#1


        @// pT0+1 increments pT0 by 8 bytes
        @// pT0+pointStep = increment of 8*pointStep bytes = 4*grpSize bytes
        MOV     pointStep,subFFTNum,LSL #2

        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount

        @// pOut0+1 increments pOut0 by 8 bytes
        @// pOut0+outPointStep == increment of 8*outPointStep bytes =
        @//    4*size bytes
        SMULBB  outPointStep,grpCount,pointStep
        LSL     pointStep,pointStep,#1


        RSB      step,pointStep,#16
        RSB      dstStep,outPointStep,#16

        @// Loop on the groups

radix2GrpLoop\name :
        MOV      setCount,pointStep,LSR #3
        VLD1.F32     dW,[pTwiddle],pointStep                @//[wi | wr]


        @// Loop on the sets


radix2SetLoop\name :


        @// point0: dX0-real part dX1-img part
        VLD2.F32    {dX0,dX1},[pSrc],pointStep
        @// point1: dX2-real part dX3-img part
        VLD2.F32    {dX2,dX3},[pSrc],step

        SUBS    setCount,setCount,#2

        .ifeqs  "\inverse", "TRUE"
            VMUL.F32   qT0,dX2,dW[0]
            VMLA.F32   qT0,dX3,dW[1]                       @// real part
            VMUL.F32   qT1,dX3,dW[0]
            VMLS.F32   qT1,dX2,dW[1]                       @// imag part

        .else

            VMUL.F32   qT0,dX2,dW[0]
            VMLS.F32   qT0,dX3,dW[1]                       @// real part
            VMUL.F32   qT1,dX3,dW[0]
            VMLA.F32   qT1,dX2,dW[1]                       @// imag part

        .endif

        VSUB.F32    dY0,dX0,qT0
        VSUB.F32    dY1,dX1,qT1
        VADD.F32    dY2,dX0,qT0
        VADD.F32    dY3,dX1,qT1

        VST2.F32    {dY0,dY1},[pDst],outPointStep
        @// dstStep = -outPointStep + 16
        VST2.F32    {dY2,dY3},[pDst],dstStep

        BGT     radix2SetLoop\name

        SUBS    grpCount,grpCount,#2
        ADD     pSrc,pSrc,pointStep
        BGT     radix2GrpLoop\name


        @// Reset and Swap pSrc and pDst for the next stage
        MOV     pTmp,pDst
        @// pDst -= 4*size; pSrc -= 8*size bytes
        SUB     pDst,pSrc,outPointStep,LSL #1
        SUB     pSrc,pTmp,outPointStep

        @// Reset pTwiddle for the next stage
        @// pTwiddle -= 4*size bytes
        SUB     pTwiddle,pTwiddle,outPointStep


        .endm



        M_START armSP_FFTFwd_CToC_FC32_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",FWD
        M_END



        M_START armSP_FFTInv_CToC_FC32_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",INV
        M_END


        .end
