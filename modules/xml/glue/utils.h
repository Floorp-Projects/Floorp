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

#ifndef	_XML_UTILS_H_
#define	_XML_UTILS_H_
#include "xp.h"
#include <stdarg.h>
#include <ctype.h>




/* utils.c data structures and defines */

#define CONTAINER_FLAG 0x01
#define LOCKED_FLAG    0x02



/* utils.c function prototypes */

XP_BEGIN_PROTOS
char *		append2Strings (const char* str1, const char* str2);
void		stringAppendBase (char* dest, const char* addition);
void		stringAppend (char* dest, const char* addition);
int16		charSearch (const char c, const char* data);
PRBool		endsWith (const char* pattern, const char* uuid);
PRBool		startsWith (const char* pattern, const char* uuid);
PRBool		substring (const char* pattern, const char* data);
int16		revCharSearch (const char c, const char* data);
PRBool		urlEquals (const char* url1, const char* url2);
XP_END_PROTOS

#endif
