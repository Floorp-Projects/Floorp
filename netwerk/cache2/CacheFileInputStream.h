/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileInputStream__h__
#define CacheFileInputStream__h__

#include "nsIAsyncInputStream.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "CacheFileChunk.h"

namespace mozilla {
namespace net {

class CacheFile;

class CacheFileInputStream : public nsIAsyncInputStream
                           , public nsISeekableStream
                           , public CacheFileChunkListener
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM

public:
  explicit CacheFileInputStream(CacheFile *aFile, nsISupports *aEntry,
                                bool aAlternativeData);

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk *aChunk) override;
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk) override;
  NS_IMETHOD OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                              CacheFileChunk *aChunk) override;
  NS_IMETHOD OnChunkUpdated(CacheFileChunk *aChunk) override;

  // Memory reporting
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  uint32_t GetPosition() const { return mPos; };
  bool IsAlternativeData() const { return mAlternativeData; };

private:
  virtual ~CacheFileInputStream();

  nsresult CloseWithStatusLocked(nsresult aStatus);
  void CleanUp();
  void ReleaseChunk();
  void EnsureCorrectChunk(bool aReleaseOnly);

  // CanRead returns negative value when output stream truncates the data before
  // the input stream's mPos.
  int64_t CanRead(CacheFileChunkReadHandle *aHandle);
  void NotifyListener();
  void MaybeNotifyListener();

  RefPtr<CacheFile>      mFile;
  RefPtr<CacheFileChunk> mChunk;
  int64_t                mPos;
  nsresult               mStatus;
  bool                   mClosed : 1;
  bool                   mInReadSegments : 1;
  bool                   mWaitingForUpdate : 1;
  bool const             mAlternativeData : 1;
  int64_t                mListeningForChunk;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  uint32_t                         mCallbackFlags;
  nsCOMPtr<nsIEventTarget>         mCallbackTarget;
  // Held purely for referencing purposes
  RefPtr<nsISupports>              mCacheEntryHandle;
};


} // namespace net
} // namespace mozilla

#endif
