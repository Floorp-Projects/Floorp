/* vim:set ts=4 sw=4 sts=4 et cin: */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef nsHttpConnectionMgr_h__
#define nsHttpConnectionMgr_h__

#include "nsHttpConnectionInfo.h"
#include "nsHttpConnection.h"
#include "nsHttpTransaction.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsClassHashtable.h"
#include "nsAutoPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsISocketTransportService.h"

#include "nsIObserver.h"
#include "nsITimer.h"

class nsHttpPipeline;

//-----------------------------------------------------------------------------

class nsHttpConnectionMgr : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    // parameter names
    enum nsParamName {
        MAX_CONNECTIONS,
        MAX_CONNECTIONS_PER_HOST,
        MAX_CONNECTIONS_PER_PROXY,
        MAX_PERSISTENT_CONNECTIONS_PER_HOST,
        MAX_PERSISTENT_CONNECTIONS_PER_PROXY,
        MAX_REQUEST_DELAY,
        MAX_PIPELINED_REQUESTS
    };

    //-------------------------------------------------------------------------
    // NOTE: functions below may only be called on the main thread.
    //-------------------------------------------------------------------------

    nsHttpConnectionMgr();

    nsresult Init(PRUint16 maxConnections,
                  PRUint16 maxConnectionsPerHost,
                  PRUint16 maxConnectionsPerProxy,
                  PRUint16 maxPersistentConnectionsPerHost,
                  PRUint16 maxPersistentConnectionsPerProxy,
                  PRUint16 maxRequestDelay,
                  PRUint16 maxPipelinedRequests);
    nsresult Shutdown();

    //-------------------------------------------------------------------------
    // NOTE: functions below may be called on any thread.
    //-------------------------------------------------------------------------

    // Schedules next pruning of dead connection to happen after
    // given time.
    void PruneDeadConnectionsAfter(PRUint32 time);

    // Stops timer scheduled for next pruning of dead connections.
    void StopPruneDeadConnectionsTimer();

    // adds a transaction to the list of managed transactions.
    nsresult AddTransaction(nsHttpTransaction *, PRInt32 priority);

    // called to reschedule the given transaction.  it must already have been
    // added to the connection manager via AddTransaction.
    nsresult RescheduleTransaction(nsHttpTransaction *, PRInt32 priority);

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

    // called when a connection is done processing a transaction.  if the 
    // connection can be reused then it will be added to the idle list, else
    // it will be closed.
    nsresult ReclaimConnection(nsHttpConnection *conn);

    // called to update a parameter after the connection manager has already
    // been initialized.
    nsresult UpdateParam(nsParamName name, PRUint16 value);

    //-------------------------------------------------------------------------
    // NOTE: functions below may be called only on the socket thread.
    //-------------------------------------------------------------------------

    // removes the next transaction for the specified connection from the
    // pending transaction queue.
    void AddTransactionToPipeline(nsHttpPipeline *);

    // called to force the transaction queue to be processed once more, giving
    // preference to the specified connection.
    nsresult ProcessPendingQ(nsHttpConnectionInfo *);

    // This is used to force an idle connection to be closed and removed from
    // the idle connection list. It is called when the idle connection detects
    // that the network peer has closed the transport.
    nsresult CloseIdleConnection(nsHttpConnection *);

