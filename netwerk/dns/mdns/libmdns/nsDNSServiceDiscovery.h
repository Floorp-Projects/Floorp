/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_netwerk_dns_mdns_libmdns_nsDNSServiceDiscovery_h
#define mozilla_netwerk_dns_mdns_libmdns_nsDNSServiceDiscovery_h

#include "nsIDNSServiceDiscovery.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace net {

class BrowseOperator;
class RegisterOperator;

class nsDNSServiceDiscovery final : public nsIDNSServiceDiscovery
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSSERVICEDISCOVERY

  explicit nsDNSServiceDiscovery() = default;

  /*
  ** The mDNS service is started on demand. If no one uses, mDNS service will not
  ** start. Therefore, all operations before service started will fail
  ** and get error code |kDNSServiceErr_ServiceNotRunning| defined in dns_sd.h.
  **/
  nsresult Init();

  nsresult StopDiscovery(nsIDNSServiceDiscoveryListener* aListener);
  nsresult UnregisterService(nsIDNSRegistrationListener* aListener);

private:
  virtual ~nsDNSServiceDiscovery() = default;

  nsRefPtrHashtable<nsISupportsHashKey, BrowseOperator> mDiscoveryMap;
  nsRefPtrHashtable<nsISupportsHashKey, RegisterOperator> mRegisterMap;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_netwerk_dns_mdns_libmdns_nsDNSServiceDiscovery_h
