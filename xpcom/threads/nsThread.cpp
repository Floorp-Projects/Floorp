/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    : mThread(nsnull), mDead(PR_FALSE), mStartLock(nsnull)
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

    // enforce matching of constants to enums in prthread.h
    NS_ASSERTION(int(nsIThread::PRIORITY_LOW)     == int(PR_PRIORITY_LOW) &&
                 int(nsIThread::PRIORITY_NORMAL)  == int(PRIORITY_NORMAL) &&
                 int(nsIThread::PRIORITY_HIGH)    == int(PRIORITY_HIGH) &&
                 int(nsIThread::PRIORITY_URGENT)  == int(PRIORITY_URGENT) &&
                 int(nsIThread::SCOPE_LOCAL)      == int(PR_LOCAL_THREAD) &&
                 int(nsIThread::SCOPE_GLOBAL)     == int(PR_GLOBAL_THREAD) &&
                 int(nsIThread::STATE_JOINABLE)   == int(PR_JOINABLE_THREAD) &&
                 int(nsIThread::STATE_UNJOINABLE) == int(PR_UNJOINABLE_THREAD),
                 "Bad constant in nsIThread!");
}

nsThread::~nsThread()
{
    if (mStartLock)
        PR_DestroyLock(mStartLock);

    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p destroyed\n", this));
#if defined(PR_LOGGING)
    if (this == gMainThread) {
        PR_DELETE( nsIThreadLog );  // changed from delete, since it is malloc'ed
    }
#endif
}

void
nsThread::Main(void* arg)
{
    nsThread* self = (nsThread*)arg;

    self->WaitUntilReadyToStartMain();

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

    if (self->mDead) {
        NS_ERROR("attempt to Exit() thread twice");
        return;
    }

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
    mStartLock = PR_NewLock();
    if (mStartLock == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    PR_Lock(mStartLock);
    mThread = PR_CreateThread(PR_USER_THREAD, Main, this,
                              priority, scope, state, stackSize);
    PR_Unlock(mStartLock);
    PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
           ("nsIThread %p created\n", this));
    if (mThread == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

/* readonly attribute nsIThread currentThread; */
NS_IMETHODIMP 
nsThread::GetCurrentThread(nsIThread * *aCurrentThread)
{
    return GetIThread(PR_CurrentThread(), aCurrentThread);
}

/* void sleep (in PRUint32 msec); */
NS_IMETHODIMP 
nsThread::Sleep(PRUint32 msec)
{
    if (PR_CurrentThread() != mThread)
        return NS_ERROR_FAILURE;
    
    if (PR_Sleep(PR_MillisecondsToInterval(msec)) != PR_SUCCESS)
        return NS_ERROR_FAILURE;

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

void 
nsThread::WaitUntilReadyToStartMain()
{
    PR_Lock(mStartLock);
    PR_Unlock(mStartLock);
    PR_DestroyLock(mStartLock);
    mStartLock = nsnull;
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

/* nsThreadPoolBusyBody implements an increment/decrement of
   nsThreadPool's mBusyThreads member variable
*/
class nsThreadPoolBusyBody {
public:
  nsThreadPoolBusyBody(nsThreadPool *aPool);
  ~nsThreadPoolBusyBody();

private:
  void* operator new(size_t) { return 0; } // local variable, only!
  nsThreadPool *mPool;
};

inline nsThreadPoolBusyBody::nsThreadPoolBusyBody(nsThreadPool *aPool) {

  nsAutoLock lock(aPool->mLock);
#ifdef DEBUG
  PRUint32 threadCount;
  if (NS_SUCCEEDED(aPool->mThreads->Count(&threadCount)))
    NS_ASSERTION(aPool->mBusyThreads < threadCount, "thread busy count exceeded thread count");
#endif
  aPool->mBusyThreads++;
  mPool = aPool;
}

inline nsThreadPoolBusyBody::~nsThreadPoolBusyBody() {

  nsAutoLock lock(mPool->mLock);
  NS_ASSERTION(mPool->mBusyThreads > 0, "thread busy count < 0");
  mPool->mBusyThreads--;
}

////////////////////////////////////////////////////////////////////////////////

nsThreadPool::nsThreadPool()
    : mMinThreads(0), mMaxThreads(0), mBusyThreads(0),
      mShuttingDown(PR_FALSE)
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
    if (mPendingRequestAdded)
        PR_DestroyCondVar(mPendingRequestAdded);
    if (mPendingRequestsAtZero)
        PR_DestroyCondVar(mPendingRequestsAtZero);
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

        rv = mPendingRequests->Count(&requestCnt);
        if (NS_FAILED(rv)) goto exit;

        rv = mThreads->Count(&threadCount);
        if (NS_FAILED(rv)) goto exit;
        
        if (requestCnt >= threadCount-mBusyThreads && threadCount < mMaxThreads) {
            PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                   ("nsIThreadPool thread %p: %d threads in pool, max = %d, requests = %d, creating new thread...\n",
                    th.get(), threadCount, mMaxThreads, requestCnt));
            rv = AddThread();
            if (NS_FAILED(rv)) goto exit;
        }

        // XXX for now AppendElement returns a PRBool
        rv = ((PRBool) mPendingRequests->AppendElement(runnable)) ? NS_OK : NS_ERROR_FAILURE;
        if (NS_SUCCEEDED(rv)) {
            if (PR_FAILURE == PR_NotifyCondVar(mPendingRequestAdded))
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

void
nsThreadPool::RequestDone(nsIRunnable* request)
{
    nsAutoLock lock(mLock);
    mRunningRequests->RemoveElement(request);
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
        rv  = mPendingRequests->Count(&requestCnt);
        
        if (NS_FAILED(rv)) {
            return nsnull;
        }
        
        if (requestCnt > 0) {
            PRInt32 pendingThread = 0;
            PRInt32 runningPos;
            while (PR_TRUE) {
                request = (nsIRunnable*)mPendingRequests->ElementAt(pendingThread);

                // if we are breaking here, it means that either we have a bad 
                // request in our list, or all pending requests are being run on
                // another worker thread.  
                if (request == nsnull) {
                    pendingThread = -1;
                    break;  
                }

                // check to see if the request is not running.                
                mRunningRequests->GetIndexOf(request, &runningPos);
                if (runningPos == -1)
                    break;

                pendingThread++;
            }
            
            if (pendingThread != -1) {
                PRBool removed = mPendingRequests->RemoveElementAt(pendingThread);
                NS_ASSERTION(removed, "nsISupportsArray broken");
                PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                       ("nsIThreadPool thread %p got request %p\n", 
                        currentThread, request));
                
                if (removed && requestCnt == 1)
                    PR_NotifyCondVar(mPendingRequestsAtZero); 
                
                PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                       ("nsIThreadPool thread %p got request %p\n", 
                        currentThread, request));
                
                mRunningRequests->AppendElement(request);
                return request;
            }
        }
    
        if (mShuttingDown)
            break;
        
        // no requests, and we're not shutting down yet...
        // if we have more than the minimum required threads already then
        // then we may be able to go away.
        PRUint32 threadCnt;
        rv = mThreads->Count(&threadCnt);
        if (NS_FAILED(rv)) break;
              
        if (threadCnt > mMinThreads) {
            // to avoid multiple thread spawns/exits, we need to
            // wait for some period of time while waiting for any
            // additional requests.  If this this wait yeilds no
            // request, then we can exit.
            //
            // TODO: determine what the optimal timeout value is.  
            // For now, just use 5 seconds.

            PRIntervalTime interval = PR_SecondsToInterval(5);
            PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                  ("nsIThreadPool thread %p waiting for %d seconds before exiting (%d threads in pool)\n",
                    currentThread, interval, threadCnt));

            (void) PR_WaitCondVar( mPendingRequestAdded, interval);  
            
            rv = mPendingRequests->Count(&requestCnt);
            if (NS_FAILED(rv) || requestCnt == 0) {
                PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                   ("nsIThreadPool thread %p: %d threads in pool, min = %d, exiting...\n",
                    currentThread, threadCnt, mMinThreads));
                RemoveThread(currentThread);
                return nsnull;   // causes nsThreadPoolRunnable::Run to quit
            }
        }
        else
        {
            PR_LOG(nsIThreadLog, PR_LOG_DEBUG,
                   ("nsIThreadPool thread %p waiting (%d threads in pool)\n",
                    currentThread, threadCnt));
            
            (void)PR_WaitCondVar(mPendingRequestAdded, PR_INTERVAL_NO_TIMEOUT);
        }
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
        rv = mPendingRequests->Count(&cnt);
        if (NS_FAILED(rv) || cnt == 0)
            break;
        (void)PR_WaitCondVar(mPendingRequestsAtZero, PR_INTERVAL_NO_TIMEOUT);
    }
