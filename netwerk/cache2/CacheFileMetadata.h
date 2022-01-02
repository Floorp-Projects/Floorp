/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileMetadata__h__
#define CacheFileMetadata__h__

#include "CacheFileIOManager.h"
#include "CacheStorageService.h"
#include "CacheHashUtils.h"
#include "CacheObserver.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/NotNull.h"
#include "nsString.h"

class nsICacheEntryMetaDataVisitor;

namespace mozilla {
namespace net {

namespace CacheFileUtils {
class CacheFileLock;
};

// Flags stored in CacheFileMetadataHeader.mFlags

// Whether an entry is a pinned entry (created with
// nsICacheStorageService.pinningCacheStorage.)
static const uint32_t kCacheEntryIsPinned = 1 << 0;

// By multiplying with the current half-life we convert the frecency
// to time independent of half-life value.  The range fits 32bits.
// When decay time changes on next run of the browser, we convert
// the frecency value to a correct internal representation again.
// It might not be 100% accurate, but for the purpose it suffice.
#define FRECENCY2INT(aFrecency) \
  ((uint32_t)((aFrecency)*CacheObserver::HalfLifeSeconds()))
#define INT2FRECENCY(aInt) \
  ((double)(aInt) / (double)CacheObserver::HalfLifeSeconds())

#define kCacheEntryVersion 3

#pragma pack(push)
#pragma pack(1)

class CacheFileMetadataHeader {
 public:
  uint32_t mVersion;
  uint32_t mFetchCount;
  uint32_t mLastFetched;
  uint32_t mLastModified;
  uint32_t mFrecency;
  uint32_t mExpirationTime;
  uint32_t mKeySize;
  uint32_t mFlags;

  void WriteToBuf(void* aBuf) {
    EnsureCorrectClassSize();

    uint8_t* ptr = static_cast<uint8_t*>(aBuf);
    MOZ_ASSERT(mVersion == kCacheEntryVersion);
    NetworkEndian::writeUint32(ptr, mVersion);
    ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mFetchCount);
    ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mLastFetched);
    ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mLastModified);
    ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mFrecency);
    ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mExpirationTime);
    ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mKeySize);
    ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mFlags);
  }

  void ReadFromBuf(const void* aBuf) {
    EnsureCorrectClassSize();

    const uint8_t* ptr = static_cast<const uint8_t*>(aBuf);
    mVersion = BigEndian::readUint32(ptr);
    ptr += sizeof(uint32_t);
    mFetchCount = BigEndian::readUint32(ptr);
    ptr += sizeof(uint32_t);
    mLastFetched = BigEndian::readUint32(ptr);
    ptr += sizeof(uint32_t);
    mLastModified = BigEndian::readUint32(ptr);
    ptr += sizeof(uint32_t);
    mFrecency = BigEndian::readUint32(ptr);
    ptr += sizeof(uint32_t);
    mExpirationTime = BigEndian::readUint32(ptr);
    ptr += sizeof(uint32_t);
    mKeySize = BigEndian::readUint32(ptr);
    ptr += sizeof(uint32_t);
    if (mVersion >= 2) {
      mFlags = BigEndian::readUint32(ptr);
    } else {
      mFlags = 0;
    }
  }

  inline void EnsureCorrectClassSize() {
    static_assert(
        (sizeof(mVersion) + sizeof(mFetchCount) + sizeof(mLastFetched) +
         sizeof(mLastModified) + sizeof(mFrecency) + sizeof(mExpirationTime) +
         sizeof(mKeySize)) +
                sizeof(mFlags) ==
            sizeof(CacheFileMetadataHeader),
        "Unexpected sizeof(CacheFileMetadataHeader)!");
  }
};

#pragma pack(pop)

#define CACHEFILEMETADATALISTENER_IID                \
  { /* a9e36125-3f01-4020-9540-9dafa8d31ba7 */       \
    0xa9e36125, 0x3f01, 0x4020, {                    \
      0x95, 0x40, 0x9d, 0xaf, 0xa8, 0xd3, 0x1b, 0xa7 \
    }                                                \
  }

class CacheFileMetadataListener : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CACHEFILEMETADATALISTENER_IID)

  NS_IMETHOD OnMetadataRead(nsresult aResult) = 0;
  NS_IMETHOD OnMetadataWritten(nsresult aResult) = 0;
  virtual bool IsKilled() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CacheFileMetadataListener,
                              CACHEFILEMETADATALISTENER_IID)

