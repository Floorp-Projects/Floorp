/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRRService_h_
#define TRRService_h_

#include "mozilla/DataMutex.h"
#include "nsHostResolver.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "TRRServiceBase.h"

class nsDNSService;
class nsIPrefBranch;
class nsINetworkLinkService;
class nsIObserverService;

namespace mozilla {
namespace net {

class TRRServiceChild;
class TRRServiceParent;

class TRRService : public TRRServiceBase,
                   public nsIObserver,
                   public nsITimerCallback,
                   public nsSupportsWeakReference,
                   public AHostResolver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  TRRService();
  nsresult Init();
  nsresult Start();
  bool Enabled(nsIRequest::TRRMode aMode = nsIRequest::TRR_FIRST_MODE);
  bool IsConfirmed() { return mConfirmationState == CONFIRM_OK; }

  bool AllowRFC1918() { return mRfc1918; }
  bool UseGET() { return mUseGET; }
  bool EarlyAAAA() { return mEarlyAAAA; }
  bool CheckIPv6Connectivity() { return mCheckIPv6Connectivity; }
  bool WaitForAllResponses() { return mWaitForAllResponses; }
  bool DisableIPv6() { return mDisableIPv6; }
  bool DisableECS() { return mDisableECS; }
  bool SkipTRRWhenParentalControlEnabled() {
    return mSkipTRRWhenParentalControlEnabled;
  }
  nsresult GetURI(nsACString& result);
  nsresult GetCredentials(nsCString& result);
  uint32_t GetRequestTimeout();

  LookupStatus CompleteLookup(nsHostRecord*, nsresult, mozilla::net::AddrInfo*,
                              bool pb, const nsACString& aOriginSuffix,
                              nsHostRecord::TRRSkippedReason aReason) override;
  LookupStatus CompleteLookupByType(nsHostRecord*, nsresult,
                                    mozilla::net::TypeRecordResultType&,
                                    uint32_t, bool pb) override;
  void AddToBlocklist(const nsACString& host, const nsACString& originSuffix,
                      bool privateBrowsing, bool aParentsToo);
  bool IsTemporarilyBlocked(const nsACString& aHost,
                            const nsACString& aOriginSuffix,
                            bool aPrivateBrowsing, bool aParentsToo);
  bool IsExcludedFromTRR(const nsACString& aHost);

  bool MaybeBootstrap(const nsACString& possible, nsACString& result);
  enum TrrOkay { OKAY_NORMAL = 0, OKAY_TIMEOUT = 1, OKAY_BAD = 2 };
  void TRRIsOkay(enum TrrOkay aReason);
  bool ParentalControlEnabled() const { return mParentalControlEnabled; }

  nsresult DispatchTRRRequest(TRR* aTrrRequest);
  already_AddRefed<nsIThread> TRRThread();
  bool IsOnTRRThread();

  bool IsUsingAutoDetectedURL() { return mURISetByDetection; }
  static const nsCString& AutoDetectedKey();

 private:
  virtual ~TRRService();

  friend class TRRServiceChild;
  friend class TRRServiceParent;
  static void AddObserver(nsIObserver* aObserver,
                          nsIObserverService* aObserverService = nullptr);
  static bool CheckCaptivePortalIsPassed();
  static bool GetParentalControlEnabledInternal();
  static bool CheckPlatformDNSStatus(nsINetworkLinkService* aLinkService);

  nsresult ReadPrefs(const char* name);
  void GetPrefBranch(nsIPrefBranch** result);
  void MaybeConfirm();
  void MaybeConfirm_locked();
  friend class ::nsDNSService;
  void SetDetectedTrrURI(const nsACString& aURI);

  bool IsDomainBlocked(const nsACString& aHost, const nsACString& aOriginSuffix,
                       bool aPrivateBrowsing);
  bool IsExcludedFromTRR_unlocked(const nsACString& aHost);

  void RebuildSuffixList(nsTArray<nsCString>&& aSuffixList);

  nsresult DispatchTRRRequestInternal(TRR* aTrrRequest, bool aWithLock);
  already_AddRefed<nsIThread> TRRThread_locked();

  // This method will process the URI and try to set mPrivateURI to that value.
  // Will return true if performed the change (if the value was different)
  // or false if mPrivateURI already had that value.
  bool MaybeSetPrivateURI(const nsACString& aURI) override;
  void ClearEntireCache();

  bool mInitialized;
  Atomic<uint32_t, Relaxed> mBlocklistDurationSeconds;

  Mutex mLock;

  nsCString mPrivateCred;  // main thread only
  nsCString mConfirmationNS;
  nsCString mBootstrapAddr;

  Atomic<bool, Relaxed> mWaitForCaptive;  // wait for the captive portal to say
                                          // OK before using TRR
  Atomic<bool, Relaxed>
      mRfc1918;  // okay with local IP addresses in DOH responses?
  Atomic<bool, Relaxed>
      mCaptiveIsPassed;           // set when captive portal check is passed
  Atomic<bool, Relaxed> mUseGET;  // do DOH using GET requests (instead of POST)
  Atomic<bool, Relaxed> mEarlyAAAA;  // allow use of AAAA results before A is in
  Atomic<bool, Relaxed> mCheckIPv6Connectivity;  // check IPv6 connectivity
  Atomic<bool, Relaxed> mWaitForAllResponses;  // Don't notify until all are in
  Atomic<bool, Relaxed> mDisableIPv6;          // don't even try
  Atomic<bool, Relaxed> mDisableECS;  // disable EDNS Client Subnet in requests
  Atomic<bool, Relaxed> mSkipTRRWhenParentalControlEnabled;
  Atomic<uint32_t, Relaxed>
      mDisableAfterFails;  // this many fails in a row means failed TRR service
  Atomic<bool, Relaxed> mPlatformDisabledTRR;

  // TRR Blocklist storage
  // mTRRBLStorage is only modified on the main thread, but we query whether it
  // is initialized or not off the main thread as well. Therefore we need to
  // lock while creating it and while accessing it off the main thread.
  DataMutex<nsDataHashtable<nsCStringHashKey, int32_t>> mTRRBLStorage;

  // A set of domains that we should not use TRR for.
  nsTHashtable<nsCStringHashKey> mExcludedDomains;
  nsTHashtable<nsCStringHashKey> mDNSSuffixDomains;

  enum ConfirmationState {
    CONFIRM_INIT = 0,
    CONFIRM_TRYING = 1,
    CONFIRM_OK = 2,
    CONFIRM_FAILED = 3
  };
  Atomic<ConfirmationState, Relaxed> mConfirmationState;
  RefPtr<TRR> mConfirmer;
  nsCOMPtr<nsITimer> mRetryConfirmTimer;
  uint32_t mRetryConfirmInterval;  // milliseconds until retry
  Atomic<uint32_t, Relaxed> mTRRFailures;
  bool mParentalControlEnabled;
};

extern TRRService* gTRRService;

}  // namespace net
}  // namespace mozilla

#endif  // TRRService_h_
