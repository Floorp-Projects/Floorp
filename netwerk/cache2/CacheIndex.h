/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheIndex__h__
#define CacheIndex__h__

#include "CacheLog.h"
#include "CacheFileIOManager.h"
#include "nsIRunnable.h"
#include "CacheHashUtils.h"
#include "nsICacheStorageService.h"
#include "nsICacheEntry.h"
#include "nsILoadContextInfo.h"
#include "nsTHashtable.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "mozilla/SHA1.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Endian.h"
#include "mozilla/TimeStamp.h"

class nsIFile;
class nsIDirectoryEnumerator;
class nsITimer;


#ifdef DEBUG
#define DEBUG_STATS 1
#endif

namespace mozilla {
namespace net {

class CacheFileMetadata;
class FileOpenHelper;
class CacheIndexIterator;

typedef struct {
  // Version of the index. The index must be ignored and deleted when the file
  // on disk was written with a newer version.
  uint32_t mVersion;

  // Timestamp of time when the last successful write of the index started.
  // During update process we use this timestamp for a quick validation of entry
  // files. If last modified time of the file is lower than this timestamp, we
  // skip parsing of such file since the information in index should be up to
  // date.
  uint32_t mTimeStamp;

  // We set this flag as soon as possible after parsing index during startup
  // and clean it after we write journal to disk during shutdown. We ignore the
  // journal and start update process whenever this flag is set during index
  // parsing.
  uint32_t mIsDirty;
} CacheIndexHeader;

struct CacheIndexRecord {
  SHA1Sum::Hash mHash;
  uint32_t      mFrecency;
  uint32_t      mExpirationTime;
  uint32_t      mAppId;

  /*
   *    1000 0000 0000 0000 0000 0000 0000 0000 : initialized
   *    0100 0000 0000 0000 0000 0000 0000 0000 : anonymous
   *    0010 0000 0000 0000 0000 0000 0000 0000 : inIsolatedMozBrowser
   *    0001 0000 0000 0000 0000 0000 0000 0000 : removed
   *    0000 1000 0000 0000 0000 0000 0000 0000 : dirty
   *    0000 0100 0000 0000 0000 0000 0000 0000 : fresh
   *    0000 0011 0000 0000 0000 0000 0000 0000 : reserved
   *    0000 0000 1111 1111 1111 1111 1111 1111 : file size (in kB)
   */
  uint32_t      mFlags;

  CacheIndexRecord()
    : mFrecency(0)
    , mExpirationTime(nsICacheEntry::NO_EXPIRATION_TIME)
    , mAppId(nsILoadContextInfo::NO_APP_ID)
    , mFlags(0)
  {}
};

class CacheIndexEntry : public PLDHashEntryHdr
{
public:
  typedef const SHA1Sum::Hash& KeyType;
  typedef const SHA1Sum::Hash* KeyTypePointer;

  explicit CacheIndexEntry(KeyTypePointer aKey)
  {
    MOZ_COUNT_CTOR(CacheIndexEntry);
    mRec = new CacheIndexRecord();
    LOG(("CacheIndexEntry::CacheIndexEntry() - Created record [rec=%p]", mRec.get()));
    memcpy(&mRec->mHash, aKey, sizeof(SHA1Sum::Hash));
  }
  CacheIndexEntry(const CacheIndexEntry& aOther)
  {
    NS_NOTREACHED("CacheIndexEntry copy constructor is forbidden!");
  }
  ~CacheIndexEntry()
  {
    MOZ_COUNT_DTOR(CacheIndexEntry);
    LOG(("CacheIndexEntry::~CacheIndexEntry() - Deleting record [rec=%p]",
         mRec.get()));
  }

  // KeyEquals(): does this entry match this key?
  bool KeyEquals(KeyTypePointer aKey) const
  {
    return memcmp(&mRec->mHash, aKey, sizeof(SHA1Sum::Hash)) == 0;
  }

  // KeyToPointer(): Convert KeyType to KeyTypePointer
  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  // HashKey(): calculate the hash number
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return (reinterpret_cast<const uint32_t *>(aKey))[0];
  }

  // ALLOW_MEMMOVE can we move this class with memmove(), or do we have
  // to use the copy constructor?
  enum { ALLOW_MEMMOVE = true };

  bool operator==(const CacheIndexEntry& aOther) const
  {
    return KeyEquals(&aOther.mRec->mHash);
  }

  CacheIndexEntry& operator=(const CacheIndexEntry& aOther)
  {
    MOZ_ASSERT(memcmp(&mRec->mHash, &aOther.mRec->mHash,
               sizeof(SHA1Sum::Hash)) == 0);
    mRec->mFrecency = aOther.mRec->mFrecency;
    mRec->mExpirationTime = aOther.mRec->mExpirationTime;
    mRec->mAppId = aOther.mRec->mAppId;
    mRec->mFlags = aOther.mRec->mFlags;
    return *this;
  }

  void InitNew()
  {
    mRec->mFrecency = 0;
    mRec->mExpirationTime = nsICacheEntry::NO_EXPIRATION_TIME;
    mRec->mAppId = nsILoadContextInfo::NO_APP_ID;
    mRec->mFlags = 0;
  }

