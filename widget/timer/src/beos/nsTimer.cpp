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

#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsCRT.h"
#include "prlog.h"
#include <stdio.h>
#include <limits.h>
#include <OS.h>
#include <Application.h>
#include <Autolock.h>
#include <Message.h>
#include <signal.h>
#include <List.h>
#include <prthread.h>

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


class TimerImpl;
class TimerManager;

class TimerManager : public BList
{
public:
					TimerManager();
					~TimerManager();
	void			AddRequest(TimerImpl *);
	bool			RemoveRequest(TimerImpl *);

private:
	BLocker			mLocker;
	sem_id			mSyncSem;
	thread_id		mTimerThreadID;
	bool			mQuitRequested;

	static int32	sTimerThreadFunc(void *);
	int32			TimerThreadFunc();
	TimerImpl		*FirstRequest()				{ return (TimerImpl *)FirstItem(); }
};


class TimerImpl : public nsITimer
{
	friend class TimerManager;
public:
					TimerImpl();
	virtual			~TimerImpl();
												
  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                );

  virtual nsresult Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                );
					
    NS_DECL_ISUPPORTS

    virtual void		Cancel();
    virtual PRUint32	GetDelay()					{ return mDelay; }
    virtual void		SetDelay(PRUint32);
  virtual PRUint32 GetPriority() {}
  virtual void SetPriority(PRUint32 aPriority) {}

  virtual PRUint32 GetType() {}
  virtual void SetType(PRUint32 aType) {}
    virtual void*		GetClosure()				{ return mClosure; }

	void				FireTimeout();

private:
    nsresult			Init(PRUint32 aDelay);	// Initialize the timer.

	bigtime_t			mWhen;		// Time when this request should be done
    PRUint32			mDelay;	    // The delay set in Init()
    nsTimerCallbackFunc	mFunc;	    // The function to call back when expired
    void				*mClosure;  // The argumnet to pass it.
    nsITimerCallback	*mCallback; // An interface to notify when expired.
	bool				mCanceled;
	PRThread			*mThread;
//  PRBool		mRepeat;    // A repeat, not implemented yet.

public:
	static TimerManager	sTimerManager;
};

TimerManager TimerImpl::sTimerManager;

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

void TimerManager::AddRequest(TimerImpl *inRequest)
{
	if(mLocker.Lock())
	{
		NS_ADDREF(inRequest);	// this is for the timer list

		// insert sorted into timer event list
		int32 count = CountItems();
		int32 pos;
		for(pos = 0; pos < count; pos++)
		{
			TimerImpl *entry = (TimerImpl *)ItemAtFast(pos);
			if(entry->mWhen > inRequest->mWhen)
				break;
		}
		AddItem(inRequest, pos);
			
		if(pos == 0)
			// We need to wake the thread to wait on the newly added event
			release_sem(mSyncSem);

		mLocker.Unlock();
	}
}

bool TimerManager::RemoveRequest(TimerImpl *inRequest)
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
		TimerImpl *tobj = 0;

		mLocker.Lock();

		bigtime_t now = system_time();

		// Fire expired pending requests
		while((tobj = FirstRequest()) != 0 && tobj->mWhen <= now)
		{
			RemoveItem((int32)0);
			mLocker.Unlock();

			if(! tobj->mCanceled)
			{
				// fire it
				if(tobj->mThread != cached)
				{
					sprintf(portname, "event%lx", tobj->mThread);
					sprintf(semname, "sync%lx", tobj->mThread);

					eventport = find_port(portname);
					syncsem = my_find_sem(semname);
					cached = tobj->mThread;
				}

				// call timer synchronously so we're sure tobj is alive
				ThreadInterfaceData	 id;
				id.data = tobj;
				id.sync = true;
				if(write_port(eventport, 'WMti', &id, sizeof(id)) == B_OK)
					while(acquire_sem(syncsem) == B_INTERRUPTED)
						;
			}
			NS_RELEASE(tobj);

			mLocker.Lock();
		}
		mLocker.Unlock();

		if(acquire_sem_etc(mSyncSem, 1, B_ABSOLUTE_TIMEOUT, 
					tobj ? tobj->mWhen : B_INFINITE_TIMEOUT) == B_BAD_SEM_ID)
			break;
	}

	return B_OK;
}

//
// TimerImpl
//
void TimerImpl::FireTimeout()
{
	if( ! mCanceled)
	{
		if(mFunc != NULL)
			(*mFunc)(this, mClosure);	    // If there's a function, call it.
		else if(mCallback != NULL)
			mCallback->Notify(this);    // But if there's an interface, notify it.
	}
}

TimerImpl::TimerImpl()
{
	NS_INIT_REFCNT();
	mFunc			= 0;
	mCallback		= 0;
	mDelay			= 0;
	mClosure		= 0;
	mWhen			= 0;
	mCanceled		= false;
}

TimerImpl::~TimerImpl()
{
	Cancel();
	NS_IF_RELEASE(mCallback);
}

void TimerImpl::SetDelay(PRUint32 aDelay)
{
	mDelay = aDelay;
	mWhen = system_time() + mDelay * 1000;

	NS_ADDREF(this);
	if (TimerImpl::sTimerManager.RemoveRequest(this))
		TimerImpl::sTimerManager.AddRequest(this);
//	NS_RELEASE(this);	// ?*?*?*?* doesn't work...
	Release();		// Is it the right way ?
}

nsresult TimerImpl::Init(nsTimerCallbackFunc aFunc, void *aClosure,
                PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType)
{
    mFunc = aFunc;
    mClosure = aClosure;
    // mRepeat = aRepeat;

    return Init(aDelay);
}

nsresult TimerImpl::Init(nsITimerCallback *aCallback,
                PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType)
{
    mCallback = aCallback;
    NS_ADDREF(mCallback);
    // mRepeat = aRepeat;
    return Init(aDelay);
}


nsresult TimerImpl::Init(PRUint32 aDelay)
{
	mDelay = aDelay;
	NS_ADDREF(this);	// this is for clients of the timer
	mWhen = system_time() + aDelay * 1000;
	
	mThread = PR_GetCurrentThread();

	sTimerManager.AddRequest(this);
	
    return NS_OK;
}

NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID);


void TimerImpl::Cancel()
{
	mCanceled = true;
	TimerImpl::sTimerManager.RemoveRequest(this);
}

nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if(nsnull == aInstancePtrResult)
		return NS_ERROR_NULL_POINTER;

    TimerImpl *timer = new TimerImpl();
    if(nsnull == timer)
		return NS_ERROR_OUT_OF_MEMORY;

    return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}

void nsTimerExpired(void *aCallData)
{
	TimerImpl* timer = (TimerImpl *)aCallData;
	timer->FireTimeout();
	NS_RELEASE(timer);
}
