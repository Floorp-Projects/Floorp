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

#include "nsHttpConnectionMgr.h"
#include "nsHttpConnection.h"
#include "nsHttpPipeline.h"
#include "nsAutoLock.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"

#include "nsIServiceManager.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//-----------------------------------------------------------------------------

nsHttpConnectionMgr::nsHttpConnectionMgr()
    : mMonitor(nsAutoMonitor::NewMonitor("nsHttpConnectionMgr"))
    , mMaxConns(0)
    , mMaxConnsPerHost(0)
    , mMaxConnsPerProxy(0)
    , mMaxPersistConnsPerHost(0)
    , mMaxPersistConnsPerProxy(0)
    , mNumActiveConns(0)
    , mNumIdleConns(0)
{
    LOG(("Creating nsHttpConnectionMgr @%x\n", this));
}

nsHttpConnectionMgr::~nsHttpConnectionMgr()
{
    LOG(("Destroying nsHttpConnectionMgr @%x\n", this));
 
    if (mMonitor)
        nsAutoMonitor::DestroyMonitor(mMonitor);
}

nsresult
nsHttpConnectionMgr::Init(PRUint16 maxConns,
                          PRUint16 maxConnsPerHost,
                          PRUint16 maxConnsPerProxy,
                          PRUint16 maxPersistConnsPerHost,
                          PRUint16 maxPersistConnsPerProxy,
                          PRUint16 maxRequestDelay,
                          PRUint16 maxPipelinedRequests)
{
    LOG(("nsHttpConnectionMgr::Init\n"));

    nsAutoMonitor mon(mMonitor);

    // do nothing if already initialized
    if (mSTS)
        return NS_OK;

    // no need to do any special synchronization here since there cannot be
    // any activity on the socket thread (because Shutdown is synchronous).
    mMaxConns = maxConns;
    mMaxConnsPerHost = maxConnsPerHost;
    mMaxConnsPerProxy = maxConnsPerProxy;
    mMaxPersistConnsPerHost = maxPersistConnsPerHost;
    mMaxPersistConnsPerProxy = maxPersistConnsPerProxy;
    mMaxRequestDelay = maxRequestDelay;
    mMaxPipelinedRequests = maxPipelinedRequests;

    nsresult rv;
    mSTS = do_GetService(kSocketTransportServiceCID, &rv);
    return rv;
}

nsresult
nsHttpConnectionMgr::Shutdown()
{
    LOG(("nsHttpConnectionMgr::Shutdown\n"));

    nsAutoMonitor mon(mMonitor);

    // do nothing if already shutdown
    if (!mSTS)
        return NS_OK;

    nsresult rv = mSTS->PostEvent(this, MSG_SHUTDOWN, 0, nsnull);

    // release our reference to the STS to prevent further events
    // from being posted.  this is how we indicate that we are
    // shutting down.
    mSTS = 0;

    if (NS_FAILED(rv)) {
        NS_WARNING("unable to post SHUTDOWN message\n");
        return rv;
    }

    // wait for shutdown event to complete
    mon.Wait();
    return NS_OK;
}

nsresult
nsHttpConnectionMgr::PostEvent(PRUint32 type, PRUint32 uparam, void *vparam)
{
    nsAutoMonitor mon(mMonitor);

    NS_ENSURE_TRUE(mSTS, NS_ERROR_NOT_INITIALIZED);

    return mSTS->PostEvent(this, type, uparam, vparam);
}

//-----------------------------------------------------------------------------

nsresult
nsHttpConnectionMgr::AddTransaction(nsHttpTransaction *trans)
{
    LOG(("nsHttpConnectionMgr::AddTransaction [trans=%x]\n", trans));

    NS_ADDREF(trans);

    nsresult rv = PostEvent(MSG_NEW_TRANSACTION, 0, trans);
    if (NS_FAILED(rv))
        NS_RELEASE(trans);

    // if successful, then OnSocketEvent will be called
    return rv;
}

