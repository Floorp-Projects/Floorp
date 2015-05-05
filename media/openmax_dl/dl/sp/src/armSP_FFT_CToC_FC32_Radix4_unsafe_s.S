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
@//  This is a modification of armSP_FFT_CToC_SC32_Radix4_unsafe_s.s
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




@// Guarding implementation by the processor name


@// Import symbols required from other files
@// (For example tables)


@//Input Registers

#define pSrc            r0
#define pDst            r2
#define pTwiddle        r1
#define subFFTNum       r6
#define subFFTSize      r7



@//Output Registers


@//Local Scratch Registers

#define grpCount        r3
#define pointStep       r4
#define outPointStep    r5
#define stepTwiddle     r12
#define setCount        r14
#define srcStep         r8
#define setStep         r9
#define dstStep         r10
#define twStep          r11
#define t1              r3

@// Neon Registers

#define dW1     D0.F32
#define dW2     D1.F32
#define dW3     D2.F32

#define dXr0    D4.F32
#define dXi0    D5.F32
#define dXr1    D6.F32
#define dXi1    D7.F32
#define dXr2    D8.F32
#define dXi2    D9.F32
#define dXr3    D10.F32
#define dXi3    D11.F32
#define dYr0    D12.F32
#define dYi0    D13.F32
#define dYr1    D14.F32
#define dYi1    D15.F32
#define dYr2    D16.F32
#define dYi2    D17.F32
#define dYr3    D18.F32
#define dYi3    D19.F32
#define qT0     d16.f32
#define qT1     d18.f32
#define qT2     d12.f32
#define qT3     d14.f32
#define dZr0    D20.F32
#define dZi0    D21.F32
#define dZr1    D22.F32
#define dZi1    D23.F32
#define dZr2    D24.F32
#define dZi2    D25.F32
#define dZr3    D26.F32
#define dZi3    D27.F32

#define qY0     Q6.F32
#define qY1     Q7.F32
#define qY2     Q8.F32
#define qY3     Q9.F32
#define qX0     Q2.F32
#define qZ0     Q10.F32
#define qZ1     Q11.F32
#define qZ2     Q12.F32
#define qZ3     Q13.F32

        .MACRO FFTSTAGE scaled, inverse , name

        @// Define stack arguments


        @// Update grpCount and grpSize rightaway inorder to reuse
        @// pGrpCount and pGrpSize regs

        LSL     grpCount,subFFTSize,#2
        LSR     subFFTNum,subFFTNum,#2
        MOV     subFFTSize,grpCount

        VLD1     dW1,[pTwiddle]                    @//[wi | wr]
        @// pT0+1 increments pT0 by 8 bytes
        @// pT0+pointStep = increment of 8*pointStep bytes = 2*grpSize bytes
        MOV     pointStep,subFFTNum,LSL #1


        @// pOut0+1 increments pOut0 by 8 bytes
        @// pOut0+outPointStep == increment of 8*outPointStep bytes
        @//   = 2*size bytes

        MOV     stepTwiddle,#0
        VLD1     dW2,[pTwiddle]                    @//[wi | wr]
        SMULBB  outPointStep,grpCount,pointStep
        LSL     pointStep,pointStep,#2             @// 2*grpSize

        VLD1     dW3,[pTwiddle]                    @//[wi | wr]
        MOV     srcStep,pointStep,LSL #1           @// srcStep = 2*pointStep
        ADD     setStep,srcStep,pointStep          @// setStep = 3*pointStep

        RSB     setStep,setStep,#0                 @// setStep = - 3*pointStep
        SUB     srcStep,srcStep,#16                @// srcStep = 2*pointStep-16

        MOV     dstStep,outPointStep,LSL #1
        ADD     dstStep,dstStep,outPointStep       @// dstStep = 3*outPointStep
        @// dstStep = - 3*outPointStep+16
        RSB     dstStep,dstStep,#16



radix4GrpLoop\name :

        VLD2    {dXr0,dXi0},[pSrc],pointStep       @//  data[0]
        ADD      stepTwiddle,stepTwiddle,pointStep
        VLD2    {dXr1,dXi1},[pSrc],pointStep       @//  data[1]
        @// set pTwiddle to the first point
        ADD      pTwiddle,pTwiddle,stepTwiddle
        VLD2    {dXr2,dXi2},[pSrc],pointStep       @//  data[2]
        MOV      twStep,stepTwiddle,LSL #2

        @//  data[3] & update pSrc for the next set
        VLD2    {dXr3,dXi3},[pSrc],setStep
        SUB      twStep,stepTwiddle,twStep         @// twStep = -3*stepTwiddle

        MOV      setCount,pointStep,LSR #3
        @// set pSrc to data[0] of the next set
        ADD     pSrc,pSrc,#16
        @// increment to data[1] of the next set
        ADD     pSrc,pSrc,pointStep


        @// Loop on the sets

