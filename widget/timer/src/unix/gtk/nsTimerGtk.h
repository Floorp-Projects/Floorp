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

#ifndef __nsTimerGtk_h
#define __nsTimerGtk_h

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <gtk/gtk.h>

/*
 * Implementation of timers using Gtk timer facility 
 */
class nsTimerGtk : public nsITimer 
{
public:

  nsTimerGtk();
  virtual ~nsTimerGtk();

  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
//              PRBool aRepeat, 
                PRUint32 aDelay);

  virtual nsresult Init(nsITimerCallback *aCallback,
//              PRBool aRepeat, 
                PRUint32 aDelay);

  NS_DECL_ISUPPORTS

  virtual void Cancel();
  virtual PRUint32 GetDelay() { return mDelay; }
  virtual void SetDelay(PRUint32 aDelay) { mDelay=aDelay; };
  virtual void* GetClosure() { return mClosure; }

  void FireTimeout();

private:
  nsresult Init(PRUint32 aDelay);

  PRUint32             mDelay;
  nsTimerCallbackFunc  mFunc;
  void *               mClosure;
  nsITimerCallback *   mCallback;
  // PRBool            mRepeat;
  nsTimerGtk *         mNext;
  guint                mTimerId;
};

#endif // __nsTimerGtk_h
