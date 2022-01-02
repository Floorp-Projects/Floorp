/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "prsystem.h"

#include "AltServiceChild.h"
#include "nsCORSListenerProxy.h"
#include "nsError.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpChannel.h"
#include "nsHTTPCompressConv.h"
#include "nsHttpAuthCache.h"
#include "nsStandardURL.h"
#include "LoadContextInfo.h"
#include "nsCategoryManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsSocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsPrintfCString.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Printf.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsSocketTransportService2.h"
#include "nsAlgorithm.h"
#include "ASpdySession.h"
#include "EventTokenBucket.h"
#include "Tickler.h"
#include "nsIXULAppInfo.h"
#include "nsICookieService.h"
#include "nsIObserverService.h"
#include "nsISiteSecurityService.h"
#include "nsIStreamConverterService.h"
#include "nsCRT.h"
#include "nsIParentalControlsService.h"
#include "nsPIDOMWindow.h"
#include "nsHttpChannelAuthProvider.h"
#include "nsINetworkLinkService.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsSocketTransportService2.h"
#include "nsIOService.h"
#include "nsISupportsPrimitives.h"
#include "nsIXULRuntime.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsRFPService.h"
#include "mozilla/net/rust_helper.h"

#include "mozilla/net/HttpConnectionMgrParent.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/RequestContextService.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/AntiTrackingRedirectHeuristic.h"
#include "mozilla/DynamicFpiRedirectHeuristic.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/StaticPrefs_image.h"
#include "mozilla/SyncRunnable.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/network/Connection.h"

#include "nsNSSComponent.h"
#include "TRRServiceChannel.h"

#include <bitset>

#if defined(XP_UNIX)
#  include <sys/utsname.h>
#endif

#if defined(XP_WIN)
#  include <windows.h>
#  include "mozilla/WindowsVersion.h"
#endif

#if defined(XP_MACOSX)
#  include <CoreServices/CoreServices.h>
#  include "nsCocoaFeatures.h"
#endif

#include "mozilla/browser/NimbusFeatures.h"

//-----------------------------------------------------------------------------
#include "mozilla/net/HttpChannelChild.h"

#define UA_PREF_PREFIX "general.useragent."
#ifdef XP_WIN
#  define UA_SPARE_PLATFORM
#endif

#define HTTP_PREF_PREFIX "network.http."
#define INTL_ACCEPT_LANGUAGES "intl.accept_languages"
#define BROWSER_PREF_PREFIX "browser.cache."
#define H2MANDATORY_SUITE "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256"
#define SAFE_HINT_HEADER_VALUE "safeHint.enabled"
#define SECURITY_PREFIX "security."
#define DOM_SECURITY_PREFIX "dom.security"

#define ACCEPT_HEADER_STYLE "text/css,*/*;q=0.1"
#define ACCEPT_HEADER_ALL "*/*"

#define UA_PREF(_pref) UA_PREF_PREFIX _pref
#define HTTP_PREF(_pref) HTTP_PREF_PREFIX _pref
#define BROWSER_PREF(_pref) BROWSER_PREF_PREFIX _pref

#define NS_HTTP_PROTOCOL_FLAGS \
  (URI_STD | ALLOWS_PROXY | ALLOWS_PROXY_HTTP | URI_LOADABLE_BY_ANYONE)

#define UA_EXPERIMENT_NAME "firefox100"_ns
#define UA_EXPERIMENT_VAR "firefoxVersion"_ns

//-----------------------------------------------------------------------------

using mozilla::dom::Promise;

namespace mozilla::net {

LazyLogModule gHttpLog("nsHttp");

static void ExperimentUserAgentUpdated(const char* /* aNimbusPref */,
                                       void* aUserData) {
  MOZ_ASSERT(aUserData != nullptr);
  nsACString* aExperimentUserAgent = static_cast<nsACString*>(aUserData);

  // Is this user enrolled in the Firefox 100 experiment?
  int firefoxVersion =
      NimbusFeatures::GetInt(UA_EXPERIMENT_NAME, UA_EXPERIMENT_VAR, 0);
  if (firefoxVersion <= 0) {
    aExperimentUserAgent->SetIsVoid(true);
    return;
  }

  const char uaFormat[] =
#ifdef XP_WIN
#  ifdef HAVE_64BIT_BUILD
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:%d.0) Gecko/20100101 "
      "Firefox/%d.0"
#  else
      "Mozilla/5.0 (Windows NT 10.0; rv:%d.0) Gecko/20100101 Firefox/%d.0"
#  endif
#elif defined(XP_MACOSX)
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:%d.0) Gecko/20100101 "
      "Firefox/%d.0"
#else
      // Linux, Android, FreeBSD, etc
      "Mozilla/5.0 (X11; Linux x86_64; rv:%d.0) Gecko/20100101 Firefox/%d.0"
#endif
      ;

  aExperimentUserAgent->Truncate();
  aExperimentUserAgent->AppendPrintf(uaFormat, firefoxVersion, firefoxVersion);
}

#ifdef ANDROID
static nsCString GetDeviceModelId() {
  // Assumed to be running on the main thread
  // We need the device property in either case
  nsAutoCString deviceModelId;
  nsCOMPtr<nsIPropertyBag2> infoService =
      do_GetService("@mozilla.org/system-info;1");
  MOZ_ASSERT(infoService, "Could not find a system info service");
  nsAutoString androidDevice;
  nsresult rv = infoService->GetPropertyAsAString(u"device"_ns, androidDevice);
  if (NS_SUCCEEDED(rv)) {
    deviceModelId = NS_LossyConvertUTF16toASCII(androidDevice);
  }
  nsAutoCString deviceString;
  rv = Preferences::GetCString(UA_PREF("device_string"), deviceString);
  if (NS_SUCCEEDED(rv)) {
    deviceString.Trim(" ", true, true);
    deviceString.ReplaceSubstring("%DEVICEID%"_ns, deviceModelId);
    return std::move(deviceString);
  }
  return std::move(deviceModelId);
}
#endif

//-----------------------------------------------------------------------------
// nsHttpHandler <public>
//-----------------------------------------------------------------------------

StaticRefPtr<nsHttpHandler> gHttpHandler;

/* static */
already_AddRefed<nsHttpHandler> nsHttpHandler::GetInstance() {
  if (!gHttpHandler) {
    gHttpHandler = new nsHttpHandler();
    DebugOnly<nsresult> rv = gHttpHandler->Init();
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    // There is code that may be executed during the final cycle collection
    // shutdown and still referencing gHttpHandler.
    ClearOnShutdown(&gHttpHandler, ShutdownPhase::CCPostLastCycleCollection);
  }
  RefPtr<nsHttpHandler> httpHandler = gHttpHandler;
  return httpHandler.forget();
}

/// Derive the HTTP Accept header for image requests based on the enabled prefs
/// for non-universal image types. This may be overridden in its entirety by
/// the image.http.accept pref.
static nsCString ImageAcceptHeader() {
  nsCString mimeTypes;

  if (mozilla::StaticPrefs::image_avif_enabled()) {
    mimeTypes.Append("image/avif,");
  }

  if (mozilla::StaticPrefs::image_jxl_enabled()) {
    mimeTypes.Append("image/jxl,");
  }

  if (mozilla::StaticPrefs::image_webp_enabled()) {
    mimeTypes.Append("image/webp,");
  }

  mimeTypes.Append("*/*");

  return mimeTypes;
}

static nsCString DocumentAcceptHeader(const nsCString& aImageAcceptHeader) {
  nsPrintfCString mimeTypes(
      "text/html,application/xhtml+xml,application/xml;q=0.9,%s;q=0.8",
      aImageAcceptHeader.get());

  return std::move(mimeTypes);
}

nsHttpHandler::nsHttpHandler()
    : mIdleTimeout(PR_SecondsToInterval(10)),
      mSpdyTimeout(PR_SecondsToInterval(180)),
      mResponseTimeout(PR_SecondsToInterval(300)),
      mImageAcceptHeader(ImageAcceptHeader()),
      mDocumentAcceptHeader(DocumentAcceptHeader(ImageAcceptHeader())),
      mLastUniqueID(NowInSeconds()),
      mDebugObservations(false),
      mEnableSpdy(false),
      mHttp2Enabled(true),
      mUseH2Deps(true),
      mEnforceHttp2TlsProfile(true),
      mCoalesceSpdy(true),
      mSpdyPersistentSettings(false),
      mAllowPush(true),
      mEnableAltSvc(false),
      mEnableAltSvcOE(false),
      mEnableOriginExtension(false),
      mEnableH2Websockets(true),
      mDumpHpackTables(false),
      mSpdyPingThreshold(PR_SecondsToInterval(58)),
      mSpdyPingTimeout(PR_SecondsToInterval(8)) {
  LOG(("Creating nsHttpHandler [this=%p].\n", this));

  mUserAgentOverride.SetIsVoid(true);

  mExperimentUserAgent.SetIsVoid(true);

  MOZ_ASSERT(!gHttpHandler, "HTTP handler already created!");

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (runtime) {
    runtime->GetProcessID(&mProcessId);
  }
}

nsHttpHandler::~nsHttpHandler() {
  LOG(("Deleting nsHttpHandler [this=%p]\n", this));

  // make sure the connection manager is shutdown
  if (mConnMgr) {
    nsresult rv = mConnMgr->Shutdown();
    if (NS_FAILED(rv)) {
      LOG(
          ("nsHttpHandler [this=%p] "
           "failed to shutdown connection manager (%08x)\n",
           this, static_cast<uint32_t>(rv)));
    }
    mConnMgr = nullptr;
  }

  // Note: don't call NeckoChild::DestroyNeckoChild() here, as it's too late
  // and it'll segfault.  NeckoChild will get cleaned up by process exit.

  nsHttp::DestroyAtomTable();
}

static const char* gCallbackPrefs[] = {
    HTTP_PREF_PREFIX,
    UA_PREF_PREFIX,
    INTL_ACCEPT_LANGUAGES,
    BROWSER_PREF("disk_cache_ssl"),
    H2MANDATORY_SUITE,
    HTTP_PREF("tcp_keepalive.short_lived_connections"),
    HTTP_PREF("tcp_keepalive.long_lived_connections"),
    SAFE_HINT_HEADER_VALUE,
    SECURITY_PREFIX,
    DOM_SECURITY_PREFIX,
    "image.http.accept",
    "image.avif.enabled",
    "image.jxl.enabled",
    "image.webp.enabled",
    nullptr,
};

nsresult nsHttpHandler::Init() {
  nsresult rv;

  LOG(("nsHttpHandler::Init\n"));
  MOZ_ASSERT(NS_IsMainThread());

  rv = nsHttp::CreateAtomTable();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIIOService> service = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("unable to continue without io service");
    return rv;
  }
  mIOService = new nsMainThreadPtrHolder<nsIIOService>(
      "nsHttpHandler::mIOService", service);

  gIOService->LaunchSocketProcess();

  if (IsNeckoChild()) NeckoChild::InitNeckoChild();

  InitUserAgentComponents();

  // This perference is only used in parent process.
  if (!IsNeckoChild()) {
    mActiveTabPriority =
        Preferences::GetBool(HTTP_PREF("active_tab_priority"), true);
    std::bitset<3> usageOfHTTPSRRPrefs;
    usageOfHTTPSRRPrefs[0] = StaticPrefs::network_dns_upgrade_with_https_rr();
    usageOfHTTPSRRPrefs[1] = StaticPrefs::network_dns_use_https_rr_as_altsvc();
    usageOfHTTPSRRPrefs[2] = StaticPrefs::network_dns_echconfig_enabled();
    Telemetry::ScalarSet(Telemetry::ScalarID::NETWORKING_HTTPS_RR_PREFS_USAGE,
                         static_cast<uint32_t>(usageOfHTTPSRRPrefs.to_ulong()));
  }

  auto initQLogDir = [&]() {
    nsCOMPtr<nsIFile> qlogDir;
    nsresult rv =
        NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(qlogDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EmptyCString();
    }

    nsAutoCString dirName("qlog_");
    dirName.AppendInt(mProcessId);
    rv = qlogDir->AppendNative(dirName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EmptyCString();
    }

    return qlogDir->HumanReadablePath();
  };
  mHttp3QlogDir = initQLogDir();

  // monitor some preference changes
  Preferences::RegisterPrefixCallbacks(nsHttpHandler::PrefsChanged,
                                       gCallbackPrefs, this);
  PrefsChanged(nullptr);

  // monitor Firefox Version Experiment enrollment
  NimbusFeatures::OnUpdate(UA_EXPERIMENT_NAME, UA_EXPERIMENT_VAR,
                           ExperimentUserAgentUpdated, &mExperimentUserAgent);

  // Load the experiment state once for startup
  ExperimentUserAgentUpdated("", &mExperimentUserAgent);

  Telemetry::ScalarSet(Telemetry::ScalarID::NETWORKING_HTTP3_ENABLED,
                       mHttp3Enabled);

  mMisc.AssignLiteral("rv:" MOZILLA_UAVERSION);

  mCompatFirefox.AssignLiteral("Firefox/" MOZILLA_UAVERSION);

  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");

  mAppName.AssignLiteral(MOZ_APP_UA_NAME);
  if (mAppName.Length() == 0 && appInfo) {
    // Try to get the UA name from appInfo, falling back to the name
    appInfo->GetUAName(mAppName);
    if (mAppName.Length() == 0) {
      appInfo->GetName(mAppName);
    }
    appInfo->GetVersion(mAppVersion);
    mAppName.StripChars(R"( ()<>@,;:\"/[]?={})");
  } else {
    mAppVersion.AssignLiteral(MOZ_APP_UA_VERSION);
  }

  // Generate the spoofed User Agent for fingerprinting resistance.
  nsRFPService::GetSpoofedUserAgent(mSpoofedUserAgent, true);

  mSessionStartTime = NowInSeconds();
  mHandlerActive = true;

  rv = InitConnectionMgr();
  if (NS_FAILED(rv)) return rv;

  mAltSvcCache = MakeUnique<AltSvcCache>();

  mRequestContextService = RequestContextService::GetOrCreate();

