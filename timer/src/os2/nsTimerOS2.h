/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nstimeros2_h
#define _nstimeros2_h

// Class definitions for the timers.
//
// These timer classes are reentrant, thread-correct and supports recycling of
// timer objects on a per-thread basis.

#include "nsITimer.h"
#include "nsITimerCallback.h"

// Implementation of the timer interface.

class nsTimerManager;

class nsTimer : public nsITimer
{
 public:
   nsTimer( nsTimerManager *aManager);
   virtual ~nsTimer();

   // nsISupports
   NS_DECL_ISUPPORTS

   // Explicit init-term for recycler
   void Construct();
   void Destruct();

   nsresult Init( nsTimerCallbackFunc aFunc, void *aClosure, PRUint32 aDelay);
   nsresult Init( nsITimerCallback *aCallback, PRUint32 aDelay);
   void     Cancel();
   PRUint32 GetDelay()                 { return mDelay; }
   void     SetDelay( PRUint32 aDelay) {}; // XXX Windows does this too...
   void    *GetClosure()               { return mClosure; }

   // Implementation
   void     Fire( ULONG aNow);

 private:
   PRUint32             mDelay;
   nsTimerCallbackFunc  mFunc;
   void                *mClosure;
   nsITimerCallback    *mCallback;
   ULONG                mFireTime;
   nsTimer             *mNext;

   friend class nsTimerManager;

   nsTimerManager      *mManager;
};


// Timer manager class, one of these per-thread

class nsTimerManager
{
 public:
   nsTimerManager();
  ~nsTimerManager();

   nsresult InitTimer( nsTimer *aTimer, PRUint32 aDelay);
   void     CancelTimer( nsTimer *aTimer);

   MRESULT  HandleMsg( ULONG msg, MPARAM mp1, MPARAM mp2);

   nsTimer *CreateTimer();
   void     DisposeTimer( nsTimer *aTimer);

 private:
   nsTimer *mTimerList; // linked list of active timers, sorted in priority order
   BOOL     mTimerSet;  // timer running?
   ULONG    mNextFire;  // absolute time of next fire
   HWND     mHWNDTimer; // window for timer messages
   BOOL     mBusy;      // dubious lock thingy

   void     StartTimer( UINT delay);
   void     StopTimer();
   void     EnsureWndClass();

   void     ProcessTimeouts( ULONG aNow);
   void     SyncTimeoutPeriod( ULONG aTickCount);

   // timer-cache
   nsTimer *mBase;
   UINT     mSize;
#ifdef PROFILE_TIMERS
   UINT     mDeleted;
   UINT     mActive;
   UINT     mCount;
   UINT     mPeak;
   TID      mTID;
#endif
};

#endif
