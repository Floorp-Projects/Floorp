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

#include "nsHttpConnectionMgr.h"
#include "nsHttpConnection.h"
#include "nsHttpPipeline.h"
#include "nsHttpHandler.h"
#include "nsAutoLock.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"

#include "nsIServiceManager.h"

#include "nsIObserverService.h"

// defined by the socket transport service while active
extern PRThread *gSocketThread;

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//-----------------------------------------------------------------------------


NS_IMPL_THREADSAFE_ISUPPORTS1(nsHttpConnectionMgr, nsIObserver)

static void
InsertTransactionSorted(nsTArray<nsHttpTransaction*> &pendingQ, nsHttpTransaction *trans)
{
    // insert into queue with smallest valued number first.  search in reverse
    // order under the assumption that many of the existing transactions will
    // have the same priority (usually 0).

    for (PRInt32 i=pendingQ.Length()-1; i>=0; --i) {
        nsHttpTransaction *t = pendingQ[i];
        if (trans->Priority() >= t->Priority()) {
            pendingQ.InsertElementAt(i+1, trans);
            return;
        }
    }
    pendingQ.InsertElementAt(0, trans);
}

//-----------------------------------------------------------------------------

nsHttpConnectionMgr::nsHttpConnectionMgr()
    : mRef(0)
    , mMonitor(nsAutoMonitor::NewMonitor("nsHttpConnectionMgr"))
    , mMaxConns(0)
    , mMaxConnsPerHost(0)
    , mMaxConnsPerProxy(0)
    , mMaxPersistConnsPerHost(0)
    , mMaxPersistConnsPerProxy(0)
    , mNumActiveConns(0)
    , mNumIdleConns(0)
    , mTimeOfNextWakeUp(LL_MAXUINT)
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

    nsresult rv;
    nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsAutoMonitor mon(mMonitor);

    // do nothing if already initialized
    if (mSocketThreadTarget)
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

    mSocketThreadTarget = sts;
    return rv;
}

nsresult
nsHttpConnectionMgr::Shutdown()
{
    LOG(("nsHttpConnectionMgr::Shutdown\n"));

    nsAutoMonitor mon(mMonitor);

    // do nothing if already shutdown
    if (!mSocketThreadTarget)
        return NS_OK;

    nsresult rv = PostEvent(&nsHttpConnectionMgr::OnMsgShutdown);

    // release our reference to the STS to prevent further events
    // from being posted.  this is how we indicate that we are
    // shutting down.
    mSocketThreadTarget = 0;

    if (NS_FAILED(rv)) {
        NS_WARNING("unable to post SHUTDOWN message");
        return rv;
    }

    // wait for shutdown event to complete
    mon.Wait();
    return NS_OK;
}

nsresult
nsHttpConnectionMgr::PostEvent(nsConnEventHandler handler, PRInt32 iparam, void *vparam)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv;
    if (!mSocketThreadTarget) {
        NS_WARNING("cannot post event if not initialized");
        rv = NS_ERROR_NOT_INITIALIZED;
    }
    else {
        nsRefPtr<nsIRunnable> event = new nsConnEvent(this, handler, iparam, vparam);
        if (!event)
            rv = NS_ERROR_OUT_OF_MEMORY;
        else
            rv = mSocketThreadTarget->Dispatch(event, NS_DISPATCH_NORMAL);
    }
    return rv;
}

void
nsHttpConnectionMgr::PruneDeadConnectionsAfter(PRUint32 timeInSeconds)
{
    LOG(("nsHttpConnectionMgr::PruneDeadConnectionsAfter\n"));

    if(!mTimer)
        mTimer = do_CreateInstance("@mozilla.org/timer;1");

    // failure to create a timer is not a fatal error, but idle connections
    // will not be cleaned up until we try to use them.
    if (mTimer) {
        mTimeOfNextWakeUp = timeInSeconds + NowInSeconds();
        mTimer->Init(this, timeInSeconds*1000, nsITimer::TYPE_ONE_SHOT);
    } else {
        NS_WARNING("failed to create: timer for pruning the dead connections!");
    }
}