nsresult
nsHttpConnectionMgr::CancelTransaction(nsHttpTransaction *trans, nsresult reason)
{
    LOG(("nsHttpConnectionMgr::CancelTransaction [trans=%x reason=%x]\n", trans, reason));

    NS_ADDREF(trans);

    nsresult rv = PostEvent(MSG_CANCEL_TRANSACTION, reason, trans);
    if (NS_FAILED(rv))
        NS_RELEASE(trans);

    // if successful, then OnSocketEvent will be called
    return rv;
}

nsresult
nsHttpConnectionMgr::PruneDeadConnections()
{
    return PostEvent(MSG_PRUNE_DEAD_CONNECTIONS, 0, nsnull);
    // if successful, then OnSocketEvent will be called
}

nsresult
nsHttpConnectionMgr::GetSTS(nsISocketTransportService **sts)
{
    nsAutoMonitor mon(mMonitor);
    NS_IF_ADDREF(*sts = mSTS);
    return NS_OK;
}

void
nsHttpConnectionMgr::AddTransactionToPipeline(nsHttpPipeline *pipeline)
{
    LOG(("nsHttpConnectionMgr::AddTransactionToPipeline [pipeline=%x]\n", pipeline));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsHttpConnectionInfo *ci = nsnull;
    pipeline->GetConnectionInfo(&ci);
    if (ci) {
        nsCStringKey key(ci->HashKey());
        nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);
        if (ent) {
            // search for another request to pipeline...
            PRInt32 i, count = ent->mPendingQ.Count();
            for (i=0; i<count; ++i) {
                nsHttpTransaction *trans = (nsHttpTransaction *) ent->mPendingQ[i];
                if (trans->Caps() & NS_HTTP_ALLOW_PIPELINING) {
                    pipeline->AddTransaction(trans);

                    // remove transaction from pending queue
                    ent->mPendingQ.RemoveElementAt(i);
                    NS_RELEASE(trans);
                    break;
                }
            }
        }
    }
}

nsresult
nsHttpConnectionMgr::ReclaimConnection(nsHttpConnection *conn)
{
    LOG(("nsHttpConnectionMgr::ReclaimConnection [conn=%x]\n", conn));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // 
    // 1) remove the connection from the active list
    // 2) if keep-alive, add connection to idle list
    // 3) post event to process the pending transaction queue
    //

    nsHttpConnectionInfo *ci = conn->ConnectionInfo();

    nsCStringKey key(ci->HashKey());
    nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);

    NS_ASSERTION(ent, "no connection entry");
    if (ent) {
        ent->mActiveConns.RemoveElement(conn);
        mNumActiveConns--;
        if (conn->CanReuse()) {
            LOG(("  adding connection to idle list\n"));
            // hold onto this connection in the idle list.  we push it to
            // the end of the list so as to ensure that we'll visit older
            // connections first before getting to this one.
            ent->mIdleConns.AppendElement(conn);
            mNumIdleConns++;
        }
        else {
            LOG(("  connection cannot be reused; closing connection\n"));
            // make sure the connection is closed and release our reference.
            conn->Close(NS_ERROR_ABORT);
            NS_RELEASE(conn);
        }
    }
 
    return ProcessPendingQ(ci);
}

nsresult
nsHttpConnectionMgr::ProcessPendingQ(nsHttpConnectionInfo *ci)
{
    LOG(("nsHttpConnectionMgr::ProcessPendingQ [ci=%s]\n", ci->HashKey().get()));

    NS_ADDREF(ci);
    nsresult rv = PostEvent(MSG_PROCESS_PENDING_Q, 0, ci);
    if (NS_FAILED(rv))
        NS_RELEASE(ci);

    return rv;
}

//-----------------------------------------------------------------------------
// enumeration callbacks

PRIntn PR_CALLBACK
nsHttpConnectionMgr::ProcessOneTransactionCB(nsHashKey *key, void *data, void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    nsConnectionEntry *ent = (nsConnectionEntry *) data;

    if (self->ProcessPendingQForEntry(ent))
        return kHashEnumerateStop;

    return kHashEnumerateNext;
}

