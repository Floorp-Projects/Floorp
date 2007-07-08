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
#include "nsIServiceManager.h"
#include "nsDebug.h"
#include "nsCOMArray.h"

// observer topics used:
#define OBSERVER_TOPIC_IDLE "idle"
#define OBSERVER_TOPIC_BACK "back"
// interval in milliseconds between internal idle time requests
#define IDLE_POLL_INTERVAL 5000



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
}

nsIdleService::~nsIdleService()
{
    if (mTimer)
        mTimer->Cancel();
}

NS_IMETHODIMP
nsIdleService::AddIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    NS_ENSURE_ARG(aIdleTime);

    // Put the time + observer in a struct we can keep:
    IdleListener listener(aObserver, aIdleTime);

    if (!mArrayListeners.AppendElement(listener))
        return NS_ERROR_OUT_OF_MEMORY;

    // Initialize our timer callback if it's not there already.
    if (!mTimer)
    {
        nsresult rv;
        mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return rv;
        mTimer->InitWithFuncCallback(IdleTimerCallback, this, IDLE_POLL_INTERVAL,
                                     nsITimer::TYPE_REPEATING_SLACK);
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
    IdleListener listener(aObserver, aTime);

    // Find the entry and remove it:
    IdleListenerComparator c;
    if (mArrayListeners.RemoveElement(listener, c))
    {
        if (mTimer && mArrayListeners.IsEmpty())
        {
            mTimer->Cancel();
            mTimer = nsnull;
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

    nsAutoString timeStr;
    timeStr.AppendInt(idleTime);

    // Change state first, and save observers that need notification, so
    // removing things will always work without upsetting notifications.
    nsCOMArray<nsIObserver> idleListeners;
    nsCOMArray<nsIObserver> hereListeners;
    for (PRUint32 i = 0; i < mArrayListeners.Length(); i++)
    {
        IdleListener& curListener = mArrayListeners.ElementAt(i);
        if ((curListener.reqIdleTime * 1000 <= idleTime) &&
            !curListener.isIdle)
        {
            curListener.isIdle = PR_TRUE;
            idleListeners.AppendObject(curListener.observer);
        }
        else if ((curListener.reqIdleTime * 1000 > idleTime) &&
                 curListener.isIdle)
        {
            curListener.isIdle = PR_FALSE;
            hereListeners.AppendObject(curListener.observer);
        }
    }

    // Notify listeners gone idle:
    for (PRInt32 i = 0; i < idleListeners.Count(); i++)
    {
        idleListeners[i]->Observe(this, OBSERVER_TOPIC_IDLE, timeStr.get());
    }

    // Notify listeners that came back:
    for (PRInt32 i = 0; i < hereListeners.Count(); i++)
    {
        hereListeners[i]->Observe(this, OBSERVER_TOPIC_BACK, timeStr.get());
    }
}

