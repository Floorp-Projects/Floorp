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
//#include <stdio.h>

PRUintn nsThread::kIThreadSelfIndex = 0;

////////////////////////////////////////////////////////////////////////////////

nsThread::nsThread()
    : mThread(nsnull), mRunnable(nsnull), mDead(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsresult
nsThread::Init(nsIRunnable* runnable,
               PRUint32 stackSize,
               PRThreadPriority priority,
               PRThreadScope scope,
               PRThreadState state)
{
    mRunnable = runnable;
    NS_ADDREF(mRunnable);

    NS_ADDREF_THIS();   // released in nsThread::Exit
    if (state == PR_JOINABLE_THREAD)
        NS_ADDREF_THIS();   // released in nsThread::Join
    mThread = PR_CreateThread(PR_USER_THREAD, Main, this,
                              priority, scope, state, stackSize);
//    printf("%x %x (%d) create\n", this, mThread, mRefCnt);
    if (mThread == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsThread::~nsThread()
{
//    printf("%x %x (%d) destroy\n", this, mThread, mRefCnt);
    NS_IF_RELEASE(mRunnable);
}

void
nsThread::Main(void* arg)
{
    nsThread* self = (nsThread*)arg;

    nsresult rv = NS_OK;
    rv = self->RegisterThreadSelf();
    NS_ASSERTION(rv == NS_OK, "failed to set thread self");

//    printf("%x %x (%d) start run\n", self, self->mThread, self->mRefCnt);
    rv = self->mRunnable->Run();
    NS_ASSERTION(NS_SUCCEEDED(rv), "runnable failed");

    PRThreadState state;
    rv = self->GetState(&state);
//    printf("%x %x (%d) end run\n", self, self->mThread, self->mRefCnt);
}

void
nsThread::Exit(void* arg)
{
    nsThread* self = (nsThread*)arg;
    nsresult rv = NS_OK;
    self->mDead = PR_TRUE;
//    printf("%x %x (%d) exit\n", self, self->mThread, self->mRefCnt - 1);
    NS_RELEASE(self);
}

NS_IMPL_ISUPPORTS(nsThread, nsIThread::GetIID());

NS_IMETHODIMP
nsThread::Join()
{
    // don't check for mDead here because nspr calls Exit (cleaning up
    // thread-local storage) before they let us join with the thread

//    printf("%x %x (%d) start join\n", this, mThread, mRefCnt);
    PRStatus status = PR_JoinThread(mThread);
    // XXX can't use NS_RELEASE here because the macro wants to set
    // this to null (bad c++)
//    printf("%x %x (%d) end join\n", this, mThread, mRefCnt);
    if (status == PR_SUCCESS) {
        this->Release();   // most likely the final release of this thread 
        return NS_OK;
    }
    else
        return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsThread::GetPriority(PRThreadPriority *result)
{
    if (mDead)
        return NS_ERROR_FAILURE;
    *result = PR_GetThreadPriority(mThread);
    return NS_OK;
}

NS_IMETHODIMP
nsThread::SetPriority(PRThreadPriority value)
{
    if (mDead)
        return NS_ERROR_FAILURE;
    PR_SetThreadPriority(mThread, value);
    return NS_OK;
}

NS_IMETHODIMP
nsThread::Interrupt()
{
    if (mDead)
        return NS_ERROR_FAILURE;
    PRStatus status = PR_Interrupt(mThread);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsThread::GetScope(PRThreadScope *result)
{
    if (mDead)
        return NS_ERROR_FAILURE;
    *result = PR_GetThreadScope(mThread);
    return NS_OK;
}

NS_IMETHODIMP
nsThread::GetState(PRThreadState *result)
{
    if (mDead)
        return NS_ERROR_FAILURE;
    *result = PR_GetThreadState(mThread);
    return NS_OK;
}

NS_IMETHODIMP
nsThread::GetPRThread(PRThread* *result)
{
    if (mDead)
        return NS_ERROR_FAILURE;
    *result = mThread;
    return NS_OK;
}

NS_BASE nsresult
NS_NewThread(nsIThread* *result, 
             nsIRunnable* runnable,
             PRUint32 stackSize,
             PRThreadPriority priority,
             PRThreadScope scope,
             PRThreadState state)
{
    nsresult rv;
    nsThread* thread = new nsThread();
    if (thread == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = thread->Init(runnable, stackSize, priority, scope, state);
    if (NS_FAILED(rv)) {
        delete thread;
        return rv;
    }

    NS_ADDREF(thread);
    *result = thread;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsThread::RegisterThreadSelf()
{
    PRStatus status;

    if (kIThreadSelfIndex == 0) {
        status = PR_NewThreadPrivateIndex(&kIThreadSelfIndex, Exit);
        if (status != PR_SUCCESS) return NS_ERROR_FAILURE;
        NS_ASSERTION(kIThreadSelfIndex != 0, "couldn't get thread private index");
    }

    status = PR_SetThreadPrivate(kIThreadSelfIndex, this);
    if (status != PR_SUCCESS) return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_BASE nsresult
nsIThread::GetCurrent(nsIThread* *result)
{
    return GetIThread(PR_CurrentThread(), result);
}

NS_BASE nsresult
nsIThread::GetIThread(PRThread* prthread, nsIThread* *result)
{
    PRStatus status;
    nsThread* thread;

    if (nsThread::kIThreadSelfIndex == 0) {
        status = PR_NewThreadPrivateIndex(&nsThread::kIThreadSelfIndex, nsThread::Exit);
        if (status != PR_SUCCESS) return NS_ERROR_FAILURE;
        NS_ASSERTION(nsThread::kIThreadSelfIndex != 0, "couldn't get thread private index");
    }

    thread = (nsThread*)PR_GetThreadPrivate(nsThread::kIThreadSelfIndex);
    if (thread == nsnull) {
        // if the current thread doesn't have an nsIThread associated
        // with it, make one
        thread = new nsThread();
        if (thread == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(thread);
        thread->SetPRThread(prthread);
        nsresult rv = thread->RegisterThreadSelf();
        if (NS_FAILED(rv)) return rv;
    }
    NS_ADDREF(thread);
    *result = thread;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsThreadPool::nsThreadPool(PRUint32 minThreads, PRUint32 maxThreads)
    : mThreads(nsnull), mRequests(nsnull),
      mMinThreads(minThreads), mMaxThreads(maxThreads), mShuttingDown(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsresult
nsThreadPool::Init(PRUint32 stackSize,
                   PRThreadPriority priority,
                   PRThreadScope scope)
{
    nsresult rv;

    rv = NS_NewISupportsArray(&mThreads);
    if (NS_FAILED(rv)) return rv;
    
    rv = NS_NewISupportsArray(&mRequests);
    if (NS_FAILED(rv)) return rv;

    mRequestMonitor = PR_NewMonitor();
    if (mRequestMonitor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    PR_CEnterMonitor(this);

    for (PRUint32 i = 0; i < mMinThreads; i++) {
        nsThreadPoolRunnable* runnable =
            new nsThreadPoolRunnable(this);
        if (runnable == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(runnable);

        nsIThread* thread;

        rv = NS_NewThread(&thread, runnable, stackSize, priority, scope, 
                          PR_JOINABLE_THREAD);  // needed for Shutdown
        NS_RELEASE(runnable);
        if (NS_FAILED(rv)) goto exit;

        rv = mThreads->AppendElement(thread);
        NS_RELEASE(thread);
        if (NS_FAILED(rv)) goto exit;
    }
    // wait for some worker thread to be ready
    PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);

  exit:
    PR_CExitMonitor(this);
    return rv;
}

nsThreadPool::~nsThreadPool()
{
    if (mThreads) {
        Shutdown();
        NS_RELEASE(mThreads);
    }

    NS_IF_RELEASE(mRequests);

    if (mRequestMonitor) {
        PR_DestroyMonitor(mRequestMonitor);
    }
}

NS_IMPL_ISUPPORTS(nsThreadPool, nsIThreadPool::GetIID());

NS_IMETHODIMP
nsThreadPool::DispatchRequest(nsIRunnable* runnable)
{
    nsresult rv;
    PR_EnterMonitor(mRequestMonitor);

    if (mShuttingDown) {
        rv = NS_ERROR_FAILURE;
    }
    else {
        rv = mRequests->AppendElement(runnable);
        if (NS_SUCCEEDED(rv))
            PR_Notify(mRequestMonitor);
    }
    PR_ExitMonitor(mRequestMonitor);
    return rv;
}

nsIRunnable*
nsThreadPool::GetRequest()
{
    nsresult rv = NS_OK;
    nsIRunnable* request = nsnull;
 
    PR_EnterMonitor(mRequestMonitor);

    while (mRequests->Count() == 0) {
        if (mShuttingDown) {
            rv = NS_ERROR_FAILURE;
            break;
        }
//        printf("thread %x waiting\n", PR_CurrentThread());
        PRStatus status = PR_Wait(mRequestMonitor, PR_INTERVAL_NO_TIMEOUT);
        if (status != PR_SUCCESS || mShuttingDown) {
            rv = NS_ERROR_FAILURE;
            break;
        }
    }

    if (NS_SUCCEEDED(rv)) {
        NS_ASSERTION(mRequests->Count() > 0, "request queue out of sync");
        request = (nsIRunnable*)(*mRequests)[0];
        NS_ASSERTION(request != nsnull, "null runnable");

        PRBool removed = mRequests->RemoveElementAt(0);
        NS_ASSERTION(removed, "nsISupportsArray broken");
    }
    PR_ExitMonitor(mRequestMonitor);
    return request;
}

NS_IMETHODIMP
nsThreadPool::ProcessPendingRequests()
{
    nsresult rv;
    PR_CEnterMonitor(this);

    while (mRequests->Count() > 0) {
        PRStatus status = PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);
        if (status != PR_SUCCESS) {
            rv = NS_ERROR_FAILURE; // our thread was interrupted!
            break;
        }
    }

    PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP
nsThreadPool::Shutdown()
{
    nsresult rv = NS_OK;
    PRUint32 count;
    PRUint32 i;

    mShuttingDown = PR_TRUE;
    ProcessPendingRequests();
    
    // then interrupt the threads and join them
    count = mThreads->Count();
    for (i = 0; i < count; i++) {
        nsIThread* thread = (nsIThread*)((*mThreads)[0]);

        // we don't care about the error from Interrupt, because the
        // thread may have already terminated its event loop
        (void)thread->Interrupt();

        rv = thread->Join();
        // don't break out of the loop because of an error here
        NS_ASSERTION(NS_SUCCEEDED(rv), "Join failed");

        NS_RELEASE(thread);
        rv = mThreads->RemoveElementAt(0);
        // don't break out of the loop because of an error here
        NS_ASSERTION(NS_SUCCEEDED(rv), "RemoveElementAt failed");
    }

    return rv;
}

NS_BASE nsresult
NS_NewThreadPool(nsIThreadPool* *result,
                 PRUint32 minThreads, PRUint32 maxThreads,
                 PRUint32 stackSize,
                 PRThreadPriority priority,
                 PRThreadScope scope)
{
    nsresult rv;
    nsThreadPool* pool = new nsThreadPool(minThreads, maxThreads);
    if (pool == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = pool->Init(stackSize, priority, scope);
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
    nsresult rv = NS_OK;
    nsIRunnable* request;

    // let the thread pool know we're ready
    PR_CEnterMonitor(mPool);
    PR_CNotify(mPool);
    PR_CExitMonitor(mPool);

    while ((request = mPool->GetRequest()) != nsnull) {
//        printf("running %x, thread %x\n", this, PR_CurrentThread());
        rv = request->Run();
        NS_ASSERTION(NS_SUCCEEDED(rv), "runnable failed");

        // let the thread pool know we're finished a run
        PR_CEnterMonitor(mPool);
        PR_CNotify(mPool);
        PR_CExitMonitor(mPool);
    }
//    printf("quitting %x, thread %x\n", this, PR_CurrentThread());
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
