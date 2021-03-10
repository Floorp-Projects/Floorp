/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRRService_h_
#define TRRService_h_

#include "mozilla/DataMutex.h"
#include "nsHostResolver.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "ODoHService.h"
#include "TRRServiceBase.h"
#include "nsICaptivePortalService.h"

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
  bool Enabled(nsIRequest::TRRMode aRequestMode = nsIRequest::TRR_DEFAULT_MODE);
  bool IsConfirmed() { return mConfirmation.mState == CONFIRM_OK; }

  bool DisableIPv6() { return mDisableIPv6; }
  nsresult GetURI(nsACString& result);
  nsresult GetCredentials(nsCString& result);
  uint32_t GetRequestTimeout();

  LookupStatus CompleteLookup(nsHostRecord*, nsresult, mozilla::net::AddrInfo*,
                              bool pb, const nsACString& aOriginSuffix,
                              TRRSkippedReason aReason,
                              TRR* aTrrRequest) override;
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
  void TRRIsOkay(nsresult aChannelStatus);
  bool ParentalControlEnabled() const { return mParentalControlEnabled; }

  nsresult DispatchTRRRequest(TRR* aTrrRequest);
  already_AddRefed<nsIThread> TRRThread();
  bool IsOnTRRThread();

  bool IsUsingAutoDetectedURL() { return mURISetByDetection; }

  // Returns a reference to a static string identifying the current DoH server
  // If the DoH server is not one of the built-in ones it will return "(other)"
  static const nsCString& ProviderKey();

 private:
  virtual ~TRRService();

  friend class TRRServiceChild;
  friend class TRRServiceParent;
  friend class ODoHService;
  static void AddObserver(nsIObserver* aObserver,
                          nsIObserverService* aObserverService = nullptr);
  static bool CheckCaptivePortalIsPassed();
  static bool GetParentalControlEnabledInternal();
  static bool CheckPlatformDNSStatus(nsINetworkLinkService* aLinkService);

  nsresult ReadPrefs(const char* name);
  void GetPrefBranch(nsIPrefBranch** result);
  void MaybeConfirm(const char* aReason);
  void MaybeConfirm_locked(const char* aReason);
  friend class ::nsDNSService;
  void SetDetectedTrrURI(const nsACString& aURI);

  bool IsDomainBlocked(const nsACString& aHost, const nsACString& aOriginSuffix,
                       bool aPrivateBrowsing);
  bool IsExcludedFromTRR_unlocked(const nsACString& aHost);

  void RebuildSuffixList(nsTArray<nsCString>&& aSuffixList);

  nsresult DispatchTRRRequestInternal(TRR* aTrrRequest, bool aWithLock);
  already_AddRefed<nsIThread> TRRThread_locked();
  already_AddRefed<nsIThread> MainThreadOrTRRThread(bool aWithLock = true);

  // This method will process the URI and try to set mPrivateURI to that value.
  // Will return true if performed the change (if the value was different)
  // or false if mPrivateURI already had that value.
  bool MaybeSetPrivateURI(const nsACString& aURI) override;
  void ClearEntireCache();

  virtual void ReadEtcHostsFile() override;
  void AddEtcHosts(const nsTArray<nsCString>&);

  bool mInitialized;
  Atomic<uint32_t, Relaxed> mBlocklistDurationSeconds;

  Mutex mLock;

  nsCString mPrivateCred;  // main thread only
  nsCString mConfirmationNS;
  nsCString mBootstrapAddr;

  Atomic<bool, Relaxed>
      mCaptiveIsPassed;  // set when captive portal check is passed
  Atomic<bool, Relaxed> mDisableIPv6;  // don't even try

  // TRR Blocklist storage
  // mTRRBLStorage is only modified on the main thread, but we query whether it
  // is initialized or not off the main thread as well. Therefore we need to
  // lock while creating it and while accessing it off the main thread.
  DataMutex<nsTHashMap<nsCStringHashKey, int32_t>> mTRRBLStorage;

  // A set of domains that we should not use TRR for.
  nsTHashtable<nsCStringHashKey> mExcludedDomains;
  nsTHashtable<nsCStringHashKey> mDNSSuffixDomains;
  nsTHashtable<nsCStringHashKey> mEtcHostsDomains;

  enum ConfirmationState {
    CONFIRM_INIT = 0,
    CONFIRM_TRYING = 1,
    CONFIRM_OK = 2,
    CONFIRM_FAILED = 3
  };

  class ConfirmationContext {
   public:
    static const size_t RESULTS_SIZE = 32;

    Atomic<ConfirmationState, Relaxed> mState;
    RefPtr<TRR> mTask;
    nsCOMPtr<nsITimer> mTimer;
    uint32_t mRetryInterval = 125;  // milliseconds until retry
    // The number of TRR requests that failed in a row.
    Atomic<uint32_t, Relaxed> mTRRFailures;

    // This buffer holds consecutive TRR failures reported by calling
    // TRRIsOkay(). It is only meant for reporting event telemetry.
    char mFailureReasons[RESULTS_SIZE] = {0};

    // The number of confirmation retries.
    uint32_t mAttemptCount = 0;

    // The results of past confirmation attempts.
    // This is circular buffer ending at mAttemptCount.
    char mResults[RESULTS_SIZE] = {0};

    // Time when first confirmation started. Needed so we can
    // record the time from start to confirmed.
    TimeStamp mFirstRequestTime;
    // The network ID at the start of the last confirmation attempt
    nsCString mNetworkId;
    // Captive portal status at the time of recording.
    int32_t mCaptivePortalStatus = nsICaptivePortalService::UNKNOWN;

    // The reason the confirmation context changed.
    nsCString mContextChangeReason;

    // What triggered the confirmation
    nsCString mTrigger;

    // String representation of consecutive failed lookups that triggered
    // confirmation.
    nsCString mFailedLookups;

    // Called when a confirmation completes successfully or when the
    // confirmation context changes.
    void RecordEvent(const char* aReason);

    // Called when a confirmation request is completed. The status is recorded
    // in the results.
    void RequestCompleted(nsresult aLookupStatus, nsresult aChannelStatus);
  };

  ConfirmationContext mConfirmation;

  bool mParentalControlEnabled;
  RefPtr<ODoHService> mODoHService;
  nsCOMPtr<nsINetworkLinkService> mLinkService;
};

extern TRRService* gTRRService;

}  // namespace net
}  // namespace mozilla

#endif  // TRRService_h_
