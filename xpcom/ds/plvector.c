/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "plvector.h"
#include "prmem.h"
#include <string.h>

#ifdef XP_WIN16
#define SIZE_T_MAX 0xFF80 /* a little less than 64K, the max alloc size on win16. */
#define MAX_ARR_ELEMS SIZE_T_MAX/sizeof(void*)
#endif

PR_IMPLEMENT(PLVector*)
PL_NewVector(PRUint32 initialSize, PRInt32 initialGrowBy)
{
    PLVector* v = (PLVector*)PR_Malloc(sizeof(PLVector*));
    if (v == NULL)
        return NULL;
    PL_VectorInitialize(v, initialSize, initialGrowBy);
    return v;
}

PR_IMPLEMENT(void)
PL_VectorDestroy(PLVector* v)
{
    PL_VectorFinalize(v);
    PR_Free(v);
}

/* Initializes an existing vector */
PR_IMPLEMENT(void)
PL_VectorInitialize(PLVector* v, PRUint32 initialSize, PRInt32 initialGrowBy)
{
    v->data = NULL;
    v->size = v->maxSize = v->growBy = 0;
    if (initialSize > 0 || initialGrowBy > 0)
        PL_VectorSetSize(v, initialSize, initialGrowBy);
}

/* Destroys the elements, but doesn't free the vector */
PR_IMPLEMENT(void)
PL_VectorFinalize(PLVector* v)
{
    /* This implementation doesn't do anything to delete the elements
       in the vector -- that's up to the caller. (Don't shoot me,
       I just copied the code from libmsg.) */
    PR_Free(v->data);
}