  void Init(uint32_t aAppId, bool aAnonymous, bool aInIsolatedMozBrowser, bool aPinned)
  {
    MOZ_ASSERT(mRec->mFrecency == 0);
    MOZ_ASSERT(mRec->mExpirationTime == nsICacheEntry::NO_EXPIRATION_TIME);
    MOZ_ASSERT(mRec->mAppId == nsILoadContextInfo::NO_APP_ID);
    // When we init the entry it must be fresh and may be dirty
    MOZ_ASSERT((mRec->mFlags & ~kDirtyMask) == kFreshMask);

    mRec->mAppId = aAppId;
    mRec->mFlags |= kInitializedMask;
    if (aAnonymous) {
      mRec->mFlags |= kAnonymousMask;
    }
    if (aInIsolatedMozBrowser) {
      mRec->mFlags |= kInIsolatedMozBrowserMask;
    }
    if (aPinned) {
      mRec->mFlags |= kPinnedMask;
    }
  }

  const SHA1Sum::Hash * Hash() const { return &mRec->mHash; }

  bool IsInitialized() const { return !!(mRec->mFlags & kInitializedMask); }

  uint32_t AppId() const { return mRec->mAppId; }
  bool     Anonymous() const { return !!(mRec->mFlags & kAnonymousMask); }
  bool     InIsolatedMozBrowser() const
  {
    return !!(mRec->mFlags & kInIsolatedMozBrowserMask);
  }

  bool IsRemoved() const { return !!(mRec->mFlags & kRemovedMask); }
  void MarkRemoved() { mRec->mFlags |= kRemovedMask; }

  bool IsDirty() const { return !!(mRec->mFlags & kDirtyMask); }
  void MarkDirty() { mRec->mFlags |= kDirtyMask; }
  void ClearDirty() { mRec->mFlags &= ~kDirtyMask; }

  bool IsFresh() const { return !!(mRec->mFlags & kFreshMask); }
  void MarkFresh() { mRec->mFlags |= kFreshMask; }

  bool IsPinned() const { return !!(mRec->mFlags & kPinnedMask); }

  void     SetFrecency(uint32_t aFrecency) { mRec->mFrecency = aFrecency; }
  uint32_t GetFrecency() const { return mRec->mFrecency; }

  void     SetExpirationTime(uint32_t aExpirationTime)
  {
    mRec->mExpirationTime = aExpirationTime;
  }
  uint32_t GetExpirationTime() const { return mRec->mExpirationTime; }

  // Sets filesize in kilobytes.
  void     SetFileSize(uint32_t aFileSize)
  {
    if (aFileSize > kFileSizeMask) {
      LOG(("CacheIndexEntry::SetFileSize() - FileSize is too large, "
           "truncating to %u", kFileSizeMask));
      aFileSize = kFileSizeMask;
    }
    mRec->mFlags &= ~kFileSizeMask;
    mRec->mFlags |= aFileSize;
  }
  // Returns filesize in kilobytes.
  uint32_t GetFileSize() const { return GetFileSize(mRec); }
  static uint32_t GetFileSize(CacheIndexRecord *aRec)
  {
    return aRec->mFlags & kFileSizeMask;
  }
  static uint32_t IsPinned(CacheIndexRecord *aRec)
  {
    return aRec->mFlags & kPinnedMask;
  }
  bool     IsFileEmpty() const { return GetFileSize() == 0; }

  void WriteToBuf(void *aBuf)
  {
    CacheIndexRecord *dst = reinterpret_cast<CacheIndexRecord *>(aBuf);

    // Copy the whole record to the buffer.
    memcpy(aBuf, mRec, sizeof(CacheIndexRecord));

    // Dirty and fresh flags should never go to disk, since they make sense only
    // during current session.
    dst->mFlags &= ~kDirtyMask;
    dst->mFlags &= ~kFreshMask;

#if defined(IS_LITTLE_ENDIAN)
    // Data in the buffer are in machine byte order and we want them in network
    // byte order.
    NetworkEndian::writeUint32(&dst->mFrecency, dst->mFrecency);
    NetworkEndian::writeUint32(&dst->mExpirationTime, dst->mExpirationTime);
    NetworkEndian::writeUint32(&dst->mAppId, dst->mAppId);
    NetworkEndian::writeUint32(&dst->mFlags, dst->mFlags);
#endif
  }

  void ReadFromBuf(void *aBuf)
  {
    CacheIndexRecord *src= reinterpret_cast<CacheIndexRecord *>(aBuf);
    MOZ_ASSERT(memcmp(&mRec->mHash, &src->mHash,
               sizeof(SHA1Sum::Hash)) == 0);

    mRec->mFrecency = NetworkEndian::readUint32(&src->mFrecency);
    mRec->mExpirationTime = NetworkEndian::readUint32(&src->mExpirationTime);
    mRec->mAppId = NetworkEndian::readUint32(&src->mAppId);
    mRec->mFlags = NetworkEndian::readUint32(&src->mFlags);
  }