class CacheFileMetadata final : public CacheFileIOListener,
                                public CacheMemoryConsumer {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CacheFileMetadata(CacheFileHandle* aHandle, const nsACString& aKey,
                    NotNull<CacheFileUtils::CacheFileLock*> aLock);
  CacheFileMetadata(bool aMemoryOnly, bool aPinned, const nsACString& aKey,
                    NotNull<CacheFileUtils::CacheFileLock*> aLock);
  CacheFileMetadata();

  void SetHandle(CacheFileHandle* aHandle);

  const nsACString& GetKey() const { return mKey; }

  void ReadMetadata(CacheFileMetadataListener* aListener);
  uint32_t CalcMetadataSize(uint32_t aElementsSize, uint32_t aHashCount);
  nsresult WriteMetadata(uint32_t aOffset,
                         CacheFileMetadataListener* aListener);
  nsresult SyncReadMetadata(nsIFile* aFile);

  bool IsAnonymous() const { return mAnonymous; }
  mozilla::OriginAttributes const& OriginAttributes() const {
    return mOriginAttributes;
  }
  bool Pinned() const { return !!(mMetaHdr.mFlags & kCacheEntryIsPinned); }

  const char* GetElement(const char* aKey);
  nsresult SetElement(const char* aKey, const char* aValue);
  void Visit(nsICacheEntryMetaDataVisitor* aVisitor);

  CacheHash::Hash16_t GetHash(uint32_t aIndex);
  nsresult SetHash(uint32_t aIndex, CacheHash::Hash16_t aHash);
  nsresult RemoveHash(uint32_t aIndex);

  void AddFlags(uint32_t aFlags);
  void RemoveFlags(uint32_t aFlags);
  uint32_t GetFlags() const { return mMetaHdr.mFlags; }
  void SetExpirationTime(uint32_t aExpirationTime);
  uint32_t GetExpirationTime() const { return mMetaHdr.mExpirationTime; }
  void SetFrecency(uint32_t aFrecency);
  uint32_t GetFrecency() const { return mMetaHdr.mFrecency; }
  uint32_t GetLastModified() const { return mMetaHdr.mLastModified; }
  uint32_t GetLastFetched() const { return mMetaHdr.mLastFetched; }
  uint32_t GetFetchCount() const { return mMetaHdr.mFetchCount; }
  // Called by upper layers to indicate the entry this metadata belongs
  // with has been fetched, i.e. delivered to the consumer.
  void OnFetched();

  int64_t Offset() { return mOffset; }
  uint32_t ElementsSize() { return mElementsSize; }
  void MarkDirty(bool aUpdateLastModified = true);
  bool IsDirty() { return mIsDirty; }
  uint32_t MemoryUsage() {
    return sizeof(CacheFileMetadata) + mHashArraySize + mBufSize;
  }

  NS_IMETHOD OnFileOpened(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnDataWritten(CacheFileHandle* aHandle, const char* aBuf,
                           nsresult aResult) override;
  NS_IMETHOD OnDataRead(CacheFileHandle* aHandle, char* aBuf,
                        nsresult aResult) override;
  NS_IMETHOD OnFileDoomed(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnEOFSet(CacheFileHandle* aHandle, nsresult aResult) override;
  NS_IMETHOD OnFileRenamed(CacheFileHandle* aHandle, nsresult aResult) override;
  virtual bool IsKilled() override {
    return mListener && mListener->IsKilled();
  }
  void InitEmptyMetadata();

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  virtual ~CacheFileMetadata();

  nsresult ParseMetadata(uint32_t aMetaOffset, uint32_t aBufOffset,
                         bool aHaveKey);
  nsresult CheckElements(const char* aBuf, uint32_t aSize);
  nsresult EnsureBuffer(uint32_t aSize);
  nsresult ParseKey(const nsACString& aKey);

  RefPtr<CacheFileHandle> mHandle;
  nsCString mKey;
  CacheHash::Hash16_t* mHashArray{nullptr};
  uint32_t mHashArraySize{0};
  uint32_t mHashCount{0};
  int64_t mOffset{0};
  // used for parsing, then points to elements
  char* mBuf{nullptr};
  uint32_t mBufSize{0};
  char* mWriteBuf{nullptr};
  CacheFileMetadataHeader mMetaHdr{0};
  uint32_t mElementsSize{0};
  bool mIsDirty : 1;
  bool mAnonymous : 1;
  bool mAllocExactSize : 1;
  bool mFirstRead : 1;
  mozilla::OriginAttributes mOriginAttributes;
  mozilla::TimeStamp mReadStart;
  nsCOMPtr<CacheFileMetadataListener> mListener;
  RefPtr<CacheFileUtils::CacheFileLock> mLock;
};

}  // namespace net
}  // namespace mozilla

#endif
