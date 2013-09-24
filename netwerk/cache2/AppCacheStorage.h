/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AppCacheStorage__h__
#define AppCacheStorage__h__

#include "CacheStorage.h"

#include "nsCOMPtr.h"
#include "nsILoadContextInfo.h"
#include "nsIApplicationCache.h"

class nsIApplicationCache;

namespace mozilla {
namespace net {

class AppCacheStorage : public CacheStorage
{
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICACHESTORAGE

public:
  AppCacheStorage(nsILoadContextInfo* aInfo,
                  nsIApplicationCache* aAppCache);

private:
  virtual ~AppCacheStorage();

  nsCOMPtr<nsIApplicationCache> mAppCache;
};

} // net
} // mozilla

#endif
