/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 cindent et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nsIOService.h"
#include "nsIProtocolHandler.h"
#include "nsIFileProtocolHandler.h"
#include "nscore.h"
#include "nsIURI.h"
#include "prprf.h"
#include "nsErrorService.h"
#include "netCore.h"
#include "nsIObserverService.h"
#include "nsXPCOM.h"
#include "nsIProxiedProtocolHandler.h"
#include "nsIProxyInfo.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsCRT.h"
#include "nsSimpleNestedURI.h"
#include "nsTArray.h"
#include "nsIConsoleService.h"
#include "nsIUploadChannel2.h"
#include "nsXULAppAPI.h"
#include "nsIProtocolProxyCallback.h"
#include "nsICancelable.h"
#include "nsINetworkLinkService.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsURLHelper.h"
#include "nsIProtocolProxyService2.h"
#include "MainThreadUtils.h"
#include "nsINode.h"
#include "nsIWidget.h"
#include "nsThreadUtils.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/net/DNS.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/net/CaptivePortalService.h"
#include "mozilla/net/NetworkConnectivityService.h"
#include "mozilla/net/SocketProcessHost.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/net/SSLTokensCache.h"
#include "mozilla/Unused.h"
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsExceptionHandler.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsNSSComponent.h"
#include "ssl.h"

#ifdef MOZ_WIDGET_GTK
#  include "nsGIOProtocolHandler.h"
#endif

namespace mozilla {
namespace net {

using mozilla::Maybe;
using mozilla::dom::ClientInfo;
using mozilla::dom::ServiceWorkerDescriptor;

#define PORT_PREF_PREFIX "network.security.ports."
#define PORT_PREF(x) PORT_PREF_PREFIX x
#define MANAGE_OFFLINE_STATUS_PREF "network.manage-offline-status"
#define OFFLINE_MIRRORS_CONNECTIVITY "network.offline-mirrors-connectivity"

// Nb: these have been misnomers since bug 715770 removed the buffer cache.
// "network.segment.count" and "network.segment.size" would be better names,
// but the old names are still used to preserve backward compatibility.
#define NECKO_BUFFER_CACHE_COUNT_PREF "network.buffer.cache.count"
#define NECKO_BUFFER_CACHE_SIZE_PREF "network.buffer.cache.size"
#define NETWORK_CAPTIVE_PORTAL_PREF "network.captive-portal-service.enabled"
#define WEBRTC_PREF_PREFIX "media.peerconnection."
#define NETWORK_DNS_PREF "network.dns."

#define MAX_RECURSION_COUNT 50

nsIOService* gIOService;
static bool gHasWarnedUploadChannel2;
static bool gCaptivePortalEnabled = false;
static LazyLogModule gIOServiceLog("nsIOService");
#undef LOG
#define LOG(args) MOZ_LOG(gIOServiceLog, LogLevel::Debug, args)

// A general port blacklist.  Connections to these ports will not be allowed
// unless the protocol overrides.
//
// TODO: I am sure that there are more ports to be added.
//       This cut is based on the classic mozilla codebase

int16_t gBadPortList[] = {
    1,     // tcpmux
    7,     // echo
    9,     // discard
    11,    // systat
    13,    // daytime
    15,    // netstat
    17,    // qotd
    19,    // chargen
    20,    // ftp-data
    21,    // ftp
    22,    // ssh
    23,    // telnet
    25,    // smtp
    37,    // time
    42,    // name
    43,    // nicname
    53,    // domain
    77,    // priv-rjs
    79,    // finger
    87,    // ttylink
    95,    // supdup
    101,   // hostriame
    102,   // iso-tsap
    103,   // gppitnp
    104,   // acr-nema
    109,   // pop2
    110,   // pop3
    111,   // sunrpc
    113,   // auth
    115,   // sftp
    117,   // uucp-path
    119,   // nntp
    123,   // ntp
    135,   // loc-srv / epmap
    139,   // netbios
    143,   // imap2
    179,   // bgp
    389,   // ldap
    427,   // afp (alternate)
    465,   // smtp (alternate)
    512,   // print / exec
    513,   // login
    514,   // shell
    515,   // printer
    526,   // tempo
    530,   // courier
    531,   // chat
    532,   // netnews
    540,   // uucp
    548,   // afp
    556,   // remotefs
    563,   // nntp+ssl
    587,   // smtp (outgoing)
    601,   // syslog-conn
    636,   // ldap+ssl
    993,   // imap+ssl
    995,   // pop3+ssl
    2049,  // nfs
    3659,  // apple-sasl
    4045,  // lockd
    6000,  // x11
    6665,  // irc (alternate)
    6666,  // irc (alternate)
    6667,  // irc (default)
    6668,  // irc (alternate)
    6669,  // irc (alternate)
    6697,  // irc+tls
    0,     // Sentinel value: This MUST be zero
};

static const char kProfileChangeNetTeardownTopic[] =
    "profile-change-net-teardown";
static const char kProfileChangeNetRestoreTopic[] =
    "profile-change-net-restore";
static const char kProfileDoChange[] = "profile-do-change";

// Necko buffer defaults
uint32_t nsIOService::gDefaultSegmentSize = 4096;
uint32_t nsIOService::gDefaultSegmentCount = 24;

bool nsIOService::sBlockToplevelDataUriNavigations = false;

////////////////////////////////////////////////////////////////////////////////

nsIOService::nsIOService()
    : mOffline(true),
      mOfflineForProfileChange(false),
      mManageLinkStatus(false),
      mConnectivity(true),
      mOfflineMirrorsConnectivity(true),
      mSettingOffline(false),
      mSetOfflineValue(false),
      mSocketProcessLaunchComplete(false),
      mShutdown(false),
      mHttpHandlerAlreadyShutingDown(false),
      mNetworkLinkServiceInitialized(false),
      mChannelEventSinks(NS_CHANNEL_EVENT_SINK_CATEGORY),
      mMutex("nsIOService::mMutex"),
      mTotalRequests(0),
      mCacheWon(0),
      mNetWon(0),
      mLastOfflineStateChange(PR_IntervalNow()),
      mLastConnectivityChange(PR_IntervalNow()),
      mLastNetworkLinkChange(PR_IntervalNow()),
      mNetTearingDownStarted(0),
      mSocketProcess(nullptr) {}

static const char* gCallbackPrefs[] = {
    PORT_PREF_PREFIX,
    MANAGE_OFFLINE_STATUS_PREF,
    NECKO_BUFFER_CACHE_COUNT_PREF,
    NECKO_BUFFER_CACHE_SIZE_PREF,
    NETWORK_CAPTIVE_PORTAL_PREF,
    nullptr,
};

static const char* gCallbackPrefsForSocketProcess[] = {
    WEBRTC_PREF_PREFIX,
    NETWORK_DNS_PREF,
    "network.ssl_tokens_cache_enabled",
    nullptr,
};

static const char* gCallbackSecurityPrefs[] = {
    // Note the prefs listed below should be in sync with the code in
    // HandleTLSPrefChange().
    "security.tls.version.min",
    "security.tls.version.max",
    "security.tls.version.enable-deprecated",
    "security.tls.hello_downgrade_check",
    "security.ssl.require_safe_negotiation",
    "security.ssl.enable_false_start",
    "security.ssl.enable_alpn",
    "security.tls.enable_0rtt_data",
    "security.ssl.disable_session_identifiers",
    "security.tls.enable_post_handshake_auth",
    "security.tls.enable_delegated_credentials",
    // Note the prefs listed below should be in sync with the code in
    // SetValidationOptionsCommon().
    "security.ssl.enable_ocsp_stapling",
    "security.ssl.enable_ocsp_must_staple",
    "security.pki.certificate_transparency.mode",
    nullptr,
};

nsresult nsIOService::Init() {
  // XXX hack until xpidl supports error info directly (bug 13423)
  nsCOMPtr<nsIErrorService> errorService = nsErrorService::GetOrCreate();
  MOZ_ALWAYS_TRUE(errorService);
  errorService->RegisterErrorStringBundle(NS_ERROR_MODULE_NETWORK,
                                          NECKO_MSGS_URL);

  SSLTokensCache::Init();

  InitializeCaptivePortalService();

  // setup our bad port list stuff
  for (int i = 0; gBadPortList[i]; i++)
    mRestrictedPortList.AppendElement(gBadPortList[i]);

  // Further modifications to the port list come from prefs
  Preferences::RegisterPrefixCallbacks(nsIOService::PrefsChanged,
                                       gCallbackPrefs, this);
  PrefsChanged();

  // Register for profile change notifications
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, kProfileChangeNetTeardownTopic, true);
    observerService->AddObserver(this, kProfileChangeNetRestoreTopic, true);
    observerService->AddObserver(this, kProfileDoChange, true);
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    observerService->AddObserver(this, NS_NETWORK_LINK_TOPIC, true);
    observerService->AddObserver(this, NS_NETWORK_ID_CHANGED_TOPIC, true);
    observerService->AddObserver(this, NS_WIDGET_WAKE_OBSERVER_TOPIC, true);
    observerService->AddObserver(this, NS_PREFSERVICE_READ_TOPIC_ID, true);
  } else
    NS_WARNING("failed to get observer service");

