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
 *  Tony Tsui <tony@igleaus.com.au>
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

#include "nsTimerXlib.h"

#include "nsIXlibWindowService.h"
#include "nsIServiceManager.h"

#include "nsVoidArray.h"
#include <unistd.h>
#include <stdio.h>

#include "prlog.h"

#include <X11/Xlib.h>

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

static int  NS_TimeToNextTimeout(struct timeval *aTimer);
static void NS_ProcessTimeouts(Display *aDisplay);

nsVoidArray *nsTimerXlib::gHighestList = (nsVoidArray *)nsnull;
nsVoidArray *nsTimerXlib::gHighList = (nsVoidArray *)nsnull;
nsVoidArray *nsTimerXlib::gNormalList = (nsVoidArray *)nsnull;
nsVoidArray *nsTimerXlib::gLowList = (nsVoidArray *)nsnull;
nsVoidArray *nsTimerXlib::gLowestList = (nsVoidArray *)nsnull;
PRBool nsTimerXlib::gTimeoutAdded = PR_FALSE;
PRBool nsTimerXlib::gProcessingTimer = PR_FALSE;

nsTimerXlib::nsTimerXlib()
{
#ifdef TIMER_DEBUG
  fprintf(stderr, "nsTimerXlib::nsTimerXlib (%p) called.\n", this);
#endif

  NS_INIT_REFCNT();
  mFunc = NULL;
  mCallback = NULL;
  mDelay = 0;
  mClosure = NULL;
  mPriority = 0;
  mType = NS_TYPE_ONE_SHOT;
}

nsTimerXlib::~nsTimerXlib()
{
#ifdef TIMER_DEBUG
  fprintf(stderr, "nsTimerXlib::~nsTimerXlib (%p) called.\n", this);
#endif
  
  Cancel();
  NS_IF_RELEASE(mCallback);
}

/*static*/ void nsTimerXlib::Shutdown()
{
  delete nsTimerXlib::gHighestList;
  nsTimerXlib::gHighestList = nsnull;

  delete nsTimerXlib::gHighList;
  nsTimerXlib::gHighList = nsnull;

  delete nsTimerXlib::gNormalList;
  nsTimerXlib::gNormalList = nsnull;

  delete nsTimerXlib::gLowList;
  nsTimerXlib::gLowList = nsnull;

  delete nsTimerXlib::gLowestList;
  nsTimerXlib::gLowestList = nsnull;

  nsTimerXlib::gTimeoutAdded = PR_FALSE;
}

NS_IMPL_ISUPPORTS1(nsTimerXlib, nsITimer)

nsresult
nsTimerXlib::Init(nsTimerCallbackFunc aFunc,
                  void *aClosure,
                  PRUint32 aDelay,
                  PRUint32 aPriority,
                  PRUint32 aType )
{
  mFunc = aFunc;
  mClosure = aClosure;
  mType = aType;
  return Init(aDelay, aPriority);
}

nsresult 
nsTimerXlib::Init(nsITimerCallback *aCallback,
                  PRUint32 aDelay,
                  PRUint32 aPriority,
                  PRUint32 aType)
{
  mType = aType;
  mCallback = aCallback;
  NS_ADDREF(mCallback);
  return Init(aDelay, aPriority);
}

nsresult
nsTimerXlib::Init(PRUint32 aDelay, PRUint32 aPriority)
{
  struct timeval Now;
#ifdef TIMER_DEBUG
  fprintf(stderr, "nsTimerXlib::Init (%p) called with delay %d\n", this, aDelay);
#endif
  
  mDelay = aDelay;
  mPriority = aPriority;
  // get the cuurent time
  
  gettimeofday(&Now, NULL);
  mFireTime.tv_sec = Now.tv_sec + (aDelay / 1000);
  mFireTime.tv_usec = Now.tv_usec + ((aDelay%1000) * 1000);

  if (mFireTime.tv_usec >= 1000000) 
  {
    mFireTime.tv_sec++;
    mFireTime.tv_usec -= 1000000;
  }

#ifdef TIMER_DEBUG
  fprintf(stderr, "fire set to %ld / %ld\n", mFireTime.tv_sec, mFireTime.tv_usec);
#endif

  if (!gTimeoutAdded)
  {
    nsTimerXlib::gHighestList = new nsVoidArray;
    nsTimerXlib::gHighList = new nsVoidArray;
    nsTimerXlib::gNormalList = new nsVoidArray;
    nsTimerXlib::gLowList = new nsVoidArray;
    nsTimerXlib::gLowestList = new nsVoidArray;
    
    nsTimerXlib::gTimeoutAdded = PR_TRUE;
  }

  switch (aPriority)
  {
    case NS_PRIORITY_HIGHEST:
      nsTimerXlib::gHighestList->InsertElementAt(this, 0);
      break;
    case NS_PRIORITY_HIGH:
      nsTimerXlib::gHighList->InsertElementAt(this, 0);
      break;
    case NS_PRIORITY_NORMAL:
      nsTimerXlib::gNormalList->InsertElementAt(this, 0);
      break;
    case NS_PRIORITY_LOW:
      nsTimerXlib::gLowList->InsertElementAt(this, 0);
      break;
    case NS_PRIORITY_LOWEST:
      nsTimerXlib::gLowestList->InsertElementAt(this, 0);
      break;
  }

  EnsureWindowService();

  return NS_OK;
}

