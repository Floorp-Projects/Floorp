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

#include "nsTimerXlib.h"

#ifndef MOZ_MONOLITHIC_TOOLKIT
#include "nsIXlibWindowService.h"
#include "nsIServiceManager.h"
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

#include <unistd.h>
#include <stdio.h>

#include "prlog.h"

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

#ifndef MOZ_MONOLITHIC_TOOLKIT
static int  NS_TimeToNextTimeout(struct timeval *aTimer);
static void NS_ProcessTimeouts(void);
#else
extern "C" int  NS_TimeToNextTimeout(struct timeval *aTimer);
extern "C" void NS_ProcessTimeouts(void);
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

nsTimerXlib *nsTimerXlib::gTimerList = NULL;
struct timeval nsTimerXlib::gTimer = {0, 0};
struct timeval nsTimerXlib::gNextFire = {0, 0};

nsTimerXlib::nsTimerXlib()
{
  //printf("nsTimerXlib::nsTimerXlib (%p) called.\n",
  //this);
  NS_INIT_REFCNT();
  mFunc = NULL;
  mCallback = NULL;
  mNext = NULL;
  mClosure = NULL;
  mType = NS_TYPE_ONE_SHOT;
}

nsTimerXlib::~nsTimerXlib()
{
  //printf("nsTimerXlib::~nsTimerXlib (%p) called.\n",
  //       this);
  Cancel();
  NS_IF_RELEASE(mCallback);
}

NS_IMPL_ISUPPORTS(nsTimerXlib, kITimerIID)

nsresult
nsTimerXlib::Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
  mFunc = aFunc;
  mClosure = aClosure;
  mType = aType;
  return Init(aDelay);
}

nsresult 
nsTimerXlib::Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
  mType = aType;
  mCallback = aCallback;
  NS_ADDREF(mCallback);
  
  return Init(aDelay);
}

nsresult
nsTimerXlib::Init(PRUint32 aDelay)
{
  struct timeval Now;
  //  printf("nsTimerXlib::Init (%p) called with delay %d\n",
  //this, aDelay);
  // get the cuurent time
  mDelay = aDelay;
  gettimeofday(&Now, NULL);
  mFireTime.tv_sec = Now.tv_sec + (aDelay / 1000);
  mFireTime.tv_usec = Now.tv_usec + ((aDelay%1000) * 1000);
  if (mFireTime.tv_usec >= 1000000) {
    mFireTime.tv_sec++;
    mFireTime.tv_usec -= 1000000;
  }
  //printf("fire set to %ld / %ld\n",
  //mFireTime.tv_sec, mFireTime.tv_usec);
  // set the next pointer to nothing.
  mNext = NULL;
  // add ourself to the list
  if (!gTimerList) {
    // no list here.  I'm the start!
    //printf("This is the beginning of the list..\n");
    gTimerList = this;
  }
  else {
    // is it before everything else on the list?
    if ((mFireTime.tv_sec < gTimerList->mFireTime.tv_sec) &&
        (mFireTime.tv_usec < gTimerList->mFireTime.tv_usec)) {
      //      printf("This is before the head of the list...\n");
      mNext = gTimerList;
      gTimerList = this;
    }
    else {
      nsTimerXlib *pPrev = gTimerList;
      nsTimerXlib *pCurrent = gTimerList;
      while (pCurrent && ((pCurrent->mFireTime.tv_sec <= mFireTime.tv_sec) &&
                          (pCurrent->mFireTime.tv_usec <= mFireTime.tv_usec))) {
        pPrev = pCurrent;
        pCurrent = pCurrent->mNext;
      }
      PR_ASSERT(pPrev);

      // isnert it after pPrev ( this could be at the end of the list)
      mNext = pPrev->mNext;
      pPrev->mNext = this;
    }
  }
  NS_ADDREF(this);

  EnsureWindowService();

  return NS_OK;
}