private:
    virtual ~nsHttpConnectionMgr();
    class nsHalfOpenSocket;
    
    // nsConnectionEntry
    //
    // mCT maps connection info hash key to nsConnectionEntry object, which
    // contains list of active and idle connections as well as the list of
    // pending transactions.
    //
    struct nsConnectionEntry
    {
        nsConnectionEntry(nsHttpConnectionInfo *ci)
            : mConnInfo(ci)
        {
            NS_ADDREF(mConnInfo);
        }
       ~nsConnectionEntry() { NS_RELEASE(mConnInfo); }

        nsHttpConnectionInfo        *mConnInfo;
        nsTArray<nsHttpTransaction*> mPendingQ;    // pending transaction queue
        nsTArray<nsHttpConnection*>  mActiveConns; // active connections
        nsTArray<nsHttpConnection*>  mIdleConns;   // idle persistent connections
        nsTArray<nsHalfOpenSocket*>  mHalfOpens;
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
        NS_DECL_NSAHTTPCONNECTION

        nsConnectionHandle(nsHttpConnection *conn) { NS_ADDREF(mConn = conn); }
        virtual ~nsConnectionHandle();

        nsHttpConnection *mConn;
    };

    // nsHalfOpenSocket is used to hold the state of an opening TCP socket
    // while we wait for it to establish and bind it to a connection

    class nsHalfOpenSocket : public nsIOutputStreamCallback,
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
                         nsHttpTransaction *trans);
        ~nsHalfOpenSocket();
        
        nsresult SetupStreams(nsISocketTransport **,
                              nsIAsyncInputStream **,
                              nsIAsyncOutputStream **,
                              PRBool isBackup);
        nsresult SetupPrimaryStreams();
        nsresult SetupBackupStreams();
        void     SetupBackupTimer();
        void     Abandon();
        
        nsHttpTransaction *Transaction() { return mTransaction; }

    private:
        nsConnectionEntry              *mEnt;
        nsRefPtr<nsHttpTransaction>    mTransaction;
        nsCOMPtr<nsISocketTransport>   mSocketTransport;
        nsCOMPtr<nsIAsyncOutputStream> mStreamOut;
        nsCOMPtr<nsIAsyncInputStream>  mStreamIn;

        // for syn retry
        nsCOMPtr<nsITimer>             mSynTimer;
        nsCOMPtr<nsISocketTransport>   mBackupTransport;
        nsCOMPtr<nsIAsyncOutputStream> mBackupStreamOut;
        nsCOMPtr<nsIAsyncInputStream>  mBackupStreamIn;
    };
    friend class nsHalfOpenSocket;

    //-------------------------------------------------------------------------
    // NOTE: these members may be accessed from any thread (use mReentrantMonitor)
    //-------------------------------------------------------------------------

    PRInt32                      mRef;
    mozilla::ReentrantMonitor    mReentrantMonitor;
    nsCOMPtr<nsIEventTarget>     mSocketThreadTarget;

    // connection limits
    PRUint16 mMaxConns;
    PRUint16 mMaxConnsPerHost;
    PRUint16 mMaxConnsPerProxy;
    PRUint16 mMaxPersistConnsPerHost;
    PRUint16 mMaxPersistConnsPerProxy;
    PRUint16 mMaxRequestDelay; // in seconds
    PRUint16 mMaxPipelinedRequests;

    PRPackedBool mIsShuttingDown;

    //-------------------------------------------------------------------------
    // NOTE: these members are only accessed on the socket transport thread
    //-------------------------------------------------------------------------

    static PLDHashOperator ProcessOneTransactionCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);

    static PLDHashOperator PruneDeadConnectionsCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    static PLDHashOperator ShutdownPassCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    static PLDHashOperator PurgeExcessIdleConnectionsCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    static PLDHashOperator ClosePersistentConnectionsCB(const nsACString &, nsAutoPtr<nsConnectionEntry> &, void *);
    PRBool   ProcessPendingQForEntry(nsConnectionEntry *);
    PRBool   AtActiveConnectionLimit(nsConnectionEntry *, PRUint8 caps);
    void     GetConnection(nsConnectionEntry *, nsHttpTransaction *,
                           PRBool, nsHttpConnection **);
    nsresult DispatchTransaction(nsConnectionEntry *, nsAHttpTransaction *,
                                 PRUint8 caps, nsHttpConnection *);
    PRBool   BuildPipeline(nsConnectionEntry *, nsAHttpTransaction *, nsHttpPipeline **);
    nsresult ProcessNewTransaction(nsHttpTransaction *);
    nsresult EnsureSocketThreadTargetIfOnline();
    void     ClosePersistentConnections(nsConnectionEntry *ent);
    nsresult CreateTransport(nsConnectionEntry *, nsHttpTransaction *);
    void     AddActiveConn(nsHttpConnection *, nsConnectionEntry *);
    void     StartedConnect();
    void     RecvdConnect();
    
    // message handlers have this signature
    typedef void (nsHttpConnectionMgr:: *nsConnEventHandler)(PRInt32, void *);

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
                    PRInt32 iparam,
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
        PRInt32              mIParam;
        void                *mVParam;
    };

    nsresult PostEvent(nsConnEventHandler  handler,
                       PRInt32             iparam = 0,
                       void               *vparam = nsnull);

    // message handlers
    void OnMsgShutdown             (PRInt32, void *);
    void OnMsgNewTransaction       (PRInt32, void *);
    void OnMsgReschedTransaction   (PRInt32, void *);
    void OnMsgCancelTransaction    (PRInt32, void *);
    void OnMsgProcessPendingQ      (PRInt32, void *);
    void OnMsgPruneDeadConnections (PRInt32, void *);
    void OnMsgReclaimConnection    (PRInt32, void *);
    void OnMsgUpdateParam          (PRInt32, void *);
    void OnMsgClosePersistentConnections (PRInt32, void *);

    // Total number of active connections in all of the ConnectionEntry objects
    // that are accessed from mCT connection table.
    PRUint16 mNumActiveConns;
    // Total number of idle connections in all of the ConnectionEntry objects
    // that are accessed from mCT connection table.
    PRUint16 mNumIdleConns;

    // Holds time in seconds for next wake-up to prune dead connections. 
    PRUint64 mTimeOfNextWakeUp;
    // Timer for next pruning of dead connections.
    nsCOMPtr<nsITimer> mTimer;

    //
    // the connection table
    //
    // this table is indexed by connection key.  each entry is a
    // nsConnectionEntry object.
    //
    nsClassHashtable<nsCStringHashKey, nsConnectionEntry> mCT;
};

#endif // !nsHttpConnectionMgr_h__
