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


NS_IMETHODIMP_(PRBool) nsTimerManager::IsTimerInQueue(nsITimer* timer)
{
  if (mReadyQueue == nsnull) return PR_FALSE;

  for (int i=0; i<mReadyQueue->Count(); i++) {
    if (mReadyQueue->ElementAt(i) == timer) return PR_TRUE;
  }
  return PR_FALSE;
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


NS_IMETHODIMP_(PRBool) nsTimerManager::HasReadyTimers(PRUint32 minTimerPriority)
{
  if (mReadyQueue == nsnull || mReadyQueue->Count() == 0) return PR_FALSE;

  nsTimer* timer = (nsTimer*) mReadyQueue->ElementAt(0);
  if (timer == nsnull) return PR_FALSE;

  return timer->GetPriority() >= minTimerPriority;
}


NS_IMETHODIMP_(void) nsTimerManager::FireNextReadyTimer(PRUint32 minTimerPriority)
{
  PR_ASSERT(HasReadyTimers(minTimerPriority) == PR_TRUE);

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
