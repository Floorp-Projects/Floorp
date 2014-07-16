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
  CacheFileInputStream(CacheFile *aFile);

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                              CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkUpdated(CacheFileChunk *aChunk);

  // Memory reporting
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  uint32_t GetPosition() const { return mPos; };

private:
  virtual ~CacheFileInputStream();

  nsresult CloseWithStatusLocked(nsresult aStatus);
  void ReleaseChunk();
  void EnsureCorrectChunk(bool aReleaseOnly);
  void CanRead(int64_t *aCanRead, const char **aBuf);
  void NotifyListener();
  void MaybeNotifyListener();

  nsRefPtr<CacheFile>      mFile;
  nsRefPtr<CacheFileChunk> mChunk;
  int64_t                  mPos;
  bool                     mClosed;
  nsresult                 mStatus;
  bool                     mWaitingForUpdate;
  int64_t                  mListeningForChunk;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  uint32_t                         mCallbackFlags;
  nsCOMPtr<nsIEventTarget>         mCallbackTarget;
};


} // net
} // mozilla

#endif