  Preferences::AddBoolVarCache(
      &sBlockToplevelDataUriNavigations,
      "security.data_uri.block_toplevel_data_uri_navigations", false);
  Preferences::AddBoolVarCache(&mOfflineMirrorsConnectivity,
                               OFFLINE_MIRRORS_CONNECTIVITY, true);

  if (IsSocketProcessChild()) {
    Preferences::RegisterCallbacks(nsIOService::OnTLSPrefChange,
                                   gCallbackSecurityPrefs, this);
  }

  gIOService = this;

  InitializeNetworkLinkService();
  InitializeProtocolProxyService();

  SetOffline(false);

  return NS_OK;
}

nsIOService::~nsIOService() {
  if (gIOService) {
    MOZ_ASSERT(gIOService == this);
    gIOService = nullptr;
  }
}

// static
void nsIOService::OnTLSPrefChange(const char* aPref, void* aSelf) {
  MOZ_ASSERT(IsSocketProcessChild());

  if (!EnsureNSSInitializedChromeOrContent()) {
    LOG(("NSS not initialized."));
    return;
  }

  nsAutoCString pref(aPref);
  // The preferences listed in gCallbackSecurityPrefs need to be in sync with
  // the code in HandleTLSPrefChange() and SetValidationOptionsCommon().
  if (HandleTLSPrefChange(pref)) {
    LOG(("HandleTLSPrefChange done"));
  } else if (pref.EqualsLiteral("security.ssl.enable_ocsp_stapling") ||
             pref.EqualsLiteral("security.ssl.enable_ocsp_must_staple") ||
             pref.EqualsLiteral("security.pki.certificate_transparency.mode")) {
    SetValidationOptionsCommon();
  }
  nsNSSComponent::ClearSSLExternalAndInternalSessionCacheNative();
}

nsresult nsIOService::InitializeCaptivePortalService() {
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    // We only initalize a captive portal service in the main process
    return NS_OK;
  }

  mCaptivePortalService = do_GetService(NS_CAPTIVEPORTAL_CID);
  if (mCaptivePortalService) {
    return static_cast<CaptivePortalService*>(mCaptivePortalService.get())
        ->Initialize();
  }

  // Instantiate and initialize the service
  RefPtr<NetworkConnectivityService> ncs =
      NetworkConnectivityService::GetSingleton();

  return NS_OK;
}

nsresult nsIOService::InitializeSocketTransportService() {
  nsresult rv = NS_OK;

  if (!mSocketTransportService) {
    mSocketTransportService =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("failed to get socket transport service");
    }
  }

  if (mSocketTransportService) {
    rv = mSocketTransportService->Init();
    NS_ASSERTION(NS_SUCCEEDED(rv), "socket transport service init failed");
    mSocketTransportService->SetOffline(false);
  }

  return rv;
}

nsresult nsIOService::InitializeNetworkLinkService() {
  nsresult rv = NS_OK;

  if (mNetworkLinkServiceInitialized) return rv;

  if (!NS_IsMainThread()) {
    NS_WARNING("Network link service should be created on main thread");
    return NS_ERROR_FAILURE;
  }

  // go into managed mode if we can, and chrome process
  if (XRE_IsParentProcess()) {
    mNetworkLinkService =
        do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID, &rv);
  }

  if (mNetworkLinkService) {
    mNetworkLinkServiceInitialized = true;
  }

  // After initializing the networkLinkService, query the connectivity state
  OnNetworkLinkEvent(NS_NETWORK_LINK_DATA_UNKNOWN);

  return rv;
}

nsresult nsIOService::InitializeProtocolProxyService() {
  nsresult rv = NS_OK;

  if (XRE_IsParentProcess()) {
    // for early-initialization
    Unused << do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  }

  return rv;
}

already_AddRefed<nsIOService> nsIOService::GetInstance() {
  if (!gIOService) {
    RefPtr<nsIOService> ios = new nsIOService();
    if (NS_SUCCEEDED(ios->Init())) {
      MOZ_ASSERT(gIOService == ios.get());
      return ios.forget();
    }
  }
  return do_AddRef(gIOService);
}

class SocketProcessListenerProxy : public SocketProcessHost::Listener {
 public:
  SocketProcessListenerProxy() = default;
  void OnProcessLaunchComplete(SocketProcessHost* aHost, bool aSucceeded) {
    if (!gIOService) {
      return;
    }

    gIOService->OnProcessLaunchComplete(aHost, aSucceeded);
  }

  void OnProcessUnexpectedShutdown(SocketProcessHost* aHost) {
    if (!gIOService) {
      return;
    }

    gIOService->OnProcessUnexpectedShutdown(aHost);
  }
};

nsresult nsIOService::LaunchSocketProcess() {
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return NS_OK;
  }

  if (mSocketProcess) {
    return NS_OK;
  }

  if (!XRE_IsE10sParentProcess()) {
    LOG(("nsIOService skipping LaunchSocketProcess because e10s is disabled"));
    return NS_OK;
  }

  if (!Preferences::GetBool("network.process.enabled", true)) {
    LOG(("nsIOService skipping LaunchSocketProcess because of the pref"));
    return NS_OK;
  }

  Preferences::RegisterPrefixCallbacks(
      nsIOService::NotifySocketProcessPrefsChanged,
      gCallbackPrefsForSocketProcess, this);

  // The subprocess is launched asynchronously, so we wait for a callback to
  // acquire the IPDL actor.
  mSocketProcess = new SocketProcessHost(new SocketProcessListenerProxy());
  LOG(("nsIOService::LaunchSocketProcess"));
  if (!mSocketProcess->Launch()) {
    NS_WARNING("Failed to launch socket process!!");
    DestroySocketProcess();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void nsIOService::DestroySocketProcess() {
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_GetProcessType() != GeckoProcessType_Default || !mSocketProcess) {
    return;
  }

  Preferences::UnregisterPrefixCallbacks(
      nsIOService::NotifySocketProcessPrefsChanged,
      gCallbackPrefsForSocketProcess, this);

  mSocketProcess->Shutdown();
  mSocketProcess = nullptr;
}

bool nsIOService::SocketProcessReady() {
  return mSocketProcess && mSocketProcess->IsConnected();
}

static bool sUseSocketProcess = false;
static bool sUseSocketProcessChecked = false;

// static
bool nsIOService::UseSocketProcess() {
  if (sUseSocketProcessChecked) {
    return sUseSocketProcess;
  }

  sUseSocketProcessChecked = true;
  if (Preferences::GetBool("network.process.enabled")) {
    sUseSocketProcess = Preferences::GetBool(
        "network.http.network_access_on_socket_process.enabled", true);
  }
  return sUseSocketProcess;
}

// static
void nsIOService::NotifySocketProcessPrefsChanged(const char* aName,
                                                  void* aSelf) {
  static_cast<nsIOService*>(aSelf)->NotifySocketProcessPrefsChanged(aName);
}

void nsIOService::NotifySocketProcessPrefsChanged(const char* aName) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!XRE_IsParentProcess()) {
    return;
  }

  dom::Pref pref(nsCString(aName), /* isLocked */ false, Nothing(), Nothing());
  Preferences::GetPreference(&pref);
  auto sendPrefUpdate = [pref]() {
    Unused << gIOService->mSocketProcess->GetActor()->SendPreferenceUpdate(
        pref);
  };
  CallOrWaitForSocketProcess(sendPrefUpdate);
}

