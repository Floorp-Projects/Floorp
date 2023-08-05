@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This is a modification of
@//  armSP_FFTInv_CCSToR_S32_preTwiddleRadix2_unsafe_s.s to support float
@//  instead of SC32.
@//

@//
@// Description:
@// Compute the "preTwiddleRadix2" stage prior to the call to the complexFFT
@// It does a Z(k) = Feven(k) + jW^(-k) FOdd(k); k=0,1,2,...N/2-1 computation
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
#define pDst            r1
#define pFFTSpec        r2
#define scale           r3


@// Output registers
#define result          r0

@//Local Scratch Registers

#define argTwiddle      r1
#define argDst          r2
#define argScale        r4
#define tmpOrder        r4
#define pTwiddle        r4
#define pOut            r5
#define subFFTSize      r7
#define subFFTNum       r6
#define N               r6
#define order           r14
#define diff            r9
@// Total num of radix stages required to complete the FFT
#define count           r8
#define x0r             r4
#define x0i             r5
#define diffMinusOne    r2
#define round           r3

#define pOut1           r2
#define size            r7
#define step            r8
#define step1           r9
#define twStep          r10
#define pTwiddleTmp     r11
#define argTwiddle1     r12
#define zero            r14

@// Neon registers

#define dX0     D0
#define dShift  D1
#define dX1     D1
#define dY0     D2
#define dY1     D3
#define dX0r    D0
#define dX0i    D1
#define dX1r    D2
#define dX1i    D3
#define dW0r    D4
#define dW0i    D5
#define dW1r    D6
#define dW1i    D7
#define dT0     D8
#define dT1     D9
#define dT2     D10
#define dT3     D11
#define qT0     D12
#define qT1     D14
#define qT2     D16
#define qT3     D18
#define dY0r    D4
#define dY0i    D5
#define dY1r    D6
#define dY1i    D7

#define dY2     D4
#define dY3     D5
#define dW0     D6
#define dW1     D7
#define dW0Tmp  D10
#define dW1Neg  D11

#define half    D13

