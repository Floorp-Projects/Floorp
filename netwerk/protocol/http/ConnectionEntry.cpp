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

void ConnectionEntry::RemoveHalfOpen(HalfOpenSocket* halfOpen) {
  // A failure to create the transport object at all
  // will result in it not being present in the halfopen table. That's expected.
  if (mHalfOpens.RemoveElement(halfOpen)) {
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

void ConnectionEntry::CancelAllTransactions(nsresult reason) {
  mPendingQ.CancelAllTransactions(reason);
}

}  // namespace net
}  // namespace mozilla
