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
 */

#ifndef __nsTimerGtk_h
#define __nsTimerGtk_h

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <gtk/gtk.h>

#include <time.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * Implementation of timers using Gtk timer facility 
 */

class nsVoidArray;

class TimeVal {
public:
  TimeVal();
  TimeVal(const TimeVal& rv);   // copy constructor
  virtual ~TimeVal();

  void Set(PRUint32 sec, PRUint32 usec);

  TimeVal& operator+=(PRInt32 msec);
  
  TimeVal& operator=(const struct timeval &);

  PRBool operator==(const TimeVal &) const;
  PRBool operator==(const struct timeval &) const;

  PRBool operator<(const TimeVal &) const;
  PRBool operator>(const TimeVal &) const;

  PRBool operator<(const struct timeval &) const;
  PRBool operator>(const struct timeval &) const;

  PRBool operator<=(const TimeVal &) const;
  PRBool operator>=(const TimeVal &) const;

  PRBool operator<=(const struct timeval &) const;
  PRBool operator>=(const struct timeval &) const;

private:
  PRUint32 mSeconds;
  PRUint32 mUSeconds;
};


class nsTimerGtk : public nsITimer 
{
public:

  nsTimerGtk();
  virtual ~nsTimerGtk();
  
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

  // clean up statics
  static void Shutdown();

  NS_DECL_ISUPPORTS

  virtual void Cancel();

  virtual PRUint32 GetDelay() { return mDelay; }
  virtual void SetDelay(PRUint32 aDelay);

  virtual PRUint32 GetPriority() { return mPriority; }
  virtual void SetPriority(PRUint32 aPriority);

  virtual PRUint32 GetType()  { return mType; }
  virtual void SetType(PRUint32 aType);

  virtual void* GetClosure() { return mClosure; }

  PRBool FireTimeout();

  static nsVoidArray  *gHighestList;
  static nsVoidArray  *gHighList;
  static nsVoidArray  *gNormalList;
  static nsVoidArray  *gLowList;
  static nsVoidArray  *gLowestList;
  static PRBool        gTimeoutAdded;
  static PRBool        gProcessingTimer;
  static guint         gTimerID;

  TimeVal              mSchedTime;
  PRUint32             mDelay;
private:
  PRUint32             mPriority;
  PRUint32             mType;
  nsTimerCallbackFunc  mFunc;
  void *               mClosure;
  nsITimerCallback *   mCallback;
};

#endif // __nsTimerGtk_h
