/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeDNSResolverOverrideChild.h"
#include "GetAddrInfo.h"

namespace mozilla {
namespace net {

NativeDNSResolverOverrideChild::NativeDNSResolverOverrideChild() {
  mOverrideService = NativeDNSResolverOverride::GetSingleton();
}

mozilla::ipc::IPCResult NativeDNSResolverOverrideChild::RecvAddIPOverride(
    const nsCString& aHost, const nsCString& aIPLiteral) {
  Unused << mOverrideService->AddIPOverride(aHost, aIPLiteral);
  return IPC_OK();
}

mozilla::ipc::IPCResult
NativeDNSResolverOverrideChild::RecvAddHTTPSRecordOverride(
    const nsCString& aHost, nsTArray<uint8_t>&& aData) {
  Unused << mOverrideService->AddHTTPSRecordOverride(aHost, aData.Elements(),
                                                     aData.Length());
  return IPC_OK();
}

mozilla::ipc::IPCResult NativeDNSResolverOverrideChild::RecvSetCnameOverride(
    const nsCString& aHost, const nsCString& aCNAME) {
  Unused << mOverrideService->SetCnameOverride(aHost, aCNAME);
  return IPC_OK();
}

mozilla::ipc::IPCResult NativeDNSResolverOverrideChild::RecvClearHostOverride(
    const nsCString& aHost) {
  Unused << mOverrideService->ClearHostOverride(aHost);
  return IPC_OK();
}

mozilla::ipc::IPCResult NativeDNSResolverOverrideChild::RecvClearOverrides() {
  Unused << mOverrideService->ClearOverrides();
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
