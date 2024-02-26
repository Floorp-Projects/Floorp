@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This is a modification of omxSP_FFTFwd_RToCCS_S32_Sfs_s.s
@//  to support float instead of SC32.
@//

@//
@// Description:
@// Compute FFT for a real signal
@//
@//


@// Include standard headers

#include "dl/api/armCOMM_s.h"
#include "dl/api/omxtypes_s.h"


@// Import symbols required from other files
@// (For example tables)

        .extern  armSP_FFTFwd_CToC_FC32_Radix2_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_FC32_Radix4_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_FC32_Radix8_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_FC32_Radix4_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_FC32_Radix4_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_FC32_Radix8_fs_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_FC32_Radix2_OutOfPlace_unsafe

@// Set debugging level
@//DEBUG_ON    SETL {TRUE}



@// Guarding implementation by the processor name



    @// Guarding implementation by the processor name

@// Import symbols required from other files
@// (For example tables)
        .extern  armSP_FFTFwd_CToC_FC32_Radix4_ls_OutOfPlace_unsafe
        .extern  armSP_FFTFwd_CToC_FC32_Radix2_ls_OutOfPlace_unsafe


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
@// Total num of radix stages required to comple the FFT
#define count           r8
#define x0r             r4
#define x0i             r5
#define diffMinusOne    r2
#define subFFTSizeTmp   r6
#define step            r3
#define step1           r4
#define twStep          r8
#define zero            r9
#define pTwiddleTmp     r5
#define t0              r10

@// Neon registers

#define dX0       d0
#define dzero     d1
#define dZero     d2
#define dShift    d3
#define dX0r      d2
#define dX0i      d3
#define dX1r      d4
#define dX1i      d5
#define dT0       d6
#define dT1       d7
#define dT2       d8
#define dT3       d9
#define qT0       d10
#define qT1       d12
#define dW0r      d14
#define dW0i      d15
#define dW1r      d16
#define dW1i      d17
#define dY0r      d14
#define dY0i      d15
#define dY1r      d16
#define dY1i      d17
#define dY0rS64   d14.s64
#define dY0iS64   d15.s64
#define qT2       d18
#define qT3       d20
@// lastThreeelements
#define dX1       d3
#define dW0       d4
#define dW1       d5
#define dY0       d10
#define dY1       d11
#define dY2       d12
#define dY3       d13

#define half      d0

    @// Allocate stack memory required by the function

    @// Write function header
        M_START     omxSP_FFTFwd_RToCCS_F32_Sfs,r11,d15

