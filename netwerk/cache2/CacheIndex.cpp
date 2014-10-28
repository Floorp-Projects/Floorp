/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheIndex.h"

#include "CacheLog.h"
#include "CacheFileIOManager.h"
#include "CacheFileMetadata.h"
#include "CacheIndexIterator.h"
#include "CacheIndexContextIterator.h"
#include "nsThreadUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsISizeOf.h"
#include "nsPrintfCString.h"
#include "mozilla/DebugOnly.h"
#include "prinrval.h"
#include "nsIFile.h"
#include "nsITimer.h"
#include "mozilla/AutoRestore.h"
#include <algorithm>


#define kMinUnwrittenChanges   300
#define kMinDumpInterval       20000 // in milliseconds
#define kMaxBufSize            16384
#define kIndexVersion          0x00000001
#define kUpdateIndexStartDelay 50000 // in milliseconds

const char kIndexName[]     = "index";
const char kTempIndexName[] = "index.tmp";
const char kJournalName[]   = "index.log";

namespace mozilla {
namespace net {

/**
 * This helper class is responsible for keeping CacheIndex::mIndexStats,
 * CacheIndex::mFrecencyArray and CacheIndex::mExpirationArray up to date.
 */
class CacheIndexEntryAutoManage
{
public:
  CacheIndexEntryAutoManage(const SHA1Sum::Hash *aHash, CacheIndex *aIndex)
    : mIndex(aIndex)
    , mOldRecord(nullptr)
    , mOldFrecency(0)
    , mOldExpirationTime(nsICacheEntry::NO_EXPIRATION_TIME)
    , mDoNotSearchInIndex(false)
    , mDoNotSearchInUpdates(false)
  {
    mIndex->AssertOwnsLock();

    mHash = aHash;
    const CacheIndexEntry *entry = FindEntry();
    mIndex->mIndexStats.BeforeChange(entry);
    if (entry && entry->IsInitialized() && !entry->IsRemoved()) {
      mOldRecord = entry->mRec;
      mOldFrecency = entry->mRec->mFrecency;
      mOldExpirationTime = entry->mRec->mExpirationTime;
    }
  }

  ~CacheIndexEntryAutoManage()
  {
    mIndex->AssertOwnsLock();

    const CacheIndexEntry *entry = FindEntry();
    mIndex->mIndexStats.AfterChange(entry);
    if (!entry || !entry->IsInitialized() || entry->IsRemoved()) {
      entry = nullptr;
    }

    if (entry && !mOldRecord) {
      mIndex->InsertRecordToFrecencyArray(entry->mRec);
      mIndex->InsertRecordToExpirationArray(entry->mRec);
      mIndex->AddRecordToIterators(entry->mRec);
    } else if (!entry && mOldRecord) {
      mIndex->RemoveRecordFromFrecencyArray(mOldRecord);
      mIndex->RemoveRecordFromExpirationArray(mOldRecord);
      mIndex->RemoveRecordFromIterators(mOldRecord);
    } else if (entry && mOldRecord) {
      bool replaceFrecency = false;
      bool replaceExpiration = false;

      if (entry->mRec != mOldRecord) {
        // record has a different address, we have to replace it
        replaceFrecency = replaceExpiration = true;
        mIndex->ReplaceRecordInIterators(mOldRecord, entry->mRec);
      } else {
        if (entry->mRec->mFrecency == 0 &&
            entry->mRec->mExpirationTime == nsICacheEntry::NO_EXPIRATION_TIME) {
          // This is a special case when we want to make sure that the entry is
          // placed at the end of the lists even when the values didn't change.
          replaceFrecency = replaceExpiration = true;
        } else {
          if (entry->mRec->mFrecency != mOldFrecency) {
            replaceFrecency = true;
          }
          if (entry->mRec->mExpirationTime != mOldExpirationTime) {
            replaceExpiration = true;
          }
        }
      }

      if (replaceFrecency) {
        mIndex->RemoveRecordFromFrecencyArray(mOldRecord);
        mIndex->InsertRecordToFrecencyArray(entry->mRec);
      }
      if (replaceExpiration) {
        mIndex->RemoveRecordFromExpirationArray(mOldRecord);
        mIndex->InsertRecordToExpirationArray(entry->mRec);
      }
    } else {
      // both entries were removed or not initialized, do nothing
    }
  }

  // We cannot rely on nsTHashtable::GetEntry() in case we are enumerating the
  // entries and returning PL_DHASH_REMOVE. Destructor is called before the
  // entry is removed. Caller must call one of following methods to skip
  // lookup in the hashtable.
  void DoNotSearchInIndex()   { mDoNotSearchInIndex = true; }
  void DoNotSearchInUpdates() { mDoNotSearchInUpdates = true; }

private:
  const CacheIndexEntry * FindEntry()
  {
    const CacheIndexEntry *entry = nullptr;

    switch (mIndex->mState) {
      case CacheIndex::READING:
      case CacheIndex::WRITING:
        if (!mDoNotSearchInUpdates) {
          entry = mIndex->mPendingUpdates.GetEntry(*mHash);
        }
        // no break
      case CacheIndex::BUILDING:
      case CacheIndex::UPDATING:
      case CacheIndex::READY:
        if (!entry && !mDoNotSearchInIndex) {
          entry = mIndex->mIndex.GetEntry(*mHash);
        }
        break;
      case CacheIndex::INITIAL:
      case CacheIndex::SHUTDOWN:
      default:
        MOZ_ASSERT(false, "Unexpected state!");
    }

    return entry;
  }

  const SHA1Sum::Hash *mHash;
  nsRefPtr<CacheIndex> mIndex;
  CacheIndexRecord    *mOldRecord;
  uint32_t             mOldFrecency;
  uint32_t             mOldExpirationTime;
  bool                 mDoNotSearchInIndex;
  bool                 mDoNotSearchInUpdates;
};

class FileOpenHelper : public CacheFileIOListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit FileOpenHelper(CacheIndex* aIndex)
    : mIndex(aIndex)
    , mCanceled(false)
  {}

  void Cancel() {
    mIndex->AssertOwnsLock();
    mCanceled = true;
  }

private:
  virtual ~FileOpenHelper() {}

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                           nsresult aResult) {
    MOZ_CRASH("FileOpenHelper::OnDataWritten should not be called!");
    return NS_ERROR_UNEXPECTED;
  }
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, char *aBuf,
                        nsresult aResult) {
    MOZ_CRASH("FileOpenHelper::OnDataRead should not be called!");
    return NS_ERROR_UNEXPECTED;
  }
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult) {
    MOZ_CRASH("FileOpenHelper::OnFileDoomed should not be called!");
    return NS_ERROR_UNEXPECTED;
  }
  NS_IMETHOD OnEOFSet(CacheFileHandle *aHandle, nsresult aResult) {
    MOZ_CRASH("FileOpenHelper::OnEOFSet should not be called!");
    return NS_ERROR_UNEXPECTED;
  }
  NS_IMETHOD OnFileRenamed(CacheFileHandle *aHandle, nsresult aResult) {
    MOZ_CRASH("FileOpenHelper::OnFileRenamed should not be called!");
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<CacheIndex> mIndex;
  bool                 mCanceled;
};

NS_IMETHODIMP FileOpenHelper::OnFileOpened(CacheFileHandle *aHandle,
                                           nsresult aResult)
{
  CacheIndexAutoLock lock(mIndex);

  if (mCanceled) {
    if (aHandle) {
      CacheFileIOManager::DoomFile(aHandle, nullptr);
    }

    return NS_OK;
  }

  mIndex->OnFileOpenedInternal(this, aHandle, aResult);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(FileOpenHelper, CacheFileIOListener);


CacheIndex * CacheIndex::gInstance = nullptr;


NS_IMPL_ADDREF(CacheIndex)
NS_IMPL_RELEASE(CacheIndex)

NS_INTERFACE_MAP_BEGIN(CacheIndex)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileIOListener)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END_THREADSAFE


CacheIndex::CacheIndex()
  : mLock("CacheIndex.mLock")
  , mState(INITIAL)
  , mShuttingDown(false)
  , mIndexNeedsUpdate(false)
  , mRemovingAll(false)
  , mIndexOnDiskIsValid(false)
  , mDontMarkIndexClean(false)
  , mIndexTimeStamp(0)
  , mUpdateEventPending(false)
  , mSkipEntries(0)
  , mProcessEntries(0)
  , mRWBuf(nullptr)
  , mRWBufSize(0)
  , mRWBufPos(0)
  , mJournalReadSuccessfully(false)
{
  LOG(("CacheIndex::CacheIndex [this=%p]", this));
  MOZ_COUNT_CTOR(CacheIndex);
  MOZ_ASSERT(!gInstance, "multiple CacheIndex instances!");
}

CacheIndex::~CacheIndex()
{
  LOG(("CacheIndex::~CacheIndex [this=%p]", this));
  MOZ_COUNT_DTOR(CacheIndex);

  ReleaseBuffer();
}

void
CacheIndex::Lock()
{
  mLock.Lock();

  MOZ_ASSERT(!mIndexStats.StateLogged());
}

void
CacheIndex::Unlock()
{
  MOZ_ASSERT(!mIndexStats.StateLogged());

  mLock.Unlock();
}

inline void
CacheIndex::AssertOwnsLock()
{
  mLock.AssertCurrentThreadOwns();
}

// static
nsresult
CacheIndex::Init(nsIFile *aCacheDirectory)
{
  LOG(("CacheIndex::Init()"));

  MOZ_ASSERT(NS_IsMainThread());

  if (gInstance) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsRefPtr<CacheIndex> idx = new CacheIndex();

  CacheIndexAutoLock lock(idx);

  nsresult rv = idx->InitInternal(aCacheDirectory);
  NS_ENSURE_SUCCESS(rv, rv);

  idx.swap(gInstance);
  return NS_OK;
}

