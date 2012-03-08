/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Gijs Kruitbosch <gijskruitbosch@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Gijs Kruitbosch <gijskruitbosch@gmail.com>
 *  Edward Lee <edward.lee@engineering.uiuc.edu>
 *  Mike Kristoffersen <mozstuff@mikek.dk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIdleService.h"
#include "nsString.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsDebug.h"
#include "nsCOMArray.h"
#include "prinrval.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

// observer topics used:
#define OBSERVER_TOPIC_IDLE "idle"
#define OBSERVER_TOPIC_BACK "back"
#define OBSERVER_TOPIC_IDLE_DAILY "idle-daily"
// interval in milliseconds between internal idle time requests.
#define MIN_IDLE_POLL_INTERVAL_MSEC (5 * PR_MSEC_PER_SEC) /* 5 sec */

// Time used by the daily idle serivce to determine a significant idle time.
#define DAILY_SIGNIFICANT_IDLE_SERVICE_SEC 300 /* 5 min */
// Pref for last time (seconds since epoch) daily notification was sent.
#define PREF_LAST_DAILY "idle.lastDailyNotification"
// Number of seconds in a day.
#define SECONDS_PER_DAY 86400

// Use this to find previously added observers in our array:
class IdleListenerComparator
{
public:
  bool Equals(IdleListener a, IdleListener b) const
  {
    return (a.observer == b.observer) &&
           (a.reqIdleTime == b.reqIdleTime);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// nsIdleServiceDaily

NS_IMPL_ISUPPORTS2(nsIdleServiceDaily, nsIObserver, nsISupportsWeakReference)

NS_IMETHODIMP
nsIdleServiceDaily::Observe(nsISupports *,
                            const char *aTopic,
                            const PRUnichar *)
{
  if (strcmp(aTopic, "profile-after-change") == 0) {
    // We are back. Start sending notifications again.
    mShutdownInProgress = false;
    return NS_OK;
  }

  if (strcmp(aTopic, "xpcom-will-shutdown") == 0 ||
      strcmp(aTopic, "profile-change-teardown") == 0) {
    mShutdownInProgress = true;
  }

  if (mShutdownInProgress || strcmp(aTopic, OBSERVER_TOPIC_BACK) == 0) {
    return NS_OK;
  }
  MOZ_ASSERT(strcmp(aTopic, OBSERVER_TOPIC_IDLE) == 0);

  // Notify anyone who cares.
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  NS_ENSURE_STATE(observerService);
  (void)observerService->NotifyObservers(nsnull,
                                         OBSERVER_TOPIC_IDLE_DAILY,
                                         nsnull);

  // Notify the category observers.
  const nsCOMArray<nsIObserver> &entries = mCategoryObservers.GetEntries();
  for (PRInt32 i = 0; i < entries.Count(); ++i) {
    (void)entries[i]->Observe(nsnull, OBSERVER_TOPIC_IDLE_DAILY, nsnull);
  }

  // Stop observing idle for today.
  (void)mIdleService->RemoveIdleObserver(this,
                                         DAILY_SIGNIFICANT_IDLE_SERVICE_SEC);

  // Set the last idle-daily time pref.
  PRInt32 nowSec = static_cast<PRInt32>(PR_Now() / PR_USEC_PER_SEC);
  Preferences::SetInt(PREF_LAST_DAILY, nowSec);

  // Start timer for the next check in one day.
  (void)mTimer->InitWithFuncCallback(DailyCallback,
                                     this,
                                     SECONDS_PER_DAY * PR_MSEC_PER_SEC,
                                     nsITimer::TYPE_ONE_SHOT);

  return NS_OK;
}

nsIdleServiceDaily::nsIdleServiceDaily(nsIIdleService* aIdleService)
  : mIdleService(aIdleService)
  , mTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
  , mCategoryObservers(OBSERVER_TOPIC_IDLE_DAILY)
  , mShutdownInProgress(false)
{
}

void
nsIdleServiceDaily::Init()
{
  // Check time of the last idle-daily notification.  If it was more than 24
  // hours ago listen for idle, otherwise set a timer for 24 hours from now.
  PRInt32 nowSec = static_cast<PRInt32>(PR_Now() / PR_USEC_PER_SEC);
  PRInt32 lastDaily = Preferences::GetInt(PREF_LAST_DAILY, 0);
  if (lastDaily < 0 || lastDaily > nowSec) {
    // The time is bogus, use default.
    lastDaily = 0;
  }

  // Check if it has been a day since the last notification.
  if (nowSec - lastDaily > SECONDS_PER_DAY) {
    // Wait for the user to become idle, so we can do todays idle tasks.
    DailyCallback(nsnull, this);
  }
  else {
    // Start timer for the next check in one day.
    (void)mTimer->InitWithFuncCallback(DailyCallback,
                                       this,
                                       SECONDS_PER_DAY * PR_MSEC_PER_SEC,
                                       nsITimer::TYPE_ONE_SHOT);
  }

  // Register for when we should terminate/pause
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-will-shutdown", true);
    obs->AddObserver(this, "profile-change-teardown", true);
    obs->AddObserver(this, "profile-after-change", true);
  }
}

nsIdleServiceDaily::~nsIdleServiceDaily()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
}

