/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *	Yannick Koehler <ykoehler@mythrium.com>
 *	Chris Seawood <cls@seawood.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDebug.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsTimerBeOS.h"

#include <OS.h>
#include <Application.h>
#include <Message.h>
#include <signal.h>

#define TIMER_INTERVAL 10000 // 10 millisecs

//#define TIMER_DEBUG 1

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

struct ThreadInterfaceData
{
	void	*data;
	int32	sync;
};

static sem_id my_find_sem(const char *name)
{
	sem_id	ret = B_ERROR;

	/* Get the sem_info for every sempahore in this team. */
	sem_info info;
	int32 cookie = 0;

	while(get_next_sem_info(0, &cookie, &info) == B_OK)
		if(strcmp(name, info.name) == 0)
		{
			ret = info.sem;
			break;
		}

	return ret;
}

static int sort_timers (const void *a, const void *b)
{
        // Should state potentially stupid assumption:
        // timers will never be null
        if ((*(nsTimerBeOS **)a)->GetSchedTime() < (*(nsTimerBeOS **)b)->GetSchedTime())
                return -1;
        if ((*(nsTimerBeOS **)a)->GetSchedTime() > (*(nsTimerBeOS **)b)->GetSchedTime())
                return 1;
        return 0;
}

TimerManager nsTimerBeOS::sTimerManager;

TimerManager::TimerManager()
        : BList(40)
{
        mProcessing = 0;
	mQuitRequested = false;
	mSyncSem = create_sem(0, "timer sync");
	if(mSyncSem < 0)
		debugger("Failed to create sem...");
	
	mTimerThreadID = spawn_thread(&sTimerThreadFunc, "timer roster", B_URGENT_DISPLAY_PRIORITY, (void *)this);
	if(mTimerThreadID < B_OK)
		// is it the right way ?
		debugger("Failed to spawn timer thread...");
	
	resume_thread(mTimerThreadID);
}

TimerManager::~TimerManager()
{
	// should we empty the list here and NS_RELEASE ?
	mQuitRequested = true;
	delete_sem(mSyncSem);

	int32 junk;
	wait_for_thread(mTimerThreadID, &junk);
}

void TimerManager::AddRequest(nsITimer *inRequest)
{
	if(mLocker.Lock())
	{
		NS_ADDREF(inRequest);	// this is for the timer list

                bool was_empty = IsEmpty();
                AddItem(inRequest);
                SortItems(sort_timers);

                // We need to wake the thread to wait on the newly added event
		if (was_empty)
			release_sem(mSyncSem);

		mLocker.Unlock();
	}
}

bool TimerManager::RemoveRequest(nsITimer *inRequest)
{
        if ((nsTimerBeOS *)mProcessing == inRequest) {
#ifdef TIMER_DEBUG
                fprintf(stderr, "Request being processed, cannot remove %p\n", inRequest);
#endif
                return true;
        }

#ifdef TIMER_DEBUG
        fprintf(stderr, "Our request isn't being processed, delete it %p != %p\n", 
                (nsTimerBeOS *)mProcessing, inRequest);
#endif
        if(mLocker.Lock())
        {
                if (RemoveItem(inRequest))
                        NS_RELEASE(inRequest);
                mLocker.Unlock();
        } else {
                NS_WARNING("Could not get lock for RemoveRequest");
        }

	return true;
}

void TimerManager::SortRequests(void)
{
        if (mLocker.Lock()) 
        {
                SortItems(sort_timers);
                mLocker.Unlock();
        }
}

int32 TimerManager::sTimerThreadFunc(void *inData)
{
	return ((TimerManager *)inData)->TimerThreadFunc();
}

