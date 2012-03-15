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
#include "nsNetCID.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"

#include "nsIServiceManager.h"

#include "nsIObserverService.h"

#include "nsISSLSocketControl.h"
#include "prnetdb.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

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
    , mReentrantMonitor("nsHttpConnectionMgr.mReentrantMonitor")
    , mMaxConns(0)
    , mMaxConnsPerHost(0)
    , mMaxConnsPerProxy(0)
    , mMaxPersistConnsPerHost(0)
    , mMaxPersistConnsPerProxy(0)
    , mIsShuttingDown(false)
    , mNumActiveConns(0)
    , mNumIdleConns(0)
    , mTimeOfNextWakeUp(LL_MAXUINT)
    , mReadTimeoutTickArmed(false)
{
    LOG(("Creating nsHttpConnectionMgr @%x\n", this));
    mCT.Init();
    mAlternateProtocolHash.Init(16);
    mSpdyPreferredHash.Init();
}

nsHttpConnectionMgr::~nsHttpConnectionMgr()
{
    LOG(("Destroying nsHttpConnectionMgr @%x\n", this));
    if (mReadTimeoutTick)
        mReadTimeoutTick->Cancel();
}

nsresult
nsHttpConnectionMgr::EnsureSocketThreadTargetIfOnline()
{
    nsresult rv;
    nsCOMPtr<nsIEventTarget> sts;
    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    if (NS_SUCCEEDED(rv)) {
        bool offline = true;
        ioService->GetOffline(&offline);

        if (!offline) {
            sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
        }
    }

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // do nothing if already initialized or if we've shut down
    if (mSocketThreadTarget || mIsShuttingDown)
        return NS_OK;

    mSocketThreadTarget = sts;

    return rv;
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

    {
        ReentrantMonitorAutoEnter mon(mReentrantMonitor);

        mMaxConns = maxConns;
        mMaxConnsPerHost = maxConnsPerHost;
        mMaxConnsPerProxy = maxConnsPerProxy;
        mMaxPersistConnsPerHost = maxPersistConnsPerHost;
        mMaxPersistConnsPerProxy = maxPersistConnsPerProxy;
        mMaxRequestDelay = maxRequestDelay;
        mMaxPipelinedRequests = maxPipelinedRequests;

        mIsShuttingDown = false;
    }

    return EnsureSocketThreadTargetIfOnline();
}

