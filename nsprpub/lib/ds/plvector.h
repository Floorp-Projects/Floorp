/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
PR_EXTERN(PRInt32)
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
