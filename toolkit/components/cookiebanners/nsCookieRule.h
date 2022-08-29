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

  explicit nsCookieRule(bool aIsOptOut, const nsACString& aName,
                        const nsACString& aValue, const nsACString& aHost,
                        const nsACString& aPath, int64_t aExpiryRelative,
                        const nsACString& aUnsetValue, bool aIsSecure,
                        bool aIsHttpOnly, bool aIsSession, int32_t aSameSite,
                        nsICookie::schemeType aSchemeMap);

 private:
  ~nsCookieRule() = default;

  nsCOMPtr<nsICookie> mCookie;
  int64_t mExpiryRelative{};
  nsCString mUnsetValue;
};

}  // namespace mozilla

#endif
