/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileMetadata__h__
#define CacheFileMetadata__h__

#include "CacheFileIOManager.h"
#include "CacheStorageService.h"
#include "CacheHashUtils.h"
#include "CacheObserver.h"
#include "mozilla/Endian.h"
#include "mozilla/BasePrincipal.h"
#include "nsAutoPtr.h"
#include "nsString.h"

class nsICacheEntryMetaDataVisitor;

namespace mozilla {
namespace net {

// By multiplying with the current half-life we convert the frecency
// to time independent of half-life value.  The range fits 32bits.
// When decay time changes on next run of the browser, we convert
// the frecency value to a correct internal representation again.
// It might not be 100% accurate, but for the purpose it suffice.
#define FRECENCY2INT(aFrecency) \
  ((uint32_t)((aFrecency) * CacheObserver::HalfLifeSeconds()))
#define INT2FRECENCY(aInt) \
  ((double)(aInt) / (double)CacheObserver::HalfLifeSeconds())


#pragma pack(push)
#pragma pack(1)

class CacheFileMetadataHeader {
public:
  uint32_t        mVersion;
  uint32_t        mFetchCount;
  uint32_t        mLastFetched;
  uint32_t        mLastModified;
  uint32_t        mFrecency;
  uint32_t        mExpirationTime;
  uint32_t        mKeySize;

  void WriteToBuf(void *aBuf)
  {
    EnsureCorrectClassSize();

    uint8_t* ptr = static_cast<uint8_t*>(aBuf);
    NetworkEndian::writeUint32(ptr, mVersion); ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mFetchCount); ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mLastFetched); ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mLastModified); ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mFrecency); ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mExpirationTime); ptr += sizeof(uint32_t);
    NetworkEndian::writeUint32(ptr, mKeySize);
  }

  void ReadFromBuf(const void *aBuf)
  {
    EnsureCorrectClassSize();

    const uint8_t* ptr = static_cast<const uint8_t*>(aBuf);
    mVersion = BigEndian::readUint32(ptr); ptr += sizeof(uint32_t);
    mFetchCount = BigEndian::readUint32(ptr); ptr += sizeof(uint32_t);
    mLastFetched = BigEndian::readUint32(ptr); ptr += sizeof(uint32_t);
    mLastModified = BigEndian::readUint32(ptr); ptr += sizeof(uint32_t);
    mFrecency = BigEndian::readUint32(ptr); ptr += sizeof(uint32_t);
    mExpirationTime = BigEndian::readUint32(ptr); ptr += sizeof(uint32_t);
    mKeySize = BigEndian::readUint32(ptr);
  }

  inline void EnsureCorrectClassSize()
  {
    static_assert((sizeof(mVersion) + sizeof(mFetchCount) +
      sizeof(mLastFetched) + sizeof(mLastModified) + sizeof(mFrecency) +
      sizeof(mExpirationTime) + sizeof(mKeySize)) ==
      sizeof(CacheFileMetadataHeader),
      "Unexpected sizeof(CacheFileMetadataHeader)!");
  }
};

#pragma pack(pop)


#define CACHEFILEMETADATALISTENER_IID \
{ /* a9e36125-3f01-4020-9540-9dafa8d31ba7 */       \
  0xa9e36125,                                      \
  0x3f01,                                          \
  0x4020,                                          \
  {0x95, 0x40, 0x9d, 0xaf, 0xa8, 0xd3, 0x1b, 0xa7} \
}

class CacheFileMetadataListener : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CACHEFILEMETADATALISTENER_IID)

  NS_IMETHOD OnMetadataRead(nsresult aResult) = 0;
  NS_IMETHOD OnMetadataWritten(nsresult aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CacheFileMetadataListener,
                              CACHEFILEMETADATALISTENER_IID)


class CacheFileMetadata : public CacheFileIOListener
                        , public CacheMemoryConsumer
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CacheFileMetadata(CacheFileHandle *aHandle,
                    const nsACString &aKey);
  CacheFileMetadata(bool aMemoryOnly,
                    const nsACString &aKey);
  CacheFileMetadata();

  void SetHandle(CacheFileHandle *aHandle);

  nsresult GetKey(nsACString &_retval);

  nsresult ReadMetadata(CacheFileMetadataListener *aListener);
  uint32_t CalcMetadataSize(uint32_t aElementsSize, uint32_t aHashCount);
  nsresult WriteMetadata(uint32_t aOffset,
                         CacheFileMetadataListener *aListener);
  nsresult SyncReadMetadata(nsIFile *aFile);

  bool     IsAnonymous() { return mAnonymous; }
  mozilla::OriginAttributes const & OriginAttributes() const { return mOriginAttributes; }

  const char * GetElement(const char *aKey);
  nsresult     SetElement(const char *aKey, const char *aValue);
  nsresult     Visit(nsICacheEntryMetaDataVisitor *aVisitor);

  CacheHash::Hash16_t GetHash(uint32_t aIndex);
  nsresult            SetHash(uint32_t aIndex, CacheHash::Hash16_t aHash);

  nsresult SetExpirationTime(uint32_t aExpirationTime);
  nsresult GetExpirationTime(uint32_t *_retval);
  nsresult SetFrecency(uint32_t aFrecency);
  nsresult GetFrecency(uint32_t *_retval);
  nsresult GetLastModified(uint32_t *_retval);
  nsresult GetLastFetched(uint32_t *_retval);
  nsresult GetFetchCount(uint32_t *_retval);
  // Called by upper layers to indicate the entry this metadata belongs
  // with has been fetched, i.e. delivered to the consumer.
  nsresult OnFetched();

  int64_t  Offset() { return mOffset; }
  uint32_t ElementsSize() { return mElementsSize; }
  void     MarkDirty(bool aUpdateLastModified = true);
  bool     IsDirty() { return mIsDirty; }
  uint32_t MemoryUsage() { return sizeof(CacheFileMetadata) + mHashArraySize + mBufSize; }

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult) override;
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                           nsresult aResult) override;
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, char *aBuf, nsresult aResult) override;
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult) override;
  NS_IMETHOD OnEOFSet(CacheFileHandle *aHandle, nsresult aResult) override;
  NS_IMETHOD OnFileRenamed(CacheFileHandle *aHandle, nsresult aResult) override;

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
  virtual ~CacheFileMetadata();

  void     InitEmptyMetadata();
  nsresult ParseMetadata(uint32_t aMetaOffset, uint32_t aBufOffset, bool aHaveKey);
  nsresult CheckElements(const char *aBuf, uint32_t aSize);
  nsresult EnsureBuffer(uint32_t aSize);
  nsresult ParseKey(const nsACString &aKey);

  nsRefPtr<CacheFileHandle>           mHandle;
  nsCString                           mKey;
  CacheHash::Hash16_t                *mHashArray;
  uint32_t                            mHashArraySize;
  uint32_t                            mHashCount;
  int64_t                             mOffset;
  char                               *mBuf; // used for parsing, then points
                                            // to elements
  uint32_t                            mBufSize;
  char                               *mWriteBuf;
  CacheFileMetadataHeader             mMetaHdr;
  uint32_t                            mElementsSize;
  bool                                mIsDirty        : 1;
  bool                                mAnonymous      : 1;
  bool                                mAllocExactSize : 1;
  bool                                mFirstRead      : 1;
  mozilla::OriginAttributes           mOriginAttributes;
  mozilla::TimeStamp                  mReadStart;
  nsCOMPtr<CacheFileMetadataListener> mListener;
};


} // namespace net
} // namespace mozilla

#endif
