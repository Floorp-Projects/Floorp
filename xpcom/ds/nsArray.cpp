/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsIWeakReference.h"
#include "nsIWeakReferenceUtils.h"
#include "nsThreadUtils.h"

// used by IndexOf()
struct findIndexOfClosure
{
    nsISupports *targetElement;
    uint32_t startIndex;
    uint32_t resultIndex;
};

static bool FindElementCallback(void* aElement, void* aClosure);

NS_INTERFACE_MAP_BEGIN(nsArray)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIMutableArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMutableArray)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsArrayCC)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIMutableArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMutableArray)
NS_INTERFACE_MAP_END

nsArrayBase::~nsArrayBase()
{
    Clear();
}


NS_IMPL_ADDREF(nsArray)
NS_IMPL_RELEASE(nsArray)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsArrayCC)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsArrayCC)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsArrayCC)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsArrayCC)
    tmp->Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsArrayCC)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mArray)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP
nsArrayBase::GetLength(uint32_t* aLength)
{
    *aLength = mArray.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsArrayBase::QueryElementAt(uint32_t aIndex,
                            const nsIID& aIID,
                            void** aResult)
{
    nsISupports * obj = mArray.SafeObjectAt(aIndex);
    if (!obj) return NS_ERROR_ILLEGAL_VALUE;

    // no need to worry about a leak here, because SafeObjectAt()
    // doesn't addref its result
    return obj->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsArrayBase::IndexOf(uint32_t aStartIndex, nsISupports* aElement,
                     uint32_t* aResult)
{
    // optimize for the common case by forwarding to mArray
    if (aStartIndex == 0) {
        uint32_t idx = mArray.IndexOf(aElement);
        if (idx == UINT32_MAX)
            return NS_ERROR_FAILURE;

        *aResult = idx;
        return NS_OK;
    }

    findIndexOfClosure closure = { aElement, aStartIndex, 0 };
    bool notFound = mArray.EnumerateForwards(FindElementCallback, &closure);
    if (notFound)
        return NS_ERROR_FAILURE;

    *aResult = closure.resultIndex;
    return NS_OK;
}

NS_IMETHODIMP
nsArrayBase::Enumerate(nsISimpleEnumerator** aResult)
{
    return NS_NewArrayEnumerator(aResult, static_cast<nsIArray*>(this));
}

// nsIMutableArray implementation

NS_IMETHODIMP
nsArrayBase::AppendElement(nsISupports* aElement, bool aWeak)
{
    bool result;
    if (aWeak) {
        nsCOMPtr<nsIWeakReference> elementRef = do_GetWeakReference(aElement);
        NS_ASSERTION(elementRef, "AppendElement: Trying to use weak references on an object that doesn't support it");
        if (!elementRef)
            return NS_ERROR_FAILURE;
        result = mArray.AppendObject(elementRef);
    }

    else {
        // add the object directly
        result = mArray.AppendObject(aElement);
    }
    return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::RemoveElementAt(uint32_t aIndex)
{
    bool result = mArray.RemoveObjectAt(aIndex);
    return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::InsertElementAt(nsISupports* aElement, uint32_t aIndex, bool aWeak)
{
    nsCOMPtr<nsISupports> elementRef;
    if (aWeak) {
        elementRef = do_GetWeakReference(aElement);
        NS_ASSERTION(elementRef, "InsertElementAt: Trying to use weak references on an object that doesn't support it");
        if (!elementRef)
            return NS_ERROR_FAILURE;
    } else {
        elementRef = aElement;
    }
    bool result = mArray.InsertObjectAt(elementRef, aIndex);
    return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::ReplaceElementAt(nsISupports* aElement, uint32_t aIndex, bool aWeak)
{
    nsCOMPtr<nsISupports> elementRef;
    if (aWeak) {
        elementRef = do_GetWeakReference(aElement);
        NS_ASSERTION(elementRef, "ReplaceElementAt: Trying to use weak references on an object that doesn't support it");
        if (!elementRef)
            return NS_ERROR_FAILURE;
    } else {
        elementRef = aElement;
    }
    bool result = mArray.ReplaceObjectAt(elementRef, aIndex);
    return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::Clear()
{
    mArray.Clear();
    return NS_OK;
}

//
// static helper routines
//
bool
FindElementCallback(void *aElement, void* aClosure)
{
    findIndexOfClosure* closure =
        static_cast<findIndexOfClosure*>(aClosure);

    nsISupports* element =
        static_cast<nsISupports*>(aElement);

    // don't start searching until we're past the startIndex
    if (closure->resultIndex >= closure->startIndex &&
        element == closure->targetElement) {
        return false;    // stop! We found it
    }
    closure->resultIndex++;

    return true;
}

nsresult
nsArrayBase::XPCOMConstructor(nsISupports* aOuter, const nsIID& aIID, void** aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsCOMPtr<nsIMutableArray> inst = Create();
    return inst->QueryInterface(aIID, aResult);
}

already_AddRefed<nsIMutableArray>
nsArrayBase::Create()
{
    nsCOMPtr<nsIMutableArray> inst;
    if (NS_IsMainThread()) {
        inst = new nsArrayCC;
    } else {
        inst = new nsArray;
    }
    return inst.forget();
}
