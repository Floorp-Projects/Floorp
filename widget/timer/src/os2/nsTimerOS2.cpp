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
 *   Pierre Phaneuf <pp@ludusdesign.com> 
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 03/31/2000       IBM Corp.      Get rid of mBusy message and call default WndProc. Message is 
 *                                         put out while wizards (i.e. Mail) is active.
 *
 */

#define INCL_DOS
#define INCL_WIN
#include <os2.h>

#include <limits.h> // UINT_MAX (don't ask...)
#include <stdio.h>

#include "nsHashtable.h"

//#define PROFILE_TIMERS    // print stats on timer usage
//#define VERIFY_THREADS    // check timers are being used thread-correctly

#include "nsTimerOS2.h"

// Implementation of timers lifted from os2fe timer.cpp.
// Which was itself lifted pretty much from the winfe.
//
// mjf 28-9-98
//
// Rewrite 16-3-1999 to work correctly in a multithreaded world:
//                    Now, each thread has an nsTimerManager.
//                    This has a Timer window owned by the thread, and
//                    a queue (linked list) of timer objects.
//                    There is a static hashtable of these TimerManager
//                    object, keyed by TID.  Thus things work.  Ahem.
//
//                    Problem: nsTimerManagers are not cleaned up until
//                             module unload time.
//
// Update 16-4-1999 to recycle timers and collect statistics on how effective
//                  this is.

// Declarations, prototypes of useful functions
MRESULT EXPENTRY fnwpTimer( HWND h, ULONG msg, MPARAM mp1, MPARAM mp2);
#define NSTIMER_TIMER (TID_USERMAX - 2)
#define TIMEOUT_NEVER ((ULONG)-1)
#define TIMERCLASS ("WarpzillaTimerClass")
#define TIMER_CACHESIZE 20

static TID QueryCurrentTID();

// Timer object (funky 'release' method for recycler)
NS_IMPL_ADDREF(nsTimer)
NS_IMPL_QUERY_INTERFACE(nsTimer, NS_GET_IID(nsITimer))

nsrefcnt nsTimer::Release()
{
   if( --mRefCnt == 0)
      mManager->DisposeTimer( this);

   return mRefCnt;
}

nsTimer::nsTimer( nsTimerManager *aManager)
{
   Construct();
   mManager = aManager;
}

void nsTimer::Construct()
{
   NS_INIT_REFCNT();
   mDelay = 0;
   mFunc = nsnull;
   mClosure = nsnull;
   mCallback = nsnull;
   mFireTime = 0;
   mNext = nsnull;
}

void nsTimer::Destruct()
{
   Cancel();
}

nsTimer::~nsTimer()
{
   Destruct();
}

nsresult nsTimer::Init( nsTimerCallbackFunc aFunc,
                        void *aClosure,
                PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType)
{
   mFunc = aFunc;
   mClosure = aClosure;

   return mManager->InitTimer( this, aDelay);
}

nsresult nsTimer::Init( nsITimerCallback *aCallback, PRUint32 aDelay,
                PRUint32 aPriority, PRUint32 aType)
{
   mCallback = aCallback;
   NS_ADDREF(mCallback); // this is daft, but test14 traps without it

   return mManager->InitTimer( this, aDelay);
}

void nsTimer::Cancel()
{
   if( mCallback || mFunc) // guard against double-cancel
   {
      mManager->CancelTimer( this);
      NS_IF_RELEASE(mCallback);
      mFunc = nsnull;
   }
}

void nsTimer::Fire( ULONG aNow)
{
   if( mFunc)
      (*mFunc)( this, mClosure);
   else if( mCallback)
      mCallback->Notify( this);
}

// Timer manager

// First, GUI stuff.

nsTimerManager::nsTimerManager() : mTimerList(nsnull), mTimerSet(FALSE),
                                   mNextFire(TIMEOUT_NEVER), mHWNDTimer(0),
                                   mBusy(FALSE), mBase(nsnull), mSize(0)
#ifdef PROFILE_TIMERS
                                   ,mDeleted(0),mActive(0),mCount(0),mPeak(0)
