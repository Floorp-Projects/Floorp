/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ODoHService.h"

#include "DNSUtils.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsIDNSService.h"
#include "nsIDNSByTypeRecord.h"
#include "nsIOService.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "ODoH.h"
#include "TRRService.h"
#include "nsURLHelper.h"
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"

static const char kODoHProxyURIPref[] = "network.trr.odoh.proxy_uri";
static const char kODoHTargetHostPref[] = "network.trr.odoh.target_host";
static const char kODoHTargetPathPref[] = "network.trr.odoh.target_path";
static const char kODoHConfigsUriPref[] = "network.trr.odoh.configs_uri";

namespace mozilla {
namespace net {

ODoHService* gODoHService = nullptr;

NS_IMPL_ISUPPORTS(ODoHService, nsIDNSListener, nsIObserver,
                  nsISupportsWeakReference, nsITimerCallback,
                  nsIStreamLoaderObserver)

ODoHService::ODoHService()
    : mLock("net::ODoHService"), mQueryODoHConfigInProgress(false) {
  gODoHService = this;
}

ODoHService::~ODoHService() { gODoHService = nullptr; }

bool ODoHService::Init() {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefBranch) {
    return false;
  }

  prefBranch->AddObserver(kODoHProxyURIPref, this, true);
  prefBranch->AddObserver(kODoHTargetHostPref, this, true);
  prefBranch->AddObserver(kODoHTargetPathPref, this, true);
  prefBranch->AddObserver(kODoHConfigsUriPref, this, true);

  ReadPrefs(nullptr);

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "xpcom-shutdown-threads", true);
  }

  return true;
}

bool ODoHService::Enabled() const {
  return StaticPrefs::network_trr_odoh_enabled();
}

NS_IMETHODIMP
ODoHService::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    ReadPrefs(NS_ConvertUTF16toUTF8(aData).get());
  } else if (!strcmp(aTopic, "xpcom-shutdown-threads")) {
    if (mTTLTimer) {
      mTTLTimer->Cancel();
      mTTLTimer = nullptr;
    }
  }

  return NS_OK;
}

nsresult ODoHService::ReadPrefs(const char* aName) {
  if (!aName || !strcmp(aName, kODoHConfigsUriPref)) {
    OnODohConfigsURIChanged();
  }
  if (!aName || !strcmp(aName, kODoHProxyURIPref) ||
      !strcmp(aName, kODoHTargetHostPref) ||
      !strcmp(aName, kODoHTargetPathPref)) {
    OnODoHPrefsChange(aName == nullptr);
  }

  return NS_OK;
}

void ODoHService::OnODohConfigsURIChanged() {
  nsAutoCString uri;
  Preferences::GetCString(kODoHConfigsUriPref, uri);

  bool updateConfig = false;
  {
    MutexAutoLock lock(mLock);
    if (!mODoHConfigsUri.Equals(uri)) {
      mODoHConfigsUri = uri;
      updateConfig = true;
    }
  }

  if (updateConfig) {
    UpdateODoHConfigFromURI();
  }
}

void ODoHService::OnODoHPrefsChange(bool aInit) {
  nsAutoCString proxyURI;
  Preferences::GetCString(kODoHProxyURIPref, proxyURI);
  nsAutoCString targetHost;
  Preferences::GetCString(kODoHTargetHostPref, targetHost);
  nsAutoCString targetPath;
  Preferences::GetCString(kODoHTargetPathPref, targetPath);

  bool updateODoHConfig = false;
  {
    MutexAutoLock lock(mLock);
    mODoHProxyURI = proxyURI;
    // Only update ODoHConfig when the host is really changed.
    if (!mODoHTargetHost.Equals(targetHost) && mODoHConfigsUri.IsEmpty()) {
      updateODoHConfig = true;
    }
    mODoHTargetHost = targetHost;
    mODoHTargetPath = targetPath;

    BuildODoHRequestURI();
  }

  if (updateODoHConfig) {
    // When this function is called from ODoHService::Init(), it's on the same
    // call stack as nsDNSService is inited. In this case, we need to dispatch
    // UpdateODoHConfigFromHTTPSRR(), since recursively getting DNS service is
    // not allowed.
    auto task = []() { gODoHService->UpdateODoHConfigFromHTTPSRR(); };
    if (aInit) {
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "ODoHService::UpdateODoHConfigFromHTTPSRR", std::move(task)));
    } else {
      task();
    }
  }
}

