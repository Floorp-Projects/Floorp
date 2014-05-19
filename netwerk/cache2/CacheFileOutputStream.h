/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileOutputStream__h__
#define CacheFileOutputStream__h__

#include "nsIAsyncOutputStream.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "CacheFileChunk.h"


namespace mozilla {
namespace net {

class CacheFile;
class CacheOutputCloseListener;

class CacheFileOutputStream : public nsIAsyncOutputStream
                            , public nsISeekableStream
                            , public CacheFileChunkListener
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM

public:
  CacheFileOutputStream(CacheFile *aFile, CacheOutputCloseListener *aCloseListener);

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                              CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkUpdated(CacheFileChunk *aChunk);

  void NotifyCloseListener();

  // Memory reporting
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
  virtual ~CacheFileOutputStream();

  nsresult CloseWithStatusLocked(nsresult aStatus);
  void ReleaseChunk();
  void EnsureCorrectChunk(bool aReleaseOnly);
  void FillHole();
  void NotifyListener();

  nsRefPtr<CacheFile>      mFile;
  nsRefPtr<CacheFileChunk> mChunk;
  nsRefPtr<CacheOutputCloseListener> mCloseListener;
  int64_t                  mPos;
  bool                     mClosed;
  nsresult                 mStatus;

  nsCOMPtr<nsIOutputStreamCallback> mCallback;
  uint32_t                          mCallbackFlags;
  nsCOMPtr<nsIEventTarget>          mCallbackTarget;
};


} // net
} // mozilla

#endif