#if defined(ANDROID)
  mProductSub.AssignLiteral(MOZILLA_UAVERSION);
#else
  mProductSub.AssignLiteral(LEGACY_UA_GECKO_TRAIL);
#endif

#if DEBUG
  // dump user agent prefs
  LOG(("> legacy-app-name = %s\n", mLegacyAppName.get()));
  LOG(("> legacy-app-version = %s\n", mLegacyAppVersion.get()));
  LOG(("> platform = %s\n", mPlatform.get()));
  LOG(("> oscpu = %s\n", mOscpu.get()));
  LOG(("> misc = %s\n", mMisc.get()));
  LOG(("> product = %s\n", mProduct.get()));
  LOG(("> product-sub = %s\n", mProductSub.get()));
  LOG(("> app-name = %s\n", mAppName.get()));
  LOG(("> app-version = %s\n", mAppVersion.get()));
  LOG(("> compat-firefox = %s\n", mCompatFirefox.get()));
  LOG(("> user-agent = %s\n", UserAgent().get()));
#endif

  // Startup the http category
  // Bring alive the objects in the http-protocol-startup category
  NS_CreateServicesFromCategory(
      NS_HTTP_STARTUP_CATEGORY,
      static_cast<nsISupports*>(static_cast<void*>(this)),
      NS_HTTP_STARTUP_TOPIC);

  nsCOMPtr<nsIObserverService> obsService =
      static_cast<nsIObserverService*>(gIOService);
  if (obsService) {
    // register the handler object as a weak callback as we don't need to worry
    // about shutdown ordering.
    obsService->AddObserver(this, "profile-change-net-teardown", true);
    obsService->AddObserver(this, "profile-change-net-restore", true);
    obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    obsService->AddObserver(this, "net:clear-active-logins", true);
    obsService->AddObserver(this, "net:prune-dead-connections", true);
    // Sent by the TorButton add-on in the Tor Browser
    obsService->AddObserver(this, "net:prune-all-connections", true);
    obsService->AddObserver(this, "net:cancel-all-connections", true);
    obsService->AddObserver(this, "last-pb-context-exited", true);
    obsService->AddObserver(this, "browser:purge-session-history", true);
    obsService->AddObserver(this, NS_NETWORK_LINK_TOPIC, true);
    obsService->AddObserver(this, "application-background", true);
    obsService->AddObserver(this, "psm:user-certificate-added", true);
    obsService->AddObserver(this, "psm:user-certificate-deleted", true);
    obsService->AddObserver(this, "intl:app-locales-changed", true);
    obsService->AddObserver(this, "browser-delayed-startup-finished", true);
    obsService->AddObserver(this, "network:captive-portal-connectivity", true);
    obsService->AddObserver(this, "network:reset-http3-excluded-list", true);

    if (!IsNeckoChild()) {
      obsService->AddObserver(this, "net:current-top-browsing-context-id",
                              true);
    }

    // disabled as its a nop right now
    // obsService->AddObserver(this, "net:failed-to-process-uri-content", true);
  }

  MakeNewRequestTokenBucket();
  mWifiTickler = new Tickler();
  if (NS_FAILED(mWifiTickler->Init())) mWifiTickler = nullptr;

  nsCOMPtr<nsIParentalControlsService> pc =
      do_CreateInstance("@mozilla.org/parental-controls-service;1");
  if (pc) {
    pc->GetParentalControlsEnabled(&mParentalControlEnabled);
  }

  return NS_OK;
}

const nsCString& nsHttpHandler::Http3QlogDir() {
  if (StaticPrefs::network_http_http3_enable_qlog()) {
    return mHttp3QlogDir;
  }

  return EmptyCString();
}

void nsHttpHandler::MakeNewRequestTokenBucket() {
  LOG(("nsHttpHandler::MakeNewRequestTokenBucket this=%p child=%d\n", this,
       IsNeckoChild()));
  if (!mConnMgr || IsNeckoChild()) {
    return;
  }
  RefPtr<EventTokenBucket> tokenBucket =
      new EventTokenBucket(RequestTokenBucketHz(), RequestTokenBucketBurst());
  // NOTE The thread or socket may be gone already.
  nsresult rv = mConnMgr->UpdateRequestTokenBucket(tokenBucket);
  if (NS_FAILED(rv)) {
    LOG(("    failed to update request token bucket\n"));
  }
}

nsresult nsHttpHandler::InitConnectionMgr() {
  // Init ConnectionManager only on parent!
  if (IsNeckoChild()) {
    return NS_OK;
  }

  if (mConnMgr) {
    return NS_OK;
  }

  if (nsIOService::UseSocketProcess(true) && XRE_IsParentProcess()) {
    mConnMgr = new HttpConnectionMgrParent();
    RefPtr<nsHttpHandler> self = this;
    auto task = [self]() {
      HttpConnectionMgrParent* parent =
          self->mConnMgr->AsHttpConnectionMgrParent();
      Unused << SocketProcessParent::GetSingleton()
                    ->SendPHttpConnectionMgrConstructor(
                        parent,
                        HttpHandlerInitArgs(
                            self->mLegacyAppName, self->mLegacyAppVersion,
                            self->mPlatform, self->mOscpu, self->mMisc,
                            self->mProduct, self->mProductSub, self->mAppName,
                            self->mAppVersion, self->mCompatFirefox,
                            self->mCompatDevice, self->mDeviceModelId));
    };
    gIOService->CallOrWaitForSocketProcess(std::move(task));
  } else {
    MOZ_ASSERT(XRE_IsSocketProcess() || !nsIOService::UseSocketProcess());
    mConnMgr = new nsHttpConnectionMgr();
  }

  return mConnMgr->Init(
      mMaxUrgentExcessiveConns, mMaxConnections,
      mMaxPersistentConnectionsPerServer, mMaxPersistentConnectionsPerProxy,
      mMaxRequestDelay, mThrottleEnabled, mThrottleVersion, mThrottleSuspendFor,
      mThrottleResumeFor, mThrottleReadLimit, mThrottleReadInterval,
      mThrottleHoldTime, mThrottleMaxTime, mBeConservativeForProxy);
}

nsresult nsHttpHandler::AddStandardRequestHeaders(
    nsHttpRequestHead* request, bool isSecure,
    ExtContentPolicyType aContentPolicyType) {
  nsresult rv;

  // Add the "User-Agent" header
  rv = request->SetHeader(nsHttp::User_Agent, UserAgent(), false,
                          nsHttpHeaderArray::eVarietyRequestDefault);
  if (NS_FAILED(rv)) return rv;

  // MIME based content negotiation lives!
  // Add the "Accept" header.  Note, this is set as an override because the
  // service worker expects to see it.  The other "default" headers are
  // hidden from service worker interception.
  nsAutoCString accept;
  if (aContentPolicyType == ExtContentPolicy::TYPE_DOCUMENT ||
      aContentPolicyType == ExtContentPolicy::TYPE_SUBDOCUMENT) {
    accept.Assign(mDocumentAcceptHeader);
  } else if (aContentPolicyType == ExtContentPolicy::TYPE_IMAGE ||
             aContentPolicyType == ExtContentPolicy::TYPE_IMAGESET) {
    accept.Assign(mImageAcceptHeader);
  } else if (aContentPolicyType == ExtContentPolicy::TYPE_STYLESHEET) {
    accept.Assign(ACCEPT_HEADER_STYLE);
  } else {
    accept.Assign(ACCEPT_HEADER_ALL);
  }

  rv = request->SetHeader(nsHttp::Accept, accept, false,
                          nsHttpHeaderArray::eVarietyRequestOverride);
  if (NS_FAILED(rv)) return rv;

  // Add the "Accept-Language" header.  This header is also exposed to the
  // service worker.
  if (mAcceptLanguagesIsDirty) {
    rv = SetAcceptLanguages();
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  // Add the "Accept-Language" header
  if (!mAcceptLanguages.IsEmpty()) {
    rv = request->SetHeader(nsHttp::Accept_Language, mAcceptLanguages, false,
                            nsHttpHeaderArray::eVarietyRequestOverride);
    if (NS_FAILED(rv)) return rv;
  }

  // Add the "Accept-Encoding" header
  if (isSecure) {
    rv = request->SetHeader(nsHttp::Accept_Encoding, mHttpsAcceptEncodings,
                            false, nsHttpHeaderArray::eVarietyRequestDefault);
  } else {
    rv = request->SetHeader(nsHttp::Accept_Encoding, mHttpAcceptEncodings,
                            false, nsHttpHeaderArray::eVarietyRequestDefault);
  }
  if (NS_FAILED(rv)) return rv;

  // add the "Send Hint" header
  if (mSafeHintEnabled || mParentalControlEnabled) {
    rv = request->SetHeader(nsHttp::Prefer, "safe"_ns, false,
                            nsHttpHeaderArray::eVarietyRequestDefault);
    if (NS_FAILED(rv)) return rv;
  }
  return NS_OK;
}

nsresult nsHttpHandler::AddConnectionHeader(nsHttpRequestHead* request,
                                            uint32_t caps) {
  // RFC2616 section 19.6.2 states that the "Connection: keep-alive"
  // and "Keep-alive" request headers should not be sent by HTTP/1.1
  // user-agents.  But this is not a problem in practice, and the
  // alternative proxy-connection is worse. see 570283

  constexpr auto close = "close"_ns;
  constexpr auto keepAlive = "keep-alive"_ns;

  const nsLiteralCString* connectionType = &close;
  if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
    connectionType = &keepAlive;
  }

  return request->SetHeader(nsHttp::Connection, *connectionType);
}

bool nsHttpHandler::IsAcceptableEncoding(const char* enc, bool isSecure) {
  if (!enc) return false;

  // we used to accept x-foo anytime foo was acceptable, but that's just
  // continuing bad behavior.. so limit it to known x-* patterns
  bool rv;
  if (isSecure) {
    rv = nsHttp::FindToken(mHttpsAcceptEncodings.get(), enc, HTTP_LWS ",") !=
         nullptr;
  } else {
    rv = nsHttp::FindToken(mHttpAcceptEncodings.get(), enc, HTTP_LWS ",") !=
         nullptr;
  }
  // gzip and deflate are inherently acceptable in modern HTTP - always
  // process them if a stream converter can also be found.
  if (!rv &&
      (!nsCRT::strcasecmp(enc, "gzip") || !nsCRT::strcasecmp(enc, "deflate") ||
       !nsCRT::strcasecmp(enc, "x-gzip") ||
       !nsCRT::strcasecmp(enc, "x-deflate"))) {
    rv = true;
  }
  LOG(("nsHttpHandler::IsAceptableEncoding %s https=%d %d\n", enc, isSecure,
       rv));
  return rv;
}

nsISiteSecurityService* nsHttpHandler::GetSSService() {
  if (!mSSService) {
    nsCOMPtr<nsISiteSecurityService> service =
        do_GetService(NS_SSSERVICE_CONTRACTID);
    mSSService = new nsMainThreadPtrHolder<nsISiteSecurityService>(
        "nsHttpHandler::mSSService", service);
  }
  return mSSService;
}

nsICookieService* nsHttpHandler::GetCookieService() {
  if (!mCookieService) {
    nsCOMPtr<nsICookieService> service =
        do_GetService(NS_COOKIESERVICE_CONTRACTID);
    mCookieService = new nsMainThreadPtrHolder<nsICookieService>(
        "nsHttpHandler::mCookieService", service);
  }
  return mCookieService;
}