radix4SetLoop\name :



        .ifeqs  "\inverse", "TRUE"
            VMUL   dZr1,dXr1,dW1[0]
            VMUL   dZi1,dXi1,dW1[0]
            VMUL   dZr2,dXr2,dW2[0]
            VMUL   dZi2,dXi2,dW2[0]
            VMUL   dZr3,dXr3,dW3[0]
            VMUL   dZi3,dXi3,dW3[0]

            VMLA   dZr1,dXi1,dW1[1]                @// real part
            VMLS   dZi1,dXr1,dW1[1]                @// imag part

            @//  data[1] for next iteration
            VLD2    {dXr1,dXi1},[pSrc],pointStep

            VMLA   dZr2,dXi2,dW2[1]                @// real part
            VMLS   dZi2,dXr2,dW2[1]                @// imag part

            @//  data[2] for next iteration
            VLD2    {dXr2,dXi2},[pSrc],pointStep

            VMLA   dZr3,dXi3,dW3[1]                @// real part
            VMLS   dZi3,dXr3,dW3[1]                @// imag part
        .else
            VMUL   dZr1,dXr1,dW1[0]
            VMUL   dZi1,dXi1,dW1[0]
            VMUL   dZr2,dXr2,dW2[0]
            VMUL   dZi2,dXi2,dW2[0]
            VMUL   dZr3,dXr3,dW3[0]
            VMUL   dZi3,dXi3,dW3[0]

            VMLS   dZr1,dXi1,dW1[1]                @// real part
            VMLA   dZi1,dXr1,dW1[1]                @// imag part

            @//  data[1] for next iteration
            VLD2    {dXr1,dXi1},[pSrc],pointStep

            VMLS   dZr2,dXi2,dW2[1]                @// real part
            VMLA   dZi2,dXr2,dW2[1]                @// imag part

            @//  data[2] for next iteration
            VLD2    {dXr2,dXi2},[pSrc],pointStep

            VMLS   dZr3,dXi3,dW3[1]                @// real part
            VMLA   dZi3,dXr3,dW3[1]                @// imag part
        .endif

        @//  data[3] & update pSrc to data[0]
        @// But don't read on the very last iteration because that reads past 
	@// the end of pSrc. The last iteration is grpCount = 4, setCount = 2.
        cmp     grpCount, #4
        cmpeq   setCount, #2                      @// Test setCount if grpCount = 4
        @// These are executed only if both grpCount = 4 and setCount = 2       
        addeq   pSrc, pSrc, setStep
        beq     radix4SkipRead\name
        VLD2    {dXr3,dXi3},[pSrc],setStep
radix4SkipRead\name:
        SUBS    setCount,setCount,#2

        @// finish first stage of 4 point FFT
        VADD    qY0,qX0,qZ2
        VSUB    qY2,qX0,qZ2

        @//  data[0] for next iteration
        VLD2    {dXr0,dXi0},[pSrc :128]!
        VADD    qY1,qZ1,qZ3
        VSUB    qY3,qZ1,qZ3

        @// finish second stage of 4 point FFT

        VSUB    qZ0,qY2,qY1


        .ifeqs  "\inverse", "TRUE"

            VADD    dZr3,dYr0,dYi3
            VST2    {dZr0,dZi0},[pDst :128],outPointStep
            VSUB    dZi3,dYi0,dYr3

            VADD    qZ2,qY2,qY1
            VST2    {dZr3,dZi3},[pDst :128],outPointStep

            VSUB    dZr1,dYr0,dYi3
            VST2    {dZr2,dZi2},[pDst :128],outPointStep
            VADD    dZi1,dYi0,dYr3

            VST2    {dZr1,dZi1},[pDst :128],dstStep


        .else

            VSUB    dZr1,dYr0,dYi3
            VST2    {dZr0,dZi0},[pDst :128],outPointStep
            VADD    dZi1,dYi0,dYr3

            VADD    qZ2,qY2,qY1
            VST2    {dZr1,dZi1},[pDst :128],outPointStep

            VADD    dZr3,dYr0,dYi3
            VST2    {dZr2,dZi2},[pDst :128],outPointStep
            VSUB    dZi3,dYi0,dYr3

            VST2    {dZr3,dZi3},[pDst :128],dstStep


        .endif

        @// increment to data[1] of the next set
        ADD     pSrc,pSrc,pointStep
        BGT     radix4SetLoop\name


        VLD1     dW1,[pTwiddle :64],stepTwiddle    @//[wi | wr]
        @// subtract 4 since grpCount multiplied by 4
        SUBS    grpCount,grpCount,#4
        VLD1     dW2,[pTwiddle :64],stepTwiddle    @//[wi | wr]
        @// increment pSrc for the next grp
        ADD     pSrc,pSrc,srcStep
        VLD1     dW3,[pTwiddle :64],twStep         @//[wi | wr]
        BGT     radix4GrpLoop\name


        @// Reset and Swap pSrc and pDst for the next stage
        MOV     t1,pDst
        @// pDst -= 2*size; pSrc -= 8*size bytes
        SUB     pDst,pSrc,outPointStep,LSL #2
        SUB     pSrc,t1,outPointStep


        .endm


        M_START armSP_FFTFwd_CToC_FC32_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE "FALSE","FALSE",FWD
        M_END


        M_START armSP_FFTInv_CToC_FC32_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE "FALSE","TRUE",INV
        M_END


        .end
