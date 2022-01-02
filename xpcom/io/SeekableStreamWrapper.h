/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SeekableStreamWrapper_h
#define mozilla_SeekableStreamWrapper_h

#include "mozilla/Assertions.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsISeekableStream.h"
#include "nsICloneableInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIInputStreamLength.h"
#include "nsIBufferedStreams.h"

namespace mozilla {

// Adapter class which converts an input stream implementing
// `nsICloneableInputStream`, such as `nsPipe`, into a stream implementing
// `nsISeekableStream`. This is done by keeping a clone of the original unwound
// input stream to allow rewinding the stream back to its original position.
class SeekableStreamWrapper final : public nsIAsyncInputStream,
                                    public nsISeekableStream,
                                    public nsICloneableInputStream,
                                    public nsIInputStreamLength,
                                    public nsIAsyncInputStreamLength,
                                    public nsIIPCSerializableInputStream,
                                    public nsIBufferedInputStream {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSITELLABLESTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMLENGTH
  NS_DECL_NSIASYNCINPUTSTREAMLENGTH
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSIBUFFEREDINPUTSTREAM

 public:
  /**
   * This function returns a nsIInputStream which is known to implement
   * nsISeekableStream. This will return |aSource| if it already implements
   * nsISeekableStream, and will wrap it if it does not.
   *
   * If the original stream implements `nsICloneableInputStream`, this will be
   * used to implement the rewinding functionality necessary for
   * `nsISeekableStream`, otherwise it will be copied into a `nsPipe`.
   */
  static nsresult MaybeWrap(already_AddRefed<nsIInputStream> aOriginal,
                            nsIInputStream** aResult);

 private:
  SeekableStreamWrapper(nsICloneableInputStream* aOriginal,
                        nsIInputStream* aCurrent)
      : mOriginal(aOriginal), mCurrent(aCurrent) {
    MOZ_ASSERT(mOriginal->GetCloneable());
  }

  ~SeekableStreamWrapper() = default;

  template <typename T>
  nsCOMPtr<T> StreamAs();

  template <typename M>
  void SerializeInternal(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed, M* aManager);

  bool IsAsyncInputStream();
  bool IsInputStreamLength();
  bool IsAsyncInputStreamLength();
  bool IsIPCSerializableInputStream();
  bool IsBufferedInputStream();

  // mOriginal is immutable after creation, and is not guarded by `mMutex`.
  nsCOMPtr<nsICloneableInputStream> mOriginal;

  // This mutex guards `mCurrent`.
  Mutex mMutex{"SeekableStreamWrapper"};
  nsCOMPtr<nsIInputStream> mCurrent;
};

}  // namespace mozilla

#endif  // mozilla_SeekableStreamWrapper_h
