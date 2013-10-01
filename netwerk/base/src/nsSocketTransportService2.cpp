// vim:set sw=4 sts=4 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nsSocketTransportService2.h"
#include "nsSocketTransport2.h"
#include "nsError.h"
#include "prnetdb.h"
#include "prerror.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "NetworkActivityMonitor.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/Likely.h"
#include "mozilla/PublicSSL.h"
#include "nsThreadUtils.h"
#include "nsIFile.h"

using namespace mozilla;
using namespace mozilla::net;

#if defined(PR_LOGGING)
PRLogModuleInfo *gSocketTransportLog = nullptr;
#endif

nsSocketTransportService *gSocketTransportService = nullptr;
PRThread                 *gSocketThread           = nullptr;

#define SEND_BUFFER_PREF "network.tcp.sendbuffer"
#define SOCKET_LIMIT_TARGET 550U
#define SOCKET_LIMIT_MIN     50U
#define BLIP_INTERVAL_PREF "network.activity.blipIntervalMilliseconds"

uint32_t nsSocketTransportService::gMaxCount;
PRCallOnceType nsSocketTransportService::gMaxCountInitOnce;

//-----------------------------------------------------------------------------
// ctor/dtor (called on the main/UI thread by the service manager)

nsSocketTransportService::nsSocketTransportService()
    : mThread(nullptr)
    , mThreadEvent(nullptr)
    , mAutodialEnabled(false)
    , mLock("nsSocketTransportService::mLock")
    , mInitialized(false)
    , mShuttingDown(false)
    , mOffline(false)
    , mGoingOffline(false)
    , mActiveListSize(SOCKET_LIMIT_MIN)
    , mIdleListSize(SOCKET_LIMIT_MIN)
    , mActiveCount(0)
    , mIdleCount(0)
    , mSentBytesCount(0)
    , mReceivedBytesCount(0)
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
    gSocketTransportService = nullptr;
}

//-----------------------------------------------------------------------------
// event queue (any thread)

already_AddRefed<nsIThread>
nsSocketTransportService::GetThreadSafely()
{
    MutexAutoLock lock(mLock);
    nsCOMPtr<nsIThread> result = mThread;
    return result.forget();
}

NS_IMETHODIMP
nsSocketTransportService::Dispatch(nsIRunnable *event, uint32_t flags)
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
    SOCKET_LOG(("nsSocketTransportService::AttachSocket [handler=%p]\n", handler));

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
    SOCKET_LOG(("nsSocketTransportService::DetachSocket [handler=%p]\n", sock->mHandler));
    NS_ABORT_IF_FALSE((listHead == mActiveList) || (listHead == mIdleList),
                      "DetachSocket invalid head");

    // inform the handler that this socket is going away
    sock->mHandler->OnSocketDetached(sock->mFD);
    mSentBytesCount += sock->mHandler->ByteCountSent();
    mReceivedBytesCount += sock->mHandler->ByteCountReceived();

    // cleanup
    sock->mFD = nullptr;
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
    NS_ABORT_IF_FALSE(!(((uint32_t)(sock - mActiveList)) < mActiveListSize),
                      "AddToPollList Socket Already Active");

    SOCKET_LOG(("nsSocketTransportService::AddToPollList [handler=%p]\n", sock->mHandler));
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
    SOCKET_LOG(("nsSocketTransportService::RemoveFromPollList [handler=%p]\n", sock->mHandler));

    uint32_t index = sock - mActiveList;
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
    NS_ABORT_IF_FALSE(!(((uint32_t)(sock - mIdleList)) < mIdleListSize),
                      "AddToIdlelList Socket Already Idle");

    SOCKET_LOG(("nsSocketTransportService::AddToIdleList [handler=%p]\n", sock->mHandler));
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
    SOCKET_LOG(("nsSocketTransportService::RemoveFromIdleList [handler=%p]\n", sock->mHandler));

    uint32_t index = sock - mIdleList;
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
    int32_t toAdd = gMaxCount - mActiveListSize;
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
    int32_t toAdd = gMaxCount - mIdleListSize;
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
    uint32_t minR = UINT16_MAX;
    for (uint32_t i=0; i<mActiveCount; ++i) {
        const SocketContext &s = mActiveList[i];
        // mPollTimeout could be less than mElapsedTime if setTimeout
        // was called with a value smaller than mElapsedTime.
        uint32_t r = (s.mElapsedTime < s.mHandler->mPollTimeout)
          ? s.mHandler->mPollTimeout - s.mElapsedTime
          : 0;
        if (r < minR)
            minR = r;
    }
    SOCKET_LOG(("poll timeout: %lu\n", minR));
    return PR_SecondsToInterval(minR);
}

