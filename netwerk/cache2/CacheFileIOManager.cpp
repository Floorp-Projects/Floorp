/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileIOManager.h"

#include "../cache/nsCacheUtils.h"
#include "CacheHashUtils.h"
#include "CacheStorageService.h"
#include "CacheIndex.h"
#include "nsThreadUtils.h"
#include "CacheFile.h"
#include "nsIFile.h"
#include "mozilla/Telemetry.h"
#include "mozilla/DebugOnly.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "private/pprio.h"
#include "mozilla/VisualEventTracer.h"

// include files for ftruncate (or equivalent)
#if defined(XP_UNIX)
#include <unistd.h>
#elif defined(XP_WIN)
#include <windows.h>
#undef CreateFile
#undef CREATE_NEW
#else
// XXX add necessary include file for ftruncate (or equivalent)
#endif


namespace mozilla {
namespace net {

#define kOpenHandlesLimit   64
#define kMetadataWriteDelay 5000

bool
CacheFileHandle::DispatchRelease()
{
  if (CacheFileIOManager::IsOnIOThreadOrCeased())
    return false;

  nsCOMPtr<nsIEventTarget> ioTarget = CacheFileIOManager::IOTarget();
  if (!ioTarget)
    return false;

  nsRefPtr<nsRunnableMethod<CacheFileHandle, nsrefcnt, false> > event =
    NS_NewNonOwningRunnableMethod(this, &CacheFileHandle::Release);
  nsresult rv = ioTarget->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
  if (NS_FAILED(rv))
    return false;

  return true;
}

NS_IMPL_ADDREF(CacheFileHandle)
NS_IMETHODIMP_(nsrefcnt)
CacheFileHandle::Release()
{
  nsrefcnt count = mRefCnt - 1;
  if (DispatchRelease()) {
    // Redispatched to the IO thread.
    return count;
  }

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());

  LOG(("CacheFileHandle::Release() [this=%p, refcnt=%d]", this, mRefCnt.get()));
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "CacheFileHandle");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileHandle)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileHandle::CacheFileHandle(const SHA1Sum::Hash *aHash, bool aPriority)
  : mHash(aHash)
  , mIsDoomed(false)
  , mPriority(aPriority)
  , mClosed(false)
  , mInvalid(false)
  , mFileExists(false)
  , mFileSize(-1)
  , mFD(nullptr)
{
  LOG(("CacheFileHandle::CacheFileHandle() [this=%p, hash=%08x%08x%08x%08x%08x]"
       , this, LOGSHA1(aHash)));
}

CacheFileHandle::CacheFileHandle(const nsACString &aKey, bool aPriority)
  : mHash(nullptr)
  , mIsDoomed(false)
  , mPriority(aPriority)
  , mClosed(false)
  , mInvalid(false)
  , mFileExists(false)
  , mFileSize(-1)
  , mFD(nullptr)
  , mKey(aKey)
{
  LOG(("CacheFileHandle::CacheFileHandle() [this=%p, key=%s]", this,
       PromiseFlatCString(aKey).get()));
}

CacheFileHandle::~CacheFileHandle()
{
  LOG(("CacheFileHandle::~CacheFileHandle() [this=%p]", this));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());

  nsRefPtr<CacheFileIOManager> ioMan = CacheFileIOManager::gInstance;
  if (ioMan) {
    ioMan->CloseHandleInternal(this);
  }
}

void
CacheFileHandle::Log()
{
  nsAutoCString leafName;
  if (mFile) {
    mFile->GetNativeLeafName(leafName);
  }

  if (!mHash) {
    // special file
    LOG(("CacheFileHandle::Log() [this=%p, hash=nullptr, isDoomed=%d, "
         "priority=%d, closed=%d, invalid=%d, "
         "fileExists=%d, fileSize=%lld, leafName=%s, key=%s]",
         this, mIsDoomed, mPriority, mClosed, mInvalid,
         mFileExists, mFileSize, leafName.get(), mKey.get()));
  }
  else {
    LOG(("CacheFileHandle::Log() [this=%p, hash=%08x%08x%08x%08x%08x, "
         "isDoomed=%d, priority=%d, closed=%d, invalid=%d, "
         "fileExists=%d, fileSize=%lld, leafName=%s, key=%s]",
         this, LOGSHA1(mHash), mIsDoomed, mPriority, mClosed,
         mInvalid, mFileExists, mFileSize, leafName.get(), mKey.get()));
  }
}

uint32_t
CacheFileHandle::FileSizeInK()
{
  MOZ_ASSERT(mFileSize != -1);
  uint64_t size64 = mFileSize;

  size64 += 0x3FF;
  size64 >>= 10;

  uint32_t size;
  if (size64 >> 32) {
    NS_WARNING("CacheFileHandle::FileSizeInK() - FileSize is too large, "
               "truncating to PR_UINT32_MAX");
    size = PR_UINT32_MAX;
  }
  else {
    size = static_cast<uint32_t>(size64);
  }

  return size;
}

/******************************************************************************
 *  CacheFileHandles::HandleHashKey
 *****************************************************************************/

void
CacheFileHandles::HandleHashKey::AddHandle(CacheFileHandle* aHandle)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());

  mHandles.InsertElementAt(0, aHandle);
}

void
CacheFileHandles::HandleHashKey::RemoveHandle(CacheFileHandle* aHandle)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());

  DebugOnly<bool> found;
  found = mHandles.RemoveElement(aHandle);
  MOZ_ASSERT(found);
}

already_AddRefed<CacheFileHandle>
CacheFileHandles::HandleHashKey::GetNewestHandle()
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());

  nsRefPtr<CacheFileHandle> handle;
  if (mHandles.Length())
    handle = mHandles[0];

  return handle.forget();
}

void
CacheFileHandles::HandleHashKey::GetHandles(nsTArray<nsRefPtr<CacheFileHandle> > &aResult)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());

  for (uint32_t i = 0; i < mHandles.Length(); ++i) {
    CacheFileHandle* handle = mHandles[i];
    aResult.AppendElement(handle);
  }
}

#ifdef DEBUG

void
CacheFileHandles::HandleHashKey::AssertHandlesState()
{
  for (uint32_t i = 0; i < mHandles.Length(); ++i) {
    CacheFileHandle* handle = mHandles[i];
    MOZ_ASSERT(handle->IsDoomed());
  }
}

#endif

/******************************************************************************
 *  CacheFileHandles
 *****************************************************************************/

CacheFileHandles::CacheFileHandles()
{
  LOG(("CacheFileHandles::CacheFileHandles() [this=%p]", this));
  MOZ_COUNT_CTOR(CacheFileHandles);
}

CacheFileHandles::~CacheFileHandles()
{
  LOG(("CacheFileHandles::~CacheFileHandles() [this=%p]", this));
  MOZ_COUNT_DTOR(CacheFileHandles);
}

nsresult
CacheFileHandles::GetHandle(const SHA1Sum::Hash *aHash,
                            bool aReturnDoomed,
                            CacheFileHandle **_retval)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());
  MOZ_ASSERT(aHash);

#ifdef DEBUG_HANDLES
  LOG(("CacheFileHandles::GetHandle() [hash=%08x%08x%08x%08x%08x]",
       LOGSHA1(aHash)));
#endif

  // find hash entry for key
  HandleHashKey *entry = mTable.GetEntry(*aHash);
  if (!entry) {
    LOG(("CacheFileHandles::GetHandle() hash=%08x%08x%08x%08x%08x "
         "no handle entries found", LOGSHA1(aHash)));
    return NS_ERROR_NOT_AVAILABLE;
  }

#ifdef DEBUG_HANDLES
  Log(entry);
#endif

  // Check if the entry is doomed
  nsRefPtr<CacheFileHandle> handle = entry->GetNewestHandle();
  if (!handle) {
    LOG(("CacheFileHandles::GetHandle() hash=%08x%08x%08x%08x%08x "
         "no handle found %p, entry %p", LOGSHA1(aHash), handle.get(), entry));
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (handle->IsDoomed()) {
    LOG(("CacheFileHandles::GetHandle() hash=%08x%08x%08x%08x%08x "
         "found doomed handle %p, entry %p", LOGSHA1(aHash), handle.get(), entry));

    // If the consumer doesn't want doomed handles, exit with NOT_AVAIL.
    if (!aReturnDoomed)
      return NS_ERROR_NOT_AVAILABLE;
  } else {
    LOG(("CacheFileHandles::GetHandle() hash=%08x%08x%08x%08x%08x "
         "found handle %p, entry %p", LOGSHA1(aHash), handle.get(), entry));
  }

  handle.forget(_retval);
  return NS_OK;
}


nsresult
CacheFileHandles::NewHandle(const SHA1Sum::Hash *aHash,
                            bool aPriority,
                            CacheFileHandle **_retval)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());
  MOZ_ASSERT(aHash);

