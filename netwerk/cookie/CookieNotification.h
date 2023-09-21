/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieNotification_h
#define mozilla_net_CookieNotification_h

#include "nsIArray.h"
#include "nsICookieNotification.h"
#include "nsICookie.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla::net {

class CookieNotification final : public nsICookieNotification {
 public:
  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIENOTIFICATION

  explicit CookieNotification(nsICookieNotification::Action aAction,
                              nsICookie* aCookie, const nsACString& aBaseDomain,
                              nsIArray* aBatchDeletedCookies = nullptr,
                              uint64_t aBrowsingContextId = 0)
      : mAction(aAction),
        mCookie(aCookie),
        mBaseDomain(aBaseDomain),
        mBatchDeletedCookies(aBatchDeletedCookies),
        mBrowsingContextId(aBrowsingContextId){};

 private:
  nsICookieNotification::Action mAction;
  nsCOMPtr<nsICookie> mCookie;
  nsCString mBaseDomain;
  nsCOMPtr<nsIArray> mBatchDeletedCookies;
  uint64_t mBrowsingContextId = 0;

  ~CookieNotification() = default;
};

}  // namespace mozilla::net

#endif  // mozilla_net_CookieNotification_h
