/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNSUtils.h"
#include "NetworkConnectivityService.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIOService.h"
#include "nsICancelable.h"
#include "xpcpublic.h"
#include "nsSocketTransport2.h"
#include "nsIHttpChannelInternal.h"
#include "nsINetworkLinkService.h"
#include "mozilla/StaticPrefs_network.h"

static mozilla::LazyLogModule gNCSLog("NetworkConnectivityService");
#undef LOG
#define LOG(args) MOZ_LOG(gNCSLog, mozilla::LogLevel::Debug, args)

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(NetworkConnectivityService, nsIDNSListener, nsIObserver,
                  nsINetworkConnectivityService, nsIStreamListener)

static StaticRefPtr<NetworkConnectivityService> gConnService;

NetworkConnectivityService::NetworkConnectivityService()
    : mDNSv4(UNKNOWN),
      mDNSv6(UNKNOWN),
      mIPv4(UNKNOWN),
      mIPv6(UNKNOWN),
      mNAT64(UNKNOWN),
      mLock("nat64prefixes") {}

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

NS_IMETHODIMP
NetworkConnectivityService::GetNAT64(ConnectivityState* aState) {
  NS_ENSURE_ARG(aState);
  *aState = mNAT64;
  return NS_OK;
}

already_AddRefed<AddrInfo> NetworkConnectivityService::MapNAT64IPs(
    AddrInfo* aNewRRSet) {
  // Add prefixes only if there are no IPv6 addresses.
  // Expect that if aNewRRSet has IPv6 addresses, they must come
  // before IPv4 addresses.
  if (aNewRRSet->Addresses().IsEmpty() ||
      aNewRRSet->Addresses()[0].raw.family == PR_AF_INET6) {
    return do_AddRef(aNewRRSet);
  }

  // Currently we only add prefixes to the first IP's clones.
  uint32_t ip = aNewRRSet->Addresses()[0].inet.ip;
  nsTArray<NetAddr> addresses = aNewRRSet->Addresses().Clone();

  {
    MutexAutoLock lock(mLock);
    for (const auto& prefix : mNAT64Prefixes) {
      NetAddr addr = NetAddr(prefix);

      // Copy the IPv4 address to the end
      addr.inet6.ip.u32[3] = ip;

      // If we have both IPv4 and NAT64, we be could insourcing NAT64
      // to avoid double NAT and improve performance. However, this
      // breaks WebRTC, so we push it to the back.
      addresses.AppendElement(addr);
    }
  }

  auto builder = aNewRRSet->Build();
  builder.SetAddresses(std::move(addresses));
  return builder.Finish();
}

// Returns true if a prefix was read and saved to the argument
static inline bool NAT64PrefixFromPref(NetAddr* prefix) {
  nsAutoCString nat64PrefixPref;

  nsresult rv = Preferences::GetCString(
      "network.connectivity-service.nat64-prefix", nat64PrefixPref);
  return !(NS_FAILED(rv) || nat64PrefixPref.IsEmpty() ||
           NS_FAILED(prefix->InitFromString(nat64PrefixPref)) ||
           prefix->raw.family != PR_AF_INET6);
}

static inline bool NAT64PrefixCompare(const NetAddr& prefix1,
                                      const NetAddr& prefix2) {
  // Compare the first 96 bits as 64 + 32
  return prefix1.inet6.ip.u64[0] == prefix2.inet6.ip.u64[0] &&
         prefix1.inet6.ip.u32[2] == prefix2.inet6.ip.u32[2];
}

void NetworkConnectivityService::PerformChecks() {
  mDNSv4 = UNKNOWN;
  mDNSv6 = UNKNOWN;

  mIPv4 = UNKNOWN;
  mIPv6 = UNKNOWN;

  mNAT64 = UNKNOWN;

  {
    MutexAutoLock lock(mLock);
    mNAT64Prefixes.Clear();

    // NAT64 checks might be disabled.
    // Since We can't guarantee a DNS response, we should set up
    // NAT64 manually now if needed.

    NetAddr priorityPrefix{};
    bool havePrefix = NAT64PrefixFromPref(&priorityPrefix);
    if (havePrefix) {
      mNAT64Prefixes.AppendElement(priorityPrefix);
      mNAT64 = OK;
    }
  }

  RecheckDNS();
  RecheckIPConnectivity();
}

