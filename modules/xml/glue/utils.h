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
