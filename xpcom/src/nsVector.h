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

#ifndef nsVector_h__
#define nsVector_h__

#include "plvector.h"
#include "nsCom.h"

class nsVector : public PLVector {
public:
// Construction
    nsVector(PRUint32 initialSize = 0, PRInt32 initialGrowBy = 0) {
        PL_VectorInitialize(this, initialSize, initialGrowBy);
    }
    ~nsVector(void) { PL_VectorFinalize(this); }

// Attributes
    PRUint32 GetSize(void) const { return PL_VectorGetSize(this); }
    PRUint32 GetUpperBound(void) const { return GetSize() - 1; }
    PRBool SetSize(PRUint32 nNewSize, PRInt32 nGrowBy = PL_VECTOR_GROW_DEFAULT) {
        return PL_VectorSetSize(this, nNewSize, nGrowBy);
    }
    PRBool IsValidIndex(PRUint32 index) { return PL_VectorIsValidIndex(this, index); }

// Operations
    // Clean up
    void Compact(void) { PL_VectorCompact(this); }
    void RemoveAll(void) { SetSize(0); }
    void Copy(nsVector* src, PRUint32 len, PRUint32 dstPos = 0, PRUint32 srcPos = 0) {
        PL_VectorCopy(this, dstPos, src, srcPos, len);
    }

    // Accessing elements
    void* Get(PRUint32 index) const { return PL_VectorGet(this, index); }
    void Set(PRUint32 index, void* newElement) { PL_VectorSet(this, index, newElement); }
    void*& ElementAt(PRUint32 index) { return *PL_VectorGetAddr(this, index); }

    // Potentially growing the array
    PRInt32 Add(void* newElement) { return PL_VectorAdd(this, newElement); }

    // overloaded operator helpers
    void* operator[](PRUint32 index) const { return Get(index); }
    void*& operator[](PRUint32 index) { return ElementAt(index); }

    // Operations that move elements around
    void Insert(PRUint32 index, void* newElement, PRInt32 count = 1) {
        PL_VectorInsert(this, index, newElement, count);
    }
    void Remove(PRUint32 index, PRInt32 count = 1) {
        PL_VectorRemove(this, index, count);
    }

#ifdef DEBUG
    void AssertValid(void) const { PL_VectorAssertValid((PLVector*)this); }
#endif
};

#endif