nsresult
CacheIndex::InitInternal(nsIFile *aCacheDirectory)
{
  nsresult rv;

  rv = aCacheDirectory->Clone(getter_AddRefs(mCacheDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  mStartTime = TimeStamp::NowLoRes();

  ReadIndexFromDisk();

  return NS_OK;
}

// static
nsresult
CacheIndex::PreShutdown()
{
  LOG(("CacheIndex::PreShutdown() [gInstance=%p]", gInstance));

  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CacheIndexAutoLock lock(index);

  LOG(("CacheIndex::PreShutdown() - [state=%d, indexOnDiskIsValid=%d, "
       "dontMarkIndexClean=%d]", index->mState, index->mIndexOnDiskIsValid,
       index->mDontMarkIndexClean));

  LOG(("CacheIndex::PreShutdown() - Closing iterators."));
  for (uint32_t i = 0; i < index->mIterators.Length(); ) {
    rv = index->mIterators[i]->CloseInternal(NS_ERROR_FAILURE);
    if (NS_FAILED(rv)) {
      // CacheIndexIterator::CloseInternal() removes itself from mIteratos iff
      // it returns success.
      LOG(("CacheIndex::PreShutdown() - Failed to remove iterator %p. "
           "[rv=0x%08x]", rv));
      i++;
    }
  }

  index->mShuttingDown = true;

  if (index->mState == READY) {
    return NS_OK; // nothing to do
  }

  nsCOMPtr<nsIRunnable> event;
  event = NS_NewRunnableMethod(index, &CacheIndex::PreShutdownInternal);

  nsCOMPtr<nsIEventTarget> ioTarget = CacheFileIOManager::IOTarget();
  MOZ_ASSERT(ioTarget);

  // PreShutdownInternal() will be executed before any queued event on INDEX
  // level. That's OK since we don't want to wait for any operation in progess.
  // We need to interrupt it and save journal as quickly as possible.
  rv = ioTarget->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("CacheIndex::PreShutdown() - Can't dispatch event");
    LOG(("CacheIndex::PreShutdown() - Can't dispatch event" ));
    return rv;
  }

  return NS_OK;
}

void
CacheIndex::PreShutdownInternal()
{
  CacheIndexAutoLock lock(this);

  LOG(("CacheIndex::PreShutdownInternal() - [state=%d, indexOnDiskIsValid=%d, "
       "dontMarkIndexClean=%d]", mState, mIndexOnDiskIsValid,
       mDontMarkIndexClean));

  MOZ_ASSERT(mShuttingDown);

  if (mUpdateTimer) {
    mUpdateTimer = nullptr;
  }

  switch (mState) {
    case WRITING:
      FinishWrite(false);
      break;
    case READY:
      // nothing to do, write the journal in Shutdown()
      break;
    case READING:
      FinishRead(false);
      break;
    case BUILDING:
    case UPDATING:
      FinishUpdate(false);
      break;
    default:
      MOZ_ASSERT(false, "Implement me!");
  }

  // We should end up in READY state
  MOZ_ASSERT(mState == READY);
}

// static
nsresult
CacheIndex::Shutdown()
{
  LOG(("CacheIndex::Shutdown() [gInstance=%p]", gInstance));

  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<CacheIndex> index;
  index.swap(gInstance);

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CacheIndexAutoLock lock(index);

  bool sanitize = CacheObserver::ClearCacheOnShutdown();

  LOG(("CacheIndex::Shutdown() - [state=%d, indexOnDiskIsValid=%d, "
       "dontMarkIndexClean=%d, sanitize=%d]", index->mState,
       index->mIndexOnDiskIsValid, index->mDontMarkIndexClean, sanitize));

  MOZ_ASSERT(index->mShuttingDown);

  EState oldState = index->mState;
  index->ChangeState(SHUTDOWN);

  if (oldState != READY) {
    LOG(("CacheIndex::Shutdown() - Unexpected state. Did posting of "
         "PreShutdownInternal() fail?"));
  }

  switch (oldState) {
    case WRITING:
      index->FinishWrite(false);
      // no break
    case READY:
      if (index->mIndexOnDiskIsValid && !index->mDontMarkIndexClean) {
        if (!sanitize && NS_FAILED(index->WriteLogToDisk())) {
          index->RemoveIndexFromDisk();
        }
      } else {
        index->RemoveIndexFromDisk();
      }
      break;
    case READING:
      index->FinishRead(false);
      break;
    case BUILDING:
    case UPDATING:
      index->FinishUpdate(false);
      break;
    default:
      MOZ_ASSERT(false, "Unexpected state!");
  }

  if (sanitize) {
    index->RemoveIndexFromDisk();
  }

  return NS_OK;
}

// static
nsresult
CacheIndex::AddEntry(const SHA1Sum::Hash *aHash)
{
  LOG(("CacheIndex::AddEntry() [hash=%08x%08x%08x%08x%08x]", LOGSHA1(aHash)));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Getters in CacheIndexStats assert when mStateLogged is true since the
  // information is incomplete between calls to BeforeChange() and AfterChange()
  // (i.e. while CacheIndexEntryAutoManage exists). We need to check whether
  // non-fresh entries exists outside the scope of CacheIndexEntryAutoManage.
  bool updateIfNonFreshEntriesExist = false;

  {
    CacheIndexEntryAutoManage entryMng(aHash, index);

    CacheIndexEntry *entry = index->mIndex.GetEntry(*aHash);
    bool entryRemoved = entry && entry->IsRemoved();
    CacheIndexEntryUpdate *updated = nullptr;

    if (index->mState == READY || index->mState == UPDATING ||
        index->mState == BUILDING) {
      MOZ_ASSERT(index->mPendingUpdates.Count() == 0);

      if (entry && !entryRemoved) {
        // Found entry in index that shouldn't exist.

        if (entry->IsFresh()) {
          // Someone removed the file on disk while FF is running. Update
          // process can fix only non-fresh entries (i.e. entries that were not
          // added within this session). Start update only if we have such
          // entries.
          //
          // TODO: This should be very rare problem. If it turns out not to be
          // true, change the update process so that it also iterates all
          // initialized non-empty entries and checks whether the file exists.

          LOG(("CacheIndex::AddEntry() - Cache file was removed outside FF "
               "process!"));

          updateIfNonFreshEntriesExist = true;
        } else if (index->mState == READY) {
          // Index is outdated, update it.
          LOG(("CacheIndex::AddEntry() - Found entry that shouldn't exist, "
               "update is needed"));
          index->mIndexNeedsUpdate = true;
        } else {
          // We cannot be here when building index since all entries are fresh
          // during building.
          MOZ_ASSERT(index->mState == UPDATING);
        }
      }

      if (!entry) {
        entry = index->mIndex.PutEntry(*aHash);
      }
    } else { // WRITING, READING
      updated = index->mPendingUpdates.GetEntry(*aHash);
      bool updatedRemoved = updated && updated->IsRemoved();

      if ((updated && !updatedRemoved) ||
          (!updated && entry && !entryRemoved && entry->IsFresh())) {
        // Fresh entry found, so the file was removed outside FF
        LOG(("CacheIndex::AddEntry() - Cache file was removed outside FF "
             "process!"));

        updateIfNonFreshEntriesExist = true;
      } else if (!updated && entry && !entryRemoved) {
        if (index->mState == WRITING) {
          LOG(("CacheIndex::AddEntry() - Found entry that shouldn't exist, "
               "update is needed"));
          index->mIndexNeedsUpdate = true;
        }
        // Ignore if state is READING since the index information is partial
      }

      updated = index->mPendingUpdates.PutEntry(*aHash);
    }

    if (updated) {
      updated->InitNew();
      updated->MarkDirty();
      updated->MarkFresh();
    } else {
      entry->InitNew();
      entry->MarkDirty();
      entry->MarkFresh();
    }
  }

  if (updateIfNonFreshEntriesExist &&
      index->mIndexStats.Count() != index->mIndexStats.Fresh()) {
    index->mIndexNeedsUpdate = true;
  }

  index->StartUpdatingIndexIfNeeded();
  index->WriteIndexToDiskIfNeeded();

  return NS_OK;
}

// static
nsresult
CacheIndex::EnsureEntryExists(const SHA1Sum::Hash *aHash)
{
  LOG(("CacheIndex::EnsureEntryExists() [hash=%08x%08x%08x%08x%08x]",
       LOGSHA1(aHash)));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  {
    CacheIndexEntryAutoManage entryMng(aHash, index);

    CacheIndexEntry *entry = index->mIndex.GetEntry(*aHash);
    bool entryRemoved = entry && entry->IsRemoved();

    if (index->mState == READY || index->mState == UPDATING ||
        index->mState == BUILDING) {
      MOZ_ASSERT(index->mPendingUpdates.Count() == 0);

      if (!entry || entryRemoved) {
        if (entryRemoved && entry->IsFresh()) {
          // This could happen only if somebody copies files to the entries
          // directory while FF is running.
          LOG(("CacheIndex::EnsureEntryExists() - Cache file was added outside "
               "FF process! Update is needed."));
          index->mIndexNeedsUpdate = true;
        } else if (index->mState == READY ||
                   (entryRemoved && !entry->IsFresh())) {
          // Removed non-fresh entries can be present as a result of
          // ProcessJournalEntry()
          LOG(("CacheIndex::EnsureEntryExists() - Didn't find entry that should"
               " exist, update is needed"));
          index->mIndexNeedsUpdate = true;
        }

        if (!entry) {
          entry = index->mIndex.PutEntry(*aHash);
        }
        entry->InitNew();
        entry->MarkDirty();
      }
      entry->MarkFresh();
    } else { // WRITING, READING
      CacheIndexEntryUpdate *updated = index->mPendingUpdates.GetEntry(*aHash);
      bool updatedRemoved = updated && updated->IsRemoved();

      if (updatedRemoved ||
          (!updated && entryRemoved && entry->IsFresh())) {
        // Fresh information about missing entry found. This could happen only
        // if somebody copies files to the entries directory while FF is running.
        LOG(("CacheIndex::EnsureEntryExists() - Cache file was added outside "
             "FF process! Update is needed."));
        index->mIndexNeedsUpdate = true;
      } else if (!updated && (!entry || entryRemoved)) {
        if (index->mState == WRITING) {
          LOG(("CacheIndex::EnsureEntryExists() - Didn't find entry that should"
               " exist, update is needed"));
          index->mIndexNeedsUpdate = true;
        }
        // Ignore if state is READING since the index information is partial
      }

      // We don't need entryRemoved and updatedRemoved info anymore
      if (entryRemoved)   entry = nullptr;
      if (updatedRemoved) updated = nullptr;

      if (updated) {
        updated->MarkFresh();
      } else {
        if (!entry) {
          // Create a new entry
          updated = index->mPendingUpdates.PutEntry(*aHash);
          updated->InitNew();
          updated->MarkFresh();
          updated->MarkDirty();
        } else {
          if (!entry->IsFresh()) {
            // To mark the entry fresh we must make a copy of index entry
            // since the index is read-only.
            updated = index->mPendingUpdates.PutEntry(*aHash);
            *updated = *entry;
            updated->MarkFresh();
          }
        }
      }
    }
  }

  index->StartUpdatingIndexIfNeeded();
  index->WriteIndexToDiskIfNeeded();

  return NS_OK;
}

// static
nsresult
CacheIndex::InitEntry(const SHA1Sum::Hash *aHash,
                      uint32_t             aAppId,
                      bool                 aAnonymous,
                      bool                 aInBrowser)
{
  LOG(("CacheIndex::InitEntry() [hash=%08x%08x%08x%08x%08x, appId=%u, "
       "anonymous=%d, inBrowser=%d]", LOGSHA1(aHash), aAppId, aAnonymous,
       aInBrowser));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  {
    CacheIndexEntryAutoManage entryMng(aHash, index);

    CacheIndexEntry *entry = index->mIndex.GetEntry(*aHash);
    CacheIndexEntryUpdate *updated = nullptr;
    bool reinitEntry = false;

    if (entry && entry->IsRemoved()) {
      entry = nullptr;
    }

    if (index->mState == READY || index->mState == UPDATING ||
        index->mState == BUILDING) {
      MOZ_ASSERT(index->mPendingUpdates.Count() == 0);
      MOZ_ASSERT(entry);
      MOZ_ASSERT(entry->IsFresh());

      if (IsCollision(entry, aAppId, aAnonymous, aInBrowser)) {
        index->mIndexNeedsUpdate = true; // TODO Does this really help in case of collision?
        reinitEntry = true;
      } else {
        if (entry->IsInitialized()) {
          return NS_OK;
        }
      }
    } else {
      updated = index->mPendingUpdates.GetEntry(*aHash);
      DebugOnly<bool> removed = updated && updated->IsRemoved();

      MOZ_ASSERT(updated || !removed);
      MOZ_ASSERT(updated || entry);

      if (updated) {
        MOZ_ASSERT(updated->IsFresh());

        if (IsCollision(updated, aAppId, aAnonymous, aInBrowser)) {
          index->mIndexNeedsUpdate = true;
          reinitEntry = true;
        } else {
          if (updated->IsInitialized()) {
            return NS_OK;
          }
        }
      } else {
        MOZ_ASSERT(entry->IsFresh());

        if (IsCollision(entry, aAppId, aAnonymous, aInBrowser)) {
          index->mIndexNeedsUpdate = true;
          reinitEntry = true;
        } else {
          if (entry->IsInitialized()) {
            return NS_OK;
          }
        }

        // make a copy of a read-only entry
        updated = index->mPendingUpdates.PutEntry(*aHash);
        *updated = *entry;
      }
    }

    if (reinitEntry) {
      // There is a collision and we are going to rewrite this entry. Initialize
      // it as a new entry.
      if (updated) {
        updated->InitNew();
        updated->MarkFresh();
      } else {
        entry->InitNew();
        entry->MarkFresh();
      }
    }

    if (updated) {
      updated->Init(aAppId, aAnonymous, aInBrowser);
      updated->MarkDirty();
    } else {
      entry->Init(aAppId, aAnonymous, aInBrowser);
      entry->MarkDirty();
    }
  }

  index->StartUpdatingIndexIfNeeded();
  index->WriteIndexToDiskIfNeeded();

  return NS_OK;
}

// static
nsresult
CacheIndex::RemoveEntry(const SHA1Sum::Hash *aHash)
{
  LOG(("CacheIndex::RemoveEntry() [hash=%08x%08x%08x%08x%08x]",
       LOGSHA1(aHash)));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  {
    CacheIndexEntryAutoManage entryMng(aHash, index);

    CacheIndexEntry *entry = index->mIndex.GetEntry(*aHash);
    bool entryRemoved = entry && entry->IsRemoved();

    if (index->mState == READY || index->mState == UPDATING ||
        index->mState == BUILDING) {
      MOZ_ASSERT(index->mPendingUpdates.Count() == 0);

      if (!entry || entryRemoved) {
        if (entryRemoved && entry->IsFresh()) {
          // This could happen only if somebody copies files to the entries
          // directory while FF is running.
          LOG(("CacheIndex::RemoveEntry() - Cache file was added outside FF "
               "process! Update is needed."));
          index->mIndexNeedsUpdate = true;
        } else if (index->mState == READY ||
                   (entryRemoved && !entry->IsFresh())) {
          // Removed non-fresh entries can be present as a result of
          // ProcessJournalEntry()
          LOG(("CacheIndex::RemoveEntry() - Didn't find entry that should exist"
               ", update is needed"));
          index->mIndexNeedsUpdate = true;
        }
      } else {
        if (entry) {
          if (!entry->IsDirty() && entry->IsFileEmpty()) {
            index->mIndex.RemoveEntry(*aHash);
            entry = nullptr;
          } else {
            entry->MarkRemoved();
            entry->MarkDirty();
            entry->MarkFresh();
          }
        }
      }
    } else { // WRITING, READING
      CacheIndexEntryUpdate *updated = index->mPendingUpdates.GetEntry(*aHash);
      bool updatedRemoved = updated && updated->IsRemoved();

      if (updatedRemoved ||
          (!updated && entryRemoved && entry->IsFresh())) {
        // Fresh information about missing entry found. This could happen only
        // if somebody copies files to the entries directory while FF is running.
        LOG(("CacheIndex::RemoveEntry() - Cache file was added outside FF "
             "process! Update is needed."));
        index->mIndexNeedsUpdate = true;
      } else if (!updated && (!entry || entryRemoved)) {
        if (index->mState == WRITING) {
          LOG(("CacheIndex::RemoveEntry() - Didn't find entry that should exist"
               ", update is needed"));
          index->mIndexNeedsUpdate = true;
        }
        // Ignore if state is READING since the index information is partial
      }

      if (!updated) {
        updated = index->mPendingUpdates.PutEntry(*aHash);
        updated->InitNew();
      }

      updated->MarkRemoved();
      updated->MarkDirty();
      updated->MarkFresh();
    }
  }

  index->StartUpdatingIndexIfNeeded();
  index->WriteIndexToDiskIfNeeded();

  return NS_OK;
}

// static
nsresult
CacheIndex::UpdateEntry(const SHA1Sum::Hash *aHash,
                        const uint32_t      *aFrecency,
                        const uint32_t      *aExpirationTime,
                        const uint32_t      *aSize)
{
  LOG(("CacheIndex::UpdateEntry() [hash=%08x%08x%08x%08x%08x, "
       "frecency=%s, expirationTime=%s, size=%s]", LOGSHA1(aHash),
       aFrecency ? nsPrintfCString("%u", *aFrecency).get() : "",
       aExpirationTime ? nsPrintfCString("%u", *aExpirationTime).get() : "",
       aSize ? nsPrintfCString("%u", *aSize).get() : ""));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  {
    CacheIndexEntryAutoManage entryMng(aHash, index);

    CacheIndexEntry *entry = index->mIndex.GetEntry(*aHash);

    if (entry && entry->IsRemoved()) {
      entry = nullptr;
    }

    if (index->mState == READY || index->mState == UPDATING ||
        index->mState == BUILDING) {
      MOZ_ASSERT(index->mPendingUpdates.Count() == 0);
      MOZ_ASSERT(entry);

      if (!HasEntryChanged(entry, aFrecency, aExpirationTime, aSize)) {
        return NS_OK;
      }

      MOZ_ASSERT(entry->IsFresh());
      MOZ_ASSERT(entry->IsInitialized());
      entry->MarkDirty();

      if (aFrecency) {
        entry->SetFrecency(*aFrecency);
      }

      if (aExpirationTime) {
        entry->SetExpirationTime(*aExpirationTime);
      }

      if (aSize) {
        entry->SetFileSize(*aSize);
      }
    } else {
      CacheIndexEntryUpdate *updated = index->mPendingUpdates.GetEntry(*aHash);
      DebugOnly<bool> removed = updated && updated->IsRemoved();

      MOZ_ASSERT(updated || !removed);
      MOZ_ASSERT(updated || entry);

      if (!updated) {
        if (!entry) {
          LOG(("CacheIndex::UpdateEntry() - Entry was found neither in mIndex "
               "nor in mPendingUpdates!"));
          NS_WARNING(("CacheIndex::UpdateEntry() - Entry was found neither in "
                      "mIndex nor in mPendingUpdates!"));
          return NS_ERROR_NOT_AVAILABLE;
        }

        // make a copy of a read-only entry
        updated = index->mPendingUpdates.PutEntry(*aHash);
        *updated = *entry;
      }

      MOZ_ASSERT(updated->IsFresh());
      MOZ_ASSERT(updated->IsInitialized());
      updated->MarkDirty();

      if (aFrecency) {
        updated->SetFrecency(*aFrecency);
      }

      if (aExpirationTime) {
        updated->SetExpirationTime(*aExpirationTime);
      }

      if (aSize) {
        updated->SetFileSize(*aSize);
      }
    }
  }

  index->WriteIndexToDiskIfNeeded();

  return NS_OK;
}

// static
nsresult
CacheIndex::RemoveAll()
{
  LOG(("CacheIndex::RemoveAll()"));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsCOMPtr<nsIFile> file;

  {
    CacheIndexAutoLock lock(index);

    MOZ_ASSERT(!index->mRemovingAll);

    if (!index->IsIndexUsable()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    AutoRestore<bool> saveRemovingAll(index->mRemovingAll);
    index->mRemovingAll = true;

    // Doom index and journal handles but don't null them out since this will be
    // done in FinishWrite/FinishRead methods.
    if (index->mIndexHandle) {
      CacheFileIOManager::DoomFile(index->mIndexHandle, nullptr);
    } else {
      // We don't have a handle to index file, so get the file here, but delete
      // it outside the lock. Ignore the result since this is not fatal.
      index->GetFile(NS_LITERAL_CSTRING(kIndexName), getter_AddRefs(file));
    }

    if (index->mJournalHandle) {
      CacheFileIOManager::DoomFile(index->mJournalHandle, nullptr);
    }

    switch (index->mState) {
      case WRITING:
        index->FinishWrite(false);
        break;
      case READY:
        // nothing to do
        break;
      case READING:
        index->FinishRead(false);
        break;
      case BUILDING:
      case UPDATING:
        index->FinishUpdate(false);
        break;
      default:
        MOZ_ASSERT(false, "Unexpected state!");
    }

    // We should end up in READY state
    MOZ_ASSERT(index->mState == READY);

    // There should not be any handle
    MOZ_ASSERT(!index->mIndexHandle);
    MOZ_ASSERT(!index->mJournalHandle);

    index->mIndexOnDiskIsValid = false;
    index->mIndexNeedsUpdate = false;

    index->mIndexStats.Clear();
    index->mFrecencyArray.Clear();
    index->mExpirationArray.Clear();
    index->mIndex.Clear();
  }

  if (file) {
    // Ignore the result. The file might not exist and the failure is not fatal.
    file->Remove(false);
  }

  return NS_OK;
}

// static
nsresult
CacheIndex::HasEntry(const nsACString &aKey, EntryStatus *_retval)
{
  LOG(("CacheIndex::HasEntry() [key=%s]", PromiseFlatCString(aKey).get()));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SHA1Sum sum;
  SHA1Sum::Hash hash;
  sum.update(aKey.BeginReading(), aKey.Length());
  sum.finish(hash);

  const CacheIndexEntry *entry = nullptr;

  switch (index->mState) {
    case READING:
    case WRITING:
      entry = index->mPendingUpdates.GetEntry(hash);
      // no break
    case BUILDING:
    case UPDATING:
    case READY:
      if (!entry) {
        entry = index->mIndex.GetEntry(hash);
      }
      break;
    case INITIAL:
    case SHUTDOWN:
      MOZ_ASSERT(false, "Unexpected state!");
  }

  if (!entry) {
    if (index->mState == READY || index->mState == WRITING) {
      *_retval = DOES_NOT_EXIST;
    } else {
      *_retval = DO_NOT_KNOW;
    }
  } else {
    if (entry->IsRemoved()) {
      if (entry->IsFresh()) {
        *_retval = DOES_NOT_EXIST;
      } else {
        *_retval = DO_NOT_KNOW;
      }
    } else {
      *_retval = EXISTS;
    }
  }

  LOG(("CacheIndex::HasEntry() - result is %u", *_retval));
  return NS_OK;
}

// static
nsresult
CacheIndex::GetEntryForEviction(SHA1Sum::Hash *aHash, uint32_t *aCnt)
{
  LOG(("CacheIndex::GetEntryForEviction()"));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index)
    return NS_ERROR_NOT_INITIALIZED;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(index->mFrecencyArray.Length() ==
             index->mExpirationArray.Length());

  if (index->mExpirationArray.Length() == 0)
    return NS_ERROR_NOT_AVAILABLE;

  SHA1Sum::Hash hash;
  bool foundEntry = false;
  uint32_t i = 0, j = 0;
  uint32_t now = PR_Now() / PR_USEC_PER_SEC;

  // find the first expired, non-forced valid entry
  for (i = 0; i < index->mExpirationArray.Length(); i++) {
    if (index->mExpirationArray[i]->mExpirationTime < now) {
      memcpy(&hash, &index->mExpirationArray[i]->mHash, sizeof(SHA1Sum::Hash));

      if (!IsForcedValidEntry(&hash)) {
        foundEntry = true;
        break;
      }

    } else {
      // all further entries have not expired yet
      break;
    }
  }

  if (foundEntry) {
    *aCnt = index->mExpirationArray.Length() - i;

    LOG(("CacheIndex::GetEntryForEviction() - returning entry from expiration "
         "array [hash=%08x%08x%08x%08x%08x, cnt=%u, expTime=%u, now=%u, "
         "frecency=%u]", LOGSHA1(&hash), *aCnt,
         index->mExpirationArray[i]->mExpirationTime, now,
         index->mExpirationArray[i]->mFrecency));
  }
  else {
    // check if we've already tried all the entries
    if (i == index->mExpirationArray.Length())
      return NS_ERROR_NOT_AVAILABLE;

    // find first non-forced valid entry with the lowest frecency
    for (j = 0; j < index->mFrecencyArray.Length(); j++) {
      memcpy(&hash, &index->mFrecencyArray[j]->mHash, sizeof(SHA1Sum::Hash));

      if (!IsForcedValidEntry(&hash)) {
        foundEntry = true;
        break;
      }
    }

    if (!foundEntry)
      return NS_ERROR_NOT_AVAILABLE;

    // forced valid entries skipped in both arrays could overlap, just use max
    *aCnt = index->mFrecencyArray.Length() - std::max(i, j);

    LOG(("CacheIndex::GetEntryForEviction() - returning entry from frecency "
         "array [hash=%08x%08x%08x%08x%08x, cnt=%u, expTime=%u, now=%u, "
         "frecency=%u]", LOGSHA1(&hash), *aCnt,
         index->mFrecencyArray[j]->mExpirationTime, now,
         index->mFrecencyArray[j]->mFrecency));
  }

  memcpy(aHash, &hash, sizeof(SHA1Sum::Hash));

  return NS_OK;
}


// static
bool CacheIndex::IsForcedValidEntry(const SHA1Sum::Hash *aHash)
{
  nsRefPtr<CacheFileHandle> handle;

  CacheFileIOManager::gInstance->mHandles.GetHandle(
    aHash, false, getter_AddRefs(handle));

  if (!handle)
    return false;

  nsCString hashKey = handle->Key();
  return CacheStorageService::Self()->IsForcedValidEntry(hashKey);
}


// static
nsresult
CacheIndex::GetCacheSize(uint32_t *_retval)
{
  LOG(("CacheIndex::GetCacheSize()"));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index)
    return NS_ERROR_NOT_INITIALIZED;

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *_retval = index->mIndexStats.Size();
  LOG(("CacheIndex::GetCacheSize() - returning %u", *_retval));
  return NS_OK;
}