PRBool
nsTimerXlib::Fire()
{
  nsCOMPtr<nsITimer> kungFuDeathGrip = this;

#ifdef TIMER_DEBUG
  fprintf(stderr, "*** PRIORITY is %x ***\n", mPriority);
#endif
  
  timeval aNow;
  gettimeofday(&aNow, NULL);

#ifdef TIMER_DEBUG  
  fprintf(stderr, "nsTimerXlib::Fire (%p) called at %ld / %ld\n",
          this, aNow.tv_sec, aNow.tv_usec);
#endif
  
  if (mFunc != NULL) {
    (*mFunc)(this, mClosure);
  }
  else if (mCallback != NULL) {
    mCallback->Notify(this);
  }

  return ((mType == NS_TYPE_REPEATING_SLACK) || (mType == NS_TYPE_REPEATING_PRECISE));
}

void
nsTimerXlib::Cancel()
{

  switch(mPriority)
  {
    case NS_PRIORITY_HIGHEST:
      nsTimerXlib::gHighestList->RemoveElement(this);
      break;
    case NS_PRIORITY_HIGH:
      nsTimerXlib::gHighList->RemoveElement(this);
      break;
    case NS_PRIORITY_NORMAL:
      nsTimerXlib::gNormalList->RemoveElement(this);
      break;
    case NS_PRIORITY_LOW:
      nsTimerXlib::gLowList->RemoveElement(this);
      break;
    case NS_PRIORITY_LOWEST:
      nsTimerXlib::gLowestList->RemoveElement(this);
      break;
  }
}

void
nsTimerXlib::ProcessTimeouts(nsVoidArray *array)
{
  PRInt32 count = array->Count();

  if (count == 0)
    return;
  
  nsTimerXlib *timer;
  
  struct timeval aNow;
  struct timeval ntv;
  int res;

  gettimeofday(&aNow, NULL);

#ifdef TIMER_DEBUG  
  fprintf(stderr, "nsTimerXlib::ProcessTimeouts called at %ld / %ld\n",
         aNow.tv_sec, aNow.tv_usec);
#endif
  
  for (int i = count; i >=0; i--)
  {
    timer = (nsTimerXlib*)array->ElementAt(i);

    if (timer)
    {
     
      if ((timer->mFireTime.tv_sec < aNow.tv_sec) ||
          ((timer->mFireTime.tv_sec == aNow.tv_sec) &&
           (timer->mFireTime.tv_usec <= aNow.tv_usec))) 
      {
        //  Make sure that the timer cannot be deleted during the
        //  Fire(...) call which may release *all* other references
        //  to p...
#ifdef TIMER_DEBUG
        fprintf(stderr, "Firing timeout for (%p)\n", timer);
#endif
//        NS_ADDREF(timer); //FIXME: Does this still apply??? TonyT
      
        res = timer->Fire();      
      
        if (res == 0) 
        {
          array->RemoveElement(timer);
//          NS_RELEASE(timer); //FIXME: Ditto to above.
        }
        else 
        {
          gettimeofday(&ntv, NULL);
          timer->mFireTime.tv_sec = ntv.tv_sec + (timer->mDelay / 1000);
          timer->mFireTime.tv_usec = ntv.tv_usec + ((timer->mDelay%1000) * 1000);
        }     
      }
    }
  }
}

void nsTimerXlib::SetDelay(PRUint32 aDelay) 
{
  mDelay = aDelay;
}

