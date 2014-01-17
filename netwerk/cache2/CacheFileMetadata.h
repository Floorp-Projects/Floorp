/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileMetadata__h__
#define CacheFileMetadata__h__

#include "CacheFileIOManager.h"
#include "CacheStorageService.h"
#include "CacheHashUtils.h"
#include "CacheObserver.h"
#include "nsAutoPtr.h"
#include "nsString.h"

namespace mozilla {
namespace net {

// By multiplying with the current half-life we convert the frecency
// to time independent of half-life value.  The range fits 32bits.
// When decay time changes on next run of the browser, we convert
// the frecency value to a correct internal representation again.
// It might not be 100% accurate, but for the purpose it suffice.
#define FRECENCY2INT(aFrecency) \
  ((uint32_t)(aFrecency * CacheObserver::HalfLifeSeconds()))
#define INT2FRECENCY(aInt) \
  ((double)(aInt) / (double)CacheObserver::HalfLifeSeconds())

typedef struct {
  uint32_t        mFetchCount;
  uint32_t        mLastFetched;
  uint32_t        mLastModified;
  uint32_t        mFrecency;
  uint32_t        mExpirationTime;
  uint32_t        mKeySize;
} CacheFileMetadataHeader;

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
                    const nsACString &aKey,
                    bool aKeyIsHash);
  CacheFileMetadata(const nsACString &aKey);

  void SetHandle(CacheFileHandle *aHandle);

  nsresult GetKey(nsACString &_retval);
  bool     KeyIsHash();

  nsresult ReadMetadata(CacheFileMetadataListener *aListener);
  nsresult WriteMetadata(uint32_t aOffset,
                         CacheFileMetadataListener *aListener);

  const char * GetElement(const char *aKey);
  nsresult     SetElement(const char *aKey, const char *aValue);

  CacheHashUtils::Hash16_t GetHash(uint32_t aIndex);
  nsresult                 SetHash(uint32_t aIndex,
                                   CacheHashUtils::Hash16_t aHash);

  nsresult SetExpirationTime(uint32_t aExpirationTime);
  nsresult GetExpirationTime(uint32_t *_retval);
  nsresult SetLastModified(uint32_t aLastModified);
  nsresult GetLastModified(uint32_t *_retval);
  nsresult SetFrecency(uint32_t aFrecency);
  nsresult GetFrecency(uint32_t *_retval);
  nsresult GetLastFetched(uint32_t *_retval);
  nsresult GetFetchCount(uint32_t *_retval);

  int64_t  Offset() { return mOffset; }
  uint32_t ElementsSize() { return mElementsSize; }
  void     MarkDirty() { mIsDirty = true; }
  bool     IsDirty() { return mIsDirty; }
  uint32_t MemoryUsage() { return mHashArraySize + mBufSize; }

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                           nsresult aResult);
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, char *aBuf, nsresult aResult);
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnEOFSet(CacheFileHandle *aHandle, nsresult aResult);

private:
  virtual ~CacheFileMetadata();

  void     InitEmptyMetadata();
  nsresult ParseMetadata(uint32_t aMetaOffset, uint32_t aBufOffset);
  nsresult CheckElements(const char *aBuf, uint32_t aSize);
  void     EnsureBuffer(uint32_t aSize);

  nsRefPtr<CacheFileHandle>           mHandle;
  nsCString                           mKey;
  bool                                mKeyIsHash;
  CacheHashUtils::Hash16_t           *mHashArray;
  uint32_t                            mHashArraySize;
  uint32_t                            mHashCount;
  int64_t                             mOffset;
  char                               *mBuf; // used for parsing, then points
                                            // to elements
  uint32_t                            mBufSize;
  char                               *mWriteBuf;
  CacheFileMetadataHeader             mMetaHdr;
  uint32_t                            mElementsSize;
  bool                                mIsDirty;
  nsCOMPtr<CacheFileMetadataListener> mListener;
};


} // net
} // mozilla

#endif
