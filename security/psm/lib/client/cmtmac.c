/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "cmtmac.h"
#include "macsocket.h"
#include "stdlib.h"

#ifndef XP_MAC
#error Link with the builtin strdup() on your platform.
#endif


static void 
my_strcpy(char *dest, const char *source)
{
	char *i = dest;
	const char *j = source;
	while(*j)
		*i++ = *j++;
	*i = '\0';
}

static int
my_strlen(const char *str)
{
	const char *c = str;
	int i = 0;
	
	while(*c++ != '\0')
		i++;
	return i;
}

char * strdup(const char *oldstr)
{
	/* used to keep the mac client library from referring to strdup elsewhere */
	char *newstr;
	
	newstr = (char *) malloc(my_strlen(oldstr)+1);
	if (newstr)
		my_strcpy(newstr, oldstr);
	return newstr;
}

