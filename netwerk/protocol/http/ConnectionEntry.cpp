/* vim:set ts=4 sw=2 sts=2 et cin: */
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

#include "ConnectionEntry.h"
#include "nsQueryObject.h"
#include "mozilla/ChaosMode.h"

namespace mozilla {
namespace net {

// ConnectionEntry
ConnectionEntry::~ConnectionEntry() {
  LOG(("ConnectionEntry::~ConnectionEntry this=%p", this));

  MOZ_ASSERT(!mIdleConns.Length());
  MOZ_ASSERT(!mActiveConns.Length());
  MOZ_ASSERT(!mHalfOpens.Length());
  MOZ_ASSERT(!PendingQueueLength());
  MOZ_ASSERT(!UrgentStartQueueLength());
  MOZ_ASSERT(!mHalfOpenFastOpenBackups.Length());
  MOZ_ASSERT(!mDoNotDestroy);
}

ConnectionEntry::ConnectionEntry(nsHttpConnectionInfo* ci)
    : mConnInfo(ci),
      mUsingSpdy(false),
      mCanUseSpdy(true),
      mPreferIPv4(false),
      mPreferIPv6(false),
      mUsedForConnection(false),
      mDoNotDestroy(false) {
  if (mConnInfo->FirstHopSSL() && !mConnInfo->IsHttp3()) {
    mUseFastOpen = gHttpHandler->UseFastOpen();
  } else {
    // Only allow the TCP fast open on a secure connection.
    mUseFastOpen = false;
  }

  LOG(("ConnectionEntry::ConnectionEntry this=%p key=%s", this,
       ci->HashKey().get()));
}

bool ConnectionEntry::AvailableForDispatchNow() {
  if (mIdleConns.Length() && mIdleConns[0]->CanReuse()) {
    return true;
  }

  return gHttpHandler->ConnMgr()->GetH2orH3ActiveConn(this, false) ? true
                                                                   : false;
}

uint32_t ConnectionEntry::UnconnectedHalfOpens() const {
  uint32_t unconnectedHalfOpens = 0;
  for (uint32_t i = 0; i < mHalfOpens.Length(); ++i) {
    if (!mHalfOpens[i]->HasConnected()) {
      ++unconnectedHalfOpens;
    }
  }
  return unconnectedHalfOpens;
}

bool ConnectionEntry::RemoveHalfOpen(HalfOpenSocket* halfOpen) {
  bool isPrimary = false;
  // A failure to create the transport object at all
  // will result in it not being present in the halfopen table. That's expected.
  if (mHalfOpens.RemoveElement(halfOpen)) {
    isPrimary = true;
    if (halfOpen->IsSpeculative()) {
      Telemetry::AutoCounter<Telemetry::HTTPCONNMGR_UNUSED_SPECULATIVE_CONN>
          unusedSpeculativeConn;
      ++unusedSpeculativeConn;

      if (halfOpen->IsFromPredictor()) {
        Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS_UNUSED>
            totalPreconnectsUnused;
        ++totalPreconnectsUnused;
      }
    }

    gHttpHandler->ConnMgr()->DecreaseNumHalfOpenConns();

  } else {
    mHalfOpenFastOpenBackups.RemoveElement(halfOpen);
  }

  if (!UnconnectedHalfOpens()) {
    // perhaps this reverted RestrictConnections()
    // use the PostEvent version of processpendingq to avoid
    // altering the pending q vector from an arbitrary stack
    nsresult rv = gHttpHandler->ConnMgr()->ProcessPendingQ(mConnInfo);
    if (NS_FAILED(rv)) {
      LOG(
          ("ConnectionEntry::RemoveHalfOpen\n"
           "    failed to process pending queue\n"));
    }
  }

  return isPrimary;
}

void ConnectionEntry::DisallowHttp2() {
  mCanUseSpdy = false;

  // If we have any spdy connections, we want to go ahead and close them when
  // they're done so we can free up some connections.
  for (uint32_t i = 0; i < mActiveConns.Length(); ++i) {
    if (mActiveConns[i]->UsingSpdy()) {
      mActiveConns[i]->DontReuse();
    }
  }
  for (uint32_t i = 0; i < mIdleConns.Length(); ++i) {
    if (mIdleConns[i]->UsingSpdy()) {
      mIdleConns[i]->DontReuse();
    }
  }

  // Can't coalesce if we're not using spdy
  mCoalescingKeys.Clear();
}

void ConnectionEntry::DontReuseHttp3Conn() {
  MOZ_ASSERT(mConnInfo->IsHttp3());

  // If we have any spdy connections, we want to go ahead and close them when
  // they're done so we can free up some connections.
  for (uint32_t i = 0; i < mActiveConns.Length(); ++i) {
    mActiveConns[i]->DontReuse();
  }

  // Can't coalesce if we're not using http3
  mCoalescingKeys.Clear();
}

void ConnectionEntry::RecordIPFamilyPreference(uint16_t family) {
  LOG(("ConnectionEntry::RecordIPFamilyPreference %p, af=%u", this, family));

  if (family == PR_AF_INET && !mPreferIPv6) {
    mPreferIPv4 = true;
  }

  if (family == PR_AF_INET6 && !mPreferIPv4) {
    mPreferIPv6 = true;
  }

  LOG(("  %p prefer ipv4=%d, ipv6=%d", this, (bool)mPreferIPv4,
       (bool)mPreferIPv6));
}

void ConnectionEntry::ResetIPFamilyPreference() {
  LOG(("ConnectionEntry::ResetIPFamilyPreference %p", this));

  mPreferIPv4 = false;
  mPreferIPv6 = false;
}

bool net::ConnectionEntry::PreferenceKnown() const {
  return (bool)mPreferIPv4 || (bool)mPreferIPv6;
}

size_t ConnectionEntry::PendingQueueLength() const {
  return mPendingQ.PendingQueueLength();
}

size_t ConnectionEntry::PendingQueueLengthForWindow(uint64_t windowId) const {
  return mPendingQ.PendingQueueLengthForWindow(windowId);
}

void ConnectionEntry::AppendPendingUrgentStartQ(
    nsTArray<RefPtr<PendingTransactionInfo>>& result) {
  mPendingQ.AppendPendingUrgentStartQ(result);
}

void ConnectionEntry::AppendPendingQForFocusedWindow(
    uint64_t windowId, nsTArray<RefPtr<PendingTransactionInfo>>& result,
    uint32_t maxCount) {
  mPendingQ.AppendPendingQForFocusedWindow(windowId, result, maxCount);
  LOG(
      ("ConnectionEntry::AppendPendingQForFocusedWindow [ci=%s], "
       "pendingQ count=%zu for focused window (id=%" PRIu64 ")\n",
       mConnInfo->HashKey().get(), result.Length(), windowId));
}

void ConnectionEntry::AppendPendingQForNonFocusedWindows(
    uint64_t windowId, nsTArray<RefPtr<PendingTransactionInfo>>& result,
    uint32_t maxCount) {
  mPendingQ.AppendPendingQForNonFocusedWindows(windowId, result, maxCount);
  LOG(
      ("ConnectionEntry::AppendPendingQForNonFocusedWindows [ci=%s], "
       "pendingQ count=%zu for non focused window\n",
       mConnInfo->HashKey().get(), result.Length()));
}

void ConnectionEntry::RemoveEmptyPendingQ() { mPendingQ.RemoveEmptyPendingQ(); }

void ConnectionEntry::InsertTransactionSorted(
    nsTArray<RefPtr<PendingTransactionInfo>>& pendingQ,
    PendingTransactionInfo* pendingTransInfo,
    bool aInsertAsFirstForTheSamePriority /*= false*/) {
  mPendingQ.InsertTransactionSorted(pendingQ, pendingTransInfo,
                                    aInsertAsFirstForTheSamePriority);
}

void ConnectionEntry::ReschedTransaction(nsHttpTransaction* aTrans) {
  mPendingQ.ReschedTransaction(aTrans);
}

void ConnectionEntry::InsertTransaction(
    PendingTransactionInfo* pendingTransInfo,
    bool aInsertAsFirstForTheSamePriority /* = false */) {
  mPendingQ.InsertTransaction(pendingTransInfo,
                              aInsertAsFirstForTheSamePriority);
}

nsTArray<RefPtr<PendingTransactionInfo>>*
ConnectionEntry::GetTransactionPendingQHelper(nsAHttpTransaction* trans) {
  return mPendingQ.GetTransactionPendingQHelper(trans);
}

bool ConnectionEntry::RestrictConnections() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (AvailableForDispatchNow()) {
    // this might be a h2/spdy connection in this connection entry that
    // is able to be immediately muxxed, or it might be one that
    // was found in the same state through a coalescing hash
    LOG(
        ("ConnectionEntry::RestrictConnections %p %s restricted due to "
         "active >=h2\n",
         this, mConnInfo->HashKey().get()));
    return true;
  }

