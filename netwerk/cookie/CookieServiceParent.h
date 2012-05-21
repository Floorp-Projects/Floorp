/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieServiceParent_h
#define mozilla_net_CookieServiceParent_h

#include "mozilla/net/PCookieServiceParent.h"

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
  virtual bool RecvGetCookieString(const IPC::URI& aHost,
                                   const bool& aIsForeign,
                                   const bool& aFromHttp,
                                   nsCString* aResult);

  virtual bool RecvSetCookieString(const IPC::URI& aHost,
                                   const bool& aIsForeign,
                                   const nsCString& aCookieString,
                                   const nsCString& aServerTime,
                                   const bool& aFromHttp);

  nsRefPtr<nsCookieService> mCookieService;
};

}
}

#endif // mozilla_net_CookieServiceParent_h

