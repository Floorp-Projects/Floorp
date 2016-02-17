/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDNSService2_h__
#define nsDNSService2_h__

#include "nsPIDNSService.h"
#include "nsIIDNService.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsHostResolver.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"

class nsDNSService final : public nsPIDNSService
                         , public nsIObserver
                         , public nsIMemoryReporter
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSPIDNSSERVICE
    NS_DECL_NSIDNSSERVICE
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIMEMORYREPORTER

    nsDNSService();

    static nsIDNSService* GetXPCOMSingleton();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    bool GetOffline() const;

private:
    ~nsDNSService();

    static nsDNSService* GetSingleton();

    uint16_t GetAFForLookup(const nsACString &host, uint32_t flags);

    nsresult PreprocessHostname(bool              aLocalDomain,
                                const nsACString &aInput,
                                nsIIDNService    *aIDN,
                                nsACString       &aACE);

    RefPtr<nsHostResolver>  mResolver;
    nsCOMPtr<nsIIDNService>   mIDN;

    // mLock protects access to mResolver and mIPv4OnlyDomains
    mozilla::Mutex            mLock;

    // mIPv4OnlyDomains is a comma-separated list of domains for which only
    // IPv4 DNS lookups are performed. This allows the user to disable IPv6 on
    // a per-domain basis and work around broken DNS servers. See bug 68796.
    nsAdoptingCString                         mIPv4OnlyDomains;
    bool                                      mDisableIPv6;
    bool                                      mDisablePrefetch;
    bool                                      mBlockDotOnion;
    bool                                      mFirstTime;
    bool                                      mNotifyResolution;
    bool                                      mOfflineLocalhost;
    nsTHashtable<nsCStringHashKey>            mLocalDomains;
};

#endif //nsDNSService2_h__
