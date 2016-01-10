/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CaptivePortalService_h_
#define CaptivePortalService_h_

#include "nsICaptivePortalService.h"
#include "nsICaptivePortalDetector.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsITimer.h"
#include "nsCOMArray.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace net {

class CaptivePortalService
  : public nsICaptivePortalService
  , public nsIObserver
  , public nsSupportsWeakReference
  , public nsITimerCallback
  , public nsICaptivePortalCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICAPTIVEPORTALSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSICAPTIVEPORTALCALLBACK

  CaptivePortalService();
  nsresult Initialize();
  nsresult Start();
  nsresult Stop();
private:
  virtual ~CaptivePortalService();
  nsresult PerformCheck();
  nsresult RearmTimer();

  nsCOMPtr<nsICaptivePortalDetector>    mCaptivePortalDetector;
  int32_t                               mState;

  nsCOMPtr<nsITimer>                    mTimer;
  bool                                  mStarted;
  bool                                  mInitialized;
  bool                                  mRequestInProgress;
  bool                                  mEverBeenCaptive;

  uint32_t                              mDelay;
  int32_t                               mSlackCount;

  uint32_t                              mMinInterval;
  uint32_t                              mMaxInterval;
  float                                 mBackoffFactor;

  // This holds a timestamp when the last time when the captive portal check
  // has changed state.
  mozilla::TimeStamp                    mLastChecked;
};

} // namespace net
} // namespace mozilla

#endif // CaptivePortalService_h_
