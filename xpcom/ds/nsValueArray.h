/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is nsValueArray.h/nsValueArray.cpp code, released
 * Dec 28, 2001.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Garrett Arch Blythe, 20-December-2001
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */
#ifndef nsValueArray_h___
#define nsValueArray_h___

//
// nsValueArray.h
//
// Implement an array class to store unsigned integer values.
// The maximum value must be known up front.  Once known, the
//  smallest memory representation will be attempted; i.e. if the
//  maximum value was 1275, then 2 bytes (uint16) would represent each value
//  in the array instead of 4 bytes (uint32).
//
#include "nscore.h"

typedef PRUint32 nsValueArrayCount;
typedef PRUint32 nsValueArrayIndex;
typedef PRUint32 nsValueArrayValue;
#define NSVALUEARRAY_INVALID ((nsValueArrayValue)-1)

class NS_COM nsValueArray {
  public:
    nsValueArray(nsValueArrayValue aMaxValue,
                 nsValueArrayCount aInitialCapacity = 0);
    ~nsValueArray();

    //
    // Assignment.
    //
  public:
    nsValueArray& operator=(const nsValueArray& other);

    //
    // Array size information.
    // Ability to add more values without growing is Capacity - Count.
    //
  public:
    inline nsValueArrayCount Count() const {
        return mCount;
    }

    inline nsValueArrayCount Capacity() const {
        return mCapacity;
    }

    void Compact();

    //
    // Array access.
    //
  public:
    nsValueArrayValue ValueAt(nsValueArrayIndex aIndex) const;

    inline nsValueArrayValue operator[](nsValueArrayIndex aIndex) const {
        return ValueAt(aIndex);
    }

    nsValueArrayIndex IndexOf(nsValueArrayValue aPossibleValue) const;

    inline PRBool AppendValue(nsValueArrayValue aValue) {
        return InsertValueAt(aValue, Count());
    }

    inline PRBool RemoveValue(nsValueArrayValue aValue) {
        return  RemoveValueAt(IndexOf(aValue));
    }

    PRBool InsertValueAt(nsValueArrayValue aValue, nsValueArrayIndex aIndex);

    PRBool RemoveValueAt(nsValueArrayIndex aIndex);

    //
    // Data members.
    //
  private:
    nsValueArrayCount mCount;
    nsValueArrayCount mCapacity;
    PRUint8* mValueArray;
    PRUint8 mBytesPerValue;
};

#endif /* nsValueArray_h___ */