nsresult
nsHttpConnectionMgr::Shutdown()
{
    LOG(("nsHttpConnectionMgr::Shutdown\n"));

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // do nothing if already shutdown
    if (!mSocketThreadTarget)
        return NS_OK;

    nsresult rv = PostEvent(&nsHttpConnectionMgr::OnMsgShutdown);

    // release our reference to the STS to prevent further events
    // from being posted.  this is how we indicate that we are
    // shutting down.
    mIsShuttingDown = true;
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
    // This object doesn't get reinitialized if the offline state changes, so our
    // socket thread target might be uninitialized if we were offline when this
    // object was being initialized, and we go online later on.  This call takes
    // care of initializing the socket thread target if that's the case.
    EnsureSocketThreadTargetIfOnline();

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

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
nsHttpConnectionMgr::ConditionallyStopPruneDeadConnectionsTimer()
{
    // Leave the timer in place if there are connections that potentially
    // need management
    if (mNumIdleConns || (mNumActiveConns && gHttpHandler->IsSpdyEnabled()))
        return;

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

    if (0 == strcmp(topic, NS_TIMER_CALLBACK_TOPIC)) {
        nsCOMPtr<nsITimer> timer = do_QueryInterface(subject);
        if (timer == mTimer) {
            PruneDeadConnections();
        }
        else if (timer == mReadTimeoutTick) {
            ReadTimeoutTick();
        }
        else {
            NS_ABORT_IF_FALSE(false, "unexpected timer-callback");
            LOG(("Unexpected timer object\n"));
            return NS_ERROR_UNEXPECTED;
        }
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
nsHttpConnectionMgr::ClosePersistentConnections()
{
    return PostEvent(&nsHttpConnectionMgr::OnMsgClosePersistentConnections);
}

nsresult
nsHttpConnectionMgr::GetSocketThreadTarget(nsIEventTarget **target)
{
    // This object doesn't get reinitialized if the offline state changes, so our
    // socket thread target might be uninitialized if we were offline when this
    // object was being initialized, and we go online later on.  This call takes
    // care of initializing the socket thread target if that's the case.
    EnsureSocketThreadTargetIfOnline();

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
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
        nsConnectionEntry *ent = mCT.Get(ci->HashKey());
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

// Given a nsHttpConnectionInfo find the connection entry object that
// contains either the nshttpconnection or nshttptransaction parameter.
// Normally this is done by the hashkey lookup of connectioninfo,
// but if spdy coalescing is in play it might be found in a redirected
// entry
nsHttpConnectionMgr::nsConnectionEntry *
nsHttpConnectionMgr::LookupConnectionEntry(nsHttpConnectionInfo *ci,
                                           nsHttpConnection *conn,
                                           nsHttpTransaction *trans)
{
    if (!ci)
        return nsnull;

    nsConnectionEntry *ent = mCT.Get(ci->HashKey());
    
    // If there is no sign of coalescing (or it is disabled) then just
    // return the primary hash lookup
    if (!ent || !ent->mUsingSpdy || ent->mCoalescingKey.IsEmpty())
        return ent;

    // If there is no preferred coalescing entry for this host (or the
    // preferred entry is the one that matched the mCT hash lookup) then
    // there is only option
    nsConnectionEntry *preferred = mSpdyPreferredHash.Get(ent->mCoalescingKey);
    if (!preferred || (preferred == ent))
        return ent;

    if (conn) {
        // The connection could be either in preferred or ent. It is most
        // likely the only active connection in preferred - so start with that.
        if (preferred->mActiveConns.Contains(conn))
            return preferred;
        if (preferred->mIdleConns.Contains(conn))
            return preferred;
    }
    
    if (trans && preferred->mPendingQ.Contains(trans))
        return preferred;
    
    // Neither conn nor trans found in preferred, use the default entry
    return ent;
}

nsresult
nsHttpConnectionMgr::CloseIdleConnection(nsHttpConnection *conn)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    LOG(("nsHttpConnectionMgr::CloseIdleConnection %p conn=%p",
         this, conn));

    if (!conn->ConnectionInfo())
        return NS_ERROR_UNEXPECTED;

    nsConnectionEntry *ent = LookupConnectionEntry(conn->ConnectionInfo(),
                                                   conn, nsnull);

    if (!ent || !ent->mIdleConns.RemoveElement(conn))
        return NS_ERROR_UNEXPECTED;

    conn->Close(NS_ERROR_ABORT);
    NS_RELEASE(conn);
    mNumIdleConns--;
    ConditionallyStopPruneDeadConnectionsTimer();
    return NS_OK;
}

// This function lets a connection, after completing the NPN phase,
// report whether or not it is using spdy through the usingSpdy
// argument. It would not be necessary if NPN were driven out of
// the connection manager. The connection entry associated with the
// connection is then updated to indicate whether or not we want to use
// spdy with that host and update the preliminary preferred host
// entries used for de-sharding hostsnames.
void
nsHttpConnectionMgr::ReportSpdyConnection(nsHttpConnection *conn,
                                          bool usingSpdy)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    
    nsConnectionEntry *ent = LookupConnectionEntry(conn->ConnectionInfo(),
                                                   conn, nsnull);

    if (!ent)
        return;

    ent->mTestedSpdy = true;

    if (!usingSpdy)
        return;
    
    ent->mUsingSpdy = true;

    PRUint32 ttl = conn->TimeToLive();
    PRUint64 timeOfExpire = NowInSeconds() + ttl;
    if (!mTimer || timeOfExpire < mTimeOfNextWakeUp)
        PruneDeadConnectionsAfter(ttl);

    // Lookup preferred directly from the hash instead of using
    // GetSpdyPreferredEnt() because we want to avoid the cert compatibility
    // check at this point because the cert is never part of the hash
    // lookup. Filtering on that has to be done at the time of use
    // rather than the time of registration (i.e. now).
    nsConnectionEntry *preferred =
        mSpdyPreferredHash.Get(ent->mCoalescingKey);

    LOG(("ReportSpdyConnection %s %s ent=%p preferred=%p\n",
         ent->mConnInfo->Host(), ent->mCoalescingKey.get(),
         ent, preferred));
    
    if (!preferred) {
        if (!ent->mCoalescingKey.IsEmpty()) {
            mSpdyPreferredHash.Put(ent->mCoalescingKey, ent);
            ent->mSpdyPreferred = true;
            preferred = ent;
        }
    }
    else if (preferred != ent) {
        // A different hostname is the preferred spdy host for this
        // IP address. That preferred mapping must have been setup while
        // this connection was negotiating NPN.

        // Call don't reuse on the current connection to shut it down as soon
        // as possible without causing any errors.
        // i.e. the current transaction(s) on this connection will be processed
        // normally, but then it will go away and future connections will be
        // coalesced through the preferred entry.

        conn->DontReuse();
    }

    ProcessAllSpdyPendingQ();
}

bool
nsHttpConnectionMgr::GetSpdyAlternateProtocol(nsACString &hostPortKey)
{
    if (!gHttpHandler->UseAlternateProtocol())
        return false;

    // The Alternate Protocol hash is protected under the monitor because
    // it is read from both the main and the network thread.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    return mAlternateProtocolHash.Contains(hostPortKey);
}

void
nsHttpConnectionMgr::ReportSpdyAlternateProtocol(nsHttpConnection *conn)
{
    // Check network.http.spdy.use-alternate-protocol pref
    if (!gHttpHandler->UseAlternateProtocol())
        return;

    // For now lets not bypass proxies due to the alternate-protocol header
    if (conn->ConnectionInfo()->UsingHttpProxy())
        return;

    nsCString hostPortKey(conn->ConnectionInfo()->Host());
    if (conn->ConnectionInfo()->Port() != 80) {
        hostPortKey.Append(NS_LITERAL_CSTRING(":"));
        hostPortKey.AppendInt(conn->ConnectionInfo()->Port());
    }

    // The Alternate Protocol hash is protected under the monitor because
    // it is read from both the main and the network thread.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // Check to see if this is already present
    if (mAlternateProtocolHash.Contains(hostPortKey))
        return;
    
    if (mAlternateProtocolHash.Count() > 2000)
        mAlternateProtocolHash.EnumerateEntries(&TrimAlternateProtocolHash,
						this);
    
    mAlternateProtocolHash.PutEntry(hostPortKey);
}

void
nsHttpConnectionMgr::RemoveSpdyAlternateProtocol(nsACString &hostPortKey)
{
    // The Alternate Protocol hash is protected under the monitor because
    // it is read from both the main and the network thread.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    return mAlternateProtocolHash.RemoveEntry(hostPortKey);
}

PLDHashOperator
nsHttpConnectionMgr::TrimAlternateProtocolHash(nsCStringHashKey *entry,
                                               void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    
    if (self->mAlternateProtocolHash.Count() > 2000)
        return PL_DHASH_REMOVE;
    return PL_DHASH_STOP;
}

nsHttpConnectionMgr::nsConnectionEntry *
nsHttpConnectionMgr::GetSpdyPreferredEnt(nsConnectionEntry *aOriginalEntry)
{
    if (!gHttpHandler->IsSpdyEnabled() ||
        !gHttpHandler->CoalesceSpdy() ||
        aOriginalEntry->mCoalescingKey.IsEmpty())
        return nsnull;

    nsConnectionEntry *preferred =
        mSpdyPreferredHash.Get(aOriginalEntry->mCoalescingKey);

    // if there is no redirection no cert validation is required
    if (preferred == aOriginalEntry)
        return aOriginalEntry;

    // if there is no preferred host or it is no longer using spdy
    // then skip pooling
    if (!preferred || !preferred->mUsingSpdy)
        return nsnull;                         

    // if there is not an active spdy session in this entry then
    // we cannot pool because the cert upon activation may not
    // be the same as the old one. Active sessions are prohibited
    // from changing certs.

    nsHttpConnection *activeSpdy = nsnull;

    for (PRUint32 index = 0; index < preferred->mActiveConns.Length(); ++index) {
        if (preferred->mActiveConns[index]->CanDirectlyActivate()) {
            activeSpdy = preferred->mActiveConns[index];
            break;
        }
    }

    if (!activeSpdy) {
        // remove the preferred status of this entry if it cannot be
        // used for pooling.
        preferred->mSpdyPreferred = false;
        RemoveSpdyPreferredEnt(preferred->mCoalescingKey);
        LOG(("nsHttpConnectionMgr::GetSpdyPreferredConnection "
             "preferred host mapping %s to %s removed due to inactivity.\n",
             aOriginalEntry->mConnInfo->Host(),
             preferred->mConnInfo->Host()));

        return nsnull;
    }

    // Check that the server cert supports redirection
    nsresult rv;
    bool isJoined = false;

    nsCOMPtr<nsISupports> securityInfo;
    nsCOMPtr<nsISSLSocketControl> sslSocketControl;
    nsCAutoString negotiatedNPN;
    
    activeSpdy->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (!securityInfo) {
        NS_WARNING("cannot obtain spdy security info");
        return nsnull;
    }

    sslSocketControl = do_QueryInterface(securityInfo, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("sslSocketControl QI Failed");
        return nsnull;
    }

    rv = sslSocketControl->JoinConnection(NS_LITERAL_CSTRING("spdy/2"),
                                          aOriginalEntry->mConnInfo->GetHost(),
                                          aOriginalEntry->mConnInfo->Port(),
                                          &isJoined);

    if (NS_FAILED(rv) || !isJoined) {
        LOG(("nsHttpConnectionMgr::GetSpdyPreferredConnection "
             "Host %s cannot be confirmed to be joined "
             "with %s connections. rv=%x isJoined=%d",
             preferred->mConnInfo->Host(), aOriginalEntry->mConnInfo->Host(),
             rv, isJoined));
        mozilla::Telemetry::Accumulate(mozilla::Telemetry::SPDY_NPN_JOIN,
                                       false);
        return nsnull;
    }

    // IP pooling confirmed
    LOG(("nsHttpConnectionMgr::GetSpdyPreferredConnection "
         "Host %s has cert valid for %s connections, "
         "so %s will be coalesced with %s",
         preferred->mConnInfo->Host(), aOriginalEntry->mConnInfo->Host(),
         aOriginalEntry->mConnInfo->Host(), preferred->mConnInfo->Host()));
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::SPDY_NPN_JOIN, true);
    return preferred;
}

void
nsHttpConnectionMgr::RemoveSpdyPreferredEnt(nsACString &aHashKey)
{
    if (aHashKey.IsEmpty())
        return;
    
    mSpdyPreferredHash.Remove(aHashKey);
}

//-----------------------------------------------------------------------------
// enumeration callbacks

PLDHashOperator
nsHttpConnectionMgr::ProcessOneTransactionCB(const nsACString &key,
                                             nsAutoPtr<nsConnectionEntry> &ent,
                                             void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;

    if (self->ProcessPendingQForEntry(ent))
        return PL_DHASH_STOP;

    return PL_DHASH_NEXT;
}

// If the global number of idle connections is preventing the opening of
// new connections to a host without idle connections, then
// close them regardless of their TTL
PLDHashOperator
nsHttpConnectionMgr::PurgeExcessIdleConnectionsCB(const nsACString &key,
                                                  nsAutoPtr<nsConnectionEntry> &ent,
                                                  void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;

    while (self->mNumIdleConns + self->mNumActiveConns + 1 >= self->mMaxConns) {
        if (!ent->mIdleConns.Length()) {
            // There are no idle conns left in this connection entry
            return PL_DHASH_NEXT;
        }
        nsHttpConnection *conn = ent->mIdleConns[0];
        ent->mIdleConns.RemoveElementAt(0);
        conn->Close(NS_ERROR_ABORT);
        NS_RELEASE(conn);
        self->mNumIdleConns--;
        self->ConditionallyStopPruneDeadConnectionsTimer();
    }
    return PL_DHASH_STOP;
}

PLDHashOperator
nsHttpConnectionMgr::PruneDeadConnectionsCB(const nsACString &key,
                                            nsAutoPtr<nsConnectionEntry> &ent,
                                            void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;

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
                timeToNextExpire = NS_MIN(timeToNextExpire, conn->TimeToLive());
            }
        }
    }

    if (ent->mUsingSpdy) {
        for (PRUint32 index = 0; index < ent->mActiveConns.Length(); ++index) {
            nsHttpConnection *conn = ent->mActiveConns[index];
            if (conn->UsingSpdy()) {
                if (!conn->CanReuse()) {
                    // marking it dont reuse will create an active tear down if
                    // the spdy session is idle.
                    conn->DontReuse();
                }
                else {
                    timeToNextExpire = NS_MIN(timeToNextExpire,
                                              conn->TimeToLive());
                }
            }
        }
    }
    
    // If time to next expire found is shorter than time to next wake-up, we need to
    // change the time for next wake-up.
    if (timeToNextExpire != PR_UINT32_MAX) {
        PRUint32 now = NowInSeconds();
        PRUint64 timeOfNextExpire = now + timeToNextExpire;
        // If pruning of dead connections is not already scheduled to happen
        // or time found for next connection to expire is is before
        // mTimeOfNextWakeUp, we need to schedule the pruning to happen
        // after timeToNextExpire.
        if (!self->mTimer || timeOfNextExpire < self->mTimeOfNextWakeUp) {
            self->PruneDeadConnectionsAfter(timeToNextExpire);
        }
    } else {
        self->ConditionallyStopPruneDeadConnectionsTimer();
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
        ent->mHalfOpens.Length()   == 0 &&
        ent->mPendingQ.Length()    == 0 &&
        ((!ent->mTestedSpdy && !ent->mUsingSpdy) ||
         !gHttpHandler->IsSpdyEnabled() ||
         self->mCT.Count() > 300)) {
        LOG(("    removing empty connection entry\n"));
        return PL_DHASH_REMOVE;
    }

    // else, use this opportunity to compact our arrays...
    ent->mIdleConns.Compact();
    ent->mActiveConns.Compact();
    ent->mPendingQ.Compact();

    return PL_DHASH_NEXT;
}

