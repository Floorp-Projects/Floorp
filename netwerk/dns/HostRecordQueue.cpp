/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HostRecordQueue.h"
#include "mozilla/Telemetry.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace net {

void HostRecordQueue::InsertRecord(nsHostRecord* aRec,
                                   nsIDNSService::DNSFlags aFlags,
                                   const MutexAutoLock& aProofOfLock) {
  if (aRec->isInList()) {
    MOZ_DIAGNOSTIC_ASSERT(!mEvictionQ.contains(aRec),
                          "Already in eviction queue");
    MOZ_DIAGNOSTIC_ASSERT(!mHighQ.contains(aRec), "Already in high queue");
    MOZ_DIAGNOSTIC_ASSERT(!mMediumQ.contains(aRec), "Already in med queue");
    MOZ_DIAGNOSTIC_ASSERT(!mLowQ.contains(aRec), "Already in low queue");
    MOZ_DIAGNOSTIC_ASSERT(false, "Already on some other queue?");
  }

  switch (AddrHostRecord::GetPriority(aFlags)) {
    case AddrHostRecord::DNS_PRIORITY_HIGH:
      mHighQ.insertBack(aRec);
      break;

    case AddrHostRecord::DNS_PRIORITY_MEDIUM:
      mMediumQ.insertBack(aRec);
      break;

    case AddrHostRecord::DNS_PRIORITY_LOW:
      mLowQ.insertBack(aRec);
      break;
  }
  mPendingCount++;
}

void HostRecordQueue::AddToEvictionQ(
    nsHostRecord* aRec, uint32_t aMaxCacheEntries,
    nsRefPtrHashtable<nsGenericHashKey<nsHostKey>, nsHostRecord>& aDB,
    const MutexAutoLock& aProofOfLock) {
  if (aRec->isInList()) {
    bool inEvictionQ = mEvictionQ.contains(aRec);
    MOZ_DIAGNOSTIC_ASSERT(!inEvictionQ, "Already in eviction queue");
    bool inHighQ = mHighQ.contains(aRec);
    MOZ_DIAGNOSTIC_ASSERT(!inHighQ, "Already in high queue");
    bool inMediumQ = mMediumQ.contains(aRec);
    MOZ_DIAGNOSTIC_ASSERT(!inMediumQ, "Already in med queue");
    bool inLowQ = mLowQ.contains(aRec);
    MOZ_DIAGNOSTIC_ASSERT(!inLowQ, "Already in low queue");
    MOZ_DIAGNOSTIC_ASSERT(false, "Already on some other queue?");

    // Bug 1678117 - it's not clear why this can happen, but let's fix it
    // for release users.
    aRec->remove();
    if (inEvictionQ) {
      MOZ_DIAGNOSTIC_ASSERT(mEvictionQSize > 0);
      mEvictionQSize--;
    } else if (inHighQ || inMediumQ || inLowQ) {
      MOZ_DIAGNOSTIC_ASSERT(mPendingCount > 0);
      mPendingCount--;
    }
  }
  mEvictionQ.insertBack(aRec);
  if (mEvictionQSize < aMaxCacheEntries) {
    mEvictionQSize++;
  } else {
    // remove first element on mEvictionQ
    RefPtr<nsHostRecord> head = mEvictionQ.popFirst();
    aDB.Remove(*static_cast<nsHostKey*>(head.get()));

    if (!head->negative) {
      // record the age of the entry upon eviction.
      TimeDuration age = TimeStamp::NowLoRes() - head->mValidStart;
      if (aRec->IsAddrRecord()) {
        Telemetry::Accumulate(Telemetry::DNS_CLEANUP_AGE,
                              static_cast<uint32_t>(age.ToSeconds() / 60));
      } else {
        Telemetry::Accumulate(Telemetry::DNS_BY_TYPE_CLEANUP_AGE,
                              static_cast<uint32_t>(age.ToSeconds() / 60));
      }
      if (head->CheckExpiration(TimeStamp::Now()) !=
          nsHostRecord::EXP_EXPIRED) {
        if (aRec->IsAddrRecord()) {
          Telemetry::Accumulate(Telemetry::DNS_PREMATURE_EVICTION,
                                static_cast<uint32_t>(age.ToSeconds() / 60));
        } else {
          Telemetry::Accumulate(Telemetry::DNS_BY_TYPE_PREMATURE_EVICTION,
                                static_cast<uint32_t>(age.ToSeconds() / 60));
        }
      }
    }
  }
}

