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
#include "nsIWeakReferenceUtils.h"
#include "nsTHashtable.h"
#include "nsThreadUtils.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/SHA1.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/TimeStamp.h"

class nsIFile;
class nsIDirectoryEnumerator;
class nsITimer;

#ifdef DEBUG
#  define DEBUG_STATS 1
#endif

namespace mozilla {
namespace net {

class CacheFileMetadata;
class FileOpenHelper;
class CacheIndexIterator;

const uint16_t kIndexTimeNotAvailable = 0xFFFFU;
const uint16_t kIndexTimeOutOfBound = 0xFFFEU;

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

static_assert(sizeof(CacheIndexHeader::mVersion) +
                      sizeof(CacheIndexHeader::mTimeStamp) +
                      sizeof(CacheIndexHeader::mIsDirty) ==
                  sizeof(CacheIndexHeader),
              "Unexpected sizeof(CacheIndexHeader)!");

#pragma pack(push, 1)
struct CacheIndexRecord {
  SHA1Sum::Hash mHash;
  uint32_t mFrecency;
  OriginAttrsHash mOriginAttrsHash;
  uint16_t mOnStartTime;
  uint16_t mOnStopTime;
  uint8_t mContentType;
  uint16_t mBaseDomainAccessCount;

  /*
   *    1000 0000 0000 0000 0000 0000 0000 0000 : initialized
   *    0100 0000 0000 0000 0000 0000 0000 0000 : anonymous
   *    0010 0000 0000 0000 0000 0000 0000 0000 : removed
   *    0001 0000 0000 0000 0000 0000 0000 0000 : dirty
   *    0000 1000 0000 0000 0000 0000 0000 0000 : fresh
   *    0000 0100 0000 0000 0000 0000 0000 0000 : pinned
   *    0000 0010 0000 0000 0000 0000 0000 0000 : has cached alt data
   *    0000 0001 0000 0000 0000 0000 0000 0000 : reserved
   *    0000 0000 1111 1111 1111 1111 1111 1111 : file size (in kB)
   */
  uint32_t mFlags;

  CacheIndexRecord()
      : mFrecency(0),
        mOriginAttrsHash(0),
        mOnStartTime(kIndexTimeNotAvailable),
        mOnStopTime(kIndexTimeNotAvailable),
        mContentType(nsICacheEntry::CONTENT_TYPE_UNKNOWN),
        mBaseDomainAccessCount(0),
        mFlags(0) {}
};
#pragma pack(pop)

static_assert(sizeof(CacheIndexRecord::mHash) +
                      sizeof(CacheIndexRecord::mFrecency) +
                      sizeof(CacheIndexRecord::mOriginAttrsHash) +
                      sizeof(CacheIndexRecord::mOnStartTime) +
                      sizeof(CacheIndexRecord::mOnStopTime) +
                      sizeof(CacheIndexRecord::mContentType) +
                      sizeof(CacheIndexRecord::mBaseDomainAccessCount) +
                      sizeof(CacheIndexRecord::mFlags) ==
                  sizeof(CacheIndexRecord),
              "Unexpected sizeof(CacheIndexRecord)!");

class CacheIndexEntry : public PLDHashEntryHdr {
 public:
  typedef const SHA1Sum::Hash& KeyType;
  typedef const SHA1Sum::Hash* KeyTypePointer;

  explicit CacheIndexEntry(KeyTypePointer aKey) {
    MOZ_COUNT_CTOR(CacheIndexEntry);
    mRec = new CacheIndexRecord();
    LOG(("CacheIndexEntry::CacheIndexEntry() - Created record [rec=%p]",
         mRec.get()));
    memcpy(&mRec->mHash, aKey, sizeof(SHA1Sum::Hash));
  }
  CacheIndexEntry(const CacheIndexEntry& aOther) {
    MOZ_ASSERT_UNREACHABLE("CacheIndexEntry copy constructor is forbidden!");
  }
  ~CacheIndexEntry() {
    MOZ_COUNT_DTOR(CacheIndexEntry);
    LOG(("CacheIndexEntry::~CacheIndexEntry() - Deleting record [rec=%p]",
         mRec.get()));
  }

  // KeyEquals(): does this entry match this key?
  bool KeyEquals(KeyTypePointer aKey) const {
    return memcmp(&mRec->mHash, aKey, sizeof(SHA1Sum::Hash)) == 0;
  }

  // KeyToPointer(): Convert KeyType to KeyTypePointer
  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  // HashKey(): calculate the hash number
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return (reinterpret_cast<const uint32_t*>(aKey))[0];
  }

  // ALLOW_MEMMOVE can we move this class with memmove(), or do we have
  // to use the copy constructor?
  enum { ALLOW_MEMMOVE = true };

  bool operator==(const CacheIndexEntry& aOther) const {
    return KeyEquals(&aOther.mRec->mHash);
  }