  // If this host is trying to negotiate a SPDY session right now,
  // don't create any new ssl connections until the result of the
  // negotiation is known.

  bool doRestrict = mConnInfo->FirstHopSSL() && gHttpHandler->IsSpdyEnabled() &&
                    mUsingSpdy &&
                    (mHalfOpens.Length() || mActiveConns.Length());

  // If there are no restrictions, we are done
  if (!doRestrict) {
    return false;
  }

  // If the restriction is based on a tcp handshake in progress
  // let that connect and then see if it was SPDY or not
  if (UnconnectedHalfOpens()) {
    return true;
  }

  // There is a concern that a host is using a mix of HTTP/1 and SPDY.
  // In that case we don't want to restrict connections just because
  // there is a single active HTTP/1 session in use.
  if (mUsingSpdy && mActiveConns.Length()) {
    bool confirmedRestrict = false;
    for (uint32_t index = 0; index < mActiveConns.Length(); ++index) {
      HttpConnectionBase* conn = mActiveConns[index];
      RefPtr<nsHttpConnection> connTCP = do_QueryObject(conn);
      if ((connTCP && !connTCP->ReportedNPN()) || conn->CanDirectlyActivate()) {
        confirmedRestrict = true;
        break;
      }
    }
    doRestrict = confirmedRestrict;
    if (!confirmedRestrict) {
      LOG(
          ("nsHttpConnectionMgr spdy connection restriction to "
           "%s bypassed.\n",
           mConnInfo->Origin()));
    }
  }
  return doRestrict;
}

uint32_t ConnectionEntry::TotalActiveConnections() const {
  // Add in the in-progress tcp connections, we will assume they are
  // keepalive enabled.
  // Exclude half-open's that has already created a usable connection.
  // This prevents the limit being stuck on ipv6 connections that
  // eventually time out after typical 21 seconds of no ACK+SYN reply.
  return mActiveConns.Length() + UnconnectedHalfOpens();
}

size_t ConnectionEntry::UrgentStartQueueLength() {
  return mPendingQ.UrgentStartQueueLength();
}

void ConnectionEntry::PrintPendingQ() { mPendingQ.PrintPendingQ(); }

void ConnectionEntry::Compact() {
  mIdleConns.Compact();
  mActiveConns.Compact();
  mPendingQ.Compact();
}

void ConnectionEntry::RemoveFromIdleConnectionsIndex(size_t inx) {
  mIdleConns.RemoveElementAt(inx);
  gHttpHandler->ConnMgr()->DecrementNumIdleConns();
}

bool ConnectionEntry::RemoveFromIdleConnections(nsHttpConnection* conn) {
  if (!mIdleConns.RemoveElement(conn)) {
    return false;
  }

  gHttpHandler->ConnMgr()->DecrementNumIdleConns();
  return true;
}

void ConnectionEntry::CancelAllTransactions(nsresult reason) {
  mPendingQ.CancelAllTransactions(reason);
}

nsresult ConnectionEntry::CloseIdleConnection(nsHttpConnection* conn) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  RefPtr<nsHttpConnection> deleteProtector(conn);
  if (!RemoveFromIdleConnections(conn)) {
    return NS_ERROR_UNEXPECTED;
  }