@ Structure offsets for the FFTSpec
        .set    ARMsFFTSpec_N, 0
        .set    ARMsFFTSpec_pBitRev, 4
        .set    ARMsFFTSpec_pTwiddle, 8
        .set    ARMsFFTSpec_pBuf, 12

        @// Define stack arguments

        @// Read the size from structure and take log
        LDR     N, [pFFTSpec, #ARMsFFTSpec_N]

        @// Read other structure parameters
        LDR     pTwiddle, [pFFTSpec, #ARMsFFTSpec_pTwiddle]
        LDR     pOut, [pFFTSpec, #ARMsFFTSpec_pBuf]

        @//  N=1 Treat seperately
        CMP     N,#1
        BGT     sizeGreaterThanOne
        VLD1.F32    dX0[0],[pSrc]
        MOV     zero,#0
        VMOV.F32    dzero[0],zero
        VMOV.F32    dZero[0],zero
        VST3.F32    {dX0[0],dzero[0],dZero[0]},[pDst]

        B       End



sizeGreaterThanOne:
        @// Do a N/2 point complex FFT including the scaling

        MOV     N,N,ASR #1                          @// N/2 point complex FFT

        CLZ     order,N                             @// N = 2^order
        RSB     order,order,#31
        MOV     subFFTSize,#1
        @//MOV     subFFTNum,N

        CMP     order,#3
        BGT     orderGreaterthan3                   @// order > 3

        CMP     order,#1
        BGE     orderGreaterthan0                   @// order > 0
        VLD1.F32    dX0,[pSrc]
        VST1.F32    dX0,[pOut]
        MOV     pSrc,pOut
        MOV     argDst,pDst
        BLT     FFTEnd

orderGreaterthan0:
        @// set the buffers appropriately for various orders
        CMP     order,#2
        MOVEQ   argDst,pDst
        MOVNE   argDst,pOut
        @// Pass the first stage destination in RN5
        MOVNE   pOut,pDst
        MOV     argTwiddle,pTwiddle

        CMP     order,#1
        BGT     orderGreaterthan1
        @// order = 1
        BL      armSP_FFTFwd_CToC_FC32_Radix2_fs_OutOfPlace_unsafe
        B       FFTEnd

orderGreaterthan1:
        CMP     order,#2
        BGT     orderGreaterthan2
        @// order =2
        BL      armSP_FFTFwd_CToC_FC32_Radix2_fs_OutOfPlace_unsafe
        BL      armSP_FFTFwd_CToC_FC32_Radix2_ls_OutOfPlace_unsafe
        B       FFTEnd

orderGreaterthan2:@// order =3
        BL      armSP_FFTFwd_CToC_FC32_Radix2_fs_OutOfPlace_unsafe
        BL      armSP_FFTFwd_CToC_FC32_Radix2_OutOfPlace_unsafe
        BL      armSP_FFTFwd_CToC_FC32_Radix2_ls_OutOfPlace_unsafe

        B       FFTEnd



orderGreaterthan3:
specialScaleCase:

        @// Set input args to fft stages
        TST     order, #2
        MOVEQ   argDst,pDst
        MOVNE   argDst,pOut
        @// Pass the first stage destination in RN5
        MOVNE   pOut,pDst
        MOV     argTwiddle,pTwiddle

        @//check for even or odd order
        @// NOTE: The following combination of BL's would work fine even though
        @// the first BL would corrupt the flags. This is because the end of
        @// the "grpZeroSetLoop" loop inside
        @// armSP_FFTFwd_CToC_FC32_Radix4_fs_OutOfPlace_unsafe sets the Z flag
        @// to EQ

        TST     order,#0x00000001
        BLEQ    armSP_FFTFwd_CToC_FC32_Radix4_fs_OutOfPlace_unsafe
        BLNE    armSP_FFTFwd_CToC_FC32_Radix8_fs_OutOfPlace_unsafe

        CMP        subFFTNum,#4
        BLT     FFTEnd


unscaledRadix4Loop:
        BEQ        lastStageUnscaledRadix4
         BL        armSP_FFTFwd_CToC_FC32_Radix4_OutOfPlace_unsafe
         CMP        subFFTNum,#4
         B        unscaledRadix4Loop

lastStageUnscaledRadix4:
        BL      armSP_FFTFwd_CToC_FC32_Radix4_ls_OutOfPlace_unsafe
        B        FFTEnd


FFTEnd:
finalComplexToRealFixup:


        @// F(0) = 1/2[Z(0) + Z'(0)] - j [Z(0) - Z'(0)]
        @// 1/2[(a+jb) + (a-jb)] - j  [(a+jb) - (a-jb)]
        @// 1/2[2a+j0] - j [0+j2b]
        @// (a+b, 0)

        @// F(N/2) = 1/2[Z(0) + Z'(0)] + j [Z(0) - Z'(0)]
        @// 1/2[(a+jb) + (a-jb)] + j  [(a+jb) - (a-jb)]
        @// 1/2[2a+j0] + j [0+j2b]
        @// (a-b, 0)

        @// F(0) and F(N/2)
        VLD2.F32    {dX0r[0],dX0i[0]},[pSrc]!
        MOV     zero,#0
        VMOV.F32    dX0r[1],zero
        MOV     step,subFFTSize,LSL #3            @// step = N/2 * 8 bytes
        VMOV.F32    dX0i[1],zero
        @// twStep = 3N/8 * 8 bytes pointing to W^1
        SUB     twStep,step,subFFTSize,LSL #1

        VADD.F32    dY0r,dX0r,dX0i                    @// F(0) = ((Z0.r+Z0.i) , 0)
        MOV     step1,subFFTSize,LSL #2           @// step1 = N/2 * 4 bytes
        VSUB.F32    dY0i,dX0r,dX0i                    @// F(N/2) = ((Z0.r-Z0.i) , 0)
        SUBS    subFFTSize,subFFTSize,#2

        VST1.F32    dY0r,[argDst],step
        ADD     pTwiddleTmp,argTwiddle,#8         @// W^2
        VST1.F32    dY0i,[argDst]!
        ADD     argTwiddle,argTwiddle,twStep      @// W^1

        VDUP.F32    dzero,zero
        SUB     argDst,argDst,step

        BLT     End
        BEQ     lastElement
        SUB     step,step,#24
        SUB     step1,step1,#8                    @// (N/4-1)*8 bytes

        @// F(k) = 1/2[Z(k) +  Z'(N/2-k)] -j*W^(k) [Z(k) -  Z'(N/2-k)]
        @// Note: W^k is stored as negative values in the table
        @// Process 4 elements at a time. E.g: F(1),F(2) and F(N/2-2),F(N/2-1)
        @// since both of them require Z(1),Z(2) and Z(N/2-2),Z(N/2-1)


        ADR     t0, HALF
        VLD1.F32    half[0], [t0]

evenOddButterflyLoop:


        VLD1.F32    dW0r,[argTwiddle],step1
        VLD1.F32    dW1r,[argTwiddle]!

        VLD2.F32    {dX0r,dX0i},[pSrc],step
        SUB     argTwiddle,argTwiddle,step1
        VLD2.F32    {dX1r,dX1i},[pSrc]!



        SUB     step1,step1,#8                    @// (N/4-2)*8 bytes
        VLD1.F32    dW0i,[pTwiddleTmp],step1
        VLD1.F32    dW1i,[pTwiddleTmp]!
        SUB     pSrc,pSrc,step

        SUB     pTwiddleTmp,pTwiddleTmp,step1
        VREV64.F32  dX1r,dX1r
        VREV64.F32  dX1i,dX1i
        SUBS    subFFTSize,subFFTSize,#4



        VSUB.F32    dT2,dX0r,dX1r                     @// a-c
        SUB     step1,step1,#8
        VADD.F32    dT0,dX0r,dX1r                     @// a+c
        VSUB.F32    dT1,dX0i,dX1i                     @// b-d
        VADD.F32    dT3,dX0i,dX1i                     @// b+d
        VMUL.F32   dT0,dT0,half[0]
        VMUL.F32   dT1,dT1,half[0]
        VZIP.F32    dW1r,dW1i
        VZIP.F32    dW0r,dW0i


        VMUL.F32   qT0,dW1r,dT2
        VMUL.F32   qT1,dW1r,dT3
        VMUL.F32   qT2,dW0r,dT2
        VMUL.F32   qT3,dW0r,dT3

        VMLA.F32   qT0,dW1i,dT3
        VMLS.F32   qT1,dW1i,dT2

        VMLS.F32   qT2,dW0i,dT3
        VMLA.F32   qT3,dW0i,dT2


        VMUL.F32  dX1r,qT0,half[0]
        VMUL.F32  dX1i,qT1,half[0]

        VSUB.F32    dY1r,dT0,dX1i                     @// F(N/2 -1)
        VADD.F32    dY1i,dT1,dX1r
        VNEG.F32    dY1i,dY1i

        VREV64.F32  dY1r,dY1r
        VREV64.F32  dY1i,dY1i


        VMUL.F32  dX0r,qT2,half[0]
        VMUL.F32  dX0i,qT3,half[0]

        VSUB.F32    dY0r,dT0,dX0i                     @// F(1)
        VADD.F32    dY0i,dT1,dX0r


        VST2.F32    {dY0r,dY0i},[argDst],step
        VST2.F32    {dY1r,dY1i},[argDst]!
        SUB     argDst,argDst,step
        SUB     step,step,#32                     @// (N/2-4)*8 bytes


        BGT     evenOddButterflyLoop

        @// set both the ptrs to the last element
        SUB     pSrc,pSrc,#8
        SUB     argDst,argDst,#8



        @// Last element can be expanded as follows
        @// 1/2[Z(k) + Z'(k)] + j w^k [Z(k) - Z'(k)]
        @// 1/2[(a+jb) + (a-jb)] + j w^k [(a+jb) - (a-jb)]
        @// 1/2[2a+j0] + j (c+jd) [0+j2b]
        @// (a-bc, -bd)
        @// Since (c,d) = (0,1) for the last element, result is just (a,-b)

lastElement:
        VLD1.F32    dX0r,[pSrc]

        VST1.F32    dX0r[0],[argDst]!
        VNEG.F32    dX0r,dX0r
        VST1.F32    dX0r[1],[argDst]!

End:
        @// Set return value
        MOV     result, #OMX_Sts_NoErr

        @// Write function tail
        M_END
HALF:   .float  0.5
        .end
