/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <string.h>

#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsSupportsArray.h"
#include "nsSupportsArrayEnumerator.h"

nsresult
nsQueryElementAt::operator()(const nsIID& aIID, void** aResult) const
{
  nsresult status =
    mCollection ? mCollection->QueryElementAt(mIndex, aIID, aResult) :
                  NS_ERROR_NULL_POINTER;

  if (mErrorPtr) {
    *mErrorPtr = status;
  }

  return status;
}

nsSupportsArray::nsSupportsArray()
{
}

nsSupportsArray::~nsSupportsArray()
{
  Clear();
}

nsresult
nsSupportsArray::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsCOMPtr<nsISupportsArray> it = new nsSupportsArray();

  return it->QueryInterface(aIID, aResult);
}

NS_IMPL_ISUPPORTS(nsSupportsArray, nsISupportsArray, nsICollection,
                  nsISerializable)

NS_IMETHODIMP
nsSupportsArray::Read(nsIObjectInputStream* aStream)
{
  // TODO(ER): This used to leak when resizing the array. Not sure if that was
  // intentional, I'm guessing not.
  nsresult rv;

  uint32_t newArraySize;
  rv = aStream->Read32(&newArraySize);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t count;
  rv = aStream->Read32(&count);
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(count <= newArraySize, "overlarge mCount!");
  if (count > newArraySize) {
    count = newArraySize;
  }

  // Don't clear out our array until we know we have enough space for the new
  // one and have successfully copied everything out of the stream.
  ISupportsArray tmp;
  if (!tmp.SetCapacity(newArraySize, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  auto elems = tmp.AppendElements(count, mozilla::fallible);
  for (uint32_t i = 0; i < count; i++) {
    rv = aStream->ReadObject(true, &elems[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Now clear out existing refs and replace with the new array.
  for (auto& item : mArray) {
    NS_IF_RELEASE(item);
  }

  mArray.Clear();
  mArray.SwapElements(tmp);

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArray::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv;

  rv = aStream->Write32(mArray.Capacity());
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->Write32(mArray.Length());
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (auto& item : mArray) {
    rv = aStream->WriteObject(item, true);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArray::GetElementAt(uint32_t aIndex, nsISupports** aOutPtr)
{
  NS_IF_ADDREF(*aOutPtr = mArray.SafeElementAt(aIndex, nullptr));
  return NS_OK;
}

NS_IMETHODIMP_(int32_t)
nsSupportsArray::IndexOf(const nsISupports* aPossibleElement)
{
  return mArray.IndexOf(aPossibleElement);
}

NS_IMETHODIMP_(int32_t)
nsSupportsArray::LastIndexOf(const nsISupports* aPossibleElement)
{
  return mArray.LastIndexOf(aPossibleElement);
}

NS_IMETHODIMP_(bool)
nsSupportsArray::InsertElementAt(nsISupports* aElement, uint32_t aIndex)
{

  if (aIndex > mArray.Length() ||
      !mArray.InsertElementAt(aIndex, aElement, mozilla::fallible)) {
    return false;
  }

  NS_IF_ADDREF(aElement);
  return true;
}

NS_IMETHODIMP_(bool)
nsSupportsArray::ReplaceElementAt(nsISupports* aElement, uint32_t aIndex)
{
  if (aIndex < mArray.Length()) {
    NS_IF_ADDREF(aElement);  // addref first in case it's the same object!
    NS_IF_RELEASE(mArray[aIndex]);
    mArray[aIndex] = aElement;
    return true;
  }
  return false;
}

NS_IMETHODIMP_(bool)
nsSupportsArray::RemoveElementAt(uint32_t aIndex)
{
  if (aIndex + 1 <= mArray.Length()) {
    NS_IF_RELEASE(mArray[aIndex]);
    mArray.RemoveElementAt(aIndex);
    return true;
  }
  return false;
}

NS_IMETHODIMP
nsSupportsArray::RemoveElement(nsISupports* aElement)
{
  int32_t theIndex = IndexOf(aElement);
  if (theIndex >= 0) {
    return RemoveElementAt(theIndex) ? NS_OK : NS_ERROR_FAILURE;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSupportsArray::Clear(void)
{
  for (auto& item : mArray) {
    NS_IF_RELEASE(item);
  }

  mArray.Clear();

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArray::Compact(void)
{
  mArray.Compact();
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArray::Enumerate(nsIEnumerator** aResult)
{
  nsSupportsArrayEnumerator* e = new nsSupportsArrayEnumerator(this);
  *aResult = e;
  NS_ADDREF(e);
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsArray::Clone(nsISupportsArray** aResult)
{
  nsCOMPtr<nsISupportsArray> newArray;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(newArray));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (auto& item : mArray) {
    // AppendElement does an odd cast of bool to nsresult, we just cast back
    // here.
    if (!(bool)newArray->AppendElement(item)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  newArray.forget(aResult);
  return NS_OK;
}

nsresult
NS_NewISupportsArray(nsISupportsArray** aInstancePtrResult)
{
  nsresult rv;
  rv = nsSupportsArray::Create(nullptr, NS_GET_IID(nsISupportsArray),
                               (void**)aInstancePtrResult);
  return rv;
}
