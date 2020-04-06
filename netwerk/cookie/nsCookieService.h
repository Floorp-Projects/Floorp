/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookieService_h__
#define nsCookieService_h__

#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsICookiePermission.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "Cookie.h"
#include "CookieKey.h"
#include "CookieStorage.h"

#include "nsString.h"
#include "nsHashKeys.h"
#include "nsIMemoryReporter.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageConnection.h"
#include "nsIFile.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

using mozilla::OriginAttributes;

class nsICookiePermission;
class nsICookieJarSettings;
class nsIEffectiveTLDService;
class nsIIDNService;
class nsIObserverService;
class nsIURI;
class nsIChannel;
class nsIArray;
class nsIThread;
class mozIThirdPartyUtil;

namespace mozilla {
namespace net {
class CookieServiceParent;
}  // namespace net
}  // namespace mozilla

using mozilla::net::CookieKey;

// these constants represent an operation being performed on cookies
enum CookieOperation { OPERATION_READ, OPERATION_WRITE };

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
 * nsCookieService:
 * class declaration
 ******************************************************************************/

class nsCookieService final : public nsICookieService,
                              public nsICookieManager,
                              public nsIObserver,
                              public nsSupportsWeakReference,
                              public nsIMemoryReporter {
 private:
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSICOOKIESERVICE
  NS_DECL_NSICOOKIEMANAGER
  NS_DECL_NSIMEMORYREPORTER

  nsCookieService();
  static already_AddRefed<nsICookieService> GetXPCOMSingleton();
  nsresult Init();

  /**
   * Start watching the observer service for messages indicating that an app has
   * been uninstalled.  When an app is uninstalled, we get the cookie service
   * (thus instantiating it, if necessary) and clear all the cookies for that
   * app.
   */
  static nsAutoCString GetPathFromURI(nsIURI* aHostURI);
  static nsresult GetBaseDomain(nsIEffectiveTLDService* aTLDService,
                                nsIURI* aHostURI, nsCString& aBaseDomain,
                                bool& aRequireHostMatch);
  static nsresult GetBaseDomainFromHost(nsIEffectiveTLDService* aTLDService,
                                        const nsACString& aHost,
                                        nsCString& aBaseDomain);
  static bool DomainMatches(mozilla::net::Cookie* aCookie,
                            const nsACString& aHost);
  static bool PathMatches(mozilla::net::Cookie* aCookie,
                          const nsACString& aPath);
  static bool CanSetCookie(nsIURI* aHostURI, const nsACString& aBaseDomain,
                           mozilla::net::CookieStruct& aCookieData,
                           bool aRequireHostMatch, CookieStatus aStatus,
                           nsCString& aCookieHeader, int64_t aServerTime,
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
  static int64_t ParseServerTime(const nsACString& aServerTime);

  static already_AddRefed<nsICookieJarSettings> GetCookieJarSettings(
      nsIChannel* aChannel);

  void GetCookiesForURI(nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
                        bool aIsThirdPartyTrackingResource,
                        bool aIsThirdPartySocialTrackingResource,
                        bool aFirstPartyStorageAccessGranted,
                        uint32_t aRejectedReason, bool aIsSafeTopLevelNav,
                        bool aIsSameSiteForeign, bool aHttpBound,
                        const OriginAttributes& aOriginAttrs,
                        nsTArray<mozilla::net::Cookie*>& aCookieList);

  /**
   * This method is a helper that allows calling nsICookieManager::Remove()
   * with OriginAttributes parameter.
   */
  nsresult Remove(const nsACString& aHost, const OriginAttributes& aAttrs,
                  const nsACString& aName, const nsACString& aPath);

 protected:
  virtual ~nsCookieService();

  bool IsInitialized() const;

  void InitCookieStorages();
  void CloseCookieStorages();

  void EnsureReadComplete(bool aInitDBConn);
  nsresult NormalizeHost(nsCString& aHost);
  nsresult GetCookieStringCommon(nsIURI* aHostURI, nsIChannel* aChannel,
                                 bool aHttpBound, nsACString& aCookie);
  void GetCookieStringInternal(
      nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
      bool aIsThirdPartyTrackingResource,
      bool aIsThirdPartySocialTrackingResource,
      bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
      bool aIsSafeTopLevelNav, bool aIsSameSiteForeign, bool aHttpBound,
      const OriginAttributes& aOriginAttrs, nsACString& aCookie);
  nsresult SetCookieStringCommon(nsIURI* aHostURI,
                                 const nsACString& aCookieHeader,
                                 const nsACString& aServerTime,
                                 nsIChannel* aChannel, bool aFromHttp);
  void SetCookieStringInternal(
      nsIURI* aHostURI, bool aIsForeign, bool aIsThirdPartyTrackingResource,
      bool aIsThirdPartySocialTrackingResource,
      bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
      nsCString& aCookieHeader, const nsACString& aServerTime, bool aFromHttp,
      const OriginAttributes& aOriginAttrs, nsIChannel* aChannel);
  bool SetCookieInternal(mozilla::net::CookieStorage* aStorage,
                         nsIURI* aHostURI, const nsACString& aBaseDomain,
                         const OriginAttributes& aOriginAttributes,
                         bool aRequireHostMatch, CookieStatus aStatus,
                         nsCString& aCookieHeader, int64_t aServerTime,
                         bool aFromHttp, nsIChannel* aChannel);
  static bool GetTokenValue(nsACString::const_char_iterator& aIter,
                            nsACString::const_char_iterator& aEndIter,
                            nsDependentCSubstring& aTokenString,
                            nsDependentCSubstring& aTokenValue,
                            bool& aEqualsFound);
  static bool ParseAttributes(nsIChannel* aChannel, nsIURI* aHostURI,
                              nsCString& aCookieHeader,
                              mozilla::net::CookieStruct& aCookieData,
                              nsACString& aExpires, nsACString& aMaxage,
                              bool& aAcceptedByParser);
  bool RequireThirdPartyCheck();
  static bool CheckDomain(mozilla::net::CookieStruct& aCookieData,
                          nsIURI* aHostURI, const nsACString& aBaseDomain,
                          bool aRequireHostMatch);
  static bool CheckPath(mozilla::net::CookieStruct& aCookieData,
                        nsIChannel* aChannel, nsIURI* aHostURI);
  static bool CheckPrefixes(mozilla::net::CookieStruct& aCookieData,
                            bool aSecureRequest);
  static bool GetExpiry(mozilla::net::CookieStruct& aCookieData,
                        const nsACString& aExpires, const nsACString& aMaxage,
                        int64_t aServerTime, int64_t aCurrentTime,
                        bool aFromHttp);
  void NotifyAccepted(nsIChannel* aChannel);
  void NotifyRejected(nsIURI* aHostURI, nsIChannel* aChannel,
                      uint32_t aRejectedReason, CookieOperation aOperation);

  nsresult GetCookiesWithOriginAttributes(
      const mozilla::OriginAttributesPattern& aPattern,
      const nsCString& aBaseDomain, nsTArray<RefPtr<nsICookie>>& aResult);
  nsresult RemoveCookiesWithOriginAttributes(
      const mozilla::OriginAttributesPattern& aPattern,
      const nsCString& aBaseDomain);

 protected:
  mozilla::net::CookieStorage* PickStorage(
      const mozilla::OriginAttributes& aAttrs);
  mozilla::net::CookieStorage* PickStorage(
      const mozilla::OriginAttributesPattern& aAttrs);

  nsresult RemoveCookiesFromExactHost(
      const nsACString& aHost,
      const mozilla::OriginAttributesPattern& aPattern);

  static void LogMessageToConsole(nsIChannel* aChannel, nsIURI* aURI,
                                  uint32_t aErrorFlags,
                                  const nsACString& aCategory,
                                  const nsACString& aMsg,
                                  const nsTArray<nsString>& aParams);

  // cached members.
  nsCOMPtr<nsICookiePermission> mPermissionService;
  nsCOMPtr<mozIThirdPartyUtil> mThirdPartyUtil;
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
  nsCOMPtr<nsIIDNService> mIDNService;

  // we have two separate Cookie Storages: one for normal browsing and one for
  // private browsing.
  RefPtr<mozilla::net::CookieDefaultStorage> mDefaultStorage;
  RefPtr<mozilla::net::CookiePrivateStorage> mPrivateStorage;

  // friends!
  friend class DBListenerErrorHandler;
  friend class CloseCookieDBListener;

  static already_AddRefed<nsCookieService> GetSingleton();
  friend class mozilla::net::CookieServiceParent;
};

#endif  // nsCookieService_h__
