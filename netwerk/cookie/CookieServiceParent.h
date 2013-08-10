/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieServiceParent_h
#define mozilla_net_CookieServiceParent_h

#include "mozilla/net/PCookieServiceParent.h"
#include "SerializedLoadContext.h"

class nsCookieService;
class nsIIOService;

namespace mozilla {
namespace net {

class CookieServiceParent : public PCookieServiceParent
{
public:
  CookieServiceParent();
  virtual ~CookieServiceParent();

protected:
  MOZ_WARN_UNUSED_RESULT bool
  GetAppInfoFromParams(const IPC::SerializedLoadContext &aLoadContext,
                       uint32_t& aAppId,
                       bool& aIsInBrowserElement,
                       bool& aIsPrivate);

   virtual bool RecvGetCookieString(const URIParams& aHost,
                                   const bool& aIsForeign,
                                   const bool& aFromHttp,
                                   const IPC::SerializedLoadContext&
                                         loadContext,
                                   nsCString* aResult) MOZ_OVERRIDE;

  virtual bool RecvSetCookieString(const URIParams& aHost,
                                   const bool& aIsForeign,
                                   const nsCString& aCookieString,
                                   const nsCString& aServerTime,
                                   const bool& aFromHttp,
                                   const IPC::SerializedLoadContext&
                                         loadContext) MOZ_OVERRIDE;

  nsRefPtr<nsCookieService> mCookieService;
};

}
}

#endif // mozilla_net_CookieServiceParent_h

