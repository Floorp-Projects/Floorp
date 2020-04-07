/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NonBlockingAsyncInputStream_h
#define NonBlockingAsyncInputStream_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsISeekableStream.h"

// This class aims to wrap a non-blocking and non-async inputStream and expose
// it as nsIAsyncInputStream.
// Probably you don't want to use this class directly. Instead use
// NS_MakeAsyncNonBlockingInputStream() as it will handle different stream
// variants without requiring you to special-case them yourself.

namespace mozilla {

class NonBlockingAsyncInputStream final : public nsIAsyncInputStream,
                                          public nsICloneableInputStream,
                                          public nsIIPCSerializableInputStream,
                                          public nsISeekableStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSITELLABLESTREAM

  // |aInputStream| must be a non-blocking, non-async inputSteam.
  static nsresult Create(already_AddRefed<nsIInputStream> aInputStream,
                         nsIAsyncInputStream** aAsyncInputStream);

 private:
  explicit NonBlockingAsyncInputStream(
      already_AddRefed<nsIInputStream> aInputStream);
  ~NonBlockingAsyncInputStream();

  template <typename M>
  void SerializeInternal(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed, M* aManager);

  class AsyncWaitRunnable;

  void RunAsyncWaitCallback(AsyncWaitRunnable* aRunnable,
                            already_AddRefed<nsIInputStreamCallback> aCallback);

  nsCOMPtr<nsIInputStream> mInputStream;

  // Raw pointers because these are just QI of mInputStream.
  nsICloneableInputStream* MOZ_NON_OWNING_REF mWeakCloneableInputStream;
  nsIIPCSerializableInputStream* MOZ_NON_OWNING_REF
      mWeakIPCSerializableInputStream;
  nsISeekableStream* MOZ_NON_OWNING_REF mWeakSeekableInputStream;
  nsITellableStream* MOZ_NON_OWNING_REF mWeakTellableInputStream;

  Mutex mLock;

  struct WaitClosureOnly {
    WaitClosureOnly(AsyncWaitRunnable* aRunnable, nsIEventTarget* aEventTarget);

    RefPtr<AsyncWaitRunnable> mRunnable;
    nsCOMPtr<nsIEventTarget> mEventTarget;
  };

  // This is set when AsyncWait is called with a callback and with
  // WAIT_CLOSURE_ONLY as flag.
  // This is protected by mLock.
  Maybe<WaitClosureOnly> mWaitClosureOnly;

  // This is protected by mLock.
  RefPtr<AsyncWaitRunnable> mAsyncWaitCallback;

  // This is protected by mLock.
  bool mClosed;
};

}  // namespace mozilla

#endif  // NonBlockingAsyncInputStream_h
