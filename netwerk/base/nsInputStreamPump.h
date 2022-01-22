/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInputStreamPump_h__
#define nsInputStreamPump_h__

#include "nsIInputStreamPump.h"
#include "nsIAsyncInputStream.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/RecursiveMutex.h"

class nsIInputStream;
class nsILoadGroup;
class nsIStreamListener;

#define NS_INPUT_STREAM_PUMP_IID                     \
  {                                                  \
    0x42f1cc9b, 0xdf5f, 0x4c9b, {                    \
      0xbd, 0x71, 0x8d, 0x4a, 0xe2, 0x27, 0xc1, 0x8a \
    }                                                \
  }

class nsInputStreamPump final : public nsIInputStreamPump,
                                public nsIInputStreamCallback,
                                public nsIThreadRetargetableRequest {
  ~nsInputStreamPump() = default;

 public:
  using RecursiveMutexAutoLock = mozilla::RecursiveMutexAutoLock;
  using RecursiveMutexAutoUnlock = mozilla::RecursiveMutexAutoUnlock;
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSIINPUTSTREAMPUMP
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSITHREADRETARGETABLEREQUEST
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INPUT_STREAM_PUMP_IID)

  nsInputStreamPump();

  static nsresult Create(nsInputStreamPump** result, nsIInputStream* stream,
                         uint32_t segsize = 0, uint32_t segcount = 0,
                         bool closeWhenDone = false,
                         nsIEventTarget* mainThreadTarget = nullptr);

  using PeekSegmentFun = void (*)(void*, const uint8_t*, uint32_t);
  /**
   * Peek into the first chunk of data that's in the stream. Note that this
   * method will not call the callback when there is no data in the stream.
   * The callback will be called at most once.
   *
   * The data from the stream will not be consumed, i.e. the pump's listener
   * can still read all the data.
   *
   * Do not call before asyncRead. Do not call after onStopRequest.
   */
  nsresult PeekStream(PeekSegmentFun callback, void* closure);

  /**
   * Dispatched (to the main thread) by OnStateStop if it's called off main
   * thread. Updates mState based on return value of OnStateStop.
   */
  nsresult CallOnStateStop();

 protected:
  enum { STATE_IDLE, STATE_START, STATE_TRANSFER, STATE_STOP };

  nsresult EnsureWaiting();
  uint32_t OnStateStart();
  uint32_t OnStateTransfer();
  uint32_t OnStateStop();
  nsresult CreateBufferedStreamIfNeeded();

  uint32_t mState{STATE_IDLE};
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIEventTarget> mTargetThread;
  nsCOMPtr<nsIEventTarget> mLabeledMainThreadTarget;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIAsyncInputStream> mAsyncStream;
  uint64_t mStreamOffset{0};
  uint64_t mStreamLength{0};
  uint32_t mSegSize{0};
  uint32_t mSegCount{0};
  nsresult mStatus{NS_OK};
  uint32_t mSuspendCount{0};
  uint32_t mLoadFlags{LOAD_NORMAL};
  bool mIsPending{false};
  // True while in OnInputStreamReady, calling OnStateStart, OnStateTransfer
  // and OnStateStop. Used to prevent calls to AsyncWait during callbacks.
  bool mProcessingCallbacks{false};
  // True if waiting on the "input stream ready" callback.
  bool mWaitingForInputStreamReady{false};
  bool mCloseWhenDone{false};
  bool mRetargeting{false};
  bool mAsyncStreamIsBuffered{false};
  // Indicate whether nsInputStreamPump is used completely off main thread.
  // If true, OnStateStop() is executed off main thread.
  bool mOffMainThread;
  // Protects state/member var accesses across multiple threads.
  mozilla::RecursiveMutex mMutex{"nsInputStreamPump"};
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsInputStreamPump, NS_INPUT_STREAM_PUMP_IID)

#endif  // !nsInputStreamChannel_h__
