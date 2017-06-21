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
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"

static inline uint32_t
PRTimeToSeconds(PRTime t_usec)
{
  PRTime usec_per_sec = PR_USEC_PER_SEC;
  return uint32_t(t_usec /= usec_per_sec);
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

class nsIOutputStream;
class nsIURI;
class nsIThread;

namespace mozilla {
namespace net {

class CacheStorageService;
class CacheStorage;
class CacheOutputCloseListener;
class CacheEntryHandle;

class CacheEntry final : public nsICacheEntry
                       , public nsIRunnable
                       , public CacheFileListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHEENTRY
  NS_DECL_NSIRUNNABLE

  CacheEntry(const nsACString& aStorageID, const nsACString& aURI, const nsACString& aEnhanceID,
             bool aUseDisk, bool aSkipSizeCheck, bool aPin);

  void AsyncOpen(nsICacheEntryOpenCallback* aCallback, uint32_t aFlags);

  CacheEntryHandle* NewHandle();
  // For a new and recreated entry w/o a callback, we need to wrap it
  // with a handle to detect writing consumer is gone.
  CacheEntryHandle* NewWriteHandle();

public:
  uint32_t GetMetadataMemoryConsumption();
  nsCString const &GetStorageID() const { return mStorageID; }
  nsCString const &GetEnhanceID() const { return mEnhanceID; }
  nsCString const &GetURI() const { return mURI; }
  // Accessible at any time
  bool IsUsingDisk() const { return mUseDisk; }
  bool IsReferenced() const;
  bool IsFileDoomed();
  bool IsDoomed() const { return mIsDoomed; }
  bool IsPinned() const { return mPinned; }

  // Methods for entry management (eviction from memory),
  // called only on the management thread.

  // TODO make these inline
  double GetFrecency() const;
  uint32_t GetExpirationTime() const;
  uint32_t UseCount() const { return mUseCount; }

  bool IsRegistered() const;
  bool CanRegister() const;
  void SetRegistered(bool aRegistered);

  TimeStamp const& LoadStart() const { return mLoadStart; }

  enum EPurge {
    PURGE_DATA_ONLY_DISK_BACKED,
    PURGE_WHOLE_ONLY_DISK_BACKED,
    PURGE_WHOLE,
  };

  bool DeferOrBypassRemovalOnPinStatus(bool aPinned);
  bool Purge(uint32_t aWhat);
  void PurgeAndDoom();
  void DoomAlreadyRemoved();

  nsresult HashingKeyWithStorage(nsACString &aResult) const;
  nsresult HashingKey(nsACString &aResult) const;

  static nsresult HashingKey(nsCSubstring const& aStorageID,
                             nsCSubstring const& aEnhanceID,
                             nsIURI* aURI,
                             nsACString &aResult);

  static nsresult HashingKey(nsCSubstring const& aStorageID,
                             nsCSubstring const& aEnhanceID,
                             nsCSubstring const& aURISpec,
                             nsACString &aResult);

  // Accessed only on the service management thread
  double mFrecency;
  ::mozilla::Atomic<uint32_t, ::mozilla::Relaxed> mSortingExpirationTime;

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
  virtual ~CacheEntry();

  // CacheFileListener
  NS_IMETHOD OnFileReady(nsresult aResult, bool aIsNew) override;
  NS_IMETHOD OnFileDoomed(nsresult aResult) override;

  // Keep the service alive during life-time of an entry
  RefPtr<CacheStorageService> mService;

  // We must monitor when a cache entry whose consumer is responsible
  // for writing it the first time gets released.  We must then invoke
  // waiting callbacks to not break the chain.
  class Callback
  {
  public:
    Callback(CacheEntry* aEntry,
             nsICacheEntryOpenCallback *aCallback,
             bool aReadOnly, bool aCheckOnAnyThread, bool aSecret);
    // Special constructor for Callback objects added to the chain
    // just to ensure proper defer dooming (recreation) of this entry.
    Callback(CacheEntry* aEntry, bool aDoomWhenFoundInPinStatus);
    Callback(Callback const &aThat);
    ~Callback();

    // Called when this callback record changes it's owning entry,
    // mainly during recreation.
    void ExchangeEntry(CacheEntry* aEntry);

    // Returns true when an entry is about to be "defer" doomed and this is
    // a "defer" callback.
    bool DeferDoom(bool *aDoom) const;

    // We are raising reference count here to take into account the pending
    // callback (that virtually holds a ref to this entry before it gets
    // it's pointer).
    RefPtr<CacheEntry> mEntry;
    nsCOMPtr<nsICacheEntryOpenCallback> mCallback;
    nsCOMPtr<nsIEventTarget> mTarget;
    bool mReadOnly : 1;
    bool mRevalidating : 1;
    bool mCheckOnAnyThread : 1;
    bool mRecheckAfterWrite : 1;
    bool mNotWanted : 1;
    bool mSecret : 1;

    // These are set only for the defer-doomer Callback instance inserted
    // to the callback chain.  When any of these is set and also any of
    // the corressponding flags on the entry is set, this callback will
    // indicate (via DeferDoom()) the entry have to be recreated/doomed.
    bool mDoomWhenFoundPinned : 1;
    bool mDoomWhenFoundNonPinned : 1;

    nsresult OnCheckThread(bool *aOnCheckThread) const;
    nsresult OnAvailThread(bool *aOnAvailThread) const;
  };

  // Since OnCacheEntryAvailable must be invoked on the main thread
  // we need a runnable for it...
  class AvailableCallbackRunnable : public Runnable
  {
  public:
    AvailableCallbackRunnable(CacheEntry* aEntry,
                              Callback const &aCallback)
      : Runnable("CacheEntry::AvailableCallbackRunnable")
      , mEntry(aEntry)
      , mCallback(aCallback)
    {}

  private:
    NS_IMETHOD Run() override
    {
      mEntry->InvokeAvailableCallback(mCallback);
      return NS_OK;
    }

    RefPtr<CacheEntry> mEntry;
    Callback mCallback;
  };

  // Since OnCacheEntryDoomed must be invoked on the main thread
  // we need a runnable for it...
  class DoomCallbackRunnable : public Runnable
  {
  public:
    DoomCallbackRunnable(CacheEntry* aEntry, nsresult aRv)
      : mEntry(aEntry), mRv(aRv) {}

  private:
    NS_IMETHOD Run() override
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

    RefPtr<CacheEntry> mEntry;
    nsresult mRv;
  };

  // Starts the load or just invokes the callback, bypasses (when required)
  // if busy.  Returns true on job done, false on bypass.
  bool Open(Callback & aCallback, bool aTruncate, bool aPriority, bool aBypassIfBusy);
  // Loads from disk asynchronously
  bool Load(bool aTruncate, bool aPriority);

  void RememberCallback(Callback & aCallback);
  void InvokeCallbacksLock();
  void InvokeCallbacks();
  bool InvokeCallbacks(bool aReadOnly);
  bool InvokeCallback(Callback & aCallback);
  void InvokeAvailableCallback(Callback const & aCallback);
  void OnFetched(Callback const & aCallback);

  nsresult OpenOutputStreamInternal(int64_t offset, nsIOutputStream * *_retval);
  nsresult OpenInputStreamInternal(int64_t offset, const char *aAltDataType, nsIInputStream * *_retval);

  void OnHandleClosed(CacheEntryHandle const* aHandle);

private:
  friend class CacheEntryHandle;
  // Increment/decrements the number of handles keeping this entry.
  void AddHandleRef() { ++mHandlesCount; }
  void ReleaseHandleRef() { --mHandlesCount; }
  // Current number of handles keeping this entry.
  uint32_t HandlesCount() const { return mHandlesCount; }

private:
  friend class CacheOutputCloseListener;
  void OnOutputClosed();

private:
  // Schedules a background operation on the management thread.
  // When executed on the management thread directly, the operation(s)
  // is (are) executed immediately.
  void BackgroundOp(uint32_t aOperation, bool aForceAsync = false);
  void StoreFrecency(double aFrecency);

  // Called only from DoomAlreadyRemoved()
  void DoomFile();
  // When this entry is doomed the first time, this method removes
  // any force-valid timing info for this entry.
  void RemoveForcedValidity();

  already_AddRefed<CacheEntryHandle> ReopenTruncated(bool aMemoryOnly,
                                                     nsICacheEntryOpenCallback* aCallback);
  void TransferCallbacks(CacheEntry & aFromEntry);

  mozilla::Mutex mLock;

  // Reflects the number of existing handles for this entry
  ::mozilla::ThreadSafeAutoRefCnt mHandlesCount;

  nsTArray<Callback> mCallbacks;
  nsCOMPtr<nsICacheEntryDoomCallback> mDoomCallback;

  RefPtr<CacheFile> mFile;

  // Using ReleaseAcquire since we only control access to mFile with this.
  // When mFileStatus is read and found success it is ensured there is mFile and
  // that it is after a successful call to Init().
  ::mozilla::Atomic<nsresult, ::mozilla::ReleaseAcquire> mFileStatus;
  nsCString mURI;
  nsCString mEnhanceID;
  nsCString mStorageID;

  // mUseDisk, mSkipSizeCheck, mIsDoomed are plain "bool", not "bool:1",
  // so as to avoid bitfield races with the byte containing
  // mSecurityInfoLoaded et al.  See bug 1278524.
  //
  // Whether it's allowed to persist the data to disk
  bool const mUseDisk;
  // Whether it should skip max size check.
  bool const mSkipSizeCheck;
  // Set when entry is doomed with AsyncDoom() or DoomAlreadyRemoved().
  bool mIsDoomed;

  // Following flags are all synchronized with the cache entry lock.

  // Whether security info has already been looked up in metadata.
  bool mSecurityInfoLoaded : 1;
  // Prevents any callback invocation
  bool mPreventCallbacks : 1;
  // true: after load and an existing file, or after output stream has been opened.
  //       note - when opening an input stream, and this flag is false, output stream
  //       is open along ; this makes input streams on new entries behave correctly
  //       when EOF is reached (WOULD_BLOCK is returned).
  // false: after load and a new file, or dropped to back to false when a writer
  //        fails to open an output stream.
  bool mHasData : 1;
  // The indication of pinning this entry was open with
  bool mPinned : 1;
  // Whether the pinning state of the entry is known (equals to the actual state
  // of the cache file)
  bool mPinningKnown : 1;

  static char const * StateString(uint32_t aState);

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

  enum ERegistration {
    NEVERREGISTERED = 0, // The entry has never been registered
    REGISTERED = 1,      // The entry is stored in the memory pool index
    DEREGISTERED = 2     // The entry has been removed from the pool
  };

  // Accessed only on the management thread.  Records the state of registration
  // this entry in the memory pool intermediate cache.
  ERegistration mRegistration;

  // If a new (empty) entry is requested to open an input stream before
  // output stream has been opened, we must open output stream internally
  // on CacheFile and hold until writer releases the entry or opens the output
  // stream for read (then we trade him mOutputStream).
  nsCOMPtr<nsIOutputStream> mOutputStream;

  // Weak reference to the current writter.  There can be more then one
  // writer at a time and OnHandleClosed() must be processed only for the
  // current one.
  CacheEntryHandle* mWriter;

  // Background thread scheduled operation.  Set (under the lock) one
  // of this flags to tell the background thread what to do.
  class Ops {
  public:
    static uint32_t const REGISTER =          1 << 0;
    static uint32_t const FRECENCYUPDATE =    1 << 1;
    static uint32_t const CALLBACKS =         1 << 2;
    static uint32_t const UNREGISTER =        1 << 3;

    Ops() : mFlags(0) { }
    uint32_t Grab() { uint32_t flags = mFlags; mFlags = 0; return flags; }
    bool Set(uint32_t aFlags) { if (mFlags & aFlags) return false; mFlags |= aFlags; return true; }
  private:
    uint32_t mFlags;
  } mBackgroundOperations;

  nsCOMPtr<nsISupports> mSecurityInfo;
  int64_t mPredictedDataSize;
  mozilla::TimeStamp mLoadStart;
  uint32_t mUseCount;
};


class CacheEntryHandle : public nsICacheEntry
{
public:
  explicit CacheEntryHandle(CacheEntry* aEntry);
  CacheEntry* Entry() const { return mEntry; }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSICACHEENTRY(mEntry->)
private:
  virtual ~CacheEntryHandle();
  RefPtr<CacheEntry> mEntry;
};


class CacheOutputCloseListener final : public Runnable
{
public:
  void OnOutputClosed();

private:
  friend class CacheEntry;

  virtual ~CacheOutputCloseListener();

  NS_DECL_NSIRUNNABLE
  explicit CacheOutputCloseListener(CacheEntry* aEntry);

private:
  RefPtr<CacheEntry> mEntry;
};

} // namespace net
} // namespace mozilla

#endif