PRBool
nsTimerXlib::Fire(struct timeval *aNow)
{
  nsCOMPtr<nsITimer> kungFuDeathGrip = this;
  //  printf("nsTimerXlib::Fire (%p) called at %ld / %ld\n",
  //         this,
  //aNow->tv_sec, aNow->tv_usec);
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
  nsTimerXlib *me = this;
  nsTimerXlib *p;
  //  printf("nsTimerXlib::Cancel (%p) called.\n",
  //         this);
  if (gTimerList == this) {
    // first element in the list lossage...
    gTimerList = mNext;
  }
  else {
    // walk until there's no next pointer
    for (p = gTimerList; p && p->mNext && (p->mNext != this); p = p->mNext)
      ;

    // if we found something valid pull it out of the list
    if (p && p->mNext && p->mNext == this) {
      p->mNext = mNext;
    }
    else {
      // get out before we delete something that looks bogus
      return;
    }
  }
  // if we got here it must have been a valid element so trash it
  NS_RELEASE(me);
  
}

void
nsTimerXlib::ProcessTimeouts(struct timeval *aNow)
{
  nsTimerXlib *p = gTimerList;
  nsTimerXlib *tmp;
  struct timeval ntv;
  int res;

  if (aNow->tv_sec == 0 &&
      aNow->tv_usec == 0) {
    gettimeofday(aNow, NULL);
  }
  //  printf("nsTimerXlib::ProcessTimeouts called at %ld / %ld\n",
  //         aNow->tv_sec, aNow->tv_usec);
  while (p) {
    if ((p->mFireTime.tv_sec < aNow->tv_sec) ||
        ((p->mFireTime.tv_sec == aNow->tv_sec) &&
         (p->mFireTime.tv_usec <= aNow->tv_usec))) {
      //  Make sure that the timer cannot be deleted during the
      //  Fire(...) call which may release *all* other references
      //  to p...
      //printf("Firing timeout for (%p)\n",
      //           p);
      NS_ADDREF(p);
      res = p->Fire(aNow);      
      if (res == 0) {
        p->Cancel();
        NS_RELEASE(p);
        p = gTimerList;
      } else {

        gettimeofday(&ntv, NULL);
        p->mFireTime.tv_sec = ntv.tv_sec + (p->mDelay / 1000);
        p->mFireTime.tv_usec = ntv.tv_usec + ((p->mDelay%1000) * 1000);
        tmp = p;
        p = p->mNext;
        NS_RELEASE(tmp);

      }
    }
    else {
      p = p->mNext;
    }
  }
}

#ifndef MOZ_MONOLITHIC_TOOLKIT
static NS_DEFINE_IID(kWindowServiceCID,NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,NS_XLIB_WINDOW_SERVICE_IID);
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

nsresult
nsTimerXlib::EnsureWindowService()
{
#ifndef MOZ_MONOLITHIC_TOOLKIT
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
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

  return NS_OK;
}

#ifndef MOZ_MONOLITHIC_TOOLKIT
static
#else
extern "C"
#endif /* !MOZ_MONOLITHIC_TOOLKIT */
int NS_TimeToNextTimeout(struct timeval *aTimer) 
{
#ifndef MOZ_MONOLITHIC_TOOLKIT
  static int once = 1;

  if (once)
  {
    once = 0;

    printf("NS_TimeToNextTimeout() lives!\n");
  }
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

  nsTimerXlib *timer;
  timer = nsTimerXlib::gTimerList;
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

#ifndef MOZ_MONOLITHIC_TOOLKIT
static
#else
extern "C"
#endif /* !MOZ_MONOLITHIC_TOOLKIT */
void
NS_ProcessTimeouts(void) 
{
#ifndef MOZ_MONOLITHIC_TOOLKIT
  static int once = 1;

  if (once)
  {
    once = 0;

    printf("NS_ProcessTimeouts() lives!\n");
  }
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

  struct timeval now;
  now.tv_sec = 0;
  now.tv_usec = 0;
  nsTimerXlib::ProcessTimeouts(&now);
}

#ifdef MOZ_MONOLITHIC_TOOLKIT
nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }  
  
  nsTimerXlib *timer = new nsTimerXlib();
  if (nsnull == timer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}
#endif /* MOZ_MONOLITHIC_TOOLKIT */
