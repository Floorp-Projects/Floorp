/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIClassInfoImpl.h"
#include "nsString.h"

NS_IMETHODIMP_(MozExternalRefCountType)
GenericClassInfo::AddRef() { return 2; }

NS_IMETHODIMP_(MozExternalRefCountType)
GenericClassInfo::Release() { return 1; }

NS_IMPL_QUERY_INTERFACE(GenericClassInfo, nsIClassInfo)

NS_IMETHODIMP
GenericClassInfo::GetInterfaces(nsTArray<nsIID>& aArray) {
  return mData->getinterfaces(aArray);
}

NS_IMETHODIMP
GenericClassInfo::GetScriptableHelper(nsIXPCScriptable** aHelper) {
  if (mData->getscriptablehelper) {
    return mData->getscriptablehelper(aHelper);
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericClassInfo::GetContractID(nsACString& aContractID) {
  NS_ERROR("GetContractID not implemented");
  aContractID.SetIsVoid(true);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericClassInfo::GetClassDescription(nsACString& aDescription) {
  aDescription.SetIsVoid(true);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericClassInfo::GetClassID(nsCID** aClassID) {
  NS_ERROR("GetClassID not implemented");
  *aClassID = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericClassInfo::GetFlags(uint32_t* aFlags) {
  *aFlags = mData->flags;
  return NS_OK;
}

NS_IMETHODIMP
GenericClassInfo::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  *aClassIDNoAlloc = mData->cid;
  return NS_OK;
}
