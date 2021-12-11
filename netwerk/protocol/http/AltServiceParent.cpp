/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "AltServiceParent.h"
#include "AlternateServices.h"
#include "nsHttpHandler.h"

namespace mozilla {
namespace net {

mozilla::ipc::IPCResult AltServiceParent::RecvClearHostMapping(
    const nsCString& aHost, const int32_t& aPort,
    const OriginAttributes& aOriginAttributes) {
  LOG(("AltServiceParent::RecvClearHostMapping [this=%p]\n", this));
  if (gHttpHandler) {
    gHttpHandler->AltServiceCache()->ClearHostMapping(aHost, aPort,
                                                      aOriginAttributes);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult AltServiceParent::RecvProcessHeader(
    const nsCString& aBuf, const nsCString& aOriginScheme,
    const nsCString& aOriginHost, const int32_t& aOriginPort,
    const nsACString& aUsername, const bool& aPrivateBrowsing,
    nsTArray<ProxyInfoCloneArgs>&& aProxyInfo, const uint32_t& aCaps,
    const OriginAttributes& aOriginAttributes) {
  LOG(("AltServiceParent::RecvProcessHeader [this=%p]\n", this));
  nsProxyInfo* pi = aProxyInfo.IsEmpty()
                        ? nullptr
                        : nsProxyInfo::DeserializeProxyInfo(aProxyInfo);
  AltSvcMapping::ProcessHeader(aBuf, aOriginScheme, aOriginHost, aOriginPort,
                               aUsername, aPrivateBrowsing, nullptr, pi, aCaps,
                               aOriginAttributes);
  return IPC_OK();
}

void AltServiceParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("AltServiceParent::ActorDestroy [this=%p]\n", this));
}

}  // namespace net
}  // namespace mozilla
