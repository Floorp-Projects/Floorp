/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/TRRServiceChild.h"
#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsHttpConnectionInfo.h"
#include "nsIDNService.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "TRRService.h"

namespace mozilla {
namespace net {

static StaticRefPtr<nsIDNSService> sDNSService;
static Atomic<TRRServiceChild*> sTRRServiceChild;

NS_IMPL_ISUPPORTS(TRRServiceChild, nsIObserver, nsISupportsWeakReference)

TRRServiceChild::TRRServiceChild() {
  MOZ_ASSERT(NS_IsMainThread());
  sTRRServiceChild = this;
}

TRRServiceChild::~TRRServiceChild() {
  MOZ_ASSERT(NS_IsMainThread());
  sTRRServiceChild = nullptr;
}

/* static */
TRRServiceChild* TRRServiceChild::GetSingleton() {
  MOZ_ASSERT(NS_IsMainThread());
  return sTRRServiceChild;
}

void TRRServiceChild::Init(const bool& aCaptiveIsPassed,
                           const bool& aParentalControlEnabled,
                           nsTArray<nsCString>&& aDNSSuffixList) {
  nsCOMPtr<nsIDNSService> dns =
      do_GetService("@mozilla.org/network/dns-service;1");
  sDNSService = dns;
  ClearOnShutdown(&sDNSService);
  MOZ_ASSERT(sDNSService);

  TRRService* trrService = TRRService::Get();
  MOZ_ASSERT(trrService);

  trrService->mCaptiveIsPassed = aCaptiveIsPassed;
  trrService->mParentalControlEnabled = aParentalControlEnabled;
  trrService->RebuildSuffixList(std::move(aDNSSuffixList));

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(this, "network:connectivity-service:dns-checks-complete",
                   true);
  obs->AddObserver(this, "network:connectivity-service:ip-checks-complete",
                   true);
}

NS_IMETHODIMP
TRRServiceChild::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  if (!strcmp(aTopic, "network:connectivity-service:ip-checks-complete") ||
      !strcmp(aTopic, "network:connectivity-service:dns-checks-complete")) {
    Unused << SendNotifyNetworkConnectivityServiceObservers(
        nsPrintfCString("%s-from-socket-process", aTopic));
  }

  return NS_OK;
}

mozilla::ipc::IPCResult TRRServiceChild::RecvUpdatePlatformDNSInformation(
    nsTArray<nsCString>&& aDNSSuffixList) {
  TRRService::Get()->RebuildSuffixList(std::move(aDNSSuffixList));
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceChild::RecvUpdateParentalControlEnabled(
    const bool& aEnabled) {
  TRRService::Get()->mParentalControlEnabled = aEnabled;
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceChild::RecvClearDNSCache(
    const bool& aTrrToo) {
  Unused << sDNSService->ClearCache(aTrrToo);
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceChild::RecvSetDetectedTrrURI(
    const nsCString& aURI) {
  TRRService::Get()->SetDetectedTrrURI(aURI);
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceChild::RecvSetDefaultTRRConnectionInfo(
    Maybe<HttpConnectionInfoCloneArgs>&& aArgs) {
  if (!aArgs) {
    TRRService::Get()->SetDefaultTRRConnectionInfo(nullptr);
    return IPC_OK();
  }

  RefPtr<nsHttpConnectionInfo> cinfo =
      nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs(aArgs.ref());
  TRRService::Get()->SetDefaultTRRConnectionInfo(cinfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceChild::RecvUpdateEtcHosts(
    nsTArray<nsCString>&& aHosts) {
  TRRService::Get()->AddEtcHosts(aHosts);
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
