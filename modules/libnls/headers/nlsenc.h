/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _NLSENC_H
#define _NLSENC_H


#include "nlsxp.h"
#include "enconv.h"

NLS_BEGIN_PROTOS

/********************** Library Initialization ***********************/

/* NLS_EncInitialize
 *
 * Initialize the libcnv library. The dataDirectory is a full or
 * partial platform specific path specification under which the library may 
 * can find the "converters" sub directories. 
 *
 * Status Return
 *		NLS_SUCCESS
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_EncInitialize(const NLS_ThreadInfo * threadInfo, const char * dataDirectory);

/* NLS_EncTerminate
 *
 * Terminate the library
 *
 * Status Return
 *		NLS_SUCCESS
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_EncTerminate(void);

/*********************************************
    Simple Conversion Routines
 ********************************************/

/* NLS_EncodingConverterExists
 *
 * Tests for the existence of an encoding converter for the source/target 
 * encoding conversion. Encoding names will be normalized by this function
 * to resolve encoding aliases etc.
 */
NLSCNVAPI_PUBLIC(nlsBool)
NLS_EncodingConverterExists(const char *from_charset, const char *to_charset);

/* NLS_ConvertBuffer
 *
 * Converts the source buffer/size to the target buffer/size based on the 
 * source/target encoding information. The encoding names will be normalized 
 * by this function to resolve encoding aliases etc. Use NLS_GetResultBufferSize
 * to calculate the best output buffer size based on the input encoding.
 *
 * Status Returns
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 *		NLS_NEW_CONVERTER_FAILED	- Function failed
 *		NLS_NO_FROM_ENCODING		- No to encoding exists
 *		NLS_NO_TO_ENCODING 			- No from encoding exists
 *		NLS_NEW_CONVERTER_FAILED 	- No converter exists
 *
 *		NLS_RESULT_TRUNCATED		- Output buffer was too small, 
 *									  result was truncated
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_ConvertBuffer(const char *from_charset, const char *to_charset, 
				  const byte *inputBuffer, size_t inputBufferLength,
                  byte * outputBuffer, size_t outputBufferLength, size_t * convertedLength);

/*********************************************
    Streaming Conversion Routines
 ********************************************/

/* NLS_NewEncodingConverter
 *
 * Create a new encoding converter instance. The encoding names will be normalized 
 * by this function to resolve encoding aliases etc.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 *		NLS_NEW_CONVERTER_FAILED	- Function failed
 *		NLS_NO_FROM_ENCODING		- No to encoding exists
 *		NLS_NO_TO_ENCODING 			- No from encoding exists
 *		NLS_NEW_CONVERTER_FAILED 	- No converter exists
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_NewEncodingConverter(const char *from_charset, const char *to_charset, EncodingConverter** converter);

/* NLS_DeleteEncodingConverter
 *
 * Delete the encoding converter instance.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteEncodingConverter(EncodingConverter * converter);



/* NLS_ConvertStreamBuffer
 *
 * Converts the source buffer/size to the target buffer/size based on the 
 * source/target encoding information. The encoding names will be normalized 
 * by this function to resolve encoding aliases etc. Use NLS_GetResultBufferSize
 * to calculate the best output buffer size based on the input encoding.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 *		NLS_RESULT_TRUNCATED		- Output buffer was too small, 
 *									  result was truncated
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_ConvertStreamBuffer(EncodingConverter * converter, const byte *inputBuffer, size_t inputBufferLength,
                        byte * outputBuffer, size_t outputBufferLength, size_t * convertedLength);

/*  NLS_GetConversionOverflowSize
 *     
 *	If NLS_ConvertStreamBuffer returned NLS_RESULT_TRUNCATED, the size of the remaining 
 *	converted text can be obtained by calling NLS_GetConversionOverflowSize.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */

NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_GetConversionOverflowSize(EncodingConverter * converter, size_t * size);


/*  NLS_ConverterGetOverflow
 *    
 *	If NLS_ConvertStreamBuffer returned NLS_RESULT_TRUNCATED, the remaining 
 *	converted text can be obtained by making consecutive calls to NLS_ConverterGetOverflow.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 *		NLS_RESULT_TRUNCATED		- Output buffer was too small, 
 *									  result was truncated
 */

NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_ConverterGetOverflow(EncodingConverter * converter,
                        byte * outputBuffer, size_t outputBufferLength, size_t * convertedLength);

