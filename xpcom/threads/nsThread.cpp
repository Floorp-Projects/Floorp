/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#include "nsThread.h"
#include "prmem.h"

////////////////////////////////////////////////////////////////////////////////

nsThread::nsThread()
    : mThread(nsnull), mRunnable(nsnull)
{
    NS_INIT_REFCNT();
}

nsresult
nsThread::Init(nsIRunnable* runnable,
               PRUint32 stackSize,
               PRThreadType type,
               PRThreadPriority priority,
               PRThreadScope scope,
               PRThreadState state)
{
    mRunnable = runnable;
    NS_ADDREF(mRunnable);

    mThread = PR_CreateThread(type, Main, this,
                              priority, scope, PR_JOINABLE_THREAD, stackSize);
    if (mThread == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsThread::~nsThread()
{
    if (mThread) {
        PRStatus status = PR_SUCCESS;
        status = PR_Interrupt(mThread);
        NS_ASSERTION(status == PR_SUCCESS, "failed to interrupt worker");
        status = PR_JoinThread(mThread);
        NS_ASSERTION(status == PR_SUCCESS, "failed to join with worker");
        PR_Free(mThread);       // XXX right?
        mThread = nsnull;
    }
}

void
nsThread::Main(void* arg)
{
    nsThread* self = (nsThread*)arg;
    nsresult rv = NS_OK;
    rv = self->mRunnable->Run();
    NS_ASSERTION(NS_SUCCEEDED(rv), "runnable failed");
}

NS_IMPL_ISUPPORTS(nsThread, nsIThread::GetIID());

NS_IMETHODIMP
nsThread::Join()
{
    PRStatus status = PR_JoinThread(mThread);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsThread::GetPriority(PRThreadPriority *result)
{
    *result = PR_GetThreadPriority(mThread);
    return NS_OK;
}

NS_IMETHODIMP
nsThread::SetPriority(PRThreadPriority value)
{
    PR_SetThreadPriority(mThread, value);
    return NS_OK;
}

NS_IMETHODIMP
nsThread::Interrupt()
{
    PRStatus status = PR_Interrupt(mThread);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsThread::GetScope(PRThreadScope *result)
{
    *result = PR_GetThreadScope(mThread);
    return NS_OK;
}

NS_IMETHODIMP
nsThread::GetType(PRThreadType *result)
{
    *result = PR_GetThreadType(mThread);
    return NS_OK;
}

NS_IMETHODIMP
nsThread::GetState(PRThreadState *result)
{
    *result = PR_GetThreadState(mThread);
    return NS_OK;
}

NS_BASE nsresult
NS_NewThread(nsIThread* *result, 
             nsIRunnable* runnable,
             PRUint32 stackSize,
             PRThreadType type,
             PRThreadPriority priority,
             PRThreadScope scope,
             PRThreadState state)
{
    nsresult rv;
    nsThread* thread = new nsThread();
    if (thread == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = thread->Init(runnable, stackSize, type, priority, scope, state);
    if (NS_FAILED(rv)) {
        delete thread;
        return rv;
    }

    NS_ADDREF(thread);
    *result = thread;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsThreadPool::nsThreadPool(PRUint32 minThreads, PRUint32 maxThreads)
    : mThreads(nsnull), mRequests(nsnull),
      mMinThreads(minThreads), mMaxThreads(maxThreads)
{
    NS_INIT_REFCNT();
}

nsresult
nsThreadPool::Init(PRUint32 stackSize,
                   PRThreadType type,
                   PRThreadPriority priority,
                   PRThreadScope scope,
                   PRThreadState state)
{
    nsresult rv;

    rv = NS_NewISupportsArray(&mThreads);
    if (NS_FAILED(rv)) return rv;
    
    rv = NS_NewISupportsArray(&mRequests);
    if (NS_FAILED(rv)) return rv;
    
    for (PRUint32 i = 0; i < mMinThreads; i++) {
        nsThreadPoolRunnable* runnable =
            new nsThreadPoolRunnable(this);
        if (runnable == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(runnable);

        nsIThread* thread;
        rv = NS_NewThread(&thread, runnable, stackSize, type, 
                          priority, scope, state);
        NS_RELEASE(runnable);
        if (NS_FAILED(rv)) return rv;

        rv = mThreads->AppendElement(thread);
        NS_RELEASE(thread);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

nsThreadPool::~nsThreadPool()
{
    NS_RELEASE(mThreads);
    NS_RELEASE(mRequests);
}

NS_IMPL_ISUPPORTS(nsThreadPool, nsIThreadPool::GetIID());

NS_IMETHODIMP
nsThreadPool::DispatchRequest(nsIRunnable* runnable)
{
    nsresult rv;
    PR_CEnterMonitor(this);

    rv = mRequests->AppendElement(runnable);
    if (NS_SUCCEEDED(rv))
        PR_CNotify(this);
    
    PR_CExitMonitor(this);
    return rv;
}

nsIRunnable*
nsThreadPool::Dequeue()
{
    PR_CEnterMonitor(this);

    NS_ASSERTION(mRequests->Count() > 0, "request queue out of sync");
    nsIRunnable* request = (nsIRunnable*)(*mRequests)[0];
    PRBool removed = mRequests->RemoveElementAt(0);
    NS_ASSERTION(removed, "nsISupportsArray broken");

    PR_CExitMonitor(this);
    return request;
}

NS_BASE nsresult
NS_NewThreadPool(nsIThreadPool* *result,
                 PRUint32 minThreads, PRUint32 maxThreads,
                 PRUint32 stackSize,
                 PRThreadType type,
                 PRThreadPriority priority,
                 PRThreadScope scope,
                 PRThreadState state)
{
    nsresult rv;
    nsThreadPool* pool = new nsThreadPool(minThreads, maxThreads);
    if (pool == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = pool->Init(stackSize, type, priority, scope, state);
    if (NS_FAILED(rv)) {
        delete pool;
        return rv;
    }
    
    NS_ADDREF(pool);
    *result = pool;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsThreadPoolRunnable::nsThreadPoolRunnable(nsThreadPool* pool)
    : mPool(pool)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mPool);
}

nsThreadPoolRunnable::~nsThreadPoolRunnable()
{
    NS_RELEASE(mPool);
}

NS_IMPL_ISUPPORTS(nsThreadPoolRunnable, nsIRunnable::GetIID());

NS_IMETHODIMP
nsThreadPoolRunnable::Run()
{
    PR_CEnterMonitor(mPool);
    while (PR_TRUE) {
        PRStatus status = PR_CWait(mPool, PR_INTERVAL_NO_TIMEOUT);
        if (status != PR_SUCCESS) break;        // interrupted -- quit

        nsIRunnable* runnable = mPool->Dequeue();

        nsresult rv = NS_OK;
        rv = runnable->Run();
        NS_ASSERTION(NS_SUCCEEDED(rv), "runnable failed");
    }
    PR_CExitMonitor(mPool);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
