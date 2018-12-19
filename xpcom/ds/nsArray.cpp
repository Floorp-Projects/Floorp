/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsThreadUtils.h"
#include "xpcjsid.h"

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

nsArrayBase::~nsArrayBase() { Clear(); }

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
nsArrayBase::GetLength(uint32_t* aLength) {
  *aLength = mArray.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsArrayBase::QueryElementAt(uint32_t aIndex, const nsIID& aIID,
                            void** aResult) {
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
                     uint32_t* aResult) {
  int32_t idx = mArray.IndexOf(aElement, aStartIndex);
  if (idx == -1) {
    return NS_ERROR_FAILURE;
  }

  *aResult = static_cast<uint32_t>(idx);
  return NS_OK;
}

NS_IMETHODIMP
nsArrayBase::ScriptedEnumerate(nsIJSIID* aElemIID, uint8_t aArgc,
                               nsISimpleEnumerator** aResult) {
  if (aArgc > 0 && aElemIID) {
    return NS_NewArrayEnumerator(aResult, static_cast<nsIArray*>(this),
                                 *aElemIID->GetID());
  }
  return NS_NewArrayEnumerator(aResult, static_cast<nsIArray*>(this));
}

NS_IMETHODIMP
nsArrayBase::EnumerateImpl(const nsID& aElemIID,
                           nsISimpleEnumerator** aResult) {
  return NS_NewArrayEnumerator(aResult, static_cast<nsIArray*>(this), aElemIID);
}

// nsIMutableArray implementation

NS_IMETHODIMP
nsArrayBase::AppendElement(nsISupports* aElement) {
  bool result = mArray.AppendObject(aElement);
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::RemoveElementAt(uint32_t aIndex) {
  bool result = mArray.RemoveObjectAt(aIndex);
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::InsertElementAt(nsISupports* aElement, uint32_t aIndex) {
  bool result = mArray.InsertObjectAt(aElement, aIndex);
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArrayBase::ReplaceElementAt(nsISupports* aElement, uint32_t aIndex) {
  mArray.ReplaceObjectAt(aElement, aIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsArrayBase::Clear() {
  mArray.Clear();
  return NS_OK;
}

// nsIArrayExtensions implementation.

NS_IMETHODIMP
nsArrayBase::Count(uint32_t* aResult) { return GetLength(aResult); }

NS_IMETHODIMP
nsArrayBase::GetElementAt(uint32_t aIndex, nsISupports** aResult) {
  nsCOMPtr<nsISupports> obj = mArray.SafeObjectAt(aIndex);
  obj.forget(aResult);
  return NS_OK;
}

nsresult nsArrayBase::XPCOMConstructor(nsISupports* aOuter, const nsIID& aIID,
                                       void** aResult) {
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsCOMPtr<nsIMutableArray> inst = Create();
  return inst->QueryInterface(aIID, aResult);
}

already_AddRefed<nsIMutableArray> nsArrayBase::Create() {
  nsCOMPtr<nsIMutableArray> inst;
  if (NS_IsMainThread()) {
    inst = new nsArrayCC;
  } else {
    inst = new nsArray;
  }
  return inst.forget();
}
