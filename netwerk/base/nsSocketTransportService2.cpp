// vim:set sw=4 sts=4 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSocketTransportService2.h"
#include "nsSocketTransport2.h"
#include "IOActivityMonitor.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "nsIOService.h"
#include "nsASocketHandler.h"
#include "nsError.h"
#include "prnetdb.h"
#include "prerror.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "mozilla/Atomics.h"
#include "mozilla/Services.h"
#include "mozilla/Likely.h"
#include "mozilla/PublicSSL.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Telemetry.h"
#include "nsThreadUtils.h"
#include "nsIFile.h"
#include "nsIWidget.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

namespace mozilla {
namespace net {

LazyLogModule gSocketTransportLog("nsSocketTransport");
LazyLogModule gUDPSocketLog("UDPSocket");
LazyLogModule gTCPSocketLog("TCPSocket");

nsSocketTransportService *gSocketTransportService = nullptr;
static Atomic<PRThread*, Relaxed> gSocketThread;

#define SEND_BUFFER_PREF "network.tcp.sendbuffer"
#define KEEPALIVE_ENABLED_PREF "network.tcp.keepalive.enabled"
#define KEEPALIVE_IDLE_TIME_PREF "network.tcp.keepalive.idle_time"
#define KEEPALIVE_RETRY_INTERVAL_PREF "network.tcp.keepalive.retry_interval"
#define KEEPALIVE_PROBE_COUNT_PREF "network.tcp.keepalive.probe_count"
#define SOCKET_LIMIT_TARGET 1000U
#define SOCKET_LIMIT_MIN      50U
#define MAX_TIME_BETWEEN_TWO_POLLS "network.sts.max_time_for_events_between_two_polls"
#define POLL_BUSY_WAIT_PERIOD "network.sts.poll_busy_wait_period"
#define POLL_BUSY_WAIT_PERIOD_TIMEOUT "network.sts.poll_busy_wait_period_timeout"
#define TELEMETRY_PREF "toolkit.telemetry.enabled"
#define MAX_TIME_FOR_PR_CLOSE_DURING_SHUTDOWN "network.sts.max_time_for_pr_close_during_shutdown"
#define POLLABLE_EVENT_TIMEOUT "network.sts.pollable_event_timeout"

#define REPAIR_POLLABLE_EVENT_TIME 10

uint32_t nsSocketTransportService::gMaxCount;
PRCallOnceType nsSocketTransportService::gMaxCountInitOnce;

// Utility functions
bool
OnSocketThread()
{
  return PR_GetCurrentThread() == gSocketThread;
}

//-----------------------------------------------------------------------------

bool
nsSocketTransportService::SocketContext::IsTimedOut(PRIntervalTime now) const
{
    return TimeoutIn(now) == 0;
}

void
nsSocketTransportService::SocketContext::EnsureTimeout(PRIntervalTime now)
{
    SOCKET_LOG(("SocketContext::EnsureTimeout socket=%p", mHandler));
    if (!mPollStartEpoch) {
        SOCKET_LOG(("  engaging"));
        mPollStartEpoch = now;
    }
}

void
nsSocketTransportService::SocketContext::DisengageTimeout()
{
    SOCKET_LOG(("SocketContext::DisengageTimeout socket=%p", mHandler));
    mPollStartEpoch = 0;
}

PRIntervalTime
nsSocketTransportService::SocketContext::TimeoutIn(PRIntervalTime now) const
{
    SOCKET_LOG(("SocketContext::TimeoutIn socket=%p, timeout=%us",
                mHandler, mHandler->mPollTimeout));

    if (mHandler->mPollTimeout == UINT16_MAX || !mPollStartEpoch) {
        SOCKET_LOG(("  not engaged"));
        return NS_SOCKET_POLL_TIMEOUT;
    }

    PRIntervalTime elapsed = (now - mPollStartEpoch);
    PRIntervalTime timeout = PR_SecondsToInterval(mHandler->mPollTimeout);

    if (elapsed >= timeout) {
        SOCKET_LOG(("  timed out!"));
        return 0;
    }
    SOCKET_LOG(("  remains %us", PR_IntervalToSeconds(timeout - elapsed)));
    return timeout - elapsed;
}

//-----------------------------------------------------------------------------
// ctor/dtor (called on the main/UI thread by the service manager)

nsSocketTransportService::nsSocketTransportService()
    : mThread(nullptr)
    , mLock("nsSocketTransportService::mLock")
    , mInitialized(false)
    , mShuttingDown(false)
    , mOffline(false)
    , mGoingOffline(false)
    , mRawThread(nullptr)
    , mActiveListSize(SOCKET_LIMIT_MIN)
    , mIdleListSize(SOCKET_LIMIT_MIN)
    , mActiveCount(0)
    , mIdleCount(0)
    , mSentBytesCount(0)
    , mReceivedBytesCount(0)
    , mSendBufferSize(0)
    , mKeepaliveIdleTimeS(600)
    , mKeepaliveRetryIntervalS(1)
    , mKeepaliveProbeCount(kDefaultTCPKeepCount)
    , mKeepaliveEnabledPref(false)
    , mPollableEventTimeout(TimeDuration::FromSeconds(6))
    , mServingPendingQueue(false)
    , mMaxTimePerPollIter(100)
    , mTelemetryEnabledPref(false)
    , mMaxTimeForPrClosePref(PR_SecondsToInterval(5))
    , mLastNetworkLinkChangeTime(0)
    , mNetworkLinkChangeBusyWaitPeriod(PR_SecondsToInterval(50))
    , mNetworkLinkChangeBusyWaitTimeout(PR_SecondsToInterval(7))
    , mSleepPhase(false)
    , mProbedMaxCount(false)
#if defined(XP_WIN)
    , mPolling(false)
#endif
{
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

    free(mActiveList);
    free(mIdleList);
    free(mPollList);
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
nsSocketTransportService::DispatchFromScript(nsIRunnable *event, uint32_t flags)
{
  nsCOMPtr<nsIRunnable> event_ref(event);
  return Dispatch(event_ref.forget(), flags);
}

NS_IMETHODIMP
nsSocketTransportService::Dispatch(already_AddRefed<nsIRunnable> event, uint32_t flags)
{
    nsCOMPtr<nsIRunnable> event_ref(event);
    SOCKET_LOG(("STS dispatch [%p]\n", event_ref.get()));

    nsCOMPtr<nsIThread> thread = GetThreadSafely();
    nsresult rv;
    rv = thread ? thread->Dispatch(event_ref.forget(), flags) : NS_ERROR_NOT_INITIALIZED;
    if (rv == NS_ERROR_UNEXPECTED) {
        // Thread is no longer accepting events. We must have just shut it
        // down on the main thread. Pretend we never saw it.
        rv = NS_ERROR_NOT_INITIALIZED;
    }
    return rv;
}

NS_IMETHODIMP
nsSocketTransportService::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketTransportService::IsOnCurrentThread(bool *result)
{
    nsCOMPtr<nsIThread> thread = GetThreadSafely();
    NS_ENSURE_TRUE(thread, NS_ERROR_NOT_INITIALIZED);
    return thread->IsOnCurrentThread(result);
}

NS_IMETHODIMP_(bool)
nsSocketTransportService::IsOnCurrentThreadInfallible()
{
    nsCOMPtr<nsIThread> thread = GetThreadSafely();
    NS_ENSURE_TRUE(thread, false);
    return thread->IsOnCurrentThread();
}

//-----------------------------------------------------------------------------
// socket api (socket thread only)

NS_IMETHODIMP
nsSocketTransportService::NotifyWhenCanAttachSocket(nsIRunnable *event)
{
    SOCKET_LOG(("nsSocketTransportService::NotifyWhenCanAttachSocket\n"));

    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    if (CanAttachSocket()) {
        return Dispatch(event, NS_DISPATCH_NORMAL);
    }

    auto *runnable = new LinkedRunnableEvent(event);
    mPendingSocketQueue.insertBack(runnable);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::AttachSocket(PRFileDesc *fd, nsASocketHandler *handler)
{
    SOCKET_LOG(("nsSocketTransportService::AttachSocket [handler=%p]\n", handler));

    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    if (!CanAttachSocket()) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    SocketContext sock;
    sock.mFD = fd;
    sock.mHandler = handler;
    sock.mPollStartEpoch = 0;

    nsresult rv = AddToIdleList(&sock);
    if (NS_SUCCEEDED(rv))
        NS_ADDREF(handler);
    return rv;
}

// the number of sockets that can be attached at any given time is
// limited.  this is done because some operating systems (e.g., Win9x)
// limit the number of sockets that can be created by an application.
// AttachSocket will fail if the limit is exceeded.  consumers should
// call CanAttachSocket and check the result before creating a socket.

bool
nsSocketTransportService::CanAttachSocket()
{
    static bool reported900FDLimit = false;

    uint32_t total = mActiveCount + mIdleCount;
    bool rv = total < gMaxCount;

    if (mTelemetryEnabledPref &&
        (((total >= 900) || !rv) && !reported900FDLimit)) {
        reported900FDLimit = true;
        Telemetry::Accumulate(Telemetry::NETWORK_SESSION_AT_900FD, true);
    }

    return rv;
}

nsresult
nsSocketTransportService::DetachSocket(SocketContext *listHead, SocketContext *sock)
{
    SOCKET_LOG(("nsSocketTransportService::DetachSocket [handler=%p]\n", sock->mHandler));
    MOZ_ASSERT((listHead == mActiveList) || (listHead == mIdleList),
               "DetachSocket invalid head");

    {
#ifdef MOZ_TASK_TRACER
	tasktracer::AutoSourceEvent taskTracerEvent(tasktracer::SourceEventType::SocketIO);
#endif
        // inform the handler that this socket is going away
        sock->mHandler->OnSocketDetached(sock->mFD);
    }
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
    LinkedRunnableEvent *runnable = mPendingSocketQueue.getFirst();
    if (runnable) {
        event = runnable->TakeEvent();
        runnable->remove();
        delete runnable;
    }
    if (event) {
        // move event from pending queue to dispatch queue
        return Dispatch(event, NS_DISPATCH_NORMAL);
    }
    return NS_OK;
}

nsresult
nsSocketTransportService::AddToPollList(SocketContext *sock)
{
    MOZ_ASSERT(!(static_cast<uint32_t>(sock - mActiveList) < mActiveListSize),
               "AddToPollList Socket Already Active");

    SOCKET_LOG(("nsSocketTransportService::AddToPollList [handler=%p]\n", sock->mHandler));
    if (mActiveCount == mActiveListSize) {
        SOCKET_LOG(("  Active List size of %d met\n", mActiveCount));
        if (!GrowActiveList()) {
            NS_ERROR("too many active sockets");
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    uint32_t newSocketIndex = mActiveCount;
    if (ChaosMode::isActive(ChaosFeature::NetworkScheduling)) {
      newSocketIndex = ChaosMode::randomUint32LessThan(mActiveCount + 1);
      PodMove(mActiveList + newSocketIndex + 1, mActiveList + newSocketIndex,
              mActiveCount - newSocketIndex);
      PodMove(mPollList + newSocketIndex + 2, mPollList + newSocketIndex + 1,
              mActiveCount - newSocketIndex);
    }

    sock->EnsureTimeout(PR_IntervalNow());
    mActiveList[newSocketIndex] = *sock;
    mActiveCount++;

    mPollList[newSocketIndex + 1].fd = sock->mFD;
    mPollList[newSocketIndex + 1].in_flags = sock->mHandler->mPollFlags;
    mPollList[newSocketIndex + 1].out_flags = 0;

    SOCKET_LOG(("  active=%u idle=%u\n", mActiveCount, mIdleCount));
    return NS_OK;
}

void
nsSocketTransportService::RemoveFromPollList(SocketContext *sock)
{
    SOCKET_LOG(("nsSocketTransportService::RemoveFromPollList [handler=%p]\n", sock->mHandler));

    uint32_t index = sock - mActiveList;
    MOZ_ASSERT(index < mActiveListSize, "invalid index");

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
    MOZ_ASSERT(!(static_cast<uint32_t>(sock - mIdleList) < mIdleListSize),
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
    if (toAdd > 100) {
        toAdd = 100;
    } else if (toAdd < 1) {
        MOZ_ASSERT(false, "CanAttachSocket() should prevent this");
        return false;
    }

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
    if (toAdd > 100) {
        toAdd = 100;
    } else if (toAdd < 1) {
        MOZ_ASSERT(false, "CanAttachSocket() should prevent this");
        return false;
    }

    mIdleListSize += toAdd;
    mIdleList = (SocketContext *)
        moz_xrealloc(mIdleList, sizeof(SocketContext) * mIdleListSize);
    return true;
}

PRIntervalTime
nsSocketTransportService::PollTimeout(PRIntervalTime now)
{
    if (mActiveCount == 0) {
        return NS_SOCKET_POLL_TIMEOUT;
    }

    // compute minimum time before any socket timeout expires.
    PRIntervalTime minR = NS_SOCKET_POLL_TIMEOUT;
    for (uint32_t i=0; i<mActiveCount; ++i) {
        const SocketContext &s = mActiveList[i];
        PRIntervalTime r = s.TimeoutIn(now);
        if (r < minR) {
            minR = r;
        }
    }
    if (minR == NS_SOCKET_POLL_TIMEOUT) {
        SOCKET_LOG(("poll timeout: none\n"));
        return NS_SOCKET_POLL_TIMEOUT;
    }
    SOCKET_LOG(("poll timeout: %" PRIu32 "\n", PR_IntervalToSeconds(minR)));
    return minR;
}

int32_t
nsSocketTransportService::Poll(TimeDuration *pollDuration,
                               PRIntervalTime ts)
{
    PRPollDesc *pollList;
    uint32_t pollCount;
    PRIntervalTime pollTimeout;
    *pollDuration = nullptr;

    // If there are pending events for this thread then
    // DoPollIteration() should service the network without blocking.
    bool pendingEvents = false;
    mRawThread->HasPendingEvents(&pendingEvents);

    if (mPollList[0].fd) {
        mPollList[0].out_flags = 0;
        pollList = mPollList;
        pollCount = mActiveCount + 1;
        pollTimeout = pendingEvents ? PR_INTERVAL_NO_WAIT : PollTimeout(ts);
    }
    else {
        // no pollable event, so busy wait...
        pollCount = mActiveCount;
        if (pollCount)
            pollList = &mPollList[1];
        else
            pollList = nullptr;
        pollTimeout =
            pendingEvents ? PR_INTERVAL_NO_WAIT : PR_MillisecondsToInterval(25);
    }

    if ((ts - mLastNetworkLinkChangeTime) < mNetworkLinkChangeBusyWaitPeriod) {
        // Being here means we are few seconds after a network change has
        // been detected.
        PRIntervalTime to = mNetworkLinkChangeBusyWaitTimeout;
        if (to) {
            pollTimeout = std::min(to, pollTimeout);
            SOCKET_LOG(("  timeout shorthened after network change event"));
        }
    }

    TimeStamp pollStart;
    if (mTelemetryEnabledPref) {
        pollStart = TimeStamp::NowLoRes();
    }

    SOCKET_LOG(("    timeout = %i milliseconds\n",
         PR_IntervalToMilliseconds(pollTimeout)));

    int32_t rv = PR_Poll(pollList, pollCount, pollTimeout);

    if (mTelemetryEnabledPref && !pollStart.IsNull()) {
        *pollDuration = TimeStamp::NowLoRes() - pollStart;
    }

    SOCKET_LOG(("    ...returned after %i milliseconds\n",
         PR_IntervalToMilliseconds(PR_IntervalNow() - ts)));

    return rv;
}

//-----------------------------------------------------------------------------
// xpcom api

NS_IMPL_ISUPPORTS(nsSocketTransportService,
                  nsISocketTransportService,
                  nsIRoutedSocketTransportService,
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

    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewNamedThread("Socket Thread", getter_AddRefs(thread), this);
    if (NS_FAILED(rv)) return rv;

    {
        MutexAutoLock lock(mLock);
        // Install our mThread, protecting against concurrent readers
        thread.swap(mThread);
    }

    nsCOMPtr<nsIPrefBranch> tmpPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (tmpPrefService) {
        tmpPrefService->AddObserver(SEND_BUFFER_PREF, this, false);
        tmpPrefService->AddObserver(KEEPALIVE_ENABLED_PREF, this, false);
        tmpPrefService->AddObserver(KEEPALIVE_IDLE_TIME_PREF, this, false);
        tmpPrefService->AddObserver(KEEPALIVE_RETRY_INTERVAL_PREF, this, false);
        tmpPrefService->AddObserver(KEEPALIVE_PROBE_COUNT_PREF, this, false);
        tmpPrefService->AddObserver(MAX_TIME_BETWEEN_TWO_POLLS, this, false);
        tmpPrefService->AddObserver(TELEMETRY_PREF, this, false);
        tmpPrefService->AddObserver(MAX_TIME_FOR_PR_CLOSE_DURING_SHUTDOWN, this, false);
        tmpPrefService->AddObserver(POLLABLE_EVENT_TIMEOUT, this, false);
    }
    UpdatePrefs();

    nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
    if (obsSvc) {
        obsSvc->AddObserver(this, "profile-initial-state", false);
        obsSvc->AddObserver(this, "last-pb-context-exited", false);
        obsSvc->AddObserver(this, NS_WIDGET_SLEEP_OBSERVER_TOPIC, true);
        obsSvc->AddObserver(this, NS_WIDGET_WAKE_OBSERVER_TOPIC, true);
        obsSvc->AddObserver(this, "xpcom-shutdown-threads", false);
        obsSvc->AddObserver(this, NS_NETWORK_LINK_TOPIC, false);
    }

    mInitialized = true;
    return NS_OK;
}

// called from main thread only
NS_IMETHODIMP
nsSocketTransportService::Shutdown(bool aXpcomShutdown)
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

        if (mPollableEvent) {
            mPollableEvent->Signal();
        }
    }

    if (!aXpcomShutdown) {
        return ShutdownThread();
    }

    return NS_OK;
}

nsresult
nsSocketTransportService::ShutdownThread()
{
    SOCKET_LOG(("nsSocketTransportService::ShutdownThread\n"));

    NS_ENSURE_STATE(NS_IsMainThread());

    if (!mInitialized || !mShuttingDown)
        return NS_OK;

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
        obsSvc->RemoveObserver(this, NS_WIDGET_SLEEP_OBSERVER_TOPIC);
        obsSvc->RemoveObserver(this, NS_WIDGET_WAKE_OBSERVER_TOPIC);
        obsSvc->RemoveObserver(this, "xpcom-shutdown-threads");
        obsSvc->RemoveObserver(this, NS_NETWORK_LINK_TOPIC);
    }

    if (mAfterWakeUpTimer) {
        mAfterWakeUpTimer->Cancel();
        mAfterWakeUpTimer = nullptr;
    }

    IOActivityMonitor::Shutdown();

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
    if (mPollableEvent) {
        mPollableEvent->Signal();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::GetKeepaliveIdleTime(int32_t *aKeepaliveIdleTimeS)
{
    MOZ_ASSERT(aKeepaliveIdleTimeS);
    if (NS_WARN_IF(!aKeepaliveIdleTimeS)) {
        return NS_ERROR_NULL_POINTER;
    }
    *aKeepaliveIdleTimeS = mKeepaliveIdleTimeS;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::GetKeepaliveRetryInterval(int32_t *aKeepaliveRetryIntervalS)
{
    MOZ_ASSERT(aKeepaliveRetryIntervalS);
    if (NS_WARN_IF(!aKeepaliveRetryIntervalS)) {
        return NS_ERROR_NULL_POINTER;
    }
    *aKeepaliveRetryIntervalS = mKeepaliveRetryIntervalS;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::GetKeepaliveProbeCount(int32_t *aKeepaliveProbeCount)
{
    MOZ_ASSERT(aKeepaliveProbeCount);
    if (NS_WARN_IF(!aKeepaliveProbeCount)) {
        return NS_ERROR_NULL_POINTER;
    }
    *aKeepaliveProbeCount = mKeepaliveProbeCount;
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
    return CreateRoutedTransport(types, typeCount, host, port, NS_LITERAL_CSTRING(""), 0,
                                 proxyInfo, result);
}

NS_IMETHODIMP
nsSocketTransportService::CreateRoutedTransport(const char **types,
                                                uint32_t typeCount,
                                                const nsACString &host,
                                                int32_t port,
                                                const nsACString &hostRoute,
                                                int32_t portRoute,
                                                nsIProxyInfo *proxyInfo,
                                                nsISocketTransport **result)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(port >= 0 && port <= 0xFFFF, NS_ERROR_ILLEGAL_VALUE);

    RefPtr<nsSocketTransport> trans = new nsSocketTransport();
    nsresult rv = trans->Init(types, typeCount, host, port, hostRoute, portRoute, proxyInfo);
    if (NS_FAILED(rv)) {
        return rv;
    }

    trans.forget(result);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::CreateUnixDomainTransport(nsIFile *aPath,
                                                    nsISocketTransport **result)
{
#ifdef XP_UNIX
    nsresult rv;

    NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

    nsAutoCString path;
    rv = aPath->GetNativePath(path);
    if (NS_FAILED(rv))
        return rv;

    RefPtr<nsSocketTransport> trans = new nsSocketTransport();

    rv = trans->InitWithFilename(path.get());
    if (NS_FAILED(rv))
        return rv;

    trans.forget(result);
    return NS_OK;
#else
    return NS_ERROR_SOCKET_ADDRESS_NOT_SUPPORTED;
#endif
}

NS_IMETHODIMP
nsSocketTransportService::OnDispatchedEvent()
{
#ifndef XP_WIN
    // On windows poll can hang and this became worse when we introduced the
    // patch for bug 698882 (see also bug 1292181), therefore we reverted the
    // behavior on windows to be as before bug 698882, e.g. write to the socket
    // also if an event dispatch is on the socket thread and writing to the
    // socket for each event.
    if (OnSocketThread()) {
        // this check is redundant to one done inside ::Signal(), but
        // we can do it here and skip obtaining the lock - given that
        // this is a relatively common occurance its worth the
        // redundant code
        SOCKET_LOG(("OnDispatchedEvent Same Thread Skip Signal\n"));
        return NS_OK;
    }
#else
    if (gIOService->IsNetTearingDown()) {
        // Poll can hang sometimes. If we are in shutdown, we are going to
        // start a watchdog. If we do not exit poll within
        // REPAIR_POLLABLE_EVENT_TIME signal a pollable event again.
        StartPollWatchdog();
    }
#endif

    MutexAutoLock lock(mLock);
    if (mPollableEvent) {
        mPollableEvent->Signal();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::OnProcessNextEvent(nsIThreadInternal *thread,
                                             bool mayWait)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransportService::AfterProcessNextEvent(nsIThreadInternal* thread,
                                                bool eventWasProcessed)
{
    return NS_OK;
}

void
nsSocketTransportService::MarkTheLastElementOfPendingQueue()
{
    mServingPendingQueue = false;
}

NS_IMETHODIMP
nsSocketTransportService::Run()
{
    SOCKET_LOG(("STS thread init %d sockets\n", gMaxCount));

#if defined(XP_WIN)
    // see bug 1361495, gethostname() triggers winsock initialization.
    // so do it here (on parent and child) to protect against it being done first
    // accidentally on the main thread.. especially via PR_GetSystemInfo(). This
    // will also improve latency of first real winsock operation
    // ..
    // If STS-thread is no longer needed this should still be run before exiting

    char ignoredStackBuffer[255];
    Unused << gethostname(ignoredStackBuffer, 255);
#endif

    psm::InitializeSSLServerCertVerificationThreads();

    gSocketThread = PR_GetCurrentThread();

    {
        MutexAutoLock lock(mLock);
        mPollableEvent.reset(new PollableEvent());
        //
        // NOTE: per bug 190000, this failure could be caused by Zone-Alarm
        // or similar software.
        //
        // NOTE: per bug 191739, this failure could also be caused by lack
        // of a loopback device on Windows and OS/2 platforms (it creates
        // a loopback socket pair on these platforms to implement a pollable
        // event object).  if we can't create a pollable event, then we'll
        // have to "busy wait" to implement the socket event queue :-(
        //
        if (!mPollableEvent->Valid()) {
            mPollableEvent = nullptr;
            NS_WARNING("running socket transport thread without a pollable event");
            SOCKET_LOG(("running socket transport thread without a pollable event"));
        }

        mPollList[0].fd = mPollableEvent ? mPollableEvent->PollableFD() : nullptr;
        mPollList[0].in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
        mPollList[0].out_flags = 0;
    }

    mRawThread = NS_GetCurrentThread();

    // hook ourselves up to observe event processing for this thread
    nsCOMPtr<nsIThreadInternal> threadInt = do_QueryInterface(mRawThread);
    threadInt->SetObserver(this);

    // make sure the pseudo random number generator is seeded on this thread
    srand(static_cast<unsigned>(PR_Now()));

    // For the calculation of the duration of the last cycle (i.e. the last for-loop
    // iteration before shutdown).
    TimeStamp startOfCycleForLastCycleCalc;
    int numberOfPendingEventsLastCycle;

    // For measuring of the poll iteration duration without time spent blocked
    // in poll().
    TimeStamp pollCycleStart;
    // Time blocked in poll().
    TimeDuration singlePollDuration;

    // For calculating the time needed for a new element to run.
    TimeStamp startOfIteration;
    TimeStamp startOfNextIteration;
    int numberOfPendingEvents;

    // If there is too many pending events queued, we will run some poll()
    // between them and the following variable is cumulative time spent
    // blocking in poll().
    TimeDuration pollDuration;

    for (;;) {
        bool pendingEvents = false;

        numberOfPendingEvents = 0;
        numberOfPendingEventsLastCycle = 0;
        if (mTelemetryEnabledPref) {
            startOfCycleForLastCycleCalc = TimeStamp::NowLoRes();
            startOfNextIteration = TimeStamp::NowLoRes();
        }
        pollDuration = nullptr;

        do {
            if (mTelemetryEnabledPref) {
                pollCycleStart = TimeStamp::NowLoRes();
            }

            DoPollIteration(&singlePollDuration);

            if (mTelemetryEnabledPref && !pollCycleStart.IsNull()) {
                Telemetry::Accumulate(Telemetry::STS_POLL_BLOCK_TIME,
                                      singlePollDuration.ToMilliseconds());
                Telemetry::AccumulateTimeDelta(
                    Telemetry::STS_POLL_CYCLE,
                    pollCycleStart + singlePollDuration,
                    TimeStamp::NowLoRes());
                pollDuration += singlePollDuration;
            }

            mRawThread->HasPendingEvents(&pendingEvents);
            if (pendingEvents) {
                if (!mServingPendingQueue) {
                  nsresult rv = Dispatch(
                    NewRunnableMethod("net::nsSocketTransportService::"
                                      "MarkTheLastElementOfPendingQueue",
                                      this,
                                      &nsSocketTransportService::
                                        MarkTheLastElementOfPendingQueue),
                    nsIEventTarget::DISPATCH_NORMAL);
                  if (NS_FAILED(rv)) {
                    NS_WARNING("Could not dispatch a new event on the "
                               "socket thread.");
                    } else {
                        mServingPendingQueue = true;
                    }

                    if (mTelemetryEnabledPref) {
                        startOfIteration = startOfNextIteration;
                        // Everything that comes after this point will
                        // be served in the next iteration. If no even
                        // arrives, startOfNextIteration will be reset at the
                        // beginning of each for-loop.
                        startOfNextIteration = TimeStamp::NowLoRes();
                    }
                }
                TimeStamp eventQueueStart = TimeStamp::NowLoRes();
                do {
                    NS_ProcessNextEvent(mRawThread);
                    numberOfPendingEvents++;
                    pendingEvents = false;
                    mRawThread->HasPendingEvents(&pendingEvents);
                } while (pendingEvents && mServingPendingQueue &&
                         ((TimeStamp::NowLoRes() -
                           eventQueueStart).ToMilliseconds() <
                          mMaxTimePerPollIter));

                if (mTelemetryEnabledPref && !mServingPendingQueue &&
                    !startOfIteration.IsNull()) {
                    Telemetry::AccumulateTimeDelta(
                        Telemetry::STS_POLL_AND_EVENTS_CYCLE,
                        startOfIteration + pollDuration,
                        TimeStamp::NowLoRes());

                    Telemetry::Accumulate(
                        Telemetry::STS_NUMBER_OF_PENDING_EVENTS,
                        numberOfPendingEvents);

                    numberOfPendingEventsLastCycle += numberOfPendingEvents;
                    numberOfPendingEvents = 0;
                    pollDuration = nullptr;
                }
            }
        } while (pendingEvents);

        bool goingOffline = false;
        // now that our event queue is empty, check to see if we should exit
        {
            MutexAutoLock lock(mLock);
            if (mShuttingDown) {
                if (mTelemetryEnabledPref &&
                    !startOfCycleForLastCycleCalc.IsNull()) {
                    Telemetry::Accumulate(
                        Telemetry::STS_NUMBER_OF_PENDING_EVENTS_IN_THE_LAST_CYCLE,
                        numberOfPendingEventsLastCycle);
                    Telemetry::AccumulateTimeDelta(
                        Telemetry::STS_POLL_AND_EVENT_THE_LAST_CYCLE,
                        startOfCycleForLastCycleCalc,
                        TimeStamp::NowLoRes());
                }
                break;
            }
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
    NS_ProcessPendingEvents(mRawThread);

    // Stopping the SLL threads can generate new events, so we need to
    // process them before nulling out gSocketThread, otherwise we can get
    // !onSocketThread assertions.
    psm::StopSSLServerCertVerificationThreads();
    NS_ProcessPendingEvents(mRawThread);

    gSocketThread = nullptr;

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
nsSocketTransportService::DoPollIteration(TimeDuration *pollDuration)
{
    SOCKET_LOG(("STS poll iter\n"));

    PRIntervalTime now = PR_IntervalNow();

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
        SOCKET_LOG(("  active [%u] { handler=%p condition=%" PRIx32 " pollflags=%hu }\n", i,
            mActiveList[i].mHandler,
            static_cast<uint32_t>(mActiveList[i].mHandler->mCondition),
            mActiveList[i].mHandler->mPollFlags));
        //---
        if (NS_FAILED(mActiveList[i].mHandler->mCondition)) {
            DetachSocket(mActiveList, &mActiveList[i]);
        } else {
            uint16_t in_flags = mActiveList[i].mHandler->mPollFlags;
            if (in_flags == 0) {
                MoveToIdleList(&mActiveList[i]);
            } else {
                // update poll flags
                mPollList[i+1].in_flags = in_flags;
                mPollList[i+1].out_flags = 0;
                mActiveList[i].EnsureTimeout(now);
            }
        }
    }
    for (i=count-1; i>=0; --i) {
        //---
        SOCKET_LOG(("  idle [%u] { handler=%p condition=%" PRIx32 " pollflags=%hu }\n", i,
            mIdleList[i].mHandler,
            static_cast<uint32_t>(mIdleList[i].mHandler->mCondition),
            mIdleList[i].mHandler->mPollFlags));
        //---
        if (NS_FAILED(mIdleList[i].mHandler->mCondition))
            DetachSocket(mIdleList, &mIdleList[i]);
        else if (mIdleList[i].mHandler->mPollFlags != 0)
            MoveToPollList(&mIdleList[i]);
    }

    {
        MutexAutoLock lock(mLock);
        if (mPollableEvent) {
            // we want to make sure the timeout is measured from the time
            // we enter poll().  This method resets the timestamp to 'now',
            // if we were first signalled between leaving poll() and here.
            // If we didn't do this and processing events took longer than
            // the allowed signal timeout, we would detect it as a
            // false-positive.  AdjustFirstSignalTimestamp is then a no-op
            // until mPollableEvent->Clear() is called.
            mPollableEvent->AdjustFirstSignalTimestamp();
        }
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
    int32_t n = 0;
    *pollDuration = nullptr;

    if (!gIOService->IsNetTearingDown()) {
        // Let's not do polling during shutdown.
#if defined(XP_WIN)
        StartPolling();
#endif
        n = Poll(pollDuration, now);
#if defined(XP_WIN)
        EndPolling();
#endif
    }

    now = PR_IntervalNow();

    if (n < 0) {
        SOCKET_LOG(("  PR_Poll error [%d] os error [%d]\n", PR_GetError(),
                    PR_GetOSError()));
    }
    else {
        //
        // service "active" sockets...
        //
        uint32_t numberOfOnSocketReadyCalls = 0;
        for (i=0; i<int32_t(mActiveCount); ++i) {
            PRPollDesc &desc = mPollList[i+1];
            SocketContext &s = mActiveList[i];
            if (n > 0 && desc.out_flags != 0) {
#ifdef MOZ_TASK_TRACER
		tasktracer::AutoSourceEvent taskTracerEvent(tasktracer::SourceEventType::SocketIO);
#endif
                s.DisengageTimeout();
                s.mHandler->OnSocketReady(desc.fd, desc.out_flags);
                numberOfOnSocketReadyCalls++;
            } else if (s.IsTimedOut(now)) {
#ifdef MOZ_TASK_TRACER
		tasktracer::AutoSourceEvent taskTracerEvent(tasktracer::SourceEventType::SocketIO);
#endif
                SOCKET_LOG(("socket %p timed out", s.mHandler));
                s.DisengageTimeout();
                s.mHandler->OnSocketReady(desc.fd, -1);
                numberOfOnSocketReadyCalls++;
            }
        }
        if (mTelemetryEnabledPref) {
            Telemetry::Accumulate(
                Telemetry::STS_NUMBER_OF_ONSOCKETREADY_CALLS,
                numberOfOnSocketReadyCalls);
        }

        //
        // check for "dead" sockets and remove them (need to do this in
        // reverse order obviously).
        //
        for (i=mActiveCount-1; i>=0; --i) {
            if (NS_FAILED(mActiveList[i].mHandler->mCondition))
                DetachSocket(mActiveList, &mActiveList[i]);
        }

        {
            MutexAutoLock lock(mLock);
            // acknowledge pollable event (should not block)
            if (n != 0 &&
                (mPollList[0].out_flags & (PR_POLL_READ | PR_POLL_EXCEPT)) &&
                mPollableEvent &&
                ((mPollList[0].out_flags & PR_POLL_EXCEPT) || !mPollableEvent->Clear())) {
                // On Windows, the TCP loopback connection in the
                // pollable event may become broken when a laptop
                // switches between wired and wireless networks or
                // wakes up from hibernation.  We try to create a
                // new pollable event.  If that fails, we fall back
                // on "busy wait".
                TryRepairPollableEvent();
            }

            if (mPollableEvent &&
                !mPollableEvent->IsSignallingAlive(mPollableEventTimeout)) {
                SOCKET_LOG(("Pollable event signalling failed/timed out"));
                TryRepairPollableEvent();
            }
        }
    }

    return NS_OK;
}

void
nsSocketTransportService::UpdateSendBufferPref(nsIPrefBranch *pref)
{
    int32_t bufferSize;

    // If the pref is set, honor it. 0 means use OS defaults.
    nsresult rv = pref->GetIntPref(SEND_BUFFER_PREF, &bufferSize);
    if (NS_SUCCEEDED(rv)) {
        mSendBufferSize = bufferSize;
        return;
    }

#if defined(XP_WIN)
    mSendBufferSize = 131072 * 4;
#endif
}

nsresult
nsSocketTransportService::UpdatePrefs()
{
    mSendBufferSize = 0;

    nsCOMPtr<nsIPrefBranch> tmpPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (tmpPrefService) {
        UpdateSendBufferPref(tmpPrefService);

        // Default TCP Keepalive Values.
        int32_t keepaliveIdleTimeS;
        nsresult rv = tmpPrefService->GetIntPref(KEEPALIVE_IDLE_TIME_PREF,
                                        &keepaliveIdleTimeS);
        if (NS_SUCCEEDED(rv))
            mKeepaliveIdleTimeS = clamped(keepaliveIdleTimeS,
                                          1, kMaxTCPKeepIdle);

        int32_t keepaliveRetryIntervalS;
        rv = tmpPrefService->GetIntPref(KEEPALIVE_RETRY_INTERVAL_PREF,
                                        &keepaliveRetryIntervalS);
        if (NS_SUCCEEDED(rv))
            mKeepaliveRetryIntervalS = clamped(keepaliveRetryIntervalS,
                                               1, kMaxTCPKeepIntvl);

        int32_t keepaliveProbeCount;
        rv = tmpPrefService->GetIntPref(KEEPALIVE_PROBE_COUNT_PREF,
                                        &keepaliveProbeCount);
        if (NS_SUCCEEDED(rv))
            mKeepaliveProbeCount = clamped(keepaliveProbeCount,
                                           1, kMaxTCPKeepCount);
        bool keepaliveEnabled = false;
        rv = tmpPrefService->GetBoolPref(KEEPALIVE_ENABLED_PREF,
                                         &keepaliveEnabled);
        if (NS_SUCCEEDED(rv) && keepaliveEnabled != mKeepaliveEnabledPref) {
            mKeepaliveEnabledPref = keepaliveEnabled;
            OnKeepaliveEnabledPrefChange();
        }

        int32_t maxTimePref;
        rv = tmpPrefService->GetIntPref(MAX_TIME_BETWEEN_TWO_POLLS,
                                        &maxTimePref);
        if (NS_SUCCEEDED(rv) && maxTimePref >= 0) {
            mMaxTimePerPollIter = maxTimePref;
        }

        int32_t pollBusyWaitPeriod;
        rv = tmpPrefService->GetIntPref(POLL_BUSY_WAIT_PERIOD, &pollBusyWaitPeriod);
        if (NS_SUCCEEDED(rv) && pollBusyWaitPeriod > 0) {
            mNetworkLinkChangeBusyWaitPeriod = PR_SecondsToInterval(pollBusyWaitPeriod);
        }

        int32_t pollBusyWaitPeriodTimeout;
        rv = tmpPrefService->GetIntPref(POLL_BUSY_WAIT_PERIOD_TIMEOUT, &pollBusyWaitPeriodTimeout);
        if (NS_SUCCEEDED(rv) && pollBusyWaitPeriodTimeout > 0) {
            mNetworkLinkChangeBusyWaitTimeout = PR_SecondsToInterval(pollBusyWaitPeriodTimeout);
        }

        bool telemetryPref = false;
        rv = tmpPrefService->GetBoolPref(TELEMETRY_PREF,
                                         &telemetryPref);
        if (NS_SUCCEEDED(rv)) {
            mTelemetryEnabledPref = telemetryPref;
        }

        int32_t maxTimeForPrClosePref;
        rv = tmpPrefService->GetIntPref(MAX_TIME_FOR_PR_CLOSE_DURING_SHUTDOWN,
                                        &maxTimeForPrClosePref);
        if (NS_SUCCEEDED(rv) && maxTimeForPrClosePref >=0) {
            mMaxTimeForPrClosePref = PR_MillisecondsToInterval(maxTimeForPrClosePref);
        }

        int32_t pollableEventTimeout;
        rv = tmpPrefService->GetIntPref(POLLABLE_EVENT_TIMEOUT,
                                        &pollableEventTimeout);
        if (NS_SUCCEEDED(rv) && pollableEventTimeout >= 0) {
            MutexAutoLock lock(mLock);
            mPollableEventTimeout = TimeDuration::FromSeconds(pollableEventTimeout);
        }
    }

    return NS_OK;
}

void
nsSocketTransportService::OnKeepaliveEnabledPrefChange()
{
    // Dispatch to socket thread if we're not executing there.
    if (!OnSocketThread()) {
      gSocketTransportService->Dispatch(
        NewRunnableMethod(
          "net::nsSocketTransportService::OnKeepaliveEnabledPrefChange",
          this,
          &nsSocketTransportService::OnKeepaliveEnabledPrefChange),
        NS_DISPATCH_NORMAL);
      return;
    }

    SOCKET_LOG(("nsSocketTransportService::OnKeepaliveEnabledPrefChange %s",
                mKeepaliveEnabledPref ? "enabled" : "disabled"));

    // Notify each socket that keepalive has been en/disabled globally.
    for (int32_t i = mActiveCount - 1; i >= 0; --i) {
        NotifyKeepaliveEnabledPrefChange(&mActiveList[i]);
    }
    for (int32_t i = mIdleCount - 1; i >= 0; --i) {
        NotifyKeepaliveEnabledPrefChange(&mIdleList[i]);
    }
}

void
nsSocketTransportService::NotifyKeepaliveEnabledPrefChange(SocketContext *sock)
{
    MOZ_ASSERT(sock, "SocketContext cannot be null!");
    MOZ_ASSERT(sock->mHandler, "SocketContext does not have a handler!");

    if (!sock || !sock->mHandler) {
        return;
    }

#ifdef MOZ_TASK_TRACER
    tasktracer::AutoSourceEvent taskTracerEvent(tasktracer::SourceEventType::SocketIO);
#endif
    sock->mHandler->OnKeepaliveEnabledPrefChange(mKeepaliveEnabledPref);
}

NS_IMETHODIMP
nsSocketTransportService::Observe(nsISupports *subject,
                                  const char *topic,
                                  const char16_t *data)
{
    SOCKET_LOG(("nsSocketTransportService::Observe topic=%s", topic));

    if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        UpdatePrefs();
        return NS_OK;
    }

    if (!strcmp(topic, "profile-initial-state")) {
        if (!Preferences::GetBool(IO_ACTIVITY_ENABLED_PREF, false)) {
          return NS_OK;
        }
        return net::IOActivityMonitor::Init(Preferences::GetInt(IO_ACTIVITY_INTERVAL_PREF, 0));
    }

    if (!strcmp(topic, "last-pb-context-exited")) {
      nsCOMPtr<nsIRunnable> ev = NewRunnableMethod(
        "net::nsSocketTransportService::ClosePrivateConnections",
        this,
        &nsSocketTransportService::ClosePrivateConnections);
      nsresult rv = Dispatch(ev, nsIEventTarget::DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!strcmp(topic, NS_TIMER_CALLBACK_TOPIC)) {
        nsCOMPtr<nsITimer> timer = do_QueryInterface(subject);
        if (timer == mAfterWakeUpTimer) {
            mAfterWakeUpTimer = nullptr;
            mSleepPhase = false;
        }

#if defined(XP_WIN)
        if (timer == mPollRepairTimer) {
            DoPollRepair();
        }
#endif

    } else if (!strcmp(topic, NS_WIDGET_SLEEP_OBSERVER_TOPIC)) {
        mSleepPhase = true;
        if (mAfterWakeUpTimer) {
            mAfterWakeUpTimer->Cancel();
            mAfterWakeUpTimer = nullptr;
        }
    } else if (!strcmp(topic, NS_WIDGET_WAKE_OBSERVER_TOPIC)) {
        if (mSleepPhase && !mAfterWakeUpTimer) {
            NS_NewTimerWithObserver(getter_AddRefs(mAfterWakeUpTimer),
                                    this, 2000, nsITimer::TYPE_ONE_SHOT);
        }
    } else if (!strcmp(topic, "xpcom-shutdown-threads")) {
        ShutdownThread();
    } else if (!strcmp(topic, NS_NETWORK_LINK_TOPIC)) {
        mLastNetworkLinkChangeTime = PR_IntervalNow();
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

    ClearPrivateSSLState();
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
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

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
    static_assert(SOCKET_LIMIT_MIN >= 32U, "Minimum Socket Limit is >= 32");
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

    Telemetry::Accumulate(Telemetry::NETWORK_PROBE_MAXCOUNT, gMaxCount);
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
    // query that and raise it if needed.

    struct rlimit rlimitData;
    if (getrlimit(RLIMIT_NOFILE, &rlimitData) == -1) // rlimit broken - use min
        return PR_SUCCESS;

    if (rlimitData.rlim_cur >= SOCKET_LIMIT_TARGET) { // larger than target!
        gMaxCount = SOCKET_LIMIT_TARGET;
        return PR_SUCCESS;
    }

    int32_t maxallowed = rlimitData.rlim_max;
    if ((uint32_t)maxallowed <= SOCKET_LIMIT_MIN) {
        return PR_SUCCESS; // so small treat as if rlimit is broken
    }

    if ((maxallowed == -1) || // no hard cap - ok to set target
        ((uint32_t)maxallowed >= SOCKET_LIMIT_TARGET)) {
        maxallowed = SOCKET_LIMIT_TARGET;
    }

    rlimitData.rlim_cur = maxallowed;
    setrlimit(RLIMIT_NOFILE, &rlimitData);
    if ((getrlimit(RLIMIT_NOFILE, &rlimitData) != -1) &&
        (rlimitData.rlim_cur > SOCKET_LIMIT_MIN)) {
        gMaxCount = rlimitData.rlim_cur;
    }

#elif defined(XP_WIN) && !defined(WIN_CE)
    // >= XP is confirmed to have at least 1000
    static_assert(SOCKET_LIMIT_TARGET <= 1000, "SOCKET_LIMIT_TARGET max value is 1000");
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

    PRFileDesc *idLayer = PR_GetIdentitiesLayer(aFD, PR_NSPR_IO_LAYER);

    NS_ENSURE_TRUE_VOID(idLayer);

    bool tcp = PR_GetDescType(idLayer) == PR_DESC_SOCKET_TCP;

    PRNetAddr peer_addr;
    PodZero(&peer_addr);
    PRStatus rv = PR_GetPeerName(aFD, &peer_addr);
    if (rv != PR_SUCCESS)
       return;

    char host[64] = {0};
    rv = PR_NetAddrToString(&peer_addr, host, sizeof(host));
    if (rv != PR_SUCCESS)
       return;

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
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    for (uint32_t i = 0; i < mActiveCount; i++)
        AnalyzeConnection(data, &mActiveList[i], true);
    for (uint32_t i = 0; i < mIdleCount; i++)
        AnalyzeConnection(data, &mIdleList[i], false);
}

#if defined(XP_WIN)
void
nsSocketTransportService::StartPollWatchdog()
{
    // Start off the timer from a runnable off of the main thread in order to
    // avoid a deadlock, see bug 1370448.
    RefPtr<nsSocketTransportService> self(this);
    NS_DispatchToMainThread(NS_NewRunnableFunction("nsSocketTransportService::StartPollWatchdog",
                                                   [self] {
         MutexAutoLock lock(self->mLock);

         // Poll can hang sometimes. If we are in shutdown, we are going to start a
         // watchdog. If we do not exit poll within REPAIR_POLLABLE_EVENT_TIME
         // signal a pollable event again.
         MOZ_ASSERT(gIOService->IsNetTearingDown());
         if (self->mPolling && !self->mPollRepairTimer) {
             NS_NewTimerWithObserver(getter_AddRefs(self->mPollRepairTimer),
                                     self, REPAIR_POLLABLE_EVENT_TIME,
                                     nsITimer::TYPE_REPEATING_SLACK);
         }
    }));
}

void
nsSocketTransportService::DoPollRepair()
{
    MutexAutoLock lock(mLock);
    if (mPolling && mPollableEvent) {
        mPollableEvent->Signal();
    } else if (mPollRepairTimer) {
        mPollRepairTimer->Cancel();
    }
}

void
nsSocketTransportService::StartPolling()
{
    MutexAutoLock lock(mLock);
    mPolling = true;
}

void
nsSocketTransportService::EndPolling()
{
    MutexAutoLock lock(mLock);
    mPolling = false;
    if (mPollRepairTimer) {
        mPollRepairTimer->Cancel();
    }
}

#endif

void nsSocketTransportService::TryRepairPollableEvent()
{
    mLock.AssertCurrentThreadOwns();

    NS_WARNING("Trying to repair mPollableEvent");
    mPollableEvent.reset(new PollableEvent());
    if (!mPollableEvent->Valid()) {
        mPollableEvent = nullptr;
    }
    SOCKET_LOG(("running socket transport thread without "
                "a pollable event now valid=%d", !!mPollableEvent));
    mPollList[0].fd = mPollableEvent ? mPollableEvent->PollableFD() : nullptr;
    mPollList[0].in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
    mPollList[0].out_flags = 0;
}

} // namespace net
} // namespace mozilla