  void Log() const {
    LOG(("CacheIndexEntry::Log() [this=%p, hash=%08x%08x%08x%08x%08x, "
         "fresh=%u, initialized=%u, removed=%u, dirty=%u, anonymous=%u, "
         "inIsolatedMozBrowser=%u, appId=%u, frecency=%u, expirationTime=%u, "
         "size=%u]",
         this, LOGSHA1(mRec->mHash), IsFresh(), IsInitialized(), IsRemoved(),
         IsDirty(), Anonymous(), InIsolatedMozBrowser(), AppId(), GetFrecency(),
         GetExpirationTime(), GetFileSize()));
  }

  static bool RecordMatchesLoadContextInfo(CacheIndexRecord *aRec,
                                           nsILoadContextInfo *aInfo)
  {
    if (!aInfo->IsPrivate() &&
        aInfo->OriginAttributesPtr()->mAppId == aRec->mAppId &&
        aInfo->IsAnonymous() == !!(aRec->mFlags & kAnonymousMask) &&
        aInfo->OriginAttributesPtr()->mInIsolatedMozBrowser == !!(aRec->mFlags & kInIsolatedMozBrowserMask)) {
      return true;
    }

    return false;
  }

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
  {
    return mallocSizeOf(mRec.get());
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
  {
    return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
  }

private:
  friend class CacheIndexEntryUpdate;
  friend class CacheIndex;
  friend class CacheIndexEntryAutoManage;

  static const uint32_t kInitializedMask = 0x80000000;
  static const uint32_t kAnonymousMask   = 0x40000000;
  static const uint32_t kInIsolatedMozBrowserMask   = 0x20000000;

  // This flag is set when the entry was removed. We need to keep this
  // information in memory until we write the index file.
  static const uint32_t kRemovedMask     = 0x10000000;

  // This flag is set when the information in memory is not in sync with the
  // information in index file on disk.
  static const uint32_t kDirtyMask       = 0x08000000;

  // This flag is set when the information about the entry is fresh, i.e.
  // we've created or opened this entry during this session, or we've seen
  // this entry during update or build process.
  static const uint32_t kFreshMask       = 0x04000000;

  // Indicates a pinned entry.
  static const uint32_t kPinnedMask      = 0x02000000;

  static const uint32_t kReservedMask    = 0x01000000;

  // FileSize in kilobytes
  static const uint32_t kFileSizeMask    = 0x00FFFFFF;

  nsAutoPtr<CacheIndexRecord> mRec;
};

class CacheIndexEntryUpdate : public CacheIndexEntry
{
public:
  explicit CacheIndexEntryUpdate(CacheIndexEntry::KeyTypePointer aKey)
    : CacheIndexEntry(aKey)
    , mUpdateFlags(0)
  {
    MOZ_COUNT_CTOR(CacheIndexEntryUpdate);
    LOG(("CacheIndexEntryUpdate::CacheIndexEntryUpdate()"));
  }
  ~CacheIndexEntryUpdate()
  {
    MOZ_COUNT_DTOR(CacheIndexEntryUpdate);
    LOG(("CacheIndexEntryUpdate::~CacheIndexEntryUpdate()"));
  }

  CacheIndexEntryUpdate& operator=(const CacheIndexEntry& aOther)
  {
    MOZ_ASSERT(memcmp(&mRec->mHash, &aOther.mRec->mHash,
               sizeof(SHA1Sum::Hash)) == 0);
    mUpdateFlags = 0;
    *(static_cast<CacheIndexEntry *>(this)) = aOther;
    return *this;
  }

  void InitNew()
  {
    mUpdateFlags = kFrecencyUpdatedMask | kExpirationUpdatedMask |
                   kFileSizeUpdatedMask;
    CacheIndexEntry::InitNew();
  }

  void SetFrecency(uint32_t aFrecency)
  {
    mUpdateFlags |= kFrecencyUpdatedMask;
    CacheIndexEntry::SetFrecency(aFrecency);
  }

  void SetExpirationTime(uint32_t aExpirationTime)
  {
    mUpdateFlags |= kExpirationUpdatedMask;
    CacheIndexEntry::SetExpirationTime(aExpirationTime);
  }

  void SetFileSize(uint32_t aFileSize)
  {
    mUpdateFlags |= kFileSizeUpdatedMask;
    CacheIndexEntry::SetFileSize(aFileSize);
  }

  void ApplyUpdate(CacheIndexEntry *aDst) {
    MOZ_ASSERT(memcmp(&mRec->mHash, &aDst->mRec->mHash,
               sizeof(SHA1Sum::Hash)) == 0);
    if (mUpdateFlags & kFrecencyUpdatedMask) {
      aDst->mRec->mFrecency = mRec->mFrecency;
    }
    if (mUpdateFlags & kExpirationUpdatedMask) {
      aDst->mRec->mExpirationTime = mRec->mExpirationTime;
    }
    aDst->mRec->mAppId = mRec->mAppId;
    if (mUpdateFlags & kFileSizeUpdatedMask) {
      aDst->mRec->mFlags = mRec->mFlags;
    } else {
      // Copy all flags except file size.
      aDst->mRec->mFlags &= kFileSizeMask;
      aDst->mRec->mFlags |= (mRec->mFlags & ~kFileSizeMask);
    }
  }

private:
  static const uint32_t kFrecencyUpdatedMask = 0x00000001;
  static const uint32_t kExpirationUpdatedMask = 0x00000002;
  static const uint32_t kFileSizeUpdatedMask = 0x00000004;

