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

#ifndef __nsTimerBeOS_h
#define __nsTimerBeOS_h

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <prthread.h>

#include <List.h>
#include <Autolock.h>

class TimerManager : public BList
{
public:
					TimerManager();
					~TimerManager();
	void			AddRequest(nsITimer *inRequest);
	bool			RemoveRequest(nsITimer *inRequest);

private:
	BLocker			mLocker;
	sem_id			mSyncSem;
	thread_id		mTimerThreadID;
	bool			mQuitRequested;

	static int32	sTimerThreadFunc(void *);
	int32			TimerThreadFunc();
	nsITimer		*FirstRequest()				{ return (nsITimer *)FirstItem(); }
};

class nsTimerBeOS : public nsITimer 
{
  friend class TimerManager;
public:

  nsTimerBeOS();
  virtual ~nsTimerBeOS();
  
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

  virtual void Cancel();

  virtual PRUint32 GetDelay() { return mDelay; }
  virtual void SetDelay(PRUint32 aDelay);

  virtual PRUint32 GetPriority() { return mPriority; }
  virtual void SetPriority(PRUint32 aPriority);

  virtual PRUint32 GetType()  { return mType; }
  virtual void SetType(PRUint32 aType);

  virtual void* GetClosure() { return mClosure; }

  NS_IMETHOD_(void) FireTimeout();

  bigtime_t	           mSchedTime;  // Time when this request should be done
  PRUint32             mDelay;      // The delay set in Init()
private:
  nsresult             Init(PRUint32 aDelay);  // Initialize the timer.

  PRUint32             mPriority;
  PRUint32             mType;
  nsTimerCallbackFunc  mFunc;	    // The function to call back when expired
  void *               mClosure;    // The argumnet to pass it.
  nsITimerCallback *   mCallback;   // An interface to notify when expired.

  bool                 mCanceled;
  PRThread *           mThread;
//  PRBool		mRepeat;    // A repeat, not implemented yet.

public:
	static TimerManager	sTimerManager;
};

#endif // __nsTimerBeOS_h
