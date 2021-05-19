/* vim:t ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnectionMgr_h__
#define nsHttpConnectionMgr_h__

#include "DnsAndConnectSocket.h"
#include "HttpConnectionBase.h"
#include "HttpConnectionMgrShell.h"
#include "nsHttpConnection.h"
#include "nsHttpTransaction.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsClassHashtable.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"
#include "ARefBase.h"
#include "nsWeakReference.h"
#include "ConnectionEntry.h"

#include "nsINamed.h"
#include "nsIObserver.h"
#include "nsITimer.h"

class nsIHttpUpgradeListener;

namespace mozilla {
namespace net {
class EventTokenBucket;
class NullHttpTransaction;
struct HttpRetParams;

//-----------------------------------------------------------------------------

// message handlers have this signature
class nsHttpConnectionMgr;
typedef void (nsHttpConnectionMgr::*nsConnEventHandler)(int32_t, ARefBase*);

class nsHttpConnectionMgr final : public HttpConnectionMgrShell,
                                  public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_HTTPCONNECTIONMGRSHELL
  NS_DECL_NSIOBSERVER

  //-------------------------------------------------------------------------
  // NOTE: functions below may only be called on the main thread.
  //-------------------------------------------------------------------------

  nsHttpConnectionMgr();

  //-------------------------------------------------------------------------
  // NOTE: functions below may be called on any thread.
  //-------------------------------------------------------------------------

  [[nodiscard]] nsresult CancelTransactions(nsHttpConnectionInfo*,
                                            nsresult reason);

  //-------------------------------------------------------------------------
  // NOTE: functions below may be called only on the socket thread.
  //-------------------------------------------------------------------------

  // called to change the connection entry associated with conn from specific
  // into a wildcard (i.e. http2 proxy friendy) mapping
  void MoveToWildCardConnEntry(nsHttpConnectionInfo* specificCI,
                               nsHttpConnectionInfo* wildcardCI,
                               HttpConnectionBase* conn);

  // Move a transaction from the pendingQ of it's connection entry to another
  // one. Returns true if the transaction is moved successfully, otherwise
  // returns false.
  bool MoveTransToNewConnEntry(nsHttpTransaction* aTrans,
                               nsHttpConnectionInfo* aNewCI);

  // This is used to force an idle connection to be closed and removed from
  // the idle connection list. It is called when the idle connection detects
  // that the network peer has closed the transport.
  [[nodiscard]] nsresult CloseIdleConnection(nsHttpConnection*);
  [[nodiscard]] nsresult RemoveIdleConnection(nsHttpConnection*);

  // The connection manager needs to know when a normal HTTP connection has been
  // upgraded to SPDY because the dispatch and idle semantics are a little
  // bit different.
  void ReportSpdyConnection(nsHttpConnection*, bool usingSpdy);

  void ReportHttp3Connection(HttpConnectionBase*);

  bool GetConnectionData(nsTArray<HttpRetParams>*);

  void ResetIPFamilyPreference(nsHttpConnectionInfo*);

  uint16_t MaxRequestDelay() { return mMaxRequestDelay; }

  // tracks and untracks active transactions according their throttle status
  void AddActiveTransaction(nsHttpTransaction* aTrans);
  void RemoveActiveTransaction(nsHttpTransaction* aTrans,
                               Maybe<bool> const& aOverride = Nothing());
  void UpdateActiveTransaction(nsHttpTransaction* aTrans);

  // called by nsHttpTransaction::WriteSegments.  decides whether the
  // transaction should limit reading its reponse data.  There are various
  // conditions this methods evaluates.  If called by an active-tab
  // non-throttled transaction, the throttling window time will be prolonged.
  bool ShouldThrottle(nsHttpTransaction* aTrans);

  // prolongs the throttling time window to now + the window preferred delay.
  // called when:
  // - any transaction is activated
  // - or when a currently unthrottled transaction for the active window
  // receives data
  void TouchThrottlingTimeWindow(bool aEnsureTicker = true);

  // return true iff the connection has pending transactions for the active tab.
  // it's mainly used to disallow throttling (limit reading) of a response
  // belonging to the same conn info to free up a connection ASAP.
  // NOTE: relatively expensive to call, there are two hashtable lookups.
  bool IsConnEntryUnderPressure(nsHttpConnectionInfo*);

  uint64_t CurrentTopBrowsingContextId() {
    return mCurrentTopBrowsingContextId;
  }

  void DoSpeculativeConnection(SpeculativeTransaction* aTrans,
                               bool aFetchHTTPSRR);

  HttpConnectionBase* GetH2orH3ActiveConn(ConnectionEntry* ent, bool aNoHttp2,
                                          bool aNoHttp3);

  void IncreaseNumDnsAndConnectSockets();
  void DecreaseNumDnsAndConnectSockets();

  // Wen a new idle connection has been added, this function is called to
  // increment mNumIdleConns and update PruneDeadConnections timer.
  void NewIdleConnectionAdded(uint32_t timeToLive);
  void DecrementNumIdleConns();

 private:
  virtual ~nsHttpConnectionMgr();

  //-------------------------------------------------------------------------
  // NOTE: functions below may be called on any thread.
  //-------------------------------------------------------------------------

  // Schedules next pruning of dead connection to happen after
  // given time.
  void PruneDeadConnectionsAfter(uint32_t time);

  // Stops timer scheduled for next pruning of dead connections if
  // there are no more idle connections or active spdy ones
  void ConditionallyStopPruneDeadConnectionsTimer();

  // Stops timer used for the read timeout tick if there are no currently
  // active connections.
  void ConditionallyStopTimeoutTick();

  // called to close active connections with no registered "traffic"
  [[nodiscard]] nsresult PruneNoTraffic();

  //-------------------------------------------------------------------------
  // NOTE: functions below may be called only on the socket thread.
  //-------------------------------------------------------------------------

  [[nodiscard]] bool ProcessPendingQForEntry(nsHttpConnectionInfo*);

  // public, so that the SPDY/http2 seesions can activate
  void ActivateTimeoutTick();

  already_AddRefed<PendingTransactionInfo> FindTransactionHelper(
      bool removeWhenFound, ConnectionEntry* aEnt, nsAHttpTransaction* aTrans);

 public:
  static nsAHttpConnection* MakeConnectionHandle(HttpConnectionBase* aWrapped);
  void RegisterOriginCoalescingKey(HttpConnectionBase*, const nsACString& host,
                                   int32_t port);
  // A test if be-conservative should be used when proxy is setup for the
  // connection
  bool BeConservativeIfProxied(nsIProxyInfo* proxy);

 protected:
  friend class ConnectionEntry;
  void IncrementActiveConnCount();
  void DecrementActiveConnCount(HttpConnectionBase*);

 private:
  friend class DnsAndConnectSocket;
  friend class PendingTransactionInfo;

  //-------------------------------------------------------------------------
  // NOTE: these members may be accessed from any thread (use mReentrantMonitor)
  //-------------------------------------------------------------------------

  ReentrantMonitor mReentrantMonitor;
  nsCOMPtr<nsIEventTarget> mSocketThreadTarget;

  // connection limits
  uint16_t mMaxUrgentExcessiveConns;
  uint16_t mMaxConns;
  uint16_t mMaxPersistConnsPerHost;
  uint16_t mMaxPersistConnsPerProxy;
  uint16_t mMaxRequestDelay;  // in seconds
  bool mThrottleEnabled;
  uint32_t mThrottleVersion;
  uint32_t mThrottleSuspendFor;
  uint32_t mThrottleResumeFor;
  uint32_t mThrottleReadLimit;
  uint32_t mThrottleReadInterval;
  uint32_t mThrottleHoldTime;
  TimeDuration mThrottleMaxTime;
  bool mBeConservativeForProxy;
  Atomic<bool, mozilla::Relaxed> mIsShuttingDown;

  //-------------------------------------------------------------------------
  // NOTE: these members are only accessed on the socket transport thread
  //-------------------------------------------------------------------------

  [[nodiscard]] bool ProcessPendingQForEntry(ConnectionEntry*,
                                             bool considerAll);
  bool DispatchPendingQ(nsTArray<RefPtr<PendingTransactionInfo>>& pendingQ,
                        ConnectionEntry* ent, bool considerAll);

  // This function selects transactions from mPendingTransactionTable to
  // dispatch according to the following conditions:
  // 1. When ActiveTabPriority() is false, only get transactions from the
  //    queue whose window id is 0.
  // 2. If |considerAll| is false, either get transactions from the focused
  //    window queue or non-focused ones.
  // 3. If |considerAll| is true, fill the |pendingQ| with the transactions from
  //    both focused window and non-focused window queues.
  void PreparePendingQForDispatching(
      ConnectionEntry* ent, nsTArray<RefPtr<PendingTransactionInfo>>& pendingQ,
      bool considerAll);

  // Return |mMaxPersistConnsPerProxy| or |mMaxPersistConnsPerHost|,
  // depending whether the proxy is used.
  uint32_t MaxPersistConnections(ConnectionEntry* ent) const;

  bool AtActiveConnectionLimit(ConnectionEntry*, uint32_t caps);
  [[nodiscard]] nsresult TryDispatchTransaction(
      ConnectionEntry* ent, bool onlyReusedConnection,
      PendingTransactionInfo* pendingTransInfo);
  [[nodiscard]] nsresult TryDispatchTransactionOnIdleConn(
      ConnectionEntry* ent, PendingTransactionInfo* pendingTransInfo,
      bool respectUrgency, bool* allUrgent = nullptr);
  [[nodiscard]] nsresult DispatchTransaction(ConnectionEntry*,
                                             nsHttpTransaction*,
                                             HttpConnectionBase*);
  [[nodiscard]] nsresult DispatchAbstractTransaction(ConnectionEntry*,
                                                     nsAHttpTransaction*,
                                                     uint32_t,
                                                     HttpConnectionBase*,
                                                     int32_t);
  [[nodiscard]] nsresult ProcessNewTransaction(nsHttpTransaction*);
  [[nodiscard]] nsresult EnsureSocketThreadTarget();
  void ReportProxyTelemetry(ConnectionEntry* ent);
  [[nodiscard]] nsresult CreateTransport(
      ConnectionEntry*, nsAHttpTransaction*, uint32_t, bool, bool, bool, bool,
      PendingTransactionInfo* pendingTransInfo);
  void StartedConnect();
  void RecvdConnect();

  ConnectionEntry* GetOrCreateConnectionEntry(nsHttpConnectionInfo*,
                                              bool allowWildCard, bool aNoHttp2,
                                              bool aNoHttp3);

  [[nodiscard]] nsresult MakeNewConnection(
      ConnectionEntry* ent, PendingTransactionInfo* pendingTransInfo);

  // Manage h2/3 connection coalescing
  // The hashtable contains arrays of weak pointers to HttpConnectionBases
  nsClassHashtable<nsCStringHashKey, nsTArray<nsWeakPtr>> mCoalescingHash;

  HttpConnectionBase* FindCoalescableConnection(ConnectionEntry* ent,
                                                bool justKidding, bool aNoHttp2,
                                                bool aNoHttp3);
  HttpConnectionBase* FindCoalescableConnectionByHashKey(ConnectionEntry* ent,
                                                         const nsCString& key,
                                                         bool justKidding,
                                                         bool aNoHttp2,
                                                         bool aNoHttp3);
  void UpdateCoalescingForNewConn(HttpConnectionBase* conn,
                                  ConnectionEntry* ent);

  void ProcessSpdyPendingQ(ConnectionEntry* ent);
  void DispatchSpdyPendingQ(nsTArray<RefPtr<PendingTransactionInfo>>& pendingQ,
                            ConnectionEntry* ent, HttpConnectionBase* connH2,
                            HttpConnectionBase* connH3);
  // used to marshall events to the socket transport thread.
  [[nodiscard]] nsresult PostEvent(nsConnEventHandler handler,
                                   int32_t iparam = 0,
                                   ARefBase* vparam = nullptr);

  void OnMsgReclaimConnection(HttpConnectionBase*);

  // message handlers
  void OnMsgShutdown(int32_t, ARefBase*);
  void OnMsgShutdownConfirm(int32_t, ARefBase*);
  void OnMsgNewTransaction(int32_t, ARefBase*);
  void OnMsgNewTransactionWithStickyConn(int32_t, ARefBase*);
  void OnMsgReschedTransaction(int32_t, ARefBase*);
  void OnMsgUpdateClassOfServiceOnTransaction(int32_t, ARefBase*);
  void OnMsgCancelTransaction(int32_t, ARefBase*);
  void OnMsgCancelTransactions(int32_t, ARefBase*);
  void OnMsgProcessPendingQ(int32_t, ARefBase*);
  void OnMsgPruneDeadConnections(int32_t, ARefBase*);
  void OnMsgSpeculativeConnect(int32_t, ARefBase*);
  void OnMsgCompleteUpgrade(int32_t, ARefBase*);
  void OnMsgUpdateParam(int32_t, ARefBase*);
  void OnMsgDoShiftReloadConnectionCleanup(int32_t, ARefBase*);
  void OnMsgProcessFeedback(int32_t, ARefBase*);
  void OnMsgProcessAllSpdyPendingQ(int32_t, ARefBase*);
  void OnMsgUpdateRequestTokenBucket(int32_t, ARefBase*);
  void OnMsgVerifyTraffic(int32_t, ARefBase*);
  void OnMsgPruneNoTraffic(int32_t, ARefBase*);
  void OnMsgUpdateCurrentTopBrowsingContextId(int32_t, ARefBase*);
  void OnMsgClearConnectionHistory(int32_t, ARefBase*);

  // Total number of active connections in all of the ConnectionEntry objects
  // that are accessed from mCT connection table.
  uint16_t mNumActiveConns;
  // Total number of idle connections in all of the ConnectionEntry objects
  // that are accessed from mCT connection table.
  uint16_t mNumIdleConns;
  // Total number of spdy or http3 connections which are a subset of the active
  // conns
  uint16_t mNumSpdyHttp3ActiveConns;
  // Total number of connections in DnsAndConnectSockets ConnectionEntry objects
  // that are accessed from mCT connection table
  uint32_t mNumDnsAndConnectSockets;

  // Holds time in seconds for next wake-up to prune dead connections.
  uint64_t mTimeOfNextWakeUp;
  // Timer for next pruning of dead connections.
  nsCOMPtr<nsITimer> mTimer;
  // Timer for pruning stalled connections after changed network.
  nsCOMPtr<nsITimer> mTrafficTimer;
  bool mPruningNoTraffic;

  // A 1s tick to call nsHttpConnection::ReadTimeoutTick on
  // active http/1 connections and check for orphaned half opens.
  // Disabled when there are no active or half open connections.
  nsCOMPtr<nsITimer> mTimeoutTick;
  bool mTimeoutTickArmed;
  uint32_t mTimeoutTickNext;

  //
  // the connection table
  //
  // this table is indexed by connection key.  each entry is a
  // ConnectionEntry object. It is unlocked and therefore must only
  // be accessed from the socket thread.
  //
  nsRefPtrHashtable<nsCStringHashKey, ConnectionEntry> mCT;

  // Read Timeout Tick handlers
  void TimeoutTick();

  // For diagnostics
  void OnMsgPrintDiagnostics(int32_t, ARefBase*);

  nsCString mLogData;
  uint64_t mCurrentTopBrowsingContextId;

  // Called on a pref change
  void SetThrottlingEnabled(bool aEnable);

  // we only want to throttle for a limited amount of time after a new
  // active transaction is added so that we don't block downloads on comet,
  // socket and any kind of longstanding requests that don't need bandwidth.
  // these methods track this time.
  bool InThrottlingTimeWindow();

  // Two hashtalbes keeping track of active transactions regarding window id and
  // throttling. Used by the throttling algorithm to obtain number of
  // transactions for the active tab and for inactive tabs according their
  // throttle status. mActiveTransactions[0] are all unthrottled transactions,
  // mActiveTransactions[1] throttled.
  nsClassHashtable<nsUint64HashKey, nsTArray<RefPtr<nsHttpTransaction>>>
      mActiveTransactions[2];

  // V1 specific
  // Whether we are inside the "stop reading" interval, altered by the throttle
  // ticker
  bool mThrottlingInhibitsReading;

  TimeStamp mThrottlingWindowEndsAt;

  // ticker for the 'stop reading'/'resume reading' signal
  nsCOMPtr<nsITimer> mThrottleTicker;
  // Checks if the combination of active transactions requires the ticker.
  bool IsThrottleTickerNeeded();
  // The method also unschedules the delayed resume of background tabs timer
  // if the ticker was about to be scheduled.
  void EnsureThrottleTickerIfNeeded();
  // V1:
  // Drops also the mThrottlingInhibitsReading flag.  Immediate or delayed
  // resume of currently throttled transactions is not affected by this method.
  // V2:
  // Immediate or delayed resume of currently throttled transactions is not
  // affected by this method.
  void DestroyThrottleTicker();
  // V1:
  // Handler for the ticker: alters the mThrottlingInhibitsReading flag.
  // V2:
  // Handler for the ticker: calls ResumeReading() for all throttled
  // transactions.
  void ThrottlerTick();

  // mechanism to delay immediate resume of background tabs and chrome initiated
  // throttled transactions after the last transaction blocking their unthrottle
  // has been removed.  Needs to be delayed because during a page load there is
  // a number of intervals when there is no transaction that would cause
  // throttling. Hence, throttling of long standing responses, like downloads,
  // would be mostly ineffective if resumed during every such interval.
  nsCOMPtr<nsITimer> mDelayedResumeReadTimer;
  // Schedule the resume
  void DelayedResumeBackgroundThrottledTransactions();
  // Simply destroys the timer
  void CancelDelayedResumeBackgroundThrottledTransactions();
  // Handler for the timer: resumes all background throttled transactions
  void ResumeBackgroundThrottledTransactions();

  // Simple helpers, iterates the given hash/array and resume.
  // @param excludeActive: skip active tabid transactions.
  void ResumeReadOf(
      nsClassHashtable<nsUint64HashKey, nsTArray<RefPtr<nsHttpTransaction>>>&,
      bool excludeActive = false);
  void ResumeReadOf(nsTArray<RefPtr<nsHttpTransaction>>*);

  // Cached status of the active tab active transactions existence,
  // saves a lot of hashtable lookups
  bool mActiveTabTransactionsExist;
  bool mActiveTabUnthrottledTransactionsExist;

  void LogActiveTransactions(char);

  // When current active tab is changed, this function uses
  // |previousId| to select background transactions and
  // |mCurrentTopBrowsingContextId| to select foreground transactions.
  // Then, it notifies selected transactions' connection of the new active tab
  // id.
  void NotifyConnectionOfBrowsingContextIdChange(uint64_t previousId);
};

}  // namespace net
}  // namespace mozilla

#endif  // !nsHttpConnectionMgr_h__