/* NLS_ResetEncodingConverter
 *
 *	Resets the converter, clearing any converted and/or unconverted
 *	text from the converter
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_ResetEncodingConverter(EncodingConverter * converter);

/* NLS_GetConversionRemSize
 *
 *	When all input text has been converted, there may have been some
 *	characters which did not form a complete convertable group. The number
 *  of remaining bytes can optained by calling NLS_GetConversionRemSize.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_GetConversionRemSize(EncodingConverter * converter, size_t * size);

/* NLS_GetConversionRem
 *
 *	When all input text has been converted, there may have been some
 *	characters which did not form a complete convertable group. The
 *  remaining bytes can optained by calling NLS_GetConversionRem.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_GetConversionRem(EncodingConverter * converter, byte * buffer, size_t bufferSize, size_t * size);

/* NLS_NewEncodingConverterEnumeration
 *
 * Create a new encoding converter enumeration instance. The encoding names will be 
 * normalized by this function to resolve encoding aliases etc. One or more of the 
 * encoding names may be spacified as NULL, resulting in all possible convertions being
 * returned.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR					- parameter verification failed   
 *		NLS_NO_FROM_ENCODING			- No to encoding exists
 *		NLS_NO_TO_ENCODING 				- No from encoding exists
 *		NLS_NEW_CONVERTER_LIST_FAILED 	- No converter exists
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_NewEncodingConverterEnumeration(const char *from_charset, const char *to_charset,
    EncondingConverterList ** converterList);

/* NLS_DeleteEncodingConverterEnumeration
 *
 * Delete the encoding converter enumeration instance.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteEncodingConverterEnumeration(EncondingConverterList * converterList);

/* NLS_CountEncodingConverters
 *
 * Get a count of the number of converters that exist.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_CountEncodingConverters(EncondingConverterList * converterList, size_t * count);

/* NLS_GetEncodingConverterListItem
 *
 *	Caller must supply two char buffers of MAX_ENCODING_NAME_SIZE size, items are 
 *	base 0 counted (0-n).
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_GetEncodingConverterListItem(EncondingConverterList * converterList, size_t itemNumber, 
			char *from_charset, char *to_charset);

/* NLS_GetResultBufferSize
 *
 *	Returns an estimated buffer size in bytes for the target conversion of the 
 *	input stream. The Estimate is garanteed to be larger than the target
 *	conversion will require.
 */

NLSCNVAPI_PUBLIC(size_t)
NLS_GetResultBufferSize(const byte *buffer, size_t length, 
			const char *from_charset, const char *to_charset);

/* NLS_GetNormalizedEncodingName
 *
 *	Will return the normalized encoding name for known encodings, 
 *	eg: UNICODE-1-1-UTF-8 -> UTF-8 etc.
 */
NLSCNVAPI_PUBLIC(const char*)
NLS_GetNormalizedEncodingName(const char* encoding);

/* NLS_GetEncodingNameFromJava
 *
 * Will return the normalized encoding name for a known java encoding name.
 * eg: 8859_1 -> ISO_8859-1:1987 etc.
 */
NLSCNVAPI_PUBLIC(const char*)
NLS_GetEncodingNameFromJava(const char* javaName);

/* NLS_GetJavaEncodingName
 *
 * Will return the java encoding name from a normalized encoding name.
 * eg: ISO_8859-1:1987 -> 8859_1 etc.
 */
NLSCNVAPI_PUBLIC(const char*)
NLS_GetJavaEncodingName(const char* encoding);

/************************ Detection ******************************/

/* NLS_DetectEncodingForBuffer
 *
 *	Detects the encoding for a byte buffer, Will cycle through the 
 *	known encoding converters trying to auto detect the encoding. 
 *	Will create an ordered list of possible encodings for the input 
 *	text.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_AUTO_DETECTION_ERROR   
 */

NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_DetectEncodingForBuffer(const byte *buffer, size_t length,
			NLS_AUTO_DETECT_RESULT *results, size_t resultsCount, size_t* detectionCount);

/* NLS_DetectEncodingForUCS2
 *
 *	Detects the best encoding to convert to from UCS2. Examins the 
 *	contents of the UniChar buffer and attempts to find a target 
 *	conversion which would generate the least loss.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_AUTO_DETECTION_ERROR   
 */
NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_DetectEncodingForUCS2(const UniChar *buffer, size_t length,
			NLS_AUTO_DETECT_RESULT *results, size_t resultsCount, size_t* detectionCount);

/* NLS_RegisterStaticLibrary
 *
 *  Adds registry entries for a statically-linked encoding converter
 *  library to the static link table, making them available for 
 *  for creation with standard methods.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed 
 */

NLSCNVAPI_PUBLIC(NLS_ErrorCode)
NLS_RegisterStaticLibrary(NLS_StaticConverterRegistry ImportStaticEntryList);
/************************ End ******************************/
NLS_END_PROTOS
#endif /* _NLSENC_H */

