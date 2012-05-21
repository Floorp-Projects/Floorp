/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMArray.h"
#include "nsCOMPtr.h"

static bool ReleaseObjects(void* aElement, void*);

// implementations of non-trivial methods in nsCOMArray_base

// copy constructor - we can't just memcpy here, because
// we have to make sure we own our own array buffer, and that each
// object gets another AddRef()
nsCOMArray_base::nsCOMArray_base(const nsCOMArray_base& aOther)
{
    // make sure we do only one allocation
    mArray.SizeTo(aOther.Count());
    AppendObjects(aOther);
}

nsCOMArray_base::~nsCOMArray_base()
{
  Clear();
}

PRInt32
nsCOMArray_base::IndexOfObject(nsISupports* aObject) const {
    nsCOMPtr<nsISupports> supports = do_QueryInterface(aObject);
    NS_ENSURE_TRUE(supports, -1);

    PRInt32 i, count;
    PRInt32 retval = -1;
    count = mArray.Count();
    for (i = 0; i < count; ++i) {
        nsCOMPtr<nsISupports> arrayItem =
            do_QueryInterface(reinterpret_cast<nsISupports*>(mArray.ElementAt(i)));
        if (arrayItem == supports) {
            retval = i;
            break;
        }
    }
    return retval;
}

bool
nsCOMArray_base::InsertObjectAt(nsISupports* aObject, PRInt32 aIndex) {
    bool result = mArray.InsertElementAt(aObject, aIndex);
    if (result)
        NS_IF_ADDREF(aObject);
    return result;
}

bool
nsCOMArray_base::InsertObjectsAt(const nsCOMArray_base& aObjects, PRInt32 aIndex) {
    bool result = mArray.InsertElementsAt(aObjects.mArray, aIndex);
    if (result) {
        // need to addref all these
        PRInt32 count = aObjects.Count();
        for (PRInt32 i = 0; i < count; ++i) {
            NS_IF_ADDREF(aObjects.ObjectAt(i));
        }
    }
    return result;
}

bool
nsCOMArray_base::ReplaceObjectAt(nsISupports* aObject, PRInt32 aIndex)
{
    // its ok if oldObject is null here
    nsISupports *oldObject =
        reinterpret_cast<nsISupports*>(mArray.SafeElementAt(aIndex));

    bool result = mArray.ReplaceElementAt(aObject, aIndex);

    // ReplaceElementAt could fail, such as if the array grows
    // so only release the existing object if the replacement succeeded
    if (result) {
        // Make sure to addref first, in case aObject == oldObject
        NS_IF_ADDREF(aObject);
        NS_IF_RELEASE(oldObject);
    }
    return result;
}

bool
nsCOMArray_base::RemoveObject(nsISupports *aObject)
{
    bool result = mArray.RemoveElement(aObject);
    if (result)
        NS_IF_RELEASE(aObject);
    return result;
}

bool
nsCOMArray_base::RemoveObjectAt(PRInt32 aIndex)
{
    if (PRUint32(aIndex) < PRUint32(Count())) {
        nsISupports* element = ObjectAt(aIndex);

        bool result = mArray.RemoveElementAt(aIndex);
        NS_IF_RELEASE(element);
        return result;
    }

    return false;
}

bool
nsCOMArray_base::RemoveObjectsAt(PRInt32 aIndex, PRInt32 aCount)
{
    if (PRUint32(aIndex) + PRUint32(aCount) <= PRUint32(Count())) {
        nsVoidArray elementsToDestroy(aCount);
        for (PRInt32 i = 0; i < aCount; ++i) {
            elementsToDestroy.InsertElementAt(mArray.FastElementAt(aIndex + i), i);
        }
        bool result = mArray.RemoveElementsAt(aIndex, aCount);
        for (PRInt32 i = 0; i < aCount; ++i) {
            nsISupports* element = static_cast<nsISupports*> (elementsToDestroy.FastElementAt(i));
            NS_IF_RELEASE(element);
        }
        return result;
    }

    return false;
}

// useful for destructors
bool
ReleaseObjects(void* aElement, void*)
{
    nsISupports* element = static_cast<nsISupports*>(aElement);
    NS_IF_RELEASE(element);
    return true;
}

void
nsCOMArray_base::Clear()
{
    nsAutoVoidArray objects;
    objects = mArray;
    mArray.Clear();
    objects.EnumerateForwards(ReleaseObjects, nsnull);
}

bool
nsCOMArray_base::SetCount(PRInt32 aNewCount)
{
    NS_ASSERTION(aNewCount >= 0,"SetCount(negative index)");
    if (aNewCount < 0)
      return false;

    PRInt32 count = Count(), i;
    nsAutoVoidArray objects;
    if (count > aNewCount) {
        objects.SetCount(count - aNewCount);
        for (i = aNewCount; i < count; ++i) {
            objects.ReplaceElementAt(ObjectAt(i), i - aNewCount);
        }
    }
    bool result = mArray.SetCount(aNewCount);
    objects.EnumerateForwards(ReleaseObjects, nsnull);
    return result;
}