// static
nsresult
CacheIndex::GetCacheStats(nsILoadContextInfo *aInfo, uint32_t *aSize, uint32_t *aCount)
{
  LOG(("CacheIndex::GetCacheStats() [info=%p]", aInfo));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!aInfo) {
    return NS_ERROR_INVALID_ARG;
  }

  *aSize = 0;
  *aCount = 0;

  for (uint32_t i = 0; i < index->mFrecencyArray.Length(); ++i) {
    CacheIndexRecord* record = index->mFrecencyArray[i];
    if (!CacheIndexEntry::RecordMatchesLoadContextInfo(record, aInfo))
      continue;

    *aSize += CacheIndexEntry::GetFileSize(record);
    ++*aCount;
  }

  return NS_OK;
}

// static
nsresult
CacheIndex::AsyncGetDiskConsumption(nsICacheStorageConsumptionObserver* aObserver)
{
  LOG(("CacheIndex::AsyncGetDiskConsumption()"));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<DiskConsumptionObserver> observer =
    DiskConsumptionObserver::Init(aObserver);

  NS_ENSURE_ARG(observer);

  if (index->mState == READY || index->mState == WRITING) {
    LOG(("CacheIndex::AsyncGetDiskConsumption - calling immediately"));
    // Safe to call the callback under the lock,
    // we always post to the main thread.
    observer->OnDiskConsumption(index->mIndexStats.Size() << 10);
    return NS_OK;
  }

  LOG(("CacheIndex::AsyncGetDiskConsumption - remembering callback"));
  // Will be called when the index get to the READY state.
  index->mDiskConsumptionObservers.AppendElement(observer);

  return NS_OK;
}

// static
nsresult
CacheIndex::GetIterator(nsILoadContextInfo *aInfo, bool aAddNew,
                        CacheIndexIterator **_retval)
{
  LOG(("CacheIndex::GetIterator() [info=%p, addNew=%d]", aInfo, aAddNew));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<CacheIndexIterator> iter;
  if (aInfo) {
    iter = new CacheIndexContextIterator(index, aAddNew, aInfo);
  } else {
    iter = new CacheIndexIterator(index, aAddNew);
  }

  iter->AddRecords(index->mFrecencyArray);

  index->mIterators.AppendElement(iter);
  iter.swap(*_retval);
  return NS_OK;
}

