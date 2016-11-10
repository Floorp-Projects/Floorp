/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#include "nsHttpConnectionMgr.h"
#include "nsHttpConnection.h"
#include "nsHttpPipeline.h"
#include "nsHttpHandler.h"
#include "nsIHttpChannelInternal.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "mozilla/net/DNS.h"
#include "nsISocketTransport.h"
#include "nsISSLSocketControl.h"
#include "mozilla/Telemetry.h"
#include "mozilla/net/DashboardTypes.h"
#include "NullHttpTransaction.h"
#include "nsIDNSRecord.h"
#include "nsITransport.h"
#include "nsInterfaceRequestorAgg.h"
#include "nsIRequestContext.h"
#include "nsISocketTransportService.h"
#include <algorithm>
#include "mozilla/ChaosMode.h"
#include "mozilla/Unused.h"
#include "nsIURI.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsHttpConnectionMgr, nsIObserver)

static void
InsertTransactionSorted(nsTArray<RefPtr<nsHttpTransaction> > &pendingQ, nsHttpTransaction *trans)
{
    // insert into queue with smallest valued number first.  search in reverse
    // order under the assumption that many of the existing transactions will
    // have the same priority (usually 0).

    for (int32_t i = pendingQ.Length() - 1; i >= 0; --i) {
        nsHttpTransaction *t = pendingQ[i];
        if (trans->Priority() >= t->Priority()) {
            if (ChaosMode::isActive(ChaosFeature::NetworkScheduling)) {
                int32_t samePriorityCount;
                for (samePriorityCount = 0; i - samePriorityCount >= 0; ++samePriorityCount) {
                    if (pendingQ[i - samePriorityCount]->Priority() != trans->Priority()) {
                        break;
                    }
                }
                // skip over 0...all of the elements with the same priority.
                i -= ChaosMode::randomUint32LessThan(samePriorityCount + 1);
            }
            pendingQ.InsertElementAt(i+1, trans);
            return;
        }
    }
    pendingQ.InsertElementAt(0, trans);
}

//-----------------------------------------------------------------------------

nsHttpConnectionMgr::nsHttpConnectionMgr()
    : mReentrantMonitor("nsHttpConnectionMgr.mReentrantMonitor")
    , mMaxConns(0)
    , mMaxPersistConnsPerHost(0)
    , mMaxPersistConnsPerProxy(0)
    , mIsShuttingDown(false)
    , mNumActiveConns(0)
    , mNumIdleConns(0)
    , mNumSpdyActiveConns(0)
    , mNumHalfOpenConns(0)
    , mTimeOfNextWakeUp(UINT64_MAX)
    , mPruningNoTraffic(false)
    , mTimeoutTickArmed(false)
    , mTimeoutTickNext(1)
{
    LOG(("Creating nsHttpConnectionMgr @%p\n", this));
}

nsHttpConnectionMgr::~nsHttpConnectionMgr()
{
    LOG(("Destroying nsHttpConnectionMgr @%p\n", this));
    if (mTimeoutTick)
        mTimeoutTick->Cancel();
}

nsresult
nsHttpConnectionMgr::EnsureSocketThreadTarget()
{
    nsresult rv;
    nsCOMPtr<nsIEventTarget> sts;
    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    if (NS_SUCCEEDED(rv))
        sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // do nothing if already initialized or if we've shut down
    if (mSocketThreadTarget || mIsShuttingDown)
        return NS_OK;

    mSocketThreadTarget = sts;

    return rv;
}

nsresult
nsHttpConnectionMgr::Init(uint16_t maxConns,
                          uint16_t maxPersistConnsPerHost,
                          uint16_t maxPersistConnsPerProxy,
                          uint16_t maxRequestDelay,
                          uint16_t maxPipelinedRequests,
                          uint16_t maxOptimisticPipelinedRequests)
{
    LOG(("nsHttpConnectionMgr::Init\n"));

    {
        ReentrantMonitorAutoEnter mon(mReentrantMonitor);

        mMaxConns = maxConns;
        mMaxPersistConnsPerHost = maxPersistConnsPerHost;
        mMaxPersistConnsPerProxy = maxPersistConnsPerProxy;
        mMaxRequestDelay = maxRequestDelay;
        mMaxPipelinedRequests = maxPipelinedRequests;
        mMaxOptimisticPipelinedRequests = maxOptimisticPipelinedRequests;

        mIsShuttingDown = false;
    }

    return EnsureSocketThreadTarget();
}

class BoolWrapper : public ARefBase
{
public:
    BoolWrapper() : mBool(false) {}
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BoolWrapper)

public: // intentional!
    bool mBool;

private:
    virtual ~BoolWrapper() {}
};

nsresult
nsHttpConnectionMgr::Shutdown()
{
    LOG(("nsHttpConnectionMgr::Shutdown\n"));

    RefPtr<BoolWrapper> shutdownWrapper = new BoolWrapper();
    {
        ReentrantMonitorAutoEnter mon(mReentrantMonitor);

        // do nothing if already shutdown
        if (!mSocketThreadTarget)
            return NS_OK;

        nsresult rv = PostEvent(&nsHttpConnectionMgr::OnMsgShutdown,
                                0, shutdownWrapper);

        // release our reference to the STS to prevent further events
        // from being posted.  this is how we indicate that we are
        // shutting down.
        mIsShuttingDown = true;
        mSocketThreadTarget = nullptr;

        if (NS_FAILED(rv)) {
            NS_WARNING("unable to post SHUTDOWN message");
            return rv;
        }
    }

    // wait for shutdown event to complete
    while (!shutdownWrapper->mBool) {
        NS_ProcessNextEvent(NS_GetCurrentThread());
    }

    return NS_OK;
}

class ConnEvent : public Runnable
{
public:
    ConnEvent(nsHttpConnectionMgr *mgr,
              nsConnEventHandler handler, int32_t iparam, ARefBase *vparam)
        : mMgr(mgr)
        , mHandler(handler)
        , mIParam(iparam)
        , mVParam(vparam) {}

    NS_IMETHOD Run() override
    {
        (mMgr->*mHandler)(mIParam, mVParam);
        return NS_OK;
    }

private:
    virtual ~ConnEvent() {}

    RefPtr<nsHttpConnectionMgr>  mMgr;
    nsConnEventHandler           mHandler;
    int32_t                      mIParam;
    RefPtr<ARefBase>             mVParam;
};

nsresult
nsHttpConnectionMgr::PostEvent(nsConnEventHandler handler,
                               int32_t iparam, ARefBase *vparam)
{
    EnsureSocketThreadTarget();

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    nsresult rv;
    if (!mSocketThreadTarget) {
        NS_WARNING("cannot post event if not initialized");
        rv = NS_ERROR_NOT_INITIALIZED;
    }
    else {
        nsCOMPtr<nsIRunnable> event = new ConnEvent(this, handler, iparam, vparam);
        rv = mSocketThreadTarget->Dispatch(event, NS_DISPATCH_NORMAL);
    }
    return rv;
}

void
nsHttpConnectionMgr::PruneDeadConnectionsAfter(uint32_t timeInSeconds)
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
    mTimeOfNextWakeUp = UINT64_MAX;
    if (mTimer) {
        mTimer->Cancel();
        mTimer = nullptr;
    }
}

void
nsHttpConnectionMgr::ConditionallyStopTimeoutTick()
{
    LOG(("nsHttpConnectionMgr::ConditionallyStopTimeoutTick "
         "armed=%d active=%d\n", mTimeoutTickArmed, mNumActiveConns));

    if (!mTimeoutTickArmed)
        return;

    if (mNumActiveConns)
        return;

    LOG(("nsHttpConnectionMgr::ConditionallyStopTimeoutTick stop==true\n"));

    mTimeoutTick->Cancel();
    mTimeoutTickArmed = false;
}

//-----------------------------------------------------------------------------
// nsHttpConnectionMgr::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpConnectionMgr::Observe(nsISupports *subject,
                             const char *topic,
                             const char16_t *data)
{
    LOG(("nsHttpConnectionMgr::Observe [topic=\"%s\"]\n", topic));

    if (0 == strcmp(topic, NS_TIMER_CALLBACK_TOPIC)) {
        nsCOMPtr<nsITimer> timer = do_QueryInterface(subject);
        if (timer == mTimer) {
            PruneDeadConnections();
        }
        else if (timer == mTimeoutTick) {
            TimeoutTick();
        } else if (timer == mTrafficTimer) {
            PruneNoTraffic();
        }
        else {
            MOZ_ASSERT(false, "unexpected timer-callback");
            LOG(("Unexpected timer object\n"));
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}


//-----------------------------------------------------------------------------

nsresult
nsHttpConnectionMgr::AddTransaction(nsHttpTransaction *trans, int32_t priority)
{
    LOG(("nsHttpConnectionMgr::AddTransaction [trans=%p %d]\n", trans, priority));
    return PostEvent(&nsHttpConnectionMgr::OnMsgNewTransaction, priority, trans);
}

nsresult
nsHttpConnectionMgr::RescheduleTransaction(nsHttpTransaction *trans, int32_t priority)
{
    LOG(("nsHttpConnectionMgr::RescheduleTransaction [trans=%p %d]\n", trans, priority));
    return PostEvent(&nsHttpConnectionMgr::OnMsgReschedTransaction, priority, trans);
}

nsresult
nsHttpConnectionMgr::CancelTransaction(nsHttpTransaction *trans, nsresult reason)
{
    LOG(("nsHttpConnectionMgr::CancelTransaction [trans=%p reason=%x]\n", trans, reason));
    return PostEvent(&nsHttpConnectionMgr::OnMsgCancelTransaction,
                     static_cast<int32_t>(reason), trans);
}

nsresult
nsHttpConnectionMgr::PruneDeadConnections()
{
    return PostEvent(&nsHttpConnectionMgr::OnMsgPruneDeadConnections);
}

//
// Called after a timeout. Check for active connections that have had no
// traffic since they were "marked" and nuke them.
nsresult
nsHttpConnectionMgr::PruneNoTraffic()
{
    LOG(("nsHttpConnectionMgr::PruneNoTraffic\n"));
    mPruningNoTraffic = true;
    return PostEvent(&nsHttpConnectionMgr::OnMsgPruneNoTraffic);
}

nsresult
nsHttpConnectionMgr::VerifyTraffic()
{
    LOG(("nsHttpConnectionMgr::VerifyTraffic\n"));
    return PostEvent(&nsHttpConnectionMgr::OnMsgVerifyTraffic);
}

nsresult
nsHttpConnectionMgr::DoShiftReloadConnectionCleanup(nsHttpConnectionInfo *aCI)
{
    return PostEvent(&nsHttpConnectionMgr::OnMsgDoShiftReloadConnectionCleanup,
                     0, aCI);
}

class SpeculativeConnectArgs : public ARefBase
{
public:
    SpeculativeConnectArgs() { mOverridesOK = false; }
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SpeculativeConnectArgs)

public: // intentional!
    RefPtr<NullHttpTransaction> mTrans;

    bool mOverridesOK;
    uint32_t mParallelSpeculativeConnectLimit;
    bool mIgnoreIdle;
    bool mIsFromPredictor;
    bool mAllow1918;

private:
    virtual ~SpeculativeConnectArgs() {}
    NS_DECL_OWNINGTHREAD
};

nsresult
nsHttpConnectionMgr::SpeculativeConnect(nsHttpConnectionInfo *ci,
                                        nsIInterfaceRequestor *callbacks,
                                        uint32_t caps,
                                        NullHttpTransaction *nullTransaction)
{
    MOZ_ASSERT(NS_IsMainThread(), "nsHttpConnectionMgr::SpeculativeConnect called off main thread!");

    if (!IsNeckoChild()) {
        // HACK: make sure PSM gets initialized on the main thread.
        net_EnsurePSMInit();
    }

    LOG(("nsHttpConnectionMgr::SpeculativeConnect [ci=%s]\n",
         ci->HashKey().get()));

    nsCOMPtr<nsISpeculativeConnectionOverrider> overrider =
        do_GetInterface(callbacks);

    bool allow1918 = overrider ? overrider->GetAllow1918() : false;

    // Hosts that are Local IP Literals should not be speculatively
    // connected - Bug 853423.
    if ((!allow1918) && ci && ci->HostIsLocalIPLiteral()) {
        LOG(("nsHttpConnectionMgr::SpeculativeConnect skipping RFC1918 "
             "address [%s]", ci->Origin()));
        return NS_OK;
    }

    RefPtr<SpeculativeConnectArgs> args = new SpeculativeConnectArgs();

    // Wrap up the callbacks and the target to ensure they're released on the target
    // thread properly.
    nsCOMPtr<nsIInterfaceRequestor> wrappedCallbacks;
    NS_NewInterfaceRequestorAggregation(callbacks, nullptr, getter_AddRefs(wrappedCallbacks));

    caps |= ci->GetAnonymous() ? NS_HTTP_LOAD_ANONYMOUS : 0;
    caps |= NS_HTTP_ERROR_SOFTLY;
    args->mTrans =
        nullTransaction ? nullTransaction : new NullHttpTransaction(ci, wrappedCallbacks, caps);

    if (overrider) {
        args->mOverridesOK = true;
        args->mParallelSpeculativeConnectLimit =
            overrider->GetParallelSpeculativeConnectLimit();
        args->mIgnoreIdle = overrider->GetIgnoreIdle();
        args->mIsFromPredictor = overrider->GetIsFromPredictor();
        args->mAllow1918 = overrider->GetAllow1918();
    }

    return PostEvent(&nsHttpConnectionMgr::OnMsgSpeculativeConnect, 0, args);
}

nsresult
nsHttpConnectionMgr::GetSocketThreadTarget(nsIEventTarget **target)
{
    EnsureSocketThreadTarget();

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    nsCOMPtr<nsIEventTarget> temp(mSocketThreadTarget);
    temp.forget(target);
    return NS_OK;
}

nsresult
nsHttpConnectionMgr::ReclaimConnection(nsHttpConnection *conn)
{
    LOG(("nsHttpConnectionMgr::ReclaimConnection [conn=%p]\n", conn));
    return PostEvent(&nsHttpConnectionMgr::OnMsgReclaimConnection, 0, conn);
}

// A structure used to marshall 2 pointers across the various necessary
// threads to complete an HTTP upgrade.
class nsCompleteUpgradeData : public ARefBase
{
public:
    nsCompleteUpgradeData(nsAHttpConnection *aConn,
                          nsIHttpUpgradeListener *aListener)
        : mConn(aConn)
        , mUpgradeListener(aListener) { }

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsCompleteUpgradeData)

    RefPtr<nsAHttpConnection> mConn;
    nsCOMPtr<nsIHttpUpgradeListener> mUpgradeListener;
private:
    virtual ~nsCompleteUpgradeData() { }
};

nsresult
nsHttpConnectionMgr::CompleteUpgrade(nsAHttpConnection *aConn,
                                     nsIHttpUpgradeListener *aUpgradeListener)
{
    RefPtr<nsCompleteUpgradeData> data =
        new nsCompleteUpgradeData(aConn, aUpgradeListener);
    return PostEvent(&nsHttpConnectionMgr::OnMsgCompleteUpgrade, 0, data);
}

nsresult
nsHttpConnectionMgr::UpdateParam(nsParamName name, uint16_t value)
{
    uint32_t param = (uint32_t(name) << 16) | uint32_t(value);
    return PostEvent(&nsHttpConnectionMgr::OnMsgUpdateParam,
                     static_cast<int32_t>(param), nullptr);
}

nsresult
nsHttpConnectionMgr::ProcessPendingQ(nsHttpConnectionInfo *ci)
{
    LOG(("nsHttpConnectionMgr::ProcessPendingQ [ci=%s]\n", ci->HashKey().get()));
    return PostEvent(&nsHttpConnectionMgr::OnMsgProcessPendingQ, 0, ci);
}

nsresult
nsHttpConnectionMgr::ProcessPendingQ()
{
    LOG(("nsHttpConnectionMgr::ProcessPendingQ [All CI]\n"));
    return PostEvent(&nsHttpConnectionMgr::OnMsgProcessPendingQ, 0, nullptr);
}

void
nsHttpConnectionMgr::OnMsgUpdateRequestTokenBucket(int32_t, ARefBase *param)
{
    EventTokenBucket *tokenBucket = static_cast<EventTokenBucket *>(param);
    gHttpHandler->SetRequestTokenBucket(tokenBucket);
}

nsresult
nsHttpConnectionMgr::UpdateRequestTokenBucket(EventTokenBucket *aBucket)
{
    // Call From main thread when a new EventTokenBucket has been made in order
    // to post the new value to the socket thread.
    return PostEvent(&nsHttpConnectionMgr::OnMsgUpdateRequestTokenBucket,
                     0, aBucket);
}

nsresult
nsHttpConnectionMgr::ClearConnectionHistory()
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
        nsAutoPtr<nsConnectionEntry>& ent = iter.Data();
        if (ent->mIdleConns.Length()   == 0 &&
            ent->mActiveConns.Length() == 0 &&
            ent->mHalfOpens.Length()   == 0 &&
            ent->mPendingQ.Length()    == 0) {
            iter.Remove();
        }
    }

    return NS_OK;
}


