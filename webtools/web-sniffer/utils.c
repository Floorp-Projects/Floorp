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
 * The Original Code is Web Sniffer.
 * 
 * The Initial Developer of the Original Code is Erik van der Poel.
 * Portions created by Erik van der Poel are
 * Copyright (C) 1998,1999,2000 Erik van der Poel.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 */

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

unsigned char *
appendString(const unsigned char *str1, const unsigned char *str2)
{
	unsigned char	*result;

	if (!str1)
	{
		return copyString(str2);
	}
	if (!str2)
	{
		return copyString(str1);
	}
	result = calloc(strlen((char *) str1) + strlen((char *) str2) + 1, 1);
	if (!result)
	{
		fprintf(stderr, "cannot calloc string\n");
		exit(0);
	}
	strcpy((char *) result, (char *) str1);
	strcat((char *) result, (char *) str2);

	return result;
}

unsigned char *
copyString(const unsigned char *str)
{
	unsigned char	*result;

	if (!str)
	{
		return NULL;
	}

	result = (unsigned char *) strdup((char *) str);
	if (!result)
	{
		fprintf(stderr, "cannot strdup string\n");
		exit(0);
	}

	return result;
}

unsigned char *
copySizedString(const unsigned char *str, int size)
{
	unsigned char	*result;

	result = calloc(size + 1, 1);
	if (!result)
	{
		fprintf(stderr, "cannot calloc string\n");
		exit(0);
	}

	strncpy((char *) result, (char *) str, size);
	result[size] = 0;

	return result;
}

unsigned char *
lowerCase(unsigned char *buf)
{
	unsigned char	c;
	unsigned char	*p;

	p = buf;
	do
	{
		c = *p;
		if (('A' <= c) && (c <= 'Z'))
		{
			*p = c + 32;
		}
		p++;
	} while (c);

	return buf;
}

void *
utilRealloc(void *ptr, size_t oldSize, size_t newSize)
{
	unsigned char	*end;
	unsigned char	*p;
	unsigned char	*ret;

	ret = realloc(ptr, newSize);
	if (ret && (newSize > oldSize))
	{
		end = &ret[newSize];
		for (p = &ret[oldSize]; p < end; p++)
		{
			*p = 0;
		}
	}

	return (void *) ret;
}