  CacheIndexEntry& operator=(const CacheIndexEntry& aOther) {
    MOZ_ASSERT(
        memcmp(&mRec->mHash, &aOther.mRec->mHash, sizeof(SHA1Sum::Hash)) == 0);
    mRec->mFrecency = aOther.mRec->mFrecency;
    mRec->mOriginAttrsHash = aOther.mRec->mOriginAttrsHash;
    mRec->mOnStartTime = aOther.mRec->mOnStartTime;
    mRec->mOnStopTime = aOther.mRec->mOnStopTime;
    mRec->mContentType = aOther.mRec->mContentType;
    mRec->mBaseDomainAccessCount = aOther.mRec->mBaseDomainAccessCount;
    mRec->mFlags = aOther.mRec->mFlags;
    return *this;
  }

  void InitNew() {
    mRec->mFrecency = 0;
    mRec->mOriginAttrsHash = 0;
    mRec->mOnStartTime = kIndexTimeNotAvailable;
    mRec->mOnStopTime = kIndexTimeNotAvailable;
    mRec->mContentType = nsICacheEntry::CONTENT_TYPE_UNKNOWN;
    mRec->mBaseDomainAccessCount = 0;
    mRec->mFlags = 0;
  }

  void Init(OriginAttrsHash aOriginAttrsHash, bool aAnonymous, bool aPinned) {
    MOZ_ASSERT(mRec->mFrecency == 0);
    MOZ_ASSERT(mRec->mOriginAttrsHash == 0);
    MOZ_ASSERT(mRec->mOnStartTime == kIndexTimeNotAvailable);
    MOZ_ASSERT(mRec->mOnStopTime == kIndexTimeNotAvailable);
    MOZ_ASSERT(mRec->mContentType == nsICacheEntry::CONTENT_TYPE_UNKNOWN);
    MOZ_ASSERT(mRec->mBaseDomainAccessCount == 0);
    // When we init the entry it must be fresh and may be dirty
    MOZ_ASSERT((mRec->mFlags & ~kDirtyMask) == kFreshMask);

    mRec->mOriginAttrsHash = aOriginAttrsHash;
    mRec->mFlags |= kInitializedMask;
    if (aAnonymous) {
      mRec->mFlags |= kAnonymousMask;
    }
    if (aPinned) {
      mRec->mFlags |= kPinnedMask;
    }
  }

  const SHA1Sum::Hash* Hash() const { return &mRec->mHash; }

  bool IsInitialized() const { return !!(mRec->mFlags & kInitializedMask); }

  mozilla::net::OriginAttrsHash OriginAttrsHash() const {
    return mRec->mOriginAttrsHash;
  }

  bool Anonymous() const { return !!(mRec->mFlags & kAnonymousMask); }

  bool IsRemoved() const { return !!(mRec->mFlags & kRemovedMask); }
  void MarkRemoved() { mRec->mFlags |= kRemovedMask; }

  bool IsDirty() const { return !!(mRec->mFlags & kDirtyMask); }
  void MarkDirty() { mRec->mFlags |= kDirtyMask; }
  void ClearDirty() { mRec->mFlags &= ~kDirtyMask; }

  bool IsFresh() const { return !!(mRec->mFlags & kFreshMask); }
  void MarkFresh() { mRec->mFlags |= kFreshMask; }

  bool IsPinned() const { return !!(mRec->mFlags & kPinnedMask); }

  void SetFrecency(uint32_t aFrecency) { mRec->mFrecency = aFrecency; }
  uint32_t GetFrecency() const { return mRec->mFrecency; }

  void SetHasAltData(bool aHasAltData) {
    aHasAltData ? mRec->mFlags |= kHasAltDataMask
                : mRec->mFlags &= ~kHasAltDataMask;
  }
  bool GetHasAltData() const { return !!(mRec->mFlags & kHasAltDataMask); }

  void SetOnStartTime(uint16_t aTime) { mRec->mOnStartTime = aTime; }
  uint16_t GetOnStartTime() const { return mRec->mOnStartTime; }

  void SetOnStopTime(uint16_t aTime) { mRec->mOnStopTime = aTime; }
  uint16_t GetOnStopTime() const { return mRec->mOnStopTime; }

  void SetContentType(uint8_t aType) { mRec->mContentType = aType; }
  uint8_t GetContentType() const { return mRec->mContentType; }

  void SetBaseDomainAccessCount(uint16_t aCount) {
    mRec->mBaseDomainAccessCount = aCount;
  }
  uint8_t GetBaseDomainAccessCount() const {
    return mRec->mBaseDomainAccessCount;
  }

  // Sets filesize in kilobytes.
  void SetFileSize(uint32_t aFileSize) {
    if (aFileSize > kFileSizeMask) {
      LOG(
          ("CacheIndexEntry::SetFileSize() - FileSize is too large, "
           "truncating to %u",
           kFileSizeMask));
      aFileSize = kFileSizeMask;
    }
    mRec->mFlags &= ~kFileSizeMask;
    mRec->mFlags |= aFileSize;
  }
  // Returns filesize in kilobytes.
  uint32_t GetFileSize() const { return GetFileSize(mRec); }
  static uint32_t GetFileSize(CacheIndexRecord* aRec) {
    return aRec->mFlags & kFileSizeMask;
  }
  static uint32_t IsPinned(CacheIndexRecord* aRec) {
    return aRec->mFlags & kPinnedMask;
  }
  bool IsFileEmpty() const { return GetFileSize() == 0; }

