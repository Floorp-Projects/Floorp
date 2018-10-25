/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkConnectivityService.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "xpcpublic.h"
#include "nsSocketTransport2.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(NetworkConnectivityService, nsINetworkConnectivityService, nsIObserver, nsIDNSListener)

static StaticRefPtr<NetworkConnectivityService> gConnService;

// static
already_AddRefed<NetworkConnectivityService>
NetworkConnectivityService::GetSingleton()
{
  if (gConnService) {
    return do_AddRef(gConnService);
  }

  RefPtr<NetworkConnectivityService> service = new NetworkConnectivityService();
  service->Init();

  gConnService = service.forget();
  ClearOnShutdown(&gConnService);
  return do_AddRef(gConnService);
}

nsresult
NetworkConnectivityService::Init()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  observerService->AddObserver(this, "network:captive-portal-connectivity", false);

  // We need to schedule this for a bit later, to avoid a recursive service
  // initialization.
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(
    NewRunnableMethod("NetworkConnectivityService::PerformChecks",
                      this,
                      &NetworkConnectivityService::PerformChecks)));
  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::GetDNSv4(int32_t *aState)
{
  NS_ENSURE_ARG(aState);
  *aState = mDNSv4;
  return NS_OK;
}


NS_IMETHODIMP
NetworkConnectivityService::GetDNSv6(int32_t *aState)
{
  NS_ENSURE_ARG(aState);
  *aState = mDNSv6;
  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::GetIPv4(int32_t *aState)
{
  NS_ENSURE_ARG(aState);
  *aState = nsSocketTransport::HasIPv4Connectivity()
              ? nsINetworkConnectivityService::OK
              : nsINetworkConnectivityService::NOT_AVAILABLE;
  return NS_OK;
}


NS_IMETHODIMP
NetworkConnectivityService::GetIPv6(int32_t *aState)
{
  NS_ENSURE_ARG(aState);
  *aState = nsSocketTransport::HasIPv6Connectivity()
              ? nsINetworkConnectivityService::OK
              : nsINetworkConnectivityService::NOT_AVAILABLE;
  return NS_OK;
}

void
NetworkConnectivityService::PerformChecks()
{
  RecheckDNS();
}

NS_IMETHODIMP
NetworkConnectivityService::OnLookupComplete(nsICancelable *aRequest,
                                             nsIDNSRecord *aRecord,
                                             nsresult aStatus)
{
  int32_t state = aRecord ? nsINetworkConnectivityService::OK
                          : nsINetworkConnectivityService::NOT_AVAILABLE;

  if (aRequest == mDNSv4Request) {
    mDNSv4 = state;
    mDNSv4Request = nullptr;
  } else if (aRequest == mDNSv6Request) {
    mDNSv6 = state;
    mDNSv6Request = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
NetworkConnectivityService::OnLookupByTypeComplete(nsICancelable *aRequest,
                                                   nsIDNSByTypeRecord *aRes,
                                                   nsresult aStatus)
{
    return NS_OK;
}


NS_IMETHODIMP
NetworkConnectivityService::RecheckDNS()
{
  bool enabled = Preferences::GetBool("network.connectivity-service.enabled", false);
  if (!enabled) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  OriginAttributes attrs;
  nsAutoCString host;
  Preferences::GetCString("network.connectivity-service.DNSv4.domain", host);

  rv = dns->AsyncResolveNative(host, nsIDNSService::RESOLVE_DISABLE_IPV6,
                               this, NS_GetCurrentThread(),
                               attrs, getter_AddRefs(mDNSv4Request));
  NS_ENSURE_SUCCESS(rv, rv);

  Preferences::GetCString("network.connectivity-service.DNSv6.domain", host);
  rv = dns->AsyncResolveNative(host, nsIDNSService::RESOLVE_DISABLE_IPV4,
                               this, NS_GetCurrentThread(),
                               attrs, getter_AddRefs(mDNSv6Request));
  return rv;
}

NS_IMETHODIMP
NetworkConnectivityService::Observe(nsISupports *aSubject,
                                    const char * aTopic,
                                    const char16_t * aData)
{
  if (!strcmp(aTopic, "network:captive-portal-connectivity")) {
    // Captive portal is cleared, so we redo the checks.
    mDNSv4 = nsINetworkConnectivityService::UNKNOWN;
    mDNSv6 = nsINetworkConnectivityService::UNKNOWN;

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
    observerService->RemoveObserver(this, "network:captive-portal-connectivity");
  }

  return NS_OK;
}

} // namespace net
} // namespace mozilla