  // The connection is closed immediately no need to call EndIdleMonitoring.
  conn->Close(NS_ERROR_ABORT);
  return NS_OK;
}

void ConnectionEntry::CloseIdleConnections() {
  while (mIdleConns.Length()) {
    RefPtr<nsHttpConnection> conn(mIdleConns[0]);
    RemoveFromIdleConnectionsIndex(0);
    // The connection is closed immediately no need to call EndIdleMonitoring.
    conn->Close(NS_ERROR_ABORT);
  }
}

void ConnectionEntry::CloseIdleConnections(uint32_t maxToClose) {
  uint32_t closed = 0;
  while (mIdleConns.Length() && (closed < maxToClose)) {
    RefPtr<nsHttpConnection> conn(mIdleConns[0]);
    RemoveFromIdleConnectionsIndex(0);
    // The connection is closed immediately no need to call EndIdleMonitoring.
    conn->Close(NS_ERROR_ABORT);
    closed++;
  }
}

nsresult ConnectionEntry::RemoveIdleConnection(nsHttpConnection* conn) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (!RemoveFromIdleConnections(conn)) {
    return NS_ERROR_UNEXPECTED;
  }

  conn->EndIdleMonitoring();
  return NS_OK;
}

bool ConnectionEntry::IsInIdleConnections(nsHttpConnection* conn) {
  return mIdleConns.Contains(conn);
}

