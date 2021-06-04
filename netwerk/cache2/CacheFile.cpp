/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFile.h"

#include <algorithm>
#include <utility>

#include "CacheFileChunk.h"
#include "CacheFileInputStream.h"
#include "CacheFileOutputStream.h"
#include "CacheFileUtils.h"
#include "CacheIndex.h"
#include "CacheLog.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "nsComponentManagerUtils.h"
#include "nsICacheEntry.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

// When CACHE_CHUNKS is defined we always cache unused chunks in mCacheChunks.
// When it is not defined, we always release the chunks ASAP, i.e. we cache
// unused chunks only when:
//  - CacheFile is memory-only
//  - CacheFile is still waiting for the handle
//  - the chunk is preloaded

//#define CACHE_CHUNKS

namespace mozilla {
namespace net {

class NotifyCacheFileListenerEvent : public Runnable {
 public:
  NotifyCacheFileListenerEvent(CacheFileListener* aCallback, nsresult aResult,
                               bool aIsNew)
      : Runnable("net::NotifyCacheFileListenerEvent"),
        mCallback(aCallback),
        mRV(aResult),
        mIsNew(aIsNew) {
    LOG(
        ("NotifyCacheFileListenerEvent::NotifyCacheFileListenerEvent() "
         "[this=%p]",
         this));
  }

 protected:
  ~NotifyCacheFileListenerEvent() {
    LOG(
        ("NotifyCacheFileListenerEvent::~NotifyCacheFileListenerEvent() "
         "[this=%p]",
         this));
  }

 public:
  NS_IMETHOD Run() override {
    LOG(("NotifyCacheFileListenerEvent::Run() [this=%p]", this));

    mCallback->OnFileReady(mRV, mIsNew);
    return NS_OK;
  }

 protected:
  nsCOMPtr<CacheFileListener> mCallback;
  nsresult mRV;
  bool mIsNew;
};

class NotifyChunkListenerEvent : public Runnable {
 public:
  NotifyChunkListenerEvent(CacheFileChunkListener* aCallback, nsresult aResult,
                           uint32_t aChunkIdx, CacheFileChunk* aChunk)
      : Runnable("net::NotifyChunkListenerEvent"),
        mCallback(aCallback),
        mRV(aResult),
        mChunkIdx(aChunkIdx),
        mChunk(aChunk) {
    LOG(("NotifyChunkListenerEvent::NotifyChunkListenerEvent() [this=%p]",
         this));
  }

 protected:
  ~NotifyChunkListenerEvent() {
    LOG(("NotifyChunkListenerEvent::~NotifyChunkListenerEvent() [this=%p]",
         this));
  }

 public:
  NS_IMETHOD Run() override {
    LOG(("NotifyChunkListenerEvent::Run() [this=%p]", this));

    mCallback->OnChunkAvailable(mRV, mChunkIdx, mChunk);
    return NS_OK;
  }

 protected:
  nsCOMPtr<CacheFileChunkListener> mCallback;
  nsresult mRV;
  uint32_t mChunkIdx;
  RefPtr<CacheFileChunk> mChunk;
};

class DoomFileHelper : public CacheFileIOListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit DoomFileHelper(CacheFileListener* aListener)
      : mListener(aListener) {}

  NS_IMETHOD OnFileOpened(CacheFileHandle* aHandle, nsresult aResult) override {
    MOZ_CRASH("DoomFileHelper::OnFileOpened should not be called!");
    return NS_ERROR_UNEXPECTED;
  }

  NS_IMETHOD OnDataWritten(CacheFileHandle* aHandle, const char* aBuf,
                           nsresult aResult) override {
    MOZ_CRASH("DoomFileHelper::OnDataWritten should not be called!");
    return NS_ERROR_UNEXPECTED;
  }

  NS_IMETHOD OnDataRead(CacheFileHandle* aHandle, char* aBuf,
                        nsresult aResult) override {
    MOZ_CRASH("DoomFileHelper::OnDataRead should not be called!");
    return NS_ERROR_UNEXPECTED;
  }

  NS_IMETHOD OnFileDoomed(CacheFileHandle* aHandle, nsresult aResult) override {
    if (mListener) mListener->OnFileDoomed(aResult);
    return NS_OK;
  }

  NS_IMETHOD OnEOFSet(CacheFileHandle* aHandle, nsresult aResult) override {
    MOZ_CRASH("DoomFileHelper::OnEOFSet should not be called!");
    return NS_ERROR_UNEXPECTED;
  }

  NS_IMETHOD OnFileRenamed(CacheFileHandle* aHandle,
                           nsresult aResult) override {
    MOZ_CRASH("DoomFileHelper::OnFileRenamed should not be called!");
    return NS_ERROR_UNEXPECTED;
  }

 private:
  virtual ~DoomFileHelper() = default;

  nsCOMPtr<CacheFileListener> mListener;
};

NS_IMPL_ISUPPORTS(DoomFileHelper, CacheFileIOListener)

NS_IMPL_ADDREF(CacheFile)
NS_IMPL_RELEASE(CacheFile)
NS_INTERFACE_MAP_BEGIN(CacheFile)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileChunkListener)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileIOListener)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileMetadataListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports,
                                   mozilla::net::CacheFileChunkListener)
NS_INTERFACE_MAP_END

CacheFile::CacheFile()
    : mLock("CacheFile.mLock"),
      mOpeningFile(false),
      mReady(false),
      mMemoryOnly(false),
      mSkipSizeCheck(false),
      mOpenAsMemoryOnly(false),
      mPinned(false),
      mPriority(false),
      mDataAccessed(false),
      mDataIsDirty(false),
      mWritingMetadata(false),
      mPreloadWithoutInputStreams(true),
      mPreloadChunkCount(0),
      mStatus(NS_OK),
      mDataSize(-1),
      mAltDataOffset(-1),
      mKill(false),
      mOutput(nullptr) {
  LOG(("CacheFile::CacheFile() [this=%p]", this));
}

CacheFile::~CacheFile() {
  LOG(("CacheFile::~CacheFile() [this=%p]", this));

  MutexAutoLock lock(mLock);
  if (!mMemoryOnly && mReady && !mKill) {
    // mReady flag indicates we have metadata plus in a valid state.
    WriteMetadataIfNeededLocked(true);
  }
}