// static
void
nsIdleServiceDaily::DailyCallback(nsITimer* aTimer, void* aClosure)
{
  nsIdleServiceDaily* me = static_cast<nsIdleServiceDaily*>(aClosure);

  // The one thing we do every day is to start waiting for the user to "have
  // a significant idle time".
  (void)me->mIdleService->AddIdleObserver(me,
                                          DAILY_SIGNIFICANT_IDLE_SERVICE_SEC);
}


/**
 * The idle services goal is to notify subscribers when a certain time has
 * passed since the last user interaction with the system.
 *
 * On some platforms this is defined as the last time user events reached this
 * application, on other platforms it is a system wide thing - the preferred
 * implementation is to use the system idle time, rather than the application
 * idle time, as the things depending on the idle service are likely to use
 * significant resources (network, disk, memory, cpu, etc.).
 *
 * When the idle service needs to use the system wide idle timer, it typically
 * needs to poll the idle time value by the means of a timer.  It needs to
 * poll fast when it is in active idle mode (when it has a listener in the idle
 * mode) as it needs to detect if the user is active in other applications.
 *
 * When the service is waiting for the first listener to become idle, or when
 * it is only monitoring application idle time, it only needs to have the timer
 * expire at the time the next listener goes idle.
 *
 * The core state of the service is determined by:
 *
 * - A list of listeners.
 *
 * - A boolean that tells if any listeners are in idle mode.
 *
 * - A delta value that indicates when, measured from the last non-idle time,
 *   the next listener should switch to idle mode.
 *
 * - An absolute time of the last time idle mode was detected (this is used to
 *   judge if we have been out of idle mode since the last invocation of the
 *   service.
 *
 * There are four entry points into the system:
 *
 * - A new listener is registered.
 *
 * - An existing listener is deregistered.
 *
 * - User interaction is detected.
 *
 * - The timer expires.
 *
 * When a new listener is added its idle timeout, is compared with the next idle
 * timeout, and if lower, that time is stored as the new timeout, and the timer
 * is reconfigured to ensure a timeout around the time the new listener should
 * timeout.
 *
 * If the next idle time is above the idle time requested by the new listener
 * it won't be informed until the timer expires, this is to avoid recursive
 * behavior and to simplify the code.  In this case the timer will be set to
 * about 10 ms.
 *
 * When an existing listener is deregistered, it is just removed from the list
 * of active listeners, we don't stop the timer, we just let it expire.
 *
 * When user interaction is detected, either because it was directly detected or
 * because we polled the system timer and found it to be unexpected low, then we
 * check the flag that tells us if any listeners are in idle mode, if there are
 * they are removed from idle mode and told so, and we reset our state
 * caculating the next timeout and restart the timer if needed.
 *
 * ---- Build in logic
 *
 * In order to avoid restarting the timer endlessly, the timer function has
 * logic that will only restart the timer, if the requested timeout is before
 * the current timeout.
 *
 */


////////////////////////////////////////////////////////////////////////////////
//// nsIdleService

