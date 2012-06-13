// vim:set sw=4 sts=4 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nsSocketTransportService2.h"
#include "nsSocketTransport2.h"
#include "nsReadableUtils.h"
#include "nsNetError.h"
#include "prnetdb.h"
#include "prerror.h"
#include "plstr.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "nsIOService.h"

#include "mozilla/FunctionTimer.h"

// XXX: There is no good header file to put these in. :(
namespace mozilla { namespace psm {

void InitializeSSLServerCertVerificationThreads();
void StopSSLServerCertVerificationThreads();

} } // namespace mozilla::psm

using namespace mozilla;

#if defined(PR_LOGGING)
PRLogModuleInfo *gSocketTransportLog = nsnull;
#endif

nsSocketTransportService *gSocketTransportService = nsnull;
PRThread                 *gSocketThread           = nsnull;

#define SEND_BUFFER_PREF "network.tcp.sendbuffer"
#define SOCKET_LIMIT_TARGET 550U
#define SOCKET_LIMIT_MIN     50U

PRUint32 nsSocketTransportService::gMaxCount;
PRCallOnceType nsSocketTransportService::gMaxCountInitOnce;

//-----------------------------------------------------------------------------
// ctor/dtor (called on the main/UI thread by the service manager)

nsSocketTransportService::nsSocketTransportService()
    : mThread(nsnull)
    , mThreadEvent(nsnull)
    , mAutodialEnabled(false)
    , mLock("nsSocketTransportService::mLock")
    , mInitialized(false)
    , mShuttingDown(false)
    , mActiveListSize(SOCKET_LIMIT_MIN)
    , mIdleListSize(SOCKET_LIMIT_MIN)
    , mActiveCount(0)
    , mIdleCount(0)
    , mSendBufferSize(0)
    , mProbedMaxCount(false)
{
#if defined(PR_LOGGING)
    gSocketTransportLog = PR_NewLogModule("nsSocketTransport");
#endif

    NS_ASSERTION(NS_IsMainThread(), "wrong thread");

    PR_CallOnce(&gMaxCountInitOnce, DiscoverMaxCount);
    mActiveList = (SocketContext *)
        moz_xmalloc(sizeof(SocketContext) * mActiveListSize);
    mIdleList = (SocketContext *)
        moz_xmalloc(sizeof(SocketContext) * mIdleListSize);
    mPollList = (PRPollDesc *)
        moz_xmalloc(sizeof(PRPollDesc) * (mActiveListSize + 1));

    NS_ASSERTION(!gSocketTransportService, "must not instantiate twice");
    gSocketTransportService = this;
}

nsSocketTransportService::~nsSocketTransportService()
{
    NS_ASSERTION(NS_IsMainThread(), "wrong thread");
    NS_ASSERTION(!mInitialized, "not shutdown properly");
    
    if (mThreadEvent)
        PR_DestroyPollableEvent(mThreadEvent);

    moz_free(mActiveList);
    moz_free(mIdleList);
    moz_free(mPollList);
    gSocketTransportService = nsnull;
}

//-----------------------------------------------------------------------------
// event queue (any thread)

already_AddRefed<nsIThread>
nsSocketTransportService::GetThreadSafely()
{
    MutexAutoLock lock(mLock);
    nsIThread* result = mThread;
    NS_IF_ADDREF(result);
    return result;
}

NS_IMETHODIMP
nsSocketTransportService::Dispatch(nsIRunnable *event, PRUint32 flags)
{
    SOCKET_LOG(("STS dispatch [%p]\n", event));

    nsCOMPtr<nsIThread> thread = GetThreadSafely();
    NS_ENSURE_TRUE(thread, NS_ERROR_NOT_INITIALIZED);
    nsresult rv = thread->Dispatch(event, flags);
    if (rv == NS_ERROR_UNEXPECTED) {
        // Thread is no longer accepting events. We must have just shut it
        // down on the main thread. Pretend we never saw it.
        rv = NS_ERROR_NOT_INITIALIZED;
    }
    return rv;
}

NS_IMETHODIMP
nsSocketTransportService::IsOnCurrentThread(bool *result)
{
    nsCOMPtr<nsIThread> thread = GetThreadSafely();
    NS_ENSURE_TRUE(thread, NS_ERROR_NOT_INITIALIZED);
    return thread->IsOnCurrentThread(result);
}

