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
 * Communications Corporation.	Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <plstr.h>
#include "aglobal.h"
#include "bsafe.h"
#include "secport.h"

void CALL_CONV T_memset (p, c, count)
POINTER p;
int c;
unsigned int count;
{
	if (count >= 0)
		memset(p, c, count);
}

void CALL_CONV T_memcpy (d, s, count)
POINTER d, s;
unsigned int count;
{
	if (count >= 0)
		memcpy(d, s, count);
}

void CALL_CONV T_memmove (d, s, count)
POINTER d, s;
unsigned int count;
{
	if (count >= 0)
		PORT_Memmove(d, s, count);
}

int CALL_CONV T_memcmp (s1, s2, count)
POINTER s1, s2;
unsigned int count;
{
	if (count == 0)
		return (0);
	else
		return(memcmp(s1, s2, count));
}

POINTER CALL_CONV T_malloc (size)
unsigned int size;
{
	return((POINTER)PORT_Alloc(size == 0 ? 1 : size));
}

POINTER CALL_CONV T_realloc (p, size)
POINTER p;
unsigned int size;
{
	POINTER result;
  
	if (p == NULL_PTR)
		return (T_malloc(size));

	if ((result = (POINTER)PORT_Realloc(p, size == 0 ? 1 : size)) == NULL_PTR)
		PORT_Free(p);
  return (result);
}

void CALL_CONV T_free (p)
POINTER p;
{
	if (p != NULL_PTR)
		PORT_Free(p);
}

unsigned int CALL_CONV T_strlen(p)
char *p;
{
    return PL_strlen(p);
}

void CALL_CONV T_strcpy(dest, src)
char *dest;
char *src;
{
    PL_strcpy(dest, src);
}

int CALL_CONV T_strcmp (a, b)
char *a, *b;
{
  return (PL_strcmp (a, b));
}
