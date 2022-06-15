/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieServiceParent_h
#define mozilla_net_CookieServiceParent_h

#include "mozilla/net/PCookieServiceParent.h"

class nsIArray;
class nsICookie;
namespace mozilla {
class OriginAttributes;
}

namespace mozilla {
namespace net {

class Cookie;
class CookieService;

class CookieServiceParent : public PCookieServiceParent {
  friend class PCookieServiceParent;

 public:
  CookieServiceParent();
  virtual ~CookieServiceParent() = default;

  void TrackCookieLoad(nsIChannel* aChannel);

  void RemoveBatchDeletedCookies(nsIArray* aCookieList);

  void RemoveAll();

  void RemoveCookie(nsICookie* aCookie);

  void AddCookie(nsICookie* aCookie);

  // This will return true if the CookieServiceParent is currently processing
  // an update from the content process. This is used in ContentParent to make
  // sure that we are only forwarding those cookie updates to other content
  // processes, not the one they originated from.
  bool ProcessingCookie() { return mProcessingCookie; }

 protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvSetCookies(
      const nsCString& aBaseDomain, const OriginAttributes& aOriginAttributes,
      nsIURI* aHost, bool aFromHttp, const nsTArray<CookieStruct>& aCookies);

  mozilla::ipc::IPCResult RecvPrepareCookieList(
      nsIURI* aHost, const bool& aIsForeign,
      const bool& aIsThirdPartyTrackingResource,
      const bool& aIsThirdPartySocialTrackingResource,
      const bool& aStorageAccessPermissionGranted,
      const uint32_t& aRejectedReason, const bool& aIsSafeTopLevelNav,
      const bool& aIsSameSiteForeign, const bool& aHadCrossSiteRedirects,
      const OriginAttributes& aAttrs);

  static void SerialializeCookieList(const nsTArray<Cookie*>& aFoundCookieList,
                                     nsTArray<CookieStruct>& aCookiesList);

  RefPtr<CookieService> mCookieService;
  bool mProcessingCookie;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieServiceParent_h
