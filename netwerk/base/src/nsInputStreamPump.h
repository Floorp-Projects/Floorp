/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "mozilla/ReentrantMonitor.h"

class nsIInputStream;
class nsILoadGroup;
class nsIStreamListener;

class nsInputStreamPump MOZ_FINAL : public nsIInputStreamPump
                                  , public nsIInputStreamCallback
                                  , public nsIThreadRetargetableRequest
{
public:
    typedef mozilla::ReentrantMonitorAutoEnter ReentrantMonitorAutoEnter;
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSIINPUTSTREAMPUMP
    NS_DECL_NSIINPUTSTREAMCALLBACK
    NS_DECL_NSITHREADRETARGETABLEREQUEST

    nsInputStreamPump(); 
    ~nsInputStreamPump();

    static nsresult
                      Create(nsInputStreamPump  **result,
                             nsIInputStream      *stream,
                             int64_t              streamPos = -1,
                             int64_t              streamLen = -1,
                             uint32_t             segsize = 0,
                             uint32_t             segcount = 0,
                             bool                 closeWhenDone = false);

    typedef void (*PeekSegmentFun)(void *closure, const uint8_t *buf,
                                   uint32_t bufLen);
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
    nsresult PeekStream(PeekSegmentFun callback, void *closure);

    /**
     * Dispatched (to the main thread) by OnStateStop if it's called off main
     * thread. Updates mState based on return value of OnStateStop.
     */
    nsresult CallOnStateStop();

protected:

    enum {
        STATE_IDLE,
        STATE_START,
        STATE_TRANSFER,
        STATE_STOP
    };

    nsresult EnsureWaiting();
    uint32_t OnStateStart();
    uint32_t OnStateTransfer();
    uint32_t OnStateStop();

    uint32_t                      mState;
    nsCOMPtr<nsILoadGroup>        mLoadGroup;
    nsCOMPtr<nsIStreamListener>   mListener;
    nsCOMPtr<nsISupports>         mListenerContext;
    nsCOMPtr<nsIEventTarget>      mTargetThread;
    nsCOMPtr<nsIInputStream>      mStream;
    nsCOMPtr<nsIAsyncInputStream> mAsyncStream;
    uint64_t                      mStreamOffset;
    uint64_t                      mStreamLength;
    uint32_t                      mSegSize;
    uint32_t                      mSegCount;
    nsresult                      mStatus;
    uint32_t                      mSuspendCount;
    uint32_t                      mLoadFlags;
    bool                          mIsPending;
    // True while in OnInputStreamReady, calling OnStateStart, OnStateTransfer
    // and OnStateStop. Used to prevent calls to AsyncWait during callbacks.
    bool                          mProcessingCallbacks;
    // True if waiting on the "input stream ready" callback.
    bool                          mWaitingForInputStreamReady;
    bool                          mCloseWhenDone;
    bool                          mRetargeting;
    // Protects state/member var accesses across multiple threads.
    mozilla::ReentrantMonitor     mMonitor;
};

#endif // !nsInputStreamChannel_h__
