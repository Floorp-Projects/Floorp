/* vim:set ts=4 sw=2 sts=2 ci et: */
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
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/net/DashboardTypes.h"
#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"
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
#define SOCKET_LOG(args) MOZ_LOG(gSocketTransportLog, LogLevel::Debug, args)
#define SOCKET_LOG1(args) MOZ_LOG(gSocketTransportLog, LogLevel::Error, args)
#define SOCKET_LOG_ENABLED() MOZ_LOG_TEST(gSocketTransportLog, LogLevel::Debug)

//
// set MOZ_LOG=UDPSocket:5
//
extern LazyLogModule gUDPSocketLog;
#define UDPSOCKET_LOG(args) MOZ_LOG(gUDPSocketLog, LogLevel::Debug, args)
#define UDPSOCKET_LOG_ENABLED() MOZ_LOG_TEST(gUDPSocketLog, LogLevel::Debug)

//-----------------------------------------------------------------------------

#define NS_SOCKET_POLL_TIMEOUT PR_INTERVAL_NO_TIMEOUT

//-----------------------------------------------------------------------------

// These maximums are borrowed from the linux kernel.
static const int32_t kMaxTCPKeepIdle = 32767;  // ~9 hours.
static const int32_t kMaxTCPKeepIntvl = 32767;
static const int32_t kMaxTCPKeepCount = 127;
static const int32_t kDefaultTCPKeepCount =
#if defined(XP_WIN)
    10;  // Hardcoded in Windows.
#elif defined(XP_MACOSX)
    8;  // Hardcoded in OSX.
#else
    4;  // Specifiable in Linux.
#endif

class LinkedRunnableEvent final
    : public LinkedListElement<LinkedRunnableEvent> {
 public:
  explicit LinkedRunnableEvent(nsIRunnable* event) : mEvent(event) {}
  ~LinkedRunnableEvent() = default;

  already_AddRefed<nsIRunnable> TakeEvent() { return mEvent.forget(); }

 private:
  nsCOMPtr<nsIRunnable> mEvent;
};

//-----------------------------------------------------------------------------

