/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDNSPrefetch_h___
#define nsDNSPrefetch_h___

#include "nsWeakReference.h"
#include "nsString.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"

#include "nsIDNSListener.h"

class nsIURI;
class nsIDNSService;

class nsDNSPrefetch MOZ_FINAL : public nsIDNSListener
{
    ~nsDNSPrefetch() {}

public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIDNSLISTENER
  
    nsDNSPrefetch(nsIURI *aURI, nsIDNSListener *aListener, bool storeTiming);
    bool TimingsValid() const {
        return !mStartTimestamp.IsNull() && !mEndTimestamp.IsNull();
    }
    // Only use the two timings if TimingsValid() returns true
    const mozilla::TimeStamp& StartTimestamp() const { return mStartTimestamp; }
    const mozilla::TimeStamp& EndTimestamp() const { return mEndTimestamp; }

    static nsresult Initialize(nsIDNSService *aDNSService);
    static nsresult Shutdown();

    // Call one of the following methods to start the Prefetch.
    nsresult PrefetchHigh(bool refreshDNS = false);
    nsresult PrefetchMedium(bool refreshDNS = false);
    nsresult PrefetchLow(bool refreshDNS = false);
  
private:
    nsCString mHostname;
    bool mStoreTiming;
    mozilla::TimeStamp mStartTimestamp;
    mozilla::TimeStamp mEndTimestamp;
    nsWeakPtr mListener;

    nsresult Prefetch(uint16_t flags);
};

#endif 
