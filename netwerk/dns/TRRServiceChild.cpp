/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/TRRServiceChild.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsIDNService.h"
#include "nsIObserverService.h"
#include "TRRService.h"

namespace mozilla {
namespace net {

static StaticRefPtr<nsIDNSService> sDNSService;

void TRRServiceChild::Init(const bool& aCaptiveIsPassed,
                           const bool& aParentalControlEnabled,
                           nsTArray<nsCString>&& aDNSSuffixList) {
  nsCOMPtr<nsIDNSService> dns =
      do_GetService("@mozilla.org/network/dns-service;1");
  sDNSService = dns;
  ClearOnShutdown(&sDNSService);
  MOZ_ASSERT(sDNSService);
  MOZ_ASSERT(gTRRService);

  gTRRService->mCaptiveIsPassed = aCaptiveIsPassed;
  gTRRService->mParentalControlEnabled = aParentalControlEnabled;
  gTRRService->RebuildSuffixList(std::move(aDNSSuffixList));
}

mozilla::ipc::IPCResult TRRServiceChild::RecvUpdatePlatformDNSInformation(
    nsTArray<nsCString>&& aDNSSuffixList, const bool& aPlatformDisabledTRR) {
  gTRRService->RebuildSuffixList(std::move(aDNSSuffixList));
  gTRRService->mPlatformDisabledTRR = aPlatformDisabledTRR;
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceChild::RecvUpdateParentalControlEnabled(
    const bool& aEnabled) {
  gTRRService->mParentalControlEnabled = aEnabled;
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceChild::RecvClearDNSCache(
    const bool& aTrrToo) {
  Unused << sDNSService->ClearCache(aTrrToo);
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceChild::RecvSetDetectedTrrURI(
    const nsCString& aURI) {
  gTRRService->SetDetectedTrrURI(aURI);
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
