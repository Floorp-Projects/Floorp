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
 *  Michael Lowe <michael.lowe@bigfoot.com>
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
 * 05/11/2000       IBM Corp.       Took code from Windows m15 and modified
 *                                  it for OS/2.  Introduced os2TimerGlue class
 *                                  to manage a list of PM windows, one window
 *                                  for each thread that requests a timer.
 *                                  The windows and os2TimerGlue will live
 *                                  until the module is unloaded.
 */

#define  INCL_WINTIMER
#include "os2TimerGlue.h"
#include "nsWindowsTimer.h"
#include "nsITimerQueue.h"
#include "nsIWindowsTimerMap.h"
#include "nsCRT.h"
#include "prlog.h"
#include <stdio.h>
#include <limits.h>
#include "nsWidgetsCID.h"
#include "nsIServiceManager.h"
#include <stdlib.h>


static NS_DEFINE_CID(kTimerManagerCID, NS_TIMERMANAGER_CID);

static nsCOMPtr<nsIWindowsTimerMap> sTimerMap;
static nsCOMPtr<nsITimerQueue> sTimerQueue;
os2TimerGlue  *sGlue = NULL;

class nsTimer;

#define NS_PRIORITY_IMMEDIATE 10

void CALLBACK FireTimeout(HWND aWindow, 
                          UINT aMessage, 
                          UINT aTimerID, 
                          DWORD aTime)
{
  PR_ASSERT(aMessage == WM_TIMER);

  if (!sTimerMap) return;

  nsTimer* timer = sTimerMap->GetTimer(aTimerID);
  if (timer == nsnull) {
    return;
  }

  if (timer->GetType() != NS_TYPE_REPEATING_PRECISE) {
    // stop OS timer: may be restarted later
    timer->KillOSTimer();
  }

  if (!sTimerQueue) return;

  QMSG wmsg;
  bool eventQueueEmpty = !(WinPeekMsg(NULLHANDLE, &wmsg, NULLHANDLE, 0, 0, 
                                        PM_NOREMOVE));
  bool timerQueueEmpty = !sTimerQueue->HasReadyTimers(NS_PRIORITY_LOWEST);

  if ((timerQueueEmpty && eventQueueEmpty) || 
      timer->GetPriority() >= NS_PRIORITY_IMMEDIATE) {
    
    // fire timer immediatly
    timer->Fire();

  } else {
    // defer timer firing
    sTimerQueue->AddReadyQueue(timer);
  }

  if (eventQueueEmpty) {
    // while event queue is empty, fire off waiting timers
    while (sTimerQueue->HasReadyTimers(NS_PRIORITY_LOWEST) &&
        !(WinPeekMsg(NULLHANDLE, &wmsg, NULLHANDLE, 0, 0, PM_NOREMOVE))) {

      sTimerQueue->FireNextReadyTimer(NS_PRIORITY_LOWEST);
    }
  }
}


NS_IMPL_ISUPPORTS1(nsTimer, nsITimer)


nsTimer::nsTimer() : nsITimer()
{
  NS_INIT_ISUPPORTS();

  mFunc = nsnull;
  mCallback = nsnull;
  mClosure = nsnull;
  mTimerID = 0;
  mTimerRunning = false;

  static int cachedService = 0;
  if (cachedService == 0) {
    cachedService = 1;

    nsresult rv;
    sTimerMap = do_GetService(kTimerManagerCID, &rv);
    sTimerQueue = do_GetService(kTimerManagerCID, &rv);
    sGlue = new os2TimerGlue();
  }
}


nsTimer::~nsTimer()
{
  KillOSTimer();
  
  NS_IF_RELEASE(mCallback);
}


NS_IMETHODIMP_(void) nsTimer::SetPriority(PRUint32 aPriority)
{ 
  PR_ASSERT(aPriority >= NS_PRIORITY_LOWEST && 
            aPriority <= NS_PRIORITY_HIGHEST); 
  mPriority = aPriority; 
}


NS_IMETHODIMP nsTimer::Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
  mFunc = aFunc;
  mClosure = aClosure;

  return Init(aDelay, aPriority, aType);
}


NS_IMETHODIMP nsTimer::Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
  mCallback = aCallback;
  NS_ADDREF(mCallback);

  return Init(aDelay, aPriority, aType);
}


nsresult nsTimer::Init(PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
  // prevent timer being released before
  // it has fired or is canceled
  NS_ADDREF_THIS();
  mTimerRunning = true;

  mDelay = aDelay;

  SetPriority(aPriority);
  SetType(aType);

  StartOSTimer(aDelay);

  return NS_OK;
}


void nsTimer::Fire()
{
  // prevent a canceled timer which is 
  // already in ready queue from firing
  if (mTimerRunning == false) return;

  // prevent notification routine 
  // from releasing timer by canceling it
  NS_ADDREF_THIS();

  // invoke notification routine
  if (mFunc != nsnull) {
    (*mFunc)(this, mClosure);
  }
  else if (mCallback != nsnull) {
    mCallback->Notify(this);
  }

  if (GetType() == NS_TYPE_REPEATING_SLACK) {
    // restart timer
    StartOSTimer(GetDelay());

  } else if (GetType() == NS_TYPE_ONE_SHOT) {

    // timer finished
    if (mTimerRunning == true) {
      mTimerRunning = false;

      NS_RELEASE_THIS();
    }
  }

  NS_RELEASE_THIS();
}


NS_IMETHODIMP_(void) nsTimer::Cancel()
{
  KillOSTimer();

  // timer finished
  if (mTimerRunning == true) {
    mTimerRunning = false;

    NS_RELEASE_THIS();
  }
}


void nsTimer::StartOSTimer(PRUint32 aDelay)
{
  PR_ASSERT(mTimerID == 0);

  if (!sTimerMap) return;

  // create OS timer
  HWND timerHWND;
  timerHWND = sGlue->Get();
  // Need a unique number for the timerID.  No way to get a unique timer ID
  //    without passing in a null hwnd.  Since there are already tests in place
  //    to ensure only one OS timer per nsTimer, this should be safe.  Could
  //    really do away with the AddTimer/GetTimer stuff with this, but want to
  //    keep this looking like Windows so will leave it in.  The only problem
  //    is that we have to keep this id below 0x7fff.  This is the easy way
  //    and likely to be safe.  If this doesn't work, we'll need to keep track
  //    of what ID's we've used and what is still open.  Hopefully these id's
  //    will be unique enough.   IBM-AKR

  ULONG ulTimerID = ((ULONG)this & TID_USERMAX);
  ULONG fixer = ((ULONG)this & 0x00018000) >> 15;

  // Doing this fixer stuff since most values will be double word aligned.
  //    This will help make it a little more unique by potentially giving us
  //    3 times as many values.  IBM-AKR
  ulTimerID = (ulTimerID | fixer);
  mTimerID = WinStartTimer(NULLHANDLE, timerHWND, ulTimerID, aDelay);

  // store mapping from OS timer to timer object
  sTimerMap->AddTimer(mTimerID, this);
}


void nsTimer::KillOSTimer()
{
  if (mTimerID != 0) {

    // remove mapping from OS timer to timer object
    if (sTimerMap) {
      sTimerMap->RemoveTimer(mTimerID);
    }

    // kill OS timer
    HWND timerHWND;
    timerHWND = sGlue->Get();
    WinStopTimer(NULLHANDLE, timerHWND, mTimerID);

    mTimerID = 0;
  }
}
