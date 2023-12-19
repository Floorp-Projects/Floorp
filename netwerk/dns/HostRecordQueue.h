/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HostRecordQueue_h__
#define HostRecordQueue_h__

#include <functional>
#include "mozilla/Mutex.h"
#include "nsHostRecord.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace net {

class HostRecordQueue final {
 public:
  HostRecordQueue() = default;
  ~HostRecordQueue() = default;
  HostRecordQueue(const HostRecordQueue& aCopy) = delete;
  HostRecordQueue& operator=(const HostRecordQueue& aCopy) = delete;

  uint32_t PendingCount() const { return mPendingCount; }
  uint32_t EvictionQSize() const { return mEvictionQSize; }

  // Insert the record to mHighQ or mMediumQ or mLowQ based on the record's
  // priority.
  void InsertRecord(nsHostRecord* aRec, nsIDNSService::DNSFlags aFlags,
                    const MutexAutoLock& aProofOfLock);
  // Insert the record to mEvictionQ. In theory, this function should be called
  // when the record is not in any queue.
  void AddToEvictionQ(
      nsHostRecord* aRec, uint32_t aMaxCacheEntries,
      nsRefPtrHashtable<nsGenericHashKey<nsHostKey>, nsHostRecord>& aDB,
      const MutexAutoLock& aProofOfLock);
  // Called for removing the record from mEvictionQ. When this function is
  // called, the record should be either in mEvictionQ or not in any queue.
  void MaybeRenewHostRecord(nsHostRecord* aRec,
                            const MutexAutoLock& aProofOfLock);
  // Called for clearing mEvictionQ.
  void FlushEvictionQ(
      nsRefPtrHashtable<nsGenericHashKey<nsHostKey>, nsHostRecord>& aDB,
      const MutexAutoLock& aProofOfLock);
  // Remove the record from the queue that contains it.
  void MaybeRemoveFromQ(nsHostRecord* aRec, const MutexAutoLock& aProofOfLock);
  // When the record's priority changes, move the record between pending queues.
  void MoveToAnotherPendingQ(nsHostRecord* aRec, nsIDNSService::DNSFlags aFlags,
                             const MutexAutoLock& aProofOfLock);
  // Returning the first record from one of the pending queue. When |aHighQOnly|
  // is true, returning the record from mHighQ only. When false, return the
  // record from mMediumQ or mLowQ.
  already_AddRefed<nsHostRecord> Dequeue(bool aHighQOnly,
                                         const MutexAutoLock& aProofOfLock);
  // Clear all queues and is called only during shutdown. |aCallback| is invoked
  // when a record is removed from a queue.
  void ClearAll(const std::function<void(nsHostRecord*)>& aCallback,
                const MutexAutoLock& aProofOfLock);

 private:
  Atomic<uint32_t> mPendingCount{0};
  Atomic<uint32_t> mEvictionQSize{0};
  LinkedList<RefPtr<nsHostRecord>> mHighQ;
  LinkedList<RefPtr<nsHostRecord>> mMediumQ;
  LinkedList<RefPtr<nsHostRecord>> mLowQ;
  LinkedList<RefPtr<nsHostRecord>> mEvictionQ;
};

}  // namespace net
}  // namespace mozilla

#endif  // HostRecordQueue_h__