already_AddRefed<nsHttpConnection> ConnectionEntry::GetIdleConnection(
    bool respectUrgency, bool urgentTrans, bool* onlyUrgent) {
  RefPtr<nsHttpConnection> conn;
  size_t index = 0;
  while (!conn && (mIdleConns.Length() > index)) {
    conn = mIdleConns[index];

    if (!conn->CanReuse()) {
      RemoveFromIdleConnectionsIndex(index);
      LOG(("   dropping stale connection: [conn=%p]\n", conn.get()));
      conn->Close(NS_ERROR_ABORT);
      conn = nullptr;
      continue;
    }

    // non-urgent transactions can only be dispatched on non-urgent
    // started or used connections.
    if (respectUrgency && conn->IsUrgentStartPreferred() && !urgentTrans) {
      LOG(("  skipping urgent: [conn=%p]", conn.get()));
      conn = nullptr;
      ++index;
      continue;
    }

    *onlyUrgent = false;

    RemoveFromIdleConnectionsIndex(index);
    conn->EndIdleMonitoring();
    LOG(("   reusing connection: [conn=%p]\n", conn.get()));
  }

  return conn.forget();
}

nsresult ConnectionEntry::RemoveActiveConnection(HttpConnectionBase* conn) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (!mActiveConns.RemoveElement(conn)) {
    return NS_ERROR_UNEXPECTED;
  }
  gHttpHandler->ConnMgr()->DecrementActiveConnCount(conn);

  return NS_OK;
}

void ConnectionEntry::ClosePersistentConnections() {
  LOG(("ConnectionEntry::ClosePersistentConnections [ci=%s]\n",
       mConnInfo->HashKey().get()));
  CloseIdleConnections();

  int32_t activeCount = mActiveConns.Length();
  for (int32_t i = 0; i < activeCount; i++) {
    mActiveConns[i]->DontReuse();
  }

  for (int32_t index = mHalfOpenFastOpenBackups.Length() - 1; index >= 0;
       --index) {
    RefPtr<HalfOpenSocket> half = mHalfOpenFastOpenBackups[index];
    half->CancelFastOpenConnection();
  }
}

uint32_t ConnectionEntry::PruneDeadConnections() {
  uint32_t timeToNextExpire = UINT32_MAX;

  for (int32_t len = mIdleConns.Length(); len > 0; --len) {
    int32_t idx = len - 1;
    RefPtr<nsHttpConnection> conn(mIdleConns[idx]);
    if (!conn->CanReuse()) {
      RemoveFromIdleConnectionsIndex(idx);
      // The connection is closed immediately no need to call
      // EndIdleMonitoring.
      conn->Close(NS_ERROR_ABORT);
    } else {
      timeToNextExpire = std::min(timeToNextExpire, conn->TimeToLive());
    }
  }

  if (mUsingSpdy) {
    for (uint32_t i = 0; i < mActiveConns.Length(); ++i) {
      RefPtr<nsHttpConnection> connTCP = do_QueryObject(mActiveConns[i]);
      // Http3 has its own timers, it is not using this one.
      if (connTCP && connTCP->UsingSpdy()) {
        if (!connTCP->CanReuse()) {
          // Marking it don't-reuse will create an active
          // tear down if the spdy session is idle.
          connTCP->DontReuse();
        } else {
          timeToNextExpire = std::min(timeToNextExpire, connTCP->TimeToLive());
        }
      }
    }
  }

  return timeToNextExpire;
}

