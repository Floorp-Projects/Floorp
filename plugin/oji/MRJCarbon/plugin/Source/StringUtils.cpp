/* ----- BEGIN LICENSE BLOCK -----
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
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
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
 * ----- END LICENSE BLOCK ----- */

/*
	StringUtils.cpp
 */

#include "StringUtils.h"

#include <string.h>

inline unsigned char toupper(unsigned char c)
{
	return (c >= 'a' && c <= 'z') ? (c - ('a' - 'A')) : c;
}

int strcasecmp(const char * str1, const char * str2)
{
#if !__POWERPC__
	
	const	unsigned char * p1 = (unsigned char *) str1;
	const	unsigned char * p2 = (unsigned char *) str2;
				unsigned char		c1, c2;
	
	while (toupper(c1 = *p1++) == toupper(c2 = *p2++))
		if (!c1)
			return(0);

#else
	
	const	unsigned char * p1 = (unsigned char *) str1 - 1;
	const	unsigned char * p2 = (unsigned char *) str2 - 1;
				unsigned long		c1, c2;
		
	while (toupper(c1 = *++p1) == toupper(c2 = *++p2))
		if (!c1)
			return(0);

#endif
	
	return(toupper(c1) - toupper(c2));
}

char* strdup(const char* str)
{
	if (str != NULL) {
		char* result = new char[::strlen(str) + 1];
		if (result != NULL)
			::strcpy(result, str);
		return result;
	}
	return NULL;
}
