/*
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