PLDHashOperator
nsHttpConnectionMgr::ShutdownPassCB(const nsACString &key,
                                    nsAutoPtr<nsConnectionEntry> &ent,
                                    void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;

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
    self->ConditionallyStopPruneDeadConnectionsTimer();

    // close all pending transactions
    while (ent->mPendingQ.Length()) {
        trans = ent->mPendingQ[0];

        ent->mPendingQ.RemoveElementAt(0);

        trans->Close(NS_ERROR_ABORT);
        NS_RELEASE(trans);
    }

    // close all half open tcp connections
    for (PRInt32 i = ((PRInt32) ent->mHalfOpens.Length()) - 1; i >= 0; i--)
        ent->mHalfOpens[i]->Abandon();

    return PL_DHASH_REMOVE;
}

//-----------------------------------------------------------------------------

bool
nsHttpConnectionMgr::ProcessPendingQForEntry(nsConnectionEntry *ent)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    LOG(("nsHttpConnectionMgr::ProcessPendingQForEntry [ci=%s]\n",
        ent->mConnInfo->HashKey().get()));

    ProcessSpdyPendingQ(ent);

    PRUint32 i, count = ent->mPendingQ.Length();
    if (count > 0) {
        LOG(("  pending-count=%u\n", count));
        nsHttpTransaction *trans = nsnull;
        nsHttpConnection *conn = nsnull;
        for (i = 0; i < count; ++i) {
            trans = ent->mPendingQ[i];

            // When this transaction has already established a half-open
            // connection, we want to prevent any duplicate half-open
            // connections from being established and bound to this
            // transaction. Allow only use of an idle persistent connection
            // (if found) for transactions referred by a half-open connection.
            bool alreadyHalfOpen = false;
            for (PRInt32 j = 0; j < ((PRInt32) ent->mHalfOpens.Length()); j++) {
                if (ent->mHalfOpens[j]->Transaction() == trans) {
                    alreadyHalfOpen = true;
                    break;
                }
            }

            GetConnection(ent, trans, alreadyHalfOpen, &conn);
            if (conn)
                break;

            NS_ABORT_IF_FALSE(count == ent->mPendingQ.Length(),
                              "something mutated pending queue from "
                              "GetConnection()");
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
            return true;
        }
    }
    return false;
}

