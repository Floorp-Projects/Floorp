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

#ifndef __nsTimerManager_h
#define __nsTimerManager_h

#include "nsITimerQueue.h"
#include "nsIWindowsTimerMap.h"

#include "nsCRT.h"
//#include "prlog.h"
//#include <stdio.h>
#include <windows.h>
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