void HostRecordQueue::MaybeRenewHostRecord(nsHostRecord* aRec,
                                           const MutexAutoLock& aProofOfLock) {
  if (!aRec->isInList()) {
    return;
  }

  bool inEvictionQ = mEvictionQ.contains(aRec);
  MOZ_DIAGNOSTIC_ASSERT(inEvictionQ, "Should be in eviction queue");
  bool inHighQ = mHighQ.contains(aRec);
  MOZ_DIAGNOSTIC_ASSERT(!inHighQ, "Already in high queue");
  bool inMediumQ = mMediumQ.contains(aRec);
  MOZ_DIAGNOSTIC_ASSERT(!inMediumQ, "Already in med queue");
  bool inLowQ = mLowQ.contains(aRec);
  MOZ_DIAGNOSTIC_ASSERT(!inLowQ, "Already in low queue");

  // we're already on the eviction queue. This is a renewal
  aRec->remove();
  if (inEvictionQ) {
    MOZ_DIAGNOSTIC_ASSERT(mEvictionQSize > 0);
    mEvictionQSize--;
  } else if (inHighQ || inMediumQ || inLowQ) {
    MOZ_DIAGNOSTIC_ASSERT(mPendingCount > 0);
    mPendingCount--;
  }
}

void HostRecordQueue::FlushEvictionQ(
    nsRefPtrHashtable<nsGenericHashKey<nsHostKey>, nsHostRecord>& aDB,
    const MutexAutoLock& aProofOfLock) {
  mEvictionQSize = 0;

  // Clear the evictionQ and remove all its corresponding entries from
  // the cache first
  if (!mEvictionQ.isEmpty()) {
    for (const RefPtr<nsHostRecord>& rec : mEvictionQ) {
      rec->Cancel();
      aDB.Remove(*static_cast<nsHostKey*>(rec));
    }
    mEvictionQ.clear();
  }
}

void HostRecordQueue::MaybeRemoveFromQ(nsHostRecord* aRec,
                                       const MutexAutoLock& aProofOfLock) {
  if (!aRec->isInList()) {
    return;
  }

  if (mHighQ.contains(aRec) || mMediumQ.contains(aRec) ||
      mLowQ.contains(aRec)) {
    mPendingCount--;
  } else if (mEvictionQ.contains(aRec)) {
    mEvictionQSize--;
  } else {
    MOZ_ASSERT(false, "record is in other queue");
  }

  aRec->remove();
}

void HostRecordQueue::MoveToAnotherPendingQ(nsHostRecord* aRec,
                                            nsIDNSService::DNSFlags aFlags,
                                            const MutexAutoLock& aProofOfLock) {
  if (!(mHighQ.contains(aRec) || mMediumQ.contains(aRec) ||
        mLowQ.contains(aRec))) {
    MOZ_ASSERT(false, "record is not in the pending queue");
    return;
  }

  aRec->remove();
  InsertRecord(aRec, aFlags, aProofOfLock);
}

already_AddRefed<nsHostRecord> HostRecordQueue::Dequeue(
    bool aHighQOnly, const MutexAutoLock& aProofOfLock) {
  RefPtr<nsHostRecord> rec;
  if (!mHighQ.isEmpty()) {
    rec = mHighQ.popFirst();
  } else if (!mMediumQ.isEmpty() && !aHighQOnly) {
    rec = mMediumQ.popFirst();
  } else if (!mLowQ.isEmpty() && !aHighQOnly) {
    rec = mLowQ.popFirst();
  }

  if (rec) {
    mPendingCount--;
  }

  return rec.forget();
}

void HostRecordQueue::ClearAll(
    const std::function<void(nsHostRecord*)>& aCallback,
    const MutexAutoLock& aProofOfLock) {
  mPendingCount = 0;

  auto clearPendingQ = [&](LinkedList<RefPtr<nsHostRecord>>& aPendingQ) {
    if (aPendingQ.isEmpty()) {
      return;
    }

    // loop through pending queue, erroring out pending lookups.
    for (const RefPtr<nsHostRecord>& rec : aPendingQ) {
      rec->Cancel();
      aCallback(rec);
    }
    aPendingQ.clear();
  };

  clearPendingQ(mHighQ);
  clearPendingQ(mMediumQ);
  clearPendingQ(mLowQ);

  mEvictionQSize = 0;
  if (!mEvictionQ.isEmpty()) {
    for (const RefPtr<nsHostRecord>& rec : mEvictionQ) {
      rec->Cancel();
    }
  }

  mEvictionQ.clear();
}

}  // namespace net
}  // namespace mozilla
