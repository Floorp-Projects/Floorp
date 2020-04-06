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

  void WriteCookieToDB(const nsACString& aBaseDomain,
                       const OriginAttributes& aOriginAttributes,
                       mozilla::net::Cookie* aCookie,
                       mozIStorageBindingParamsArray* aParamsArray) override{};

  void RemoveAllInternal() override {}

  void RemoveCookieFromListInternal(
      const CookieListIter& aIter,
      mozIStorageBindingParamsArray* aParamsArray = nullptr) override {}

  void MaybeCreateDeleteBindingParamsArray(
      mozIStorageBindingParamsArray** aParamsArray) override {}

  void DeleteFromDB(mozIStorageBindingParamsArray* aParamsArray) override {}
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookiePrivateStorage_h
