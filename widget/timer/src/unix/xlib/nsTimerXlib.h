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
 *  Tony Tsui <tony@igelaus.com.au>
 */

#ifndef __nsTimerXlib_h
#define __nsTimerXlib_h

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <sys/time.h>

class nsVoidArray;

class nsTimerXlib : public nsITimer
{
public:
  static void ProcessTimeouts(nsVoidArray *array);
  nsTimerXlib();
  virtual ~nsTimerXlib();

  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                );

  virtual nsresult Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                );

  NS_DECL_ISUPPORTS

  virtual void Cancel();
  PRBool Fire();
  
  virtual PRUint32 GetDelay() { return mDelay; }
  virtual void SetDelay(PRUint32 aDelay);

  virtual PRUint32 GetPriority() { return mPriority; }
  virtual void SetPriority(PRUint32 aPriority);

  virtual PRUint32 GetType() { return mType; }
  virtual void SetType(PRUint32 aType);

  virtual void *GetClosure() { return mClosure; }

  static nsVoidArray *gHighestList;
  static nsVoidArray *gHighList;
  static nsVoidArray *gNormalList;
  static nsVoidArray *gLowList;
  static nsVoidArray *gLowestList;

  static PRBool      gTimeoutAdded;
  static PRBool      gProcessingTimer;

  struct timeval       mFireTime;  
  PRUint32             mDelay;
private:
  nsresult Init(PRUint32 aDelay, PRUint32 aPriority);
  nsresult EnsureWindowService();

  void *               mClosure;
  nsITimerCallback *   mCallback;
  nsTimerCallbackFunc  mFunc;
  PRUint32             mPriority;
  PRUint32             mType;

};
#endif // __nsTimerXlib_h