nsresult CacheFile::Init(const nsACString& aKey, bool aCreateNew,
                         bool aMemoryOnly, bool aSkipSizeCheck, bool aPriority,
                         bool aPinned, CacheFileListener* aCallback) {
  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(!mHandle);

  MOZ_ASSERT(!(aMemoryOnly && aPinned));

  nsresult rv;

  mKey = aKey;
  mOpenAsMemoryOnly = mMemoryOnly = aMemoryOnly;
  mSkipSizeCheck = aSkipSizeCheck;
  mPriority = aPriority;
  mPinned = aPinned;

  // Some consumers (at least nsHTTPCompressConv) assume that Read() can read
  // such amount of data that was announced by Available().
  // CacheFileInputStream::Available() uses also preloaded chunks to compute
  // number of available bytes in the input stream, so we have to make sure the
  // preloadChunkCount won't change during CacheFile's lifetime since otherwise
  // we could potentially release some cached chunks that was used to calculate
  // available bytes but would not be available later during call to
  // CacheFileInputStream::Read().
  mPreloadChunkCount = CacheObserver::PreloadChunkCount();

  LOG(
      ("CacheFile::Init() [this=%p, key=%s, createNew=%d, memoryOnly=%d, "
       "priority=%d, listener=%p]",
       this, mKey.get(), aCreateNew, aMemoryOnly, aPriority, aCallback));

  if (mMemoryOnly) {
    MOZ_ASSERT(!aCallback);

    mMetadata = new CacheFileMetadata(mOpenAsMemoryOnly, false, mKey);
    mReady = true;
    mDataSize = mMetadata->Offset();
    return NS_OK;
  }
  uint32_t flags;
  if (aCreateNew) {
    MOZ_ASSERT(!aCallback);
    flags = CacheFileIOManager::CREATE_NEW;

    // make sure we can use this entry immediately
    mMetadata = new CacheFileMetadata(mOpenAsMemoryOnly, mPinned, mKey);
    mReady = true;
    mDataSize = mMetadata->Offset();
  } else {
    flags = CacheFileIOManager::CREATE;
  }

  if (mPriority) {
    flags |= CacheFileIOManager::PRIORITY;
  }

  if (mPinned) {
    flags |= CacheFileIOManager::PINNED;
  }

  mOpeningFile = true;
  mListener = aCallback;
  rv = CacheFileIOManager::OpenFile(mKey, flags, this);
  if (NS_FAILED(rv)) {
    mListener = nullptr;
    mOpeningFile = false;

    if (mPinned) {
      LOG(
          ("CacheFile::Init() - CacheFileIOManager::OpenFile() failed "
           "but we want to pin, fail the file opening. [this=%p]",
           this));
      return NS_ERROR_NOT_AVAILABLE;
    }

    if (aCreateNew) {
      NS_WARNING("Forcing memory-only entry since OpenFile failed");
      LOG(
          ("CacheFile::Init() - CacheFileIOManager::OpenFile() failed "
           "synchronously. We can continue in memory-only mode since "
           "aCreateNew == true. [this=%p]",
           this));

      mMemoryOnly = true;
    } else if (rv == NS_ERROR_NOT_INITIALIZED) {
      NS_WARNING(
          "Forcing memory-only entry since CacheIOManager isn't "
          "initialized.");
      LOG(
          ("CacheFile::Init() - CacheFileIOManager isn't initialized, "
           "initializing entry as memory-only. [this=%p]",
           this));

      mMemoryOnly = true;
      mMetadata = new CacheFileMetadata(mOpenAsMemoryOnly, mPinned, mKey);
      mReady = true;
      mDataSize = mMetadata->Offset();

      RefPtr<NotifyCacheFileListenerEvent> ev;
      ev = new NotifyCacheFileListenerEvent(aCallback, NS_OK, true);
      rv = NS_DispatchToCurrentThread(ev);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult CacheFile::OnChunkRead(nsresult aResult, CacheFileChunk* aChunk) {
  CacheFileAutoLock lock(this);

  nsresult rv;

  uint32_t index = aChunk->Index();

  LOG(("CacheFile::OnChunkRead() [this=%p, rv=0x%08" PRIx32
       ", chunk=%p, idx=%u]",
       this, static_cast<uint32_t>(aResult), aChunk, index));

  if (aChunk->mDiscardedChunk) {
    // We discard only unused chunks, so it must be still unused when reading
    // data finishes.
    MOZ_ASSERT(aChunk->mRefCnt == 2);
    aChunk->mActiveChunk = false;
    ReleaseOutsideLock(
        RefPtr<CacheFileChunkListener>(std::move(aChunk->mFile)));

    DebugOnly<bool> removed = mDiscardedChunks.RemoveElement(aChunk);
    MOZ_ASSERT(removed);
    return NS_OK;
  }

  if (NS_FAILED(aResult)) {
    SetError(aResult);
  }

  if (HaveChunkListeners(index)) {
    rv = NotifyChunkListeners(index, aResult, aChunk);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult CacheFile::OnChunkWritten(nsresult aResult, CacheFileChunk* aChunk) {
  // In case the chunk was reused, made dirty and released between calls to
  // CacheFileChunk::Write() and CacheFile::OnChunkWritten(), we must write
  // the chunk to the disk again. When the chunk is unused and is dirty simply
  // addref and release (outside the lock) the chunk which ensures that
  // CacheFile::DeactivateChunk() will be called again.
  RefPtr<CacheFileChunk> deactivateChunkAgain;

  CacheFileAutoLock lock(this);

  nsresult rv;

  LOG(("CacheFile::OnChunkWritten() [this=%p, rv=0x%08" PRIx32
       ", chunk=%p, idx=%u]",
       this, static_cast<uint32_t>(aResult), aChunk, aChunk->Index()));

  MOZ_ASSERT(!mMemoryOnly);
  MOZ_ASSERT(!mOpeningFile);
  MOZ_ASSERT(mHandle);

  if (aChunk->mDiscardedChunk) {
    // We discard only unused chunks, so it must be still unused when writing
    // data finishes.
    MOZ_ASSERT(aChunk->mRefCnt == 2);
    aChunk->mActiveChunk = false;
    ReleaseOutsideLock(
        RefPtr<CacheFileChunkListener>(std::move(aChunk->mFile)));

    DebugOnly<bool> removed = mDiscardedChunks.RemoveElement(aChunk);
    MOZ_ASSERT(removed);
    return NS_OK;
  }

  if (NS_FAILED(aResult)) {
    SetError(aResult);
  }

  if (NS_SUCCEEDED(aResult) && !aChunk->IsDirty()) {
    // update hash value in metadata
    mMetadata->SetHash(aChunk->Index(), aChunk->Hash());
  }

  // notify listeners if there is any
  if (HaveChunkListeners(aChunk->Index())) {
    // don't release the chunk since there are some listeners queued
    rv = NotifyChunkListeners(aChunk->Index(), aResult, aChunk);
    if (NS_SUCCEEDED(rv)) {
      MOZ_ASSERT(aChunk->mRefCnt != 2);
      return NS_OK;
    }
  }

  if (aChunk->mRefCnt != 2) {
    LOG(
        ("CacheFile::OnChunkWritten() - Chunk is still used [this=%p, chunk=%p,"
         " refcnt=%" PRIuPTR "]",
         this, aChunk, aChunk->mRefCnt.get()));

    return NS_OK;
  }

  if (aChunk->IsDirty()) {
    LOG(
        ("CacheFile::OnChunkWritten() - Unused chunk is dirty. We must go "
         "through deactivation again. [this=%p, chunk=%p]",
         this, aChunk));

    deactivateChunkAgain = aChunk;
    return NS_OK;
  }

  bool keepChunk = false;
  if (NS_SUCCEEDED(aResult)) {
    keepChunk = ShouldCacheChunk(aChunk->Index());
    LOG(("CacheFile::OnChunkWritten() - %s unused chunk [this=%p, chunk=%p]",
         keepChunk ? "Caching" : "Releasing", this, aChunk));
  } else {
    LOG(
        ("CacheFile::OnChunkWritten() - Releasing failed chunk [this=%p, "
         "chunk=%p]",
         this, aChunk));
  }

  RemoveChunkInternal(aChunk, keepChunk);

  WriteMetadataIfNeededLocked();

  return NS_OK;
}

nsresult CacheFile::OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                                     CacheFileChunk* aChunk) {
  MOZ_CRASH("CacheFile::OnChunkAvailable should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult CacheFile::OnChunkUpdated(CacheFileChunk* aChunk) {
  MOZ_CRASH("CacheFile::OnChunkUpdated should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult CacheFile::OnFileOpened(CacheFileHandle* aHandle, nsresult aResult) {
  // Using an 'auto' class to perform doom or fail the listener
  // outside the CacheFile's lock.
  class AutoFailDoomListener {
   public:
    explicit AutoFailDoomListener(CacheFileHandle* aHandle)
        : mHandle(aHandle), mAlreadyDoomed(false) {}
    ~AutoFailDoomListener() {
      if (!mListener) return;

      if (mHandle) {
        if (mAlreadyDoomed) {
          mListener->OnFileDoomed(mHandle, NS_OK);
        } else {
          CacheFileIOManager::DoomFile(mHandle, mListener);
        }
      } else {
        mListener->OnFileDoomed(nullptr, NS_ERROR_NOT_AVAILABLE);
      }
    }

    CacheFileHandle* mHandle;
    nsCOMPtr<CacheFileIOListener> mListener;
    bool mAlreadyDoomed;
  } autoDoom(aHandle);

  nsCOMPtr<CacheFileListener> listener;
  bool isNew = false;
  nsresult retval = NS_OK;

  {
    CacheFileAutoLock lock(this);

    MOZ_ASSERT(mOpeningFile);
    MOZ_ASSERT((NS_SUCCEEDED(aResult) && aHandle) ||
               (NS_FAILED(aResult) && !aHandle));
    MOZ_ASSERT((mListener && !mMetadata) ||  // !createNew
               (!mListener && mMetadata));   // createNew
    MOZ_ASSERT(!mMemoryOnly || mMetadata);   // memory-only was set on new entry

    LOG(("CacheFile::OnFileOpened() [this=%p, rv=0x%08" PRIx32 ", handle=%p]",
         this, static_cast<uint32_t>(aResult), aHandle));

    mOpeningFile = false;

    autoDoom.mListener.swap(mDoomAfterOpenListener);

    if (mMemoryOnly) {
      // We can be here only in case the entry was initilized as createNew and
      // SetMemoryOnly() was called.

      // Just don't store the handle into mHandle and exit
      autoDoom.mAlreadyDoomed = true;
      return NS_OK;
    }

    if (NS_FAILED(aResult)) {
      if (mMetadata) {
        // This entry was initialized as createNew, just switch to memory-only
        // mode.
        NS_WARNING("Forcing memory-only entry since OpenFile failed");
        LOG(
            ("CacheFile::OnFileOpened() - CacheFileIOManager::OpenFile() "
             "failed asynchronously. We can continue in memory-only mode since "
             "aCreateNew == true. [this=%p]",
             this));

        mMemoryOnly = true;
        return NS_OK;
      }

      if (aResult == NS_ERROR_FILE_INVALID_PATH) {
        // CacheFileIOManager doesn't have mCacheDirectory, switch to
        // memory-only mode.
        NS_WARNING(
            "Forcing memory-only entry since CacheFileIOManager doesn't "
            "have mCacheDirectory.");
        LOG(
            ("CacheFile::OnFileOpened() - CacheFileIOManager doesn't have "
             "mCacheDirectory, initializing entry as memory-only. [this=%p]",
             this));

        mMemoryOnly = true;
        mMetadata = new CacheFileMetadata(mOpenAsMemoryOnly, mPinned, mKey);
        mReady = true;
        mDataSize = mMetadata->Offset();

        isNew = true;
        retval = NS_OK;
      } else {
        // CacheFileIOManager::OpenFile() failed for another reason.
        isNew = false;
        retval = aResult;
      }

      mListener.swap(listener);
    } else {
      mHandle = aHandle;
      if (NS_FAILED(mStatus)) {
        CacheFileIOManager::DoomFile(mHandle, nullptr);
      }

      if (mMetadata) {
        InitIndexEntry();

        // The entry was initialized as createNew, don't try to read metadata.
        mMetadata->SetHandle(mHandle);

        // Write all cached chunks, otherwise they may stay unwritten.
        for (auto iter = mCachedChunks.Iter(); !iter.Done(); iter.Next()) {
          uint32_t idx = iter.Key();
          RefPtr<CacheFileChunk>& chunk = iter.Data();

          LOG(("CacheFile::OnFileOpened() - write [this=%p, idx=%u, chunk=%p]",
               this, idx, chunk.get()));

          mChunks.InsertOrUpdate(idx, RefPtr{chunk});
          chunk->mFile = this;
          chunk->mActiveChunk = true;

          MOZ_ASSERT(chunk->IsReady());

          // This would be cleaner if we had an nsRefPtr constructor that took
          // a RefPtr<Derived>.
          ReleaseOutsideLock(std::move(chunk));

          iter.Remove();
        }

        return NS_OK;
      }
    }
  }

  if (listener) {
    listener->OnFileReady(retval, isNew);
    return NS_OK;
  }

  MOZ_ASSERT(NS_SUCCEEDED(aResult));
  MOZ_ASSERT(!mMetadata);
  MOZ_ASSERT(mListener);

  mMetadata = new CacheFileMetadata(mHandle, mKey);
  mMetadata->ReadMetadata(this);
  return NS_OK;
}

nsresult CacheFile::OnDataWritten(CacheFileHandle* aHandle, const char* aBuf,
                                  nsresult aResult) {
  MOZ_CRASH("CacheFile::OnDataWritten should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult CacheFile::OnDataRead(CacheFileHandle* aHandle, char* aBuf,
                               nsresult aResult) {
  MOZ_CRASH("CacheFile::OnDataRead should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult CacheFile::OnMetadataRead(nsresult aResult) {
  MOZ_ASSERT(mListener);

  LOG(("CacheFile::OnMetadataRead() [this=%p, rv=0x%08" PRIx32 "]", this,
       static_cast<uint32_t>(aResult)));

  bool isNew = false;
  if (NS_SUCCEEDED(aResult)) {
    mPinned = mMetadata->Pinned();
    mReady = true;
    mDataSize = mMetadata->Offset();
    if (mDataSize == 0 && mMetadata->ElementsSize() == 0) {
      isNew = true;
      mMetadata->MarkDirty();
    } else {
      const char* altData = mMetadata->GetElement(CacheFileUtils::kAltDataKey);
      if (altData && (NS_FAILED(CacheFileUtils::ParseAlternativeDataInfo(
                          altData, &mAltDataOffset, &mAltDataType)) ||
                      (mAltDataOffset > mDataSize))) {
        // alt-metadata cannot be parsed or alt-data offset is invalid
        mMetadata->InitEmptyMetadata();
        isNew = true;
        mAltDataOffset = -1;
        mAltDataType.Truncate();
        mDataSize = 0;
      } else {
        CacheFileAutoLock lock(this);
        PreloadChunks(0);
      }
    }

    InitIndexEntry();
  }

  nsCOMPtr<CacheFileListener> listener;
  mListener.swap(listener);
  listener->OnFileReady(aResult, isNew);
  return NS_OK;
}

nsresult CacheFile::OnMetadataWritten(nsresult aResult) {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::OnMetadataWritten() [this=%p, rv=0x%08" PRIx32 "]", this,
       static_cast<uint32_t>(aResult)));

  MOZ_ASSERT(mWritingMetadata);
  mWritingMetadata = false;

  MOZ_ASSERT(!mMemoryOnly);
  MOZ_ASSERT(!mOpeningFile);

  if (NS_WARN_IF(NS_FAILED(aResult))) {
    // TODO close streams with an error ???
    SetError(aResult);
  }

  if (mOutput || mInputs.Length() || mChunks.Count()) return NS_OK;

  if (IsDirty()) WriteMetadataIfNeededLocked();

  if (!mWritingMetadata) {
    LOG(("CacheFile::OnMetadataWritten() - Releasing file handle [this=%p]",
         this));
    CacheFileIOManager::ReleaseNSPRHandle(mHandle);
  }

  return NS_OK;
}

nsresult CacheFile::OnFileDoomed(CacheFileHandle* aHandle, nsresult aResult) {
  nsCOMPtr<CacheFileListener> listener;

  {
    CacheFileAutoLock lock(this);

    MOZ_ASSERT(mListener);

    LOG(("CacheFile::OnFileDoomed() [this=%p, rv=0x%08" PRIx32 ", handle=%p]",
         this, static_cast<uint32_t>(aResult), aHandle));

    mListener.swap(listener);
  }

  listener->OnFileDoomed(aResult);
  return NS_OK;
}

nsresult CacheFile::OnEOFSet(CacheFileHandle* aHandle, nsresult aResult) {
  MOZ_CRASH("CacheFile::OnEOFSet should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult CacheFile::OnFileRenamed(CacheFileHandle* aHandle, nsresult aResult) {
  MOZ_CRASH("CacheFile::OnFileRenamed should not be called!");
  return NS_ERROR_UNEXPECTED;
}

bool CacheFile::IsKilled() {
  bool killed = mKill;
  if (killed) {
    LOG(("CacheFile is killed, this=%p", this));
  }

  return killed;
}

nsresult CacheFile::OpenInputStream(nsICacheEntry* aEntryHandle,
                                    nsIInputStream** _retval) {
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

  if (!mReady) {
    LOG(("CacheFile::OpenInputStream() - CacheFile is not ready [this=%p]",
         this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_FAILED(mStatus)) {
    LOG(
        ("CacheFile::OpenInputStream() - CacheFile is in a failure state "
         "[this=%p, status=0x%08" PRIx32 "]",
         this, static_cast<uint32_t>(mStatus)));

    // Don't allow opening the input stream when this CacheFile is in
    // a failed state.  This is the only way to protect consumers correctly
    // from reading a broken entry.  When the file is in the failed state,
    // it's also doomed, so reopening the entry won't make any difference -
    // data will still be inaccessible anymore.  Note that for just doomed
    // files, we must allow reading the data.
    return mStatus;
  }

  // Once we open input stream we no longer allow preloading of chunks without
  // input stream, i.e. we will no longer keep first few chunks preloaded when
  // the last input stream is closed.
  mPreloadWithoutInputStreams = false;

  CacheFileInputStream* input =
      new CacheFileInputStream(this, aEntryHandle, false);
  LOG(("CacheFile::OpenInputStream() - Creating new input stream %p [this=%p]",
       input, this));

  mInputs.AppendElement(input);
  NS_ADDREF(input);

  mDataAccessed = true;
  *_retval = do_AddRef(input).take();
  return NS_OK;
}

nsresult CacheFile::OpenAlternativeInputStream(nsICacheEntry* aEntryHandle,
                                               const char* aAltDataType,
                                               nsIInputStream** _retval) {
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

  if (NS_WARN_IF(!mReady)) {
    LOG(
        ("CacheFile::OpenAlternativeInputStream() - CacheFile is not ready "
         "[this=%p]",
         this));
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mAltDataOffset == -1) {
    LOG(
        ("CacheFile::OpenAlternativeInputStream() - Alternative data is not "
         "available [this=%p]",
         this));
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_FAILED(mStatus)) {
    LOG(
        ("CacheFile::OpenAlternativeInputStream() - CacheFile is in a failure "
         "state [this=%p, status=0x%08" PRIx32 "]",
         this, static_cast<uint32_t>(mStatus)));

    // Don't allow opening the input stream when this CacheFile is in
    // a failed state.  This is the only way to protect consumers correctly
    // from reading a broken entry.  When the file is in the failed state,
    // it's also doomed, so reopening the entry won't make any difference -
    // data will still be inaccessible anymore.  Note that for just doomed
    // files, we must allow reading the data.
    return mStatus;
  }

  if (mAltDataType != aAltDataType) {
    LOG(
        ("CacheFile::OpenAlternativeInputStream() - Alternative data is of a "
         "different type than requested [this=%p, availableType=%s, "
         "requestedType=%s]",
         this, mAltDataType.get(), aAltDataType));
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Once we open input stream we no longer allow preloading of chunks without
  // input stream, i.e. we will no longer keep first few chunks preloaded when
  // the last input stream is closed.
  mPreloadWithoutInputStreams = false;

  CacheFileInputStream* input =
      new CacheFileInputStream(this, aEntryHandle, true);

  LOG(
      ("CacheFile::OpenAlternativeInputStream() - Creating new input stream %p "
       "[this=%p]",
       input, this));

  mInputs.AppendElement(input);
  NS_ADDREF(input);

  mDataAccessed = true;
  *_retval = do_AddRef(input).take();

  return NS_OK;
}

nsresult CacheFile::OpenOutputStream(CacheOutputCloseListener* aCloseListener,
                                     nsIOutputStream** _retval) {
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

  nsresult rv;

  if (!mReady) {
    LOG(("CacheFile::OpenOutputStream() - CacheFile is not ready [this=%p]",
         this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mOutput) {
    LOG(
        ("CacheFile::OpenOutputStream() - We already have output stream %p "
         "[this=%p]",
         mOutput, this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_FAILED(mStatus)) {
    LOG(
        ("CacheFile::OpenOutputStream() - CacheFile is in a failure state "
         "[this=%p, status=0x%08" PRIx32 "]",
         this, static_cast<uint32_t>(mStatus)));

    // The CacheFile is already doomed. It make no sense to allow to write any
    // data to such entry.
    return mStatus;
  }

  // Fail if there is any input stream opened for alternative data
  for (uint32_t i = 0; i < mInputs.Length(); ++i) {
    if (mInputs[i]->IsAlternativeData()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  if (mAltDataOffset != -1) {
    // Remove alt-data
    rv = Truncate(mAltDataOffset);
    if (NS_FAILED(rv)) {
      LOG(
          ("CacheFile::OpenOutputStream() - Truncating alt-data failed "
           "[rv=0x%08" PRIx32 "]",
           static_cast<uint32_t>(rv)));
      return rv;
    }
    SetAltMetadata(nullptr);
    mAltDataOffset = -1;
    mAltDataType.Truncate();
  }

  // Once we open output stream we no longer allow preloading of chunks without
  // input stream. There is no reason to believe that some input stream will be
  // opened soon. Otherwise we would cache unused chunks of all newly created
  // entries until the CacheFile is destroyed.
  mPreloadWithoutInputStreams = false;

  mOutput = new CacheFileOutputStream(this, aCloseListener, false);

  LOG(
      ("CacheFile::OpenOutputStream() - Creating new output stream %p "
       "[this=%p]",
       mOutput, this));

  mDataAccessed = true;
  *_retval = do_AddRef(mOutput).take();
  return NS_OK;
}

nsresult CacheFile::OpenAlternativeOutputStream(
    CacheOutputCloseListener* aCloseListener, const char* aAltDataType,
    nsIAsyncOutputStream** _retval) {
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

  if (!mReady) {
    LOG(
        ("CacheFile::OpenAlternativeOutputStream() - CacheFile is not ready "
         "[this=%p]",
         this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mOutput) {
    LOG(
        ("CacheFile::OpenAlternativeOutputStream() - We already have output "
         "stream %p [this=%p]",
         mOutput, this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_FAILED(mStatus)) {
    LOG(
        ("CacheFile::OpenAlternativeOutputStream() - CacheFile is in a failure "
         "state [this=%p, status=0x%08" PRIx32 "]",
         this, static_cast<uint32_t>(mStatus)));

    // The CacheFile is already doomed. It make no sense to allow to write any
    // data to such entry.
    return mStatus;
  }

  // Fail if there is any input stream opened for alternative data
  for (uint32_t i = 0; i < mInputs.Length(); ++i) {
    if (mInputs[i]->IsAlternativeData()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  nsresult rv;

  if (mAltDataOffset != -1) {
    // Truncate old alt-data
    rv = Truncate(mAltDataOffset);
    if (NS_FAILED(rv)) {
      LOG(
          ("CacheFile::OpenAlternativeOutputStream() - Truncating old alt-data "
           "failed [rv=0x%08" PRIx32 "]",
           static_cast<uint32_t>(rv)));
      return rv;
    }
  } else {
    mAltDataOffset = mDataSize;
  }

  nsAutoCString altMetadata;
  CacheFileUtils::BuildAlternativeDataInfo(aAltDataType, mAltDataOffset,
                                           altMetadata);
  rv = SetAltMetadata(altMetadata.get());
  if (NS_FAILED(rv)) {
    LOG(
        ("CacheFile::OpenAlternativeOutputStream() - Set Metadata for alt-data"
         "failed [rv=0x%08" PRIx32 "]",
         static_cast<uint32_t>(rv)));
    return rv;
  }

  // Once we open output stream we no longer allow preloading of chunks without
  // input stream. There is no reason to believe that some input stream will be
  // opened soon. Otherwise we would cache unused chunks of all newly created
  // entries until the CacheFile is destroyed.
  mPreloadWithoutInputStreams = false;

  mOutput = new CacheFileOutputStream(this, aCloseListener, true);

  LOG(
      ("CacheFile::OpenAlternativeOutputStream() - Creating new output stream "
       "%p [this=%p]",
       mOutput, this));

  mDataAccessed = true;
  mAltDataType = aAltDataType;
  *_retval = do_AddRef(mOutput).take();
  return NS_OK;
}

nsresult CacheFile::SetMemoryOnly() {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::SetMemoryOnly() mMemoryOnly=%d [this=%p]", mMemoryOnly,
       this));

  if (mMemoryOnly) return NS_OK;

  MOZ_ASSERT(mReady);

  if (!mReady) {
    LOG(("CacheFile::SetMemoryOnly() - CacheFile is not ready [this=%p]",
         this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mDataAccessed) {
    LOG(("CacheFile::SetMemoryOnly() - Data was already accessed [this=%p]",
         this));
    return NS_ERROR_NOT_AVAILABLE;
  }

  // TODO what to do when this isn't a new entry and has an existing metadata???
  mMemoryOnly = true;
  return NS_OK;
}

nsresult CacheFile::Doom(CacheFileListener* aCallback) {
  LOG(("CacheFile::Doom() [this=%p, listener=%p]", this, aCallback));

  CacheFileAutoLock lock(this);

  return DoomLocked(aCallback);
}

nsresult CacheFile::DoomLocked(CacheFileListener* aCallback) {
  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

  LOG(("CacheFile::DoomLocked() [this=%p, listener=%p]", this, aCallback));

  nsresult rv = NS_OK;

  if (mMemoryOnly) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  if (mHandle && mHandle->IsDoomed()) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  nsCOMPtr<CacheFileIOListener> listener;
  if (aCallback || !mHandle) {
    listener = new DoomFileHelper(aCallback);
  }
  if (mHandle) {
    rv = CacheFileIOManager::DoomFile(mHandle, listener);
  } else if (mOpeningFile) {
    mDoomAfterOpenListener = listener;
  }

  return rv;
}

nsresult CacheFile::ThrowMemoryCachedData() {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::ThrowMemoryCachedData() [this=%p]", this));

  if (mMemoryOnly) {
    // This method should not be called when the CacheFile was initialized as
    // memory-only, but it can be called when CacheFile end up as memory-only
    // due to e.g. IO failure since CacheEntry doesn't know it.
    LOG(
        ("CacheFile::ThrowMemoryCachedData() - Ignoring request because the "
         "entry is memory-only. [this=%p]",
         this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mOpeningFile) {
    // mayhemer, note: we shouldn't get here, since CacheEntry prevents loading
    // entries from being purged.

    LOG(
        ("CacheFile::ThrowMemoryCachedData() - Ignoring request because the "
         "entry is still opening the file [this=%p]",
         this));

    return NS_ERROR_ABORT;
  }

  // We cannot release all cached chunks since we need to keep preloaded chunks
  // in memory. See initialization of mPreloadChunkCount for explanation.
  CleanUpCachedChunks();

  return NS_OK;
}

nsresult CacheFile::GetElement(const char* aKey, char** _retval) {
  CacheFileAutoLock lock(this);
  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  const char* value;
  value = mMetadata->GetElement(aKey);
  if (!value) return NS_ERROR_NOT_AVAILABLE;

  *_retval = NS_xstrdup(value);
  return NS_OK;
}

nsresult CacheFile::SetElement(const char* aKey, const char* aValue) {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::SetElement() this=%p", this));

  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  if (!strcmp(aKey, CacheFileUtils::kAltDataKey)) {
    NS_ERROR(
        "alt-data element is reserved for internal use and must not be "
        "changed via CacheFile::SetElement()");
    return NS_ERROR_FAILURE;
  }

  PostWriteTimer();
  return mMetadata->SetElement(aKey, aValue);
}

nsresult CacheFile::VisitMetaData(nsICacheEntryMetaDataVisitor* aVisitor) {
  CacheFileAutoLock lock(this);
  MOZ_ASSERT(mMetadata);
  MOZ_ASSERT(mReady);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  mMetadata->Visit(aVisitor);
  return NS_OK;
}

nsresult CacheFile::ElementsSize(uint32_t* _retval) {
  CacheFileAutoLock lock(this);

  if (!mMetadata) return NS_ERROR_NOT_AVAILABLE;

  *_retval = mMetadata->ElementsSize();
  return NS_OK;
}

nsresult CacheFile::SetExpirationTime(uint32_t aExpirationTime) {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::SetExpirationTime() this=%p, expiration=%u", this,
       aExpirationTime));

  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  PostWriteTimer();
  mMetadata->SetExpirationTime(aExpirationTime);
  return NS_OK;
}

nsresult CacheFile::GetExpirationTime(uint32_t* _retval) {
  CacheFileAutoLock lock(this);
  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  *_retval = mMetadata->GetExpirationTime();
  return NS_OK;
}

nsresult CacheFile::SetFrecency(uint32_t aFrecency) {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::SetFrecency() this=%p, frecency=%u", this, aFrecency));

  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  PostWriteTimer();

  if (mHandle && !mHandle->IsDoomed())
    CacheFileIOManager::UpdateIndexEntry(mHandle, &aFrecency, nullptr, nullptr,
                                         nullptr, nullptr);

  mMetadata->SetFrecency(aFrecency);
  return NS_OK;
}

nsresult CacheFile::GetFrecency(uint32_t* _retval) {
  CacheFileAutoLock lock(this);
  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);
  *_retval = mMetadata->GetFrecency();
  return NS_OK;
}

nsresult CacheFile::SetNetworkTimes(uint64_t aOnStartTime,
                                    uint64_t aOnStopTime) {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::SetNetworkTimes() this=%p, aOnStartTime=%" PRIu64
       ", aOnStopTime=%" PRIu64 "",
       this, aOnStartTime, aOnStopTime));

  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  PostWriteTimer();

  nsAutoCString onStartTime;
  onStartTime.AppendInt(aOnStartTime);
  nsresult rv =
      mMetadata->SetElement("net-response-time-onstart", onStartTime.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString onStopTime;
  onStopTime.AppendInt(aOnStopTime);
  rv = mMetadata->SetElement("net-response-time-onstop", onStopTime.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint16_t onStartTime16 = aOnStartTime <= kIndexTimeOutOfBound
                               ? aOnStartTime
                               : kIndexTimeOutOfBound;
  uint16_t onStopTime16 =
      aOnStopTime <= kIndexTimeOutOfBound ? aOnStopTime : kIndexTimeOutOfBound;

  if (mHandle && !mHandle->IsDoomed()) {
    CacheFileIOManager::UpdateIndexEntry(
        mHandle, nullptr, nullptr, &onStartTime16, &onStopTime16, nullptr);
  }
  return NS_OK;
}

nsresult CacheFile::GetOnStartTime(uint64_t* _retval) {
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mMetadata);
  const char* onStartTimeStr =
      mMetadata->GetElement("net-response-time-onstart");
  if (!onStartTimeStr) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsresult rv;
  *_retval = nsDependentCString(onStartTimeStr).ToInteger64(&rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return NS_OK;
}

nsresult CacheFile::GetOnStopTime(uint64_t* _retval) {
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mMetadata);
  const char* onStopTimeStr = mMetadata->GetElement("net-response-time-onstop");
  if (!onStopTimeStr) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsresult rv;
  *_retval = nsDependentCString(onStopTimeStr).ToInteger64(&rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return NS_OK;
}

nsresult CacheFile::SetContentType(uint8_t aContentType) {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::SetContentType() this=%p, contentType=%u", this,
       aContentType));

  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  PostWriteTimer();

  // Save the content type to metadata for case we need to rebuild the index.
  nsAutoCString contentType;
  contentType.AppendInt(aContentType);
  nsresult rv = mMetadata->SetElement("ctid", contentType.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mHandle && !mHandle->IsDoomed()) {
    CacheFileIOManager::UpdateIndexEntry(mHandle, nullptr, nullptr, nullptr,
                                         nullptr, &aContentType);
  }
  return NS_OK;
}

nsresult CacheFile::SetAltMetadata(const char* aAltMetadata) {
  AssertOwnsLock();
  LOG(("CacheFile::SetAltMetadata() this=%p, aAltMetadata=%s", this,
       aAltMetadata ? aAltMetadata : ""));

  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  PostWriteTimer();

  nsresult rv =
      mMetadata->SetElement(CacheFileUtils::kAltDataKey, aAltMetadata);

  bool hasAltData = !!aAltMetadata;

  if (NS_FAILED(rv)) {
    // Removing element shouldn't fail because it doesn't allocate memory.
    mMetadata->SetElement(CacheFileUtils::kAltDataKey, nullptr);

    mAltDataOffset = -1;
    mAltDataType.Truncate();
    hasAltData = false;
  }

  if (mHandle && !mHandle->IsDoomed()) {
    CacheFileIOManager::UpdateIndexEntry(mHandle, nullptr, &hasAltData, nullptr,
                                         nullptr, nullptr);
  }
  return rv;
}

nsresult CacheFile::GetLastModified(uint32_t* _retval) {
  CacheFileAutoLock lock(this);
  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  *_retval = mMetadata->GetLastModified();
  return NS_OK;
}

nsresult CacheFile::GetLastFetched(uint32_t* _retval) {
  CacheFileAutoLock lock(this);
  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  *_retval = mMetadata->GetLastFetched();
  return NS_OK;
}

nsresult CacheFile::GetFetchCount(uint32_t* _retval) {
  CacheFileAutoLock lock(this);
  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);
  *_retval = mMetadata->GetFetchCount();
  return NS_OK;
}

nsresult CacheFile::GetDiskStorageSizeInKB(uint32_t* aDiskStorageSize) {
  if (!mHandle) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aDiskStorageSize = mHandle->FileSizeInK();
  return NS_OK;
}

nsresult CacheFile::OnFetched() {
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::OnFetched() this=%p", this));

  MOZ_ASSERT(mMetadata);
  NS_ENSURE_TRUE(mMetadata, NS_ERROR_UNEXPECTED);

  PostWriteTimer();

  mMetadata->OnFetched();
  return NS_OK;
}

void CacheFile::Lock() { mLock.Lock(); }

void CacheFile::Unlock() {
  // move the elements out of mObjsToRelease
  // so that they can be released after we unlock
  nsTArray<RefPtr<nsISupports>> objs = std::move(mObjsToRelease);

  mLock.Unlock();
}

void CacheFile::AssertOwnsLock() const { mLock.AssertCurrentThreadOwns(); }

void CacheFile::ReleaseOutsideLock(RefPtr<nsISupports> aObject) {
  AssertOwnsLock();

  mObjsToRelease.AppendElement(std::move(aObject));
}

nsresult CacheFile::GetChunkLocked(uint32_t aIndex, ECallerType aCaller,
                                   CacheFileChunkListener* aCallback,
                                   CacheFileChunk** _retval) {
  AssertOwnsLock();

  LOG(("CacheFile::GetChunkLocked() [this=%p, idx=%u, caller=%d, listener=%p]",
       this, aIndex, aCaller, aCallback));

  MOZ_ASSERT(mReady);
  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);
  MOZ_ASSERT((aCaller == READER && aCallback) ||
             (aCaller == WRITER && !aCallback) ||
             (aCaller == PRELOADER && !aCallback));

  // Preload chunks from disk when this is disk backed entry and the listener
  // is reader.
  bool preload = !mMemoryOnly && (aCaller == READER);

  nsresult rv;

  RefPtr<CacheFileChunk> chunk;
  if (mChunks.Get(aIndex, getter_AddRefs(chunk))) {
    LOG(("CacheFile::GetChunkLocked() - Found chunk %p in mChunks [this=%p]",
         chunk.get(), this));

    // Preloader calls this method to preload only non-loaded chunks.
    MOZ_ASSERT(aCaller != PRELOADER, "Unexpected!");

    // We might get failed chunk between releasing the lock in
    // CacheFileChunk::OnDataWritten/Read and CacheFile::OnChunkWritten/Read
    rv = chunk->GetStatus();
    if (NS_FAILED(rv)) {
      SetError(rv);
      LOG(
          ("CacheFile::GetChunkLocked() - Found failed chunk in mChunks "
           "[this=%p]",
           this));
      return rv;
    }

    if (chunk->IsReady() || aCaller == WRITER) {
      chunk.swap(*_retval);
    } else {
      QueueChunkListener(aIndex, aCallback);
    }

    if (preload) {
      PreloadChunks(aIndex + 1);
    }

    return NS_OK;
  }

  if (mCachedChunks.Get(aIndex, getter_AddRefs(chunk))) {
    LOG(("CacheFile::GetChunkLocked() - Reusing cached chunk %p [this=%p]",
         chunk.get(), this));

    // Preloader calls this method to preload only non-loaded chunks.
    MOZ_ASSERT(aCaller != PRELOADER, "Unexpected!");

    mChunks.InsertOrUpdate(aIndex, RefPtr{chunk});
    mCachedChunks.Remove(aIndex);
    chunk->mFile = this;
    chunk->mActiveChunk = true;

    MOZ_ASSERT(chunk->IsReady());

    chunk.swap(*_retval);

    if (preload) {
      PreloadChunks(aIndex + 1);
    }

    return NS_OK;
  }

  int64_t off = aIndex * static_cast<int64_t>(kChunkSize);

  if (off < mDataSize) {
    // We cannot be here if this is memory only entry since the chunk must exist
    MOZ_ASSERT(!mMemoryOnly);
    if (mMemoryOnly) {
      // If this ever really happen it is better to fail rather than crashing on
      // a null handle.
      LOG(
          ("CacheFile::GetChunkLocked() - Unexpected state! Offset < mDataSize "
           "for memory-only entry. [this=%p, off=%" PRId64
           ", mDataSize=%" PRId64 "]",
           this, off, mDataSize));

      return NS_ERROR_UNEXPECTED;
    }

    chunk = new CacheFileChunk(this, aIndex, aCaller == WRITER);
    mChunks.InsertOrUpdate(aIndex, RefPtr{chunk});
    chunk->mActiveChunk = true;

    LOG(
        ("CacheFile::GetChunkLocked() - Reading newly created chunk %p from "
         "the disk [this=%p]",
         chunk.get(), this));

    // Read the chunk from the disk
    rv = chunk->Read(mHandle,
                     std::min(static_cast<uint32_t>(mDataSize - off),
                              static_cast<uint32_t>(kChunkSize)),
                     mMetadata->GetHash(aIndex), this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      RemoveChunkInternal(chunk, false);
      return rv;
    }

    if (aCaller == WRITER) {
      chunk.swap(*_retval);
    } else if (aCaller != PRELOADER) {
      QueueChunkListener(aIndex, aCallback);
    }

    if (preload) {
      PreloadChunks(aIndex + 1);
    }

    return NS_OK;
  } else if (off == mDataSize) {
    if (aCaller == WRITER) {
      // this listener is going to write to the chunk
      chunk = new CacheFileChunk(this, aIndex, true);
      mChunks.InsertOrUpdate(aIndex, RefPtr{chunk});
      chunk->mActiveChunk = true;

      LOG(("CacheFile::GetChunkLocked() - Created new empty chunk %p [this=%p]",
           chunk.get(), this));

      chunk->InitNew();
      mMetadata->SetHash(aIndex, chunk->Hash());

      if (HaveChunkListeners(aIndex)) {
        rv = NotifyChunkListeners(aIndex, NS_OK, chunk);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      chunk.swap(*_retval);
      return NS_OK;
    }
  } else {
    if (aCaller == WRITER) {
      // this chunk was requested by writer, but we need to fill the gap first

      // Fill with zero the last chunk if it is incomplete
      if (mDataSize % kChunkSize) {
        rv = PadChunkWithZeroes(mDataSize / kChunkSize);
        NS_ENSURE_SUCCESS(rv, rv);

        MOZ_ASSERT(!(mDataSize % kChunkSize));
      }

      uint32_t startChunk = mDataSize / kChunkSize;

      if (mMemoryOnly) {
        // We need to create all missing CacheFileChunks if this is memory-only
        // entry
        for (uint32_t i = startChunk; i < aIndex; i++) {
          rv = PadChunkWithZeroes(i);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      } else {
        // We don't need to create CacheFileChunk for other empty chunks unless
        // there is some input stream waiting for this chunk.

        if (startChunk != aIndex) {
          // Make sure the file contains zeroes at the end of the file
          rv = CacheFileIOManager::TruncateSeekSetEOF(
              mHandle, startChunk * kChunkSize, aIndex * kChunkSize, nullptr);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        for (uint32_t i = startChunk; i < aIndex; i++) {
          if (HaveChunkListeners(i)) {
            rv = PadChunkWithZeroes(i);
            NS_ENSURE_SUCCESS(rv, rv);
          } else {
            mMetadata->SetHash(i, kEmptyChunkHash);
            mDataSize = (i + 1) * kChunkSize;
          }
        }
      }

      MOZ_ASSERT(mDataSize == off);
      rv = GetChunkLocked(aIndex, WRITER, nullptr, getter_AddRefs(chunk));
      NS_ENSURE_SUCCESS(rv, rv);

      chunk.swap(*_retval);
      return NS_OK;
    }
  }

  // We can be here only if the caller is reader since writer always create a
  // new chunk above and preloader calls this method to preload only chunks that
  // are not loaded but that do exist.
  MOZ_ASSERT(aCaller == READER, "Unexpected!");

  if (mOutput) {
    // the chunk doesn't exist but mOutput may create it
    QueueChunkListener(aIndex, aCallback);
  } else {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

void CacheFile::PreloadChunks(uint32_t aIndex) {
  AssertOwnsLock();

  uint32_t limit = aIndex + mPreloadChunkCount;

  for (uint32_t i = aIndex; i < limit; ++i) {
    int64_t off = i * static_cast<int64_t>(kChunkSize);

    if (off >= mDataSize) {
      // This chunk is beyond EOF.
      return;
    }

    if (mChunks.GetWeak(i) || mCachedChunks.GetWeak(i)) {
      // This chunk is already in memory or is being read right now.
      continue;
    }

    LOG(("CacheFile::PreloadChunks() - Preloading chunk [this=%p, idx=%u]",
         this, i));

    RefPtr<CacheFileChunk> chunk;
    GetChunkLocked(i, PRELOADER, nullptr, getter_AddRefs(chunk));
    // We've checked that we don't have this chunk, so no chunk must be
    // returned.
    MOZ_ASSERT(!chunk);
  }
}

bool CacheFile::ShouldCacheChunk(uint32_t aIndex) {
  AssertOwnsLock();

#ifdef CACHE_CHUNKS
  // We cache all chunks.
  return true;
#else

  if (mPreloadChunkCount != 0 && mInputs.Length() == 0 &&
      mPreloadWithoutInputStreams && aIndex < mPreloadChunkCount) {
    // We don't have any input stream yet, but it is likely that some will be
    // opened soon. Keep first mPreloadChunkCount chunks in memory. The
    // condition is here instead of in MustKeepCachedChunk() since these
    // chunks should be preloaded and can be kept in memory as an optimization,
    // but they can be released at any time until they are considered as
    // preloaded chunks for any input stream.
    return true;
  }

  // Cache only chunks that we really need to keep.
  return MustKeepCachedChunk(aIndex);
#endif
}

bool CacheFile::MustKeepCachedChunk(uint32_t aIndex) {
  AssertOwnsLock();

  // We must keep the chunk when this is memory only entry or we don't have
  // a handle yet.
  if (mMemoryOnly || mOpeningFile) {
    return true;
  }

  if (mPreloadChunkCount == 0) {
    // Preloading of chunks is disabled
    return false;
  }

  // Check whether this chunk should be considered as preloaded chunk for any
  // existing input stream.

  // maxPos is the position of the last byte in the given chunk
  int64_t maxPos = static_cast<int64_t>(aIndex + 1) * kChunkSize - 1;

  // minPos is the position of the first byte in a chunk that precedes the given
  // chunk by mPreloadChunkCount chunks
  int64_t minPos;
  if (mPreloadChunkCount >= aIndex) {
    minPos = 0;
  } else {
    minPos = static_cast<int64_t>(aIndex - mPreloadChunkCount) * kChunkSize;
  }

  for (uint32_t i = 0; i < mInputs.Length(); ++i) {
    int64_t inputPos = mInputs[i]->GetPosition();
    if (inputPos >= minPos && inputPos <= maxPos) {
      return true;
    }
  }

  return false;
}

nsresult CacheFile::DeactivateChunk(CacheFileChunk* aChunk) {
  nsresult rv;

  // Avoid lock reentrancy by increasing the RefCnt
  RefPtr<CacheFileChunk> chunk = aChunk;

  {
    CacheFileAutoLock lock(this);

    LOG(("CacheFile::DeactivateChunk() [this=%p, chunk=%p, idx=%u]", this,
         aChunk, aChunk->Index()));

    MOZ_ASSERT(mReady);
    MOZ_ASSERT((mHandle && !mMemoryOnly && !mOpeningFile) ||
               (!mHandle && mMemoryOnly && !mOpeningFile) ||
               (!mHandle && !mMemoryOnly && mOpeningFile));

    if (aChunk->mRefCnt != 2) {
      LOG(
          ("CacheFile::DeactivateChunk() - Chunk is still used [this=%p, "
           "chunk=%p, refcnt=%" PRIuPTR "]",
           this, aChunk, aChunk->mRefCnt.get()));

      // somebody got the reference before the lock was acquired
      return NS_OK;
    }

    if (aChunk->mDiscardedChunk) {
      aChunk->mActiveChunk = false;
      ReleaseOutsideLock(
          RefPtr<CacheFileChunkListener>(std::move(aChunk->mFile)));

      DebugOnly<bool> removed = mDiscardedChunks.RemoveElement(aChunk);
      MOZ_ASSERT(removed);
      return NS_OK;
    }

#ifdef DEBUG
    {
      // We can be here iff the chunk is in the hash table
      RefPtr<CacheFileChunk> chunkCheck;
      mChunks.Get(chunk->Index(), getter_AddRefs(chunkCheck));
      MOZ_ASSERT(chunkCheck == chunk);

      // We also shouldn't have any queued listener for this chunk
      ChunkListeners* listeners;
      mChunkListeners.Get(chunk->Index(), &listeners);
      MOZ_ASSERT(!listeners);
    }
#endif

    if (NS_FAILED(chunk->GetStatus())) {
      SetError(chunk->GetStatus());
    }

    if (NS_FAILED(mStatus)) {
      // Don't write any chunk to disk since this entry will be doomed
      LOG(
          ("CacheFile::DeactivateChunk() - Releasing chunk because of status "
           "[this=%p, chunk=%p, mStatus=0x%08" PRIx32 "]",
           this, chunk.get(), static_cast<uint32_t>(mStatus)));

      RemoveChunkInternal(chunk, false);
      return mStatus;
    }

    if (chunk->IsDirty() && !mMemoryOnly && !mOpeningFile) {
      LOG(
          ("CacheFile::DeactivateChunk() - Writing dirty chunk to the disk "
           "[this=%p]",
           this));

      mDataIsDirty = true;

      rv = chunk->Write(mHandle, this);
      if (NS_FAILED(rv)) {
        LOG(
            ("CacheFile::DeactivateChunk() - CacheFileChunk::Write() failed "
             "synchronously. Removing it. [this=%p, chunk=%p, rv=0x%08" PRIx32
             "]",
             this, chunk.get(), static_cast<uint32_t>(rv)));

        RemoveChunkInternal(chunk, false);

        SetError(rv);
        return rv;
      }

      // Chunk will be removed in OnChunkWritten if it is still unused

      // chunk needs to be released under the lock to be able to rely on
      // CacheFileChunk::mRefCnt in CacheFile::OnChunkWritten()
      chunk = nullptr;
      return NS_OK;
    }

    bool keepChunk = ShouldCacheChunk(aChunk->Index());
    LOG(("CacheFile::DeactivateChunk() - %s unused chunk [this=%p, chunk=%p]",
         keepChunk ? "Caching" : "Releasing", this, chunk.get()));

    RemoveChunkInternal(chunk, keepChunk);

    if (!mMemoryOnly) WriteMetadataIfNeededLocked();
  }

  return NS_OK;
}

void CacheFile::RemoveChunkInternal(CacheFileChunk* aChunk, bool aCacheChunk) {
  AssertOwnsLock();

  aChunk->mActiveChunk = false;
  ReleaseOutsideLock(RefPtr<CacheFileChunkListener>(std::move(aChunk->mFile)));

  if (aCacheChunk) {
    mCachedChunks.InsertOrUpdate(aChunk->Index(), RefPtr{aChunk});
  }

  mChunks.Remove(aChunk->Index());
}

bool CacheFile::OutputStreamExists(bool aAlternativeData) {
  AssertOwnsLock();

  if (!mOutput) {
    return false;
  }

  return mOutput->IsAlternativeData() == aAlternativeData;
}

int64_t CacheFile::BytesFromChunk(uint32_t aIndex, bool aAlternativeData) {
  AssertOwnsLock();

  int64_t dataSize;

  if (mAltDataOffset != -1) {
    if (aAlternativeData) {
      dataSize = mDataSize;
    } else {
      dataSize = mAltDataOffset;
    }
  } else {
    MOZ_ASSERT(!aAlternativeData);
    dataSize = mDataSize;
  }

  if (!dataSize) {
    return 0;
  }

  // Index of the last existing chunk.
  uint32_t lastChunk = (dataSize - 1) / kChunkSize;
  if (aIndex > lastChunk) {
    return 0;
  }

  // We can use only preloaded chunks for the given stream to calculate
  // available bytes if this is an entry stored on disk, since only those
  // chunks are guaranteed not to be released.
  uint32_t maxPreloadedChunk;
  if (mMemoryOnly) {
    maxPreloadedChunk = lastChunk;
  } else {
    maxPreloadedChunk = std::min(aIndex + mPreloadChunkCount, lastChunk);
  }

  uint32_t i;
  for (i = aIndex; i <= maxPreloadedChunk; ++i) {
    CacheFileChunk* chunk;

    chunk = mChunks.GetWeak(i);
    if (chunk) {
      MOZ_ASSERT(i == lastChunk || chunk->DataSize() == kChunkSize);
      if (chunk->IsReady()) {
        continue;
      }

      // don't search this chunk in cached
      break;
    }

    chunk = mCachedChunks.GetWeak(i);
    if (chunk) {
      MOZ_ASSERT(i == lastChunk || chunk->DataSize() == kChunkSize);
      continue;
    }

    break;
  }

  // theoretic bytes in advance
  int64_t advance = int64_t(i - aIndex) * kChunkSize;
  // real bytes till the end of the file
  int64_t tail = dataSize - (aIndex * kChunkSize);

  return std::min(advance, tail);
}

nsresult CacheFile::Truncate(int64_t aOffset) {
  AssertOwnsLock();

  LOG(("CacheFile::Truncate() [this=%p, offset=%" PRId64 "]", this, aOffset));

  nsresult rv;

  // If we ever need to truncate on non alt-data boundary, we need to handle
  // existing input streams.
  MOZ_ASSERT(aOffset == mAltDataOffset,
             "Truncating normal data not implemented");
  MOZ_ASSERT(mReady);
  MOZ_ASSERT(!mOutput);

  uint32_t lastChunk = 0;
  if (mDataSize > 0) {
    lastChunk = (mDataSize - 1) / kChunkSize;
  }

  uint32_t newLastChunk = 0;
  if (aOffset > 0) {
    newLastChunk = (aOffset - 1) / kChunkSize;
  }

  uint32_t bytesInNewLastChunk = aOffset - newLastChunk * kChunkSize;

  LOG(
      ("CacheFileTruncate() - lastChunk=%u, newLastChunk=%u, "
       "bytesInNewLastChunk=%u",
       lastChunk, newLastChunk, bytesInNewLastChunk));

  // Remove all truncated chunks from mCachedChunks
  for (auto iter = mCachedChunks.Iter(); !iter.Done(); iter.Next()) {
    uint32_t idx = iter.Key();

    if (idx > newLastChunk) {
      // This is unused chunk, simply remove it.
      LOG(("CacheFile::Truncate() - removing cached chunk [idx=%u]", idx));
      iter.Remove();
    }
  }

  // We need to make sure no input stream holds a reference to a chunk we're
  // going to discard. In theory, if alt-data begins at chunk boundary, input
  // stream for normal data can get the chunk containing only alt-data via
  // EnsureCorrectChunk() call. The input stream won't read the data from such
  // chunk, but it will keep the reference until the stream is closed and we
  // cannot simply discard this chunk.
  int64_t maxInputChunk = -1;
  for (uint32_t i = 0; i < mInputs.Length(); ++i) {
    int64_t inputChunk = mInputs[i]->GetChunkIdx();

    if (maxInputChunk < inputChunk) {
      maxInputChunk = inputChunk;
    }

    MOZ_RELEASE_ASSERT(mInputs[i]->GetPosition() <= aOffset);
  }

  MOZ_RELEASE_ASSERT(maxInputChunk <= newLastChunk + 1);
  if (maxInputChunk == newLastChunk + 1) {
    // Truncating must be done at chunk boundary
    MOZ_RELEASE_ASSERT(bytesInNewLastChunk == kChunkSize);
    newLastChunk++;
    bytesInNewLastChunk = 0;
    LOG(
        ("CacheFile::Truncate() - chunk %p is still in use, using "
         "newLastChunk=%u and bytesInNewLastChunk=%u",
         mChunks.GetWeak(newLastChunk), newLastChunk, bytesInNewLastChunk));
  }

  // Discard all truncated chunks in mChunks
  for (auto iter = mChunks.Iter(); !iter.Done(); iter.Next()) {
    uint32_t idx = iter.Key();

    if (idx > newLastChunk) {
      RefPtr<CacheFileChunk>& chunk = iter.Data();
      LOG(("CacheFile::Truncate() - discarding chunk [idx=%u, chunk=%p]", idx,
           chunk.get()));

      if (HaveChunkListeners(idx)) {
        NotifyChunkListeners(idx, NS_ERROR_NOT_AVAILABLE, chunk);
      }

      chunk->mDiscardedChunk = true;
      mDiscardedChunks.AppendElement(chunk);
      iter.Remove();
    }
  }

  // Remove hashes of all removed chunks from the metadata
  for (uint32_t i = lastChunk; i > newLastChunk; --i) {
    mMetadata->RemoveHash(i);
  }

  // Truncate new last chunk
  if (bytesInNewLastChunk == kChunkSize) {
    LOG(("CacheFile::Truncate() - not truncating last chunk."));
  } else {
    RefPtr<CacheFileChunk> chunk;
    if (mChunks.Get(newLastChunk, getter_AddRefs(chunk))) {
      LOG(("CacheFile::Truncate() - New last chunk %p got from mChunks.",
           chunk.get()));
    } else if (mCachedChunks.Get(newLastChunk, getter_AddRefs(chunk))) {
      LOG(("CacheFile::Truncate() - New last chunk %p got from mCachedChunks.",
           chunk.get()));
    } else {
      // New last chunk isn't loaded but we need to update the hash.
      MOZ_ASSERT(!mMemoryOnly);
      MOZ_ASSERT(mHandle);

      rv = GetChunkLocked(newLastChunk, PRELOADER, nullptr,
                          getter_AddRefs(chunk));
      if (NS_FAILED(rv)) {
        return rv;
      }
      // We've checked that we don't have this chunk, so no chunk must be
      // returned.
      MOZ_ASSERT(!chunk);

      if (!mChunks.Get(newLastChunk, getter_AddRefs(chunk))) {
        return NS_ERROR_UNEXPECTED;
      }

      LOG(("CacheFile::Truncate() - New last chunk %p got from preloader.",
           chunk.get()));
    }

    rv = chunk->GetStatus();
    if (NS_FAILED(rv)) {
      LOG(
          ("CacheFile::Truncate() - New last chunk is failed "
           "[status=0x%08" PRIx32 "]",
           static_cast<uint32_t>(rv)));
      return rv;
    }

    chunk->Truncate(bytesInNewLastChunk);

    // If the chunk is ready set the new hash now. If it's still being loaded
    // CacheChunk::Truncate() made the chunk dirty and the hash will be updated
    // in OnChunkWritten().
    if (chunk->IsReady()) {
      mMetadata->SetHash(newLastChunk, chunk->Hash());
    }
  }

  if (mHandle) {
    rv = CacheFileIOManager::TruncateSeekSetEOF(mHandle, aOffset, aOffset,
                                                nullptr);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mDataSize = aOffset;

  return NS_OK;
}

static uint32_t StatusToTelemetryEnum(nsresult aStatus) {
  if (NS_SUCCEEDED(aStatus)) {
    return 0;
  }

  switch (aStatus) {
    case NS_BASE_STREAM_CLOSED:
      return 0;  // Log this as a success
    case NS_ERROR_OUT_OF_MEMORY:
      return 2;
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
      return 3;
    case NS_ERROR_FILE_CORRUPTED:
      return 4;
    case NS_ERROR_FILE_NOT_FOUND:
      return 5;
    case NS_BINDING_ABORTED:
      return 6;
    default:
      return 1;  // other error
  }

  MOZ_ASSERT_UNREACHABLE("We should never get here");
}

void CacheFile::RemoveInput(CacheFileInputStream* aInput, nsresult aStatus) {
  AssertOwnsLock();

  LOG(("CacheFile::RemoveInput() [this=%p, input=%p, status=0x%08" PRIx32 "]",
       this, aInput, static_cast<uint32_t>(aStatus)));

  DebugOnly<bool> found;
  found = mInputs.RemoveElement(aInput);
  MOZ_ASSERT(found);

  ReleaseOutsideLock(
      already_AddRefed<nsIInputStream>(static_cast<nsIInputStream*>(aInput)));

  if (!mMemoryOnly) WriteMetadataIfNeededLocked();

  // If the input didn't read all data, there might be left some preloaded
  // chunks that won't be used anymore.
  CleanUpCachedChunks();

  Telemetry::Accumulate(Telemetry::NETWORK_CACHE_V2_INPUT_STREAM_STATUS,
                        StatusToTelemetryEnum(aStatus));
}

void CacheFile::RemoveOutput(CacheFileOutputStream* aOutput, nsresult aStatus) {
  AssertOwnsLock();

  nsresult rv;

  LOG(("CacheFile::RemoveOutput() [this=%p, output=%p, status=0x%08" PRIx32 "]",
       this, aOutput, static_cast<uint32_t>(aStatus)));

  if (mOutput != aOutput) {
    LOG(
        ("CacheFile::RemoveOutput() - This output was already removed, ignoring"
         " call [this=%p]",
         this));
    return;
  }

  mOutput = nullptr;

  // Cancel all queued chunk and update listeners that cannot be satisfied
  NotifyListenersAboutOutputRemoval();

  if (!mMemoryOnly) WriteMetadataIfNeededLocked();

  // Make sure the CacheFile status is set to a failure when the output stream
  // is closed with a fatal error.  This way we propagate correctly and w/o any
  // windows the failure state of this entry to end consumers.
  if (NS_SUCCEEDED(mStatus) && NS_FAILED(aStatus) &&
      aStatus != NS_BASE_STREAM_CLOSED) {
    if (aOutput->IsAlternativeData()) {
      MOZ_ASSERT(mAltDataOffset != -1);
      // If there is no alt-data input stream truncate only alt-data, otherwise
      // doom the entry.
      bool altDataInputExists = false;
      for (uint32_t i = 0; i < mInputs.Length(); ++i) {
        if (mInputs[i]->IsAlternativeData()) {
          altDataInputExists = true;
          break;
        }
      }
      if (altDataInputExists) {
        SetError(aStatus);
      } else {
        rv = Truncate(mAltDataOffset);
        if (NS_FAILED(rv)) {
          LOG(
              ("CacheFile::RemoveOutput() - Truncating alt-data failed "
               "[rv=0x%08" PRIx32 "]",
               static_cast<uint32_t>(rv)));
          SetError(aStatus);
        } else {
          SetAltMetadata(nullptr);
          mAltDataOffset = -1;
          mAltDataType.Truncate();
        }
      }
    } else {
      SetError(aStatus);
    }
  }

  // Notify close listener as the last action
  aOutput->NotifyCloseListener();

  Telemetry::Accumulate(Telemetry::NETWORK_CACHE_V2_OUTPUT_STREAM_STATUS,
                        StatusToTelemetryEnum(aStatus));
}

nsresult CacheFile::NotifyChunkListener(CacheFileChunkListener* aCallback,
                                        nsIEventTarget* aTarget,
                                        nsresult aResult, uint32_t aChunkIdx,
                                        CacheFileChunk* aChunk) {
  LOG(
      ("CacheFile::NotifyChunkListener() [this=%p, listener=%p, target=%p, "
       "rv=0x%08" PRIx32 ", idx=%u, chunk=%p]",
       this, aCallback, aTarget, static_cast<uint32_t>(aResult), aChunkIdx,
       aChunk));

  RefPtr<NotifyChunkListenerEvent> ev;
  ev = new NotifyChunkListenerEvent(aCallback, aResult, aChunkIdx, aChunk);
  if (aTarget) {
    return aTarget->Dispatch(ev, NS_DISPATCH_NORMAL);
  }
  return NS_DispatchToCurrentThread(ev);
}

void CacheFile::QueueChunkListener(uint32_t aIndex,
                                   CacheFileChunkListener* aCallback) {
  LOG(("CacheFile::QueueChunkListener() [this=%p, idx=%u, listener=%p]", this,
       aIndex, aCallback));

  AssertOwnsLock();

  MOZ_ASSERT(aCallback);

  ChunkListenerItem* item = new ChunkListenerItem();
  item->mTarget = CacheFileIOManager::IOTarget();
  if (!item->mTarget) {
    LOG(
        ("CacheFile::QueueChunkListener() - Cannot get Cache I/O thread! Using "
         "main thread for callback."));
    item->mTarget = GetMainThreadEventTarget();
  }
  item->mCallback = aCallback;

  mChunkListeners.GetOrInsertNew(aIndex)->mItems.AppendElement(item);
}

nsresult CacheFile::NotifyChunkListeners(uint32_t aIndex, nsresult aResult,
                                         CacheFileChunk* aChunk) {
  LOG(("CacheFile::NotifyChunkListeners() [this=%p, idx=%u, rv=0x%08" PRIx32
       ", "
       "chunk=%p]",
       this, aIndex, static_cast<uint32_t>(aResult), aChunk));

  AssertOwnsLock();

  nsresult rv, rv2;

  ChunkListeners* listeners;
  mChunkListeners.Get(aIndex, &listeners);
  MOZ_ASSERT(listeners);

  rv = NS_OK;
  for (uint32_t i = 0; i < listeners->mItems.Length(); i++) {
    ChunkListenerItem* item = listeners->mItems[i];
    rv2 = NotifyChunkListener(item->mCallback, item->mTarget, aResult, aIndex,
                              aChunk);
    if (NS_FAILED(rv2) && NS_SUCCEEDED(rv)) rv = rv2;
    delete item;
  }

  mChunkListeners.Remove(aIndex);

  return rv;
}

bool CacheFile::HaveChunkListeners(uint32_t aIndex) {
  ChunkListeners* listeners;
  mChunkListeners.Get(aIndex, &listeners);
  return !!listeners;
}

void CacheFile::NotifyListenersAboutOutputRemoval() {
  LOG(("CacheFile::NotifyListenersAboutOutputRemoval() [this=%p]", this));

  AssertOwnsLock();

  // First fail all chunk listeners that wait for non-existent chunk
  for (auto iter = mChunkListeners.Iter(); !iter.Done(); iter.Next()) {
    uint32_t idx = iter.Key();
    auto listeners = iter.UserData();

    LOG(
        ("CacheFile::NotifyListenersAboutOutputRemoval() - fail "
         "[this=%p, idx=%u]",
         this, idx));

    RefPtr<CacheFileChunk> chunk;
    mChunks.Get(idx, getter_AddRefs(chunk));
    if (chunk) {
      // Skip these listeners because the chunk is being read. We don't have
      // assertion here to check its state because it might be already in READY
      // state while CacheFile::OnChunkRead() is waiting on Cache I/O thread for
      // a lock so the listeners hasn't been notified yet. In any case, the
      // listeners will be notified from CacheFile::OnChunkRead().
      continue;
    }

    for (uint32_t i = 0; i < listeners->mItems.Length(); i++) {
      ChunkListenerItem* item = listeners->mItems[i];
      NotifyChunkListener(item->mCallback, item->mTarget,
                          NS_ERROR_NOT_AVAILABLE, idx, nullptr);
      delete item;
    }

    iter.Remove();
  }

  // Fail all update listeners
  for (const auto& entry : mChunks) {
    const RefPtr<CacheFileChunk>& chunk = entry.GetData();
    LOG(
        ("CacheFile::NotifyListenersAboutOutputRemoval() - fail2 "
         "[this=%p, idx=%u]",
         this, entry.GetKey()));

    if (chunk->IsReady()) {
      chunk->NotifyUpdateListeners();
    }
  }
}

bool CacheFile::DataSize(int64_t* aSize) {
  CacheFileAutoLock lock(this);

  if (OutputStreamExists(false)) {
    return false;
  }

  if (mAltDataOffset == -1) {
    *aSize = mDataSize;
  } else {
    *aSize = mAltDataOffset;
  }

  return true;
}

nsresult CacheFile::GetAltDataSize(int64_t* aSize) {
  CacheFileAutoLock lock(this);
  if (mOutput) {
    return NS_ERROR_IN_PROGRESS;
  }

  if (mAltDataOffset == -1) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aSize = mDataSize - mAltDataOffset;
  return NS_OK;
}

nsresult CacheFile::GetAltDataType(nsACString& aType) {
  CacheFileAutoLock lock(this);

  if (mAltDataOffset == -1) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aType = mAltDataType;
  return NS_OK;
}

bool CacheFile::IsDoomed() {
  CacheFileAutoLock lock(this);

  if (!mHandle) return false;

  return mHandle->IsDoomed();
}

bool CacheFile::IsWriteInProgress() {
  CacheFileAutoLock lock(this);

  bool result = false;

  if (!mMemoryOnly) {
    result =
        mDataIsDirty || (mMetadata && mMetadata->IsDirty()) || mWritingMetadata;
  }

  result = result || mOpeningFile || mOutput || mChunks.Count();

  return result;
}

bool CacheFile::EntryWouldExceedLimit(int64_t aOffset, int64_t aSize,
                                      bool aIsAltData) {
  CacheFileAutoLock lock(this);

  if (mSkipSizeCheck || aSize < 0) {
    return false;
  }

  int64_t totalSize = aOffset + aSize;
  if (aIsAltData) {
    totalSize += (mAltDataOffset == -1) ? mDataSize : mAltDataOffset;
  }

  if (CacheObserver::EntryIsTooBig(totalSize, !mMemoryOnly)) {
    return true;
  }

  return false;
}

bool CacheFile::IsDirty() { return mDataIsDirty || mMetadata->IsDirty(); }

void CacheFile::WriteMetadataIfNeeded() {
  LOG(("CacheFile::WriteMetadataIfNeeded() [this=%p]", this));

  CacheFileAutoLock lock(this);

  if (!mMemoryOnly) WriteMetadataIfNeededLocked();
}

void CacheFile::WriteMetadataIfNeededLocked(bool aFireAndForget) {
  // When aFireAndForget is set to true, we are called from dtor.
  // |this| must not be referenced after this method returns!

  LOG(("CacheFile::WriteMetadataIfNeededLocked() [this=%p]", this));

  nsresult rv;

  AssertOwnsLock();
  MOZ_ASSERT(!mMemoryOnly);

  if (!mMetadata) {
    MOZ_CRASH("Must have metadata here");
    return;
  }

  if (NS_FAILED(mStatus)) return;

  if (!IsDirty() || mOutput || mInputs.Length() || mChunks.Count() ||
      mWritingMetadata || mOpeningFile || mKill)
    return;

  if (!aFireAndForget) {
    // if aFireAndForget is set, we are called from dtor. Write
    // scheduler hard-refers CacheFile otherwise, so we cannot be here.
    CacheFileIOManager::UnscheduleMetadataWrite(this);
  }

  LOG(("CacheFile::WriteMetadataIfNeededLocked() - Writing metadata [this=%p]",
       this));

  rv = mMetadata->WriteMetadata(mDataSize, aFireAndForget ? nullptr : this);
  if (NS_SUCCEEDED(rv)) {
    mWritingMetadata = true;
    mDataIsDirty = false;
  } else {
    LOG(
        ("CacheFile::WriteMetadataIfNeededLocked() - Writing synchronously "
         "failed [this=%p]",
         this));
    // TODO: close streams with error
    SetError(rv);
  }
}

void CacheFile::PostWriteTimer() {
  if (mMemoryOnly) return;

  LOG(("CacheFile::PostWriteTimer() [this=%p]", this));

  CacheFileIOManager::ScheduleMetadataWrite(this);
}

void CacheFile::CleanUpCachedChunks() {
  for (auto iter = mCachedChunks.Iter(); !iter.Done(); iter.Next()) {
    uint32_t idx = iter.Key();
    const RefPtr<CacheFileChunk>& chunk = iter.Data();

    LOG(("CacheFile::CleanUpCachedChunks() [this=%p, idx=%u, chunk=%p]", this,
         idx, chunk.get()));

    if (MustKeepCachedChunk(idx)) {
      LOG(("CacheFile::CleanUpCachedChunks() - Keeping chunk"));
      continue;
    }

    LOG(("CacheFile::CleanUpCachedChunks() - Removing chunk"));
    iter.Remove();
  }
}

nsresult CacheFile::PadChunkWithZeroes(uint32_t aChunkIdx) {
  AssertOwnsLock();

  // This method is used to pad last incomplete chunk with zeroes or create
  // a new chunk full of zeroes
  MOZ_ASSERT(mDataSize / kChunkSize == aChunkIdx);

  nsresult rv;
  RefPtr<CacheFileChunk> chunk;
  rv = GetChunkLocked(aChunkIdx, WRITER, nullptr, getter_AddRefs(chunk));
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(
      ("CacheFile::PadChunkWithZeroes() - Zeroing hole in chunk %d, range %d-%d"
       " [this=%p]",
       aChunkIdx, chunk->DataSize(), kChunkSize - 1, this));

  CacheFileChunkWriteHandle hnd = chunk->GetWriteHandle(kChunkSize);
  if (!hnd.Buf()) {
    ReleaseOutsideLock(std::move(chunk));
    SetError(NS_ERROR_OUT_OF_MEMORY);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t offset = hnd.DataSize();
  memset(hnd.Buf() + offset, 0, kChunkSize - offset);
  hnd.UpdateDataSize(offset, kChunkSize - offset);

  ReleaseOutsideLock(std::move(chunk));

  return NS_OK;
}

void CacheFile::SetError(nsresult aStatus) {
  AssertOwnsLock();

  if (NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
    if (mHandle) {
      CacheFileIOManager::DoomFile(mHandle, nullptr);
    }
  }
}

nsresult CacheFile::InitIndexEntry() {
  MOZ_ASSERT(mHandle);

  if (mHandle->IsDoomed()) return NS_OK;

  nsresult rv;

  rv = CacheFileIOManager::InitIndexEntry(
      mHandle, GetOriginAttrsHash(mMetadata->OriginAttributes()),
      mMetadata->IsAnonymous(), mPinned);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t frecency = mMetadata->GetFrecency();

  bool hasAltData =
      mMetadata->GetElement(CacheFileUtils::kAltDataKey) ? true : false;

  static auto toUint16 = [](const char* s) -> uint16_t {
    if (s) {
      nsresult rv;
      uint64_t n64 = nsDependentCString(s).ToInteger64(&rv);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      return n64 <= kIndexTimeOutOfBound ? n64 : kIndexTimeOutOfBound;
    }
    return kIndexTimeNotAvailable;
  };

  const char* onStartTimeStr =
      mMetadata->GetElement("net-response-time-onstart");
  uint16_t onStartTime = toUint16(onStartTimeStr);

  const char* onStopTimeStr = mMetadata->GetElement("net-response-time-onstop");
  uint16_t onStopTime = toUint16(onStopTimeStr);

  const char* contentTypeStr = mMetadata->GetElement("ctid");
  uint8_t contentType = nsICacheEntry::CONTENT_TYPE_UNKNOWN;
  if (contentTypeStr) {
    int64_t n64 = nsDependentCString(contentTypeStr).ToInteger64(&rv);
    if (NS_FAILED(rv) || n64 < nsICacheEntry::CONTENT_TYPE_UNKNOWN ||
        n64 >= nsICacheEntry::CONTENT_TYPE_LAST) {
      n64 = nsICacheEntry::CONTENT_TYPE_UNKNOWN;
    }
    contentType = n64;
  }

  rv = CacheFileIOManager::UpdateIndexEntry(
      mHandle, &frecency, &hasAltData, &onStartTime, &onStopTime, &contentType);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

size_t CacheFile::SizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  CacheFileAutoLock lock(const_cast<CacheFile*>(this));

  size_t n = 0;
  n += mKey.SizeOfExcludingThisIfUnshared(mallocSizeOf);
  n += mChunks.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (const auto& chunk : mChunks.Values()) {
    n += chunk->SizeOfIncludingThis(mallocSizeOf);
  }
  n += mCachedChunks.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (const auto& chunk : mCachedChunks.Values()) {
    n += chunk->SizeOfIncludingThis(mallocSizeOf);
  }
  // Ignore metadata if it's still being read. It's not safe to access buffers
  // in CacheFileMetadata because they might be reallocated on another thread
  // outside CacheFile's lock.
  if (mMetadata && mReady) {
    n += mMetadata->SizeOfIncludingThis(mallocSizeOf);
  }

  // Input streams are not elsewhere reported.
  n += mInputs.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (uint32_t i = 0; i < mInputs.Length(); ++i) {
    n += mInputs[i]->SizeOfIncludingThis(mallocSizeOf);
  }

  // Output streams are not elsewhere reported.
  if (mOutput) {
    n += mOutput->SizeOfIncludingThis(mallocSizeOf);
  }

  // The listeners are usually classes reported just above.
  n += mChunkListeners.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mObjsToRelease.ShallowSizeOfExcludingThis(mallocSizeOf);

  // mHandle reported directly from CacheFileIOManager.

  return n;
}

size_t CacheFile::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

}  // namespace net
}  // namespace mozilla
