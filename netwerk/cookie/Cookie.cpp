/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Cookie.h"
#include "CookieStorage.h"
#include "mozilla/Encoding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsIURLParser.h"
#include "nsURLHelper.h"
#include <cstdlib>

namespace mozilla {
namespace net {

/******************************************************************************
 * Cookie:
 * creation helper
 ******************************************************************************/

// This is a counter that keeps track of the last used creation time, each time
// we create a new Cookie. This is nominally the time (in microseconds) the
// cookie was created, but is guaranteed to be monotonically increasing for
// cookies added at runtime after the database has been read in. This is
// necessary to enforce ordering among cookies whose creation times would
// otherwise overlap, since it's possible two cookies may be created at the
// same time, or that the system clock isn't monotonic.
static int64_t gLastCreationTime;

int64_t Cookie::GenerateUniqueCreationTime(int64_t aCreationTime) {
  // Check if the creation time given to us is greater than the running maximum
  // (it should always be monotonically increasing).
  if (aCreationTime > gLastCreationTime) {
    gLastCreationTime = aCreationTime;
    return aCreationTime;
  }

  // Make up our own.
  return ++gLastCreationTime;
}

already_AddRefed<Cookie> Cookie::Create(
    const CookieStruct& aCookieData,
    const OriginAttributes& aOriginAttributes) {
  RefPtr<Cookie> cookie =
      Cookie::FromCookieStruct(aCookieData, aOriginAttributes);

  // If the creationTime given to us is higher than the running maximum,
  // update our maximum.
  if (cookie->mData.creationTime() > gLastCreationTime) {
    gLastCreationTime = cookie->mData.creationTime();
  }

  return cookie.forget();
}

already_AddRefed<Cookie> Cookie::FromCookieStruct(
    const CookieStruct& aCookieData,
    const OriginAttributes& aOriginAttributes) {
  RefPtr<Cookie> cookie = new Cookie(aCookieData, aOriginAttributes);

  // Ensure mValue contains a valid UTF-8 sequence. Otherwise XPConnect will
  // truncate the string after the first invalid octet.
  UTF_8_ENCODING->DecodeWithoutBOMHandling(aCookieData.value(),
                                           cookie->mData.value());

  // If sameSite/rawSameSite values aren't sensible reset to Default
  // cf. 5.4.7 in draft-ietf-httpbis-rfc6265bis-09
  if (!Cookie::ValidateSameSite(cookie->mData)) {
    cookie->mData.sameSite() = nsICookie::SAMESITE_LAX;
    cookie->mData.rawSameSite() = nsICookie::SAMESITE_NONE;
  }

  return cookie.forget();
}

already_AddRefed<Cookie> Cookie::CreateValidated(
    const CookieStruct& aCookieData,
    const OriginAttributes& aOriginAttributes) {
  if (!StaticPrefs::network_cookie_fixup_on_db_load()) {
    return Cookie::Create(aCookieData, aOriginAttributes);
  }

  RefPtr<Cookie> cookie =
      Cookie::FromCookieStruct(aCookieData, aOriginAttributes);

  int64_t currentTimeInUsec = PR_Now();
  // Assert that the last creation time is not higher than the current time.
  // The 10000 wiggle room accounts for the fact that calling
  // GenerateUniqueCreationTime might go over the value of PR_Now(), but we'd
  // most likely not add 10000 cookies in a row.
  MOZ_ASSERT(gLastCreationTime < currentTimeInUsec + 10000,
             "Last creation time must not be higher than NOW");

  // If the creationTime given to us is higher than the current time then
  // update the creation time to now.
  if (cookie->mData.creationTime() > currentTimeInUsec) {
    uint64_t diffInSeconds =
        (cookie->mData.creationTime() - currentTimeInUsec) / PR_USEC_PER_SEC;
    mozilla::glean::networking::cookie_creation_fixup_diff.AccumulateSamples(
        {diffInSeconds});
    glean::networking::cookie_timestamp_fixed_count.Get("creationTime"_ns)
        .Add(1);

    cookie->mData.creationTime() =
        GenerateUniqueCreationTime(currentTimeInUsec);
  }

  if (cookie->mData.lastAccessed() > currentTimeInUsec) {
    uint64_t diffInSeconds =
        (cookie->mData.lastAccessed() - currentTimeInUsec) / PR_USEC_PER_SEC;
    mozilla::glean::networking::cookie_access_fixup_diff.AccumulateSamples(
        {diffInSeconds});
    glean::networking::cookie_timestamp_fixed_count.Get("lastAccessed"_ns)
        .Add(1);

    cookie->mData.lastAccessed() = currentTimeInUsec;
  }

  return cookie.forget();
}

size_t Cookie::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) +
         mData.name().SizeOfExcludingThisIfUnshared(MallocSizeOf) +
         mData.value().SizeOfExcludingThisIfUnshared(MallocSizeOf) +
         mData.host().SizeOfExcludingThisIfUnshared(MallocSizeOf) +
         mData.path().SizeOfExcludingThisIfUnshared(MallocSizeOf) +
         mFilePathCache.SizeOfExcludingThisIfUnshared(MallocSizeOf);
}