#endif
{
   // As a part of the PM setup protocol (see somewhere under
   // http://www.mozilla.org/ports/os2/code_docs/), create a HMQ if
   // appropriate.
   if( FALSE == WinQueryQueueInfo( HMQ_CURRENT, 0, 0))
   {
      HAB hab = WinInitialize( 0);
      WinCreateMsgQueue( hab, 0);
   }
   // Register timer window class if we haven't done that yet.
   EnsureWndClass();

   // Create timer window
   mHWNDTimer = WinCreateWindow( HWND_DESKTOP, TIMERCLASS, 0, 0, 0, 0, 0,
                                 0, HWND_DESKTOP, HWND_TOP, 0, 0, 0);
   NS_ASSERTION(mHWNDTimer, "Couldn't create Timer window");

   // stick pointer to us in the window word
   WinSetWindowPtr( mHWNDTimer, QWL_USER, this);

#ifdef PROFILE_TIMERS
   mTID = QueryCurrentTID();
#endif
}

nsTimerManager::~nsTimerManager()
{
   // I reckon PM has already gone down by this point.
   // There might be an API to do this (register listener for app shutdown)
   // at some point, or maybe when we get into the widget DLL.
   if( mHWNDTimer)
      WinDestroyWindow( mHWNDTimer);

#ifdef PROFILE_TIMERS
   printf( "\n----- Timer Statistics %d -----\n", (int)mTID);
   printf( "  Calls to NS_NewTimer:   %d\n", mCount);
   printf( "  Actual timers created:  %d\n", mPeak);
   double d = mCount ? (double)(mCount - mPeak) / (double)mCount : 0;
   printf( "  Timers recycled:        %d (%d%%)\n", mCount - mPeak, (int)(d*100.0));
   UINT i = mSize+mActive+mDeleted;
   printf( "  Cache+Active+Deleted:   %d\n", i);
   if( i != mPeak)
      printf( "  WARNING: Timers leaked: %d\n", mPeak - i);
   printf( "------------------------------\n\n");
#endif

   // delete timers in the 'cache' list
   nsTimer *pTemp, *pNext = mBase;
   while( 0 != pNext)
   {
      pTemp = pNext->mNext;
      delete pNext;
      pNext = pTemp;
      // and then the 'active' list...
      if( 0 == pNext) { pNext = mTimerList; mTimerList = 0; }
   }
}

void nsTimerManager::EnsureWndClass()
{
   static BOOL bRegistered = FALSE;

   if( !bRegistered)
   {
      BOOL rc = WinRegisterClass( 0, TIMERCLASS, fnwpTimer, 0, 4);
      NS_ASSERTION(rc,"Can't register class");
      bRegistered = TRUE;
   }
}

MRESULT nsTimerManager::HandleMsg( ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mRC = 0;

   if( msg == WM_TIMER && SHORT1FROMMP(mp1) == NSTIMER_TIMER)
   {
      // Block only one entry into this function, or else.
      // (windows does this; don't completely understand or belive
      //  it's necessary -- maybe if nested eventloops happen).
      if( !mBusy)
      {
          mBusy = TRUE;
          // see if we need to fork off any timeout functions
          if( mTimerList)
             ProcessTimeouts( 0);
          mBusy = FALSE;
      }
      else {
         // printf( "mBusy == TRUE but want to fire Timer!\n");
         mRC = WinDefWindowProc( mHWNDTimer, msg, mp1, mp2);   // handle the message
      }
   }
   else
      mRC = WinDefWindowProc( mHWNDTimer, msg, mp1, mp2);

   return mRC;
}

void nsTimerManager::StopTimer()
{
   WinStopTimer( 0/*hab*/, mHWNDTimer, NSTIMER_TIMER);
}

void nsTimerManager::StartTimer( UINT aDelay)
{
   WinStartTimer( 0/*hab*/, mHWNDTimer, NSTIMER_TIMER, aDelay);
}

MRESULT EXPENTRY fnwpTimer( HWND h, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   nsTimerManager *pMgr = (nsTimerManager*) WinQueryWindowPtr( h, QWL_USER);
   MRESULT         mRet = 0;
   if( pMgr)
      mRet = pMgr->HandleMsg( msg, mp1, mp2);
   else
      mRet = WinDefWindowProc( h, msg, mp1, mp2);
   return mRet;
}

// Recycling management
nsTimer *nsTimerManager::CreateTimer()
{
   nsTimer *rc;
   if( !mBase)
   {
      rc = new nsTimer( this);
#ifdef PROFILE_TIMERS
      mPeak++;
#endif
   }
   else
   {
      rc = mBase;
      mBase = rc->mNext;
      rc->Construct();
      mSize--;
   }
#ifdef PROFILE_TIMERS
   mCount++;
#endif
   return rc;
}

