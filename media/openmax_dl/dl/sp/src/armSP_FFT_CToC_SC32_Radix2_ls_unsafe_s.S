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
@// File Name:  armSP_FFT_CToC_SC32_Radix2_ls_unsafe_s.s
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   7493
@// Last Modified Date:       Mon, 24 Sep 2007
@// 
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@// 
@// 
@//
@// Description:
@// Compute the last stage of a Radix 2 DIT in-order out-of-place FFT
@// stage for a N point complex signal.
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
#define pTmp		r4

@// Neon Registers

#define dWr	D0.S32
#define dWi	d1.s32
#define dXr0	d2.s32
#define dXi0	d3.s32
#define dXr1	d4.s32
#define dXi1	d5.s32
#define dYr0	d6.s32
#define dYi0	d7.s32
#define dYr1	d8.s32
#define dYi1	d9.s32
#define qT0	q5.s64
#define qT1	q6.s64
	
        .macro FFTSTAGE scaled, inverse, name
        
        
        MOV     outPointStep,subFFTSize,LSL #3
        @// Update grpCount and grpSize rightaway 
        
        MOV     subFFTNum,#1                            @//after the last stage
        LSL     grpCount,subFFTSize,#1
        
        @// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
                               
        RSB      dstStep,outPointStep,#16
        
        
        @// Loop on 2 grps at a time for the last stage

grpLoop\name :	
        VLD2    {dWr,dWi},[pTwiddle, :64]!
        
        VLD4    {dXr0,dXi0,dXr1,dXi1},[pSrc, :128]!
        SUBS    grpCount,grpCount,#4                   @// grpCount is multiplied by 2 
        
        .ifeqs  "\inverse", "TRUE"
            VMULL   qT0,dWr,dXr1
            VMLAL   qT0,dWi,dXi1                       @// real part
            VMULL   qT1,dWr,dXi1
            VMLSL   qT1,dWi,dXr1                       @// imag part
                
        .else
        
            VMULL   qT0,dWr,dXr1
            VMLSL   qT0,dWi,dXi1                       @// real part
            VMULL   qT1,dWr,dXi1
            VMLAL   qT1,dWi,dXr1                       @// imag part
                    
        .endif
        
        VRSHRN  dXr1,qT0,#31
        VRSHRN  dXi1,qT1,#31
        
                
        .ifeqs "\scaled", "TRUE"
        
            VHSUB    dYr0,dXr0,dXr1
            VHSUB    dYi0,dXi0,dXi1
            VHADD    dYr1,dXr0,dXr1
            VHADD    dYi1,dXi0,dXi1
            
        .else
        
            VSUB    dYr0,dXr0,dXr1
            VSUB    dYi0,dXi0,dXi1
            VADD    dYr1,dXr0,dXr1
            VADD    dYi1,dXi0,dXi1
            
         
        .endif
        
        VST2    {dYr0,dYi0},[pDst],outPointStep
        VST2    {dYr1,dYi1},[pDst],dstStep                  @// dstStep =  step = -outPointStep + 16
               
        bgt     grpLoop\name
        
        
        @// Reset and Swap pSrc and pDst for the next stage     
        MOV     pTmp,pDst
        SUB     pDst,pSrc,outPointStep,LSL #1       @// pDst -= 4*size; pSrc -= 8*size bytes           
        SUB     pSrc,pTmp,outPointStep
        
        @// Reset pTwiddle for the next stage
        SUB     pTwiddle,pTwiddle,outPointStep      @// pTwiddle -= 4*size bytes
                
        .endm
        
        
                
        M_START armSP_FFTFwd_CToC_SC32_Radix2_ls_OutOfPlace_unsafe,r4,""
        FFTSTAGE "FALSE","FALSE",fwd
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC32_Radix2_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "FALSE","TRUE",inv
        M_END
 
        
        
        M_START armSP_FFTFwd_CToC_SC32_Sfs_Radix2_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","FALSE",fwdsfs
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC32_Sfs_Radix2_ls_OutOfPlace_unsafe,r4
        FFTSTAGE "TRUE","TRUE",invsfs
        M_END
	
	.end
