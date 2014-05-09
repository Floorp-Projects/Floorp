/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMaybeWeakPtr.h"

void*
nsMaybeWeakPtr_base::GetValueAs(const nsIID &iid) const
{
  nsresult rv;
  void *ref;
  if (mPtr) {
    rv = mPtr->QueryInterface(iid, &ref);
    if (NS_SUCCEEDED(rv)) {
      return ref;
    }
  }

  nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(mPtr);
  if (weakRef) {
    rv = weakRef->QueryReferent(iid, &ref);
    if (NS_SUCCEEDED(rv)) {
      return ref;
    }
  }

  return nullptr;
}

nsresult
NS_AppendWeakElementBase(isupports_array_type *aArray,
                         nsISupports *aElement,
                         bool aOwnsWeak)
{
  nsCOMPtr<nsISupports> ref;
  if (aOwnsWeak) {
    nsCOMPtr<nsIWeakReference> weakRef;
    weakRef = do_GetWeakReference(aElement);
    reinterpret_cast<nsCOMPtr<nsISupports>*>(&weakRef)->swap(ref);
  } else {
    ref = aElement;
  }

  if (aArray->IndexOf(ref) != aArray->NoIndex) {
    return NS_ERROR_INVALID_ARG; // already present
  }
  if (!aArray->AppendElement(ref)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
NS_RemoveWeakElementBase(isupports_array_type *aArray,
                         nsISupports *aElement)
{
  size_t index = aArray->IndexOf(aElement);
  if (index != aArray->NoIndex) {
    aArray->RemoveElementAt(index);
    return NS_OK;
  }

  // Don't use do_GetWeakReference; it should only be called if we know
  // the object supports weak references.
  nsCOMPtr<nsISupportsWeakReference> supWeakRef = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(supWeakRef, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIWeakReference> weakRef;
  nsresult rv = supWeakRef->GetWeakReference(getter_AddRefs(weakRef));
  NS_ENSURE_SUCCESS(rv, rv);

  index = aArray->IndexOf(weakRef);
  if (index == aArray->NoIndex) {
    return NS_ERROR_INVALID_ARG;
  }

  aArray->RemoveElementAt(index);
  return NS_OK;
}