class nsSocketTransportService final : public nsPISocketTransportService,
                                       public nsISerialEventTarget,
                                       public nsIThreadObserver,
                                       public nsIRunnable,
                                       public nsIObserver {
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
  void GetSocketConnections(nsTArray<SocketInfo>*);
  uint64_t GetSentBytes() { return mSentBytesCount; }
  uint64_t GetReceivedBytes() { return mReceivedBytesCount; }

  // Returns true if keepalives are enabled in prefs.
  bool IsKeepaliveEnabled() { return mKeepaliveEnabledPref; }

  bool IsTelemetryEnabledAndNotSleepPhase();
  PRIntervalTime MaxTimeForPrClosePref() { return mMaxTimeForPrClosePref; }

  bool IsEsniEnabled() {
    return mEsniEnabled && !mTrustedMitmDetected && !mNotTrustedMitmDetected;
  }

  void SetNotTrustedMitmDetected() { mNotTrustedMitmDetected = true; }

  // According the preference value of `network.socket.forcePort` this method
  // possibly remaps the port number passed as the arg.
  void ApplyPortRemap(uint16_t* aPort);

  // Reads the preference string and updates (rewrites) the mPortRemapping
  // array on the socket thread.  Returns true if the whole pref string was
  // correctly formed.
  bool UpdatePortRemapPreference(nsACString const& aPortMappingPref);

 protected:
  virtual ~nsSocketTransportService();

 private:
  //-------------------------------------------------------------------------
  // misc (any thread)
  //-------------------------------------------------------------------------

  nsCOMPtr<nsIThread> mThread;  // protected by mLock
  UniquePtr<PollableEvent> mPollableEvent;

  // Returns mThread, protecting the get-and-addref with mLock
  already_AddRefed<nsIThread> GetThreadSafely();

  //-------------------------------------------------------------------------
  // initialization and shutdown (any thread)
  //-------------------------------------------------------------------------

  Mutex mLock;
  bool mInitialized;
  bool mShuttingDown;
  // indicates whether we are currently in the
  // process of shutting down
  bool mOffline;
  bool mGoingOffline;

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

  struct SocketContext {
    PRFileDesc* mFD;
    nsASocketHandler* mHandler;
    PRIntervalTime mPollStartEpoch;  // time we started to poll this socket

   public:
    // Returns true iff the socket has not been signalled longer than
    // the desired timeout (mHandler->mPollTimeout).
    bool IsTimedOut(PRIntervalTime now) const;
    // Engages the timeout by marking the epoch we start polling this socket.
    // If epoch is already marked this does nothing, hence, this method can be
    // called everytime we put this socket to poll() list with in-flags set.
    void EnsureTimeout(PRIntervalTime now);
    // Called after an event on a socket has been signalled to turn of the
    // timeout calculation.
    void DisengageTimeout();
    // Returns the number of intervals this socket is about to timeout in,
    // or 0 (zero) when it has already timed out.  Returns
    // NS_SOCKET_POLL_TIMEOUT when there is no timeout set on the socket.
    PRIntervalTime TimeoutIn(PRIntervalTime now) const;
    // When a socket timeout is reset and later set again, it may happen
    // that mPollStartEpoch is not reset in between.  We have to manually
    // call this on every iteration over sockets to ensure the epoch reset.
    void MaybeResetEpoch();
  };

  SocketContext* mActiveList; /* mListSize entries */
  SocketContext* mIdleList;   /* mListSize entries */
  nsIThread* mRawThread;

  uint32_t mActiveListSize;
  uint32_t mIdleListSize;
  uint32_t mActiveCount;
  uint32_t mIdleCount;

  nsresult DetachSocket(SocketContext*, SocketContext*);
  nsresult AddToIdleList(SocketContext*);
  nsresult AddToPollList(SocketContext*);
  void RemoveFromIdleList(SocketContext*);
  void RemoveFromPollList(SocketContext*);
  void MoveToIdleList(SocketContext* sock);
  void MoveToPollList(SocketContext* sock);

  bool GrowActiveList();
  bool GrowIdleList();
  void InitMaxCount();

  // Total bytes number transfered through all the sockets except active ones
  uint64_t mSentBytesCount;
  uint64_t mReceivedBytesCount;
  //-------------------------------------------------------------------------
  // poll list (socket thread only)
  //
  // first element of the poll list is mPollableEvent (or null if the pollable
  // event cannot be created).
  //-------------------------------------------------------------------------

  PRPollDesc* mPollList; /* mListSize + 1 entries */

  PRIntervalTime PollTimeout(
      PRIntervalTime now);  // computes ideal poll timeout
  nsresult DoPollIteration(TimeDuration* pollDuration);
  // perfoms a single poll iteration
  int32_t Poll(TimeDuration* pollDuration, PRIntervalTime now);
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
  nsresult UpdatePrefs();
  static void UpdatePrefs(const char* aPref, void* aSelf);
  void UpdateSendBufferPref();
  int32_t mSendBufferSize;
  // Number of seconds of connection is idle before first keepalive ping.
  int32_t mKeepaliveIdleTimeS;
  // Number of seconds between retries should keepalive pings fail.
  int32_t mKeepaliveRetryIntervalS;
  // Number of keepalive probes to send.
  int32_t mKeepaliveProbeCount;
  // True if TCP keepalive is enabled globally.
  bool mKeepaliveEnabledPref;
  // Timeout of pollable event signalling.
  TimeDuration mPollableEventTimeout;

  Atomic<bool> mServingPendingQueue;
  Atomic<int32_t, Relaxed> mMaxTimePerPollIter;
  Atomic<PRIntervalTime, Relaxed> mMaxTimeForPrClosePref;
  // Timestamp of the last network link change event, tracked
  // also on child processes.
  Atomic<PRIntervalTime, Relaxed> mLastNetworkLinkChangeTime;
  // Preference for how long we do busy wait after network link
  // change has been detected.
  Atomic<PRIntervalTime, Relaxed> mNetworkLinkChangeBusyWaitPeriod;
  // Preference for the value of timeout for poll() we use during
  // the network link change event period.
  Atomic<PRIntervalTime, Relaxed> mNetworkLinkChangeBusyWaitTimeout;

  // Between a computer going to sleep and waking up the PR_*** telemetry
  // will be corrupted - so do not record it.
  Atomic<bool, Relaxed> mSleepPhase;
  nsCOMPtr<nsITimer> mAfterWakeUpTimer;

  // Lazily created array of forced port remappings.  The tuple members meaning
  // is exactly:
  // <0> the greater-or-equal port number of the range to remap
  // <1> the less-or-equal port number of the range to remap
  // <2> the port number to remap to, when the given port number falls to the
  // range
  typedef CopyableTArray<Tuple<uint16_t, uint16_t, uint16_t>> TPortRemapping;
  Maybe<TPortRemapping> mPortRemapping;

  // Called on the socket thread to apply the mapping build on the main thread
  // from the preference.
  void ApplyPortRemapPreference(TPortRemapping const& portRemapping);

  void OnKeepaliveEnabledPrefChange();
  void NotifyKeepaliveEnabledPrefChange(SocketContext* sock);

  // Socket thread only for dynamically adjusting max socket size
#if defined(XP_WIN)
  void ProbeMaxCount();
#endif
  bool mProbedMaxCount;

  void AnalyzeConnection(nsTArray<SocketInfo>* data, SocketContext* context,
                         bool aActive);

  void ClosePrivateConnections();
  void DetachSocketWithGuard(bool aGuardLocals, SocketContext* socketList,
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

  void TryRepairPollableEvent();

  bool mEsniEnabled;
  bool mTrustedMitmDetected;
  bool mNotTrustedMitmDetected;
};

extern nsSocketTransportService* gSocketTransportService;
bool OnSocketThread();

}  // namespace net
}  // namespace mozilla

#endif  // !nsSocketTransportService_h__
