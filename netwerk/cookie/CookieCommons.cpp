/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Cookie.h"
#include "CookieCommons.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "nsIEffectiveTLDService.h"

namespace mozilla {
namespace net {

// static
bool CookieCommons::DomainMatches(Cookie* aCookie, const nsACString& aHost) {
  // first, check for an exact host or domain cookie match, e.g. "google.com"
  // or ".google.com"; second a subdomain match, e.g.
  // host = "mail.google.com", cookie domain = ".google.com".
  return aCookie->RawHost() == aHost ||
         (aCookie->IsDomain() && StringEndsWith(aHost, aCookie->Host()));
}

// static
bool CookieCommons::PathMatches(Cookie* aCookie, const nsACString& aPath) {
  nsCString cookiePath(aCookie->GetFilePath());

  // if our cookie path is empty we can't really perform our prefix check, and
  // also we can't check the last character of the cookie path, so we would
  // never return a successful match.
  if (cookiePath.IsEmpty()) {
    return false;
  }

  // if the cookie path and the request path are identical, they match.
  if (cookiePath.Equals(aPath)) {
    return true;
  }

  // if the cookie path is a prefix of the request path, and the last character
  // of the cookie path is %x2F ("/"), they match.
  bool isPrefix = StringBeginsWith(aPath, cookiePath);
  if (isPrefix && cookiePath.Last() == '/') {
    return true;
  }

  // if the cookie path is a prefix of the request path, and the first character
  // of the request path that is not included in the cookie path is a %x2F ("/")
  // character, they match.
  uint32_t cookiePathLen = cookiePath.Length();
  if (isPrefix && aPath[cookiePathLen] == '/') {
    return true;
  }

  return false;
}

// Get the base domain for aHostURI; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". Only properly-formed URI's are tolerated, though a trailing
// dot may be present. If aHostURI is an IP address, an alias such as
// 'localhost', an eTLD such as 'co.uk', or the empty string, aBaseDomain will
// be the exact host, and aRequireHostMatch will be true to indicate that
// substring matches should not be performed.
nsresult CookieCommons::GetBaseDomain(nsIEffectiveTLDService* aTLDService,
                                      nsIURI* aHostURI, nsCString& aBaseDomain,
                                      bool& aRequireHostMatch) {
  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = aTLDService->GetBaseDomain(aHostURI, 0, aBaseDomain);
  aRequireHostMatch = rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
                      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS;
  if (aRequireHostMatch) {
    // aHostURI is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. use the host as a key in such
    // cases.
    rv = aHostURI->GetAsciiHost(aBaseDomain);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // aHost (and thus aBaseDomain) may be the string '.'. If so, fail.
  if (aBaseDomain.Length() == 1 && aBaseDomain.Last() == '.') {
    return NS_ERROR_INVALID_ARG;
  }

  // block any URIs without a host that aren't file:// URIs.
  if (aBaseDomain.IsEmpty() && !aHostURI->SchemeIs("file")) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

// Get the base domain for aHost; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". This is done differently than GetBaseDomain(mTLDService, ): it
// is assumed that aHost is already normalized, and it may contain a leading dot
// (indicating that it represents a domain). A trailing dot may be present.
// If aHost is an IP address, an alias such as 'localhost', an eTLD such as
// 'co.uk', or the empty string, aBaseDomain will be the exact host, and a
// leading dot will be treated as an error.
nsresult CookieCommons::GetBaseDomainFromHost(
    nsIEffectiveTLDService* aTLDService, const nsACString& aHost,
    nsCString& aBaseDomain) {
  // aHost must not be the string '.'.
  if (aHost.Length() == 1 && aHost.Last() == '.') {
    return NS_ERROR_INVALID_ARG;
  }

  // aHost may contain a leading dot; if so, strip it now.
  bool domain = !aHost.IsEmpty() && aHost.First() == '.';

  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = aTLDService->GetBaseDomainFromHost(Substring(aHost, domain), 0,
                                                   aBaseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // aHost is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. use the host as a key in such
    // cases; however, we reject any such hosts with a leading dot, since it
    // doesn't make sense for them to be domain cookies.
    if (domain) {
      return NS_ERROR_INVALID_ARG;
    }

    aBaseDomain = aHost;
    return NS_OK;
  }
  return rv;
}

// Notify observers that a cookie was rejected due to the users' prefs.
void CookieCommons::NotifyRejected(nsIURI* aHostURI, nsIChannel* aChannel,
                                   uint32_t aRejectedReason,
                                   CookieOperation aOperation) {
  if (aOperation == OPERATION_WRITE) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(aHostURI, "cookie-rejected", nullptr);
    }
  } else {
    MOZ_ASSERT(aOperation == OPERATION_READ);
  }

  ContentBlockingNotifier::OnDecision(
      aChannel, ContentBlockingNotifier::BlockingDecision::eBlock,
      aRejectedReason);
}

bool CookieCommons::CheckPathSize(const CookieStruct& aCookieData) {
  return aCookieData.path().Length() <= kMaxBytesPerPath;
}

bool CookieCommons::CheckNameAndValueSize(const CookieStruct& aCookieData) {
  // reject cookie if it's over the size limit, per RFC2109
  return (aCookieData.name().Length() + aCookieData.value().Length()) <=
         kMaxBytesPerCookie;
}

bool CookieCommons::CheckName(const CookieStruct& aCookieData) {
  const char illegalNameCharacters[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
      0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x00};

  return aCookieData.name().FindCharInSet(illegalNameCharacters, 0) == -1;
}

bool CookieCommons::CheckHttpValue(const CookieStruct& aCookieData) {
  // reject cookie if value contains an RFC 6265 disallowed character - see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1191423
  // NOTE: this is not the full set of characters disallowed by 6265 - notably
  // 0x09, 0x20, 0x22, 0x2C, 0x5C, and 0x7F are missing from this list. This is
  // for parity with Chrome. This only applies to cookies set via the Set-Cookie
  // header, as document.cookie is defined to be UTF-8. Hooray for
  // symmetry!</sarcasm>
  const char illegalCharacters[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
      0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x3B, 0x00};
  return aCookieData.value().FindCharInSet(illegalCharacters, 0) == -1;
}

}  // namespace net
}  // namespace mozilla
