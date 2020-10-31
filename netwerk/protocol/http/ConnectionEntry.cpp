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

namespace mozilla {
namespace net {

static uint64_t TabIdForQueuing(nsAHttpTransaction* transaction) {
  return gHttpHandler->ActiveTabPriority()
             ? transaction->TopLevelOuterContentWindowId()
             : 0;
}

// ConnectionEntry
ConnectionEntry::~ConnectionEntry() {
  LOG(("ConnectionEntry::~ConnectionEntry this=%p", this));

  MOZ_ASSERT(!mIdleConns.Length());
  MOZ_ASSERT(!mActiveConns.Length());
  MOZ_ASSERT(!mHalfOpens.Length());
  MOZ_ASSERT(!mUrgentStartQ.Length());
  MOZ_ASSERT(!PendingQLength());
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

uint32_t ConnectionEntry::UnconnectedHalfOpens() {
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

size_t ConnectionEntry::PendingQLength() const {
  size_t length = 0;
  for (auto it = mPendingTransactionTable.ConstIter(); !it.Done(); it.Next()) {
    length += it.UserData()->Length();
  }

  return length;
}

void ConnectionEntry::InsertTransaction(
    PendingTransactionInfo* info,
    bool aInsertAsFirstForTheSamePriority /*= false*/) {
  LOG(
      ("ConnectionEntry::InsertTransaction"
       " trans=%p, windowId=%" PRIu64 "\n",
       info->mTransaction.get(),
       info->mTransaction->TopLevelOuterContentWindowId()));

  uint64_t windowId = TabIdForQueuing(info->mTransaction);
  nsTArray<RefPtr<PendingTransactionInfo>>* infoArray;
  if (!mPendingTransactionTable.Get(windowId, &infoArray)) {
    infoArray = new nsTArray<RefPtr<PendingTransactionInfo>>();
    mPendingTransactionTable.Put(windowId, infoArray);
  }

  gHttpHandler->ConnMgr()->InsertTransactionSorted(
      *infoArray, info, aInsertAsFirstForTheSamePriority);
}

void ConnectionEntry::AppendPendingQForFocusedWindow(
    uint64_t windowId, nsTArray<RefPtr<PendingTransactionInfo>>& result,
    uint32_t maxCount) {
  nsTArray<RefPtr<PendingTransactionInfo>>* infoArray = nullptr;
  if (!mPendingTransactionTable.Get(windowId, &infoArray)) {
    result.Clear();
    return;
  }

  uint32_t countToAppend = maxCount;
  countToAppend = countToAppend > infoArray->Length() || countToAppend == 0
                      ? infoArray->Length()
                      : countToAppend;

  result.InsertElementsAt(result.Length(), infoArray->Elements(),
                          countToAppend);
  infoArray->RemoveElementsAt(0, countToAppend);

  LOG(
      ("ConnectionEntry::AppendPendingQForFocusedWindow [ci=%s], "
       "pendingQ count=%zu window.count=%zu for focused window (id=%" PRIu64
       ")\n",
       mConnInfo->HashKey().get(), result.Length(), infoArray->Length(),
       windowId));
}

void ConnectionEntry::AppendPendingQForNonFocusedWindows(
    uint64_t windowId, nsTArray<RefPtr<PendingTransactionInfo>>& result,
    uint32_t maxCount) {
  // XXX Adjust the order of transactions in a smarter manner.
  uint32_t totalCount = 0;
  for (auto it = mPendingTransactionTable.Iter(); !it.Done(); it.Next()) {
    if (windowId && it.Key() == windowId) {
      continue;
    }

    uint32_t count = 0;
    for (; count < it.UserData()->Length(); ++count) {
      if (maxCount && totalCount == maxCount) {
        break;
      }

      // Because elements in |result| could come from multiple penndingQ,
      // call |InsertTransactionSorted| to make sure the order is correct.
      gHttpHandler->ConnMgr()->InsertTransactionSorted(
          result, it.UserData()->ElementAt(count));
      ++totalCount;
    }
    it.UserData()->RemoveElementsAt(0, count);

    if (maxCount && totalCount == maxCount) {
      if (it.UserData()->Length()) {
        // There are still some pending transactions for background
        // tabs but we limit their dispatch.  This is considered as
        // an active tab optimization.
        nsHttp::NotifyActiveTabLoadOptimization();
      }
      break;
    }
  }

  LOG(
      ("ConnectionEntry::AppendPendingQForNonFocusedWindows [ci=%s], "
       "pendingQ count=%zu for non focused window\n",
       mConnInfo->HashKey().get(), result.Length()));
}

void ConnectionEntry::RemoveEmptyPendingQ() {
  for (auto it = mPendingTransactionTable.Iter(); !it.Done(); it.Next()) {
    if (it.UserData()->IsEmpty()) {
      it.Remove();
    }
  }
}

}  // namespace net
}  // namespace mozilla
