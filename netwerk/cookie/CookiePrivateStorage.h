/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookiePrivateStorage_h
#define mozilla_net_CookiePrivateStorage_h

#include "CookieStorage.h"

namespace mozilla {
namespace net {

class CookiePrivateStorage final : public CookieStorage {
 public:
  static already_AddRefed<CookiePrivateStorage> Create();

  void StaleCookies(const nsTArray<Cookie*>& aCookieList,
                    int64_t aCurrentTimeInUsec) override;

  void Close() override{};

 protected:
  const char* NotificationTopic() const override {
    return "private-cookie-changed";
  }

  void NotifyChangedInternal(nsISupports* aSubject, const char16_t* aData,
                             bool aOldCookieIsSession) override {}

  void RemoveAllInternal() override {}

  void RemoveCookieFromDB(const CookieListIter& aIter) override {}

  already_AddRefed<nsIArray> PurgeCookies(int64_t aCurrentTimeInUsec,
                                          uint16_t aMaxNumberOfCookies,
                                          int64_t aCookiePurgeAge) override;

  void StoreCookie(const nsACString& aBaseDomain,
                   const OriginAttributes& aOriginAttributes,
                   Cookie* aCookie) override {}
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookiePrivateStorage_h
