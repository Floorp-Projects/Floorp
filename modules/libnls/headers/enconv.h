/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


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