#ifdef DEBUG_HANDLES
  LOG(("CacheFileHandles::NewHandle() [hash=%08x%08x%08x%08x%08x]", LOGSHA1(aHash)));
#endif

  // find hash entry for key
  HandleHashKey *entry = mTable.PutEntry(*aHash);

#ifdef DEBUG_HANDLES
  Log(entry);
#endif

#ifdef DEBUG
  entry->AssertHandlesState();
#endif

  nsRefPtr<CacheFileHandle> handle = new CacheFileHandle(entry->Hash(), aPriority);
  entry->AddHandle(handle);

  LOG(("CacheFileHandles::NewHandle() hash=%08x%08x%08x%08x%08x "
       "created new handle %p, entry=%p", LOGSHA1(aHash), handle.get(), entry));

  handle.forget(_retval);
  return NS_OK;
}

void
CacheFileHandles::RemoveHandle(CacheFileHandle *aHandle)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());
  MOZ_ASSERT(aHandle);

  if (!aHandle)
    return;

#ifdef DEBUG_HANDLES
  LOG(("CacheFileHandles::RemoveHandle() [handle=%p, hash=%08x%08x%08x%08x%08x]"
       , aHandle, LOGSHA1(aHandle->Hash())));
#endif

  // find hash entry for key
  HandleHashKey *entry = mTable.GetEntry(*aHandle->Hash());
  if (!entry) {
    MOZ_ASSERT(CacheFileIOManager::IsShutdown(),
      "Should find entry when removing a handle before shutdown");

    LOG(("CacheFileHandles::RemoveHandle() hash=%08x%08x%08x%08x%08x "
         "no entries found", LOGSHA1(aHandle->Hash())));
    return;
  }

#ifdef DEBUG_HANDLES
  Log(entry);
#endif

  LOG(("CacheFileHandles::RemoveHandle() hash=%08x%08x%08x%08x%08x "
       "removing handle %p", LOGSHA1(entry->Hash()), aHandle));
  entry->RemoveHandle(aHandle);

  if (entry->IsEmpty()) {
    LOG(("CacheFileHandles::RemoveHandle() hash=%08x%08x%08x%08x%08x "
         "list is empty, removing entry %p", LOGSHA1(entry->Hash()), entry));
    mTable.RemoveEntry(*entry->Hash());
  }
}

static PLDHashOperator
GetAllHandlesEnum(CacheFileHandles::HandleHashKey* aEntry, void *aClosure)
{
  nsTArray<nsRefPtr<CacheFileHandle> > *array =
    static_cast<nsTArray<nsRefPtr<CacheFileHandle> > *>(aClosure);

  aEntry->GetHandles(*array);
  return PL_DHASH_NEXT;
}

void
CacheFileHandles::GetAllHandles(nsTArray<nsRefPtr<CacheFileHandle> > *_retval)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());
  mTable.EnumerateEntries(&GetAllHandlesEnum, _retval);
}

void
CacheFileHandles::ClearAll()
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());
  mTable.Clear();
}

uint32_t
CacheFileHandles::HandleCount()
{
  return mTable.Count();
}

#ifdef DEBUG_HANDLES
void
CacheFileHandles::Log(CacheFileHandlesEntry *entry)
{
  LOG(("CacheFileHandles::Log() BEGIN [entry=%p]", entry));

  nsTArray<nsRefPtr<CacheFileHandle> > array;
  aEntry->GetHandles(array);

  for (uint32_t i = 0; i < array.Length(); ++i) {
    CacheFileHandle *handle = array[i];
    handle->Log();
  }

  LOG(("CacheFileHandles::Log() END [entry=%p]", entry));
}
#endif

// Events

class ShutdownEvent : public nsRunnable {
public:
  ShutdownEvent(mozilla::Mutex *aLock, mozilla::CondVar *aCondVar)
    : mLock(aLock)
    , mCondVar(aCondVar)
  {
    MOZ_COUNT_CTOR(ShutdownEvent);
  }

  ~ShutdownEvent()
  {
    MOZ_COUNT_DTOR(ShutdownEvent);
  }

  NS_IMETHOD Run()
  {
    MutexAutoLock lock(*mLock);

    CacheFileIOManager::gInstance->ShutdownInternal();

    mCondVar->Notify();
    return NS_OK;
  }

protected:
  mozilla::Mutex   *mLock;
  mozilla::CondVar *mCondVar;
};

class OpenFileEvent : public nsRunnable {
public:
  OpenFileEvent(const nsACString &aKey,
                uint32_t aFlags,
                CacheFileIOListener *aCallback)
    : mFlags(aFlags)
    , mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
    , mKey(aKey)
  {
    MOZ_COUNT_CTOR(OpenFileEvent);

    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
    mIOMan = CacheFileIOManager::gInstance;
    MOZ_ASSERT(mTarget);

    MOZ_EVENT_TRACER_NAME_OBJECT(static_cast<nsIRunnable*>(this), aKey.BeginReading());
    MOZ_EVENT_TRACER_WAIT(static_cast<nsIRunnable*>(this), "net::cache::open-background");
  }

  ~OpenFileEvent()
  {
    MOZ_COUNT_DTOR(OpenFileEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      mRV = NS_OK;

      if (mFlags & CacheFileIOManager::SPECIAL_FILE) {
      }
      else if (mFlags & CacheFileIOManager::NOHASH) {
        nsACString::const_char_iterator begin, end;
        begin = mKey.BeginReading();
        end = mKey.EndReading();
        uint32_t i = 0;
        while (begin != end && i < (SHA1Sum::HashSize << 1)) {
          if (!(i & 1))
            mHash[i >> 1] = 0;
          uint8_t shift = (i & 1) ? 0 : 4;
          if (*begin >= '0' && *begin <= '9')
            mHash[i >> 1] |= (*begin - '0') << shift;
          else if (*begin >= 'A' && *begin <= 'F')
            mHash[i >> 1] |= (*begin - 'A' + 10) << shift;
          else
            break;

          ++i;
          ++begin;
        }

        if (i != (SHA1Sum::HashSize << 1) || begin != end)
          mRV = NS_ERROR_INVALID_ARG;
      }
      else {
        SHA1Sum sum;
        sum.update(mKey.BeginReading(), mKey.Length());
        sum.finish(mHash);
      }

      MOZ_EVENT_TRACER_EXEC(static_cast<nsIRunnable*>(this),
                            "net::cache::open-background");
      if (NS_SUCCEEDED(mRV)) {
        if (!mIOMan)
          mRV = NS_ERROR_NOT_INITIALIZED;
        else {
          if (mFlags & CacheFileIOManager::SPECIAL_FILE)
            mRV = mIOMan->OpenSpecialFileInternal(mKey, mFlags,
                                                  getter_AddRefs(mHandle));
          else
            mRV = mIOMan->OpenFileInternal(&mHash, mFlags,
                                           getter_AddRefs(mHandle));
          mIOMan = nullptr;
          if (mHandle) {
            MOZ_EVENT_TRACER_NAME_OBJECT(mHandle.get(), mKey.get());
            if (mHandle->Key().IsEmpty())
              mHandle->Key() = mKey;
          }
        }
      }
      MOZ_EVENT_TRACER_DONE(static_cast<nsIRunnable*>(this), "net::cache::open-background");

      MOZ_EVENT_TRACER_WAIT(static_cast<nsIRunnable*>(this), "net::cache::open-result");
      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      MOZ_EVENT_TRACER_EXEC(static_cast<nsIRunnable*>(this), "net::cache::open-result");
      mCallback->OnFileOpened(mHandle, mRV);
      MOZ_EVENT_TRACER_DONE(static_cast<nsIRunnable*>(this), "net::cache::open-result");
    }
    return NS_OK;
  }

protected:
  SHA1Sum::Hash                 mHash;
  uint32_t                      mFlags;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsRefPtr<CacheFileIOManager>  mIOMan;
  nsRefPtr<CacheFileHandle>     mHandle;
  nsresult                      mRV;
  nsCString                     mKey;
};