// static
nsresult
CacheIndex::IsUpToDate(bool *_retval)
{
  LOG(("CacheIndex::IsUpToDate()"));

  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CacheIndexAutoLock lock(index);

  if (!index->IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *_retval = (index->mState == READY || index->mState == WRITING) &&
             !index->mIndexNeedsUpdate && !index->mShuttingDown;

  LOG(("CacheIndex::IsUpToDate() - returning %p", *_retval));
  return NS_OK;
}

bool
CacheIndex::IsIndexUsable()
{
  MOZ_ASSERT(mState != INITIAL);

  switch (mState) {
    case INITIAL:
    case SHUTDOWN:
      return false;

    case READING:
    case WRITING:
    case BUILDING:
    case UPDATING:
    case READY:
      break;
  }

  return true;
}

// static
bool
CacheIndex::IsCollision(CacheIndexEntry *aEntry,
                        uint32_t         aAppId,
                        bool             aAnonymous,
                        bool             aInBrowser)
{
  if (!aEntry->IsInitialized()) {
    return false;
  }

  if (aEntry->AppId() != aAppId || aEntry->Anonymous() != aAnonymous ||
      aEntry->InBrowser() != aInBrowser) {
    LOG(("CacheIndex::IsCollision() - Collision detected for entry hash=%08x"
         "%08x%08x%08x%08x, expected values: appId=%u, anonymous=%d, "
         "inBrowser=%d; actual values: appId=%u, anonymous=%d, inBrowser=%d]",
         LOGSHA1(aEntry->Hash()), aAppId, aAnonymous, aInBrowser,
         aEntry->AppId(), aEntry->Anonymous(), aEntry->InBrowser()));
    return true;
  }

  return false;
}

// static
bool
CacheIndex::HasEntryChanged(CacheIndexEntry *aEntry,
                            const uint32_t  *aFrecency,
                            const uint32_t  *aExpirationTime,
                            const uint32_t  *aSize)
{
  if (aFrecency && *aFrecency != aEntry->GetFrecency()) {
    return true;
  }

  if (aExpirationTime && *aExpirationTime != aEntry->GetExpirationTime()) {
    return true;
  }

  if (aSize &&
      (*aSize & CacheIndexEntry::kFileSizeMask) != aEntry->GetFileSize()) {
    return true;
  }

  return false;
}

void
CacheIndex::ProcessPendingOperations()
{
  LOG(("CacheIndex::ProcessPendingOperations()"));

  AssertOwnsLock();

  mPendingUpdates.EnumerateEntries(&CacheIndex::UpdateEntryInIndex, this);

  MOZ_ASSERT(mPendingUpdates.Count() == 0);

  EnsureCorrectStats();
}

// static
PLDHashOperator
CacheIndex::UpdateEntryInIndex(CacheIndexEntryUpdate *aEntry, void* aClosure)
{
  CacheIndex *index = static_cast<CacheIndex *>(aClosure);

  LOG(("CacheFile::UpdateEntryInIndex() [hash=%08x%08x%08x%08x%08x]",
       LOGSHA1(aEntry->Hash())));

  MOZ_ASSERT(aEntry->IsFresh());

  CacheIndexEntry *entry = index->mIndex.GetEntry(*aEntry->Hash());

  CacheIndexEntryAutoManage emng(aEntry->Hash(), index);
  emng.DoNotSearchInUpdates();

  if (aEntry->IsRemoved()) {
    if (entry) {
      if (entry->IsRemoved()) {
        MOZ_ASSERT(entry->IsFresh());
        MOZ_ASSERT(entry->IsDirty());
      } else if (!entry->IsDirty() && entry->IsFileEmpty()) {
        // Entries with empty file are not stored in index on disk. Just remove
        // the entry, but only in case the entry is not dirty, i.e. the entry
        // file was empty when we wrote the index.
        index->mIndex.RemoveEntry(*aEntry->Hash());
        entry = nullptr;
      } else {
        entry->MarkRemoved();
        entry->MarkDirty();
        entry->MarkFresh();
      }
    }

    return PL_DHASH_REMOVE;
  }

  if (entry) {
    // Some information in mIndex can be newer than in mPendingUpdates (see bug
    // 1074832). This will copy just those values that were really updated.
    aEntry->ApplyUpdate(entry);
  } else {
    // There is no entry in mIndex, copy all information from mPendingUpdates
    // to mIndex.
    entry = index->mIndex.PutEntry(*aEntry->Hash());
    *entry = *aEntry;
  }

  return PL_DHASH_REMOVE;
}

bool
CacheIndex::WriteIndexToDiskIfNeeded()
{
  if (mState != READY || mShuttingDown) {
    return false;
  }

  if (!mLastDumpTime.IsNull() &&
      (TimeStamp::NowLoRes() - mLastDumpTime).ToMilliseconds() <
      kMinDumpInterval) {
    return false;
  }

  if (mIndexStats.Dirty() < kMinUnwrittenChanges) {
    return false;
  }

  WriteIndexToDisk();
  return true;
}

void
CacheIndex::WriteIndexToDisk()
{
  LOG(("CacheIndex::WriteIndexToDisk()"));
  mIndexStats.Log();

  nsresult rv;

  AssertOwnsLock();
  MOZ_ASSERT(mState == READY);
  MOZ_ASSERT(!mRWBuf);
  MOZ_ASSERT(!mRWHash);

  ChangeState(WRITING);

  mProcessEntries = mIndexStats.ActiveEntriesCount();

  mIndexFileOpener = new FileOpenHelper(this);
  rv = CacheFileIOManager::OpenFile(NS_LITERAL_CSTRING(kTempIndexName),
                                    CacheFileIOManager::SPECIAL_FILE |
                                    CacheFileIOManager::CREATE,
                                    mIndexFileOpener);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::WriteIndexToDisk() - Can't open file [rv=0x%08x]", rv));
    FinishWrite(false);
    return;
  }

  // Write index header to a buffer, it will be written to disk together with
  // records in WriteRecords() once we open the file successfully.
  AllocBuffer();
  mRWHash = new CacheHash();

  CacheIndexHeader *hdr = reinterpret_cast<CacheIndexHeader *>(mRWBuf);
  NetworkEndian::writeUint32(&hdr->mVersion, kIndexVersion);
  NetworkEndian::writeUint32(&hdr->mTimeStamp,
                             static_cast<uint32_t>(PR_Now() / PR_USEC_PER_SEC));
  NetworkEndian::writeUint32(&hdr->mIsDirty, 1);

  mRWBufPos = sizeof(CacheIndexHeader);
  mSkipEntries = 0;
}

namespace { // anon

struct WriteRecordsHelper
{
  char    *mBuf;
  uint32_t mSkip;
  uint32_t mProcessMax;
  uint32_t mProcessed;
#ifdef DEBUG
  bool     mHasMore;
#endif
};

} // anon

void
CacheIndex::WriteRecords()
{
  LOG(("CacheIndex::WriteRecords()"));

  nsresult rv;

  AssertOwnsLock();
  MOZ_ASSERT(mState == WRITING);

  int64_t fileOffset;

  if (mSkipEntries) {
    MOZ_ASSERT(mRWBufPos == 0);
    fileOffset = sizeof(CacheIndexHeader);
    fileOffset += sizeof(CacheIndexRecord) * mSkipEntries;
  } else {
    MOZ_ASSERT(mRWBufPos == sizeof(CacheIndexHeader));
    fileOffset = 0;
  }
  uint32_t hashOffset = mRWBufPos;

  WriteRecordsHelper data;
  data.mBuf = mRWBuf + mRWBufPos;
  data.mSkip = mSkipEntries;
  data.mProcessMax = (mRWBufSize - mRWBufPos) / sizeof(CacheIndexRecord);
  MOZ_ASSERT(data.mProcessMax != 0 || mProcessEntries == 0); // TODO make sure we can write an empty index
  data.mProcessed = 0;
#ifdef DEBUG
  data.mHasMore = false;
#endif

  mIndex.EnumerateEntries(&CacheIndex::CopyRecordsToRWBuf, &data);
  MOZ_ASSERT(mRWBufPos != static_cast<uint32_t>(data.mBuf - mRWBuf) ||
             mProcessEntries == 0);
  mRWBufPos = data.mBuf - mRWBuf;
  mSkipEntries += data.mProcessed;
  MOZ_ASSERT(mSkipEntries <= mProcessEntries);

  mRWHash->Update(mRWBuf + hashOffset, mRWBufPos - hashOffset);

  if (mSkipEntries == mProcessEntries) {
    MOZ_ASSERT(!data.mHasMore);

    // We've processed all records
    if (mRWBufPos + sizeof(CacheHash::Hash32_t) > mRWBufSize) {
      // realloc buffer to spare another write cycle
      mRWBufSize = mRWBufPos + sizeof(CacheHash::Hash32_t);
      mRWBuf = static_cast<char *>(moz_xrealloc(mRWBuf, mRWBufSize));
    }

    NetworkEndian::writeUint32(mRWBuf + mRWBufPos, mRWHash->GetHash());
    mRWBufPos += sizeof(CacheHash::Hash32_t);
  } else {
    MOZ_ASSERT(data.mHasMore);
  }

  rv = CacheFileIOManager::Write(mIndexHandle, fileOffset, mRWBuf, mRWBufPos,
                                 mSkipEntries == mProcessEntries, this);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::WriteRecords() - CacheFileIOManager::Write() failed "
         "synchronously [rv=0x%08x]", rv));
    FinishWrite(false);
  }

  mRWBufPos = 0;
}

void
CacheIndex::FinishWrite(bool aSucceeded)
{
  LOG(("CacheIndex::FinishWrite() [succeeded=%d]", aSucceeded));

  MOZ_ASSERT((!aSucceeded && mState == SHUTDOWN) || mState == WRITING);

  AssertOwnsLock();

  mIndexHandle = nullptr;
  mRWHash = nullptr;
  ReleaseBuffer();

  if (aSucceeded) {
    // Opening of the file must not be in progress if writing succeeded.
    MOZ_ASSERT(!mIndexFileOpener);

    mIndex.EnumerateEntries(&CacheIndex::ApplyIndexChanges, this);
    mIndexOnDiskIsValid = true;
  } else {
    if (mIndexFileOpener) {
      // If opening of the file is still in progress (e.g. WRITE process was
      // canceled by RemoveAll()) then we need to cancel the opener to make sure
      // that OnFileOpenedInternal() won't be called.
      mIndexFileOpener->Cancel();
      mIndexFileOpener = nullptr;
    }
  }

  ProcessPendingOperations();
  mIndexStats.Log();

  if (mState == WRITING) {
    ChangeState(READY);
    mLastDumpTime = TimeStamp::NowLoRes();
  }
}

// static
PLDHashOperator
CacheIndex::CopyRecordsToRWBuf(CacheIndexEntry *aEntry, void* aClosure)
{
  if (aEntry->IsRemoved()) {
    return PL_DHASH_NEXT;
  }

  if (!aEntry->IsInitialized()) {
    return PL_DHASH_NEXT;
  }

  if (aEntry->IsFileEmpty()) {
    return PL_DHASH_NEXT;
  }

  WriteRecordsHelper *data = static_cast<WriteRecordsHelper *>(aClosure);
  if (data->mSkip) {
    data->mSkip--;
    return PL_DHASH_NEXT;
  }

  if (data->mProcessed == data->mProcessMax) {
#ifdef DEBUG
    data->mHasMore = true;
#endif
    return PL_DHASH_STOP;
  }

  aEntry->WriteToBuf(data->mBuf);
  data->mBuf += sizeof(CacheIndexRecord);
  data->mProcessed++;

  return PL_DHASH_NEXT;
}

// static
PLDHashOperator
CacheIndex::ApplyIndexChanges(CacheIndexEntry *aEntry, void* aClosure)
{
  CacheIndex *index = static_cast<CacheIndex *>(aClosure);

  CacheIndexEntryAutoManage emng(aEntry->Hash(), index);

  if (aEntry->IsRemoved()) {
    emng.DoNotSearchInIndex();
    return PL_DHASH_REMOVE;
  }

  if (aEntry->IsDirty()) {
    aEntry->ClearDirty();
  }

  return PL_DHASH_NEXT;
}

nsresult
CacheIndex::GetFile(const nsACString &aName, nsIFile **_retval)
{
  nsresult rv;

  nsCOMPtr<nsIFile> file;
  rv = mCacheDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(aName);
  NS_ENSURE_SUCCESS(rv, rv);

  file.swap(*_retval);
  return NS_OK;
}

nsresult
CacheIndex::RemoveFile(const nsACString &aName)
{
  MOZ_ASSERT(mState == SHUTDOWN);

  nsresult rv;

  nsCOMPtr<nsIFile> file;
  rv = GetFile(aName, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    rv = file->Remove(false);
    if (NS_FAILED(rv)) {
      LOG(("CacheIndex::RemoveFile() - Cannot remove old entry file from disk."
           "[name=%s]", PromiseFlatCString(aName).get()));
      NS_WARNING("Cannot remove old entry file from the disk");
      return rv;
    }
  }

  return NS_OK;
}

void
CacheIndex::RemoveIndexFromDisk()
{
  LOG(("CacheIndex::RemoveIndexFromDisk()"));

  RemoveFile(NS_LITERAL_CSTRING(kIndexName));
  RemoveFile(NS_LITERAL_CSTRING(kTempIndexName));
  RemoveFile(NS_LITERAL_CSTRING(kJournalName));
}

class WriteLogHelper
{
public:
  explicit WriteLogHelper(PRFileDesc *aFD)
    : mStatus(NS_OK)
    , mFD(aFD)
    , mBufSize(kMaxBufSize)
    , mBufPos(0)
  {
    mHash = new CacheHash();
    mBuf = static_cast<char *>(moz_xmalloc(mBufSize));
  }

  ~WriteLogHelper() {
    free(mBuf);
  }

  nsresult AddEntry(CacheIndexEntry *aEntry);
  nsresult Finish();

private:

  nsresult FlushBuffer();

  nsresult            mStatus;
  PRFileDesc         *mFD;
  char               *mBuf;
  uint32_t            mBufSize;
  int32_t             mBufPos;
  nsRefPtr<CacheHash> mHash;
};

nsresult
WriteLogHelper::AddEntry(CacheIndexEntry *aEntry)
{
  nsresult rv;

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  if (mBufPos + sizeof(CacheIndexRecord) > mBufSize) {
    mHash->Update(mBuf, mBufPos);

    rv = FlushBuffer();
    if (NS_FAILED(rv)) {
      mStatus = rv;
      return rv;
    }
    MOZ_ASSERT(mBufPos + sizeof(CacheIndexRecord) <= mBufSize);
  }

  aEntry->WriteToBuf(mBuf + mBufPos);
  mBufPos += sizeof(CacheIndexRecord);

  return NS_OK;
}

nsresult
WriteLogHelper::Finish()
{
  nsresult rv;

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  mHash->Update(mBuf, mBufPos);
  if (mBufPos + sizeof(CacheHash::Hash32_t) > mBufSize) {
    rv = FlushBuffer();
    if (NS_FAILED(rv)) {
      mStatus = rv;
      return rv;
    }
    MOZ_ASSERT(mBufPos + sizeof(CacheHash::Hash32_t) <= mBufSize);
  }

  NetworkEndian::writeUint32(mBuf + mBufPos, mHash->GetHash());
  mBufPos += sizeof(CacheHash::Hash32_t);

  rv = FlushBuffer();
  NS_ENSURE_SUCCESS(rv, rv);

  mStatus = NS_ERROR_UNEXPECTED; // Don't allow any other operation
  return NS_OK;
}

