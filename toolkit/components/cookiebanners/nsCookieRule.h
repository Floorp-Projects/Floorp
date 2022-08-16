/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_nsCookieRule_h__
#define mozilla_nsCookieRule_h__

#include "nsICookieRule.h"
#include "nsICookie.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/StaticPrefs_cookiebanners.h"

namespace mozilla {

class nsCookieRule final : public nsICookieRule {
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIERULE

 public:
  nsCookieRule() = default;

  explicit nsCookieRule(
      bool aIsOptOut, const nsACString& aHost, const nsACString& aName,
      const nsACString& aValue, const nsACString& aPath = "/"_ns,
      int64_t aExpiryRelative =
          StaticPrefs::cookiebanners_cookieInjector_defaultExpiryRelative(),
      const nsACString& aUnsetValue = ""_ns, bool aIsSecure = true,
      bool aIsHttpOnly = false, bool aIsSession = false,
      int32_t aSameSite = nsICookie::SAMESITE_LAX,
      nsICookie::schemeType aSchemeMap = nsICookie::SCHEME_HTTPS);

 private:
  ~nsCookieRule() = default;

  nsCOMPtr<nsICookie> mCookie;
  int64_t mExpiryRelative{};
  nsCString mUnsetValue;
};

}  // namespace mozilla

#endif
