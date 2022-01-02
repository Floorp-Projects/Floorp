/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CaptivePortalService_h_
#define CaptivePortalService_h_

#include "nsICaptivePortalService.h"
#include "nsICaptivePortalDetector.h"
#include "nsINamed.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsITimer.h"
#include "nsCOMArray.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace net {

class CaptivePortalService : public nsICaptivePortalService,
                             public nsIObserver,
                             public nsSupportsWeakReference,
                             public nsITimerCallback,
                             public nsICaptivePortalCallback,
                             public nsINamed {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICAPTIVEPORTALSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSICAPTIVEPORTALCALLBACK
  NS_DECL_NSINAMED

  nsresult Initialize();
  nsresult Start();
  nsresult Stop();

  static already_AddRefed<nsICaptivePortalService> GetSingleton();

  // This method is only called in the content process, in order to mirror
  // the captive portal state in the parent process.
  void SetStateInChild(int32_t aState);

 private:
  static const uint32_t kDefaultInterval = 60 * 1000;  // check every 60 seconds

  CaptivePortalService();
  virtual ~CaptivePortalService();
  nsresult PerformCheck();
  nsresult RearmTimer();
  void NotifyConnectivityAvailable(bool aCaptive);

  nsCOMPtr<nsICaptivePortalDetector> mCaptivePortalDetector;
  int32_t mState{UNKNOWN};

  nsCOMPtr<nsITimer> mTimer;
  bool mStarted{false};
  bool mInitialized{false};
  bool mRequestInProgress{false};
  bool mEverBeenCaptive{false};

  uint32_t mDelay{kDefaultInterval};
  int32_t mSlackCount{0};

  uint32_t mMinInterval{kDefaultInterval};
  uint32_t mMaxInterval{25 * kDefaultInterval};
  float mBackoffFactor{5.0};

  void StateTransition(int32_t aNewState);

  // This holds a timestamp when the last time when the captive portal check
  // has changed state.
  mozilla::TimeStamp mLastChecked;
};

}  // namespace net
}  // namespace mozilla

#endif  // CaptivePortalService_h_
