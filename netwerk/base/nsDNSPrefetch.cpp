/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDNSPrefetch.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#include "nsIDNSAdditionalInfo.h"
#include "nsIDNSListener.h"
#include "nsIDNSService.h"
#include "nsIDNSByTypeRecord.h"
#include "nsICancelable.h"
#include "nsIURI.h"
#include "mozilla/Atomics.h"
#include "mozilla/Preferences.h"

static mozilla::StaticRefPtr<nsIDNSService> sDNSService;

nsresult nsDNSPrefetch::Initialize(nsIDNSService* aDNSService) {
  MOZ_ASSERT(NS_IsMainThread());

  sDNSService = aDNSService;
  return NS_OK;
}

nsresult nsDNSPrefetch::Shutdown() {
  sDNSService = nullptr;
  return NS_OK;
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
  aURI->GetPort(&mPort);
}

nsDNSPrefetch::nsDNSPrefetch(nsIURI* aURI,
                             mozilla::OriginAttributes& aOriginAttributes,
                             nsIRequest::TRRMode aTRRMode)
    : mOriginAttributes(aOriginAttributes),
      mStoreTiming(false),
      mTRRMode(aTRRMode),
      mListener(nullptr) {
  aURI->GetAsciiHost(mHostname);
}

nsresult nsDNSPrefetch::Prefetch(nsIDNSService::DNSFlags flags) {
  if (mHostname.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  if (!sDNSService) return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICancelable> tmpOutstanding;

  if (mStoreTiming) mStartTimestamp = mozilla::TimeStamp::Now();
  // If AsyncResolve fails, for example because prefetching is disabled,
  // then our timing will be useless. However, in such a case,
  // mEndTimestamp will be a null timestamp and callers should check
  // TimingsValid() before using the timing.
  nsCOMPtr<nsIEventTarget> target = mozilla::GetCurrentSerialEventTarget();

  flags |= nsIDNSService::GetFlagsFromTRRMode(mTRRMode);

  return sDNSService->AsyncResolveNative(
      mHostname, nsIDNSService::RESOLVE_TYPE_DEFAULT,
      flags | nsIDNSService::RESOLVE_SPECULATE, nullptr, this, target,
      mOriginAttributes, getter_AddRefs(tmpOutstanding));
}

nsresult nsDNSPrefetch::PrefetchLow(nsIDNSService::DNSFlags aFlags) {
  return Prefetch(nsIDNSService::RESOLVE_PRIORITY_LOW | aFlags);
}

nsresult nsDNSPrefetch::PrefetchMedium(nsIDNSService::DNSFlags aFlags) {
  return Prefetch(nsIDNSService::RESOLVE_PRIORITY_MEDIUM | aFlags);
}

nsresult nsDNSPrefetch::PrefetchHigh(nsIDNSService::DNSFlags aFlags) {
  return Prefetch(aFlags);
}

namespace {

class HTTPSRRListener final : public nsIDNSListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  explicit HTTPSRRListener(
      std::function<void(nsIDNSHTTPSSVCRecord*)>&& aCallback)
      : mResultCallback(std::move(aCallback)) {}

 private:
  ~HTTPSRRListener() = default;
  std::function<void(nsIDNSHTTPSSVCRecord*)> mResultCallback;
};

NS_IMPL_ISUPPORTS(HTTPSRRListener, nsIDNSListener)

NS_IMETHODIMP
HTTPSRRListener::OnLookupComplete(nsICancelable* aRequest, nsIDNSRecord* aRec,
                                  nsresult aStatus) {
  if (NS_FAILED(aStatus)) {
    mResultCallback(nullptr);
    return NS_OK;
  }

  nsCOMPtr<nsIDNSHTTPSSVCRecord> httpsRecord = do_QueryInterface(aRec);
  mResultCallback(httpsRecord);
  return NS_OK;
}

};  // namespace

nsresult nsDNSPrefetch::FetchHTTPSSVC(
    bool aRefreshDNS, bool aPrefetch,
    std::function<void(nsIDNSHTTPSSVCRecord*)>&& aCallback) {
  if (!sDNSService) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIEventTarget> target = mozilla::GetCurrentSerialEventTarget();
  nsIDNSService::DNSFlags flags = nsIDNSService::GetFlagsFromTRRMode(mTRRMode);
  if (aRefreshDNS) {
    flags |= nsIDNSService::RESOLVE_BYPASS_CACHE;
  }
  if (aPrefetch) {
    flags |= nsIDNSService::RESOLVE_SPECULATE;
  }

  nsCOMPtr<nsICancelable> tmpOutstanding;
  nsCOMPtr<nsIDNSListener> listener = new HTTPSRRListener(std::move(aCallback));
  nsCOMPtr<nsIDNSAdditionalInfo> info;
  if (mPort != -1) {
    sDNSService->NewAdditionalInfo(""_ns, mPort, getter_AddRefs(info));
  }
  return sDNSService->AsyncResolveNative(
      mHostname, nsIDNSService::RESOLVE_TYPE_HTTPSSVC, flags, info, listener,
      target, mOriginAttributes, getter_AddRefs(tmpOutstanding));
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

  return NS_OK;
}
