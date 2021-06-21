/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheIndexIterator__h__
#define CacheIndexIterator__h__

#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "mozilla/SHA1.h"
#include "mozilla/StaticMutex.h"

namespace mozilla {
namespace net {

class CacheIndex;
class CacheIndexRecordWrapper;

class CacheIndexIterator {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheIndexIterator)

  CacheIndexIterator(CacheIndex* aIndex, bool aAddNew);

 protected:
  virtual ~CacheIndexIterator();

 public:
  // Returns a hash of a next entry. If there is no entry NS_ERROR_NOT_AVAILABLE
  // is returned and the iterator is closed. Other error is returned when the
  // iterator is closed for other reason, e.g. shutdown.
  nsresult GetNextHash(SHA1Sum::Hash* aHash);

  // Closes the iterator. This means the iterator is removed from the list of
  // iterators in CacheIndex.
  nsresult Close();

 protected:
  friend class CacheIndex;

  nsresult CloseInternal(nsresult aStatus);

  bool ShouldBeNewAdded() { return mAddNew; }
  virtual void AddRecord(CacheIndexRecordWrapper* aRecord,
                         const StaticMutexAutoLock& aProofOfLock);
  bool RemoveRecord(CacheIndexRecordWrapper* aRecord,
                    const StaticMutexAutoLock& aProofOfLock);
  bool ReplaceRecord(CacheIndexRecordWrapper* aOldRecord,
                     CacheIndexRecordWrapper* aNewRecord,
                     const StaticMutexAutoLock& aProofOfLock);
  void ClearRecords(const StaticMutexAutoLock& aProofOfLock);

  nsresult mStatus;
  RefPtr<CacheIndex> mIndex;
  nsTArray<RefPtr<CacheIndexRecordWrapper>> mRecords;
  bool mAddNew;
};

}  // namespace net
}  // namespace mozilla

#endif