PRIntn PR_CALLBACK
nsHttpConnectionMgr::PurgeOneIdleConnectionCB(nsHashKey *key, void *data, void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    nsConnectionEntry *ent = (nsConnectionEntry *) data;

    if (ent->mIdleConns.Count() > 0) {
        nsHttpConnection *conn = (nsHttpConnection *) ent->mIdleConns[0];
        ent->mIdleConns.RemoveElementAt(0);
        conn->Close(NS_ERROR_ABORT);
        NS_RELEASE(conn);
        self->mNumIdleConns--;
        return kHashEnumerateStop;
    }

    return kHashEnumerateNext;
}

PRIntn PR_CALLBACK
nsHttpConnectionMgr::PruneDeadConnectionsCB(nsHashKey *key, void *data, void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    nsConnectionEntry *ent = (nsConnectionEntry *) data;

    LOG(("  pruning [ci=%s]\n", ent->mConnInfo->HashKey().get()));

    PRInt32 count = ent->mIdleConns.Count();
    if (count > 0) {
        for (PRInt32 i=count-1; i>=0; --i) {
            nsHttpConnection *conn = (nsHttpConnection *) ent->mIdleConns[i];
            if (!conn->CanReuse()) {
                ent->mIdleConns.RemoveElementAt(i);
                conn->Close(NS_ERROR_ABORT);
                NS_RELEASE(conn);
                self->mNumIdleConns--;
            }
        }
    }

    // if this entry is empty, then we can remove it.
    if (ent->mIdleConns.Count()   == 0 &&
        ent->mActiveConns.Count() == 0 &&
        ent->mPendingQ.Count()    == 0) {
        LOG(("    removing empty connection entry\n"));
        return kHashEnumerateRemove;
    }

    // else, use this opportunity to compact our arrays...
    ent->mIdleConns.Compact();
    ent->mActiveConns.Compact();
    ent->mPendingQ.Compact();

    return kHashEnumerateNext;
}

PRIntn PR_CALLBACK
nsHttpConnectionMgr::ShutdownPassCB(nsHashKey *key, void *data, void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    nsConnectionEntry *ent = (nsConnectionEntry *) data;

    nsHttpTransaction *trans;
    nsHttpConnection *conn;

    // close all active connections
    while (ent->mActiveConns.Count()) {
        conn = (nsHttpConnection *) ent->mActiveConns[0];

        ent->mActiveConns.RemoveElementAt(0);
        self->mNumActiveConns--;

        conn->Close(NS_ERROR_ABORT);
        NS_RELEASE(conn);
    }

    // close all idle connections
    while (ent->mIdleConns.Count()) {
        conn = (nsHttpConnection *) ent->mIdleConns[0];

        ent->mIdleConns.RemoveElementAt(0);
        self->mNumIdleConns--;

        conn->Close(NS_ERROR_ABORT);
        NS_RELEASE(conn);
    }

    // close all pending transactions
    while (ent->mPendingQ.Count()) {
        trans = (nsHttpTransaction *) ent->mPendingQ[0];

        ent->mPendingQ.RemoveElementAt(0);

        trans->Close(NS_ERROR_ABORT);
        NS_RELEASE(trans);
    }

    delete ent;
    return kHashEnumerateRemove;
}

//-----------------------------------------------------------------------------

