/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AltServiceParent_h__
#define AltServiceParent_h__

#include "mozilla/net/PAltServiceParent.h"

namespace mozilla {
namespace net {

class AltServiceParent final : public PAltServiceParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(AltServiceParent, override)
  AltServiceParent() = default;

  mozilla::ipc::IPCResult RecvClearHostMapping(
      const nsCString& aHost, const int32_t& aPort,
      const OriginAttributes& aOriginAttributes);

  mozilla::ipc::IPCResult RecvProcessHeader(
      const nsCString& aBuf, const nsCString& aOriginScheme,
      const nsCString& aOriginHost, const int32_t& aOriginPort,
      const nsACString& aUsername, const bool& aPrivateBrowsing,
      nsTArray<ProxyInfoCloneArgs>&& aProxyInfo, const uint32_t& aCaps,
      const OriginAttributes& aOriginAttributes);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  virtual ~AltServiceParent() = default;
};

}  // namespace net
}  // namespace mozilla

#endif  // AltServiceParent_h__
