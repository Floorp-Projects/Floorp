/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDNSServiceInfo.h"
#include "nsHashPropertyBag.h"
#include "nsIProperty.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsImpl.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(nsDNSServiceInfo, nsIDNSServiceInfo)

nsDNSServiceInfo::nsDNSServiceInfo(nsIDNSServiceInfo* aServiceInfo) {
  if (NS_WARN_IF(!aServiceInfo)) {
    return;
  }

  nsAutoCString str;
  uint16_t value;

  if (NS_SUCCEEDED(aServiceInfo->GetHost(str))) {
    Unused << NS_WARN_IF(NS_FAILED(SetHost(str)));
  }
  if (NS_SUCCEEDED(aServiceInfo->GetAddress(str))) {
    Unused << NS_WARN_IF(NS_FAILED(SetAddress(str)));
  }
  if (NS_SUCCEEDED(aServiceInfo->GetPort(&value))) {
    Unused << NS_WARN_IF(NS_FAILED(SetPort(value)));
  }
  if (NS_SUCCEEDED(aServiceInfo->GetServiceName(str))) {
    Unused << NS_WARN_IF(NS_FAILED(SetServiceName(str)));
  }
  if (NS_SUCCEEDED(aServiceInfo->GetServiceType(str))) {
    Unused << NS_WARN_IF(NS_FAILED(SetServiceType(str)));
  }
  if (NS_SUCCEEDED(aServiceInfo->GetDomainName(str))) {
    Unused << NS_WARN_IF(NS_FAILED(SetDomainName(str)));
  }

  nsCOMPtr<nsIPropertyBag2> attributes;  // deep copy
  if (NS_SUCCEEDED(aServiceInfo->GetAttributes(getter_AddRefs(attributes)))) {
    RefPtr<nsHashPropertyBag> newAttributes = new nsHashPropertyBag();
    newAttributes->CopyFrom(attributes);
    Unused << NS_WARN_IF(NS_FAILED(SetAttributes(newAttributes)));
  }
}

NS_IMETHODIMP
nsDNSServiceInfo::GetHost(nsACString& aHost) {
  if (!mIsHostSet) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  aHost = mHost;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::SetHost(const nsACString& aHost) {
  mHost = aHost;
  mIsHostSet = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::GetAddress(nsACString& aAddress) {
  if (!mIsAddressSet) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  aAddress = mAddress;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::SetAddress(const nsACString& aAddress) {
  mAddress = aAddress;
  mIsAddressSet = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::GetPort(uint16_t* aPort) {
  if (NS_WARN_IF(!aPort)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!mIsPortSet) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aPort = mPort;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::SetPort(uint16_t aPort) {
  mPort = aPort;
  mIsPortSet = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::GetServiceName(nsACString& aServiceName) {
  if (!mIsServiceNameSet) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  aServiceName = mServiceName;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::SetServiceName(const nsACString& aServiceName) {
  mServiceName = aServiceName;
  mIsServiceNameSet = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::GetServiceType(nsACString& aServiceType) {
  if (!mIsServiceTypeSet) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  aServiceType = mServiceType;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::SetServiceType(const nsACString& aServiceType) {
  mServiceType = aServiceType;
  mIsServiceTypeSet = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::GetDomainName(nsACString& aDomainName) {
  if (!mIsDomainNameSet) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  aDomainName = mDomainName;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::SetDomainName(const nsACString& aDomainName) {
  mDomainName = aDomainName;
  mIsDomainNameSet = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::GetAttributes(nsIPropertyBag2** aAttributes) {
  if (!mIsAttributesSet) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsCOMPtr<nsIPropertyBag2> attributes(mAttributes);
  attributes.forget(aAttributes);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceInfo::SetAttributes(nsIPropertyBag2* aAttributes) {
  mAttributes = aAttributes;
  mIsAttributesSet = aAttributes != nullptr;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