nsresult nsHttpHandler::GetIOService(nsIIOService** result) {
  NS_ENSURE_ARG_POINTER(result);

  *result = do_AddRef(mIOService.get()).take();
  return NS_OK;
}

void nsHttpHandler::NotifyObservers(nsIChannel* chan, const char* event) {
  LOG(("nsHttpHandler::NotifyObservers [chan=%p event=\"%s\"]\n", chan, event));
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) obsService->NotifyObservers(chan, event, nullptr);
}

nsresult nsHttpHandler::AsyncOnChannelRedirect(
    nsIChannel* oldChan, nsIChannel* newChan, uint32_t flags,
    nsIEventTarget* mainThreadEventTarget) {
  MOZ_ASSERT(NS_IsMainThread() && (oldChan && newChan));

  nsCOMPtr<nsIURI> oldURI;
  oldChan->GetURI(getter_AddRefs(oldURI));
  MOZ_ASSERT(oldURI);

  nsCOMPtr<nsIURI> newURI;
  newChan->GetURI(getter_AddRefs(newURI));
  MOZ_ASSERT(newURI);

  PrepareForAntiTrackingRedirectHeuristic(oldChan, oldURI, newChan, newURI);

  DynamicFpiRedirectHeuristic(oldChan, oldURI, newChan, newURI);

  // TODO E10S This helper has to be initialized on the other process
  RefPtr<nsAsyncRedirectVerifyHelper> redirectCallbackHelper =
      new nsAsyncRedirectVerifyHelper();

  return redirectCallbackHelper->Init(oldChan, newChan, flags,
                                      mainThreadEventTarget);
}

/* static */
nsresult nsHttpHandler::GenerateHostPort(const nsCString& host, int32_t port,
                                         nsACString& hostLine) {
  return NS_GenerateHostPort(host, port, hostLine);
}

//-----------------------------------------------------------------------------
// nsHttpHandler <private>
//-----------------------------------------------------------------------------

const nsCString& nsHttpHandler::UserAgent() {
  if (nsContentUtils::ShouldResistFingerprinting() &&
      !mSpoofedUserAgent.IsEmpty()) {
    LOG(("using spoofed userAgent : %s\n", mSpoofedUserAgent.get()));
    return mSpoofedUserAgent;
  }

  if (!mUserAgentOverride.IsVoid()) {
    LOG(("using general.useragent.override : %s\n", mUserAgentOverride.get()));
    return mUserAgentOverride;
  }

  if (!mExperimentUserAgent.IsVoid()) {
    LOG(("using Firefox 100 Experiment User-Agent : %s\n",
         mExperimentUserAgent.get()));
    return mExperimentUserAgent;
  }

  if (mUserAgentIsDirty) {
    BuildUserAgent();
    mUserAgentIsDirty = false;
  }

  return mUserAgent;
}

void nsHttpHandler::BuildUserAgent() {
  LOG(("nsHttpHandler::BuildUserAgent\n"));

  MOZ_ASSERT(!mLegacyAppName.IsEmpty() && !mLegacyAppVersion.IsEmpty(),
             "HTTP cannot send practical requests without this much");

  // preallocate to worst-case size, which should always be better
  // than if we didn't preallocate at all.
  mUserAgent.SetCapacity(mLegacyAppName.Length() + mLegacyAppVersion.Length() +
#ifndef UA_SPARE_PLATFORM
                         mPlatform.Length() +
#endif
                         mOscpu.Length() + mMisc.Length() + mProduct.Length() +
                         mProductSub.Length() + mAppName.Length() +
                         mAppVersion.Length() + mCompatFirefox.Length() +
                         mCompatDevice.Length() + mDeviceModelId.Length() + 13);

  // Application portion
  mUserAgent.Assign(mLegacyAppName);
  mUserAgent += '/';
  mUserAgent += mLegacyAppVersion;
  mUserAgent += ' ';

  // Application comment
  mUserAgent += '(';
#ifndef UA_SPARE_PLATFORM
  if (!mPlatform.IsEmpty()) {
    mUserAgent += mPlatform;
    mUserAgent.AppendLiteral("; ");
  }
#endif
  if (!mCompatDevice.IsEmpty()) {
    mUserAgent += mCompatDevice;
    mUserAgent.AppendLiteral("; ");
  } else if (!mOscpu.IsEmpty()) {
    mUserAgent += mOscpu;
    mUserAgent.AppendLiteral("; ");
  }
  if (!mDeviceModelId.IsEmpty()) {
    mUserAgent += mDeviceModelId;
    mUserAgent.AppendLiteral("; ");
  }
  mUserAgent += mMisc;
  mUserAgent += ')';

  // Product portion
  mUserAgent += ' ';
  mUserAgent += mProduct;
  mUserAgent += '/';
  mUserAgent += mProductSub;

  bool isFirefox = mAppName.EqualsLiteral("Firefox");
  if (isFirefox || mCompatFirefoxEnabled) {
    // "Firefox/x.y" (compatibility) app token
    mUserAgent += ' ';
    mUserAgent += mCompatFirefox;
  }
  if (!isFirefox) {
    // App portion
    mUserAgent += ' ';
    mUserAgent += mAppName;
    mUserAgent += '/';
    mUserAgent += mAppVersion;
  }
}

#ifdef XP_WIN
#  define OSCPU_WINDOWS "Windows NT %ld.%ld"
#  define OSCPU_WIN64 OSCPU_WINDOWS "; Win64; x64"
#endif

void nsHttpHandler::InitUserAgentComponents() {
  // Don't build user agent components in socket process, since the system info
  // is not available.
  if (XRE_IsSocketProcess()) {
    mUserAgentIsDirty = true;
    return;
  }

  // Gather platform.
  mPlatform.AssignLiteral(
#if defined(ANDROID)
      "Android"
#elif defined(XP_WIN)
      "Windows"
#elif defined(XP_MACOSX)
      "Macintosh"
#elif defined(XP_UNIX)
      // We historically have always had X11 here,
      // and there seems little a webpage can sensibly do
      // based on it being something else, so use X11 for
      // backwards compatibility in all cases.
      "X11"
#endif
  );

#ifdef ANDROID
  nsCOMPtr<nsIPropertyBag2> infoService =
      do_GetService("@mozilla.org/system-info;1");
  MOZ_ASSERT(infoService, "Could not find a system info service");
  nsresult rv;
  // Add the Android version number to the Fennec platform identifier.
  nsAutoString androidVersion;
  rv = infoService->GetPropertyAsAString(u"release_version"_ns, androidVersion);
  if (NS_SUCCEEDED(rv)) {
    mPlatform += " ";
    // If the 2nd character is a ".", we know the major version is a single
    // digit. If we're running on a version below 4 we pretend to be on
    // Android KitKat (4.4) to work around scripts sniffing for low versions.
    if (androidVersion[1] == 46 && androidVersion[0] < 52) {
      mPlatform += "4.4";
    } else {
      mPlatform += NS_LossyConvertUTF16toASCII(androidVersion);
    }
  }

  // Add the `Mobile` or `Tablet` or `TV` token when running on device.
  bool isTablet;
  rv = infoService->GetPropertyAsBool(u"tablet"_ns, &isTablet);
  if (NS_SUCCEEDED(rv) && isTablet) {
    mCompatDevice.AssignLiteral("Tablet");
  } else {
    bool isTV;
    rv = infoService->GetPropertyAsBool(u"tv"_ns, &isTV);
    if (NS_SUCCEEDED(rv) && isTV) {
      mCompatDevice.AssignLiteral("TV");
    } else {
      mCompatDevice.AssignLiteral("Mobile");
    }
  }

  if (Preferences::GetBool(UA_PREF("use_device"), false)) {
    mDeviceModelId = mozilla::net::GetDeviceModelId();
  }
#endif  // ANDROID

  // Gather OS/CPU.
#if defined(XP_WIN)
  OSVERSIONINFO info = {sizeof(OSVERSIONINFO)};
  if (!GetVersionEx(&info) || info.dwMajorVersion >= 10) {
    // Cap the reported Windows version to 10.0. This way, Microsoft doesn't
    // get to change Web compat-sensitive values without our veto. The
    // compat-sensitivity keeps going up as 10.0 stays as the current value
    // for longer and longer. If the system-reported version ever changes,
    // we'll be able to take our time to evaluate the Web compat impact
    // instead of having to scramble to react like happened with macOS
    // changing from 10.x to 11.x.
    info.dwMajorVersion = 10;
    info.dwMinorVersion = 0;
  }

  const char* format;
#  if defined _M_X64 || defined _M_AMD64
  format = OSCPU_WIN64;
#  else
  BOOL isWow64 = FALSE;
  if (!IsWow64Process(GetCurrentProcess(), &isWow64)) {
    isWow64 = FALSE;
  }
  format = isWow64 ? OSCPU_WIN64 : OSCPU_WINDOWS;
#  endif

  SmprintfPointer buf =
      mozilla::Smprintf(format, info.dwMajorVersion, info.dwMinorVersion);
  if (buf) {
    mOscpu = buf.get();
  }
#elif defined(XP_MACOSX)
  SInt32 majorVersion = nsCocoaFeatures::macOSVersionMajor();
  SInt32 minorVersion = nsCocoaFeatures::macOSVersionMinor();

  // Cap the reported macOS version at 10.15 (like Safari) to avoid breaking
  // sites that assume the UA's macOS version always begins with "10.".
  int uaVersion = (majorVersion >= 11 || minorVersion > 15) ? 15 : minorVersion;

  // Always return an "Intel" UA string, even on ARM64 macOS like Safari does.
  mOscpu = nsPrintfCString("Intel Mac OS X 10.%d", uaVersion);
#elif defined(XP_UNIX)
  struct utsname name {};
  int ret = uname(&name);
  if (ret >= 0) {
    nsAutoCString buf;
    buf = (char*)name.sysname;
    buf += ' ';

#  ifdef AIX
    // AIX uname returns machine specific info in the uname.machine
    // field and does not return the cpu type like other platforms.
    // We use the AIX version and release numbers instead.
    buf += (char*)name.version;
    buf += '.';
    buf += (char*)name.release;
#  else
    buf += (char*)name.machine;
#  endif

    mOscpu.Assign(buf);
  }
#endif

  mUserAgentIsDirty = true;
}

uint32_t nsHttpHandler::MaxSocketCount() {
  PR_CallOnce(&nsSocketTransportService::gMaxCountInitOnce,
              nsSocketTransportService::DiscoverMaxCount);
  // Don't use the full max count because sockets can be held in
  // the persistent connection pool for a long time and that could
  // starve other users.

  uint32_t maxCount = nsSocketTransportService::gMaxCount;
  if (maxCount <= 8) {
    maxCount = 1;
  } else {
    maxCount -= 8;
  }

  return maxCount;
}

// static
void nsHttpHandler::PrefsChanged(const char* pref, void* self) {
  static_cast<nsHttpHandler*>(self)->PrefsChanged(pref);
}

