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
@// File Name:  omxtypes_s.h
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   9622
@// Last Modified Date:       Wed, 06 Feb 2008
@// 
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@// 
@//

@// Mandatory return codes - use cases are explicitly described for each function 
	.equ	OMX_Sts_NoErr, 0    @// No error the function completed successfully 
	.equ	OMX_Sts_Err, -2    @// Unknown/unspecified error     
	.equ	OMX_Sts_InvalidBitstreamValErr, -182  @// Invalid value detected during bitstream processing     
	.equ	OMX_Sts_MemAllocErr, -9    @// Not enough memory allocated for the operation 
	.equ	OMX_StsACAAC_GainCtrErr, -159  @// AAC: Unsupported gain control data detected 
	.equ	OMX_StsACAAC_PrgNumErr, -167  @// AAC: Invalid number of elements for one program   
	.equ	OMX_StsACAAC_CoefValErr, -163  @// AAC: Invalid quantized coefficient value               
	.equ	OMX_StsACAAC_MaxSfbErr, -162  @// AAC: Invalid maxSfb value in relation to numSwb     
	.equ	OMX_StsACAAC_PlsDataErr, -160  @// AAC: pulse escape sequence data error 

@// Optional return codes - use cases are explicitly described for each function
	.equ	OMX_Sts_BadArgErr, -5    @// Bad Arguments 

	.equ	OMX_StsACAAC_TnsNumFiltErr, -157  @// AAC: Invalid number of TNS filters  
	.equ	OMX_StsACAAC_TnsLenErr, -156  @// AAC: Invalid TNS region length     
	.equ	OMX_StsACAAC_TnsOrderErr, -155  @// AAC: Invalid order of TNS filter                    
	.equ	OMX_StsACAAC_TnsCoefResErr, -154  @// AAC: Invalid bit-resolution for TNS filter coefficients  
	.equ	OMX_StsACAAC_TnsCoefErr, -153  @// AAC: Invalid TNS filter coefficients                    
	.equ	OMX_StsACAAC_TnsDirectErr, -152  @// AAC: Invalid TNS filter direction    
	.equ	OMX_StsICJP_JPEGMarkerErr, -183  @// JPEG marker encountered within an entropy-coded block; 
                                            @// Huffman decoding operation terminated early.           
	.equ	OMX_StsICJP_JPEGMarker, -181  @// JPEG marker encountered; Huffman decoding 
                                            @// operation terminated early.                         
	.equ	OMX_StsIPPP_ContextMatchErr, -17   @// Context parameter doesn't match to the operation 

	.equ	OMX_StsSP_EvenMedianMaskSizeErr, -180  @// Even size of the Median Filter mask was replaced by the odd one 

	.equ	OMX_Sts_MaximumEnumeration, 0x7FFFFFFF



	.equ	OMX_MIN_S8, (-128)
	.equ	OMX_MIN_U8, 0
	.equ	OMX_MIN_S16, (-32768)
	.equ	OMX_MIN_U16, 0


	.equ	OMX_MIN_S32, (-2147483647-1)
	.equ	OMX_MIN_U32, 0

	.equ	OMX_MAX_S8, (127)
	.equ	OMX_MAX_U8, (255)
	.equ	OMX_MAX_S16, (32767)
	.equ	OMX_MAX_U16, (0xFFFF)
	.equ	OMX_MAX_S32, (2147483647)
	.equ	OMX_MAX_U32, (0xFFFFFFFF)

	.equ	OMX_VC_UPPER, 0x1                 @// Used by the PredictIntra functions   
	.equ	OMX_VC_LEFT, 0x2                 @// Used by the PredictIntra functions 
	.equ	OMX_VC_UPPER_RIGHT, 0x40          @// Used by the PredictIntra functions   

	.equ	NULL, 0