void ConnectionEntry::VerifyTraffic() {
  if (!mConnInfo->IsHttp3()) {
    // Iterate over all active connections and check them.
    for (uint32_t index = 0; index < mActiveConns.Length(); ++index) {
      RefPtr<nsHttpConnection> conn = do_QueryObject(mActiveConns[index]);
      if (conn) {
        conn->CheckForTraffic(true);
      }
    }
    // Iterate the idle connections and unmark them for traffic checks.
    for (uint32_t index = 0; index < mIdleConns.Length(); ++index) {
      RefPtr<nsHttpConnection> conn = do_QueryObject(mIdleConns[index]);
      if (conn) {
        conn->CheckForTraffic(false);
      }
    }
  }
}

void ConnectionEntry::InsertIntoIdleConnections_internal(
    nsHttpConnection* conn) {
  uint32_t idx;
  for (idx = 0; idx < mIdleConns.Length(); idx++) {
    nsHttpConnection* idleConn = mIdleConns[idx];
    if (idleConn->MaxBytesRead() < conn->MaxBytesRead()) {
      break;
    }
  }

  mIdleConns.InsertElementAt(idx, conn);
}

void ConnectionEntry::InsertIntoIdleConnections(nsHttpConnection* conn) {
  InsertIntoIdleConnections_internal(conn);
  gHttpHandler->ConnMgr()->NewIdleConnectionAdded(conn->TimeToLive());
  conn->BeginIdleMonitoring();
}

bool ConnectionEntry::IsInActiveConns(HttpConnectionBase* conn) {
  return mActiveConns.Contains(conn);
}

void ConnectionEntry::InsertIntoActiveConns(HttpConnectionBase* conn) {
  mActiveConns.AppendElement(conn);
  gHttpHandler->ConnMgr()->IncrementActiveConnCount();
}

void ConnectionEntry::MakeAllDontReuseExcept(HttpConnectionBase* conn) {
  for (uint32_t index = 0; index < mActiveConns.Length(); ++index) {
    HttpConnectionBase* otherConn = mActiveConns[index];
    if (otherConn != conn) {
      LOG(
          ("ConnectionEntry::MakeAllDontReuseExcept shutting down old "
           "connection (%p) "
           "because new "
           "spdy connection (%p) takes precedence\n",
           otherConn, conn));
      otherConn->DontReuse();
    }
  }

  // Cancel any other pending connections - their associated transactions
  // are in the pending queue and will be dispatched onto this new connection
  for (int32_t index = HalfOpensLength() - 1; index >= 0; --index) {
    RefPtr<HalfOpenSocket> half = mHalfOpens[index];
    LOG((
        "ConnectionEntry::MakeAllDontReuseExcept forcing halfopen abandon %p\n",
        half.get()));
    mHalfOpens[index]->Abandon();
  }

  for (int32_t index = mHalfOpenFastOpenBackups.Length() - 1; index >= 0;
       --index) {
    LOG(
        ("ConnectionEntry::MakeAllDontReuseExcept shutting down connection in "
         "fast "
         "open state (%p) because new spdy connection (%p) takes "
         "precedence\n",
         mHalfOpenFastOpenBackups[index].get(), conn));
    RefPtr<HalfOpenSocket> half = mHalfOpenFastOpenBackups[index];
    half->CancelFastOpenConnection();
  }
}

bool ConnectionEntry::FindConnToClaim(
    PendingTransactionInfo* pendingTransInfo) {
  nsHttpTransaction* trans = pendingTransInfo->Transaction();

  uint32_t halfOpenLength = HalfOpensLength();
  for (uint32_t i = 0; i < halfOpenLength; i++) {
    auto halfOpen = mHalfOpens[i];
    if (halfOpen->AcceptsTransaction(trans) &&
        pendingTransInfo->TryClaimingHalfOpen(halfOpen)) {
      // We've found a speculative connection or a connection that
      // is free to be used in the half open list.
      // A free to be used connection is a connection that was
      // open for a concrete transaction, but that trunsaction
      // ended up using another connection.
      LOG(
          ("ConnectionEntry::FindConnToClaim [ci = %s]\n"
           "Found a speculative or a free-to-use half open connection\n",
           mConnInfo->HashKey().get()));

      // return OK because we have essentially opened a new connection
      // by converting a speculative half-open to general use
      return true;
    }
  }

  // consider null transactions that are being used to drive the ssl handshake
  // if the transaction creating this connection can re-use persistent
  // connections
  if (trans->Caps() & NS_HTTP_ALLOW_KEEPALIVE) {
    uint32_t activeLength = mActiveConns.Length();
    for (uint32_t i = 0; i < activeLength; i++) {
      if (pendingTransInfo->TryClaimingActiveConn(mActiveConns[i])) {
        LOG(
            ("ConnectionEntry::FindConnectingSocket [ci = %s] "
             "Claiming a null transaction for later use\n",
             mConnInfo->HashKey().get()));
        return true;
      }
    }
  }
  return false;
}