PR_IMPLEMENT(PRBool)
PL_VectorSetSize(PLVector* v, PRUint32 newSize, PRInt32 growBy)
{
    if (growBy != -1)
        v->growBy = growBy;  /* set new size */

    if (newSize == 0) {
        /* shrink to nothing */
        PR_Free(v->data);
        v->data = NULL;
        v->size = v->maxSize = 0;
    }
    else if (v->data == NULL) {
        /* create one with exact size */
#ifdef SIZE_T_MAX
        PR_ASSERT(newSize <= SIZE_T_MAX/sizeof(void*));    /* no overflow */
#endif
        v->data = (void**)PR_Malloc(newSize * sizeof(void *));
        if (v->data == NULL) {
            v->size = 0;
            return PR_FALSE;
        }

        memset(v->data, 0, newSize * sizeof(void *));  /* zero fill */

        v->size = v->maxSize = newSize;
    }
    else if (newSize <= v->maxSize) {
        /* it fits */
        if (newSize > v->size) {
            /* initialize the new elements */

            memset(&v->data[v->size], 0, (newSize-v->size) * sizeof(void *));
        }
        v->size = newSize;
    }
    else {
        /* otherwise, grow array */
        PRUint32 newMax;
        void** newData;
        PRInt32 ngrowBy = v->growBy;
        if (ngrowBy == 0) {
            /* heuristically determine growth when ngrowBy == 0
               (this avoids heap fragmentation in many situations) */
            ngrowBy = PR_MIN(1024, PR_MAX(4, v->size / 8));
        }
#ifdef MAX_ARR_ELEMS
        if (v->size + ngrowBy > MAX_ARR_ELEMS)
            ngrowBy = MAX_ARR_ELEMS - v->size;
#endif
        if (newSize < v->maxSize + ngrowBy)
            newMax = v->maxSize + ngrowBy;  /* granularity */
        else
            newMax = newSize;  /* no slush */

#ifdef SIZE_T_MAX
        if (newMax >= SIZE_T_MAX/sizeof(void*))
            return PR_FALSE;
        PR_ASSERT(newMax <= SIZE_T_MAX/sizeof(void*)); /* no overflow */
#endif
        PR_ASSERT(newMax >= v->maxSize);  /* no wrap around */
        newData = (void**)PR_Malloc(newMax * sizeof(void*));

        if (newData != NULL) {
            /* copy new data from old */
            memcpy(newData, v->data, v->size * sizeof(void*));

            /* construct remaining elements */
            PR_ASSERT(newSize > v->size);

            memset(&newData[v->size], 0, (newSize-v->size) * sizeof(void*));

            /* get rid of old stuff (note: no destructors called) */
            PR_Free(v->data);
            v->data = newData;
            v->size = newSize;
            v->maxSize = newMax;
        }
        else {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

PR_IMPLEMENT(PRBool)
PL_VectorIsValidIndex(PLVector* v, PRUint32 index)
{
    return (index < v->size) ? PR_TRUE : PR_FALSE;
}


PR_IMPLEMENT(void)
PL_VectorCompact(PLVector* v)
{
    if (v->size != v->maxSize) {
        /* shrink to desired size */
#ifdef SIZE_T_MAX
        PR_ASSERT(v->size <= SIZE_T_MAX/sizeof(void *)); /* no overflow */
#endif
        void ** newData = NULL;
        if (v->size != 0) {
            newData = (void **)PR_Malloc(v->size * sizeof(void *));
            /* copy new data from old */
            memcpy(newData, v->data, v->size * sizeof(void *));
        }

        /* get rid of old stuff (note: no destructors called) */
        PR_Free(v->data);
        v->data = newData;
        v->maxSize = v->size;
    }
}

#if 0   /* becomes Copy */
PR_IMPLEMENT(void)
PL_VectorSplice(PLVector* v, PRUint32 startIndex, PLVector* newVector)
{
    PRUint32 i;
    PR_ASSERT(newVector != NULL);

    if (PL_VectorGetSize(newVector) > 0) {
        PL_VectorInsert(v, startIndex, PL_VectorGet(newVector, 0), 
                        PL_VectorGetSize(newVector));
        for (i = 0; i < PL_VectorGetSize(newVector); i++)
            PL_VectorSet(v, startIndex + i, PL_VectorGet(newVector, i));
    }
}
#endif

PR_IMPLEMENT(void)
PL_VectorCopy(PLVector* dstVector, PRUint32 dstPosition,
              PLVector* srcVector, PRUint32 srcPosition, PRUint32 length)
{
    PR_ASSERT(0);       /* XXX not implemented yet */
#if 0
    PL_VectorSetSize(dstVector, PR_MAX(PL_VectorGetSize(dstVector),
                                       PL_VectorGetSize(srcVector)),
                     PL_VECTOR_GROW_DEFAULT);

    if (v->data)
        PR_Free(v->data);
    v->size = oldA->v->size;
    v->maxSize = oldA->v->maxSize;
    v->growBy = oldA->v->growBy;
    v->data = (void**)PR_Malloc(v->size * sizeof(void *));
    if (v->data == NULL) {
        v->size = 0;
    }
    else {
        memcpy(v->data, oldA->v->data, v->size * sizeof(void *));
    }
#endif
}

PR_IMPLEMENT(PLVector*)
PL_VectorClone(PLVector* v)
{
    PLVector* newVec = PL_NewVector(v->size, v->growBy);
    PL_VectorCopy(newVec, 0, v, 0, v->size);
    return newVec;
}

/* Accessing elements */

PR_IMPLEMENT(void)
PL_VectorSet(PLVector* v, PRUint32 index, void* newElement)
{
    if (index >= v->size) {
        if (!PL_VectorSetSize(v, index+1, PL_VECTOR_GROW_DEFAULT))
            return;
    }
    v->data[index] = newElement;
}

/* Adds at the end */
PR_IMPLEMENT(PRUint32)
PL_VectorAdd(PLVector* v, void* newElement)
{
    PRUint32 index = v->size;
#ifdef XP_WIN16
    if (index >= SIZE_T_MAX / 4L) {
        return -1;	     
    }
#endif			
    PL_VectorSet(v, index, newElement);
    return index;
}

/* Inserts new element count times at index */
PR_IMPLEMENT(void)
PL_VectorInsert(PLVector* v, PRUint32 index, void* newElement, PRUint32 count)
{
    PR_ASSERT(count > 0);     /* zero or negative size not allowed */

    if (index >= v->size) {
        /* adding after the end of the array */
        if (!PL_VectorSetSize(v, index + count, PL_VECTOR_GROW_DEFAULT))
            return;  /* grow so index is valid */
    }
    else {
        /* inserting in the middle of the array */
        PRUint32 nOldSize = v->size;
        if (!PL_VectorSetSize(v, v->size + count, PL_VECTOR_GROW_DEFAULT))
            return;  /* grow it to new size */
        /* shift old data up to fill gap */
        memmove(&v->data[index+count], &v->data[index],
                (nOldSize-index) * sizeof(void *));

        /* re-init slots we copied from */
        memset(&v->data[index], 0, count * sizeof(void *));
    }

    /* insert new value in the gap */
    PR_ASSERT(index + count <= v->size);
    while (count--)
        v->data[index++] = newElement;
}

/* Removes count elements at index */
PR_IMPLEMENT(void)
PL_VectorRemove(PLVector* v, PRUint32 index, PRUint32 count)
{
    PRUint32 moveCount;
    /* PR_ASSERT(count >= 0); */
    PR_ASSERT(index + count <= v->size);

    /* just remove a range */
    moveCount = v->size - (index + count);

    if (moveCount)
        memmove(&v->data[index], &v->data[index + count],
                moveCount * sizeof(void *));
    v->size -= count;
}

#ifdef DEBUG

PR_IMPLEMENT(void)
PL_VectorAssertValid(PLVector* v)
{
    if (v->data == NULL) {
        PR_ASSERT(v->size == 0);
        PR_ASSERT(v->maxSize == 0);
    }
    else {        
        /* PR_ASSERT(v->size >= 0); */
        /* PR_ASSERT(v->maxSize >= 0); */
        PR_ASSERT(v->size <= v->maxSize);
    }
}

#endif