void nsTimerManager::DisposeTimer( nsTimer *aTimer)
{
   if( TIMER_CACHESIZE == mSize)
   {
      delete aTimer;
#ifdef PROFILE_TIMERS
      mDeleted++;
#endif
   }
   else
   {
      aTimer->Destruct();
      aTimer->mNext = mBase;
      mBase = aTimer;
      mSize++;
   }
}

// Complicated functions to maintain the list of timers.
// This code has something of a pedigree -- I worked it through once,
// ages ago, for os2fe, and convinced myself it was correct.

// Adjust the period of the window timer according to the contents of
// the timer list, and record the absolute time that the next firing
// will occur.  May well stop the window timer entirely.
void nsTimerManager::SyncTimeoutPeriod( ULONG aTickCount)
{
   // May want us to set tick count ourselves.
   if( 0 == aTickCount)
      aTickCount = WinGetCurrentTime( 0/*hab*/);

   // If there's no list, we should clear the timer.
   if( !mTimerList)
   {
      if( mTimerSet)
      {
         StopTimer();
         mNextFire = TIMEOUT_NEVER;
         mTimerSet = FALSE;
      }
   }
   else
   {
      // See if we need to clear the current timer.
      // Circumstances are that if the timer will not
      //    fire on time for the next timeout.
      BOOL bSetTimer = FALSE;
      nsTimer *pTimeout = mTimerList;
      if( mTimerSet)
      {
         if( pTimeout->mFireTime != mNextFire)
         {
            StopTimer();
            mNextFire = TIMEOUT_NEVER;
            mTimerSet = FALSE;

            // Set the timer.
            bSetTimer = TRUE;
         }
      }
      else
      {
         // No timer set, attempt.
         bSetTimer = TRUE;
      }

      if( bSetTimer)
      {
         ULONG ulFireWhen = pTimeout->mFireTime > aTickCount ?
                                      pTimeout->mFireTime - aTickCount : 0;
         if( ulFireWhen > UINT_MAX)
            ulFireWhen = UINT_MAX;

         NS_ASSERTION(mTimerSet == FALSE,"logic error in timer sync magic");
         StartTimer( ulFireWhen);

         // Set the fire time.
         mNextFire = pTimeout->mFireTime;
      }
   }
}

// Walk down the timeout list and fire those which are due.
void nsTimerManager::ProcessTimeouts( ULONG aNow)
{
   nsTimer *p = mTimerList;

   if( 0 == aNow)
      aNow = WinGetCurrentTime( 0/*hab*/);

   BOOL bCalledSync = FALSE;

   // loop over all entries
   while( p)
   {
      // send it
      if( p->mFireTime < aNow)
      {
         // Fire it, making sure the timer doesn't die.
         NS_ADDREF(p);
         p->Fire( aNow);

         // Clear the timer.
         // Period synced.
         p->Cancel();
         bCalledSync = TRUE;
         NS_RELEASE(p);

         // Reset the loop (can't look at p->pNext now, and called
         // code may have added/cleared timers).
         // (could do this by going recursive and returning).
         p = mTimerList;
      }
      else
      {
         // Make sure we fire a timer.
         // Also, we need to check to see if things are backing up (they
         // may be asking to be fired long before we ever get to them,
         // and we don't want to pass in negative values to the real
         // timer code, or it takes days to fire....
         if( FALSE == bCalledSync)
         {
            SyncTimeoutPeriod( aNow);
            bCalledSync = TRUE;
         }
         //  Get next timer.
         p = p->mNext;
      }
   }
}

