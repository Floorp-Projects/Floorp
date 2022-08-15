/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CachePurgeLock_h__
#define mozilla_net_CachePurgeLock_h__

#include "nsICachePurgeLock.h"
#include "mozilla/MultiInstanceLock.h"

namespace mozilla::net {

class CachePurgeLock : public nsICachePurgeLock {
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHEPURGELOCK
 private:
  virtual ~CachePurgeLock() = default;

  MultiInstLockHandle mLock = MULTI_INSTANCE_LOCK_HANDLE_ERROR;
};

}  // namespace mozilla::net

#endif  // mozilla_net_CachePurgeLock_h__