  void WriteToBuf(void* aBuf) {
    uint8_t* ptr = static_cast<uint8_t*>(aBuf);
    memcpy(ptr, mRec->mHash, sizeof(SHA1Sum::Hash));
    ptr += sizeof(SHA1Sum::Hash);
    NetworkEndian::writeUint32(ptr, mRec->mFrecency);
    ptr += sizeof(uint32_t);
    NetworkEndian::writeUint64(ptr, mRec->mOriginAttrsHash);
    ptr += sizeof(uint64_t);
    NetworkEndian::writeUint16(ptr, mRec->mOnStartTime);
    ptr += sizeof(uint16_t);
    NetworkEndian::writeUint16(ptr, mRec->mOnStopTime);
    ptr += sizeof(uint16_t);
    *ptr = mRec->mContentType;
    ptr += sizeof(uint8_t);
    NetworkEndian::writeUint16(ptr, mRec->mBaseDomainAccessCount);
    ptr += sizeof(uint16_t);
    // Dirty and fresh flags should never go to disk, since they make sense only
    // during current session.
    NetworkEndian::writeUint32(ptr, mRec->mFlags & ~(kDirtyMask | kFreshMask));
  }

  void ReadFromBuf(void* aBuf) {
    const uint8_t* ptr = static_cast<const uint8_t*>(aBuf);
    MOZ_ASSERT(memcmp(&mRec->mHash, ptr, sizeof(SHA1Sum::Hash)) == 0);
    ptr += sizeof(SHA1Sum::Hash);
    mRec->mFrecency = NetworkEndian::readUint32(ptr);
    ptr += sizeof(uint32_t);
    mRec->mOriginAttrsHash = NetworkEndian::readUint64(ptr);
    ptr += sizeof(uint64_t);
    mRec->mOnStartTime = NetworkEndian::readUint16(ptr);
    ptr += sizeof(uint16_t);
    mRec->mOnStopTime = NetworkEndian::readUint16(ptr);
    ptr += sizeof(uint16_t);
    mRec->mContentType = *ptr;
    ptr += sizeof(uint8_t);
    mRec->mBaseDomainAccessCount = NetworkEndian::readUint16(ptr);
    ptr += sizeof(uint16_t);
    mRec->mFlags = NetworkEndian::readUint32(ptr);
  }

  void Log() const {
    LOG(
        ("CacheIndexEntry::Log() [this=%p, hash=%08x%08x%08x%08x%08x, fresh=%u,"
         " initialized=%u, removed=%u, dirty=%u, anonymous=%u, "
         "originAttrsHash=%" PRIx64 ", frecency=%u, hasAltData=%u, "
         "onStartTime=%u, onStopTime=%u, contentType=%u, "
         "baseDomainAccessCount=%u, size=%u]",
         this, LOGSHA1(mRec->mHash), IsFresh(), IsInitialized(), IsRemoved(),
         IsDirty(), Anonymous(), OriginAttrsHash(), GetFrecency(),
         GetHasAltData(), GetOnStartTime(), GetOnStopTime(), GetContentType(),
         GetBaseDomainAccessCount(), GetFileSize()));
  }

  static bool RecordMatchesLoadContextInfo(CacheIndexRecord* aRec,
                                           nsILoadContextInfo* aInfo) {
    MOZ_ASSERT(aInfo);

    if (!aInfo->IsPrivate() &&
        GetOriginAttrsHash(*aInfo->OriginAttributesPtr()) ==
            aRec->mOriginAttrsHash &&
        aInfo->IsAnonymous() == !!(aRec->mFlags & kAnonymousMask)) {
      return true;
    }

    return false;
  }

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(mRec.get());
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
  }

 private:
  friend class CacheIndexEntryUpdate;
  friend class CacheIndex;
  friend class CacheIndexEntryAutoManage;

  static const uint32_t kInitializedMask = 0x80000000;
  static const uint32_t kAnonymousMask = 0x40000000;

  // This flag is set when the entry was removed. We need to keep this
  // information in memory until we write the index file.
  static const uint32_t kRemovedMask = 0x20000000;

  // This flag is set when the information in memory is not in sync with the
  // information in index file on disk.
  static const uint32_t kDirtyMask = 0x10000000;

  // This flag is set when the information about the entry is fresh, i.e.
  // we've created or opened this entry during this session, or we've seen
  // this entry during update or build process.
  static const uint32_t kFreshMask = 0x08000000;

  // Indicates a pinned entry.
  static const uint32_t kPinnedMask = 0x04000000;

  // Indicates there is cached alternative data in the entry.
  static const uint32_t kHasAltDataMask = 0x02000000;
  static const uint32_t kReservedMask = 0x01000000;

