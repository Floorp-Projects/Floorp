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

