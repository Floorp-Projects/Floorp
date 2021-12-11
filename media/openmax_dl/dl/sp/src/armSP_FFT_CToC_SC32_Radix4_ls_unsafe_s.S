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
@// File Name:  armSP_FFT_CToC_SC32_Radix4_ls_unsafe_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   7767
@// Last Modified Date:       Thu, 27 Sep 2007
@// 
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@// 
@// 
@//
@// Description:
@// Compute a Radix 4 FFT stage for a N point complex signal
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

#define pSrc		r0
#define pDst		r2
#define pTwiddle	r1
#define subFFTNum	r6
#define subFFTSize	r7



@//Output Registers


@//Local Scratch Registers

#define outPointStep	r3
#define grpCount	r4
#define dstStep		r5
#define grpTwStep	r8
#define stepTwiddle	r9
#define twStep		r10
#define pTmp		r4
#define step16		r11
#define step24		r12


@// Neon Registers

#define dButterfly1Real02	D0.S32
#define dButterfly1Imag02	D1.S32
#define dButterfly1Real13	D2.S32
#define dButterfly1Imag13	D3.S32
#define dButterfly2Real02	D4.S32
#define dButterfly2Imag02	D5.S32
#define dButterfly2Real13	D6.S32
#define dButterfly2Imag13	D7.S32
#define dXr0			D0.S32
#define dXi0			D1.S32
#define dXr1			D2.S32
#define dXi1			D3.S32
#define dXr2			D4.S32
#define dXi2			D5.S32
#define dXr3			D6.S32
#define dXi3			D7.S32

#define dYr0			D16.S32
#define dYi0			D17.S32
#define dYr1			D18.S32
#define dYi1			D19.S32
#define dYr2			D20.S32
#define dYi2			D21.S32
#define dYr3			D22.S32
#define dYi3			D23.S32

#define dW1r			D8.S32
#define dW1i			D9.S32
#define dW2r			D10.S32
#define dW2i			D11.S32
#define dW3r			D12.S32
#define dW3i			D13.S32
#define qT0			Q7.S64
#define qT1			Q8.S64
#define qT2			Q9.S64
#define qT3			Q10.S64
#define qT4			Q11.S64
#define qT5			Q12.S64

#define dZr0			D14.S32
#define dZi0			D15.S32
#define dZr1			D26.S32
#define dZi1			D27.S32
#define dZr2			D28.S32
#define dZi2			D29.S32
#define dZr3			D30.S32
#define dZi3			D31.S32

