/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieServiceChild_h__
#define mozilla_net_CookieServiceChild_h__

#include "CookieKey.h"
#include "mozilla/net/PCookieServiceChild.h"
#include "nsClassHashtable.h"
#include "nsICookieService.h"
#include "mozIThirdPartyUtil.h"
#include "nsWeakReference.h"
#include "nsThreadUtils.h"

class nsIEffectiveTLDService;
class nsILoadInfo;

namespace mozilla {
namespace net {

class Cookie;
class CookieStruct;

class CookieServiceChild final : public PCookieServiceChild,
                                 public nsICookieService,
                                 public nsSupportsWeakReference {
  friend class PCookieServiceChild;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIESERVICE

  typedef nsTArray<RefPtr<Cookie>> CookiesList;
  typedef nsClassHashtable<CookieKey, CookiesList> CookiesMap;

  CookieServiceChild();

  void Init();

  static already_AddRefed<CookieServiceChild> GetSingleton();

  void TrackCookieLoad(nsIChannel* aChannel);

 private:
  ~CookieServiceChild();

  void RecordDocumentCookie(Cookie* aCookie, const OriginAttributes& aAttrs);

  uint32_t CountCookiesFromHashTable(const nsACString& aBaseDomain,
                                     const OriginAttributes& aOriginAttrs);

  static bool RequireThirdPartyCheck(nsILoadInfo* aLoadInfo);

  mozilla::ipc::IPCResult RecvTrackCookiesLoad(
      nsTArray<CookieStruct>&& aCookiesList, const OriginAttributes& aAttrs);

  mozilla::ipc::IPCResult RecvRemoveAll();

  mozilla::ipc::IPCResult RecvRemoveBatchDeletedCookies(
      nsTArray<CookieStruct>&& aCookiesList,
      nsTArray<OriginAttributes>&& aAttrsList);

  mozilla::ipc::IPCResult RecvRemoveCookie(const CookieStruct& aCookie,
                                           const OriginAttributes& aAttrs);

  mozilla::ipc::IPCResult RecvAddCookie(const CookieStruct& aCookie,
                                        const OriginAttributes& aAttrs);

  void RemoveSingleCookie(const CookieStruct& aCookie,
                          const OriginAttributes& aAttrs);

  CookiesMap mCookiesMap;
  nsCOMPtr<mozIThirdPartyUtil> mThirdPartyUtil;
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieServiceChild_h__
