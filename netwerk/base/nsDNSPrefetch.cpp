/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDNSPrefetch.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#include "nsIDNSListener.h"
#include "nsIDNSService.h"
#include "nsICancelable.h"
#include "nsIURI.h"
#include "mozilla/Preferences.h"

static nsIDNSService* sDNSService = nullptr;
static bool sESNIEnabled = false;

nsresult nsDNSPrefetch::Initialize(nsIDNSService* aDNSService) {
  NS_IF_RELEASE(sDNSService);
  sDNSService = aDNSService;
  NS_IF_ADDREF(sDNSService);
  mozilla::Preferences::AddBoolVarCache(&sESNIEnabled,
                                        "network.security.esni.enabled");
  return NS_OK;
}

nsresult nsDNSPrefetch::Shutdown() {
  NS_IF_RELEASE(sDNSService);
  return NS_OK;
}

nsDNSPrefetch::nsDNSPrefetch(nsIURI* aURI,
                             mozilla::OriginAttributes& aOriginAttributes,
                             nsIDNSListener* aListener, bool storeTiming)
    : mOriginAttributes(aOriginAttributes),
      mStoreTiming(storeTiming),
      mListener(do_GetWeakReference(aListener)) {
  aURI->GetAsciiHost(mHostname);
  mIsHttps = false;
  aURI->SchemeIs("https", &mIsHttps);
}

nsresult nsDNSPrefetch::Prefetch(uint16_t flags) {
  // This can work properly only if this call is on the main thread.
  // Curenlty we use nsDNSPrefetch only in nsHttpChannel which will call
  // PrefetchHigh() from the main thread. Let's add assertion to catch
  // if things change.
  MOZ_ASSERT(NS_IsMainThread(), "Expecting DNS callback on main thread.");

  if (mHostname.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  if (!sDNSService) return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICancelable> tmpOutstanding;

  if (mStoreTiming) mStartTimestamp = mozilla::TimeStamp::Now();
  // If AsyncResolve fails, for example because prefetching is disabled,
  // then our timing will be useless. However, in such a case,
  // mEndTimestamp will be a null timestamp and callers should check
  // TimingsValid() before using the timing.
  nsCOMPtr<nsIEventTarget> main = mozilla::GetMainThreadEventTarget();

  nsresult rv = sDNSService->AsyncResolveNative(
      mHostname, flags | nsIDNSService::RESOLVE_SPECULATE, this, main,
      mOriginAttributes, getter_AddRefs(tmpOutstanding));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Fetch esni keys if needed.
  if (sESNIEnabled && mIsHttps) {
    nsAutoCString esniHost;
    esniHost.Append("_esni.");
    esniHost.Append(mHostname);
    sDNSService->AsyncResolveByTypeNative(
        esniHost, nsIDNSService::RESOLVE_TYPE_TXT,
        flags | nsIDNSService::RESOLVE_SPECULATE, this, main, mOriginAttributes,
        getter_AddRefs(tmpOutstanding));
  }
  return NS_OK;
}

nsresult nsDNSPrefetch::PrefetchLow(bool refreshDNS) {
  return Prefetch(nsIDNSService::RESOLVE_PRIORITY_LOW |
                  (refreshDNS ? nsIDNSService::RESOLVE_BYPASS_CACHE : 0));
}

nsresult nsDNSPrefetch::PrefetchMedium(bool refreshDNS) {
  return Prefetch(nsIDNSService::RESOLVE_PRIORITY_MEDIUM |
                  (refreshDNS ? nsIDNSService::RESOLVE_BYPASS_CACHE : 0));
}

nsresult nsDNSPrefetch::PrefetchHigh(bool refreshDNS) {
  return Prefetch(refreshDNS ? nsIDNSService::RESOLVE_BYPASS_CACHE : 0);
}

NS_IMPL_ISUPPORTS(nsDNSPrefetch, nsIDNSListener)

NS_IMETHODIMP
nsDNSPrefetch::OnLookupComplete(nsICancelable* request, nsIDNSRecord* rec,
                                nsresult status) {
  MOZ_ASSERT(NS_IsMainThread(), "Expecting DNS callback on main thread.");

  if (mStoreTiming) {
    mEndTimestamp = mozilla::TimeStamp::Now();
  }
  nsCOMPtr<nsIDNSListener> listener = do_QueryReferent(mListener);
  if (listener) {
    listener->OnLookupComplete(request, rec, status);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSPrefetch::OnLookupByTypeComplete(nsICancelable* request,
                                      nsIDNSByTypeRecord* res,
                                      nsresult status) {
  MOZ_ASSERT(NS_IsMainThread(), "Expecting DNS callback on main thread.");

  if (mStoreTiming) {
    mEndTimestamp = mozilla::TimeStamp::Now();
  }
  nsCOMPtr<nsIDNSListener> listener = do_QueryReferent(mListener);
  if (listener) {
    listener->OnLookupByTypeComplete(request, res, status);
  }
  return NS_OK;
}
