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

class nsInputStreamPump : public nsIInputStreamPump
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
                             PRInt64              streamPos = -1,
                             PRInt64              streamLen = -1,
                             PRUint32             segsize = 0,
                             PRUint32             segcount = 0,
                             bool                 closeWhenDone = false);

    typedef void (*PeekSegmentFun)(void *closure, const PRUint8 *buf,
                                   PRUint32 bufLen);
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
    PRUint32 OnStateStart();
    PRUint32 OnStateTransfer();
    PRUint32 OnStateStop();

    PRUint32                      mState;
    nsCOMPtr<nsILoadGroup>        mLoadGroup;
    nsCOMPtr<nsIStreamListener>   mListener;
    nsCOMPtr<nsISupports>         mListenerContext;
    nsCOMPtr<nsIThread>           mTargetThread;
    nsCOMPtr<nsIInputStream>      mStream;
    nsCOMPtr<nsIAsyncInputStream> mAsyncStream;
    PRUint64                      mStreamOffset;
    PRUint64                      mStreamLength;
    PRUint32                      mSegSize;
    PRUint32                      mSegCount;
    nsresult                      mStatus;
    PRUint32                      mSuspendCount;
    PRUint32                      mLoadFlags;
    bool                          mIsPending;
    bool                          mWaiting; // true if waiting on async source
    bool                          mCloseWhenDone;
};

#endif // !nsInputStreamChannel_h__