int32_t
nsSocketTransportService::Poll(bool wait, uint32_t *interval)
{
    PRPollDesc *pollList;
    uint32_t pollCount;
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
            pollList = nullptr;
        pollTimeout = PR_MillisecondsToInterval(25);
    }

    if (!wait)
        pollTimeout = PR_INTERVAL_NO_WAIT;

    PRIntervalTime ts = PR_IntervalNow();

    SOCKET_LOG(("    timeout = %i milliseconds\n",
         PR_IntervalToMilliseconds(pollTimeout)));
    int32_t rv = PR_Poll(pollList, pollCount, pollTimeout);

    PRIntervalTime passedInterval = PR_IntervalNow() - ts;

    SOCKET_LOG(("    ...returned after %i milliseconds\n",
         PR_IntervalToMilliseconds(passedInterval))); 

    *interval = PR_IntervalToSeconds(passedInterval);
    return rv;
}

//-----------------------------------------------------------------------------
// xpcom api

NS_IMPL_ISUPPORTS6(nsSocketTransportService,
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
    if (!NS_IsMainThread()) {
        NS_ERROR("wrong thread");
        return NS_ERROR_UNEXPECTED;
    }

    if (mInitialized)
        return NS_OK;

    if (mShuttingDown)
        return NS_ERROR_UNEXPECTED;

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

    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewThread(getter_AddRefs(thread), this);
    if (NS_FAILED(rv)) return rv;
    
    {
        MutexAutoLock lock(mLock);
        // Install our mThread, protecting against concurrent readers
        thread.swap(mThread);
    }

    nsCOMPtr<nsIPrefBranch> tmpPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (tmpPrefService) {
        tmpPrefService->AddObserver(SEND_BUFFER_PREF, this, false);
    }
    UpdatePrefs();

    nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
    if (obsSvc) {
        obsSvc->AddObserver(this, "profile-initial-state", false);
        obsSvc->AddObserver(this, "last-pb-context-exited", false);
    }

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
        mThread = nullptr;
    }

    nsCOMPtr<nsIPrefBranch> tmpPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (tmpPrefService) 
        tmpPrefService->RemoveObserver(SEND_BUFFER_PREF, this);

    nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
    if (obsSvc) {
        obsSvc->RemoveObserver(this, "profile-initial-state");
        obsSvc->RemoveObserver(this, "last-pb-context-exited");
    }

    mozilla::net::NetworkActivityMonitor::Shutdown();

    mInitialized = false;
    mShuttingDown = false;

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::GetOffline(bool *offline)
{
    *offline = mOffline;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::SetOffline(bool offline)
{
    MutexAutoLock lock(mLock);
    if (!mOffline && offline) {
        // signal the socket thread to go offline, so it will detach sockets
        mGoingOffline = true;
        mOffline = true;
    }
    else if (mOffline && !offline) {
        mOffline = false;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::CreateTransport(const char **types,
                                          uint32_t typeCount,
                                          const nsACString &host,
                                          int32_t port,
                                          nsIProxyInfo *proxyInfo,
                                          nsISocketTransport **result)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);
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
nsSocketTransportService::CreateUnixDomainTransport(nsIFile *aPath,
                                                    nsISocketTransport **result)
{
    nsresult rv;

    NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

    nsAutoCString path;
    rv = aPath->GetNativePath(path);
    if (NS_FAILED(rv))
        return rv;

    nsRefPtr<nsSocketTransport> trans = new nsSocketTransport();
    if (!trans)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = trans->InitWithFilename(path.get());
    if (NS_FAILED(rv))
        return rv;

    trans.forget(result);
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
                                             bool mayWait, uint32_t depth)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::AfterProcessNextEvent(nsIThreadInternal* thread,
                                                uint32_t depth)
{
    return NS_OK;
}

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

NS_IMETHODIMP
nsSocketTransportService::Run()
{
    PR_SetCurrentThreadName("Socket Thread");

#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        NS_ASSERTION(NuwaMarkCurrentThread != nullptr,
                     "NuwaMarkCurrentThread is undefined!");
        NuwaMarkCurrentThread(nullptr, nullptr);
    }
#endif

    SOCKET_LOG(("STS thread init\n"));

    psm::InitializeSSLServerCertVerificationThreads();

    gSocketThread = PR_GetCurrentThread();

    // add thread event to poll list (mThreadEvent may be nullptr)
    mPollList[0].fd = mThreadEvent;
    mPollList[0].in_flags = PR_POLL_READ;
    mPollList[0].out_flags = 0;

    nsIThread *thread = NS_GetCurrentThread();

    // hook ourselves up to observe event processing for this thread
    nsCOMPtr<nsIThreadInternal> threadInt = do_QueryInterface(thread);
    threadInt->SetObserver(this);

    // make sure the pseudo random number generator is seeded on this thread
    srand(static_cast<unsigned>(PR_Now()));

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

        bool goingOffline = false;
        // now that our event queue is empty, check to see if we should exit
        {
            MutexAutoLock lock(mLock);
            if (mShuttingDown)
                break;
            if (mGoingOffline) {
                mGoingOffline = false;
                goingOffline = true;
            }
        }
        // Avoid potential deadlock
        if (goingOffline)
            Reset(true);
    }

    SOCKET_LOG(("STS shutting down thread\n"));

    // detach all sockets, including locals
    Reset(false);

    // Final pass over the event queue. This makes sure that events posted by
    // socket detach handlers get processed.
    NS_ProcessPendingEvents(thread);

    gSocketThread = nullptr;

    psm::StopSSLServerCertVerificationThreads();

    SOCKET_LOG(("STS thread exit\n"));
    return NS_OK;
}

