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

#ifndef __nsTimer_h
#define __nsTimer_h

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <windows.h>

/*
 * Implementation of timers lifted from Windows front-end file timer.cpp
 */
class nsTimer : public nsITimer {
public:
  nsTimer();
  virtual ~nsTimer();

  NS_IMETHOD Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                );

  NS_IMETHOD Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                );

  NS_DECL_ISUPPORTS

  NS_IMETHOD_(void) Cancel();

  NS_IMETHOD_(PRUint32) GetDelay() { return mDelay; }
  NS_IMETHOD_(void) SetDelay(PRUint32 aDelay) { mDelay = aDelay; }

  NS_IMETHOD_(PRUint32) GetPriority() { return mPriority; }
  NS_IMETHOD_(void) SetPriority(PRUint32 aPriority);

  NS_IMETHOD_(PRUint32) GetType() { return mType; }
  NS_IMETHOD_(void) SetType(PRUint32 aType) { mType = aType; }

  NS_IMETHOD_(void*) GetClosure() { return mClosure; }

  virtual void Fire();

  void StartOSTimer(PRUint32 aDelay);
  void KillOSTimer();

private:
  nsresult Init(PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType);

  PRUint32 mDelay;
  PRUint32 mPriority;
  PRUint32 mType;

  nsTimerCallbackFunc mFunc;
  void *mClosure;
  nsITimerCallback *mCallback;

  UINT mTimerID;

  bool mTimerRunning;
};

#endif // __nsTimer_h