static inline void NotifyObservers(const char* aTopic) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->NotifyObservers(nullptr, aTopic, nullptr);
}

void NetworkConnectivityService::SaveNAT64Prefixes(nsIDNSRecord* aRecord) {
  nsCOMPtr<nsIDNSAddrRecord> rec = do_QueryInterface(aRecord);
  MutexAutoLock lock(mLock);
  mNAT64Prefixes.Clear();

  NetAddr priorityPrefix{};
  bool havePrefix = NAT64PrefixFromPref(&priorityPrefix);
  if (havePrefix) {
    mNAT64 = OK;
    mNAT64Prefixes.AppendElement(priorityPrefix);
  }

  if (!rec) {
    if (!havePrefix) {
      mNAT64 = NOT_AVAILABLE;
    }
    return;
  }

  mNAT64 = UNKNOWN;
  NetAddr addr{};

  // use port 80 as dummy value for NetAddr
  while (NS_SUCCEEDED(rec->GetNextAddr(80, &addr))) {
    if (addr.raw.family != AF_INET6 || addr.IsIPAddrV4Mapped()) {
      // These are not the kind of addresses we are looking for.
      continue;
    }

    // RFC 7050 does not require the embedded IPv4 to be
    // at the end of IPv6. In practice, and as we assume,
    // it is always at the end.
    // The embedded IP must be 192.0.0.170 or 192.0.0.171

    // Clear the last bit to compare with the next one.
    addr.inet6.ip.u8[15] &= ~(uint32_t)1;
    if ((addr.inet6.ip.u8[12] != 192) || (addr.inet6.ip.u8[13] != 0) ||
        (addr.inet6.ip.u8[14] != 0) || (addr.inet6.ip.u8[15] != 170)) {
      continue;
    }

    mNAT64Prefixes.AppendElement(addr);
  }

  size_t length = mNAT64Prefixes.Length();
  if (length == 0) {
    mNAT64 = NOT_AVAILABLE;
    return;
  }

  // Remove duplicates. Typically a DNS64 resolver sends every
  // prefix twice with address with different last bits. We want
  // a list of unique prefixes while reordering is not allowed.
  // We must not handle the case with an element in-between
  // two identical ones, which is never the case for a properly
  // configured DNS64 resolver.

  NetAddr prev = mNAT64Prefixes[0];

  for (size_t i = 1; i < length; i++) {
    if (NAT64PrefixCompare(prev, mNAT64Prefixes[i])) {
      mNAT64Prefixes.RemoveElementAt(i);
      i--;
      length--;
    } else {
      prev = mNAT64Prefixes[i];
    }
  }

  // The prioritized address might also appear in the record we received.

  if (havePrefix) {
    for (size_t i = 1; i < length; i++) {
      if (NAT64PrefixCompare(priorityPrefix, mNAT64Prefixes[i])) {
        mNAT64Prefixes.RemoveElementAt(i);
        // It wouldn't appear more than once.
        break;
      }
    }
  }

  mNAT64 = OK;
}

