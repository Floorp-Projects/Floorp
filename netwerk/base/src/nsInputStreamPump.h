/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInputStreamPump_h__
#define nsInputStreamPump_h__

#include "nsIInputStreamPump.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIAsyncInputStream.h"
#include "nsIThread.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

class nsInputStreamPump MOZ_FINAL : public nsIInputStreamPump
                                  , public nsIInputStreamCallback
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSIINPUTSTREAMPUMP
    NS_DECL_NSIINPUTSTREAMCALLBACK

    nsInputStreamPump(); 
    ~nsInputStreamPump();

    static NS_HIDDEN_(nsresult)
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
    NS_HIDDEN_(nsresult) PeekStream(PeekSegmentFun callback, void *closure);

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
    nsCOMPtr<nsIThread>           mTargetThread;
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
    bool                          mWaiting; // true if waiting on async source
    bool                          mCloseWhenDone;
};

#endif // !nsInputStreamChannel_h__
