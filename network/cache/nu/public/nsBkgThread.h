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

#ifndef nsBkgThread_h__
#define nsBkgThread_h__

//#include "nsISupports.h"
#include <prthread.h>
#include <prinrval.h>

/*
    Creates a background thread that maintains odd 
    tasks like updating, expiration, validation and 
    garbage collection. 
    
    Note that this is a noop active when the cache 
    manager is offline. 

    In PRThread terms, this is a PR_USER_THREAD with 
    local scope.
*/
class nsBkgThread//: public nsISupports
{

public:
            nsBkgThread(PRIntervalTime iSleepTime, PRBool bStart=PR_TRUE);
    virtual ~nsBkgThread();
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
    void            Process(void);
    virtual void    Run(void) = 0;
    void            Stop(void);
protected:
    PRThread*       m_pThread;
    PRBool          m_bContinue;
    PRIntervalTime  m_SleepTime;

private:
    nsBkgThread(const nsBkgThread& o);
    nsBkgThread& operator=(const nsBkgThread& o);

};

#endif // nsBkgThread_h__