nsHttpConnectionMgr::nsConnectionEntry *
nsHttpConnectionMgr::LookupPreferredHash(nsHttpConnectionMgr::nsConnectionEntry *ent)
{
    nsConnectionEntry *preferred = nullptr;
    uint32_t len = ent->mCoalescingKeys.Length();
    for (uint32_t i = 0; !preferred && (i < len); ++i) {
        preferred = mSpdyPreferredHash.Get(ent->mCoalescingKeys[i]);
    }
    return preferred;
}

void
nsHttpConnectionMgr::StorePreferredHash(nsHttpConnectionMgr::nsConnectionEntry *ent)
{
    if (ent->mCoalescingKeys.IsEmpty()) {
        return;
    }

    ent->mInPreferredHash = true;
    uint32_t len = ent->mCoalescingKeys.Length();
    for (uint32_t i = 0; i < len; ++i) {
        mSpdyPreferredHash.Put(ent->mCoalescingKeys[i], ent);
    }
}

void
nsHttpConnectionMgr::RemovePreferredHash(nsHttpConnectionMgr::nsConnectionEntry *ent)
{
    if (!ent->mInPreferredHash || ent->mCoalescingKeys.IsEmpty()) {
        return;
    }

    ent->mInPreferredHash = false;
    uint32_t len = ent->mCoalescingKeys.Length();
    for (uint32_t i = 0; i < len; ++i) {
        mSpdyPreferredHash.Remove(ent->mCoalescingKeys[i]);
    }
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
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    if (!ci)
        return nullptr;

    nsConnectionEntry *ent = mCT.Get(ci->HashKey());

    // If there is no sign of coalescing (or it is disabled) then just
    // return the primary hash lookup
    if (!ent || !ent->mUsingSpdy || ent->mCoalescingKeys.IsEmpty())
        return ent;

    // If there is no preferred coalescing entry for this host (or the
    // preferred entry is the one that matched the mCT hash lookup) then
    // there is only option
    nsConnectionEntry *preferred = LookupPreferredHash(ent);
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
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::CloseIdleConnection %p conn=%p",
         this, conn));

    if (!conn->ConnectionInfo())
        return NS_ERROR_UNEXPECTED;

    nsConnectionEntry *ent = LookupConnectionEntry(conn->ConnectionInfo(),
                                                   conn, nullptr);

    RefPtr<nsHttpConnection> deleteProtector(conn);
    if (!ent || !ent->mIdleConns.RemoveElement(conn))
        return NS_ERROR_UNEXPECTED;

    conn->Close(NS_ERROR_ABORT);
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
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    nsConnectionEntry *ent = LookupConnectionEntry(conn->ConnectionInfo(),
                                                   conn, nullptr);

    if (!ent)
        return;

    if (!usingSpdy)
        return;

    ent->mUsingSpdy = true;
    mNumSpdyActiveConns++;

    uint32_t ttl = conn->TimeToLive();
    uint64_t timeOfExpire = NowInSeconds() + ttl;
    if (!mTimer || timeOfExpire < mTimeOfNextWakeUp)
        PruneDeadConnectionsAfter(ttl);

    // Lookup preferred directly from the hash instead of using
    // GetSpdyPreferredEnt() because we want to avoid the cert compatibility
    // check at this point because the cert is never part of the hash
    // lookup. Filtering on that has to be done at the time of use
    // rather than the time of registration (i.e. now).
    nsConnectionEntry *joinedConnection;
    nsConnectionEntry *preferred = LookupPreferredHash(ent);

    LOG(("ReportSpdyConnection %p,%s conn %p prefers %p,%s\n",
         ent, ent->mConnInfo->Origin(), conn, preferred,
         preferred ? preferred->mConnInfo->Origin() : ""));

    if (!preferred) {
        // this becomes the preferred entry
        StorePreferredHash(ent);
        preferred = ent;
    } else if ((preferred != ent) &&
               (joinedConnection = GetSpdyPreferredEnt(ent)) &&
               (joinedConnection != ent)) {
        //
        // A connection entry (e.g. made with a different hostname) with
        // the same IP address is preferred for future transactions over this
        // connection entry. Gracefully close down the connection to help
        // new transactions migrate over.

        LOG(("ReportSpdyConnection graceful close of conn=%p ent=%p to "
                 "migrate to preferred (desharding)\n", conn, ent));
        conn->DontReuse();
    } else if (preferred != ent) {
        LOG (("ReportSpdyConnection preferred host may be in false start or "
              "may have insufficient cert. Leave mapping in place but do not "
              "abandon this connection yet."));
    }

    if ((preferred == ent) && conn->CanDirectlyActivate()) {
        // this is a new spdy connection to the preferred entry

        // Cancel any other pending connections - their associated transactions
        // are in the pending queue and will be dispatched onto this connection
        for (int32_t index = ent->mHalfOpens.Length() - 1;
             index >= 0; --index) {
            LOG(("ReportSpdyConnection forcing halfopen abandon %p\n",
                 ent->mHalfOpens[index]));
            ent->mHalfOpens[index]->Abandon();
        }

        if (ent->mActiveConns.Length() > 1) {
            // this is a new connection to an established preferred spdy host.
            // if there is more than 1 live and established spdy connection (e.g.
            // some could still be handshaking, shutting down, etc..) then close
            // this one down after any transactions that are on it are complete.
            // This probably happened due to the parallel connection algorithm
            // that is used only before the host is known to speak spdy.
            for (uint32_t index = 0; index < ent->mActiveConns.Length(); ++index) {
                nsHttpConnection *otherConn = ent->mActiveConns[index];
                if (otherConn != conn) {
                    LOG(("ReportSpdyConnection shutting down connection (%p) because new "
                         "spdy connection (%p) takes precedence\n", otherConn, conn));
                    otherConn->DontReuse();
                }
            }
        }
    }

    ProcessPendingQ(ent->mConnInfo);
    PostEvent(&nsHttpConnectionMgr::OnMsgProcessAllSpdyPendingQ);
}

nsHttpConnectionMgr::nsConnectionEntry *
nsHttpConnectionMgr::GetSpdyPreferredEnt(nsConnectionEntry *aOriginalEntry)
{
    if (!gHttpHandler->IsSpdyEnabled() ||
        !gHttpHandler->CoalesceSpdy() ||
        aOriginalEntry->mConnInfo->GetNoSpdy() ||
        aOriginalEntry->mCoalescingKeys.IsEmpty()) {
        return nullptr;
    }

    nsConnectionEntry *preferred = LookupPreferredHash(aOriginalEntry);

    // if there is no redirection no cert validation is required
    if (preferred == aOriginalEntry)
        return aOriginalEntry;

    // if there is no preferred host or it is no longer using spdy
    // then skip pooling
    if (!preferred || !preferred->mUsingSpdy)
        return nullptr;

    // if there is not an active spdy session in this entry then
    // we cannot pool because the cert upon activation may not
    // be the same as the old one. Active sessions are prohibited
    // from changing certs.

    nsHttpConnection *activeSpdy = nullptr;

    for (uint32_t index = 0; index < preferred->mActiveConns.Length(); ++index) {
        if (preferred->mActiveConns[index]->CanDirectlyActivate()) {
            activeSpdy = preferred->mActiveConns[index];
            break;
        }
    }

    if (!activeSpdy) {
        // remove the preferred status of this entry if it cannot be
        // used for pooling.
        RemovePreferredHash(preferred);
        LOG(("nsHttpConnectionMgr::GetSpdyPreferredEnt "
             "preferred host mapping %s to %s removed due to inactivity.\n",
             aOriginalEntry->mConnInfo->Origin(),
             preferred->mConnInfo->Origin()));

        return nullptr;
    }

    // Check that the server cert supports redirection
    nsresult rv;
    bool isJoined = false;

    nsCOMPtr<nsISupports> securityInfo;
    nsCOMPtr<nsISSLSocketControl> sslSocketControl;
    nsAutoCString negotiatedNPN;

    activeSpdy->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (!securityInfo) {
        NS_WARNING("cannot obtain spdy security info");
        return nullptr;
    }

    sslSocketControl = do_QueryInterface(securityInfo, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("sslSocketControl QI Failed");
        return nullptr;
    }

    // try all the spdy versions we support.
    const SpdyInformation *info = gHttpHandler->SpdyInfo();
    for (uint32_t index = SpdyInformation::kCount;
         NS_SUCCEEDED(rv) && index > 0; --index) {
        if (info->ProtocolEnabled(index - 1)) {
            rv = sslSocketControl->JoinConnection(info->VersionString[index - 1],
                                                  aOriginalEntry->mConnInfo->GetOrigin(),
                                                  aOriginalEntry->mConnInfo->OriginPort(),
                                                  &isJoined);
            if (NS_SUCCEEDED(rv) && isJoined) {
                break;
            }
        }
    }

    if (NS_FAILED(rv) || !isJoined) {
        LOG(("nsHttpConnectionMgr::GetSpdyPreferredEnt "
             "Host %s cannot be confirmed to be joined "
             "with %s connections. rv=%x isJoined=%d",
             preferred->mConnInfo->Origin(), aOriginalEntry->mConnInfo->Origin(),
             rv, isJoined));
        Telemetry::Accumulate(Telemetry::SPDY_NPN_JOIN, false);
        return nullptr;
    }

    // IP pooling confirmed
    LOG(("nsHttpConnectionMgr::GetSpdyPreferredEnt "
         "Host %s has cert valid for %s connections, "
         "so %s will be coalesced with %s",
         preferred->mConnInfo->Origin(), aOriginalEntry->mConnInfo->Origin(),
         aOriginalEntry->mConnInfo->Origin(), preferred->mConnInfo->Origin()));
    Telemetry::Accumulate(Telemetry::SPDY_NPN_JOIN, true);
    return preferred;
}

//-----------------------------------------------------------------------------

bool
nsHttpConnectionMgr::ProcessPendingQForEntry(nsConnectionEntry *ent, bool considerAll)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    LOG(("nsHttpConnectionMgr::ProcessPendingQForEntry "
         "[ci=%s ent=%p active=%d idle=%d queued=%d]\n",
         ent->mConnInfo->HashKey().get(), ent, ent->mActiveConns.Length(),
         ent->mIdleConns.Length(), ent->mPendingQ.Length()));

    ProcessSpdyPendingQ(ent);

    nsHttpTransaction *trans;
    nsresult rv;
    bool dispatchedSuccessfully = false;

    // if !considerAll iterate the pending list until one is dispatched successfully.
    // Keep iterating afterwards only until a transaction fails to dispatch.
    // if considerAll == true then try and dispatch all items.
    for (uint32_t i = 0; i < ent->mPendingQ.Length(); ) {
        trans = ent->mPendingQ[i];

        // When this transaction has already established a half-open
        // connection, we want to prevent any duplicate half-open
        // connections from being established and bound to this
        // transaction. Allow only use of an idle persistent connection
        // (if found) for transactions referred by a half-open connection.
        bool alreadyHalfOpen = false;
        for (int32_t j = 0; j < ((int32_t) ent->mHalfOpens.Length()); ++j) {
            if (ent->mHalfOpens[j]->Transaction() == trans) {
                alreadyHalfOpen = true;
                break;
            }
        }

        rv = TryDispatchTransaction(ent,
                                    alreadyHalfOpen || !!trans->TunnelProvider(),
                                    trans);
        if (NS_SUCCEEDED(rv) || (rv != NS_ERROR_NOT_AVAILABLE)) {
            if (NS_SUCCEEDED(rv))
                LOG(("  dispatching pending transaction...\n"));
            else
                LOG(("  removing pending transaction based on "
                     "TryDispatchTransaction returning hard error %x\n", rv));

            if (ent->mPendingQ.RemoveElement(trans)) {
                // trans is now potentially destroyed
                dispatchedSuccessfully = true;
                continue; // dont ++i as we just made the array shorter
            }

            LOG(("  transaction not found in pending queue\n"));
        }

        if (dispatchedSuccessfully && !considerAll)
            break;

        ++i;
    }
    return dispatchedSuccessfully;
}

bool
nsHttpConnectionMgr::ProcessPendingQForEntry(nsHttpConnectionInfo *ci)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    nsConnectionEntry *ent = mCT.Get(ci->HashKey());
    if (ent)
        return ProcessPendingQForEntry(ent, false);
    return false;
}

bool
nsHttpConnectionMgr::SupportsPipelining(nsHttpConnectionInfo *ci)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    nsConnectionEntry *ent = mCT.Get(ci->HashKey());
    if (ent)
        return ent->SupportsPipelining();
    return false;
}

// nsHttpPipelineFeedback used to hold references across events

class nsHttpPipelineFeedback : public ARefBase
{
public:
    nsHttpPipelineFeedback(nsHttpConnectionInfo *ci,
                           nsHttpConnectionMgr::PipelineFeedbackInfoType info,
                           nsHttpConnection *conn, uint32_t data)
        : mConnInfo(ci)
        , mConn(conn)
        , mInfo(info)
        , mData(data)
        {
        }


    RefPtr<nsHttpConnectionInfo> mConnInfo;
    RefPtr<nsHttpConnection> mConn;
    nsHttpConnectionMgr::PipelineFeedbackInfoType mInfo;
    uint32_t mData;
private:
    ~nsHttpPipelineFeedback() {}
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsHttpPipelineFeedback)
};

void
nsHttpConnectionMgr::PipelineFeedbackInfo(nsHttpConnectionInfo *ci,
                                          PipelineFeedbackInfoType info,
                                          nsHttpConnection *conn,
                                          uint32_t data)
{
    if (!ci)
        return;

    // Post this to the socket thread if we are not running there already
    if (PR_GetCurrentThread() != gSocketThread) {
        RefPtr<nsHttpPipelineFeedback> fb =
            new nsHttpPipelineFeedback(ci, info, conn, data);
        PostEvent(&nsHttpConnectionMgr::OnMsgProcessFeedback, 0, fb);
        return;
    }

    nsConnectionEntry *ent = mCT.Get(ci->HashKey());
    if (ent)
        ent->OnPipelineFeedbackInfo(info, conn, data);
}

void
nsHttpConnectionMgr::ReportFailedToProcess(nsIURI *uri)
{
    MOZ_ASSERT(uri);

    nsAutoCString host;
    int32_t port = -1;
    nsAutoCString username;
    bool usingSSL = false;
    bool isHttp = false;

    nsresult rv = uri->SchemeIs("https", &usingSSL);
    if (NS_SUCCEEDED(rv) && usingSSL)
        isHttp = true;
    if (NS_SUCCEEDED(rv) && !isHttp)
        rv = uri->SchemeIs("http", &isHttp);
    if (NS_SUCCEEDED(rv))
        rv = uri->GetAsciiHost(host);
    if (NS_SUCCEEDED(rv))
        rv = uri->GetPort(&port);
    if (NS_SUCCEEDED(rv))
        uri->GetUsername(username);
    if (NS_FAILED(rv) || !isHttp || host.IsEmpty())
        return;

    // report the event for all the permutations of anonymous and
    // private versions of this host
    RefPtr<nsHttpConnectionInfo> ci =
        new nsHttpConnectionInfo(host, port, EmptyCString(), username, nullptr,
                                 NeckoOriginAttributes(), usingSSL);
    ci->SetAnonymous(false);
    ci->SetPrivate(false);
    PipelineFeedbackInfo(ci, RedCorruptedContent, nullptr, 0);

    ci = ci->Clone();
    ci->SetAnonymous(false);
    ci->SetPrivate(true);
    PipelineFeedbackInfo(ci, RedCorruptedContent, nullptr, 0);

    ci = ci->Clone();
    ci->SetAnonymous(true);
    ci->SetPrivate(false);
    PipelineFeedbackInfo(ci, RedCorruptedContent, nullptr, 0);

    ci = ci->Clone();
    ci->SetAnonymous(true);
    ci->SetPrivate(true);
    PipelineFeedbackInfo(ci, RedCorruptedContent, nullptr, 0);
}

// we're at the active connection limit if any one of the following conditions is true:
//  (1) at max-connections
//  (2) keep-alive enabled and at max-persistent-connections-per-server/proxy
//  (3) keep-alive disabled and at max-connections-per-server
bool
nsHttpConnectionMgr::AtActiveConnectionLimit(nsConnectionEntry *ent, uint32_t caps)
{
    nsHttpConnectionInfo *ci = ent->mConnInfo;

    LOG(("nsHttpConnectionMgr::AtActiveConnectionLimit [ci=%s caps=%x]\n",
        ci->HashKey().get(), caps));

    // update maxconns if potentially limited by the max socket count
    // this requires a dynamic reduction in the max socket count to a point
    // lower than the max-connections pref.
    uint32_t maxSocketCount = gHttpHandler->MaxSocketCount();
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

    // Add in the in-progress tcp connections, we will assume they are
    // keepalive enabled.
    // Exclude half-open's that has already created a usable connection.
    // This prevents the limit being stuck on ipv6 connections that
    // eventually time out after typical 21 seconds of no ACK+SYN reply.
    uint32_t totalCount =
        ent->mActiveConns.Length() + ent->UnconnectedHalfOpens();

    uint16_t maxPersistConns;

    if (ci->UsingHttpProxy() && !ci->UsingConnect())
        maxPersistConns = mMaxPersistConnsPerProxy;
    else
        maxPersistConns = mMaxPersistConnsPerHost;

    LOG(("   connection count = %d, limit %d\n", totalCount, maxPersistConns));

    // use >= just to be safe
    bool result = (totalCount >= maxPersistConns);
    LOG(("  result: %s", result ? "true" : "false"));
    return result;
}

