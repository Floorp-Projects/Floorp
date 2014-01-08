/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheEntry.h"
#include "CacheStorageService.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISeekableStream.h"
#include "nsIURI.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsICacheStorage.h"
#include "nsISerializable.h"
#include "nsIStreamTransportService.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsProxyRelease.h"
#include "nsSerializationHelper.h"
#include "nsThreadUtils.h"
#include "mozilla/Telemetry.h"
#include <math.h>
#include <algorithm>

namespace mozilla {
namespace net {

static uint32_t const ENTRY_WANTED =
  nsICacheEntryOpenCallback::ENTRY_WANTED;
static uint32_t const RECHECK_AFTER_WRITE_FINISHED =
  nsICacheEntryOpenCallback::RECHECK_AFTER_WRITE_FINISHED;
static uint32_t const ENTRY_NEEDS_REVALIDATION =
  nsICacheEntryOpenCallback::ENTRY_NEEDS_REVALIDATION;
static uint32_t const ENTRY_NOT_WANTED =
  nsICacheEntryOpenCallback::ENTRY_NOT_WANTED;

NS_IMPL_ISUPPORTS1(CacheEntryHandle, nsICacheEntry)

// CacheEntryHandle

CacheEntryHandle::CacheEntryHandle(CacheEntry* aEntry)
: mEntry(aEntry)
{
  MOZ_COUNT_CTOR(CacheEntryHandle);
  ++mEntry->mHandlersCount;

  LOG(("New CacheEntryHandle %p for entry %p", this, aEntry));
}

CacheEntryHandle::~CacheEntryHandle()
{
  --mEntry->mHandlersCount;
  mEntry->OnHandleClosed(this);

  MOZ_COUNT_DTOR(CacheEntryHandle);
}

// CacheEntry::Callback

CacheEntry::Callback::Callback(CacheEntry* aEntry,
                               nsICacheEntryOpenCallback *aCallback,
                               bool aReadOnly, bool aCheckOnAnyThread)
: mEntry(aEntry)
, mCallback(aCallback)
, mTargetThread(do_GetCurrentThread())
, mReadOnly(aReadOnly)
, mCheckOnAnyThread(aCheckOnAnyThread)
, mRecheckAfterWrite(false)
, mNotWanted(false)
{
  MOZ_COUNT_CTOR(CacheEntry::Callback);
  ++mEntry->mHandlersCount;
}

CacheEntry::Callback::Callback(CacheEntry::Callback const &aThat)
: mEntry(aThat.mEntry)
, mCallback(aThat.mCallback)
, mTargetThread(aThat.mTargetThread)
, mReadOnly(aThat.mReadOnly)
, mCheckOnAnyThread(aThat.mCheckOnAnyThread)
, mRecheckAfterWrite(aThat.mRecheckAfterWrite)
, mNotWanted(aThat.mNotWanted)
{
  MOZ_COUNT_CTOR(CacheEntry::Callback);
  ++mEntry->mHandlersCount;
}

CacheEntry::Callback::~Callback()
{
  --mEntry->mHandlersCount;
  MOZ_COUNT_DTOR(CacheEntry::Callback);
}

void CacheEntry::Callback::ExchangeEntry(CacheEntry* aEntry)
{
  if (mEntry == aEntry)
    return;

  ++aEntry->mHandlersCount;
  --mEntry->mHandlersCount;
  mEntry = aEntry;
}

nsresult CacheEntry::Callback::OnCheckThread(bool *aOnCheckThread) const
{
  if (!mCheckOnAnyThread) {
    // Check we are on the target
    return mTargetThread->IsOnCurrentThread(aOnCheckThread);
  }

  // We can invoke check anywhere
  *aOnCheckThread = true;
  return NS_OK;
}

nsresult CacheEntry::Callback::OnAvailThread(bool *aOnAvailThread) const
{
  return mTargetThread->IsOnCurrentThread(aOnAvailThread);
}

// CacheEntry

NS_IMPL_ISUPPORTS3(CacheEntry,
                   nsICacheEntry,
                   nsIRunnable,
                   CacheFileListener)

CacheEntry::CacheEntry(const nsACString& aStorageID,
                       nsIURI* aURI,
                       const nsACString& aEnhanceID,
                       bool aUseDisk)
: mFrecency(0)
, mSortingExpirationTime(uint32_t(-1))
, mLock("CacheEntry")
, mFileStatus(NS_ERROR_NOT_INITIALIZED)
, mURI(aURI)
, mEnhanceID(aEnhanceID)
, mStorageID(aStorageID)
, mUseDisk(aUseDisk)
, mIsDoomed(false)
, mSecurityInfoLoaded(false)
, mPreventCallbacks(false)
, mHasData(false)
, mState(NOTLOADED)
, mRegistration(NEVERREGISTERED)
, mWriter(nullptr)
, mPredictedDataSize(0)
, mDataSize(0)
, mReleaseThread(NS_GetCurrentThread())
{
  MOZ_COUNT_CTOR(CacheEntry);

  mService = CacheStorageService::Self();

  CacheStorageService::Self()->RecordMemoryOnlyEntry(
    this, !aUseDisk, true /* overwrite */);
}

CacheEntry::~CacheEntry()
{
  ProxyRelease(mURI, mReleaseThread);

  LOG(("CacheEntry::~CacheEntry [this=%p]", this));
  MOZ_COUNT_DTOR(CacheEntry);
}

#ifdef PR_LOG

char const * CacheEntry::StateString(uint32_t aState)
{
  switch (aState) {
  case NOTLOADED:     return "NOTLOADED";
  case LOADING:       return "LOADING";
  case EMPTY:         return "EMPTY";
  case WRITING:       return "WRITING";
  case READY:         return "READY";
  case REVALIDATING:  return "REVALIDATING";
  }

  return "?";
}

#endif

nsresult CacheEntry::HashingKeyWithStorage(nsACString &aResult)
{
  return HashingKey(mStorageID, mEnhanceID, mURI, aResult);
}

nsresult CacheEntry::HashingKey(nsACString &aResult)
{
  return HashingKey(EmptyCString(), mEnhanceID, mURI, aResult);
}

// static
nsresult CacheEntry::HashingKey(nsCSubstring const& aStorageID,
                                nsCSubstring const& aEnhanceID,
                                nsIURI* aURI,
                                nsACString &aResult)
{
  /**
   * This key is used to salt hash that is a base for disk file name.
   * Changing it will cause we will not be able to find files on disk.
   */

  if (aStorageID.Length()) {
    aResult.Append(aStorageID);
    aResult.Append(':');
  }

  if (aEnhanceID.Length()) {
    aResult.Append(aEnhanceID);
    aResult.Append(':');
  }

  nsAutoCString spec;
  nsresult rv = aURI->GetAsciiSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  aResult.Append(spec);

  return NS_OK;
}

void CacheEntry::AsyncOpen(nsICacheEntryOpenCallback* aCallback, uint32_t aFlags)
{
  LOG(("CacheEntry::AsyncOpen [this=%p, state=%s, flags=%d, callback=%p]",
    this, StateString(mState), aFlags, aCallback));

  bool readonly = aFlags & nsICacheStorage::OPEN_READONLY;
  bool truncate = aFlags & nsICacheStorage::OPEN_TRUNCATE;
  bool priority = aFlags & nsICacheStorage::OPEN_PRIORITY;
  bool multithread = aFlags & nsICacheStorage::CHECK_MULTITHREADED;

  MOZ_ASSERT(!readonly || !truncate, "Bad flags combination");
  MOZ_ASSERT(!(truncate && mState > LOADING), "Must not call truncate on already loaded entry");

  Callback callback(this, aCallback, readonly, multithread);

  mozilla::MutexAutoLock lock(mLock);

  RememberCallback(callback);

  // Load() opens the lock
  if (Load(truncate, priority)) {
    // Loading is in progress...
    return;
  }

  InvokeCallbacks();
}

bool CacheEntry::Load(bool aTruncate, bool aPriority)
{
  LOG(("CacheEntry::Load [this=%p, trunc=%d]", this, aTruncate));

  mLock.AssertCurrentThreadOwns();

  if (mState > LOADING) {
    LOG(("  already loaded"));
    return false;
  }

  if (mState == LOADING) {
    LOG(("  already loading"));
    return true;
  }

  mState = LOADING;

  MOZ_ASSERT(!mFile);

  bool directLoad = aTruncate || !mUseDisk;
  if (directLoad)
    mFileStatus = NS_OK;
  else
    mLoadStart = TimeStamp::Now();

  mFile = new CacheFile();

  BackgroundOp(Ops::REGISTER);

  {
    mozilla::MutexAutoUnlock unlock(mLock);

    nsresult rv;

    nsAutoCString fileKey;
    rv = HashingKeyWithStorage(fileKey);

    LOG(("  performing load, file=%p", mFile.get()));
    if (NS_SUCCEEDED(rv)) {
      rv = mFile->Init(fileKey,
                       aTruncate,
                       !mUseDisk,
                       aPriority,
                       false /* key is not a hash */,
                       directLoad ? nullptr : this);
    }

    if (NS_FAILED(rv)) {
      mFileStatus = rv;
      AsyncDoom(nullptr);
      return false;
    }
  }

  if (directLoad) {
    // Just fake the load has already been done as "new".
    mState = EMPTY;
  }

  return mState == LOADING;
}

NS_IMETHODIMP CacheEntry::OnFileReady(nsresult aResult, bool aIsNew)
{
  LOG(("CacheEntry::OnFileReady [this=%p, rv=0x%08x, new=%d]",
      this, aResult, aIsNew));

  MOZ_ASSERT(!mLoadStart.IsNull());

  if (NS_SUCCEEDED(aResult)) {
    if (aIsNew) {
      mozilla::Telemetry::AccumulateTimeDelta(
        mozilla::Telemetry::NETWORK_CACHE_V2_MISS_TIME_MS,
        mLoadStart);
    }
    else {
      mozilla::Telemetry::AccumulateTimeDelta(
        mozilla::Telemetry::NETWORK_CACHE_V2_HIT_TIME_MS,
        mLoadStart);
    }
  }

  // OnFileReady, that is the only code that can transit from LOADING
  // to any follow-on state, can only be invoked ones on an entry,
  // thus no need to lock.  Until this moment there is no consumer that
  // could manipulate the entry state.
  mozilla::MutexAutoLock lock(mLock);

  MOZ_ASSERT(mState == LOADING);

  mState = (aIsNew || NS_FAILED(aResult))
    ? EMPTY
    : READY;

  mFileStatus = aResult;

  if (mState == READY)
    mHasData = true;

  InvokeCallbacks();
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::OnFileDoomed(nsresult aResult)
{
  if (mDoomCallback) {
    nsRefPtr<DoomCallbackRunnable> event =
      new DoomCallbackRunnable(this, aResult);
    NS_DispatchToMainThread(event);
  }

  return NS_OK;
}

already_AddRefed<CacheEntry> CacheEntry::ReopenTruncated(nsICacheEntryOpenCallback* aCallback)
{
  LOG(("CacheEntry::ReopenTruncated [this=%p]", this));

  mLock.AssertCurrentThreadOwns();

  // Hold callbacks invocation, AddStorageEntry would invoke from doom prematurly
  mPreventCallbacks = true;

  nsRefPtr<CacheEntryHandle> handle;
  nsRefPtr<CacheEntry> newEntry;
  {
    mozilla::MutexAutoUnlock unlock(mLock);

    // The following call dooms this entry (calls DoomAlreadyRemoved on us)
    nsresult rv = CacheStorageService::Self()->AddStorageEntry(
      GetStorageID(), GetURI(), GetEnhanceID(),
      mUseDisk,
      true, // always create
      true, // truncate existing (this one)
      getter_AddRefs(handle));

    LOG(("  exchanged entry %p by entry %p, rv=0x%08x", this, newEntry.get(), rv));

    if (NS_SUCCEEDED(rv)) {
      newEntry = handle->Entry();
      newEntry->AsyncOpen(aCallback, nsICacheStorage::OPEN_TRUNCATE);
    }
    else {
      AsyncDoom(nullptr);
    }
  }

  mPreventCallbacks = false;

  if (!newEntry)
    return nullptr;

  newEntry->TransferCallbacks(*this);
  mCallbacks.Clear();

  return newEntry.forget();
}

void CacheEntry::TransferCallbacks(CacheEntry & aFromEntry)
{
  mozilla::MutexAutoLock lock(mLock);

  LOG(("CacheEntry::TransferCallbacks [entry=%p, from=%p]",
    this, &aFromEntry));

  if (!mCallbacks.Length())
    mCallbacks.SwapElements(aFromEntry.mCallbacks);
  else
    mCallbacks.AppendElements(aFromEntry.mCallbacks);

  uint32_t callbacksLength = mCallbacks.Length();
  if (callbacksLength) {
    // Carry the entry reference (unfortunatelly, needs to be done manually...)
    for (uint32_t i = 0; i < callbacksLength; ++i)
      mCallbacks[i].ExchangeEntry(this);

    BackgroundOp(Ops::CALLBACKS, true);
  }
}

void CacheEntry::RememberCallback(Callback const& aCallback)
{
  LOG(("CacheEntry::RememberCallback [this=%p, cb=%p]", this, aCallback.mCallback.get()));

  mLock.AssertCurrentThreadOwns();

  mCallbacks.AppendElement(aCallback);
}

void CacheEntry::InvokeCallbacksLock()
{
  mozilla::MutexAutoLock lock(mLock);
  InvokeCallbacks();
}

void CacheEntry::InvokeCallbacks()
{
  mLock.AssertCurrentThreadOwns();

  LOG(("CacheEntry::InvokeCallbacks BEGIN [this=%p]", this));

  // Invoke first all r/w callbacks, then all r/o callbacks.
  if (InvokeCallbacks(false))
    InvokeCallbacks(true);

  LOG(("CacheEntry::InvokeCallbacks END [this=%p]", this));
}

bool CacheEntry::InvokeCallbacks(bool aReadOnly)
{
  mLock.AssertCurrentThreadOwns();

  uint32_t i = 0;
  while (i < mCallbacks.Length()) {
    if (mPreventCallbacks) {
      LOG(("  callbacks prevented!"));
      return false;
    }

    if (!mIsDoomed && (mState == WRITING || mState == REVALIDATING)) {
      LOG(("  entry is being written/revalidated"));
      return false;
    }

    if (mCallbacks[i].mReadOnly != aReadOnly) {
      // Callback is not r/w or r/o, go to another one in line
      ++i;
      continue;
    }

    bool onCheckThread;
    nsresult rv = mCallbacks[i].OnCheckThread(&onCheckThread);

    if (NS_SUCCEEDED(rv) && !onCheckThread) {
      // Redispatch to the target thread
      nsRefPtr<nsRunnableMethod<CacheEntry> > event =
        NS_NewRunnableMethod(this, &CacheEntry::InvokeCallbacksLock);

      rv = mCallbacks[i].mTargetThread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
      if (NS_SUCCEEDED(rv)) {
        LOG(("  re-dispatching to target thread"));
        return false;
      }
    }

    Callback callback = mCallbacks[i];
    mCallbacks.RemoveElementAt(i);

    if (NS_SUCCEEDED(rv) && !InvokeCallback(callback)) {
      // Callback didn't fire, put it back and go to another one in line.
      // Only reason InvokeCallback returns false is that onCacheEntryCheck
      // returns RECHECK_AFTER_WRITE_FINISHED.  If we would stop the loop, other
      // readers or potential writers would be unnecessarily kept from being
      // invoked.
      mCallbacks.InsertElementAt(i, callback);
      ++i;
    }
  }

  return true;
}

bool CacheEntry::InvokeCallback(Callback & aCallback)
{
  LOG(("CacheEntry::InvokeCallback [this=%p, state=%s, cb=%p]",
    this, StateString(mState), aCallback.mCallback.get()));

  mLock.AssertCurrentThreadOwns();

  // When this entry is doomed we want to notify the callback any time
  if (!mIsDoomed) {
    // When we are here, the entry must be loaded from disk
    MOZ_ASSERT(mState > LOADING);

    if (mState == WRITING || mState == REVALIDATING) {
      // Prevent invoking other callbacks since one of them is now writing
      // or revalidating this entry.  No consumers should get this entry
      // until metadata are filled with values downloaded from the server
      // or the entry revalidated and output stream has been opened.
      LOG(("  entry is being written/revalidated, callback bypassed"));
      return false;
    }

    // mRecheckAfterWrite flag already set means the callback has already passed
    // the onCacheEntryCheck call. Until the current write is not finished this
    // callback will be bypassed.
    if (!aCallback.mRecheckAfterWrite) {

      if (!aCallback.mReadOnly) {
        if (mState == EMPTY) {
          // Advance to writing state, we expect to invoke the callback and let
          // it fill content of this entry.  Must set and check the state here
          // to prevent more then one
          mState = WRITING;
          LOG(("  advancing to WRITING state"));
        }

        if (!aCallback.mCallback) {
          // We can be given no callback only in case of recreate, it is ok
          // to advance to WRITING state since the caller of recreate is expected
          // to write this entry now.
          return true;
        }
      }

      if (mState == READY) {
        // Metadata present, validate the entry
        uint32_t checkResult;
        {
          // mayhemer: TODO check and solve any potential races of concurent OnCacheEntryCheck
          mozilla::MutexAutoUnlock unlock(mLock);

          nsresult rv = aCallback.mCallback->OnCacheEntryCheck(
            this, nullptr, &checkResult);
          LOG(("  OnCacheEntryCheck: rv=0x%08x, result=%d", rv, checkResult));

          if (NS_FAILED(rv))
            checkResult = ENTRY_NOT_WANTED;
        }

        switch (checkResult) {
        case ENTRY_WANTED:
          // Nothing more to do here, the consumer is responsible to handle
          // the result of OnCacheEntryCheck it self.
          // Proceed to callback...
          break;

        case RECHECK_AFTER_WRITE_FINISHED:
          LOG(("  consumer will check on the entry again after write is done"));
          // The consumer wants the entry to complete first.
          aCallback.mRecheckAfterWrite = true;
          break;

        case ENTRY_NEEDS_REVALIDATION:
          LOG(("  will be holding callbacks until entry is revalidated"));
          // State is READY now and from that state entry cannot transit to any other
          // state then REVALIDATING for which cocurrency is not an issue.  Potentially
          // no need to lock here.
          mState = REVALIDATING;
          break;

        case ENTRY_NOT_WANTED:
          LOG(("  consumer not interested in the entry"));
          // Do not give this entry to the consumer, it is not interested in us.
          aCallback.mNotWanted = true;
          break;
        }
      }
    }
  }

  if (aCallback.mCallback) {
    if (!mIsDoomed && aCallback.mRecheckAfterWrite) {
      // If we don't have data and the callback wants a complete entry,
      // don't invoke now.
      bool bypass = !mHasData;
      if (!bypass) {
        int64_t _unused;
        bypass = !mFile->DataSize(&_unused);
      }

      if (bypass) {
        LOG(("  bypassing, entry data still being written"));
        return false;
      }

      // Entry is complete now, do the check+avail call again
      aCallback.mRecheckAfterWrite = false;
      return InvokeCallback(aCallback);
    }

    mozilla::MutexAutoUnlock unlock(mLock);
    InvokeAvailableCallback(aCallback);
  }

  return true;
}

void CacheEntry::InvokeAvailableCallback(Callback const & aCallback)
{
  LOG(("CacheEntry::InvokeAvailableCallback [this=%p, state=%s, cb=%p, r/o=%d, n/w=%d]",
    this, StateString(mState), aCallback.mCallback.get(), aCallback.mReadOnly, aCallback.mNotWanted));

  nsresult rv;

  uint32_t const state = mState;

  // When we are here, the entry must be loaded from disk
  MOZ_ASSERT(state > LOADING || mIsDoomed);

  bool onAvailThread;
  rv = aCallback.OnAvailThread(&onAvailThread);
  if (NS_FAILED(rv)) {
    LOG(("  target thread dead?"));
    return;
  }

  if (!onAvailThread) {
    // Dispatch to the right thread
    nsRefPtr<AvailableCallbackRunnable> event =
      new AvailableCallbackRunnable(this, aCallback);

    rv = aCallback.mTargetThread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
    LOG(("  redispatched, (rv = 0x%08x)", rv));
    return;
  }

  if (mIsDoomed || aCallback.mNotWanted) {
    LOG(("  doomed or not wanted, notifying OCEA with NS_ERROR_CACHE_KEY_NOT_FOUND"));
    aCallback.mCallback->OnCacheEntryAvailable(
      nullptr, false, nullptr, NS_ERROR_CACHE_KEY_NOT_FOUND);
    return;
  }

  if (state == READY) {
    LOG(("  ready/has-meta, notifying OCEA with entry and NS_OK"));
    {
      mozilla::MutexAutoLock lock(mLock);
      BackgroundOp(Ops::FRECENCYUPDATE);
    }

    nsRefPtr<CacheEntryHandle> handle = NewHandle();
    aCallback.mCallback->OnCacheEntryAvailable(
      handle, false, nullptr, NS_OK);
    return;
  }

  if (aCallback.mReadOnly) {
    LOG(("  r/o and not ready, notifying OCEA with NS_ERROR_CACHE_KEY_NOT_FOUND"));
    aCallback.mCallback->OnCacheEntryAvailable(
      nullptr, false, nullptr, NS_ERROR_CACHE_KEY_NOT_FOUND);
    return;
  }

  // This is a new or potentially non-valid entry and needs to be fetched first.
  // The CacheEntryHandle blocks other consumers until the channel
  // either releases the entry or marks metadata as filled or whole entry valid,
  // i.e. until MetaDataReady() or SetValid() on the entry is called respectively.

  // Consumer will be responsible to fill or validate the entry metadata and data.

  nsRefPtr<CacheEntryHandle> handle = NewWriteHandle();
  rv = aCallback.mCallback->OnCacheEntryAvailable(
    handle, state == WRITING, nullptr, NS_OK);

  if (NS_FAILED(rv)) {
    LOG(("  writing/revalidating failed (0x%08x)", rv));

    // Consumer given a new entry failed to take care of the entry.
    OnHandleClosed(handle);
    return;
  }

  LOG(("  writing/revalidating"));
}

CacheEntryHandle* CacheEntry::NewHandle()
{
  return new CacheEntryHandle(this);
}

CacheEntryHandle* CacheEntry::NewWriteHandle()
{
  mozilla::MutexAutoLock lock(mLock);

  BackgroundOp(Ops::FRECENCYUPDATE);
  return (mWriter = new CacheEntryHandle(this));
}

void CacheEntry::OnHandleClosed(CacheEntryHandle const* aHandle)
{
  LOG(("CacheEntry::OnHandleClosed [this=%p, state=%s, handle=%p]", this, StateString(mState), aHandle));

  nsCOMPtr<nsIOutputStream> outputStream;

  {
    mozilla::MutexAutoLock lock(mLock);

    if (mWriter != aHandle) {
      LOG(("  not the writer"));
      return;
    }

    if (mOutputStream) {
      // No one took our internal output stream, so there are no data
      // and output stream has to be open symultaneously with input stream
      // on this entry again.
      mHasData = false;
    }

    outputStream.swap(mOutputStream);
    mWriter = nullptr;

    if (mState == WRITING) {
      LOG(("  reverting to state EMPTY - write failed"));
      mState = EMPTY;
    }
    else if (mState == REVALIDATING) {
      LOG(("  reverting to state READY - reval failed"));
      mState = READY;
    }

    InvokeCallbacks();
  }

  if (outputStream) {
    LOG(("  abandoning phantom output stream"));
    outputStream->Close();
  }
}

void CacheEntry::OnOutputClosed()
{
  // Called when the file's output stream is closed.  Invoke any callbacks
  // waiting for complete entry.

  mozilla::MutexAutoLock lock(mLock);
  InvokeCallbacks();
}

bool CacheEntry::UsingDisk() const
{
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  return mUseDisk;
}

bool CacheEntry::SetUsingDisk(bool aUsingDisk)
{
  // Called by the service when this entry is reopen to reflect
  // demanded storage target.

  if (mState >= READY) {
    // Don't modify after this entry has been filled.
    return false;
  }

  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  bool changed = mUseDisk != aUsingDisk;
  mUseDisk = aUsingDisk;
  return changed;
}

bool CacheEntry::IsReferenced() const
{
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  // No need to lock, since:
  // 1. increasing this counter from 0 to non-null happens only under
  //    the the service lock
  // 2. this check is made also only under the service lock
  return mHandlersCount > 0;
}

uint32_t CacheEntry::GetMetadataMemoryConsumption()
{
  NS_ENSURE_SUCCESS(mFileStatus, 0);

  uint32_t size;
  if (NS_FAILED(mFile->ElementsSize(&size)))
    return 0;

  return size;
}

// nsICacheEntry

NS_IMETHODIMP CacheEntry::GetPersistToDisk(bool *aPersistToDisk)
{
  // No need to sync when only reading.
  // When consumer needs to be consistent with state of the memory storage entries
  // table, then let it use GetUseDisk getter that must be called under the service lock.
  *aPersistToDisk = mUseDisk;
  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetPersistToDisk(bool aPersistToDisk)
{
  LOG(("CacheEntry::SetPersistToDisk [this=%p, persist=%d]", this, aPersistToDisk));

  if (mState >= READY) {
    LOG(("  failed, called after filling the entry"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mUseDisk == aPersistToDisk)
    return NS_OK;

  mozilla::MutexAutoLock lock(CacheStorageService::Self()->Lock());

  mUseDisk = aPersistToDisk;
  CacheStorageService::Self()->RecordMemoryOnlyEntry(
    this, !aPersistToDisk, false /* don't overwrite */);

  // File persistence is setup just before we open output stream on it.

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetKey(nsACString & aKey)
{
  return mURI->GetAsciiSpec(aKey);
}

NS_IMETHODIMP CacheEntry::GetFetchCount(int32_t *aFetchCount)
{
  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  return mFile->GetFetchCount(reinterpret_cast<uint32_t*>(aFetchCount));
}

NS_IMETHODIMP CacheEntry::GetLastFetched(uint32_t *aLastFetched)
{
  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  return mFile->GetLastFetched(aLastFetched);
}

NS_IMETHODIMP CacheEntry::GetLastModified(uint32_t *aLastModified)
{
  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  return mFile->GetLastModified(aLastModified);
}

NS_IMETHODIMP CacheEntry::GetExpirationTime(uint32_t *aExpirationTime)
{
  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  return mFile->GetExpirationTime(aExpirationTime);
}

NS_IMETHODIMP CacheEntry::SetExpirationTime(uint32_t aExpirationTime)
{
  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  nsresult rv = mFile->SetExpirationTime(aExpirationTime);
  NS_ENSURE_SUCCESS(rv, rv);

  // Aligned assignment, thus atomic.
  mSortingExpirationTime = aExpirationTime;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::OpenInputStream(int64_t offset, nsIInputStream * *_retval)
{
  LOG(("CacheEntry::OpenInputStream [this=%p]", this));

  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  nsresult rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = mFile->OpenInputStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISeekableStream> seekable =
    do_QueryInterface(stream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::MutexAutoLock lock(mLock);

  if (!mHasData) {
    // So far output stream on this new entry not opened, do it now.
    LOG(("  creating phantom output stream"));
    rv = OpenOutputStreamInternal(0, getter_AddRefs(mOutputStream));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  stream.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::OpenOutputStream(int64_t offset, nsIOutputStream * *_retval)
{
  LOG(("CacheEntry::OpenOutputStream [this=%p]", this));

  nsresult rv;

  mozilla::MutexAutoLock lock(mLock);

  MOZ_ASSERT(mState > EMPTY);

  if (mOutputStream) {
    LOG(("  giving phantom output stream"));
    mOutputStream.forget(_retval);
  }
  else {
    rv = OpenOutputStreamInternal(offset, _retval);
    if (NS_FAILED(rv)) return rv;
  }

  // Entry considered ready when writer opens output stream.
  if (mState < READY)
    mState = READY;

  // Invoke any pending readers now.
  InvokeCallbacks();

  return NS_OK;
}

nsresult CacheEntry::OpenOutputStreamInternal(int64_t offset, nsIOutputStream * *_retval)
{
  LOG(("CacheEntry::OpenOutputStreamInternal [this=%p]", this));

  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  mLock.AssertCurrentThreadOwns();

  if (mIsDoomed) {
    LOG(("  doomed..."));
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(mState > LOADING);

  nsresult rv;

  // No need to sync on mUseDisk here, we don't need to be consistent
  // with content of the memory storage entries hash table.
  if (!mUseDisk) {
    rv = mFile->SetMemoryOnly();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsRefPtr<CacheOutputCloseListener> listener =
    new CacheOutputCloseListener(this);

  nsCOMPtr<nsIOutputStream> stream;
  rv = mFile->OpenOutputStream(listener, getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISeekableStream> seekable =
    do_QueryInterface(stream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Prevent opening output stream again.
  mHasData = true;

  stream.swap(*_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetPredictedDataSize(int64_t *aPredictedDataSize)
{
  *aPredictedDataSize = mPredictedDataSize;
  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetPredictedDataSize(int64_t aPredictedDataSize)
{
  mPredictedDataSize = aPredictedDataSize;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  {
    mozilla::MutexAutoLock lock(mLock);
    if (mSecurityInfoLoaded) {
      NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
      return NS_OK;
    }
  }

  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  char const* info;
  nsCOMPtr<nsISupports> secInfo;
  nsresult rv;

  rv = mFile->GetElement("security-info", &info);
  NS_ENSURE_SUCCESS(rv, rv);

  if (info) {
    rv = NS_DeserializeObject(nsDependentCString(info),
                              getter_AddRefs(secInfo));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  {
    mozilla::MutexAutoLock lock(mLock);

    mSecurityInfo.swap(secInfo);
    mSecurityInfoLoaded = true;

    NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  }

  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetSecurityInfo(nsISupports *aSecurityInfo)
{
  nsresult rv;

  NS_ENSURE_SUCCESS(mFileStatus, mFileStatus);

  {
    mozilla::MutexAutoLock lock(mLock);

    mSecurityInfo = aSecurityInfo;
    mSecurityInfoLoaded = true;
  }

  nsCOMPtr<nsISerializable> serializable =
    do_QueryInterface(aSecurityInfo);
  if (aSecurityInfo && !serializable)
    return NS_ERROR_UNEXPECTED;

  nsCString info;
  if (serializable) {
    rv = NS_SerializeToString(serializable, info);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mFile->SetElement("security-info", info.Length() ? info.get() : nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetStorageDataSize(uint32_t *aStorageDataSize)
{
  NS_ENSURE_ARG(aStorageDataSize);

  int64_t dataSize;
  nsresult rv = GetDataSize(&dataSize);
  if (NS_FAILED(rv))
    return rv;

  *aStorageDataSize = (uint32_t)std::min(int64_t(uint32_t(-1)), dataSize);

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::AsyncDoom(nsICacheEntryDoomCallback *aCallback)
{
  LOG(("CacheEntry::AsyncDoom [this=%p]", this));

  {
    mozilla::MutexAutoLock lock(mLock);

    if (mIsDoomed || mDoomCallback)
      return NS_ERROR_IN_PROGRESS; // to aggregate have DOOMING state

    mIsDoomed = true;
    mDoomCallback = aCallback;
    BackgroundOp(Ops::DOOM);
  }

  // Immediately remove the entry from the storage hash table
  CacheStorageService::Self()->RemoveEntry(this);

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetMetaDataElement(const char * aKey, char * *aRetval)
{
  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  const char *value;
  nsresult rv = mFile->GetElement(aKey, &value);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!value)
    return NS_ERROR_NOT_AVAILABLE;

  *aRetval = NS_strdup(value);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::SetMetaDataElement(const char * aKey, const char * aValue)
{
  NS_ENSURE_SUCCESS(mFileStatus, NS_ERROR_NOT_AVAILABLE);

  return mFile->SetElement(aKey, aValue);
}

NS_IMETHODIMP CacheEntry::MetaDataReady()
{
  mozilla::MutexAutoLock lock(mLock);

  LOG(("CacheEntry::MetaDataReady [this=%p, state=%s]", this, StateString(mState)));

  MOZ_ASSERT(mState > EMPTY);

  if (mState == WRITING)
    mState = READY;

  InvokeCallbacks();

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::SetValid()
{
  LOG(("CacheEntry::SetValid [this=%p, state=%s]", this, StateString(mState)));

  nsCOMPtr<nsIOutputStream> outputStream;

  {
    mozilla::MutexAutoLock lock(mLock);

    MOZ_ASSERT(mState > EMPTY);

    mState = READY;
    mHasData = true;

    InvokeCallbacks();

    outputStream.swap(mOutputStream);
  }

  if (outputStream) {
    LOG(("  abandoning phantom output stream"));
    outputStream->Close();
  }

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::Recreate(nsICacheEntry **_retval)
{
  LOG(("CacheEntry::Recreate [this=%p, state=%s]", this, StateString(mState)));

  mozilla::MutexAutoLock lock(mLock);

  nsRefPtr<CacheEntry> newEntry = ReopenTruncated(nullptr);
  if (newEntry) {
    nsRefPtr<CacheEntryHandle> handle = newEntry->NewWriteHandle();
    handle.forget(_retval);
    return NS_OK;
  }

  BackgroundOp(Ops::CALLBACKS, true);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::SetDataSize(uint32_t size)
{
  // ?
  mDataSize = size;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetDataSize(int64_t *aDataSize)
{
  LOG(("CacheEntry::GetDataSize [this=%p]", this));
  *aDataSize = 0;

  {
    mozilla::MutexAutoLock lock(mLock);

    if (!mHasData) {
      LOG(("  write in progress (no data)"));
      return NS_ERROR_IN_PROGRESS;
    }
  }

  NS_ENSURE_SUCCESS(mFileStatus, mFileStatus);

  // mayhemer: TODO Problem with compression?
  if (!mFile->DataSize(aDataSize)) {
    LOG(("  write in progress (stream active)"));
    return NS_ERROR_IN_PROGRESS;
  }

  LOG(("  size=%lld", *aDataSize));
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::MarkValid()
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::MaybeMarkValid()
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::HasWriteAccess(bool aWriteAllowed, bool *aWriteAccess)
{
  *aWriteAccess = aWriteAllowed;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::Close()
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetStoragePolicy(nsCacheStoragePolicy *aStoragePolicy)
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetStoragePolicy(nsCacheStoragePolicy aStoragePolicy)
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}

// nsIRunnable

NS_IMETHODIMP CacheEntry::Run()
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  mozilla::MutexAutoLock lock(mLock);

  BackgroundOp(mBackgroundOperations.Grab());
  return NS_OK;
}

// Management methods

double CacheEntry::GetFrecency() const
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return mFrecency;
}

uint32_t CacheEntry::GetExpirationTime() const
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return mSortingExpirationTime;
}

bool CacheEntry::IsRegistered() const
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return mRegistration == REGISTERED;
}

bool CacheEntry::CanRegister() const
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return mRegistration == NEVERREGISTERED;
}

void CacheEntry::SetRegistered(bool aRegistered)
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  if (aRegistered) {
    MOZ_ASSERT(mRegistration == NEVERREGISTERED);
    mRegistration = REGISTERED;
  }
  else {
    MOZ_ASSERT(mRegistration == REGISTERED);
    mRegistration = DEREGISTERED;
  }
}

bool CacheEntry::Purge(uint32_t aWhat)
{
  LOG(("CacheEntry::Purge [this=%p, what=%d]", this, aWhat));

  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  switch (aWhat) {
  case PURGE_DATA_ONLY_DISK_BACKED:
  case PURGE_WHOLE_ONLY_DISK_BACKED:
    // This is an in-memory only entry, don't purge it
    if (!mUseDisk) {
      LOG(("  not using disk"));
      return false;
    }
  }

  if (mState == WRITING || mState == LOADING || mFrecency == 0) {
    // In-progress (write or load) entries should (at least for consistency and from
    // the logical point of view) stay in memory.
    // Zero-frecency entries are those which have never been given to any consumer, those
    // are actually very fresh and should not go just because frecency had not been set
    // so far.
    LOG(("  state=%s, frecency=%1.10f", StateString(mState), mFrecency));
    return false;
  }

  switch (aWhat) {
  case PURGE_WHOLE_ONLY_DISK_BACKED:
  case PURGE_WHOLE:
    {
      if (!CacheStorageService::Self()->RemoveEntry(this, true)) {
        LOG(("  not purging, still referenced"));
        return false;
      }

      CacheStorageService::Self()->UnregisterEntry(this);

      // Entry removed it self from control arrays, return true
      return true;
    }

  case PURGE_DATA_ONLY_DISK_BACKED:
    {
      NS_ENSURE_SUCCESS(mFileStatus, false);

      mFile->ThrowMemoryCachedData();

      // Entry has been left in control arrays, return false (not purged)
      return false;
    }
  }

  LOG(("  ?"));
  return false;
}

void CacheEntry::PurgeAndDoom()
{
  LOG(("CacheEntry::PurgeAndDoom [this=%p]", this));

  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  CacheStorageService::Self()->RemoveEntry(this);
  DoomAlreadyRemoved();
}

void CacheEntry::DoomAlreadyRemoved()
{
  LOG(("CacheEntry::DoomAlreadyRemoved [this=%p]", this));

  mIsDoomed = true;

  if (!CacheStorageService::IsOnManagementThread()) {
    mozilla::MutexAutoLock lock(mLock);

    BackgroundOp(Ops::DOOM);
    return;
  }

  CacheStorageService::Self()->UnregisterEntry(this);

  {
    mozilla::MutexAutoLock lock(mLock);

    if (mCallbacks.Length()) {
      // Must force post here since may be indirectly called from
      // InvokeCallbacks of this entry and we don't want reentrancy here.
      BackgroundOp(Ops::CALLBACKS, true);
    }
  }

  if (NS_SUCCEEDED(mFileStatus)) {
    nsresult rv = mFile->Doom(mDoomCallback ? this : nullptr);
    if (NS_SUCCEEDED(rv)) {
      LOG(("  file doomed"));
      return;
    }
  }

  OnFileDoomed(NS_OK);
}

void CacheEntry::BackgroundOp(uint32_t aOperations, bool aForceAsync)
{
  mLock.AssertCurrentThreadOwns();

  if (!CacheStorageService::IsOnManagementThread() || aForceAsync) {
    if (mBackgroundOperations.Set(aOperations))
      CacheStorageService::Self()->Dispatch(this);

    LOG(("CacheEntry::BackgroundOp this=%p dipatch of %x", this, aOperations));
    return;
  }

  mozilla::MutexAutoUnlock unlock(mLock);

  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  if (aOperations & Ops::FRECENCYUPDATE) {
    #ifndef M_LN2
    #define M_LN2 0.69314718055994530942
    #endif

    // Half-life is 90 days.
    static double const half_life = 90.0 * (24 * 60 * 60);
    // Must convert from seconds to milliseconds since PR_Now() gives usecs.
    static double const decay = (M_LN2 / half_life) / static_cast<double>(PR_USEC_PER_SEC);

    double now_decay = static_cast<double>(PR_Now()) * decay;

    if (mFrecency == 0) {
      mFrecency = now_decay;
    }
    else {
      // TODO: when C++11 enabled, use std::log1p(n) which is equal to log(n + 1) but
      // more precise.
      mFrecency = log(exp(mFrecency - now_decay) + 1) + now_decay;
    }
    LOG(("CacheEntry FRECENCYUPDATE [this=%p, frecency=%1.10f]", this, mFrecency));
  }

  if (aOperations & Ops::REGISTER) {
    LOG(("CacheEntry REGISTER [this=%p]", this));

    CacheStorageService::Self()->RegisterEntry(this);
  }

  if (aOperations & Ops::DOOM) {
    LOG(("CacheEntry DOOM [this=%p]", this));

    DoomAlreadyRemoved();
  }

  if (aOperations & Ops::CALLBACKS) {
    LOG(("CacheEntry CALLBACKS (invoke) [this=%p]", this));

    mozilla::MutexAutoLock lock(mLock);
    InvokeCallbacks();
  }
}

// CacheOutputCloseListener

CacheOutputCloseListener::CacheOutputCloseListener(CacheEntry* aEntry)
: mEntry(aEntry)
{
  MOZ_COUNT_CTOR(CacheOutputCloseListener);
}

CacheOutputCloseListener::~CacheOutputCloseListener()
{
  MOZ_COUNT_DTOR(CacheOutputCloseListener);
}

void CacheOutputCloseListener::OnOutputClosed()
{
  // We need this class and to redispatch since this callback is invoked
  // under the file's lock and to do the job we need to enter the entry's
  // lock too.  That would lead to potential deadlocks.
  NS_DispatchToCurrentThread(this);
}

NS_IMETHODIMP CacheOutputCloseListener::Run()
{
  mEntry->OnOutputClosed();
  return NS_OK;
}

} // net
} // mozilla