bool ConnectionEntry::MakeFirstActiveSpdyConnDontReuse() {
  if (!mUsingSpdy) {
    return false;
  }

  for (uint32_t index = 0; index < mActiveConns.Length(); ++index) {
    HttpConnectionBase* conn = mActiveConns[index];
    if (conn->UsingSpdy() && conn->CanReuse()) {
      conn->DontReuse();
      return true;
    }
  }
  return false;
}

// Return an active h2 or h3 connection
// that can be directly activated or null.
HttpConnectionBase* ConnectionEntry::GetH2orH3ActiveConn(bool aNoHttp3) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  HttpConnectionBase* experienced = nullptr;
  HttpConnectionBase* noExperience = nullptr;
  uint32_t activeLen = mActiveConns.Length();

  // activeLen should generally be 1.. this is a setup race being resolved
  // take a conn who can activate and is experienced
  for (uint32_t index = 0; index < activeLen; ++index) {
    HttpConnectionBase* tmp = mActiveConns[index];
    if (tmp->CanDirectlyActivate()) {
      if (tmp->IsExperienced()) {
        experienced = tmp;
        break;
      }
      noExperience = tmp;  // keep looking for a better option
    }
  }

  // if that worked, cleanup anything else and exit
  if (experienced) {
    for (uint32_t index = 0; index < activeLen; ++index) {
      HttpConnectionBase* tmp = mActiveConns[index];
      // in the case where there is a functional h2 session, drop the others
      if (tmp != experienced) {
        tmp->DontReuse();
      }
    }
    for (int32_t index = mHalfOpenFastOpenBackups.Length() - 1; index >= 0;
         --index) {
      LOG(
          ("GetH2orH3ActiveConn() shutting down connection in fast "
           "open state (%p) because we have an experienced spdy "
           "connection (%p).\n",
           mHalfOpenFastOpenBackups[index].get(), experienced));
      RefPtr<HalfOpenSocket> half = mHalfOpenFastOpenBackups[index];
      half->CancelFastOpenConnection();
    }

    LOG(
        ("GetH2orH3ActiveConn() request for ent %p %s "
         "found an active experienced connection %p in native connection "
         "entry\n",
         this, mConnInfo->HashKey().get(), experienced));
    return experienced;
  }

  if (noExperience) {
    LOG(
        ("GetH2orH3ActiveConn() request for ent %p %s "
         "found an active but inexperienced connection %p in native connection "
         "entry\n",
         this, mConnInfo->HashKey().get(), noExperience));
    return noExperience;
  }

  return nullptr;
}

void ConnectionEntry::CloseActiveConnections() {
  while (mActiveConns.Length()) {
    RefPtr<HttpConnectionBase> conn(mActiveConns[0]);
    mActiveConns.RemoveElementAt(0);
    gHttpHandler->ConnMgr()->DecrementActiveConnCount(conn);

    // Since HttpConnectionBase::Close doesn't break the bond with
    // the connection's transaction, we must explicitely tell it
    // to close its transaction and not just self.
    conn->CloseTransaction(conn->Transaction(), NS_ERROR_ABORT, true);
  }
}

void ConnectionEntry::CloseAllActiveConnsWithNullTransactcion(
    nsresult aCloseCode) {
  for (uint32_t index = 0; index < mActiveConns.Length(); ++index) {
    RefPtr<HttpConnectionBase> activeConn = mActiveConns[index];
    nsAHttpTransaction* liveTransaction = activeConn->Transaction();
    if (liveTransaction && liveTransaction->IsNullTransaction()) {
      LOG(
          ("ConnectionEntry::CloseAllActiveConnsWithNullTransactcion "
           "also canceling Null Transaction %p on conn %p\n",
           liveTransaction, activeConn.get()));
      activeConn->CloseTransaction(liveTransaction, aCloseCode);
    }
  }
}