// we're at the active connection limit if any one of the following conditions is true:
//  (1) at max-connections
//  (2) keep-alive enabled and at max-persistent-connections-per-server/proxy
//  (3) keep-alive disabled and at max-connections-per-server
bool
nsHttpConnectionMgr::AtActiveConnectionLimit(nsConnectionEntry *ent, PRUint8 caps)
{
    nsHttpConnectionInfo *ci = ent->mConnInfo;

    LOG(("nsHttpConnectionMgr::AtActiveConnectionLimit [ci=%s caps=%x]\n",
        ci->HashKey().get(), caps));

    // update maxconns if potentially limited by the max socket count
    // this requires a dynamic reduction in the max socket count to a point
    // lower than the max-connections pref.
    PRUint32 maxSocketCount = gHttpHandler->MaxSocketCount();
    if (mMaxConns > maxSocketCount) {
        mMaxConns = maxSocketCount;
        LOG(("nsHttpConnectionMgr %p mMaxConns dynamically reduced to %u",
             this, mMaxConns));
    }

    // If there are more active connections than the global limit, then we're
    // done. Purging idle connections won't get us below it.
    if (mNumActiveConns >= mMaxConns) {
        LOG(("  num active conns == max conns\n"));
        return true;
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

    // Add in the in-progress tcp connections, we will assume they are
    // keepalive enabled.
    totalCount += ent->mHalfOpens.Length();
    persistCount += ent->mHalfOpens.Length();
    
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
nsHttpConnectionMgr::ClosePersistentConnections(nsConnectionEntry *ent)
{
    LOG(("nsHttpConnectionMgr::ClosePersistentConnections [ci=%s]\n",
         ent->mConnInfo->HashKey().get()));
    while (ent->mIdleConns.Length()) {
        nsHttpConnection *conn = ent->mIdleConns[0];
        ent->mIdleConns.RemoveElementAt(0);
        mNumIdleConns--;
        conn->Close(NS_ERROR_ABORT);
        NS_RELEASE(conn);
    }
    
    PRInt32 activeCount = ent->mActiveConns.Length();
    for (PRInt32 i=0; i < activeCount; i++)
        ent->mActiveConns[i]->DontReuse();
}

PLDHashOperator
nsHttpConnectionMgr::ClosePersistentConnectionsCB(const nsACString &key,
                                                  nsAutoPtr<nsConnectionEntry> &ent,
                                                  void *closure)
{
    nsHttpConnectionMgr *self = static_cast<nsHttpConnectionMgr *>(closure);
    self->ClosePersistentConnections(ent);
    return PL_DHASH_NEXT;
}

void
nsHttpConnectionMgr::GetConnection(nsConnectionEntry *ent,
                                   nsHttpTransaction *trans,
                                   bool onlyReusedConnection,
                                   nsHttpConnection **result)
{
    LOG(("nsHttpConnectionMgr::GetConnection [ci=%s caps=%x]\n",
        ent->mConnInfo->HashKey().get(), PRUint32(trans->Caps())));

    // First, see if an existing connection can be used - either an idle
    // persistent connection or an active spdy session may be reused instead of
    // establishing a new socket. We do not need to check the connection limits
    // yet as they govern the maximum number of open connections and reusing
    // an old connection never increases that.

    *result = nsnull;

    nsHttpConnection *conn = nsnull;
    bool addConnToActiveList = true;

    if (trans->Caps() & NS_HTTP_ALLOW_KEEPALIVE) {

        // if willing to use spdy look for an active spdy connections
        // before considering idle http ones.
        if (gHttpHandler->IsSpdyEnabled()) {
            conn = GetSpdyPreferredConn(ent);
            if (conn)
                addConnToActiveList = false;
        }
        
        // search the idle connection list. Each element in the list
        // has a reference, so if we remove it from the list into a local
        // ptr, that ptr now owns the reference
        while (!conn && (ent->mIdleConns.Length() > 0)) {
            conn = ent->mIdleConns[0];
            // we check if the connection can be reused before even checking if
            // it is a "matching" connection.
            if (!conn->CanReuse()) {
                LOG(("   dropping stale connection: [conn=%x]\n", conn));
                conn->Close(NS_ERROR_ABORT);
                NS_RELEASE(conn);
            }
            else {
                LOG(("   reusing connection [conn=%x]\n", conn));
                conn->EndIdleMonitoring();
            }

            ent->mIdleConns.RemoveElementAt(0);
            mNumIdleConns--;

            // If there are no idle connections left at all, we need to make
            // sure that we are not pruning dead connections anymore.
            ConditionallyStopPruneDeadConnectionsTimer();
        }
    }

    if (!conn) {

        // If the onlyReusedConnection parameter is TRUE, then GetConnection()
        // does not create new transports under any circumstances.
        if (onlyReusedConnection)
            return;
        
        if (gHttpHandler->IsSpdyEnabled() &&
            ent->mConnInfo->UsingSSL() &&
            !ent->mConnInfo->UsingHttpProxy())
        {
            // If this host is trying to negotiate a SPDY session right now,
            // don't create any new connections until the result of the
            // negotiation is known.
    
            if ((!ent->mTestedSpdy || ent->mUsingSpdy) &&
                (ent->mHalfOpens.Length() || ent->mActiveConns.Length()))
                return;
        }
        
        // Check if we need to purge an idle connection. Note that we may have
        // removed one above; if so, this will be a no-op. We do this before
        // checking the active connection limit to catch the case where we do
        // have an idle connection, but the purge timer hasn't fired yet.
        // XXX this just purges a random idle connection.  we should instead
        // enumerate the entire hash table to find the eldest idle connection.
        if (mNumIdleConns && mNumIdleConns + mNumActiveConns + 1 >= mMaxConns)
            mCT.Enumerate(PurgeExcessIdleConnectionsCB, this);

        // Need to make a new TCP connection. First, we check if we've hit
        // either the maximum connection limit globally or for this particular
        // host or proxy. If we have, we're done.
        if (AtActiveConnectionLimit(ent, trans->Caps())) {
            LOG(("nsHttpConnectionMgr::GetConnection [ci = %s]"
                 "at active connection limit - will queue\n",
                 ent->mConnInfo->HashKey().get()));
            return;
        }

        LOG(("nsHttpConnectionMgr::GetConnection Open Connection "
             "%s %s ent=%p spdy=%d",
             ent->mConnInfo->Host(), ent->mCoalescingKey.get(),
             ent, ent->mUsingSpdy));
        
        nsresult rv = CreateTransport(ent, trans);
        if (NS_FAILED(rv))
            trans->Close(rv);
        return;
    }

    if (addConnToActiveList) {
        // hold an owning ref to this connection
        ent->mActiveConns.AppendElement(conn);
        mNumActiveConns++;
    }
    
    NS_ADDREF(conn);
    *result = conn;
}

void
nsHttpConnectionMgr::AddActiveConn(nsHttpConnection *conn,
                                   nsConnectionEntry *ent)
{
    NS_ADDREF(conn);
    ent->mActiveConns.AppendElement(conn);
    mNumActiveConns++;
    ActivateTimeoutTick();
}

void
nsHttpConnectionMgr::StartedConnect()
{
    mNumActiveConns++;
}

void
nsHttpConnectionMgr::RecvdConnect()
{
    mNumActiveConns--;
}

nsresult
nsHttpConnectionMgr::CreateTransport(nsConnectionEntry *ent,
                                     nsHttpTransaction *trans)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsRefPtr<nsHalfOpenSocket> sock = new nsHalfOpenSocket(ent, trans);
    nsresult rv = sock->SetupPrimaryStreams();
    NS_ENSURE_SUCCESS(rv, rv);

    ent->mHalfOpens.AppendElement(sock);
    return NS_OK;
}

nsresult
nsHttpConnectionMgr::DispatchTransaction(nsConnectionEntry *ent,
                                         nsHttpTransaction *aTrans,
                                         PRUint8 caps,
                                         nsHttpConnection *conn)
{
    LOG(("nsHttpConnectionMgr::DispatchTransaction [ci=%s trans=%x caps=%x conn=%x]\n",
        ent->mConnInfo->HashKey().get(), aTrans, caps, conn));
    nsresult rv;
    
    PRInt32 priority = aTrans->Priority();

    if (conn->UsingSpdy()) {
        LOG(("Spdy Dispatch Transaction via Activate(). Transaction host = %s,"
             "Connection host = %s\n",
             aTrans->ConnectionInfo()->Host(),
             conn->ConnectionInfo()->Host()));
        rv = conn->Activate(aTrans, caps, priority);
        NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "SPDY Cannot Fail Dispatch");
        return rv;
    }

    nsConnectionHandle *handle = new nsConnectionHandle(conn);
    if (!handle)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(handle);

    nsHttpPipeline *pipeline = nsnull;
    nsAHttpTransaction *trans = aTrans;

    if (conn->SupportsPipelining() && (caps & NS_HTTP_ALLOW_PIPELINING)) {
        LOG(("  looking to build pipeline...\n"));
        if (BuildPipeline(ent, trans, &pipeline))
            trans = pipeline;
    }

    // give the transaction the indirect reference to the connection.
    trans->SetConnection(handle);

    rv = conn->Activate(trans, caps, priority);

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

bool
nsHttpConnectionMgr::BuildPipeline(nsConnectionEntry *ent,
                                   nsAHttpTransaction *firstTrans,
                                   nsHttpPipeline **result)
{
    if (mMaxPipelinedRequests < 2)
        return false;

    nsHttpPipeline *pipeline = nsnull;
    nsHttpTransaction *trans;

    PRUint32 i = 0, numAdded = 0;
    while (i < ent->mPendingQ.Length()) {
        trans = ent->mPendingQ[i];
        if (trans->Caps() & NS_HTTP_ALLOW_PIPELINING) {
            if (numAdded == 0) {
                pipeline = new nsHttpPipeline;
                if (!pipeline)
                    return false;
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
        return false;

    LOG(("  pipelined %u transactions\n", numAdded));
    NS_ADDREF(*result = pipeline);
    return true;
}

nsresult
nsHttpConnectionMgr::ProcessNewTransaction(nsHttpTransaction *trans)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

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

    nsConnectionEntry *ent = mCT.Get(ci->HashKey());
    if (!ent) {
        nsHttpConnectionInfo *clone = ci->Clone();
        if (!clone)
            return NS_ERROR_OUT_OF_MEMORY;
        ent = new nsConnectionEntry(clone);
        if (!ent)
            return NS_ERROR_OUT_OF_MEMORY;
        mCT.Put(ci->HashKey(), ent);
    }

    // SPDY coalescing of hostnames means we might redirect from this
    // connection entry onto the preferred one.
    nsConnectionEntry *preferredEntry = GetSpdyPreferredEnt(ent);
    if (preferredEntry && (preferredEntry != ent)) {
        LOG(("nsHttpConnectionMgr::ProcessNewTransaction trans=%p "
             "redirected via coalescing from %s to %s\n", trans,
             ent->mConnInfo->Host(), preferredEntry->mConnInfo->Host()));

        ent = preferredEntry;
    }

    // If we are doing a force reload then close out any existing conns
    // to this host so that changes in DNS, LBs, etc.. are reflected
    if (caps & NS_HTTP_CLEAR_KEEPALIVES)
        ClosePersistentConnections(ent);

    // Check if the transaction already has a sticky reference to a connection.
    // If so, then we can just use it directly by transferring its reference
    // to the new connection var instead of calling GetConnection() to search
    // for an available one.

    nsAHttpConnection *wrappedConnection = trans->Connection();
    nsHttpConnection  *conn;
    conn = wrappedConnection ? wrappedConnection->TakeHttpConnection() : nsnull;

    if (conn) {
        NS_ASSERTION(caps & NS_HTTP_STICKY_CONNECTION, "unexpected caps");

        trans->SetConnection(nsnull);
    }
    else
        GetConnection(ent, trans, false, &conn);

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

// This function tries to dispatch the pending spdy transactions on
// the connection entry sent in as an argument. It will do so on the
// active spdy connection either in that same entry or in the
// redirected 'preferred' entry for the same coalescing hash key if
// coalescing is enabled.

void
nsHttpConnectionMgr::ProcessSpdyPendingQ(nsConnectionEntry *ent)
{
    nsHttpConnection *conn = GetSpdyPreferredConn(ent);
    if (!conn)
        return;

    for (PRInt32 index = ent->mPendingQ.Length() - 1;
         index >= 0 && conn->CanDirectlyActivate();
         --index) {
        nsHttpTransaction *trans = ent->mPendingQ[index];

        if (!(trans->Caps() & NS_HTTP_ALLOW_KEEPALIVE) ||
            trans->Caps() & NS_HTTP_DISALLOW_SPDY)
            continue;
 
        ent->mPendingQ.RemoveElementAt(index);

        nsresult rv = DispatchTransaction(ent, trans, trans->Caps(), conn);
        if (NS_FAILED(rv)) {
            // this cannot happen, but if due to some bug it does then
            // close the transaction
            NS_ABORT_IF_FALSE(false, "Dispatch SPDY Transaction");
            LOG(("ProcessSpdyPendingQ Dispatch Transaction failed trans=%p\n",
                    trans));
            trans->Close(rv);
        }
        NS_RELEASE(trans);
    }
}

PLDHashOperator
nsHttpConnectionMgr::ProcessSpdyPendingQCB(const nsACString &key,
                                           nsAutoPtr<nsConnectionEntry> &ent,
                                           void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;
    self->ProcessSpdyPendingQ(ent);
    return PL_DHASH_NEXT;
}

void
nsHttpConnectionMgr::ProcessAllSpdyPendingQ()
{
    mCT.Enumerate(ProcessSpdyPendingQCB, this);
}

nsHttpConnection *
nsHttpConnectionMgr::GetSpdyPreferredConn(nsConnectionEntry *ent)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ABORT_IF_FALSE(ent, "no connection entry");

    nsConnectionEntry *preferred = GetSpdyPreferredEnt(ent);

    // this entry is spdy-enabled if it is involved in a redirect
    if (preferred)
        // all new connections for this entry will use spdy too
        ent->mUsingSpdy = true;
    else
        preferred = ent;
    
    nsHttpConnection *conn = nsnull;
    
    if (preferred->mUsingSpdy) {
        for (PRUint32 index = 0;
             index < preferred->mActiveConns.Length();
             ++index) {
            if (preferred->mActiveConns[index]->CanDirectlyActivate()) {
                conn = preferred->mActiveConns[index];
                break;
            }
        }
    }
    
    return conn;
}

//-----------------------------------------------------------------------------

void
nsHttpConnectionMgr::OnMsgShutdown(PRInt32, void *)
{
    LOG(("nsHttpConnectionMgr::OnMsgShutdown\n"));

    mCT.Enumerate(ShutdownPassCB, this);

    if (mReadTimeoutTick) {
        mReadTimeoutTick->Cancel();
        mReadTimeoutTick = nsnull;
        mReadTimeoutTickArmed = false;
    }
    
    // signal shutdown complete
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
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

    nsConnectionEntry *ent = LookupConnectionEntry(trans->ConnectionInfo(),
                                                   nsnull, trans);

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
        nsConnectionEntry *ent = LookupConnectionEntry(trans->ConnectionInfo(),
                                                       nsnull, trans);

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
    nsConnectionEntry *ent = mCT.Get(ci->HashKey());
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

    // check canreuse() for all idle connections plus any active connections on
    // connection entries that are using spdy.
    if (mNumIdleConns || (mNumActiveConns && gHttpHandler->IsSpdyEnabled()))
        mCT.Enumerate(PruneDeadConnectionsCB, this);
}

void
nsHttpConnectionMgr::OnMsgClosePersistentConnections(PRInt32, void *)
{
    LOG(("nsHttpConnectionMgr::OnMsgClosePersistentConnections\n"));

    mCT.Enumerate(ClosePersistentConnectionsCB, this);
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

    nsConnectionEntry *ent = LookupConnectionEntry(conn->ConnectionInfo(),
                                                   conn, nsnull);
    nsHttpConnectionInfo *ci = nsnull;

    if (!ent) {
        // this should never happen
        LOG(("nsHttpConnectionMgr::OnMsgReclaimConnection ent == null\n"));
        NS_ABORT_IF_FALSE(false, "no connection entry");
        NS_ADDREF(ci = conn->ConnectionInfo());
    }
    else {
        NS_ADDREF(ci = ent->mConnInfo);

        // If the connection is in the active list, remove that entry
        // and the reference held by the mActiveConns list.
        // This is never the final reference on conn as the event context
        // is also holding one that is released at the end of this function.

        if (ent->mUsingSpdy) {
            // Spdy connections aren't reused in the traditional HTTP way in
            // the idleconns list, they are actively multplexed as active
            // conns. Even when they have 0 transactions on them they are
            // considered active connections. So when one is reclaimed it
            // is really complete and is meant to be shut down and not
            // reused.
            conn->DontReuse();
        }
        
        if (ent->mActiveConns.RemoveElement(conn)) {
            nsHttpConnection *temp = conn;
            NS_RELEASE(temp);
            mNumActiveConns--;
        }

        if (conn->CanReuse()) {
            LOG(("  adding connection to idle list\n"));
            // Keep The idle connection list sorted with the connections that
            // have moved the largest data pipelines at the front because these
            // connections have the largest cwnds on the server.

            // The linear search is ok here because the number of idleconns
            // in a single entry is generally limited to a small number (i.e. 6)

            PRUint32 idx;
            for (idx = 0; idx < ent->mIdleConns.Length(); idx++) {
                nsHttpConnection *idleConn = ent->mIdleConns[idx];
                if (idleConn->MaxBytesRead() < conn->MaxBytesRead())
                    break;
            }

            NS_ADDREF(conn);
            ent->mIdleConns.InsertElementAt(idx, conn);
            mNumIdleConns++;
            conn->BeginIdleMonitoring();

            // If the added connection was first idle connection or has shortest
            // time to live among the watched connections, pruning dead
            // connections needs to be done when it can't be reused anymore.
            PRUint32 timeToLive = conn->TimeToLive();
            if(!mTimer || NowInSeconds() + timeToLive < mTimeOfNextWakeUp)
                PruneDeadConnectionsAfter(timeToLive);
        }
        else {
            LOG(("  connection cannot be reused; closing connection\n"));
            // make sure the connection is closed and release our reference.
            conn->Close(NS_ERROR_ABORT);
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

// nsHttpConnectionMgr::nsConnectionEntry
nsHttpConnectionMgr::nsConnectionEntry::~nsConnectionEntry()
{
    if (mSpdyPreferred)
        gHttpHandler->ConnMgr()->RemoveSpdyPreferredEnt(mCoalescingKey);

    NS_RELEASE(mConnInfo);
}

// Read Timeout Tick handlers

void
nsHttpConnectionMgr::ActivateTimeoutTick()
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    LOG(("nsHttpConnectionMgr::ActivateTimeoutTick() "
         "this=%p mReadTimeoutTick=%p\n"));

    // right now the spdy timeout code is the only thing hooked to the timeout
    // tick, so disable it if spdy is not being used. However pipelining code
    // will also want this functionality soon.
    if (!gHttpHandler->IsSpdyEnabled())
        return;

    // The timer tick should be enabled if it is not already pending.
    // Upon running the tick will rearm itself if there are active
    // connections available.

    if (mReadTimeoutTick && mReadTimeoutTickArmed)
        return;

    if (!mReadTimeoutTick) {
        mReadTimeoutTick = do_CreateInstance(NS_TIMER_CONTRACTID);
        if (!mReadTimeoutTick) {
            NS_WARNING("failed to create timer for http timeout management");
            return;
        }
    }

    NS_ABORT_IF_FALSE(!mReadTimeoutTickArmed, "timer tick armed");
    mReadTimeoutTickArmed = true;
    // pipeline will expect a 1000ms granuality
    mReadTimeoutTick->Init(this, 15000, nsITimer::TYPE_REPEATING_SLACK);
}

void
nsHttpConnectionMgr::ReadTimeoutTick()
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ABORT_IF_FALSE(mReadTimeoutTick, "no readtimeout tick");

    LOG(("nsHttpConnectionMgr::ReadTimeoutTick active=%d\n",
         mNumActiveConns));

    if (!mNumActiveConns && mReadTimeoutTickArmed) {
        mReadTimeoutTick->Cancel();
        mReadTimeoutTickArmed = false;
        return;
    }

    mCT.Enumerate(ReadTimeoutTickCB, this);
}

PLDHashOperator
nsHttpConnectionMgr::ReadTimeoutTickCB(const nsACString &key,
                                       nsAutoPtr<nsConnectionEntry> &ent,
                                       void *closure)
{
    nsHttpConnectionMgr *self = (nsHttpConnectionMgr *) closure;

    LOG(("nsHttpConnectionMgr::ReadTimeoutTickCB() this=%p host=%s\n",
         self, ent->mConnInfo->Host()));

    PRIntervalTime now = PR_IntervalNow();
    for (PRUint32 index = 0; index < ent->mActiveConns.Length(); ++index)
        ent->mActiveConns[index]->ReadTimeoutTick(now);

    return PL_DHASH_NEXT;
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
                                                            bool *reset)
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

nsresult
nsHttpConnectionMgr::
nsConnectionHandle::TakeTransport(nsISocketTransport  **aTransport,
                                  nsIAsyncInputStream **aInputStream,
                                  nsIAsyncOutputStream **aOutputStream)
{
    return mConn->TakeTransport(aTransport, aInputStream, aOutputStream);
}

void
nsHttpConnectionMgr::nsConnectionHandle::GetSecurityInfo(nsISupports **result)
{
    mConn->GetSecurityInfo(result);
}

bool
nsHttpConnectionMgr::nsConnectionHandle::IsPersistent()
{
    return mConn->IsPersistent();
}

bool
nsHttpConnectionMgr::nsConnectionHandle::IsReused()
{
    return mConn->IsReused();
}

nsresult
nsHttpConnectionMgr::nsConnectionHandle::PushBack(const char *buf, PRUint32 bufLen)
{
    return mConn->PushBack(buf, bufLen);
}


//////////////////////// nsHalfOpenSocket


NS_IMPL_THREADSAFE_ISUPPORTS4(nsHttpConnectionMgr::nsHalfOpenSocket,
                              nsIOutputStreamCallback,
                              nsITransportEventSink,
                              nsIInterfaceRequestor,
                              nsITimerCallback)

nsHttpConnectionMgr::
nsHalfOpenSocket::nsHalfOpenSocket(nsConnectionEntry *ent,
                                   nsHttpTransaction *trans)
    : mEnt(ent),
      mTransaction(trans)
{
    NS_ABORT_IF_FALSE(ent && trans, "constructor with null arguments");
    LOG(("Creating nsHalfOpenSocket [this=%p trans=%p ent=%s]\n",
         this, trans, ent->mConnInfo->Host()));
}

nsHttpConnectionMgr::nsHalfOpenSocket::~nsHalfOpenSocket()
{
    NS_ABORT_IF_FALSE(!mStreamOut, "streamout not null");
    NS_ABORT_IF_FALSE(!mBackupStreamOut, "backupstreamout not null");
    NS_ABORT_IF_FALSE(!mSynTimer, "syntimer not null");
    LOG(("Destroying nsHalfOpenSocket [this=%p]\n", this));
    
    if (mEnt) {
        // A failure to create the transport object at all
        // will result in this not being present in the halfopen table
        // so ignore failures of RemoveElement()
        mEnt->mHalfOpens.RemoveElement(this);
    }
}

nsresult
nsHttpConnectionMgr::
nsHalfOpenSocket::SetupStreams(nsISocketTransport **transport,
                               nsIAsyncInputStream **instream,
                               nsIAsyncOutputStream **outstream,
                               bool isBackup)
{
    nsresult rv;

    const char* types[1];
    types[0] = (mEnt->mConnInfo->UsingSSL()) ?
        "ssl" : gHttpHandler->DefaultSocketType();
    PRUint32 typeCount = (types[0] != nsnull);

    nsCOMPtr<nsISocketTransport> socketTransport;
    nsCOMPtr<nsISocketTransportService> sts;

    sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = sts->CreateTransport(types, typeCount,
                              nsDependentCString(mEnt->mConnInfo->Host()),
                              mEnt->mConnInfo->Port(),
                              mEnt->mConnInfo->ProxyInfo(),
                              getter_AddRefs(socketTransport));
    NS_ENSURE_SUCCESS(rv, rv);
    
    PRUint32 tmpFlags = 0;
    if (mTransaction->Caps() & NS_HTTP_REFRESH_DNS)
        tmpFlags = nsISocketTransport::BYPASS_CACHE;

    if (mTransaction->Caps() & NS_HTTP_LOAD_ANONYMOUS)
        tmpFlags |= nsISocketTransport::ANONYMOUS_CONNECT;

    // For backup connections, we disable IPv6. That's because some users have
    // broken IPv6 connectivity (leading to very long timeouts), and disabling
    // IPv6 on the backup connection gives them a much better user experience
    // with dual-stack hosts, though they still pay the 250ms delay for each new
    // connection. This strategy is also known as "happy eyeballs".
    if (isBackup && gHttpHandler->FastFallbackToIPv4())
        tmpFlags |= nsISocketTransport::DISABLE_IPV6;

    socketTransport->SetConnectionFlags(tmpFlags);

    socketTransport->SetQoSBits(gHttpHandler->GetQoSBits());

    rv = socketTransport->SetEventSink(this, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = socketTransport->SetSecurityCallbacks(this);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIOutputStream> sout;
    rv = socketTransport->OpenOutputStream(nsITransport::OPEN_UNBUFFERED,
                                            0, 0,
                                            getter_AddRefs(sout));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInputStream> sin;
    rv = socketTransport->OpenInputStream(nsITransport::OPEN_UNBUFFERED,
                                           0, 0,
                                           getter_AddRefs(sin));
    NS_ENSURE_SUCCESS(rv, rv);

    socketTransport.forget(transport);
    CallQueryInterface(sin, instream);
    CallQueryInterface(sout, outstream);

    rv = (*outstream)->AsyncWait(this, 0, 0, nsnull);
    if (NS_SUCCEEDED(rv))
        gHttpHandler->ConnMgr()->StartedConnect();

    return rv;
}

nsresult
nsHttpConnectionMgr::nsHalfOpenSocket::SetupPrimaryStreams()
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsresult rv;

    rv = SetupStreams(getter_AddRefs(mSocketTransport),
                      getter_AddRefs(mStreamIn),
                      getter_AddRefs(mStreamOut),
                      false);
    LOG(("nsHalfOpenSocket::SetupPrimaryStream [this=%p ent=%s rv=%x]",
         this, mEnt->mConnInfo->Host(), rv));
    if (NS_FAILED(rv)) {
        if (mStreamOut)
            mStreamOut->AsyncWait(nsnull, 0, 0, nsnull);
        mStreamOut = nsnull;
        mStreamIn = nsnull;
        mSocketTransport = nsnull;
    }
    return rv;
}

nsresult
nsHttpConnectionMgr::nsHalfOpenSocket::SetupBackupStreams()
{
    nsresult rv = SetupStreams(getter_AddRefs(mBackupTransport),
                               getter_AddRefs(mBackupStreamIn),
                               getter_AddRefs(mBackupStreamOut),
                               true);
    LOG(("nsHalfOpenSocket::SetupBackupStream [this=%p ent=%s rv=%x]",
         this, mEnt->mConnInfo->Host(), rv));
    if (NS_FAILED(rv)) {
        if (mBackupStreamOut)
            mBackupStreamOut->AsyncWait(nsnull, 0, 0, nsnull);
        mBackupStreamOut = nsnull;
        mBackupStreamIn = nsnull;
        mBackupTransport = nsnull;
    }
    return rv;
}

void
nsHttpConnectionMgr::nsHalfOpenSocket::SetupBackupTimer()
{
    PRUint16 timeout = gHttpHandler->GetIdleSynTimeout();
    NS_ABORT_IF_FALSE(!mSynTimer, "timer already initd");
    
    if (timeout) {
        // Setup the timer that will establish a backup socket
        // if we do not get a writable event on the main one.
        // We do this because a lost SYN takes a very long time
        // to repair at the TCP level.
        //
        // Failure to setup the timer is something we can live with,
        // so don't return an error in that case.
        nsresult rv;
        mSynTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            mSynTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
            LOG(("nsHalfOpenSocket::SetupBackupTimer()"));
        }
    }
}

void
nsHttpConnectionMgr::nsHalfOpenSocket::CancelBackupTimer()
{
    // If the syntimer is still armed, we can cancel it because no backup
    // socket should be formed at this point
    if (!mSynTimer)
        return;

    LOG(("nsHalfOpenSocket::CancelBackupTimer()"));
    mSynTimer->Cancel();
    mSynTimer = nsnull;
}

void
nsHttpConnectionMgr::nsHalfOpenSocket::Abandon()
{
    LOG(("nsHalfOpenSocket::Abandon [this=%p ent=%s]",
         this, mEnt->mConnInfo->Host()));

    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsRefPtr<nsHalfOpenSocket> deleteProtector(this);

    if (mStreamOut) {
        gHttpHandler->ConnMgr()->RecvdConnect();
        mStreamOut->AsyncWait(nsnull, 0, 0, nsnull);
        mStreamOut = nsnull;
    }
    if (mBackupStreamOut) {
        gHttpHandler->ConnMgr()->RecvdConnect();
        mBackupStreamOut->AsyncWait(nsnull, 0, 0, nsnull);
        mBackupStreamOut = nsnull;
    }

    CancelBackupTimer();

    mEnt = nsnull;
}

NS_IMETHODIMP // method for nsITimerCallback
nsHttpConnectionMgr::nsHalfOpenSocket::Notify(nsITimer *timer)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ABORT_IF_FALSE(timer == mSynTimer, "wrong timer");

    if (!gHttpHandler->ConnMgr()->
        AtActiveConnectionLimit(mEnt, mTransaction->Caps())) {
        SetupBackupStreams();
    }

    mSynTimer = nsnull;
    return NS_OK;
}

// method for nsIAsyncOutputStreamCallback
NS_IMETHODIMP
nsHttpConnectionMgr::
nsHalfOpenSocket::OnOutputStreamReady(nsIAsyncOutputStream *out)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ABORT_IF_FALSE(out == mStreamOut ||
                      out == mBackupStreamOut, "stream mismatch");
    LOG(("nsHalfOpenSocket::OnOutputStreamReady [this=%p ent=%s %s]\n", 
         this, mEnt->mConnInfo->Host(),
         out == mStreamOut ? "primary" : "backup"));
    PRInt32 index;
    nsresult rv;
    
    gHttpHandler->ConnMgr()->RecvdConnect();

    CancelBackupTimer();

    // assign the new socket to the http connection
    nsRefPtr<nsHttpConnection> conn = new nsHttpConnection();
    LOG(("nsHalfOpenSocket::OnOutputStreamReady "
         "Created new nshttpconnection %p\n", conn.get()));

    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    nsCOMPtr<nsIEventTarget>        callbackTarget;
    mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks),
                                       getter_AddRefs(callbackTarget));
    if (out == mStreamOut) {
        rv = conn->Init(mEnt->mConnInfo,
                        gHttpHandler->ConnMgr()->mMaxRequestDelay,
                        mSocketTransport, mStreamIn, mStreamOut,
                        callbacks, callbackTarget);

        // The nsHttpConnection object now owns these streams and sockets
        mStreamOut = nsnull;
        mStreamIn = nsnull;
        mSocketTransport = nsnull;
    }
    else {
        rv = conn->Init(mEnt->mConnInfo,
                        gHttpHandler->ConnMgr()->mMaxRequestDelay,
                        mBackupTransport, mBackupStreamIn, mBackupStreamOut,
                        callbacks, callbackTarget);

        // The nsHttpConnection object now owns these streams and sockets
        mBackupStreamOut = nsnull;
        mBackupStreamIn = nsnull;
        mBackupTransport = nsnull;
    }

    if (NS_FAILED(rv)) {
        LOG(("nsHalfOpenSocket::OnOutputStreamReady "
             "conn->init (%p) failed %x\n", conn.get(), rv));
        return rv;
    }

    // if this is still in the pending list, remove it and dispatch it
    index = mEnt->mPendingQ.IndexOf(mTransaction);
    if (index != -1) {
        mEnt->mPendingQ.RemoveElementAt(index);
        nsHttpTransaction *temp = mTransaction;
        NS_RELEASE(temp);
        gHttpHandler->ConnMgr()->AddActiveConn(conn, mEnt);
        rv = gHttpHandler->ConnMgr()->DispatchTransaction(mEnt, mTransaction,
                                                          mTransaction->Caps(),
                                                          conn);
    }
    else {
        // this transaction was dispatched off the pending q before all the
        // sockets established themselves.

        // We need to establish a small non-zero idle timeout so the connection
        // mgr perceives this socket as suitable for persistent connection reuse
        const PRIntervalTime k5Sec = PR_SecondsToInterval(5);
        if (k5Sec < gHttpHandler->IdleTimeout())
            conn->SetIdleTimeout(k5Sec);
        else
            conn->SetIdleTimeout(gHttpHandler->IdleTimeout());

        // After about 1 second allow for the possibility of restarting a
        // transaction due to server close. Keep at sub 1 second as that is the
        // minimum granularity we can expect a server to be timing out with.
        conn->SetIsReusedAfter(950);

        nsRefPtr<nsHttpConnection> copy(conn);  // because onmsg*() expects to drop a reference
        gHttpHandler->ConnMgr()->OnMsgReclaimConnection(NS_OK, conn.forget().get());
    }

    return rv;
}

