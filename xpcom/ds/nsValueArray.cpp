/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsValueArray.h/nsValueArray.cpp code, released
 * Dec 28, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Blythe, 20-December-2001
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// nsValueArray.cpp
//
// Implement an array class to store unsigned integer values.
// The maximum value must be known up front.  Once known, the
//  smallest memory representation will be attempted; i.e. if the
//  maximum value was 1275, then 2 bytes (uint16) would represent each value
//  in the array instead of 4 bytes (uint32).
//
#include "nsValueArray.h"
#include "nsCRT.h"
#include "prmem.h"
#include "prbit.h"

#define NSVALUEARRAY_LINEAR_GROWBY 8
#define NSVALUEARRAY_LINEAR_THRESHOLD  128

nsValueArray::nsValueArray(nsValueArrayValue aMaxValue, nsValueArrayCount aInitialCapacity) {
    mCount = 0;
    mCapacity = 0;
    mValueArray = nsnull;

    PRUint8 test8 = (PRUint8)aMaxValue;
    PRUint16 test16 = (PRUint16)aMaxValue;
    PRUint32 test32 = (PRUint32)aMaxValue;
    if ((nsValueArrayValue)test8 == aMaxValue) {
        mBytesPerValue = sizeof(test8);
    }
    else if ((nsValueArrayValue)test16 == aMaxValue) {
        mBytesPerValue = sizeof(test16);
    }
    else if ((nsValueArrayValue)test32 == aMaxValue) {
        mBytesPerValue = sizeof(test32);
    }
    else {
        NS_ASSERTION(0, "not supported yet, add it yourself...");
        mBytesPerValue = 0;
    }

    if (aInitialCapacity) {
        mValueArray = (PRUint8*)PR_Malloc(aInitialCapacity * mBytesPerValue);
        if (nsnull != mValueArray) {
            mCapacity = aInitialCapacity;
        }
    }
}

nsValueArray::~nsValueArray() {
    if (nsnull != mValueArray) {
        PR_Free(mValueArray);
        mValueArray = nsnull;
    }
}

//
// Copy it.
//
nsValueArray& nsValueArray::operator=(const nsValueArray& aOther) {
    //
    // Free off what you know if not enough space, or units differ.
    //
    if ((mBytesPerValue != aOther.mBytesPerValue || mCapacity < aOther.mCount) && nsnull != mValueArray) {
        PR_Free(mValueArray);
        mValueArray = nsnull;
        mCount = mCapacity = 0;
    }

    //
    // Copy some attribs.
    //
    mBytesPerValue = aOther.mBytesPerValue;
    mCount = aOther.mCount;

    //
    // Anything to do?
    //
    if (0 != mCount) {
        //
        // May need to allocate our buffer.
        //
        if (0 == mCapacity) {
            mValueArray = (PRUint8*)PR_Malloc(mCount * mBytesPerValue);
            mCapacity = mCount;
        }

        NS_ASSERTION(nsnull != mValueArray, "loss of value array assignment and original data.");
        if (nsnull != mValueArray) {
            memcpy(mValueArray, aOther.mValueArray, mCount * mBytesPerValue);
        }
        else {
            mCount = mCapacity = 0;
        }
    }
    
    return *this;
}

