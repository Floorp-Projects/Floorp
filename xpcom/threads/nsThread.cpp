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
 */

#include "nsThread.h"
#include "prmem.h"
#include "prlog.h"
#include "nsAutoLock.h"

PRUintn nsThread::kIThreadSelfIndex = 0;
static nsIThread *gMainThread = 0;

#if defined(PR_LOGGING)
//
// Log module for nsIThread logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsIThread:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
// gSocketLog is defined in nsSocketTransport.cpp
//
PRLogModuleInfo* nsIThreadLog = nsnull;

#endif /* PR_LOGGING */

////////////////////////////////////////////////////////////////////////////////

nsThread::nsThread()
    : mThread(nsnull), mDead(PR_FALSE)
{
    NS_INIT_REFCNT();

#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for nsIThread logging 
    // if necessary...
    //
    if (nsIThreadLog == nsnull) {
        nsIThreadLog = PR_NewLogModule("nsIThread");
    }
#endif /* PR_LOGGING */
}

nsThread::~nsThread()
{
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p destroyed\n", this));
}

void
nsThread::Main(void* arg)
{
    nsThread* self = (nsThread*)arg;

    nsresult rv = NS_OK;
    rv = self->RegisterThreadSelf();
    NS_ASSERTION(rv == NS_OK, "failed to set thread self");

    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p start run %p\n", self, self->mRunnable.get()));
    rv = self->mRunnable->Run();
    NS_ASSERTION(NS_SUCCEEDED(rv), "runnable failed");

#ifdef DEBUG
    PRThreadState state;
    rv = self->GetState(&state);
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p end run %p\n", self, self->mRunnable.get()));
#endif

    // explicitly drop the runnable now in case there are circular references
    // between it and the thread object
    self->mRunnable = nsnull;
}

void
nsThread::Exit(void* arg)
{
    nsThread* self = (nsThread*)arg;
    self->mDead = PR_TRUE;
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p exited\n", self));
    NS_RELEASE(self);
}

NS_METHOD
nsThread::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsThread* thread = new nsThread();
    if (!thread) return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = thread->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) delete thread;
    return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsThread, nsIThread)