class ReadEvent : public nsRunnable {
public:
  ReadEvent(CacheFileHandle *aHandle, int64_t aOffset, char *aBuf,
            int32_t aCount, CacheFileIOListener *aCallback)
    : mHandle(aHandle)
    , mOffset(aOffset)
    , mBuf(aBuf)
    , mCount(aCount)
    , mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(ReadEvent);
    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());

    MOZ_EVENT_TRACER_NAME_OBJECT(static_cast<nsIRunnable*>(this), aHandle->Key().get());
    MOZ_EVENT_TRACER_WAIT(static_cast<nsIRunnable*>(this), "net::cache::read-background");
  }

  ~ReadEvent()
  {
    MOZ_COUNT_DTOR(ReadEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      MOZ_EVENT_TRACER_EXEC(static_cast<nsIRunnable*>(this), "net::cache::read-background");
      if (mHandle->IsClosed())
        mRV = NS_ERROR_NOT_INITIALIZED;
      else
        mRV = CacheFileIOManager::gInstance->ReadInternal(
          mHandle, mOffset, mBuf, mCount);
      MOZ_EVENT_TRACER_DONE(static_cast<nsIRunnable*>(this), "net::cache::read-background");

      MOZ_EVENT_TRACER_WAIT(static_cast<nsIRunnable*>(this), "net::cache::read-result");
      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      MOZ_EVENT_TRACER_EXEC(static_cast<nsIRunnable*>(this), "net::cache::read-result");
      if (mCallback)
        mCallback->OnDataRead(mHandle, mBuf, mRV);
      MOZ_EVENT_TRACER_DONE(static_cast<nsIRunnable*>(this), "net::cache::read-result");
    }
    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle>     mHandle;
  int64_t                       mOffset;
  char                         *mBuf;
  int32_t                       mCount;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsresult                      mRV;
};

class WriteEvent : public nsRunnable {
public:
  WriteEvent(CacheFileHandle *aHandle, int64_t aOffset, const char *aBuf,
             int32_t aCount, bool aValidate, CacheFileIOListener *aCallback)
    : mHandle(aHandle)
    , mOffset(aOffset)
    , mBuf(aBuf)
    , mCount(aCount)
    , mValidate(aValidate)
    , mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(WriteEvent);
    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());

    MOZ_EVENT_TRACER_NAME_OBJECT(static_cast<nsIRunnable*>(this), aHandle->Key().get());
    MOZ_EVENT_TRACER_WAIT(static_cast<nsIRunnable*>(this), "net::cache::write-background");
  }

  ~WriteEvent()
  {
    MOZ_COUNT_DTOR(WriteEvent);

    if (!mCallback && mBuf) {
      free(const_cast<char *>(mBuf));
    }
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      MOZ_EVENT_TRACER_EXEC(static_cast<nsIRunnable*>(this), "net::cache::write-background");
      if (mHandle->IsClosed())
        mRV = NS_ERROR_NOT_INITIALIZED;
      else
        mRV = CacheFileIOManager::gInstance->WriteInternal(
          mHandle, mOffset, mBuf, mCount, mValidate);
      MOZ_EVENT_TRACER_DONE(static_cast<nsIRunnable*>(this), "net::cache::write-background");

      MOZ_EVENT_TRACER_WAIT(static_cast<nsIRunnable*>(this), "net::cache::write-result");
      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      MOZ_EVENT_TRACER_EXEC(static_cast<nsIRunnable*>(this), "net::cache::write-result");
      if (mCallback)
        mCallback->OnDataWritten(mHandle, mBuf, mRV);
      else {
        free(const_cast<char *>(mBuf));
        mBuf = nullptr;
      }
      MOZ_EVENT_TRACER_DONE(static_cast<nsIRunnable*>(this), "net::cache::write-result");
    }
    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle>     mHandle;
  int64_t                       mOffset;
  const char                   *mBuf;
  int32_t                       mCount;
  bool                          mValidate;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsresult                      mRV;
};

class DoomFileEvent : public nsRunnable {
public:
  DoomFileEvent(CacheFileHandle *aHandle,
                CacheFileIOListener *aCallback)
    : mCallback(aCallback)
    , mHandle(aHandle)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(DoomFileEvent);
    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());

    MOZ_EVENT_TRACER_NAME_OBJECT(static_cast<nsIRunnable*>(this), aHandle->Key().get());
    MOZ_EVENT_TRACER_WAIT(static_cast<nsIRunnable*>(this), "net::cache::doom-background");
  }

  ~DoomFileEvent()
  {
    MOZ_COUNT_DTOR(DoomFileEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      MOZ_EVENT_TRACER_EXEC(static_cast<nsIRunnable*>(this), "net::cache::doom-background");
      if (mHandle->IsClosed())
        mRV = NS_ERROR_NOT_INITIALIZED;
      else
        mRV = CacheFileIOManager::gInstance->DoomFileInternal(mHandle);
      MOZ_EVENT_TRACER_DONE(static_cast<nsIRunnable*>(this), "net::cache::doom-background");

      MOZ_EVENT_TRACER_WAIT(static_cast<nsIRunnable*>(this), "net::cache::doom-result");
      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      MOZ_EVENT_TRACER_EXEC(static_cast<nsIRunnable*>(this), "net::cache::doom-result");
      if (mCallback)
        mCallback->OnFileDoomed(mHandle, mRV);
      MOZ_EVENT_TRACER_DONE(static_cast<nsIRunnable*>(this), "net::cache::doom-result");
    }
    return NS_OK;
  }

protected:
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsRefPtr<CacheFileHandle>     mHandle;
  nsresult                      mRV;
};

class DoomFileByKeyEvent : public nsRunnable {
public:
  DoomFileByKeyEvent(const nsACString &aKey,
                     CacheFileIOListener *aCallback)
    : mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(DoomFileByKeyEvent);

    SHA1Sum sum;
    sum.update(aKey.BeginReading(), aKey.Length());
    sum.finish(mHash);

    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
    mIOMan = CacheFileIOManager::gInstance;
    MOZ_ASSERT(mTarget);
  }

  ~DoomFileByKeyEvent()
  {
    MOZ_COUNT_DTOR(DoomFileByKeyEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      if (!mIOMan)
        mRV = NS_ERROR_NOT_INITIALIZED;
      else {
        mRV = mIOMan->DoomFileByKeyInternal(&mHash);
        mIOMan = nullptr;
      }

      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      if (mCallback)
        mCallback->OnFileDoomed(nullptr, mRV);
    }
    return NS_OK;
  }

protected:
  SHA1Sum::Hash                 mHash;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsRefPtr<CacheFileIOManager>  mIOMan;
  nsresult                      mRV;
};

class ReleaseNSPRHandleEvent : public nsRunnable {
public:
  ReleaseNSPRHandleEvent(CacheFileHandle *aHandle)
    : mHandle(aHandle)
  {
    MOZ_COUNT_CTOR(ReleaseNSPRHandleEvent);
  }

  ~ReleaseNSPRHandleEvent()
  {
    MOZ_COUNT_DTOR(ReleaseNSPRHandleEvent);
  }

  NS_IMETHOD Run()
  {
    if (mHandle->mFD && !mHandle->IsClosed())
      CacheFileIOManager::gInstance->ReleaseNSPRHandleInternal(mHandle);

    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle>     mHandle;
};

class TruncateSeekSetEOFEvent : public nsRunnable {
public:
  TruncateSeekSetEOFEvent(CacheFileHandle *aHandle, int64_t aTruncatePos,
                          int64_t aEOFPos, CacheFileIOListener *aCallback)
    : mHandle(aHandle)
    , mTruncatePos(aTruncatePos)
    , mEOFPos(aEOFPos)
    , mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(TruncateSeekSetEOFEvent);
    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
  }

  ~TruncateSeekSetEOFEvent()
  {
    MOZ_COUNT_DTOR(TruncateSeekSetEOFEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      if (mHandle->IsClosed())
        mRV = NS_ERROR_NOT_INITIALIZED;
      else
        mRV = CacheFileIOManager::gInstance->TruncateSeekSetEOFInternal(
          mHandle, mTruncatePos, mEOFPos);

      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      if (mCallback)
        mCallback->OnEOFSet(mHandle, mRV);
    }
    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle>     mHandle;
  int64_t                       mTruncatePos;
  int64_t                       mEOFPos;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsresult                      mRV;
};

class RenameFileEvent : public nsRunnable {
public:
  RenameFileEvent(CacheFileHandle *aHandle, const nsACString &aNewName,
                  CacheFileIOListener *aCallback)
    : mHandle(aHandle)
    , mNewName(aNewName)
    , mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(RenameFileEvent);
    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
  }

  ~RenameFileEvent()
  {
    MOZ_COUNT_DTOR(RenameFileEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      if (mHandle->IsClosed())
        mRV = NS_ERROR_NOT_INITIALIZED;
      else
        mRV = CacheFileIOManager::gInstance->RenameFileInternal(mHandle,
                                                                mNewName);

      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      if (mCallback)
        mCallback->OnFileRenamed(mHandle, mRV);
    }
    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle>     mHandle;
  nsCString                     mNewName;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsresult                      mRV;
};

class InitIndexEntryEvent : public nsRunnable {
public:
  InitIndexEntryEvent(CacheFileHandle *aHandle, uint32_t aAppId,
                      bool aAnonymous, bool aInBrowser)
    : mHandle(aHandle)
    , mAppId(aAppId)
    , mAnonymous(aAnonymous)
    , mInBrowser(aInBrowser)
  {
    MOZ_COUNT_CTOR(InitIndexEntryEvent);
  }

  ~InitIndexEntryEvent()
  {
    MOZ_COUNT_DTOR(InitIndexEntryEvent);
  }