bool Cookie::IsStale() const {
  int64_t currentTimeInUsec = PR_Now();

  return currentTimeInUsec - LastAccessed() >
         StaticPrefs::network_cookie_staleThreshold() * PR_USEC_PER_SEC;
}

/******************************************************************************
 * Cookie:
 * xpcom impl
 ******************************************************************************/

// xpcom getters
NS_IMETHODIMP Cookie::GetName(nsACString& aName) {
  aName = Name();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetValue(nsACString& aValue) {
  aValue = Value();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetHost(nsACString& aHost) {
  aHost = Host();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetRawHost(nsACString& aHost) {
  aHost = RawHost();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetPath(nsACString& aPath) {
  aPath = Path();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetExpiry(int64_t* aExpiry) {
  *aExpiry = Expiry();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetIsSession(bool* aIsSession) {
  *aIsSession = IsSession();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetIsDomain(bool* aIsDomain) {
  *aIsDomain = IsDomain();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetIsSecure(bool* aIsSecure) {
  *aIsSecure = IsSecure();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetIsHttpOnly(bool* aHttpOnly) {
  *aHttpOnly = IsHttpOnly();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetCreationTime(int64_t* aCreation) {
  *aCreation = CreationTime();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetLastAccessed(int64_t* aTime) {
  *aTime = LastAccessed();
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetSameSite(int32_t* aSameSite) {
  if (StaticPrefs::network_cookie_sameSite_laxByDefault()) {
    *aSameSite = SameSite();
  } else {
    *aSameSite = RawSameSite();
  }
  return NS_OK;
}
NS_IMETHODIMP Cookie::GetSchemeMap(nsICookie::schemeType* aSchemeMap) {
  *aSchemeMap = static_cast<nsICookie::schemeType>(SchemeMap());
  return NS_OK;
}

NS_IMETHODIMP
Cookie::GetOriginAttributes(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) {
  if (NS_WARN_IF(!ToJSValue(aCx, mOriginAttributes, aVal))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

const OriginAttributes& Cookie::OriginAttributesNative() {
  return mOriginAttributes;
}

const Cookie& Cookie::AsCookie() { return *this; }

const nsCString& Cookie::GetFilePath() {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  if (Path().IsEmpty()) {
    // If we don't have a path, just return the (empty) file path cache.
    return mFilePathCache;
  }
  if (!mFilePathCache.IsEmpty()) {
    // If we've computed the answer before, just return it.
    return mFilePathCache;
  }

  nsIURLParser* parser = net_GetStdURLParser();
  NS_ENSURE_TRUE(parser, mFilePathCache);

  int32_t pathLen = Path().Length();
  int32_t filepathLen = 0;
  uint32_t filepathPos = 0;

  nsresult rv = parser->ParsePath(PromiseFlatCString(Path()).get(), pathLen,
                                  &filepathPos, &filepathLen, nullptr,
                                  nullptr,            // don't care about query
                                  nullptr, nullptr);  // don't care about ref
  NS_ENSURE_SUCCESS(rv, mFilePathCache);

  mFilePathCache = Substring(Path(), filepathPos, filepathLen);

  return mFilePathCache;
}

// compatibility method, for use with the legacy nsICookie interface.
// here, expires == 0 denotes a session cookie.
NS_IMETHODIMP
Cookie::GetExpires(uint64_t* aExpires) {
  if (IsSession()) {
    *aExpires = 0;
  } else {
    *aExpires = Expiry() > 0 ? Expiry() : 1;
  }
  return NS_OK;
}

// static
bool Cookie::ValidateSameSite(const CookieStruct& aCookieData) {
  // For proper migration towards a laxByDefault world,
  // sameSite is initialized to LAX even though the server
  // has never sent it.
  if (aCookieData.rawSameSite() == aCookieData.sameSite()) {
    return aCookieData.rawSameSite() >= nsICookie::SAMESITE_NONE &&
           aCookieData.rawSameSite() <= nsICookie::SAMESITE_STRICT;
  }
  return aCookieData.rawSameSite() == nsICookie::SAMESITE_NONE &&
         aCookieData.sameSite() == nsICookie::SAMESITE_LAX;
}

already_AddRefed<Cookie> Cookie::Clone() const {
  return Create(mData, OriginAttributesRef());
}

NS_IMPL_ISUPPORTS(Cookie, nsICookie)

}  // namespace net
}  // namespace mozilla
