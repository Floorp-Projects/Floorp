/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnectionMgr_h__
#define nsHttpConnectionMgr_h__

#include "nsHttpConnectionInfo.h"
#include "nsHttpConnection.h"
#include "nsHttpTransaction.h"
#include "NullHttpTransaction.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsAutoPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsISocketTransportService.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"
#include "mozilla/net/DashboardTypes.h"

#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsIX509Cert3.h"

class nsHttpPipeline;

class nsIHttpUpgradeListener;

//-----------------------------------------------------------------------------

class nsHttpConnectionMgr : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    // parameter names
    enum nsParamName {
        MAX_CONNECTIONS,
        MAX_PERSISTENT_CONNECTIONS_PER_HOST,
        MAX_PERSISTENT_CONNECTIONS_PER_PROXY,
        MAX_REQUEST_DELAY,
        MAX_PIPELINED_REQUESTS,
        MAX_OPTIMISTIC_PIPELINED_REQUESTS
    };

    //-------------------------------------------------------------------------
    // NOTE: functions below may only be called on the main thread.
    //-------------------------------------------------------------------------

    nsHttpConnectionMgr();

    nsresult Init(uint16_t maxConnections,
                  uint16_t maxPersistentConnectionsPerHost,
                  uint16_t maxPersistentConnectionsPerProxy,
                  uint16_t maxRequestDelay,
                  uint16_t maxPipelinedRequests,
                  uint16_t maxOptimisticPipelinedRequests);
    nsresult Shutdown();

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

    // adds a transaction to the list of managed transactions.
    nsresult AddTransaction(nsHttpTransaction *, int32_t priority);

    // called to reschedule the given transaction.  it must already have been
    // added to the connection manager via AddTransaction.
    nsresult RescheduleTransaction(nsHttpTransaction *, int32_t priority);

    // cancels a transaction w/ the given reason.
    nsresult CancelTransaction(nsHttpTransaction *, nsresult reason);

    // called to force the connection manager to prune its list of idle
    // connections.
    nsresult PruneDeadConnections();

    // Close all idle persistent connections and prevent any active connections
    // from being reused.
    nsresult ClosePersistentConnections();

    // called to get a reference to the socket transport service.  the socket
    // transport service is not available when the connection manager is down.
    nsresult GetSocketThreadTarget(nsIEventTarget **);

    // called to indicate a transaction for the connectionInfo is likely coming
    // soon. The connection manager may use this information to start a TCP
    // and/or SSL level handshake for that resource immediately so that it is
    // ready when the transaction is submitted. No obligation is taken on by the
    // connection manager, nor is the submitter obligated to actually submit a
    // real transaction for this connectionInfo.
    nsresult SpeculativeConnect(nsHttpConnectionInfo *,
                                nsIInterfaceRequestor *);

    // called when a connection is done processing a transaction.  if the 
    // connection can be reused then it will be added to the idle list, else
    // it will be closed.
    nsresult ReclaimConnection(nsHttpConnection *conn);

    // called by the main thread to execute the taketransport() logic on the
    // socket thread after a 101 response has been received and the socket
    // needs to be transferred to an expectant upgrade listener such as
    // websockets.
    nsresult CompleteUpgrade(nsAHttpConnection *aConn,
                             nsIHttpUpgradeListener *aUpgradeListener);

    // called to update a parameter after the connection manager has already
    // been initialized.
    nsresult UpdateParam(nsParamName name, uint16_t value);

    // Lookup/Cancel HTTP->SPDY redirections
    bool GetSpdyAlternateProtocol(nsACString &key);
    void ReportSpdyAlternateProtocol(nsHttpConnection *);
    void RemoveSpdyAlternateProtocol(nsACString &key);

    // Pipielining Interfaces and Datatypes

    const static uint32_t kPipelineInfoTypeMask = 0xffff0000;
    const static uint32_t kPipelineInfoIDMask   = ~kPipelineInfoTypeMask;

    const static uint32_t kPipelineInfoTypeRed     = 0x00010000;
    const static uint32_t kPipelineInfoTypeBad     = 0x00020000;
    const static uint32_t kPipelineInfoTypeNeutral = 0x00040000;
    const static uint32_t kPipelineInfoTypeGood    = 0x00080000;

    enum PipelineFeedbackInfoType
    {
        // Used when an HTTP response less than 1.1 is received
        RedVersionTooLow = kPipelineInfoTypeRed | kPipelineInfoTypeBad | 0x0001,

        // Used when a HTTP Server response header that is on the banned from
        // pipelining list is received
        RedBannedServer = kPipelineInfoTypeRed | kPipelineInfoTypeBad | 0x0002,
    
        // Used when a response is terminated early, when it fails an
        // integrity check such as assoc-req or when a 304 contained a Last-Modified
        // differnet than the entry being validated.
        RedCorruptedContent = kPipelineInfoTypeRed | kPipelineInfoTypeBad | 0x0004,

        // Used when a pipeline is only partly satisfied - for instance if the
        // server closed the connection after responding to the first
        // request but left some requests unprocessed.
        RedCanceledPipeline = kPipelineInfoTypeRed | kPipelineInfoTypeBad | 0x0005,

        // Used when a connection that we expected to stay persistently open
        // was closed by the server. Not used when simply timed out.
        BadExplicitClose = kPipelineInfoTypeBad | 0x0003,

        // Used when there is a gap of around 400 - 1200ms in between data being
        // read from the server
        BadSlowReadMinor = kPipelineInfoTypeBad | 0x0006,

        // Used when there is a gap of > 1200ms in between data being
        // read from the server
        BadSlowReadMajor = kPipelineInfoTypeBad | 0x0007,

        // Used when a response is received that is not framed with either chunked
        // encoding or a complete content length.
        BadInsufficientFraming = kPipelineInfoTypeBad | 0x0008,
        
        // Used when a very large response is recevied in a potential pipelining
        // context. Large responses cause head of line blocking.
        BadUnexpectedLarge = kPipelineInfoTypeBad | 0x000B,

        // Used when a response is received that has headers that appear to support
        // pipelining.
        NeutralExpectedOK = kPipelineInfoTypeNeutral | 0x0009,

        // Used when a response is received successfully to a pipelined request.
        GoodCompletedOK = kPipelineInfoTypeGood | 0x000A
    };
    
    // called to provide information relevant to the pipelining manager
    // may be called from any thread
    void     PipelineFeedbackInfo(nsHttpConnectionInfo *,
                                  PipelineFeedbackInfoType info,
                                  nsHttpConnection *,
                                  uint32_t);

    void ReportFailedToProcess(nsIURI *uri);

    // Causes a large amount of connection diagnostic information to be
    // printed to the javascript console
    void PrintDiagnostics();

    //-------------------------------------------------------------------------
    // NOTE: functions below may be called only on the socket thread.
    //-------------------------------------------------------------------------

    // called to force the transaction queue to be processed once more, giving
    // preference to the specified connection.
    nsresult ProcessPendingQ(nsHttpConnectionInfo *);
    bool     ProcessPendingQForEntry(nsHttpConnectionInfo *);

    // Try and process all pending transactions
    nsresult ProcessPendingQ();

    // This is used to force an idle connection to be closed and removed from
    // the idle connection list. It is called when the idle connection detects
    // that the network peer has closed the transport.
    nsresult CloseIdleConnection(nsHttpConnection *);

    // The connection manager needs to know when a normal HTTP connection has been
    // upgraded to SPDY because the dispatch and idle semantics are a little
    // bit different.
    void ReportSpdyConnection(nsHttpConnection *, bool usingSpdy);

    
    bool     SupportsPipelining(nsHttpConnectionInfo *);

    bool GetConnectionData(nsTArray<mozilla::net::HttpRetParams> *);