//
// Insert a value into the array.
// No validity checking other than index is done.
//
PRBool nsValueArray::InsertValueAt(nsValueArrayValue aValue, nsValueArrayIndex aIndex) {
    PRBool retval = PR_FALSE;

    nsValueArrayCount count = Count();
    if (aIndex <= count) {
        //
        // If we're at capacity, then we'll need to grow a little.
        //
        if (Capacity() == count) {
            PRUint8* reallocRes = nsnull;
            nsValueArrayCount growBy = NSVALUEARRAY_LINEAR_GROWBY;

            //
            // Up to a particular limit we grow in small increments.
            // Otherwise, grow exponentially.
            //
            if (count >= NSVALUEARRAY_LINEAR_THRESHOLD) {
                growBy = PR_BIT(PR_CeilingLog2(count + 1)) - count;
            }

            if (nsnull == mValueArray) {
                reallocRes = (PRUint8*)PR_Malloc((count + growBy) * mBytesPerValue);
            }
            else {
                reallocRes = (PRUint8*)PR_Realloc(mValueArray, (count + growBy) * mBytesPerValue);
            }
            if (nsnull != reallocRes) {
                mValueArray = reallocRes;
                mCapacity += growBy;
            }
        }

        //
        // Only if we are below capacity do we continue.
        //
        if (Capacity() > count) {
            //
            // All those at and beyond the insertion point need to move.
            //
            if (aIndex < count) {
                memmove(&mValueArray[(aIndex + 1) * mBytesPerValue], &mValueArray[aIndex * mBytesPerValue], (count - aIndex) * mBytesPerValue);
            }

            //
            // Do the assignment.
            //
            switch (mBytesPerValue) {
                case sizeof(PRUint8):
                    *((PRUint8*)&mValueArray[aIndex * mBytesPerValue]) = (PRUint8)aValue;
                    NS_ASSERTION(*((PRUint8*)&mValueArray[aIndex * mBytesPerValue]) == aValue, "Lossy value array detected.  Report a higher maximum upon construction!");
                    break;
                case sizeof(PRUint16):
                    *((PRUint16*)&mValueArray[aIndex * mBytesPerValue]) = (PRUint16)aValue;
                    NS_ASSERTION(*((PRUint16*)&mValueArray[aIndex * mBytesPerValue]) == aValue, "Lossy value array detected.  Report a higher maximum upon construction!");
                    break;
                case sizeof(PRUint32):
                    *((PRUint32*)&mValueArray[aIndex * mBytesPerValue]) = (PRUint32)aValue;
                    NS_ASSERTION(*((PRUint32*)&mValueArray[aIndex * mBytesPerValue]) == aValue, "Lossy value array detected.  Report a higher maximum upon construction!");
                    break;
                default:
                    NS_ASSERTION(0, "surely you've been warned prior to this!");
                    break;
            }

            //
            // Up the count by 1.
            //
            mCount++;
        }
    }

    return retval;
}

//
// Remove the index from the value array.
// The array does not shrink until Compact() is invoked.
//
PRBool nsValueArray::RemoveValueAt(nsValueArrayIndex aIndex) {
    PRBool retval = PR_FALSE;

    nsValueArrayCount count = Count();
    if (aIndex < count) {
        //
        // Move memory around if appropriate.
        //
        if (aIndex != (count - 1)) {
            memmove(&mValueArray[aIndex * mBytesPerValue], &mValueArray[(aIndex + 1) * mBytesPerValue], (count - aIndex - 1) * mBytesPerValue);
        }

        //
        // Update our count.
        //
        mCount--;
    }

    return retval;
}

//
// Shrink as much as possible.
//
void nsValueArray::Compact() {
    nsValueArrayCount count = Count();
    if (Capacity() != count)
    {
        if (0 == count) {
            PR_Free(mValueArray);
            mValueArray = nsnull;
            mCapacity = 0;
        }
        else {
            PRUint8* reallocRes = (PRUint8*)PR_Realloc(mValueArray, count * mBytesPerValue);
            if (nsnull != reallocRes) {
                mValueArray = reallocRes;
                mCapacity = count;
            }
        }
    }
}

//
// Return the value at the index.
//
nsValueArrayValue nsValueArray::ValueAt(nsValueArrayIndex aIndex) const {
    nsValueArrayValue retval = NSVALUEARRAY_INVALID;
    
    if (aIndex < Count()) {
        switch (mBytesPerValue) {
            case sizeof(PRUint8):
                retval = (nsValueArrayIndex)*((PRUint8*)&mValueArray[aIndex * mBytesPerValue]);
                break;
            case sizeof(PRUint16):
                retval = (nsValueArrayIndex)*((PRUint16*)&mValueArray[aIndex * mBytesPerValue]);
                break;
            case sizeof(PRUint32):
                retval = (nsValueArrayIndex)*((PRUint32*)&mValueArray[aIndex * mBytesPerValue]);
                break;
            default:
                NS_ASSERTION(0, "unexpected for sure.");
                break;
        }
    }
    
    return retval;
}

//
// Return the first encountered index of the value.
//
nsValueArrayIndex nsValueArray::IndexOf(nsValueArrayValue aPossibleValue) const {
    nsValueArrayIndex retval = NSVALUEARRAY_INVALID;
    nsValueArrayIndex traverse;
    
    for (traverse = 0; traverse < Count(); traverse++) {
        if (aPossibleValue == ValueAt(traverse)) {
            retval = traverse;
            break;
        }
    }
    
    return retval;
}
