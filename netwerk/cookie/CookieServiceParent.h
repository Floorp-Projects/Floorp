/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieServiceParent_h
#define mozilla_net_CookieServiceParent_h

#include "mozilla/net/PCookieServiceParent.h"
#include "mozilla/net/CookieKey.h"

class nsIArray;
class nsICookie;
namespace mozilla {
class OriginAttributes;
}

class nsIEffectiveTLDService;

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

  void RemoveCookie(const Cookie& aCookie);

  void AddCookie(const Cookie& aCookie);

  // This will return true if the CookieServiceParent is currently processing
  // an update from the content process. This is used in ContentParent to make
  // sure that we are only forwarding those cookie updates to other content
  // processes, not the one they originated from.
  bool ProcessingCookie() { return mProcessingCookie; }

  bool ContentProcessHasCookie(const Cookie& cookie);
  bool InsecureCookieOrSecureOrigin(const Cookie& cookie);
  void UpdateCookieInContentList(nsIURI* aHostURI,
                                 const OriginAttributes& aOriginAttrs);

  mozilla::ipc::IPCResult SetCookies(
      const nsCString& aBaseDomain, const OriginAttributes& aOriginAttributes,
      nsIURI* aHost, bool aFromHttp, const nsTArray<CookieStruct>& aCookies,
      dom::BrowsingContext* aBrowsingContext = nullptr);

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

  static void SerializeCookieList(const nsTArray<Cookie*>& aFoundCookieList,
                                  nsTArray<CookieStruct>& aCookiesList,
                                  nsIURI* aHostURI);

  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
  RefPtr<CookieService> mCookieService;
  bool mProcessingCookie;
  nsTHashMap<CookieKey, bool> mCookieKeysInContent;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieServiceParent_h