void
nsHttpConnectionMgr::ClosePersistentConnections(nsConnectionEntry *ent)
{
    LOG(("nsHttpConnectionMgr::ClosePersistentConnections [ci=%s]\n",
         ent->mConnInfo->HashKey().get()));
    while (ent->mIdleConns.Length()) {
        RefPtr<nsHttpConnection> conn(ent->mIdleConns[0]);
        ent->mIdleConns.RemoveElementAt(0);
        mNumIdleConns--;
        conn->Close(NS_ERROR_ABORT);
    }

    int32_t activeCount = ent->mActiveConns.Length();
    for (int32_t i=0; i < activeCount; i++)
        ent->mActiveConns[i]->DontReuse();
}

bool
nsHttpConnectionMgr::RestrictConnections(nsConnectionEntry *ent)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    // If this host is trying to negotiate a SPDY session right now,
    // don't create any new ssl connections until the result of the
    // negotiation is known.

    bool doRestrict =
        ent->mConnInfo->FirstHopSSL() && gHttpHandler->IsSpdyEnabled() &&
        ent->mUsingSpdy && (ent->mHalfOpens.Length() || ent->mActiveConns.Length());

    // If there are no restrictions, we are done
    if (!doRestrict)
        return false;

    // If the restriction is based on a tcp handshake in progress
    // let that connect and then see if it was SPDY or not
    if (ent->UnconnectedHalfOpens()) {
        return true;
    }

    // There is a concern that a host is using a mix of HTTP/1 and SPDY.
    // In that case we don't want to restrict connections just because
    // there is a single active HTTP/1 session in use.
    if (ent->mUsingSpdy && ent->mActiveConns.Length()) {
        bool confirmedRestrict = false;
        for (uint32_t index = 0; index < ent->mActiveConns.Length(); ++index) {
            nsHttpConnection *conn = ent->mActiveConns[index];
            if (!conn->ReportedNPN() || conn->CanDirectlyActivate()) {
                confirmedRestrict = true;
                break;
            }
        }
        doRestrict = confirmedRestrict;
        if (!confirmedRestrict) {
            LOG(("nsHttpConnectionMgr spdy connection restriction to "
                 "%s bypassed.\n", ent->mConnInfo->Origin()));
        }
    }
    return doRestrict;
}

// returns NS_OK if a connection was started
// return NS_ERROR_NOT_AVAILABLE if a new connection cannot be made due to
//        ephemeral limits
// returns other NS_ERROR on hard failure conditions
nsresult
nsHttpConnectionMgr::MakeNewConnection(nsConnectionEntry *ent,
                                       nsHttpTransaction *trans)
{
    LOG(("nsHttpConnectionMgr::MakeNewConnection %p ent=%p trans=%p",
         this, ent, trans));
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    uint32_t halfOpenLength = ent->mHalfOpens.Length();
    for (uint32_t i = 0; i < halfOpenLength; i++) {
        if (ent->mHalfOpens[i]->IsSpeculative()) {
            // We've found a speculative connection in the half
            // open list. Remove the speculative bit from it and that
            // connection can later be used for this transaction
            // (or another one in the pending queue) - we don't
            // need to open a new connection here.
            LOG(("nsHttpConnectionMgr::MakeNewConnection [ci = %s]\n"
                 "Found a speculative half open connection\n",
                 ent->mConnInfo->HashKey().get()));

            uint32_t flags;
            ent->mHalfOpens[i]->SetSpeculative(false);
            nsISocketTransport *transport = ent->mHalfOpens[i]->SocketTransport();
            if (transport && NS_SUCCEEDED(transport->GetConnectionFlags(&flags))) {
                flags &= ~nsISocketTransport::DISABLE_RFC1918;
                transport->SetConnectionFlags(flags);
            }

            Telemetry::AutoCounter<Telemetry::HTTPCONNMGR_USED_SPECULATIVE_CONN> usedSpeculativeConn;
            ++usedSpeculativeConn;

            if (ent->mHalfOpens[i]->IsFromPredictor()) {
              Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS_USED> totalPreconnectsUsed;
              ++totalPreconnectsUsed;
            }

            // return OK because we have essentially opened a new connection
            // by converting a speculative half-open to general use
            return NS_OK;
        }
    }

    // consider null transactions that are being used to drive the ssl handshake if
    // the transaction creating this connection can re-use persistent connections
    if (trans->Caps() & NS_HTTP_ALLOW_KEEPALIVE) {
        uint32_t activeLength = ent->mActiveConns.Length();
        for (uint32_t i = 0; i < activeLength; i++) {
            nsAHttpTransaction *activeTrans = ent->mActiveConns[i]->Transaction();
            NullHttpTransaction *nullTrans = activeTrans ? activeTrans->QueryNullTransaction() : nullptr;
            if (nullTrans && nullTrans->Claim()) {
                LOG(("nsHttpConnectionMgr::MakeNewConnection [ci = %s] "
                     "Claiming a null transaction for later use\n",
                     ent->mConnInfo->HashKey().get()));
                return NS_OK;
            }
        }
    }

    // If this host is trying to negotiate a SPDY session right now,
    // don't create any new connections until the result of the
    // negotiation is known.
    if (!(trans->Caps() & NS_HTTP_DISALLOW_SPDY) &&
        (trans->Caps() & NS_HTTP_ALLOW_KEEPALIVE) &&
        RestrictConnections(ent)) {
        LOG(("nsHttpConnectionMgr::MakeNewConnection [ci = %s] "
             "Not Available Due to RestrictConnections()\n",
             ent->mConnInfo->HashKey().get()));
        return NS_ERROR_NOT_AVAILABLE;
    }

    // We need to make a new connection. If that is going to exceed the
    // global connection limit then try and free up some room by closing
    // an idle connection to another host. We know it won't select "ent"
    // because we have already determined there are no idle connections
    // to our destination

    if ((mNumIdleConns + mNumActiveConns + 1 >= mMaxConns) && mNumIdleConns) {
        // If the global number of connections is preventing the opening of new
        // connections to a host without idle connections, then close them
        // regardless of their TTL.
        auto iter = mCT.Iter();
        while (mNumIdleConns + mNumActiveConns + 1 >= mMaxConns &&
               !iter.Done()) {
            nsAutoPtr<nsConnectionEntry> &entry = iter.Data();
            if (!entry->mIdleConns.Length()) {
              iter.Next();
              continue;
            }
            RefPtr<nsHttpConnection> conn(entry->mIdleConns[0]);
            entry->mIdleConns.RemoveElementAt(0);
            conn->Close(NS_ERROR_ABORT);
            mNumIdleConns--;
            ConditionallyStopPruneDeadConnectionsTimer();
        }
    }

    if ((mNumIdleConns + mNumActiveConns + 1 >= mMaxConns) &&
        mNumActiveConns && gHttpHandler->IsSpdyEnabled())
    {
        // If the global number of connections is preventing the opening of new
        // connections to a host without idle connections, then close any spdy
        // ASAP.
        for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
            nsAutoPtr<nsConnectionEntry> &entry = iter.Data();
            if (!entry->mUsingSpdy) {
                continue;
            }

            for (uint32_t index = 0;
                 index < entry->mActiveConns.Length();
                 ++index) {
                nsHttpConnection *conn = entry->mActiveConns[index];
                if (conn->UsingSpdy() && conn->CanReuse()) {
                    conn->DontReuse();
                    // Stop on <= (particularly =) because this dontreuse
                    // causes async close.
                    if (mNumIdleConns + mNumActiveConns + 1 <= mMaxConns) {
                        goto outerLoopEnd;
                    }
                }
            }
        }
      outerLoopEnd:
        ;
    }

    if (AtActiveConnectionLimit(ent, trans->Caps()))
        return NS_ERROR_NOT_AVAILABLE;

    nsresult rv = CreateTransport(ent, trans, trans->Caps(), false, false, true);
    if (NS_FAILED(rv)) {
        /* hard failure */
        LOG(("nsHttpConnectionMgr::MakeNewConnection [ci = %s trans = %p] "
             "CreateTransport() hard failure.\n",
             ent->mConnInfo->HashKey().get(), trans));
        trans->Close(rv);
        if (rv == NS_ERROR_NOT_AVAILABLE)
            rv = NS_ERROR_FAILURE;
        return rv;
    }

    return NS_OK;
}

bool
nsHttpConnectionMgr::AddToShortestPipeline(nsConnectionEntry *ent,
                                           nsHttpTransaction *trans,
                                           nsHttpTransaction::Classifier classification,
                                           uint16_t depthLimit)
{
    if (classification == nsAHttpTransaction::CLASS_SOLO)
        return false;

    uint32_t maxdepth = ent->MaxPipelineDepth(classification);
    if (maxdepth == 0) {
        ent->CreditPenalty();
        maxdepth = ent->MaxPipelineDepth(classification);
    }

    if (ent->PipelineState() == PS_RED)
        return false;

    if (ent->PipelineState() == PS_YELLOW && ent->mYellowConnection)
        return false;

    // The maximum depth of a pipeline in yellow is 1 pipeline of
    // depth 2 for entire CI. When that transaction completes successfully
    // we transition to green and that expands the allowed depth
    // to any number of pipelines of up to depth 4.  When a transaction
    // queued at position 3 or deeper succeeds we open it all the way
    // up to depths limited only by configuration. The staggered start
    // in green is simply because a successful yellow test of depth=2
    // might really just be a race condition (i.e. depth=1 from the
    // server's point of view), while depth=3 is a stronger indicator -
    // keeping the pipelines to a modest depth during that period limits
    // the damage if something is going to go wrong.

    maxdepth = std::min<uint32_t>(maxdepth, depthLimit);

    if (maxdepth < 2)
        return false;

    nsAHttpTransaction *activeTrans;

    nsHttpConnection *bestConn = nullptr;
    uint32_t activeCount = ent->mActiveConns.Length();
    uint32_t bestConnLength = 0;
    uint32_t connLength;

    for (uint32_t i = 0; i < activeCount; ++i) {
        nsHttpConnection *conn = ent->mActiveConns[i];
        if (!conn->SupportsPipelining())
            continue;

        if (conn->Classification() != classification)
            continue;

        activeTrans = conn->Transaction();
        if (!activeTrans ||
            activeTrans->IsDone() ||
            NS_FAILED(activeTrans->Status()))
            continue;

        connLength = activeTrans->PipelineDepth();

        if (maxdepth <= connLength)
            continue;

        if (!bestConn || (connLength < bestConnLength)) {
            bestConn = conn;
            bestConnLength = connLength;
        }
    }

    if (!bestConn)
        return false;

    activeTrans = bestConn->Transaction();
    nsresult rv = activeTrans->AddTransaction(trans);
    if (NS_FAILED(rv))
        return false;

    LOG(("   scheduling trans %p on pipeline at position %d\n",
         trans, trans->PipelinePosition()));

    if ((ent->PipelineState() == PS_YELLOW) && (trans->PipelinePosition() > 1))
        ent->SetYellowConnection(bestConn);

    if (!trans->GetPendingTime().IsNull()) {
        if (trans->UsesPipelining())
            AccumulateTimeDelta(
                Telemetry::TRANSACTION_WAIT_TIME_HTTP_PIPELINES,
                trans->GetPendingTime(), TimeStamp::Now());
        else
            AccumulateTimeDelta(
                Telemetry::TRANSACTION_WAIT_TIME_HTTP,
                trans->GetPendingTime(), TimeStamp::Now());
        trans->SetPendingTime(false);
    }
    return true;
}

bool
nsHttpConnectionMgr::IsUnderPressure(nsConnectionEntry *ent,
                                   nsHttpTransaction::Classifier classification)
{
    // A connection entry is declared to be "under pressure" if most of the
    // allowed parallel connections are already used up. In that case we want to
    // favor existing pipelines over more parallelism so as to reserve any
    // unused parallel connections for types that don't have existing pipelines.
    //
    // The definition of connection pressure is a pretty liberal one here - that
    // is why we are using the more restrictive maxPersist* counters.
    //
    // Pipelines are also favored when the requested classification is already
    // using 3 or more of the connections. Failure to do this could result in
    // one class (e.g. images) establishing self replenishing queues on all the
    // connections that would starve the other transaction types.

    int32_t currentConns = ent->mActiveConns.Length();
    int32_t maxConns =
        (ent->mConnInfo->UsingHttpProxy() && !ent->mConnInfo->UsingConnect()) ?
        mMaxPersistConnsPerProxy : mMaxPersistConnsPerHost;

    // Leave room for at least 3 distinct types to operate concurrently,
    // this satisfies the typical {html, js/css, img} page.
    if (currentConns >= (maxConns - 2))
        return true;                           /* prefer pipeline */

    int32_t sameClass = 0;
    for (int32_t i = 0; i < currentConns; ++i)
        if (classification == ent->mActiveConns[i]->Classification())
            if (++sameClass == 3)
                return true;                   /* prefer pipeline */

    return false;                              /* normal behavior */
}

// returns OK if a connection is found for the transaction
//   and the transaction is started.
// returns ERROR_NOT_AVAILABLE if no connection can be found and it
//   should be queued until circumstances change
// returns other ERROR when transaction has a hard failure and should
//   not remain in the pending queue
nsresult
nsHttpConnectionMgr::TryDispatchTransaction(nsConnectionEntry *ent,
                                            bool onlyReusedConnection,
                                            nsHttpTransaction *trans)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::TryDispatchTransaction without conn "
         "[trans=%p ci=%p ci=%s caps=%x tunnelprovider=%p onlyreused=%d "
         "active=%d idle=%d]\n", trans,
         ent->mConnInfo.get(), ent->mConnInfo->HashKey().get(),
         uint32_t(trans->Caps()), trans->TunnelProvider(),
         onlyReusedConnection, ent->mActiveConns.Length(),
         ent->mIdleConns.Length()));

    nsHttpTransaction::Classifier classification = trans->Classification();
    uint32_t caps = trans->Caps();

    // no keep-alive means no pipelines either
    if (!(caps & NS_HTTP_ALLOW_KEEPALIVE))
        caps = caps & ~NS_HTTP_ALLOW_PIPELINING;

    // 0 - If this should use spdy then dispatch it post haste.
    // 1 - If there is connection pressure then see if we can pipeline this on
    //     a connection of a matching type instead of using a new conn
    // 2 - If there is an idle connection, use it!
    // 3 - if class == reval or script and there is an open conn of that type
    //     then pipeline onto shortest pipeline of that class if limits allow
    // 4 - If we aren't up against our connection limit,
    //     then open a new one
    // 5 - Try a pipeline if we haven't already - this will be unusual because
    //     it implies a low connection pressure situation where
    //     MakeNewConnection() failed.. that is possible, but unlikely, due to
    //     global limits
    // 6 - no connection is available - queue it

    bool attemptedOptimisticPipeline = !(caps & NS_HTTP_ALLOW_PIPELINING);
    RefPtr<nsHttpConnection> unusedSpdyPersistentConnection;

    // step 0
    // look for existing spdy connection - that's always best because it is
    // essentially pipelining without head of line blocking

    if (!(caps & NS_HTTP_DISALLOW_SPDY) && gHttpHandler->IsSpdyEnabled()) {
        RefPtr<nsHttpConnection> conn = GetSpdyPreferredConn(ent);
        if (conn) {
            if ((caps & NS_HTTP_ALLOW_KEEPALIVE) || !conn->IsExperienced()) {
                LOG(("   dispatch to spdy: [conn=%p]\n", conn.get()));
                trans->RemoveDispatchedAsBlocking();  /* just in case */
                DispatchTransaction(ent, trans, conn);
                return NS_OK;
            }
            unusedSpdyPersistentConnection = conn;
        }
    }

    // If this is not a blocking transaction and the request context for it is
    // currently processing one or more blocking transactions then we
    // need to just leave it in the queue until those are complete unless it is
    // explicitly marked as unblocked.
    if (!(caps & NS_HTTP_LOAD_AS_BLOCKING)) {
        if (!(caps & NS_HTTP_LOAD_UNBLOCKED)) {
            nsIRequestContext *requestContext = trans->RequestContext();
            if (requestContext) {
                uint32_t blockers = 0;
                if (NS_SUCCEEDED(requestContext->GetBlockingTransactionCount(&blockers)) &&
                    blockers) {
                    // need to wait for blockers to clear
                    LOG(("   blocked by request context: [rc=%p trans=%p blockers=%d]\n",
                         requestContext, trans, blockers));
                    return NS_ERROR_NOT_AVAILABLE;
                }
            }
        }
    } else {
        // Mark the transaction and its load group as blocking right now to prevent
        // other transactions from being reordered in the queue due to slow syns.
        trans->DispatchedAsBlocking();
    }

    // step 1
    // If connection pressure, then we want to favor pipelining of any kind
    if (IsUnderPressure(ent, classification) && !attemptedOptimisticPipeline) {
        attemptedOptimisticPipeline = true;
        if (AddToShortestPipeline(ent, trans,
                                  classification,
                                  mMaxOptimisticPipelinedRequests)) {
            LOG(("   dispatched step 1 trans=%p\n", trans));
            return NS_OK;
        }
    }

    // Subject most transactions at high parallelism to rate pacing.
    // It will only be actually submitted to the
    // token bucket once, and if possible it is granted admission synchronously.
    // It is important to leave a transaction in the pending queue when blocked by
    // pacing so it can be found on cancel if necessary.
    // Transactions that cause blocking or bypass it (e.g. js/css) are not rate
    // limited.
    if (gHttpHandler->UseRequestTokenBucket()) {
        // submit even whitelisted transactions to the token bucket though they will
        // not be slowed by it
        bool runNow = trans->TryToRunPacedRequest();
        if (!runNow) {
            if ((mNumActiveConns - mNumSpdyActiveConns) <=
                gHttpHandler->RequestTokenBucketMinParallelism()) {
                runNow = true; // white list it
            } else if (caps & (NS_HTTP_LOAD_AS_BLOCKING | NS_HTTP_LOAD_UNBLOCKED)) {
                runNow = true; // white list it
            }
        }
        if (!runNow) {
            LOG(("   blocked due to rate pacing trans=%p\n", trans));
            return NS_ERROR_NOT_AVAILABLE;
        }
    }

    // step 2
    // consider an idle persistent connection
    if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
        RefPtr<nsHttpConnection> conn;
        while (!conn && (ent->mIdleConns.Length() > 0)) {
            conn = ent->mIdleConns[0];
            ent->mIdleConns.RemoveElementAt(0);
            mNumIdleConns--;

            // we check if the connection can be reused before even checking if
            // it is a "matching" connection.
            if (!conn->CanReuse()) {
                LOG(("   dropping stale connection: [conn=%p]\n", conn.get()));
                conn->Close(NS_ERROR_ABORT);
                conn = nullptr;
            }
            else {
                LOG(("   reusing connection [conn=%p]\n", conn.get()));
                conn->EndIdleMonitoring();
            }

            // If there are no idle connections left at all, we need to make
            // sure that we are not pruning dead connections anymore.
            ConditionallyStopPruneDeadConnectionsTimer();
        }
        if (conn) {
            // This will update the class of the connection to be the class of
            // the transaction dispatched on it.
            AddActiveConn(conn, ent);
            DispatchTransaction(ent, trans, conn);
            LOG(("   dispatched step 2 (idle) trans=%p\n", trans));
            return NS_OK;
        }
    }

    // step 3
    // consider pipelining scripts and revalidations
    if (!attemptedOptimisticPipeline &&
        (classification == nsHttpTransaction::CLASS_REVALIDATION ||
         classification == nsHttpTransaction::CLASS_SCRIPT)) {
        // Assignation kept here for documentation purpose; Never read after
        attemptedOptimisticPipeline = true;
        if (AddToShortestPipeline(ent, trans,
                                  classification,
                                  mMaxOptimisticPipelinedRequests)) {
            LOG(("   dispatched step 3 (pipeline) trans=%p\n", trans));
            return NS_OK;
        }
    }

    // step 4
    if (!onlyReusedConnection) {
        nsresult rv = MakeNewConnection(ent, trans);
        if (NS_SUCCEEDED(rv)) {
            // this function returns NOT_AVAILABLE for asynchronous connects
            LOG(("   dispatched step 4 (async new conn) trans=%p\n", trans));
            return NS_ERROR_NOT_AVAILABLE;
        }

        if (rv != NS_ERROR_NOT_AVAILABLE) {
            // not available return codes should try next step as they are
            // not hard errors. Other codes should stop now
            LOG(("   failed step 4 (%x) trans=%p\n", rv, trans));
            return rv;
        }
    } else if (trans->TunnelProvider() && trans->TunnelProvider()->MaybeReTunnel(trans)) {
        LOG(("   sort of dispatched step 4a tunnel requeue trans=%p\n", trans));
        // the tunnel provider took responsibility for making a new tunnel
        return NS_OK;
    }

    // step 5
    if (caps & NS_HTTP_ALLOW_PIPELINING) {
        if (AddToShortestPipeline(ent, trans,
                                  classification,
                                  mMaxPipelinedRequests)) {
            LOG(("   dispatched step 5 trans=%p\n", trans));
            return NS_OK;
        }
    }

    // step 6
    if (unusedSpdyPersistentConnection) {
        // to avoid deadlocks, we need to throw away this perfectly valid SPDY
        // connection to make room for a new one that can service a no KEEPALIVE
        // request
        unusedSpdyPersistentConnection->DontReuse();
    }

    LOG(("   not dispatched (queued) trans=%p\n", trans));
    return NS_ERROR_NOT_AVAILABLE;                /* queue it */
}