void ConnectionEntry::PruneNoTraffic() {
  LOG(("  pruning no traffic [ci=%s]\n", mConnInfo->HashKey().get()));
  if (mConnInfo->IsHttp3()) {
    return;
  }

  uint32_t numConns = mActiveConns.Length();
  if (numConns) {
    // Walk the list backwards to allow us to remove entries easily.
    for (int index = numConns - 1; index >= 0; index--) {
      RefPtr<nsHttpConnection> conn = do_QueryObject(mActiveConns[index]);
      if (conn && conn->NoTraffic()) {
        mActiveConns.RemoveElementAt(index);
        gHttpHandler->ConnMgr()->DecrementActiveConnCount(conn);
        conn->Close(NS_ERROR_ABORT);
        LOG(
            ("  closed active connection due to no traffic "
             "[conn=%p]\n",
             conn.get()));
      }
    }
  }
}

uint32_t ConnectionEntry::TimeoutTick() {
  uint32_t timeoutTickNext = 3600;  // 1hr

  if (mConnInfo->IsHttp3()) {
    return timeoutTickNext;
  }

  LOG(
      ("ConnectionEntry::TimeoutTick() this=%p host=%s "
       "idle=%zu active=%zu"
       " half-len=%zu pending=%zu"
       " urgentStart pending=%zu\n",
       this, mConnInfo->Origin(), IdleConnectionsLength(), ActiveConnsLength(),
       mHalfOpens.Length(), PendingQueueLength(), UrgentStartQueueLength()));

  // First call the tick handler for each active connection.
  PRIntervalTime tickTime = PR_IntervalNow();
  for (uint32_t index = 0; index < mActiveConns.Length(); ++index) {
    RefPtr<nsHttpConnection> conn = do_QueryObject(mActiveConns[index]);
    if (conn) {
      uint32_t connNextTimeout = conn->ReadTimeoutTick(tickTime);
      timeoutTickNext = std::min(timeoutTickNext, connNextTimeout);
    }
  }

  // Now check for any stalled half open sockets.
  if (mHalfOpens.Length()) {
    TimeStamp currentTime = TimeStamp::Now();
    double maxConnectTime_ms = gHttpHandler->ConnectTimeout();

    for (uint32_t index = mHalfOpens.Length(); index > 0;) {
      index--;

      HalfOpenSocket* half = mHalfOpens[index];
      double delta = half->Duration(currentTime);
      // If the socket has timed out, close it so the waiting
      // transaction will get the proper signal.
      if (delta > maxConnectTime_ms) {
        LOG(("Force timeout of half open to %s after %.2fms.\n",
             mConnInfo->HashKey().get(), delta));
        if (half->SocketTransport()) {
          half->SocketTransport()->Close(NS_ERROR_NET_TIMEOUT);
        }
        if (half->BackupTransport()) {
          half->BackupTransport()->Close(NS_ERROR_NET_TIMEOUT);
        }
      }

      // If this half open hangs around for 5 seconds after we've
      // closed() it then just abandon the socket.
      if (delta > maxConnectTime_ms + 5000) {
        LOG(("Abandon half open to %s after %.2fms.\n",
             mConnInfo->HashKey().get(), delta));
        half->Abandon();
      }
    }
  }
  if (mHalfOpens.Length()) {
    timeoutTickNext = 1;
  }

  return timeoutTickNext;
}

void ConnectionEntry::MoveConnection(HttpConnectionBase* proxyConn,
                                     ConnectionEntry* otherEnt) {
  // To avoid changing mNumActiveConns/mNumIdleConns counter use internal
  // functions.
  RefPtr<HttpConnectionBase> deleteProtector(proxyConn);
  if (mActiveConns.RemoveElement(proxyConn)) {
    otherEnt->mActiveConns.AppendElement(proxyConn);
    return;
  }

  RefPtr<nsHttpConnection> proxyConnTCP = do_QueryObject(proxyConn);
  if (proxyConnTCP) {
    if (mIdleConns.RemoveElement(proxyConnTCP)) {
      otherEnt->InsertIntoIdleConnections_internal(proxyConnTCP);
      return;
    }
  }
}

