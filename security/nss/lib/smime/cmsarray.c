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

/*
 * CMS array functions.
 *
 * $Id: cmsarray.c,v 1.4 2003/12/06 01:16:50 wchang0222%aol.com Exp $
 */

#include "cmslocal.h"

#include "secerr.h"

/*
 * ARRAY FUNCTIONS
 *
 * In NSS, arrays are rather primitive arrays of pointers.
 * Makes it easy to walk the array, but hard to count elements
 * and manage the storage.
 *
 * This is a feeble attempt to encapsulate the functionality
 * and get rid of hundreds of lines of similar code
 */

/*
 * NSS_CMSArray_Alloc - allocate an array in an arena
 *
 * This allocates space for the array of pointers
 */
void **
NSS_CMSArray_Alloc(PRArenaPool *poolp, int n)
{
    return (void **)PORT_ArenaZAlloc(poolp, n * sizeof(void *));
}

/*
 * NSS_CMSArray_Add - add an element to the end of an array
 *
 * The array of pointers is either created (if array was empty before) or grown.
 */
SECStatus
NSS_CMSArray_Add(PRArenaPool *poolp, void ***array, void *obj)
{
    void **p;
    int n;
    void **dest;

    PORT_Assert(array != NULL);
    if (array == NULL)
	return SECFailure;

    if (*array == NULL) {
	dest = (void **)PORT_ArenaAlloc(poolp, 2 * sizeof(void *));
	n = 0;
    } else {
	n = 0; p = *array;
	while (*p++)
	    n++;
	dest = (void **)PORT_ArenaGrow (poolp, 
			      *array,
			      (n + 1) * sizeof(void *),
			      (n + 2) * sizeof(void *));
    }

    if (dest == NULL)
	return SECFailure;

    dest[n] = obj;
    dest[n+1] = NULL;
    *array = dest;
    return SECSuccess;
}

/*
 * NSS_CMSArray_IsEmpty - check if array is empty
 */
PRBool
NSS_CMSArray_IsEmpty(void **array)
{
    return (array == NULL || array[0] == NULL);
}

/*
 * NSS_CMSArray_Count - count number of elements in array
 */
int
NSS_CMSArray_Count(void **array)
{
    int n = 0;

    if (array == NULL)
	return 0;

    while (*array++ != NULL)
	n++;

    return n;
}

/*
 * NSS_CMSArray_Sort - sort an array in place
 *
 * If "secondary" or "tertiary are not NULL, it must be arrays with the same
 *  number of elements as "primary". The same reordering will get applied to it.
 *
 * "compare" is a function that returns 
 *  < 0 when the first element is less than the second
 *  = 0 when the first element is equal to the second
 *  > 0 when the first element is greater than the second
 * to acheive ascending ordering.
 */
void
NSS_CMSArray_Sort(void **primary, int (*compare)(void *,void *), void **secondary, void **tertiary)
{
    int n, i, limit, lastxchg;
    void *tmp;

    n = NSS_CMSArray_Count(primary);

    PORT_Assert(secondary == NULL || NSS_CMSArray_Count(secondary) == n);
    PORT_Assert(tertiary == NULL || NSS_CMSArray_Count(tertiary) == n);
    
    if (n <= 1)	/* ordering is fine */
	return;
    
    /* yes, ladies and gentlemen, it's BUBBLE SORT TIME! */
    limit = n - 1;
    while (1) {
	lastxchg = 0;
	for (i = 0; i < limit; i++) {
	    if ((*compare)(primary[i], primary[i+1]) > 0) {
		/* exchange the neighbours */
		tmp = primary[i+1];
		primary[i+1] = primary[i];
		primary[i] = tmp;
		if (secondary) {		/* secondary array? */
		    tmp = secondary[i+1];	/* exchange there as well */
		    secondary[i+1] = secondary[i];
		    secondary[i] = tmp;
		}
		if (tertiary) {			/* tertiary array? */
		    tmp = tertiary[i+1];	/* exchange there as well */
		    tertiary[i+1] = tertiary[i];
		    tertiary[i] = tmp;
		}
		lastxchg = i+1;	/* index of the last element bubbled up */
	    }
	}
	if (lastxchg == 0)	/* no exchanges, so array is sorted */
	    break;		/* we're done */
	limit = lastxchg;	/* array is sorted up to [limit] */
    }
}

#if 0

/* array iterator stuff... not used */

typedef void **NSSCMSArrayIterator;

/* iterator */
NSSCMSArrayIterator
NSS_CMSArray_First(void **array)
{
    if (array == NULL || array[0] == NULL)
	return NULL;
    return (NSSCMSArrayIterator)&(array[0]);
}

void *
NSS_CMSArray_Obj(NSSCMSArrayIterator iter)
{
    void **p = (void **)iter;

    return *iter;	/* which is NULL if we are at the end of the array */
}

NSSCMSArrayIterator
NSS_CMSArray_Next(NSSCMSArrayIterator iter)
{
    void **p = (void **)iter;

    return (NSSCMSArrayIterator)(p + 1);
}

#endif
