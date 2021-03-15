/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAndroidNetworkLinkService.h"
#include "nsServiceManagerUtils.h"

#include "nsIObserverService.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Services.h"
#include "mozilla/Logging.h"

#include "AndroidBridge.h"
#include "mozilla/java/GeckoAppShellWrappers.h"

namespace java = mozilla::java;

static mozilla::LazyLogModule gNotifyAddrLog("nsAndroidNetworkLinkService");
#define LOG(args) MOZ_LOG(gNotifyAddrLog, mozilla::LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(nsAndroidNetworkLinkService, nsINetworkLinkService,
                  nsIObserver)

nsAndroidNetworkLinkService::nsAndroidNetworkLinkService()
    : mStatusIsKnown(false) {}

nsresult nsAndroidNetworkLinkService::Init() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  rv = observerService->AddObserver(this, "xpcom-shutdown-threads", false);
  NS_ENSURE_SUCCESS(rv, rv);

  mNetlinkSvc = new mozilla::net::NetlinkService();
  rv = mNetlinkSvc->Init(this);
  if (NS_FAILED(rv)) {
    mNetlinkSvc = nullptr;
    LOG(("Cannot initialize NetlinkService [rv=0x%08" PRIx32 "]",
         static_cast<uint32_t>(rv)));
    return rv;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsAndroidNetworkLinkService::Shutdown() {
  // remove xpcom shutdown observer
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService)
    observerService->RemoveObserver(this, "xpcom-shutdown-threads");

  if (mNetlinkSvc) {
    mNetlinkSvc->Shutdown();
    mNetlinkSvc = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::Observe(nsISupports* subject, const char* topic,
                                     const char16_t* data) {
  if (!strcmp("xpcom-shutdown-threads", topic)) {
    Shutdown();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::GetIsLinkUp(bool* aIsUp) {
  if (mNetlinkSvc && mStatusIsKnown) {
    mNetlinkSvc->GetIsLinkUp(aIsUp);
    return NS_OK;
  }

  if (!mozilla::AndroidBridge::Bridge()) {
    // Fail soft here and assume a connection exists
    NS_WARNING("GetIsLinkUp is not supported without a bridge connection");
    *aIsUp = true;
    return NS_OK;
  }

  *aIsUp = java::GeckoAppShell::IsNetworkLinkUp();
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::GetLinkStatusKnown(bool* aIsKnown) {
  if (mStatusIsKnown) {
    *aIsKnown = true;
    return NS_OK;
  }

  NS_ENSURE_TRUE(mozilla::AndroidBridge::Bridge(), NS_ERROR_NOT_IMPLEMENTED);

  *aIsKnown = java::GeckoAppShell::IsNetworkLinkKnown();
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::GetLinkType(uint32_t* aLinkType) {
  NS_ENSURE_ARG_POINTER(aLinkType);

  if (!mozilla::AndroidBridge::Bridge()) {
    // Fail soft here and assume a connection exists
    NS_WARNING("GetLinkType is not supported without a bridge connection");
    *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
    return NS_OK;
  }

  *aLinkType = java::GeckoAppShell::GetNetworkLinkType();
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::GetNetworkID(nsACString& aNetworkID) {
  if (!mNetlinkSvc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mNetlinkSvc->GetNetworkID(aNetworkID);
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::GetDnsSuffixList(
    nsTArray<nsCString>& aDnsSuffixList) {
  aDnsSuffixList.Clear();
  if (!mozilla::AndroidBridge::Bridge()) {
    NS_WARNING("GetDnsSuffixList is not supported without a bridge connection");
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto suffixList = java::GeckoAppShell::GetDNSDomains();
  if (!suffixList) {
    return NS_OK;
  }

  nsAutoCString list(suffixList->ToCString());
  for (const nsACString& suffix : list.Split(',')) {
    aDnsSuffixList.AppendElement(suffix);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::GetResolvers(nsTArray<RefPtr<nsINetAddr>>& aResolvers) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::GetNativeResolvers(
    nsTArray<mozilla::net::NetAddr>& aResolvers) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAndroidNetworkLinkService::GetPlatformDNSIndications(
    uint32_t* aPlatformDNSIndications) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsAndroidNetworkLinkService::OnNetworkChanged() {
  if (mozilla::StaticPrefs::network_notify_changed()) {
    RefPtr<nsAndroidNetworkLinkService> self = this;
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "nsAndroidNetworkLinkService::OnNetworkChanged", [self]() {
          self->NotifyObservers(NS_NETWORK_LINK_TOPIC,
                                NS_NETWORK_LINK_DATA_CHANGED);
        }));
  }
}

void nsAndroidNetworkLinkService::OnNetworkIDChanged() {
  RefPtr<nsAndroidNetworkLinkService> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsAndroidNetworkLinkService::OnNetworkIDChanged", [self]() {
        self->NotifyObservers(NS_NETWORK_ID_CHANGED_TOPIC, nullptr);
      }));
}

void nsAndroidNetworkLinkService::OnLinkUp() {
  RefPtr<nsAndroidNetworkLinkService> self = this;
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("nsAndroidNetworkLinkService::OnLinkUp", [self]() {
        self->NotifyObservers(NS_NETWORK_LINK_TOPIC, NS_NETWORK_LINK_DATA_UP);
      }));
}

void nsAndroidNetworkLinkService::OnLinkDown() {
  RefPtr<nsAndroidNetworkLinkService> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsAndroidNetworkLinkService::OnLinkDown", [self]() {
        self->NotifyObservers(NS_NETWORK_LINK_TOPIC, NS_NETWORK_LINK_DATA_DOWN);
      }));
}

void nsAndroidNetworkLinkService::OnLinkStatusKnown() { mStatusIsKnown = true; }

void nsAndroidNetworkLinkService::OnDnsSuffixListUpdated() {
  RefPtr<nsAndroidNetworkLinkService> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsAndroidNetworkLinkService::OnDnsSuffixListUpdated", [self]() {
        self->NotifyObservers(NS_DNS_SUFFIX_LIST_UPDATED_TOPIC, nullptr);
      }));
}

/* Sends the given event. Assumes aTopic/aData never goes out of scope (static
 * strings are ideal).
 */
void nsAndroidNetworkLinkService::NotifyObservers(const char* aTopic,
                                                  const char* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("nsAndroidNetworkLinkService::NotifyObservers: topic:%s data:%s\n",
       aTopic, aData ? aData : ""));

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  if (observerService) {
    observerService->NotifyObservers(
        static_cast<nsINetworkLinkService*>(this), aTopic,
        aData ? NS_ConvertASCIItoUTF16(aData).get() : nullptr);
  }
}