NS_IMETHODIMP
nsThread::Join()
{
    // don't check for mDead here because nspr calls Exit (cleaning up
    // thread-local storage) before they let us join with the thread

    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p start join\n", this));
    PRStatus status = PR_JoinThread(mThread);
    // XXX can't use NS_RELEASE here because the macro wants to set
    // this to null (bad c++)
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p end join\n", this));
    if (status == PR_SUCCESS) {
        NS_RELEASE_THIS();   // most likely the final release of this thread 
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

NS_IMETHODIMP
nsThread::Init(nsIRunnable* runnable,
               PRUint32 stackSize,
               PRThreadPriority priority,
               PRThreadScope scope,
               PRThreadState state)
{
    mRunnable = runnable;

    NS_ADDREF_THIS();   // released in nsThread::Exit
    if (state == PR_JOINABLE_THREAD)
        NS_ADDREF_THIS();   // released in nsThread::Join
    mThread = PR_CreateThread(PR_USER_THREAD, Main, this,
                              priority, scope, state, stackSize);
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p created\n", this));
    if (mThread == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_COM nsresult
NS_NewThread(nsIThread* *result, 
             nsIRunnable* runnable,
             PRUint32 stackSize,
             PRThreadState state,
             PRThreadPriority priority,
             PRThreadScope scope)
{
    nsresult rv;
    nsThread* thread = new nsThread();
    if (thread == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(thread);

    rv = thread->Init(runnable, stackSize, priority, scope, state);
    if (NS_FAILED(rv)) {
        NS_RELEASE(thread);
        return rv;
    }

    *result = thread;
    return NS_OK;
}

NS_COM nsresult
NS_NewThread(nsIThread* *result, 
             PRUint32 stackSize,
             PRThreadState state,
             PRThreadPriority priority,
             PRThreadScope scope)
{
    nsThread* thread = new nsThread();
    if (thread == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
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

NS_COM nsresult
nsIThread::GetCurrent(nsIThread* *result)
{
    return GetIThread(PR_CurrentThread(), result);
}

NS_COM nsresult
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
        NS_ADDREF(thread);      // released by Exit
        thread->SetPRThread(prthread);
        nsresult rv = thread->RegisterThreadSelf();
        if (NS_FAILED(rv)) return rv;
    }
    NS_ADDREF(thread);
    *result = thread;
    return NS_OK;
}

NS_COM nsresult
nsIThread::SetMainThread()
{
    // strictly speaking, it could be set twice. but practically speaking,
    // it's almost certainly an error if it is
    if (gMainThread != 0) {
        NS_ERROR("Setting main thread twice?");
        return NS_ERROR_FAILURE;
    }
    return GetCurrent(&gMainThread);
}

NS_COM nsresult
nsIThread::GetMainThread(nsIThread **result)
{
    NS_ASSERTION(result, "bad result pointer");
    if (gMainThread == 0)
        return NS_ERROR_FAILURE;
    *result = gMainThread;
    NS_ADDREF(gMainThread);
    return NS_OK;
}

void 
nsThread::Shutdown()
{
    if (gMainThread) {
        // XXX nspr doesn't seem to be calling the main thread's destructor
        // callback, so let's help it out:
        nsThread::Exit(NS_STATIC_CAST(nsThread*, gMainThread));
        nsrefcnt cnt;
        NS_RELEASE2(gMainThread, cnt);
        NS_WARN_IF_FALSE(cnt == 0, "Main thread being held past XPCOM shutdown.");
    }
}

////////////////////////////////////////////////////////////////////////////////

nsThreadPool::nsThreadPool()
    : mMinThreads(0), mMaxThreads(0), mShuttingDown(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsThreadPool::~nsThreadPool()
{
    if (mThreads) {
        Shutdown();
    }

    if (mLock)
        PR_DestroyLock(mLock);
    if (mThreadExit)
        PR_DestroyCondVar(mThreadExit);
    if (mRequestAdded)
        PR_DestroyCondVar(mRequestAdded);
    if (mRequestsAtZero)
        PR_DestroyCondVar(mRequestsAtZero);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsThreadPool, nsIThreadPool)

NS_IMETHODIMP
nsThreadPool::DispatchRequest(nsIRunnable* runnable)
{
    nsresult rv;
    nsAutoLock lock(mLock);

#if defined(PR_LOGGING)
    nsCOMPtr<nsIThread> th;
    nsIThread::GetCurrent(getter_AddRefs(th));
#endif

    NS_ASSERTION(mMinThreads > 0, "forgot to call Init");
    if (mShuttingDown) {
        rv = NS_ERROR_FAILURE;
    }
    else {
        PRUint32 requestCnt, threadCount;

        rv = mRequests->Count(&requestCnt);
        if (NS_FAILED(rv)) goto exit;

        rv = mThreads->Count(&threadCount);
        if (NS_FAILED(rv)) goto exit;
        
        if ((requestCnt >= threadCount) && (threadCount < mMaxThreads)) {
            PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                   ("nsIThreadPool thread %p: %d threads in pool, max = %d, requests = %d, creating new thread...\n",
                    th.get(), threadCount, mMaxThreads, requestCnt));
            rv = AddThread();
            if (NS_FAILED(rv)) goto exit;
        }

        // XXX for now AppendElement returns a PRBool
        rv = ((PRBool) mRequests->AppendElement(runnable)) ? NS_OK : NS_ERROR_FAILURE;
        if (NS_SUCCEEDED(rv)) {
            if (PR_FAILURE == PR_NotifyCondVar(mRequestAdded))
                goto exit;
        }
    }
     
exit:
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThreadPool thread %p dispatched %p status %x\n", th.get(), runnable, rv));
    return rv;
}

nsresult
nsThreadPool::RemoveThread(nsIThread* currentThread)
{
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThreadPool thread %p being removed\n",
            currentThread));
    
    nsresult rv =  mThreads->DeleteLastElement(currentThread);

    PR_NotifyCondVar(mThreadExit);
    return rv;
}

nsIRunnable*
nsThreadPool::GetRequest(nsIThread* currentThread)
{
    nsresult rv = NS_OK;
    nsIRunnable* request = nsnull;
    nsAutoLock lock(mLock);

    PRUint32 requestCnt;
    while (PR_TRUE) {
        requestCnt = 0;
        rv  = mRequests->Count(&requestCnt);
        
        if (NS_FAILED(rv)) {
            return nsnull;
        }

        if (requestCnt > 0) {
            request = (nsIRunnable*)mRequests->ElementAt(0);
            NS_ASSERTION(request != nsnull, "null runnable");

            PRBool removed = mRequests->RemoveElementAt(0);
            NS_ASSERTION(removed, "nsISupportsArray broken");
            PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                   ("nsIThreadPool thread %p got request %p\n", 
                    currentThread, request));
    
            if (removed && requestCnt == 1)
                PR_NotifyCondVar(mRequestsAtZero); 

            PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                   ("nsIThreadPool thread %p got request %p\n", 
                    currentThread, request));
            return request;
        }
    
        // no requests, and we're not shutting down yet...
        // if we have more than the minimum required threads already then
        // we can just go away
        PRUint32 threadCnt;
        rv = mThreads->Count(&threadCnt);
        if (NS_FAILED(rv)) break;
              
        if (threadCnt > mMinThreads) {
            PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                   ("nsIThreadPool thread %p: %d threads in pool, min = %d, exiting...\n",
                    currentThread, threadCnt, mMinThreads));
            RemoveThread(currentThread);
            return nsnull;   // causes nsThreadPoolRunnable::Run to quit
        }

        PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
               ("nsIThreadPool thread %p waiting (%d threads in pool)\n",
                currentThread, threadCnt));
        
        if (mShuttingDown)
            break;
        
        (void)PR_WaitCondVar(mRequestAdded, PR_INTERVAL_NO_TIMEOUT);
    }
    // no requests, we are going to dump the thread.
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThreadPool thread %p -- no more requests, exiting...\n", 
            currentThread));
    RemoveThread(currentThread);
    return nsnull;
}

