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

#define PRS_RESOURCE_DELIMITER			UnicodeString("=")
#define PRS_RESOURCE_DELIMITER_CHAR		'='
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
#define isResourceDelimiter(x) ((x) == PRS_RESOURCE_DELIMITER_CHAR)

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