  uint32_t mUpdateFlags;
};

class CacheIndexStats
{
public:
  CacheIndexStats()
    : mCount(0)
    , mNotInitialized(0)
    , mRemoved(0)
    , mDirty(0)
    , mFresh(0)
    , mEmpty(0)
    , mSize(0)
#ifdef DEBUG
    , mStateLogged(false)
    , mDisableLogging(false)
#endif
  {
  }

  bool operator==(const CacheIndexStats& aOther) const
  {
    return
#ifdef DEBUG
           aOther.mStateLogged == mStateLogged &&
#endif
           aOther.mCount == mCount &&
           aOther.mNotInitialized == mNotInitialized &&
           aOther.mRemoved == mRemoved &&
           aOther.mDirty == mDirty &&
           aOther.mFresh == mFresh &&
           aOther.mEmpty == mEmpty &&
           aOther.mSize == mSize;
  }

#ifdef DEBUG
  void DisableLogging() {
    mDisableLogging = true;
  }
#endif

  void Log() {
    LOG(("CacheIndexStats::Log() [count=%u, notInitialized=%u, removed=%u, "
         "dirty=%u, fresh=%u, empty=%u, size=%u]", mCount, mNotInitialized,
         mRemoved, mDirty, mFresh, mEmpty, mSize));
  }

  void Clear() {
    MOZ_ASSERT(!mStateLogged, "CacheIndexStats::Clear() - state logged!");

    mCount = 0;
    mNotInitialized = 0;
    mRemoved = 0;
    mDirty = 0;
    mFresh = 0;
    mEmpty = 0;
    mSize = 0;
  }

#ifdef DEBUG
  bool StateLogged() {
    return mStateLogged;
  }
#endif

  uint32_t Count() {
    MOZ_ASSERT(!mStateLogged, "CacheIndexStats::Count() - state logged!");
    return mCount;
  }

  uint32_t Dirty() {
    MOZ_ASSERT(!mStateLogged, "CacheIndexStats::Dirty() - state logged!");
    return mDirty;
  }

  uint32_t Fresh() {
    MOZ_ASSERT(!mStateLogged, "CacheIndexStats::Fresh() - state logged!");
    return mFresh;
  }

  uint32_t ActiveEntriesCount() {
    MOZ_ASSERT(!mStateLogged, "CacheIndexStats::ActiveEntriesCount() - state "
               "logged!");
    return mCount - mRemoved - mNotInitialized - mEmpty;
  }

  uint32_t Size() {
    MOZ_ASSERT(!mStateLogged, "CacheIndexStats::Size() - state logged!");
    return mSize;
  }

  void BeforeChange(const CacheIndexEntry *aEntry) {
#ifdef DEBUG_STATS
    if (!mDisableLogging) {
      LOG(("CacheIndexStats::BeforeChange()"));
      Log();
    }
#endif

    MOZ_ASSERT(!mStateLogged, "CacheIndexStats::BeforeChange() - state "
               "logged!");
#ifdef DEBUG
    mStateLogged = true;
#endif
    if (aEntry) {
      MOZ_ASSERT(mCount);
      mCount--;
      if (aEntry->IsDirty()) {
        MOZ_ASSERT(mDirty);
        mDirty--;
      }
      if (aEntry->IsFresh()) {
        MOZ_ASSERT(mFresh);
        mFresh--;
      }
      if (aEntry->IsRemoved()) {
        MOZ_ASSERT(mRemoved);
        mRemoved--;
      } else {
        if (!aEntry->IsInitialized()) {
          MOZ_ASSERT(mNotInitialized);
          mNotInitialized--;
        } else {
          if (aEntry->IsFileEmpty()) {
            MOZ_ASSERT(mEmpty);
            mEmpty--;
          } else {
            MOZ_ASSERT(mSize >= aEntry->GetFileSize());
            mSize -= aEntry->GetFileSize();
          }
        }
      }
    }
  }

  void AfterChange(const CacheIndexEntry *aEntry) {
    MOZ_ASSERT(mStateLogged, "CacheIndexStats::AfterChange() - state not "
               "logged!");
#ifdef DEBUG
    mStateLogged = false;
#endif
    if (aEntry) {
      ++mCount;
      if (aEntry->IsDirty()) {
        mDirty++;
      }
      if (aEntry->IsFresh()) {
        mFresh++;
      }
      if (aEntry->IsRemoved()) {
        mRemoved++;
      } else {
        if (!aEntry->IsInitialized()) {
          mNotInitialized++;
        } else {
          if (aEntry->IsFileEmpty()) {
            mEmpty++;
          } else {
            mSize += aEntry->GetFileSize();
          }
        }
      }
    }

#ifdef DEBUG_STATS
    if (!mDisableLogging) {
      LOG(("CacheIndexStats::AfterChange()"));
      Log();
    }
#endif
  }

private:
  uint32_t mCount;
  uint32_t mNotInitialized;
  uint32_t mRemoved;
  uint32_t mDirty;
  uint32_t mFresh;
  uint32_t mEmpty;
  uint32_t mSize;
#ifdef DEBUG
  // We completely remove the data about an entry from the stats in
  // BeforeChange() and set this flag to true. The entry is then modified,
  // deleted or created and the data is again put into the stats and this flag
  // set to false. Statistics must not be read during this time since the
  // information is not correct.
  bool     mStateLogged;

