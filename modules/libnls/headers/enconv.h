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


#ifndef ENCONV_H
#define ENCONV_H

#include "nlsxp.h"

typedef struct _NLS_AUTO_DETECT_RESULT
{
	const char*		fEncoding;
	uint32			fFeatures;
	uint32			fFlaws;
	uint32			fErrors;
} NLS_AUTO_DETECT_RESULT;

#ifndef NLS_CPLUSPLUS

typedef struct _EncodingConverter EncodingConverter;
typedef struct _EncondingConverterList EncondingConverterList;

#else

typedef struct OpaqueCCCDataObject *CCCDataObject;

//
// Forward Delclartion of libnls private classes
//
class TEncodingRegistry;
class TEncodingConverter;
class TEncodingEntry;

#ifdef NLS_MAC
#pragma export on
#endif

class NLSCNVAPI_PUBLIC_CLASS EncodingConverter
{
public:
	/*
	 * Constructor/destructor
	 */
	EncodingConverter(const char *from_charset, const char *to_charset);
	~EncodingConverter();

	size_t						convert(const byte *inputBuffer, size_t inputBufferLength,
									byte * outputBuffer, size_t outputBufferLength);

	NLS_ErrorCode				status();
	void						reset();

	/* If output buffer was two small, remainder can be retrieved */
	size_t						remainderSize();
	size_t						nextBuffer(byte * buffer, size_t bufferSize); 

	/* If conversion was incomplete there will be bytes leftover*/ 
	size_t						leftoverSize();
	size_t						leftover(byte * outputBuffer, size_t outputBufferLength);

	static nlsBool				exists(const char *from_charset, const char *to_charset);
	static const char*			normalizedEncodingName(const char* encoding);
	static const char*			normalizedJavaName(const char* javaName);
	static const char*			javaEncodingName(const char* encoding);
	static size_t				resultBufferSize(const byte *buffer, size_t length, 
									const char *from_charset, const char *to_charset);
	static void					registerStaticLibrary(NLS_StaticConverterRegistry ImportStaticEntryList);

	/* Data directory set & get */
	static const	char*		getDataDirectory();
	static void				setDataDirectory(const char* path);
	static NLS_ErrorCode		initializeEncodingRegistry(void);
	static TEncodingRegistry*	getEncodingRegistry(void);
	static void deleteEncodingRegistry(void);

private:
	int16						intermidiateCSIDForUnicodeConversion(int16 csid);
	size_t						unicodeStringLength(UniChar *ustr);

	NLS_ErrorCode		fStatus;
    const char*			fFromCharset;
	const char*			fToCharset;
	CCCDataObject		fObj;
	CCCDataObject		fPreUniConvObj;
	CCCDataObject		fPostUniConvObj;
	size_t				fLeftOverSize;
	size_t				fLeftOverAllocatedSize;
	byte*				fLeftOverBuffer;
	size_t				fRemSize;
	size_t				fRemAllocatedSize;
	byte*				fRemBuffer;
	TEncodingConverter	*fEngine;

	static	char*				fgConverterPath;
	static	const char*			kDefaultConverterPath;
	static  TEncodingRegistry*	fgRegistry;

};

class NLSCNVAPI_PUBLIC_CLASS EncondingConverterList
{
public:
	/*
	 * Constructor/destructor
	 */
	EncondingConverterList(const char *from_charset, const char *to_charset);
	~EncondingConverterList();

	size_t						count();
	void						index(size_t number, char *from_charset, char *to_charset);

private:
	NLS_ErrorCode		fStatus;
    const char*			fFromCharset;
	const char*			fToCharset;
	size_t				fCount;
	const char**		fFromArray;
	const char**		fToArray;
	TEncodingRegistry*  fRegistry;
};

#ifdef NLS_MAC
#pragma export off
#endif

#endif /* #ifndef NLS_CPLUSPLUS */
#endif /* #ifndef ENCONV_H */
