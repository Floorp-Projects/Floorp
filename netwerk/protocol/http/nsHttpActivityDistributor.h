/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpActivityDistributor_h__
#define nsHttpActivityDistributor_h__

#include "nsIHttpActivityObserver.h"
#include "nsTArray.h"
#include "nsProxyRelease.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace net {

class nsHttpActivityDistributor : public nsIHttpActivityDistributor {
 public:
  using ObserverArray =
      nsTArray<nsMainThreadPtrHandle<nsIHttpActivityObserver>>;
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHTTPACTIVITYOBSERVER
  NS_DECL_NSIHTTPACTIVITYDISTRIBUTOR

  nsHttpActivityDistributor() = default;

 protected:
  virtual ~nsHttpActivityDistributor() = default;

  ObserverArray mObservers;
  Mutex mLock{"nsHttpActivityDistributor.mLock"};
  bool mActivated{false};
};

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpActivityDistributor_h__