void
nsHttpConnectionMgr::StopPruneDeadConnectionsTimer()
{
    LOG(("nsHttpConnectionMgr::StopPruneDeadConnectionsTimer\n"));

    // Reset mTimeOfNextWakeUp so that we can find a new shortest value.
    mTimeOfNextWakeUp = LL_MAXUINT;
    if (mTimer) {
        mTimer->Cancel();
        mTimer = NULL;
    }
}

//-----------------------------------------------------------------------------
// nsHttpConnectionMgr::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpConnectionMgr::Observe(nsISupports *subject,
                             const char *topic,
                             const PRUnichar *data)
{
    LOG(("nsHttpConnectionMgr::Observe [topic=\"%s\"]\n", topic));

    if (0 == strcmp(topic, "timer-callback")) {
        // prune dead connections
        PruneDeadConnections();
#ifdef DEBUG
        nsCOMPtr<nsITimer> timer = do_QueryInterface(subject);
        NS_ASSERTION(timer == mTimer, "unexpected timer-callback");
#endif
    }

    return NS_OK;
}


//-----------------------------------------------------------------------------

nsresult
nsHttpConnectionMgr::AddTransaction(nsHttpTransaction *trans, PRInt32 priority)
{
    LOG(("nsHttpConnectionMgr::AddTransaction [trans=%x %d]\n", trans, priority));

    NS_ADDREF(trans);
    nsresult rv = PostEvent(&nsHttpConnectionMgr::OnMsgNewTransaction, priority, trans);
    if (NS_FAILED(rv))
        NS_RELEASE(trans);
    return rv;
}

nsresult
nsHttpConnectionMgr::RescheduleTransaction(nsHttpTransaction *trans, PRInt32 priority)
{
    LOG(("nsHttpConnectionMgr::RescheduleTransaction [trans=%x %d]\n", trans, priority));

    NS_ADDREF(trans);
    nsresult rv = PostEvent(&nsHttpConnectionMgr::OnMsgReschedTransaction, priority, trans);
    if (NS_FAILED(rv))
        NS_RELEASE(trans);
    return rv;
}

nsresult
nsHttpConnectionMgr::CancelTransaction(nsHttpTransaction *trans, nsresult reason)
{
    LOG(("nsHttpConnectionMgr::CancelTransaction [trans=%x reason=%x]\n", trans, reason));

    NS_ADDREF(trans);
    nsresult rv = PostEvent(&nsHttpConnectionMgr::OnMsgCancelTransaction, reason, trans);
    if (NS_FAILED(rv))
        NS_RELEASE(trans);
    return rv;
}

nsresult
nsHttpConnectionMgr::PruneDeadConnections()
{
    return PostEvent(&nsHttpConnectionMgr::OnMsgPruneDeadConnections);
}

nsresult
nsHttpConnectionMgr::GetSocketThreadTarget(nsIEventTarget **target)
{
    nsAutoMonitor mon(mMonitor);
    NS_IF_ADDREF(*target = mSocketThreadTarget);
    return NS_OK;
}

