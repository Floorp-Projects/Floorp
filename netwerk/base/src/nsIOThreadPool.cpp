/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#define MAX_THREADS 4
#define THREAD_IDLE_TIMEOUT PR_SecondsToInterval(10)

#define PLEVENT_FROM_LINK(_link) \
    ((PLEvent*) ((char*) (_link) - offsetof(PLEvent, link)))

//-----------------------------------------------------------------------------
// pool of joinable threads used for general purpose i/o tasks
//
// the main entry point to this class is nsIEventTarget.  events posted to
// the thread pool are dispatched on one of the threads.  a variable number
// of threads are maintained.  the threads die off if they remain idle for
// more than THREAD_IDLE_TIMEOUT.  the thread pool shuts down when it receives
// the "xpcom-shutdown" event.
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

    int GetCurrentThreadIndex();

    PR_STATIC_CALLBACK(void) ThreadFunc(void *);

    // mLock protects all (exceptions during Init and Shutdown)
    PRLock    *mLock;
    PRCondVar *mCV;
    PRThread  *mThreads[MAX_THREADS];
    PRUint32   mNumIdleThreads;
    PRCList    mEventQ;
    PRBool     mShutdown;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsIOThreadPool, nsIEventTarget, nsIObserver)

nsresult
nsIOThreadPool::Init()
{
#if defined(PR_LOGGING)
    if (!gIOThreadPoolLog)
        gIOThreadPoolLog = PR_NewLogModule("nsIOThreadPool");
#endif

    mNumIdleThreads = 0;
    mShutdown = PR_FALSE;

    mLock = PR_NewLock();
    if (!mLock)
        return NS_ERROR_OUT_OF_MEMORY;

    mCV = PR_NewCondVar(mLock);
    if (!mCV)
        return NS_ERROR_OUT_OF_MEMORY;

    memset(mThreads, 0, sizeof(mThreads));
    PR_INIT_CLIST(&mEventQ);

    // we want to shutdown the i/o thread pool at xpcom-shutdown time...
    nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
    if (os)
        os->AddObserver(this, "xpcom-shutdown", PR_FALSE);
    return NS_OK;
}

nsIOThreadPool::~nsIOThreadPool()
{
    LOG(("Destroying nsIOThreadPool @%p\n", this));

#ifdef DEBUG
    NS_ASSERTION(PR_CLIST_IS_EMPTY(&mEventQ), "leaking events");
    for (int i=0; i<MAX_THREADS; ++i)
        NS_ASSERTION(mThreads[i] == nsnull, "leaking thread");
#endif

    if (mCV)
        PR_DestroyCondVar(mCV);
    if (mLock)
        PR_DestroyLock(mLock);
}

void
nsIOThreadPool::Shutdown()
{
    LOG(("nsIOThreadPool::Shutdown\n"));

    // synchronize with background threads...
    {
        nsAutoLock lock(mLock);
        mShutdown = PR_TRUE;
        PR_NotifyAllCondVar(mCV);
    }

    for (int i=0; i<MAX_THREADS; ++i) {
        if (mThreads[i]) {
            PR_JoinThread(mThreads[i]);
            mThreads[i] = 0;
        }         
    }
}

int
nsIOThreadPool::GetCurrentThreadIndex()
{
    PRThread *current = PR_GetCurrentThread();

    for (int i=0; i<MAX_THREADS; ++i)
        if (current == mThreads[i])
            return i;

    return -1;
}

NS_IMETHODIMP
nsIOThreadPool::PostEvent(PLEvent *event)
{
    LOG(("nsIOThreadPool::PostEvent [event=%p]\n", event));

    nsAutoLock lock(mLock);

    if (mShutdown)
        return NS_ERROR_UNEXPECTED;
    
    nsresult rv = NS_OK;

    PR_APPEND_LINK(&event->link, &mEventQ);

    // now, look for an available thread...
    if (mNumIdleThreads)
        PR_NotifyCondVar(mCV); // wake up an idle thread
    else {
        // try to create a new thread unless we have reached our maximum...
        for (int i=0; i<MAX_THREADS; ++i) {
            if (!mThreads[i]) {
                NS_ADDREF_THIS(); // the thread owns a reference to us
                mThreads[i] = PR_CreateThread(PR_USER_THREAD,
                                              ThreadFunc,
                                              this,
                                              PR_PRIORITY_NORMAL,
                                              PR_GLOBAL_THREAD,
                                              PR_JOINABLE_THREAD,
                                              0);
                if (!mThreads[i])
                    rv = NS_ERROR_OUT_OF_MEMORY;
                break;
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
nsIOThreadPool::IsOnCurrentThread(PRBool *result)
{
    // fudging this a bit since we actually cover several threads...
    *result = (GetCurrentThreadIndex() != -1);
    return NS_OK;
}

NS_IMETHODIMP
nsIOThreadPool::Observe(nsISupports *, const char *topic, const PRUnichar *)
{
    NS_ASSERTION(strcmp(topic, "xpcom-shutdown") == 0, "unexpected topic");
    Shutdown();
    return NS_OK;
}

void
nsIOThreadPool::ThreadFunc(void *arg)
{
    nsIOThreadPool *pool = (nsIOThreadPool *) arg;

    LOG(("entering ThreadFunc\n"));

    // XXX not using a nsAutoLock here because we need to temporarily unlock
    // and relock in the inner loop.  nsAutoLock has a bug, causing it to warn
    // of a bogus deadlock, which makes us avoid it here.  (see bug 221331)
    PR_Lock(pool->mLock);

    for (;;) {
        // never wait if we are shutting down; always process queued events...
        if (PR_CLIST_IS_EMPTY(&pool->mEventQ) && !pool->mShutdown) {
            pool->mNumIdleThreads++;
            PR_WaitCondVar(pool->mCV, THREAD_IDLE_TIMEOUT);
            pool->mNumIdleThreads--;
        }

        // if the queue is still empty, then kill this thread...
        if (PR_CLIST_IS_EMPTY(&pool->mEventQ))
            break;

        // handle one event at a time: we don't want this one thread to hog
        // all the events while other threads may be able to help out ;-)
        do {
            PLEvent *event = PLEVENT_FROM_LINK(PR_LIST_HEAD(&pool->mEventQ));
            PR_REMOVE_AND_INIT_LINK(&event->link);

            LOG(("event:%p\n", event));

            // release lock!
            PR_Unlock(pool->mLock);
            PL_HandleEvent(event);
            PR_Lock(pool->mLock);
        }
        while (!PR_CLIST_IS_EMPTY(&pool->mEventQ));
    }

    // thread is dying... cleanup mThreads, unless of course if we are
    // shutting down, in which case Shutdown will clean up mThreads for us.
    if (!pool->mShutdown)
        pool->mThreads[pool->GetCurrentThreadIndex()] = nsnull;

    PR_Unlock(pool->mLock);

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