NS_METHOD
nsThreadPool::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsThreadPool* pool = new nsThreadPool();
    if (!pool) return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = pool->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) delete pool;
    return rv;
}

NS_IMETHODIMP
nsThreadPool::ProcessPendingRequests()
{
    nsresult rv;
    while (PR_TRUE) {
        PRUint32 cnt;
        rv = mRequests->Count(&cnt);
        if (NS_FAILED(rv) || cnt == 0)
            break;
        (void)PR_WaitCondVar(mRequestsAtZero, PR_INTERVAL_NO_TIMEOUT);
    }
#ifdef DEBUG
    PRUint32 requestCount;
    (void)mRequests->Count(&requestCount);
    NS_ASSERTION(requestCount == 0, "not all requests processed");
#endif
    return rv;
}

PRBool 
nsThreadPool::InterruptThreads(nsISupports* aElement, void *aData)
{
    nsCOMPtr<nsIThread> thread = do_QueryInterface(aElement);
    NS_ASSERTION(thread, "bad thread in array");
    (void) thread->Interrupt();
    return PR_TRUE;
}

NS_IMETHODIMP
nsThreadPool::Shutdown()
{
    nsresult rv = NS_OK;
    PRUint32 count = 0;

#if defined(PR_LOGGING)
    nsCOMPtr<nsIThread> th;
    nsIThread::GetCurrent(getter_AddRefs(th));
#endif
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThreadPool thread %p shutting down\n", th.get()));
    
    nsAutoLock lock(mLock);
    mShuttingDown = PR_TRUE;
    rv = ProcessPendingRequests();
    NS_ASSERTION(NS_SUCCEEDED(rv), "ProcessPendingRequests failed");
    // keep trying... don't bail with an error here

// fix Add assert that there are no more requests to be handled.

    // then interrupt the threads
    rv = mThreads->EnumerateForwards(nsThreadPool::InterruptThreads, nsnull);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Interruption failed");
    if (NS_FAILED(rv)) return rv;
    
    while (PR_TRUE) {
      rv = mThreads->Count(&count);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
      if (NS_FAILED(rv)) return rv;
      if (count == 0 )
          break;
      PR_WaitCondVar(mThreadExit, PR_INTERVAL_NO_TIMEOUT);
   }
    
    mThreads = nsnull;
    return rv;
}