  // FileSize in kilobytes
  static const uint32_t kFileSizeMask = 0x00FFFFFF;

  nsAutoPtr<CacheIndexRecord> mRec;
};

class CacheIndexEntryUpdate : public CacheIndexEntry {
 public:
  explicit CacheIndexEntryUpdate(CacheIndexEntry::KeyTypePointer aKey)
      : CacheIndexEntry(aKey), mUpdateFlags(0) {
    MOZ_COUNT_CTOR(CacheIndexEntryUpdate);
    LOG(("CacheIndexEntryUpdate::CacheIndexEntryUpdate()"));
  }
  ~CacheIndexEntryUpdate() {
    MOZ_COUNT_DTOR(CacheIndexEntryUpdate);
    LOG(("CacheIndexEntryUpdate::~CacheIndexEntryUpdate()"));
  }

  CacheIndexEntryUpdate& operator=(const CacheIndexEntry& aOther) {
    MOZ_ASSERT(
        memcmp(&mRec->mHash, &aOther.mRec->mHash, sizeof(SHA1Sum::Hash)) == 0);
    mUpdateFlags = 0;
    *(static_cast<CacheIndexEntry*>(this)) = aOther;
    return *this;
  }

  void InitNew() {
    mUpdateFlags = kFrecencyUpdatedMask | kHasAltDataUpdatedMask |
                   kOnStartTimeUpdatedMask | kOnStopTimeUpdatedMask |
                   kContentTypeUpdatedMask | kFileSizeUpdatedMask;
    CacheIndexEntry::InitNew();
  }

  void SetFrecency(uint32_t aFrecency) {
    mUpdateFlags |= kFrecencyUpdatedMask;
    CacheIndexEntry::SetFrecency(aFrecency);
  }

  void SetHasAltData(bool aHasAltData) {
    mUpdateFlags |= kHasAltDataUpdatedMask;
    CacheIndexEntry::SetHasAltData(aHasAltData);
  }

  void SetOnStartTime(uint16_t aTime) {
    mUpdateFlags |= kOnStartTimeUpdatedMask;
    CacheIndexEntry::SetOnStartTime(aTime);
  }

  void SetOnStopTime(uint16_t aTime) {
    mUpdateFlags |= kOnStopTimeUpdatedMask;
    CacheIndexEntry::SetOnStopTime(aTime);
  }

  void SetContentType(uint8_t aType) {
    mUpdateFlags |= kContentTypeUpdatedMask;
    CacheIndexEntry::SetContentType(aType);
  }

  void SetBaseDomainAccessCount(uint16_t aCount) {
    mUpdateFlags |= kBaseDomainAccessCountUpdatedMask;
    CacheIndexEntry::SetBaseDomainAccessCount(aCount);
  }

  void SetFileSize(uint32_t aFileSize) {
    mUpdateFlags |= kFileSizeUpdatedMask;
    CacheIndexEntry::SetFileSize(aFileSize);
  }

  void ApplyUpdate(CacheIndexEntry* aDst) {
    MOZ_ASSERT(
        memcmp(&mRec->mHash, &aDst->mRec->mHash, sizeof(SHA1Sum::Hash)) == 0);
    if (mUpdateFlags & kFrecencyUpdatedMask) {
      aDst->mRec->mFrecency = mRec->mFrecency;
    }
    aDst->mRec->mOriginAttrsHash = mRec->mOriginAttrsHash;
    if (mUpdateFlags & kOnStartTimeUpdatedMask) {
      aDst->mRec->mOnStartTime = mRec->mOnStartTime;
    }
    if (mUpdateFlags & kOnStopTimeUpdatedMask) {
      aDst->mRec->mOnStopTime = mRec->mOnStopTime;
    }
    if (mUpdateFlags & kContentTypeUpdatedMask) {
      aDst->mRec->mContentType = mRec->mContentType;
    }
    if (mUpdateFlags & kBaseDomainAccessCountUpdatedMask) {
      aDst->mRec->mBaseDomainAccessCount = mRec->mBaseDomainAccessCount;
    }
    if (mUpdateFlags & kHasAltDataUpdatedMask &&
        ((aDst->mRec->mFlags ^ mRec->mFlags) & kHasAltDataMask)) {
      // Toggle the bit if we need to.
      aDst->mRec->mFlags ^= kHasAltDataMask;
    }

    if (mUpdateFlags & kFileSizeUpdatedMask) {
      // Copy all flags except |HasAltData|.
      aDst->mRec->mFlags |= (mRec->mFlags & ~kHasAltDataMask);
    } else {
      // Copy all flags except |HasAltData| and file size.
      aDst->mRec->mFlags &= kFileSizeMask;
      aDst->mRec->mFlags |= (mRec->mFlags & ~kHasAltDataMask & ~kFileSizeMask);
    }
  }