int32 TimerManager::TimerThreadFunc()
{
	char		portname[64];
	char		semname[64];
	port_id		eventport = 0;
	sem_id		syncsem = 0;
	PRThread	*cached = (PRThread *)-1;
        bigtime_t	now = system_time();
                
	while(! mQuitRequested)
	{
                nsITimer *tobj = 0;
                status_t ac_status;

                // Only process timers once every TIMER_INTERVAL
                snooze_until(now + TIMER_INTERVAL, B_SYSTEM_TIMEBASE);
                now = system_time();

		// Fire expired pending requests
                while (1)
		{
			nsTimerBeOS *tobjbeos;

                        if (!mLocker.Lock())
                                continue;

                        tobj = FirstRequest();
                        mLocker.Unlock();

                        // No more entries
                        if (!tobj)
                                break;

                        if (mProcessing)
                                continue;

                        tobjbeos = (nsTimerBeOS *)tobj;
                        mProcessing = tobjbeos;

                        // Since requests are sorted,
                        // exit loop if we reach a request
                        // that's later than now
                        if (tobjbeos->GetSchedTime() > now)
                        {
                                mProcessing = 0;
                                break;
                        }

                        if (tobjbeos->IsCanceled())
                        {
                                if (mLocker.Lock()) 
                                {
                                        if (RemoveItem((int32)0))
                                                NS_RELEASE(tobjbeos);
                                        mLocker.Unlock();
                                } else 
                                {
                                        NS_WARNING("could not get Lock()");
                                }
                                mProcessing = 0;
                                continue;
                        }

                        // At this point we should have the sem
			// fire timeout

//#define FIRE_CALLBACKS_FROM_TIMER_THREAD 1

#ifndef FIRE_CALLBACKS_FROM_TIMER_THREAD
                        PRThread *thread = tobjbeos->GetThread();

                        if(thread != cached)
                        {
                                sprintf(portname, "event%lx", (uint32)thread);
                                sprintf(semname, "sync%lx", (uint32)thread);
                                
                                eventport = find_port(portname);
                                syncsem = my_find_sem(semname);
                                cached = thread;
                        }

			// call timer synchronously so we're sure tobj is alive
                        ThreadInterfaceData	 id;
                        id.data = tobjbeos;
                        id.sync = true;

                        if(write_port(eventport, 'WMti', &id, sizeof(id)) == B_OK)
#else
                                tobjbeos->FireTimeout();
#endif
                        {
                                ac_status = B_INTERRUPTED;
                                while(ac_status == B_INTERRUPTED)
                                {
                                        ac_status = acquire_sem(syncsem);
#ifdef TIMER_DEBUG
                                        if (ac_status == B_BAD_SEM_ID)
                                                fprintf(stderr, "Bad semaphore id for syncsem %s\n", semname);
#endif
                                }
                        }

                        NS_ASSERTION(tobjbeos, "how did they delete it?");

                        // Check if request has been cancelled in callback
                        if (tobjbeos->IsCanceled())
                        {
                                if (mLocker.Lock()) {
                                        if (HasItem(tobjbeos))
                                        {
#ifdef TIMER_DEBUG
                                                fprintf(stderr, "Removing previously canceled item %p\n", tobjbeos);
#endif
                                                if (RemoveItem(tobjbeos))
                                                        NS_RELEASE(tobjbeos);
                                        }
                                        mLocker.Unlock();
                                } else
                                {
                                        NS_WARNING("cannot get lock");
                                }
                                mProcessing = 0;
                                continue;
                        }

                        if (tobjbeos->GetType() == NS_TYPE_ONE_SHOT)
                        {                           
                                if (mLocker.Lock()) 
                                {
                                        if (RemoveItem(tobjbeos))
                                                NS_RELEASE(tobjbeos);
                                        mLocker.Unlock();
                                } else 
                                {
                                        NS_WARNING("Unable to get lock to remove processed one shot\n");
                                }
                        } else
                        {
                                tobjbeos->SetDelay(tobjbeos->GetDelay());
                        }

                        mProcessing = 0;

#ifdef TIMER_DEBUG
                        fprintf (stderr, "Loop again\n");
#endif
                }

                if (tobj)
                {
                        ac_status = acquire_sem_etc(mSyncSem, 1, B_ABSOLUTE_TIMEOUT, 
                                                           ((nsTimerBeOS *)tobj)->GetSchedTime());
                } else 
                {
                        ac_status = acquire_sem(mSyncSem);
                }
                if (ac_status == B_BAD_SEM_ID)
                {
#ifdef TIMER_DEBUG
                        fprintf(stderr, "end loop bad sem\n");
#endif
                        break;
                }
	}

	return B_OK;
}

//
// nsTimerBeOS
//
void nsTimerBeOS::FireTimeout()
{
        NS_ADDREF_THIS();
	if( ! mCanceled)
	{
		if(mFunc != NULL)
			(*mFunc)(this, mClosure);	    // If there's a function, call it.
		else if(mCallback != NULL)
			mCallback->Notify(this);    // But if there's an interface, notify it.
	}
        NS_RELEASE_THIS();
}

nsTimerBeOS::nsTimerBeOS()
{
	NS_INIT_REFCNT();
	mFunc			= 0;
	mCallback		= 0;
	mDelay			= 0;
	mClosure		= 0;
	mSchedTime		= 0;
        mPriority		= 0;
        mType			= NS_TYPE_ONE_SHOT;
	mCanceled		= false;
}

nsTimerBeOS::~nsTimerBeOS()
{
#ifdef TIMER_DEBUG
        fprintf(stderr, "nsTimerBeOS Destructor\n");
#endif
	Cancel();
	NS_IF_RELEASE(mCallback);
}

void nsTimerBeOS::SetDelay(PRUint32 aDelay)
{
	mDelay = aDelay;
	mSchedTime = system_time() + mDelay * 1000;

        // Since we could be resetting the delay on a timer
        // that the manager already knows about,
        // make the manager resort its list
        nsTimerBeOS::sTimerManager.SortRequests();
}

void nsTimerBeOS::SetPriority(PRUint32 aPriority)
{
  mPriority = aPriority;
}

void nsTimerBeOS::SetType(PRUint32 aType)
{
  mType = aType;
}

nsresult nsTimerBeOS::Init(nsTimerCallbackFunc aFunc, void *aClosure,
                PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType)
{
    mFunc = aFunc;
    mClosure = aClosure;

    return Init(aDelay, aType);
}

nsresult nsTimerBeOS::Init(nsITimerCallback *aCallback,
                PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType)
{
    mCallback = aCallback;
    NS_IF_ADDREF(mCallback);

    return Init(aDelay, aType);
}


nsresult nsTimerBeOS::Init(PRUint32 aDelay, PRUint32 aType)
{
	mDelay = aDelay;
        mType = aType;
	mSchedTime = system_time() + aDelay * 1000;
	
	mThread = PR_GetCurrentThread();

	sTimerManager.AddRequest(this);

        return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsTimerBeOS, nsITimer);


void nsTimerBeOS::Cancel()
{
        if (!mCanceled) {
                mCanceled = true;
                nsTimerBeOS::sTimerManager.RemoveRequest(this);
        }
#ifdef TIMER_DEBUG
        fprintf(stderr, "Cancel: %p with mRefCnt %d\n", this, mRefCnt);
#endif
}

#ifdef MOZ_MONOLITHIC_TOOLKIT
nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if(nsnull == aInstancePtrResult)
		return NS_ERROR_NULL_POINTER;

    nsTimerBeOS *timer = new nsTimerBeOS();
    if(nsnull == timer)
		return NS_ERROR_OUT_OF_MEMORY;

    return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}
#endif

