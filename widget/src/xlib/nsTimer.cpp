/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "prlog.h"

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

extern "C" int  NS_TimeToNextTimeout(struct timeval *aTimer);
extern "C" void NS_ProcessTimeouts(void);

class TimerImpl : public nsITimer
{
public:
  static TimerImpl *gTimerList;
  static struct timeval gTimer;
  static struct timeval gNextFire;
  static void ProcessTimeouts(struct timeval *aNow);
  TimerImpl();
  ~TimerImpl();

  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                        void *aClosure,
                        PRUint32 aDelay);

  virtual nsresult Init(nsITimerCallback *aCallback,
                        PRUint32 aDelay);

  NS_DECL_ISUPPORTS

  virtual void Cancel();
  void Fire(struct timeval *aNow);
  
  virtual PRUint32 GetDelay() { return 0; };
  virtual void SetDelay(PRUint32 aDelay) {};
  virtual void *GetClosure() { return mClosure; }

  // this needs to be public so that the mainloop can
  // find the next fire time...
  struct timeval mFireTime;
  TimerImpl *mNext;

private:
  nsresult Init(PRUint32 aDelay);
  nsTimerCallbackFunc mFunc;
  void *mClosure;
  PRUint32 mDelay;
  nsITimerCallback *mCallback;

};


TimerImpl *TimerImpl::gTimerList = NULL;
struct timeval TimerImpl::gTimer = {0, 0};
struct timeval TimerImpl::gNextFire = {0, 0};

TimerImpl::TimerImpl()
{
  //printf("TimerImpl::TimerImpl (%p) called.\n",
  //this);
  NS_INIT_REFCNT();
  mFunc = NULL;
  mCallback = NULL;
  mNext = NULL;
  mClosure = NULL;
}

TimerImpl::~TimerImpl()
{
  //printf("TimerImpl::~TimerImpl (%p) called.\n",
  //       this);
  Cancel();
  NS_IF_RELEASE(mCallback);
}

NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID)

nsresult
TimerImpl::Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay)
{
  mFunc = aFunc;
  mClosure = aClosure;
  return Init(aDelay);
}

nsresult 
TimerImpl::Init(nsITimerCallback *aCallback,
                PRUint32 aDelay)
{
    mCallback = aCallback;
    NS_ADDREF(mCallback);

    return Init(aDelay);
}

nsresult
TimerImpl::Init(PRUint32 aDelay)
{
  struct timeval Now;
  //  printf("TimerImpl::Init (%p) called with delay %d\n",
  //this, aDelay);
  // get the cuurent time
  gettimeofday(&Now, NULL);
  mFireTime.tv_sec = Now.tv_sec + (aDelay / 1000);
  mFireTime.tv_usec = Now.tv_usec + (aDelay * 1000);
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
      TimerImpl *pPrev = gTimerList;
      TimerImpl *pCurrent = gTimerList;
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
  return NS_OK;
}

void
TimerImpl::Fire(struct timeval *aNow)
{
  //  printf("TimerImpl::Fire (%p) called at %ld / %ld\n",
  //         this,
  //aNow->tv_sec, aNow->tv_usec);
  if (mFunc != NULL) {
    (*mFunc)(this, mClosure);
  }
  else if (mCallback != NULL) {
    mCallback->Notify(this);
  }
}

void
TimerImpl::Cancel()
{
  TimerImpl *me = this;
  TimerImpl *p;
  //  printf("TimerImpl::Cancel (%p) called.\n",
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
TimerImpl::ProcessTimeouts(struct timeval *aNow)
{
  TimerImpl *p = gTimerList;
  if (aNow->tv_sec == 0 &&
      aNow->tv_usec == 0) {
    gettimeofday(aNow, NULL);
  }
  //  printf("TimerImpl::ProcessTimeouts called at %ld / %ld\n",
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
      p->Fire(aNow);
      //  Clear the timer.
      //  Period synced.
      p->Cancel();
      NS_RELEASE(p);
      //  Reset the loop (can't look at p->pNext now, and called
      //      code may have added/cleared timers).
      //  (could do this by going recursive and returning).
      p = gTimerList;
    }
    else {
      p = p->mNext;
    }
  }
}

NS_BASE nsresult NS_NewTimer(nsITimer **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }  
  
  TimerImpl *timer = new TimerImpl();
  if (nsnull == timer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}

int NS_TimeToNextTimeout(struct timeval *aTimer) {
  TimerImpl *timer;
  timer = TimerImpl::gTimerList;
  if (timer) {
    if ((timer->mFireTime.tv_sec < aTimer->tv_sec) ||
        ((timer->mFireTime.tv_sec == aTimer->tv_sec) &&
         (timer->mFireTime.tv_usec <= aTimer->tv_usec))) {
      aTimer->tv_sec = 0;
      aTimer->tv_usec = 0;
      return 1;
    }
    else {
      aTimer->tv_sec -= timer->mFireTime.tv_sec;
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

void NS_ProcessTimeouts(void) {
  struct timeval now;
  now.tv_sec = 0;
  now.tv_usec = 0;
  TimerImpl::ProcessTimeouts(&now);
}
