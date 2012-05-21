/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpPipeline_h__
#define nsHttpPipeline_h__

#include "nsHttp.h"
#include "nsAHttpConnection.h"
#include "nsAHttpTransaction.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

class nsHttpPipeline : public nsAHttpConnection
                     , public nsAHttpTransaction
                     , public nsAHttpSegmentReader
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSAHTTPCONNECTION(mConnection)
    NS_DECL_NSAHTTPTRANSACTION
    NS_DECL_NSAHTTPSEGMENTREADER

    nsHttpPipeline();
    virtual ~nsHttpPipeline();

private:
    nsresult FillSendBuf();
    
    static NS_METHOD ReadFromPipe(nsIInputStream *, void *, const char *,
                                  PRUint32, PRUint32, PRUint32 *);

    // convenience functions
    nsAHttpTransaction *Request(PRInt32 i)
    {
        if (mRequestQ.Length() == 0)
            return nsnull;

        return mRequestQ[i];
    }
    nsAHttpTransaction *Response(PRInt32 i)
    {
        if (mResponseQ.Length() == 0)
            return nsnull;

        return mResponseQ[i];
    }

    // overload of nsAHttpTransaction::QueryPipeline()
    nsHttpPipeline *QueryPipeline();

    nsAHttpConnection            *mConnection;
    nsTArray<nsAHttpTransaction*> mRequestQ;  // array of transactions
    nsTArray<nsAHttpTransaction*> mResponseQ; // array of transactions
    nsresult                      mStatus;

    // these flags indicate whether or not the first request or response
    // is partial.  a partial request means that Request(0) has been 
    // partially written out to the socket.  a partial response means
    // that Response(0) has been partially read in from the socket.
    bool mRequestIsPartial;
    bool mResponseIsPartial;

    // indicates whether or not the pipeline has been explicitly closed.
    bool mClosed;

    // indicates whether or not a true pipeline (more than 1 request without
    // a synchronous response) has been formed.
    bool mUtilizedPipeline;

    // used when calling ReadSegments/WriteSegments on a transaction.
    nsAHttpSegmentReader *mReader;
    nsAHttpSegmentWriter *mWriter;

    // send buffer
    nsCOMPtr<nsIInputStream>  mSendBufIn;
    nsCOMPtr<nsIOutputStream> mSendBufOut;

    // the push back buffer.  not exceeding nsIOService::gDefaultSegmentSize bytes.
    char     *mPushBackBuf;
    PRUint32  mPushBackLen;
    PRUint32  mPushBackMax;

    // The number of transactions completed on this pipeline.
    PRUint32  mHttp1xTransactionCount;

    // For support of OnTransportStatus()
    PRUint64  mReceivingFromProgress;
    PRUint64  mSendingToProgress;
    bool      mSuppressSendEvents;
};

#endif // nsHttpPipeline_h__
