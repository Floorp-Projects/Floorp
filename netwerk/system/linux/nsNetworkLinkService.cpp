/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set et sw=2 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsNetworkLinkService.h"
#include "nsString.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"

#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Services.h"

using namespace mozilla;

static LazyLogModule gNotifyAddrLog("nsNetworkLinkService");
#define LOG(args) MOZ_LOG(gNotifyAddrLog, mozilla::LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(nsNetworkLinkService, nsINetworkLinkService, nsIObserver)

nsNetworkLinkService::nsNetworkLinkService() : mStatusIsKnown(false) {}

NS_IMETHODIMP
nsNetworkLinkService::GetIsLinkUp(bool* aIsUp) {
  if (!mNetlinkSvc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mNetlinkSvc->GetIsLinkUp(aIsUp);
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetLinkStatusKnown(bool* aIsKnown) {
  *aIsKnown = mStatusIsKnown;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetLinkType(uint32_t* aLinkType) {
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::GetNetworkID(nsACString& aNetworkID) {
  if (!mNetlinkSvc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mNetlinkSvc->GetNetworkID(aNetworkID);
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkLinkService::Observe(nsISupports* subject, const char* topic,
                              const char16_t* data) {
  if (!strcmp("xpcom-shutdown-threads", topic)) {
    Shutdown();
  }

  return NS_OK;
}

nsresult nsNetworkLinkService::Init() {
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

nsresult nsNetworkLinkService::Shutdown() {
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

void nsNetworkLinkService::OnNetworkChanged() {
  if (StaticPrefs::network_notify_changed()) {
    if (!mNetworkChangeTime.IsNull()) {
      mozilla::Telemetry::AccumulateTimeDelta(
          mozilla::Telemetry::NETWORK_TIME_BETWEEN_NETWORK_CHANGE_EVENTS,
          mNetworkChangeTime);
    }
    mNetworkChangeTime = TimeStamp::Now();

    RefPtr<nsNetworkLinkService> self = this;
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "nsNetworkLinkService::OnNetworkChanged",
        [self]() { self->SendEvent(NS_NETWORK_LINK_DATA_CHANGED); }));
  }
}

void nsNetworkLinkService::OnLinkUp() {
  RefPtr<nsNetworkLinkService> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsNetworkLinkService::OnLinkUp",
      [self]() { self->SendEvent(NS_NETWORK_LINK_DATA_UP); }));
}

void nsNetworkLinkService::OnLinkDown() {
  RefPtr<nsNetworkLinkService> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsNetworkLinkService::OnLinkDown",
      [self]() { self->SendEvent(NS_NETWORK_LINK_DATA_DOWN); }));
}

void nsNetworkLinkService::OnLinkStatusKnown() { mStatusIsKnown = true; }

/* Sends the given event. Assumes aEventID never goes out of scope (static
 * strings are ideal).
 */
void nsNetworkLinkService::SendEvent(const char* aEventID) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("SendEvent: %s\n", aEventID));

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  if (observerService) {
    observerService->NotifyObservers(static_cast<nsINetworkLinkService*>(this),
                                     NS_NETWORK_LINK_TOPIC,
                                     NS_ConvertASCIItoUTF16(aEventID).get());
  }
}