nsIdleService::nsIdleService() : mCurrentlySetToTimeoutAtInPR(0),
                                 mAnyObserverIdle(false),
                                 mDeltaToNextIdleSwitchInS(PR_UINT32_MAX),
                                 mLastUserInteractionInPR(0)
{
  mDailyIdle = new nsIdleServiceDaily(this);
  mDailyIdle->Init();
}

nsIdleService::~nsIdleService()
{
  if(mTimer) {
    mTimer->Cancel();
  }
}

NS_IMETHODIMP
nsIdleService::AddIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTimeInS)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  // We don't accept idle time at 0, and we can't handle idle time that are too
  // high either - no more than ~136 years.
  NS_ENSURE_ARG_RANGE(aIdleTimeInS, 1, (PR_UINT32_MAX / 10) - 1);

  // Put the time + observer in a struct we can keep:
  IdleListener listener(aObserver, aIdleTimeInS);

  if (!mArrayListeners.AppendElement(listener)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Create our timer callback if it's not there already.
  if (!mTimer) {
    nsresult rv;
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Check if the newly added observer has a smaller wait time than what we
  // are wating for now.
  if (mDeltaToNextIdleSwitchInS > aIdleTimeInS) {
    // If it is, then this is the next to move to idle (at this point we
    // don't care if it should have switched already).
    mDeltaToNextIdleSwitchInS = aIdleTimeInS;
  }

  // Ensure timer is running.
  ReconfigureTimer();

  return NS_OK;
}

NS_IMETHODIMP
nsIdleService::RemoveIdleObserver(nsIObserver* aObserver, PRUint32 aTimeInS)
{

  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_ARG(aTimeInS);
  IdleListener listener(aObserver, aTimeInS);

  // Find the entry and remove it, if it was the last entry, we just let the
  // existing timer run to completion (there might be a new registration in a
  // little while.
  IdleListenerComparator c;
  if (mArrayListeners.RemoveElement(listener, c)) {
    return NS_OK;
  }

  // If we get here, we haven't removed anything:
  return NS_ERROR_FAILURE;
}

void
nsIdleService::ResetIdleTimeOut(PRUint32 idleDeltaInMS)
{
  // Store the time
  mLastUserInteractionInPR = PR_Now() - (idleDeltaInMS * PR_USEC_PER_MSEC);

  // If no one is idle, then we are done, any existing timers can keep running.
  if (!mAnyObserverIdle) {
    return;
  }

  // Mark all idle services as non-idle, and calculate the next idle timeout.
  Telemetry::AutoTimer<Telemetry::IDLE_NOTIFY_BACK_MS> timer;
  nsCOMArray<nsIObserver> notifyList;
  mDeltaToNextIdleSwitchInS = PR_UINT32_MAX;

  // Loop through all listeners, and find any that have detected idle.
  for (PRUint32 i = 0; i < mArrayListeners.Length(); i++) {
    IdleListener& curListener = mArrayListeners.ElementAt(i);

    // If the listener was idle, then he shouldn't be any longer.
    if (curListener.isIdle) {
      notifyList.AppendObject(curListener.observer);
      curListener.isIdle = false;
    }

    // Check if the listener is the next one to timeout.
    mDeltaToNextIdleSwitchInS = PR_MIN(mDeltaToNextIdleSwitchInS,
                                       curListener.reqIdleTime);
  }

  // When we are done, then we wont have anyone idle.
  mAnyObserverIdle = false;

  // Restart the idle timer, and do so before anyone can delay us.
  ReconfigureTimer();

  PRInt32 numberOfPendingNotifications = notifyList.Count();
  Telemetry::Accumulate(Telemetry::IDLE_NOTIFY_BACK_LISTENERS,
                        numberOfPendingNotifications);

  // Bail if nothing to do.
  if (!numberOfPendingNotifications) {
    return;
  }

  // Now send "back" events to all, if any should have timed out allready, then
  // they will be reawaken by the timer that is already running.

  // We need a text string to send with any state change events.
  nsAutoString timeStr;

  timeStr.AppendInt((PRInt32)(idleDeltaInMS / PR_MSEC_PER_SEC));

  // Send the "non-idle" events.
  while (numberOfPendingNotifications--) {
    notifyList[numberOfPendingNotifications]->Observe(this,
                                                      OBSERVER_TOPIC_BACK,
                                                      timeStr.get());
  }

}

NS_IMETHODIMP
nsIdleService::GetIdleTime(PRUint32* idleTime)
{
  // Check sanity of in parameter.
  if (!idleTime) {
    return NS_ERROR_NULL_POINTER;
  }

  // Polled idle time in ms.
  PRUint32 polledIdleTimeMS;

  bool polledIdleTimeIsValid = PollIdleTime(&polledIdleTimeMS);

  // If we don't have any valid data, then we are not in idle - pr. definition.
  if (!polledIdleTimeIsValid && 0 == mLastUserInteractionInPR) {
    *idleTime = 0;
    return NS_OK;
  }

  // If we never got a reset, just return the pulled time.
  if (0 == mLastUserInteractionInPR) {
    *idleTime = polledIdleTimeMS;
    return NS_OK;
  }

  // timeSinceReset is in milliseconds.
  PRUint32 timeSinceResetInMS = (PR_Now() - mLastUserInteractionInPR) /
                                PR_USEC_PER_MSEC;

  // If we did't get pulled data, return the time since last idle reset.
  if (!polledIdleTimeIsValid) {
    // We need to convert to ms before returning the time.
    *idleTime = timeSinceResetInMS;
    return NS_OK;
  }

  // Otherwise return the shortest time detected (in ms).
  *idleTime = NS_MIN(timeSinceResetInMS, polledIdleTimeMS);

  return NS_OK;
}


bool
nsIdleService::PollIdleTime(PRUint32* /*aIdleTime*/)
{
  // Default behavior is not to have the ability to poll an idle time.
  return false;
}

bool
nsIdleService::UsePollMode()
{
  PRUint32 dummy;
  return PollIdleTime(&dummy);
}

void
nsIdleService::StaticIdleTimerCallback(nsITimer* aTimer, void* aClosure)
{
  static_cast<nsIdleService*>(aClosure)->IdleTimerCallback();
}

void
nsIdleService::IdleTimerCallback(void)
{
  // Remember that we no longer have a timer running.
  mCurrentlySetToTimeoutAtInPR = 0;

  // Get the current idle time.
  PRUint32 currentIdleTimeInMS;

  if (NS_FAILED(GetIdleTime(&currentIdleTimeInMS))) {
    return;
  }

  // Check if we have had some user interaction we didn't handle previously
  // we do the caluculation in ms to lessen the chance for rounding errors to
  // trigger wrong results, it is also very important that we call PR_Now AFTER
  // the call to GetIdleTime().
  if (((PR_Now() - mLastUserInteractionInPR) / PR_USEC_PER_MSEC) >
      currentIdleTimeInMS)
  {
    // We had user activity, so handle that part first (to ensure the listeners
    // don't risk getting an non-idle after they get a new idle indication.
    ResetIdleTimeOut(currentIdleTimeInMS);

    // NOTE: We can't bail here, as we might have something already timed out.
  }

  // Find the idle time in S.
  PRUint32 currentIdleTimeInS = currentIdleTimeInMS / PR_MSEC_PER_SEC;

  // Restart timer and bail if no-one are expected to be in idle
  if (mDeltaToNextIdleSwitchInS > currentIdleTimeInS) {
    // If we didn't expect anyone to be idle, then just re-start the timer.
    ReconfigureTimer();
    return;
  }

  // Tell expired listeners they are expired,and find the next timeout
  Telemetry::AutoTimer<Telemetry::IDLE_NOTIFY_IDLE_MS> timer;

  // We need to initialise the time to the next idle switch.
  mDeltaToNextIdleSwitchInS = PR_UINT32_MAX;

  // Create list of observers that should be notified.
  nsCOMArray<nsIObserver> notifyList;

  for (PRUint32 i = 0; i < mArrayListeners.Length(); i++) {
    IdleListener& curListener = mArrayListeners.ElementAt(i);

    // We are only interested in items, that are not in the idle state.
    if (!curListener.isIdle) {
      // If they have an idle time smaller than the actual idle time.
      if (curListener.reqIdleTime <= currentIdleTimeInS) {
        // Then add the listener to the list of listeners that should be
        // notified.
        notifyList.AppendObject(curListener.observer);
        // This listener is now idle.
        curListener.isIdle = true;
      } else {
        // Listeners that are not timed out yet are candidates for timing out.
        mDeltaToNextIdleSwitchInS = PR_MIN(mDeltaToNextIdleSwitchInS,
                                           curListener.reqIdleTime);
      }
    }
  }

  // Restart the timer before any notifications that could slow us down are
  // done.
  ReconfigureTimer();

  PRInt32 numberOfPendingNotifications = notifyList.Count();
  Telemetry::Accumulate(Telemetry::IDLE_NOTIFY_IDLE_LISTENERS,
                        numberOfPendingNotifications);

  // Bail if nothing to do.
  if (!numberOfPendingNotifications) {
    return;
  }

  // Remember we have someone idle.
  mAnyObserverIdle = true;

  // We need a text string to send with any state change events.
  nsAutoString timeStr;
  timeStr.AppendInt(currentIdleTimeInS);

  // Notify all listeners that just timed out.
  while (numberOfPendingNotifications--) {
    notifyList[numberOfPendingNotifications]->Observe(this,
                                                      OBSERVER_TOPIC_IDLE,
                                                      timeStr.get());
  }
}

void
nsIdleService::SetTimerExpiryIfBefore(PRTime aNextTimeoutInPR)
{
  // Bail if we don't have a timer service.
  if (!mTimer) {
    return;
  }

  // If the new timeout is before the old one or we don't have a timer running,
  // then restart the timer.
  if (mCurrentlySetToTimeoutAtInPR > aNextTimeoutInPR ||
      !mCurrentlySetToTimeoutAtInPR) {

    mCurrentlySetToTimeoutAtInPR = aNextTimeoutInPR ;

    // Stop the current timer (it's ok to try'n stop it, even it isn't running).
    mTimer->Cancel();

    // Check that the timeout is actually in the future, otherwise make it so.
    PRTime currentTimeInPR = PR_Now();
    if (currentTimeInPR > mCurrentlySetToTimeoutAtInPR) {
      mCurrentlySetToTimeoutAtInPR = currentTimeInPR;
    }

    // Add 10 ms to ensure we don't undershoot, and never get a "0" timer.
    mCurrentlySetToTimeoutAtInPR += 10 * PR_USEC_PER_MSEC;

    // Start the timer
    mTimer->InitWithFuncCallback(StaticIdleTimerCallback,
                                 this,
                                 (mCurrentlySetToTimeoutAtInPR -
                                  currentTimeInPR) / PR_USEC_PER_MSEC,
                                 nsITimer::TYPE_ONE_SHOT);

  }
}


void
nsIdleService::ReconfigureTimer(void)
{
  // Check if either someone is idle, or someone will become idle.
  if (!mAnyObserverIdle && PR_UINT32_MAX == mDeltaToNextIdleSwitchInS) {
    // If not, just let any existing timers run to completion
    // And bail out.
    return;
  }

  // Find the next timeout value, assuming we are not polling.

  // We need to store the current time, so we don't get artifacts from the time
  // ticking while we are processing.
  PRTime curTimeInPR = PR_Now();

  PRTime nextTimeoutAtInPR = mLastUserInteractionInPR +
                             (((PRTime)mDeltaToNextIdleSwitchInS) *
                              PR_USEC_PER_SEC);

  // Check if we should correct the timeout time because we should poll before.
  if (mAnyObserverIdle && UsePollMode()) {
    PRTime pollTimeout = curTimeInPR +
                         MIN_IDLE_POLL_INTERVAL_MSEC * PR_USEC_PER_SEC;

    if (nextTimeoutAtInPR > pollTimeout) {
      nextTimeoutAtInPR = pollTimeout;
    }
  }

  SetTimerExpiryIfBefore(nextTimeoutAtInPR);
}

