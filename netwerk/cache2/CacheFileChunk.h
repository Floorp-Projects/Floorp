/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileChunk__h__
#define CacheFileChunk__h__

#include "CacheFileIOManager.h"
#include "CacheStorageService.h"
#include "CacheHashUtils.h"
#include "nsAutoPtr.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace net {

#define kChunkSize        16384
#define kEmptyChunkHash   0xA8CA

class CacheFileChunk;
class CacheFile;
class ValidityPair;


#define CACHEFILECHUNKLISTENER_IID \
{ /* baf16149-2ab5-499c-a9c2-5904eb95c288 */       \
  0xbaf16149,                                      \
  0x2ab5,                                          \
  0x499c,                                          \
  {0xa9, 0xc2, 0x59, 0x04, 0xeb, 0x95, 0xc2, 0x88} \
}

class CacheFileChunkListener : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CACHEFILECHUNKLISTENER_IID)

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk *aChunk) = 0;
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk) = 0;
  NS_IMETHOD OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                              CacheFileChunk *aChunk) = 0;
  NS_IMETHOD OnChunkUpdated(CacheFileChunk *aChunk) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CacheFileChunkListener,
                              CACHEFILECHUNKLISTENER_IID)


class ChunkListenerItem {
public:
  ChunkListenerItem()  { MOZ_COUNT_CTOR(ChunkListenerItem); }
  ~ChunkListenerItem() { MOZ_COUNT_DTOR(ChunkListenerItem); }

  nsCOMPtr<nsIEventTarget>         mTarget;
  nsCOMPtr<CacheFileChunkListener> mCallback;
};

class ChunkListeners {
public:
  ChunkListeners()  { MOZ_COUNT_CTOR(ChunkListeners); }
  ~ChunkListeners() { MOZ_COUNT_DTOR(ChunkListeners); }

  nsTArray<ChunkListenerItem *> mItems;
};

class CacheFileChunk : public CacheFileIOListener
                     , public CacheMemoryConsumer
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CacheFileChunk(CacheFile *aFile, uint32_t aIndex);

  void     InitNew(CacheFileChunkListener *aCallback);
  nsresult Read(CacheFileHandle *aHandle, uint32_t aLen,
                CacheHashUtils::Hash16_t aHash,
                CacheFileChunkListener *aCallback);
  nsresult Write(CacheFileHandle *aHandle, CacheFileChunkListener *aCallback);
  void     WaitForUpdate(CacheFileChunkListener *aCallback);
  nsresult CancelWait(CacheFileChunkListener *aCallback);
  nsresult NotifyUpdateListeners();

  uint32_t                 Index();
  CacheHashUtils::Hash16_t Hash();
  uint32_t                 DataSize();
  void                     UpdateDataSize(uint32_t aOffset, uint32_t aLen,
                                          bool aEOF);

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                           nsresult aResult);
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, char *aBuf, nsresult aResult);
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnEOFSet(CacheFileHandle *aHandle, nsresult aResult);

  bool   IsReady();
  bool   IsDirty();

  char *       BufForWriting();
  const char * BufForReading();
  void         EnsureBufSize(uint32_t aBufSize);
  uint32_t     MemorySize() { return mRWBufSize + mBufSize; }

private:
  friend class CacheFileInputStream;
  friend class CacheFileOutputStream;
  friend class CacheFile;

  virtual ~CacheFileChunk();

  enum EState {
    INITIAL = 0,
    READING = 1,
    WRITING = 2,
    READY   = 3,
    ERROR   = 4
  };

  uint32_t mIndex;
  EState   mState;
  bool     mIsDirty;
  bool     mRemovingChunk;
  uint32_t mDataSize;

  char    *mBuf;
  uint32_t mBufSize;

  char                    *mRWBuf;
  uint32_t                 mRWBufSize;
  CacheHashUtils::Hash16_t mReadHash;

  nsRefPtr<CacheFile>              mFile; // is null if chunk is cached to
                                          // prevent reference cycles
  nsCOMPtr<CacheFileChunkListener> mListener;
  nsTArray<ChunkListenerItem *>    mUpdateListeners;
  nsTArray<ValidityPair>           mValidityMap;
};


} // net
} // mozilla

#endif
