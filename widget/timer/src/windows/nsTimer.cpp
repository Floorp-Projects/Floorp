/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Michael Lowe <michael.lowe@bigfoot.com>
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

#include "nsWindowsTimer.h"
#include "nsITimerQueue.h"
#include "nsIWindowsTimerMap.h"
#include "nsCRT.h"
#include "prlog.h"
#include <stdio.h>
#include <windows.h>
#include <limits.h>
#include "nsWidgetsCID.h"
#include "nsIServiceManager.h"
#include <stdlib.h>


static NS_DEFINE_CID(kTimerManagerCID, NS_TIMERMANAGER_CID);

static nsCOMPtr<nsIWindowsTimerMap> sTimerMap;
static nsCOMPtr<nsITimerQueue> sTimerQueue;

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

  MSG wmsg;
  bool eventQueueEmpty = !::PeekMessage(&wmsg, NULL, 0, 0, PM_NOREMOVE);
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
        !::PeekMessage(&wmsg, NULL, 0, 0, PM_NOREMOVE)) {

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
  mTimerID = ::SetTimer(NULL, 0, aDelay, (TIMERPROC)FireTimeout);

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
    ::KillTimer(NULL, mTimerID);

    mTimerID = 0;
  }
}
