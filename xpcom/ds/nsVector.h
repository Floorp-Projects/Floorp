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
    PRBool IsValidIndex(PRUint32 indx) { return PL_VectorIsValidIndex(this, indx); }

// Operations
    // Clean up
    void Compact(void) { PL_VectorCompact(this); }
    void RemoveAll(void) { SetSize(0); }
    void Copy(nsVector* src, PRUint32 len, PRUint32 dstPos = 0, PRUint32 srcPos = 0) {
        PL_VectorCopy(this, dstPos, src, srcPos, len);
    }

    // Accessing elements
    void* Get(PRUint32 indx) const { return PL_VectorGet(this, indx); }
    void Set(PRUint32 indx, void* newElement) { PL_VectorSet(this, indx, newElement); }
    void*& ElementAt(PRUint32 indx) { return *PL_VectorGetAddr(this, indx); }

    // Potentially growing the array
    PRInt32 Add(void* newElement) { return PL_VectorAdd(this, newElement); }

    // overloaded operator helpers
    void* operator[](PRUint32 indx) const { return Get(indx); }
    void*& operator[](PRUint32 indx) { return ElementAt(indx); }

    // Operations that move elements around
    void Insert(PRUint32 indx, void* newElement, PRInt32 count = 1) {
        PL_VectorInsert(this, indx, newElement, count);
    }
    void Remove(PRUint32 indx, PRInt32 count = 1) {
        PL_VectorRemove(this, indx, count);
    }

#ifdef DEBUG
    void AssertValid(void) const { PL_VectorAssertValid((PLVector*)this); }
#endif
};

#endif
