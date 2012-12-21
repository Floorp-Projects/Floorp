/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMArray.h"
#include "nsCOMPtr.h"

// This specialization is private to nsCOMArray.
// It exists solely to automatically zero-out newly created array elements.
template<>
class nsTArrayElementTraits<nsISupports*>
{
    typedef nsISupports* E;
public:
    // Zero out the value
    static inline void Construct(E *e) {
        new (static_cast<void *>(e)) E();
    }
    // Invoke the copy-constructor in place.
    template<class A>
    static inline void Construct(E *e, const A &arg) {
        new (static_cast<void *>(e)) E(arg);
    }
    // Invoke the destructor in place.
    static inline void Destruct(E *e) {
        e->~E();
    }
};

static void ReleaseObjects(nsTArray<nsISupports*> &aArray);

// implementations of non-trivial methods in nsCOMArray_base

nsCOMArray_base::nsCOMArray_base(const nsCOMArray_base& aOther)
{
    // make sure we do only one allocation
    mArray.SetCapacity(aOther.Count());
    AppendObjects(aOther);
}

nsCOMArray_base::~nsCOMArray_base()
{
    Clear();
}

int32_t
nsCOMArray_base::IndexOf(nsISupports* aObject) const {
    return mArray.IndexOf(aObject);
}

int32_t
nsCOMArray_base::IndexOfObject(nsISupports* aObject) const {
    nsCOMPtr<nsISupports> supports = do_QueryInterface(aObject);
    NS_ENSURE_TRUE(supports, -1);

    uint32_t i, count;
    int32_t retval = -1;
    count = mArray.Length();
    for (i = 0; i < count; ++i) {
        nsCOMPtr<nsISupports> arrayItem = do_QueryInterface(mArray[i]);
        if (arrayItem == supports) {
            retval = i;
            break;
        }
    }
    return retval;
}

bool
nsCOMArray_base::EnumerateForwards(nsBaseArrayEnumFunc aFunc, void* aData) const
{
    for (uint32_t index = 0; index < mArray.Length(); index++)
        if (!(*aFunc)(mArray[index], aData))
            return false;

    return true;
}

bool
nsCOMArray_base::EnumerateBackwards(nsBaseArrayEnumFunc aFunc, void* aData) const
{
    for (uint32_t index = mArray.Length(); index--; )
        if (!(*aFunc)(mArray[index], aData))
            return false;

    return true;
}

int
nsCOMArray_base::nsCOMArrayComparator(const void* aElement1, const void* aElement2, void* aData)
{
    nsCOMArrayComparatorContext* ctx = static_cast<nsCOMArrayComparatorContext*>(aData);
    return (*ctx->mComparatorFunc)(*static_cast<nsISupports* const*>(aElement1),
                                   *static_cast<nsISupports* const*>(aElement2),
                                   ctx->mData);
}

void nsCOMArray_base::Sort(nsBaseArrayComparatorFunc aFunc, void* aData)
{
    if (mArray.Length() > 1) {
        nsCOMArrayComparatorContext ctx = {aFunc, aData};
        NS_QuickSort(mArray.Elements(), mArray.Length(), sizeof(nsISupports*),
                     nsCOMArrayComparator, &ctx);
    }
}

bool
nsCOMArray_base::InsertObjectAt(nsISupports* aObject, int32_t aIndex) {
    if ((uint32_t)aIndex > mArray.Length())
        return false;

    if (!mArray.InsertElementAt(aIndex, aObject))
        return false;

    NS_IF_ADDREF(aObject);
    return true;
}

bool
nsCOMArray_base::InsertObjectsAt(const nsCOMArray_base& aObjects, int32_t aIndex) {
    if ((uint32_t)aIndex > mArray.Length())
        return false;

    if (!mArray.InsertElementsAt(aIndex, aObjects.mArray))
        return false;

    // need to addref all these
    int32_t count = aObjects.Count();
    for (int32_t i = 0; i < count; ++i)
        NS_IF_ADDREF(aObjects.ObjectAt(i));

    return true;
}

bool
nsCOMArray_base::ReplaceObjectAt(nsISupports* aObject, int32_t aIndex)
{
    bool result = mArray.EnsureLengthAtLeast(aIndex + 1);
    if (result) {
        nsISupports *oldObject = mArray[aIndex];
        // Make sure to addref first, in case aObject == oldObject
        NS_IF_ADDREF(mArray[aIndex] = aObject);
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
nsCOMArray_base::RemoveObjectAt(int32_t aIndex)
{
    if (uint32_t(aIndex) < mArray.Length()) {
        nsISupports* element = mArray[aIndex];

        mArray.RemoveElementAt(aIndex);
        NS_IF_RELEASE(element);
        return true;
    }

    return false;
}

bool
nsCOMArray_base::RemoveObjectsAt(int32_t aIndex, int32_t aCount)
{
    if (uint32_t(aIndex) + uint32_t(aCount) <= mArray.Length()) {
        nsTArray<nsISupports*> elementsToDestroy(aCount);
        elementsToDestroy.AppendElements(mArray.Elements() + aIndex, aCount);
        mArray.RemoveElementsAt(aIndex, aCount);
        ReleaseObjects(elementsToDestroy);
        return true;
    }

    return false;
}

// useful for destructors
void
ReleaseObjects(nsTArray<nsISupports*> &aArray)
{
    for (uint32_t i = 0; i < aArray.Length(); i++)
        NS_IF_RELEASE(aArray[i]);
}

void
nsCOMArray_base::Clear()
{
    nsTArray<nsISupports*> objects;
    objects.SwapElements(mArray);
    ReleaseObjects(objects);
}

bool
nsCOMArray_base::SetCount(int32_t aNewCount)
{
    NS_ASSERTION(aNewCount >= 0,"SetCount(negative index)");
    if (aNewCount < 0)
        return false;

    int32_t count = mArray.Length();
    if (count > aNewCount)
        RemoveObjectsAt(aNewCount, mArray.Length() - aNewCount);
    return mArray.SetLength(aNewCount);
}

size_t
nsCOMArray_base::SizeOfExcludingThis(
                   nsBaseArraySizeOfElementIncludingThisFunc aSizeOfElementIncludingThis,
                   nsMallocSizeOfFun aMallocSizeOf, void* aData) const
{
    size_t n = mArray.SizeOfExcludingThis(aMallocSizeOf);

    if (aSizeOfElementIncludingThis)
        for (uint32_t index = 0; index < mArray.Length(); index++)
            n += aSizeOfElementIncludingThis(mArray[index], aMallocSizeOf, aData);

    return n;
}