  NS_IMETHOD Run()
  {
    if (mHandle->IsClosed() || mHandle->IsDoomed())
      return NS_OK;

    CacheIndex::InitEntry(mHandle->Hash(), mAppId, mAnonymous, mInBrowser);

    // We cannot set the filesize before we init the entry. If we're opening
    // an existing entry file, frecency and expiration time will be set after
    // parsing the entry file, but we must set the filesize here since nobody is
    // going to set it if there is no write to the file.
    uint32_t sizeInK = mHandle->FileSizeInK();
    CacheIndex::UpdateEntry(mHandle->Hash(), nullptr, nullptr, &sizeInK);

    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle> mHandle;
  uint32_t                  mAppId;
  bool                      mAnonymous;
  bool                      mInBrowser;
};

class UpdateIndexEntryEvent : public nsRunnable {
public:
  UpdateIndexEntryEvent(CacheFileHandle *aHandle, const uint32_t *aFrecency,
                        const uint32_t *aExpirationTime)
    : mHandle(aHandle)
    , mHasFrecency(false)
    , mHasExpirationTime(false)
  {
    MOZ_COUNT_CTOR(UpdateIndexEntryEvent);
    if (aFrecency) {
      mHasFrecency = true;
      mFrecency = *aFrecency;
    }
    if (aExpirationTime) {
      mHasExpirationTime = true;
      mExpirationTime = *aExpirationTime;
    }
  }

  ~UpdateIndexEntryEvent()
  {
    MOZ_COUNT_DTOR(UpdateIndexEntryEvent);
  }

  NS_IMETHOD Run()
  {
    if (mHandle->IsClosed() || mHandle->IsDoomed())
      return NS_OK;

    CacheIndex::UpdateEntry(mHandle->Hash(),
                            mHasFrecency ? &mFrecency : nullptr,
                            mHasExpirationTime ? &mExpirationTime : nullptr,
                            nullptr);
    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle> mHandle;
  bool                      mHasFrecency;
  bool                      mHasExpirationTime;
  uint32_t                  mFrecency;
  uint32_t                  mExpirationTime;
};

class MetadataWriteScheduleEvent : public nsRunnable
{
public:
  enum EMode {
    SCHEDULE,
    UNSCHEDULE,
    SHUTDOWN
  } mMode;

  nsRefPtr<CacheFile> mFile;
  nsRefPtr<CacheFileIOManager> mIOMan;

  MetadataWriteScheduleEvent(CacheFileIOManager * aManager,
                             CacheFile * aFile,
                             EMode aMode)
    : mMode(aMode)
    , mFile(aFile)
    , mIOMan(aManager)
  { }

  virtual ~MetadataWriteScheduleEvent() { }

