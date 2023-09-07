/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_BounceTrackingStorageObserver_h__
#define mozilla_BounceTrackingStorageObserver_h__

#include "mozilla/Logging.h"
#include "nsIObserver.h"

namespace mozilla {

extern LazyLogModule gBounceTrackingProtectionLog;

class BounceTrackingStorageObserver final : public nsIObserver {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 public:
  BounceTrackingStorageObserver() = default;
  nsresult Init();

 private:
  ~BounceTrackingStorageObserver() = default;
};

}  // namespace mozilla

#endif
