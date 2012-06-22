/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAHttpTransaction_h__
#define nsAHttpTransaction_h__

#include "nsISupports.h"
#include "nsTArray.h"

class nsAHttpConnection;
class nsAHttpSegmentReader;
class nsAHttpSegmentWriter;
class nsIInterfaceRequestor;
class nsIEventTarget;
class nsITransport;
class nsHttpRequestHead;
class nsHttpPipeline;

//----------------------------------------------------------------------------
// Abstract base class for a HTTP transaction:
//
// A transaction is a "sink" for the response data.  The connection pushes
// data to the transaction by writing to it.  The transaction supports
// WriteSegments and may refuse to accept data if its buffers are full (its
// write function returns NS_BASE_STREAM_WOULD_BLOCK in this case).
//----------------------------------------------------------------------------

class nsAHttpTransaction : public nsISupports
{
public:
    // called by the connection when it takes ownership of the transaction.
    virtual void SetConnection(nsAHttpConnection *) = 0;

    // used to obtain the connection associated with this transaction
    virtual nsAHttpConnection *Connection() = 0;

    // called by the connection to get security callbacks to set on the
    // socket transport.
    virtual void GetSecurityCallbacks(nsIInterfaceRequestor **,
                                      nsIEventTarget **) = 0;

    // called to report socket status (see nsITransportEventSink)
    virtual void OnTransportStatus(nsITransport* transport,
                                   nsresult status, PRUint64 progress) = 0;

    // called to check the transaction status.
    virtual bool     IsDone() = 0;
    virtual nsresult Status() = 0;
    virtual PRUint8  Caps() = 0;

    // called to find out how much request data is available for writing.
    virtual PRUint32 Available() = 0;

    // called to read request data from the transaction.
    virtual nsresult ReadSegments(nsAHttpSegmentReader *reader,
                                  PRUint32 count, PRUint32 *countRead) = 0;

    // called to write response data to the transaction.
    virtual nsresult WriteSegments(nsAHttpSegmentWriter *writer,
                                   PRUint32 count, PRUint32 *countWritten) = 0;

    // called to close the transaction
    virtual void Close(nsresult reason) = 0;

    // called to indicate a failure at the SSL setup level
    virtual void SetSSLConnectFailed() = 0;
    
    // called to retrieve the request headers of the transaction
    virtual nsHttpRequestHead *RequestHead() = 0;

    // determine the number of real http/1.x transactions on this
    // abstract object. Pipelines may have multiple, SPDY has 0,
    // normal http transactions have 1.
    virtual PRUint32 Http1xTransactionCount() = 0;

    // called to remove the unused sub transactions from an object that can
    // handle multiple transactions simultaneously (i.e. pipelines or spdy).
    //
    // Returns NS_ERROR_NOT_IMPLEMENTED if the object does not implement
    // sub-transactions.
    //
    // Returns NS_ERROR_ALREADY_OPENED if the subtransactions have been
    // at least partially written and cannot be moved.
    //
    virtual nsresult TakeSubTransactions(
        nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions) = 0;

    // called to add a sub-transaction in the case of pipelined transactions
    // classes that do not implement sub transactions
    // return NS_ERROR_NOT_IMPLEMENTED
    virtual nsresult AddTransaction(nsAHttpTransaction *transaction) = 0;
    
    // The total length of the outstanding pipeline comprised of transacations
    // and sub-transactions.
    virtual PRUint32 PipelineDepth() = 0;

    // Used to inform the connection that it is being used in a pipelined
    // context. That may influence the handling of some errors.
    // The value is the pipeline position (> 1).
    virtual nsresult SetPipelinePosition(PRInt32) = 0;
    virtual PRInt32  PipelinePosition() = 0;

    // If we used rtti this would be the result of doing
    // dynamic_cast<nsHttpPipeline *>(this).. i.e. it can be nsnull for
    // non pipeline implementations of nsAHttpTransaction
    virtual nsHttpPipeline *QueryPipeline() { return nsnull; }

    // equivalent to !!dynamic_cast<NullHttpTransaction *>(this)
    // A null transaction is expected to return BASE_STREAM_CLOSED on all of
    // its IO functions all the time.
    virtual bool IsNullTransaction() { return false; }
    
    // Every transaction is classified into one of the types below. When using
    // HTTP pipelines, only transactions with the same type appear on the same
    // pipeline.
    enum Classifier  {
        // Transactions that expect a short 304 (no-content) response
        CLASS_REVALIDATION,

        // Transactions for content expected to be CSS or JS
        CLASS_SCRIPT,

        // Transactions for content expected to be an image
        CLASS_IMAGE,

        // Transactions that cannot involve a pipeline 
        CLASS_SOLO,

        // Transactions that do not fit any of the other categories. HTML
        // is normally GENERAL.
        CLASS_GENERAL,

        CLASS_MAX
    };
};

#define NS_DECL_NSAHTTPTRANSACTION \
    void SetConnection(nsAHttpConnection *); \
    nsAHttpConnection *Connection(); \
    void GetSecurityCallbacks(nsIInterfaceRequestor **, \
                              nsIEventTarget **);       \
    void OnTransportStatus(nsITransport* transport, \
                           nsresult status, PRUint64 progress); \
    bool     IsDone(); \
    nsresult Status(); \
    PRUint8  Caps();   \
    PRUint32 Available(); \
    nsresult ReadSegments(nsAHttpSegmentReader *, PRUint32, PRUint32 *); \
    nsresult WriteSegments(nsAHttpSegmentWriter *, PRUint32, PRUint32 *); \
    void     Close(nsresult reason);                                    \
    void     SetSSLConnectFailed();                                     \
    nsHttpRequestHead *RequestHead();                                   \
    PRUint32 Http1xTransactionCount();                                  \
    nsresult TakeSubTransactions(nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions); \
    nsresult AddTransaction(nsAHttpTransaction *);                      \
    PRUint32 PipelineDepth();                                           \
    nsresult SetPipelinePosition(PRInt32);                              \
    PRInt32  PipelinePosition();

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

class nsAHttpSegmentReader
{
public:
    // any returned failure code stops segment iteration
    virtual nsresult OnReadSegment(const char *segment,
                                   PRUint32 count,
                                   PRUint32 *countRead) = 0;

    // Ask the segment reader to commit to accepting size bytes of
    // data from subsequent OnReadSegment() calls or throw hard
    // (i.e. not wouldblock) exceptions. Implementations
    // can return NS_ERROR_FAILURE if they never make commitments of that size
    // (the default), NS_BASE_STREAM_WOULD_BLOCK if they cannot make
    // the commitment now but might in the future, or NS_OK
    // if they make the commitment.
    //
    // Spdy uses this to make sure frames are atomic.
    virtual nsresult CommitToSegmentSize(PRUint32 size)
    {
        return NS_ERROR_FAILURE;
    }
};

#define NS_DECL_NSAHTTPSEGMENTREADER \
    nsresult OnReadSegment(const char *, PRUint32, PRUint32 *);

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

class nsAHttpSegmentWriter
{
public:
    // any returned failure code stops segment iteration
    virtual nsresult OnWriteSegment(char *segment,
                                    PRUint32 count,
                                    PRUint32 *countWritten) = 0;
};

#define NS_DECL_NSAHTTPSEGMENTWRITER \
    nsresult OnWriteSegment(char *, PRUint32, PRUint32 *);

#endif // nsAHttpTransaction_h__
