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
 * 05/31/2000       IBM Corp.       Including os2TimerGlue.h instead of windows.h
 */

#ifndef __nsTimerManager_h
#define __nsTimerManager_h

#include "nsITimerQueue.h"
#include "nsIWindowsTimerMap.h"

#include "nsCRT.h"
//#include "prlog.h"
//#include <stdio.h>
#include "os2TimerGlue.h"
//#include <limits.h>
#include "nsHashtable.h"
#include "nsVoidArray.h"

class nsTimer;

class nsTimerManager : public nsITimerQueue, nsIWindowsTimerMap {
private:
  // pending timers
  nsHashtable* mTimers;

  // ready timers
  nsVoidArray* mReadyQueue;

  nsresult Init();

public:
  nsTimerManager();
  virtual ~nsTimerManager();

  NS_DECL_ISUPPORTS

  // --- manage map of Windows timer id's to Timer objects ---
  NS_IMETHOD_(nsTimer*) GetTimer(UINT id);
  NS_IMETHOD_(void) AddTimer(UINT timerID, nsTimer* timer);
  NS_IMETHOD_(void) RemoveTimer(UINT timerID);

  // --- manage priority queue of ready timers ---
  NS_IMETHOD_(void) AddReadyQueue(nsITimer* timer);
  // is this timer in the ready queue?
  NS_IMETHOD_(PRBool) IsTimerInQueue(nsITimer* timer);
  // does a timer above a priority level exist in the ready queue?
  NS_IMETHOD_(PRBool) HasReadyTimers(PRUint32 minTimerPriority);
  // fire the next timer above a priority level in the ready queue
  NS_IMETHOD_(void) FireNextReadyTimer(PRUint32 minTimerPriority);
};


#endif // __nsTimerManager_h
