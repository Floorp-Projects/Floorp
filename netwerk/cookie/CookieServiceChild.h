/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieServiceChild_h__
#define mozilla_net_CookieServiceChild_h__

#include "mozilla/net/PCookieServiceChild.h"
#include "nsClassHashtable.h"
#include "nsCookieKey.h"
#include "nsICookieService.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "mozIThirdPartyUtil.h"
#include "nsWeakReference.h"
#include "nsThreadUtils.h"

class nsCookie;
class nsICookiePermission;
class nsIEffectiveTLDService;
class nsILoadInfo;

struct nsCookieAttributes;

namespace mozilla {
namespace net {
class CookieStruct;

class CookieServiceChild : public PCookieServiceChild,
                           public nsICookieService,
                           public nsIObserver,
                           public nsITimerCallback,
                           public nsSupportsWeakReference {
  friend class PCookieServiceChild;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIESERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  typedef nsTArray<RefPtr<nsCookie>> CookiesList;
  typedef nsClassHashtable<nsCookieKey, CookiesList> CookiesMap;

  CookieServiceChild();

  static already_AddRefed<CookieServiceChild> GetSingleton();

  void TrackCookieLoad(nsIChannel* aChannel);

 protected:
  virtual ~CookieServiceChild();
  void MoveCookies();

  void SerializeURIs(nsIURI* aHostURI, nsIChannel* aChannel,
                     nsCString& aHostSpec, nsCString& aHostCharset,
                     nsCString& aOriginatingSpec,
                     nsCString& aOriginatingCharset);

  nsresult GetCookieStringInternal(nsIURI* aHostURI, nsIChannel* aChannel,
                                   char** aCookieString);

  void GetCookieStringFromCookieHashTable(
      nsIURI* aHostURI, bool aIsForeign, bool aIsTrackingResource,
      bool aFirstPartyStorageAccessGranted, bool aIsSafeTopLevelNav,
      bool aIsSameSiteForeign, nsIChannel* aChannel, nsCString& aCookieString);

  nsresult SetCookieStringInternal(nsIURI* aHostURI, nsIChannel* aChannel,
                                   const char* aCookieString,
                                   const char* aServerTime, bool aFromHttp);

  void RecordDocumentCookie(nsCookie* aCookie, const OriginAttributes& aAttrs);

  void SetCookieInternal(nsCookieAttributes& aCookieAttributes,
                         const mozilla::OriginAttributes& aAttrs,
                         nsIChannel* aChannel, bool aFromHttp,
                         nsICookiePermission* aPermissionService);

  uint32_t CountCookiesFromHashTable(const nsCString& aBaseDomain,
                                     const OriginAttributes& aOriginAttrs);

  void PrefChanged(nsIPrefBranch* aPrefBranch);

  bool RequireThirdPartyCheck(nsILoadInfo* aLoadInfo);

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

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  CookiesMap mCookiesMap;
  nsCOMPtr<nsITimer> mCookieTimer;
  nsCOMPtr<mozIThirdPartyUtil> mThirdPartyUtil;
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
  bool mThirdPartySession;
  bool mThirdPartyNonsecureSession;
  bool mIPCOpen;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieServiceChild_h__