nsresult
nsHttpConnectionMgr::DispatchTransaction(nsConnectionEntry *ent,
                                         nsHttpTransaction *trans,
                                         nsHttpConnection *conn)
{
    uint32_t caps = trans->Caps();
    int32_t priority = trans->Priority();
    nsresult rv;

    LOG(("nsHttpConnectionMgr::DispatchTransaction "
         "[ent-ci=%s %p trans=%p caps=%x conn=%p priority=%d]\n",
         ent->mConnInfo->HashKey().get(), ent, trans, caps, conn, priority));

    // It is possible for a rate-paced transaction to be dispatched independent
    // of the token bucket when the amount of parallelization has changed or
    // when a muxed connection (e.g. spdy or pipelines) becomes available.
    trans->CancelPacing(NS_OK);

    if (conn->UsingSpdy()) {
        LOG(("Spdy Dispatch Transaction via Activate(). Transaction host = %s, "
             "Connection host = %s\n",
             trans->ConnectionInfo()->Origin(),
             conn->ConnectionInfo()->Origin()));
        rv = conn->Activate(trans, caps, priority);
        MOZ_ASSERT(NS_SUCCEEDED(rv), "SPDY Cannot Fail Dispatch");
        if (NS_SUCCEEDED(rv) && !trans->GetPendingTime().IsNull()) {
            AccumulateTimeDelta(Telemetry::TRANSACTION_WAIT_TIME_SPDY,
                trans->GetPendingTime(), TimeStamp::Now());
            trans->SetPendingTime(false);
        }
        return rv;
    }

    MOZ_ASSERT(conn && !conn->Transaction(),
               "DispatchTranaction() on non spdy active connection");

    if (!(caps & NS_HTTP_ALLOW_PIPELINING))
        conn->Classify(nsAHttpTransaction::CLASS_SOLO);
    else
        conn->Classify(trans->Classification());

    rv = DispatchAbstractTransaction(ent, trans, caps, conn, priority);

    if (NS_SUCCEEDED(rv) && !trans->GetPendingTime().IsNull()) {
        if (trans->UsesPipelining())
            AccumulateTimeDelta(Telemetry::TRANSACTION_WAIT_TIME_HTTP_PIPELINES,
                trans->GetPendingTime(), TimeStamp::Now());
        else
            AccumulateTimeDelta(Telemetry::TRANSACTION_WAIT_TIME_HTTP,
                trans->GetPendingTime(), TimeStamp::Now());
        trans->SetPendingTime(false);
    }
    return rv;
}

//-----------------------------------------------------------------------------
// ConnectionHandle
//
// thin wrapper around a real connection, used to keep track of references
// to the connection to determine when the connection may be reused.  the
// transaction (or pipeline) owns a reference to this handle.  this extra
// layer of indirection greatly simplifies consumer code, avoiding the
// need for consumer code to know when to give the connection back to the
// connection manager.
//
class ConnectionHandle : public nsAHttpConnection
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSAHTTPCONNECTION(mConn)

    explicit ConnectionHandle(nsHttpConnection *conn) : mConn(conn) { }
    void Reset() { mConn = nullptr; }
private:
    virtual ~ConnectionHandle();
    RefPtr<nsHttpConnection> mConn;
};

nsAHttpConnection *
nsHttpConnectionMgr::MakeConnectionHandle(nsHttpConnection *aWrapped)
{
    return new ConnectionHandle(aWrapped);
}

ConnectionHandle::~ConnectionHandle()
{
    if (mConn) {
        gHttpHandler->ReclaimConnection(mConn);
    }
}

NS_IMPL_ISUPPORTS0(ConnectionHandle)

// Use this method for dispatching nsAHttpTransction's. It can only safely be
// used upon first use of a connection when NPN has not negotiated SPDY vs
// HTTP/1 yet as multiplexing onto an existing SPDY session requires a
// concrete nsHttpTransaction
nsresult
nsHttpConnectionMgr::DispatchAbstractTransaction(nsConnectionEntry *ent,
                                                 nsAHttpTransaction *aTrans,
                                                 uint32_t caps,
                                                 nsHttpConnection *conn,
                                                 int32_t priority)
{
    MOZ_ASSERT(!conn->UsingSpdy(),
               "Spdy Must Not Use DispatchAbstractTransaction");
    LOG(("nsHttpConnectionMgr::DispatchAbstractTransaction "
         "[ci=%s trans=%p caps=%x conn=%p]\n",
         ent->mConnInfo->HashKey().get(), aTrans, caps, conn));

    /* Use pipeline datastructure even if connection does not currently qualify
       to pipeline this transaction because a different pipeline-eligible
       transaction might be placed on the active connection. Make an exception
       for CLASS_SOLO as that connection will never pipeline until it goes
       quiescent */

    RefPtr<nsAHttpTransaction> transaction;
    nsresult rv;
    if (conn->Classification() != nsAHttpTransaction::CLASS_SOLO) {
        LOG(("   using pipeline datastructure.\n"));
        RefPtr<nsHttpPipeline> pipeline;
        rv = BuildPipeline(ent, aTrans, getter_AddRefs(pipeline));
        if (!NS_SUCCEEDED(rv))
            return rv;
        transaction = pipeline;
    }
    else {
        LOG(("   not using pipeline datastructure due to class solo.\n"));
        transaction = aTrans;
    }

    RefPtr<ConnectionHandle> handle = new ConnectionHandle(conn);

    // give the transaction the indirect reference to the connection.
    transaction->SetConnection(handle);

    rv = conn->Activate(transaction, caps, priority);
    if (NS_FAILED(rv)) {
        LOG(("  conn->Activate failed [rv=%x]\n", rv));
        ent->mActiveConns.RemoveElement(conn);
        if (conn == ent->mYellowConnection)
            ent->OnYellowComplete();
        DecrementActiveConnCount(conn);
        ConditionallyStopTimeoutTick();

        // sever back references to connection, and do so without triggering
        // a call to ReclaimConnection ;-)
        transaction->SetConnection(nullptr);
        handle->Reset(); // destroy the connection
    }

    // As transaction goes out of scope it will drop the last refernece to the
    // pipeline if activation failed, in which case this will destroy
    // the pipeline, which will cause each the transactions owned by the
    // pipeline to be restarted.

    return rv;
}

nsresult
nsHttpConnectionMgr::BuildPipeline(nsConnectionEntry *ent,
                                   nsAHttpTransaction *firstTrans,
                                   nsHttpPipeline **result)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    /* form a pipeline here even if nothing is pending so that we
       can stream-feed it as new transactions arrive */

    /* the first transaction can go in unconditionally - 1 transaction
       on a nsHttpPipeline object is not a real HTTP pipeline */

    RefPtr<nsHttpPipeline> pipeline = new nsHttpPipeline();
    pipeline->AddTransaction(firstTrans);
    pipeline.forget(result);
    return NS_OK;
}

void
nsHttpConnectionMgr::ReportProxyTelemetry(nsConnectionEntry *ent)
{
    enum { PROXY_NONE = 1, PROXY_HTTP = 2, PROXY_SOCKS = 3, PROXY_HTTPS = 4 };

    if (!ent->mConnInfo->UsingProxy())
        Telemetry::Accumulate(Telemetry::HTTP_PROXY_TYPE, PROXY_NONE);
    else if (ent->mConnInfo->UsingHttpsProxy())
        Telemetry::Accumulate(Telemetry::HTTP_PROXY_TYPE, PROXY_HTTPS);
    else if (ent->mConnInfo->UsingHttpProxy())
        Telemetry::Accumulate(Telemetry::HTTP_PROXY_TYPE, PROXY_HTTP);
    else
        Telemetry::Accumulate(Telemetry::HTTP_PROXY_TYPE, PROXY_SOCKS);
}

nsresult
nsHttpConnectionMgr::ProcessNewTransaction(nsHttpTransaction *trans)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    // since "adds" and "cancels" are processed asynchronously and because
    // various events might trigger an "add" directly on the socket thread,
    // we must take care to avoid dispatching a transaction that has already
    // been canceled (see bug 190001).
    if (NS_FAILED(trans->Status())) {
        LOG(("  transaction was canceled... dropping event!\n"));
        return NS_OK;
    }

    trans->SetPendingTime();

    Http2PushedStream *pushedStream = trans->GetPushedStream();
    if (pushedStream) {
        LOG(("  ProcessNewTransaction %p tied to h2 session push %p\n",
             trans, pushedStream->Session()));
        return pushedStream->Session()->
            AddStream(trans, trans->Priority(), false, nullptr) ?
            NS_OK : NS_ERROR_UNEXPECTED;
    }

    nsresult rv = NS_OK;
    nsHttpConnectionInfo *ci = trans->ConnectionInfo();
    MOZ_ASSERT(ci);

    nsConnectionEntry *ent =
        GetOrCreateConnectionEntry(ci, !!trans->TunnelProvider());

    // SPDY coalescing of hostnames means we might redirect from this
    // connection entry onto the preferred one.
    nsConnectionEntry *preferredEntry = GetSpdyPreferredEnt(ent);
    if (preferredEntry && (preferredEntry != ent)) {
        LOG(("nsHttpConnectionMgr::ProcessNewTransaction trans=%p "
             "redirected via coalescing from %s to %s\n", trans,
             ent->mConnInfo->Origin(), preferredEntry->mConnInfo->Origin()));

        ent = preferredEntry;
    }

    ReportProxyTelemetry(ent);

    // Check if the transaction already has a sticky reference to a connection.
    // If so, then we can just use it directly by transferring its reference
    // to the new connection variable instead of searching for a new one

    nsAHttpConnection *wrappedConnection = trans->Connection();
    RefPtr<nsHttpConnection> conn;
    if (wrappedConnection)
        conn = wrappedConnection->TakeHttpConnection();

    if (conn) {
        MOZ_ASSERT(trans->Caps() & NS_HTTP_STICKY_CONNECTION);
        LOG(("nsHttpConnectionMgr::ProcessNewTransaction trans=%p "
             "sticky connection=%p\n", trans, conn.get()));

        if (static_cast<int32_t>(ent->mActiveConns.IndexOf(conn)) == -1) {
            LOG(("nsHttpConnectionMgr::ProcessNewTransaction trans=%p "
                 "sticky connection=%p needs to go on the active list\n", trans, conn.get()));

            // make sure it isn't on the idle list - we expect this to be an
            // unknown fresh connection
            MOZ_ASSERT(static_cast<int32_t>(ent->mIdleConns.IndexOf(conn)) == -1);
            MOZ_ASSERT(!conn->IsExperienced());

            AddActiveConn(conn, ent); // make it active
        }

        trans->SetConnection(nullptr);
        rv = DispatchTransaction(ent, trans, conn);
    } else {
        rv = TryDispatchTransaction(ent, !!trans->TunnelProvider(), trans);
    }

    if (NS_SUCCEEDED(rv)) {
        LOG(("  ProcessNewTransaction Dispatch Immediately trans=%p\n", trans));
        return rv;
    }

    if (rv == NS_ERROR_NOT_AVAILABLE) {
        LOG(("  adding transaction to pending queue "
             "[trans=%p pending-count=%u]\n",
             trans, ent->mPendingQ.Length()+1));
        // put this transaction on the pending queue...
        InsertTransactionSorted(ent->mPendingQ, trans);
        return NS_OK;
    }

    LOG(("  ProcessNewTransaction Hard Error trans=%p rv=%x\n", trans, rv));
    return rv;
}


void
nsHttpConnectionMgr::AddActiveConn(nsHttpConnection *conn,
                                   nsConnectionEntry *ent)
{
    ent->mActiveConns.AppendElement(conn);
    mNumActiveConns++;
    ActivateTimeoutTick();
}

void
nsHttpConnectionMgr::DecrementActiveConnCount(nsHttpConnection *conn)
{
    mNumActiveConns--;
    if (conn->EverUsedSpdy())
        mNumSpdyActiveConns--;
}

void
nsHttpConnectionMgr::StartedConnect()
{
    mNumActiveConns++;
    ActivateTimeoutTick(); // likely disabled by RecvdConnect()
}

void
nsHttpConnectionMgr::RecvdConnect()
{
    mNumActiveConns--;
    ConditionallyStopTimeoutTick();
}

