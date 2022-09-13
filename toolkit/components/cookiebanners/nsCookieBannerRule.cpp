/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookieBannerRule.h"

#include "mozilla/Logging.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCookieRule.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(nsCookieBannerRule, nsICookieBannerRule)

LazyLogModule gCookieRuleLog("nsCookieBannerRule");

NS_IMETHODIMP
nsCookieBannerRule::ClearCookies() {
  mCookiesOptOut.Clear();
  mCookiesOptIn.Clear();

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::AddCookie(bool aIsOptOut, const nsACString& aName,
                              const nsACString& aValue, const nsACString& aHost,
                              const nsACString& aPath, int64_t aExpiryRelative,
                              const nsACString& aUnsetValue, bool aIsSecure,
                              bool aIsHttpOnly, bool aIsSession,
                              int32_t aSameSite,
                              nsICookie::schemeType aSchemeMap) {
  MOZ_LOG(gCookieRuleLog, LogLevel::Debug,
          ("%s: mDomain: %s, aIsOptOut: %d, aHost: %s, aName: %s", __FUNCTION__,
           mDomain.get(), aIsOptOut, nsPromiseFlatCString(aHost).get(),
           nsPromiseFlatCString(aName).get()));

  // Create and insert cookie rule.
  nsCOMPtr<nsICookieRule> cookieRule = new nsCookieRule(
      aIsOptOut, aName, aValue, aHost, aPath, aExpiryRelative, aUnsetValue,
      aIsSecure, aIsHttpOnly, aIsSession, aSameSite, aSchemeMap);
  Cookies(aIsOptOut).AppendElement(cookieRule);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::GetId(nsACString& aId) {
  aId.Assign(mId);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::SetId(const nsACString& aId) {
  mId.Assign(aId);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::GetDomain(nsACString& aDomain) {
  aDomain.Assign(mDomain);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::SetDomain(const nsACString& aDomain) {
  mDomain.Assign(aDomain);
  return NS_OK;
}

nsTArray<nsCOMPtr<nsICookieRule>>& nsCookieBannerRule::Cookies(bool isOptOut) {
  if (isOptOut) {
    return mCookiesOptOut;
  }
  return mCookiesOptIn;
}

NS_IMETHODIMP
nsCookieBannerRule::GetCookiesOptOut(
    nsTArray<RefPtr<nsICookieRule>>& aCookies) {
  nsTArray<nsCOMPtr<nsICookieRule>>& cookies = Cookies(true);
  for (nsICookieRule* cookie : cookies) {
    aCookies.AppendElement(cookie);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::GetCookiesOptIn(nsTArray<RefPtr<nsICookieRule>>& aCookies) {
  nsTArray<nsCOMPtr<nsICookieRule>>& cookies = Cookies(false);
  for (nsICookieRule* cookie : cookies) {
    aCookies.AppendElement(cookie);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::GetClickRule(nsIClickRule** aClickRule) {
  NS_IF_ADDREF(*aClickRule = mClickRule);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::AddClickRule(const nsACString& aPresence,
                                 const nsACString& aHide,
                                 const nsACString& aOptOut,
                                 const nsACString& aOptIn) {
  mClickRule = MakeRefPtr<nsClickRule>(aPresence, aHide, aOptOut, aOptIn);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::ClearClickRule() {
  mClickRule = nullptr;

  return NS_OK;
}

}  // namespace mozilla