void nsIOService::OnProcessLaunchComplete(SocketProcessHost* aHost,
                                          bool aSucceeded) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("nsIOService::OnProcessLaunchComplete aSucceeded=%d\n", aSucceeded));

  mSocketProcessLaunchComplete = true;

  if (mShutdown || !SocketProcessReady()) {
    return;
  }

  if (!mPendingEvents.IsEmpty()) {
    nsTArray<std::function<void()>> pendingEvents;
    mPendingEvents.SwapElements(pendingEvents);
    for (auto& func : pendingEvents) {
      func();
    }
  }
}

void nsIOService::CallOrWaitForSocketProcess(
    const std::function<void()>& aFunc) {
  MOZ_ASSERT(NS_IsMainThread());
  if (IsSocketProcessLaunchComplete() && SocketProcessReady()) {
    aFunc();
  } else {
    mPendingEvents.AppendElement(aFunc);  // infallible
  }
}

int32_t nsIOService::SocketProcessPid() {
  if (!mSocketProcess) {
    return 0;
  }
  if (SocketProcessParent* actor = mSocketProcess->GetActor()) {
    return (int32_t)actor->OtherPid();
  }
  return 0;
}

bool nsIOService::IsSocketProcessLaunchComplete() {
  MOZ_ASSERT(NS_IsMainThread());
  return mSocketProcessLaunchComplete;
}

void nsIOService::OnProcessUnexpectedShutdown(SocketProcessHost* aHost) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("nsIOService::OnProcessUnexpectedShutdown\n"));
  DestroySocketProcess();
}

RefPtr<MemoryReportingProcess> nsIOService::GetSocketProcessMemoryReporter() {
  // Check the prefs here again, since we don't want to create
  // SocketProcessMemoryReporter for some tests.
  if (!Preferences::GetBool("network.process.enabled") ||
      !SocketProcessReady()) {
    return nullptr;
  }

  return new SocketProcessMemoryReporter();
}

