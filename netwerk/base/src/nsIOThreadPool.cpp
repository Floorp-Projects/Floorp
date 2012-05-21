/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIEventTarget.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "prclist.h"
#include "prlog.h"

#if defined(PR_LOGGING)
//
// NSPR_LOG_MODULES=nsIOThreadPool:5
//
static PRLogModuleInfo *gIOThreadPoolLog = nsnull;
#endif
#define LOG(args) PR_LOG(gIOThreadPoolLog, PR_LOG_DEBUG, args)

// this number specifies the maximum number of threads.
#define MAX_THREADS 4

// this number specifies how long to wait before killing an idle thread.  it's
// important to pick a large enough value here to minimize thread churn.
#define IDLE_TIMEOUT PR_SecondsToInterval(60)

#define PLEVENT_FROM_LINK(_link) \
    ((PLEvent*) ((char*) (_link) - offsetof(PLEvent, link)))

//-----------------------------------------------------------------------------
// pool of joinable threads used for general purpose i/o tasks
//
// the main entry point to this class is nsIEventTarget.  events posted to
// the thread pool are dispatched on one of the threads.  a variable number
// of threads are maintained.  the threads die off if they remain idle for
// more than THREAD_IDLE_TIMEOUT.  the thread pool shuts down when it receives
// the "xpcom-shutdown-threads" event.
//-----------------------------------------------------------------------------

class nsIOThreadPool : public nsIEventTarget
                     , public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIEVENTTARGET
    NS_DECL_NSIOBSERVER

    nsresult Init();
    void     Shutdown();

private:
    virtual ~nsIOThreadPool();

    static void ThreadFunc(void *);

    // mLock protects all (exceptions during Init and Shutdown)
    PRLock    *mLock;
    PRCondVar *mIdleThreadCV;   // notified to wake up an idle thread
    PRCondVar *mExitThreadCV;   // notified when a thread exits
    PRUint32   mNumThreads;     // number of active + idle threads
    PRUint32   mNumIdleThreads; // number of idle threads
    PRCList    mEventQ;         // queue of PLEvent structs
    bool       mShutdown;       // set to true if shutting down
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsIOThreadPool, nsIEventTarget, nsIObserver)

nsresult
nsIOThreadPool::Init()
{
#if defined(PR_LOGGING)
    if (!gIOThreadPoolLog)
        gIOThreadPoolLog = PR_NewLogModule("nsIOThreadPool");
#endif

    mNumThreads = 0;
    mNumIdleThreads = 0;
    mShutdown = false;

    mLock = nsAutoLock::NewLock("nsIOThreadPool::mLock");
    if (!mLock)
        return NS_ERROR_OUT_OF_MEMORY;

    mIdleThreadCV = PR_NewCondVar(mLock);
    if (!mIdleThreadCV)
        return NS_ERROR_OUT_OF_MEMORY;

    mExitThreadCV = PR_NewCondVar(mLock);
    if (!mExitThreadCV)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_INIT_CLIST(&mEventQ);

    // We want to shutdown the i/o thread pool at xpcom-shutdown-threads time.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os)
        os->AddObserver(this, "xpcom-shutdown-threads", false);
    return NS_OK;
}

nsIOThreadPool::~nsIOThreadPool()
{
    LOG(("Destroying nsIOThreadPool @%p\n", this));

#ifdef DEBUG
    NS_ASSERTION(PR_CLIST_IS_EMPTY(&mEventQ), "leaking events");
    NS_ASSERTION(mNumThreads == 0, "leaking thread(s)");
#endif

    if (mIdleThreadCV)
        PR_DestroyCondVar(mIdleThreadCV);
    if (mExitThreadCV)
        PR_DestroyCondVar(mExitThreadCV);
    if (mLock)
        nsAutoLock::DestroyLock(mLock);
}

void
nsIOThreadPool::Shutdown()
{
    LOG(("nsIOThreadPool::Shutdown\n"));

    // synchronize with background threads...
    {
        nsAutoLock lock(mLock);
        mShutdown = true;

        PR_NotifyAllCondVar(mIdleThreadCV);

        while (mNumThreads != 0)
            PR_WaitCondVar(mExitThreadCV, PR_INTERVAL_NO_TIMEOUT);
    }
}

