/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

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
#include "nsIEventQueue.h"

//-----------------------------------------------------------------------------

class nsHttpTransaction;
class nsHttpRequestHead;
class nsHttpResponseHead;
class nsHttpChunkedDecoder;

//-----------------------------------------------------------------------------
// nsHttpTransaction represents a single HTTP transaction.  It is thread-safe,
// intended to run on the socket thread.
//-----------------------------------------------------------------------------

class nsHttpTransaction : public nsAHttpTransaction
                        , public nsIOutputStreamNotify
                        , public nsISocketEventHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAMNOTIFY
    NS_DECL_NSISOCKETEVENTHANDLER

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
    // @param eventQ
    //        the event queue were notifications should be sent.
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
                  PRBool                 reqBodyIncludesHeaders,
                  nsIEventQueue         *eventQ,
                  nsIInterfaceRequestor *callbacks,
                  nsITransportEventSink *eventsink,
                  nsIAsyncInputStream  **responseBody);

    // attributes
    PRUint8                Caps()           { return mCaps; }
    nsHttpConnectionInfo  *ConnectionInfo() { return mConnInfo; }
    nsHttpRequestHead     *RequestHead()    { return mRequestHead; }
    nsHttpResponseHead    *ResponseHead()   { return mHaveAllHeaders ? mResponseHead : nsnull; }
    nsISupports           *SecurityInfo()   { return mSecurityInfo; }
    nsresult               TransportStatus(){ return mTransportStatus; }

    nsIInterfaceRequestor *Callbacks()      { return mCallbacks; } 
    nsIEventQueue         *ConsumerEventQ() { return mConsumerEventQ; }
    nsAHttpConnection     *Connection()     { return mConnection; }

    // Called to take ownership of the response headers; the transaction
    // will drop any reference to the response headers after this call.
    nsHttpResponseHead *TakeResponseHead();

    // Called to find out if the transaction generated a complete response.
    PRBool ResponseIsComplete() { return mResponseIsComplete; }

    //-------------------------------------------------------------------------
    // nsAHttpTransaction methods:
    //-------------------------------------------------------------------------

    void SetConnection(nsAHttpConnection *conn)
    {
        NS_IF_RELEASE(mConnection);
        NS_IF_ADDREF(mConnection = conn);
    }
    void GetSecurityCallbacks(nsIInterfaceRequestor **cb)
    {
        NS_IF_ADDREF(*cb = mCallbacks);
    }
    void     OnTransportStatus(nsresult status, PRUint32 progress);
    PRBool   IsDone() { return mTransactionDone; }
    nsresult Status() { return mStatus; }
    PRUint32 Available();
    nsresult ReadSegments(nsAHttpSegmentReader *, PRUint32, PRUint32 *);
    nsresult WriteSegments(nsAHttpSegmentWriter *, PRUint32, PRUint32 *);
    void     Close(nsresult);

private:
    nsresult Restart();
    void     ParseLine(char *line);
    nsresult ParseLineSegment(char *seg, PRUint32 len);
    nsresult ParseHead(char *, PRUint32 count, PRUint32 *countRead);
    nsresult HandleContentStart();
    nsresult HandleContent(char *, PRUint32 count, PRUint32 *contentRead, PRUint32 *contentRemaining);
    nsresult ProcessData(char *, PRUint32, PRUint32 *);
    void     DeleteSelfOnConsumerThread();

    static void *PR_CALLBACK TransportStatus_Handler(PLEvent *);
    static void  PR_CALLBACK TransportStatus_Cleanup(PLEvent *);
    static void *PR_CALLBACK DeleteThis_Handler(PLEvent *);
    static void  PR_CALLBACK DeleteThis_Cleanup(PLEvent *);

    static NS_METHOD ReadRequestSegment(nsIInputStream *, void *, const char *,
                                        PRUint32, PRUint32, PRUint32 *);
    static NS_METHOD WritePipeSegment(nsIOutputStream *, void *, char *,
                                      PRUint32, PRUint32, PRUint32 *);

private:
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsITransportEventSink> mTransportSink;
    nsCOMPtr<nsIEventQueue>         mConsumerEventQ;
    nsCOMPtr<nsISupports>           mSecurityInfo;
    nsCOMPtr<nsIAsyncInputStream>   mPipeIn;
    nsCOMPtr<nsIAsyncOutputStream>  mPipeOut;

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

    PRInt32                         mContentLength;   // equals -1 if unknown
    PRUint32                        mContentRead;     // count of consumed content bytes

    nsHttpChunkedDecoder           *mChunkedDecoder;

    nsresult                        mStatus;

    // this lock is used to protect access to members which may be accessed
    // from both the main thread as well as the socket thread.
    PRLock                         *mLock;

    // these transport status fields are protected by mLock
    nsresult                        mTransportStatus;
    PRUint32                        mTransportProgress;
    PRUint32                        mTransportProgressMax;
    PRPackedBool                    mTransportStatusInProgress;

    PRUint16                        mRestartCount;        // the number of times this transaction has been restarted
    PRUint8                         mCaps;

    PRPackedBool                    mConnected;
    PRPackedBool                    mHaveStatusLine;
    PRPackedBool                    mHaveAllHeaders;
    PRPackedBool                    mTransactionDone;
    PRPackedBool                    mResponseIsComplete;  // == mTransactionDone && NS_SUCCEEDED(mStatus) ?
    PRPackedBool                    mDidContentStart;
    PRPackedBool                    mNoContent;           // expecting an empty entity body?
    PRPackedBool                    mPrematureEOF;
    PRPackedBool                    mDestroying;
};

#endif // nsHttpTransaction_h__
