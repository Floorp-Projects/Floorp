/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHttpTransaction_h__
#define nsHttpTransaction_h__

#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsAHttpTransaction.h"
#include "nsAHttpConnection.h"
#include "nsCOMPtr.h"
#include "nsInt64.h"

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
                  PRBool                 reqBodyIncludesHeaders,
                  nsIEventTarget        *consumerTarget,
                  nsIInterfaceRequestor *callbacks,
                  nsITransportEventSink *eventsink,
                  nsIAsyncInputStream  **responseBody);

    // attributes
    PRUint8                Caps()           { return mCaps; }
    nsHttpConnectionInfo  *ConnectionInfo() { return mConnInfo; }
    nsHttpRequestHead     *RequestHead()    { return mRequestHead; }
    nsHttpResponseHead    *ResponseHead()   { return mHaveAllHeaders ? mResponseHead : nsnull; }
    nsISupports           *SecurityInfo()   { return mSecurityInfo; }

    nsIInterfaceRequestor *Callbacks()      { return mCallbacks; } 
    nsIEventTarget        *ConsumerTarget() { return mConsumerTarget; }
    nsAHttpConnection     *Connection()     { return mConnection; }

    // Called to take ownership of the response headers; the transaction
    // will drop any reference to the response headers after this call.
    nsHttpResponseHead *TakeResponseHead();

    // Called to find out if the transaction generated a complete response.
    PRBool ResponseIsComplete() { return mResponseIsComplete; }

    void   SetSSLConnectFailed() { mSSLConnectFailed = PR_TRUE; }
    PRBool    SSLConnectFailed() { return mSSLConnectFailed; }

    // These methods may only be used by the connection manager.
    void    SetPriority(PRInt32 priority) { mPriority = priority; }
    PRInt32    Priority()                 { return mPriority; }

    const TimingStruct& Timings() const { return mTimings; }

private:
    nsresult Restart();
    void     ParseLine(char *line);
    nsresult ParseLineSegment(char *seg, PRUint32 len);
    nsresult ParseHead(char *, PRUint32 count, PRUint32 *countRead);
    nsresult HandleContentStart();
    nsresult HandleContent(char *, PRUint32 count, PRUint32 *contentRead, PRUint32 *contentRemaining);
    nsresult ProcessData(char *, PRUint32, PRUint32 *);
    void     DeleteSelfOnConsumerThread();

    static NS_METHOD ReadRequestSegment(nsIInputStream *, void *, const char *,
                                        PRUint32, PRUint32, PRUint32 *);
    static NS_METHOD WritePipeSegment(nsIOutputStream *, void *, char *,
                                      PRUint32, PRUint32, PRUint32 *);

    PRBool TimingEnabled() const { return PR_TRUE; }

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

    nsInt64                         mContentLength;   // equals -1 if unknown
    nsInt64                         mContentRead;     // count of consumed content bytes

    nsHttpChunkedDecoder           *mChunkedDecoder;

    TimingStruct                    mTimings;

    nsresult                        mStatus;

    PRInt16                         mPriority;

    PRUint16                        mRestartCount;        // the number of times this transaction has been restarted
    PRUint8                         mCaps;

    // state flags
    PRUint32                        mClosed             : 1;
    PRUint32                        mConnected          : 1;
    PRUint32                        mHaveStatusLine     : 1;
    PRUint32                        mHaveAllHeaders     : 1;
    PRUint32                        mTransactionDone    : 1;
    PRUint32                        mResponseIsComplete : 1;
    PRUint32                        mDidContentStart    : 1;
    PRUint32                        mNoContent          : 1; // expecting an empty entity body
    PRUint32                        mSentData           : 1;
    PRUint32                        mReceivedData       : 1;
    PRUint32                        mStatusEventPending : 1;
    PRUint32                        mHasRequestBody     : 1;
    PRUint32                        mSSLConnectFailed   : 1;

    // mClosed           := transaction has been explicitly closed
    // mTransactionDone  := transaction ran to completion or was interrupted
    // mResponseComplete := transaction ran to completion
};

#endif // nsHttpTransaction_h__
