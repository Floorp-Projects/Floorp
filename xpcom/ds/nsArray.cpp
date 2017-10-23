/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsThreadUtils.h"

// used by IndexOf()
struct MOZ_STACK_CLASS findIndexOfClosure
{
  // This is only used for pointer comparison, so we can just use a void*.
  void* targetElement;
  uint32_t startIndex;
  uint32_t resultIndex;
};

static bool FindElementCallback(void* aElement, void* aClosure);

NS_INTERFACE_MAP_BEGIN(nsArray)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIArrayExtensions)
  NS_INTERFACE_MAP_ENTRY(nsIMutableArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMutableArray)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsArrayCC)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIArrayExtensions)
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
  nsISupports* obj = mArray.SafeObjectAt(aIndex);
  if (!obj) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

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
    if (idx == UINT32_MAX) {
      return NS_ERROR_FAILURE;
    }

    *aResult = idx;
    return NS_OK;
  }

  findIndexOfClosure closure = { aElement, aStartIndex, 0 };
  bool notFound = mArray.EnumerateForwards(FindElementCallback, &closure);
  if (notFound) {
    return NS_ERROR_FAILURE;
  }

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
nsArrayBase::AppendElement(nsISupports* aElement)
{
  bool result = mArray.AppendObject(aElement);
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::RemoveElementAt(uint32_t aIndex)
{
  bool result = mArray.RemoveObjectAt(aIndex);
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::InsertElementAt(nsISupports* aElement, uint32_t aIndex)
{
  bool result = mArray.InsertObjectAt(aElement, aIndex);
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::ReplaceElementAt(nsISupports* aElement, uint32_t aIndex)
{
  mArray.ReplaceObjectAt(aElement, aIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsArrayBase::Clear()
{
  mArray.Clear();
  return NS_OK;
}

// nsIArrayExtensions implementation.

NS_IMETHODIMP
nsArrayBase::Count(uint32_t* aResult)
{
  return GetLength(aResult);
}

NS_IMETHODIMP
nsArrayBase::GetElementAt(uint32_t aIndex, nsISupports** aResult)
{
  nsCOMPtr<nsISupports> obj = mArray.SafeObjectAt(aIndex);
  obj.forget(aResult);
  return NS_OK;
}

//
// static helper routines
//
bool
FindElementCallback(void* aElement, void* aClosure)
{
  findIndexOfClosure* closure = static_cast<findIndexOfClosure*>(aClosure);
  nsISupports* element = static_cast<nsISupports*>(aElement);

  // don't start searching until we're past the startIndex
  if (closure->resultIndex >= closure->startIndex &&
      element == closure->targetElement) {
    return false;    // stop! We found it
  }
  closure->resultIndex++;

  return true;
}

nsresult
nsArrayBase::XPCOMConstructor(nsISupports* aOuter, const nsIID& aIID,
                              void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

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