NS_IMETHODIMP
nsIOService::SocketProcessTelemetryPing() {
  CallOrWaitForSocketProcess([]() {
    Unused << gIOService->mSocketProcess->GetActor()
                  ->SendSocketProcessTelemetryPing();
  });
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsIOService, nsIIOService, nsINetUtil, nsISpeculativeConnect,
                  nsIObserver, nsIIOServiceInternal, nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////

nsresult nsIOService::RecheckCaptivePortal() {
  MOZ_ASSERT(NS_IsMainThread(), "Must be called on the main thread");
  if (!mCaptivePortalService) {
    return NS_OK;
  }
  nsCOMPtr<nsIRunnable> task = NewRunnableMethod(
      "nsIOService::RecheckCaptivePortal", mCaptivePortalService,
      &nsICaptivePortalService::RecheckCaptivePortal);
  return NS_DispatchToMainThread(task);
}

nsresult nsIOService::RecheckCaptivePortalIfLocalRedirect(nsIChannel* newChan) {
  nsresult rv;

  if (!mCaptivePortalService) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  rv = newChan->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString host;
  rv = uri->GetHost(host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRNetAddr prAddr;
  if (PR_StringToNetAddr(host.BeginReading(), &prAddr) != PR_SUCCESS) {
    // The redirect wasn't to an IP literal, so there's probably no need
    // to trigger the captive portal detection right now. It can wait.
    return NS_OK;
  }

  NetAddr netAddr;
  PRNetAddrToNetAddr(&prAddr, &netAddr);
  if (IsIPAddrLocal(&netAddr)) {
    // Redirects to local IP addresses are probably captive portals
    RecheckCaptivePortal();
  }

  return NS_OK;
}

nsresult nsIOService::AsyncOnChannelRedirect(
    nsIChannel* oldChan, nsIChannel* newChan, uint32_t flags,
    nsAsyncRedirectVerifyHelper* helper) {
  // If a redirect to a local network address occurs, then chances are we
  // are in a captive portal, so we trigger a recheck.
  RecheckCaptivePortalIfLocalRedirect(newChan);

  // This is silly. I wish there was a simpler way to get at the global
  // reference of the contentSecurityManager. But it lives in the XPCOM
  // service registry.
  nsCOMPtr<nsIChannelEventSink> sink =
      do_GetService(NS_CONTENTSECURITYMANAGER_CONTRACTID);
  if (sink) {
    nsresult rv =
        helper->DelegateOnChannelRedirect(sink, oldChan, newChan, flags);
    if (NS_FAILED(rv)) return rv;
  }

  // Finally, our category
  nsCOMArray<nsIChannelEventSink> entries;
  mChannelEventSinks.GetEntries(entries);
  int32_t len = entries.Count();
  for (int32_t i = 0; i < len; ++i) {
    nsresult rv =
        helper->DelegateOnChannelRedirect(entries[i], oldChan, newChan, flags);
    if (NS_FAILED(rv)) return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(oldChan));

  // Collect the redirection from HTTP(S) only.
  if (httpChan) {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIURI> newURI;
    newChan->GetURI(getter_AddRefs(newURI));
    MOZ_ASSERT(newURI);

    nsAutoCString scheme;
    newURI->GetScheme(scheme);
    MOZ_ASSERT(!scheme.IsEmpty());

    Telemetry::AccumulateCategoricalKeyed(
        scheme,
        oldChan->IsDocument()
            ? Telemetry::LABELS_NETWORK_HTTP_REDIRECT_TO_SCHEME::topLevel
            : Telemetry::LABELS_NETWORK_HTTP_REDIRECT_TO_SCHEME::subresource);
  }

  return NS_OK;
}

nsresult nsIOService::CacheProtocolHandler(const char* scheme,
                                           nsIProtocolHandler* handler) {
  MOZ_ASSERT(NS_IsMainThread());

  for (unsigned int i = 0; i < NS_N(gScheme); i++) {
    if (!nsCRT::strcasecmp(scheme, gScheme[i])) {
      nsresult rv;
      NS_ASSERTION(!mWeakHandler[i], "Protocol handler already cached");
      // Make sure the handler supports weak references.
      nsCOMPtr<nsISupportsWeakReference> factoryPtr =
          do_QueryInterface(handler, &rv);
      if (!factoryPtr) {
        // Don't cache handlers that don't support weak reference as
        // there is real danger of a circular reference.
#ifdef DEBUG_dp
        printf(
            "DEBUG: %s protcol handler doesn't support weak ref. Not cached.\n",
            scheme);
#endif /* DEBUG_dp */
        return NS_ERROR_FAILURE;
      }
      mWeakHandler[i] = do_GetWeakReference(handler);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult nsIOService::GetCachedProtocolHandler(const char* scheme,
                                               nsIProtocolHandler** result,
                                               uint32_t start, uint32_t end) {
  MOZ_ASSERT(NS_IsMainThread());

  uint32_t len = end - start - 1;
  for (unsigned int i = 0; i < NS_N(gScheme); i++) {
    if (!mWeakHandler[i]) continue;

    // handle unterminated strings
    // start is inclusive, end is exclusive, len = end - start - 1
    if (end ? (!nsCRT::strncasecmp(scheme + start, gScheme[i], len) &&
               gScheme[i][len] == '\0')
            : (!nsCRT::strcasecmp(scheme, gScheme[i]))) {
      return CallQueryReferent(mWeakHandler[i].get(), result);
    }
  }
  return NS_ERROR_FAILURE;
}

static bool UsesExternalProtocolHandler(const char* aScheme) {
  if (NS_LITERAL_CSTRING("file").Equals(aScheme) ||
      NS_LITERAL_CSTRING("chrome").Equals(aScheme) ||
      NS_LITERAL_CSTRING("resource").Equals(aScheme)) {
    // Don't allow file:, chrome: or resource: URIs to be handled with
    // nsExternalProtocolHandler, since internally we rely on being able to
    // use and read from these URIs.
    return false;
  }

  // When ftp protocol is disabled, return true if external protocol handler was
  // not explicitly disabled by the prererence.
  if (NS_LITERAL_CSTRING("ftp").Equals(aScheme) &&
      !Preferences::GetBool("network.ftp.enabled", true) &&
      Preferences::GetBool("network.protocol-handler.external.ftp", true)) {
    return true;
  }

  for (const auto& forcedExternalScheme : gForcedExternalSchemes) {
    if (!nsCRT::strcasecmp(forcedExternalScheme, aScheme)) {
      return true;
    }
  }

  nsAutoCString pref("network.protocol-handler.external.");
  pref += aScheme;

  return Preferences::GetBool(pref.get(), false);
}

NS_IMETHODIMP
nsIOService::GetProtocolHandler(const char* scheme,
                                nsIProtocolHandler** result) {
  nsresult rv;

  NS_ENSURE_ARG_POINTER(scheme);
  // XXX we may want to speed this up by introducing our own protocol
  // scheme -> protocol handler mapping, avoiding the string manipulation
  // and service manager stuff

  rv = GetCachedProtocolHandler(scheme, result);
  if (NS_SUCCEEDED(rv)) return rv;

  if (scheme[0] != '\0' && !UsesExternalProtocolHandler(scheme)) {
    nsAutoCString contractID(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX);
    contractID += scheme;
    ToLowerCase(contractID);

    rv = CallGetService(contractID.get(), result);
    if (NS_SUCCEEDED(rv)) {
      CacheProtocolHandler(scheme, *result);
      return rv;
    }

#ifdef MOZ_WIDGET_GTK
    // check to see whether GVFS can handle this URI scheme. otherwise, we
    // failover to using the default protocol handler.

    RefPtr<nsGIOProtocolHandler> gioHandler =
        nsGIOProtocolHandler::GetSingleton();
    if (gioHandler->IsSupportedProtocol(nsCString(scheme))) {
      gioHandler.forget(result);
      return NS_OK;
    }
#endif
  }

  // Okay we don't have a protocol handler to handle this url type, so use
  // the default protocol handler.  This will cause urls to get dispatched
  // out to the OS ('cause we can't do anything with them) when we try to
  // read from a channel created by the default protocol handler.

  rv = CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "default", result);
  if (NS_FAILED(rv)) return NS_ERROR_UNKNOWN_PROTOCOL;

  return rv;
}

NS_IMETHODIMP
nsIOService::ExtractScheme(const nsACString& inURI, nsACString& scheme) {
  return net_ExtractURLScheme(inURI, scheme);
}

NS_IMETHODIMP
nsIOService::HostnameIsLocalIPAddress(nsIURI* aURI, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_ARG_POINTER(innerURI);

  nsAutoCString host;
  nsresult rv = innerURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aResult = false;

  PRNetAddr addr;
  PRStatus result = PR_StringToNetAddr(host.get(), &addr);
  if (result == PR_SUCCESS) {
    NetAddr netAddr;
    PRNetAddrToNetAddr(&addr, &netAddr);
    if (IsIPAddrLocal(&netAddr)) {
      *aResult = true;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsIOService::HostnameIsSharedIPAddress(nsIURI* aURI, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_ARG_POINTER(innerURI);

  nsAutoCString host;
  nsresult rv = innerURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aResult = false;

  PRNetAddr addr;
  PRStatus result = PR_StringToNetAddr(host.get(), &addr);
  if (result == PR_SUCCESS) {
    NetAddr netAddr;
    PRNetAddrToNetAddr(&addr, &netAddr);
    if (IsIPAddrShared(&netAddr)) {
      *aResult = true;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetProtocolFlags(const char* scheme, uint32_t* flags) {
  nsCOMPtr<nsIProtocolHandler> handler;
  nsresult rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
  if (NS_FAILED(rv)) return rv;

  // We can't call DoGetProtocolFlags here because we don't have a URI. This
  // API is used by (and only used by) extensions, which is why it's still
  // around. Calling this on a scheme with dynamic flags will throw.
  rv = handler->GetProtocolFlags(flags);
#if !IS_ORIGIN_IS_FULL_SPEC_DEFINED
  MOZ_RELEASE_ASSERT(!(*flags & nsIProtocolHandler::ORIGIN_IS_FULL_SPEC),
                     "ORIGIN_IS_FULL_SPEC is unsupported but used");
#endif
  return rv;
}

class AutoIncrement {
 public:
  explicit AutoIncrement(uint32_t* var) : mVar(var) { ++*var; }
  ~AutoIncrement() { --*mVar; }

 private:
  uint32_t* mVar;
};

nsresult nsIOService::NewURI(const nsACString& aSpec, const char* aCharset,
                             nsIURI* aBaseURI, nsIURI** result) {
  return NS_NewURI(result, aSpec, aCharset, aBaseURI);
}

NS_IMETHODIMP
nsIOService::NewFileURI(nsIFile* file, nsIURI** result) {
  nsresult rv;
  NS_ENSURE_ARG_POINTER(file);

  nsCOMPtr<nsIProtocolHandler> handler;

  rv = GetProtocolHandler("file", getter_AddRefs(handler));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFileProtocolHandler> fileHandler(do_QueryInterface(handler, &rv));
  if (NS_FAILED(rv)) return rv;

  return fileHandler->NewFileURI(file, result);
}

// static
already_AddRefed<nsIURI> nsIOService::CreateExposableURI(nsIURI* aURI) {
  MOZ_ASSERT(aURI, "Must have a URI");
  nsCOMPtr<nsIURI> uri = aURI;

  nsAutoCString userPass;
  uri->GetUserPass(userPass);
  if (!userPass.IsEmpty()) {
    DebugOnly<nsresult> rv =
        NS_MutateURI(uri).SetUserPass(EmptyCString()).Finalize(uri);
    MOZ_ASSERT(NS_SUCCEEDED(rv) && uri, "Mutating URI should never fail");
  }
  return uri.forget();
}

NS_IMETHODIMP
nsIOService::CreateExposableURI(nsIURI* aURI, nsIURI** _result) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(_result);
  nsCOMPtr<nsIURI> exposableURI = CreateExposableURI(aURI);
  exposableURI.forget(_result);
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::NewChannelFromURI(nsIURI* aURI, nsINode* aLoadingNode,
                               nsIPrincipal* aLoadingPrincipal,
                               nsIPrincipal* aTriggeringPrincipal,
                               uint32_t aSecurityFlags,
                               uint32_t aContentPolicyType,
                               nsIChannel** result) {
  return NewChannelFromURIWithProxyFlags(aURI,
                                         nullptr,  // aProxyURI
                                         0,        // aProxyFlags
                                         aLoadingNode, aLoadingPrincipal,
                                         aTriggeringPrincipal, aSecurityFlags,
                                         aContentPolicyType, result);
}
nsresult nsIOService::NewChannelFromURIWithClientAndController(
    nsIURI* aURI, nsINode* aLoadingNode, nsIPrincipal* aLoadingPrincipal,
    nsIPrincipal* aTriggeringPrincipal,
    const Maybe<ClientInfo>& aLoadingClientInfo,
    const Maybe<ServiceWorkerDescriptor>& aController, uint32_t aSecurityFlags,
    uint32_t aContentPolicyType, uint32_t aSandboxFlags, nsIChannel** aResult) {
  return NewChannelFromURIWithProxyFlagsInternal(
      aURI,
      nullptr,  // aProxyURI
      0,        // aProxyFlags
      aLoadingNode, aLoadingPrincipal, aTriggeringPrincipal, aLoadingClientInfo,
      aController, aSecurityFlags, aContentPolicyType, aSandboxFlags, aResult);
}

NS_IMETHODIMP
nsIOService::NewChannelFromURIWithLoadInfo(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                           nsIChannel** result) {
  return NewChannelFromURIWithProxyFlagsInternal(aURI,
                                                 nullptr,  // aProxyURI
                                                 0,        // aProxyFlags
                                                 aLoadInfo, result);
}

nsresult nsIOService::NewChannelFromURIWithProxyFlagsInternal(
    nsIURI* aURI, nsIURI* aProxyURI, uint32_t aProxyFlags,
    nsINode* aLoadingNode, nsIPrincipal* aLoadingPrincipal,
    nsIPrincipal* aTriggeringPrincipal,
    const Maybe<ClientInfo>& aLoadingClientInfo,
    const Maybe<ServiceWorkerDescriptor>& aController, uint32_t aSecurityFlags,
    uint32_t aContentPolicyType, uint32_t aSandboxFlags, nsIChannel** result) {
  // Ideally all callers of NewChannelFromURIWithProxyFlagsInternal provide
  // the necessary arguments to create a loadinfo.
  //
  // Note, historically this could be called with nullptr aLoadingNode,
  // aLoadingPrincipal, and aTriggeringPrincipal from addons using
  // newChannelFromURIWithProxyFlags().  This code tried to accomodate
  // by not creating a LoadInfo in such cases.  Now that both the legacy
  // addons and that API are gone we could possibly require always creating a
  // LoadInfo here.  See bug 1432205.
  nsCOMPtr<nsILoadInfo> loadInfo;

  // TYPE_DOCUMENT loads don't require a loadingNode or principal, but other
  // types do.
  if (aLoadingNode || aLoadingPrincipal ||
      aContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT) {
    loadInfo = new LoadInfo(aLoadingPrincipal, aTriggeringPrincipal,
                            aLoadingNode, aSecurityFlags, aContentPolicyType,
                            aLoadingClientInfo, aController, aSandboxFlags);
  }
  if (!loadInfo) {
    JSContext* cx = nsContentUtils::GetCurrentJSContext();
    // if coming from JS we like to know the JS stack, otherwise
    // we just assert that we are able to create a valid loadinfo!
    if (cx) {
      JS::UniqueChars chars = xpc_PrintJSStack(cx,
                                               /*showArgs=*/false,
                                               /*showLocals=*/false,
                                               /*showThisProps=*/false);
      nsDependentCString stackTrace(chars.get());
      CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::Bug_1541161,
                                         stackTrace);
    }
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "Please pass security info when creating a channel");
    return NS_ERROR_INVALID_ARG;
  }
  return NewChannelFromURIWithProxyFlagsInternal(aURI, aProxyURI, aProxyFlags,
                                                 loadInfo, result);
}

nsresult nsIOService::NewChannelFromURIWithProxyFlagsInternal(
    nsIURI* aURI, nsIURI* aProxyURI, uint32_t aProxyFlags,
    nsILoadInfo* aLoadInfo, nsIChannel** result) {
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoCString scheme;
  rv = aURI->GetScheme(scheme);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIProtocolHandler> handler;
  rv = GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
  if (NS_FAILED(rv)) return rv;

  uint32_t protoFlags;
  rv = handler->DoGetProtocolFlags(aURI, &protoFlags);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsIProxiedProtocolHandler> pph = do_QueryInterface(handler);
  if (pph) {
    rv = pph->NewProxiedChannel(aURI, nullptr, aProxyFlags, aProxyURI,
                                aLoadInfo, getter_AddRefs(channel));
  } else {
    rv = handler->NewChannel(aURI, aLoadInfo, getter_AddRefs(channel));
  }
  if (NS_FAILED(rv)) return rv;

  // Make sure that all the individual protocolhandlers attach a loadInfo.
  if (aLoadInfo) {
    // make sure we have the same instance of loadInfo on the newly created
    // channel
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    if (aLoadInfo != loadInfo) {
      MOZ_ASSERT(false, "newly created channel must have a loadinfo attached");
      return NS_ERROR_UNEXPECTED;
    }

    // If we're sandboxed, make sure to clear any owner the channel
    // might already have.
    if (loadInfo->GetLoadingSandboxed()) {
      channel->SetOwner(nullptr);
    }
  }

  // Some extensions override the http protocol handler and provide their own
  // implementation. The channels returned from that implementation doesn't
  // seem to always implement the nsIUploadChannel2 interface, presumably
  // because it's a new interface.
  // Eventually we should remove this and simply require that http channels
  // implement the new interface.
  // See bug 529041
  if (!gHasWarnedUploadChannel2 && scheme.EqualsLiteral("http")) {
    nsCOMPtr<nsIUploadChannel2> uploadChannel2 = do_QueryInterface(channel);
    if (!uploadChannel2) {
      nsCOMPtr<nsIConsoleService> consoleService =
          do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (consoleService) {
        consoleService->LogStringMessage(
            u"Http channel implementation "
            "doesn't support nsIUploadChannel2. An extension has "
            "supplied a non-functional http protocol handler. This will "
            "break behavior and in future releases not work at all.");
      }
      gHasWarnedUploadChannel2 = true;
    }
  }

  channel.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::NewChannelFromURIWithProxyFlags(
    nsIURI* aURI, nsIURI* aProxyURI, uint32_t aProxyFlags,
    nsINode* aLoadingNode, nsIPrincipal* aLoadingPrincipal,
    nsIPrincipal* aTriggeringPrincipal, uint32_t aSecurityFlags,
    uint32_t aContentPolicyType, nsIChannel** result) {
  return NewChannelFromURIWithProxyFlagsInternal(
      aURI, aProxyURI, aProxyFlags, aLoadingNode, aLoadingPrincipal,
      aTriggeringPrincipal, Maybe<ClientInfo>(),
      Maybe<ServiceWorkerDescriptor>(), aSecurityFlags, aContentPolicyType, 0,
      result);
}

NS_IMETHODIMP
nsIOService::NewChannel(const nsACString& aSpec, const char* aCharset,
                        nsIURI* aBaseURI, nsINode* aLoadingNode,
                        nsIPrincipal* aLoadingPrincipal,
                        nsIPrincipal* aTriggeringPrincipal,
                        uint32_t aSecurityFlags, uint32_t aContentPolicyType,
                        nsIChannel** result) {
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  rv = NewURI(aSpec, aCharset, aBaseURI, getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  return NewChannelFromURI(uri, aLoadingNode, aLoadingPrincipal,
                           aTriggeringPrincipal, aSecurityFlags,
                           aContentPolicyType, result);
}

bool nsIOService::IsLinkUp() {
  InitializeNetworkLinkService();

  if (!mNetworkLinkService) {
    // We cannot decide, assume the link is up
    return true;
  }

  bool isLinkUp;
  nsresult rv;
  rv = mNetworkLinkService->GetIsLinkUp(&isLinkUp);
  if (NS_FAILED(rv)) {
    return true;
  }

  return isLinkUp;
}

NS_IMETHODIMP
nsIOService::GetOffline(bool* offline) {
  if (mOfflineMirrorsConnectivity) {
    *offline = mOffline || !mConnectivity;
  } else {
    *offline = mOffline;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::SetOffline(bool offline) {
  LOG(("nsIOService::SetOffline offline=%d\n", offline));
  // When someone wants to go online (!offline) after we got XPCOM shutdown
  // throw ERROR_NOT_AVAILABLE to prevent return to online state.
  if ((mShutdown || mOfflineForProfileChange) && !offline)
    return NS_ERROR_NOT_AVAILABLE;

  // SetOffline() may re-enter while it's shutting down services.
  // If that happens, save the most recent value and it will be
  // processed when the first SetOffline() call is done bringing
  // down the service.
  mSetOfflineValue = offline;
  if (mSettingOffline) {
    return NS_OK;
  }

  mSettingOffline = true;

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();

  NS_ASSERTION(observerService, "The observer service should not be null");

  if (XRE_IsParentProcess()) {
    if (observerService) {
      (void)observerService->NotifyObservers(nullptr,
                                             NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC,
                                             offline ? u"true" : u"false");
    }
  }

  nsIIOService* subject = static_cast<nsIIOService*>(this);
  while (mSetOfflineValue != mOffline) {
    offline = mSetOfflineValue;

    if (offline && !mOffline) {
      mOffline = true;  // indicate we're trying to shutdown

      // don't care if notifications fail
      if (observerService)
        observerService->NotifyObservers(subject,
                                         NS_IOSERVICE_GOING_OFFLINE_TOPIC,
                                         u"" NS_IOSERVICE_OFFLINE);

      if (mSocketTransportService) mSocketTransportService->SetOffline(true);

      mLastOfflineStateChange = PR_IntervalNow();
      if (observerService)
        observerService->NotifyObservers(subject,
                                         NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                         u"" NS_IOSERVICE_OFFLINE);
    } else if (!offline && mOffline) {
      // go online
      InitializeSocketTransportService();
      mOffline = false;  // indicate success only AFTER we've
                         // brought up the services

      mLastOfflineStateChange = PR_IntervalNow();
      // don't care if notification fails
      // Only send the ONLINE notification if there is connectivity
      if (observerService && mConnectivity) {
        observerService->NotifyObservers(subject,
                                         NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                         (u"" NS_IOSERVICE_ONLINE));
      }
    }
  }

  // Don't notify here, as the above notifications (if used) suffice.
  if ((mShutdown || mOfflineForProfileChange) && mOffline) {
    if (mSocketTransportService) {
      DebugOnly<nsresult> rv = mSocketTransportService->Shutdown(mShutdown);
      NS_ASSERTION(NS_SUCCEEDED(rv),
                   "socket transport service shutdown failed");
    }
  }

  mSettingOffline = false;

  return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetConnectivity(bool* aConnectivity) {
  *aConnectivity = mConnectivity;
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::SetConnectivity(bool aConnectivity) {
  LOG(("nsIOService::SetConnectivity aConnectivity=%d\n", aConnectivity));
  // This should only be called from ContentChild to pass the connectivity
  // value from the chrome process to the content process.
  if (XRE_IsParentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return SetConnectivityInternal(aConnectivity);
}

nsresult nsIOService::SetConnectivityInternal(bool aConnectivity) {
  LOG(("nsIOService::SetConnectivityInternal aConnectivity=%d\n",
       aConnectivity));
  if (mConnectivity == aConnectivity) {
    // Nothing to do here.
    return NS_OK;
  }
  mConnectivity = aConnectivity;

  // This is used for PR_Connect PR_Close telemetry so it is important that
  // we have statistic about network change event even if we are offline.
  mLastConnectivityChange = PR_IntervalNow();

  if (mCaptivePortalService) {
    if (aConnectivity && gCaptivePortalEnabled) {
      // This will also trigger a captive portal check for the new network
      static_cast<CaptivePortalService*>(mCaptivePortalService.get())->Start();
    } else {
      static_cast<CaptivePortalService*>(mCaptivePortalService.get())->Stop();
    }
  }

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    return NS_OK;
  }
  // This notification sends the connectivity to the child processes
  if (XRE_IsParentProcess()) {
    observerService->NotifyObservers(nullptr,
                                     NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC,
                                     aConnectivity ? u"true" : u"false");
  }

  if (mOffline) {
    // We don't need to send any notifications if we're offline
    return NS_OK;
  }

  if (aConnectivity) {
    // If we were previously offline due to connectivity=false,
    // send the ONLINE notification
    observerService->NotifyObservers(static_cast<nsIIOService*>(this),
                                     NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                     (u"" NS_IOSERVICE_ONLINE));
  } else {
    // If we were previously online and lost connectivity
    // send the OFFLINE notification
    observerService->NotifyObservers(static_cast<nsIIOService*>(this),
                                     NS_IOSERVICE_GOING_OFFLINE_TOPIC,
                                     u"" NS_IOSERVICE_OFFLINE);
    observerService->NotifyObservers(static_cast<nsIIOService*>(this),
                                     NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                     u"" NS_IOSERVICE_OFFLINE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::AllowPort(int32_t inPort, const char* scheme, bool* _retval) {
  int32_t port = inPort;
  if (port == -1) {
    *_retval = true;
    return NS_OK;
  }

  if (port == 0) {
    *_retval = false;
    return NS_OK;
  }

  nsTArray<int32_t> restrictedPortList;
  {
    MutexAutoLock lock(mMutex);
    restrictedPortList.Assign(mRestrictedPortList);
  }

  // first check to see if the port is in our blacklist:
  int32_t badPortListCnt = restrictedPortList.Length();
  for (int i = 0; i < badPortListCnt; i++) {
    if (port == restrictedPortList[i]) {
      *_retval = false;

      // check to see if the protocol wants to override
      if (!scheme) return NS_OK;

      // We don't support get protocol handler off main thread.
      if (!NS_IsMainThread()) {
        return NS_OK;
      }
      nsCOMPtr<nsIProtocolHandler> handler;
      nsresult rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
      if (NS_FAILED(rv)) return rv;

      // let the protocol handler decide
      return handler->AllowPort(port, scheme, _retval);
    }
  }

  *_retval = true;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

// static
void nsIOService::PrefsChanged(const char* pref, void* self) {
  static_cast<nsIOService*>(self)->PrefsChanged(pref);
}

void nsIOService::PrefsChanged(const char* pref) {
  // Look for extra ports to block
  if (!pref || strcmp(pref, PORT_PREF("banned")) == 0)
    ParsePortList(PORT_PREF("banned"), false);

  // ...as well as previous blocks to remove.
  if (!pref || strcmp(pref, PORT_PREF("banned.override")) == 0)
    ParsePortList(PORT_PREF("banned.override"), true);

  if (!pref || strcmp(pref, MANAGE_OFFLINE_STATUS_PREF) == 0) {
    bool manage;
    if (mNetworkLinkServiceInitialized &&
        NS_SUCCEEDED(
            Preferences::GetBool(MANAGE_OFFLINE_STATUS_PREF, &manage))) {
      LOG(("nsIOService::PrefsChanged ManageOfflineStatus manage=%d\n",
           manage));
      SetManageOfflineStatus(manage);
    }
  }

  if (!pref || strcmp(pref, NECKO_BUFFER_CACHE_COUNT_PREF) == 0) {
    int32_t count;
    if (NS_SUCCEEDED(
            Preferences::GetInt(NECKO_BUFFER_CACHE_COUNT_PREF, &count)))
      /* check for bogus values and default if we find such a value */
      if (count > 0) gDefaultSegmentCount = count;
  }

  if (!pref || strcmp(pref, NECKO_BUFFER_CACHE_SIZE_PREF) == 0) {
    int32_t size;
    if (NS_SUCCEEDED(Preferences::GetInt(NECKO_BUFFER_CACHE_SIZE_PREF, &size)))
      /* check for bogus values and default if we find such a value
       * the upper limit here is arbitrary. having a 1mb segment size
       * is pretty crazy.  if you remove this, consider adding some
       * integer rollover test.
       */
      if (size > 0 && size < 1024 * 1024) gDefaultSegmentSize = size;
    NS_WARNING_ASSERTION(!(size & (size - 1)),
                         "network segment size is not a power of 2!");
  }

  if (!pref || strcmp(pref, NETWORK_CAPTIVE_PORTAL_PREF) == 0) {
    nsresult rv = Preferences::GetBool(NETWORK_CAPTIVE_PORTAL_PREF,
                                       &gCaptivePortalEnabled);
    if (NS_SUCCEEDED(rv) && mCaptivePortalService) {
      if (gCaptivePortalEnabled) {
        static_cast<CaptivePortalService*>(mCaptivePortalService.get())
            ->Start();
      } else {
        static_cast<CaptivePortalService*>(mCaptivePortalService.get())->Stop();
      }
    }
  }
}

void nsIOService::ParsePortList(const char* pref, bool remove) {
  nsAutoCString portList;
  nsTArray<int32_t> restrictedPortList;
  {
    MutexAutoLock lock(mMutex);
    restrictedPortList.Assign(std::move(mRestrictedPortList));
  }
  // Get a pref string and chop it up into a list of ports.
  Preferences::GetCString(pref, portList);
  if (!portList.IsVoid()) {
    nsTArray<nsCString> portListArray;
    ParseString(portList, ',', portListArray);
    uint32_t index;
    for (index = 0; index < portListArray.Length(); index++) {
      portListArray[index].StripWhitespace();
      int32_t portBegin, portEnd;

      if (PR_sscanf(portListArray[index].get(), "%d-%d", &portBegin,
                    &portEnd) == 2) {
        if ((portBegin < 65536) && (portEnd < 65536)) {
          int32_t curPort;
          if (remove) {
            for (curPort = portBegin; curPort <= portEnd; curPort++)
              restrictedPortList.RemoveElement(curPort);
          } else {
            for (curPort = portBegin; curPort <= portEnd; curPort++)
              restrictedPortList.AppendElement(curPort);
          }
        }
      } else {
        nsresult aErrorCode;
        int32_t port = portListArray[index].ToInteger(&aErrorCode);
        if (NS_SUCCEEDED(aErrorCode) && port < 65536) {
          if (remove)
            restrictedPortList.RemoveElement(port);
          else
            restrictedPortList.AppendElement(port);
        }
      }
    }
  }

  MutexAutoLock lock(mMutex);
  mRestrictedPortList.Assign(std::move(restrictedPortList));
}

class nsWakeupNotifier : public Runnable {
 public:
  explicit nsWakeupNotifier(nsIIOServiceInternal* ioService)
      : Runnable("net::nsWakeupNotifier"), mIOService(ioService) {}

  NS_IMETHOD Run() override { return mIOService->NotifyWakeup(); }

 private:
  virtual ~nsWakeupNotifier() = default;
  nsCOMPtr<nsIIOServiceInternal> mIOService;
};

NS_IMETHODIMP
nsIOService::NotifyWakeup() {
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();

  NS_ASSERTION(observerService, "The observer service should not be null");

  if (observerService && StaticPrefs::network_notify_changed()) {
    (void)observerService->NotifyObservers(nullptr, NS_NETWORK_LINK_TOPIC,
                                           (u"" NS_NETWORK_LINK_DATA_CHANGED));
  }

  RecheckCaptivePortal();

  return NS_OK;
}

void nsIOService::SetHttpHandlerAlreadyShutingDown() {
  if (!mShutdown && !mOfflineForProfileChange) {
    mNetTearingDownStarted = PR_IntervalNow();
    mHttpHandlerAlreadyShutingDown = true;
  }
}

// nsIObserver interface
NS_IMETHODIMP
nsIOService::Observe(nsISupports* subject, const char* topic,
                     const char16_t* data) {
  if (!strcmp(topic, kProfileChangeNetTeardownTopic)) {
    if (!mHttpHandlerAlreadyShutingDown) {
      mNetTearingDownStarted = PR_IntervalNow();
    }
    mHttpHandlerAlreadyShutingDown = false;
    if (!mOffline) {
      mOfflineForProfileChange = true;
      SetOffline(true);
    }
  } else if (!strcmp(topic, kProfileChangeNetRestoreTopic)) {
    if (mOfflineForProfileChange) {
      mOfflineForProfileChange = false;
      SetOffline(false);
    }
  } else if (!strcmp(topic, kProfileDoChange)) {
    if (!data) {
      return NS_OK;
    }
    if (NS_LITERAL_STRING("startup").Equals(data)) {
      // Lazy initialization of network link service (see bug 620472)
      InitializeNetworkLinkService();
      // Set up the initilization flag regardless the actuall result.
      // If we fail here, we will fail always on.
      mNetworkLinkServiceInitialized = true;

      // And now reflect the preference setting
      PrefsChanged(MANAGE_OFFLINE_STATUS_PREF);

      // Bug 870460 - Read cookie database at an early-as-possible time
      // off main thread. Hence, we have more chance to finish db query
      // before something calls into the cookie service.
      nsCOMPtr<nsISupports> cookieServ =
          do_GetService(NS_COOKIESERVICE_CONTRACTID);
    } else if (NS_LITERAL_STRING("xpcshell-do-get-profile").Equals(data)) {
      // xpcshell doesn't read user profile.
      LaunchSocketProcess();
    }
  } else if (!strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    // Remember we passed XPCOM shutdown notification to prevent any
    // changes of the offline status from now. We must not allow going
    // online after this point.
    mShutdown = true;

    if (!mHttpHandlerAlreadyShutingDown && !mOfflineForProfileChange) {
      mNetTearingDownStarted = PR_IntervalNow();
    }
    mHttpHandlerAlreadyShutingDown = false;

    SetOffline(true);

    if (mCaptivePortalService) {
      static_cast<CaptivePortalService*>(mCaptivePortalService.get())->Stop();
      mCaptivePortalService = nullptr;
    }

    SSLTokensCache::Shutdown();

    DestroySocketProcess();

    if (IsSocketProcessChild()) {
      Preferences::UnregisterCallbacks(nsIOService::OnTLSPrefChange,
                                       gCallbackSecurityPrefs, this);
      NSSShutdownForSocketProcess();
    }
  } else if (!strcmp(topic, NS_NETWORK_LINK_TOPIC)) {
    OnNetworkLinkEvent(NS_ConvertUTF16toUTF8(data).get());
  } else if (!strcmp(topic, NS_NETWORK_ID_CHANGED_TOPIC)) {
    LOG(("nsIOService::OnNetworkLinkEvent Network id changed"));
  } else if (!strcmp(topic, NS_WIDGET_WAKE_OBSERVER_TOPIC)) {
    // coming back alive from sleep
    // this indirection brought to you by:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1152048#c19
    nsCOMPtr<nsIRunnable> wakeupNotifier = new nsWakeupNotifier(this);
    NS_DispatchToMainThread(wakeupNotifier);
  } else if (!strcmp(topic, NS_PREFSERVICE_READ_TOPIC_ID)) {
    // Launch socket process after we load user's pref. This is to make sure
    // that socket process can get the latest prefs.
    LaunchSocketProcess();
  }

  return NS_OK;
}

// nsINetUtil interface
NS_IMETHODIMP
nsIOService::ParseRequestContentType(const nsACString& aTypeHeader,
                                     nsACString& aCharset, bool* aHadCharset,
                                     nsACString& aContentType) {
  net_ParseRequestContentType(aTypeHeader, aContentType, aCharset, aHadCharset);
  return NS_OK;
}

// nsINetUtil interface
NS_IMETHODIMP
nsIOService::ParseResponseContentType(const nsACString& aTypeHeader,
                                      nsACString& aCharset, bool* aHadCharset,
                                      nsACString& aContentType) {
  net_ParseContentType(aTypeHeader, aContentType, aCharset, aHadCharset);
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::ProtocolHasFlags(nsIURI* uri, uint32_t flags, bool* result) {
  NS_ENSURE_ARG(uri);

  *result = false;
  nsAutoCString scheme;
  nsresult rv = uri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // Grab the protocol flags from the URI.
  uint32_t protocolFlags;
  nsCOMPtr<nsIProtocolHandler> handler;
  rv = GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = handler->DoGetProtocolFlags(uri, &protocolFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  *result = (protocolFlags & flags) == flags;
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::URIChainHasFlags(nsIURI* uri, uint32_t flags, bool* result) {
  nsresult rv = ProtocolHasFlags(uri, flags, result);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*result) {
    return rv;
  }

  // Dig deeper into the chain.  Note that this is not a do/while loop to
  // avoid the extra addref/release on |uri| in the common (non-nested) case.
  nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(uri);
  while (nestedURI) {
    nsCOMPtr<nsIURI> innerURI;
    rv = nestedURI->GetInnerURI(getter_AddRefs(innerURI));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ProtocolHasFlags(innerURI, flags, result);

    if (*result) {
      return rv;
    }

    nestedURI = do_QueryInterface(innerURI);
  }

  return rv;
}

NS_IMETHODIMP
nsIOService::SetManageOfflineStatus(bool aManage) {
  LOG(("nsIOService::SetManageOfflineStatus aManage=%d\n", aManage));
  mManageLinkStatus = aManage;

  // When detection is not activated, the default connectivity state is true.
  if (!mManageLinkStatus) {
    SetConnectivityInternal(true);
    return NS_OK;
  }

  InitializeNetworkLinkService();
  // If the NetworkLinkService is already initialized, it does not call
  // OnNetworkLinkEvent. This is needed, when mManageLinkStatus goes from
  // false to true.
  OnNetworkLinkEvent(NS_NETWORK_LINK_DATA_UNKNOWN);
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetManageOfflineStatus(bool* aManage) {
  *aManage = mManageLinkStatus;
  return NS_OK;
}

// input argument 'data' is already UTF8'ed
nsresult nsIOService::OnNetworkLinkEvent(const char* data) {
  if (IsNeckoChild() || IsSocketProcessChild()) {
    // There is nothing IO service could do on the child process
    // with this at the moment.  Feel free to add functionality
    // here at will, though.
    return NS_OK;
  }

  if (mShutdown) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCString dataAsString(data);
  for (auto* cp : mozilla::dom::ContentParent::AllProcesses(
           mozilla::dom::ContentParent::eLive)) {
    PNeckoParent* neckoParent = SingleManagedOrNull(cp->ManagedPNeckoParent());
    if (!neckoParent) {
      continue;
    }
    Unused << neckoParent->SendNetworkChangeNotification(dataAsString);
  }

  LOG(("nsIOService::OnNetworkLinkEvent data:%s\n", data));
  if (!mNetworkLinkService) {
    return NS_ERROR_FAILURE;
  }

  if (!mManageLinkStatus) {
    LOG(("nsIOService::OnNetworkLinkEvent mManageLinkStatus=false\n"));
    return NS_OK;
  }

  bool isUp = true;
  if (!strcmp(data, NS_NETWORK_LINK_DATA_CHANGED)) {
    mLastNetworkLinkChange = PR_IntervalNow();
    // CHANGED means UP/DOWN didn't change
    // but the status of the captive portal may have changed.
    RecheckCaptivePortal();
    return NS_OK;
  }
  if (!strcmp(data, NS_NETWORK_LINK_DATA_DOWN)) {
    isUp = false;
  } else if (!strcmp(data, NS_NETWORK_LINK_DATA_UP)) {
    isUp = true;
  } else if (!strcmp(data, NS_NETWORK_LINK_DATA_UNKNOWN)) {
    nsresult rv = mNetworkLinkService->GetIsLinkUp(&isUp);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    NS_WARNING("Unhandled network event!");
    return NS_OK;
  }

  return SetConnectivityInternal(isUp);
}

NS_IMETHODIMP
nsIOService::EscapeString(const nsACString& aString, uint32_t aEscapeType,
                          nsACString& aResult) {
  NS_ENSURE_ARG_MAX(aEscapeType, 4);

  nsAutoCString stringCopy(aString);
  nsCString result;

  if (!NS_Escape(stringCopy, result, (nsEscapeMask)aEscapeType))
    return NS_ERROR_OUT_OF_MEMORY;

  aResult.Assign(result);

  return NS_OK;
}

NS_IMETHODIMP
nsIOService::EscapeURL(const nsACString& aStr, uint32_t aFlags,
                       nsACString& aResult) {
  aResult.Truncate();
  NS_EscapeURL(aStr.BeginReading(), aStr.Length(), aFlags | esc_AlwaysCopy,
               aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::UnescapeString(const nsACString& aStr, uint32_t aFlags,
                            nsACString& aResult) {
  aResult.Truncate();
  NS_UnescapeURL(aStr.BeginReading(), aStr.Length(), aFlags | esc_AlwaysCopy,
                 aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsIOService::ExtractCharsetFromContentType(const nsACString& aTypeHeader,
                                           nsACString& aCharset,
                                           int32_t* aCharsetStart,
                                           int32_t* aCharsetEnd,
                                           bool* aHadCharset) {
  nsAutoCString ignored;
  net_ParseContentType(aTypeHeader, ignored, aCharset, aHadCharset,
                       aCharsetStart, aCharsetEnd);
  if (*aHadCharset && *aCharsetStart == *aCharsetEnd) {
    *aHadCharset = false;
  }
  return NS_OK;
}

// nsISpeculativeConnect
class IOServiceProxyCallback final : public nsIProtocolProxyCallback {
  ~IOServiceProxyCallback() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLPROXYCALLBACK

  IOServiceProxyCallback(nsIInterfaceRequestor* aCallbacks,
                         nsIOService* aIOService)
      : mCallbacks(aCallbacks), mIOService(aIOService) {}

 private:
  RefPtr<nsIInterfaceRequestor> mCallbacks;
  RefPtr<nsIOService> mIOService;
};

NS_IMPL_ISUPPORTS(IOServiceProxyCallback, nsIProtocolProxyCallback)

NS_IMETHODIMP
IOServiceProxyCallback::OnProxyAvailable(nsICancelable* request,
                                         nsIChannel* channel, nsIProxyInfo* pi,
                                         nsresult status) {
  // Checking proxy status for speculative connect
  nsAutoCString type;
  if (NS_SUCCEEDED(status) && pi && NS_SUCCEEDED(pi->GetType(type)) &&
      !type.EqualsLiteral("direct")) {
    // proxies dont do speculative connect
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = channel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  if (NS_FAILED(rv)) return NS_OK;

  nsCOMPtr<nsIProtocolHandler> handler;
  rv = mIOService->GetProtocolHandler(scheme.get(), getter_AddRefs(handler));
  if (NS_FAILED(rv)) return NS_OK;

  nsCOMPtr<nsISpeculativeConnect> speculativeHandler =
      do_QueryInterface(handler);
  if (!speculativeHandler) return NS_OK;

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  nsCOMPtr<nsIPrincipal> principal = loadInfo->GetLoadingPrincipal();

  nsLoadFlags loadFlags = 0;
  channel->GetLoadFlags(&loadFlags);
  if (loadFlags & nsIRequest::LOAD_ANONYMOUS) {
    speculativeHandler->SpeculativeAnonymousConnect(uri, principal, mCallbacks);
  } else {
    speculativeHandler->SpeculativeConnect(uri, principal, mCallbacks);
  }

  return NS_OK;
}

nsresult nsIOService::SpeculativeConnectInternal(
    nsIURI* aURI, nsIPrincipal* aPrincipal, nsIInterfaceRequestor* aCallbacks,
    bool aAnonymous) {
  NS_ENSURE_ARG(aURI);

  if (!aURI->SchemeIs("http") && !aURI->SchemeIs("https")) {
    // We don't speculatively connect to non-HTTP[S] URIs.
    return NS_OK;
  }

  if (IsNeckoChild()) {
    gNeckoChild->SendSpeculativeConnect(aURI, aPrincipal, aAnonymous);
    return NS_OK;
  }

  // Check for proxy information. If there is a proxy configured then a
  // speculative connect should not be performed because the potential
  // reward is slim with tcp peers closely located to the browser.
  nsresult rv;
  nsCOMPtr<nsIProtocolProxyService> pps =
      do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> loadingPrincipal = aPrincipal;

  MOZ_ASSERT(aPrincipal, "We expect passing a principal here.");

  if (!aPrincipal) {
    return NS_ERROR_INVALID_ARG;
  }

  // dummy channel used to create a TCP connection.
  // we perform security checks on the *real* channel, responsible
  // for any network loads. this real channel just checks the TCP
  // pool if there is an available connection created by the
  // channel we create underneath - hence it's safe to use
  // the systemPrincipal as the loadingPrincipal for this channel.
  nsCOMPtr<nsIChannel> channel;
  rv = NewChannelFromURI(aURI,
                         nullptr,  // aLoadingNode,
                         loadingPrincipal,
                         nullptr,  // aTriggeringPrincipal,
                         nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                         nsIContentPolicy::TYPE_SPECULATIVE,
                         getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aAnonymous) {
    nsLoadFlags loadFlags = 0;
    channel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIRequest::LOAD_ANONYMOUS;
    channel->SetLoadFlags(loadFlags);
  }

  nsCOMPtr<nsICancelable> cancelable;
  RefPtr<IOServiceProxyCallback> callback =
      new IOServiceProxyCallback(aCallbacks, this);
  nsCOMPtr<nsIProtocolProxyService2> pps2 = do_QueryInterface(pps);
  if (pps2) {
    return pps2->AsyncResolve2(channel, 0, callback, nullptr,
                               getter_AddRefs(cancelable));
  }
  return pps->AsyncResolve(channel, 0, callback, nullptr,
                           getter_AddRefs(cancelable));
}

NS_IMETHODIMP
nsIOService::SpeculativeConnect(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                nsIInterfaceRequestor* aCallbacks) {
  return SpeculativeConnectInternal(aURI, aPrincipal, aCallbacks, false);
}

NS_IMETHODIMP
nsIOService::SpeculativeAnonymousConnect(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                         nsIInterfaceRequestor* aCallbacks) {
  return SpeculativeConnectInternal(aURI, aPrincipal, aCallbacks, true);
}

/*static*/
bool nsIOService::BlockToplevelDataUriNavigations() {
  return sBlockToplevelDataUriNavigations;
}

NS_IMETHODIMP
nsIOService::NotImplemented() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsIOService::GetSocketProcessLaunched(bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = SocketProcessReady();
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
