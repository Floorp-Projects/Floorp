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

#ifndef __nsTimerXlib_h
#define __nsTimerXlib_h

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <sys/time.h>

class nsTimerXlib : public nsITimer
{
public:
  static nsTimerXlib *gTimerList;
  static struct timeval gTimer;
  static struct timeval gNextFire;
  static void ProcessTimeouts(struct timeval *aNow);
  nsTimerXlib();
  virtual ~nsTimerXlib();

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
  struct timeval       mFireTime;
  nsTimerXlib *        mNext;

private:
  nsresult Init(PRUint32 aDelay);
  nsresult EnsureWindowService();

  nsTimerCallbackFunc  mFunc;
  void *               mClosure;
  PRUint32             mDelay;
  nsITimerCallback *   mCallback;
};

#endif // __nsTimerXlib_h
