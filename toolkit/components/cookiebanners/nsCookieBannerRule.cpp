/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookieBannerRule.h"

#include "mozilla/Logging.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsClickRule.h"
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
  LogRule(gCookieRuleLog, "AddCookie:", this, LogLevel::Debug);
  MOZ_LOG(
      gCookieRuleLog, LogLevel::Debug,
      ("%s: aIsOptOut: %d, aHost: %s, aName: %s", __FUNCTION__, aIsOptOut,
       nsPromiseFlatCString(aHost).get(), nsPromiseFlatCString(aName).get()));

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
nsCookieBannerRule::GetDomains(nsTArray<nsCString>& aDomains) {
  aDomains.Clear();

  AppendToArray(aDomains, mDomains);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::SetDomains(const nsTArray<nsCString>& aDomains) {
  AppendToArray(mDomains, aDomains);

  return NS_OK;
}

nsTArray<nsCOMPtr<nsICookieRule>>& nsCookieBannerRule::Cookies(bool isOptOut) {
  if (isOptOut) {
    return mCookiesOptOut;
  }
  return mCookiesOptIn;
}

NS_IMETHODIMP
nsCookieBannerRule::GetCookies(bool aIsOptOut, const nsACString& aDomain,
                               nsTArray<RefPtr<nsICookieRule>>& aCookies) {
  nsTArray<nsCOMPtr<nsICookieRule>>& cookies = Cookies(aIsOptOut);
  for (nsICookieRule* cookie : cookies) {
    // If we don't need to set a domain we can simply return the existing rule.
    if (aDomain.IsEmpty()) {
      aCookies.AppendElement(cookie);
      continue;
    }
    // Otherwise get a copy.
    nsCOMPtr<nsICookieRule> ruleForDomain;
    nsresult rv = cookie->CopyForDomain(aDomain, getter_AddRefs(ruleForDomain));
    NS_ENSURE_SUCCESS(rv, rv);
    aCookies.AppendElement(ruleForDomain.forget());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::GetCookiesOptOut(
    nsTArray<RefPtr<nsICookieRule>>& aCookies) {
  return GetCookies(true, ""_ns, aCookies);
}

NS_IMETHODIMP
nsCookieBannerRule::GetCookiesOptIn(nsTArray<RefPtr<nsICookieRule>>& aCookies) {
  return GetCookies(false, ""_ns, aCookies);
}

NS_IMETHODIMP
nsCookieBannerRule::GetIsGlobalRule(bool* aIsGlobalRule) {
  *aIsGlobalRule = mDomains.IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::GetClickRule(nsIClickRule** aClickRule) {
  NS_IF_ADDREF(*aClickRule = mClickRule);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::AddClickRule(const nsACString& aPresence,
                                 const bool aSkipPresenceVisibilityCheck,
                                 const nsIClickRule::RunContext aRunContext,
                                 const nsACString& aHide,
                                 const nsACString& aOptOut,
                                 const nsACString& aOptIn) {
  mClickRule =
      MakeRefPtr<nsClickRule>(this, aPresence, aSkipPresenceVisibilityCheck,
                              aRunContext, aHide, aOptOut, aOptIn);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerRule::ClearClickRule() {
  mClickRule = nullptr;

  return NS_OK;
}

// Static
void nsCookieBannerRule::LogRule(LazyLogModule& aLogger, const char* aMessage,
                                 nsICookieBannerRule* aRule,
                                 LogLevel aLogLevel) {
  NS_ENSURE_TRUE_VOID(aMessage);
  NS_ENSURE_TRUE_VOID(aRule);

  // Exit early if logging is disabled for the given log-level.
  if (!MOZ_LOG_TEST(aLogger, aLogLevel)) {
    return;
  }

  nsAutoCString id;
  nsresult rv = aRule->GetId(id);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsTArray<nsCString> domains;
  rv = aRule->GetDomains(domains);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Create a comma delimited string of domains this rule supports.
  nsAutoCString domainsStr("*");
  for (const nsCString& domain : domains) {
    if (domainsStr.EqualsLiteral("*")) {
      domainsStr.Truncate();
    } else {
      domainsStr.AppendLiteral(",");
    }
    domainsStr.Append(domain);
  }

  MOZ_LOG(aLogger, aLogLevel,
          ("%s Rule: id=%s; domains=[%s]; isGlobal: %d", aMessage, id.get(),
           PromiseFlatCString(domainsStr).get(), domains.IsEmpty()));
}

}  // namespace mozilla