PRBool
nsHttpConnectionMgr::ProcessPendingQForEntry(nsConnectionEntry *ent)
{
    LOG(("nsHttpConnectionMgr::ProcessPendingQForEntry [ci=%s]\n",
        ent->mConnInfo->HashKey().get()));

    PRInt32 i, count = ent->mPendingQ.Count();
    if (count > 0) {
        LOG(("  pending-count=%u\n", count));
        nsHttpTransaction *trans = nsnull;
        nsHttpConnection *conn = nsnull;
        for (i=0; i<count; ++i) {
            trans = (nsHttpTransaction *) ent->mPendingQ[i];
            GetConnection(ent, trans->Caps(), &conn);
            if (conn)
                break;
        }
        if (conn) {
            LOG(("  dispatching pending transaction...\n"));

            // remove pending transaction
            ent->mPendingQ.RemoveElementAt(i);

            nsresult rv = DispatchTransaction(ent, trans, trans->Caps(), conn);
            if (NS_SUCCEEDED(rv))
                NS_RELEASE(trans);
            else {
                LOG(("  DispatchTransaction failed [rv=%x]\n", rv));
                // on failure, just put the transaction back
                ent->mPendingQ.InsertElementAt(trans, i);
                // might be something wrong with the connection... close it.
                conn->Close(rv);
            }

            NS_RELEASE(conn);
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

// we're at the active connection limit if any one of the following conditions is true:
//  (1) at max-connections
//  (2) keep-alive enabled and at max-persistent-connections-per-server/proxy
//  (3) keep-alive disabled and at max-connections-per-server
PRBool
nsHttpConnectionMgr::AtActiveConnectionLimit(nsConnectionEntry *ent, PRUint8 caps)
{
    nsHttpConnectionInfo *ci = ent->mConnInfo;

    LOG(("nsHttpConnectionMgr::AtActiveConnectionLimit [ci=%s caps=%x]\n",
        ci->HashKey().get(), caps));

    // use >= just to be safe
    if (mNumActiveConns >= mMaxConns) {
        LOG(("  num active conns == max conns\n"));
        return PR_TRUE;
    }

    nsHttpConnection *conn;
    PRInt32 i, totalCount, persistCount = 0;
    
    totalCount = ent->mActiveConns.Count();

    // count the number of persistent connections
    for (i=0; i<totalCount; ++i) {
        conn = (nsHttpConnection *) ent->mActiveConns[i];
        if (conn->IsKeepAlive()) // XXX make sure this is thread-safe
            persistCount++;
    }

    LOG(("   total=%d, persist=%d\n", totalCount, persistCount));

    PRUint16 maxConns;
    PRUint16 maxPersistConns;

    if (ci->UsingHttpProxy() && !ci->UsingSSL()) {
        maxConns = mMaxConnsPerProxy;
        maxPersistConns = mMaxPersistConnsPerProxy;
    }
    else {
        maxConns = mMaxConnsPerHost;
        maxPersistConns = mMaxPersistConnsPerHost;
    }

    // use >= just to be safe
    return (totalCount >= maxConns) || ( (caps & NS_HTTP_ALLOW_KEEPALIVE) &&
                                         (persistCount >= maxPersistConns) );
}

void
nsHttpConnectionMgr::GetConnection(nsConnectionEntry *ent, PRUint8 caps,
                                   nsHttpConnection **result)
{
    LOG(("nsHttpConnectionMgr::GetConnection [ci=%s caps=%x]\n",
        ent->mConnInfo->HashKey().get(), PRUint32(caps)));

    *result = nsnull;

    if (AtActiveConnectionLimit(ent, caps)) {
        LOG(("  at active connection limit!\n"));
        return;
    }

    nsHttpConnection *conn = nsnull;

    if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
        // search the idle connection list
        while (!conn && (ent->mIdleConns.Count() > 0)) {
            conn = (nsHttpConnection *) ent->mIdleConns[0];
            // we check if the connection can be reused before even checking if
            // it is a "matching" connection.
            if (!conn->CanReuse()) {
                LOG(("   dropping stale connection: [conn=%x]\n", conn));
                conn->Close(NS_ERROR_ABORT);
                NS_RELEASE(conn);
            }
            else
                LOG(("   reusing connection [conn=%x]\n", conn));
            ent->mIdleConns.RemoveElementAt(0);
            mNumIdleConns--;
        }
    }

    if (!conn) {
        conn = new nsHttpConnection();
        if (!conn)
            return;
        NS_ADDREF(conn);

        nsresult rv = conn->Init(ent->mConnInfo, mMaxRequestDelay);
        if (NS_FAILED(rv)) {
            NS_RELEASE(conn);
            return;
        }
        
        // We created a new connection that will become active, purge the
        // oldest idle connection if we've reached the upper limit.
        if (mNumIdleConns + mNumActiveConns + 1 > mMaxConns)
            mCT.Enumerate(PurgeOneIdleConnectionCB, this);

        // XXX this just purges a random idle connection.  we should instead
        // enumerate the entire hash table to find the eldest idle connection.
    }

    *result = conn;
}

nsresult
nsHttpConnectionMgr::DispatchTransaction(nsConnectionEntry *ent,
                                         nsAHttpTransaction *trans,
                                         PRUint8 caps,
                                         nsHttpConnection *conn)
{
    LOG(("nsHttpConnectionMgr::DispatchTransaction [ci=%s trans=%x caps=%x conn=%x]\n",
        ent->mConnInfo->HashKey().get(), trans, caps, conn));

    nsHttpPipeline *pipeline = nsnull;
    if (conn->SupportsPipelining() && (caps & NS_HTTP_ALLOW_PIPELINING)) {
        LOG(("  looking to build pipeline...\n"));
        if (BuildPipeline(ent, trans, &pipeline))
            trans = pipeline;
    }

    // hold an owning ref to this connection
    ent->mActiveConns.AppendElement(conn);
    mNumActiveConns++;
    NS_ADDREF(conn);

    nsresult rv = conn->Activate(trans, caps);

    if (NS_FAILED(rv)) {
        LOG(("  conn->Activate failed [rv=%x]\n", rv));
        ent->mActiveConns.RemoveElement(conn);
        mNumActiveConns--;
        NS_RELEASE(conn);
    }

    // if we were unable to activate the pipeline, then this will destroy
    // the pipeline, which will cause each the transactions owned by the 
    // pipeline to be restarted.
    NS_IF_RELEASE(pipeline);

    return rv;
}

PRBool
nsHttpConnectionMgr::BuildPipeline(nsConnectionEntry *ent,
                                   nsAHttpTransaction *firstTrans,
                                   nsHttpPipeline **result)
{
    if (mMaxPipelinedRequests < 2)
        return PR_FALSE;

    nsHttpPipeline *pipeline = nsnull;
    nsHttpTransaction *trans;

    PRInt32 i = 0, numAdded = 0;
    while (i < ent->mPendingQ.Count()) {
        trans = (nsHttpTransaction *) ent->mPendingQ[i];
        if (trans->Caps() & NS_HTTP_ALLOW_PIPELINING) {
            if (numAdded == 0) {
                pipeline = new nsHttpPipeline;
                if (!pipeline)
                    return PR_FALSE;
                pipeline->AddTransaction(firstTrans);
                numAdded = 1;
            }
            pipeline->AddTransaction(trans);

            // remove transaction from pending queue
            ent->mPendingQ.RemoveElementAt(i);
            NS_RELEASE(trans);

            if (++numAdded == mMaxPipelinedRequests)
                break;
        }
        else
            ++i; // skip to next pending transaction
    }

    if (numAdded == 0)
        return PR_FALSE;

    LOG(("  pipelined %u transactions\n", numAdded));
    NS_ADDREF(*result = pipeline);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------

void
nsHttpConnectionMgr::OnMsgShutdown()
{
    LOG(("nsHttpConnectionMgr::OnMsgShutdown\n"));

    mCT.Reset(ShutdownPassCB, this);

    // signal shutdown complete
    nsAutoMonitor mon(mMonitor);
    mon.Notify();
}

nsresult
nsHttpConnectionMgr::OnMsgNewTransaction(nsHttpTransaction *trans)
{
    LOG(("nsHttpConnectionMgr::OnMsgNewTransaction [trans=%x]\n", trans));

    PRUint8 caps = trans->Caps();
    nsHttpConnectionInfo *ci = trans->ConnectionInfo();
    NS_ASSERTION(ci, "no connection info");

    nsCStringKey key(ci->HashKey());
    nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);
    if (!ent) {
        ent = new nsConnectionEntry(ci);
        if (!ent)
            return NS_ERROR_OUT_OF_MEMORY;
        mCT.Put(&key, ent);
    }

    nsHttpConnection *conn;
    GetConnection(ent, caps, &conn);

    nsresult rv;
    if (!conn) {
        LOG(("  adding transaction to pending queue [trans=%x pending-count=%u]\n",
            trans, ent->mPendingQ.Count()+1));
        // put this transaction on the pending queue...
        ent->mPendingQ.AppendElement(trans);
        NS_ADDREF(trans);
        rv = NS_OK;
    }
    else {
        rv = DispatchTransaction(ent, trans, caps, conn);
        NS_RELEASE(conn);
    }

    return rv;
}

void
nsHttpConnectionMgr::OnMsgCancelTransaction(nsHttpTransaction *trans,
                                            nsresult reason)
{
    LOG(("nsHttpConnectionMgr::OnMsgCancelTransaction [trans=%x]\n", trans));

    nsAHttpConnection *conn = trans->Connection();
    if (conn)
        conn->CloseTransaction(trans, reason);
    else {
        // make sure the transaction is not on the pending queue...
        nsHttpConnectionInfo *ci = trans->ConnectionInfo();
        nsCStringKey key(ci->HashKey());
        nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);
        if (ent) {
            PRInt32 index = ent->mPendingQ.IndexOf(trans);
            if (index >= 0) {
                ent->mPendingQ.RemoveElementAt(index);
                nsHttpTransaction *temp = trans;
                NS_RELEASE(temp); // b/c NS_RELEASE nulls its argument!
            }
        }
        trans->Close(reason);
    }
}

void
nsHttpConnectionMgr::OnMsgProcessPendingQ(nsHttpConnectionInfo *ci)
{
    LOG(("nsHttpConnectionMgr::OnMsgProcessPendingQ [ci=%s]\n", ci->HashKey().get()));

    // start by processing the queue identified by the given connection info.
    nsCStringKey key(ci->HashKey());
    nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);
    if (ent && ProcessPendingQForEntry(ent))
        return;

    // if we reach here, it means that we couldn't dispatch a transaction
    // for the specified connection info.  walk the connection table...

    mCT.Enumerate(ProcessOneTransactionCB, this);
}

