/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieService_h
#define mozilla_net_CookieService_h

#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "Cookie.h"

#include "nsString.h"
#include "nsIMemoryReporter.h"
#include "mozilla/MemoryReporting.h"

class nsICookieJarSettings;
class nsIEffectiveTLDService;
class nsIIDNService;
class nsIURI;
class nsIChannel;
class mozIThirdPartyUtil;

namespace mozilla {
namespace net {

class CookiePersistentStorage;
class CookiePrivateStorage;
class CookieStorage;

// these constants represent a decision about a cookie based on user prefs.
enum CookieStatus {
  STATUS_ACCEPTED,
  STATUS_ACCEPT_SESSION,
  STATUS_REJECTED,
  // STATUS_REJECTED_WITH_ERROR indicates the cookie should be rejected because
  // of an error (rather than something the user can control). this is used for
  // notification purposes, since we only want to notify of rejections where
  // the user can do something about it (e.g. whitelist the site).
  STATUS_REJECTED_WITH_ERROR
};

/******************************************************************************
 * CookieService:
 * class declaration
 ******************************************************************************/

class CookieService final : public nsICookieService,
                            public nsICookieManager,
                            public nsIObserver,
                            public nsSupportsWeakReference,
                            public nsIMemoryReporter {
 private:
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSICOOKIESERVICE
  NS_DECL_NSICOOKIEMANAGER
  NS_DECL_NSIMEMORYREPORTER

  static already_AddRefed<CookieService> GetSingleton();

  CookieService();
  static already_AddRefed<nsICookieService> GetXPCOMSingleton();
  nsresult Init();

  /**
   * Start watching the observer service for messages indicating that an app has
   * been uninstalled.  When an app is uninstalled, we get the cookie service
   * (thus instantiating it, if necessary) and clear all the cookies for that
   * app.
   */
  static nsAutoCString GetPathFromURI(nsIURI* aHostURI);
  static bool CanSetCookie(nsIURI* aHostURI, const nsACString& aBaseDomain,
                           CookieStruct& aCookieData, bool aRequireHostMatch,
                           CookieStatus aStatus, nsCString& aCookieHeader,
                           bool aFromHttp, nsIChannel* aChannel,
                           bool& aSetCookie,
                           mozIThirdPartyUtil* aThirdPartyUtil);
  static CookieStatus CheckPrefs(nsICookieJarSettings* aCookieJarSettings,
                                 nsIURI* aHostURI, bool aIsForeign,
                                 bool aIsThirdPartyTrackingResource,
                                 bool aIsThirdPartySocialTrackingResource,
                                 bool aFirstPartyStorageAccessGranted,
                                 const nsACString& aCookieHeader,
                                 const int aNumOfCookies,
                                 const OriginAttributes& aOriginAttrs,
                                 uint32_t* aRejectedReason);

  static already_AddRefed<nsICookieJarSettings> GetCookieJarSettings(
      nsIChannel* aChannel);

  void GetCookiesForURI(nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
                        bool aIsThirdPartyTrackingResource,
                        bool aIsThirdPartySocialTrackingResource,
                        bool aFirstPartyStorageAccessGranted,
                        uint32_t aRejectedReason, bool aIsSafeTopLevelNav,
                        bool aIsSameSiteForeign, bool aHttpBound,
                        const OriginAttributes& aOriginAttrs,
                        nsTArray<Cookie*>& aCookieList);

  /**
   * This method is a helper that allows calling nsICookieManager::Remove()
   * with OriginAttributes parameter.
   */
  nsresult Remove(const nsACString& aHost, const OriginAttributes& aAttrs,
                  const nsACString& aName, const nsACString& aPath);

  bool SetCookiesFromIPC(const nsACString& aBaseDomain,
                         const OriginAttributes& aAttrs, nsIURI* aHostURI,
                         bool aFromHttp,
                         const nsTArray<CookieStruct>& aCookies);

 protected:
  virtual ~CookieService();

  bool IsInitialized() const;

  void InitCookieStorages();
  void CloseCookieStorages();

  void EnsureReadComplete(bool aInitDBConn);
  nsresult NormalizeHost(nsCString& aHost);
  nsresult SetCookieStringCommon(nsIURI* aHostURI,
                                 const nsACString& aCookieHeader,
                                 nsIChannel* aChannel, bool aFromHttp);
  void SetCookieStringInternal(nsIURI* aHostURI, bool aIsForeign,
                               bool aIsThirdPartyTrackingResource,
                               bool aIsThirdPartySocialTrackingResource,
                               bool aFirstPartyStorageAccessGranted,
                               uint32_t aRejectedReason,
                               nsCString& aCookieHeader, bool aFromHttp,
                               const OriginAttributes& aOriginAttrs,
                               nsIChannel* aChannel);
  bool SetCookieInternal(CookieStorage* aStorage, nsIURI* aHostURI,
                         const nsACString& aBaseDomain,
                         const OriginAttributes& aOriginAttributes,
                         bool aRequireHostMatch, CookieStatus aStatus,
                         nsCString& aCookieHeader, bool aFromHttp,
                         nsIChannel* aChannel);
  static bool GetTokenValue(nsACString::const_char_iterator& aIter,
                            nsACString::const_char_iterator& aEndIter,
                            nsDependentCSubstring& aTokenString,
                            nsDependentCSubstring& aTokenValue,
                            bool& aEqualsFound);
  static bool ParseAttributes(nsIChannel* aChannel, nsIURI* aHostURI,
                              nsCString& aCookieHeader,
                              CookieStruct& aCookieData, nsACString& aExpires,
                              nsACString& aMaxage, bool& aAcceptedByParser);
  bool RequireThirdPartyCheck();
  static bool CheckDomain(CookieStruct& aCookieData, nsIURI* aHostURI,
                          const nsACString& aBaseDomain,
                          bool aRequireHostMatch);
  static bool CheckPath(CookieStruct& aCookieData, nsIChannel* aChannel,
                        nsIURI* aHostURI);
  static bool CheckPrefixes(CookieStruct& aCookieData, bool aSecureRequest);
  static bool GetExpiry(CookieStruct& aCookieData, const nsACString& aExpires,
                        const nsACString& aMaxage, int64_t aCurrentTime,
                        bool aFromHttp);
  void NotifyAccepted(nsIChannel* aChannel);

  nsresult GetCookiesWithOriginAttributes(
      const OriginAttributesPattern& aPattern, const nsCString& aBaseDomain,
      nsTArray<RefPtr<nsICookie>>& aResult);
  nsresult RemoveCookiesWithOriginAttributes(
      const OriginAttributesPattern& aPattern, const nsCString& aBaseDomain);

 protected:
  CookieStorage* PickStorage(const OriginAttributes& aAttrs);
  CookieStorage* PickStorage(const OriginAttributesPattern& aAttrs);

  nsresult RemoveCookiesFromExactHost(const nsACString& aHost,
                                      const OriginAttributesPattern& aPattern);

  static void LogMessageToConsole(nsIChannel* aChannel, nsIURI* aURI,
                                  uint32_t aErrorFlags,
                                  const nsACString& aCategory,
                                  const nsACString& aMsg,
                                  const nsTArray<nsString>& aParams);

  // cached members.
  nsCOMPtr<mozIThirdPartyUtil> mThirdPartyUtil;
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
  nsCOMPtr<nsIIDNService> mIDNService;

  // we have two separate Cookie Storages: one for normal browsing and one for
  // private browsing.
  RefPtr<CookiePersistentStorage> mPersistentStorage;
  RefPtr<CookiePrivateStorage> mPrivateStorage;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieService_h
