/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      Added PR_CALLBACK for Optlink use in OS2
 */

#ifndef nsThread_h__
#define nsThread_h__

#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIThreadPool.h"
#include "nsISupportsArray.h"
#include "nsCOMArray.h"
#include "prcvar.h"
#include "nsCOMPtr.h"

class nsThreadPoolBusyBody;

class nsThread : public nsIThread 
{
public:
    NS_DECL_ISUPPORTS

    // nsIThread methods:
    NS_DECL_NSITHREAD
 
    // nsThread methods:
    nsThread();
    virtual ~nsThread();

    nsresult RegisterThreadSelf();
    void SetPRThread(PRThread* thread) { mThread = thread; }
    void WaitUntilReadyToStartMain();

    static void PR_CALLBACK Main(void* arg);
    static void PR_CALLBACK Exit(void* arg);
    static void PR_CALLBACK Shutdown();

    static PRUintn kIThreadSelfIndex;

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

protected:
    PRThread*                   mThread;
    nsCOMPtr<nsIRunnable>       mRunnable;
    PRBool                      mDead;
    PRLock*                     mStartLock;
};

////////////////////////////////////////////////////////////////////////////////

class nsThreadPool : public nsIThreadPool
{
friend class nsThreadPoolBusyBody;
public:
    NS_DECL_ISUPPORTS

    // nsIThreadPool methods:
    NS_DECL_NSITHREADPOOL

    // nsThreadPool methods:
    nsThreadPool();
    virtual ~nsThreadPool();

    nsIRunnable* GetRequest(nsIThread* currentThread);
    void         RequestDone(nsIRunnable *request);

    static PRBool InterruptThreads(nsISupports* aElement, 
                                   void *aData);

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
    
protected:
    nsresult AddThread();
    nsresult RemoveThread(nsIThread* currentThread);

    nsCOMPtr<nsISupportsArray>  mThreads;
    nsCOMArray<nsIRunnable>     mPendingRequests;
    nsCOMArray<nsIRunnable>     mRunningRequests;
    
    PRLock*                     mLock;
    PRCondVar*                  mThreadExit;
    PRCondVar*                  mPendingRequestAdded;
    PRCondVar*                  mPendingRequestsAtZero;

    
    PRUint32                    mStackSize;
    PRThreadPriority            mPriority;
    PRThreadScope               mScope;

    PRUint32                    mMinThreads;
    PRUint32                    mMaxThreads;
    PRUint32                    mBusyThreads;
    PRBool                      mShuttingDown;
};

////////////////////////////////////////////////////////////////////////////////

class nsThreadPoolRunnable : public nsIRunnable 
{
public:
    NS_DECL_ISUPPORTS

    // nsIRunnable methods:
    NS_DECL_NSIRUNNABLE

    // nsThreadPoolRunnable methods:
    nsThreadPoolRunnable(nsThreadPool* pool);
    virtual ~nsThreadPoolRunnable();

protected:
    nsCOMPtr<nsThreadPool>      mPool;

};

////////////////////////////////////////////////////////////////////////////////

#endif // nsThread_h__