void ConnectionEntry::InsertIntoHalfOpens(HalfOpenSocket* sock) {
  mHalfOpens.AppendElement(sock);
  gHttpHandler->ConnMgr()->IncreaseNumHalfOpenConns();
}

void ConnectionEntry::InsertIntoHalfOpenFastOpenBackups(HalfOpenSocket* sock) {
  mHalfOpenFastOpenBackups.AppendElement(sock);
}

void ConnectionEntry::CloseAllHalfOpens() {
  for (int32_t i = int32_t(HalfOpensLength()) - 1; i >= 0; i--) {
    mHalfOpens[i]->Abandon();
  }
}

void ConnectionEntry::RemoveHalfOpenFastOpenBackups(HalfOpenSocket* sock) {
  mHalfOpenFastOpenBackups.RemoveElement(sock);
}

bool ConnectionEntry::IsInHalfOpens(HalfOpenSocket* sock) {
  return mHalfOpens.Contains(sock);
}

HttpRetParams ConnectionEntry::GetConnectionData() {
  HttpRetParams data;
  data.host = mConnInfo->Origin();
  data.port = mConnInfo->OriginPort();
  for (uint32_t i = 0; i < mActiveConns.Length(); i++) {
    HttpConnInfo info;
    RefPtr<nsHttpConnection> connTCP = do_QueryObject(mActiveConns[i]);
    if (connTCP) {
      info.ttl = connTCP->TimeToLive();
    } else {
      info.ttl = 0;
    }
    info.rtt = mActiveConns[i]->Rtt();
    info.SetHTTPProtocolVersion(mActiveConns[i]->Version());
    data.active.AppendElement(info);
  }
  for (uint32_t i = 0; i < mIdleConns.Length(); i++) {
    HttpConnInfo info;
    info.ttl = mIdleConns[i]->TimeToLive();
    info.rtt = mIdleConns[i]->Rtt();
    info.SetHTTPProtocolVersion(mIdleConns[i]->Version());
    data.idle.AppendElement(info);
  }
  for (uint32_t i = 0; i < mHalfOpens.Length(); i++) {
    HalfOpenSockets hSocket;
    hSocket.speculative = mHalfOpens[i]->IsSpeculative();
    data.halfOpens.AppendElement(hSocket);
  }
  if (mConnInfo->IsHttp3()) {
    data.httpVersion = "HTTP/3"_ns;
  } else if (mUsingSpdy) {
    data.httpVersion = "HTTP/2"_ns;
  } else {
    data.httpVersion = "HTTP <= 1.1"_ns;
  }
  data.ssl = mConnInfo->EndToEndSSL();
  return data;
}

void ConnectionEntry::LogConnections() {
  if (!mConnInfo->IsHttp3()) {
    LOG(("active urgent conns ["));
    for (HttpConnectionBase* conn : mActiveConns) {
      RefPtr<nsHttpConnection> connTCP = do_QueryObject(conn);
      MOZ_ASSERT(connTCP);
      if (connTCP->IsUrgentStartPreferred()) {
        LOG(("  %p", conn));
      }
    }
    LOG(("] active regular conns ["));
    for (HttpConnectionBase* conn : mActiveConns) {
      RefPtr<nsHttpConnection> connTCP = do_QueryObject(conn);
      MOZ_ASSERT(connTCP);
      if (!connTCP->IsUrgentStartPreferred()) {
        LOG(("  %p", conn));
      }
    }

    LOG(("] idle urgent conns ["));
    for (nsHttpConnection* conn : mIdleConns) {
      if (conn->IsUrgentStartPreferred()) {
        LOG(("  %p", conn));
      }
    }
    LOG(("] idle regular conns ["));
    for (nsHttpConnection* conn : mIdleConns) {
      if (!conn->IsUrgentStartPreferred()) {
        LOG(("  %p", conn));
      }
    }
  } else {
    LOG(("active conns ["));
    for (HttpConnectionBase* conn : mActiveConns) {
      LOG(("  %p", conn));
    }
    MOZ_ASSERT(mIdleConns.Length() == 0);
  }
  LOG(("]"));
}

}  // namespace net
}  // namespace mozilla
