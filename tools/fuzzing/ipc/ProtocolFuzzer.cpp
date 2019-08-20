/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorBridgeParent.h"

#include "ProtocolFuzzer.h"

namespace mozilla {
namespace ipc {

nsTArray<nsCString> LoadIPCMessageBlacklist(const char* aPath) {
  nsTArray<nsCString> blacklist;
  if (aPath) {
    nsresult result = Faulty::ReadFile(aPath, blacklist);
    MOZ_RELEASE_ASSERT(result == NS_OK);
  }
  return blacklist;
}

mozilla::dom::ContentParent* ProtocolFuzzerHelper::CreateContentParent(
    mozilla::dom::ContentParent* aOpener, const nsAString& aRemoteType) {
  auto* cp = new mozilla::dom::ContentParent(aOpener, aRemoteType);
  // TODO: this duplicates MessageChannel::Open
  cp->GetIPCChannel()->mWorkerThread = PR_GetCurrentThread();
  cp->GetIPCChannel()->mMonitor = new RefCountedMonitor();
  return cp;
}

void ProtocolFuzzerHelper::CompositorBridgeParentSetup() {
  mozilla::layers::CompositorBridgeParent::Setup();
}

}  // namespace ipc
}  // namespace mozilla
