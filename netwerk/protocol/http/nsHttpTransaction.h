/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpTransaction_h__
#define nsHttpTransaction_h__

#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsAHttpTransaction.h"
#include "nsAHttpConnection.h"
#include "nsCOMPtr.h"

#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsISocketTransportService.h"
#include "nsITransport.h"
#include "nsIEventTarget.h"
#include "TimingStruct.h"

//-----------------------------------------------------------------------------

class nsHttpTransaction;
class nsHttpRequestHead;
class nsHttpResponseHead;
class nsHttpChunkedDecoder;
class nsIHttpActivityObserver;

//-----------------------------------------------------------------------------
// nsHttpTransaction represents a single HTTP transaction.  It is thread-safe,
// intended to run on the socket thread.
//-----------------------------------------------------------------------------

class nsHttpTransaction : public nsAHttpTransaction
                        , public nsIInputStreamCallback
                        , public nsIOutputStreamCallback
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSAHTTPTRANSACTION
    NS_DECL_NSIINPUTSTREAMCALLBACK
    NS_DECL_NSIOUTPUTSTREAMCALLBACK

    nsHttpTransaction();
    virtual ~nsHttpTransaction();

    //
    // called to initialize the transaction
    // 
    // @param caps
    //        the transaction capabilities (see nsHttp.h)
    // @param connInfo
    //        the connection type for this transaction.
    // @param reqHeaders
    //        the request header struct
    // @param reqBody
    //        the request body (POST or PUT data stream)
    // @param reqBodyIncludesHeaders
    //        fun stuff to support NPAPI plugins.
    // @param target
    //        the dispatch target were notifications should be sent.
    // @param callbacks
    //        the notification callbacks to be given to PSM.
    // @param responseBody
    //        the input stream that will contain the response data.  async
    //        wait on this input stream for data.  on first notification,
    //        headers should be available (check transaction status).
    //
    nsresult Init(PRUint8                caps,
                  nsHttpConnectionInfo  *connInfo,
                  nsHttpRequestHead     *reqHeaders,
                  nsIInputStream        *reqBody,
                  bool                   reqBodyIncludesHeaders,
                  nsIEventTarget        *consumerTarget,
                  nsIInterfaceRequestor *callbacks,
                  nsITransportEventSink *eventsink,
                  nsIAsyncInputStream  **responseBody);

    // attributes
    nsHttpConnectionInfo  *ConnectionInfo() { return mConnInfo; }
    nsHttpResponseHead    *ResponseHead()   { return mHaveAllHeaders ? mResponseHead : nsnull; }
    nsISupports           *SecurityInfo()   { return mSecurityInfo; }

    nsIInterfaceRequestor *Callbacks()      { return mCallbacks; } 
    nsIEventTarget        *ConsumerTarget() { return mConsumerTarget; }

    // Called to take ownership of the response headers; the transaction
    // will drop any reference to the response headers after this call.
    nsHttpResponseHead *TakeResponseHead();

    // Called to find out if the transaction generated a complete response.
    bool ResponseIsComplete() { return mResponseIsComplete; }

    bool      SSLConnectFailed() { return mSSLConnectFailed; }

    // SetPriority() may only be used by the connection manager.
    void    SetPriority(PRInt32 priority) { mPriority = priority; }
    PRInt32    Priority()                 { return mPriority; }

    const TimingStruct& Timings() const { return mTimings; }
    enum Classifier Classification() { return mClassification; }

private:
    nsresult Restart();
    nsresult RestartInProgress();
    char    *LocateHttpStart(char *buf, PRUint32 len,
                             bool aAllowPartialMatch);
    nsresult ParseLine(char *line);
    nsresult ParseLineSegment(char *seg, PRUint32 len);
    nsresult ParseHead(char *, PRUint32 count, PRUint32 *countRead);
    nsresult HandleContentStart();
    nsresult HandleContent(char *, PRUint32 count, PRUint32 *contentRead, PRUint32 *contentRemaining);
    nsresult ProcessData(char *, PRUint32, PRUint32 *);
    void     DeleteSelfOnConsumerThread();

    Classifier Classify();
    void       CancelPipeline(PRUint32 reason);

    static NS_METHOD ReadRequestSegment(nsIInputStream *, void *, const char *,
                                        PRUint32, PRUint32, PRUint32 *);
    static NS_METHOD WritePipeSegment(nsIOutputStream *, void *, char *,
                                      PRUint32, PRUint32, PRUint32 *);

    bool TimingEnabled() const { return mCaps & NS_HTTP_TIMING_ENABLED; }