  // Disables logging in this instance of CacheIndexStats
  bool     mDisableLogging;
#endif
};

class CacheIndex : public CacheFileIOListener
                 , public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  CacheIndex();

  static nsresult Init(nsIFile *aCacheDirectory);
  static nsresult PreShutdown();
  static nsresult Shutdown();

  // Following methods can be called only on IO thread.

  // Add entry to the index. The entry shouldn't be present in index. This
  // method is called whenever a new handle for a new entry file is created. The
  // newly created entry is not initialized and it must be either initialized
  // with InitEntry() or removed with RemoveEntry().
  static nsresult AddEntry(const SHA1Sum::Hash *aHash);

  // Inform index about an existing entry that should be present in index. This
  // method is called whenever a new handle for an existing entry file is
  // created. Like in case of AddEntry(), either InitEntry() or RemoveEntry()
  // must be called on the entry, since the entry is not initizlized if the
  // index is outdated.
  static nsresult EnsureEntryExists(const SHA1Sum::Hash *aHash);

  // Initialize the entry. It MUST be present in index. Call to AddEntry() or
  // EnsureEntryExists() must precede the call to this method.
  static nsresult InitEntry(const SHA1Sum::Hash *aHash,
                            uint32_t             aAppId,
                            bool                 aAnonymous,
                            bool                 aInIsolatedMozBrowser,
                            bool                 aPinned);

  // Remove entry from index. The entry should be present in index.
  static nsresult RemoveEntry(const SHA1Sum::Hash *aHash);

  // Update some information in entry. The entry MUST be present in index and
  // MUST be initialized. Call to AddEntry() or EnsureEntryExists() and to
  // InitEntry() must precede the call to this method.
  // Pass nullptr if the value didn't change.
  static nsresult UpdateEntry(const SHA1Sum::Hash *aHash,
                              const uint32_t      *aFrecency,
                              const uint32_t      *aExpirationTime,
                              const uint32_t      *aSize);

  // Remove all entries from the index. Called when clearing the whole cache.
  static nsresult RemoveAll();

  enum EntryStatus {
    EXISTS         = 0,
    DOES_NOT_EXIST = 1,
    DO_NOT_KNOW    = 2
  };

  // Returns status of the entry in index for the given key. It can be called
  // on any thread.
  // If _pinned is non-null, it's filled with pinning status of the entry.
  static nsresult HasEntry(const nsACString &aKey, EntryStatus *_retval,
                           bool *_pinned = nullptr);
  static nsresult HasEntry(const SHA1Sum::Hash &hash, EntryStatus *_retval,
                           bool *_pinned = nullptr);

  // Returns a hash of the least important entry that should be evicted if the
  // cache size is over limit and also returns a total number of all entries in
  // the index minus the number of forced valid entries and unpinned entries
  // that we encounter when searching (see below)
  static nsresult GetEntryForEviction(bool aIgnoreEmptyEntries, SHA1Sum::Hash *aHash, uint32_t *aCnt);

  // Checks if a cache entry is currently forced valid. Used to prevent an entry
  // (that has been forced valid) from being evicted when the cache size reaches
  // its limit.
  static bool IsForcedValidEntry(const SHA1Sum::Hash *aHash);

  // Returns cache size in kB.
  static nsresult GetCacheSize(uint32_t *_retval);

  // Returns number of entry files in the cache
  static nsresult GetEntryFileCount(uint32_t *_retval);

  // Synchronously returns the disk occupation and number of entries per-context.
  // Callable on any thread.
  static nsresult GetCacheStats(nsILoadContextInfo *aInfo, uint32_t *aSize, uint32_t *aCount);

  // Asynchronously gets the disk cache size, used for display in the UI.
  static nsresult AsyncGetDiskConsumption(nsICacheStorageConsumptionObserver* aObserver);

  // Returns an iterator that returns entries matching a given context that were
  // present in the index at the time this method was called. If aAddNew is true
  // then the iterator will also return entries created after this call.
  // NOTE: When some entry is removed from index it is removed also from the
  // iterator regardless what aAddNew was passed.
  static nsresult GetIterator(nsILoadContextInfo *aInfo, bool aAddNew,
                              CacheIndexIterator **_retval);

  // Returns true if we _think_ that the index is up to date. I.e. the state is
  // READY or WRITING and mIndexNeedsUpdate as well as mShuttingDown is false.
  static nsresult IsUpToDate(bool *_retval);

  // Called from CacheStorageService::Clear() and CacheFileContextEvictor::EvictEntries(),
  // sets a flag that blocks notification to AsyncGetDiskConsumption.
  static void OnAsyncEviction(bool aEvicting);

