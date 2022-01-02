/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheIndexContextIterator.h"
#include "CacheIndex.h"
#include "nsString.h"

namespace mozilla::net {

CacheIndexContextIterator::CacheIndexContextIterator(CacheIndex* aIndex,
                                                     bool aAddNew,
                                                     nsILoadContextInfo* aInfo)
    : CacheIndexIterator(aIndex, aAddNew), mInfo(aInfo) {}

void CacheIndexContextIterator::AddRecord(
    CacheIndexRecordWrapper* aRecord, const StaticMutexAutoLock& aProofOfLock) {
  if (CacheIndexEntry::RecordMatchesLoadContextInfo(aRecord, mInfo)) {
    CacheIndexIterator::AddRecord(aRecord, aProofOfLock);
  }
}

}  // namespace mozilla::net