//-----------------------------------------------------------------------------
// socket api (socket thread only)

NS_IMETHODIMP
nsSocketTransportService::NotifyWhenCanAttachSocket(nsIRunnable *event)
{
    SOCKET_LOG(("nsSocketTransportService::NotifyWhenCanAttachSocket\n"));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (CanAttachSocket()) {
        return Dispatch(event, NS_DISPATCH_NORMAL);
    }

    mPendingSocketQ.PutEvent(event);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::AttachSocket(PRFileDesc *fd, nsASocketHandler *handler)
{
    SOCKET_LOG(("nsSocketTransportService::AttachSocket [handler=%x]\n", handler));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (!CanAttachSocket()) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    SocketContext sock;
    sock.mFD = fd;
    sock.mHandler = handler;
    sock.mElapsedTime = 0;

    nsresult rv = AddToIdleList(&sock);
    if (NS_SUCCEEDED(rv))
        NS_ADDREF(handler);
    return rv;
}

nsresult
nsSocketTransportService::DetachSocket(SocketContext *listHead, SocketContext *sock)
{
    SOCKET_LOG(("nsSocketTransportService::DetachSocket [handler=%x]\n", sock->mHandler));
    NS_ABORT_IF_FALSE((listHead == mActiveList) || (listHead == mIdleList),
                      "DetachSocket invalid head");

    // inform the handler that this socket is going away
    sock->mHandler->OnSocketDetached(sock->mFD);

    // cleanup
    sock->mFD = nsnull;
    NS_RELEASE(sock->mHandler);

    if (listHead == mActiveList)
        RemoveFromPollList(sock);
    else
        RemoveFromIdleList(sock);

    // NOTE: sock is now an invalid pointer
    
    //
    // notify the first element on the pending socket queue...
    //
    nsCOMPtr<nsIRunnable> event;
    if (mPendingSocketQ.GetPendingEvent(getter_AddRefs(event))) {
        // move event from pending queue to dispatch queue
        return Dispatch(event, NS_DISPATCH_NORMAL);
    }
    return NS_OK;
}

nsresult
nsSocketTransportService::AddToPollList(SocketContext *sock)
{
    NS_ABORT_IF_FALSE(!(((PRUint32)(sock - mActiveList)) < mActiveListSize),
                      "AddToPollList Socket Already Active");

    SOCKET_LOG(("nsSocketTransportService::AddToPollList [handler=%x]\n", sock->mHandler));
    if (mActiveCount == mActiveListSize) {
        SOCKET_LOG(("  Active List size of %d met\n", mActiveCount));
        if (!GrowActiveList()) {
            NS_ERROR("too many active sockets");
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    
    mActiveList[mActiveCount] = *sock;
    mActiveCount++;

    mPollList[mActiveCount].fd = sock->mFD;
    mPollList[mActiveCount].in_flags = sock->mHandler->mPollFlags;
    mPollList[mActiveCount].out_flags = 0;

    SOCKET_LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
    return NS_OK;
}

void
nsSocketTransportService::RemoveFromPollList(SocketContext *sock)
{
    SOCKET_LOG(("nsSocketTransportService::RemoveFromPollList [handler=%x]\n", sock->mHandler));

    PRUint32 index = sock - mActiveList;
    NS_ABORT_IF_FALSE(index < mActiveListSize, "invalid index");

    SOCKET_LOG(("  index=%u mActiveCount=%u\n", index, mActiveCount));

    if (index != mActiveCount-1) {
        mActiveList[index] = mActiveList[mActiveCount-1];
        mPollList[index+1] = mPollList[mActiveCount];
    }
    mActiveCount--;

    SOCKET_LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
}

nsresult
nsSocketTransportService::AddToIdleList(SocketContext *sock)
{
    NS_ABORT_IF_FALSE(!(((PRUint32)(sock - mIdleList)) < mIdleListSize),
                      "AddToIdlelList Socket Already Idle");

    SOCKET_LOG(("nsSocketTransportService::AddToIdleList [handler=%x]\n", sock->mHandler));
    if (mIdleCount == mIdleListSize) {
        SOCKET_LOG(("  Idle List size of %d met\n", mIdleCount));
        if (!GrowIdleList()) {
            NS_ERROR("too many idle sockets");
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    mIdleList[mIdleCount] = *sock;
    mIdleCount++;

    SOCKET_LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
    return NS_OK;
}

void
nsSocketTransportService::RemoveFromIdleList(SocketContext *sock)
{
    SOCKET_LOG(("nsSocketTransportService::RemoveFromIdleList [handler=%x]\n", sock->mHandler));

    PRUint32 index = sock - mIdleList;
    NS_ASSERTION(index < mIdleListSize, "invalid index in idle list");

    if (index != mIdleCount-1)
        mIdleList[index] = mIdleList[mIdleCount-1];
    mIdleCount--;

    SOCKET_LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
}

void
nsSocketTransportService::MoveToIdleList(SocketContext *sock)
{
    nsresult rv = AddToIdleList(sock);
    if (NS_FAILED(rv))
        DetachSocket(mActiveList, sock);
    else
        RemoveFromPollList(sock);
}

void
nsSocketTransportService::MoveToPollList(SocketContext *sock)
{
    nsresult rv = AddToPollList(sock);
    if (NS_FAILED(rv))
        DetachSocket(mIdleList, sock);
    else
        RemoveFromIdleList(sock);
}

bool
nsSocketTransportService::GrowActiveList()
{
    PRInt32 toAdd = gMaxCount - mActiveListSize;
    if (toAdd > 100)
        toAdd = 100;
    if (toAdd < 1)
        return false;
    
    mActiveListSize += toAdd;
    mActiveList = (SocketContext *)
        moz_xrealloc(mActiveList, sizeof(SocketContext) * mActiveListSize);
    mPollList = (PRPollDesc *)
        moz_xrealloc(mPollList, sizeof(PRPollDesc) * (mActiveListSize + 1));
    return true;
}

bool
nsSocketTransportService::GrowIdleList()
{
    PRInt32 toAdd = gMaxCount - mIdleListSize;
    if (toAdd > 100)
        toAdd = 100;
    if (toAdd < 1)
        return false;

    mIdleListSize += toAdd;
    mIdleList = (SocketContext *)
        moz_xrealloc(mIdleList, sizeof(SocketContext) * mIdleListSize);
    return true;
}

PRIntervalTime
nsSocketTransportService::PollTimeout()
{
    if (mActiveCount == 0)
        return NS_SOCKET_POLL_TIMEOUT;

    // compute minimum time before any socket timeout expires.
    PRUint32 minR = PR_UINT16_MAX;
    for (PRUint32 i=0; i<mActiveCount; ++i) {
        const SocketContext &s = mActiveList[i];
        // mPollTimeout could be less than mElapsedTime if setTimeout
        // was called with a value smaller than mElapsedTime.
        PRUint32 r = (s.mElapsedTime < s.mHandler->mPollTimeout)
          ? s.mHandler->mPollTimeout - s.mElapsedTime
          : 0;
        if (r < minR)
            minR = r;
    }
    SOCKET_LOG(("poll timeout: %lu\n", minR));
    return PR_SecondsToInterval(minR);
}

PRInt32
nsSocketTransportService::Poll(bool wait, PRUint32 *interval)
{
    PRPollDesc *pollList;
    PRUint32 pollCount;
    PRIntervalTime pollTimeout;

    if (mPollList[0].fd) {
        mPollList[0].out_flags = 0;
        pollList = mPollList;
        pollCount = mActiveCount + 1;
        pollTimeout = PollTimeout();
    }
    else {
        // no pollable event, so busy wait...
        pollCount = mActiveCount;
        if (pollCount)
            pollList = &mPollList[1];
        else
            pollList = nsnull;
        pollTimeout = PR_MillisecondsToInterval(25);
    }

    if (!wait)
        pollTimeout = PR_INTERVAL_NO_WAIT;

    PRIntervalTime ts = PR_IntervalNow();

    SOCKET_LOG(("    timeout = %i milliseconds\n",
         PR_IntervalToMilliseconds(pollTimeout)));
    PRInt32 rv = PR_Poll(pollList, pollCount, pollTimeout);

    PRIntervalTime passedInterval = PR_IntervalNow() - ts;

    SOCKET_LOG(("    ...returned after %i milliseconds\n",
         PR_IntervalToMilliseconds(passedInterval))); 

    *interval = PR_IntervalToSeconds(passedInterval);
    return rv;
}

//-----------------------------------------------------------------------------
// xpcom api

NS_IMPL_THREADSAFE_ISUPPORTS6(nsSocketTransportService,
                              nsISocketTransportService,
                              nsIEventTarget,
                              nsIThreadObserver,
                              nsIRunnable,
                              nsPISocketTransportService,
                              nsIObserver)

// called from main thread only
NS_IMETHODIMP
nsSocketTransportService::Init()
{
    NS_TIME_FUNCTION;

    if (!NS_IsMainThread()) {
        NS_ERROR("wrong thread");
        return NS_ERROR_UNEXPECTED;
    }

    if (mInitialized)
        return NS_OK;

    if (mShuttingDown)
        return NS_ERROR_UNEXPECTED;

    // Don't initialize inside the offline mode
    if (gIOService->IsOffline() && !gIOService->IsComingOnline())
        return NS_ERROR_OFFLINE;

    if (!mThreadEvent) {
        mThreadEvent = PR_NewPollableEvent();
        //
        // NOTE: per bug 190000, this failure could be caused by Zone-Alarm
        // or similar software.
        //
        // NOTE: per bug 191739, this failure could also be caused by lack
        // of a loopback device on Windows and OS/2 platforms (NSPR creates
        // a loopback socket pair on these platforms to implement a pollable
        // event object).  if we can't create a pollable event, then we'll
        // have to "busy wait" to implement the socket event queue :-(
        //
        if (!mThreadEvent) {
            NS_WARNING("running socket transport thread without a pollable event");
            SOCKET_LOG(("running socket transport thread without a pollable event"));
        }
    }
    
    NS_TIME_FUNCTION_MARK("Created thread");

    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewThread(getter_AddRefs(thread), this);
    if (NS_FAILED(rv)) return rv;
    
    {
        MutexAutoLock lock(mLock);
        // Install our mThread, protecting against concurrent readers
        thread.swap(mThread);
    }

    nsCOMPtr<nsIPrefBranch> tmpPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (tmpPrefService) 
        tmpPrefService->AddObserver(SEND_BUFFER_PREF, this, false);
    UpdatePrefs();

    NS_TIME_FUNCTION_MARK("UpdatePrefs");

    mInitialized = true;
    return NS_OK;
}

// called from main thread only
NS_IMETHODIMP
nsSocketTransportService::Shutdown()
{
    SOCKET_LOG(("nsSocketTransportService::Shutdown\n"));

    NS_ENSURE_STATE(NS_IsMainThread());

    if (!mInitialized)
        return NS_OK;

    if (mShuttingDown)
        return NS_ERROR_UNEXPECTED;

    {
        MutexAutoLock lock(mLock);

        // signal the socket thread to shutdown
        mShuttingDown = true;

        if (mThreadEvent)
            PR_SetPollableEvent(mThreadEvent);
        // else wait for Poll timeout
    }

    // join with thread
    mThread->Shutdown();
    {
        MutexAutoLock lock(mLock);
        // Drop our reference to mThread and make sure that any concurrent
        // readers are excluded
        mThread = nsnull;
    }

    nsCOMPtr<nsIPrefBranch> tmpPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (tmpPrefService) 
        tmpPrefService->RemoveObserver(SEND_BUFFER_PREF, this);

    mInitialized = false;
    mShuttingDown = false;

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::CreateTransport(const char **types,
                                          PRUint32 typeCount,
                                          const nsACString &host,
                                          PRInt32 port,
                                          nsIProxyInfo *proxyInfo,
                                          nsISocketTransport **result)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_OFFLINE);
    NS_ENSURE_TRUE(port >= 0 && port <= 0xFFFF, NS_ERROR_ILLEGAL_VALUE);

    nsSocketTransport *trans = new nsSocketTransport();
    if (!trans)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);

    nsresult rv = trans->Init(types, typeCount, host, port, proxyInfo);
    if (NS_FAILED(rv)) {
        NS_RELEASE(trans);
        return rv;
    }

    *result = trans;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::GetAutodialEnabled(bool *value)
{
    *value = mAutodialEnabled;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::SetAutodialEnabled(bool value)
{
    mAutodialEnabled = value;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::OnDispatchedEvent(nsIThreadInternal *thread)
{
    MutexAutoLock lock(mLock);
    if (mThreadEvent)
        PR_SetPollableEvent(mThreadEvent);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::OnProcessNextEvent(nsIThreadInternal *thread,
                                             bool mayWait, PRUint32 depth)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::AfterProcessNextEvent(nsIThreadInternal* thread,
                                                PRUint32 depth)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::Run()
{
    PR_SetCurrentThreadName("Socket Thread");

    SOCKET_LOG(("STS thread init\n"));

    psm::InitializeSSLServerCertVerificationThreads();

    gSocketThread = PR_GetCurrentThread();

    // add thread event to poll list (mThreadEvent may be NULL)
    mPollList[0].fd = mThreadEvent;
    mPollList[0].in_flags = PR_POLL_READ;
    mPollList[0].out_flags = 0;

    nsIThread *thread = NS_GetCurrentThread();

    // hook ourselves up to observe event processing for this thread
    nsCOMPtr<nsIThreadInternal> threadInt = do_QueryInterface(thread);
    threadInt->SetObserver(this);

    // make sure the pseudo random number generator is seeded on this thread
    srand(PR_Now());

    for (;;) {
        bool pendingEvents = false;
        thread->HasPendingEvents(&pendingEvents);

        do {
            // If there are pending events for this thread then
            // DoPollIteration() should service the network without blocking.
            DoPollIteration(!pendingEvents);
            
            // If nothing was pending before the poll, it might be now
            if (!pendingEvents)
                thread->HasPendingEvents(&pendingEvents);

            if (pendingEvents) {
                NS_ProcessNextEvent(thread);
                pendingEvents = false;
                thread->HasPendingEvents(&pendingEvents);
            }
        } while (pendingEvents);

        // now that our event queue is empty, check to see if we should exit
        {
            MutexAutoLock lock(mLock);
            if (mShuttingDown)
                break;
        }
    }

    SOCKET_LOG(("STS shutting down thread\n"));

    // detach any sockets
    PRInt32 i;
    for (i=mActiveCount-1; i>=0; --i)
        DetachSocket(mActiveList, &mActiveList[i]);
    for (i=mIdleCount-1; i>=0; --i)
        DetachSocket(mIdleList, &mIdleList[i]);

    // Final pass over the event queue. This makes sure that events posted by
    // socket detach handlers get processed.
    NS_ProcessPendingEvents(thread);

    gSocketThread = nsnull;

    psm::StopSSLServerCertVerificationThreads();

    SOCKET_LOG(("STS thread exit\n"));
    return NS_OK;
}

nsresult
nsSocketTransportService::DoPollIteration(bool wait)
{
    SOCKET_LOG(("STS poll iter [%d]\n", wait));

    PRInt32 i, count;

    //
    // poll loop
    //
    // walk active list backwards to see if any sockets should actually be
    // idle, then walk the idle list backwards to see if any idle sockets
    // should become active.  take care to check only idle sockets that
    // were idle to begin with ;-)
    //
    count = mIdleCount;
    for (i=mActiveCount-1; i>=0; --i) {
        //---
        SOCKET_LOG(("  active [%u] { handler=%x condition=%x pollflags=%hu }\n", i,
            mActiveList[i].mHandler,
            mActiveList[i].mHandler->mCondition,
            mActiveList[i].mHandler->mPollFlags));
        //---
        if (NS_FAILED(mActiveList[i].mHandler->mCondition))
            DetachSocket(mActiveList, &mActiveList[i]);
        else {
            PRUint16 in_flags = mActiveList[i].mHandler->mPollFlags;
            if (in_flags == 0)
                MoveToIdleList(&mActiveList[i]);
            else {
                // update poll flags
                mPollList[i+1].in_flags = in_flags;
                mPollList[i+1].out_flags = 0;
            }
        }
    }
    for (i=count-1; i>=0; --i) {
        //---
        SOCKET_LOG(("  idle [%u] { handler=%x condition=%x pollflags=%hu }\n", i,
            mIdleList[i].mHandler,
            mIdleList[i].mHandler->mCondition,
            mIdleList[i].mHandler->mPollFlags));
        //---
        if (NS_FAILED(mIdleList[i].mHandler->mCondition))
            DetachSocket(mIdleList, &mIdleList[i]);
        else if (mIdleList[i].mHandler->mPollFlags != 0)
            MoveToPollList(&mIdleList[i]);
    }

    SOCKET_LOG(("  calling PR_Poll [active=%u idle=%u]\n", mActiveCount, mIdleCount));

#if defined(XP_WIN)
    // 30 active connections is the historic limit before firefox 7's 256. A few
    //  windows systems have troubles with the higher limit, so actively probe a
    // limit the first time we exceed 30.
    if ((mActiveCount > 30) && !mProbedMaxCount)
        ProbeMaxCount();
#endif

    // Measures seconds spent while blocked on PR_Poll
    PRUint32 pollInterval;

    PRInt32 n = Poll(wait, &pollInterval);
    if (n < 0) {
        SOCKET_LOG(("  PR_Poll error [%d]\n", PR_GetError()));
    }
    else {
        //
        // service "active" sockets...
        //
        for (i=0; i<PRInt32(mActiveCount); ++i) {
            PRPollDesc &desc = mPollList[i+1];
            SocketContext &s = mActiveList[i];
            if (n > 0 && desc.out_flags != 0) {
                s.mElapsedTime = 0;
                s.mHandler->OnSocketReady(desc.fd, desc.out_flags);
            }
            // check for timeout errors unless disabled...
            else if (s.mHandler->mPollTimeout != PR_UINT16_MAX) {
                // update elapsed time counter
                if (NS_UNLIKELY(pollInterval > (PR_UINT16_MAX - s.mElapsedTime)))
                    s.mElapsedTime = PR_UINT16_MAX;
                else
                    s.mElapsedTime += PRUint16(pollInterval);
                // check for timeout expiration 
                if (s.mElapsedTime >= s.mHandler->mPollTimeout) {
                    s.mElapsedTime = 0;
                    s.mHandler->OnSocketReady(desc.fd, -1);
                }
            }
        }

        //
        // check for "dead" sockets and remove them (need to do this in
        // reverse order obviously).
        //
        for (i=mActiveCount-1; i>=0; --i) {
            if (NS_FAILED(mActiveList[i].mHandler->mCondition))
                DetachSocket(mActiveList, &mActiveList[i]);
        }

        if (n != 0 && mPollList[0].out_flags == PR_POLL_READ) {
            // acknowledge pollable event (wait should not block)
            if (PR_WaitForPollableEvent(mThreadEvent) != PR_SUCCESS) {
                // On Windows, the TCP loopback connection in the
                // pollable event may become broken when a laptop
                // switches between wired and wireless networks or
                // wakes up from hibernation.  We try to create a
                // new pollable event.  If that fails, we fall back
                // on "busy wait".
                {
                    MutexAutoLock lock(mLock);
                    PR_DestroyPollableEvent(mThreadEvent);
                    mThreadEvent = PR_NewPollableEvent();
                }
                if (!mThreadEvent) {
                    NS_WARNING("running socket transport thread without "
                               "a pollable event");
                    SOCKET_LOG(("running socket transport thread without "
                         "a pollable event"));
                }
                mPollList[0].fd = mThreadEvent;
                // mPollList[0].in_flags was already set to PR_POLL_READ
                // in Run().
                mPollList[0].out_flags = 0;
            }
        }
    }

    return NS_OK;
}

nsresult
nsSocketTransportService::UpdatePrefs()
{
    mSendBufferSize = 0;
    
    nsCOMPtr<nsIPrefBranch> tmpPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (tmpPrefService) {
        PRInt32 bufferSize;
        nsresult rv = tmpPrefService->GetIntPref(SEND_BUFFER_PREF, &bufferSize);
        if (NS_SUCCEEDED(rv) && bufferSize > 0)
            mSendBufferSize = bufferSize;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::Observe(nsISupports *subject,
                                  const char *topic,
                                  const PRUnichar *data)
{
    if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        UpdatePrefs();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::GetSendBufferSize(PRInt32 *value)
{
    *value = mSendBufferSize;
    return NS_OK;
}


/// ugly OS specific includes are placed at the bottom of the src for clarity

#if defined(XP_WIN)
#include <windows.h>
#elif defined(XP_UNIX) && !defined(AIX) && !defined(NEXTSTEP) && !defined(QNX)
#include <sys/resource.h>
#endif

// Right now the only need to do this is on windows.
#if defined(XP_WIN)
void
nsSocketTransportService::ProbeMaxCount()
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mProbedMaxCount)
        return;
    mProbedMaxCount = true;

    PRInt32 startedMaxCount = gMaxCount;

    // Allocate and test a PR_Poll up to the gMaxCount number of unconnected
    // sockets. See bug 692260 - windows should be able to handle 1000 sockets
    // in select() without a problem, but LSPs have been known to balk at lower
    // numbers. (64 in the bug).

    // Allocate
    struct PRPollDesc pfd[SOCKET_LIMIT_TARGET];
    PRUint32 numAllocated = 0;

    for (PRUint32 index = 0 ; index < gMaxCount; ++index) {
        pfd[index].in_flags = PR_POLL_READ | PR_POLL_WRITE | PR_POLL_EXCEPT;
        pfd[index].out_flags = 0;
        pfd[index].fd =  PR_OpenTCPSocket(PR_AF_INET);
        if (!pfd[index].fd) {
            SOCKET_LOG(("Socket Limit Test index %d failed\n", index));
            if (index < SOCKET_LIMIT_MIN)
                gMaxCount = SOCKET_LIMIT_MIN;
            else
                gMaxCount = index;
            break;
        }
        ++numAllocated;
    }

    // Test
    PR_STATIC_ASSERT(SOCKET_LIMIT_MIN >= 32U);
    while (gMaxCount <= numAllocated) {
        PRInt32 rv = PR_Poll(pfd, gMaxCount, PR_MillisecondsToInterval(0));
        
        SOCKET_LOG(("Socket Limit Test poll() size=%d rv=%d\n",
                    gMaxCount, rv));

        if (rv >= 0)
            break;

        SOCKET_LOG(("Socket Limit Test poll confirmationSize=%d rv=%d error=%d\n",
                    gMaxCount, rv, PR_GetError()));

        gMaxCount -= 32;
        if (gMaxCount <= SOCKET_LIMIT_MIN) {
            gMaxCount = SOCKET_LIMIT_MIN;
            break;
        }
    }

    // Free
    for (PRUint32 index = 0 ; index < numAllocated; ++index)
        if (pfd[index].fd)
            PR_Close(pfd[index].fd);

    SOCKET_LOG(("Socket Limit Test max was confirmed at %d\n", gMaxCount));
}
#endif // windows

PRStatus
nsSocketTransportService::DiscoverMaxCount()
{
    gMaxCount = SOCKET_LIMIT_MIN;

#if defined(XP_UNIX) && !defined(AIX) && !defined(NEXTSTEP) && !defined(QNX)
    // On unix and os x network sockets and file
    // descriptors are the same. OS X comes defaulted at 256,
    // most linux at 1000. We can reliably use [sg]rlimit to
    // query that and raise it. We will try to raise it 250 past
    // our target number of SOCKET_LIMIT_TARGET so that some descriptors
    // are still available for other things.

    struct rlimit rlimitData;
    if (getrlimit(RLIMIT_NOFILE, &rlimitData) == -1)
        return PR_SUCCESS;
    if (rlimitData.rlim_cur >=  SOCKET_LIMIT_TARGET + 250) {
        gMaxCount = SOCKET_LIMIT_TARGET;
        return PR_SUCCESS;
    }

    PRInt32 maxallowed = rlimitData.rlim_max;
    if (maxallowed == -1) {                       /* no limit */
        maxallowed = SOCKET_LIMIT_TARGET + 250;
    } else if ((PRUint32)maxallowed < SOCKET_LIMIT_MIN + 250) {
        return PR_SUCCESS;
    } else if ((PRUint32)maxallowed > SOCKET_LIMIT_TARGET + 250) {
        maxallowed = SOCKET_LIMIT_TARGET + 250;
    }

    rlimitData.rlim_cur = maxallowed;
    setrlimit(RLIMIT_NOFILE, &rlimitData);
    if (getrlimit(RLIMIT_NOFILE, &rlimitData) != -1)
        if (rlimitData.rlim_cur > SOCKET_LIMIT_MIN + 250)
            gMaxCount = rlimitData.rlim_cur - 250;

#elif defined(XP_WIN) && !defined(WIN_CE)
    // >= XP is confirmed to have at least 1000
    gMaxCount = SOCKET_LIMIT_TARGET;
#else
    // other platforms are harder to test - so leave at safe legacy value
#endif

    return PR_SUCCESS;
}