nsresult
WriteLogHelper::FlushBuffer()
{
  MOZ_ASSERT(NS_SUCCEEDED(mStatus));

  int32_t bytesWritten = PR_Write(mFD, mBuf, mBufPos);

  if (bytesWritten != mBufPos) {
    return NS_ERROR_FAILURE;
  }

  mBufPos = 0;
  return NS_OK;
}

nsresult
CacheIndex::WriteLogToDisk()
{
  LOG(("CacheIndex::WriteLogToDisk()"));

  nsresult rv;

  MOZ_ASSERT(mPendingUpdates.Count() == 0);
  MOZ_ASSERT(mState == SHUTDOWN);

  RemoveFile(NS_LITERAL_CSTRING(kTempIndexName));

  nsCOMPtr<nsIFile> indexFile;
  rv = GetFile(NS_LITERAL_CSTRING(kIndexName), getter_AddRefs(indexFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> logFile;
  rv = GetFile(NS_LITERAL_CSTRING(kJournalName), getter_AddRefs(logFile));
  NS_ENSURE_SUCCESS(rv, rv);

  mIndexStats.Log();

  PRFileDesc *fd = nullptr;
  rv = logFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
                                 0600, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  WriteLogHelper wlh(fd);
  mIndex.EnumerateEntries(&CacheIndex::WriteEntryToLog, &wlh);

  rv = wlh.Finish();
  PR_Close(fd);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = indexFile->OpenNSPRFileDesc(PR_RDWR, 0600, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  CacheIndexHeader header;
  int32_t bytesRead = PR_Read(fd, &header, sizeof(CacheIndexHeader));
  if (bytesRead != sizeof(CacheIndexHeader)) {
    PR_Close(fd);
    return NS_ERROR_FAILURE;
  }

  NetworkEndian::writeUint32(&header.mIsDirty, 0);

  int64_t offset = PR_Seek64(fd, 0, PR_SEEK_SET);
  if (offset == -1) {
    PR_Close(fd);
    return NS_ERROR_FAILURE;
  }

  int32_t bytesWritten = PR_Write(fd, &header, sizeof(CacheIndexHeader));
  PR_Close(fd);
  if (bytesWritten != sizeof(CacheIndexHeader)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// static
PLDHashOperator
CacheIndex::WriteEntryToLog(CacheIndexEntry *aEntry, void* aClosure)
{
  WriteLogHelper *wlh = static_cast<WriteLogHelper *>(aClosure);

  if (aEntry->IsRemoved() || aEntry->IsDirty()) {
    wlh->AddEntry(aEntry);
  }

  return PL_DHASH_REMOVE;
}

void
CacheIndex::ReadIndexFromDisk()
{
  LOG(("CacheIndex::ReadIndexFromDisk()"));

  nsresult rv;

  AssertOwnsLock();
  MOZ_ASSERT(mState == INITIAL);

  ChangeState(READING);

  mIndexFileOpener = new FileOpenHelper(this);
  rv = CacheFileIOManager::OpenFile(NS_LITERAL_CSTRING(kIndexName),
                                    CacheFileIOManager::SPECIAL_FILE |
                                    CacheFileIOManager::OPEN,
                                    mIndexFileOpener);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::ReadIndexFromDisk() - CacheFileIOManager::OpenFile() "
         "failed [rv=0x%08x, file=%s]", rv, kIndexName));
    FinishRead(false);
    return;
  }

  mJournalFileOpener = new FileOpenHelper(this);
  rv = CacheFileIOManager::OpenFile(NS_LITERAL_CSTRING(kJournalName),
                                    CacheFileIOManager::SPECIAL_FILE |
                                    CacheFileIOManager::OPEN,
                                    mJournalFileOpener);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::ReadIndexFromDisk() - CacheFileIOManager::OpenFile() "
         "failed [rv=0x%08x, file=%s]", rv, kJournalName));
    FinishRead(false);
  }

  mTmpFileOpener = new FileOpenHelper(this);
  rv = CacheFileIOManager::OpenFile(NS_LITERAL_CSTRING(kTempIndexName),
                                    CacheFileIOManager::SPECIAL_FILE |
                                    CacheFileIOManager::OPEN,
                                    mTmpFileOpener);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::ReadIndexFromDisk() - CacheFileIOManager::OpenFile() "
         "failed [rv=0x%08x, file=%s]", rv, kTempIndexName));
    FinishRead(false);
  }
}

void
CacheIndex::StartReadingIndex()
{
  LOG(("CacheIndex::StartReadingIndex()"));

  nsresult rv;

  AssertOwnsLock();

  MOZ_ASSERT(mIndexHandle);
  MOZ_ASSERT(mState == READING);
  MOZ_ASSERT(!mIndexOnDiskIsValid);
  MOZ_ASSERT(!mDontMarkIndexClean);
  MOZ_ASSERT(!mJournalReadSuccessfully);
  MOZ_ASSERT(mIndexHandle->FileSize() >= 0);

  int64_t entriesSize = mIndexHandle->FileSize() - sizeof(CacheIndexHeader) -
                        sizeof(CacheHash::Hash32_t);

  if (entriesSize < 0 || entriesSize % sizeof(CacheIndexRecord)) {
    LOG(("CacheIndex::StartReadingIndex() - Index is corrupted"));
    FinishRead(false);
    return;
  }

  AllocBuffer();
  mSkipEntries = 0;
  mRWHash = new CacheHash();

  mRWBufPos = std::min(mRWBufSize,
                       static_cast<uint32_t>(mIndexHandle->FileSize()));

  rv = CacheFileIOManager::Read(mIndexHandle, 0, mRWBuf, mRWBufPos, this);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::StartReadingIndex() - CacheFileIOManager::Read() failed "
         "synchronously [rv=0x%08x]", rv));
    FinishRead(false);
  }
}

void
CacheIndex::ParseRecords()
{
  LOG(("CacheIndex::ParseRecords()"));

  nsresult rv;

  AssertOwnsLock();

  uint32_t entryCnt = (mIndexHandle->FileSize() - sizeof(CacheIndexHeader) -
                     sizeof(CacheHash::Hash32_t)) / sizeof(CacheIndexRecord);
  uint32_t pos = 0;

  if (!mSkipEntries) {
    CacheIndexHeader *hdr = reinterpret_cast<CacheIndexHeader *>(
                              moz_xmalloc(sizeof(CacheIndexHeader)));
    memcpy(hdr, mRWBuf, sizeof(CacheIndexHeader));

    if (NetworkEndian::readUint32(&hdr->mVersion) != kIndexVersion) {
      free(hdr);
      FinishRead(false);
      return;
    }

    mIndexTimeStamp = NetworkEndian::readUint32(&hdr->mTimeStamp);

    if (NetworkEndian::readUint32(&hdr->mIsDirty)) {
      if (mJournalHandle) {
        CacheFileIOManager::DoomFile(mJournalHandle, nullptr);
        mJournalHandle = nullptr;
      }
      free(hdr);
    } else {
      NetworkEndian::writeUint32(&hdr->mIsDirty, 1);

      // Mark index dirty. The buffer is freed by CacheFileIOManager when
      // nullptr is passed as the listener and the call doesn't fail
      // synchronously.
      rv = CacheFileIOManager::Write(mIndexHandle, 0,
                                     reinterpret_cast<char *>(hdr),
                                     sizeof(CacheIndexHeader), true, nullptr);
      if (NS_FAILED(rv)) {
        // This is not fatal, just free the memory
        free(hdr);
      }
    }

    pos += sizeof(CacheIndexHeader);
  }

  uint32_t hashOffset = pos;

  while (pos + sizeof(CacheIndexRecord) <= mRWBufPos &&
         mSkipEntries != entryCnt) {
    CacheIndexRecord *rec = reinterpret_cast<CacheIndexRecord *>(mRWBuf + pos);
    CacheIndexEntry tmpEntry(&rec->mHash);
    tmpEntry.ReadFromBuf(mRWBuf + pos);

    if (tmpEntry.IsDirty() || !tmpEntry.IsInitialized() ||
        tmpEntry.IsFileEmpty() || tmpEntry.IsFresh() || tmpEntry.IsRemoved()) {
      LOG(("CacheIndex::ParseRecords() - Invalid entry found in index, removing"
           " whole index [dirty=%d, initialized=%d, fileEmpty=%d, fresh=%d, "
           "removed=%d]", tmpEntry.IsDirty(), tmpEntry.IsInitialized(),
           tmpEntry.IsFileEmpty(), tmpEntry.IsFresh(), tmpEntry.IsRemoved()));
      FinishRead(false);
      return;
    }

    CacheIndexEntryAutoManage emng(tmpEntry.Hash(), this);

    CacheIndexEntry *entry = mIndex.PutEntry(*tmpEntry.Hash());
    *entry = tmpEntry;

    pos += sizeof(CacheIndexRecord);
    mSkipEntries++;
  }

  mRWHash->Update(mRWBuf + hashOffset, pos - hashOffset);

  if (pos != mRWBufPos) {
    memmove(mRWBuf, mRWBuf + pos, mRWBufPos - pos);
    mRWBufPos -= pos;
    pos = 0;
  }

  int64_t fileOffset = sizeof(CacheIndexHeader) +
                       mSkipEntries * sizeof(CacheIndexRecord) + mRWBufPos;

  MOZ_ASSERT(fileOffset <= mIndexHandle->FileSize());
  if (fileOffset == mIndexHandle->FileSize()) {
    if (mRWHash->GetHash() != NetworkEndian::readUint32(mRWBuf)) {
      LOG(("CacheIndex::ParseRecords() - Hash mismatch, [is %x, should be %x]",
           mRWHash->GetHash(),
           NetworkEndian::readUint32(mRWBuf)));
      FinishRead(false);
      return;
    }

    mIndexOnDiskIsValid = true;
    mJournalReadSuccessfully = false;

    if (mJournalHandle) {
      StartReadingJournal();
    } else {
      FinishRead(false);
    }

    return;
  }

  pos = mRWBufPos;
  uint32_t toRead = std::min(mRWBufSize - pos,
                             static_cast<uint32_t>(mIndexHandle->FileSize() -
                                                   fileOffset));
  mRWBufPos = pos + toRead;

  rv = CacheFileIOManager::Read(mIndexHandle, fileOffset, mRWBuf + pos, toRead,
                                this);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::ParseRecords() - CacheFileIOManager::Read() failed "
         "synchronously [rv=0x%08x]", rv));
    FinishRead(false);
    return;
  }
}

void
CacheIndex::StartReadingJournal()
{
  LOG(("CacheIndex::StartReadingJournal()"));

  nsresult rv;

  AssertOwnsLock();

  MOZ_ASSERT(mJournalHandle);
  MOZ_ASSERT(mIndexOnDiskIsValid);
  MOZ_ASSERT(mTmpJournal.Count() == 0);
  MOZ_ASSERT(mJournalHandle->FileSize() >= 0);

  int64_t entriesSize = mJournalHandle->FileSize() -
                        sizeof(CacheHash::Hash32_t);

  if (entriesSize < 0 || entriesSize % sizeof(CacheIndexRecord)) {
    LOG(("CacheIndex::StartReadingJournal() - Journal is corrupted"));
    FinishRead(false);
    return;
  }

  mSkipEntries = 0;
  mRWHash = new CacheHash();

  mRWBufPos = std::min(mRWBufSize,
                       static_cast<uint32_t>(mJournalHandle->FileSize()));

  rv = CacheFileIOManager::Read(mJournalHandle, 0, mRWBuf, mRWBufPos, this);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::StartReadingJournal() - CacheFileIOManager::Read() failed"
         " synchronously [rv=0x%08x]", rv));
    FinishRead(false);
  }
}

void
CacheIndex::ParseJournal()
{
  LOG(("CacheIndex::ParseRecords()"));

  nsresult rv;

  AssertOwnsLock();

  uint32_t entryCnt = (mJournalHandle->FileSize() -
                       sizeof(CacheHash::Hash32_t)) / sizeof(CacheIndexRecord);

  uint32_t pos = 0;

  while (pos + sizeof(CacheIndexRecord) <= mRWBufPos &&
         mSkipEntries != entryCnt) {
    CacheIndexRecord *rec = reinterpret_cast<CacheIndexRecord *>(mRWBuf + pos);
    CacheIndexEntry tmpEntry(&rec->mHash);
    tmpEntry.ReadFromBuf(mRWBuf + pos);

    CacheIndexEntry *entry = mTmpJournal.PutEntry(*tmpEntry.Hash());
    *entry = tmpEntry;

    if (entry->IsDirty() || entry->IsFresh()) {
      LOG(("CacheIndex::ParseJournal() - Invalid entry found in journal, "
           "ignoring whole journal [dirty=%d, fresh=%d]", entry->IsDirty(),
           entry->IsFresh()));
      FinishRead(false);
      return;
    }

    pos += sizeof(CacheIndexRecord);
    mSkipEntries++;
  }

  mRWHash->Update(mRWBuf, pos);

  if (pos != mRWBufPos) {
    memmove(mRWBuf, mRWBuf + pos, mRWBufPos - pos);
    mRWBufPos -= pos;
    pos = 0;
  }

  int64_t fileOffset = mSkipEntries * sizeof(CacheIndexRecord) + mRWBufPos;

  MOZ_ASSERT(fileOffset <= mJournalHandle->FileSize());
  if (fileOffset == mJournalHandle->FileSize()) {
    if (mRWHash->GetHash() != NetworkEndian::readUint32(mRWBuf)) {
      LOG(("CacheIndex::ParseJournal() - Hash mismatch, [is %x, should be %x]",
           mRWHash->GetHash(),
           NetworkEndian::readUint32(mRWBuf)));
      FinishRead(false);
      return;
    }

    mJournalReadSuccessfully = true;
    FinishRead(true);
    return;
  }

  pos = mRWBufPos;
  uint32_t toRead = std::min(mRWBufSize - pos,
                             static_cast<uint32_t>(mJournalHandle->FileSize() -
                                                   fileOffset));
  mRWBufPos = pos + toRead;

  rv = CacheFileIOManager::Read(mJournalHandle, fileOffset, mRWBuf + pos,
                                toRead, this);
  if (NS_FAILED(rv)) {
    LOG(("CacheIndex::ParseJournal() - CacheFileIOManager::Read() failed "
         "synchronously [rv=0x%08x]", rv));
    FinishRead(false);
    return;
  }
}