nsresult
nsHttpConnectionMgr::CreateTransport(nsConnectionEntry *ent,
                                     nsAHttpTransaction *trans,
                                     uint32_t caps,
                                     bool speculative,
                                     bool isFromPredictor,
                                     bool allow1918)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    RefPtr<nsHalfOpenSocket> sock = new nsHalfOpenSocket(ent, trans, caps);
    if (speculative) {
        sock->SetSpeculative(true);
        sock->SetAllow1918(allow1918);
        Telemetry::AutoCounter<Telemetry::HTTPCONNMGR_TOTAL_SPECULATIVE_CONN> totalSpeculativeConn;
        ++totalSpeculativeConn;

        if (isFromPredictor) {
          sock->SetIsFromPredictor(true);
          Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS_CREATED> totalPreconnectsCreated;
          ++totalPreconnectsCreated;
        }
    }

    // The socket stream holds the reference to the half open
    // socket - so if the stream fails to init the half open
    // will go away.
    nsresult rv = sock->SetupPrimaryStreams();
    NS_ENSURE_SUCCESS(rv, rv);

    ent->mHalfOpens.AppendElement(sock);
    mNumHalfOpenConns++;
    return NS_OK;
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
    if (!conn || !conn->CanDirectlyActivate())
        return;

    nsTArray<RefPtr<nsHttpTransaction> > leftovers;
    uint32_t index;

    // Dispatch all the transactions we can
    for (index = 0;
         index < ent->mPendingQ.Length() && conn->CanDirectlyActivate();
         ++index) {
        nsHttpTransaction *trans = ent->mPendingQ[index];

        if (!(trans->Caps() & NS_HTTP_ALLOW_KEEPALIVE) ||
            trans->Caps() & NS_HTTP_DISALLOW_SPDY) {
            leftovers.AppendElement(trans);
            continue;
        }

        nsresult rv = DispatchTransaction(ent, trans, conn);
        if (NS_FAILED(rv)) {
            // this cannot happen, but if due to some bug it does then
            // close the transaction
            MOZ_ASSERT(false, "Dispatch SPDY Transaction");
            LOG(("ProcessSpdyPendingQ Dispatch Transaction failed trans=%p\n",
                    trans));
            trans->Close(rv);
        }
    }

    // Slurp up the rest of the pending queue into our leftovers bucket (we
    // might have some left if conn->CanDirectlyActivate returned false)
    for (; index < ent->mPendingQ.Length(); ++index) {
        nsHttpTransaction *trans = ent->mPendingQ[index];
        leftovers.AppendElement(trans);
    }

    // Put the leftovers back in the pending queue and get rid of the
    // transactions we dispatched
    leftovers.SwapElements(ent->mPendingQ);
    leftovers.Clear();
}

void
nsHttpConnectionMgr::OnMsgProcessAllSpdyPendingQ(int32_t, ARefBase *)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::OnMsgProcessAllSpdyPendingQ\n"));
    for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
        ProcessSpdyPendingQ(iter.Data());
    }
}

nsHttpConnection *
nsHttpConnectionMgr::GetSpdyPreferredConn(nsConnectionEntry *ent)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(ent);

    nsConnectionEntry *preferred = GetSpdyPreferredEnt(ent);
    // this entry is spdy-enabled if it is involved in a redirect
    if (preferred) {
        // all new connections for this entry will use spdy too
        ent->mUsingSpdy = true;
    } else {
        preferred = ent;
    }

    if (!preferred->mUsingSpdy) {
        return nullptr;
    }

    nsHttpConnection *rv = nullptr;
    uint32_t activeLen = preferred->mActiveConns.Length();
    uint32_t index;

    // activeLen should generally be 1.. this is a setup race being resolved
    // take a conn who can activate and is experienced
    for (index = 0; index < activeLen; ++index) {
        nsHttpConnection *tmp = preferred->mActiveConns[index];
        if (tmp->CanDirectlyActivate() && tmp->IsExperienced()) {
            rv = tmp;
            break;
        }
    }

    // if that worked, cleanup anything else
    if (rv) {
        for (index = 0; index < activeLen; ++index) {
            nsHttpConnection *tmp = preferred->mActiveConns[index];
            // in the case where there is a functional h2 session, drop the others
            if (tmp != rv) {
                tmp->DontReuse();
            }
        }
        return rv;
    }

    // take a conn who can activate and leave the rest alone
    for (index = 0; index < activeLen; ++index) {
        nsHttpConnection *tmp = preferred->mActiveConns[index];
        if (tmp->CanDirectlyActivate()) {
            rv = tmp;
            break;
        }
    }
    return rv;
}

//-----------------------------------------------------------------------------

void
nsHttpConnectionMgr::OnMsgShutdown(int32_t, ARefBase *param)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::OnMsgShutdown\n"));

    gHttpHandler->StopRequestTokenBucket();

    for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
        nsAutoPtr<nsConnectionEntry>& ent = iter.Data();

        // Close all active connections.
        while (ent->mActiveConns.Length()) {
            RefPtr<nsHttpConnection> conn(ent->mActiveConns[0]);
            ent->mActiveConns.RemoveElementAt(0);
            DecrementActiveConnCount(conn);
            conn->Close(NS_ERROR_ABORT, true);
        }

        // Close all idle connections.
        while (ent->mIdleConns.Length()) {
            RefPtr<nsHttpConnection> conn(ent->mIdleConns[0]);

            ent->mIdleConns.RemoveElementAt(0);
            mNumIdleConns--;

            conn->Close(NS_ERROR_ABORT);
        }

        // If all idle connections are removed we can stop pruning dead
        // connections.
        ConditionallyStopPruneDeadConnectionsTimer();

        // Close all pending transactions.
        while (ent->mPendingQ.Length()) {
            nsHttpTransaction *trans = ent->mPendingQ[0];
            trans->Close(NS_ERROR_ABORT);
            ent->mPendingQ.RemoveElementAt(0);
        }

        // Close all half open tcp connections.
        for (int32_t i = int32_t(ent->mHalfOpens.Length()) - 1; i >= 0; i--) {
            ent->mHalfOpens[i]->Abandon();
        }

        iter.Remove();
    }

    if (mTimeoutTick) {
        mTimeoutTick->Cancel();
        mTimeoutTick = nullptr;
        mTimeoutTickArmed = false;
    }
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
    if (mTrafficTimer) {
      mTrafficTimer->Cancel();
      mTrafficTimer = nullptr;
    }

    // signal shutdown complete
    nsCOMPtr<nsIRunnable> runnable =
        new ConnEvent(this, &nsHttpConnectionMgr::OnMsgShutdownConfirm,
                      0, param);
    NS_DispatchToMainThread(runnable);
}

void
nsHttpConnectionMgr::OnMsgShutdownConfirm(int32_t priority, ARefBase *param)
{
    MOZ_ASSERT(NS_IsMainThread());
    LOG(("nsHttpConnectionMgr::OnMsgShutdownConfirm\n"));

    BoolWrapper *shutdown = static_cast<BoolWrapper *>(param);
    shutdown->mBool = true;
}

void
nsHttpConnectionMgr::OnMsgNewTransaction(int32_t priority, ARefBase *param)
{
    LOG(("nsHttpConnectionMgr::OnMsgNewTransaction [trans=%p]\n", param));

    nsHttpTransaction *trans = static_cast<nsHttpTransaction *>(param);
    trans->SetPriority(priority);
    nsresult rv = ProcessNewTransaction(trans);
    if (NS_FAILED(rv))
        trans->Close(rv); // for whatever its worth
}

void
nsHttpConnectionMgr::OnMsgReschedTransaction(int32_t priority, ARefBase *param)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::OnMsgReschedTransaction [trans=%p]\n", param));

    RefPtr<nsHttpTransaction> trans = static_cast<nsHttpTransaction *>(param);
    trans->SetPriority(priority);

    nsConnectionEntry *ent = LookupConnectionEntry(trans->ConnectionInfo(),
                                                   nullptr, trans);

    if (ent) {
        int32_t index = ent->mPendingQ.IndexOf(trans);
        if (index >= 0) {
            ent->mPendingQ.RemoveElementAt(index);
            InsertTransactionSorted(ent->mPendingQ, trans);
        }
    }
}

void
nsHttpConnectionMgr::OnMsgCancelTransaction(int32_t reason, ARefBase *param)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::OnMsgCancelTransaction [trans=%p]\n", param));

    nsresult closeCode = static_cast<nsresult>(reason);

    // caller holds a ref to param/trans on stack
    nsHttpTransaction *trans = static_cast<nsHttpTransaction *>(param);

    //
    // if the transaction owns a connection and the transaction is not done,
    // then ask the connection to close the transaction.  otherwise, close the
    // transaction directly (removing it from the pending queue first).
    //
    RefPtr<nsAHttpConnection> conn(trans->Connection());
    if (conn && !trans->IsDone()) {
        conn->CloseTransaction(trans, closeCode);
    } else {
        nsConnectionEntry *ent =
            LookupConnectionEntry(trans->ConnectionInfo(), nullptr, trans);

        if (ent) {
            int32_t transIndex = ent->mPendingQ.IndexOf(trans);
            if (transIndex >= 0) {
                LOG(("nsHttpConnectionMgr::OnMsgCancelTransaction [trans=%p]"
                     " found in pending queue\n", trans));
                ent->mPendingQ.RemoveElementAt(transIndex);
            }

            // Abandon all half-open sockets belonging to the given transaction.
            for (uint32_t index = 0;
                 index < ent->mHalfOpens.Length();
                 ++index) {
                nsHalfOpenSocket *half = ent->mHalfOpens[index];
                if (trans == half->Transaction()) {
                    half->Abandon();
                    // there is only one, and now mHalfOpens[] has been changed.
                    break;
                }
            }
        }

        trans->Close(closeCode);

        // Cancel is a pretty strong signal that things might be hanging
        // so we want to cancel any null transactions related to this connection
        // entry. They are just optimizations, but they aren't hooked up to
        // anything that might get canceled from the rest of gecko, so best
        // to assume that's what was meant by the cancel we did receive if
        // it only applied to something in the queue.
        for (uint32_t index = 0;
             ent && (index < ent->mActiveConns.Length());
             ++index) {
            nsHttpConnection *activeConn = ent->mActiveConns[index];
            nsAHttpTransaction *liveTransaction = activeConn->Transaction();
            if (liveTransaction && liveTransaction->IsNullTransaction()) {
                LOG(("nsHttpConnectionMgr::OnMsgCancelTransaction [trans=%p] "
                     "also canceling Null Transaction %p on conn %p\n",
                     trans, liveTransaction, activeConn));
                activeConn->CloseTransaction(liveTransaction, closeCode);
            }
        }
    }
}

void
nsHttpConnectionMgr::OnMsgProcessPendingQ(int32_t, ARefBase *param)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    nsHttpConnectionInfo *ci = static_cast<nsHttpConnectionInfo *>(param);

    if (!ci) {
        LOG(("nsHttpConnectionMgr::OnMsgProcessPendingQ [ci=nullptr]\n"));
        // Try and dispatch everything
        for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
            ProcessPendingQForEntry(iter.Data(), true);
        }
        return;
    }

    LOG(("nsHttpConnectionMgr::OnMsgProcessPendingQ [ci=%s]\n",
         ci->HashKey().get()));

    // start by processing the queue identified by the given connection info.
    nsConnectionEntry *ent = mCT.Get(ci->HashKey());
    if (!(ent && ProcessPendingQForEntry(ent, false))) {
        // if we reach here, it means that we couldn't dispatch a transaction
        // for the specified connection info.  walk the connection table...
        for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
            if (ProcessPendingQForEntry(iter.Data(), false)) {
                break;
            }
        }
    }
}

nsresult
nsHttpConnectionMgr::CancelTransactions(nsHttpConnectionInfo *ci, nsresult code)
{
    LOG(("nsHttpConnectionMgr::CancelTransactions %s\n",ci->HashKey().get()));

    int32_t intReason = static_cast<int32_t>(code);
    return PostEvent(&nsHttpConnectionMgr::OnMsgCancelTransactions, intReason, ci);
}

void
nsHttpConnectionMgr::OnMsgCancelTransactions(int32_t code, ARefBase *param)
{
    nsresult reason = static_cast<nsresult>(code);
    nsHttpConnectionInfo *ci = static_cast<nsHttpConnectionInfo *>(param);
    nsConnectionEntry *ent = mCT.Get(ci->HashKey());
    LOG(("nsHttpConnectionMgr::OnMsgCancelTransactions %s %p\n",
         ci->HashKey().get(), ent));
    if (!ent) {
        return;
    }

    for (int32_t i = ent->mPendingQ.Length() - 1; i >= 0; --i) {
        nsHttpTransaction *trans = ent->mPendingQ[i];
        LOG(("nsHttpConnectionMgr::OnMsgCancelTransactions %s %p %p\n",
             ci->HashKey().get(), ent, trans));
        trans->Close(reason);
        ent->mPendingQ.RemoveElementAt(i);
    }
}

void
nsHttpConnectionMgr::OnMsgPruneDeadConnections(int32_t, ARefBase *)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::OnMsgPruneDeadConnections\n"));

    // Reset mTimeOfNextWakeUp so that we can find a new shortest value.
    mTimeOfNextWakeUp = UINT64_MAX;

    // check canreuse() for all idle connections plus any active connections on
    // connection entries that are using spdy.
    if (mNumIdleConns || (mNumActiveConns && gHttpHandler->IsSpdyEnabled())) {
        for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
            nsAutoPtr<nsConnectionEntry>& ent = iter.Data();

            LOG(("  pruning [ci=%s]\n", ent->mConnInfo->HashKey().get()));

            // Find out how long it will take for next idle connection to not
            // be reusable anymore.
            uint32_t timeToNextExpire = UINT32_MAX;
            int32_t count = ent->mIdleConns.Length();
            if (count > 0) {
                for (int32_t i = count - 1; i >= 0; --i) {
                    RefPtr<nsHttpConnection> conn(ent->mIdleConns[i]);
                    if (!conn->CanReuse()) {
                        ent->mIdleConns.RemoveElementAt(i);
                        conn->Close(NS_ERROR_ABORT);
                        mNumIdleConns--;
                    } else {
                        timeToNextExpire =
                            std::min(timeToNextExpire, conn->TimeToLive());
                    }
                }
            }

            if (ent->mUsingSpdy) {
                for (uint32_t i = 0; i < ent->mActiveConns.Length(); ++i) {
                    nsHttpConnection* conn = ent->mActiveConns[i];
                    if (conn->UsingSpdy()) {
                        if (!conn->CanReuse()) {
                            // Marking it don't-reuse will create an active
                            // tear down if the spdy session is idle.
                            conn->DontReuse();
                        } else {
                            timeToNextExpire =
                                std::min(timeToNextExpire, conn->TimeToLive());
                        }
                    }
                }
            }

            // If time to next expire found is shorter than time to next
            // wake-up, we need to change the time for next wake-up.
            if (timeToNextExpire != UINT32_MAX) {
                uint32_t now = NowInSeconds();
                uint64_t timeOfNextExpire = now + timeToNextExpire;
                // If pruning of dead connections is not already scheduled to
                // happen or time found for next connection to expire is is
                // before mTimeOfNextWakeUp, we need to schedule the pruning to
                // happen after timeToNextExpire.
                if (!mTimer || timeOfNextExpire < mTimeOfNextWakeUp) {
                    PruneDeadConnectionsAfter(timeToNextExpire);
                }
            } else {
                ConditionallyStopPruneDeadConnectionsTimer();
            }

            // If this entry is empty, we have too many entries, and this
            // doesn't represent some painfully determined red condition, then
            // we can clean it up and restart from yellow.
            if (ent->PipelineState()       != PS_RED &&
                mCT.Count()                >  125 &&
                ent->mIdleConns.Length()   == 0 &&
                ent->mActiveConns.Length() == 0 &&
                ent->mHalfOpens.Length()   == 0 &&
                ent->mPendingQ.Length()    == 0 &&
                (!ent->mUsingSpdy || mCT.Count() > 300)) {
                LOG(("    removing empty connection entry\n"));
                iter.Remove();
                continue;
            }

            // Otherwise use this opportunity to compact our arrays...
            ent->mIdleConns.Compact();
            ent->mActiveConns.Compact();
            ent->mPendingQ.Compact();
        }
    }
}

void
nsHttpConnectionMgr::OnMsgPruneNoTraffic(int32_t, ARefBase *)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::OnMsgPruneNoTraffic\n"));

    // Prune connections without traffic
    for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {

        // Close the connections with no registered traffic.
        nsAutoPtr<nsConnectionEntry>& ent = iter.Data();

        LOG(("  pruning no traffic [ci=%s]\n",
             ent->mConnInfo->HashKey().get()));

        uint32_t numConns = ent->mActiveConns.Length();
        if (numConns) {
            // Walk the list backwards to allow us to remove entries easily.
            for (int index = numConns - 1; index >= 0; index--) {
                if (ent->mActiveConns[index]->NoTraffic()) {
                    RefPtr<nsHttpConnection> conn = ent->mActiveConns[index];
                    ent->mActiveConns.RemoveElementAt(index);
                    DecrementActiveConnCount(conn);
                    conn->Close(NS_ERROR_ABORT);
                    LOG(("  closed active connection due to no traffic "
                         "[conn=%p]\n", conn.get()));
                }
            }
        }
    }

    mPruningNoTraffic = false; // not pruning anymore
}

void
nsHttpConnectionMgr::OnMsgVerifyTraffic(int32_t, ARefBase *)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::OnMsgVerifyTraffic\n"));

    if (mPruningNoTraffic) {
      // Called in the time gap when the timeout to prune notraffic
      // connections has triggered but the pruning hasn't happened yet.
      return;
    }

    // Mark connections for traffic verification
    for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
        nsAutoPtr<nsConnectionEntry>& ent = iter.Data();

        // Iterate over all active connections and check them.
        for (uint32_t index = 0; index < ent->mActiveConns.Length(); ++index) {
            ent->mActiveConns[index]->CheckForTraffic(true);
        }
        // Iterate the idle connections and unmark them for traffic checks.
        for (uint32_t index = 0; index < ent->mIdleConns.Length(); ++index) {
            ent->mIdleConns[index]->CheckForTraffic(false);
        }
    }

    // If the timer is already there. we just re-init it
    if(!mTrafficTimer) {
        mTrafficTimer = do_CreateInstance("@mozilla.org/timer;1");
    }

    // failure to create a timer is not a fatal error, but dead
    // connections will not be cleaned up as nicely
    if (mTrafficTimer) {
        // Give active connections time to get more traffic before killing
        // them off. Default: 5000 milliseconds
        mTrafficTimer->Init(this, gHttpHandler->NetworkChangedTimeout(),
                            nsITimer::TYPE_ONE_SHOT);
    } else {
        NS_WARNING("failed to create timer for VerifyTraffic!");
    }
}

