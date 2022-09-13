/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_nsCookieBannerRule_h__
#define mozilla_nsCookieBannerRule_h__

#include "nsICookieBannerRule.h"
#include "nsICookieRule.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsIClickRule;

namespace mozilla {

class nsCookieBannerRule final : public nsICookieBannerRule {
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIEBANNERRULE

 public:
  nsCookieBannerRule() = default;
  explicit nsCookieBannerRule(const nsACString& aDomain) : mDomain(aDomain) {}

 private:
  ~nsCookieBannerRule() = default;

  nsCString mId;
  nsCString mDomain;
  nsTArray<nsCOMPtr<nsICookieRule>> mCookiesOptOut;
  nsTArray<nsCOMPtr<nsICookieRule>> mCookiesOptIn;

  // Internal getter for easy access of cookie rule arrays.
  nsTArray<nsCOMPtr<nsICookieRule>>& Cookies(bool isOptOut);

  nsCOMPtr<nsIClickRule> mClickRule;
};

}  // namespace mozilla

#endif