NS_IMETHODIMP
nsThreadPool::Init(PRUint32 minThreadCount,
                   PRUint32 maxThreadCount,
                   PRUint32 stackSize,
                   PRThreadPriority priority,
                   PRThreadScope scope)
{
    nsresult rv;
    
    mStackSize = stackSize;
    mPriority = priority;
    mScope = scope;
    NS_ASSERTION(minThreadCount > 0 && minThreadCount <= maxThreadCount, "bad min/max values");
    mMinThreads = minThreadCount;
    mMaxThreads = maxThreadCount;

    rv = NS_NewISupportsArray(getter_AddRefs(mThreads));
    if (NS_FAILED(rv)) return rv;
    
    rv = NS_NewISupportsArray(getter_AddRefs(mRequests));
    if (NS_FAILED(rv)) return rv;

    mLock = PR_NewLock();
    if (mLock == nsnull)
        goto cleanup;
    
    mRequestAdded = PR_NewCondVar(mLock);    
    if (mRequestAdded == nsnull)
        goto cleanup;

    mThreadExit = PR_NewCondVar(mLock);
    if (mThreadExit == nsnull)
        goto cleanup;
    
    mRequestsAtZero = PR_NewCondVar(mLock);
    if (mRequestsAtZero == nsnull)
        goto cleanup;
    
    return NS_OK;

 cleanup:
    if (mLock)
        PR_DestroyLock(mLock);
    if (mThreadExit)
        PR_DestroyCondVar(mThreadExit);
    if (mRequestAdded)
        PR_DestroyCondVar(mRequestAdded);
    if (mRequestsAtZero)
        PR_DestroyCondVar(mRequestsAtZero);
        
    return NS_ERROR_OUT_OF_MEMORY;
}


nsresult
nsThreadPool::AddThread()
{
    nsresult rv;

#ifdef DEBUG
    PRUint32 cnt;
    rv = mThreads->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    if (cnt >= mMaxThreads)
        return NS_ERROR_FAILURE;
#endif

    nsThreadPoolRunnable* runnable = new nsThreadPoolRunnable(this);
    if (runnable == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(runnable);

    nsCOMPtr<nsIThread> thread;

    rv = NS_NewThread(getter_AddRefs(thread), 
                      runnable, 
                      mStackSize, 
                      PR_UNJOINABLE_THREAD, 
                      mPriority, 
                      mScope);

    // Let the thread own the runnable.
    NS_RELEASE(runnable);
    if (NS_FAILED(rv)) return rv;
    
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThreadPool adding new thread %p (%d total)\n",
            thread.get(), cnt + 1));

    rv = mThreads->AppendElement(thread) ? NS_OK : NS_ERROR_FAILURE;
    return rv;
}

NS_COM nsresult
NS_NewThreadPool(nsIThreadPool* *result,
                 PRUint32 minThreads, PRUint32 maxThreads,
                 PRUint32 stackSize,
                 PRThreadPriority priority,
                 PRThreadScope scope)
{
    nsresult rv;
    nsThreadPool* pool = new nsThreadPool();
    if (pool == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(pool);

    rv = pool->Init(minThreads, maxThreads, stackSize, priority, scope);
    if (NS_FAILED(rv)) {
        NS_RELEASE(pool);
        return rv;
    }
    
    *result = pool;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsThreadPoolRunnable::nsThreadPoolRunnable(nsThreadPool* pool)
    : mPool(pool)
{
    NS_INIT_REFCNT();
}

nsThreadPoolRunnable::~nsThreadPoolRunnable()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsThreadPoolRunnable, nsIRunnable)

NS_IMETHODIMP
nsThreadPoolRunnable::Run()
{
    nsresult rv = NS_OK;
    nsIRunnable* request;

    nsCOMPtr<nsIThread> currentThread;
    nsIThread::GetCurrent(getter_AddRefs(currentThread));

    while ((request = mPool->GetRequest(currentThread)) != nsnull) {
        PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
               ("nsIThreadPool thread %p running %p\n", 
                currentThread.get(), request));
        rv = request->Run();
        NS_ASSERTION(NS_SUCCEEDED(rv), "runnable failed");

        PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
               ("nsIThreadPool thread %p completed %p status=%x\n",
                currentThread.get(), request, rv));
        NS_RELEASE(request);
    }
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThreadPool thread %p quitting %p\n",
            currentThread.get(), this));
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
