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

#ifndef PRSXP_H_
#define PRSXP_H_

#include "nlsxp.h"
#include "unistring.h"

#ifndef nil
#define nil 0
#endif

#ifndef NULL
#define NULL 0
#endif



/* flush out all the system macros */
#if defined(macintosh) || defined(__MWERKS__) || defined(applec)
#ifndef PRS_MAC
#define PRS_MAC
#endif
#endif

#if defined(__unix) || defined(unix) || defined(UNIX) || defined(XP_UNIX)
#ifndef PRS_UNIX
#define PRS_UNIX
#endif
#endif

#if defined(_WINDOWS) || defined(XP_PC) || defined(_CONSOLE)
#ifndef PRS_PC
#define PRS_PC
#endif /* ifndef PRS_PC */
#endif

#if !defined(PRS_MAC) && !defined(PRS_UNIX) && !defined(PRS_PC)
#error "libPRS can not determine system type" 
#endif



/* X-platform stuff */
#ifdef PRS_MAC
#define PRS_DEFSET_INCOMING			NLS_ENCODING_MAC_ROMAN
#define PRS_DEFSET_INCOMING			NLS_ENCODING_MAC_ROMAN
#define PRS_NEWLINE		       		UnicodeString("\n")
#define PRS_DIRECTORY_SEPARATOR			'/'
#define PRS_DIRECTORY_SEPARATOR_STR		"/"
#else
#define PRS_DEFSET_INCOMING			NLS_ENCODING_ISO_8859_1
#endif

#ifdef PRS_PC
#define PRS_NEWLINE		       	UnicodeString("\n")
#define PRS_DIRECTORY_SEPARATOR         '\\'
#define PRS_DIRECTORY_SEPARATOR_STR     "\\"
#endif

#ifdef PRS_UNIX
#define PRS_NEWLINE		       	        UnicodeString("\n")
#define PRS_DIRECTORY_SEPARATOR			'/'
#define PRS_DIRECTORY_SEPARATOR_STR		"/"
#endif

#define PRS_DEFSET_EXPORT			NLS_ENCODING_UTF_8
#define PRS_DEFSET_RESOURCE			NLS_ENCODING_ESCAPED_UNICODE
#define PRS_DEFSET_OUTGOING			PRS_DEFSET_INCOMING

#define PRS_EXPORT_SUFFIX				".export"
#define PRS_RESOURCE_SUFFIX				".properties"

#define PRS_MAX_RESOURCE_NAME_LENGTH	132

#define PRS_RESOURCE_DELIMETER			UnicodeString("=")
#define PRS_RESOURCE_DELIMETER_CHAR		'='
#define PRS_ID_UNKNOWN					0x0000
#define PRS_ID_STRING					0x0101			// Any text not within a tag. Delimeted by tags on either side. 
#define PRS_ID_TAG			       		0x0102			// An HTML Tag. Delimited by < >.
#define PRS_ID_SUBSTRING				0x0103			// A quoted string occuring within a tag or Javascript.
struct PRS_ErrorPair { int code; char *message; };
typedef PRS_ErrorPair *PRS_ErrorCode;


// #define PRS_NEWLINE "\r\n"
// #define PRS_NEWLINE_LENGTH 2
#define PRS_WHITESPACE UnicodeString(" ")
// #define PRS_WHITESPACE_LENGTH 1

#define PRS_REGISTRY_DEFAULT_HASHSIZE 91

#define PRS_EOF 0xFFFF

#define isWhite(x) (((x) == 13) || ((x) == 10) || ((x) == ' ') || ((x) == '	'))
#define isNewline(x) (((x) == 13) || ((x) == 10))
#define isTagStart(x) ((x) == '<')
#define isTagEnd(x) ((x) == '>')
#define isResourceDelimeter(x) ((x) == PRS_RESOURCE_DELIMETER_CHAR)

extern PRS_ErrorCode PRS_SUCCESS;
extern PRS_ErrorCode PRS_RESERVED;
extern PRS_ErrorCode PRS_FAIL;
extern PRS_ErrorCode PRS_OUT_OF_MEMORY;
extern PRS_ErrorCode PRS_FILE_WRITE_ERROR;
extern PRS_ErrorCode PRS_FILE_READ_ERROR;
extern PRS_ErrorCode PRS_BAD_PARAMETER;
extern PRS_ErrorCode PRS_FILE_NOT_FOUND;		
extern PRS_ErrorCode PRS_FILE_CREATE_ERROR;	
extern PRS_ErrorCode PRS_FILE_CLOSE_ERROR;	
extern PRS_ErrorCode PRS_PARSE_ERROR;		
extern PRS_ErrorCode PRS_PARSE_ERROR_TAG;	
extern PRS_ErrorCode PRS_PARSE_ERROR_QUOTE;	
extern PRS_ErrorCode PRS_RES_FORMAT_EXCEPTION;


#endif // !defined(PRSXP_H_)







