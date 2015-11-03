/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CaptivePortalService.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"

#define kInterfaceName "captive-portal-inteface"

static const char kOpenCaptivePortalLoginEvent[] = "captive-portal-login";
static const char kAbortCaptivePortalLoginEvent[] = "captive-portal-login-abort";
static const char kCaptivePortalLoginSuccessEvent[] = "captive-portal-login-success";

static const uint32_t kDefaultInterval = 60*1000; // check every 60 seconds

namespace mozilla {
namespace net {

static LazyLogModule gCaptivePortalLog("CaptivePortalService");
#undef LOG
#define LOG(args) MOZ_LOG(gCaptivePortalLog, mozilla::LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(CaptivePortalService, nsICaptivePortalService, nsIObserver,
                  nsISupportsWeakReference, nsITimerCallback,
                  nsICaptivePortalCallback)

CaptivePortalService::CaptivePortalService()
  : mState(UNKNOWN)
  , mStarted(false)
  , mInitialized(false)
  , mRequestInProgress(false)
  , mEverBeenCaptive(false)
  , mDelay(kDefaultInterval)
  , mSlackCount(0)
  , mMinInterval(kDefaultInterval)
  , mMaxInterval(25*kDefaultInterval)
  , mBackoffFactor(5.0)
{
  mLastChecked = TimeStamp::Now();
}

CaptivePortalService::~CaptivePortalService()
{
}

nsresult
CaptivePortalService::PerformCheck()
{
  LOG(("CaptivePortalService::PerformCheck mRequestInProgress:%d mInitialized:%d mStarted:%d\n",
        mRequestInProgress, mInitialized, mStarted));
  // Don't issue another request if last one didn't complete
  if (mRequestInProgress || !mInitialized || !mStarted) {
    return NS_OK;
  }

  nsresult rv;
  if (!mCaptivePortalDetector) {
    mCaptivePortalDetector =
      do_GetService("@mozilla.org/toolkit/captive-detector;1", &rv);
    if (NS_FAILED(rv)) {
        LOG(("Unable to get a captive portal detector\n"));
        return rv;
    }
  }

  LOG(("CaptivePortalService::PerformCheck - Calling CheckCaptivePortal\n"));
  mRequestInProgress = true;
  mCaptivePortalDetector->CheckCaptivePortal(
    MOZ_UTF16(kInterfaceName), this);
  return NS_OK;
}

nsresult
CaptivePortalService::RearmTimer()
{
  // Start a timer to recheck
  if (mTimer) {
    mTimer->Cancel();
  }

  if (!mTimer) {
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  }

  if (mTimer && mDelay > 0) {
    LOG(("CaptivePortalService - Reloading timer with delay %u\n", mDelay));
    return mTimer->InitWithCallback(this, mDelay, nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
}

nsresult
CaptivePortalService::Initialize()
{
  if (mInitialized || XRE_GetProcessType() != GeckoProcessType_Default) {
    return NS_OK;
  }
  mInitialized = true;

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, kOpenCaptivePortalLoginEvent, true);
    observerService->AddObserver(this, kAbortCaptivePortalLoginEvent, true);
    observerService->AddObserver(this, kCaptivePortalLoginSuccessEvent, true);
  }

  LOG(("Initialized CaptivePortalService\n"));
  return NS_OK;
}

nsresult
CaptivePortalService::Start()
{
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mStarted) {
    return NS_OK;
  }

  mStarted = true;
  mEverBeenCaptive = false;

  // Get the delay prefs
  Preferences::GetUint("network.captive-portal-service.minInterval", &mMinInterval);
  Preferences::GetUint("network.captive-portal-service.maxInterval", &mMaxInterval);
  Preferences::GetFloat("network.captive-portal-service.backoffFactor", &mBackoffFactor);

  LOG(("CaptivePortalService::Start min:%u max:%u backoff:%.2f\n",
       mMinInterval, mMaxInterval, mBackoffFactor));

  mSlackCount = 0;
  mDelay = mMinInterval;

  // When Start is called, perform a check immediately
  PerformCheck();
  RearmTimer();
  return NS_OK;
}

