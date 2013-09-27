/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheEntry__h__
#define CacheEntry__h__

#include "nsICacheEntry.h"
#include "CacheFile.h"

#include "nsIRunnable.h"
#include "nsIOutputStream.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsICacheEntryDoomCallback.h"

#include "nsCOMPtr.h"
#include "nsRefPtrHashtable.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"

static inline uint32_t
PRTimeToSeconds(PRTime t_usec)
{
  PRTime usec_per_sec = PR_USEC_PER_SEC;
  return uint32_t(t_usec /= usec_per_sec);
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

class nsIStorageStream;
class nsIOutputStream;
class nsIURI;

namespace mozilla {
namespace net {

class CacheStorageService;
class CacheStorage;

namespace {
class FrecencyComparator;
class ExpirationComparator;
class EvictionRunnable;
class WalkRunnable;
}

class CacheEntry : public nsICacheEntry
                 , public nsIRunnable
                 , public CacheFileListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHEENTRY
  NS_DECL_NSIRUNNABLE

  CacheEntry(const nsACString& aStorageID, nsIURI* aURI, const nsACString& aEnhanceID,
             bool aUseDisk);

  void AsyncOpen(nsICacheEntryOpenCallback* aCallback, uint32_t aFlags);

public:
  uint32_t GetMetadataMemoryConsumption();
  nsCString const &GetStorageID() const { return mStorageID; }
  nsCString const &GetEnhanceID() const { return mEnhanceID; }
  nsIURI* GetURI() const { return mURI; }
  bool UsingDisk() const;
  bool SetUsingDisk(bool aUsingDisk);

  // Methods for entry management (eviction from memory),
  // called only on the management thread.

  // TODO make these inline
  double GetFrecency() const;
  uint32_t GetExpirationTime() const;

  bool IsRegistered() const;
  bool CanRegister() const;
  void SetRegistered(bool aRegistered);

  enum EPurge {
    PURGE_DATA_ONLY_DISK_BACKED,
    PURGE_WHOLE_ONLY_DISK_BACKED,
    PURGE_WHOLE,
  };

  bool Purge(uint32_t aWhat);
  void PurgeAndDoom();
  void DoomAlreadyRemoved();

  nsresult HashingKeyWithStorage(nsACString &aResult);
  nsresult HashingKey(nsACString &aResult);

  static nsresult HashingKey(nsCSubstring const& aStorageID,
                             nsCSubstring const& aEnhanceID,
                             nsIURI* aURI,
                             nsACString &aResult);

  // Accessed only on the service management thread
  double mFrecency;
  uint32_t mSortingExpirationTime;

private:
  virtual ~CacheEntry();

  // CacheFileListener
  NS_IMETHOD OnFileReady(nsresult aResult, bool aIsNew);
  NS_IMETHOD OnFileDoomed(nsresult aResult);

  // Keep the service alive during life-time of an entry
  nsRefPtr<CacheStorageService> mService;

  // We must monitor when a cache entry whose consumer is responsible
  // for writing it the first time gets released.  We must then invoke
  // waiting callbacks to not break the chain.
  class Handle : public nsICacheEntry
  {
  public:
    Handle(CacheEntry* aEntry);
    virtual ~Handle();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_FORWARD_NSICACHEENTRY(mEntry->)
  private:
    nsRefPtr<CacheEntry> mEntry;
  };

  // Since OnCacheEntryAvailable must be invoked on the main thread
  // we need a runnable for it...
  class AvailableCallbackRunnable : public nsRunnable
  {
  public:
    AvailableCallbackRunnable(CacheEntry* aEntry,
                              nsICacheEntryOpenCallback* aCallback,
                              bool aReadOnly,
                              bool aNotWanted)
      : mEntry(aEntry), mCallback(aCallback)
      , mReadOnly(aReadOnly), mNotWanted(aNotWanted) {}

  private:
    NS_IMETHOD Run()
    {
      mEntry->InvokeAvailableCallback(mCallback, mReadOnly, mNotWanted);
      return NS_OK;
    }

    nsRefPtr<CacheEntry> mEntry;
    nsCOMPtr<nsICacheEntryOpenCallback> mCallback;
    bool mReadOnly : 1;
    bool mNotWanted : 1;
  };

  // Since OnCacheEntryDoomed must be invoked on the main thread
  // we need a runnable for it...
  class DoomCallbackRunnable : public nsRunnable
  {
  public:
    DoomCallbackRunnable(CacheEntry* aEntry, nsresult aRv)
      : mEntry(aEntry), mRv(aRv) {}

  private:
    NS_IMETHOD Run()
    {
      nsCOMPtr<nsICacheEntryDoomCallback> callback;
      {
        mozilla::MutexAutoLock lock(mEntry->mLock);
        mEntry->mDoomCallback.swap(callback);
      }

      if (callback)
        callback->OnCacheEntryDoomed(mRv);
      return NS_OK;
    }

    nsRefPtr<CacheEntry> mEntry;
    nsresult mRv;
  };

  // Loads from disk asynchronously
  bool Load(bool aTruncate, bool aPriority);
  void OnLoaded();

  void RememberCallback(nsICacheEntryOpenCallback* aCallback, bool aReadOnly);
  bool PendingCallbacks();
  void InvokeCallbacks();
  bool InvokeCallback(nsICacheEntryOpenCallback* aCallback, bool aReadOnly);
  void InvokeAvailableCallback(nsICacheEntryOpenCallback* aCallback, bool aReadOnly, bool aNotWanted);
  void InvokeCallbacksMainThread();

  nsresult OpenOutputStreamInternal(int64_t offset, nsIOutputStream * *_retval);

  // When this entry is new and recreated w/o a callback, we need to wrap it
  // with a handle to detect writing consumer is gone.
  Handle* NewWriteHandle();
  void OnWriterClosed(Handle const* aHandle);

  // Schedules a background operation on the management thread.
  // When executed on the management thread directly, the operation(s)
  // is (are) executed immediately.
  void BackgroundOp(uint32_t aOperation, bool aForceAsync = false);

  already_AddRefed<CacheEntry> ReopenTruncated(nsICacheEntryOpenCallback* aCallback);
  void TransferCallbacks(CacheEntry const& aFromEntry);

  mozilla::Mutex mLock;

  nsCOMArray<nsICacheEntryOpenCallback> mCallbacks, mReadOnlyCallbacks;
  nsCOMPtr<nsICacheEntryDoomCallback> mDoomCallback;

  nsRefPtr<CacheFile> mFile;
  nsresult mFileStatus;
  nsCOMPtr<nsIURI> mURI;
  nsCString mEnhanceID;
  nsCString mStorageID;

  // Whether it's allowed to persist the data to disk
  // Synchronized by the service management lock.
  // Hence, leave it as a standalone boolean.
  bool mUseDisk;

  // Set when entry is doomed with AsyncDoom() or DoomAlreadyRemoved().
  // Left as a standalone flag to not bother with locking (there is no need).
  bool mIsDoomed;

  // Following flags are all synchronized with the cache entry lock.

  // Whether security info has already been looked up in metadata.
  bool mSecurityInfoLoaded : 1;
  // Prevents any callback invocation
  bool mPreventCallbacks : 1;
  // Accessed only on the management thread.
  // Whether this entry is registered in the storage service helper arrays
  bool mIsRegistered : 1;
  // After deregistration entry is no allowed to register again
  bool mIsRegistrationAllowed : 1;
  // Way around when having a callback that cannot be invoked on non-main thread
  bool mHasMainThreadOnlyCallback : 1;
  // true: after load and an existing file, or after output stream has been opened.
  //       note - when opening an input stream, and this flag is false, output stream
  //       is open along ; this makes input streams on new entries behave correctly
  //       when EOF is reached (WOULD_BLOCK is returned).
  // false: after load and a new file, or dropped to back to false when a writer
  //        fails to open an output stream.
  bool mHasData : 1;

#ifdef MOZ_LOGGING
  static char const * StateString(uint32_t aState);
#endif

  enum EState {      // transiting to:
    NOTLOADED = 0,   // -> LOADING | EMPTY
    LOADING = 1,     // -> EMPTY | READY
    EMPTY = 2,       // -> WRITING
    WRITING = 3,     // -> EMPTY | READY
    READY = 4,       // -> REVALIDATING
    REVALIDATING = 5 // -> READY
  };

  // State of this entry.
  EState mState;

  // If a new (empty) entry is requested to open an input stream before
  // output stream has been opened, we must open output stream internally
  // on CacheFile and hold until writer releases the entry or opens the output
  // stream for read (then we trade him mOutputStream).
  nsCOMPtr<nsIOutputStream> mOutputStream;

  // Weak reference to the current writter.  There can be more then one
  // writer at a time and OnWriterClosed() must be processed only for the
  // current one.
  Handle* mWriter;

  // Background thread scheduled operation.  Set (under the lock) one
  // of this flags to tell the background thread what to do.
  class Ops {
  public:
    static uint32_t const REGISTER =          1 << 0;
    static uint32_t const FRECENCYUPDATE =    1 << 1;
    static uint32_t const DOOM =              1 << 2;
    static uint32_t const CALLBACKS =         1 << 3;

    Ops() : mFlags(0) { }
    uint32_t Grab() { uint32_t flags = mFlags; mFlags = 0; return flags; }
    bool Set(uint32_t aFlags) { if (mFlags & aFlags) return false; mFlags |= aFlags; return true; }
  private:
    uint32_t mFlags;
  } mBackgroundOperations;

  nsCOMPtr<nsISupports> mSecurityInfo;

  int64_t mPredictedDataSize;
  uint32_t mDataSize; // ???

  mozilla::TimeStamp mLoadStart;
};

} // net
} // mozilla

#endif
