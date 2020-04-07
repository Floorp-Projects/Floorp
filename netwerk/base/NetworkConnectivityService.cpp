/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkConnectivityService.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "xpcpublic.h"
#include "nsSocketTransport2.h"
#include "nsINetworkLinkService.h"

static LazyLogModule gNCSLog("NetworkConnectivityService");
#undef LOG
#define LOG(args) MOZ_LOG(gNCSLog, mozilla::LogLevel::Debug, args)

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(NetworkConnectivityService, nsIDNSListener, nsIObserver,
                  nsINetworkConnectivityService, nsIStreamListener)

static StaticRefPtr<NetworkConnectivityService> gConnService;

// static
already_AddRefed<NetworkConnectivityService>
NetworkConnectivityService::GetSingleton() {
  if (gConnService) {
    return do_AddRef(gConnService);
  }

  RefPtr<NetworkConnectivityService> service = new NetworkConnectivityService();
  service->Init();

  gConnService = std::move(service);
  ClearOnShutdown(&gConnService);
  return do_AddRef(gConnService);
}

nsresult NetworkConnectivityService::Init() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  observerService->AddObserver(this, NS_NETWORK_LINK_TOPIC, false);
  observerService->AddObserver(this, "network:captive-portal-connectivity",
                               false);

  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::GetDNSv4(ConnectivityState* aState) {
  NS_ENSURE_ARG(aState);
  *aState = mDNSv4;
  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::GetDNSv6(ConnectivityState* aState) {
  NS_ENSURE_ARG(aState);
  *aState = mDNSv6;
  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::GetIPv4(ConnectivityState* aState) {
  NS_ENSURE_ARG(aState);
  *aState = mIPv4;
  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::GetIPv6(ConnectivityState* aState) {
  NS_ENSURE_ARG(aState);
  *aState = mIPv6;
  return NS_OK;
}

void NetworkConnectivityService::PerformChecks() {
  mDNSv4 = UNKNOWN;
  mDNSv6 = UNKNOWN;

  mIPv4 = UNKNOWN;
  mIPv6 = UNKNOWN;

  RecheckDNS();
  RecheckIPConnectivity();
}

static inline void NotifyObservers(const char* aTopic) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->NotifyObservers(nullptr, aTopic, nullptr);
}

NS_IMETHODIMP
NetworkConnectivityService::OnLookupComplete(nsICancelable* aRequest,
                                             nsIDNSRecord* aRecord,
                                             nsresult aStatus) {
  ConnectivityState state = aRecord ? OK : NOT_AVAILABLE;

  if (aRequest == mDNSv4Request) {
    mDNSv4 = state;
    mDNSv4Request = nullptr;
  } else if (aRequest == mDNSv6Request) {
    mDNSv6 = state;
    mDNSv6Request = nullptr;
  }

  if (!mDNSv4Request && !mDNSv6Request) {
    NotifyObservers("network:connectivity-service:dns-checks-complete");
  }
  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::RecheckDNS() {
  bool enabled =
      Preferences::GetBool("network.connectivity-service.enabled", false);
  if (!enabled) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  OriginAttributes attrs;
  nsAutoCString host;
  Preferences::GetCString("network.connectivity-service.DNSv4.domain", host);

  rv = dns->AsyncResolveNative(
      host,
      nsIDNSService::RESOLVE_DISABLE_IPV6 | nsIDNSService::RESOLVE_DISABLE_TRR,
      this, NS_GetCurrentThread(), attrs, getter_AddRefs(mDNSv4Request));
  NS_ENSURE_SUCCESS(rv, rv);

  Preferences::GetCString("network.connectivity-service.DNSv6.domain", host);
  rv = dns->AsyncResolveNative(
      host,
      nsIDNSService::RESOLVE_DISABLE_IPV4 | nsIDNSService::RESOLVE_DISABLE_TRR,
      this, NS_GetCurrentThread(), attrs, getter_AddRefs(mDNSv6Request));
  return rv;
}

NS_IMETHODIMP
NetworkConnectivityService::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData) {
  if (!strcmp(aTopic, "network:captive-portal-connectivity")) {
    // Captive portal is cleared, so we redo the checks.
    PerformChecks();
  } else if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    if (mDNSv4Request) {
      mDNSv4Request->Cancel(NS_ERROR_ABORT);
      mDNSv4Request = nullptr;
    }
    if (mDNSv6Request) {
      mDNSv6Request->Cancel(NS_ERROR_ABORT);
      mDNSv6Request = nullptr;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    observerService->RemoveObserver(this,
                                    "network:captive-portal-connectivity");
    observerService->RemoveObserver(this, NS_NETWORK_LINK_TOPIC);
  } else if (!strcmp(aTopic, NS_NETWORK_LINK_TOPIC) &&
             !NS_LITERAL_STRING(NS_NETWORK_LINK_DATA_UNKNOWN).Equals(aData)) {
    PerformChecks();
  }

  return NS_OK;
}

static inline already_AddRefed<nsIChannel> SetupIPCheckChannel(bool ipv4) {
  nsresult rv;
  nsAutoCString url;

  if (ipv4) {
    rv = Preferences::GetCString("network.connectivity-service.IPv4.url", url);
  } else {
    rv = Preferences::GetCString("network.connectivity-service.IPv6.url", url);
  }
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), url);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(
      getter_AddRefs(channel), uri, nsContentUtils::GetSystemPrincipal(),
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      nsIContentPolicy::TYPE_OTHER,
      nullptr,  // nsICookieJarSettings
      nullptr,  // aPerformanceStorage
      nullptr,  // aLoadGroup
      nullptr,
      nsIRequest::LOAD_BYPASS_CACHE |    // don't read from the cache
          nsIRequest::INHIBIT_CACHING |  // don't write the response to cache
          nsIRequest::LOAD_ANONYMOUS);   // prevent privacy leaks

  channel->SetTRRMode(nsIRequest::TRR_DISABLED_MODE);

  {
    // Prevent HTTPS-Only Mode from upgrading the OCSP request.
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    uint32_t httpsOnlyStatus = loadInfo->GetHttpsOnlyStatus();
    httpsOnlyStatus |= nsILoadInfo::HTTPS_ONLY_EXEMPT;
    loadInfo->SetHttpsOnlyStatus(httpsOnlyStatus);
  }

  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIHttpChannelInternal> internalChan = do_QueryInterface(channel);
  NS_ENSURE_TRUE(internalChan, nullptr);

  if (ipv4) {
    internalChan->SetIPv6Disabled();
  } else {
    internalChan->SetIPv4Disabled();
  }

  return channel.forget();
}