void nsTimerXlib::SetPriority(PRUint32 aPriority) 
{
  mPriority = aPriority;
}
              
void nsTimerXlib::SetType(PRUint32 aType) 
{
  mType = aType;
}
           

static NS_DEFINE_IID(kWindowServiceCID,NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,NS_XLIB_WINDOW_SERVICE_IID);

nsresult
nsTimerXlib::EnsureWindowService()
{
  nsIXlibWindowService * xlibWindowService = nsnull;

  nsresult rv = nsServiceManager::GetService(kWindowServiceCID,
                                             kWindowServiceIID,
                                             (nsISupports **)&xlibWindowService);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Couldn't obtain window service.");

  if (NS_OK == rv && nsnull != xlibWindowService)
  {
    xlibWindowService->SetTimeToNextTimeoutFunc(NS_TimeToNextTimeout);
    xlibWindowService->SetProcessTimeoutsProc(NS_ProcessTimeouts);

    NS_RELEASE(xlibWindowService);
  }

  return NS_OK;
}

static
int NS_TimeToNextTimeout(struct timeval *aTimer) 
{
  static int once = 1;

  if (once)
  {
    once = 0;

    printf("NS_TimeToNextTimeout() lives!\n");
  }
  
  nsTimerXlib *timer;

  // Find the next timeout.
  
  if (nsTimerXlib::gHighestList->Count() > 0)
    timer = (nsTimerXlib*)nsTimerXlib::gHighestList->ElementAt(0);
  else
    if (nsTimerXlib::gHighList->Count() > 0)
      timer = (nsTimerXlib*)nsTimerXlib::gHighList->ElementAt(0);
    else
      if (nsTimerXlib::gNormalList->Count() > 0)
        timer = (nsTimerXlib*)nsTimerXlib::gNormalList->ElementAt(0);
      else
        if (nsTimerXlib::gLowList->Count() > 0)
          timer = (nsTimerXlib*)nsTimerXlib::gLowList->ElementAt(0);
        else
          if (nsTimerXlib::gLowestList->Count() > 0)
            timer = (nsTimerXlib*)nsTimerXlib::gLowestList->ElementAt(0);
          else
            timer = NULL;
    
  if (timer) {
    if ((timer->mFireTime.tv_sec < aTimer->tv_sec) ||
        ((timer->mFireTime.tv_sec == aTimer->tv_sec) &&
         (timer->mFireTime.tv_usec <= aTimer->tv_usec))) {
      aTimer->tv_sec = 0;
      aTimer->tv_usec = 0;
      return 1;
    }
    else {
      if (aTimer->tv_sec < timer->mFireTime.tv_sec)
        aTimer->tv_sec = timer->mFireTime.tv_sec - aTimer->tv_sec;
      else 
        aTimer->tv_sec = 0;
      // handle the overflow case
      if (aTimer->tv_usec < timer->mFireTime.tv_usec) {
        aTimer->tv_usec = timer->mFireTime.tv_usec - aTimer->tv_usec;
        // make sure we don't go past zero when we decrement
        if (aTimer->tv_sec)
          aTimer->tv_sec--;
      }
      else {
        aTimer->tv_usec -= timer->mFireTime.tv_usec;
      }
      return 1;
    }
  }
  else {
    return 0;
  }
}

static
void
NS_ProcessTimeouts(Display *aDisplay) 
{
  static int once = 1;

  if (once)
  {
    once = 0;

    printf("NS_ProcessTimeouts() lives!\n");
  }

  nsTimerXlib::gProcessingTimer = PR_TRUE;

  nsTimerXlib::ProcessTimeouts(nsTimerXlib::gHighestList);
  nsTimerXlib::ProcessTimeouts(nsTimerXlib::gHighList);
  nsTimerXlib::ProcessTimeouts(nsTimerXlib::gNormalList);

  if (XPending(aDisplay) == 0)
  {
#ifdef TIMER_DEBUG
    fprintf(stderr, "\n Handling Low Priority Stuff!!! Display is 0x%x\n", aDisplay);
#endif    
    nsTimerXlib::ProcessTimeouts(nsTimerXlib::gLowList);
    nsTimerXlib::ProcessTimeouts(nsTimerXlib::gLowestList);
  }
#ifdef TIMER_DEBUG  
  else
    fprintf(stderr, "\n Handling Event Stuff!!!", aDisplay);
#endif
  
  nsTimerXlib::gProcessingTimer = PR_FALSE;
}

