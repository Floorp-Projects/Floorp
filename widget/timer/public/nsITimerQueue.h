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
#ifndef nsITimerQueue_h___
#define nsITimerQueue_h___

#include "nscore.h"
#include "nsISupports.h"

class nsTimer;

/// Interface IID for nsITimerQueue
#define NS_ITIMERQUEUE_IID         \
{ 0x7d1f1070, 0x1dd2, 0x11b2,  \
{ 0xa5, 0x18, 0xe5, 0x2b, 0x26, 0xa2, 0x48, 0x7b } }


class nsITimerQueue : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITIMERQUEUE_IID)

  NS_IMETHOD_(void) AddReadyQueue(nsITimer* timer)=0;
  NS_IMETHOD_(PRBool) IsTimerInQueue(nsITimer* timer)=0;
  NS_IMETHOD_(PRBool) HasReadyTimers(PRUint32 minTimerPriority)=0;
  NS_IMETHOD_(void) FireNextReadyTimer(PRUint32 minTimerPriority)=0;
};

#endif // nsITimerQueue_h___
