/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#ifndef _NLSUTL_H
#define _NLSUTL_H


#include "nlsxp.h"
#include "nlsenc.h"

NLS_BEGIN_PROTOS


/******************** Accept Language List ************************/
#define NLS_MAX_ACCEPT_LANGUAGE 16
#define NLS_MAX_ACCEPT_LENGTH 18
typedef char NLS_ACCEPT_LANGUAGE_LIST[NLS_MAX_ACCEPT_LANGUAGE][NLS_MAX_ACCEPT_LENGTH]; 

/* NLS_AcceptLangList
 *
 * Will parse an Accept-Language string of the form 
 * "en;q=1.0,fr;q=0.9..."
 * The NLS_ACCEPT_LANGUAGE_LIST array will be loaded with the ordered
 * language elements based on the priority of the languages specified.
 * The number of languages will be returned as the result of the 
 * call.
 */
NLSUNIAPI_PUBLIC(size_t)
NLS_AcceptLangList(
    const char * acceptLanguage,
    NLS_ACCEPT_LANGUAGE_LIST acceptLanguageList
);
 
/******************** Accept Charset List ************************/
#define NLS_MAX_ACCEPT_CHARSET 16
#define NLS_MAX_ACCEPT_CHARSET_LENGTH 128
typedef char NLS_ACCEPT_CHARSET_LIST[NLS_MAX_ACCEPT_CHARSET][NLS_MAX_ACCEPT_CHARSET_LENGTH]; 

/* NLS_AcceptCharsetList
 *
 * Will parse an Accept-Charset string of the form 
 * "UTF8;q=0.9,SJIS;q=0.8;*;q=1.0,..."
 * The NLS_ACCEPT_CHARSET_LIST array will be loaded with the ordered
 * charset elements based on the priority of the charset specified.
 * The number of charsets will be returned as the result of the 
 * call. The NLS_ACCEPT_CHARSET_LIST will be loaded with normalized 
 * charset names. "*" is considered as a valid charset name, and will 
 * be sorted as a charset.
 */
NLSUNIAPI_PUBLIC(size_t)
NLS_AcceptCharsetList(
    const char * acceptCharset,
    NLS_ACCEPT_CHARSET_LIST acceptCharsetList
);

/* NLS_TargetEncodingFromCharsetList
 *
 * Returns the best target encoding for Accept-charset list. 
 * The routine takes a NLS_ACCEPT_CHARSET_LIST returned by 
 * NLS_AcceptCharsetList and returns the best target encoding 
 * based on the sourceEncoding specified. The target encoding will
 * be a normalized encoding name. The result of this routine may 
 * be NULL if no target encoding is found, or if the target encoding 
 * is identical to the source encoding.
 */

NLSUNIAPI_PUBLIC(const char*)
NLS_TargetEncodingFromCharsetList(
	const char* sourceEncoding,
	NLS_ACCEPT_CHARSET_LIST acceptCharsetList, 
	size_t count
);

/*********************   Character Name Entities  *******************/
 
/* NLS_Latin1NameEntityValue
 *
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode) NLS_Latin1NameEntityValue(
    const char * name,
    int *value
);


/************************ End ******************************/
NLS_END_PROTOS
#endif /* _NLSUTL_H */

