/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *	John C. Griggs <johng@corel.com>
 *
 */
#ifndef __nsTimerEventHandler_h__
#define __nsTimerEventHandler_h__

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <qobject.h>

class nsTimerEventHandler : public QObject
{
    Q_OBJECT
public:
    nsTimerEventHandler(nsITimer *aTimer,
                        nsTimerCallbackFunc aFunc,
                        void *aClosure,
                        nsITimerCallback *aCallback);
    ~nsTimerEventHandler();

public slots:    
    void FireTimeout();

private:
    nsTimerCallbackFunc mFunc;
    void                *mClosure;
    nsITimerCallback    *mCallback;
    nsITimer            *mTimer;
    PRUint32            mTimerHandlerID;
};

#endif // __nsTimerEventHandler_h__
