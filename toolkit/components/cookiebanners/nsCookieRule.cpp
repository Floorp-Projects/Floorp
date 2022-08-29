/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookieRule.h"

#include "mozilla/OriginAttributes.h"
#include "nsICookie.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "Cookie.h"
#include "prtime.h"
#include "mozilla/StaticPrefs_cookiebanners.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(nsCookieRule, nsICookieRule)

nsCookieRule::nsCookieRule(bool aIsOptOut, const nsACString& aName,
                           const nsACString& aValue, const nsACString& aHost,
                           const nsACString& aPath, int64_t aExpiryRelative,
                           const nsACString& aUnsetValue, bool aIsSecure,
                           bool aIsHttpOnly, bool aIsSession, int32_t aSameSite,
                           nsICookie::schemeType aSchemeMap) {
  mExpiryRelative = aExpiryRelative;
  mUnsetValue = aUnsetValue;

  net::CookieStruct cookieData(nsCString(aName), nsCString(aValue),
                               nsCString(aHost), nsCString(aPath), 0, 0, 0,
                               aIsHttpOnly, aIsSession, aIsSecure, aSameSite,
                               aSameSite, aSchemeMap);

  OriginAttributes attrs;
  mCookie = net::Cookie::Create(cookieData, attrs);
}

/* readonly attribute int64_t expiryRelative; */
NS_IMETHODIMP nsCookieRule::GetExpiryRelative(int64_t* aExpiryRelative) {
  NS_ENSURE_ARG_POINTER(aExpiryRelative);

  *aExpiryRelative = mExpiryRelative;

  return NS_OK;
}

/* readonly attribute AUTF8String unsetValue; */
NS_IMETHODIMP nsCookieRule::GetUnsetValue(nsACString& aUnsetValue) {
  aUnsetValue = mUnsetValue;

  return NS_OK;
}

/* readonly attribute nsICookie cookie; */
NS_IMETHODIMP nsCookieRule::GetCookie(nsICookie** aCookie) {
  NS_ENSURE_ARG_POINTER(aCookie);

  // Copy cookie and update expiry, creation and last accessed time.
  nsICookie* cookie = mCookie;
  RefPtr<net::Cookie> cookieNative = static_cast<net::Cookie*>(cookie)->Clone();

  int64_t currentTimeInUsec = PR_Now();
  cookieNative->SetCreationTime(
      net::Cookie::GenerateUniqueCreationTime(currentTimeInUsec));
  cookieNative->SetLastAccessed(currentTimeInUsec);
  cookieNative->SetExpiry((currentTimeInUsec / PR_USEC_PER_SEC) +
                          mExpiryRelative);

  cookieNative.forget(aCookie);
  return NS_OK;
}

}  // namespace mozilla