#define qX0			Q0.S32
#define qY0			Q8.S32
#define qY1			Q9.S32   
#define qY2			Q10.S32
#define qY3			Q11.S32
#define qZ0			Q7.S32
#define qZ1			Q13.S32   
#define qZ2			Q14.S32
#define qZ3			Q15.S32


        
        .MACRO FFTSTAGE scaled, inverse , name
        
        @// Define stack arguments
        
        
        @// pOut0+1 increments pOut0 by 8 bytes
        @// pOut0+outPointStep == increment of 8*outPointStep bytes 
        MOV     outPointStep,subFFTSize,LSL #3
        
        @// Update grpCount and grpSize rightaway 
        
        VLD2    {dW1r,dW1i},[pTwiddle, :128]                          @// [wi|wr]
        MOV     step16,#16
        LSL     grpCount,subFFTSize,#2
        
        VLD1    dW2r,[pTwiddle, :64]                             @// [wi|wr]
        MOV     subFFTNum,#1                            @//after the last stage
        
        VLD1    dW3r,[pTwiddle, :64],step16                     @// [wi|wr]
        MOV     stepTwiddle,#0
        
        VLD1    dW2i,[pTwiddle, :64]!                            @// [wi|wr]
        SUB     grpTwStep,stepTwiddle,#8                    @// grpTwStep = -8 to start with       
        
        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
        VLD1    dW3i,[pTwiddle, :64],grpTwStep                           @// [wi|wr]
        MOV     dstStep,outPointStep,LSL #1
        
        VLD4     {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i
        ADD     dstStep,dstStep,outPointStep                @// dstStep = 3*outPointStep
        RSB     dstStep,dstStep,#16                         @// dstStep = - 3*outPointStep+16
        MOV     step24,#24 

        VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i
        

        @// Process two groups at a time
        
grpLoop\name :	
        
        VZIP    dW2r,dW2i
        ADD     stepTwiddle,stepTwiddle,#16                 @// increment for the next iteration
        VZIP    dW3r,dW3i
        ADD     grpTwStep,stepTwiddle,#4
        VUZP     dButterfly1Real13, dButterfly2Real13        @// B.r D.r
        SUB     twStep,stepTwiddle,#16                      @// -16+stepTwiddle
        VUZP     dButterfly1Imag13, dButterfly2Imag13        @// B.i D.i
        MOV     grpTwStep,grpTwStep,LSL #1
        VUZP     dButterfly1Real02, dButterfly2Real02        @// A.r C.r
        RSB     grpTwStep,grpTwStep,#0                      @// -8-2*stepTwiddle
        
        
        VUZP     dButterfly1Imag02, dButterfly2Imag02        @// A.i C.i
        
        
        SUBS    grpCount,grpCount,#8                    @// grpCount is multiplied by 4
                
        .ifeqs  "\inverse", "TRUE"
            VMULL   qT0,dW1r,dXr1
            VMLAL   qT0,dW1i,dXi1                       @// real part
            VMULL   qT1,dW1r,dXi1
            VMLSL   qT1,dW1i,dXr1                       @// imag part
                
        .else
        
            VMULL   qT0,dW1r,dXr1
            VMLSL   qT0,dW1i,dXi1                       @// real part
            VMULL   qT1,dW1r,dXi1
            VMLAL   qT1,dW1i,dXr1                       @// imag part
                    
        .endif
        
        VLD2    {dW1r,dW1i},[pTwiddle, :128],stepTwiddle      @// [wi|wr]
        
        .ifeqs  "\inverse", "TRUE"
            VMULL   qT2,dW2r,dXr2
            VMLAL   qT2,dW2i,dXi2                       @// real part
            VMULL   qT3,dW2r,dXi2
            VLD1    dW2r,[pTwiddle, :64],step16                  @// [wi|wr]
            VMLSL   qT3,dW2i,dXr2                       @// imag part
                
        .else
        
            VMULL   qT2,dW2r,dXr2
            VMLSL   qT2,dW2i,dXi2                       @// real part
            VMULL   qT3,dW2r,dXi2
            VLD1    dW2r,[pTwiddle, :64],step16                  @// [wi|wr]
            VMLAL   qT3,dW2i,dXr2                       @// imag part
                    
        .endif
        
        
        VRSHRN  dZr1,qT0,#31
        VLD1    dW2i,[pTwiddle, :64],twStep                  @// [wi|wr] 
        VRSHRN  dZi1,qT1,#31
        
        VMOV     qZ0,qX0                                @// move qX0 so as to load for the next iteration
        VLD4     {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i
        
                
        .ifeqs  "\inverse", "TRUE"
            VMULL   qT4,dW3r,dXr3
            VMLAL   qT4,dW3i,dXi3                       @// real part
            VMULL   qT5,dW3r,dXi3
            VLD1    dW3r,[pTwiddle, :64],step24
            VMLSL   qT5,dW3i,dXr3                       @// imag part
                
        .else
        
            VMULL   qT4,dW3r,dXr3
            VMLSL   qT4,dW3i,dXi3                       @// real part
            VMULL   qT5,dW3r,dXi3
            VLD1    dW3r,[pTwiddle, :64],step24
            VMLAL   qT5,dW3i,dXr3                       @// imag part
                    
        .endif
        
        VRSHRN  dZr2,qT2,#31
        VLD1    dW3i,[pTwiddle, :64],grpTwStep                           @// [wi|wr]
        VRSHRN  dZi2,qT3,#31
        
        VRSHRN  dZr3,qT4,#31
        VRSHRN  dZi3,qT5,#31
        VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc, :256]! @// AC.r AC.i BD.r BD.i
        
                
        .ifeqs "\scaled", "TRUE"
        
            @// finish first stage of 4 point FFT 
            
            VHADD    qY0,qZ0,qZ2
            VHSUB    qY2,qZ0,qZ2
            VHADD    qY1,qZ1,qZ3
            VHSUB    qY3,qZ1,qZ3
            
                        
            @// finish second stage of 4 point FFT 
            
            .ifeqs  "\inverse", "TRUE"

                VHSUB    qZ0,qY2,qY1
            
                VHADD    dZr3,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VHSUB    dZi3,dYi0,dYr3
                                
                VHADD    qZ2,qY2,qY1
                VST2    {dZr3,dZi3},[pDst, :128],outPointStep
            
                VHSUB    dZr1,dYr0,dYi3
                VST2    {dZr2,dZi2},[pDst, :128],outPointStep
                VHADD    dZi1,dYi0,dYr3
                
                VST2    {dZr1,dZi1},[pDst, :128],dstStep              @// dstStep = -outPointStep + 16
            
                                
            .else
                
                VHSUB    qZ0,qY2,qY1
            
                VHSUB    dZr1,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst, :128],outPointStep
                VHADD    dZi1,dYi0,dYr3
            
                VHADD    qZ2,qY2,qY1
                VST2    {dZr1,dZi1},[pDst, :128],outPointStep
            
                VHADD    dZr3,dYr0,dYi3
                VST2    {dZr2,dZi2},[pDst, :128],outPointStep
                VHSUB    dZi3,dYi0,dYr3
                
                VST2    {dZr3,dZi3},[pDst, :128],dstStep              @// dstStep = -outPointStep + 16

            
            .endif
            
        
        
        .else
        
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
                
                VST2    {dZr1,dZi1},[pDst, :128],dstStep              @// dstStep = -outPointStep + 16
            
                                
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
                
                VST2    {dZr3,dZi3},[pDst, :128],dstStep              @// dstStep = -outPointStep + 16

            
            .endif
            
        .endif
        
        BGT     grpLoop\name
           
                
        @// Reset and Swap pSrc and pDst for the next stage     
        MOV     pTmp,pDst
        SUB     pSrc,pSrc,#64                       @// Extra increment done in final iteration of the loop
        SUB     pDst,pSrc,outPointStep,LSL #2       @// pDst -= 4*size; pSrc -= 8*size bytes           
        SUB     pSrc,pTmp,outPointStep
        SUB     pTwiddle,pTwiddle,subFFTSize,LSL #1
        SUB     pTwiddle,pTwiddle,#16               @// Extra increment done in final iteration of the loop
        
        .endm
        
        
        M_START armSP_FFTFwd_CToC_SC32_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","FALSE",fwd
        M_END

        
        M_START armSP_FFTInv_CToC_SC32_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",inv
        M_END
 
        
        M_START armSP_FFTFwd_CToC_SC32_Sfs_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","FALSE",fwdsfs
        M_END

        
        M_START armSP_FFTInv_CToC_SC32_Sfs_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","TRUE",invsfs
        M_END

        
	.end
