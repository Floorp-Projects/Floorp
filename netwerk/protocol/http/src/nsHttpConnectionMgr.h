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
#include "nsHttpTransaction.h"
#include "nsHashtable.h"
#include "prmon.h"

#include "nsISocketTransportService.h"

//-----------------------------------------------------------------------------

class nsHttpConnection;
class nsHttpPipeline;

//-----------------------------------------------------------------------------

class nsHttpConnectionMgr : public nsISocketEventHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISOCKETEVENTHANDLER

    //-------------------------------------------------------------------------
    // NOTE: functions below may only be called on the main thread.
    //-------------------------------------------------------------------------

    nsHttpConnectionMgr();
    virtual ~nsHttpConnectionMgr();

    // XXX cleanup the way we pass around prefs.
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

    // adds a transaction to the list of managed transactions.
    nsresult AddTransaction(nsHttpTransaction *);

    // cancels a transaction w/ the given reason.
    nsresult CancelTransaction(nsHttpTransaction *, nsresult reason);

    // called to force the connection manager to prune its list of idle
    // connections.
    nsresult PruneDeadConnections();

    // called to get a reference to the socket transport service.  the socket
    // transport service is not available when the connection manager is down.
    nsresult GetSTS(nsISocketTransportService **);

    //-------------------------------------------------------------------------
    // NOTE: functions below may be called only on the socket thread.
    //-------------------------------------------------------------------------

    // removes the next transaction for the specified connection from the
    // pending transaction queue.
    void     AddTransactionToPipeline(nsHttpPipeline *);

    // called by a connection when it has been closed or when it becomes idle.
    nsresult ReclaimConnection(nsHttpConnection *conn);

    // called to force the transaction queue to be processed once more, giving
    // preference to the specified connection.
    nsresult ProcessPendingQ(nsHttpConnectionInfo *);

private:

    enum {
        MSG_SHUTDOWN,
        MSG_NEW_TRANSACTION,
        MSG_CANCEL_TRANSACTION,
        MSG_PROCESS_PENDING_Q,
        MSG_PRUNE_DEAD_CONNECTIONS
    };

    struct nsConnectionEntry
    {
        nsConnectionEntry(nsHttpConnectionInfo *ci)
            : mConnInfo(ci)
        {
            NS_ADDREF(mConnInfo);
        }
       ~nsConnectionEntry() { NS_RELEASE(mConnInfo); }

        nsHttpConnectionInfo *mConnInfo;
        nsVoidArray           mPendingQ;    // pending transaction queue
        nsVoidArray           mActiveConns; // active connections
        nsVoidArray           mIdleConns;   // idle persistent connections
    };

    //-------------------------------------------------------------------------
    // NOTE: these members may be accessed from any thread (use mMonitor)
    //-------------------------------------------------------------------------

    PRMonitor                           *mMonitor;
    nsCOMPtr<nsISocketTransportService>  mSTS; // cached reference to STS

    // connection limits
    PRUint16 mMaxConns;
    PRUint16 mMaxConnsPerHost;
    PRUint16 mMaxConnsPerProxy;
    PRUint16 mMaxPersistConnsPerHost;
    PRUint16 mMaxPersistConnsPerProxy;
    PRUint16 mMaxRequestDelay; // in seconds
    PRUint16 mMaxPipelinedRequests;

    //-------------------------------------------------------------------------
    // NOTE: these members are only accessed on the socket transport thread
    //-------------------------------------------------------------------------

    static PRIntn PR_CALLBACK ProcessOneTransactionCB(nsHashKey *, void *, void *);
    static PRIntn PR_CALLBACK PurgeOneIdleConnectionCB(nsHashKey *, void *, void *);
    static PRIntn PR_CALLBACK PruneDeadConnectionsCB(nsHashKey *, void *, void *);
    static PRIntn PR_CALLBACK ShutdownPassCB(nsHashKey *, void *, void *);

    PRBool   ProcessPendingQForEntry(nsConnectionEntry *);
    PRBool   AtActiveConnectionLimit(nsConnectionEntry *, PRUint8 caps);
    void     GetConnection(nsConnectionEntry *, PRUint8 caps, nsHttpConnection **);
    nsresult DispatchTransaction(nsConnectionEntry *, nsAHttpTransaction *,
                                 PRUint8 caps, nsHttpConnection *);
    PRBool   BuildPipeline(nsConnectionEntry *, nsAHttpTransaction *, nsHttpPipeline **);
    nsresult PostEvent(PRUint32 type, PRUint32 uparam, void *vparam);

    void     OnMsgShutdown();
    nsresult OnMsgNewTransaction(nsHttpTransaction *);
    void     OnMsgCancelTransaction(nsHttpTransaction *trans, nsresult reason);
    void     OnMsgProcessPendingQ(nsHttpConnectionInfo *ci);
    void     OnMsgPruneDeadConnections();

    // counters
    PRUint16 mNumActiveConns;
    PRUint16 mNumIdleConns;

    //
    // the connection table
    //
    // this table is indexed by connection key.  each entry is a
    // ConnectionEntry object.
    //
    nsHashtable mCT;
};

#endif // !nsHttpConnectionMgr_h__