void
nsHttpConnectionMgr::AddTransactionToPipeline(nsHttpPipeline *pipeline)
{
    LOG(("nsHttpConnectionMgr::AddTransactionToPipeline [pipeline=%x]\n", pipeline));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsRefPtr<nsHttpConnectionInfo> ci;
    pipeline->GetConnectionInfo(getter_AddRefs(ci));
    if (ci) {
        nsCStringKey key(ci->HashKey());
        nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);
        if (ent) {
            // search for another request to pipeline...
            PRInt32 i, count = ent->mPendingQ.Length();
            for (i=0; i<count; ++i) {
                nsHttpTransaction *trans = ent->mPendingQ[i];
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

    NS_ADDREF(conn);
    nsresult rv = PostEvent(&nsHttpConnectionMgr::OnMsgReclaimConnection, 0, conn);
    if (NS_FAILED(rv))
        NS_RELEASE(conn);
    return rv;
}

nsresult
nsHttpConnectionMgr::UpdateParam(nsParamName name, PRUint16 value)
{
    PRUint32 param = (PRUint32(name) << 16) | PRUint32(value);
    return PostEvent(&nsHttpConnectionMgr::OnMsgUpdateParam, 0, (void *) param);
}

nsresult
nsHttpConnectionMgr::ProcessPendingQ(nsHttpConnectionInfo *ci)
{
    LOG(("nsHttpConnectionMgr::ProcessPendingQ [ci=%s]\n", ci->HashKey().get()));

    NS_ADDREF(ci);
    nsresult rv = PostEvent(&nsHttpConnectionMgr::OnMsgProcessPendingQ, 0, ci);
    if (NS_FAILED(rv))
        NS_RELEASE(ci);
    return rv;
}

//-----------------------------------------------------------------------------
// enumeration callbacks

PRIntn
nsHttpConnectionMgr::ProcessOneTransactionCB(nsHashKey *key, void *data, void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    nsConnectionEntry *ent = (nsConnectionEntry *) data;

    if (self->ProcessPendingQForEntry(ent))
        return kHashEnumerateStop;

    return kHashEnumerateNext;
}

PRIntn
nsHttpConnectionMgr::PurgeOneIdleConnectionCB(nsHashKey *key, void *data, void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    nsConnectionEntry *ent = (nsConnectionEntry *) data;

    if (ent->mIdleConns.Length() > 0) {
        nsHttpConnection *conn = ent->mIdleConns[0];
        ent->mIdleConns.RemoveElementAt(0);
        conn->Close(NS_ERROR_ABORT);
        NS_RELEASE(conn);
        self->mNumIdleConns--;
        if (0 == self->mNumIdleConns)
            self->StopPruneDeadConnectionsTimer();
        return kHashEnumerateStop;
    }

    return kHashEnumerateNext;
}

PRIntn
nsHttpConnectionMgr::PruneDeadConnectionsCB(nsHashKey *key, void *data, void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    nsConnectionEntry *ent = (nsConnectionEntry *) data;

    LOG(("  pruning [ci=%s]\n", ent->mConnInfo->HashKey().get()));

    // Find out how long it will take for next idle connection to not be reusable
    // anymore.
    PRUint32 timeToNextExpire = PR_UINT32_MAX;
    PRInt32 count = ent->mIdleConns.Length();
    if (count > 0) {
        for (PRInt32 i=count-1; i>=0; --i) {
            nsHttpConnection *conn = ent->mIdleConns[i];
            if (!conn->CanReuse()) {
                ent->mIdleConns.RemoveElementAt(i);
                conn->Close(NS_ERROR_ABORT);
                NS_RELEASE(conn);
                self->mNumIdleConns--;
            } else {
                timeToNextExpire = PR_MIN(timeToNextExpire, conn->TimeToLive());
            }
        }
    }

    // If time to next expire found is shorter than time to next wake-up, we need to
    // change the time for next wake-up.
    PRUint32 now = NowInSeconds();
    if (0 < ent->mIdleConns.Length()) {
        PRUint64 timeOfNextExpire = now + timeToNextExpire;
        // If pruning of dead connections is not already scheduled to happen
        // or time found for next connection to expire is is before
        // mTimeOfNextWakeUp, we need to schedule the pruning to happen
        // after timeToNextExpire.
        if (!self->mTimer || timeOfNextExpire < self->mTimeOfNextWakeUp) {
            self->PruneDeadConnectionsAfter(timeToNextExpire);
        }
    } else if (0 == self->mNumIdleConns) {
        self->StopPruneDeadConnectionsTimer();
    }
#ifdef DEBUG
    count = ent->mActiveConns.Length();
    if (count > 0) {
        for (PRInt32 i=count-1; i>=0; --i) {
            nsHttpConnection *conn = ent->mActiveConns[i];
            LOG(("    active conn [%x] with trans [%x]\n", conn, conn->Transaction()));
        }
    }
#endif

    // if this entry is empty, then we can remove it.
    if (ent->mIdleConns.Length()   == 0 &&
        ent->mActiveConns.Length() == 0 &&
        ent->mPendingQ.Length()    == 0) {
        LOG(("    removing empty connection entry\n"));
        delete ent;
        return kHashEnumerateRemove;
    }

    // else, use this opportunity to compact our arrays...
    ent->mIdleConns.Compact();
    ent->mActiveConns.Compact();
    ent->mPendingQ.Compact();

    return kHashEnumerateNext;
}

PRIntn
nsHttpConnectionMgr::ShutdownPassCB(nsHashKey *key, void *data, void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    nsConnectionEntry *ent = (nsConnectionEntry *) data;

    nsHttpTransaction *trans;
    nsHttpConnection *conn;

    // close all active connections
    while (ent->mActiveConns.Length()) {
        conn = ent->mActiveConns[0];

        ent->mActiveConns.RemoveElementAt(0);
        self->mNumActiveConns--;

        conn->Close(NS_ERROR_ABORT);
        NS_RELEASE(conn);
    }

    // close all idle connections
    while (ent->mIdleConns.Length()) {
        conn = ent->mIdleConns[0];

        ent->mIdleConns.RemoveElementAt(0);
        self->mNumIdleConns--;

        conn->Close(NS_ERROR_ABORT);
        NS_RELEASE(conn);
    }
    // If all idle connections are removed,
    // we can stop pruning dead connections.
    if (0 == self->mNumIdleConns)
        self->StopPruneDeadConnectionsTimer();

    // close all pending transactions
    while (ent->mPendingQ.Length()) {
        trans = ent->mPendingQ[0];

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

    PRInt32 i, count = ent->mPendingQ.Length();
    if (count > 0) {
        LOG(("  pending-count=%u\n", count));
        nsHttpTransaction *trans = nsnull;
        nsHttpConnection *conn = nsnull;
        for (i=0; i<count; ++i) {
            trans = ent->mPendingQ[i];
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
                ent->mPendingQ.InsertElementAt(i, trans);
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

    // If we have more active connections than the limit, then we're done --
    // purging idle connections won't get us below it.
    if (mNumActiveConns >= mMaxConns) {
        LOG(("  num active conns == max conns\n"));
        return PR_TRUE;
    }

    nsHttpConnection *conn;
    PRInt32 i, totalCount, persistCount = 0;
    
    totalCount = ent->mActiveConns.Length();

    // count the number of persistent connections
    for (i=0; i<totalCount; ++i) {
        conn = ent->mActiveConns[i];
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

    nsHttpConnection *conn = nsnull;

    if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
        // search the idle connection list
        while (!conn && (ent->mIdleConns.Length() > 0)) {
            conn = ent->mIdleConns[0];
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

            // If there are no idle connections left at all, we need to make
            // sure that we are not pruning dead connections anymore.
            if (0 == mNumIdleConns)
                StopPruneDeadConnectionsTimer();
        }
    }

    if (!conn) {
        // Check if we need to purge an idle connection. Note that we may have
        // removed one above; if so, this will be a no-op. We do this before
        // checking the active connection limit to catch the case where we do
        // have an idle connection, but the purge timer hasn't fired yet.
        // XXX this just purges a random idle connection.  we should instead
        // enumerate the entire hash table to find the eldest idle connection.
        if (mNumIdleConns && mNumIdleConns + mNumActiveConns + 1 >= mMaxConns)
            mCT.Enumerate(PurgeOneIdleConnectionCB, this);

        // Need to make a new TCP connection. First, we check if we've hit
        // either the maximum connection limit globally or for this particular
        // host or proxy. If we have, we're done.
        if (AtActiveConnectionLimit(ent, caps)) {
            LOG(("  at active connection limit!\n"));
            return;
        }

        conn = new nsHttpConnection();
        if (!conn)
            return;
        NS_ADDREF(conn);

        nsresult rv = conn->Init(ent->mConnInfo, mMaxRequestDelay);
        if (NS_FAILED(rv)) {
            NS_RELEASE(conn);
            return;
        }
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

    nsConnectionHandle *handle = new nsConnectionHandle(conn);
    if (!handle)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(handle);

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

    // give the transaction the indirect reference to the connection.
    trans->SetConnection(handle);

    nsresult rv = conn->Activate(trans, caps);

    if (NS_FAILED(rv)) {
        LOG(("  conn->Activate failed [rv=%x]\n", rv));
        ent->mActiveConns.RemoveElement(conn);
        mNumActiveConns--;
        // sever back references to connection, and do so without triggering
        // a call to ReclaimConnection ;-)
        trans->SetConnection(nsnull);
        NS_RELEASE(handle->mConn);
        // destroy the connection
        NS_RELEASE(conn);
    }

    // if we were unable to activate the pipeline, then this will destroy
    // the pipeline, which will cause each the transactions owned by the 
    // pipeline to be restarted.
    NS_IF_RELEASE(pipeline);

    NS_RELEASE(handle);
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

    PRUint32 i = 0, numAdded = 0;
    while (i < ent->mPendingQ.Length()) {
        trans = ent->mPendingQ[i];
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

nsresult
nsHttpConnectionMgr::ProcessNewTransaction(nsHttpTransaction *trans)
{
    // since "adds" and "cancels" are processed asynchronously and because
    // various events might trigger an "add" directly on the socket thread,
    // we must take care to avoid dispatching a transaction that has already
    // been canceled (see bug 190001).
    if (NS_FAILED(trans->Status())) {
        LOG(("  transaction was canceled... dropping event!\n"));
        return NS_OK;
    }

    PRUint8 caps = trans->Caps();
    nsHttpConnectionInfo *ci = trans->ConnectionInfo();
    NS_ASSERTION(ci, "no connection info");

    nsCStringKey key(ci->HashKey());
    nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);
    if (!ent) {
        nsHttpConnectionInfo *clone = ci->Clone();
        if (!clone)
            return NS_ERROR_OUT_OF_MEMORY;
        ent = new nsConnectionEntry(clone);
        if (!ent)
            return NS_ERROR_OUT_OF_MEMORY;
        mCT.Put(&key, ent);
    }

    nsHttpConnection *conn;

    // check if the transaction already has a sticky reference to a connection.
    // if so, then we can just use it directly.  XXX check if alive??
    // XXX add a TakeConnection method or something to make this clearer!
    nsConnectionHandle *handle = (nsConnectionHandle *) trans->Connection();
    if (handle) {
        NS_ASSERTION(caps & NS_HTTP_STICKY_CONNECTION, "unexpected caps");
        NS_ASSERTION(handle->mConn, "no connection");

        // steal reference from connection handle.
        // XXX prevent SetConnection(nsnull) from calling ReclaimConnection
        conn = handle->mConn;
        handle->mConn = nsnull;

        // destroy connection handle.
        trans->SetConnection(nsnull);

        // remove sticky connection from active connection list; we'll add it
        // right back in DispatchTransaction.
        if (ent->mActiveConns.RemoveElement(conn))
            mNumActiveConns--;
        else {
            NS_ERROR("sticky connection not found in active list");
            return NS_ERROR_UNEXPECTED;
        }
    }
    else
        GetConnection(ent, caps, &conn);

    nsresult rv;
    if (!conn) {
        LOG(("  adding transaction to pending queue [trans=%x pending-count=%u]\n",
            trans, ent->mPendingQ.Length()+1));
        // put this transaction on the pending queue...
        InsertTransactionSorted(ent->mPendingQ, trans);
        NS_ADDREF(trans);
        rv = NS_OK;
    }
    else {
        rv = DispatchTransaction(ent, trans, caps, conn);
        NS_RELEASE(conn);
    }

    return rv;
}

//-----------------------------------------------------------------------------

void
nsHttpConnectionMgr::OnMsgShutdown(PRInt32, void *)
{
    LOG(("nsHttpConnectionMgr::OnMsgShutdown\n"));

    mCT.Reset(ShutdownPassCB, this);

    // signal shutdown complete
    nsAutoMonitor mon(mMonitor);
    mon.Notify();
}

void
nsHttpConnectionMgr::OnMsgNewTransaction(PRInt32 priority, void *param)
{
    LOG(("nsHttpConnectionMgr::OnMsgNewTransaction [trans=%p]\n", param));

    nsHttpTransaction *trans = (nsHttpTransaction *) param;
    trans->SetPriority(priority);
    nsresult rv = ProcessNewTransaction(trans);
    if (NS_FAILED(rv))
        trans->Close(rv); // for whatever its worth
    NS_RELEASE(trans);
}

void
nsHttpConnectionMgr::OnMsgReschedTransaction(PRInt32 priority, void *param)
{
    LOG(("nsHttpConnectionMgr::OnMsgNewTransaction [trans=%p]\n", param));

    nsHttpTransaction *trans = (nsHttpTransaction *) param;
    trans->SetPriority(priority);

    nsHttpConnectionInfo *ci = trans->ConnectionInfo();
    nsCStringKey key(ci->HashKey());
    nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);
    if (ent) {
        PRInt32 index = ent->mPendingQ.IndexOf(trans);
        if (index >= 0) {
            ent->mPendingQ.RemoveElementAt(index);
            InsertTransactionSorted(ent->mPendingQ, trans);
        }
    }

    NS_RELEASE(trans);
}

void
nsHttpConnectionMgr::OnMsgCancelTransaction(PRInt32 reason, void *param)
{
    LOG(("nsHttpConnectionMgr::OnMsgCancelTransaction [trans=%p]\n", param));

    nsHttpTransaction *trans = (nsHttpTransaction *) param;
    //
    // if the transaction owns a connection and the transaction is not done,
    // then ask the connection to close the transaction.  otherwise, close the
    // transaction directly (removing it from the pending queue first).
    //
    nsAHttpConnection *conn = trans->Connection();
    if (conn && !trans->IsDone())
        conn->CloseTransaction(trans, reason);
    else {
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
    NS_RELEASE(trans);
}

void
nsHttpConnectionMgr::OnMsgProcessPendingQ(PRInt32, void *param)
{
    nsHttpConnectionInfo *ci = (nsHttpConnectionInfo *) param;

    LOG(("nsHttpConnectionMgr::OnMsgProcessPendingQ [ci=%s]\n", ci->HashKey().get()));

    // start by processing the queue identified by the given connection info.
    nsCStringKey key(ci->HashKey());
    nsConnectionEntry *ent = (nsConnectionEntry *) mCT.Get(&key);
    if (!(ent && ProcessPendingQForEntry(ent))) {
        // if we reach here, it means that we couldn't dispatch a transaction
        // for the specified connection info.  walk the connection table...
        mCT.Enumerate(ProcessOneTransactionCB, this);
    }

    NS_RELEASE(ci);
}

void
nsHttpConnectionMgr::OnMsgPruneDeadConnections(PRInt32, void *)
{
    LOG(("nsHttpConnectionMgr::OnMsgPruneDeadConnections\n"));

    // Reset mTimeOfNextWakeUp so that we can find a new shortest value.
    mTimeOfNextWakeUp = LL_MAXUINT;
    if (mNumIdleConns > 0) 
        mCT.Enumerate(PruneDeadConnectionsCB, this);
}

void
nsHttpConnectionMgr::OnMsgReclaimConnection(PRInt32, void *param)
{
    LOG(("nsHttpConnectionMgr::OnMsgReclaimConnection [conn=%p]\n", param));

    nsHttpConnection *conn = (nsHttpConnection *) param;

    // 
    // 1) remove the connection from the active list
    // 2) if keep-alive, add connection to idle list
    // 3) post event to process the pending transaction queue
    //

    nsHttpConnectionInfo *ci = conn->ConnectionInfo();
    NS_ADDREF(ci);

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
            // If the added connection was first idle connection or has shortest
            // time to live among the idle connections, pruning dead
            // connections needs to be done when it can't be reused anymore.
            PRUint32 timeToLive = conn->TimeToLive();
            if(!mTimer || NowInSeconds() + timeToLive < mTimeOfNextWakeUp)
                PruneDeadConnectionsAfter(timeToLive);
        }
        else {
            LOG(("  connection cannot be reused; closing connection\n"));
            // make sure the connection is closed and release our reference.
            conn->Close(NS_ERROR_ABORT);
            nsHttpConnection *temp = conn;
            NS_RELEASE(temp);
        }
    }
 
    OnMsgProcessPendingQ(NS_OK, ci); // releases |ci|
    NS_RELEASE(conn);
}

void
nsHttpConnectionMgr::OnMsgUpdateParam(PRInt32, void *param)
{
    PRUint16 name  = (NS_PTR_TO_INT32(param) & 0xFFFF0000) >> 16;
    PRUint16 value =  NS_PTR_TO_INT32(param) & 0x0000FFFF;

    switch (name) {
    case MAX_CONNECTIONS:
        mMaxConns = value;
        break;
    case MAX_CONNECTIONS_PER_HOST:
        mMaxConnsPerHost = value;
        break;
    case MAX_CONNECTIONS_PER_PROXY:
        mMaxConnsPerProxy = value;
        break;
    case MAX_PERSISTENT_CONNECTIONS_PER_HOST:
        mMaxPersistConnsPerHost = value;
        break;
    case MAX_PERSISTENT_CONNECTIONS_PER_PROXY:
        mMaxPersistConnsPerProxy = value;
        break;
    case MAX_REQUEST_DELAY:
        mMaxRequestDelay = value;
        break;
    case MAX_PIPELINED_REQUESTS:
        mMaxPipelinedRequests = value;
        break;
    default:
        NS_NOTREACHED("unexpected parameter name");
    }
}

//-----------------------------------------------------------------------------
// nsHttpConnectionMgr::nsConnectionHandle

nsHttpConnectionMgr::nsConnectionHandle::~nsConnectionHandle()
{
    if (mConn) {
        gHttpHandler->ReclaimConnection(mConn);
        NS_RELEASE(mConn);
    }
}

NS_IMPL_THREADSAFE_ISUPPORTS0(nsHttpConnectionMgr::nsConnectionHandle)

nsresult
nsHttpConnectionMgr::nsConnectionHandle::OnHeadersAvailable(nsAHttpTransaction *trans,
                                                            nsHttpRequestHead *req,
                                                            nsHttpResponseHead *resp,
                                                            PRBool *reset)
{
    return mConn->OnHeadersAvailable(trans, req, resp, reset);
}

nsresult
nsHttpConnectionMgr::nsConnectionHandle::ResumeSend()
{
    return mConn->ResumeSend();
}

nsresult
nsHttpConnectionMgr::nsConnectionHandle::ResumeRecv()
{
    return mConn->ResumeRecv();
}

void
nsHttpConnectionMgr::nsConnectionHandle::CloseTransaction(nsAHttpTransaction *trans, nsresult reason)
{
    mConn->CloseTransaction(trans, reason);
}

void
nsHttpConnectionMgr::nsConnectionHandle::GetConnectionInfo(nsHttpConnectionInfo **result)
{
    mConn->GetConnectionInfo(result);
}

void
nsHttpConnectionMgr::nsConnectionHandle::GetSecurityInfo(nsISupports **result)
{
    mConn->GetSecurityInfo(result);
}

PRBool
nsHttpConnectionMgr::nsConnectionHandle::IsPersistent()
{
    return mConn->IsPersistent();
}

PRBool
nsHttpConnectionMgr::nsConnectionHandle::IsReused()
{
    return mConn->IsReused();
}

nsresult
nsHttpConnectionMgr::nsConnectionHandle::PushBack(const char *buf, PRUint32 bufLen)
{
    return mConn->PushBack(buf, bufLen);
}