static nsresult ExtractHost(const nsACString& aURI, nsCString& aResult) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!uri->SchemeIs("https")) {
    LOG(("ODoHService host uri is not https"));
    return NS_ERROR_FAILURE;
  }

  return uri->GetAsciiHost(aResult);
}

void ODoHService::BuildODoHRequestURI() {
  mLock.AssertCurrentThreadOwns();

  mODoHRequestURI.Truncate();
  if (mODoHTargetHost.IsEmpty() || mODoHTargetPath.IsEmpty()) {
    return;
  }

  if (mODoHProxyURI.IsEmpty()) {
    mODoHRequestURI.Append(mODoHTargetHost);
    mODoHRequestURI.AppendLiteral("/");
    mODoHRequestURI.Append(mODoHTargetPath);
  } else {
    nsAutoCString hostStr;
    if (NS_FAILED(ExtractHost(mODoHTargetHost, hostStr))) {
      return;
    }

    mODoHRequestURI.Append(mODoHProxyURI);
    mODoHRequestURI.AppendLiteral("?targethost=");
    mODoHRequestURI.Append(hostStr);
    mODoHRequestURI.AppendLiteral("&targetpath=/");
    mODoHRequestURI.Append(mODoHTargetPath);
  }
}

void ODoHService::GetRequestURI(nsACString& aResult) {
  MutexAutoLock lock(mLock);
  aResult = mODoHRequestURI;
}

nsresult ODoHService::UpdateODoHConfig() {
  LOG(("ODoHService::UpdateODoHConfig"));
  if (mQueryODoHConfigInProgress) {
    return NS_OK;
  }

  if (NS_SUCCEEDED(UpdateODoHConfigFromURI())) {
    return NS_OK;
  }

  return UpdateODoHConfigFromHTTPSRR();
}

nsresult ODoHService::UpdateODoHConfigFromURI() {
  LOG(("ODoHService::UpdateODoHConfigFromURI"));

  nsAutoCString configUri;
  {
    MutexAutoLock lock(mLock);
    configUri = mODoHConfigsUri;
  }

  if (configUri.IsEmpty() || !StringBeginsWith(configUri, "https://"_ns)) {
    LOG(("ODoHService::UpdateODoHConfigFromURI: uri is invalid"));
    return UpdateODoHConfigFromHTTPSRR();
  }

  nsCOMPtr<nsIEventTarget> target = gTRRService->MainThreadOrTRRThread();
  if (!target) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!target->IsOnCurrentThread()) {
    nsresult rv = target->Dispatch(NS_NewRunnableFunction(
        "ODoHService::UpdateODoHConfigFromURI",
        []() { gODoHService->UpdateODoHConfigFromURI(); }));
    if (NS_SUCCEEDED(rv)) {
      // Set mQueryODoHConfigInProgress to true to avoid updating ODoHConfigs
      // when waiting the runnable to be executed.
      mQueryODoHConfigInProgress = true;
    }
    return rv;
  }

  // In case any error happens, we should reset mQueryODoHConfigInProgress.
  auto guard = MakeScopeExit([&]() { mQueryODoHConfigInProgress = false; });

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), configUri);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = DNSUtils::CreateChannelHelper(uri, getter_AddRefs(channel));
  if (NS_FAILED(rv) || !channel) {
    LOG(("NewChannel failed!"));
    return rv;
  }

  channel->SetLoadFlags(
      nsIRequest::LOAD_ANONYMOUS | nsIRequest::INHIBIT_CACHING |
      nsIRequest::LOAD_BYPASS_CACHE | nsIChannel::LOAD_BYPASS_URL_CLASSIFIER);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  if (!httpChannel) {
    return NS_ERROR_UNEXPECTED;
  }

  // This connection should not use TRR
  rv = httpChannel->SetTRRMode(nsIRequest::TRR_DISABLED_MODE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = httpChannel->AsyncOpen(loader);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // AsyncOpen succeeded, dismiss the guard.
  guard.release();
  mLoader.swap(loader);
  return rv;
}