void
nsSocketTransportService::DetachSocketWithGuard(bool aGuardLocals,
                                                SocketContext *socketList,
                                                int32_t index)
{
    bool isGuarded = false;
    if (aGuardLocals) {
        socketList[index].mHandler->IsLocal(&isGuarded);
        if (!isGuarded)
            socketList[index].mHandler->KeepWhenOffline(&isGuarded);
    }
    if (!isGuarded)
        DetachSocket(socketList, &socketList[index]);
}

void
nsSocketTransportService::Reset(bool aGuardLocals)
{
    // detach any sockets
    int32_t i;
    for (i = mActiveCount - 1; i >= 0; --i) {
        DetachSocketWithGuard(aGuardLocals, mActiveList, i);
    }
    for (i = mIdleCount - 1; i >= 0; --i) {
        DetachSocketWithGuard(aGuardLocals, mIdleList, i);
    }
}

nsresult
nsSocketTransportService::DoPollIteration(bool wait)
{
    SOCKET_LOG(("STS poll iter [%d]\n", wait));

    int32_t i, count;

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
        SOCKET_LOG(("  active [%u] { handler=%p condition=%x pollflags=%hu }\n", i,
            mActiveList[i].mHandler,
            mActiveList[i].mHandler->mCondition,
            mActiveList[i].mHandler->mPollFlags));
        //---
        if (NS_FAILED(mActiveList[i].mHandler->mCondition))
            DetachSocket(mActiveList, &mActiveList[i]);
        else {
            uint16_t in_flags = mActiveList[i].mHandler->mPollFlags;
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
        SOCKET_LOG(("  idle [%u] { handler=%p condition=%x pollflags=%hu }\n", i,
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
    uint32_t pollInterval;

    int32_t n = Poll(wait, &pollInterval);
    if (n < 0) {
        SOCKET_LOG(("  PR_Poll error [%d]\n", PR_GetError()));
    }
    else {
        //
        // service "active" sockets...
        //
        for (i=0; i<int32_t(mActiveCount); ++i) {
            PRPollDesc &desc = mPollList[i+1];
            SocketContext &s = mActiveList[i];
            if (n > 0 && desc.out_flags != 0) {
                s.mElapsedTime = 0;
                s.mHandler->OnSocketReady(desc.fd, desc.out_flags);
            }
            // check for timeout errors unless disabled...
            else if (s.mHandler->mPollTimeout != UINT16_MAX) {
                // update elapsed time counter
                // (NOTE: We explicitly cast UINT16_MAX to be an unsigned value
                // here -- otherwise, some compilers will treat it as signed,
                // which makes them fire signed/unsigned-comparison build
                // warnings for the comparison against 'pollInterval'.)
                if (MOZ_UNLIKELY(pollInterval >
                                static_cast<uint32_t>(UINT16_MAX) -
                                s.mElapsedTime))
                    s.mElapsedTime = UINT16_MAX;
                else
                    s.mElapsedTime += uint16_t(pollInterval);
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
        int32_t bufferSize;
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
        return NS_OK;
    }

    if (!strcmp(topic, "profile-initial-state")) {
        int32_t blipInterval = Preferences::GetInt(BLIP_INTERVAL_PREF, 0);
        if (blipInterval <= 0) {
            return NS_OK;
        }

        return net::NetworkActivityMonitor::Init(blipInterval);
    }

    if (!strcmp(topic, "last-pb-context-exited")) {
        nsCOMPtr<nsIRunnable> ev =
          NS_NewRunnableMethod(this,
                               &nsSocketTransportService::ClosePrivateConnections);
        nsresult rv = Dispatch(ev, nsIEventTarget::DISPATCH_NORMAL);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

void
nsSocketTransportService::ClosePrivateConnections()
{
    // Must be called on the socket thread.
#ifdef DEBUG
    bool onSTSThread;
    IsOnCurrentThread(&onSTSThread);
    MOZ_ASSERT(onSTSThread);
#endif

    for (int32_t i = mActiveCount - 1; i >= 0; --i) {
        if (mActiveList[i].mHandler->mIsPrivate) {
            DetachSocket(mActiveList, &mActiveList[i]);
        }
    }
    for (int32_t i = mIdleCount - 1; i >= 0; --i) {
        if (mIdleList[i].mHandler->mIsPrivate) {
            DetachSocket(mIdleList, &mIdleList[i]);
        }
    }

    mozilla::ClearPrivateSSLState();
}

NS_IMETHODIMP
nsSocketTransportService::GetSendBufferSize(int32_t *value)
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

    // Allocate and test a PR_Poll up to the gMaxCount number of unconnected
    // sockets. See bug 692260 - windows should be able to handle 1000 sockets
    // in select() without a problem, but LSPs have been known to balk at lower
    // numbers. (64 in the bug).

    // Allocate
    struct PRPollDesc pfd[SOCKET_LIMIT_TARGET];
    uint32_t numAllocated = 0;

    for (uint32_t index = 0 ; index < gMaxCount; ++index) {
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
        int32_t rv = PR_Poll(pfd, gMaxCount, PR_MillisecondsToInterval(0));
        
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
    for (uint32_t index = 0 ; index < numAllocated; ++index)
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

    int32_t maxallowed = rlimitData.rlim_max;
    if (maxallowed == -1) {                       /* no limit */
        maxallowed = SOCKET_LIMIT_TARGET + 250;
    } else if ((uint32_t)maxallowed < SOCKET_LIMIT_MIN + 250) {
        return PR_SUCCESS;
    } else if ((uint32_t)maxallowed > SOCKET_LIMIT_TARGET + 250) {
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


// Used to return connection info to Dashboard.cpp
void
nsSocketTransportService::AnalyzeConnection(nsTArray<SocketInfo> *data,
        struct SocketContext *context, bool aActive)
{
    if (context->mHandler->mIsPrivate)
        return;
    PRFileDesc *aFD = context->mFD;
    bool tcp = (PR_GetDescType(aFD) == PR_DESC_SOCKET_TCP);

    PRNetAddr peer_addr;
    PR_GetPeerName(aFD, &peer_addr);

    char host[64] = {0};
    PR_NetAddrToString(&peer_addr, host, sizeof(host));

    uint16_t port;
    if (peer_addr.raw.family == PR_AF_INET)
        port = peer_addr.inet.port;
    else
        port = peer_addr.ipv6.port;
    port = PR_ntohs(port);
    uint64_t sent = context->mHandler->ByteCountSent();
    uint64_t received = context->mHandler->ByteCountReceived();
    SocketInfo info = { nsCString(host), sent, received, port, aActive, tcp };

    data->AppendElement(info);
}

void
nsSocketTransportService::GetSocketConnections(nsTArray<SocketInfo> *data)
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    for (uint32_t i = 0; i < mActiveCount; i++)
        AnalyzeConnection(data, &mActiveList[i], true);
    for (uint32_t i = 0; i < mIdleCount; i++)
        AnalyzeConnection(data, &mIdleList[i], false);
}


