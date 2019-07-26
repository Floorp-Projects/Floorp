/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Encoding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsAutoPtr.h"
#include "nsCookie.h"
#include <stdlib.h>

/******************************************************************************
 * nsCookie:
 * creation helper
 ******************************************************************************/

// This is a counter that keeps track of the last used creation time, each time
// we create a new nsCookie. This is nominally the time (in microseconds) the
// cookie was created, but is guaranteed to be monotonically increasing for
// cookies added at runtime after the database has been read in. This is
// necessary to enforce ordering among cookies whose creation times would
// otherwise overlap, since it's possible two cookies may be created at the same
// time, or that the system clock isn't monotonic.
static int64_t gLastCreationTime;

int64_t nsCookie::GenerateUniqueCreationTime(int64_t aCreationTime) {
  // Check if the creation time given to us is greater than the running maximum
  // (it should always be monotonically increasing).
  if (aCreationTime > gLastCreationTime) {
    gLastCreationTime = aCreationTime;
    return aCreationTime;
  }

  // Make up our own.
  return ++gLastCreationTime;
}

// static
already_AddRefed<nsCookie> nsCookie::Create(
    const nsACString& aName, const nsACString& aValue, const nsACString& aHost,
    const nsACString& aPath, int64_t aExpiry, int64_t aLastAccessed,
    int64_t aCreationTime, bool aIsSession, bool aIsSecure, bool aIsHttpOnly,
    const OriginAttributes& aOriginAttributes, int32_t aSameSite,
    int32_t aRawSameSite) {
  mozilla::net::CookieStruct cookieData(
      nsCString(aName), nsCString(aValue), nsCString(aHost), nsCString(aPath),
      aExpiry, aLastAccessed, aCreationTime, aIsHttpOnly, aIsSession, aIsSecure,
      aSameSite, aRawSameSite);

  return Create(cookieData, aOriginAttributes);
}

already_AddRefed<nsCookie> nsCookie::Create(
    const mozilla::net::CookieStruct& aCookieData,
    const OriginAttributes& aOriginAttributes) {
  RefPtr<nsCookie> cookie = new nsCookie(aCookieData, aOriginAttributes);

  // Ensure mValue contains a valid UTF-8 sequence. Otherwise XPConnect will
  // truncate the string after the first invalid octet.
  UTF_8_ENCODING->DecodeWithoutBOMHandling(aCookieData.value(),
                                           cookie->mData.value());

  // If the creationTime given to us is higher than the running maximum,
  // update our maximum.
  if (cookie->mData.creationTime() > gLastCreationTime) {
    gLastCreationTime = cookie->mData.creationTime();
  }

  // If sameSite is not a sensible value, assume strict
  if (cookie->mData.sameSite() < 0 ||
      cookie->mData.sameSite() > nsICookie::SAMESITE_STRICT) {
    cookie->mData.sameSite() = nsICookie::SAMESITE_STRICT;
  }

  // If rawSameSite is not a sensible value, assume equal to sameSite.
  if (!nsCookie::ValidateRawSame(cookie->mData)) {
    cookie->mData.rawSameSite() = nsICookie::SAMESITE_NONE;
  }

  return cookie.forget();
}

size_t nsCookie::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) +
         mData.name().SizeOfExcludingThisIfUnshared(MallocSizeOf) +
         mData.value().SizeOfExcludingThisIfUnshared(MallocSizeOf) +
         mData.host().SizeOfExcludingThisIfUnshared(MallocSizeOf) +
         mData.path().SizeOfExcludingThisIfUnshared(MallocSizeOf);
}

bool nsCookie::IsStale() const {
  int64_t currentTimeInUsec = PR_Now();

  return currentTimeInUsec - LastAccessed() >
         mozilla::StaticPrefs::network_cookie_staleThreshold() *
             PR_USEC_PER_SEC;
}

/******************************************************************************
 * nsCookie:
 * xpcom impl
 ******************************************************************************/

// xpcom getters
NS_IMETHODIMP nsCookie::GetName(nsACString& aName) {
  aName = Name();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetValue(nsACString& aValue) {
  aValue = Value();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetHost(nsACString& aHost) {
  aHost = Host();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetRawHost(nsACString& aHost) {
  aHost = RawHost();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetPath(nsACString& aPath) {
  aPath = Path();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetExpiry(int64_t* aExpiry) {
  *aExpiry = Expiry();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetIsSession(bool* aIsSession) {
  *aIsSession = IsSession();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetIsDomain(bool* aIsDomain) {
  *aIsDomain = IsDomain();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetIsSecure(bool* aIsSecure) {
  *aIsSecure = IsSecure();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetIsHttpOnly(bool* aHttpOnly) {
  *aHttpOnly = IsHttpOnly();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetCreationTime(int64_t* aCreation) {
  *aCreation = CreationTime();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetLastAccessed(int64_t* aTime) {
  *aTime = LastAccessed();
  return NS_OK;
}
NS_IMETHODIMP nsCookie::GetSameSite(int32_t* aSameSite) {
  if (mozilla::StaticPrefs::network_cookie_sameSite_laxByDefault()) {
    *aSameSite = SameSite();
  } else {
    *aSameSite = RawSameSite();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCookie::GetOriginAttributes(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aVal) {
  if (NS_WARN_IF(!ToJSValue(aCx, mOriginAttributes, aVal))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// compatibility method, for use with the legacy nsICookie interface.
// here, expires == 0 denotes a session cookie.
NS_IMETHODIMP
nsCookie::GetExpires(uint64_t* aExpires) {
  if (IsSession()) {
    *aExpires = 0;
  } else {
    *aExpires = Expiry() > 0 ? Expiry() : 1;
  }
  return NS_OK;
}

// static
bool nsCookie::ValidateRawSame(const mozilla::net::CookieStruct& aCookieData) {
  return aCookieData.rawSameSite() == aCookieData.sameSite() ||
         aCookieData.rawSameSite() == nsICookie::SAMESITE_NONE;
}

NS_IMPL_ISUPPORTS(nsCookie, nsICookie)
