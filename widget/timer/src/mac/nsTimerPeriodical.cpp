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

#include "nsTimerPeriodical.h"
#include "nsCOMPtr.h"
#include "prlog.h"


// this is also in the windows code. Prolly needs to be in nsITimer.h
#define NS_PRIORITY_IMMEDIATE NS_PRIORITY_HIGHEST


nsTimerPeriodical * nsTimerPeriodical::gPeriodical = nsnull;

//----------------------------------------------------------------------------------------
nsTimerPeriodical * nsTimerPeriodical::GetPeriodical()
//----------------------------------------------------------------------------------------
{
  if (gPeriodical == NULL)
    gPeriodical = new nsTimerPeriodical();
  return gPeriodical;
}

#pragma mark -

//----------------------------------------------------------------------------------------
nsTimerPeriodical::nsTimerPeriodical()
//----------------------------------------------------------------------------------------
: mTimers(nsnull)
, mReadyTimers(nsnull)
, mFiringTimer(nsnull)
{
}

//----------------------------------------------------------------------------------------
nsTimerPeriodical::~nsTimerPeriodical()
//----------------------------------------------------------------------------------------
{
  PR_ASSERT(!mTimers && !mReadyTimers);
}

#pragma mark -

//----------------------------------------------------------------------------------------
nsresult nsTimerPeriodical::AddTimer(nsTimerImpl * aTimer)
//----------------------------------------------------------------------------------------
{
  NS_ADDREF(aTimer);    // we hold a ref to the timer until it fires

  AddTimerToList(aTimer, mTimers);
  
  if (mTimers)
    StartRepeating();
 
  return NS_OK;
}

//----------------------------------------------------------------------------------------
nsresult nsTimerPeriodical::RemoveTimer(nsTimerImpl * aTimer)
//----------------------------------------------------------------------------------------
{
  // if someone tries to cancel a timer that is firing now, set a flag that
  // gets it removed once the fire callback returns
  if (aTimer == mFiringTimer)
  {
    aTimer->mCanceledInCallback = PR_TRUE;
    return NS_OK;
  }
  
  RemoveTimerFromList(aTimer, mTimers);
  RemoveTimerFromList(aTimer, mReadyTimers);

  if (aTimer && !aTimer->mTimerSpent) {   // if we haven't fired the timer, release it
    aTimer->mTimerSpent = PR_TRUE;
    NS_RELEASE(aTimer);
  }
  
  return NS_OK;
}

// Called through every event loop
// Loops through the list of available timers, and 
// fires off the appropriate ones
//----------------------------------------------------------------------------------------
void nsTimerPeriodical::RepeatAction(const EventRecord &inMacEvent)
//----------------------------------------------------------------------------------------
{
  ProcessTimers(::TickCount());
  
  if (!mTimers)
    StopRepeating();
  
  // ProcessTimers can put timers in the ready queue, so start idling
  // if we have any
  if (mReadyTimers)
    StartIdling();
}


//----------------------------------------------------------------------------------------
void nsTimerPeriodical::IdleAction(const EventRecord &inMacEvent)
//----------------------------------------------------------------------------------------
{
  FireNextReadyTimer();
  
  if (!mReadyTimers)
    StopIdling();
}


#pragma mark -


//----------------------------------------------------------------------------------------
void nsTimerPeriodical::ProcessTimers(UInt32 currentTicks)
//----------------------------------------------------------------------------------------
{
  PRBool done = PR_FALSE;

  while (!done)
  {
    nsTimerImpl* curTimer = mTimers;
    while (curTimer)
    {
      NS_ASSERTION(curTimer->IsGoodTimer(), "Bad timer!");

      nsTimerImpl* nextTimer = curTimer->mNext;
      
      if (curTimer->GetFireTime() <= currentTicks)
      {
        ProcessExpiredTimer(curTimer);    // this changes the list
        break;
      }

      curTimer = nextTimer;
    }
    done = (curTimer == nsnull);    // we got to the end
  }
}


//----------------------------------------------------------------------------------------
void nsTimerPeriodical::FireAndReprimeTimer(nsTimerImpl* aTimer)
//----------------------------------------------------------------------------------------
{
  nsCOMPtr<nsITimer> kungFuDeathGrip(aTimer);

  // if this timer is a precise timer, set the next fire time
  // before we execute the callback
  if (aTimer->GetType() == NS_TYPE_REPEATING_PRECISE)
    aTimer->SetDelay(aTimer->GetDelay());

  mFiringTimer = aTimer;

  aTimer->Fire();

  mFiringTimer = nsnull;

  // if this is a slack timer, set the delay now
  if (aTimer->GetType() == NS_TYPE_REPEATING_SLACK)
    aTimer->SetDelay(aTimer->GetDelay());

  // if this is a repeating timer, put it back in the list
  if (aTimer->GetType() != NS_TYPE_ONE_SHOT && !aTimer->mCanceledInCallback)
  {
    AddTimerToList(aTimer, mTimers);
  }
  else
  {
    if (!aTimer->mTimerSpent)
    {
      aTimer->mTimerSpent = PR_TRUE;
      aTimer->mCanceledInCallback = PR_FALSE;       // just in case someone wants to reuse it
      NS_RELEASE(aTimer);   // this timer is dead
    }
  }
}


