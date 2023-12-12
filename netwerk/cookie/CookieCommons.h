/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieCommons_h
#define mozilla_net_CookieCommons_h

#include <cstdint>
#include <functional>
#include "mozIThirdPartyUtil.h"
#include "prtime.h"
#include "nsString.h"
#include "nsICookie.h"
#include "mozilla/net/NeckoChannelParams.h"

class nsIChannel;
class nsIConsoleReportCollector;
class nsICookieJarSettings;
class nsIEffectiveTLDService;
class nsIPrincipal;
class nsIURI;

namespace mozilla {

namespace dom {
class Document;
}

namespace net {

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

class Cookie;

// pref string constants
static const char kPrefMaxNumberOfCookies[] = "network.cookie.maxNumber";
static const char kPrefMaxCookiesPerHost[] = "network.cookie.maxPerHost";
static const char kPrefCookieQuotaPerHost[] = "network.cookie.quotaPerHost";
static const char kPrefCookiePurgeAge[] = "network.cookie.purgeAge";

// default limits for the cookie list. these can be tuned by the
// network.cookie.maxNumber and network.cookie.maxPerHost prefs respectively.
static const uint32_t kMaxCookiesPerHost = 180;
static const uint32_t kCookieQuotaPerHost = 150;
static const uint32_t kMaxNumberOfCookies = 3000;
static const uint32_t kMaxBytesPerCookie = 4096;
static const uint32_t kMaxBytesPerPath = 1024;

static const int64_t kCookiePurgeAge =
    int64_t(30 * 24 * 60 * 60) * PR_USEC_PER_SEC;  // 30 days in microseconds

class CookieCommons final {
 public:
  static bool DomainMatches(Cookie* aCookie, const nsACString& aHost);

  static bool PathMatches(Cookie* aCookie, const nsACString& aPath);

  static nsresult GetBaseDomain(nsIEffectiveTLDService* aTLDService,
                                nsIURI* aHostURI, nsACString& aBaseDomain,
                                bool& aRequireHostMatch);

  static nsresult GetBaseDomain(nsIPrincipal* aPrincipal,
                                nsACString& aBaseDomain);

  static nsresult GetBaseDomainFromHost(nsIEffectiveTLDService* aTLDService,
                                        const nsACString& aHost,
                                        nsCString& aBaseDomain);

  static void NotifyRejected(nsIURI* aHostURI, nsIChannel* aChannel,
                             uint32_t aRejectedReason,
                             CookieOperation aOperation);

  static bool CheckPathSize(const CookieStruct& aCookieData);

  static bool CheckNameAndValueSize(const CookieStruct& aCookieData);

  static bool CheckName(const CookieStruct& aCookieData);

  static bool CheckValue(const CookieStruct& aCookieData);

  static bool CheckCookiePermission(nsIChannel* aChannel,
                                    CookieStruct& aCookieData);

  static bool CheckCookiePermission(nsIPrincipal* aPrincipal,
                                    nsICookieJarSettings* aCookieJarSettings,
                                    CookieStruct& aCookieData);

  static already_AddRefed<Cookie> CreateCookieFromDocument(
      dom::Document* aDocument, const nsACString& aCookieString,
      int64_t aCurrentTimeInUsec, nsIEffectiveTLDService* aTLDService,
      mozIThirdPartyUtil* aThirdPartyUtil,
      std::function<bool(const nsACString&, const OriginAttributes&)>&&
          aHasExistingCookiesLambda,
      nsIURI** aDocumentURI, nsACString& aBaseDomain, OriginAttributes& aAttrs);

  static already_AddRefed<nsICookieJarSettings> GetCookieJarSettings(
      nsIChannel* aChannel);

  static bool ShouldIncludeCrossSiteCookieForDocument(Cookie* aCookie,
                                                      dom::Document* aDocument);

  static bool IsSchemeSupported(nsIPrincipal* aPrincipal);
  static bool IsSchemeSupported(nsIURI* aURI);
  static bool IsSchemeSupported(const nsACString& aScheme);

  static nsICookie::schemeType URIToSchemeType(nsIURI* aURI);

  static nsICookie::schemeType PrincipalToSchemeType(nsIPrincipal* aPrincipal);

  static nsICookie::schemeType SchemeToSchemeType(const nsACString& aScheme);

  // Returns true if the channel is a safe top-level navigation or if it's a
  // download request
  static bool IsSafeTopLevelNav(nsIChannel* aChannel);

  // Returns true if the channel is a foreign with respect to the host-uri.
  // For loads of TYPE_DOCUMENT, this function returns true if it's a cross
  // site navigation.
  // `aHadCrossSiteRedirects` will be true iff the channel had a cross-site
  // redirect before the final URI.
  static bool IsSameSiteForeign(nsIChannel* aChannel, nsIURI* aHostURI,
                                bool* aHadCrossSiteRedirects);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieCommons_h