void
CacheIndex::MergeJournal()
{
  LOG(("CacheIndex::MergeJournal()"));

  AssertOwnsLock();

  mTmpJournal.EnumerateEntries(&CacheIndex::ProcessJournalEntry, this);

  MOZ_ASSERT(mTmpJournal.Count() == 0);
}

// static
PLDHashOperator
CacheIndex::ProcessJournalEntry(CacheIndexEntry *aEntry, void* aClosure)
{
  CacheIndex *index = static_cast<CacheIndex *>(aClosure);

  LOG(("CacheIndex::ProcessJournalEntry() [hash=%08x%08x%08x%08x%08x]",
       LOGSHA1(aEntry->Hash())));

  CacheIndexEntry *entry = index->mIndex.GetEntry(*aEntry->Hash());

  CacheIndexEntryAutoManage emng(aEntry->Hash(), index);

  if (aEntry->IsRemoved()) {
    if (entry) {
      entry->MarkRemoved();
      entry->MarkDirty();
    }
  } else {
    if (!entry) {
      entry = index->mIndex.PutEntry(*aEntry->Hash());
    }

    *entry = *aEntry;
    entry->MarkDirty();
  }

  return PL_DHASH_REMOVE;
}

void
CacheIndex::EnsureNoFreshEntry()
{
#ifdef DEBUG_STATS
  CacheIndexStats debugStats;
  debugStats.DisableLogging();
  mIndex.EnumerateEntries(&CacheIndex::SumIndexStats, &debugStats);
  MOZ_ASSERT(debugStats.Fresh() == 0);
#endif
}

void
CacheIndex::EnsureCorrectStats()
{
#ifdef DEBUG_STATS
  MOZ_ASSERT(mPendingUpdates.Count() == 0);
  CacheIndexStats debugStats;
  debugStats.DisableLogging();
  mIndex.EnumerateEntries(&CacheIndex::SumIndexStats, &debugStats);
  MOZ_ASSERT(debugStats == mIndexStats);
#endif
}

// static
PLDHashOperator
CacheIndex::SumIndexStats(CacheIndexEntry *aEntry, void* aClosure)
{
  CacheIndexStats *stats = static_cast<CacheIndexStats *>(aClosure);
  stats->BeforeChange(nullptr);
  stats->AfterChange(aEntry);
  return PL_DHASH_NEXT;
}

void
CacheIndex::FinishRead(bool aSucceeded)
{
  LOG(("CacheIndex::FinishRead() [succeeded=%d]", aSucceeded));
  AssertOwnsLock();

  MOZ_ASSERT((!aSucceeded && mState == SHUTDOWN) || mState == READING);

  MOZ_ASSERT(
    // -> rebuild
    (!aSucceeded && !mIndexOnDiskIsValid && !mJournalReadSuccessfully) ||
    // -> update
    (!aSucceeded && mIndexOnDiskIsValid && !mJournalReadSuccessfully) ||
    // -> ready
    (aSucceeded && mIndexOnDiskIsValid && mJournalReadSuccessfully));

  if (mState == SHUTDOWN) {
    RemoveFile(NS_LITERAL_CSTRING(kTempIndexName));
    RemoveFile(NS_LITERAL_CSTRING(kJournalName));
  } else {
    if (mIndexHandle && !mIndexOnDiskIsValid) {
      CacheFileIOManager::DoomFile(mIndexHandle, nullptr);
    }

    if (mJournalHandle) {
      CacheFileIOManager::DoomFile(mJournalHandle, nullptr);
    }
  }

  if (mIndexFileOpener) {
    mIndexFileOpener->Cancel();
    mIndexFileOpener = nullptr;
  }
  if (mJournalFileOpener) {
    mJournalFileOpener->Cancel();
    mJournalFileOpener = nullptr;
  }
  if (mTmpFileOpener) {
    mTmpFileOpener->Cancel();
    mTmpFileOpener = nullptr;
  }

  mIndexHandle = nullptr;
  mJournalHandle = nullptr;
  mRWHash = nullptr;
  ReleaseBuffer();

  if (mState == SHUTDOWN) {
    return;
  }

  if (!mIndexOnDiskIsValid) {
    MOZ_ASSERT(mTmpJournal.Count() == 0);
    EnsureNoFreshEntry();
    ProcessPendingOperations();
    // Remove all entries that we haven't seen during this session
    mIndex.EnumerateEntries(&CacheIndex::RemoveNonFreshEntries, this);
    StartUpdatingIndex(true);
    return;
  }

  if (!mJournalReadSuccessfully) {
    mTmpJournal.Clear();
    EnsureNoFreshEntry();
    ProcessPendingOperations();
    StartUpdatingIndex(false);
    return;
  }

  MergeJournal();
  EnsureNoFreshEntry();
  ProcessPendingOperations();
  mIndexStats.Log();

  ChangeState(READY);
  mLastDumpTime = TimeStamp::NowLoRes(); // Do not dump new index immediately
}

// static
void
CacheIndex::DelayedUpdate(nsITimer *aTimer, void *aClosure)
{
  LOG(("CacheIndex::DelayedUpdate()"));

  nsresult rv;
  nsRefPtr<CacheIndex> index = gInstance;

  if (!index) {
    return;
  }

  CacheIndexAutoLock lock(index);

  index->mUpdateTimer = nullptr;

  if (!index->IsIndexUsable()) {
    return;
  }

  if (index->mState == READY && index->mShuttingDown) {
    return;
  }

  // mUpdateEventPending must be false here since StartUpdatingIndex() won't
  // schedule timer if it is true.
  MOZ_ASSERT(!index->mUpdateEventPending);
  if (index->mState != BUILDING && index->mState != UPDATING) {
    LOG(("CacheIndex::DelayedUpdate() - Update was canceled"));
    return;
  }

  // We need to redispatch to run with lower priority
  nsRefPtr<CacheIOThread> ioThread = CacheFileIOManager::IOThread();
  MOZ_ASSERT(ioThread);

  index->mUpdateEventPending = true;
  rv = ioThread->Dispatch(index, CacheIOThread::INDEX);
  if (NS_FAILED(rv)) {
    index->mUpdateEventPending = false;
    NS_WARNING("CacheIndex::DelayedUpdate() - Can't dispatch event");
    LOG(("CacheIndex::DelayedUpdate() - Can't dispatch event" ));
    index->FinishUpdate(false);
  }
}

