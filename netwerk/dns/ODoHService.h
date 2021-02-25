/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ODoHService_h_
#define ODoHService_h_

#include "DNS.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "nsString.h"
#include "nsIDNSListener.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace net {

class ODoH;

class ODoHService : public nsIDNSListener,
                    public nsIObserver,
                    public nsSupportsWeakReference,
                    public nsITimerCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  ODoHService();
  bool Init();
  bool Enabled() const;

  const Maybe<nsTArray<ObliviousDoHConfig>>& ODoHConfigs();
  void AppendPendingODoHRequest(ODoH* aRequest);
  bool RemovePendingODoHRequest(ODoH* aRequest);
  void GetRequestURI(nsACString& aResult);
  // Send a DNS query to reterive the ODoHConfig.
  nsresult UpdateODoHConfig();

 private:
  virtual ~ODoHService();
  nsresult ReadPrefs(const char* aName);
  void OnODoHPrefsChange(bool aInit);
  void BuildODoHRequestURI();
  void StartTTLTimer(uint32_t aTTL);

  Mutex mLock;
  Atomic<bool, Relaxed> mQueryODoHConfigInProgress;
  nsCString mODoHProxyURI;
  nsCString mODoHTargetHost;
  nsCString mODoHTargetPath;
  nsCString mODoHRequestURI;
  Maybe<nsTArray<ObliviousDoHConfig>> mODoHConfigs;
  nsTArray<RefPtr<ODoH>> mPendingRequests;
  // This timer is always touched on main thread to avoid race conditions.
  nsCOMPtr<nsITimer> mTTLTimer;
};

extern ODoHService* gODoHService;

}  // namespace net
}  // namespace mozilla

#endif
