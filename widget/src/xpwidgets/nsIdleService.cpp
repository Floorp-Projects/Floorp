/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
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

// observer topics used:
#define OBSERVER_TOPIC_IDLE "idle"
#define OBSERVER_TOPIC_BACK "back"
#define OBSERVER_TOPIC_IDLE_DAILY "idle-daily"
// interval in milliseconds between internal idle time requests
#define MIN_IDLE_POLL_INTERVAL 5000
#define MAX_IDLE_POLL_INTERVAL 300000
// Pref for last time (seconds since epoch) daily notification was sent
#define PREF_LAST_DAILY "idle.lastDailyNotification"

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

nsIdleService::nsIdleService()
{
    // Immediately create a timer to handle the daily notification
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    StartTimer(MAX_IDLE_POLL_INTERVAL);
}

nsIdleService::~nsIdleService()
{
    StopTimer();
}

NS_IMETHODIMP
nsIdleService::AddIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    NS_ENSURE_ARG(aIdleTime);

    // Put the time + observer in a struct we can keep:
    IdleListener listener(aObserver, aIdleTime * 1000);

    if (!mArrayListeners.AppendElement(listener))
        return NS_ERROR_OUT_OF_MEMORY;

    // Create our timer callback if it's not there already
    if (!mTimer) {
        nsresult rv;
        mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // Make sure our observer goes into 'idle' immediately if applicable.
    CheckAwayState();

    return NS_OK;
}

NS_IMETHODIMP
nsIdleService::RemoveIdleObserver(nsIObserver* aObserver, PRUint32 aTime)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    NS_ENSURE_ARG(aTime);
    IdleListener listener(aObserver, aTime * 1000);

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
nsIdleService::IdleTimerCallback(nsITimer* aTimer, void* aClosure)
{
    static_cast<nsIdleService*>(aClosure)->CheckAwayState();
}

void
nsIdleService::CheckAwayState()
{
    // Get the idle time.
    PRUint32 idleTime;
    if (NS_FAILED(GetIdleTime(&idleTime)))
        return;

    // Dynamically figure out what's the best time to poll again
    PRUint32 nextPoll = MAX_IDLE_POLL_INTERVAL;

    nsAutoString timeStr;
    timeStr.AppendInt(idleTime);

    // Change state first, and save observers that need notification, so
    // removing things will always work without upsetting notifications.
    nsCOMArray<nsIObserver> idleListeners;
    nsCOMArray<nsIObserver> hereListeners;
    for (PRUint32 i = 0; i < mArrayListeners.Length(); i++) {
        IdleListener& curListener = mArrayListeners.ElementAt(i);

        // Assume the next best poll time is the time left before idle
        PRUint32 curPoll = curListener.reqIdleTime - idleTime;

        // For listeners that haven't gone idle yet:
        if (!curListener.isIdle) {
            // User has been idle longer than the listener expects
            if (idleTime >= curListener.reqIdleTime) {
                curListener.isIdle = PR_TRUE;
                idleListeners.AppendObject(curListener.observer);

                // We'll need to poll frequently to notice if the user is back
                curPoll = MIN_IDLE_POLL_INTERVAL;
            }
        }
        // For listeners that are waiting for the user to come back:
        else {
            // Short idle time means the user is back
            if (idleTime < curListener.reqIdleTime) {
                curListener.isIdle = PR_FALSE;
                hereListeners.AppendObject(curListener.observer);
            }
            // Keep polling frequently to detect if the user comes back
            else {
                curPoll = MIN_IDLE_POLL_INTERVAL;
            }
        }

        // Take the shortest poll time needed for each listener
        nextPoll = PR_MIN(nextPoll, curPoll);
    }

    // Notify listeners gone idle:
    for (PRInt32 i = 0; i < idleListeners.Count(); i++) {
        idleListeners[i]->Observe(this, OBSERVER_TOPIC_IDLE, timeStr.get());
    }

    // Notify listeners that came back:
    for (PRInt32 i = 0; i < hereListeners.Count(); i++) {
        hereListeners[i]->Observe(this, OBSERVER_TOPIC_BACK, timeStr.get());
    }

    // The user has been idle for a while, so try sending the daily idle
    if (idleTime >= MAX_IDLE_POLL_INTERVAL) {
        nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (pref) {
            // Get the current number of seconds since epoch
            PRUint32 nowSec = PR_Now() / PR_USEC_PER_SEC;

            // Get the last notification time; default to 0 for the first time
            PRInt32 lastDaily = 0;
            pref->GetIntPref(PREF_LAST_DAILY, &lastDaily);

            // Has it been a day (24*60*60 seconds) since the last notification
            if (nowSec - lastDaily > 86400) {
                nsCOMPtr<nsIObserverService> observerService =
                    do_GetService("@mozilla.org/observer-service;1");
                observerService->NotifyObservers(nsnull,
                                                 OBSERVER_TOPIC_IDLE_DAILY,
                                                 nsnull);

                pref->SetIntPref(PREF_LAST_DAILY, nowSec);
            }
        }
    }

    // Restart the timer with the dynamically optimized poll time
    StartTimer(nextPoll);
}

void
nsIdleService::StartTimer(PRUint32 aDelay)
{
    if (mTimer) {
        StopTimer();
        mTimer->InitWithFuncCallback(IdleTimerCallback, this, aDelay,
                                     nsITimer::TYPE_ONE_SHOT);
    }
}

void
nsIdleService::StopTimer()
{
    if (mTimer) {
        mTimer->Cancel();
    }
}

void
nsIdleService::IdleTimeWasModified()
{
    StartTimer(0);
}
