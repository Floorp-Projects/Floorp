/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSocketTransportService2_h__
#define nsSocketTransportService2_h__

#include "nsPISocketTransportService.h"
#include "nsIThreadInternal.h"
#include "nsIRunnable.h"
#include "nsCOMPtr.h"
#include "prinrval.h"
#include "mozilla/Logging.h"
#include "prinit.h"
#include "nsIObserver.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Mutex.h"
#include "mozilla/net/DashboardTypes.h"
#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"
#include "nsITimer.h"
#include "mozilla/UniquePtr.h"
#include "PollableEvent.h"

class nsASocketHandler;
struct PRPollDesc;
class nsIPrefBranch;

//-----------------------------------------------------------------------------

namespace mozilla {
namespace net {

//
// set MOZ_LOG=nsSocketTransport:5
//
extern LazyLogModule gSocketTransportLog;
#define SOCKET_LOG(args)     MOZ_LOG(gSocketTransportLog, LogLevel::Debug, args)
#define SOCKET_LOG_ENABLED() MOZ_LOG_TEST(gSocketTransportLog, LogLevel::Debug)

//
// set MOZ_LOG=UDPSocket:5
//
extern LazyLogModule gUDPSocketLog;
#define UDPSOCKET_LOG(args)     MOZ_LOG(gUDPSocketLog, LogLevel::Debug, args)
#define UDPSOCKET_LOG_ENABLED() MOZ_LOG_TEST(gUDPSocketLog, LogLevel::Debug)

//-----------------------------------------------------------------------------

#define NS_SOCKET_POLL_TIMEOUT PR_INTERVAL_NO_TIMEOUT

//-----------------------------------------------------------------------------

// These maximums are borrowed from the linux kernel.
static const int32_t kMaxTCPKeepIdle  = 32767; // ~9 hours.
static const int32_t kMaxTCPKeepIntvl = 32767;
static const int32_t kMaxTCPKeepCount   = 127;
static const int32_t kDefaultTCPKeepCount =
#if defined (XP_WIN)
                                              10; // Hardcoded in Windows.
#elif defined (XP_MACOSX)
                                              8;  // Hardcoded in OSX.
#else
                                              4;  // Specifiable in Linux.
#endif

class LinkedRunnableEvent final : public LinkedListElement<LinkedRunnableEvent>
{
public:
  explicit LinkedRunnableEvent(nsIRunnable *event) : mEvent(event) {}
  ~LinkedRunnableEvent() {}

  already_AddRefed<nsIRunnable> TakeEvent()
  {
    return mEvent.forget();
  }
private:
    nsCOMPtr<nsIRunnable> mEvent;
};

//-----------------------------------------------------------------------------

class nsSocketTransportService final : public nsPISocketTransportService
                                     , public nsIEventTarget
                                     , public nsIThreadObserver
                                     , public nsIRunnable
                                     , public nsIObserver
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSPISOCKETTRANSPORTSERVICE
    NS_DECL_NSISOCKETTRANSPORTSERVICE
    NS_DECL_NSIROUTEDSOCKETTRANSPORTSERVICE
    NS_DECL_NSIEVENTTARGET_FULL
    NS_DECL_NSITHREADOBSERVER
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIOBSERVER

    nsSocketTransportService();

    // Max Socket count may need to get initialized/used by nsHttpHandler
    // before this class is initialized.
    static uint32_t gMaxCount;
    static PRCallOnceType gMaxCountInitOnce;
    static PRStatus DiscoverMaxCount();

    bool CanAttachSocket();

    // Called by the networking dashboard on the socket thread only
    // Fills the passed array with socket information
    void GetSocketConnections(nsTArray<SocketInfo> *);
    uint64_t GetSentBytes() { return mSentBytesCount; }
    uint64_t GetReceivedBytes() { return mReceivedBytesCount; }

    // Returns true if keepalives are enabled in prefs.
    bool IsKeepaliveEnabled() { return mKeepaliveEnabledPref; }

    bool IsTelemetryEnabledAndNotSleepPhase() { return mTelemetryEnabledPref &&
                                                       !mSleepPhase; }
    PRIntervalTime MaxTimeForPrClosePref() {return mMaxTimeForPrClosePref; }

protected:

    virtual ~nsSocketTransportService();

private:

    //-------------------------------------------------------------------------
    // misc (any thread)
    //-------------------------------------------------------------------------

    nsCOMPtr<nsIThread>      mThread;    // protected by mLock
    UniquePtr<PollableEvent> mPollableEvent;

    // Returns mThread, protecting the get-and-addref with mLock
    already_AddRefed<nsIThread> GetThreadSafely();

    //-------------------------------------------------------------------------
    // initialization and shutdown (any thread)
    //-------------------------------------------------------------------------

