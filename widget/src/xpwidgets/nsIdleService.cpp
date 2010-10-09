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
 *  Mike Kristoffersen <moz@mikek.dk>
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
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsDebug.h"
#include "nsCOMArray.h"
#include "prinrval.h"
#include "mozilla/Services.h"

// observer topics used:
#define OBSERVER_TOPIC_IDLE "idle"
#define OBSERVER_TOPIC_BACK "back"
#define OBSERVER_TOPIC_IDLE_DAILY "idle-daily"
// interval in seconds between internal idle time requests.
#define MIN_IDLE_POLL_INTERVAL 5
#define MAX_IDLE_POLL_INTERVAL 300 /* 5 min */
// Pref for last time (seconds since epoch) daily notification was sent.
#define PREF_LAST_DAILY "idle.lastDailyNotification"
// Number of seconds in a day.
#define SECONDS_PER_DAY 86400

// Use this to find previously added observers in our array:
class IdleListenerComparator
{
public:
  PRBool Equals(IdleListener a, IdleListener b) const
  {
    return (a.observer == b.observer) &&
           (a.reqIdleTime == b.reqIdleTime);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// nsIdleServiceDaily

NS_IMPL_ISUPPORTS1(nsIdleServiceDaily, nsIObserver)

NS_IMETHODIMP
nsIdleServiceDaily::Observe(nsISupports *,
                            const char *,
                            const PRUnichar *)
{
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
  if (NS_SUCCEEDED(mIdleService->RemoveIdleObserver(this, MAX_IDLE_POLL_INTERVAL))) {
    mObservesIdle = false;
  }

  // Set the last idle-daily time pref.
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (pref) {
    PRInt32 nowSec = static_cast<PRInt32>(PR_Now() / PR_USEC_PER_SEC);
    (void)pref->SetIntPref(PREF_LAST_DAILY, nowSec);
  }

  // Start timer for the next check in one day.
  (void)mTimer->InitWithFuncCallback(DailyCallback, this, SECONDS_PER_DAY * 1000,
                                     nsITimer::TYPE_ONE_SHOT);

  return NS_OK;
}

nsIdleServiceDaily::nsIdleServiceDaily(nsIdleService* aIdleService)
  : mIdleService(aIdleService)
  , mObservesIdle(false)
  , mTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
  , mCategoryObservers(OBSERVER_TOPIC_IDLE_DAILY)
{
  // Check time of the last idle-daily notification.  If it was more than 24
  // hours ago listen for idle, otherwise set a timer for 24 hours from now.
  PRInt32 lastDaily = 0;
  PRInt32 nowSec = static_cast<PRInt32>(PR_Now() / PR_USEC_PER_SEC);
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (pref) {
    if (NS_FAILED(pref->GetIntPref(PREF_LAST_DAILY, &lastDaily)) ||
        lastDaily < 0 || lastDaily > nowSec) {
      // The time is bogus, use default.
      lastDaily = 0;
    }
  }

  // Check if it has been a day since the last notification.
  if (nowSec - lastDaily > SECONDS_PER_DAY) {
    // Wait for the user to become idle, so we can do todays idle tasks.
    DailyCallback(nsnull, this);
  }
  else {
    // Start timer for the next check in one day.
    (void)mTimer->InitWithFuncCallback(DailyCallback, this, SECONDS_PER_DAY * 1000,
                                       nsITimer::TYPE_ONE_SHOT);
  }
}

void
nsIdleServiceDaily::Shutdown()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
  if (mIdleService && mObservesIdle) {
    if (NS_SUCCEEDED(mIdleService->RemoveIdleObserver(this, MAX_IDLE_POLL_INTERVAL))) {
      mObservesIdle = false;
    }
    mIdleService = nsnull;
  }
}

// static
void
nsIdleServiceDaily::DailyCallback(nsITimer* aTimer, void* aClosure)
{
  nsIdleServiceDaily* me = static_cast<nsIdleServiceDaily*>(aClosure);

  // The one thing we do every day is to start waiting for the user to "have
  // a significant idle time".
  if (NS_SUCCEEDED(me->mIdleService->AddIdleObserver(me, MAX_IDLE_POLL_INTERVAL))) {
    me->mObservesIdle = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
//// nsIdleService

nsIdleService::nsIdleService() : mLastIdleReset(0)
                               , mLastHandledActivity(0)
                               , mPolledIdleTimeIsValid(false)
{
  mDailyIdle = new nsIdleServiceDaily(this);
}

nsIdleService::~nsIdleService()
{
  StopTimer();
  mDailyIdle->Shutdown();
}

NS_IMETHODIMP
nsIdleService::AddIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_ARG(aIdleTime);

  // Put the time + observer in a struct we can keep:
  IdleListener listener(aObserver, aIdleTime);

  if (!mArrayListeners.AppendElement(listener)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Create our timer callback if it's not there already.
  if (!mTimer) {
    nsresult rv;
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Make sure our observer goes into 'idle' immediately if applicable.
  CheckAwayState(false);

  return NS_OK;
}

NS_IMETHODIMP
nsIdleService::RemoveIdleObserver(nsIObserver* aObserver, PRUint32 aTime)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_ARG(aTime);
  IdleListener listener(aObserver, aTime);

  // Find the entry and remove it:
  IdleListenerComparator c;
  if (mArrayListeners.RemoveElement(listener, c)) {
    if (mArrayListeners.IsEmpty()) {
      StopTimer();
    }
    return NS_OK;
  }

  // If we get here, we haven't removed anything:
  return NS_ERROR_FAILURE;
}

void
nsIdleService::ResetIdleTimeOut()
{
  // A zero in mLastIdleReset indicates that this function has never been
  // called.
  bool calledBefore = mLastIdleReset != 0;
  mLastIdleReset = PR_IntervalToSeconds(PR_IntervalNow());
  if (!mLastIdleReset) mLastIdleReset = 1;

  // Now check if this changes anything
  // Note that if we have never been called before, we cannot do the
  // optimization of passing true to CheckAwayState, which avoids
  // calculating the timer (because if we have never been called before,
  // we need to recalculate the timer and start it there).
  CheckAwayState(calledBefore);
}

NS_IMETHODIMP
nsIdleService::GetIdleTime(PRUint32* idleTime)
{
  // Check sanity of in parameter.
  if (!idleTime) {
    return NS_ERROR_NULL_POINTER;
  }

  // Polled idle time in ms
  PRUint32 polledIdleTimeMS;

  mPolledIdleTimeIsValid = PollIdleTime(&polledIdleTimeMS);

  // If we don't have any valid data, then we are not in idle - pr. definition.
  if (!mPolledIdleTimeIsValid && 0 == mLastIdleReset) {
    *idleTime = 0;
    return NS_OK;
  }

  // If we never got a reset, just return the pulled time.
  if (0 == mLastIdleReset) {
    *idleTime = polledIdleTimeMS;
    return NS_OK;
  }

  // timeSinceReset is in seconds.
  PRUint32 timeSinceReset =
    PR_IntervalToSeconds(PR_IntervalNow()) - mLastIdleReset;

  // If we did't get pulled data, return the time since last idle reset.
  if (!mPolledIdleTimeIsValid) {
    // We need to convert to ms before returning the time.
    *idleTime = timeSinceReset * 1000;
    return NS_OK;
  }

  // Otherwise return the shortest time detected (in ms).
  *idleTime = NS_MIN(timeSinceReset * 1000, polledIdleTimeMS);

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
nsIdleService::IdleTimerCallback(nsITimer* aTimer, void* aClosure)
{
  static_cast<nsIdleService*>(aClosure)->CheckAwayState(false);
}

void
nsIdleService::CheckAwayState(bool aNoTimeReset)
{
  /**
   * Find our last detected idle time (it's important this happens before the
   * call below to GetIdleTime, as we use the two values to detect if there
   * has been user activity since the last time we were here).
   */
  PRUint32 curTime = static_cast<PRUint32>(PR_Now() / PR_USEC_PER_SEC);
  PRUint32 lastTime = curTime - mLastHandledActivity;

  // Get the idle time (in seconds).
  PRUint32 idleTime;
  if (NS_FAILED(GetIdleTime(&idleTime))) {
    return;
  }

  // If we have no valid data about the idle time, stop
  if (!mPolledIdleTimeIsValid && 0 == mLastIdleReset) {
    return;
  }

  // GetIdleTime returns the time in ms, internally we only calculate in s.
  idleTime /= 1000;

  // We need a text string to send with any state change events.
  nsAutoString timeStr;
  timeStr.AppendInt(idleTime);

  // Set the time for last user activity.
  mLastHandledActivity = curTime - idleTime;

  /**
   * Now, if the idle time, is less than what we expect, it means the
   * user was active since last time that we checked.
   */
  nsCOMArray<nsIObserver> notifyList;

  if (lastTime > idleTime) {
    // Loop trough all listeners, and find any that have detected idle.
    for (PRUint32 i = 0; i < mArrayListeners.Length(); i++) {
      IdleListener& curListener = mArrayListeners.ElementAt(i);

      if (curListener.isIdle) {
        notifyList.AppendObject(curListener.observer);
        curListener.isIdle = false;
      }
    }

    // Send the "non-idle" events.
    for (PRInt32 i = 0; i < notifyList.Count(); i++) {
      notifyList[i]->Observe(this, OBSERVER_TOPIC_BACK, timeStr.get());
    }
  }

  /**
   * Now we need to check for listeners that have expired, and while we are
   * looping through all the elements, we will also calculate when, if ever
   * the next one will need to be notified.
   */

  // Clean up the list, so it's ready for the next iteration.
  notifyList.Clear();

  // Bail out if we don't need to calculate new times.
  if (aNoTimeReset) {
    return;
  }
  /**
   * Placet to store the wait time to the next notification, note that
   * PR_UINT32_MAX means no-one are listening (or that they have such a big
   * delay that it doesn't matter).
   */
  PRUint32 nextWaitTime = PR_UINT32_MAX;

  /**
   * Place to remember if there are any listeners that are in the idle state,
   * if there are, we need to poll frequently in a polling environment to detect
   * when the user becomes active again.
   */
  bool anyOneIdle = false;

  for (PRUint32 i = 0; i < mArrayListeners.Length(); i++) {
    IdleListener& curListener = mArrayListeners.ElementAt(i);

    // We are only interested in items, that are not in the idle state.
    if (!curListener.isIdle) {
      // If they have an idle time smaller than the actual idle time.
      if (curListener.reqIdleTime <= idleTime) {
        // then add the listener to the list of listeners that should be
        // notified.
        notifyList.AppendObject(curListener.observer);
        // This listener is now idle.
        curListener.isIdle = true;
      } else {
        // If it hasn't expired yet, then we should note the time when it should
        // expire.
        nextWaitTime = PR_MIN(nextWaitTime, curListener.reqIdleTime);
      }
    }

    // Remember if anyone becomes idle (it's safe to do this as a binary compare
    // as we are or'ing).
    anyOneIdle |= curListener.isIdle;
  }

  // In order to find when the next idle event should time out, we need to
  // subtract the time we should wait, from the time that has already passed.
  nextWaitTime -= idleTime;

  // Notify all listeners that just timed out.
  for (PRInt32 i = 0; i < notifyList.Count(); i++) {
    notifyList[i]->Observe(this, OBSERVER_TOPIC_IDLE, timeStr.get());
  }

  // If we are in poll mode, we need to poll for activity if anyone are idle,
  // otherwise we can wait polling until they would expire.
  if (UsePollMode() &&
      anyOneIdle &&
      nextWaitTime > MIN_IDLE_POLL_INTERVAL) {
    nextWaitTime = MIN_IDLE_POLL_INTERVAL;
  }

  // Start the timer if there is anything to wait for.
  if (PR_UINT32_MAX != nextWaitTime) {
    StartTimer(nextWaitTime);
  }
}

void
nsIdleService::StartTimer(PRUint32 aDelay)
{
  if (mTimer) {
    StopTimer();
    if (aDelay) {
      mTimer->InitWithFuncCallback(IdleTimerCallback, this, aDelay*1000,
                                   nsITimer::TYPE_ONE_SHOT);
    }
  }
}

void
nsIdleService::StopTimer()
{
  if (mTimer) {
    mTimer->Cancel();
  }
}