void
nsHttpConnectionMgr::OnMsgDoShiftReloadConnectionCleanup(int32_t, ARefBase *param)
{
    LOG(("nsHttpConnectionMgr::OnMsgDoShiftReloadConnectionCleanup\n"));
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    nsHttpConnectionInfo *ci = static_cast<nsHttpConnectionInfo *>(param);

    for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
        ClosePersistentConnections(iter.Data());
    }

    if (ci)
        ResetIPFamilyPreference(ci);
}

void
nsHttpConnectionMgr::OnMsgReclaimConnection(int32_t, ARefBase *param)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::OnMsgReclaimConnection [conn=%p]\n", param));

    nsHttpConnection *conn = static_cast<nsHttpConnection *>(param);

    //
    // 1) remove the connection from the active list
    // 2) if keep-alive, add connection to idle list
    // 3) post event to process the pending transaction queue
    //

    nsConnectionEntry *ent = LookupConnectionEntry(conn->ConnectionInfo(),
                                                   conn, nullptr);
    if (!ent) {
        // this can happen if the connection is made outside of the
        // connection manager and is being "reclaimed" for use with
        // future transactions. HTTP/2 tunnels work like this.
        ent = GetOrCreateConnectionEntry(conn->ConnectionInfo(), true);
        LOG(("nsHttpConnectionMgr::OnMsgReclaimConnection conn %p "
             "forced new hash entry %s\n",
             conn, conn->ConnectionInfo()->HashKey().get()));
    }

    MOZ_ASSERT(ent);
    RefPtr<nsHttpConnectionInfo> ci(ent->mConnInfo);

    // If the connection is in the active list, remove that entry
    // and the reference held by the mActiveConns list.
    // This is never the final reference on conn as the event context
    // is also holding one that is released at the end of this function.

    if (conn->EverUsedSpdy()) {
        // Spdy connections aren't reused in the traditional HTTP way in
        // the idleconns list, they are actively multplexed as active
        // conns. Even when they have 0 transactions on them they are
        // considered active connections. So when one is reclaimed it
        // is really complete and is meant to be shut down and not
        // reused.
        conn->DontReuse();
    }

    // a connection that still holds a reference to a transaction was
    // not closed naturally (i.e. it was reset or aborted) and is
    // therefore not something that should be reused.
    if (conn->Transaction()) {
        conn->DontReuse();
    }

    if (ent->mActiveConns.RemoveElement(conn)) {
        if (conn == ent->mYellowConnection) {
            ent->OnYellowComplete();
        }
        DecrementActiveConnCount(conn);
        ConditionallyStopTimeoutTick();
    }

    if (conn->CanReuse()) {
        LOG(("  adding connection to idle list\n"));
        // Keep The idle connection list sorted with the connections that
        // have moved the largest data pipelines at the front because these
        // connections have the largest cwnds on the server.

        // The linear search is ok here because the number of idleconns
        // in a single entry is generally limited to a small number (i.e. 6)

        uint32_t idx;
        for (idx = 0; idx < ent->mIdleConns.Length(); idx++) {
            nsHttpConnection *idleConn = ent->mIdleConns[idx];
            if (idleConn->MaxBytesRead() < conn->MaxBytesRead())
                break;
        }

        ent->mIdleConns.InsertElementAt(idx, conn);
        mNumIdleConns++;
        conn->BeginIdleMonitoring();

        // If the added connection was first idle connection or has shortest
        // time to live among the watched connections, pruning dead
        // connections needs to be done when it can't be reused anymore.
        uint32_t timeToLive = conn->TimeToLive();
        if(!mTimer || NowInSeconds() + timeToLive < mTimeOfNextWakeUp)
            PruneDeadConnectionsAfter(timeToLive);
    } else {
        LOG(("  connection cannot be reused; closing connection\n"));
        conn->Close(NS_ERROR_ABORT);
    }

    OnMsgProcessPendingQ(0, ci);
}

void
nsHttpConnectionMgr::OnMsgCompleteUpgrade(int32_t, ARefBase *param)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    nsCompleteUpgradeData *data = static_cast<nsCompleteUpgradeData *>(param);
    LOG(("nsHttpConnectionMgr::OnMsgCompleteUpgrade "
         "this=%p conn=%p listener=%p\n", this, data->mConn.get(),
         data->mUpgradeListener.get()));

    nsCOMPtr<nsISocketTransport> socketTransport;
    nsCOMPtr<nsIAsyncInputStream> socketIn;
    nsCOMPtr<nsIAsyncOutputStream> socketOut;

    nsresult rv;
    rv = data->mConn->TakeTransport(getter_AddRefs(socketTransport),
                                    getter_AddRefs(socketIn),
                                    getter_AddRefs(socketOut));

    if (NS_SUCCEEDED(rv))
        data->mUpgradeListener->OnTransportAvailable(socketTransport,
                                                     socketIn,
                                                     socketOut);
}

void
nsHttpConnectionMgr::OnMsgUpdateParam(int32_t inParam, ARefBase *)
{
    uint32_t param = static_cast<uint32_t>(inParam);
    uint16_t name  = ((param) & 0xFFFF0000) >> 16;
    uint16_t value =  param & 0x0000FFFF;

    switch (name) {
    case MAX_CONNECTIONS:
        mMaxConns = value;
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
    case MAX_OPTIMISTIC_PIPELINED_REQUESTS:
        mMaxOptimisticPipelinedRequests = value;
        break;
    default:
        NS_NOTREACHED("unexpected parameter name");
    }
}

// nsHttpConnectionMgr::nsConnectionEntry
nsHttpConnectionMgr::nsConnectionEntry::~nsConnectionEntry()
{
    MOZ_COUNT_DTOR(nsConnectionEntry);
    gHttpHandler->ConnMgr()->RemovePreferredHash(this);
}

void
nsHttpConnectionMgr::OnMsgProcessFeedback(int32_t, ARefBase *param)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    nsHttpPipelineFeedback *fb = static_cast<nsHttpPipelineFeedback *>(param);
    PipelineFeedbackInfo(fb->mConnInfo, fb->mInfo, fb->mConn, fb->mData);
}

// Read Timeout Tick handlers

void
nsHttpConnectionMgr::ActivateTimeoutTick()
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnectionMgr::ActivateTimeoutTick() "
         "this=%p mTimeoutTick=%p\n", this, mTimeoutTick.get()));

    // The timer tick should be enabled if it is not already pending.
    // Upon running the tick will rearm itself if there are active
    // connections available.

    if (mTimeoutTick && mTimeoutTickArmed) {
        // make sure we get one iteration on a quick tick
        if (mTimeoutTickNext > 1) {
            mTimeoutTickNext = 1;
            mTimeoutTick->SetDelay(1000);
        }
        return;
    }

    if (!mTimeoutTick) {
        mTimeoutTick = do_CreateInstance(NS_TIMER_CONTRACTID);
        if (!mTimeoutTick) {
            NS_WARNING("failed to create timer for http timeout management");
            return;
        }
        mTimeoutTick->SetTarget(mSocketThreadTarget);
    }

    MOZ_ASSERT(!mTimeoutTickArmed, "timer tick armed");
    mTimeoutTickArmed = true;
    mTimeoutTick->Init(this, 1000, nsITimer::TYPE_REPEATING_SLACK);
}

void
nsHttpConnectionMgr::TimeoutTick()
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(mTimeoutTick, "no readtimeout tick");

    LOG(("nsHttpConnectionMgr::TimeoutTick active=%d\n", mNumActiveConns));
    // The next tick will be between 1 second and 1 hr
    // Set it to the max value here, and the TimeoutTick()s can
    // reduce it to their local needs.
    mTimeoutTickNext = 3600; // 1hr

    for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
        nsAutoPtr<nsConnectionEntry>& ent = iter.Data();

        LOG(("nsHttpConnectionMgr::TimeoutTick() this=%p host=%s "
             "idle=%d active=%d half-len=%d pending=%d\n",
             this, ent->mConnInfo->Origin(), ent->mIdleConns.Length(),
             ent->mActiveConns.Length(), ent->mHalfOpens.Length(),
             ent->mPendingQ.Length()));

        // First call the tick handler for each active connection.
        PRIntervalTime tickTime = PR_IntervalNow();
        for (uint32_t index = 0; index < ent->mActiveConns.Length(); ++index) {
            uint32_t connNextTimeout =
                ent->mActiveConns[index]->ReadTimeoutTick(tickTime);
            mTimeoutTickNext = std::min(mTimeoutTickNext, connNextTimeout);
        }

        // Now check for any stalled half open sockets.
        if (ent->mHalfOpens.Length()) {
            TimeStamp currentTime = TimeStamp::Now();
            double maxConnectTime_ms = gHttpHandler->ConnectTimeout();

            for (uint32_t index = ent->mHalfOpens.Length(); index > 0; ) {
                index--;

                nsHalfOpenSocket *half = ent->mHalfOpens[index];
                double delta = half->Duration(currentTime);
                // If the socket has timed out, close it so the waiting
                // transaction will get the proper signal.
                if (delta > maxConnectTime_ms) {
                    LOG(("Force timeout of half open to %s after %.2fms.\n",
                         ent->mConnInfo->HashKey().get(), delta));
                    if (half->SocketTransport()) {
                        half->SocketTransport()->Close(NS_ERROR_ABORT);
                    }
                    if (half->BackupTransport()) {
                        half->BackupTransport()->Close(NS_ERROR_ABORT);
                    }
                }

                // If this half open hangs around for 5 seconds after we've
                // closed() it then just abandon the socket.
                if (delta > maxConnectTime_ms + 5000) {
                    LOG(("Abandon half open to %s after %.2fms.\n",
                         ent->mConnInfo->HashKey().get(), delta));
                    half->Abandon();
                }
            }
        }
        if (ent->mHalfOpens.Length()) {
            mTimeoutTickNext = 1;
        }
    }

    if (mTimeoutTick) {
        mTimeoutTickNext = std::max(mTimeoutTickNext, 1U);
        mTimeoutTick->SetDelay(mTimeoutTickNext * 1000);
    }
}

// GetOrCreateConnectionEntry finds a ent for a particular CI for use in
// dispatching a transaction according to these rules
// 1] use an ent that matches the ci that can be dispatched immediately
// 2] otherwise use an ent of wildcard(ci) than can be dispatched immediately
// 3] otherwise create an ent that matches ci and make new conn on it

nsHttpConnectionMgr::nsConnectionEntry *
nsHttpConnectionMgr::GetOrCreateConnectionEntry(nsHttpConnectionInfo *specificCI,
                                                bool prohibitWildCard)
{
    // step 1
    nsConnectionEntry *specificEnt = mCT.Get(specificCI->HashKey());
    if (specificEnt && specificEnt->AvailableForDispatchNow()) {
        return specificEnt;
    }

    if (!specificCI->UsingHttpsProxy()) {
        prohibitWildCard = true;
    }

    // step 2
    if (!prohibitWildCard) {
        RefPtr<nsHttpConnectionInfo> wildCardProxyCI;
        specificCI->CreateWildCard(getter_AddRefs(wildCardProxyCI));
        nsConnectionEntry *wildCardEnt = mCT.Get(wildCardProxyCI->HashKey());
        if (wildCardEnt && wildCardEnt->AvailableForDispatchNow()) {
            return wildCardEnt;
        }
    }

    // step 3
    if (!specificEnt) {
        RefPtr<nsHttpConnectionInfo> clone(specificCI->Clone());
        specificEnt = new nsConnectionEntry(clone);
        mCT.Put(clone->HashKey(), specificEnt);
    }
    return specificEnt;
}

nsresult
ConnectionHandle::OnHeadersAvailable(nsAHttpTransaction *trans,
                                     nsHttpRequestHead *req,
                                     nsHttpResponseHead *resp,
                                     bool *reset)
{
    return mConn->OnHeadersAvailable(trans, req, resp, reset);
}

void
ConnectionHandle::CloseTransaction(nsAHttpTransaction *trans, nsresult reason)
{
    mConn->CloseTransaction(trans, reason);
}

nsresult
ConnectionHandle::TakeTransport(nsISocketTransport  **aTransport,
                                nsIAsyncInputStream **aInputStream,
                                nsIAsyncOutputStream **aOutputStream)
{
    return mConn->TakeTransport(aTransport, aInputStream, aOutputStream);
}

void
nsHttpConnectionMgr::OnMsgSpeculativeConnect(int32_t, ARefBase *param)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    SpeculativeConnectArgs *args = static_cast<SpeculativeConnectArgs *>(param);

    LOG(("nsHttpConnectionMgr::OnMsgSpeculativeConnect [ci=%s]\n",
         args->mTrans->ConnectionInfo()->HashKey().get()));

    nsConnectionEntry *ent =
        GetOrCreateConnectionEntry(args->mTrans->ConnectionInfo(), false);

    // If spdy has previously made a preferred entry for this host via
    // the ip pooling rules. If so, connect to the preferred host instead of
    // the one directly passed in here.
    nsConnectionEntry *preferredEntry = GetSpdyPreferredEnt(ent);
    if (preferredEntry)
        ent = preferredEntry;

    uint32_t parallelSpeculativeConnectLimit =
        gHttpHandler->ParallelSpeculativeConnectLimit();
    bool ignoreIdle = false;
    bool isFromPredictor = false;
    bool allow1918 = false;

    if (args->mOverridesOK) {
        parallelSpeculativeConnectLimit = args->mParallelSpeculativeConnectLimit;
        ignoreIdle = args->mIgnoreIdle;
        isFromPredictor = args->mIsFromPredictor;
        allow1918 = args->mAllow1918;
    }

    bool keepAlive = args->mTrans->Caps() & NS_HTTP_ALLOW_KEEPALIVE;
    if (mNumHalfOpenConns < parallelSpeculativeConnectLimit &&
        ((ignoreIdle && (ent->mIdleConns.Length() < parallelSpeculativeConnectLimit)) ||
         !ent->mIdleConns.Length()) &&
        !(keepAlive && RestrictConnections(ent)) &&
        !AtActiveConnectionLimit(ent, args->mTrans->Caps())) {
        CreateTransport(ent, args->mTrans, args->mTrans->Caps(), true, isFromPredictor, allow1918);
    } else {
        LOG(("OnMsgSpeculativeConnect Transport "
             "not created due to existing connection count\n"));
    }
}

bool
ConnectionHandle::IsPersistent()
{
    return mConn->IsPersistent();
}

bool
ConnectionHandle::IsReused()
{
    return mConn->IsReused();
}

void
ConnectionHandle::DontReuse()
{
    mConn->DontReuse();
}

nsresult
ConnectionHandle::PushBack(const char *buf, uint32_t bufLen)
{
    return mConn->PushBack(buf, bufLen);
}


//////////////////////// nsHalfOpenSocket

NS_IMPL_ISUPPORTS(nsHttpConnectionMgr::nsHalfOpenSocket,
                  nsIOutputStreamCallback,
                  nsITransportEventSink,
                  nsIInterfaceRequestor,
                  nsITimerCallback)

nsHttpConnectionMgr::
nsHalfOpenSocket::nsHalfOpenSocket(nsConnectionEntry *ent,
                                   nsAHttpTransaction *trans,
                                   uint32_t caps)
    : mEnt(ent)
    , mTransaction(trans)
    , mDispatchedMTransaction(false)
    , mCaps(caps)
    , mSpeculative(false)
    , mIsFromPredictor(false)
    , mAllow1918(true)
    , mHasConnected(false)
    , mPrimaryConnectedOK(false)
    , mBackupConnectedOK(false)
{
    MOZ_ASSERT(ent && trans, "constructor with null arguments");
    LOG(("Creating nsHalfOpenSocket [this=%p trans=%p ent=%s key=%s]\n",
         this, trans, ent->mConnInfo->Origin(), ent->mConnInfo->HashKey().get()));
}

nsHttpConnectionMgr::nsHalfOpenSocket::~nsHalfOpenSocket()
{
    MOZ_ASSERT(!mStreamOut);
    MOZ_ASSERT(!mBackupStreamOut);
    MOZ_ASSERT(!mSynTimer);
    LOG(("Destroying nsHalfOpenSocket [this=%p]\n", this));

    if (mEnt)
        mEnt->RemoveHalfOpen(this);
}