#ifdef DEBUG
    PRUint32 requestCount;
    (void)mPendingRequests->Count(&requestCount);
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
    
    rv = NS_NewISupportsArray(getter_AddRefs(mPendingRequests));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mRunningRequests));
    if (NS_FAILED(rv)) return rv;

    mLock = PR_NewLock();
    if (mLock == nsnull)
        goto cleanup;
    
    mPendingRequestAdded = PR_NewCondVar(mLock);    
    if (mPendingRequestAdded == nsnull)
        goto cleanup;

    mThreadExit = PR_NewCondVar(mLock);
    if (mThreadExit == nsnull)
        goto cleanup;
    
    mPendingRequestsAtZero = PR_NewCondVar(mLock);
    if (mPendingRequestsAtZero == nsnull)
        goto cleanup;
    
    return NS_OK;

 cleanup:
    if (mLock)
        PR_DestroyLock(mLock);
    if (mThreadExit)
        PR_DestroyCondVar(mThreadExit);
    if (mPendingRequestAdded)
        PR_DestroyCondVar(mPendingRequestAdded);
    if (mPendingRequestsAtZero)
        PR_DestroyCondVar(mPendingRequestsAtZero);
        
    return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsThreadPool::AddThread()
{
    nsresult rv;

#if defined(DEBUG) || defined(FORCE_PR_LOG)
    PRUint32 cnt;
    rv = mThreads->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
#endif
#ifdef DEBUG
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
        nsThreadPoolBusyBody bumpBusyCount(mPool);
        rv = request->Run();
        NS_ASSERTION(NS_SUCCEEDED(rv), "runnable failed");

        // let the pool know that the request has finished running.
        mPool->RequestDone(request);

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
