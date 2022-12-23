/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDNSPrefetch_h___
#define nsDNSPrefetch_h___

#include <functional>

#include "nsIWeakReferenceUtils.h"
#include "nsString.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasePrincipal.h"

#include "nsIDNSListener.h"
#include "nsIRequest.h"
#include "nsIDNSService.h"

class nsIURI;
class nsIDNSHTTPSSVCRecord;

class nsDNSPrefetch final : public nsIDNSListener {
  ~nsDNSPrefetch() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  nsDNSPrefetch(nsIURI* aURI, mozilla::OriginAttributes& aOriginAttributes,
                nsIRequest::TRRMode aTRRMode, nsIDNSListener* aListener,
                bool storeTiming);
  // For fetching HTTPS RR.
  nsDNSPrefetch(nsIURI* aURI, mozilla::OriginAttributes& aOriginAttributes,
                nsIRequest::TRRMode aTRRMode);
  bool TimingsValid() const {
    return !mStartTimestamp.IsNull() && !mEndTimestamp.IsNull();
  }
  // Only use the two timings if TimingsValid() returns true
  const mozilla::TimeStamp& StartTimestamp() const { return mStartTimestamp; }
  const mozilla::TimeStamp& EndTimestamp() const { return mEndTimestamp; }

  static nsresult Initialize(nsIDNSService* aDNSService);
  static nsresult Shutdown();

  // Call one of the following methods to start the Prefetch.
  nsresult PrefetchHigh(
      nsIDNSService::DNSFlags = nsIDNSService::RESOLVE_DEFAULT_FLAGS);
  nsresult PrefetchMedium(
      nsIDNSService::DNSFlags = nsIDNSService::RESOLVE_DEFAULT_FLAGS);
  nsresult PrefetchLow(
      nsIDNSService::DNSFlags = nsIDNSService::RESOLVE_DEFAULT_FLAGS);

  nsresult FetchHTTPSSVC(
      bool aRefreshDNS, bool aPrefetch,
      std::function<void(nsIDNSHTTPSSVCRecord*)>&& aCallback);

 private:
  nsCString mHostname;
  int32_t mPort{-1};
  mozilla::OriginAttributes mOriginAttributes;
  bool mStoreTiming;
  nsIRequest::TRRMode mTRRMode;
  mozilla::TimeStamp mStartTimestamp;
  mozilla::TimeStamp mEndTimestamp;
  nsWeakPtr mListener;

  nsresult Prefetch(nsIDNSService::DNSFlags flags);
};

#endif