//----------------------------------------------------------------------------------------
void nsTimerPeriodical::FireNextReadyTimer()
//----------------------------------------------------------------------------------------
{
  if (mReadyTimers)
  {
    nsTimerImpl* aTimer = mReadyTimers;
    
    RemoveTimerFromList(aTimer, mReadyTimers);
  
    FireAndReprimeTimer(aTimer);
  }
}


//----------------------------------------------------------------------------------------
void nsTimerPeriodical::AddToReadyQueue(nsTimerImpl* aTimer)
//----------------------------------------------------------------------------------------
{
  PRUint32  priority = aTimer->GetPriority();
  
  NS_ASSERTION(aTimer && priority < NS_PRIORITY_IMMEDIATE, "Should have non-immediate timer here");

  // do an insertion sort by priority
  if (mReadyTimers)
  {
    if (priority > mReadyTimers->GetPriority())
    {
      mReadyTimers->mPrev = aTimer;
      aTimer->mNext = mReadyTimers;
      mReadyTimers = aTimer;
    }
    else
    {
      nsTimerImpl *readyTimer = mReadyTimers;
      nsTimerImpl *prevTimer;
      // we know we will enter the while loop at least the first
      // time, and thus prevt will be initialized
      while (readyTimer && (readyTimer->GetPriority() >= priority))
      {
        NS_ASSERTION(readyTimer->IsGoodTimer(), "Bad timer!");

        prevTimer = readyTimer;
        readyTimer = readyTimer->mNext;
      }
      aTimer->mPrev = prevTimer;
      aTimer->mNext = prevTimer->mNext;
      prevTimer->mNext = aTimer;
      if (aTimer->mNext) aTimer->mNext->mPrev = aTimer;
    }
  }
  else
  {
    mReadyTimers = aTimer;
  }
}


//----------------------------------------------------------------------------------------
void nsTimerPeriodical::ProcessExpiredTimer(nsTimerImpl* aTimer)
//----------------------------------------------------------------------------------------
{
  RemoveTimerFromList(aTimer, mTimers);
  
  if (aTimer->GetPriority() >= NS_PRIORITY_IMMEDIATE)
  {
    FireAndReprimeTimer(aTimer);
  }
  else
  {
    // add to the ready queue
    AddToReadyQueue(aTimer);  
  }
}


#pragma mark -

//----------------------------------------------------------------------------------------
nsresult nsTimerPeriodical::AddTimerToList(nsTimerImpl* aTimer, nsTimerImpl*& timerList)
//----------------------------------------------------------------------------------------
{
  // make sure it's not already there
  RemoveTimerFromList(aTimer, timerList);
  // keep list sorted by fire time
  if (timerList)
  {
    if (aTimer->GetFireTime() < timerList->GetFireTime())
    {
      timerList->mPrev = aTimer;
      aTimer->mNext = timerList;
      timerList = aTimer;
    }
    else
    {
      nsTimerImpl *t = timerList;
      nsTimerImpl *prevt;
      // we know we will enter the while loop at least the first
      // time, and thus prevt will be initialized
      while (t && (t->GetFireTime() <= aTimer->GetFireTime()))
      {
        prevt = t;
        t = t->mNext;
      }
      aTimer->mPrev = prevt;
      aTimer->mNext = prevt->mNext;
      prevt->mNext = aTimer;
      if (aTimer->mNext) aTimer->mNext->mPrev = aTimer;
    }
  }
  else
  {
    timerList = aTimer;
  }
  
  return NS_OK;
}


//----------------------------------------------------------------------------------------
nsresult nsTimerPeriodical::RemoveTimerFromList(nsTimerImpl* aTimer, nsTimerImpl*& timerList)
//----------------------------------------------------------------------------------------
{
  nsTimerImpl* t = timerList;
  nsTimerImpl* next_t = nsnull;
  if (t) next_t = t->mNext;

  while (t)
  {
    if (t == aTimer)
    {
      if (timerList == t) timerList = t->mNext;
      if (t->mPrev) t->mPrev->mNext = t->mNext;
      if (t->mNext) t->mNext->mPrev = t->mPrev;
      t->mNext = nsnull;
      t->mPrev = nsnull;
    }
    t = next_t;
    if (t) next_t = t->mNext;
  }
 
  return NS_OK;
}
