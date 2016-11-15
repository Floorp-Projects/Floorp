/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieServiceParent_h
#define mozilla_net_CookieServiceParent_h

#include "mozilla/net/PCookieServiceParent.h"

class nsCookieService;
namespace mozilla { class NeckoOriginAttributes; }

namespace mozilla {
namespace net {

class CookieServiceParent : public PCookieServiceParent
{
public:
  CookieServiceParent();
  virtual ~CookieServiceParent();

protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvGetCookieString(const URIParams& aHost,
                                                      const bool& aIsForeign,
                                                      const bool& aFromHttp,
                                                      const NeckoOriginAttributes& aAttrs,
                                                      nsCString* aResult) override;

  virtual mozilla::ipc::IPCResult RecvSetCookieString(const URIParams& aHost,
                                                      const bool& aIsForeign,
                                                      const nsCString& aCookieString,
                                                      const nsCString& aServerTime,
                                                      const bool& aFromHttp,
                                                      const NeckoOriginAttributes& aAttrs) override;

  RefPtr<nsCookieService> mCookieService;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_CookieServiceParent_h

