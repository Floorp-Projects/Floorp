/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookiePrivateStorage_h
#define mozilla_net_CookiePrivateStorage_h

#include "CookieStorage.h"

class nsICookieTransactionCallback;

namespace mozilla {
namespace net {

class CookiePrivateStorage final : public CookieStorage {
 public:
  static already_AddRefed<CookiePrivateStorage> Create();

  void StaleCookies(const nsTArray<Cookie*>& aCookieList,
                    int64_t aCurrentTimeInUsec) override;

  void Close() override{};

  void EnsureInitialized() override{};

  nsresult RunInTransaction(nsICookieTransactionCallback* aCallback) override {
    // It might make sense for this to be a no-op, or to return
    // `NS_ERROR_NOT_AVAILABLE`, or to evalute `aCallback` (in case it has
    // side-effects), but for now, just crash.
    MOZ_CRASH("RunInTransaction is not supported for private storage");
  };

 protected:
  const char* NotificationTopic() const override {
    return "private-cookie-changed";
  }

  void NotifyChangedInternal(nsICookieNotification* aNotification,
                             bool aOldCookieIsSession) override {}

  void RemoveAllInternal() override {}

  void RemoveCookieFromDB(const Cookie& aCookie) override {}

  already_AddRefed<nsIArray> PurgeCookies(int64_t aCurrentTimeInUsec,
                                          uint16_t aMaxNumberOfCookies,
                                          int64_t aCookiePurgeAge) override;

  void StoreCookie(const nsACString& aBaseDomain,
                   const OriginAttributes& aOriginAttributes,
                   Cookie* aCookie) override {}

 private:
  void CollectCookieJarSizeData() override{};
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookiePrivateStorage_h