// method for nsITransportEventSink
NS_IMETHODIMP
nsHttpConnectionMgr::nsHalfOpenSocket::OnTransportStatus(nsITransport *trans,
                                                         nsresult status,
                                                         PRUint64 progress,
                                                         PRUint64 progressMax)
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mTransaction)
        mTransaction->OnTransportStatus(trans, status, progress);

    if (trans != mSocketTransport)
        return NS_OK;

    // if we are doing spdy coalescing and haven't recorded the ip address
    // for this entry before then make the hash key if our dns lookup
    // just completed

    if (status == nsISocketTransport::STATUS_CONNECTED_TO &&
        gHttpHandler->IsSpdyEnabled() &&
        gHttpHandler->CoalesceSpdy() &&
        mEnt && mEnt->mConnInfo && mEnt->mConnInfo->UsingSSL() &&
        !mEnt->mConnInfo->UsingHttpProxy() &&
        mEnt->mCoalescingKey.IsEmpty()) {

        PRNetAddr addr;
        nsresult rv = mSocketTransport->GetPeerAddr(&addr);
        if (NS_SUCCEEDED(rv)) {
            mEnt->mCoalescingKey.SetCapacity(72);
            PR_NetAddrToString(&addr, mEnt->mCoalescingKey.BeginWriting(), 64);
            mEnt->mCoalescingKey.SetLength(
                strlen(mEnt->mCoalescingKey.BeginReading()));

            if (mEnt->mConnInfo->GetAnonymous())
                mEnt->mCoalescingKey.AppendLiteral("~A:");
            else
                mEnt->mCoalescingKey.AppendLiteral("~.:");
            mEnt->mCoalescingKey.AppendInt(mEnt->mConnInfo->Port());

            LOG(("nsHttpConnectionMgr::nsHalfOpenSocket::OnTransportStatus "
                 "STATUS_CONNECTED_TO Established New Coalescing Key for host "
                 "%s [%s]", mEnt->mConnInfo->Host(),
                 mEnt->mCoalescingKey.get()));

            gHttpHandler->ConnMgr()->ProcessSpdyPendingQ(mEnt);
        }
    }

    switch (status) {
    case nsISocketTransport::STATUS_CONNECTING_TO:
        // Passed DNS resolution, now trying to connect, start the backup timer
        // only prevent creating another backup transport.
        // We also check for mEnt presence to not instantiate the timer after
        // this half open socket has already been abandoned.  It may happen
        // when we get this notification right between main-thread calls to
        // nsHttpConnectionMgr::Shutdown and nsSocketTransportService::Shutdown
        // where the first abandones all half open socket instances and only
        // after that the second stops the socket thread.
        if (mEnt && !mBackupTransport && !mSynTimer)
            SetupBackupTimer();
        break;

    case nsISocketTransport::STATUS_CONNECTED_TO:
        // TCP connection's up, now transfer or SSL negotiantion starts,
        // no need for backup socket
        CancelBackupTimer();
        break;

    default:
        break;
    }

    return NS_OK;
}

