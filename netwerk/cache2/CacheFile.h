/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFile__h__
#define CacheFile__h__

#include "CacheFileChunk.h"
#include "nsWeakReference.h"
#include "CacheFileIOManager.h"
#include "CacheFileMetadata.h"
#include "nsRefPtrHashtable.h"
#include "nsClassHashtable.h"
#include "mozilla/Mutex.h"

class nsIInputStream;
class nsIOutputStream;

namespace mozilla {
namespace net {

class CacheFileInputStream;
class CacheFileOutputStream;
class CacheOutputCloseListener;
class MetadataWriteTimer;

#define CACHEFILELISTENER_IID \
{ /* 95e7f284-84ba-48f9-b1fc-3a7336b4c33c */       \
  0x95e7f284,                                      \
  0x84ba,                                          \
  0x48f9,                                          \
  {0xb1, 0xfc, 0x3a, 0x73, 0x36, 0xb4, 0xc3, 0x3c} \
}

class CacheFileListener : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CACHEFILELISTENER_IID)

  NS_IMETHOD OnFileReady(nsresult aResult, bool aIsNew) = 0;
  NS_IMETHOD OnFileDoomed(nsresult aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CacheFileListener, CACHEFILELISTENER_IID)


class CacheFile : public CacheFileChunkListener
                , public CacheFileIOListener
                , public CacheFileMetadataListener
                , public nsSupportsWeakReference
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CacheFile();

  nsresult Init(const nsACString &aKey,
                bool aCreateNew,
                bool aMemoryOnly,
                bool aPriority,
                bool aKeyIsHash,
                CacheFileListener *aCallback);

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                              CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkUpdated(CacheFileChunk *aChunk);

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                           nsresult aResult);
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, char *aBuf, nsresult aResult);
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnEOFSet(CacheFileHandle *aHandle, nsresult aResult);

  NS_IMETHOD OnMetadataRead(nsresult aResult);
  NS_IMETHOD OnMetadataWritten(nsresult aResult);

  NS_IMETHOD OpenInputStream(nsIInputStream **_retval);
  NS_IMETHOD OpenOutputStream(CacheOutputCloseListener *aCloseListener, nsIOutputStream **_retval);
  NS_IMETHOD SetMemoryOnly();
  NS_IMETHOD Doom(CacheFileListener *aCallback);

  nsresult   ThrowMemoryCachedData();

  // metadata forwarders
  nsresult GetElement(const char *aKey, const char **_retval);
  nsresult SetElement(const char *aKey, const char *aValue);
  nsresult ElementsSize(uint32_t *_retval);
  nsresult SetExpirationTime(uint32_t aExpirationTime);
  nsresult GetExpirationTime(uint32_t *_retval);
  nsresult SetLastModified(uint32_t aLastModified);
  nsresult GetLastModified(uint32_t *_retval);
  nsresult GetLastFetched(uint32_t *_retval);
  nsresult GetFetchCount(uint32_t *_retval);

  bool DataSize(int64_t* aSize);
  void Key(nsACString& aKey) { aKey = mKey; }

private:
  friend class CacheFileChunk;
  friend class CacheFileInputStream;
  friend class CacheFileOutputStream;
  friend class CacheFileAutoLock;
  friend class MetadataWriteTimer;
  friend class MetadataListenerHelper;

  virtual ~CacheFile();

  void     Lock();
  void     Unlock();
  void     AssertOwnsLock();
  void     ReleaseOutsideLock(nsISupports *aObject);

  nsresult GetChunk(uint32_t aIndex, bool aWriter,
                    CacheFileChunkListener *aCallback,
                    CacheFileChunk **_retval);
  nsresult GetChunkLocked(uint32_t aIndex, bool aWriter,
                          CacheFileChunkListener *aCallback,
                          CacheFileChunk **_retval);
  nsresult RemoveChunk(CacheFileChunk *aChunk);

  nsresult RemoveInput(CacheFileInputStream *aInput);
  nsresult RemoveOutput(CacheFileOutputStream *aOutput);
  nsresult NotifyChunkListener(CacheFileChunkListener *aCallback,
                               nsIEventTarget *aTarget,
                               nsresult aResult,
                               uint32_t aChunkIdx,
                               CacheFileChunk *aChunk);
  nsresult QueueChunkListener(uint32_t aIndex,
                              CacheFileChunkListener *aCallback);
  nsresult NotifyChunkListeners(uint32_t aIndex, nsresult aResult,
                                CacheFileChunk *aChunk);
  bool     HaveChunkListeners(uint32_t aIndex);
  void     NotifyListenersAboutOutputRemoval();

  bool IsDirty();
  void WriteMetadataIfNeeded();
  void PostWriteTimer();

  static PLDHashOperator WriteAllCachedChunks(const uint32_t& aIdx,
                                              nsRefPtr<CacheFileChunk>& aChunk,
                                              void* aClosure);

  static PLDHashOperator FailListenersIfNonExistentChunk(
                           const uint32_t& aIdx,
                           nsAutoPtr<mozilla::net::ChunkListeners>& aListeners,
                           void* aClosure);

  static PLDHashOperator FailUpdateListeners(const uint32_t& aIdx,
                                             nsRefPtr<CacheFileChunk>& aChunk,
                                             void* aClosure);

  nsresult PadChunkWithZeroes(uint32_t aChunkIdx);

  mozilla::Mutex mLock;
  bool           mOpeningFile;
  bool           mReady;
  bool           mMemoryOnly;
  bool           mDataAccessed;
  bool           mDataIsDirty;
  bool           mWritingMetadata;
  bool           mKeyIsHash;
  nsresult       mStatus;
  int64_t        mDataSize;
  nsCString      mKey;

  nsRefPtr<CacheFileHandle>    mHandle;
  nsRefPtr<CacheFileMetadata>  mMetadata;
  nsCOMPtr<CacheFileListener>  mListener;
  nsRefPtr<MetadataWriteTimer> mTimer;
  nsCOMPtr<CacheFileIOListener>   mDoomAfterOpenListener;

  nsRefPtrHashtable<nsUint32HashKey, CacheFileChunk> mChunks;
  nsClassHashtable<nsUint32HashKey, ChunkListeners> mChunkListeners;
  nsRefPtrHashtable<nsUint32HashKey, CacheFileChunk> mCachedChunks;

  nsTArray<CacheFileInputStream*> mInputs;
  CacheFileOutputStream          *mOutput;

  nsTArray<nsISupports*>          mObjsToRelease;
};

class CacheFileAutoLock {
public:
  CacheFileAutoLock(CacheFile *aFile)
    : mFile(aFile)
    , mLocked(true)
  {
    mFile->Lock();
  }
  ~CacheFileAutoLock()
  {
    if (mLocked)
      mFile->Unlock();
  }
  void Lock()
  {
    MOZ_ASSERT(!mLocked);
    mFile->Lock();
    mLocked = true;
  }
  void Unlock()
  {
    MOZ_ASSERT(mLocked);
    mFile->Unlock();
    mLocked = false;
  }

private:
  nsRefPtr<CacheFile> mFile;
  bool mLocked;
};

} // net
} // mozilla

#endif