private:
    virtual ~nsHttpConnectionMgr();

    enum PipeliningState {
        // Host has proven itself pipeline capable through past experience and
        // large pipeline depths are allowed on multiple connections.
        PS_GREEN,

        // Not enough information is available yet with this host to be certain
        // of pipeline capability. Small pipelines on a single connection are
        // allowed in order to decide whether or not to proceed to green.
        PS_YELLOW,

        // One or more bad events has happened that indicate that pipelining
        // to this host (or a particular type of transaction with this host)
        // is a bad idea. Pipelining is not currently allowed, but time and
        // other positive experiences will eventually allow it to try again.
        PS_RED
    };
    
    class nsHalfOpenSocket;

    // nsConnectionEntry
    //
    // mCT maps connection info hash key to nsConnectionEntry object, which
    // contains list of active and idle connections as well as the list of
    // pending transactions.
    //
    class nsConnectionEntry
    {
    public:
        nsConnectionEntry(nsHttpConnectionInfo *ci);
        ~nsConnectionEntry();

        nsHttpConnectionInfo        *mConnInfo;
        nsTArray<nsHttpTransaction*> mPendingQ;    // pending transaction queue
        nsTArray<nsHttpConnection*>  mActiveConns; // active connections
        nsTArray<nsHttpConnection*>  mIdleConns;   // idle persistent connections
        nsTArray<nsHalfOpenSocket*>  mHalfOpens;

        // calculate the number of half open sockets that have not had at least 1
        // connection complete
        uint32_t UnconnectedHalfOpens();

        // Remove a particular half open socket from the mHalfOpens array
        void RemoveHalfOpen(nsHalfOpenSocket *);

        // Pipeline depths for various states
        const static uint32_t kPipelineUnlimited  = 1024; // fully open - extended green
        const static uint32_t kPipelineOpen       = 6;    // 6 on each conn - normal green
        const static uint32_t kPipelineRestricted = 2;    // 2 on just 1 conn in yellow
        
        nsHttpConnectionMgr::PipeliningState PipelineState();
        void OnPipelineFeedbackInfo(
            nsHttpConnectionMgr::PipelineFeedbackInfoType info,
            nsHttpConnection *, uint32_t);
        bool SupportsPipelining();
        uint32_t MaxPipelineDepth(nsAHttpTransaction::Classifier classification);
        void CreditPenalty();

        nsHttpConnectionMgr::PipeliningState mPipelineState;

        void SetYellowConnection(nsHttpConnection *);
        void OnYellowComplete();
        uint32_t                  mYellowGoodEvents;
        uint32_t                  mYellowBadEvents;
        nsHttpConnection         *mYellowConnection;

        // initialGreenDepth is the max depth of a pipeline when you first
        // transition to green. Normally this is kPipelineOpen, but it can
        // be kPipelineUnlimited in aggressive mode.
        uint32_t                  mInitialGreenDepth;

        // greenDepth is the current max allowed depth of a pipeline when
        // in the green state. Normally this starts as kPipelineOpen and
        // grows to kPipelineUnlimited after a pipeline of depth 3 has been
        // successfully transacted.
        uint32_t                  mGreenDepth;

        // pipeliningPenalty is the current amount of penalty points this host
        // entry has earned for participating in events that are not conducive
        // to good pipelines - such as head of line blocking, canceled pipelines,
        // etc.. penalties are paid back either through elapsed time or simply
        // healthy transactions. Having penalty points means that this host is
        // not currently eligible for pipelines.
        int16_t                   mPipeliningPenalty;

        // some penalty points only apply to particular classifications of
        // transactions - this allows a server that perhaps has head of line
        // blocking problems on CGI queries to still serve JS pipelined.
        int16_t                   mPipeliningClassPenalty[nsAHttpTransaction::CLASS_MAX];

        // for calculating penalty repair credits
        mozilla::TimeStamp        mLastCreditTime;

        // Spdy sometimes resolves the address in the socket manager in order
        // to re-coalesce sharded HTTP hosts. The dotted decimal address is
        // combined with the Anonymous flag from the connection information
        // to build the hash key for hosts in the same ip pool.
        //
        // When a set of hosts are coalesced together one of them is marked
        // mSpdyPreferred. The mapping is maintained in the connection mananger
        // mSpdyPreferred hash.
        //
        nsCString mCoalescingKey;

        // To have the UsingSpdy flag means some host with the same connection
        // entry has done NPN=spdy/* at some point. It does not mean every
        // connection is currently using spdy.
        bool mUsingSpdy;

        // mTestedSpdy is set after NPN negotiation has occurred and we know
        // with confidence whether a host speaks spdy or not (which is reflected
        // in mUsingSpdy). Before mTestedSpdy is set, handshake parallelism is
        // minimized so that we can multiplex on a single spdy connection.
        bool mTestedSpdy;

        bool mSpdyPreferred;
    };

    // nsConnectionHandle
    //
    // thin wrapper around a real connection, used to keep track of references
    // to the connection to determine when the connection may be reused.  the
    // transaction (or pipeline) owns a reference to this handle.  this extra
    // layer of indirection greatly simplifies consumer code, avoiding the
    // need for consumer code to know when to give the connection back to the
    // connection manager.
    //
    class nsConnectionHandle : public nsAHttpConnection
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSAHTTPCONNECTION(mConn)

        nsConnectionHandle(nsHttpConnection *conn) { NS_ADDREF(mConn = conn); }
        virtual ~nsConnectionHandle();

        nsHttpConnection *mConn;
    };

    // nsHalfOpenSocket is used to hold the state of an opening TCP socket
    // while we wait for it to establish and bind it to a connection

    class nsHalfOpenSocket MOZ_FINAL : public nsIOutputStreamCallback,
                                       public nsITransportEventSink,
                                       public nsIInterfaceRequestor,
                                       public nsITimerCallback
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOUTPUTSTREAMCALLBACK
        NS_DECL_NSITRANSPORTEVENTSINK
        NS_DECL_NSIINTERFACEREQUESTOR
        NS_DECL_NSITIMERCALLBACK

        nsHalfOpenSocket(nsConnectionEntry *ent,
                         nsAHttpTransaction *trans,
                         uint32_t caps);
        ~nsHalfOpenSocket();
        
        nsresult SetupStreams(nsISocketTransport **,
                              nsIAsyncInputStream **,
                              nsIAsyncOutputStream **,
                              bool isBackup);
        nsresult SetupPrimaryStreams();
        nsresult SetupBackupStreams();
        void     SetupBackupTimer();
        void     CancelBackupTimer();
        void     Abandon();
        double   Duration(mozilla::TimeStamp epoch);
        nsISocketTransport *SocketTransport() { return mSocketTransport; }
        nsISocketTransport *BackupTransport() { return mBackupTransport; }

        nsAHttpTransaction *Transaction() { return mTransaction; }

        bool IsSpeculative() { return mSpeculative; }
        void SetSpeculative(bool val) { mSpeculative = val; }

        bool HasConnected() { return mHasConnected; }

        void PrintDiagnostics(nsCString &log);
    private:
        nsConnectionEntry              *mEnt;
        nsRefPtr<nsAHttpTransaction>   mTransaction;
        nsCOMPtr<nsISocketTransport>   mSocketTransport;
        nsCOMPtr<nsIAsyncOutputStream> mStreamOut;
        nsCOMPtr<nsIAsyncInputStream>  mStreamIn;
        uint32_t                       mCaps;

        // mSpeculative is set if the socket was created from
        // SpeculativeConnect(). It is cleared when a transaction would normally
        // start a new connection from scratch but instead finds this one in
        // the half open list and claims it for its own use. (which due to
        // the vagaries of scheduling from the pending queue might not actually
        // match up - but it prevents a speculative connection from opening
        // more connections that are needed.)
        bool                           mSpeculative;

        mozilla::TimeStamp             mPrimarySynStarted;
        mozilla::TimeStamp             mBackupSynStarted;

        // for syn retry
        nsCOMPtr<nsITimer>             mSynTimer;
        nsCOMPtr<nsISocketTransport>   mBackupTransport;
        nsCOMPtr<nsIAsyncOutputStream> mBackupStreamOut;
        nsCOMPtr<nsIAsyncInputStream>  mBackupStreamIn;

        bool                           mHasConnected;
    };
    friend class nsHalfOpenSocket;

    //-------------------------------------------------------------------------
    // NOTE: these members may be accessed from any thread (use mReentrantMonitor)
    //-------------------------------------------------------------------------

    mozilla::ReentrantMonitor    mReentrantMonitor;
    nsCOMPtr<nsIEventTarget>     mSocketThreadTarget;

    // connection limits
    uint16_t mMaxConns;
    uint16_t mMaxPersistConnsPerHost;
    uint16_t mMaxPersistConnsPerProxy;
    uint16_t mMaxRequestDelay; // in seconds
    uint16_t mMaxPipelinedRequests;
    uint16_t mMaxOptimisticPipelinedRequests;
    bool mIsShuttingDown;

    //-------------------------------------------------------------------------
    // NOTE: these members are only accessed on the socket transport thread
    //-------------------------------------------------------------------------

    static PLDHashOperator ProcessOneTransactionCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    static PLDHashOperator ProcessAllTransactionsCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);

    static PLDHashOperator PruneDeadConnectionsCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    static PLDHashOperator ShutdownPassCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    static PLDHashOperator PurgeExcessIdleConnectionsCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    static PLDHashOperator ClosePersistentConnectionsCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    bool     ProcessPendingQForEntry(nsConnectionEntry *, bool considerAll);
    bool     IsUnderPressure(nsConnectionEntry *ent,
                             nsHttpTransaction::Classifier classification);
    bool     AtActiveConnectionLimit(nsConnectionEntry *, uint32_t caps);
    nsresult TryDispatchTransaction(nsConnectionEntry *ent,
                                    bool onlyReusedConnection,
                                    nsHttpTransaction *trans);
    nsresult DispatchTransaction(nsConnectionEntry *,
                                 nsHttpTransaction *,
                                 nsHttpConnection *);
    nsresult DispatchAbstractTransaction(nsConnectionEntry *,
                                         nsAHttpTransaction *,
                                         uint32_t,
                                         nsHttpConnection *,
                                         int32_t);
    nsresult BuildPipeline(nsConnectionEntry *,
                           nsAHttpTransaction *,
                           nsHttpPipeline **);
    bool     RestrictConnections(nsConnectionEntry *);
    nsresult ProcessNewTransaction(nsHttpTransaction *);
    nsresult EnsureSocketThreadTarget();
    void     ClosePersistentConnections(nsConnectionEntry *ent);
    void     ReportProxyTelemetry(nsConnectionEntry *ent);
    nsresult CreateTransport(nsConnectionEntry *, nsAHttpTransaction *,
                             uint32_t, bool);
    void     AddActiveConn(nsHttpConnection *, nsConnectionEntry *);
    void     StartedConnect();
    void     RecvdConnect();

    nsConnectionEntry *GetOrCreateConnectionEntry(nsHttpConnectionInfo *);

    nsresult MakeNewConnection(nsConnectionEntry *ent,
                               nsHttpTransaction *trans);
    bool     AddToShortestPipeline(nsConnectionEntry *ent,
                                   nsHttpTransaction *trans,
                                   nsHttpTransaction::Classifier classification,
                                   uint16_t depthLimit);

    // Manage the preferred spdy connection entry for this address
    nsConnectionEntry *GetSpdyPreferredEnt(nsConnectionEntry *aOriginalEntry);
    void               RemoveSpdyPreferredEnt(nsACString &aDottedDecimal);
    nsHttpConnection  *GetSpdyPreferredConn(nsConnectionEntry *ent);
    nsDataHashtable<nsCStringHashKey, nsConnectionEntry *>   mSpdyPreferredHash;
    nsConnectionEntry *LookupConnectionEntry(nsHttpConnectionInfo *ci,
                                             nsHttpConnection *conn,
                                             nsHttpTransaction *trans);

    void               ProcessSpdyPendingQ(nsConnectionEntry *ent);
    void               ProcessAllSpdyPendingQ();
    static PLDHashOperator ProcessSpdyPendingQCB(
        const nsACString &key, nsAutoPtr<nsConnectionEntry> &ent,
        void *closure);

    // message handlers have this signature
    typedef void (nsHttpConnectionMgr:: *nsConnEventHandler)(int32_t, void *);

    // nsConnEvent
    //
    // subclass of nsRunnable used to marshall events to the socket transport
    // thread.  this class is used to implement PostEvent.
    //
    class nsConnEvent;
    friend class nsConnEvent;
    class nsConnEvent : public nsRunnable
    {
    public:
        nsConnEvent(nsHttpConnectionMgr *mgr,
                    nsConnEventHandler handler,
                    int32_t iparam,
                    void *vparam)
            : mMgr(mgr)
            , mHandler(handler)
            , mIParam(iparam)
            , mVParam(vparam)
        {
            NS_ADDREF(mMgr);
        }

        NS_IMETHOD Run()
        {
            (mMgr->*mHandler)(mIParam, mVParam);
            return NS_OK;
        }

    private:
        virtual ~nsConnEvent()
        {
            NS_RELEASE(mMgr);
        }

        nsHttpConnectionMgr *mMgr;
        nsConnEventHandler   mHandler;
        int32_t              mIParam;
        void                *mVParam;
    };

    nsresult PostEvent(nsConnEventHandler  handler,
                       int32_t             iparam = 0,
                       void               *vparam = nullptr);

    // message handlers
    void OnMsgShutdown             (int32_t, void *);
    void OnMsgShutdownConfirm      (int32_t, void *);
    void OnMsgNewTransaction       (int32_t, void *);
    void OnMsgReschedTransaction   (int32_t, void *);
    void OnMsgCancelTransaction    (int32_t, void *);
    void OnMsgProcessPendingQ      (int32_t, void *);
    void OnMsgPruneDeadConnections (int32_t, void *);
    void OnMsgSpeculativeConnect   (int32_t, void *);
    void OnMsgReclaimConnection    (int32_t, void *);
    void OnMsgCompleteUpgrade      (int32_t, void *);
    void OnMsgUpdateParam          (int32_t, void *);
    void OnMsgClosePersistentConnections (int32_t, void *);
    void OnMsgProcessFeedback      (int32_t, void *);

    // Total number of active connections in all of the ConnectionEntry objects
    // that are accessed from mCT connection table.
    uint16_t mNumActiveConns;
    // Total number of idle connections in all of the ConnectionEntry objects
    // that are accessed from mCT connection table.
    uint16_t mNumIdleConns;
    // Total number of connections in mHalfOpens ConnectionEntry objects
    // that are accessed from mCT connection table
    uint32_t mNumHalfOpenConns;

    // Holds time in seconds for next wake-up to prune dead connections. 
    uint64_t mTimeOfNextWakeUp;
    // Timer for next pruning of dead connections.
    nsCOMPtr<nsITimer> mTimer;

    // A 1s tick to call nsHttpConnection::ReadTimeoutTick on
    // active http/1 connections and check for orphaned half opens.
    // Disabled when there are no active or half open connections.
    nsCOMPtr<nsITimer> mTimeoutTick;
    bool mTimeoutTickArmed;

    //
    // the connection table
    //
    // this table is indexed by connection key.  each entry is a
    // nsConnectionEntry object.
    //
    nsClassHashtable<nsCStringHashKey, nsConnectionEntry> mCT;

    // mAlternateProtocolHash is used only for spdy/* upgrades for now
    // protected by the monitor
    nsTHashtable<nsCStringHashKey> mAlternateProtocolHash;
    static PLDHashOperator TrimAlternateProtocolHash(nsCStringHashKey *entry,
                                                     void *closure);

    static PLDHashOperator ReadConnectionEntry(const nsACString &key,
                                               nsAutoPtr<nsConnectionEntry> &ent,
                                               void *aArg);

    // Read Timeout Tick handlers
    void ActivateTimeoutTick();
    void TimeoutTick();
    static PLDHashOperator TimeoutTickCB(const nsACString &key,
                                         nsAutoPtr<nsConnectionEntry> &ent,
                                         void *closure);

    // For diagnostics
    void OnMsgPrintDiagnostics(int32_t, void *);
    static PLDHashOperator PrintDiagnosticsCB(const nsACString &key,
                                              nsAutoPtr<nsConnectionEntry> &ent,
                                              void *closure);
    nsCString mLogData;
};

#endif // !nsHttpConnectionMgr_h__