@ Structure offsets for the FFTSpec
        .set    ARMsFFTSpec_N, 0
        .set    ARMsFFTSpec_pBitRev, 4
        .set    ARMsFFTSpec_pTwiddle, 8
        .set    ARMsFFTSpec_pBuf, 12

        .MACRO FFTSTAGE scaled, inverse, name

        @// Read the size from structure and take log
        LDR     N, [pFFTSpec, #ARMsFFTSpec_N]

        @// Read other structure parameters
        LDR     pTwiddle, [pFFTSpec, #ARMsFFTSpec_pTwiddle]
        LDR     pOut, [pFFTSpec, #ARMsFFTSpec_pBuf]

        VMOV.F32    half, #0.5


        MOV     size,N,ASR #1                 @// preserve the contents of N
        MOV     step,N,LSL #2                 @// step = N/2 * 8 bytes


        @// Z(k) = 1/2 {[F(k) +  F'(N/2-k)] +j*W^(-k) [F(k) -  F'(N/2-k)]}
        @// Note: W^(k) is stored as negated value and also need to
        @// conjugate the values from the table

        @// Z(0) : no need of twiddle multiply
        @// Z(0) = 1/2 { [F(0) +  F'(N/2)] +j [F(0) -  F'(N/2)] }

        VLD1.F32    dX0,[pSrc],step
        ADD     pOut1,pOut,step               @// pOut1 = pOut+ N/2*8 bytes

        VLD1.F32    dX1,[pSrc]!
        @// twStep = 3N/8 * 8 bytes pointing to W^1
        SUB     twStep,step,size,LSL #1

        MOV     step1,size,LSL #2             @// step1 = N/4 * 8 = N/2*4 bytes
        SUB     step1,step1,#8                @// (N/4-1)*8 bytes

        VADD.F32    dY0,dX0,dX1                   @// [b+d | a+c]
        VSUB.F32    dY1,dX0,dX1                   @// [b-d | a-c]
        VMUL.F32    dY0, dY0, half[0]
        VMUL.F32    dY1, dY1, half[0]

        @// dY0= [a-c | a+c] ;dY1= [b-d | b+d]
        VZIP.F32    dY0,dY1

        VSUB.F32   dX0,dY0,dY1
        SUBS   size,size,#2
        VADD.F32   dX1,dY0,dY1

        SUB     pSrc,pSrc,step

        VST1.F32    dX0[0],[pOut1]!
        ADD     pTwiddleTmp,pTwiddle,#8       @// W^2
        VST1.F32    dX1[1],[pOut1]!
        ADD     argTwiddle1,pTwiddle,twStep   @// W^1


        BLT     decrementScale\name
        BEQ     lastElement\name


        @// Z(k) = 1/2[F(k) +  F'(N/2-k)] +j*W^(-k) [F(k) -  F'(N/2-k)]
        @// Note: W^k is stored as negative values in the table and also
        @// need to conjugate the values from the table.
        @//
        @// Process 4 elements at a time. E.g: Z(1),Z(2) and Z(N/2-2),Z(N/2-1)
        @// since both of them require F(1),F(2) and F(N/2-2),F(N/2-1)


        SUB     step,step,#24
evenOddButterflyLoop\name :


        VLD1.F32    dW0r,[argTwiddle1],step1
        VLD1.F32    dW1r,[argTwiddle1]!

        VLD2.F32    {dX0r,dX0i},[pSrc],step
        SUB     argTwiddle1,argTwiddle1,step1
        VLD2.F32    {dX1r,dX1i},[pSrc]!

        SUB     step1,step1,#8                @// (N/4-2)*8 bytes
        VLD1.F32    dW0i,[pTwiddleTmp],step1
        VLD1.F32    dW1i,[pTwiddleTmp]!
        SUB     pSrc,pSrc,step

        SUB     pTwiddleTmp,pTwiddleTmp,step1
        VREV64.F32  dX1r,dX1r
        VREV64.F32  dX1i,dX1i
        SUBS    size,size,#4


        VSUB.F32    dT2,dX0r,dX1r                 @// a-c
        VADD.F32    dT3,dX0i,dX1i                 @// b+d
        VADD.F32    dT0,dX0r,dX1r                 @// a+c
        VSUB.F32    dT1,dX0i,dX1i                 @// b-d
        SUB     step1,step1,#8

        VMUL.F32    dT2, dT2, half[0]
        VMUL.F32    dT3, dT3, half[0]

        VMUL.F32    dT0, dT0, half[0]
        VMUL.F32    dT1, dT1, half[0]

        VZIP.F32    dW1r,dW1i
        VZIP.F32    dW0r,dW0i


        VMUL.F32   dX1r,dW1r,dT2
        VMUL.F32   dX1i,dW1r,dT3
        VMUL.F32   dX0r,dW0r,dT2
        VMUL.F32   dX0i,dW0r,dT3

        VMLS.F32   dX1r,dW1i,dT3
        VMLA.F32   dX1i,dW1i,dT2

        VMLA.F32   dX0r,dW0i,dT3
        VMLS.F32   dX0i,dW0i,dT2


        VADD.F32    dY1r,dT0,dX1i                 @// F(N/2 -1)
        VSUB.F32    dY1i,dX1r,dT1

        VREV64.F32  dY1r,dY1r
        VREV64.F32  dY1i,dY1i


        VADD.F32    dY0r,dT0,dX0i                 @// F(1)
        VSUB.F32    dY0i,dT1,dX0r


        VST2.F32    {dY0r,dY0i},[pOut1],step
        VST2.F32    {dY1r,dY1i},[pOut1]!
        SUB     pOut1,pOut1,step
        SUB     step,step,#32                 @// (N/2-4)*8 bytes


        BGT     evenOddButterflyLoop\name


        @// set both the ptrs to the last element
        SUB     pSrc,pSrc,#8
        SUB     pOut1,pOut1,#8

        @// Last element can be expanded as follows
        @// 1/2[Z(k) + Z'(k)] - j w^-k [Z(k) - Z'(k)] (since W^k is stored as
        @// -ve)
        @// 1/2[(a+jb) + (a-jb)] - j w^-k [(a+jb) - (a-jb)]
        @// 1/2[2a+j0] - j (c-jd) [0+j2b]
        @// (a+bc, -bd)
        @// Since (c,d) = (0,1) for the last element, result is just (a,-b)

lastElement\name :
        VLD1.F32    dX0r,[pSrc]

        VST1.F32    dX0r[0],[pOut1]!
        VNEG.F32    dX0r,dX0r
        VST1.F32    dX0r[1],[pOut1]



decrementScale\name :

        .endm

        M_START armSP_FFTInv_CCSToR_F32_preTwiddleRadix2_unsafe,r4

            FFTSTAGE "FALSE","TRUE",Inv
        M_END

        .end
