/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_netwerk_dns_mdns_libmdns_nsDNSServiceInfo_h
#define mozilla_netwerk_dns_mdns_libmdns_nsDNSServiceInfo_h

#include "nsCOMPtr.h"
#include "nsIDNSServiceDiscovery.h"
#include "nsIPropertyBag2.h"
#include "nsString.h"

namespace mozilla {
namespace net {

class nsDNSServiceInfo final : public nsIDNSServiceInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDNSSERVICEINFO

  explicit nsDNSServiceInfo() = default;
  explicit nsDNSServiceInfo(nsIDNSServiceInfo* aServiceInfo);

private:
  virtual ~nsDNSServiceInfo() = default;

private:
  nsCString mHost;
  nsCString mAddress;
  uint16_t mPort = 0;
  nsCString mServiceName;
  nsCString mServiceType;
  nsCString mDomainName;
  nsCOMPtr<nsIPropertyBag2> mAttributes;

  bool mIsHostSet = false;
  bool mIsAddressSet = false;
  bool mIsPortSet = false;
  bool mIsServiceNameSet = false;
  bool mIsServiceTypeSet = false;
  bool mIsDomainNameSet = false;
  bool mIsAttributesSet = false;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_netwerk_dns_mdns_libmdns_nsDNSServiceInfo_h