NS_IMETHODIMP
nsIOThreadPool::PostEvent(PLEvent *event)
{
    LOG(("nsIOThreadPool::PostEvent [event=%p]\n", event));

    nsAutoLock lock(mLock);

    // if we are shutting down, then prevent additional events from being
    // added to the queue...
    if (mShutdown)
        return NS_ERROR_UNEXPECTED;
    
    nsresult rv = NS_OK;

    PR_APPEND_LINK(&event->link, &mEventQ);

    // now, look for an available idle thread...
    if (mNumIdleThreads)
        PR_NotifyCondVar(mIdleThreadCV); // wake up an idle thread

    // or, try to create a new thread unless we have reached our maximum...
    else if (mNumThreads < MAX_THREADS) {
        NS_ADDREF_THIS(); // the thread owns a reference to us
        mNumThreads++;
        PRThread *thread = PR_CreateThread(PR_USER_THREAD,
                                           ThreadFunc,
                                           this,
                                           PR_PRIORITY_NORMAL,
                                           PR_GLOBAL_THREAD,
                                           PR_UNJOINABLE_THREAD,
                                           0);
        if (!thread) {
            NS_RELEASE_THIS();
            mNumThreads--;
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    }
    // else, we expect one of the active threads to process the event queue.

    return rv;
}

NS_IMETHODIMP
nsIOThreadPool::IsOnCurrentThread(bool *result)
{
    // no one should be calling this method.  if this assertion gets hit,
    // then we need to think carefully about what this method should be
    // returning.
    NS_NOTREACHED("nsIOThreadPool::IsOnCurrentThread");

    // fudging this a bit since we actually cover several threads...
    *result = false;
    return NS_OK;
}

NS_IMETHODIMP
nsIOThreadPool::Observe(nsISupports *, const char *topic, const PRUnichar *)
{
    NS_ASSERTION(strcmp(topic, "xpcom-shutdown-threads") == 0, "unexpected topic");
    Shutdown();
    return NS_OK;
}

void
nsIOThreadPool::ThreadFunc(void *arg)
{
    nsIOThreadPool *pool = (nsIOThreadPool *) arg;

    LOG(("entering ThreadFunc\n"));

    {
        nsAutoLock lock(pool->mLock);

        for (;;) {
            PRIntervalTime start = PR_IntervalNow(), timeout = IDLE_TIMEOUT;
            //
            // wait for one or more of the following to occur:
            //  (1) the event queue has an event to process
            //  (2) the shutdown flag has been set
            //  (3) the thread has been idle for too long
            //
            // PR_WaitCondVar will return when any of these conditions is true.
            //
            while (PR_CLIST_IS_EMPTY(&pool->mEventQ) && !pool->mShutdown) {
                pool->mNumIdleThreads++;
                PR_WaitCondVar(pool->mIdleThreadCV, timeout);
                pool->mNumIdleThreads--;

                PRIntervalTime delta = PR_IntervalNow() - start;
                if (delta >= timeout)
                    break;
                timeout -= delta;
                start += delta;
            }

            // if the queue is still empty, then kill this thread (either we
            // are shutting down or the thread exceeded the idle timeout)...
            if (PR_CLIST_IS_EMPTY(&pool->mEventQ))
                break;

            // handle one event at a time: we don't want this one thread to hog
            // all the events while other threads may be able to help out ;-)
            do {
                PLEvent *event = PLEVENT_FROM_LINK(PR_LIST_HEAD(&pool->mEventQ));
                PR_REMOVE_AND_INIT_LINK(&event->link);

                LOG(("event:%p\n", event));

                // release lock!
                lock.unlock();
                PL_HandleEvent(event);
                lock.lock();
            }
            while (!PR_CLIST_IS_EMPTY(&pool->mEventQ));
        }

        // thread is going away...
        pool->mNumThreads--;
        PR_NotifyCondVar(pool->mExitThreadCV);
    }

    // release our reference to the pool
    NS_RELEASE(pool);

    LOG(("leaving ThreadFunc\n"));
}

//-----------------------------------------------------------------------------

NS_METHOD
net_NewIOThreadPool(nsISupports *outer, REFNSIID iid, void **result)
{
    nsIOThreadPool *pool = new nsIOThreadPool();
    if (!pool)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(pool);
    nsresult rv = pool->Init();
    if (NS_SUCCEEDED(rv))
        rv = pool->QueryInterface(iid, result);
    NS_RELEASE(pool);
    return rv;
}