 private:
  static const uint32_t kFrecencyUpdatedMask = 0x00000001;
  static const uint32_t kContentTypeUpdatedMask = 0x00000002;
  static const uint32_t kFileSizeUpdatedMask = 0x00000004;
  static const uint32_t kHasAltDataUpdatedMask = 0x00000008;
  static const uint32_t kOnStartTimeUpdatedMask = 0x00000010;
  static const uint32_t kOnStopTimeUpdatedMask = 0x00000020;
  static const uint32_t kBaseDomainAccessCountUpdatedMask = 0x00000040;

  uint32_t mUpdateFlags;
};

class CacheIndexStats {
 public:
  CacheIndexStats()
      : mCount(0),
        mNotInitialized(0),
        mRemoved(0),
        mDirty(0),
        mFresh(0),
        mEmpty(0),
        mSize(0)
#ifdef DEBUG
        ,
        mStateLogged(false),
        mDisableLogging(false)
#endif
  {
  }

  bool operator==(const CacheIndexStats& aOther) const {
    return
#ifdef DEBUG
        aOther.mStateLogged == mStateLogged &&
#endif
        aOther.mCount == mCount && aOther.mNotInitialized == mNotInitialized &&
        aOther.mRemoved == mRemoved && aOther.mDirty == mDirty &&
        aOther.mFresh == mFresh && aOther.mEmpty == mEmpty &&
        aOther.mSize == mSize;
  }

#ifdef DEBUG
  void DisableLogging() { mDisableLogging = true; }
#endif

  void Log() {
    LOG(
        ("CacheIndexStats::Log() [count=%u, notInitialized=%u, removed=%u, "
         "dirty=%u, fresh=%u, empty=%u, size=%u]",
         mCount, mNotInitialized, mRemoved, mDirty, mFresh, mEmpty, mSize));
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
  bool StateLogged() { return mStateLogged; }
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
    MOZ_ASSERT(!mStateLogged,
               "CacheIndexStats::ActiveEntriesCount() - state "
               "logged!");
    return mCount - mRemoved - mNotInitialized - mEmpty;
  }

  uint32_t Size() {
    MOZ_ASSERT(!mStateLogged, "CacheIndexStats::Size() - state logged!");
    return mSize;
  }