NS_IMETHODIMP
NetworkConnectivityService::OnLookupComplete(nsICancelable* aRequest,
                                             nsIDNSRecord* aRecord,
                                             nsresult aStatus) {
  ConnectivityState state = NS_SUCCEEDED(aStatus) ? OK : NOT_AVAILABLE;

  if (aRequest == mDNSv4Request) {
    mDNSv4 = state;
    mDNSv4Request = nullptr;
  } else if (aRequest == mDNSv6Request) {
    mDNSv6 = state;
    mDNSv6Request = nullptr;
  } else if (aRequest == mNAT64Request) {
    mNAT64Request = nullptr;
    SaveNAT64Prefixes(aRecord);
  }

  if (!mDNSv4Request && !mDNSv6Request && !mNAT64Request) {
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

  if (nsIOService::UseSocketProcess()) {
    SocketProcessParent* parent = SocketProcessParent::GetSingleton();
    if (parent) {
      Unused << parent->SendRecheckDNS();
    }
  }

  nsresult rv;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  OriginAttributes attrs;
  nsAutoCString host;
  Preferences::GetCString("network.connectivity-service.DNSv4.domain", host);

  rv = dns->AsyncResolveNative(host, nsIDNSService::RESOLVE_TYPE_DEFAULT,
                               nsIDNSService::RESOLVE_DISABLE_IPV6 |
                                   nsIDNSService::RESOLVE_TRR_DISABLED_MODE,
                               nullptr, this, NS_GetCurrentThread(), attrs,
                               getter_AddRefs(mDNSv4Request));
  NS_ENSURE_SUCCESS(rv, rv);

  Preferences::GetCString("network.connectivity-service.DNSv6.domain", host);
  rv = dns->AsyncResolveNative(host, nsIDNSService::RESOLVE_TYPE_DEFAULT,
                               nsIDNSService::RESOLVE_DISABLE_IPV4 |
                                   nsIDNSService::RESOLVE_TRR_DISABLED_MODE,
                               nullptr, this, NS_GetCurrentThread(), attrs,
                               getter_AddRefs(mDNSv6Request));
  NS_ENSURE_SUCCESS(rv, rv);

  if (StaticPrefs::network_connectivity_service_nat64_check()) {
    rv = dns->AsyncResolveNative("ipv4only.arpa"_ns,
                                 nsIDNSService::RESOLVE_TYPE_DEFAULT,
                                 nsIDNSService::RESOLVE_DISABLE_IPV4 |
                                     nsIDNSService::RESOLVE_TRR_DISABLED_MODE,
                                 nullptr, this, NS_GetCurrentThread(), attrs,
                                 getter_AddRefs(mNAT64Request));
    NS_ENSURE_SUCCESS(rv, rv);
  }
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
    if (mNAT64Request) {
      mNAT64Request->Cancel(NS_ERROR_ABORT);
      mNAT64Request = nullptr;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    observerService->RemoveObserver(this,
                                    "network:captive-portal-connectivity");
    observerService->RemoveObserver(this, NS_NETWORK_LINK_TOPIC);
  } else if (!strcmp(aTopic, NS_NETWORK_LINK_TOPIC) &&
             !NS_LITERAL_STRING_FROM_CSTRING(NS_NETWORK_LINK_DATA_UNKNOWN)
                  .Equals(aData)) {
    PerformChecks();
  }

  return NS_OK;
}

already_AddRefed<nsIChannel> NetworkConnectivityService::SetupIPCheckChannel(
    bool ipv4) {
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
  if (XRE_IsSocketProcess()) {
    rv = DNSUtils::CreateChannelHelper(uri, getter_AddRefs(channel));
    if (NS_FAILED(rv)) {
      return nullptr;
    }
    channel->SetLoadFlags(
        nsIRequest::LOAD_BYPASS_CACHE |  // don't read from the cache
        nsIRequest::INHIBIT_CACHING |    // don't write the response to cache
        nsIRequest::LOAD_ANONYMOUS);
  } else {
    rv = NS_NewChannel(
        getter_AddRefs(channel), uri, nsContentUtils::GetSystemPrincipal(),
        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
        nsIContentPolicy::TYPE_OTHER,
        nullptr,  // nsICookieJarSettings
        nullptr,  // aPerformanceStorage
        nullptr,  // aLoadGroup
        nullptr,
        nsIRequest::LOAD_BYPASS_CACHE |    // don't read from the cache
            nsIRequest::INHIBIT_CACHING |  // don't write the response to cache
            nsIRequest::LOAD_ANONYMOUS);   // prevent privacy leaks
    NS_ENSURE_SUCCESS(rv, nullptr);

    {
      // Prevent HTTPS-Only Mode from upgrading the OCSP request.
      nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
      uint32_t httpsOnlyStatus = loadInfo->GetHttpsOnlyStatus();
      httpsOnlyStatus |= nsILoadInfo::HTTPS_ONLY_EXEMPT;
      loadInfo->SetHttpsOnlyStatus(httpsOnlyStatus);

      // allow deprecated HTTP request from SystemPrincipal
      loadInfo->SetAllowDeprecatedSystemRequests(true);
    }
  }

  rv = channel->SetTRRMode(nsIRequest::TRR_DISABLED_MODE);
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

  if (nsIOService::UseSocketProcess()) {
    SocketProcessParent* parent = SocketProcessParent::GetSingleton();
    if (parent) {
      Unused << parent->SendRecheckIPConnectivity();
    }
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
