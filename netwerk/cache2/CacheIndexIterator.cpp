/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheIndexIterator.h"
#include "CacheIndex.h"
#include "nsString.h"
#include "mozilla/DebugOnly.h"


namespace mozilla {
namespace net {

CacheIndexIterator::CacheIndexIterator(CacheIndex *aIndex, bool aAddNew)
  : mStatus(NS_OK)
  , mIndex(aIndex)
  , mAddNew(aAddNew)
{
  LOG(("CacheIndexIterator::CacheIndexIterator() [this=%p]", this));
}

CacheIndexIterator::~CacheIndexIterator()
{
  LOG(("CacheIndexIterator::~CacheIndexIterator() [this=%p]", this));

  Close();
}

nsresult
CacheIndexIterator::GetNextHash(SHA1Sum::Hash *aHash)
{
  LOG(("CacheIndexIterator::GetNextHash() [this=%p]", this));

  CacheIndexAutoLock lock(mIndex);

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  if (!mRecords.Length()) {
    CloseInternal(NS_ERROR_NOT_AVAILABLE);
    return mStatus;
  }

  memcpy(aHash, mRecords[mRecords.Length() - 1]->mHash, sizeof(SHA1Sum::Hash));
  mRecords.RemoveElementAt(mRecords.Length() - 1);

  return NS_OK;
}

nsresult
CacheIndexIterator::Close()
{
  LOG(("CacheIndexIterator::Close() [this=%p]", this));

  CacheIndexAutoLock lock(mIndex);

  return CloseInternal(NS_ERROR_NOT_AVAILABLE);
}

nsresult
CacheIndexIterator::CloseInternal(nsresult aStatus)
{
  LOG(("CacheIndexIterator::CloseInternal() [this=%p, status=0x%08x]", this,
       aStatus));

  // Make sure status will be a failure
  MOZ_ASSERT(NS_FAILED(aStatus));
  if (NS_SUCCEEDED(aStatus)) {
    aStatus = NS_ERROR_UNEXPECTED;
  }

  if (NS_FAILED(mStatus)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  DebugOnly<bool> removed = mIndex->mIterators.RemoveElement(this);
  MOZ_ASSERT(removed);
  mStatus = aStatus;

  return NS_OK;
}

void
CacheIndexIterator::AddRecord(CacheIndexRecord *aRecord)
{
  LOG(("CacheIndexIterator::AddRecord() [this=%p, record=%p]", this, aRecord));

  mRecords.AppendElement(aRecord);
}

void
CacheIndexIterator::AddRecords(const nsTArray<CacheIndexRecord *> &aRecords)
{
  LOG(("CacheIndexIterator::AddRecords() [this=%p]", this));

  mRecords.AppendElements(aRecords);
}

bool
CacheIndexIterator::RemoveRecord(CacheIndexRecord *aRecord)
{
  LOG(("CacheIndexIterator::RemoveRecord() [this=%p, record=%p]", this,
       aRecord));

  return mRecords.RemoveElement(aRecord);
}

bool
CacheIndexIterator::ReplaceRecord(CacheIndexRecord *aOldRecord,
                                  CacheIndexRecord *aNewRecord)
{
  LOG(("CacheIndexIterator::ReplaceRecord() [this=%p, oldRecord=%p, "
       "newRecord=%p]", this, aOldRecord, aNewRecord));

  if (RemoveRecord(aOldRecord)) {
    AddRecord(aNewRecord);
    return true;
  }

  return false;
}

} // net
} // mozilla