nsresult
nsHttpConnectionMgr::
nsHalfOpenSocket::SetupStreams(nsISocketTransport **transport,
                               nsIAsyncInputStream **instream,
                               nsIAsyncOutputStream **outstream,
                               bool isBackup)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    nsresult rv;
    const char *socketTypes[1];
    uint32_t typeCount = 0;
    const nsHttpConnectionInfo *ci = mEnt->mConnInfo;
    if (ci->FirstHopSSL()) {
        socketTypes[typeCount++] = "ssl";
    } else {
        socketTypes[typeCount] = gHttpHandler->DefaultSocketType();
        if (socketTypes[typeCount]) {
            typeCount++;
        }
    }

    nsCOMPtr<nsISocketTransport> socketTransport;
    nsCOMPtr<nsISocketTransportService> sts;

    sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("nsHalfOpenSocket::SetupStreams [this=%p ent=%s] "
         "setup routed transport to origin %s:%d via %s:%d\n",
         this, ci->HashKey().get(),
         ci->Origin(), ci->OriginPort(), ci->RoutedHost(), ci->RoutedPort()));

    nsCOMPtr<nsIRoutedSocketTransportService> routedSTS(do_QueryInterface(sts));
    if (routedSTS) {
        rv = routedSTS->CreateRoutedTransport(
            socketTypes, typeCount,
            ci->GetOrigin(), ci->OriginPort(), ci->GetRoutedHost(), ci->RoutedPort(),
            ci->ProxyInfo(), getter_AddRefs(socketTransport));
    } else {
        if (!ci->GetRoutedHost().IsEmpty()) {
            // There is a route requested, but the legacy nsISocketTransportService
            // can't handle it.
            // Origin should be reachable on origin host name, so this should
            // not be a problem - but log it.
            LOG(("nsHalfOpenSocket this=%p using legacy nsISocketTransportService "
                 "means explicit route %s:%d will be ignored.\n", this,
                 ci->RoutedHost(), ci->RoutedPort()));
        }

        rv = sts->CreateTransport(socketTypes, typeCount,
                                  ci->GetOrigin(), ci->OriginPort(),
                                  ci->ProxyInfo(),
                                  getter_AddRefs(socketTransport));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t tmpFlags = 0;
    if (mCaps & NS_HTTP_REFRESH_DNS)
        tmpFlags = nsISocketTransport::BYPASS_CACHE;

    if (mCaps & NS_HTTP_LOAD_ANONYMOUS)
        tmpFlags |= nsISocketTransport::ANONYMOUS_CONNECT;

    if (ci->GetPrivate())
        tmpFlags |= nsISocketTransport::NO_PERMANENT_STORAGE;

    // For backup connections, we disable IPv6. That's because some users have
    // broken IPv6 connectivity (leading to very long timeouts), and disabling
    // IPv6 on the backup connection gives them a much better user experience
    // with dual-stack hosts, though they still pay the 250ms delay for each new
    // connection. This strategy is also known as "happy eyeballs".
    if (mEnt->mPreferIPv6) {
        tmpFlags |= nsISocketTransport::DISABLE_IPV4;
    }
    else if (mEnt->mPreferIPv4 ||
             (isBackup && gHttpHandler->FastFallbackToIPv4())) {
        tmpFlags |= nsISocketTransport::DISABLE_IPV6;
    }

    if (!Allow1918()) {
        tmpFlags |= nsISocketTransport::DISABLE_RFC1918;
    }

    socketTransport->SetConnectionFlags(tmpFlags);

    nsAutoCString firstPartyDomain =
      NS_ConvertUTF16toUTF8(mEnt->mConnInfo->GetOriginAttributes().mFirstPartyDomain);
    if (!firstPartyDomain.IsEmpty()) {
        socketTransport->SetFirstPartyDomain(firstPartyDomain);
    }

    socketTransport->SetQoSBits(gHttpHandler->GetQoSBits());

    if (!ci->GetNetworkInterfaceId().IsEmpty()) {
        socketTransport->SetNetworkInterfaceId(ci->GetNetworkInterfaceId());
    }

    rv = socketTransport->SetEventSink(this, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = socketTransport->SetSecurityCallbacks(this);
    NS_ENSURE_SUCCESS(rv, rv);

    Telemetry::Accumulate(Telemetry::HTTP_CONNECTION_ENTRY_CACHE_HIT_1,
                          mEnt->mUsedForConnection);
    mEnt->mUsedForConnection = true;

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

    rv = (*outstream)->AsyncWait(this, 0, 0, nullptr);
    if (NS_SUCCEEDED(rv))
        gHttpHandler->ConnMgr()->StartedConnect();

    return rv;
}

nsresult
nsHttpConnectionMgr::nsHalfOpenSocket::SetupPrimaryStreams()
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    nsresult rv;

    mPrimarySynStarted = TimeStamp::Now();
    rv = SetupStreams(getter_AddRefs(mSocketTransport),
                      getter_AddRefs(mStreamIn),
                      getter_AddRefs(mStreamOut),
                      false);
    LOG(("nsHalfOpenSocket::SetupPrimaryStream [this=%p ent=%s rv=%x]",
         this, mEnt->mConnInfo->Origin(), rv));
    if (NS_FAILED(rv)) {
        if (mStreamOut)
            mStreamOut->AsyncWait(nullptr, 0, 0, nullptr);
        mStreamOut = nullptr;
        mStreamIn = nullptr;
        mSocketTransport = nullptr;
    }
    return rv;
}

nsresult
nsHttpConnectionMgr::nsHalfOpenSocket::SetupBackupStreams()
{
    MOZ_ASSERT(mTransaction);
    MOZ_ASSERT(!mTransaction->IsNullTransaction(),
               "null transactions dont have backup streams");

    mBackupSynStarted = TimeStamp::Now();
    nsresult rv = SetupStreams(getter_AddRefs(mBackupTransport),
                               getter_AddRefs(mBackupStreamIn),
                               getter_AddRefs(mBackupStreamOut),
                               true);
    LOG(("nsHalfOpenSocket::SetupBackupStream [this=%p ent=%s rv=%x]",
         this, mEnt->mConnInfo->Origin(), rv));
    if (NS_FAILED(rv)) {
        if (mBackupStreamOut)
            mBackupStreamOut->AsyncWait(nullptr, 0, 0, nullptr);
        mBackupStreamOut = nullptr;
        mBackupStreamIn = nullptr;
        mBackupTransport = nullptr;
    }
    return rv;
}

void
nsHttpConnectionMgr::nsHalfOpenSocket::SetupBackupTimer()
{
    uint16_t timeout = gHttpHandler->GetIdleSynTimeout();
    MOZ_ASSERT(!mSynTimer, "timer already initd");
    if (timeout && !mTransaction->IsDone() && !mTransaction->IsNullTransaction()) {
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
            LOG(("nsHalfOpenSocket::SetupBackupTimer() [this=%p]", this));
        }
    } else if (timeout) {
        LOG(("nsHalfOpenSocket::SetupBackupTimer() [this=%p], did not arm\n", this));
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
    mSynTimer = nullptr;
}

void
nsHttpConnectionMgr::nsHalfOpenSocket::Abandon()
{
    LOG(("nsHalfOpenSocket::Abandon [this=%p ent=%s] %p %p %p %p",
         this, mEnt->mConnInfo->Origin(),
         mSocketTransport.get(), mBackupTransport.get(),
         mStreamOut.get(), mBackupStreamOut.get()));

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    RefPtr<nsHalfOpenSocket> deleteProtector(this);

    // Tell socket (and backup socket) to forget the half open socket.
    if (mSocketTransport) {
        mSocketTransport->SetEventSink(nullptr, nullptr);
        mSocketTransport->SetSecurityCallbacks(nullptr);
        mSocketTransport = nullptr;
    }
    if (mBackupTransport) {
        mBackupTransport->SetEventSink(nullptr, nullptr);
        mBackupTransport->SetSecurityCallbacks(nullptr);
        mBackupTransport = nullptr;
    }

    // Tell output stream (and backup) to forget the half open socket.
    if (mStreamOut) {
        gHttpHandler->ConnMgr()->RecvdConnect();
        mStreamOut->AsyncWait(nullptr, 0, 0, nullptr);
        mStreamOut = nullptr;
    }
    if (mBackupStreamOut) {
        gHttpHandler->ConnMgr()->RecvdConnect();
        mBackupStreamOut->AsyncWait(nullptr, 0, 0, nullptr);
        mBackupStreamOut = nullptr;
    }

    // Lose references to input stream (and backup).
    mStreamIn = mBackupStreamIn = nullptr;

    // Stop the timer - we don't want any new backups.
    CancelBackupTimer();

    // Remove the half open from the connection entry.
    if (mEnt)
        mEnt->RemoveHalfOpen(this);
    mEnt = nullptr;
}

double
nsHttpConnectionMgr::nsHalfOpenSocket::Duration(TimeStamp epoch)
{
    if (mPrimarySynStarted.IsNull())
        return 0;

    return (epoch - mPrimarySynStarted).ToMilliseconds();
}


NS_IMETHODIMP // method for nsITimerCallback
nsHttpConnectionMgr::nsHalfOpenSocket::Notify(nsITimer *timer)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(timer == mSynTimer, "wrong timer");
    MOZ_ASSERT(mTransaction && !mTransaction->IsNullTransaction(),
               "null transactions dont have backup streams");

    SetupBackupStreams();

    mSynTimer = nullptr;
    return NS_OK;
}

// method for nsIAsyncOutputStreamCallback
NS_IMETHODIMP
nsHttpConnectionMgr::
nsHalfOpenSocket::OnOutputStreamReady(nsIAsyncOutputStream *out)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(out == mStreamOut || out == mBackupStreamOut,
               "stream mismatch");
    LOG(("nsHalfOpenSocket::OnOutputStreamReady [this=%p ent=%s %s]\n",
         this, mEnt->mConnInfo->Origin(),
         out == mStreamOut ? "primary" : "backup"));
    int32_t index;
    nsresult rv;

    gHttpHandler->ConnMgr()->RecvdConnect();

    CancelBackupTimer();

    // assign the new socket to the http connection
    RefPtr<nsHttpConnection> conn = new nsHttpConnection();
    LOG(("nsHalfOpenSocket::OnOutputStreamReady "
         "Created new nshttpconnection %p\n", conn.get()));

    // Some capabilities are needed before a transaciton actually gets
    // scheduled (e.g. how to negotiate false start)
    conn->SetTransactionCaps(mTransaction->Caps());

    NetAddr peeraddr;
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks));
    if (out == mStreamOut) {
        TimeDuration rtt = TimeStamp::Now() - mPrimarySynStarted;
        rv = conn->Init(mEnt->mConnInfo,
                        gHttpHandler->ConnMgr()->mMaxRequestDelay,
                        mSocketTransport, mStreamIn, mStreamOut,
                        mPrimaryConnectedOK, callbacks,
                        PR_MillisecondsToInterval(
                          static_cast<uint32_t>(rtt.ToMilliseconds())));

        if (NS_SUCCEEDED(mSocketTransport->GetPeerAddr(&peeraddr)))
            mEnt->RecordIPFamilyPreference(peeraddr.raw.family);

        // The nsHttpConnection object now owns these streams and sockets
        mStreamOut = nullptr;
        mStreamIn = nullptr;
        mSocketTransport = nullptr;
    } else if (out == mBackupStreamOut) {
        MOZ_ASSERT(!mTransaction->IsNullTransaction(),
                   "null transactions dont have backup streams");
        TimeDuration rtt = TimeStamp::Now() - mBackupSynStarted;
        rv = conn->Init(mEnt->mConnInfo,
                        gHttpHandler->ConnMgr()->mMaxRequestDelay,
                        mBackupTransport, mBackupStreamIn, mBackupStreamOut,
                        mBackupConnectedOK, callbacks,
                        PR_MillisecondsToInterval(
                          static_cast<uint32_t>(rtt.ToMilliseconds())));

        if (NS_SUCCEEDED(mBackupTransport->GetPeerAddr(&peeraddr)))
            mEnt->RecordIPFamilyPreference(peeraddr.raw.family);

        // The nsHttpConnection object now owns these streams and sockets
        mBackupStreamOut = nullptr;
        mBackupStreamIn = nullptr;
        mBackupTransport = nullptr;
    } else {
        MOZ_ASSERT(false, "unexpected stream");
        rv = NS_ERROR_UNEXPECTED;
    }

    if (NS_FAILED(rv)) {
        LOG(("nsHalfOpenSocket::OnOutputStreamReady "
             "conn->init (%p) failed %x\n", conn.get(), rv));
        return rv;
    }

    // This half-open socket has created a connection.  This flag excludes it
    // from counter of actual connections used for checking limits.
    mHasConnected = true;

    // if this is still in the pending list, remove it and dispatch it
    index = mEnt->mPendingQ.IndexOf(mTransaction);
    if (index != -1) {
        MOZ_ASSERT(!mSpeculative,
                   "Speculative Half Open found mTransaction");
        RefPtr<nsHttpTransaction> temp = mEnt->mPendingQ[index];
        mEnt->mPendingQ.RemoveElementAt(index);
        gHttpHandler->ConnMgr()->AddActiveConn(conn, mEnt);
        rv = gHttpHandler->ConnMgr()->DispatchTransaction(mEnt, temp, conn);
    } else {
        // this transaction was dispatched off the pending q before all the
        // sockets established themselves.

        // After about 1 second allow for the possibility of restarting a
        // transaction due to server close. Keep at sub 1 second as that is the
        // minimum granularity we can expect a server to be timing out with.
        conn->SetIsReusedAfter(950);

        // if we are using ssl and no other transactions are waiting right now,
        // then form a null transaction to drive the SSL handshake to
        // completion. Afterwards the connection will be 100% ready for the next
        // transaction to use it. Make an exception for SSL tunneled HTTP proxy as the
        // NullHttpTransaction does not know how to drive Connect
        if (mEnt->mConnInfo->FirstHopSSL() && !mEnt->mPendingQ.Length() &&
            !mEnt->mConnInfo->UsingConnect()) {
            LOG(("nsHalfOpenSocket::OnOutputStreamReady null transaction will "
                 "be used to finish SSL handshake on conn %p\n", conn.get()));
            RefPtr<nsAHttpTransaction> trans;
            if (mTransaction->IsNullTransaction() && !mDispatchedMTransaction) {
                // null transactions cannot be put in the entry queue, so that
                // explains why it is not present.
                mDispatchedMTransaction = true;
                trans = mTransaction;
            } else {
                trans = new NullHttpTransaction(mEnt->mConnInfo,
                                                callbacks,
                                                mCaps & ~NS_HTTP_ALLOW_PIPELINING);
            }

            gHttpHandler->ConnMgr()->AddActiveConn(conn, mEnt);
            conn->Classify(nsAHttpTransaction::CLASS_SOLO);
            rv = gHttpHandler->ConnMgr()->
                DispatchAbstractTransaction(mEnt, trans, mCaps, conn, 0);
        } else {
            // otherwise just put this in the persistent connection pool
            LOG(("nsHalfOpenSocket::OnOutputStreamReady no transaction match "
                 "returning conn %p to pool\n", conn.get()));
            gHttpHandler->ConnMgr()->OnMsgReclaimConnection(0, conn);
        }
    }

    return rv;
}

// method for nsITransportEventSink
NS_IMETHODIMP
nsHttpConnectionMgr::nsHalfOpenSocket::OnTransportStatus(nsITransport *trans,
                                                         nsresult status,
                                                         int64_t progress,
                                                         int64_t progressMax)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mTransaction)
        mTransaction->OnTransportStatus(trans, status, progress);

    MOZ_ASSERT(trans == mSocketTransport || trans == mBackupTransport);
    if (status == NS_NET_STATUS_CONNECTED_TO) {
        if (trans == mSocketTransport) {
            mPrimaryConnectedOK = true;
        } else {
            mBackupConnectedOK = true;
        }
    }

    // The rest of this method only applies to the primary transport
    if (trans != mSocketTransport) {
        return NS_OK;
    }

    // if we are doing spdy coalescing and haven't recorded the ip address
    // for this entry before then make the hash key if our dns lookup
    // just completed. We can't do coalescing if using a proxy because the
    // ip addresses are not available to the client.

    if (status == NS_NET_STATUS_CONNECTING_TO &&
        gHttpHandler->IsSpdyEnabled() &&
        gHttpHandler->CoalesceSpdy() &&
        mEnt && mEnt->mConnInfo && mEnt->mConnInfo->EndToEndSSL() &&
        !mEnt->mConnInfo->UsingProxy() &&
        mEnt->mCoalescingKeys.IsEmpty()) {

        nsCOMPtr<nsIDNSRecord> dnsRecord(do_GetInterface(mSocketTransport));
        nsTArray<NetAddr> addressSet;
        nsresult rv = NS_ERROR_NOT_AVAILABLE;
        if (dnsRecord) {
            rv = dnsRecord->GetAddresses(addressSet);
        }

        if (NS_SUCCEEDED(rv) && !addressSet.IsEmpty()) {
            for (uint32_t i = 0; i < addressSet.Length(); ++i) {
                nsCString *newKey = mEnt->mCoalescingKeys.AppendElement(nsCString());
                newKey->SetCapacity(kIPv6CStrBufSize + 26);
                NetAddrToString(&addressSet[i], newKey->BeginWriting(), kIPv6CStrBufSize);
                newKey->SetLength(strlen(newKey->BeginReading()));
                if (mEnt->mConnInfo->GetAnonymous()) {
                    newKey->AppendLiteral("~A:");
                } else {
                    newKey->AppendLiteral("~.:");
                }
                newKey->AppendInt(mEnt->mConnInfo->OriginPort());
                LOG(("nsHttpConnectionMgr::nsHalfOpenSocket::OnTransportStatus "
                     "STATUS_CONNECTING_TO Established New Coalescing Key # %d for host "
                     "%s [%s]", i, mEnt->mConnInfo->Origin(), newKey->get()));
            }
            gHttpHandler->ConnMgr()->ProcessSpdyPendingQ(mEnt);
        }
    }

    switch (status) {
    case NS_NET_STATUS_CONNECTING_TO:
        // Passed DNS resolution, now trying to connect, start the backup timer
        // only prevent creating another backup transport.
        // We also check for mEnt presence to not instantiate the timer after
        // this half open socket has already been abandoned.  It may happen
        // when we get this notification right between main-thread calls to
        // nsHttpConnectionMgr::Shutdown and nsSocketTransportService::Shutdown
        // where the first abandons all half open socket instances and only
        // after that the second stops the socket thread.
        if (mEnt && !mBackupTransport && !mSynTimer)
            SetupBackupTimer();
        break;

    case NS_NET_STATUS_CONNECTED_TO:
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
        mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks));
        if (callbacks)
            return callbacks->GetInterface(iid, result);
    }
    return NS_ERROR_NO_INTERFACE;
}