    Mutex         mLock;
    bool          mInitialized;
    bool          mShuttingDown;
                            // indicates whether we are currently in the
                            // process of shutting down
    bool          mOffline;
    bool          mGoingOffline;

    // Detaches all sockets.
    void Reset(bool aGuardLocals);

    nsresult ShutdownThread();

    //-------------------------------------------------------------------------
    // socket lists (socket thread only)
    //
    // only "active" sockets are on the poll list.  the active list is kept
    // in sync with the poll list such that:
    //
    //   mActiveList[k].mFD == mPollList[k+1].fd
    //
    // where k=0,1,2,...
    //-------------------------------------------------------------------------

    struct SocketContext
    {
        PRFileDesc       *mFD;
        nsASocketHandler *mHandler;
        uint16_t          mElapsedTime;  // time elapsed w/o activity
    };

    SocketContext *mActiveList;                   /* mListSize entries */
    SocketContext *mIdleList;                     /* mListSize entries */
    nsIThread     *mRawThread;

    uint32_t mActiveListSize;
    uint32_t mIdleListSize;
    uint32_t mActiveCount;
    uint32_t mIdleCount;

    nsresult DetachSocket(SocketContext *, SocketContext *);
    nsresult AddToIdleList(SocketContext *);
    nsresult AddToPollList(SocketContext *);
    void RemoveFromIdleList(SocketContext *);
    void RemoveFromPollList(SocketContext *);
    void MoveToIdleList(SocketContext *sock);
    void MoveToPollList(SocketContext *sock);

    bool GrowActiveList();
    bool GrowIdleList();
    void   InitMaxCount();

    // Total bytes number transfered through all the sockets except active ones
    uint64_t mSentBytesCount;
    uint64_t mReceivedBytesCount;
    //-------------------------------------------------------------------------
    // poll list (socket thread only)
    //
    // first element of the poll list is mPollableEvent (or null if the pollable
    // event cannot be created).
    //-------------------------------------------------------------------------

    PRPollDesc *mPollList;                        /* mListSize + 1 entries */

    PRIntervalTime PollTimeout();            // computes ideal poll timeout
    nsresult       DoPollIteration(TimeDuration *pollDuration);
                                             // perfoms a single poll iteration
    int32_t        Poll(uint32_t *interval,
                        TimeDuration *pollDuration);
                                             // calls PR_Poll.  the out param
                                             // interval indicates the poll
                                             // duration in seconds.
                                             // pollDuration is used only for
                                             // telemetry

    //-------------------------------------------------------------------------
    // pending socket queue - see NotifyWhenCanAttachSocket
    //-------------------------------------------------------------------------
    AutoCleanLinkedList<LinkedRunnableEvent> mPendingSocketQueue;

    // Preference Monitor for SendBufferSize and Keepalive prefs.
    nsresult    UpdatePrefs();
    void        UpdateSendBufferPref(nsIPrefBranch *);
    int32_t     mSendBufferSize;
    // Number of seconds of connection is idle before first keepalive ping.
    int32_t     mKeepaliveIdleTimeS;
    // Number of seconds between retries should keepalive pings fail.
    int32_t     mKeepaliveRetryIntervalS;
    // Number of keepalive probes to send.
    int32_t     mKeepaliveProbeCount;
    // True if TCP keepalive is enabled globally.
    bool        mKeepaliveEnabledPref;

    Atomic<bool>                    mServingPendingQueue;
    Atomic<int32_t, Relaxed>        mMaxTimePerPollIter;
    Atomic<bool, Relaxed>           mTelemetryEnabledPref;
    Atomic<PRIntervalTime, Relaxed> mMaxTimeForPrClosePref;

    // Between a computer going to sleep and waking up the PR_*** telemetry
    // will be corrupted - so do not record it.
    Atomic<bool, Relaxed>           mSleepPhase;
    nsCOMPtr<nsITimer>              mAfterWakeUpTimer;

    void OnKeepaliveEnabledPrefChange();
    void NotifyKeepaliveEnabledPrefChange(SocketContext *sock);

    // Socket thread only for dynamically adjusting max socket size
#if defined(XP_WIN)
    void ProbeMaxCount();
#endif
    bool mProbedMaxCount;

    void AnalyzeConnection(nsTArray<SocketInfo> *data,
                           SocketContext *context, bool aActive);

    void ClosePrivateConnections();
    void DetachSocketWithGuard(bool aGuardLocals,
                               SocketContext *socketList,
                               int32_t index);

    void MarkTheLastElementOfPendingQueue();

#if defined(XP_WIN)
    Atomic<bool> mPolling;
    nsCOMPtr<nsITimer> mPollRepairTimer;
    void StartPollWatchdog();
    void DoPollRepair();
    void StartPolling();
    void EndPolling();
#endif
};

extern nsSocketTransportService *gSocketTransportService;
bool OnSocketThread();

} // namespace net
} // namespace mozilla

#endif // !nsSocketTransportService_h__