void nsHttpHandler::PrefsChanged(const char* pref) {
  nsresult rv = NS_OK;
  int32_t val;

  LOG(("nsHttpHandler::PrefsChanged [pref=%s]\n", pref));

  if (pref) {
    gIOService->NotifySocketProcessPrefsChanged(pref);
  }

#define PREF_CHANGED(p) ((pref == nullptr) || !strcmp(pref, p))
#define MULTI_PREF_CHANGED(p) \
  ((pref == nullptr) || !strncmp(pref, p, sizeof(p) - 1))

  // If a security pref changed, lets clear our connection pool reuse
  if (MULTI_PREF_CHANGED(SECURITY_PREFIX)) {
    LOG(("nsHttpHandler::PrefsChanged Security Pref Changed %s\n", pref));
    if (mConnMgr) {
      rv = mConnMgr->DoShiftReloadConnectionCleanup();
      if (NS_FAILED(rv)) {
        LOG(
            ("nsHttpHandler::PrefsChanged "
             "DoShiftReloadConnectionCleanup failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
      rv = mConnMgr->PruneDeadConnections();
      if (NS_FAILED(rv)) {
        LOG(
            ("nsHttpHandler::PrefsChanged "
             "PruneDeadConnections failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
    }
  }

  //
  // UA components
  //

  bool cVar = false;

  if (PREF_CHANGED(UA_PREF("compatMode.firefox"))) {
    rv = Preferences::GetBool(UA_PREF("compatMode.firefox"), &cVar);
    mCompatFirefoxEnabled = (NS_SUCCEEDED(rv) && cVar);
    mUserAgentIsDirty = true;
  }

  // general.useragent.override
  if (PREF_CHANGED(UA_PREF("override"))) {
    Preferences::GetCString(UA_PREF("override"), mUserAgentOverride);
    mUserAgentIsDirty = true;
  }

#ifdef ANDROID
  // general.useragent.use_device
  if (PREF_CHANGED(UA_PREF("use_device"))) {
    if (Preferences::GetBool(UA_PREF("use_device"), false)) {
      if (!XRE_IsSocketProcess()) {
        mDeviceModelId = mozilla::net::GetDeviceModelId();
        if (gIOService->SocketProcessReady()) {
          Unused << SocketProcessParent::GetSingleton()
                        ->SendUpdateDeviceModelId(mDeviceModelId);
        }
      }
    } else {
      mDeviceModelId.Truncate();
    }
    mUserAgentIsDirty = true;
  }
#endif

  //
  // HTTP options
  //

  if (PREF_CHANGED(HTTP_PREF("keep-alive.timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("keep-alive.timeout"), &val);
    if (NS_SUCCEEDED(rv)) {
      mIdleTimeout = PR_SecondsToInterval(clamped(val, 1, 0xffff));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("request.max-attempts"))) {
    rv = Preferences::GetInt(HTTP_PREF("request.max-attempts"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxRequestAttempts = (uint16_t)clamped(val, 1, 0xffff);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("request.max-start-delay"))) {
    rv = Preferences::GetInt(HTTP_PREF("request.max-start-delay"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxRequestDelay = (uint16_t)clamped(val, 0, 0xffff);
      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_REQUEST_DELAY,
                                   mMaxRequestDelay);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged (request.max-start-delay)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("response.timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("response.timeout"), &val);
    if (NS_SUCCEEDED(rv)) {
      mResponseTimeout = PR_SecondsToInterval(clamped(val, 0, 0xffff));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("network-changed.timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("network-changed.timeout"), &val);
    if (NS_SUCCEEDED(rv)) mNetworkChangedTimeout = clamped(val, 1, 600) * 1000;
  }

  if (PREF_CHANGED(HTTP_PREF("max-connections"))) {
    rv = Preferences::GetInt(HTTP_PREF("max-connections"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxConnections =
          (uint16_t)clamped((uint32_t)val, (uint32_t)1, MaxSocketCount());

      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_CONNECTIONS,
                                   mMaxConnections);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged (max-connections)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(
          HTTP_PREF("max-urgent-start-excessive-connections-per-host"))) {
    rv = Preferences::GetInt(
        HTTP_PREF("max-urgent-start-excessive-connections-per-host"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxUrgentExcessiveConns = (uint8_t)clamped(val, 1, 0xff);
      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_URGENT_START_Q,
                                   mMaxUrgentExcessiveConns);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged "
               "(max-urgent-start-excessive-connections-per-host)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-server"))) {
    rv = Preferences::GetInt(HTTP_PREF("max-persistent-connections-per-server"),
                             &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxPersistentConnectionsPerServer = (uint8_t)clamped(val, 1, 0xff);
      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(
            nsHttpConnectionMgr::MAX_PERSISTENT_CONNECTIONS_PER_HOST,
            mMaxPersistentConnectionsPerServer);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged "
               "(max-persistent-connections-per-server)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-proxy"))) {
    rv = Preferences::GetInt(HTTP_PREF("max-persistent-connections-per-proxy"),
                             &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxPersistentConnectionsPerProxy = (uint8_t)clamped(val, 1, 0xff);
      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(
            nsHttpConnectionMgr::MAX_PERSISTENT_CONNECTIONS_PER_PROXY,
            mMaxPersistentConnectionsPerProxy);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged "
               "(max-persistent-connections-per-proxy)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("redirection-limit"))) {
    rv = Preferences::GetInt(HTTP_PREF("redirection-limit"), &val);
    if (NS_SUCCEEDED(rv)) mRedirectionLimit = (uint8_t)clamped(val, 0, 0xff);
  }

  if (PREF_CHANGED(HTTP_PREF("connection-retry-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("connection-retry-timeout"), &val);
    if (NS_SUCCEEDED(rv)) mIdleSynTimeout = (uint16_t)clamped(val, 0, 3000);
  }

  if (PREF_CHANGED(HTTP_PREF("fast-fallback-to-IPv4"))) {
    rv = Preferences::GetBool(HTTP_PREF("fast-fallback-to-IPv4"), &cVar);
    if (NS_SUCCEEDED(rv)) mFastFallbackToIPv4 = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("fallback-connection-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("fallback-connection-timeout"), &val);
    if (NS_SUCCEEDED(rv)) {
      mFallbackSynTimeout = (uint16_t)clamped(val, 0, 10 * 60);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("version"))) {
    nsAutoCString httpVersion;
    Preferences::GetCString(HTTP_PREF("version"), httpVersion);
    if (!httpVersion.IsVoid()) {
      if (httpVersion.EqualsLiteral("1.1")) {
        mHttpVersion = HttpVersion::v1_1;
      } else if (httpVersion.EqualsLiteral("0.9")) {
        mHttpVersion = HttpVersion::v0_9;
      } else {
        mHttpVersion = HttpVersion::v1_0;
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("proxy.version"))) {
    nsAutoCString httpVersion;
    Preferences::GetCString(HTTP_PREF("proxy.version"), httpVersion);
    if (!httpVersion.IsVoid()) {
      if (httpVersion.EqualsLiteral("1.1")) {
        mProxyHttpVersion = HttpVersion::v1_1;
      } else {
        mProxyHttpVersion = HttpVersion::v1_0;
      }
      // it does not make sense to issue a HTTP/0.9 request to a proxy server
    }
  }

  if (PREF_CHANGED(HTTP_PREF("proxy.respect-be-conservative"))) {
    rv =
        Preferences::GetBool(HTTP_PREF("proxy.respect-be-conservative"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mBeConservativeForProxy = cVar;
      if (mConnMgr) {
        Unused << mConnMgr->UpdateParam(
            nsHttpConnectionMgr::PROXY_BE_CONSERVATIVE,
            static_cast<int32_t>(mBeConservativeForProxy));
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("qos"))) {
    rv = Preferences::GetInt(HTTP_PREF("qos"), &val);
    if (NS_SUCCEEDED(rv)) mQoSBits = (uint8_t)clamped(val, 0, 0xff);
  }

  if (PREF_CHANGED(HTTP_PREF("accept-encoding"))) {
    nsAutoCString acceptEncodings;
    rv = Preferences::GetCString(HTTP_PREF("accept-encoding"), acceptEncodings);
    if (NS_SUCCEEDED(rv)) {
      rv = SetAcceptEncodings(acceptEncodings.get(), false);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("accept-encoding.secure"))) {
    nsAutoCString acceptEncodings;
    rv = Preferences::GetCString(HTTP_PREF("accept-encoding.secure"),
                                 acceptEncodings);
    if (NS_SUCCEEDED(rv)) {
      rv = SetAcceptEncodings(acceptEncodings.get(), true);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("default-socket-type"))) {
    nsAutoCString sval;
    rv = Preferences::GetCString(HTTP_PREF("default-socket-type"), sval);
    if (NS_SUCCEEDED(rv)) {
      if (sval.IsEmpty()) {
        mDefaultSocketType.SetIsVoid(true);
      } else {
        // verify that this socket type is actually valid
        nsCOMPtr<nsISocketProviderService> sps =
            nsSocketProviderService::GetOrCreate();
        if (sps) {
          nsCOMPtr<nsISocketProvider> sp;
          rv = sps->GetSocketProvider(sval.get(), getter_AddRefs(sp));
          if (NS_SUCCEEDED(rv)) {
            // OK, this looks like a valid socket provider.
            mDefaultSocketType.Assign(sval);
          }
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("prompt-temp-redirect"))) {
    rv = Preferences::GetBool(HTTP_PREF("prompt-temp-redirect"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mPromptTempRedirect = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("assoc-req.enforce"))) {
    cVar = false;
    rv = Preferences::GetBool(HTTP_PREF("assoc-req.enforce"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnforceAssocReq = cVar;
  }

  // enable Persistent caching for HTTPS - bug#205921
  if (PREF_CHANGED(BROWSER_PREF("disk_cache_ssl"))) {
    cVar = false;
    rv = Preferences::GetBool(BROWSER_PREF("disk_cache_ssl"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnablePersistentHttpsCaching = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("phishy-userpass-length"))) {
    rv = Preferences::GetInt(HTTP_PREF("phishy-userpass-length"), &val);
    if (NS_SUCCEEDED(rv)) {
      mPhishyUserPassLength = (uint8_t)clamped(val, 0, 0xff);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enabled"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enabled"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnableSpdy = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enabled.http2"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enabled.http2"), &cVar);
    if (NS_SUCCEEDED(rv)) mHttp2Enabled = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enabled.deps"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enabled.deps"), &cVar);
    if (NS_SUCCEEDED(rv)) mUseH2Deps = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enforce-tls-profile"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enforce-tls-profile"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnforceHttp2TlsProfile = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.coalesce-hostnames"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.coalesce-hostnames"), &cVar);
    if (NS_SUCCEEDED(rv)) mCoalesceSpdy = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.persistent-settings"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.persistent-settings"), &cVar);
    if (NS_SUCCEEDED(rv)) mSpdyPersistentSettings = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.timeout"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdyTimeout = PR_SecondsToInterval(clamped(val, 1, 0xffff));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.chunk-size"))) {
    // keep this within http/2 ranges of 1 to 2^24-1
    rv = Preferences::GetInt(HTTP_PREF("spdy.chunk-size"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdySendingChunkSize = (uint32_t)clamped(val, 1, 0xffffff);
    }
  }

  // The amount of idle seconds on a spdy connection before initiating a
  // server ping. 0 will disable.
  if (PREF_CHANGED(HTTP_PREF("spdy.ping-threshold"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.ping-threshold"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdyPingThreshold =
          PR_SecondsToInterval((uint16_t)clamped(val, 0, 0x7fffffff));
    }
  }

  // The amount of seconds to wait for a spdy ping response before
  // closing the session.
  if (PREF_CHANGED(HTTP_PREF("spdy.ping-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.ping-timeout"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdyPingTimeout =
          PR_SecondsToInterval((uint16_t)clamped(val, 0, 0x7fffffff));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.allow-push"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.allow-push"), &cVar);
    if (NS_SUCCEEDED(rv)) mAllowPush = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("altsvc.enabled"))) {
    rv = Preferences::GetBool(HTTP_PREF("altsvc.enabled"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnableAltSvc = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("altsvc.oe"))) {
    rv = Preferences::GetBool(HTTP_PREF("altsvc.oe"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnableAltSvcOE = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("originextension"))) {
    rv = Preferences::GetBool(HTTP_PREF("originextension"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnableOriginExtension = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.websockets"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.websockets"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mEnableH2Websockets = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.push-allowance"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.push-allowance"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdyPushAllowance = static_cast<uint32_t>(
          clamped(val, 1024, static_cast<int32_t>(ASpdySession::kInitialRwin)));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.pull-allowance"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.pull-allowance"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdyPullAllowance =
          static_cast<uint32_t>(clamped(val, 1024, 0x7fffffff));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.default-concurrent"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.default-concurrent"), &val);
    if (NS_SUCCEEDED(rv)) {
      mDefaultSpdyConcurrent = static_cast<uint32_t>(
          std::max<int32_t>(std::min<int32_t>(val, 9999), 1));
    }
  }

  // The amount of seconds to wait for a spdy ping response before
  // closing the session.
  if (PREF_CHANGED(HTTP_PREF("spdy.send-buffer-size"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.send-buffer-size"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdySendBufferSize = (uint32_t)clamped(val, 1500, 0x7fffffff);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enable-hpack-dump"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enable-hpack-dump"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mDumpHpackTables = cVar;
    }
  }

  // The maximum amount of time to wait for socket transport to be
  // established
  if (PREF_CHANGED(HTTP_PREF("connection-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("connection-timeout"), &val);
    if (NS_SUCCEEDED(rv)) {
      // the pref is in seconds, but the variable is in milliseconds
      mConnectTimeout = clamped(val, 1, 0xffff) * PR_MSEC_PER_SEC;
    }
  }

  // The maximum amount of time to wait for a tls handshake to finish.
  if (PREF_CHANGED(HTTP_PREF("tls-handshake-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("tls-handshake-timeout"), &val);
    if (NS_SUCCEEDED(rv)) {
      // the pref is in seconds, but the variable is in milliseconds
      mTLSHandshakeTimeout = clamped(val, 1, 0xffff) * PR_MSEC_PER_SEC;
    }
  }

  // The maximum number of current global half open sockets allowable
  // for starting a new speculative connection.
  if (PREF_CHANGED(HTTP_PREF("speculative-parallel-limit"))) {
    rv = Preferences::GetInt(HTTP_PREF("speculative-parallel-limit"), &val);
    if (NS_SUCCEEDED(rv)) {
      mParallelSpeculativeConnectLimit = (uint32_t)clamped(val, 0, 1024);
    }
  }

  // Whether or not to block requests for non head js/css items (e.g. media)
  // while those elements load.
  if (PREF_CHANGED(HTTP_PREF("rendering-critical-requests-prioritization"))) {
    rv = Preferences::GetBool(
        HTTP_PREF("rendering-critical-requests-prioritization"), &cVar);
    if (NS_SUCCEEDED(rv)) mCriticalRequestPrioritization = cVar;
  }

  // on transition of network.http.diagnostics to true print
  // a bunch of information to the console
  if (pref && PREF_CHANGED(HTTP_PREF("diagnostics"))) {
    rv = Preferences::GetBool(HTTP_PREF("diagnostics"), &cVar);
    if (NS_SUCCEEDED(rv) && cVar) {
      if (mConnMgr) mConnMgr->PrintDiagnostics();
    }
  }

  if (PREF_CHANGED(HTTP_PREF("max_response_header_size"))) {
    rv = Preferences::GetInt(HTTP_PREF("max_response_header_size"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxHttpResponseHeaderSize = val;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.enable"))) {
    rv = Preferences::GetBool(HTTP_PREF("throttle.enable"), &mThrottleEnabled);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(nsHttpConnectionMgr::THROTTLING_ENABLED,
                                      static_cast<int32_t>(mThrottleEnabled));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.version"))) {
    Unused << Preferences::GetInt(HTTP_PREF("throttle.version"), &val);
    mThrottleVersion = (uint32_t)clamped(val, 1, 2);
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.suspend-for"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.suspend-for"), &val);
    mThrottleSuspendFor = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(
          nsHttpConnectionMgr::THROTTLING_SUSPEND_FOR, mThrottleSuspendFor);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.resume-for"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.resume-for"), &val);
    mThrottleResumeFor = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(
          nsHttpConnectionMgr::THROTTLING_RESUME_FOR, mThrottleResumeFor);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.read-limit-bytes"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.read-limit-bytes"), &val);
    mThrottleReadLimit = (uint32_t)clamped(val, 0, 500000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(
          nsHttpConnectionMgr::THROTTLING_READ_LIMIT, mThrottleReadLimit);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.read-interval-ms"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.read-interval-ms"), &val);
    mThrottleReadInterval = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(
          nsHttpConnectionMgr::THROTTLING_READ_INTERVAL, mThrottleReadInterval);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.hold-time-ms"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.hold-time-ms"), &val);
    mThrottleHoldTime = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(nsHttpConnectionMgr::THROTTLING_HOLD_TIME,
                                      mThrottleHoldTime);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.max-time-ms"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.max-time-ms"), &val);
    mThrottleMaxTime = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(nsHttpConnectionMgr::THROTTLING_MAX_TIME,
                                      mThrottleMaxTime);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("send_window_size"))) {
    Unused << Preferences::GetInt(HTTP_PREF("send_window_size"), &val);
    mSendWindowSize = val >= 0 ? val : 0;
  }

  if (PREF_CHANGED(HTTP_PREF("on_click_priority"))) {
    Unused << Preferences::GetBool(HTTP_PREF("on_click_priority"),
                                   &mUrgentStartEnabled);
  }

  if (PREF_CHANGED(HTTP_PREF("tailing.enabled"))) {
    Unused << Preferences::GetBool(HTTP_PREF("tailing.enabled"),
                                   &mTailBlockingEnabled);
  }
  if (PREF_CHANGED(HTTP_PREF("tailing.delay-quantum"))) {
    Unused << Preferences::GetInt(HTTP_PREF("tailing.delay-quantum"), &val);
    mTailDelayQuantum = (uint32_t)clamped(val, 0, 60000);
  }
  if (PREF_CHANGED(HTTP_PREF("tailing.delay-quantum-after-domcontentloaded"))) {
    Unused << Preferences::GetInt(
        HTTP_PREF("tailing.delay-quantum-after-domcontentloaded"), &val);
    mTailDelayQuantumAfterDCL = (uint32_t)clamped(val, 0, 60000);
  }
  if (PREF_CHANGED(HTTP_PREF("tailing.delay-max"))) {
    Unused << Preferences::GetInt(HTTP_PREF("tailing.delay-max"), &val);
    mTailDelayMax = (uint32_t)clamped(val, 0, 60000);
  }
  if (PREF_CHANGED(HTTP_PREF("tailing.total-max"))) {
    Unused << Preferences::GetInt(HTTP_PREF("tailing.total-max"), &val);
    mTailTotalMax = (uint32_t)clamped(val, 0, 60000);
  }

  if (PREF_CHANGED(HTTP_PREF("focused_window_transaction_ratio"))) {
    float ratio = 0;
    rv = Preferences::GetFloat(HTTP_PREF("focused_window_transaction_ratio"),
                               &ratio);
    if (NS_SUCCEEDED(rv)) {
      if (ratio > 0 && ratio < 1) {
        mFocusedWindowTransactionRatio = ratio;
      } else {
        NS_WARNING("Wrong value for focused_window_transaction_ratio");
      }
    }
  }

  //
  // INTL options
  //

  if (PREF_CHANGED(INTL_ACCEPT_LANGUAGES)) {
    // We don't want to set the new accept languages here since
    // this pref is a complex type and it may be racy with flushing
    // string resources.
    mAcceptLanguagesIsDirty = true;
  }

  //
  // Tracking options
  //

  // Hint option
  if (PREF_CHANGED(SAFE_HINT_HEADER_VALUE)) {
    cVar = false;
    rv = Preferences::GetBool(SAFE_HINT_HEADER_VALUE, &cVar);
    if (NS_SUCCEEDED(rv)) {
      mSafeHintEnabled = cVar;
    }
  }

  // toggle to true anytime a token bucket related pref is changed.. that
  // includes telemetry and allow-experiments because of the abtest profile
  bool requestTokenBucketUpdated = false;

  // "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256" is the required h2 interop
  // suite.

  if (PREF_CHANGED(H2MANDATORY_SUITE)) {
    cVar = false;
    rv = Preferences::GetBool(H2MANDATORY_SUITE, &cVar);
    if (NS_SUCCEEDED(rv)) {
      mH2MandatorySuiteEnabled = cVar;
    }
  }

  // network.http.debug-observations
  if (PREF_CHANGED("network.http.debug-observations")) {
    cVar = false;
    rv = Preferences::GetBool("network.http.debug-observations", &cVar);
    if (NS_SUCCEEDED(rv)) {
      mDebugObservations = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("pacing.requests.enabled"))) {
    rv = Preferences::GetBool(HTTP_PREF("pacing.requests.enabled"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mRequestTokenBucketEnabled = cVar;
      requestTokenBucketUpdated = true;
    }
  }
  if (PREF_CHANGED(HTTP_PREF("pacing.requests.min-parallelism"))) {
    rv =
        Preferences::GetInt(HTTP_PREF("pacing.requests.min-parallelism"), &val);
    if (NS_SUCCEEDED(rv)) {
      mRequestTokenBucketMinParallelism =
          static_cast<uint16_t>(clamped(val, 1, 1024));
      requestTokenBucketUpdated = true;
    }
  }
  if (PREF_CHANGED(HTTP_PREF("pacing.requests.hz"))) {
    rv = Preferences::GetInt(HTTP_PREF("pacing.requests.hz"), &val);
    if (NS_SUCCEEDED(rv)) {
      mRequestTokenBucketHz = static_cast<uint32_t>(clamped(val, 1, 10000));
      requestTokenBucketUpdated = true;
    }
  }
  if (PREF_CHANGED(HTTP_PREF("pacing.requests.burst"))) {
    rv = Preferences::GetInt(HTTP_PREF("pacing.requests.burst"), &val);
    if (NS_SUCCEEDED(rv)) {
      mRequestTokenBucketBurst = val ? val : 1;
      requestTokenBucketUpdated = true;
    }
  }
  if (requestTokenBucketUpdated) {
    MakeNewRequestTokenBucket();
  }

  // Keepalive values for initial and idle connections.
  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.short_lived_connections"))) {
    rv = Preferences::GetBool(
        HTTP_PREF("tcp_keepalive.short_lived_connections"), &cVar);
    if (NS_SUCCEEDED(rv) && cVar != mTCPKeepaliveShortLivedEnabled) {
      mTCPKeepaliveShortLivedEnabled = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.short_lived_time"))) {
    rv = Preferences::GetInt(HTTP_PREF("tcp_keepalive.short_lived_time"), &val);
    if (NS_SUCCEEDED(rv) && val > 0) {
      mTCPKeepaliveShortLivedTimeS = clamped(val, 1, 300);  // Max 5 mins.
    }
  }

  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.short_lived_idle_time"))) {
    rv = Preferences::GetInt(HTTP_PREF("tcp_keepalive.short_lived_idle_time"),
                             &val);
    if (NS_SUCCEEDED(rv) && val > 0) {
      mTCPKeepaliveShortLivedIdleTimeS = clamped(val, 1, kMaxTCPKeepIdle);
    }
  }

  // Keepalive values for Long-lived Connections.
  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.long_lived_connections"))) {
    rv = Preferences::GetBool(HTTP_PREF("tcp_keepalive.long_lived_connections"),
                              &cVar);
    if (NS_SUCCEEDED(rv) && cVar != mTCPKeepaliveLongLivedEnabled) {
      mTCPKeepaliveLongLivedEnabled = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.long_lived_idle_time"))) {
    rv = Preferences::GetInt(HTTP_PREF("tcp_keepalive.long_lived_idle_time"),
                             &val);
    if (NS_SUCCEEDED(rv) && val > 0) {
      mTCPKeepaliveLongLivedIdleTimeS = clamped(val, 1, kMaxTCPKeepIdle);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("enforce-framing.http1")) ||
      PREF_CHANGED(HTTP_PREF("enforce-framing.soft")) ||
      PREF_CHANGED(HTTP_PREF("enforce-framing.strict_chunked_encoding"))) {
    rv = Preferences::GetBool(HTTP_PREF("enforce-framing.http1"), &cVar);
    if (NS_SUCCEEDED(rv) && cVar) {
      mEnforceH1Framing = FRAMECHECK_STRICT;
    } else {
      rv = Preferences::GetBool(
          HTTP_PREF("enforce-framing.strict_chunked_encoding"), &cVar);
      if (NS_SUCCEEDED(rv) && cVar) {
        mEnforceH1Framing = FRAMECHECK_STRICT_CHUNKED;
      } else {
        rv = Preferences::GetBool(HTTP_PREF("enforce-framing.soft"), &cVar);
        if (NS_SUCCEEDED(rv) && cVar) {
          mEnforceH1Framing = FRAMECHECK_BARELY;
        } else {
          mEnforceH1Framing = FRAMECHECK_LAX;
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.default-hpack-buffer"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.default-hpack-buffer"), &val);
    if (NS_SUCCEEDED(rv)) {
      mDefaultHpackBuffer = val;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("http3.enabled"))) {
    rv = Preferences::GetBool(HTTP_PREF("http3.enabled"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mHttp3Enabled = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("http3.default-qpack-table-size"))) {
    rv = Preferences::GetInt(HTTP_PREF("http3.default-qpack-table-size"), &val);
    if (NS_SUCCEEDED(rv)) {
      mQpackTableSize = val;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("http3.default-max-stream-blocked"))) {
    rv = Preferences::GetInt(HTTP_PREF("http3.default-max-stream-blocked"),
                             &val);
    if (NS_SUCCEEDED(rv)) {
      mHttp3MaxBlockedStreams = clamped(val, 0, 0xffff);
    }
  }

  const bool imageAcceptPrefChanged =
      PREF_CHANGED("image.http.accept") || PREF_CHANGED("image.avif.enabled") ||
      PREF_CHANGED("image.jxl.enabled") || PREF_CHANGED("image.webp.enabled");

  if (imageAcceptPrefChanged) {
    nsAutoCString userSetImageAcceptHeader;

    if (Preferences::HasUserValue("image.http.accept")) {
      rv = Preferences::GetCString("image.http.accept",
                                   userSetImageAcceptHeader);
      if (NS_FAILED(rv)) {
        userSetImageAcceptHeader.Truncate();
      }
    }

    if (userSetImageAcceptHeader.IsEmpty()) {
      mImageAcceptHeader.Assign(ImageAcceptHeader());
    } else {
      mImageAcceptHeader.Assign(userSetImageAcceptHeader);
    }
  }

  if (PREF_CHANGED("network.http.accept") || imageAcceptPrefChanged) {
    nsAutoCString userSetDocumentAcceptHeader;

    if (Preferences::HasUserValue("network.http.accept")) {
      rv = Preferences::GetCString("network.http.accept",
                                   userSetDocumentAcceptHeader);
      if (NS_FAILED(rv)) {
        userSetDocumentAcceptHeader.Truncate();
      }
    }

    if (userSetDocumentAcceptHeader.IsEmpty()) {
      mDocumentAcceptHeader.Assign(DocumentAcceptHeader(mImageAcceptHeader));
    } else {
      mDocumentAcceptHeader.Assign(userSetDocumentAcceptHeader);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("http3.alt-svc-mapping-for-testing"))) {
    nsAutoCString altSvcMappings;
    rv = Preferences::GetCString(HTTP_PREF("http3.alt-svc-mapping-for-testing"),
                                 altSvcMappings);
    if (NS_SUCCEEDED(rv)) {
      for (const nsACString& tokenSubstring :
           nsCCharSeparatedTokenizer(altSvcMappings, ',').ToRange()) {
        nsAutoCString token{tokenSubstring};
        int32_t index = token.Find(";");
        if (index != kNotFound) {
          mAltSvcMappingTemptativeMap.InsertOrUpdate(
              Substring(token, 0, index),
              MakeUnique<nsCString>(Substring(token, index + 1)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("http3.enable_qlog"))) {
    // Initialize the directory.
    nsCOMPtr<nsIFile> qlogDir;
    if (Preferences::GetBool(HTTP_PREF("http3.enable_qlog")) &&
        !mHttp3QlogDir.IsEmpty() &&
        NS_SUCCEEDED(NS_NewNativeLocalFile(mHttp3QlogDir, false,
                                           getter_AddRefs(qlogDir)))) {
      // Here we do main thread IO, but since this only happens
      // when enabling a developer feature it's not a problem for users.
      rv = qlogDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
      if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
        NS_WARNING("Creating qlog dir failed");
      }
    }
  }

  // Enable HTTP response timeout if TCP Keepalives are disabled.
  mResponseTimeoutEnabled =
      !mTCPKeepaliveShortLivedEnabled && !mTCPKeepaliveLongLivedEnabled;

#undef PREF_CHANGED
#undef MULTI_PREF_CHANGED
}

/**
 *  Allocates a C string into that contains a ISO 639 language list
 *  notated with HTTP "q" values for output with a HTTP Accept-Language
 *  header. Previous q values will be stripped because the order of
 *  the langs imply the q value. The q values are calculated by dividing
 *  1.0 amongst the number of languages present.
 *
 *  Ex: passing: "en, ja"
 *      returns: "en,ja;q=0.5"
 *
 *      passing: "en, ja, fr_CA"
 *      returns: "en,ja;q=0.7,fr_CA;q=0.3"
 */
static nsresult PrepareAcceptLanguages(const char* i_AcceptLanguages,
                                       nsACString& o_AcceptLanguages) {
  if (!i_AcceptLanguages) return NS_OK;

  const nsAutoCString ns_accept_languages(i_AcceptLanguages);
  return rust_prepare_accept_languages(&ns_accept_languages,
                                       &o_AcceptLanguages);
}

// Ensure that we've fetched the AcceptLanguages setting
/* static */
void nsHttpHandler::PresetAcceptLanguages() {
  if (!gHttpHandler) {
    RefPtr<nsHttpHandler> handler = nsHttpHandler::GetInstance();
    Unused << handler.get();
  }
  [[maybe_unused]] nsresult rv = gHttpHandler->SetAcceptLanguages();
}

nsresult nsHttpHandler::SetAcceptLanguages() {
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Forward to the main thread synchronously.
    SyncRunnable::DispatchToThread(
        mainThread, new SyncRunnable(NS_NewRunnableFunction(
                        "nsHttpHandler::SetAcceptLanguages",
                        [&rv]() { rv = gHttpHandler->SetAcceptLanguages(); })));
    return rv;
  }

  MOZ_ASSERT(NS_IsMainThread());

  mAcceptLanguagesIsDirty = false;

  nsAutoCString acceptLanguages;
  Preferences::GetLocalizedCString(INTL_ACCEPT_LANGUAGES, acceptLanguages);

  nsAutoCString buf;
  nsresult rv = PrepareAcceptLanguages(acceptLanguages.get(), buf);
  if (NS_SUCCEEDED(rv)) {
    mAcceptLanguages.Assign(buf);
  }
  return rv;
}

nsresult nsHttpHandler::SetAcceptEncodings(const char* aAcceptEncodings,
                                           bool isSecure) {
  if (isSecure) {
    mHttpsAcceptEncodings = aAcceptEncodings;
  } else {
    // use legacy list if a secure override is not specified
    mHttpAcceptEncodings = aAcceptEncodings;
    if (mHttpsAcceptEncodings.IsEmpty()) {
      mHttpsAcceptEncodings = aAcceptEncodings;
    }
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsHttpHandler, nsIHttpProtocolHandler,
                  nsIProxiedProtocolHandler, nsIProtocolHandler, nsIObserver,
                  nsISupportsWeakReference, nsISpeculativeConnect)

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetScheme(nsACString& aScheme) {
  aScheme.AssignLiteral("http");
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetDefaultPort(int32_t* result) {
  *result = NS_HTTP_DEFAULT_PORT;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProtocolFlags(uint32_t* result) {
  *result = NS_HTTP_PROTOCOL_FLAGS;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::NewChannel(nsIURI* uri, nsILoadInfo* aLoadInfo,
                          nsIChannel** result) {
  LOG(("nsHttpHandler::NewChannel\n"));

  NS_ENSURE_ARG_POINTER(uri);
  NS_ENSURE_ARG_POINTER(result);

  // Verify that we have been given a valid scheme
  if (!uri->SchemeIs("http") && !uri->SchemeIs("https")) {
    NS_WARNING("Invalid URI scheme");
    return NS_ERROR_UNEXPECTED;
  }

  return NewProxiedChannel(uri, nullptr, 0, nullptr, aLoadInfo, result);
}

NS_IMETHODIMP
nsHttpHandler::AllowPort(int32_t port, const char* scheme, bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProxiedProtocolHandler
//-----------------------------------------------------------------------------

nsresult nsHttpHandler::SetupChannelInternal(
    HttpBaseChannel* aChannel, nsIURI* uri, nsIProxyInfo* givenProxyInfo,
    uint32_t proxyResolveFlags, nsIURI* proxyURI, nsILoadInfo* aLoadInfo,
    nsIChannel** result) {
  RefPtr<HttpBaseChannel> httpChannel = aChannel;

  nsCOMPtr<nsProxyInfo> proxyInfo;
  if (givenProxyInfo) {
    proxyInfo = do_QueryInterface(givenProxyInfo);
    NS_ENSURE_ARG(proxyInfo);
  }

  uint32_t caps = mCapabilities;

  uint64_t channelId;
  nsresult rv = NewChannelId(channelId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = httpChannel->Init(uri, caps, proxyInfo, proxyResolveFlags, proxyURI,
                         channelId, aLoadInfo->GetExternalContentPolicyType());
  if (NS_FAILED(rv)) return rv;

  // TRRServiceChannel doesn't need loadInfo.
  if (aLoadInfo) {
    // set the loadInfo on the new channel
    rv = httpChannel->SetLoadInfo(aLoadInfo);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  httpChannel.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::NewProxiedChannel(nsIURI* uri, nsIProxyInfo* givenProxyInfo,
                                 uint32_t proxyResolveFlags, nsIURI* proxyURI,
                                 nsILoadInfo* aLoadInfo, nsIChannel** result) {
  HttpBaseChannel* httpChannel;

  LOG(("nsHttpHandler::NewProxiedChannel [proxyInfo=%p]\n", givenProxyInfo));

  if (IsNeckoChild()) {
    httpChannel = new HttpChannelChild();
  } else {
    // HACK: make sure PSM gets initialized on the main thread.
    net_EnsurePSMInit();
    httpChannel = new nsHttpChannel();
  }

  return SetupChannelInternal(httpChannel, uri, givenProxyInfo,
                              proxyResolveFlags, proxyURI, aLoadInfo, result);
}

nsresult nsHttpHandler::CreateTRRServiceChannel(
    nsIURI* uri, nsIProxyInfo* givenProxyInfo, uint32_t proxyResolveFlags,
    nsIURI* proxyURI, nsILoadInfo* aLoadInfo, nsIChannel** result) {
  HttpBaseChannel* httpChannel = new TRRServiceChannel();

  LOG(("nsHttpHandler::CreateTRRServiceChannel [proxyInfo=%p]\n",
       givenProxyInfo));

  return SetupChannelInternal(httpChannel, uri, givenProxyInfo,
                              proxyResolveFlags, proxyURI, aLoadInfo, result);
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIHttpProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetUserAgent(nsACString& value) {
  value = UserAgent();
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAppName(nsACString& value) {
  value = mLegacyAppName;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAppVersion(nsACString& value) {
  value = mLegacyAppVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetPlatform(nsACString& value) {
  value = mPlatform;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetOscpu(nsACString& value) {
  value = mOscpu;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetMisc(nsACString& value) {
  value = mMisc;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAltSvcCacheKeys(nsTArray<nsCString>& value) {
  return mAltSvcCache->GetAltSvcCacheKeys(value);
}

NS_IMETHODIMP
nsHttpHandler::GetAuthCacheKeys(nsTArray<nsCString>& aValues) {
  mAuthCache.CollectKeys(aValues);
  mPrivateAuthCache.CollectKeys(aValues);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::Observe(nsISupports* subject, const char* topic,
                       const char16_t* data) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("nsHttpHandler::Observe [topic=\"%s\"]\n", topic));

  nsresult rv;
  if (!strcmp(topic, "profile-change-net-teardown") ||
      !strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mHandlerActive = false;

    // clear cache of all authentication credentials.
    mAuthCache.ClearAll();
    mPrivateAuthCache.ClearAll();
    if (mWifiTickler) mWifiTickler->Cancel();

    // Inform nsIOService that network is tearing down.
    gIOService->SetHttpHandlerAlreadyShutingDown();

    ShutdownConnectionManager();

    // need to reset the session start time since cache validation may
    // depend on this value.
    mSessionStartTime = NowInSeconds();

    // Since nsHttpHandler::Observe() is also called in socket process, we don't
    // want to do telemetry twice.
    if (XRE_IsParentProcess()) {
      if (!StaticPrefs::privacy_donottrackheader_enabled()) {
        Telemetry::Accumulate(Telemetry::DNT_USAGE, 2);
      } else {
        Telemetry::Accumulate(Telemetry::DNT_USAGE, 1);
      }
    }
  } else if (!strcmp(topic, "profile-change-net-restore")) {
    // initialize connection manager
    rv = InitConnectionMgr();
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mAltSvcCache = MakeUnique<AltSvcCache>();
  } else if (!strcmp(topic, "net:clear-active-logins")) {
    mAuthCache.ClearAll();
    mPrivateAuthCache.ClearAll();
  } else if (!strcmp(topic, "net:cancel-all-connections")) {
    if (mConnMgr) {
      mConnMgr->AbortAndCloseAllConnections(0, nullptr);
    }
  } else if (!strcmp(topic, "net:prune-dead-connections")) {
    if (mConnMgr) {
      rv = mConnMgr->PruneDeadConnections();
      if (NS_FAILED(rv)) {
        LOG(("    PruneDeadConnections failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
    }
  } else if (!strcmp(topic, "net:prune-all-connections")) {
    if (mConnMgr) {
      rv = mConnMgr->DoShiftReloadConnectionCleanup();
      if (NS_FAILED(rv)) {
        LOG(("    DoShiftReloadConnectionCleanup failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
      rv = mConnMgr->PruneDeadConnections();
      if (NS_FAILED(rv)) {
        LOG(("    PruneDeadConnections failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
    }
#if 0
    } else if (!strcmp(topic, "net:failed-to-process-uri-content")) {
         // nop right now - we used to cancel h1 pipelines based on this,
         // but those are no longer implemented
         nsCOMPtr<nsIURI> uri = do_QueryInterface(subject);
#endif
  } else if (!strcmp(topic, "last-pb-context-exited")) {
    mPrivateAuthCache.ClearAll();
    if (mAltSvcCache) {
      mAltSvcCache->ClearAltServiceMappings();
    }
  } else if (!strcmp(topic, "browser:purge-session-history")) {
    if (mConnMgr) {
      Unused << mConnMgr->ClearConnectionHistory();
    }
    if (mAltSvcCache) {
      mAltSvcCache->ClearAltServiceMappings();
    }
  } else if (!strcmp(topic, NS_NETWORK_LINK_TOPIC)) {
    nsAutoCString converted = NS_ConvertUTF16toUTF8(data);
    if (!strcmp(converted.get(), NS_NETWORK_LINK_DATA_CHANGED)) {
      if (mConnMgr) {
        rv = mConnMgr->PruneDeadConnections();
        if (NS_FAILED(rv)) {
          LOG(("    PruneDeadConnections failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
        rv = mConnMgr->VerifyTraffic();
        if (NS_FAILED(rv)) {
          LOG(("    VerifyTraffic failed (%08x)\n", static_cast<uint32_t>(rv)));
        }
      }
    }
  } else if (!strcmp(topic, "application-background")) {
    // going to the background on android means we should close
    // down idle connections for power conservation
    if (mConnMgr) {
      rv = mConnMgr->DoShiftReloadConnectionCleanup();
      if (NS_FAILED(rv)) {
        LOG(("    DoShiftReloadConnectionCleanup failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
    }
  } else if (!strcmp(topic, "net:current-top-browsing-context-id")) {
    // The window id will be updated by HttpConnectionMgrParent.
    if (XRE_IsParentProcess()) {
      nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(subject);
      MOZ_RELEASE_ASSERT(wrapper);

      uint64_t id = 0;
      wrapper->GetData(&id);
      MOZ_ASSERT(id);

      static uint64_t sCurrentBrowsingContextId = 0;
      if (sCurrentBrowsingContextId != id) {
        sCurrentBrowsingContextId = id;
        if (mConnMgr) {
          mConnMgr->UpdateCurrentTopBrowsingContextId(
              sCurrentBrowsingContextId);
        }
      }
    }
  } else if (!strcmp(topic, "psm:user-certificate-added")) {
    // A user certificate has just been added.
    // We should immediately disable speculative connect
    mSpeculativeConnectEnabled = false;
  } else if (!strcmp(topic, "psm:user-certificate-deleted")) {
    // If a user certificate has been removed, we need to check if there
    // are others installed
    MaybeEnableSpeculativeConnect();
  } else if (!strcmp(topic, "intl:app-locales-changed")) {
    // If the locale changed, there's a chance the accept language did too
    mAcceptLanguagesIsDirty = true;
  } else if (!strcmp(topic, "browser-delayed-startup-finished")) {
    MaybeEnableSpeculativeConnect();
  } else if (!strcmp(topic, "network:captive-portal-connectivity")) {
    nsAutoCString data8 = NS_ConvertUTF16toUTF8(data);
    mThroughCaptivePortal = data8.EqualsLiteral("captive");
  } else if (!strcmp(topic, "network:reset-http3-excluded-list")) {
    MutexAutoLock lock(mHttpExclusionLock);
    mExcludedHttp3Origins.Clear();
  }

  return NS_OK;
}

// nsISpeculativeConnect

static bool CanEnableSpeculativeConnect() {
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));

  MOZ_ASSERT(!NS_IsMainThread(), "Must run on the background thread");
  // Check if any 3rd party PKCS#11 module are installed, as they may produce
  // client certificates
  bool activeSmartCards = false;
  nsresult rv = component->HasActiveSmartCards(&activeSmartCards);
  if (NS_FAILED(rv) || activeSmartCards) {
    return false;
  }

  // If there are any client certificates installed, we can't enable speculative
  // connect, as it may pop up the certificate chooser at any time.
  bool hasUserCerts = false;
  rv = component->HasUserCertsInstalled(&hasUserCerts);
  if (NS_FAILED(rv) || hasUserCerts) {
    return false;
  }

  // No smart cards and no client certificates means
  // we can enable speculative connect.
  return true;
}

void nsHttpHandler::MaybeEnableSpeculativeConnect() {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  // We don't need to and can't check this in the child and socket process.
  if (!XRE_IsParentProcess()) {
    return;
  }

  net_EnsurePSMInit();

  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction("CanEnableSpeculativeConnect", [] {
        gHttpHandler->mSpeculativeConnectEnabled =
            CanEnableSpeculativeConnect();
      }));
}

nsresult nsHttpHandler::SpeculativeConnectInternal(
    nsIURI* aURI, nsIPrincipal* aPrincipal, nsIInterfaceRequestor* aCallbacks,
    bool anonymous) {
  if (IsNeckoChild()) {
    gNeckoChild->SendSpeculativeConnect(aURI, aPrincipal, anonymous);
    return NS_OK;
  }

  if (!mHandlerActive) return NS_OK;

  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (mDebugObservations && obsService) {
    // this is basically used for test coverage of an otherwise 'hintable'
    // feature
    obsService->NotifyObservers(nullptr, "speculative-connect-request",
                                nullptr);
    for (auto* cp :
         dom::ContentParent::AllProcesses(dom::ContentParent::eLive)) {
      PNeckoParent* neckoParent =
          SingleManagedOrNull(cp->ManagedPNeckoParent());
      if (!neckoParent) {
        continue;
      }
      Unused << neckoParent->SendSpeculativeConnectRequest();
    }
  }

  nsISiteSecurityService* sss = gHttpHandler->GetSSService();
  bool isStsHost = false;
  if (!sss) return NS_OK;

  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(aCallbacks);
  uint32_t flags = 0;
  if (loadContext && loadContext->UsePrivateBrowsing()) {
    flags |= nsISocketProvider::NO_PERMANENT_STORAGE;
  }

  OriginAttributes originAttributes;
  // If the principal is given, we use the originAttributes from this
  // principal. Otherwise, we use the originAttributes from the loadContext.
  if (aPrincipal) {
    originAttributes = aPrincipal->OriginAttributesRef();
  } else if (loadContext) {
    loadContext->GetOriginAttributes(originAttributes);
  }

  StoragePrincipalHelper::UpdateOriginAttributesForNetworkState(
      aURI, originAttributes);

  nsCOMPtr<nsIURI> clone;
  if (NS_SUCCEEDED(sss->IsSecureURI(aURI, flags, originAttributes, nullptr,
                                    nullptr, &isStsHost)) &&
      isStsHost) {
    if (NS_SUCCEEDED(NS_GetSecureUpgradedURI(aURI, getter_AddRefs(clone)))) {
      aURI = clone.get();
      // (NOTE: We better make sure |clone| stays alive until the end
      // of the function now, since our aURI arg now points to it!)
    }
  }

  nsAutoCString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  if (NS_FAILED(rv)) return rv;

  // If this is HTTPS, make sure PSM is initialized as the channel
  // creation path may have been bypassed
  if (scheme.EqualsLiteral("https")) {
    if (!IsNeckoChild()) {
      // make sure PSM gets initialized on the main thread.
      net_EnsurePSMInit();
    }
  }
  // Ensure that this is HTTP or HTTPS, otherwise we don't do preconnect here
  else if (!scheme.EqualsLiteral("http")) {
    return NS_ERROR_UNEXPECTED;
  }

  // Construct connection info object
  if (aURI->SchemeIs("https") && !mSpeculativeConnectEnabled) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString host;
  rv = aURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  int32_t port = -1;
  rv = aURI->GetPort(&port);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString username;
  aURI->GetUsername(username);

  RefPtr<nsHttpConnectionInfo> ci =
      new nsHttpConnectionInfo(host, port, ""_ns, username, nullptr,
                               originAttributes, aURI->SchemeIs("https"));
  ci->SetAnonymous(anonymous);
  if (originAttributes.mPrivateBrowsingId > 0) {
    ci->SetPrivate(true);
  }
  return SpeculativeConnect(ci, aCallbacks);
}

NS_IMETHODIMP
nsHttpHandler::SpeculativeConnect(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                  nsIInterfaceRequestor* aCallbacks) {
  return SpeculativeConnectInternal(aURI, aPrincipal, aCallbacks, false);
}

NS_IMETHODIMP
nsHttpHandler::SpeculativeAnonymousConnect(nsIURI* aURI,
                                           nsIPrincipal* aPrincipal,
                                           nsIInterfaceRequestor* aCallbacks) {
  return SpeculativeConnectInternal(aURI, aPrincipal, aCallbacks, true);
}

void nsHttpHandler::TickleWifi(nsIInterfaceRequestor* cb) {
  if (!cb || !mWifiTickler) return;

  // If B2G requires a similar mechanism nsINetworkManager, currently only avail
  // on B2G, contains the necessary information on wifi and gateway

  nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(cb);
  nsCOMPtr<nsPIDOMWindowOuter> piWindow = do_QueryInterface(domWindow);
  if (!piWindow) return;

  RefPtr<dom::Navigator> navigator = piWindow->GetNavigator();
  if (!navigator) return;

  RefPtr<dom::network::Connection> networkProperties =
      navigator->GetConnection(IgnoreErrors());
  if (!networkProperties) return;

  uint32_t gwAddress = networkProperties->GetDhcpGateway();
  bool isWifi = networkProperties->GetIsWifi();
  if (!gwAddress || !isWifi) return;

  mWifiTickler->SetIPV4Address(gwAddress);
  mWifiTickler->Tickle();
}

//-----------------------------------------------------------------------------
// nsHttpsHandler implementation
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsHttpsHandler, nsIHttpProtocolHandler,
                  nsIProxiedProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference, nsISpeculativeConnect)

nsresult nsHttpsHandler::Init() {
  nsCOMPtr<nsIProtocolHandler> httpHandler(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http"));
  MOZ_ASSERT(httpHandler.get() != nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetScheme(nsACString& aScheme) {
  aScheme.AssignLiteral("https");
  return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetDefaultPort(int32_t* aPort) {
  *aPort = NS_HTTPS_DEFAULT_PORT;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetProtocolFlags(uint32_t* aProtocolFlags) {
  *aProtocolFlags = NS_HTTP_PROTOCOL_FLAGS | URI_IS_POTENTIALLY_TRUSTWORTHY;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                           nsIChannel** _retval) {
  MOZ_ASSERT(gHttpHandler);
  if (!gHttpHandler) return NS_ERROR_UNEXPECTED;
  return gHttpHandler->NewChannel(aURI, aLoadInfo, _retval);
}

NS_IMETHODIMP
nsHttpsHandler::AllowPort(int32_t aPort, const char* aScheme, bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::EnsureHSTSDataReadyNative(
    RefPtr<mozilla::net::HSTSDataCallbackWrapper> aCallback) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), "http://example.com");
  NS_ENSURE_SUCCESS(rv, rv);

  bool shouldUpgrade = false;
  bool willCallback = false;
  OriginAttributes originAttributes;
  auto func = [callback(aCallback)](bool aResult, nsresult aStatus) {
    callback->DoCallback(aResult);
  };
  rv = NS_ShouldSecureUpgrade(uri, nullptr, nullptr, false, false,
                              originAttributes, shouldUpgrade, std::move(func),
                              willCallback);
  if (NS_FAILED(rv) || !willCallback) {
    aCallback->DoCallback(false);
    return rv;
  }

  return rv;
}

NS_IMETHODIMP
nsHttpHandler::EnsureHSTSDataReady(JSContext* aCx, Promise** aPromise) {
  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  if (IsNeckoChild()) {
    gNeckoChild->SendEnsureHSTSData()->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [promise(promise)](
            NeckoChild::EnsureHSTSDataPromise::ResolveOrRejectValue&& aResult) {
          if (aResult.IsResolve()) {
            promise->MaybeResolve(aResult.ResolveValue());
          } else {
            promise->MaybeReject(NS_ERROR_FAILURE);
          }
        });
    promise.forget(aPromise);
    return NS_OK;
  }

  auto callback = [promise(promise)](bool aResult) {
    promise->MaybeResolve(aResult);
  };

  RefPtr<HSTSDataCallbackWrapper> wrapper =
      new HSTSDataCallbackWrapper(std::move(callback));
  promise.forget(aPromise);
  return EnsureHSTSDataReadyNative(wrapper);
}

NS_IMETHODIMP
nsHttpHandler::ClearCORSPreflightCache() {
  nsCORSListenerProxy::ClearCache();
  return NS_OK;
}

void nsHttpHandler::ShutdownConnectionManager() {
  // ensure connection manager is shutdown
  if (mConnMgr) {
    nsresult rv = mConnMgr->Shutdown();
    if (NS_FAILED(rv)) {
      LOG(
          ("nsHttpHandler::ShutdownConnectionManager\n"
           "    failed to shutdown connection manager\n"));
    }
  }
}

nsresult nsHttpHandler::NewChannelId(uint64_t& channelId) {
  channelId =
      // channelId is sometimes passed to JavaScript code (e.g. devtools),
      // and since on Linux PID_MAX_LIMIT is 2^22 we cannot
      // shift PID more than 31 bits left. Otherwise resulting values
      // will be exceed safe JavaScript integer range.
      ((static_cast<uint64_t>(mProcessId) << 31) & 0xFFFFFFFF80000000LL) |
      mNextChannelId++;
  return NS_OK;
}

void nsHttpHandler::NotifyActiveTabLoadOptimization() {
  SetLastActiveTabLoadOptimizationHit(TimeStamp::Now());
}

TimeStamp nsHttpHandler::GetLastActiveTabLoadOptimizationHit() {
  MutexAutoLock lock(mLastActiveTabLoadOptimizationLock);

  return mLastActiveTabLoadOptimizationHit;
}

void nsHttpHandler::SetLastActiveTabLoadOptimizationHit(TimeStamp const& when) {
  MutexAutoLock lock(mLastActiveTabLoadOptimizationLock);

  if (mLastActiveTabLoadOptimizationHit.IsNull() ||
      (!when.IsNull() && mLastActiveTabLoadOptimizationHit < when)) {
    mLastActiveTabLoadOptimizationHit = when;
  }
}

bool nsHttpHandler::IsBeforeLastActiveTabLoadOptimization(
    TimeStamp const& when) {
  MutexAutoLock lock(mLastActiveTabLoadOptimizationLock);

  return !mLastActiveTabLoadOptimizationHit.IsNull() &&
         when <= mLastActiveTabLoadOptimizationHit;
}

void nsHttpHandler::ExcludeHttp2OrHttp3Internal(
    const nsHttpConnectionInfo* ci) {
  LOG(("nsHttpHandler::ExcludeHttp2OrHttp3Internal ci=%s",
       ci->HashKey().get()));
  // The excluded list needs to be stayed synced between parent process and
  // socket process, so we send this information to the parent process here.
  if (XRE_IsSocketProcess()) {
    MOZ_ASSERT(OnSocketThread());

    RefPtr<nsHttpConnectionInfo> cinfo = ci->Clone();
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "nsHttpHandler::ExcludeHttp2OrHttp3Internal",
        [cinfo{std::move(cinfo)}]() {
          HttpConnectionInfoCloneArgs connInfoArgs;
          nsHttpConnectionInfo::SerializeHttpConnectionInfo(cinfo,
                                                            connInfoArgs);
          Unused << SocketProcessChild::GetSingleton()->SendExcludeHttp2OrHttp3(
              connInfoArgs);
        }));
  }

  MOZ_ASSERT_IF(nsIOService::UseSocketProcess() && XRE_IsParentProcess(),
                NS_IsMainThread());
  MOZ_ASSERT_IF(!nsIOService::UseSocketProcess(), OnSocketThread());

  if (ci->IsHttp3()) {
    if (!mExcludedHttp3Origins.Contains(ci->GetRoutedHost())) {
      MutexAutoLock lock(mHttpExclusionLock);
      mExcludedHttp3Origins.Insert(ci->GetRoutedHost());
    }
    mConnMgr->ExcludeHttp3(ci);
  } else {
    if (!mExcludedHttp2Origins.Contains(ci->GetOrigin())) {
      MutexAutoLock lock(mHttpExclusionLock);
      mExcludedHttp2Origins.Insert(ci->GetOrigin());
    }
    mConnMgr->ExcludeHttp2(ci);
  }
}

void nsHttpHandler::ExcludeHttp2(const nsHttpConnectionInfo* ci) {
  ExcludeHttp2OrHttp3Internal(ci);
}

bool nsHttpHandler::IsHttp2Excluded(const nsHttpConnectionInfo* ci) {
  MutexAutoLock lock(mHttpExclusionLock);
  return mExcludedHttp2Origins.Contains(ci->GetOrigin());
}

void nsHttpHandler::ExcludeHttp3(const nsHttpConnectionInfo* ci) {
  MOZ_ASSERT(ci->IsHttp3());
  ExcludeHttp2OrHttp3Internal(ci);
}

bool nsHttpHandler::IsHttp3Excluded(const nsACString& aRoutedHost) {
  MutexAutoLock lock(mHttpExclusionLock);
  return mExcludedHttp3Origins.Contains(aRoutedHost);
}

HttpTrafficAnalyzer* nsHttpHandler::GetHttpTrafficAnalyzer() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (!StaticPrefs::network_traffic_analyzer_enabled()) {
    return nullptr;
  }

  return &mHttpTrafficAnalyzer;
}

bool nsHttpHandler::IsHttp3VersionSupported(const nsACString& version) {
  if (!StaticPrefs::network_http_http3_support_version1() &&
      version.EqualsLiteral("h3")) {
    return false;
  }
  for (uint32_t i = 0; i < kHttp3VersionCount; i++) {
    if (version.Equals(kHttp3Versions[i])) {
      return true;
    }
  }
  return false;
}

bool nsHttpHandler::IsHttp3SupportedByServer(
    nsHttpResponseHead* aResponseHead) {
  if ((aResponseHead->Version() != HttpVersion::v2_0) ||
      (aResponseHead->Status() >= 500) || (aResponseHead->Status() == 421)) {
    return false;
  }

  nsAutoCString altSvc;
  Unused << aResponseHead->GetHeader(nsHttp::Alternate_Service, altSvc);
  if (altSvc.IsEmpty() || !nsHttp::IsReasonableHeaderValue(altSvc)) {
    return false;
  }

  for (uint32_t i = 0; i < kHttp3VersionCount; i++) {
    nsAutoCString value(kHttp3Versions[i]);
    value.Append("="_ns);
    if (strstr(altSvc.get(), value.get())) {
      return true;
    }
  }

  return false;
}

nsresult nsHttpHandler::InitiateTransaction(HttpTransactionShell* aTrans,
                                            int32_t aPriority) {
  return mConnMgr->AddTransaction(aTrans, aPriority);
}
nsresult nsHttpHandler::InitiateTransactionWithStickyConn(
    HttpTransactionShell* aTrans, int32_t aPriority,
    HttpTransactionShell* aTransWithStickyConn) {
  return mConnMgr->AddTransactionWithStickyConn(aTrans, aPriority,
                                                aTransWithStickyConn);
}

nsresult nsHttpHandler::RescheduleTransaction(HttpTransactionShell* trans,
                                              int32_t priority) {
  return mConnMgr->RescheduleTransaction(trans, priority);
}

void nsHttpHandler::UpdateClassOfServiceOnTransaction(
    HttpTransactionShell* trans, uint32_t classOfService) {
  mConnMgr->UpdateClassOfServiceOnTransaction(trans, classOfService);
}

nsresult nsHttpHandler::CancelTransaction(HttpTransactionShell* trans,
                                          nsresult reason) {
  return mConnMgr->CancelTransaction(trans, reason);
}

void nsHttpHandler::AddHttpChannel(uint64_t aId, nsISupports* aChannel) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsWeakPtr channel(do_GetWeakReference(aChannel));
  mIDToHttpChannelMap.InsertOrUpdate(aId, std::move(channel));
}

void nsHttpHandler::RemoveHttpChannel(uint64_t aId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> idleRunnable(NewCancelableRunnableMethod<uint64_t>(
        "nsHttpHandler::RemoveHttpChannel", this,
        &nsHttpHandler::RemoveHttpChannel, aId));

    NS_DispatchToMainThreadQueue(do_AddRef(idleRunnable),
                                 EventQueuePriority::Idle);
    return;
  }

  auto entry = mIDToHttpChannelMap.Lookup(aId);
  if (entry) {
    entry.Remove();
  }
}

nsWeakPtr nsHttpHandler::GetWeakHttpChannel(uint64_t aId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  return mIDToHttpChannelMap.Get(aId);
}

nsresult nsHttpHandler::CompleteUpgrade(
    HttpTransactionShell* aTrans, nsIHttpUpgradeListener* aUpgradeListener) {
  return mConnMgr->CompleteUpgrade(aTrans, aUpgradeListener);
}

nsresult nsHttpHandler::DoShiftReloadConnectionCleanupWithConnInfo(
    nsHttpConnectionInfo* aCi) {
  MOZ_ASSERT(aCi);
  return mConnMgr->DoShiftReloadConnectionCleanupWithConnInfo(aCi);
}

void nsHttpHandler::ClearHostMapping(nsHttpConnectionInfo* aConnInfo) {
  if (XRE_IsSocketProcess()) {
    AltServiceChild::ClearHostMapping(aConnInfo);
    return;
  }

  AltServiceCache()->ClearHostMapping(aConnInfo);
}

void nsHttpHandler::SetHttpHandlerInitArgs(const HttpHandlerInitArgs& aArgs) {
  MOZ_ASSERT(XRE_IsSocketProcess());
  mLegacyAppName = aArgs.mLegacyAppName();
  mLegacyAppVersion = aArgs.mLegacyAppVersion();
  mPlatform = aArgs.mPlatform();
  mOscpu = aArgs.mOscpu();
  mMisc = aArgs.mMisc();
  mProduct = aArgs.mProduct();
  mProductSub = aArgs.mProductSub();
  mAppName = aArgs.mAppName();
  mAppVersion = aArgs.mAppVersion();
  mCompatFirefox = aArgs.mCompatFirefox();
  mCompatDevice = aArgs.mCompatDevice();
  mDeviceModelId = aArgs.mDeviceModelId();
}

void nsHttpHandler::SetDeviceModelId(const nsCString& aModelId) {
  MOZ_ASSERT(XRE_IsSocketProcess());
  mDeviceModelId = aModelId;
}

void nsHttpHandler::MaybeAddAltSvcForTesting(
    nsIURI* aUri, const nsACString& aUsername, bool aPrivateBrowsing,
    nsIInterfaceRequestor* aCallbacks,
    const OriginAttributes& aOriginAttributes) {
  if (!IsHttp3Enabled() || mAltSvcMappingTemptativeMap.IsEmpty()) {
    return;
  }

  bool isHttps = false;
  if (NS_FAILED(aUri->SchemeIs("https", &isHttps)) || !isHttps) {
    // Only set for HTTPS.
    return;
  }

  nsAutoCString originHost;
  if (NS_FAILED(aUri->GetAsciiHost(originHost))) {
    return;
  }

  nsCString* map = mAltSvcMappingTemptativeMap.Get(originHost);
  if (map) {
    int32_t originPort = 80;
    aUri->GetPort(&originPort);
    LOG(("nsHttpHandler::MaybeAddAltSvcForTesting for %s map: %s",
         originHost.get(), PromiseFlatCString(*map).get()));
    AltSvcMapping::ProcessHeader(
        *map, nsCString("https"), originHost, originPort, aUsername,
        aPrivateBrowsing, aCallbacks, nullptr, 0, aOriginAttributes, true);
  }
}

bool nsHttpHandler::UseHTTPSRRAsAltSvcEnabled() const {
  return StaticPrefs::network_dns_use_https_rr_as_altsvc();
}

bool nsHttpHandler::EchConfigEnabled(bool aIsHttp3) const {
  if (!aIsHttp3) {
    return StaticPrefs::network_dns_echconfig_enabled();
  }

  return StaticPrefs::network_dns_echconfig_enabled() &&
         StaticPrefs::network_dns_http3_echconfig_enabled();
}

bool nsHttpHandler::FallbackToOriginIfConfigsAreECHAndAllFailed() const {
  return StaticPrefs::
      network_dns_echconfig_fallback_to_origin_when_all_failed();
}

bool nsHttpHandler::UseHTTPSRRForSpeculativeConnection() const {
  return StaticPrefs::network_dns_use_https_rr_for_speculative_connection();
}

void nsHttpHandler::ExcludeHTTPSRRHost(const nsACString& aHost) {
  MOZ_ASSERT(NS_IsMainThread());

  mExcludedHostsForHTTPSRRUpgrade.Insert(aHost);
}

bool nsHttpHandler::IsHostExcludedForHTTPSRR(const nsACString& aHost) {
  MOZ_ASSERT(NS_IsMainThread());

  return mExcludedHostsForHTTPSRRUpgrade.Contains(aHost);
}

void nsHttpHandler::Exclude0RttTcp(const nsHttpConnectionInfo* ci) {
  MOZ_ASSERT(OnSocketThread());

  if (!StaticPrefs::network_http_early_data_disable_on_error() ||
      (mExcluded0RttTcpOrigins.Count() >=
       StaticPrefs::network_http_early_data_max_error())) {
    return;
  }

  mExcluded0RttTcpOrigins.Insert(ci->GetOrigin());
}

bool nsHttpHandler::Is0RttTcpExcluded(const nsHttpConnectionInfo* ci) {
  MOZ_ASSERT(OnSocketThread());
  if (!StaticPrefs::network_http_early_data_disable_on_error()) {
    return false;
  }

  if (mExcluded0RttTcpOrigins.Count() >=
      StaticPrefs::network_http_early_data_max_error()) {
    return true;
  }

  return mExcluded0RttTcpOrigins.Contains(ci->GetOrigin());
}

}  // namespace mozilla::net
