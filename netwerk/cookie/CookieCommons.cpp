/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieCommons.h"
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
  if (cookiePath.IsEmpty()) return false;

  // if the cookie path and the request path are identical, they match.
  if (cookiePath.Equals(aPath)) return true;

  // if the cookie path is a prefix of the request path, and the last character
  // of the cookie path is %x2F ("/"), they match.
  bool isPrefix = StringBeginsWith(aPath, cookiePath);
  if (isPrefix && cookiePath.Last() == '/') return true;

  // if the cookie path is a prefix of the request path, and the first character
  // of the request path that is not included in the cookie path is a %x2F ("/")
  // character, they match.
  uint32_t cookiePathLen = cookiePath.Length();
  if (isPrefix && aPath[cookiePathLen] == '/') return true;

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
  if (aBaseDomain.Length() == 1 && aBaseDomain.Last() == '.')
    return NS_ERROR_INVALID_ARG;

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
  if (aHost.Length() == 1 && aHost.Last() == '.') return NS_ERROR_INVALID_ARG;

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
    if (domain) return NS_ERROR_INVALID_ARG;

    aBaseDomain = aHost;
    return NS_OK;
  }
  return rv;
}

}  // namespace net
}  // namespace mozilla
