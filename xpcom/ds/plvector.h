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

#ifndef plvector_h__
#define plvector_h__

#include "prtypes.h"
#include "prlog.h" 

PR_BEGIN_EXTERN_C

/* Vectors are extensible arrays */

typedef struct PLVector {
    void**      data;     /* the actual array of data */
    PRUint32    size;     /* # of elements (upperBound - 1) */
    PRUint32    maxSize;  /* max allocated */
    PRInt32     growBy;   /* grow amount */
} PLVector;

PR_EXTERN(PLVector*)
PL_NewVector(PRUint32 initialSize, PRInt32 initialGrowBy);

PR_EXTERN(void)
PL_VectorDestroy(PLVector* v);

/* Initializes an existing vector */
PR_EXTERN(void)
PL_VectorInitialize(PLVector* v, PRUint32 initialSize, PRInt32 initialGrowBy);

/* Destroys the elements, but doesn't free the vector */
PR_EXTERN(void)
PL_VectorFinalize(PLVector* v);

#define PL_VectorGetSize(v)     ((v)->size)

#define PL_VECTOR_GROW_DEFAULT  (-1)

PR_EXTERN(PRBool)
PL_VectorSetSize(PLVector* v, PRUint32 newSize, PRInt32 growBy);

PR_EXTERN(PRBool)
PL_VectorIsValidIndex(PLVector* v, PRUint32 index);

PR_EXTERN(void)
PL_VectorCompact(PLVector* v);

PR_EXTERN(void)
PL_VectorCopy(PLVector* dstVector, PRUint32 dstPosition,
              PLVector* srcVector, PRUint32 srcPosition, PRUint32 length);

PR_EXTERN(PLVector*)
PL_VectorClone(PLVector* v);

/* Accessing elements */

#define PL_VectorGetAddr(v, index) (PR_ASSERT((index) < (v)->size), &(v)->data[index])

#define PL_VectorGet(v, index)     (*PL_VectorGetAddr(v, index))

PR_EXTERN(void)
PL_VectorSet(PLVector* v, PRUint32 index, void* newElement);

/* Adds at the end */
PR_EXTERN(PRUint32)
PL_VectorAdd(PLVector* v, void* newElement);

/* Inserts new element count times at index */
PR_EXTERN(void)
PL_VectorInsert(PLVector* v, PRUint32 index, void* newElement, PRUint32 count);

/* Removes count elements at index */
PR_EXTERN(void)
PL_VectorRemove(PLVector* v, PRUint32 index, PRUint32 count);

#ifdef DEBUG

PR_EXTERN(void)
PL_VectorAssertValid(PLVector* v);

#endif

PR_END_EXTERN_C

#endif /* plvector_h__ */
