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
 */

#include "nsWindowsTimer.h"
#include "nsTimerManager.h"
#include "nsCRT.h"
#include "prlog.h"
#include <stdio.h>
#include <windows.h>
#include <limits.h>
#include "nsHashtable.h"
#include "nsDeque.h"


NS_IMPL_ISUPPORTS2(nsTimerManager, nsITimerQueue, nsIWindowsTimerMap)


nsTimerManager::nsTimerManager() : nsITimerQueue(), nsIWindowsTimerMap()
{
  NS_INIT_ISUPPORTS();

  Init();
}


nsTimerManager::~nsTimerManager()
{
  delete mTimers;
  delete mReadyQueue;
}


nsresult nsTimerManager::Init()
{
  mReadyQueue = new nsVoidArray();
  if (mReadyQueue == nsnull) return NS_ERROR_OUT_OF_MEMORY;

  mTimers = new nsHashtable(128);
  if (mTimers == nsnull) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


NS_IMETHODIMP_(bool) nsTimerManager::IsTimerInQueue(nsITimer* timer)
{
  if (mReadyQueue == nsnull) return false;

  for (int i=0; i<mReadyQueue->Count(); i++) {
    if (mReadyQueue->ElementAt(i) == timer) return true;
  }
  return false;
}


NS_IMETHODIMP_(void) nsTimerManager::AddReadyQueue(nsITimer* timer)
{
  if (mReadyQueue == nsnull || IsTimerInQueue(timer)) return;

  int i = 0;
  for (; i<mReadyQueue->Count(); i++) {
    nsTimer* cur = (nsTimer*) mReadyQueue->ElementAt(i);
    if (cur->GetPriority() < timer->GetPriority()) break;
  }

  mReadyQueue->InsertElementAt(timer, i);

  NS_ADDREF(timer);
}


NS_IMETHODIMP_(bool) nsTimerManager::HasReadyTimers(PRUint32 minTimerPriority)
{
  if (mReadyQueue == nsnull || mReadyQueue->Count() == 0) return false;

  nsTimer* timer = (nsTimer*) mReadyQueue->ElementAt(0);
  if (timer == nsnull) return false;

  return timer->GetPriority() >= minTimerPriority;
}


NS_IMETHODIMP_(void) nsTimerManager::FireNextReadyTimer(PRUint32 minTimerPriority)
{
  PR_ASSERT(HasReadyTimers(minTimerPriority) == true);

  if (mReadyQueue == nsnull) return;

  nsTimer* timer = (nsTimer*) mReadyQueue->ElementAt(0);
  if (timer == nsnull) return;

  if (timer->GetPriority() >= minTimerPriority) {
    mReadyQueue->RemoveElementAt(0);

    timer->Fire();

    NS_RELEASE(timer);
  }
}


NS_IMETHODIMP_(void) nsTimerManager::AddTimer(UINT timerID, nsTimer* timer)
{
  if (mTimers != nsnull) {
    nsVoidKey key((void *) timerID);
    mTimers->Put(&key, timer);
  }
}


NS_IMETHODIMP_(void) nsTimerManager::RemoveTimer(UINT timerID)
{
  if (mTimers != nsnull) {
    nsVoidKey key((void *) timerID);
    mTimers->Remove(&key);
  }
}


NS_IMETHODIMP_(nsTimer*) nsTimerManager::GetTimer(UINT timerID)
{
  if (mTimers != nsnull) {
    nsVoidKey key((void *) timerID);
    return (nsTimer *) mTimers->Get(&key);
  }
  return nsnull;
}