  NS_IMETHOD Run()
  {
    nsRefPtr<CacheFileIOManager> ioMan = CacheFileIOManager::gInstance;
    if (!ioMan) {
      NS_WARNING("CacheFileIOManager already gone in MetadataWriteScheduleEvent::Run()");
      return NS_OK;
    }

    switch (mMode)
    {
    case SCHEDULE:
      ioMan->ScheduleMetadataWriteInternal(mFile);
      break;
    case UNSCHEDULE:
      ioMan->UnscheduleMetadataWriteInternal(mFile);
      break;
    case SHUTDOWN:
      ioMan->ShutdownMetadataWriteSchedulingInternal();
      break;
    }
    return NS_OK;
  }
};

CacheFileIOManager * CacheFileIOManager::gInstance = nullptr;

NS_IMPL_ISUPPORTS1(CacheFileIOManager, nsITimerCallback)

CacheFileIOManager::CacheFileIOManager()
  : mShuttingDown(false)
  , mTreeCreated(false)
{
  LOG(("CacheFileIOManager::CacheFileIOManager [this=%p]", this));
  MOZ_COUNT_CTOR(CacheFileIOManager);
  MOZ_ASSERT(!gInstance, "multiple CacheFileIOManager instances!");
}

CacheFileIOManager::~CacheFileIOManager()
{
  LOG(("CacheFileIOManager::~CacheFileIOManager [this=%p]", this));
  MOZ_COUNT_DTOR(CacheFileIOManager);
}

nsresult
CacheFileIOManager::Init()
{
  LOG(("CacheFileIOManager::Init()"));

  MOZ_ASSERT(NS_IsMainThread());

  if (gInstance)
    return NS_ERROR_ALREADY_INITIALIZED;

  nsRefPtr<CacheFileIOManager> ioMan = new CacheFileIOManager();

  nsresult rv = ioMan->InitInternal();
  NS_ENSURE_SUCCESS(rv, rv);

  ioMan.swap(gInstance);
  return NS_OK;
}

nsresult
CacheFileIOManager::InitInternal()
{
  nsresult rv;

  mIOThread = new CacheIOThread();

  rv = mIOThread->Init();
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Can't create background thread");
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::Shutdown()
{
  LOG(("CacheFileIOManager::Shutdown() [gInstance=%p]", gInstance));

  MOZ_ASSERT(NS_IsMainThread());

  if (!gInstance)
    return NS_ERROR_NOT_INITIALIZED;

  Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_SHUTDOWN_V2> shutdownTimer;

  CacheIndex::PreShutdown();

  ShutdownMetadataWriteScheduling();

  {
    mozilla::Mutex lock("CacheFileIOManager::Shutdown() lock");
    mozilla::CondVar condVar(lock, "CacheFileIOManager::Shutdown() condVar");

    MutexAutoLock autoLock(lock);
    nsRefPtr<ShutdownEvent> ev = new ShutdownEvent(&lock, &condVar);
    DebugOnly<nsresult> rv;
    rv = gInstance->mIOThread->Dispatch(ev, CacheIOThread::CLOSE);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    condVar.Wait();
  }

  MOZ_ASSERT(gInstance->mHandles.HandleCount() == 0);
  MOZ_ASSERT(gInstance->mHandlesByLastUsed.Length() == 0);

  if (gInstance->mIOThread)
    gInstance->mIOThread->Shutdown();

  CacheIndex::Shutdown();

  nsRefPtr<CacheFileIOManager> ioMan;
  ioMan.swap(gInstance);

  return NS_OK;
}

nsresult
CacheFileIOManager::ShutdownInternal()
{
  LOG(("CacheFileIOManager::ShutdownInternal() [this=%p]", this));

  MOZ_ASSERT(mIOThread->IsCurrentThread());

  // No new handles can be created after this flag is set
  mShuttingDown = true;

  // close all handles and delete all associated files
  nsTArray<nsRefPtr<CacheFileHandle> > handles;
  mHandles.GetAllHandles(&handles);
  handles.AppendElements(mSpecialHandles);

  for (uint32_t i=0 ; i<handles.Length() ; i++) {
    CacheFileHandle *h = handles[i];
    h->mClosed = true;

    h->Log();

    // Close file handle
    if (h->mFD) {
      ReleaseNSPRHandleInternal(h);
    }

    // Remove file if entry is doomed or invalid
    if (h->mFileExists && (h->mIsDoomed || h->mInvalid)) {
      LOG(("CacheFileIOManager::ShutdownInternal() - Removing file from disk"));
      h->mFile->Remove(false);
    }

    if (!h->IsSpecialFile() && !h->mIsDoomed &&
        (h->mInvalid || !h->mFileExists)) {
      CacheIndex::RemoveEntry(h->Hash());
    }

    // Remove the handle from mHandles/mSpecialHandles
    if (h->IsSpecialFile())
      mSpecialHandles.RemoveElement(h);
    else
      mHandles.RemoveHandle(h);
  }

  // Assert the table is empty. When we are here, no new handles can be added
  // and handles will no longer remove them self from this table and we don't 
  // want to keep invalid handles here. Also, there is no lookup after this 
  // point to happen.
  MOZ_ASSERT(mHandles.HandleCount() == 0);

  return NS_OK;
}

nsresult
CacheFileIOManager::OnProfile()
{
  LOG(("CacheFileIOManager::OnProfile() [gInstance=%p]", gInstance));

  nsRefPtr<CacheFileIOManager> ioMan = gInstance;
  if (!ioMan) {
    // CacheFileIOManager::Init() failed, probably could not create the IO
    // thread, just go with it...
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv;

  nsCOMPtr<nsIFile> directory;

#if defined(MOZ_WIDGET_ANDROID)
  char* cachePath = getenv("CACHE_DIRECTORY");
  if (cachePath && *cachePath) {
    rv = NS_NewNativeLocalFile(nsDependentCString(cachePath),
                               true, getter_AddRefs(directory));
  }
#endif

  if (!directory) {
    rv = NS_GetSpecialDirectory(NS_APP_CACHE_PARENT_DIR,
                                getter_AddRefs(directory));
  }

  if (!directory) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                getter_AddRefs(directory));
  }

  if (directory) {
    rv = directory->Append(NS_LITERAL_STRING("cache2"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = directory->Clone(getter_AddRefs(ioMan->mCacheDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    CacheIndex::Init(directory);
  }

  return NS_OK;
}

// static
already_AddRefed<nsIEventTarget>
CacheFileIOManager::IOTarget()
{
  nsCOMPtr<nsIEventTarget> target;
  if (gInstance && gInstance->mIOThread)
    target = gInstance->mIOThread->Target();

  return target.forget();
}

// static
already_AddRefed<CacheIOThread>
CacheFileIOManager::IOThread()
{
  nsRefPtr<CacheIOThread> thread;
  if (gInstance)
    thread = gInstance->mIOThread;

  return thread.forget();
}

// static
bool
CacheFileIOManager::IsOnIOThread()
{
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;
  if (ioMan && ioMan->mIOThread)
    return ioMan->mIOThread->IsCurrentThread();

  return false;
}

// static
bool
CacheFileIOManager::IsOnIOThreadOrCeased()
{
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;
  if (ioMan && ioMan->mIOThread)
    return ioMan->mIOThread->IsCurrentThread();

  // Ceased...
  return true;
}

// static
bool
CacheFileIOManager::IsShutdown()
{
  if (!gInstance)
    return true;
  return gInstance->mShuttingDown;
}

// static
nsresult
CacheFileIOManager::ScheduleMetadataWrite(CacheFile * aFile)
{
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;
  NS_ENSURE_TRUE(ioMan, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_TRUE(!ioMan->mShuttingDown, NS_ERROR_NOT_INITIALIZED);

  nsRefPtr<MetadataWriteScheduleEvent> event = new MetadataWriteScheduleEvent(
    ioMan, aFile, MetadataWriteScheduleEvent::SCHEDULE);
  nsCOMPtr<nsIEventTarget> target = ioMan->IOTarget();
  NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
  return target->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
}

nsresult
CacheFileIOManager::ScheduleMetadataWriteInternal(CacheFile * aFile)
{
  MOZ_ASSERT(IsOnIOThreadOrCeased());

  nsresult rv;

  if (!mMetadataWritesTimer) {
    mMetadataWritesTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mMetadataWritesTimer->InitWithCallback(
      this, kMetadataWriteDelay, nsITimer::TYPE_ONE_SHOT);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mScheduledMetadataWrites.IndexOf(aFile) !=
      mScheduledMetadataWrites.NoIndex) {
    return NS_OK;
  }

  mScheduledMetadataWrites.AppendElement(aFile);

  return NS_OK;
}

// static
nsresult
CacheFileIOManager::UnscheduleMetadataWrite(CacheFile * aFile)
{
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;
  NS_ENSURE_TRUE(ioMan, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_TRUE(!ioMan->mShuttingDown, NS_ERROR_NOT_INITIALIZED);

  nsRefPtr<MetadataWriteScheduleEvent> event = new MetadataWriteScheduleEvent(
    ioMan, aFile, MetadataWriteScheduleEvent::UNSCHEDULE);
  nsCOMPtr<nsIEventTarget> target = ioMan->IOTarget();
  NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
  return target->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
}

nsresult
CacheFileIOManager::UnscheduleMetadataWriteInternal(CacheFile * aFile)
{
  MOZ_ASSERT(IsOnIOThreadOrCeased());

  mScheduledMetadataWrites.RemoveElement(aFile);

  if (mScheduledMetadataWrites.Length() == 0 &&
      mMetadataWritesTimer) {
    mMetadataWritesTimer->Cancel();
    mMetadataWritesTimer = nullptr;
  }

  return NS_OK;
}

// static
nsresult
CacheFileIOManager::ShutdownMetadataWriteScheduling()
{
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;
  NS_ENSURE_TRUE(ioMan, NS_ERROR_NOT_INITIALIZED);

  nsRefPtr<MetadataWriteScheduleEvent> event = new MetadataWriteScheduleEvent(
    ioMan, nullptr, MetadataWriteScheduleEvent::SHUTDOWN);
  nsCOMPtr<nsIEventTarget> target = ioMan->IOTarget();
  NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
  return target->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
}

nsresult
CacheFileIOManager::ShutdownMetadataWriteSchedulingInternal()
{
  MOZ_ASSERT(IsOnIOThreadOrCeased());

  nsTArray<nsRefPtr<CacheFile> > files;
  files.SwapElements(mScheduledMetadataWrites);
  for (uint32_t i = 0; i < files.Length(); ++i) {
    CacheFile * file = files[i];
    file->WriteMetadataIfNeeded();
  }

  if (mMetadataWritesTimer) {
    mMetadataWritesTimer->Cancel();
    mMetadataWritesTimer = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
CacheFileIOManager::Notify(nsITimer * aTimer)
{
  MOZ_ASSERT(IsOnIOThreadOrCeased());
  MOZ_ASSERT(mMetadataWritesTimer == aTimer);

  mMetadataWritesTimer = nullptr;

  nsTArray<nsRefPtr<CacheFile> > files;
  files.SwapElements(mScheduledMetadataWrites);
  for (uint32_t i = 0; i < files.Length(); ++i) {
    CacheFile * file = files[i];
    file->WriteMetadataIfNeeded();
  }

  return NS_OK;
}

nsresult
CacheFileIOManager::OpenFile(const nsACString &aKey,
                             uint32_t aFlags,
                             CacheFileIOListener *aCallback)
{
  LOG(("CacheFileIOManager::OpenFile() [key=%s, flags=%d, listener=%p]",
       PromiseFlatCString(aKey).get(), aFlags, aCallback));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (!ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  bool priority = aFlags & CacheFileIOManager::PRIORITY;
  nsRefPtr<OpenFileEvent> ev = new OpenFileEvent(aKey, aFlags, aCallback);
  rv = ioMan->mIOThread->Dispatch(ev, priority
    ? CacheIOThread::OPEN_PRIORITY
    : CacheIOThread::OPEN);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::OpenFileInternal(const SHA1Sum::Hash *aHash,
                                     uint32_t aFlags,
                                     CacheFileHandle **_retval)
{
  LOG(("CacheFileIOManager::OpenFileInternal() [hash=%08x%08x%08x%08x%08x, "
       "flags=%d]", LOGSHA1(aHash), aFlags));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsresult rv;

  if (mShuttingDown)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mTreeCreated) {
    rv = CreateCacheTree();
    if (NS_FAILED(rv)) return rv;
  }

  nsCOMPtr<nsIFile> file;
  rv = GetFile(aHash, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<CacheFileHandle> handle;
  mHandles.GetHandle(aHash, false, getter_AddRefs(handle));

  if ((aFlags & (OPEN | CREATE | CREATE_NEW)) == CREATE_NEW) {
    if (handle) {
      rv = DoomFileInternal(handle);
      NS_ENSURE_SUCCESS(rv, rv);
      handle = nullptr;
    }

    rv = mHandles.NewHandle(aHash, aFlags & PRIORITY, getter_AddRefs(handle));
    NS_ENSURE_SUCCESS(rv, rv);

    bool exists;
    rv = file->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      CacheIndex::RemoveEntry(aHash);

      LOG(("CacheFileIOManager::OpenFileInternal() - Removing old file from "
           "disk"));
      rv = file->Remove(false);
      if (NS_FAILED(rv)) {
        NS_WARNING("Cannot remove old entry from the disk");
        LOG(("CacheFileIOManager::OpenFileInternal() - Removing old file failed"
             ". [rv=0x%08x]", rv));
      }
    }

    CacheIndex::AddEntry(aHash);
    handle->mFile.swap(file);
    handle->mFileSize = 0;
  }

  if (handle) {
    handle.swap(*_retval);
    return NS_OK;
  }

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists && (aFlags & (OPEN | CREATE | CREATE_NEW)) == OPEN)
    return NS_ERROR_NOT_AVAILABLE;

  rv = mHandles.NewHandle(aHash, aFlags & PRIORITY, getter_AddRefs(handle));
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    rv = file->GetFileSize(&handle->mFileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    handle->mFileExists = true;

    CacheIndex::EnsureEntryExists(aHash);
  }
  else {
    handle->mFileSize = 0;

    CacheIndex::AddEntry(aHash);
  }

  handle->mFile.swap(file);
  handle.swap(*_retval);
  return NS_OK;
}

nsresult
CacheFileIOManager::OpenSpecialFileInternal(const nsACString &aKey,
                                            uint32_t aFlags,
                                            CacheFileHandle **_retval)
{
  LOG(("CacheFileIOManager::OpenSpecialFileInternal() [key=%s, flags=%d]",
       PromiseFlatCString(aKey).get(), aFlags));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsresult rv;

  if (mShuttingDown)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mTreeCreated) {
    rv = CreateCacheTree();
    if (NS_FAILED(rv)) return rv;
  }

  nsCOMPtr<nsIFile> file;
  rv = GetSpecialFile(aKey, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<CacheFileHandle> handle;
  for (uint32_t i = 0 ; i < mSpecialHandles.Length() ; i++) {
    if (!mSpecialHandles[i]->IsDoomed() && mSpecialHandles[i]->Key() == aKey) {
      handle = mSpecialHandles[i];
      break;
    }
  }

  if ((aFlags & (OPEN | CREATE | CREATE_NEW)) == CREATE_NEW) {
    if (handle) {
      rv = DoomFileInternal(handle);
      NS_ENSURE_SUCCESS(rv, rv);
      handle = nullptr;
    }

    handle = new CacheFileHandle(aKey, aFlags & PRIORITY);
    mSpecialHandles.AppendElement(handle);

    bool exists;
    rv = file->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      LOG(("CacheFileIOManager::OpenSpecialFileInternal() - Removing file from "
           "disk"));
      rv = file->Remove(false);
      if (NS_FAILED(rv)) {
        NS_WARNING("Cannot remove old entry from the disk");
        LOG(("CacheFileIOManager::OpenSpecialFileInternal() - Removing file "
             "failed. [rv=0x%08x]", rv));
      }
    }

    handle->mFile.swap(file);
    handle->mFileSize = 0;
  }

  if (handle) {
    handle.swap(*_retval);
    return NS_OK;
  }

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists && (aFlags & (OPEN | CREATE | CREATE_NEW)) == OPEN)
    return NS_ERROR_NOT_AVAILABLE;

  handle = new CacheFileHandle(aKey, aFlags & PRIORITY);
  mSpecialHandles.AppendElement(handle);

  if (exists) {
    rv = file->GetFileSize(&handle->mFileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    handle->mFileExists = true;
  }
  else {
    handle->mFileSize = 0;
  }

  handle->mFile.swap(file);
  handle.swap(*_retval);
  return NS_OK;
}

nsresult
CacheFileIOManager::CloseHandleInternal(CacheFileHandle *aHandle)
{
  LOG(("CacheFileIOManager::CloseHandleInternal() [handle=%p]", aHandle));
  aHandle->Log();

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());

  // Close file handle
  if (aHandle->mFD) {
    ReleaseNSPRHandleInternal(aHandle);
  }

  // Delete the file if the entry was doomed or invalid
  if (aHandle->mIsDoomed || aHandle->mInvalid) {
    LOG(("CacheFileIOManager::CloseHandleInternal() - Removing file from "
         "disk"));
    aHandle->mFile->Remove(false);
  }

  if (!aHandle->IsSpecialFile() && !aHandle->mIsDoomed &&
      (aHandle->mInvalid || !aHandle->mFileExists)) {
    CacheIndex::RemoveEntry(aHandle->Hash());
  }

  // Remove the handle from hashtable
  if (aHandle->IsSpecialFile())
    mSpecialHandles.RemoveElement(aHandle);
  else if (!mShuttingDown) // Don't touch after shutdown
    mHandles.RemoveHandle(aHandle);

  return NS_OK;
}

nsresult
CacheFileIOManager::Read(CacheFileHandle *aHandle, int64_t aOffset,
                         char *aBuf, int32_t aCount,
                         CacheFileIOListener *aCallback)
{
  LOG(("CacheFileIOManager::Read() [handle=%p, offset=%lld, count=%d, "
       "listener=%p]", aHandle, aOffset, aCount, aCallback));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (aHandle->IsClosed() || !ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<ReadEvent> ev = new ReadEvent(aHandle, aOffset, aBuf, aCount,
                                         aCallback);
  rv = ioMan->mIOThread->Dispatch(ev, aHandle->IsPriority()
    ? CacheIOThread::READ_PRIORITY
    : CacheIOThread::READ);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::ReadInternal(CacheFileHandle *aHandle, int64_t aOffset,
                                 char *aBuf, int32_t aCount)
{
  LOG(("CacheFileIOManager::ReadInternal() [handle=%p, offset=%lld, count=%d]",
       aHandle, aOffset, aCount));

  nsresult rv;

  if (!aHandle->mFileExists) {
    NS_WARNING("Trying to read from non-existent file");
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!aHandle->mFD) {
    rv = OpenNSPRHandle(aHandle);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    NSPRHandleUsed(aHandle);
  }

  // Check again, OpenNSPRHandle could figure out the file was gone.
  if (!aHandle->mFileExists) {
    NS_WARNING("Trying to read from non-existent file");
    return NS_ERROR_NOT_AVAILABLE;
  }

  int64_t offset = PR_Seek64(aHandle->mFD, aOffset, PR_SEEK_SET);
  if (offset == -1)
    return NS_ERROR_FAILURE;

  int32_t bytesRead = PR_Read(aHandle->mFD, aBuf, aCount);
  if (bytesRead != aCount)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
CacheFileIOManager::Write(CacheFileHandle *aHandle, int64_t aOffset,
                          const char *aBuf, int32_t aCount, bool aValidate,
                          CacheFileIOListener *aCallback)
{
  LOG(("CacheFileIOManager::Write() [handle=%p, offset=%lld, count=%d, "
       "validate=%d, listener=%p]", aHandle, aOffset, aCount, aValidate,
       aCallback));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (aHandle->IsClosed() || !ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<WriteEvent> ev = new WriteEvent(aHandle, aOffset, aBuf, aCount,
                                           aValidate, aCallback);
  rv = ioMan->mIOThread->Dispatch(ev, CacheIOThread::WRITE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::WriteInternal(CacheFileHandle *aHandle, int64_t aOffset,
                                  const char *aBuf, int32_t aCount,
                                  bool aValidate)
{
  LOG(("CacheFileIOManager::WriteInternal() [handle=%p, offset=%lld, count=%d, "
       "validate=%d]", aHandle, aOffset, aCount, aValidate));

  nsresult rv;

  if (!aHandle->mFileExists) {
    rv = CreateFile(aHandle);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aHandle->mFD) {
    rv = OpenNSPRHandle(aHandle);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    NSPRHandleUsed(aHandle);
  }

  // Check again, OpenNSPRHandle could figure out the file was gone.
  if (!aHandle->mFileExists) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Write invalidates the entry by default
  aHandle->mInvalid = true;

  int64_t offset = PR_Seek64(aHandle->mFD, aOffset, PR_SEEK_SET);
  if (offset == -1)
    return NS_ERROR_FAILURE;

  int32_t bytesWritten = PR_Write(aHandle->mFD, aBuf, aCount);

  if (bytesWritten != -1 && aHandle->mFileSize < aOffset+bytesWritten) {
    aHandle->mFileSize = aOffset+bytesWritten;

    if (!aHandle->IsDoomed() && !aHandle->IsSpecialFile()) {
      uint32_t size = aHandle->FileSizeInK();
      CacheIndex::UpdateEntry(aHandle->Hash(), nullptr, nullptr, &size);
    }
  }

  if (bytesWritten != aCount)
    return NS_ERROR_FAILURE;

  // Write was successful and this write validates the entry (i.e. metadata)
  if (aValidate)
    aHandle->mInvalid = false;

  return NS_OK;
}

nsresult
CacheFileIOManager::DoomFile(CacheFileHandle *aHandle,
                             CacheFileIOListener *aCallback)
{
  LOG(("CacheFileIOManager::DoomFile() [handle=%p, listener=%p]",
       aHandle, aCallback));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (aHandle->IsClosed() || !ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<DoomFileEvent> ev = new DoomFileEvent(aHandle, aCallback);
  rv = ioMan->mIOThread->Dispatch(ev, aHandle->IsPriority()
    ? CacheIOThread::OPEN_PRIORITY
    : CacheIOThread::OPEN);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::DoomFileInternal(CacheFileHandle *aHandle)
{
  LOG(("CacheFileIOManager::DoomFileInternal() [handle=%p]", aHandle));
  aHandle->Log();

  nsresult rv;

  if (aHandle->IsDoomed())
    return NS_OK;

  if (aHandle->mFileExists) {
    // we need to move the current file to the doomed directory
    if (aHandle->mFD)
      ReleaseNSPRHandleInternal(aHandle);

    // find unused filename
    nsCOMPtr<nsIFile> file;
    rv = GetDoomedFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> parentDir;
    rv = file->GetParent(getter_AddRefs(parentDir));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString leafName;
    rv = file->GetNativeLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aHandle->mFile->MoveToNative(parentDir, leafName);
    if (NS_ERROR_FILE_NOT_FOUND == rv || NS_ERROR_FILE_TARGET_DOES_NOT_EXIST == rv) {
      LOG(("  file already removed under our hands"));
      aHandle->mFileExists = false;
      rv = NS_OK;
    }
    else {
      NS_ENSURE_SUCCESS(rv, rv);
      aHandle->mFile.swap(file);
    }
  }

  if (!aHandle->IsSpecialFile())
    CacheIndex::RemoveEntry(aHandle->Hash());

  aHandle->mIsDoomed = true;
  return NS_OK;
}

nsresult
CacheFileIOManager::DoomFileByKey(const nsACString &aKey,
                                  CacheFileIOListener *aCallback)
{
  LOG(("CacheFileIOManager::DoomFileByKey() [key=%s, listener=%p]",
       PromiseFlatCString(aKey).get(), aCallback));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (!ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<DoomFileByKeyEvent> ev = new DoomFileByKeyEvent(aKey, aCallback);
  rv = ioMan->mIOThread->Dispatch(ev, CacheIOThread::OPEN);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::DoomFileByKeyInternal(const SHA1Sum::Hash *aHash)
{
  LOG(("CacheFileIOManager::DoomFileByKeyInternal() [hash=%08x%08x%08x%08x%08x]"
       , LOGSHA1(aHash)));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());

  nsresult rv;

  if (mShuttingDown)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mCacheDirectory)
    return NS_ERROR_FILE_INVALID_PATH;

  // Find active handle
  nsRefPtr<CacheFileHandle> handle;
  mHandles.GetHandle(aHash, true, getter_AddRefs(handle));

  if (handle) {
    handle->Log();

    if (handle->IsDoomed())
      return NS_OK;

    return DoomFileInternal(handle);
  }

  // There is no handle for this file, delete the file if exists
  nsCOMPtr<nsIFile> file;
  rv = GetFile(aHash, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists)
    return NS_ERROR_NOT_AVAILABLE;

  LOG(("CacheFileIOManager::DoomFileByKeyInternal() - Removing file from "
       "disk"));
  rv = file->Remove(false);
  if (NS_FAILED(rv)) {
    NS_WARNING("Cannot remove old entry from the disk");
    LOG(("CacheFileIOManager::DoomFileByKeyInternal() - Removing file failed. "
         "[rv=0x%08x]", rv));
  }

  CacheIndex::RemoveEntry(aHash);

  return NS_OK;
}

nsresult
CacheFileIOManager::ReleaseNSPRHandle(CacheFileHandle *aHandle)
{
  LOG(("CacheFileIOManager::ReleaseNSPRHandle() [handle=%p]", aHandle));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (aHandle->IsClosed() || !ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<ReleaseNSPRHandleEvent> ev = new ReleaseNSPRHandleEvent(aHandle);
  rv = ioMan->mIOThread->Dispatch(ev, CacheIOThread::CLOSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::ReleaseNSPRHandleInternal(CacheFileHandle *aHandle)
{
  LOG(("CacheFileIOManager::ReleaseNSPRHandleInternal() [handle=%p]", aHandle));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());
  MOZ_ASSERT(aHandle->mFD);

  DebugOnly<bool> found;
  found = mHandlesByLastUsed.RemoveElement(aHandle);
  MOZ_ASSERT(found);

  PR_Close(aHandle->mFD);
  aHandle->mFD = nullptr;

  return NS_OK;
}

nsresult
CacheFileIOManager::TruncateSeekSetEOF(CacheFileHandle *aHandle,
                                       int64_t aTruncatePos, int64_t aEOFPos,
                                       CacheFileIOListener *aCallback)
{
  LOG(("CacheFileIOManager::TruncateSeekSetEOF() [handle=%p, truncatePos=%lld, "
       "EOFPos=%lld, listener=%p]", aHandle, aTruncatePos, aEOFPos, aCallback));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (aHandle->IsClosed() || !ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<TruncateSeekSetEOFEvent> ev = new TruncateSeekSetEOFEvent(
                                           aHandle, aTruncatePos, aEOFPos,
                                           aCallback);
  rv = ioMan->mIOThread->Dispatch(ev, CacheIOThread::WRITE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
void CacheFileIOManager::GetCacheDirectory(nsIFile** result)
{
  *result = nullptr;

  nsRefPtr<CacheFileIOManager> ioMan = gInstance;
  if (!ioMan)
    return;

  nsCOMPtr<nsIFile> file = ioMan->mCacheDirectory;
  file.forget(result);
}

static nsresult
TruncFile(PRFileDesc *aFD, uint32_t aEOF)
{
#if defined(XP_UNIX)
  if (ftruncate(PR_FileDesc2NativeHandle(aFD), aEOF) != 0) {
    NS_ERROR("ftruncate failed");
    return NS_ERROR_FAILURE;
  }
#elif defined(XP_WIN)
  int32_t cnt = PR_Seek(aFD, aEOF, PR_SEEK_SET);
  if (cnt == -1)
    return NS_ERROR_FAILURE;
  if (!SetEndOfFile((HANDLE) PR_FileDesc2NativeHandle(aFD))) {
    NS_ERROR("SetEndOfFile failed");
    return NS_ERROR_FAILURE;
  }
#else
  MOZ_ASSERT(false, "Not implemented!");
#endif

  return NS_OK;
}

nsresult
CacheFileIOManager::TruncateSeekSetEOFInternal(CacheFileHandle *aHandle,
                                               int64_t aTruncatePos,
                                               int64_t aEOFPos)
{
  LOG(("CacheFileIOManager::TruncateSeekSetEOFInternal() [handle=%p, "
       "truncatePos=%lld, EOFPos=%lld]", aHandle, aTruncatePos, aEOFPos));

  nsresult rv;

  if (!aHandle->mFileExists) {
    rv = CreateFile(aHandle);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aHandle->mFD) {
    rv = OpenNSPRHandle(aHandle);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    NSPRHandleUsed(aHandle);
  }

  // Check again, OpenNSPRHandle could figure out the file was gone.
  if (!aHandle->mFileExists) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // This operation always invalidates the entry
  aHandle->mInvalid = true;

  rv = TruncFile(aHandle->mFD, static_cast<uint32_t>(aTruncatePos));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TruncFile(aHandle->mFD, static_cast<uint32_t>(aEOFPos));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::RenameFile(CacheFileHandle *aHandle,
                               const nsACString &aNewName,
                               CacheFileIOListener *aCallback)
{
  LOG(("CacheFileIOManager::RenameFile() [handle=%p, newName=%s, listener=%p]",
       aHandle, PromiseFlatCString(aNewName).get(), aCallback));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (aHandle->IsClosed() || !ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aHandle->IsSpecialFile())
    return NS_ERROR_UNEXPECTED;

  nsRefPtr<RenameFileEvent> ev = new RenameFileEvent(aHandle, aNewName,
                                                     aCallback);
  rv = ioMan->mIOThread->Dispatch(ev, CacheIOThread::WRITE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::RenameFileInternal(CacheFileHandle *aHandle,
                                       const nsACString &aNewName)
{
  LOG(("CacheFileIOManager::RenameFileInternal() [handle=%p, newName=%s]",
       aHandle, PromiseFlatCString(aNewName).get()));

  nsresult rv;

  MOZ_ASSERT(aHandle->IsSpecialFile());

  if (aHandle->IsDoomed())
    return NS_ERROR_NOT_AVAILABLE;

  // Doom old handle if it exists and is not doomed
  for (uint32_t i = 0 ; i < mSpecialHandles.Length() ; i++) {
    if (!mSpecialHandles[i]->IsDoomed() &&
        mSpecialHandles[i]->Key() == aNewName) {
      MOZ_ASSERT(aHandle != mSpecialHandles[i]);
      rv = DoomFileInternal(mSpecialHandles[i]);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
  }

  nsCOMPtr<nsIFile> file;
  rv = GetSpecialFile(aNewName, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    LOG(("CacheFileIOManager::RenameFileInternal() - Removing old file from "
         "disk"));
    rv = file->Remove(false);
    if (NS_FAILED(rv)) {
      NS_WARNING("Cannot remove file from the disk");
      LOG(("CacheFileIOManager::RenameFileInternal() - Removing old file failed"
           ". [rv=0x%08x]", rv));
    }
  }

  if (!aHandle->FileExists()) {
    aHandle->mKey = aNewName;
    return NS_OK;
  }

  if (aHandle->mFD)
    ReleaseNSPRHandleInternal(aHandle);

  rv = aHandle->mFile->MoveToNative(nullptr, aNewName);
  NS_ENSURE_SUCCESS(rv, rv);

  aHandle->mKey = aNewName;
  return NS_OK;
}

nsresult
CacheFileIOManager::InitIndexEntry(CacheFileHandle *aHandle,
                                   uint32_t         aAppId,
                                   bool             aAnonymous,
                                   bool             aInBrowser)
{
  LOG(("CacheFileIOManager::InitIndexEntry() [handle=%p, appId=%u, anonymous=%d"
       ", inBrowser=%d]", aHandle, aAppId, aAnonymous, aInBrowser));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (aHandle->IsClosed() || !ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  if (aHandle->IsSpecialFile())
    return NS_ERROR_UNEXPECTED;

  nsRefPtr<InitIndexEntryEvent> ev =
    new InitIndexEntryEvent(aHandle, aAppId, aAnonymous, aInBrowser);
  rv = ioMan->mIOThread->Dispatch(ev, CacheIOThread::WRITE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::UpdateIndexEntry(CacheFileHandle *aHandle,
                                     const uint32_t  *aFrecency,
                                     const uint32_t  *aExpirationTime)
{
  LOG(("CacheFileIOManager::UpdateIndexEntry() [handle=%p, frecency=%s, "
       "expirationTime=%s]", aHandle,
       aFrecency ? nsPrintfCString("%u", *aFrecency).get() : "",
       aExpirationTime ? nsPrintfCString("%u", *aExpirationTime).get() : ""));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (aHandle->IsClosed() || !ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  if (aHandle->IsSpecialFile())
    return NS_ERROR_UNEXPECTED;

  nsRefPtr<UpdateIndexEntryEvent> ev =
    new UpdateIndexEntryEvent(aHandle, aFrecency, aExpirationTime);
  rv = ioMan->mIOThread->Dispatch(ev, CacheIOThread::WRITE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::EnumerateEntryFiles(EEnumerateMode aMode,
                                        CacheEntriesEnumerator** aEnumerator)
{
  LOG(("CacheFileIOManager::EnumerateEntryFiles(%d)", aMode));

  nsresult rv;
  nsRefPtr<CacheFileIOManager> ioMan = gInstance;

  if (!ioMan)
    return NS_ERROR_NOT_INITIALIZED;

  if (!ioMan->mCacheDirectory)
    return NS_ERROR_FILE_NOT_FOUND;

  nsCOMPtr<nsIFile> file;
  rv = ioMan->mCacheDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  switch (aMode) {
  case ENTRIES:
    rv = file->AppendNative(NS_LITERAL_CSTRING(kEntriesDir));
    NS_ENSURE_SUCCESS(rv, rv);

    break;

  case DOOMED:
    rv = file->AppendNative(NS_LITERAL_CSTRING(kDoomedDir));
    NS_ENSURE_SUCCESS(rv, rv);

    break;

  default:
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoPtr<CacheEntriesEnumerator> enumerator(
    new CacheEntriesEnumerator(file));

  rv = enumerator->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  *aEnumerator = enumerator.forget();
  return NS_OK;
}

nsresult
CacheFileIOManager::CreateFile(CacheFileHandle *aHandle)
{
  MOZ_ASSERT(!aHandle->mFD);
  MOZ_ASSERT(aHandle->mFile);

  nsresult rv;

  if (aHandle->IsDoomed()) {
    nsCOMPtr<nsIFile> file;

    rv = GetDoomedFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    aHandle->mFile.swap(file);
  } else {
    bool exists;
    if (NS_SUCCEEDED(aHandle->mFile->Exists(&exists)) && exists) {
      NS_WARNING("Found a file that should not exist!");
    }
  }

  rv = OpenNSPRHandle(aHandle, true);
  NS_ENSURE_SUCCESS(rv, rv);

  aHandle->mFileSize = 0;
  return NS_OK;
}

void
CacheFileIOManager::HashToStr(const SHA1Sum::Hash *aHash, nsACString &_retval)
{
  _retval.Assign("");
  const char hexChars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  for (uint32_t i=0 ; i<sizeof(SHA1Sum::Hash) ; i++) {
    _retval.Append(hexChars[(*aHash)[i] >> 4]);
    _retval.Append(hexChars[(*aHash)[i] & 0xF]);
  }
}

nsresult
CacheFileIOManager::StrToHash(const nsACString &aHash, SHA1Sum::Hash *_retval)
{
  if (aHash.Length() != 2*sizeof(SHA1Sum::Hash))
    return NS_ERROR_INVALID_ARG;

  for (uint32_t i=0 ; i<aHash.Length() ; i++) {
    uint8_t value;

    if (aHash[i] >= '0' && aHash[i] <= '9')
      value = aHash[i] - '0';
    else if (aHash[i] >= 'A' && aHash[i] <= 'F')
      value = aHash[i] - 'A' + 10;
    else if (aHash[i] >= 'a' && aHash[i] <= 'f')
      value = aHash[i] - 'a' + 10;
    else
      return NS_ERROR_INVALID_ARG;

    if (i%2 == 0)
      (reinterpret_cast<uint8_t *>(_retval))[i/2] = value << 4;
    else
      (reinterpret_cast<uint8_t *>(_retval))[i/2] += value;
  }

  return NS_OK;
}

nsresult
CacheFileIOManager::GetFile(const SHA1Sum::Hash *aHash, nsIFile **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file;
  rv = mCacheDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING(kEntriesDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString leafName;
  HashToStr(aHash, leafName);

  rv = file->AppendNative(leafName);
  NS_ENSURE_SUCCESS(rv, rv);

  file.swap(*_retval);
  return NS_OK;
}

nsresult
CacheFileIOManager::GetSpecialFile(const nsACString &aKey, nsIFile **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file;
  rv = mCacheDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  file.swap(*_retval);
  return NS_OK;
}

nsresult
CacheFileIOManager::GetDoomedFile(nsIFile **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file;
  rv = mCacheDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING(kDoomedDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING("dummyleaf"));
  NS_ENSURE_SUCCESS(rv, rv);

  srand(static_cast<unsigned>(PR_Now()));
  nsAutoCString leafName;
  uint32_t iter=0;
  while (true) {
    iter++;
    leafName.AppendInt(rand());
    rv = file->SetNativeLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    bool exists;
    if (NS_SUCCEEDED(file->Exists(&exists)) && !exists)
      break;

    leafName.Truncate();
  }

//  Telemetry::Accumulate(Telemetry::DISK_CACHE_GETDOOMEDFILE_ITERATIONS, iter);

  file.swap(*_retval);
  return NS_OK;
}

nsresult
CacheFileIOManager::CheckAndCreateDir(nsIFile *aFile, const char *aDir)
{
  nsresult rv;
  bool exists;

  nsCOMPtr<nsIFile> file;
  if (!aDir) {
    file = aFile;
  } else {
    nsAutoCString dir(aDir);
    rv = aFile->Clone(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = file->AppendNative(dir);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = file->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists)
    rv = file->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_FAILED(rv)) {
    NS_WARNING("Cannot create directory");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
CacheFileIOManager::CreateCacheTree()
{
  MOZ_ASSERT(!mTreeCreated);

  if (!mCacheDirectory)
    return NS_ERROR_FILE_INVALID_PATH;

  nsresult rv;

  // ensure parent directory exists
  nsCOMPtr<nsIFile> parentDir;
  rv = mCacheDirectory->GetParent(getter_AddRefs(parentDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = CheckAndCreateDir(parentDir, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure cache directory exists
  rv = CheckAndCreateDir(mCacheDirectory, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure entries directory exists
  rv = CheckAndCreateDir(mCacheDirectory, kEntriesDir);
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure doomed directory exists
  rv = CheckAndCreateDir(mCacheDirectory, kDoomedDir);
  NS_ENSURE_SUCCESS(rv, rv);

  mTreeCreated = true;
  return NS_OK;
}

nsresult
CacheFileIOManager::OpenNSPRHandle(CacheFileHandle *aHandle, bool aCreate)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());
  MOZ_ASSERT(!aHandle->mFD);
  MOZ_ASSERT(mHandlesByLastUsed.IndexOf(aHandle) == mHandlesByLastUsed.NoIndex);
  MOZ_ASSERT(mHandlesByLastUsed.Length() <= kOpenHandlesLimit);
  MOZ_ASSERT((aCreate && !aHandle->mFileExists) ||
             (!aCreate && aHandle->mFileExists));

  nsresult rv;

  if (mHandlesByLastUsed.Length() == kOpenHandlesLimit) {
    // close handle that hasn't been used for the longest time
    rv = ReleaseNSPRHandleInternal(mHandlesByLastUsed[0]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCreate) {
    rv = aHandle->mFile->OpenNSPRFileDesc(
           PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0600, &aHandle->mFD);
    NS_ENSURE_SUCCESS(rv, rv);

    aHandle->mFileExists = true;
  }
  else {
    rv = aHandle->mFile->OpenNSPRFileDesc(PR_RDWR, 0600, &aHandle->mFD);
    if (NS_ERROR_FILE_NOT_FOUND == rv) {
      LOG(("  file doesn't exists"));
      aHandle->mFileExists = false;
      return DoomFileInternal(aHandle);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mHandlesByLastUsed.AppendElement(aHandle);
  return NS_OK;
}

void
CacheFileIOManager::NSPRHandleUsed(CacheFileHandle *aHandle)
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThreadOrCeased());
  MOZ_ASSERT(aHandle->mFD);

  DebugOnly<bool> found;
  found = mHandlesByLastUsed.RemoveElement(aHandle);
  MOZ_ASSERT(found);

  mHandlesByLastUsed.AppendElement(aHandle);
}

} // net
} // mozilla
