/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "AltServiceChild.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/net/SocketProcessChild.h"
#include "nsHttpConnectionInfo.h"
#include "nsProxyInfo.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

StaticRefPtr<AltServiceChild> sAltServiceChild;

AltServiceChild::AltServiceChild() {
  LOG(("AltServiceChild ctor [%p]\n", this));
}

AltServiceChild::~AltServiceChild() {
  LOG(("AltServiceChild dtor [%p]\n", this));
}

// static
bool AltServiceChild::EnsureAltServiceChild() {
  MOZ_ASSERT(XRE_IsSocketProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (sAltServiceChild) {
    return true;
  }

  SocketProcessChild* socketChild = SocketProcessChild::GetSingleton();
  if (!socketChild || socketChild->IsShuttingDown()) {
    return false;
  }

  sAltServiceChild = new AltServiceChild();
  ClearOnShutdown(&sAltServiceChild);

  if (!socketChild->SendPAltServiceConstructor(sAltServiceChild)) {
    sAltServiceChild = nullptr;
    return false;
  }

  return true;
}

// static
void AltServiceChild::ClearHostMapping(nsHttpConnectionInfo* aCi) {
  LOG(("AltServiceChild::ClearHostMapping ci=%s", aCi->HashKey().get()));
  MOZ_ASSERT(aCi);

  RefPtr<nsHttpConnectionInfo> ci = aCi->Clone();
  auto task = [ci{std::move(ci)}]() {
    if (!EnsureAltServiceChild()) {
      return;
    }

    if (!ci->GetOrigin().IsEmpty() && sAltServiceChild->CanSend()) {
      Unused << sAltServiceChild->SendClearHostMapping(
          ci->GetOrigin(), ci->OriginPort(), ci->GetOriginAttributes());
    }
  };

  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(NS_NewRunnableFunction(
        "net::AltServiceChild::ClearHostMapping", task)));
    return;
  }

  task();
}

// static
void AltServiceChild::ProcessHeader(
    const nsCString& aBuf, const nsCString& aOriginScheme,
    const nsCString& aOriginHost, int32_t aOriginPort,
    const nsCString& aUsername, bool aPrivateBrowsing,
    nsIInterfaceRequestor* aCallbacks, nsProxyInfo* aProxyInfo, uint32_t aCaps,
    const OriginAttributes& aOriginAttributes) {
  LOG(("AltServiceChild::ProcessHeader"));
  MOZ_ASSERT(NS_IsMainThread());

  if (!EnsureAltServiceChild()) {
    return;
  }

  if (!sAltServiceChild->CanSend()) {
    return;
  }

  nsTArray<ProxyInfoCloneArgs> proxyInfoArray;
  if (aProxyInfo) {
    nsProxyInfo::SerializeProxyInfo(aProxyInfo, proxyInfoArray);
  }

  Unused << sAltServiceChild->SendProcessHeader(
      aBuf, aOriginScheme, aOriginHost, aOriginPort, aUsername,
      aPrivateBrowsing, proxyInfoArray, aCaps, aOriginAttributes);
}

}  // namespace net
}  // namespace mozilla
