/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieCommons_h
#define mozilla_net_CookieCommons_h

#include <cstdint>
#include "prtime.h"
#include "nsString.h"

class nsIChannel;
class nsIEffectiveTLDService;
class nsIURI;

namespace mozilla {
namespace net {

// these constants represent an operation being performed on cookies
enum CookieOperation { OPERATION_READ, OPERATION_WRITE };

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
                                nsIURI* aHostURI, nsCString& aBaseDomain,
                                bool& aRequireHostMatch);

  static nsresult GetBaseDomainFromHost(nsIEffectiveTLDService* aTLDService,
                                        const nsACString& aHost,
                                        nsCString& aBaseDomain);

  static void NotifyRejected(nsIURI* aHostURI, nsIChannel* aChannel,
                             uint32_t aRejectedReason,
                             CookieOperation aOperation);

  static bool CheckPathSize(const CookieStruct& aCookieData);

  static bool CheckNameAndValueSize(const CookieStruct& aCookieData);

  static bool CheckName(const CookieStruct& aCookieData);

  static bool CheckHttpValue(const CookieStruct& aCookieData);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieCommons_h
