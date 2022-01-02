/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookiePrivateStorage.h"
#include "Cookie.h"

namespace mozilla {
namespace net {

// static
already_AddRefed<CookiePrivateStorage> CookiePrivateStorage::Create() {
  RefPtr<CookiePrivateStorage> storage = new CookiePrivateStorage();
  storage->Init();

  return storage.forget();
}

void CookiePrivateStorage::StaleCookies(const nsTArray<Cookie*>& aCookieList,
                                        int64_t aCurrentTimeInUsec) {
  int32_t count = aCookieList.Length();
  for (int32_t i = 0; i < count; ++i) {
    Cookie* cookie = aCookieList.ElementAt(i);

    if (cookie->IsStale()) {
      cookie->SetLastAccessed(aCurrentTimeInUsec);
    }
  }
}

// purges expired and old cookies in a batch operation.
already_AddRefed<nsIArray> CookiePrivateStorage::PurgeCookies(
    int64_t aCurrentTimeInUsec, uint16_t aMaxNumberOfCookies,
    int64_t aCookiePurgeAge) {
  RefPtr<CookiePrivateStorage> self = this;
  return PurgeCookiesWithCallbacks(
      aCurrentTimeInUsec, aMaxNumberOfCookies, aCookiePurgeAge,
      [self](const CookieListIter& iter) { self->RemoveCookieFromList(iter); },
      nullptr);
}

}  // namespace net
}  // namespace mozilla
