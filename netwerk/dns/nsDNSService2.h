/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDNSService2_h__
#define nsDNSService2_h__

#include "nsPIDNSService.h"
#include "nsIIDNService.h"
#include "nsIObserver.h"
#include "nsHostResolver.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"

class nsDNSService MOZ_FINAL : public nsPIDNSService
                             , public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSPIDNSSERVICE
    NS_DECL_NSIDNSSERVICE
    NS_DECL_NSIOBSERVER

    nsDNSService();
    ~nsDNSService();

private:
    uint16_t GetAFForLookup(const nsACString &host, uint32_t flags);

    nsRefPtr<nsHostResolver>  mResolver;
    nsCOMPtr<nsIIDNService>   mIDN;

    // mLock protects access to mResolver and mIPv4OnlyDomains
    mozilla::Mutex            mLock;

    // mIPv4OnlyDomains is a comma-separated list of domains for which only
    // IPv4 DNS lookups are performed. This allows the user to disable IPv6 on
    // a per-domain basis and work around broken DNS servers. See bug 68796.
    nsAdoptingCString         mIPv4OnlyDomains;
    bool                      mDisableIPv6;
    bool                      mDisablePrefetch;
    bool                      mFirstTime;
    bool                      mOffline;
    nsTHashtable<nsCStringHashKey> mLocalDomains;
};

#endif //nsDNSService2_h__