already_AddRefed<nsHttpConnection>
ConnectionHandle::TakeHttpConnection()
{
    // return our connection object to the caller and clear it internally
    // do not drop our reference - the caller now owns it.
    MOZ_ASSERT(mConn);
    return mConn.forget();
}

uint32_t
ConnectionHandle::CancelPipeline(nsresult reason)
{
    // no pipeline to cancel
    return 0;
}

nsAHttpTransaction::Classifier
ConnectionHandle::Classification()
{
    if (mConn)
        return mConn->Classification();

    LOG(("ConnectionHandle::Classification this=%p "
         "has null mConn using CLASS_SOLO default", this));
    return nsAHttpTransaction::CLASS_SOLO;
}

// nsConnectionEntry

nsHttpConnectionMgr::
nsConnectionEntry::nsConnectionEntry(nsHttpConnectionInfo *ci)
    : mConnInfo(ci)
    , mPipelineState(PS_YELLOW)
    , mYellowGoodEvents(0)
    , mYellowBadEvents(0)
    , mYellowConnection(nullptr)
    , mGreenDepth(kPipelineOpen)
    , mPipeliningPenalty(0)
    , mUsingSpdy(false)
    , mInPreferredHash(false)
    , mPreferIPv4(false)
    , mPreferIPv6(false)
    , mUsedForConnection(false)
{
    MOZ_COUNT_CTOR(nsConnectionEntry);
    if (gHttpHandler->GetPipelineAggressive()) {
        mGreenDepth = kPipelineUnlimited;
        mPipelineState = PS_GREEN;
    }
    mInitialGreenDepth = mGreenDepth;
    memset(mPipeliningClassPenalty, 0, sizeof(int16_t) * nsAHttpTransaction::CLASS_MAX);
}

bool
nsHttpConnectionMgr::nsConnectionEntry::AvailableForDispatchNow()
{
    if (mIdleConns.Length() && mIdleConns[0]->CanReuse()) {
        return true;
    }

    return gHttpHandler->ConnMgr()->
        GetSpdyPreferredConn(this) ? true : false;
}

bool
nsHttpConnectionMgr::nsConnectionEntry::SupportsPipelining()
{
    return mPipelineState != nsHttpConnectionMgr::PS_RED;
}

nsHttpConnectionMgr::PipeliningState
nsHttpConnectionMgr::nsConnectionEntry::PipelineState()
{
    return mPipelineState;
}

void
nsHttpConnectionMgr::
nsConnectionEntry::OnPipelineFeedbackInfo(
    nsHttpConnectionMgr::PipelineFeedbackInfoType info,
    nsHttpConnection *conn,
    uint32_t data)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mPipelineState == PS_YELLOW) {
        if (info & kPipelineInfoTypeBad)
            mYellowBadEvents++;
        else if (info & (kPipelineInfoTypeNeutral | kPipelineInfoTypeGood))
            mYellowGoodEvents++;
    }

    if (mPipelineState == PS_GREEN && info == GoodCompletedOK) {
        int32_t depth = data;
        LOG(("Transaction completed at pipeline depth of %d. Host = %s\n",
             depth, mConnInfo->Origin()));

        if (depth >= 3)
            mGreenDepth = kPipelineUnlimited;
    }

    nsAHttpTransaction::Classifier classification;
    if (conn)
        classification = conn->Classification();
    else if (info == BadInsufficientFraming ||
             info == BadUnexpectedLarge)
        classification = (nsAHttpTransaction::Classifier) data;
    else
        classification = nsAHttpTransaction::CLASS_SOLO;

    if (gHttpHandler->GetPipelineAggressive() &&
        info & kPipelineInfoTypeBad &&
        info != BadExplicitClose &&
        info != RedVersionTooLow &&
        info != RedBannedServer &&
        info != RedCorruptedContent &&
        info != BadInsufficientFraming) {
        LOG(("minor negative feedback ignored "
             "because of pipeline aggressive mode"));
    }
    else if (info & kPipelineInfoTypeBad) {
        if ((info & kPipelineInfoTypeRed) && (mPipelineState != PS_RED)) {
            LOG(("transition to red from %d. Host = %s.\n",
                 mPipelineState, mConnInfo->Origin()));
            mPipelineState = PS_RED;
            mPipeliningPenalty = 0;
        }

        if (mLastCreditTime.IsNull())
            mLastCreditTime = TimeStamp::Now();

        // Red* events impact the host globally via mPipeliningPenalty, while
        // Bad* events impact the per class penalty.

        // The individual penalties should be < 16bit-signed-maxint - 25000
        // (approx 7500). Penalties are paid-off either when something promising
        // happens (a successful transaction, or promising headers) or when
        // time goes by at a rate of 1 penalty point every 16 seconds.

        switch (info) {
        case RedVersionTooLow:
            mPipeliningPenalty += 1000;
            break;
        case RedBannedServer:
            mPipeliningPenalty += 7000;
            break;
        case RedCorruptedContent:
            mPipeliningPenalty += 7000;
            break;
        case RedCanceledPipeline:
            mPipeliningPenalty += 60;
            break;
        case BadExplicitClose:
            mPipeliningClassPenalty[classification] += 250;
            break;
        case BadSlowReadMinor:
            mPipeliningClassPenalty[classification] += 5;
            break;
        case BadSlowReadMajor:
            mPipeliningClassPenalty[classification] += 25;
            break;
        case BadInsufficientFraming:
            mPipeliningClassPenalty[classification] += 7000;
            break;
        case BadUnexpectedLarge:
            mPipeliningClassPenalty[classification] += 120;
            break;

        default:
            MOZ_ASSERT(false, "Unknown Bad/Red Pipeline Feedback Event");
        }

        const int16_t kPenalty = 25000;
        mPipeliningPenalty = std::min(mPipeliningPenalty, kPenalty);
        mPipeliningClassPenalty[classification] =
          std::min(mPipeliningClassPenalty[classification], kPenalty);

        LOG(("Assessing red penalty to %s class %d for event %d. "
             "Penalty now %d, throttle[%d] = %d\n", mConnInfo->Origin(),
             classification, info, mPipeliningPenalty, classification,
             mPipeliningClassPenalty[classification]));
    }
    else {
        // hand out credits for neutral and good events such as
        // "headers look ok" events

        mPipeliningPenalty = std::max(mPipeliningPenalty - 1, 0);
        mPipeliningClassPenalty[classification] = std::max(mPipeliningClassPenalty[classification] - 1, 0);
    }

    if (mPipelineState == PS_RED && !mPipeliningPenalty)
    {
        LOG(("transition %s to yellow\n", mConnInfo->Origin()));
        mPipelineState = PS_YELLOW;
        mYellowConnection = nullptr;
    }
}

void
nsHttpConnectionMgr::
nsConnectionEntry::SetYellowConnection(nsHttpConnection *conn)
{
    MOZ_ASSERT(!mYellowConnection && mPipelineState == PS_YELLOW,
               "yellow connection already set or state is not yellow");
    mYellowConnection = conn;
    mYellowGoodEvents = mYellowBadEvents = 0;
}

void
nsHttpConnectionMgr::
nsConnectionEntry::OnYellowComplete()
{
    if (mPipelineState == PS_YELLOW) {
        if (mYellowGoodEvents && !mYellowBadEvents) {
            LOG(("transition %s to green\n", mConnInfo->Origin()));
            mPipelineState = PS_GREEN;
            mGreenDepth = mInitialGreenDepth;
        }
        else {
            // The purpose of the yellow state is to witness at least
            // one successful pipelined transaction without seeing any
            // kind of negative feedback before opening the flood gates.
            // If we haven't confirmed that, then transfer back to red.
            LOG(("transition %s to red from yellow return\n",
                 mConnInfo->Origin()));
            mPipelineState = PS_RED;
        }
    }

    mYellowConnection = nullptr;
}

void
nsHttpConnectionMgr::
nsConnectionEntry::CreditPenalty()
{
    if (mLastCreditTime.IsNull())
        return;

    // Decrease penalty values by 1 for every 16 seconds
    // (i.e 3.7 per minute, or 1000 every 4h20m)

    TimeStamp now = TimeStamp::Now();
    TimeDuration elapsedTime = now - mLastCreditTime;
    uint32_t creditsEarned =
        static_cast<uint32_t>(elapsedTime.ToSeconds()) >> 4;

    bool failed = false;
    if (creditsEarned > 0) {
        mPipeliningPenalty =
            std::max(int32_t(mPipeliningPenalty - creditsEarned), 0);
        if (mPipeliningPenalty > 0)
            failed = true;

        for (int32_t i = 0; i < nsAHttpTransaction::CLASS_MAX; ++i) {
            mPipeliningClassPenalty[i]  =
                std::max(int32_t(mPipeliningClassPenalty[i] - creditsEarned), 0);
            failed = failed || (mPipeliningClassPenalty[i] > 0);
        }

        // update last credit mark to reflect elapsed time
        mLastCreditTime += TimeDuration::FromSeconds(creditsEarned << 4);
    }
    else {
        failed = true;                         /* just assume this */
    }

    // If we are no longer red then clear the credit counter - you only
    // get credits for time spent in the red state
    if (!failed)
        mLastCreditTime = TimeStamp();    /* reset to null timestamp */

    if (mPipelineState == PS_RED && !mPipeliningPenalty)
    {
        LOG(("transition %s to yellow based on time credit\n",
             mConnInfo->Origin()));
        mPipelineState = PS_YELLOW;
        mYellowConnection = nullptr;
    }
}

uint32_t
nsHttpConnectionMgr::
nsConnectionEntry::MaxPipelineDepth(nsAHttpTransaction::Classifier aClass)
{
    // Still subject to configuration limit no matter return value

    if ((mPipelineState == PS_RED) || (mPipeliningClassPenalty[aClass] > 0))
        return 0;

    if (mPipelineState == PS_YELLOW)
        return kPipelineRestricted;

    return mGreenDepth;
}

bool
nsHttpConnectionMgr::GetConnectionData(nsTArray<HttpRetParams> *aArg)
{
    for (auto iter = mCT.Iter(); !iter.Done(); iter.Next()) {
        nsAutoPtr<nsConnectionEntry>& ent = iter.Data();

        if (ent->mConnInfo->GetPrivate()) {
            continue;
        }

        HttpRetParams data;
        data.host = ent->mConnInfo->Origin();
        data.port = ent->mConnInfo->OriginPort();
        for (uint32_t i = 0; i < ent->mActiveConns.Length(); i++) {
            HttpConnInfo info;
            info.ttl = ent->mActiveConns[i]->TimeToLive();
            info.rtt = ent->mActiveConns[i]->Rtt();
            if (ent->mActiveConns[i]->UsingSpdy()) {
                info.SetHTTP2ProtocolVersion(
                    ent->mActiveConns[i]->GetSpdyVersion());
            } else {
                info.SetHTTP1ProtocolVersion(
                    ent->mActiveConns[i]->GetLastHttpResponseVersion());
            }
            data.active.AppendElement(info);
        }
        for (uint32_t i = 0; i < ent->mIdleConns.Length(); i++) {
            HttpConnInfo info;
            info.ttl = ent->mIdleConns[i]->TimeToLive();
            info.rtt = ent->mIdleConns[i]->Rtt();
            info.SetHTTP1ProtocolVersion(
                ent->mIdleConns[i]->GetLastHttpResponseVersion());
            data.idle.AppendElement(info);
        }
        for (uint32_t i = 0; i < ent->mHalfOpens.Length(); i++) {
            HalfOpenSockets hSocket;
            hSocket.speculative = ent->mHalfOpens[i]->IsSpeculative();
            data.halfOpens.AppendElement(hSocket);
        }
        data.spdy = ent->mUsingSpdy;
        data.ssl = ent->mConnInfo->EndToEndSSL();
        aArg->AppendElement(data);
    }

    return true;
}

void
nsHttpConnectionMgr::ResetIPFamilyPreference(nsHttpConnectionInfo *ci)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    nsConnectionEntry *ent = LookupConnectionEntry(ci, nullptr, nullptr);
    if (ent)
        ent->ResetIPFamilyPreference();
}

uint32_t
nsHttpConnectionMgr::
nsConnectionEntry::UnconnectedHalfOpens()
{
    uint32_t unconnectedHalfOpens = 0;
    for (uint32_t i = 0; i < mHalfOpens.Length(); ++i) {
        if (!mHalfOpens[i]->HasConnected())
            ++unconnectedHalfOpens;
    }
    return unconnectedHalfOpens;
}

void
nsHttpConnectionMgr::
nsConnectionEntry::RemoveHalfOpen(nsHalfOpenSocket *halfOpen)
{
    // A failure to create the transport object at all
    // will result in it not being present in the halfopen table. That's expected.
    if (mHalfOpens.RemoveElement(halfOpen)) {

        if (halfOpen->IsSpeculative()) {
            Telemetry::AutoCounter<Telemetry::HTTPCONNMGR_UNUSED_SPECULATIVE_CONN> unusedSpeculativeConn;
            ++unusedSpeculativeConn;

            if (halfOpen->IsFromPredictor()) {
                Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS_UNUSED> totalPreconnectsUnused;
                ++totalPreconnectsUnused;
            }
        }

        MOZ_ASSERT(gHttpHandler->ConnMgr()->mNumHalfOpenConns);
        if (gHttpHandler->ConnMgr()->mNumHalfOpenConns) { // just in case
            gHttpHandler->ConnMgr()->mNumHalfOpenConns--;
        }
    }

    if (!UnconnectedHalfOpens())
        // perhaps this reverted RestrictConnections()
        // use the PostEvent version of processpendingq to avoid
        // altering the pending q vector from an arbitrary stack
        gHttpHandler->ConnMgr()->ProcessPendingQ(mConnInfo);
}

void
nsHttpConnectionMgr::
nsConnectionEntry::RecordIPFamilyPreference(uint16_t family)
{
  if (family == PR_AF_INET && !mPreferIPv6)
    mPreferIPv4 = true;

  if (family == PR_AF_INET6 && !mPreferIPv4)
    mPreferIPv6 = true;
}

void
nsHttpConnectionMgr::
nsConnectionEntry::ResetIPFamilyPreference()
{
  mPreferIPv4 = false;
  mPreferIPv6 = false;
}

void
nsHttpConnectionMgr::MoveToWildCardConnEntry(nsHttpConnectionInfo *specificCI,
                                             nsHttpConnectionInfo *wildCardCI,
                                             nsHttpConnection *proxyConn)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(specificCI->UsingHttpsProxy());

    LOG(("nsHttpConnectionMgr::MakeConnEntryWildCard conn %p has requested to "
         "change CI from %s to %s\n", proxyConn, specificCI->HashKey().get(),
         wildCardCI->HashKey().get()));

    nsConnectionEntry *ent = LookupConnectionEntry(specificCI, proxyConn, nullptr);
    LOG(("nsHttpConnectionMgr::MakeConnEntryWildCard conn %p using ent %p (spdy %d)\n",
         proxyConn, ent, ent ? ent->mUsingSpdy : 0));

    if (!ent || !ent->mUsingSpdy) {
        return;
    }

    nsConnectionEntry *wcEnt = GetOrCreateConnectionEntry(wildCardCI, true);
    if (wcEnt == ent) {
        // nothing to do!
        return;
    }
    wcEnt->mUsingSpdy = true;

    LOG(("nsHttpConnectionMgr::MakeConnEntryWildCard ent %p "
         "idle=%d active=%d half=%d pending=%d\n", ent,
         ent->mIdleConns.Length(), ent->mActiveConns.Length(),
         ent->mHalfOpens.Length(), ent->mPendingQ.Length()));

    LOG(("nsHttpConnectionMgr::MakeConnEntryWildCard wc-ent %p "
         "idle=%d active=%d half=%d pending=%d\n", wcEnt,
         wcEnt->mIdleConns.Length(), wcEnt->mActiveConns.Length(),
         wcEnt->mHalfOpens.Length(), wcEnt->mPendingQ.Length()));

    int32_t count = ent->mActiveConns.Length();
    RefPtr<nsHttpConnection> deleteProtector(proxyConn);
    for (int32_t i = 0; i < count; ++i) {
        if (ent->mActiveConns[i] == proxyConn) {
            ent->mActiveConns.RemoveElementAt(i);
            wcEnt->mActiveConns.InsertElementAt(0, proxyConn);
            return;
        }
    }

    count = ent->mIdleConns.Length();
    for (int32_t i = 0; i < count; ++i) {
        if (ent->mIdleConns[i] == proxyConn) {
            ent->mIdleConns.RemoveElementAt(i);
            wcEnt->mIdleConns.InsertElementAt(0, proxyConn);
            return;
        }
    }
}

} // namespace net
} // namespace mozilla