private:
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsITransportEventSink> mTransportSink;
    nsCOMPtr<nsIEventTarget>        mConsumerTarget;
    nsCOMPtr<nsISupports>           mSecurityInfo;
    nsCOMPtr<nsIAsyncInputStream>   mPipeIn;
    nsCOMPtr<nsIAsyncOutputStream>  mPipeOut;

    nsCOMPtr<nsISupports>             mChannel;
    nsCOMPtr<nsIHttpActivityObserver> mActivityDistributor;

    nsCString                       mReqHeaderBuf;    // flattened request headers
    nsCOMPtr<nsIInputStream>        mRequestStream;
    PRUint32                        mRequestSize;

    nsAHttpConnection              *mConnection;      // hard ref
    nsHttpConnectionInfo           *mConnInfo;        // hard ref
    nsHttpRequestHead              *mRequestHead;     // weak ref
    nsHttpResponseHead             *mResponseHead;    // hard ref

    nsAHttpSegmentReader           *mReader;
    nsAHttpSegmentWriter           *mWriter;

    nsCString                       mLineBuf;         // may contain a partial line

    PRInt64                         mContentLength;   // equals -1 if unknown
    PRInt64                         mContentRead;     // count of consumed content bytes

    // After a 304/204 or other "no-content" style response we will skip over
    // up to MAX_INVALID_RESPONSE_BODY_SZ bytes when looking for the next
    // response header to deal with servers that actually sent a response
    // body where they should not have. This member tracks how many bytes have
    // so far been skipped.
    PRUint32                        mInvalidResponseBytesRead;

    nsHttpChunkedDecoder           *mChunkedDecoder;

    TimingStruct                    mTimings;

    nsresult                        mStatus;

    PRInt16                         mPriority;

    PRUint16                        mRestartCount;        // the number of times this transaction has been restarted
    PRUint8                         mCaps;
    enum Classifier                 mClassification;
    PRInt32                         mPipelinePosition;
    PRInt64                         mMaxPipelineObjectSize;

    // state flags, all logically boolean, but not packed together into a
    // bitfield so as to avoid bitfield-induced races.  See bug 560579.
    bool                            mClosed;
    bool                            mConnected;
    bool                            mHaveStatusLine;
    bool                            mHaveAllHeaders;
    bool                            mTransactionDone;
    bool                            mResponseIsComplete;
    bool                            mDidContentStart;
    bool                            mNoContent; // expecting an empty entity body
    bool                            mSentData;
    bool                            mReceivedData;
    bool                            mStatusEventPending;
    bool                            mHasRequestBody;
    bool                            mSSLConnectFailed;
    bool                            mHttpResponseMatched;
    bool                            mPreserveStream;

    // mClosed           := transaction has been explicitly closed
    // mTransactionDone  := transaction ran to completion or was interrupted
    // mResponseComplete := transaction ran to completion

    // For Restart-In-Progress Functionality
    bool                            mReportedStart;
    bool                            mReportedResponseHeader;

    // protected by nsHttp::GetLock()
    nsHttpResponseHead             *mForTakeResponseHead;
    bool                            mResponseHeadTaken;

    class RestartVerifier 
    {

        // When a idemptotent transaction has received part of its response body
        // and incurs an error it can be restarted. To do this we mark the place
        // where we stopped feeding the body to the consumer and start the
        // network call over again. If everything we track (headers, length, etc..)
        // matches up to the place where we left off then the consumer starts being
        // fed data again with the new information. This can be done N times up
        // to the normal restart (i.e. with no response info) limit.

    public:
        RestartVerifier()
            : mContentLength(-1)
            , mAlreadyProcessed(0)
            , mToReadBeforeRestart(0)
            , mSetup(false)
        {}
        ~RestartVerifier() {}
        
        void Set(PRInt64 contentLength, nsHttpResponseHead *head);
        bool Verify(PRInt64 contentLength, nsHttpResponseHead *head);
        bool IsDiscardingContent() { return mToReadBeforeRestart != 0; }
        bool IsSetup() { return mSetup; }
        PRInt64 AlreadyProcessed() { return mAlreadyProcessed; }
        void SetAlreadyProcessed(PRInt64 val) {
            mAlreadyProcessed = val;
            mToReadBeforeRestart = val;
        }
        PRInt64 ToReadBeforeRestart() { return mToReadBeforeRestart; }
        void HaveReadBeforeRestart(PRUint32 amt)
        {
            NS_ABORT_IF_FALSE(amt <= mToReadBeforeRestart,
                              "too large of a HaveReadBeforeRestart deduction");
            mToReadBeforeRestart -= amt;
        }

    private:
        // This is the data from the first complete response header
        // used to make sure that all subsequent response headers match

        PRInt64                         mContentLength;
        nsCString                       mETag;
        nsCString                       mLastModified;
        nsCString                       mContentRange;
        nsCString                       mContentEncoding;
        nsCString                       mTransferEncoding;

        // This is the amount of data that has been passed to the channel
        // from previous iterations of the transaction and must therefore
        // be skipped in the new one.
        PRInt64                         mAlreadyProcessed;

        // The amount of data that must be discarded in the current iteration
        // (where iteration > 0) to reach the mAlreadyProcessed high water
        // mark.
        PRInt64                         mToReadBeforeRestart;

        // true when ::Set has been called with a response header
        bool                            mSetup;
    } mRestartInProgressVerifier;
};

#endif // nsHttpTransaction_h__