void
nsHttpConnectionMgr::OnMsgPruneDeadConnections()
{
    LOG(("nsHttpConnectionMgr::OnMsgPruneDeadConnections\n"));

    if (mNumIdleConns > 0) 
        mCT.Enumerate(PruneDeadConnectionsCB, this);
}

//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS1(nsHttpConnectionMgr, nsISocketEventHandler)

// called on the socket thread
NS_IMETHODIMP
nsHttpConnectionMgr::OnSocketEvent(PRUint32 type, PRUint32 uparam, void *vparam)
{
    switch (type) {
    case MSG_SHUTDOWN:
        OnMsgShutdown();
        break;
    case MSG_NEW_TRANSACTION:
        {
            nsHttpTransaction *trans = (nsHttpTransaction *) vparam;

            nsresult rv = OnMsgNewTransaction(trans);
            if (NS_FAILED(rv))
                trans->Close(rv); // for whatever its worth

            NS_RELEASE(trans);
        }
        break;
    case MSG_CANCEL_TRANSACTION:
        {
            nsHttpTransaction *trans = (nsHttpTransaction *) vparam;
            OnMsgCancelTransaction(trans, (nsresult) uparam);
            NS_RELEASE(trans);
        }
        break;
    case MSG_PROCESS_PENDING_Q:
        {
            nsHttpConnectionInfo *ci = (nsHttpConnectionInfo *) vparam;
            OnMsgProcessPendingQ(ci);
            NS_RELEASE(ci);
        }
        break;
    case MSG_PRUNE_DEAD_CONNECTIONS:
        OnMsgPruneDeadConnections();
        break;
    }
    return NS_OK;
}