nsresult ODoHService::UpdateODoHConfigFromHTTPSRR() {
  LOG(("ODoHService::UpdateODoHConfigFromHTTPSRR"));

  nsAutoCString uri;
  {
    MutexAutoLock lock(mLock);
    uri = mODoHTargetHost;
  }

  nsCOMPtr<nsIDNSService> dns(
      do_GetService("@mozilla.org/network/dns-service;1"));
  if (!dns) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!gTRRService) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString hostStr;
  nsresult rv = ExtractHost(uri, hostStr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsICancelable> tmpOutstanding;
  nsCOMPtr<nsIEventTarget> target = gTRRService->MainThreadOrTRRThread();
  // We'd like to bypass the DNS cache, since ODoHConfigs will be updated
  // manually by ODoHService.
  uint32_t flags =
      nsIDNSService::RESOLVE_DISABLE_ODOH | nsIDNSService::RESOLVE_BYPASS_CACHE;
  rv = dns->AsyncResolveNative(hostStr, nsIDNSService::RESOLVE_TYPE_HTTPSSVC,
                               flags, nullptr, this, target, OriginAttributes(),
                               getter_AddRefs(tmpOutstanding));
  LOG(("ODoHService::UpdateODoHConfig [host=%s rv=%" PRIx32 "]", hostStr.get(),
       static_cast<uint32_t>(rv)));

  if (NS_SUCCEEDED(rv)) {
    mQueryODoHConfigInProgress = true;
  }
  return rv;
}

void ODoHService::StartTTLTimer(uint32_t aTTL) {
  if (mTTLTimer) {
    mTTLTimer->Cancel();
    mTTLTimer = nullptr;
  }
  LOG(("ODoHService::StartTTLTimer ttl=%d(s)", aTTL));
  NS_NewTimerWithCallback(getter_AddRefs(mTTLTimer), this, aTTL * 1000,
                          nsITimer::TYPE_ONE_SHOT);
}

NS_IMETHODIMP
ODoHService::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(aTimer == mTTLTimer);
  UpdateODoHConfig();
  return NS_OK;
}

void ODoHService::ODoHConfigUpdateDone(uint32_t aTTL,
                                       Span<const uint8_t> aRawConfig) {
  MOZ_ASSERT_IF(XRE_IsParentProcess() && gTRRService,
                NS_IsMainThread() || gTRRService->IsOnTRRThread());
  MOZ_ASSERT_IF(XRE_IsSocketProcess(), NS_IsMainThread());

  mQueryODoHConfigInProgress = false;
  mODoHConfigs.reset();

  nsTArray<ObliviousDoHConfig> configs;
  if (ODoHDNSPacket::ParseODoHConfigs(aRawConfig, configs)) {
    mODoHConfigs.emplace(std::move(configs));
  }

  // Let observers know whether ODoHService is activated or not.
  bool hasODoHConfigs = mODoHConfigs && !mODoHConfigs->IsEmpty();
  if (aTTL < StaticPrefs::network_trr_odoh_min_ttl()) {
    aTTL = StaticPrefs::network_trr_odoh_min_ttl();
  }
  auto task = [hasODoHConfigs, aTTL]() {
    MOZ_ASSERT(NS_IsMainThread());
    if (XRE_IsSocketProcess()) {
      SocketProcessChild::GetSingleton()->SendODoHServiceActivated(
          hasODoHConfigs);
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    if (observerService) {
      observerService->NotifyObservers(nullptr, "odoh-service-activated",
                                       hasODoHConfigs ? u"true" : u"false");
    }

    if (aTTL) {
      gODoHService->StartTTLTimer(aTTL);
    }
  };

  if (NS_IsMainThread()) {
    task();
  } else {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("ODoHService::Activated", std::move(task)));
  }

  if (!mPendingRequests.IsEmpty()) {
    nsTArray<RefPtr<ODoH>> requests = std::move(mPendingRequests);
    nsCOMPtr<nsIEventTarget> target = gTRRService->MainThreadOrTRRThread();
    for (auto& query : requests) {
      target->Dispatch(query.forget());
    }
  }
}