NS_IMETHODIMP
NetworkConnectivityService::RecheckIPConnectivity() {
  bool enabled =
      Preferences::GetBool("network.connectivity-service.enabled", false);
  if (!enabled) {
    return NS_OK;
  }

  if (xpc::AreNonLocalConnectionsDisabled() &&
      !Preferences::GetBool("network.captive-portal-service.testMode", false)) {
    return NS_OK;
  }

  if (mIPv4Channel) {
    mIPv4Channel->Cancel(NS_ERROR_ABORT);
    mIPv4Channel = nullptr;
  }
  if (mIPv6Channel) {
    mIPv6Channel->Cancel(NS_ERROR_ABORT);
    mIPv6Channel = nullptr;
  }

  nsresult rv;
  mHasNetworkId = false;
  mCheckedNetworkId = false;
  mIPv4Channel = SetupIPCheckChannel(/* ipv4 = */ true);
  if (mIPv4Channel) {
    rv = mIPv4Channel->AsyncOpen(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mIPv6Channel = SetupIPCheckChannel(/* ipv4 = */ false);
  if (mIPv6Channel) {
    rv = mIPv6Channel->AsyncOpen(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::OnStartRequest(nsIRequest* aRequest) {
  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::OnStopRequest(nsIRequest* aRequest,
                                          nsresult aStatusCode) {
  if (aStatusCode == NS_ERROR_ABORT) {
    return NS_OK;
  }

  ConnectivityState status = NS_FAILED(aStatusCode) ? NOT_AVAILABLE : OK;

  if (aRequest == mIPv4Channel) {
    mIPv4 = status;
    mIPv4Channel = nullptr;

    if (mIPv4 == nsINetworkConnectivityService::OK) {
      Telemetry::AccumulateCategorical(
          mHasNetworkId ? Telemetry::LABELS_NETWORK_ID_ONLINE::present
                        : Telemetry::LABELS_NETWORK_ID_ONLINE::absent);
      LOG(("mHasNetworkId : %d\n", mHasNetworkId));
    }
  } else if (aRequest == mIPv6Channel) {
    mIPv6 = status;
    mIPv6Channel = nullptr;
  }

  if (!mIPv6Channel && !mIPv4Channel) {
    NotifyObservers("network:connectivity-service:ip-checks-complete");
  }

  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::OnDataAvailable(nsIRequest* aRequest,
                                            nsIInputStream* aInputStream,
                                            uint64_t aOffset, uint32_t aCount) {
  nsAutoCString data;

  // We perform this check here, instead of doing it in OnStopRequest in case
  // a network down event occurs after the data has arrived but before we fire
  // OnStopRequest. That would cause us to report a missing networkID, even
  // though it was not empty while receiving data.
  if (aRequest == mIPv4Channel && !mCheckedNetworkId) {
    nsCOMPtr<nsINetworkLinkService> nls =
        do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID);
    nsAutoCString networkId;
    if (nls) {
      nls->GetNetworkID(networkId);
    }
    mHasNetworkId = !networkId.IsEmpty();
    mCheckedNetworkId = true;
  }

  Unused << NS_ReadInputStreamToString(aInputStream, data, aCount);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