nsresult
CacheIndex::ScheduleUpdateTimer(uint32_t aDelay)
{
  LOG(("CacheIndex::ScheduleUpdateTimer() [delay=%u]", aDelay));

  MOZ_ASSERT(!mUpdateTimer);

  nsresult rv;

  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIEventTarget> ioTarget = CacheFileIOManager::IOTarget();
  MOZ_ASSERT(ioTarget);

  rv = timer->SetTarget(ioTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = timer->InitWithFuncCallback(CacheIndex::DelayedUpdate, nullptr,
                                   aDelay, nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  mUpdateTimer.swap(timer);
  return NS_OK;
}

nsresult
CacheIndex::SetupDirectoryEnumerator()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mDirEnumerator);

  nsresult rv;
  nsCOMPtr<nsIFile> file;

  rv = mCacheDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING(kEntriesDir));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    NS_WARNING("CacheIndex::SetupDirectoryEnumerator() - Entries directory "
               "doesn't exist!");
    LOG(("CacheIndex::SetupDirectoryEnumerator() - Entries directory doesn't "
          "exist!" ));
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = file->GetDirectoryEntries(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  mDirEnumerator = do_QueryInterface(enumerator, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
CacheIndex::InitEntryFromDiskData(CacheIndexEntry *aEntry,
                                  CacheFileMetadata *aMetaData,
                                  int64_t aFileSize)
{
  aEntry->InitNew();
  aEntry->MarkDirty();
  aEntry->MarkFresh();
  aEntry->Init(aMetaData->AppId(), aMetaData->IsAnonymous(),
               aMetaData->IsInBrowser());

  uint32_t expirationTime;
  aMetaData->GetExpirationTime(&expirationTime);
  aEntry->SetExpirationTime(expirationTime);

  uint32_t frecency;
  aMetaData->GetFrecency(&frecency);
  aEntry->SetFrecency(frecency);

  aEntry->SetFileSize(static_cast<uint32_t>(
                        std::min(static_cast<int64_t>(PR_UINT32_MAX),
                                 (aFileSize + 0x3FF) >> 10)));
}

bool
CacheIndex::IsUpdatePending()
{
  AssertOwnsLock();

  if (mUpdateTimer || mUpdateEventPending) {
    return true;
  }

  return false;
}

void
CacheIndex::BuildIndex()
{
  LOG(("CacheIndex::BuildIndex()"));

  AssertOwnsLock();

  MOZ_ASSERT(mPendingUpdates.Count() == 0);

  nsresult rv;

  if (!mDirEnumerator) {
    {
      // Do not do IO under the lock.
      CacheIndexAutoUnlock unlock(this);
      rv = SetupDirectoryEnumerator();
    }
    if (mState == SHUTDOWN) {
      // The index was shut down while we released the lock. FinishUpdate() was
      // already called from Shutdown(), so just simply return here.
      return;
    }

    if (NS_FAILED(rv)) {
      FinishUpdate(false);
      return;
    }
  }

  while (true) {
    if (CacheIOThread::YieldAndRerun()) {
      LOG(("CacheIndex::BuildIndex() - Breaking loop for higher level events."));
      mUpdateEventPending = true;
      return;
    }

    nsCOMPtr<nsIFile> file;
    {
      // Do not do IO under the lock.
      CacheIndexAutoUnlock unlock(this);
      rv = mDirEnumerator->GetNextFile(getter_AddRefs(file));
    }
    if (mState == SHUTDOWN) {
      return;
    }
    if (!file) {
      FinishUpdate(NS_SUCCEEDED(rv));
      return;
    }

    nsAutoCString leaf;
    rv = file->GetNativeLeafName(leaf);
    if (NS_FAILED(rv)) {
      LOG(("CacheIndex::BuildIndex() - GetNativeLeafName() failed! Skipping "
           "file."));
      mDontMarkIndexClean = true;
      continue;
    }

    SHA1Sum::Hash hash;
    rv = CacheFileIOManager::StrToHash(leaf, &hash);
    if (NS_FAILED(rv)) {
      LOG(("CacheIndex::BuildIndex() - Filename is not a hash, removing file. "
           "[name=%s]", leaf.get()));
      file->Remove(false);
      continue;
    }

    CacheIndexEntry *entry = mIndex.GetEntry(hash);
    if (entry && entry->IsRemoved()) {
      LOG(("CacheIndex::BuildIndex() - Found file that should not exist. "
           "[name=%s]", leaf.get()));
      entry->Log();
      MOZ_ASSERT(entry->IsFresh());
      entry = nullptr;
    }

#ifdef DEBUG
    nsRefPtr<CacheFileHandle> handle;
    CacheFileIOManager::gInstance->mHandles.GetHandle(&hash, false,
                                                      getter_AddRefs(handle));
#endif

    if (entry) {
      // the entry is up to date
      LOG(("CacheIndex::BuildIndex() - Skipping file because the entry is up to"
           " date. [name=%s]", leaf.get()));
      entry->Log();
      MOZ_ASSERT(entry->IsFresh()); // The entry must be from this session
      // there must be an active CacheFile if the entry is not initialized
      MOZ_ASSERT(entry->IsInitialized() || handle);
      continue;
    }

    MOZ_ASSERT(!handle);

    nsRefPtr<CacheFileMetadata> meta = new CacheFileMetadata();
    int64_t size = 0;

    {
      // Do not do IO under the lock.
      CacheIndexAutoUnlock unlock(this);
      rv = meta->SyncReadMetadata(file);

      if (NS_SUCCEEDED(rv)) {
        rv = file->GetFileSize(&size);
        if (NS_FAILED(rv)) {
          LOG(("CacheIndex::BuildIndex() - Cannot get filesize of file that was"
               " successfully parsed. [name=%s]", leaf.get()));
        }
      }
    }
    if (mState == SHUTDOWN) {
      return;
    }

    // Nobody could add the entry while the lock was released since we modify
    // the index only on IO thread and this loop is executed on IO thread too.
    entry = mIndex.GetEntry(hash);
    MOZ_ASSERT(!entry || entry->IsRemoved());

    if (NS_FAILED(rv)) {
      LOG(("CacheIndex::BuildIndex() - CacheFileMetadata::SyncReadMetadata() "
           "failed, removing file. [name=%s]", leaf.get()));
      file->Remove(false);
    } else {
      CacheIndexEntryAutoManage entryMng(&hash, this);
      entry = mIndex.PutEntry(hash);
      InitEntryFromDiskData(entry, meta, size);
      LOG(("CacheIndex::BuildIndex() - Added entry to index. [hash=%s]",
           leaf.get()));
      entry->Log();
    }
  }

  NS_NOTREACHED("We should never get here");
}

bool
CacheIndex::StartUpdatingIndexIfNeeded(bool aSwitchingToReadyState)
{
  // Start updating process when we are in or we are switching to READY state
  // and index needs update, but not during shutdown or when removing all
  // entries.
  if ((mState == READY || aSwitchingToReadyState) && mIndexNeedsUpdate &&
      !mShuttingDown && !mRemovingAll) {
    LOG(("CacheIndex::StartUpdatingIndexIfNeeded() - starting update process"));
    mIndexNeedsUpdate = false;
    StartUpdatingIndex(false);
    return true;
  }

  return false;
}

void
CacheIndex::StartUpdatingIndex(bool aRebuild)
{
  LOG(("CacheIndex::StartUpdatingIndex() [rebuild=%d]", aRebuild));

  AssertOwnsLock();

  nsresult rv;

  mIndexStats.Log();

  ChangeState(aRebuild ? BUILDING : UPDATING);
  mDontMarkIndexClean = false;

  if (mShuttingDown || mRemovingAll) {
    FinishUpdate(false);
    return;
  }

  if (IsUpdatePending()) {
    LOG(("CacheIndex::StartUpdatingIndex() - Update is already pending"));
    return;
  }

  uint32_t elapsed = (TimeStamp::NowLoRes() - mStartTime).ToMilliseconds();
  if (elapsed < kUpdateIndexStartDelay) {
    LOG(("CacheIndex::StartUpdatingIndex() - %u ms elapsed since startup, "
         "scheduling timer to fire in %u ms.", elapsed,
         kUpdateIndexStartDelay - elapsed));
    rv = ScheduleUpdateTimer(kUpdateIndexStartDelay - elapsed);
    if (NS_SUCCEEDED(rv)) {
      return;
    }

    LOG(("CacheIndex::StartUpdatingIndex() - ScheduleUpdateTimer() failed. "
         "Starting update immediately."));
  } else {
    LOG(("CacheIndex::StartUpdatingIndex() - %u ms elapsed since startup, "
         "starting update now.", elapsed));
  }

  nsRefPtr<CacheIOThread> ioThread = CacheFileIOManager::IOThread();
  MOZ_ASSERT(ioThread);

  // We need to dispatch an event even if we are on IO thread since we need to
  // update the index with the correct priority.
  mUpdateEventPending = true;
  rv = ioThread->Dispatch(this, CacheIOThread::INDEX);
  if (NS_FAILED(rv)) {
    mUpdateEventPending = false;
    NS_WARNING("CacheIndex::StartUpdatingIndex() - Can't dispatch event");
    LOG(("CacheIndex::StartUpdatingIndex() - Can't dispatch event" ));
    FinishUpdate(false);
  }
}

void
CacheIndex::UpdateIndex()
{
  LOG(("CacheIndex::UpdateIndex()"));

  AssertOwnsLock();

  MOZ_ASSERT(mPendingUpdates.Count() == 0);

  nsresult rv;

  if (!mDirEnumerator) {
    {
      // Do not do IO under the lock.
      CacheIndexAutoUnlock unlock(this);
      rv = SetupDirectoryEnumerator();
    }
    if (mState == SHUTDOWN) {
      // The index was shut down while we released the lock. FinishUpdate() was
      // already called from Shutdown(), so just simply return here.
      return;
    }

    if (NS_FAILED(rv)) {
      FinishUpdate(false);
      return;
    }
  }

  while (true) {
    if (CacheIOThread::YieldAndRerun()) {
      LOG(("CacheIndex::UpdateIndex() - Breaking loop for higher level "
           "events."));
      mUpdateEventPending = true;
      return;
    }

    nsCOMPtr<nsIFile> file;
    {
      // Do not do IO under the lock.
      CacheIndexAutoUnlock unlock(this);
      rv = mDirEnumerator->GetNextFile(getter_AddRefs(file));
    }
    if (mState == SHUTDOWN) {
      return;
    }
    if (!file) {
      FinishUpdate(NS_SUCCEEDED(rv));
      return;
    }

    nsAutoCString leaf;
    rv = file->GetNativeLeafName(leaf);
    if (NS_FAILED(rv)) {
      LOG(("CacheIndex::UpdateIndex() - GetNativeLeafName() failed! Skipping "
           "file."));
      mDontMarkIndexClean = true;
      continue;
    }

    SHA1Sum::Hash hash;
    rv = CacheFileIOManager::StrToHash(leaf, &hash);
    if (NS_FAILED(rv)) {
      LOG(("CacheIndex::UpdateIndex() - Filename is not a hash, removing file. "
           "[name=%s]", leaf.get()));
      file->Remove(false);
      continue;
    }

    CacheIndexEntry *entry = mIndex.GetEntry(hash);
    if (entry && entry->IsRemoved()) {
      if (entry->IsFresh()) {
        LOG(("CacheIndex::UpdateIndex() - Found file that should not exist. "
             "[name=%s]", leaf.get()));
        entry->Log();
      }
      entry = nullptr;
    }

#ifdef DEBUG
    nsRefPtr<CacheFileHandle> handle;
    CacheFileIOManager::gInstance->mHandles.GetHandle(&hash, false,
                                                      getter_AddRefs(handle));
#endif

    if (entry && entry->IsFresh()) {
      // the entry is up to date
      LOG(("CacheIndex::UpdateIndex() - Skipping file because the entry is up "
           " to date. [name=%s]", leaf.get()));
      entry->Log();
      // there must be an active CacheFile if the entry is not initialized
      MOZ_ASSERT(entry->IsInitialized() || handle);
      continue;
    }

    MOZ_ASSERT(!handle);

    if (entry) {
      PRTime lastModifiedTime;
      {
        // Do not do IO under the lock.
        CacheIndexAutoUnlock unlock(this);
        rv = file->GetLastModifiedTime(&lastModifiedTime);
      }
      if (mState == SHUTDOWN) {
        return;
      }
      if (NS_FAILED(rv)) {
        LOG(("CacheIndex::UpdateIndex() - Cannot get lastModifiedTime. "
             "[name=%s]", leaf.get()));
        // Assume the file is newer than index
      } else {
        if (mIndexTimeStamp > (lastModifiedTime / PR_MSEC_PER_SEC)) {
          LOG(("CacheIndex::UpdateIndex() - Skipping file because of last "
               "modified time. [name=%s, indexTimeStamp=%u, "
               "lastModifiedTime=%u]", leaf.get(), mIndexTimeStamp,
               lastModifiedTime / PR_MSEC_PER_SEC));

          CacheIndexEntryAutoManage entryMng(&hash, this);
          entry->MarkFresh();
          continue;
        }
      }
    }

    nsRefPtr<CacheFileMetadata> meta = new CacheFileMetadata();
    int64_t size = 0;

    {
      // Do not do IO under the lock.
      CacheIndexAutoUnlock unlock(this);
      rv = meta->SyncReadMetadata(file);

      if (NS_SUCCEEDED(rv)) {
        rv = file->GetFileSize(&size);
        if (NS_FAILED(rv)) {
          LOG(("CacheIndex::UpdateIndex() - Cannot get filesize of file that "
               "was successfully parsed. [name=%s]", leaf.get()));
        }
      }
    }
    if (mState == SHUTDOWN) {
      return;
    }

    // Nobody could add the entry while the lock was released since we modify
    // the index only on IO thread and this loop is executed on IO thread too.
    entry = mIndex.GetEntry(hash);
    MOZ_ASSERT(!entry || !entry->IsFresh());

    CacheIndexEntryAutoManage entryMng(&hash, this);

    if (NS_FAILED(rv)) {
      LOG(("CacheIndex::UpdateIndex() - CacheFileMetadata::SyncReadMetadata() "
           "failed, removing file. [name=%s]", leaf.get()));
      file->Remove(false);
      if (entry) {
        entry->MarkRemoved();
        entry->MarkFresh();
        entry->MarkDirty();
      }
    } else {
      entry = mIndex.PutEntry(hash);
      InitEntryFromDiskData(entry, meta, size);
      LOG(("CacheIndex::UpdateIndex() - Added/updated entry to/in index. "
           "[hash=%s]", leaf.get()));
      entry->Log();
    }
  }

  NS_NOTREACHED("We should never get here");
}

void
CacheIndex::FinishUpdate(bool aSucceeded)
{
  LOG(("CacheIndex::FinishUpdate() [succeeded=%d]", aSucceeded));

  MOZ_ASSERT(mState == UPDATING || mState == BUILDING ||
             (!aSucceeded && mState == SHUTDOWN));

  AssertOwnsLock();

  if (mDirEnumerator) {
    if (NS_IsMainThread()) {
      LOG(("CacheIndex::FinishUpdate() - posting of PreShutdownInternal failed?"
           " Cannot safely release mDirEnumerator, leaking it!"));
      NS_WARNING(("CacheIndex::FinishUpdate() - Leaking mDirEnumerator!"));
      // This can happen only in case dispatching event to IO thread failed in
      // CacheIndex::PreShutdown().
      mDirEnumerator.forget(); // Leak it since dir enumerator is not threadsafe
    } else {
      mDirEnumerator->Close();
      mDirEnumerator = nullptr;
    }
  }

  if (!aSucceeded) {
    mDontMarkIndexClean = true;
  }

  if (mState == SHUTDOWN) {
    return;
  }

  if (mState == UPDATING && aSucceeded) {
    // If we've iterated over all entries successfully then all entries that
    // really exist on the disk are now marked as fresh. All non-fresh entries
    // don't exist anymore and must be removed from the index.
    mIndex.EnumerateEntries(&CacheIndex::RemoveNonFreshEntries, this);
  }

  // Make sure we won't start update. If the build or update failed, there is no
  // reason to believe that it will succeed next time.
  mIndexNeedsUpdate = false;

  ChangeState(READY);
  mLastDumpTime = TimeStamp::NowLoRes(); // Do not dump new index immediately
}

// static
PLDHashOperator
CacheIndex::RemoveNonFreshEntries(CacheIndexEntry *aEntry, void* aClosure)
{
  if (aEntry->IsFresh()) {
    return PL_DHASH_NEXT;
  }

  LOG(("CacheFile::RemoveNonFreshEntries() - Removing entry. "
       "[hash=%08x%08x%08x%08x%08x]", LOGSHA1(aEntry->Hash())));

  CacheIndex *index = static_cast<CacheIndex *>(aClosure);

  CacheIndexEntryAutoManage emng(aEntry->Hash(), index);
  emng.DoNotSearchInIndex();

  return PL_DHASH_REMOVE;
}

#ifdef PR_LOGGING
// static
char const *
CacheIndex::StateString(EState aState)
{
  switch (aState) {
    case INITIAL:  return "INITIAL";
    case READING:  return "READING";
    case WRITING:  return "WRITING";
    case BUILDING: return "BUILDING";
    case UPDATING: return "UPDATING";
    case READY:    return "READY";
    case SHUTDOWN: return "SHUTDOWN";
  }

  MOZ_ASSERT(false, "Unexpected state!");
  return "?";
}
#endif

void
CacheIndex::ChangeState(EState aNewState)
{
  LOG(("CacheIndex::ChangeState() changing state %s -> %s", StateString(mState),
       StateString(aNewState)));

  // All pending updates should be processed before changing state
  MOZ_ASSERT(mPendingUpdates.Count() == 0);

  // PreShutdownInternal() should change the state to READY from every state. It
  // may go through different states, but once we are in READY state the only
  // possible transition is to SHUTDOWN state.
  MOZ_ASSERT(!mShuttingDown || mState != READY || aNewState == SHUTDOWN);

  // Start updating process when switching to READY state if needed
  if (aNewState == READY && StartUpdatingIndexIfNeeded(true)) {
    return;
  }

  // Try to evict entries over limit everytime we're leaving state READING,
  // BUILDING or UPDATING, but not during shutdown or when removing all
  // entries.
  if (!mShuttingDown && !mRemovingAll && aNewState != SHUTDOWN &&
      (mState == READING || mState == BUILDING || mState == UPDATING))  {
    CacheFileIOManager::EvictIfOverLimit();
  }

  mState = aNewState;

  if (mState != SHUTDOWN) {
    CacheFileIOManager::CacheIndexStateChanged();
  }

  if (mState == READY && mDiskConsumptionObservers.Length()) {
    for (uint32_t i = 0; i < mDiskConsumptionObservers.Length(); ++i) {
      DiskConsumptionObserver* o = mDiskConsumptionObservers[i];
      // Safe to call under the lock.  We always post to the main thread.
      o->OnDiskConsumption(mIndexStats.Size() << 10);
    }

    mDiskConsumptionObservers.Clear();
  }
}

void
CacheIndex::AllocBuffer()
{
  switch (mState) {
    case WRITING:
      mRWBufSize = sizeof(CacheIndexHeader) + sizeof(CacheHash::Hash32_t) +
                   mProcessEntries * sizeof(CacheIndexRecord);
      if (mRWBufSize > kMaxBufSize) {
        mRWBufSize = kMaxBufSize;
      }
      break;
    case READING:
      mRWBufSize = kMaxBufSize;
      break;
    default:
      MOZ_ASSERT(false, "Unexpected state!");
  }

  mRWBuf = static_cast<char *>(moz_xmalloc(mRWBufSize));
}

void
CacheIndex::ReleaseBuffer()
{
  if (!mRWBuf) {
    return;
  }

  free(mRWBuf);
  mRWBuf = nullptr;
  mRWBufSize = 0;
  mRWBufPos = 0;
}

namespace { // anon

class FrecencyComparator
{
public:
  bool Equals(CacheIndexRecord* a, CacheIndexRecord* b) const {
    return a->mFrecency == b->mFrecency;
  }
  bool LessThan(CacheIndexRecord* a, CacheIndexRecord* b) const {
    // Place entries with frecency 0 at the end of the array.
    if (a->mFrecency == 0) {
      return false;
    }
    if (b->mFrecency == 0) {
      return true;
    }
    return a->mFrecency < b->mFrecency;
  }
};

class ExpirationComparator
{
public:
  bool Equals(CacheIndexRecord* a, CacheIndexRecord* b) const {
    return a->mExpirationTime == b->mExpirationTime;
  }
  bool LessThan(CacheIndexRecord* a, CacheIndexRecord* b) const {
    return a->mExpirationTime < b->mExpirationTime;
  }
};

} // anon

void
CacheIndex::InsertRecordToFrecencyArray(CacheIndexRecord *aRecord)
{
  LOG(("CacheIndex::InsertRecordToFrecencyArray() [record=%p, hash=%08x%08x%08x"
       "%08x%08x]", aRecord, LOGSHA1(aRecord->mHash)));

  MOZ_ASSERT(!mFrecencyArray.Contains(aRecord));
  mFrecencyArray.InsertElementSorted(aRecord, FrecencyComparator());
}

void
CacheIndex::InsertRecordToExpirationArray(CacheIndexRecord *aRecord)
{
  LOG(("CacheIndex::InsertRecordToExpirationArray() [record=%p, hash=%08x%08x"
       "%08x%08x%08x]", aRecord, LOGSHA1(aRecord->mHash)));

  MOZ_ASSERT(!mExpirationArray.Contains(aRecord));
  mExpirationArray.InsertElementSorted(aRecord, ExpirationComparator());
}

void
CacheIndex::RemoveRecordFromFrecencyArray(CacheIndexRecord *aRecord)
{
  LOG(("CacheIndex::RemoveRecordFromFrecencyArray() [record=%p]", aRecord));

  DebugOnly<bool> removed;
  removed = mFrecencyArray.RemoveElement(aRecord);
  MOZ_ASSERT(removed);
}

void
CacheIndex::RemoveRecordFromExpirationArray(CacheIndexRecord *aRecord)
{
  LOG(("CacheIndex::RemoveRecordFromExpirationArray() [record=%p]", aRecord));

  DebugOnly<bool> removed;
  removed = mExpirationArray.RemoveElement(aRecord);
  MOZ_ASSERT(removed);
}

void
CacheIndex::AddRecordToIterators(CacheIndexRecord *aRecord)
{
  AssertOwnsLock();

  for (uint32_t i = 0; i < mIterators.Length(); ++i) {
    // Add a new record only when iterator is supposed to be updated.
    if (mIterators[i]->ShouldBeNewAdded()) {
      mIterators[i]->AddRecord(aRecord);
    }
  }
}

void
CacheIndex::RemoveRecordFromIterators(CacheIndexRecord *aRecord)
{
  AssertOwnsLock();

  for (uint32_t i = 0; i < mIterators.Length(); ++i) {
    // Remove the record from iterator always, it makes no sence to return
    // non-existing entries. Also the pointer to the record is no longer valid
    // once the entry is removed from index.
    mIterators[i]->RemoveRecord(aRecord);
  }
}

void
CacheIndex::ReplaceRecordInIterators(CacheIndexRecord *aOldRecord,
                                     CacheIndexRecord *aNewRecord)
{
  AssertOwnsLock();

  for (uint32_t i = 0; i < mIterators.Length(); ++i) {
    // We have to replace the record always since the pointer is no longer
    // valid after this point. NOTE: Replacing the record doesn't mean that
    // a new entry was added, it just means that the data in the entry was
    // changed (e.g. a file size) and we had to track this change in
    // mPendingUpdates since mIndex was read-only.
    mIterators[i]->ReplaceRecord(aOldRecord, aNewRecord);
  }
}

nsresult
CacheIndex::Run()
{
  LOG(("CacheIndex::Run()"));

  CacheIndexAutoLock lock(this);

  if (!IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mState == READY && mShuttingDown) {
    return NS_OK;
  }

  mUpdateEventPending = false;

  switch (mState) {
    case BUILDING:
      BuildIndex();
      break;
    case UPDATING:
      UpdateIndex();
      break;
    default:
      LOG(("CacheIndex::Run() - Update/Build was canceled"));
  }

  return NS_OK;
}

nsresult
CacheIndex::OnFileOpenedInternal(FileOpenHelper *aOpener,
                                 CacheFileHandle *aHandle, nsresult aResult)
{
  LOG(("CacheIndex::OnFileOpenedInternal() [opener=%p, handle=%p, "
       "result=0x%08x]", aOpener, aHandle, aResult));

  nsresult rv;

  AssertOwnsLock();

  if (!IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mState == READY && mShuttingDown) {
    return NS_OK;
  }

  switch (mState) {
    case WRITING:
      MOZ_ASSERT(aOpener == mIndexFileOpener);
      mIndexFileOpener = nullptr;

      if (NS_FAILED(aResult)) {
        LOG(("CacheIndex::OnFileOpenedInternal() - Can't open index file for "
             "writing [rv=0x%08x]", aResult));
        FinishWrite(false);
      } else {
        mIndexHandle = aHandle;
        WriteRecords();
      }
      break;
    case READING:
      if (aOpener == mIndexFileOpener) {
        mIndexFileOpener = nullptr;

        if (NS_SUCCEEDED(aResult)) {
          if (aHandle->FileSize() == 0) {
            FinishRead(false);
            CacheFileIOManager::DoomFile(aHandle, nullptr);
            break;
          } else {
            mIndexHandle = aHandle;
          }
        } else {
          FinishRead(false);
          break;
        }
      } else if (aOpener == mJournalFileOpener) {
        mJournalFileOpener = nullptr;
        mJournalHandle = aHandle;
      } else if (aOpener == mTmpFileOpener) {
        mTmpFileOpener = nullptr;
        mTmpHandle = aHandle;
      } else {
        MOZ_ASSERT(false, "Unexpected state!");
      }

      if (mIndexFileOpener || mJournalFileOpener || mTmpFileOpener) {
        // Some opener still didn't finish
        break;
      }

      // We fail and cancel all other openers when we opening index file fails.
      MOZ_ASSERT(mIndexHandle);

      if (mTmpHandle) {
        CacheFileIOManager::DoomFile(mTmpHandle, nullptr);
        mTmpHandle = nullptr;

        if (mJournalHandle) { // this shouldn't normally happen
          LOG(("CacheIndex::OnFileOpenedInternal() - Unexpected state, all "
               "files [%s, %s, %s] should never exist. Removing whole index.",
               kIndexName, kJournalName, kTempIndexName));
          FinishRead(false);
          break;
        }
      }

      if (mJournalHandle) {
        // Rename journal to make sure we update index on next start in case
        // firefox crashes
        rv = CacheFileIOManager::RenameFile(
          mJournalHandle, NS_LITERAL_CSTRING(kTempIndexName), this);
        if (NS_FAILED(rv)) {
          LOG(("CacheIndex::OnFileOpenedInternal() - CacheFileIOManager::"
               "RenameFile() failed synchronously [rv=0x%08x]", rv));
          FinishRead(false);
          break;
        }
      } else {
        StartReadingIndex();
      }

      break;
    default:
      MOZ_ASSERT(false, "Unexpected state!");
  }

  return NS_OK;
}

nsresult
CacheIndex::OnFileOpened(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheIndex::OnFileOpened should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheIndex::OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                          nsresult aResult)
{
  LOG(("CacheIndex::OnDataWritten() [handle=%p, result=0x%08x]", aHandle,
       aResult));

  nsresult rv;

  CacheIndexAutoLock lock(this);

  if (!IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mState == READY && mShuttingDown) {
    return NS_OK;
  }

  switch (mState) {
    case WRITING:
      if (mIndexHandle != aHandle) {
        LOG(("CacheIndex::OnDataWritten() - ignoring notification since it "
             "belongs to previously canceled operation [state=%d]", mState));
        break;
      }

      if (NS_FAILED(aResult)) {
        FinishWrite(false);
      } else {
        if (mSkipEntries == mProcessEntries) {
          rv = CacheFileIOManager::RenameFile(mIndexHandle,
                                              NS_LITERAL_CSTRING(kIndexName),
                                              this);
          if (NS_FAILED(rv)) {
            LOG(("CacheIndex::OnDataWritten() - CacheFileIOManager::"
                 "RenameFile() failed synchronously [rv=0x%08x]", rv));
            FinishWrite(false);
          }
        } else {
          WriteRecords();
        }
      }
      break;
    default:
      // Writing was canceled.
      LOG(("CacheIndex::OnDataWritten() - ignoring notification since the "
           "operation was previously canceled [state=%d]", mState));
  }

  return NS_OK;
}

nsresult
CacheIndex::OnDataRead(CacheFileHandle *aHandle, char *aBuf, nsresult aResult)
{
  LOG(("CacheIndex::OnDataRead() [handle=%p, result=0x%08x]", aHandle,
       aResult));

  CacheIndexAutoLock lock(this);

  if (!IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  switch (mState) {
    case READING:
      MOZ_ASSERT(mIndexHandle == aHandle || mJournalHandle == aHandle);

      if (NS_FAILED(aResult)) {
        FinishRead(false);
      } else {
        if (!mIndexOnDiskIsValid) {
          ParseRecords();
        } else {
          ParseJournal();
        }
      }
      break;
    default:
      // Reading was canceled.
      LOG(("CacheIndex::OnDataRead() - ignoring notification since the "
           "operation was previously canceled [state=%d]", mState));
  }

  return NS_OK;
}

nsresult
CacheIndex::OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheIndex::OnFileDoomed should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheIndex::OnEOFSet(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheIndex::OnEOFSet should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheIndex::OnFileRenamed(CacheFileHandle *aHandle, nsresult aResult)
{
  LOG(("CacheIndex::OnFileRenamed() [handle=%p, result=0x%08x]", aHandle,
       aResult));

  CacheIndexAutoLock lock(this);

  if (!IsIndexUsable()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mState == READY && mShuttingDown) {
    return NS_OK;
  }

  switch (mState) {
    case WRITING:
      // This is a result of renaming the new index written to tmpfile to index
      // file. This is the last step when writing the index and the whole
      // writing process is successful iff renaming was successful.

      if (mIndexHandle != aHandle) {
        LOG(("CacheIndex::OnFileRenamed() - ignoring notification since it "
             "belongs to previously canceled operation [state=%d]", mState));
        break;
      }

      FinishWrite(NS_SUCCEEDED(aResult));
      break;
    case READING:
      // This is a result of renaming journal file to tmpfile. It is renamed
      // before we start reading index and journal file and it should normally
      // succeed. If it fails give up reading of index.

      if (mJournalHandle != aHandle) {
        LOG(("CacheIndex::OnFileRenamed() - ignoring notification since it "
             "belongs to previously canceled operation [state=%d]", mState));
        break;
      }

      if (NS_FAILED(aResult)) {
        FinishRead(false);
      } else {
        StartReadingIndex();
      }
      break;
    default:
      // Reading/writing was canceled.
      LOG(("CacheIndex::OnFileRenamed() - ignoring notification since the "
           "operation was previously canceled [state=%d]", mState));
  }

  return NS_OK;
}

// Memory reporting

size_t
CacheIndex::SizeOfExcludingThisInternal(mozilla::MallocSizeOf mallocSizeOf) const
{
  CacheIndexAutoLock lock(const_cast<CacheIndex*>(this));

  size_t n = 0;
  nsCOMPtr<nsISizeOf> sizeOf;

  // mIndexHandle and mJournalHandle are reported via SizeOfHandlesRunnable
  // in CacheFileIOManager::SizeOfExcludingThisInternal as part of special
  // handles array.

  sizeOf = do_QueryInterface(mCacheDirectory);
  if (sizeOf) {
    n += sizeOf->SizeOfIncludingThis(mallocSizeOf);
  }

  sizeOf = do_QueryInterface(mUpdateTimer);
  if (sizeOf) {
    n += sizeOf->SizeOfIncludingThis(mallocSizeOf);
  }

  n += mallocSizeOf(mRWBuf);
  n += mallocSizeOf(mRWHash);

  n += mIndex.SizeOfExcludingThis(mallocSizeOf);
  n += mPendingUpdates.SizeOfExcludingThis(mallocSizeOf);
  n += mTmpJournal.SizeOfExcludingThis(mallocSizeOf);

  // mFrecencyArray and mExpirationArray items are reported by
  // mIndex/mPendingUpdates
  n += mFrecencyArray.SizeOfExcludingThis(mallocSizeOf);
  n += mExpirationArray.SizeOfExcludingThis(mallocSizeOf);
  n += mDiskConsumptionObservers.SizeOfExcludingThis(mallocSizeOf);

  return n;
}

// static
size_t
CacheIndex::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
  if (!gInstance)
    return 0;

  return gInstance->SizeOfExcludingThisInternal(mallocSizeOf);
}

// static
size_t
CacheIndex::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
  return mallocSizeOf(gInstance) + SizeOfExcludingThis(mallocSizeOf);
}

} // net
} // mozilla