// Init a timer - insert it into the list, adjust the window timer to be
// running fast enough to make sure it fires when it should.
nsresult nsTimerManager::InitTimer( nsTimer *aTimer, PRUint32 aDelay)
{
#ifdef VERIFY_THREADS
   if( aTimer->mTID != QueryCurrentTID())
      printf( "*** Thread assumption violated in base/src/os2/nsTimerOS2.cpp\n");
#endif

   ULONG ulNow = WinGetCurrentTime( 0/*hab*/);
  
   aTimer->mDelay = aDelay;
   aTimer->mFireTime = (ULONG) aDelay + ulNow;
   aTimer->mNext = nsnull;

   // add it to the list
   if( !mTimerList)
      mTimerList = aTimer;
   else
   {
      // is it before everything else on the list?
      if( aTimer->mFireTime < mTimerList->mFireTime)
      {
         aTimer->mNext = mTimerList;
         mTimerList = aTimer;
      }
      else
      {
         nsTimer *pPrev = mTimerList;
         nsTimer *pCurrent = mTimerList;

         while( pCurrent &&
               (pCurrent->mFireTime <= aTimer->mFireTime))
         {
            pPrev = pCurrent;
            pCurrent = pCurrent->mNext;
         }

         NS_ASSERTION(pPrev,"logic error in InitTimer");

         // insert it after pPrev (this could be at the end of the list)
         aTimer->mNext = pPrev->mNext;
         pPrev->mNext = aTimer;
      }
   }

#ifdef PROFILE_TIMERS
   mActive++;
#endif

   // Sync the timer fire period.
   SyncTimeoutPeriod( ulNow);

   return NS_OK;
}

// Cancel a timer - remove it from the list, adjust the window timer as
// appropriate.
void nsTimerManager::CancelTimer( nsTimer *aTimer)
{
   if( mTimerList == aTimer)
   {
      // first element in the list lossage
      mTimerList = aTimer->mNext;
   }
   else
   {
      // walk until no next pointer
      nsTimer *p = mTimerList;
      while( p && p->mNext && (p->mNext != aTimer))
         p = p->mNext;

      // if we found something valid pull it out of the list
      if( p && p->mNext && p->mNext == aTimer)
      {
         p->mNext = aTimer->mNext;
      }
      else
      {
         // we come here at app-shutdown/crash when timers which are still in the
         // active-timer list are deleted.
         return;
      }
   }

#ifdef PROFILE_TIMERS
   mActive--;
#endif

   // Adjust window timer
   SyncTimeoutPeriod( 0);
}

// Data structure to hold and create nsTimerManagers.
// This is (meant to be) thread-safe.
class nsTimerManagers
{
   nsHashtable mTable;
   HMTX        mMutex;

   struct TIDKey : public nsVoidKey
   {
      TIDKey() : nsVoidKey((void*)QueryCurrentTID())
      {}
   };

   struct Lock
   {
      HMTX mHmtx;
      Lock( HMTX hmtx) : mHmtx(hmtx)
      {
         DosRequestMutexSem( mHmtx, SEM_INDEFINITE_WAIT);
      }
     ~Lock()
      {
         DosReleaseMutexSem( mHmtx);
      }
   };

 public:
   nsTimerManagers() : mMutex(0)
   {
      DosCreateMutexSem( 0, &mMutex, 0, 0 /*unowned*/);
   }

   static PRBool mgr_dtor( nsHashKey *aKey, void *aData, void */*aClosure*/)
   {
      nsTimerManager *pManager = (nsTimerManager*) aData;
      delete pManager;
      return PR_TRUE;
   }

  ~nsTimerManagers()
   {
      mTable.Enumerate( &mgr_dtor);
      if( mMutex)
         DosCloseMutexSem( mMutex);
   }

   nsTimerManager *Get()
   {
      Lock   lock(mMutex);
      TIDKey key;

      nsTimerManager *aMgr = (nsTimerManager*) mTable.Get( &key);

      if( !aMgr)
      {
         aMgr = new nsTimerManager;
         mTable.Put( &key, aMgr);
      }

      return aMgr;
   }

} TimerManagers;

// Entry into the DLL.  No, I dunno why there's not a normal factory
// for this either.  Ease of use?

nsresult NS_NewTimer( nsITimer **aInstance)
{
   if( !aInstance)
      return NS_ERROR_NULL_POINTER;

   // Assumption: timers are created in the same thread they are to be
   //             used in.  This seems likely, but we shall see.
   //             If things seem screwy, set the VERIFY_THREADS flag on this file.

   nsTimer *timer = TimerManagers.Get()->CreateTimer();
   if( !timer)
      return NS_ERROR_OUT_OF_MEMORY;

   return timer->QueryInterface( NS_GET_IID(nsITimer), (void **) aInstance);
}

TID QueryCurrentTID()
{
   PTIB pTib = 0;
   PPIB pPib = 0;
   DosGetInfoBlocks( &pTib, &pPib);
   return pTib->tib_ptib2->tib2_ultid;
}
