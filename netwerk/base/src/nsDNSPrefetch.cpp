/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDNSPrefetch.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsNetUtil.h"

#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsICancelable.h"

static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static nsIDNSService *sDNSService = nullptr;

nsresult
nsDNSPrefetch::Initialize(nsIDNSService *aDNSService)
{
    NS_IF_RELEASE(sDNSService);
    sDNSService =  aDNSService;
    NS_IF_ADDREF(sDNSService);
    return NS_OK;
}

nsresult
nsDNSPrefetch::Shutdown()
{
    NS_IF_RELEASE(sDNSService);
    return NS_OK;
}

nsDNSPrefetch::nsDNSPrefetch(nsIURI *aURI, bool storeTiming)
    : mStoreTiming(storeTiming)
{
    aURI->GetAsciiHost(mHostname);
}

nsresult 
nsDNSPrefetch::Prefetch(PRUint16 flags)
{
    if (mHostname.IsEmpty())
        return NS_ERROR_NOT_AVAILABLE;
  
    if (!sDNSService)
        return NS_ERROR_NOT_AVAILABLE;
    
    nsCOMPtr<nsICancelable> tmpOutstanding;  

    if (mStoreTiming)
        mStartTimestamp = mozilla::TimeStamp::Now();
    // If AsyncResolve fails, for example because prefetching is disabled,
    // then our timing will be useless. However, in such a case,
    // mEndTimestamp will be a null timestamp and callers should check
    // TimingsValid() before using the timing.
    return sDNSService->AsyncResolve(mHostname, flags | nsIDNSService::RESOLVE_SPECULATE,
                                     this, nullptr, getter_AddRefs(tmpOutstanding));
}

nsresult
nsDNSPrefetch::PrefetchLow()
{
    return Prefetch(nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult
nsDNSPrefetch::PrefetchMedium()
{
    return Prefetch(nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult
nsDNSPrefetch::PrefetchHigh()
{
    return Prefetch(0);
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsDNSPrefetch, nsIDNSListener)

NS_IMETHODIMP
nsDNSPrefetch::OnLookupComplete(nsICancelable *request,
                                nsIDNSRecord  *rec,
                                nsresult       status)
{
    if (mStoreTiming)
        mEndTimestamp = mozilla::TimeStamp::Now();
    return NS_OK;
}
