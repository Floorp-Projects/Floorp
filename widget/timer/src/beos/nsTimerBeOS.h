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
        void			SortRequests();

private:
	BLocker			mLocker;
	sem_id			mSyncSem;
	thread_id		mTimerThreadID;
        void			*mProcessing;
	bool			mQuitRequested;

	static int32	sTimerThreadFunc(void *);
	int32			TimerThreadFunc();
	nsITimer		*FirstRequest()	{ return (nsITimer *)FirstItem(); }
};

class nsTimerBeOS : public nsITimer 
{
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

  bigtime_t GetSchedTime() { return mSchedTime; }
  bool IsCanceled() { return mCanceled; }
  PRThread* GetThread() { return mThread; }

private:
  nsresult             Init(PRUint32 aDelay, PRUint32 aType);  // Initialize the timer.

  PRUint32             mPriority;
  PRUint32             mType;
  PRUint32             mDelay;      // The delay set in Init()
  nsTimerCallbackFunc  mFunc;	    // The function to call back when expired
  void *               mClosure;    // The argumnet to pass it.
  nsITimerCallback *   mCallback;   // An interface to notify when expired.

  bool                 mCanceled;
  PRThread *           mThread;

  bigtime_t		mSchedTime;  // Time when this request should be done

public:
  static TimerManager	sTimerManager;
};

#endif // __nsTimerBeOS_h
