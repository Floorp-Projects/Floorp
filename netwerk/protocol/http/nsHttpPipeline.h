/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpPipeline_h__
#define nsHttpPipeline_h__

#include "nsAHttpConnection.h"
#include "nsAHttpTransaction.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

class nsIInputStream;
class nsIOutputStream;

namespace mozilla { namespace net {

class nsHttpPipeline MOZ_FINAL : public nsAHttpConnection
                               , public nsAHttpTransaction
                               , public nsAHttpSegmentReader
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSAHTTPCONNECTION(mConnection)
    NS_DECL_NSAHTTPTRANSACTION
    NS_DECL_NSAHTTPSEGMENTREADER

    nsHttpPipeline();

  bool ResponseTimeoutEnabled() const MOZ_OVERRIDE MOZ_FINAL {
    return true;
  }

private:
    virtual ~nsHttpPipeline();

    nsresult FillSendBuf();

    static NS_METHOD ReadFromPipe(nsIInputStream *, void *, const char *,
                                  uint32_t, uint32_t, uint32_t *);

    // convenience functions
    nsAHttpTransaction *Request(int32_t i)
    {
        if (mRequestQ.Length() == 0)
            return nullptr;

        return mRequestQ[i];
    }
    nsAHttpTransaction *Response(int32_t i)
    {
        if (mResponseQ.Length() == 0)
            return nullptr;

        return mResponseQ[i];
    }

    // overload of nsAHttpTransaction::QueryPipeline()
    nsHttpPipeline *QueryPipeline();

    nsRefPtr<nsAHttpConnection>   mConnection;
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

    // send buffer
    nsCOMPtr<nsIInputStream>  mSendBufIn;
    nsCOMPtr<nsIOutputStream> mSendBufOut;

    // the push back buffer.  not exceeding nsIOService::gDefaultSegmentSize bytes.
    char     *mPushBackBuf;
    uint32_t  mPushBackLen;
    uint32_t  mPushBackMax;

    // The number of transactions completed on this pipeline.
    uint32_t  mHttp1xTransactionCount;

    // For support of OnTransportStatus()
    uint64_t  mReceivingFromProgress;
    uint64_t  mSendingToProgress;
    bool      mSuppressSendEvents;
};

}} // namespace mozilla::net

#endif // nsHttpPipeline_h__