  void BeforeChange(const CacheIndexEntry* aEntry) {
#ifdef DEBUG_STATS
    if (!mDisableLogging) {
      LOG(("CacheIndexStats::BeforeChange()"));
      Log();
    }
#endif

    MOZ_ASSERT(!mStateLogged,
               "CacheIndexStats::BeforeChange() - state "
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

  void AfterChange(const CacheIndexEntry* aEntry) {
    MOZ_ASSERT(mStateLogged,
               "CacheIndexStats::AfterChange() - state not "
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
  bool mStateLogged;

  // Disables logging in this instance of CacheIndexStats
  bool mDisableLogging;
#endif
};

class CacheIndex final : public CacheFileIOListener, public nsIRunnable {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  CacheIndex();

  static nsresult Init(nsIFile* aCacheDirectory);
  static nsresult PreShutdown();
  static nsresult Shutdown();

  // Following methods can be called only on IO thread.

  // Add entry to the index. The entry shouldn't be present in index. This
  // method is called whenever a new handle for a new entry file is created. The
  // newly created entry is not initialized and it must be either initialized
  // with InitEntry() or removed with RemoveEntry().
  static nsresult AddEntry(const SHA1Sum::Hash* aHash);

  // Inform index about an existing entry that should be present in index. This
  // method is called whenever a new handle for an existing entry file is
  // created. Like in case of AddEntry(), either InitEntry() or RemoveEntry()
  // must be called on the entry, since the entry is not initizlized if the
  // index is outdated.
  static nsresult EnsureEntryExists(const SHA1Sum::Hash* aHash);

  // Initialize the entry. It MUST be present in index. Call to AddEntry() or
  // EnsureEntryExists() must precede the call to this method.
  static nsresult InitEntry(const SHA1Sum::Hash* aHash,
                            OriginAttrsHash aOriginAttrsHash, bool aAnonymous,
                            bool aPinned);

  // Remove entry from index. The entry should be present in index.
  static nsresult RemoveEntry(const SHA1Sum::Hash* aHash);

  // Update some information in entry. The entry MUST be present in index and
  // MUST be initialized. Call to AddEntry() or EnsureEntryExists() and to
  // InitEntry() must precede the call to this method.
  // Pass nullptr if the value didn't change.
  static nsresult UpdateEntry(
      const SHA1Sum::Hash* aHash, const uint32_t* aFrecency,
      const bool* aHasAltData, const uint16_t* aOnStartTime,
      const uint16_t* aOnStopTime, const uint8_t* aContentType,
      const uint16_t* aBaseDomainAccessCount, const uint32_t aTelemetryReportID,
      const uint32_t* aSize);

  // Remove all entries from the index. Called when clearing the whole cache.
  static nsresult RemoveAll();

  enum EntryStatus { EXISTS = 0, DOES_NOT_EXIST = 1, DO_NOT_KNOW = 2 };

  // Returns status of the entry in index for the given key. It can be called
  // on any thread.
  // If the optional aCB callback is given, the it will be called with a
  // CacheIndexEntry only if _retval is EXISTS when the method returns.
  static nsresult HasEntry(
      const nsACString& aKey, EntryStatus* _retval,
      const std::function<void(const CacheIndexEntry*)>& aCB = nullptr);
  static nsresult HasEntry(
      const SHA1Sum::Hash& hash, EntryStatus* _retval,
      const std::function<void(const CacheIndexEntry*)>& aCB = nullptr);

  // Returns a hash of the least important entry that should be evicted if the
  // cache size is over limit and also returns a total number of all entries in
  // the index minus the number of forced valid entries and unpinned entries
  // that we encounter when searching (see below)
  static nsresult GetEntryForEviction(bool aIgnoreEmptyEntries,
                                      SHA1Sum::Hash* aHash, uint32_t* aCnt);

  // Checks if a cache entry is currently forced valid. Used to prevent an entry
  // (that has been forced valid) from being evicted when the cache size reaches
  // its limit.
  static bool IsForcedValidEntry(const SHA1Sum::Hash* aHash);

  // Returns cache size in kB.
  static nsresult GetCacheSize(uint32_t* _retval);

  // Returns number of entry files in the cache
  static nsresult GetEntryFileCount(uint32_t* _retval);

  // Synchronously returns the disk occupation and number of entries
  // per-context. Callable on any thread. It will ignore loadContextInfo and get
  // stats for all entries if the aInfo is a nullptr.
  static nsresult GetCacheStats(nsILoadContextInfo* aInfo, uint32_t* aSize,
                                uint32_t* aCount);

  // Asynchronously gets the disk cache size, used for display in the UI.
  static nsresult AsyncGetDiskConsumption(
      nsICacheStorageConsumptionObserver* aObserver);

  // Returns an iterator that returns entries matching a given context that were
  // present in the index at the time this method was called. If aAddNew is true
  // then the iterator will also return entries created after this call.
  // NOTE: When some entry is removed from index it is removed also from the
  // iterator regardless what aAddNew was passed.
  static nsresult GetIterator(nsILoadContextInfo* aInfo, bool aAddNew,
                              CacheIndexIterator** _retval);

  // Returns true if we _think_ that the index is up to date. I.e. the state is
  // READY or WRITING and mIndexNeedsUpdate as well as mShuttingDown is false.
  static nsresult IsUpToDate(bool* _retval);

  // Called from CacheStorageService::Clear() and
  // CacheFileContextEvictor::EvictEntries(), sets a flag that blocks
  // notification to AsyncGetDiskConsumption.
  static void OnAsyncEviction(bool aEvicting);

  // We keep track of total bytes written to the cache to be able to do
  // a telemetry report after writting certain amount of data to the cache.
  static void UpdateTotalBytesWritten(uint32_t aBytesWritten);

  // Memory reporting
  static size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
  static size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

 private:
  friend class CacheIndexEntryAutoManage;
  friend class FileOpenHelper;
  friend class CacheIndexIterator;

  virtual ~CacheIndex();

  NS_IMETHOD OnFileOpened(CacheFileHandle* aHandle, nsresult aResult) override;
  nsresult OnFileOpenedInternal(FileOpenHelper* aOpener,
                                CacheFileHandle* aHandle, nsresult aResult);
  NS_IMETHOD OnDataWritten(CacheFileHandle* aHandle, const char* aBuf,
                           nsresult aResult) override;
  NS_IMETHOD OnDataRead(CacheFileHandle* aHandle, char* aBuf,
                        nsresult aResult) override;
  NS_IMETHOD OnFileDoomed(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnEOFSet(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnFileRenamed(CacheFileHandle* aHandle, nsresult aResult) override;

  nsresult InitInternal(nsIFile* aCacheDirectory);
  void PreShutdownInternal();

  // This method returns false when index is not initialized or is shut down.
  bool IsIndexUsable();

  // This method checks whether the entry has the same values of
  // originAttributes and isAnonymous. We don't expect to find a collision
  // since these values are part of the key that we hash and we use a strong
  // hash function.
  static bool IsCollision(CacheIndexEntry* aEntry,
                          OriginAttrsHash aOriginAttrsHash, bool aAnonymous);

  // Checks whether any of the information about the entry has changed.
  static bool HasEntryChanged(
      CacheIndexEntry* aEntry, const uint32_t* aFrecency,
      const bool* aHasAltData, const uint16_t* aOnStartTime,
      const uint16_t* aOnStopTime, const uint8_t* aContentType,
      const uint16_t* aBaseDomainAccessCount, const uint32_t* aSize);

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
  nsresult GetFile(const nsACString& aName, nsIFile** _retval);
  nsresult RemoveFile(const nsACString& aName);
  void RemoveAllIndexFiles();
  void RemoveJournalAndTempFile();
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
  static void DelayedUpdate(nsITimer* aTimer, void* aClosure);
  void DelayedUpdateLocked();
  // Posts timer event that start update or build process.
  nsresult ScheduleUpdateTimer(uint32_t aDelay);
  nsresult SetupDirectoryEnumerator();
  nsresult InitEntryFromDiskData(CacheIndexEntry* aEntry,
                                 CacheFileMetadata* aMetaData,
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
    INITIAL = 0,

    // Index is being read from the disk.
    // Possible transitions:
    //  -> INITIAL  - We failed to dispatch a read event.
    //  -> BUILDING - No or corrupted index file was found.
    //  -> UPDATING - No or corrupted journal file was found.
    //              - Dirty flag was set in index header.
    //  -> READY    - Index was read successfully or was interrupted by
    //                pre-shutdown.
    //  -> SHUTDOWN - This could happen only in case of pre-shutdown failure.
    READING = 1,

    // Index is being written to the disk.
    // Possible transitions:
    //  -> READY    - Writing of index finished or was interrupted by
    //                pre-shutdown..
    //  -> UPDATING - Writing of index finished, but index was found outdated
    //                during writing.
    //  -> SHUTDOWN - This could happen only in case of pre-shutdown failure.
    WRITING = 2,

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
    READY = 5,

    // Index is shutting down.
    SHUTDOWN = 6
  };

  static char const* StateString(EState aState);
  void ChangeState(EState aNewState);
  void NotifyAsyncGetDiskConsumptionCallbacks();

  // Allocates and releases buffer used for reading and writing index.
  void AllocBuffer();
  void ReleaseBuffer();

  // Methods used by CacheIndexEntryAutoManage to keep the iterators up to date.
  void AddRecordToIterators(CacheIndexRecord* aRecord);
  void RemoveRecordFromIterators(CacheIndexRecord* aRecord);
  void ReplaceRecordInIterators(CacheIndexRecord* aOldRecord,
                                CacheIndexRecord* aNewRecord);

  // Memory reporting (private part)
  size_t SizeOfExcludingThisInternal(mozilla::MallocSizeOf mallocSizeOf) const;

  void ReportHashStats();

  // Reports telemetry about cache, i.e. size, entry count, content type stats
  // and first party cache isolation stats. Clears first party cache isolation
  // counters stored in the index entries and bumps a telemetry report ID.
  void DoTelemetryReport();

  static mozilla::StaticRefPtr<CacheIndex> gInstance;
  static StaticMutex sLock;

  nsCOMPtr<nsIFile> mCacheDirectory;

  EState mState;
  // Timestamp of time when the index was initialized. We use it to delay
  // initial update or build of index.
  TimeStamp mStartTime;
  // Set to true in PreShutdown(), it is checked on variaous places to prevent
  // starting any process (write, update, etc.) during shutdown.
  bool mShuttingDown;
  // When set to true, update process should start as soon as possible. This
  // flag is set whenever we find some inconsistency which would be fixed by
  // update process. The flag is checked always when switching to READY state.
  // To make sure we start the update process as soon as possible, methods that
  // set this flag should also call StartUpdatingIndexIfNeeded() to cover the
  // case when we are currently in READY state.
  bool mIndexNeedsUpdate;
  // Set at the beginning of RemoveAll() which clears the whole index. When
  // removing all entries we must stop any pending reading, writing, updating or
  // building operation. This flag is checked at various places and it prevents
  // we won't start another operation (e.g. canceling reading of the index would
  // normally start update or build process)
  bool mRemovingAll;
  // Whether the index file on disk exists and is valid.
  bool mIndexOnDiskIsValid;
  // When something goes wrong during updating or building process, we don't
  // mark index clean (and also don't write journal) to ensure that update or
  // build will be initiated on the next start.
  bool mDontMarkIndexClean;
  // Timestamp value from index file. It is used during update process to skip
  // entries that were last modified before this timestamp.
  uint32_t mIndexTimeStamp;
  // Timestamp of last time the index was dumped to disk.
  // NOTE: The index might not be necessarily dumped at this time. The value
  // is used to schedule next dump of the index.
  TimeStamp mLastDumpTime;

  // Timer of delayed update/build.
  nsCOMPtr<nsITimer> mUpdateTimer;
  // True when build or update event is posted
  bool mUpdateEventPending;

  // Helper members used when reading/writing index from/to disk.
  // Contains number of entries that should be skipped:
  //  - in hashtable when writing index because they were already written
  //  - in index file when reading index because they were already read
  uint32_t mSkipEntries;
  // Number of entries that should be written to disk. This is number of entries
  // in hashtable that are initialized and are not marked as removed when
  // writing begins.
  uint32_t mProcessEntries;
  char* mRWBuf;
  uint32_t mRWBufSize;
  uint32_t mRWBufPos;
  RefPtr<CacheHash> mRWHash;

  // True if read or write operation is pending. It is used to ensure that
  // mRWBuf is not freed until OnDataRead or OnDataWritten is called.
  bool mRWPending;

  // Reading of journal succeeded if true.
  bool mJournalReadSuccessfully;

  // Handle used for writing and reading index file.
  RefPtr<CacheFileHandle> mIndexHandle;
  // Handle used for reading journal file.
  RefPtr<CacheFileHandle> mJournalHandle;
  // Used to check the existence of the file during reading process.
  RefPtr<CacheFileHandle> mTmpHandle;

  RefPtr<FileOpenHelper> mIndexFileOpener;
  RefPtr<FileOpenHelper> mJournalFileOpener;
  RefPtr<FileOpenHelper> mTmpFileOpener;

  // Directory enumerator used when building and updating index.
  nsCOMPtr<nsIDirectoryEnumerator> mDirEnumerator;

  // Main index hashtable.
  nsTHashtable<CacheIndexEntry> mIndex;

  // We cannot add, remove or change any entry in mIndex in states READING and
  // WRITING. We track all changes in mPendingUpdates during these states.
  nsTHashtable<CacheIndexEntryUpdate> mPendingUpdates;

  // Contains information statistics for mIndex + mPendingUpdates.
  CacheIndexStats mIndexStats;

  // When reading journal, we must first parse the whole file and apply the
  // changes iff the journal was read successfully. mTmpJournal is used to store
  // entries from the journal file. We throw away all these entries if parsing
  // of the journal fails or the hash does not match.
  nsTHashtable<CacheIndexEntry> mTmpJournal;

  // FrecencyArray maintains order of entry records for eviction. Ideally, the
  // records would be ordered by frecency all the time, but since this would be
  // quite expensive, we allow certain amount of entries to be out of order.
  // When the frecency is updated the new value is always bigger than the old
  // one. Instead of keeping updated entries at the same position, we move them
  // at the end of the array. This protects recently updated entries from
  // eviction. The array is sorted once we hit the limit of maximum unsorted
  // entries.
  class FrecencyArray {
    class Iterator {
     public:
      explicit Iterator(nsTArray<CacheIndexRecord*>* aRecs)
          : mRecs(aRecs), mIdx(0) {
        while (!Done() && !(*mRecs)[mIdx]) {
          mIdx++;
        }
      }

      bool Done() const { return mIdx == mRecs->Length(); }

      CacheIndexRecord* Get() const {
        MOZ_ASSERT(!Done());
        return (*mRecs)[mIdx];
      }

      void Next() {
        MOZ_ASSERT(!Done());
        ++mIdx;
        while (!Done() && !(*mRecs)[mIdx]) {
          mIdx++;
        }
      }

     private:
      nsTArray<CacheIndexRecord*>* mRecs;
      uint32_t mIdx;
    };

   public:
    Iterator Iter() { return Iterator(&mRecs); }

    FrecencyArray() : mUnsortedElements(0), mRemovedElements(0) {}

    // Methods used by CacheIndexEntryAutoManage to keep the array up to date.
    void AppendRecord(CacheIndexRecord* aRecord);
    void RemoveRecord(CacheIndexRecord* aRecord);
    void ReplaceRecord(CacheIndexRecord* aOldRecord,
                       CacheIndexRecord* aNewRecord);
    void SortIfNeeded();

    size_t Length() const { return mRecs.Length() - mRemovedElements; }
    void Clear() { mRecs.Clear(); }

   private:
    friend class CacheIndex;

    nsTArray<CacheIndexRecord*> mRecs;
    uint32_t mUnsortedElements;
    // Instead of removing elements from the array immediately, we null them out
    // and the iterator skips them when accessing the array. The null pointers
    // are placed at the end during sorting and we strip them out all at once.
    // This saves moving a lot of memory in nsTArray::RemoveElementsAt.
    uint32_t mRemovedElements;
  };

  FrecencyArray mFrecencyArray;

  nsTArray<CacheIndexIterator*> mIterators;

  // This flag is true iff we are between CacheStorageService:Clear() and
  // processing all contexts to be evicted.  It will make UI to show
  // "calculating" instead of any intermediate cache size.
  bool mAsyncGetDiskConsumptionBlocked;

  class DiskConsumptionObserver : public Runnable {
   public:
    static DiskConsumptionObserver* Init(
        nsICacheStorageConsumptionObserver* aObserver) {
      nsWeakPtr observer = do_GetWeakReference(aObserver);
      if (!observer) return nullptr;

      return new DiskConsumptionObserver(observer);
    }

    void OnDiskConsumption(int64_t aSize) {
      mSize = aSize;
      NS_DispatchToMainThread(this);
    }

   private:
    explicit DiskConsumptionObserver(nsWeakPtr const& aWeakObserver)
        : Runnable("net::CacheIndex::DiskConsumptionObserver"),
          mObserver(aWeakObserver),
          mSize(0) {}
    virtual ~DiskConsumptionObserver() {
      if (mObserver && !NS_IsMainThread()) {
        NS_ReleaseOnMainThreadSystemGroup("DiskConsumptionObserver::mObserver",
                                          mObserver.forget());
      }
    }

    NS_IMETHOD Run() override {
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

  // Number of bytes written to the cache since the last telemetry report
  uint64_t mTotalBytesWritten;
};

}  // namespace net
}  // namespace mozilla

#endif
