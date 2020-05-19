/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIOService_h__
#define nsIOService_h__

#include "nsStringFwd.h"
#include "nsIIOService.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIWeakReferenceUtils.h"
#include "nsINetUtil.h"
#include "nsIChannelEventSink.h"
#include "nsCategoryCache.h"
#include "nsISpeculativeConnect.h"
#include "nsDataHashtable.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "prtime.h"
#include "nsICaptivePortalService.h"

#define NS_N(x) (sizeof(x) / sizeof(*x))

// We don't want to expose this observer topic.
// Intended internal use only for remoting offline/inline events.
// See Bug 552829
#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"
#define NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC "ipc:network:set-connectivity"

static const char gScheme[][sizeof("moz-safe-about")] = {
    "chrome",   "file",          "http",      "https",
    "jar",      "data",          "about",     "moz-safe-about",
    "resource", "moz-extension", "page-icon", "blob"};

static const char gForcedExternalSchemes[][sizeof("moz-nullprincipal")] = {
    "place", "fake-favicon-uri", "favicon", "moz-nullprincipal"};

class nsINetworkLinkService;
class nsIPrefBranch;
class nsIProtocolProxyService2;
class nsIProxyInfo;
class nsPISocketTransportService;

namespace mozilla {
class MemoryReportingProcess;
namespace net {
class NeckoChild;
class nsAsyncRedirectVerifyHelper;
class SocketProcessHost;
class SocketProcessMemoryReporter;

class nsIOService final : public nsIIOService,
                          public nsIObserver,
                          public nsINetUtil,
                          public nsISpeculativeConnect,
                          public nsSupportsWeakReference,
                          public nsIIOServiceInternal {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIIOSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSINETUTIL
  NS_DECL_NSISPECULATIVECONNECT
  NS_DECL_NSIIOSERVICEINTERNAL

  // Gets the singleton instance of the IO Service, creating it as needed
  // Returns nullptr on out of memory or failure to initialize.
  static already_AddRefed<nsIOService> GetInstance();

  nsresult Init();
  nsresult NewURI(const char* aSpec, nsIURI* aBaseURI, nsIURI** result,
                  nsIProtocolHandler** hdlrResult);

  // Called by channels before a redirect happens. This notifies the global
  // redirect observers.
  nsresult AsyncOnChannelRedirect(nsIChannel* oldChan, nsIChannel* newChan,
                                  uint32_t flags,
                                  nsAsyncRedirectVerifyHelper* helper);

  bool IsOffline() { return mOffline; }
  PRIntervalTime LastOfflineStateChange() { return mLastOfflineStateChange; }
  PRIntervalTime LastConnectivityChange() { return mLastConnectivityChange; }
  PRIntervalTime LastNetworkLinkChange() { return mLastNetworkLinkChange; }
  bool IsNetTearingDown() {
    return mShutdown || mOfflineForProfileChange ||
           mHttpHandlerAlreadyShutingDown;
  }
  PRIntervalTime NetTearingDownStarted() { return mNetTearingDownStarted; }

  // nsHttpHandler is going to call this function to inform nsIOService that
  // network is in process of tearing down. Moving nsHttpConnectionMgr::Shutdown
  // to nsIOService caused problems (bug 1242755) so we doing it in this way. As
  // soon as nsIOService gets notification that it is shutdown it is going to
  // reset mHttpHandlerAlreadyShutingDown.
  void SetHttpHandlerAlreadyShutingDown();

  bool IsLinkUp();

  static bool IsDataURIUniqueOpaqueOrigin();
  static bool BlockToplevelDataUriNavigations();

  // Converts an internal URI (e.g. one that has a username and password in
  // it) into one which we can expose to the user, for example on the URL bar.
  static already_AddRefed<nsIURI> CreateExposableURI(nsIURI*);

  // Used to count the total number of HTTP requests made
  void IncrementRequestNumber() { mTotalRequests++; }
  uint32_t GetTotalRequestNumber() { return mTotalRequests; }
  // Used to keep "race cache with network" stats
  void IncrementCacheWonRequestNumber() { mCacheWon++; }
  uint32_t GetCacheWonRequestNumber() { return mCacheWon; }
  void IncrementNetWonRequestNumber() { mNetWon++; }
  uint32_t GetNetWonRequestNumber() { return mNetWon; }

  // Used to trigger a recheck of the captive portal status
  nsresult RecheckCaptivePortal();

  void OnProcessLaunchComplete(SocketProcessHost* aHost, bool aSucceeded);
  void OnProcessUnexpectedShutdown(SocketProcessHost* aHost);
  bool SocketProcessReady();
  static void NotifySocketProcessPrefsChanged(const char* aName, void* aSelf);
  void NotifySocketProcessPrefsChanged(const char* aName);
  static bool UseSocketProcess(bool aCheckAgain = false);

  bool IsSocketProcessLaunchComplete();

  // Call func immediately if socket process is launched completely. Otherwise,
  // |func| will be queued and then executed in the *main thread* once socket
  // process is launced.
  void CallOrWaitForSocketProcess(const std::function<void()>& aFunc);

  int32_t SocketProcessPid();
  SocketProcessHost* SocketProcess() { return mSocketProcess; }

  friend SocketProcessMemoryReporter;
  RefPtr<MemoryReportingProcess> GetSocketProcessMemoryReporter();

  static void OnTLSPrefChange(const char* aPref, void* aSelf);

  nsresult LaunchSocketProcess();

 private:
  // These shouldn't be called directly:
  // - construct using GetInstance
  // - destroy using Release
  nsIOService();
  ~nsIOService();
  nsresult SetConnectivityInternal(bool aConnectivity);

  nsresult OnNetworkLinkEvent(const char* data);

  nsresult GetCachedProtocolHandler(const char* scheme,
                                    nsIProtocolHandler** hdlrResult,
                                    uint32_t start = 0, uint32_t end = 0);
  nsresult CacheProtocolHandler(const char* scheme, nsIProtocolHandler* hdlr);

  nsresult InitializeCaptivePortalService();
  nsresult RecheckCaptivePortalIfLocalRedirect(nsIChannel* newChan);

  // Prefs wrangling
  static void PrefsChanged(const char* pref, void* self);
  void PrefsChanged(const char* pref = nullptr);
  void ParsePortList(const char* pref, bool remove);

  nsresult InitializeSocketTransportService();
  nsresult InitializeNetworkLinkService();
  nsresult InitializeProtocolProxyService();

  // consolidated helper function
  void LookupProxyInfo(nsIURI* aURI, nsIURI* aProxyURI, uint32_t aProxyFlags,
                       nsCString* aScheme, nsIProxyInfo** outPI);

  nsresult NewChannelFromURIWithProxyFlagsInternal(
      nsIURI* aURI, nsIURI* aProxyURI, uint32_t aProxyFlags,
      nsINode* aLoadingNode, nsIPrincipal* aLoadingPrincipal,
      nsIPrincipal* aTriggeringPrincipal,
      const mozilla::Maybe<mozilla::dom::ClientInfo>& aLoadingClientInfo,
      const mozilla::Maybe<mozilla::dom::ServiceWorkerDescriptor>& aController,
      uint32_t aSecurityFlags, uint32_t aContentPolicyType,
      uint32_t aSandboxFlags, nsIChannel** result);

  nsresult NewChannelFromURIWithProxyFlagsInternal(nsIURI* aURI,
                                                   nsIURI* aProxyURI,
                                                   uint32_t aProxyFlags,
                                                   nsILoadInfo* aLoadInfo,
                                                   nsIChannel** result);

  nsresult SpeculativeConnectInternal(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                      nsIInterfaceRequestor* aCallbacks,
                                      bool aAnonymous);

  void DestroySocketProcess();

 private:
  bool mOffline;
  mozilla::Atomic<bool, mozilla::Relaxed> mOfflineForProfileChange;
  bool mManageLinkStatus;
  bool mConnectivity;
  // If true, the connectivity state will be mirrored by IOService.offline
  // meaning if !mConnectivity, GetOffline() will return true
  bool mOfflineMirrorsConnectivity;

  // Used to handle SetOffline() reentrancy.  See the comment in
  // SetOffline() for more details.
  bool mSettingOffline;
  bool mSetOfflineValue;

  bool mSocketProcessLaunchComplete;

  mozilla::Atomic<bool, mozilla::Relaxed> mShutdown;
  mozilla::Atomic<bool, mozilla::Relaxed> mHttpHandlerAlreadyShutingDown;

  nsCOMPtr<nsPISocketTransportService> mSocketTransportService;
  nsCOMPtr<nsICaptivePortalService> mCaptivePortalService;
  nsCOMPtr<nsINetworkLinkService> mNetworkLinkService;
  bool mNetworkLinkServiceInitialized;

  // Cached protocol handlers, only accessed on the main thread
  nsWeakPtr mWeakHandler[NS_N(gScheme)];

  // cached categories
  nsCategoryCache<nsIChannelEventSink> mChannelEventSinks;

  Mutex mMutex;
  nsTArray<int32_t> mRestrictedPortList;

  static bool sBlockToplevelDataUriNavigations;

  uint32_t mTotalRequests;
  uint32_t mCacheWon;
  uint32_t mNetWon;

  // These timestamps are needed for collecting telemetry on PR_Connect,
  // PR_ConnectContinue and PR_Close blocking time.  If we spend very long
  // time in any of these functions we want to know if and what network
  // change has happened shortly before.
  mozilla::Atomic<PRIntervalTime> mLastOfflineStateChange;
  mozilla::Atomic<PRIntervalTime> mLastConnectivityChange;
  mozilla::Atomic<PRIntervalTime> mLastNetworkLinkChange;

  // Time a network tearing down started.
  mozilla::Atomic<PRIntervalTime> mNetTearingDownStarted;

  SocketProcessHost* mSocketProcess;

  // Events should be executed after the socket process is launched. Will
  // dispatch these events while socket process fires OnProcessLaunchComplete.
  // Note: this array is accessed only on the main thread.
  nsTArray<std::function<void()>> mPendingEvents;

 public:
  // Used for all default buffer sizes that necko allocates.
  static uint32_t gDefaultSegmentSize;
  static uint32_t gDefaultSegmentCount;
};

/**
 * Reference to the IO service singleton. May be null.
 */
extern nsIOService* gIOService;

}  // namespace net
}  // namespace mozilla

#endif  // nsIOService_h__
