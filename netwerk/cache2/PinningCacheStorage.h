/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PinninCacheStorage__h__
#define PinninCacheStorage__h__

#include "CacheStorage.h"

namespace mozilla {
namespace net {

class PinningCacheStorage : public CacheStorage
{
public:
  PinningCacheStorage(nsILoadContextInfo* aInfo)
    : CacheStorage(aInfo, true, false)
  {
  }

  virtual bool IsPinning() const override;
};

} // net
} // mozilla

#endif
