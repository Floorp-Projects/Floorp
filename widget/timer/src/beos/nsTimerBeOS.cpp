/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsVoidArray.h"
#include "nsTimerBeOS.h"
#include "nsCOMPtr.h"

#include <Application.h>
#include <Message.h>
#include <signal.h>

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

TimerManager nsTimerBeOS::sTimerManager;

TimerManager::TimerManager()
 : BList(40)
{
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

		// insert sorted into timer event list
		int32 count = CountItems();
		int32 pos;
		for(pos = 0; pos < count; pos++)
		{
			nsITimer *entry = (nsITimer *)ItemAtFast(pos);
			if(((nsTimerBeOS *)entry)->mSchedTime > ((nsTimerBeOS*)inRequest)->mSchedTime)
				break;
		}
		AddItem(inRequest, pos);
			
		if(pos == 0)
			// We need to wake the thread to wait on the newly added event
			release_sem(mSyncSem);

		mLocker.Unlock();
	}
}

bool TimerManager::RemoveRequest(nsITimer *inRequest)
{
	bool	found = false;

	if(mLocker.Lock())
	{
		if(RemoveItem(inRequest))
		{
			NS_RELEASE(inRequest);
			found = true;
		}

		mLocker.Unlock();
	}

	return found;
}

int32 TimerManager::sTimerThreadFunc(void *inData)
{
	return ((TimerManager *)inData)->TimerThreadFunc();
}

int32 TimerManager::TimerThreadFunc()
{
	char		portname[64];
	char		semname[64];
	port_id		eventport;
	sem_id		syncsem;
	PRThread	*cached = (PRThread *)-1;

	while(! mQuitRequested)
	{
		nsITimer *tobj = 0;

		mLocker.Lock();

		bigtime_t now = system_time();

		// Fire expired pending requests
		while((tobj = FirstRequest()) != 0 && ((nsTimerBeOS*)tobj)->mSchedTime <= now)
		{
			nsTimerBeOS *tobjbeos = (nsTimerBeOS *)tobj;
			
			RemoveItem((int32)0);
			mLocker.Unlock();

			if(! tobjbeos->mCanceled)
			{
				// fire it
				if(tobjbeos->mThread != cached)
				{
					sprintf(portname, "event%lx", (uint32)tobjbeos->mThread);
					sprintf(semname, "sync%lx", (uint32)tobjbeos->mThread);

					eventport = find_port(portname);
					syncsem = my_find_sem(semname);
					cached = tobjbeos->mThread;
				}

				// call timer synchronously so we're sure tobj is alive
				ThreadInterfaceData	 id;
				id.data = tobjbeos;
				id.sync = true;
				if(write_port(eventport, 'WMti', &id, sizeof(id)) == B_OK)
					while(acquire_sem(syncsem) == B_INTERRUPTED)
						;
			}
			NS_RELEASE(tobjbeos);

			mLocker.Lock();
		}
		mLocker.Unlock();

		if(acquire_sem_etc(mSyncSem, 1, B_ABSOLUTE_TIMEOUT, 
					tobj ? ((nsTimerBeOS *)tobj)->mSchedTime : B_INFINITE_TIMEOUT) == B_BAD_SEM_ID)
			break;
	}

	return B_OK;
}

//
// nsTimerBeOS
//
void nsTimerBeOS::FireTimeout()
{
	if( ! mCanceled)
	{
		if(mFunc != NULL)
			(*mFunc)(this, mClosure);	    // If there's a function, call it.
		else if(mCallback != NULL)
			mCallback->Notify(this);    // But if there's an interface, notify it.
	}
}

nsTimerBeOS::nsTimerBeOS()
{
	NS_INIT_REFCNT();
	mFunc			= 0;
	mCallback		= 0;
	mDelay			= 0;
	mClosure		= 0;
	mSchedTime			= 0;
	mCanceled		= false;
}

nsTimerBeOS::~nsTimerBeOS()
{
	Cancel();
	NS_IF_RELEASE(mCallback);
}

void nsTimerBeOS::SetDelay(PRUint32 aDelay)
{
	mDelay = aDelay;
	mSchedTime = system_time() + mDelay * 1000;

	NS_ADDREF(this);
	if (nsTimerBeOS::sTimerManager.RemoveRequest(this))
		nsTimerBeOS::sTimerManager.AddRequest(this);
	Release();	
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

    return Init(aDelay);
}

nsresult nsTimerBeOS::Init(nsITimerCallback *aCallback,
                PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType)
{
    mCallback = aCallback;
    NS_ADDREF(mCallback);

    return Init(aDelay);
}


nsresult nsTimerBeOS::Init(PRUint32 aDelay)
{
	mDelay = aDelay;
	NS_ADDREF(this);	// this is for clients of the timer
	mSchedTime = system_time() + aDelay * 1000;
	
	mThread = PR_GetCurrentThread();

	sTimerManager.AddRequest(this);
	
    return NS_OK;
}

NS_IMPL_ISUPPORTS(nsTimerBeOS, kITimerIID);


void nsTimerBeOS::Cancel()
{
	mCanceled = true;
	nsTimerBeOS::sTimerManager.RemoveRequest(this);
}

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


