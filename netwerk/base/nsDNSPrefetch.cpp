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
#include "mozilla/Atomics.h"
#include "mozilla/Preferences.h"

static nsIDNSService* sDNSService = nullptr;
static mozilla::Atomic<bool, mozilla::Relaxed> sESNIEnabled(false);
const char kESNIPref[] = "network.security.esni.enabled";

nsresult nsDNSPrefetch::Initialize(nsIDNSService* aDNSService) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_IF_RELEASE(sDNSService);
  sDNSService = aDNSService;
  NS_IF_ADDREF(sDNSService);
  mozilla::Preferences::RegisterCallback(nsDNSPrefetch::PrefChanged, kESNIPref);
  PrefChanged(nullptr, nullptr);
  return NS_OK;
}

nsresult nsDNSPrefetch::Shutdown() {
  NS_IF_RELEASE(sDNSService);
  mozilla::Preferences::UnregisterCallback(nsDNSPrefetch::PrefChanged,
                                           kESNIPref);
  return NS_OK;
}

// static
void nsDNSPrefetch::PrefChanged(const char* aPref, void* aClosure) {
  if (!aPref || strcmp(aPref, kESNIPref) == 0) {
    bool enabled = false;
    if (NS_SUCCEEDED(mozilla::Preferences::GetBool(kESNIPref, &enabled))) {
      sESNIEnabled = enabled;
    }
  }
}

nsDNSPrefetch::nsDNSPrefetch(nsIURI* aURI,
                             mozilla::OriginAttributes& aOriginAttributes,
                             nsIRequest::TRRMode aTRRMode,
                             nsIDNSListener* aListener, bool storeTiming)
    : mOriginAttributes(aOriginAttributes),
      mStoreTiming(storeTiming),
      mTRRMode(aTRRMode),
      mListener(do_GetWeakReference(aListener)) {
  aURI->GetAsciiHost(mHostname);
  mIsHttps = aURI->SchemeIs("https");
}

nsresult nsDNSPrefetch::Prefetch(uint32_t flags) {
  if (mHostname.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  if (!sDNSService) return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICancelable> tmpOutstanding;

  if (mStoreTiming) mStartTimestamp = mozilla::TimeStamp::Now();
  // If AsyncResolve fails, for example because prefetching is disabled,
  // then our timing will be useless. However, in such a case,
  // mEndTimestamp will be a null timestamp and callers should check
  // TimingsValid() before using the timing.
  nsCOMPtr<nsIEventTarget> target = mozilla::GetCurrentThreadEventTarget();

  flags |= nsIDNSService::GetFlagsFromTRRMode(mTRRMode);

  nsresult rv = sDNSService->AsyncResolveNative(
      mHostname, flags | nsIDNSService::RESOLVE_SPECULATE, this, target,
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
        flags | nsIDNSService::RESOLVE_SPECULATE, this, target,
        mOriginAttributes, getter_AddRefs(tmpOutstanding));
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
  if (mStoreTiming) {
    mEndTimestamp = mozilla::TimeStamp::Now();
  }
  nsCOMPtr<nsIDNSListener> listener = do_QueryReferent(mListener);
  if (listener) {
    listener->OnLookupComplete(request, rec, status);
  }
  // OnLookupComplete should be called on the target thread, so we release
  // mListener here to make sure mListener is also released on the target
  // thread.
  mListener = nullptr;
  return NS_OK;
}
