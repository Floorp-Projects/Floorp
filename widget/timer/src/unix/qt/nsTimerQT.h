/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsITimer.h"
#include "nsITimerCallback.h"


#include <qobject.h>

class nsTimerEventHandler : public QObject
{
    Q_OBJECT
public:
    nsTimerEventHandler(nsITimer * aTimer,
                        nsTimerCallbackFunc aFunc,
                        void *aClosure,
                        nsITimerCallback *aCallback);

public slots:    
    void FireTimeout();

private:
    nsTimerCallbackFunc mFunc;
    void              * mClosure;
    nsITimerCallback  * mCallback;
    nsITimer          * mTimer;
};

/*
 * Implementation of timers using Qt QTimer class 
 */
class TimerImpl : //public QObject,
				  public nsITimer
                  
{
//    Q_OBJECT
public:

public:
    TimerImpl();
    virtual ~TimerImpl();
    
    virtual nsresult Init(nsTimerCallbackFunc aFunc,
                          void *aClosure,
//                        PRBool aRepeat, 
                          PRUint32 aDelay);
    
    virtual nsresult Init(nsITimerCallback *aCallback,
//                        PRBool aRepeat, 
                          PRUint32 aDelay);
    
    NS_DECL_ISUPPORTS
    
    virtual void Cancel();
    virtual PRUint32 GetDelay() { return mDelay; }
    virtual void SetDelay(PRUint32 aDelay) { mDelay=aDelay; };
    virtual void* GetClosure() { return mClosure; }

//public slots:    
    void FireTimeout();
    
private:
    nsresult Init(PRUint32 aDelay);
    
private:
    PRUint32            mDelay;
    nsTimerCallbackFunc mFunc;
    void              * mClosure;
    nsITimerCallback  * mCallback;
    PRBool              mRepeat;
    TimerImpl         * mNext;
    QTimer            * mTimer;
    nsTimerEventHandler  * mEventHandler;
};