nsresult
CaptivePortalService::Stop()
{
  LOG(("CaptivePortalService::Stop\n"));

  if (!mStarted) {
    return NS_OK;
  }

  if (mTimer) {
    mTimer->Cancel();
  }
  mTimer = nullptr;
  mRequestInProgress = false;
  mStarted = false;
  if (mCaptivePortalDetector) {
    mCaptivePortalDetector->Abort(MOZ_UTF16(kInterfaceName));
  }
  mCaptivePortalDetector = nullptr;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// CaptivePortalService::nsICaptivePortalService
//-----------------------------------------------------------------------------

NS_IMETHODIMP
CaptivePortalService::GetState(int32_t *aState)
{
  *aState = UNKNOWN;
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP
CaptivePortalService::RecheckCaptivePortal()
{
  LOG(("CaptivePortalService::RecheckCaptivePortal\n"));

  // This is called for user activity. We need to reset the slack count,
  // so the checks continue to be quite frequent.
  mSlackCount = 0;
  mDelay = mMinInterval;

  PerformCheck();
  RearmTimer();
  return NS_OK;
}

NS_IMETHODIMP
CaptivePortalService::GetLastChecked(uint64_t *aLastChecked)
{
  double duration = (TimeStamp::Now() - mLastChecked).ToMilliseconds();
  *aLastChecked = static_cast<uint64_t>(duration);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// CaptivePortalService::nsITimer
// This callback gets called every mDelay miliseconds
// It issues a checkCaptivePortal operation if one isn't already in progress
//-----------------------------------------------------------------------------
NS_IMETHODIMP
CaptivePortalService::Notify(nsITimer *aTimer)
{
  LOG(("CaptivePortalService::Notify\n"));
  MOZ_ASSERT(aTimer == mTimer);

  PerformCheck();

  // This is needed because we don't want to always make requests very often.
  // Every 10 checks, we the delay is increased mBackoffFactor times
  // to a maximum delay of mMaxInterval
  mSlackCount++;
  if (mSlackCount % 10 == 0) {
    mDelay = mDelay * mBackoffFactor;
  }
  if (mDelay > mMaxInterval) {
    mDelay = mMaxInterval;
  }

  // Note - if mDelay is 0, the timer will not be rearmed.
  RearmTimer();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// CaptivePortalService::nsIObserver
//-----------------------------------------------------------------------------
NS_IMETHODIMP
CaptivePortalService::Observe(nsISupports *aSubject,
                              const char * aTopic,
                              const char16_t * aData)
{
  LOG(("CaptivePortalService::Observe() topic=%s\n", aTopic));
  if (!strcmp(aTopic, kOpenCaptivePortalLoginEvent)) {
    // A redirect or altered content has been detected.
    // The user needs to log in. We are in a captive portal.
    mState = LOCKED_PORTAL;
    mLastChecked = TimeStamp::Now();
    mEverBeenCaptive = true;
  } else if (!strcmp(aTopic, kCaptivePortalLoginSuccessEvent)) {
    // The user has successfully logged in. We have connectivity.
    mState = UNLOCKED_PORTAL;
    mLastChecked = TimeStamp::Now();
    mRequestInProgress = false;
    mSlackCount = 0;
    mDelay = mMinInterval;
    RearmTimer();
  } else if (!strcmp(aTopic, kAbortCaptivePortalLoginEvent)) {
    // The login has been aborted
    mRequestInProgress = false;
    mState = UNKNOWN;
    mLastChecked = TimeStamp::Now();
    mSlackCount = 0;
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// CaptivePortalService::nsICaptivePortalCallback
//-----------------------------------------------------------------------------
NS_IMETHODIMP
CaptivePortalService::Prepare()
{
  LOG(("CaptivePortalService::Prepare\n"));
  // XXX: Finish preparation shouldn't be called until dns and routing is available.
  if (mCaptivePortalDetector) {
    mCaptivePortalDetector->FinishPreparation(MOZ_UTF16(kInterfaceName));
  }
  return NS_OK;
}

NS_IMETHODIMP
CaptivePortalService::Complete(bool success)
{
  LOG(("CaptivePortalService::Complete(success=%d) mState=%d\n", success, mState));
  mLastChecked = TimeStamp::Now();
  if ((mState == UNKNOWN || mState == NOT_CAPTIVE) && success) {
    mState = NOT_CAPTIVE;
    // If this check succeeded and we have never been in a captive portal
    // since the service was started, there is no need to keep polling
    if (!mEverBeenCaptive) {
      mDelay = 0;
      if (mTimer) {
        mTimer->Cancel();
      }
    }
  }

  mRequestInProgress = false;
  return NS_OK;
}

} // namespace net
} // namespace mozilla