NS_IMETHODIMP
ODoHService::OnLookupComplete(nsICancelable* aRequest, nsIDNSRecord* aRec,
                              nsresult aStatus) {
  MOZ_ASSERT_IF(XRE_IsParentProcess() && gTRRService,
                NS_IsMainThread() || gTRRService->IsOnTRRThread());
  MOZ_ASSERT_IF(XRE_IsSocketProcess(), NS_IsMainThread());

  nsCOMPtr<nsIDNSHTTPSSVCRecord> httpsRecord;
  nsCString rawODoHConfig;
  auto notifyDone = MakeScopeExit([&]() {
    uint32_t ttl = 0;
    if (httpsRecord) {
      Unused << httpsRecord->GetTtl(&ttl);
    }

    ODoHConfigUpdateDone(
        ttl,
        Span(reinterpret_cast<const uint8_t*>(rawODoHConfig.BeginReading()),
             rawODoHConfig.Length()));
  });

  LOG(("ODoHService::OnLookupComplete [aStatus=%" PRIx32 "]",
       static_cast<uint32_t>(aStatus)));
  if (NS_FAILED(aStatus)) {
    return NS_OK;
  }

  httpsRecord = do_QueryInterface(aRec);
  if (!httpsRecord) {
    return NS_OK;
  }

  nsTArray<RefPtr<nsISVCBRecord>> svcbRecords;
  httpsRecord->GetRecords(svcbRecords);
  for (const auto& record : svcbRecords) {
    Unused << record->GetODoHConfig(rawODoHConfig);
    if (!rawODoHConfig.IsEmpty()) {
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
ODoHService::OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                              nsresult aStatus, uint32_t aLength,
                              const uint8_t* aContent) {
  MOZ_ASSERT_IF(XRE_IsParentProcess() && gTRRService,
                NS_IsMainThread() || gTRRService->IsOnTRRThread());
  MOZ_ASSERT_IF(XRE_IsSocketProcess(), NS_IsMainThread());
  LOG(("ODoHService::OnStreamComplete aLength=%d\n", aLength));

  {
    MutexAutoLock lock(mLock);
    mLoader = nullptr;
  }
  ODoHConfigUpdateDone(0, Span(aContent, aLength));
  return NS_OK;
}

const Maybe<nsTArray<ObliviousDoHConfig>>& ODoHService::ODoHConfigs() {
  MOZ_ASSERT_IF(XRE_IsParentProcess() && gTRRService,
                NS_IsMainThread() || gTRRService->IsOnTRRThread());
  MOZ_ASSERT_IF(XRE_IsSocketProcess(), NS_IsMainThread());

  return mODoHConfigs;
}

void ODoHService::AppendPendingODoHRequest(ODoH* aRequest) {
  LOG(("ODoHService::AppendPendingODoHQuery\n"));
  MOZ_ASSERT_IF(XRE_IsParentProcess() && gTRRService,
                NS_IsMainThread() || gTRRService->IsOnTRRThread());
  MOZ_ASSERT_IF(XRE_IsSocketProcess(), NS_IsMainThread());

  mPendingRequests.AppendElement(aRequest);
}

bool ODoHService::RemovePendingODoHRequest(ODoH* aRequest) {
  MOZ_ASSERT_IF(XRE_IsParentProcess() && gTRRService,
                NS_IsMainThread() || gTRRService->IsOnTRRThread());
  MOZ_ASSERT_IF(XRE_IsSocketProcess(), NS_IsMainThread());

  return mPendingRequests.RemoveElement(aRequest);
}

}  // namespace net
}  // namespace mozilla