// method for nsIInterfaceRequestor
NS_IMETHODIMP
nsHttpConnectionMgr::nsHalfOpenSocket::GetInterface(const nsIID &iid,
                                                    void **result)
{
    if (mTransaction) {
        nsCOMPtr<nsIInterfaceRequestor> callbacks;
        mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks), nsnull);
        if (callbacks)
            return callbacks->GetInterface(iid, result);
    }
    return NS_ERROR_NO_INTERFACE;
}


nsHttpConnection *
nsHttpConnectionMgr::nsConnectionHandle::TakeHttpConnection()
{
    // return our connection object to the caller and clear it internally
    // do not drop our reference - the caller now owns it.

    NS_ASSERTION(mConn, "no connection");
    nsHttpConnection *conn = mConn;
    mConn = nsnull;
    return conn;
}

bool
nsHttpConnectionMgr::nsConnectionHandle::LastTransactionExpectedNoContent()
{
    return mConn->LastTransactionExpectedNoContent();
}

void
nsHttpConnectionMgr::
nsConnectionHandle::SetLastTransactionExpectedNoContent(bool val)
{
     mConn->SetLastTransactionExpectedNoContent(val);
}

nsISocketTransport *
nsHttpConnectionMgr::nsConnectionHandle::Transport()
{
    if (!mConn)
        return nsnull;
    return mConn->Transport();
}