  // Memory reporting
  static size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
  static size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

private:
  friend class CacheIndexEntryAutoManage;
  friend class FileOpenHelper;
  friend class CacheIndexIterator;

  virtual ~CacheIndex();

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult) override;
  nsresult   OnFileOpenedInternal(FileOpenHelper *aOpener,
                                  CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                           nsresult aResult) override;
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, char *aBuf, nsresult aResult) override;
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult) override;
  NS_IMETHOD OnEOFSet(CacheFileHandle *aHandle, nsresult aResult) override;
  NS_IMETHOD OnFileRenamed(CacheFileHandle *aHandle, nsresult aResult) override;

  nsresult InitInternal(nsIFile *aCacheDirectory);
  void     PreShutdownInternal();

  // This method returns false when index is not initialized or is shut down.
  bool IsIndexUsable();

  // This method checks whether the entry has the same values of appId,
  // isAnonymous and isInBrowser. We don't expect to find a collision since
  // these values are part of the key that we hash and we use a strong hash
  // function.
  static bool IsCollision(CacheIndexEntry *aEntry,
                          uint32_t         aAppId,
                          bool             aAnonymous,
                          bool             aInIsolatedMozBrowser);

  // Checks whether any of the information about the entry has changed.
  static bool HasEntryChanged(CacheIndexEntry *aEntry,
                              const uint32_t  *aFrecency,
                              const uint32_t  *aExpirationTime,
                              const uint32_t  *aSize);

  // Merge all pending operations from mPendingUpdates into mIndex.
  void ProcessPendingOperations();

  // Following methods perform writing of the index file.
  //
  // The index is written periodically, but not earlier than once in
  // kMinDumpInterval and there must be at least kMinUnwrittenChanges
  // differences between index on disk and in memory. Index is always first
  // written to a temporary file and the old index file is replaced when the
  // writing process succeeds.
  //
  // Starts writing of index when both limits (minimal delay between writes and
  // minimum number of changes in index) were exceeded.
  bool WriteIndexToDiskIfNeeded();
  // Starts writing of index file.
  void WriteIndexToDisk();
  // Serializes part of mIndex hashtable to the write buffer a writes the buffer
  // to the file.
  void WriteRecords();
  // Finalizes writing process.
  void FinishWrite(bool aSucceeded);

  // Following methods perform writing of the journal during shutdown. All these
  // methods must be called only during shutdown since they write/delete files
  // directly on the main thread instead of using CacheFileIOManager that does
  // it asynchronously on IO thread. Journal contains only entries that are
  // dirty, i.e. changes that are not present in the index file on the disk.
  // When the log is written successfully, the dirty flag in index file is
  // cleared.
  nsresult GetFile(const nsACString &aName, nsIFile **_retval);
  nsresult RemoveFile(const nsACString &aName);
  void     RemoveIndexFromDisk();
  // Writes journal to the disk and clears dirty flag in index header.
  nsresult WriteLogToDisk();

  // Following methods perform reading of the index from the disk.
  //
  // Index is read at startup just after initializing the CacheIndex. There are
  // 3 files used when manipulating with index: index file, journal file and
  // a temporary file. All files contain the hash of the data, so we can check
  // whether the content is valid and complete. Index file contains also a dirty
  // flag in the index header which is unset on a clean shutdown. During opening
  // and reading of the files we determine the status of the whole index from
  // the states of the separate files. Following table shows all possible
  // combinations:
  //
  // index, journal, tmpfile
  // M      *        *       - index is missing    -> BUILD
  // I      *        *       - index is invalid    -> BUILD
  // D      *        *       - index is dirty      -> UPDATE
  // C      M        *       - index is dirty      -> UPDATE
  // C      I        *       - unexpected state    -> UPDATE
  // C      V        E       - unexpected state    -> UPDATE
  // C      V        M       - index is up to date -> READY
  //
  // where the letters mean:
  //   * - any state
  //   E - file exists
  //   M - file is missing
  //   I - data is invalid (parsing failed or hash didn't match)
  //   D - dirty (data in index file is correct, but dirty flag is set)
  //   C - clean (index file is clean)
  //   V - valid (data in journal file is correct)
  //
  // Note: We accept the data from journal only when the index is up to date as
  // a whole (i.e. C,V,M state).
  //
  // We rename the journal file to the temporary file as soon as possible after
  // initial test to ensure that we start update process on the next startup if
  // FF crashes during parsing of the index.
  //
  // Initiates reading index from disk.
  void ReadIndexFromDisk();
  // Starts reading data from index file.
  void StartReadingIndex();
  // Parses data read from index file.
  void ParseRecords();
  // Starts reading data from journal file.
  void StartReadingJournal();
  // Parses data read from journal file.
  void ParseJournal();
  // Merges entries from journal into mIndex.
  void MergeJournal();
  // In debug build this method checks that we have no fresh entry in mIndex
  // after we finish reading index and before we process pending operations.
  void EnsureNoFreshEntry();
  // In debug build this method is called after processing pending operations
  // to make sure mIndexStats contains correct information.
  void EnsureCorrectStats();
  // Finalizes reading process.
  void FinishRead(bool aSucceeded);

  // Following methods perform updating and building of the index.
  // Timer callback that starts update or build process.
  static void DelayedUpdate(nsITimer *aTimer, void *aClosure);
  // Posts timer event that start update or build process.
  nsresult ScheduleUpdateTimer(uint32_t aDelay);
  nsresult SetupDirectoryEnumerator();
  void InitEntryFromDiskData(CacheIndexEntry *aEntry,
                             CacheFileMetadata *aMetaData,
                             int64_t aFileSize);
  // Returns true when either a timer is scheduled or event is posted.
  bool IsUpdatePending();
  // Iterates through all files in entries directory that we didn't create/open
  // during this session, parses them and adds the entries to the index.
  void BuildIndex();

  bool StartUpdatingIndexIfNeeded(bool aSwitchingToReadyState = false);
  // Starts update or build process or fires a timer when it is too early after
  // startup.
  void StartUpdatingIndex(bool aRebuild);
  // Iterates through all files in entries directory that we didn't create/open
  // during this session and theirs last modified time is newer than timestamp
  // in the index header. Parses the files and adds the entries to the index.
  void UpdateIndex();
  // Finalizes update or build process.
  void FinishUpdate(bool aSucceeded);

  void RemoveNonFreshEntries();

  enum EState {
    // Initial state in which the index is not usable
    // Possible transitions:
    //  -> READING
    INITIAL  = 0,

    // Index is being read from the disk.
    // Possible transitions:
    //  -> INITIAL  - We failed to dispatch a read event.
    //  -> BUILDING - No or corrupted index file was found.
    //  -> UPDATING - No or corrupted journal file was found.
    //              - Dirty flag was set in index header.
    //  -> READY    - Index was read successfully or was interrupted by
    //                pre-shutdown.
    //  -> SHUTDOWN - This could happen only in case of pre-shutdown failure.
    READING  = 1,

    // Index is being written to the disk.
    // Possible transitions:
    //  -> READY    - Writing of index finished or was interrupted by
    //                pre-shutdown..
    //  -> UPDATING - Writing of index finished, but index was found outdated
    //                during writing.
    //  -> SHUTDOWN - This could happen only in case of pre-shutdown failure.
    WRITING  = 2,

    // Index is being build.
    // Possible transitions:
    //  -> READY    - Building of index finished or was interrupted by
    //                pre-shutdown.
    //  -> SHUTDOWN - This could happen only in case of pre-shutdown failure.
    BUILDING = 3,

    // Index is being updated.
    // Possible transitions:
    //  -> READY    - Updating of index finished or was interrupted by
    //                pre-shutdown.
    //  -> SHUTDOWN - This could happen only in case of pre-shutdown failure.
    UPDATING = 4,

    // Index is ready.
    // Possible transitions:
    //  -> UPDATING - Index was found outdated.
    //  -> SHUTDOWN - Index is shutting down.
    READY    = 5,

    // Index is shutting down.
    SHUTDOWN = 6
  };

  static char const * StateString(EState aState);
  void ChangeState(EState aNewState);
  void NotifyAsyncGetDiskConsumptionCallbacks();

  // Allocates and releases buffer used for reading and writing index.
  void AllocBuffer();
  void ReleaseBuffer();

  // Methods used by CacheIndexEntryAutoManage to keep the arrays up to date.
  void InsertRecordToFrecencyArray(CacheIndexRecord *aRecord);
  void RemoveRecordFromFrecencyArray(CacheIndexRecord *aRecord);

  // Methods used by CacheIndexEntryAutoManage to keep the iterators up to date.
  void AddRecordToIterators(CacheIndexRecord *aRecord);
  void RemoveRecordFromIterators(CacheIndexRecord *aRecord);
  void ReplaceRecordInIterators(CacheIndexRecord *aOldRecord,
                                CacheIndexRecord *aNewRecord);

  // Memory reporting (private part)
  size_t SizeOfExcludingThisInternal(mozilla::MallocSizeOf mallocSizeOf) const;

  void ReportHashStats();

  static CacheIndex *gInstance;
  static StaticMutex sLock;

  nsCOMPtr<nsIFile> mCacheDirectory;

  EState         mState;
  // Timestamp of time when the index was initialized. We use it to delay
  // initial update or build of index.
  TimeStamp      mStartTime;
  // Set to true in PreShutdown(), it is checked on variaous places to prevent
  // starting any process (write, update, etc.) during shutdown.
  bool           mShuttingDown;
  // When set to true, update process should start as soon as possible. This
  // flag is set whenever we find some inconsistency which would be fixed by
  // update process. The flag is checked always when switching to READY state.
  // To make sure we start the update process as soon as possible, methods that
  // set this flag should also call StartUpdatingIndexIfNeeded() to cover the
  // case when we are currently in READY state.
  bool           mIndexNeedsUpdate;
  // Set at the beginning of RemoveAll() which clears the whole index. When
  // removing all entries we must stop any pending reading, writing, updating or
  // building operation. This flag is checked at various places and it prevents
  // we won't start another operation (e.g. canceling reading of the index would
  // normally start update or build process)
  bool           mRemovingAll;
  // Whether the index file on disk exists and is valid.
  bool           mIndexOnDiskIsValid;
  // When something goes wrong during updating or building process, we don't
  // mark index clean (and also don't write journal) to ensure that update or
  // build will be initiated on the next start.
  bool           mDontMarkIndexClean;
  // Timestamp value from index file. It is used during update process to skip
  // entries that were last modified before this timestamp.
  uint32_t       mIndexTimeStamp;
  // Timestamp of last time the index was dumped to disk.
  // NOTE: The index might not be necessarily dumped at this time. The value
  // is used to schedule next dump of the index.
  TimeStamp      mLastDumpTime;

  // Timer of delayed update/build.
  nsCOMPtr<nsITimer> mUpdateTimer;
  // True when build or update event is posted
  bool               mUpdateEventPending;

  // Helper members used when reading/writing index from/to disk.
  // Contains number of entries that should be skipped:
  //  - in hashtable when writing index because they were already written
  //  - in index file when reading index because they were already read
  uint32_t                  mSkipEntries;
  // Number of entries that should be written to disk. This is number of entries
  // in hashtable that are initialized and are not marked as removed when writing
  // begins.
  uint32_t                  mProcessEntries;
  char                     *mRWBuf;
  uint32_t                  mRWBufSize;
  uint32_t                  mRWBufPos;
  RefPtr<CacheHash>         mRWHash;

  // True if read or write operation is pending. It is used to ensure that
  // mRWBuf is not freed until OnDataRead or OnDataWritten is called.
  bool                      mRWPending;

  // Reading of journal succeeded if true.
  bool                      mJournalReadSuccessfully;

  // Handle used for writing and reading index file.
  RefPtr<CacheFileHandle> mIndexHandle;
  // Handle used for reading journal file.
  RefPtr<CacheFileHandle> mJournalHandle;
  // Used to check the existence of the file during reading process.
  RefPtr<CacheFileHandle> mTmpHandle;

  RefPtr<FileOpenHelper>    mIndexFileOpener;
  RefPtr<FileOpenHelper>    mJournalFileOpener;
  RefPtr<FileOpenHelper>    mTmpFileOpener;

  // Directory enumerator used when building and updating index.
  nsCOMPtr<nsIDirectoryEnumerator> mDirEnumerator;

  // Main index hashtable.
  nsTHashtable<CacheIndexEntry> mIndex;

  // We cannot add, remove or change any entry in mIndex in states READING and
  // WRITING. We track all changes in mPendingUpdates during these states.
  nsTHashtable<CacheIndexEntryUpdate> mPendingUpdates;

  // Contains information statistics for mIndex + mPendingUpdates.
  CacheIndexStats               mIndexStats;

  // When reading journal, we must first parse the whole file and apply the
  // changes iff the journal was read successfully. mTmpJournal is used to store
  // entries from the journal file. We throw away all these entries if parsing
  // of the journal fails or the hash does not match.
  nsTHashtable<CacheIndexEntry> mTmpJournal;

  // An array that keeps entry records ordered by eviction preference; we take
  // the entry with lowest valid frecency. Zero frecency is an initial value
  // and such entries are stored at the end of the array. Uninitialized entries
  // and entries marked as deleted are not present in this array.
  nsTArray<CacheIndexRecord *>  mFrecencyArray;
  bool                          mFrecencyArraySorted;

  nsTArray<CacheIndexIterator *> mIterators;

  // This flag is true iff we are between CacheStorageService:Clear() and processing
  // all contexts to be evicted.  It will make UI to show "calculating" instead of
  // any intermediate cache size.
  bool mAsyncGetDiskConsumptionBlocked;

  class DiskConsumptionObserver : public Runnable
  {
  public:
    static DiskConsumptionObserver* Init(nsICacheStorageConsumptionObserver* aObserver)
    {
      nsWeakPtr observer = do_GetWeakReference(aObserver);
      if (!observer)
        return nullptr;

      return new DiskConsumptionObserver(observer);
    }

    void OnDiskConsumption(int64_t aSize)
    {
      mSize = aSize;
      NS_DispatchToMainThread(this);
    }

  private:
    explicit DiskConsumptionObserver(nsWeakPtr const &aWeakObserver)
      : mObserver(aWeakObserver) { }
    virtual ~DiskConsumptionObserver() {
      if (mObserver && !NS_IsMainThread()) {
        NS_ReleaseOnMainThread(mObserver.forget());
      }
    }

    NS_IMETHODIMP Run()
    {
      MOZ_ASSERT(NS_IsMainThread());

      nsCOMPtr<nsICacheStorageConsumptionObserver> observer =
        do_QueryReferent(mObserver);

      mObserver = nullptr;

      if (observer) {
        observer->OnNetworkCacheDiskConsumption(mSize);
      }

      return NS_OK;
    }

    nsWeakPtr mObserver;
    int64_t mSize;
  };

  // List of async observers that want to get disk consumption information
  nsTArray<RefPtr<DiskConsumptionObserver> > mDiskConsumptionObservers;
};

} // namespace net
} // namespace mozilla

#endif
