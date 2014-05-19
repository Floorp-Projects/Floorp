/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAHttpTransaction_h__
#define nsAHttpTransaction_h__

#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

class nsIInterfaceRequestor;
class nsIEventTarget;
class nsITransport;
class nsILoadGroupConnectionInfo;

namespace mozilla { namespace net {

class nsAHttpConnection;
class nsAHttpSegmentReader;
class nsAHttpSegmentWriter;
class nsHttpTransaction;
class nsHttpPipeline;
class nsHttpRequestHead;
class nsHttpConnectionInfo;
class SpdyConnectTransaction;

//----------------------------------------------------------------------------
// Abstract base class for a HTTP transaction:
//
// A transaction is a "sink" for the response data.  The connection pushes
// data to the transaction by writing to it.  The transaction supports
// WriteSegments and may refuse to accept data if its buffers are full (its
// write function returns NS_BASE_STREAM_WOULD_BLOCK in this case).
//----------------------------------------------------------------------------

// 2af6d634-13e3-494c-8903-c9dce5c22fc0
#define NS_AHTTPTRANSACTION_IID \
{ 0x2af6d634, 0x13e3, 0x494c, {0x89, 0x03, 0xc9, 0xdc, 0xe5, 0xc2, 0x2f, 0xc0 }}

class nsAHttpTransaction : public nsSupportsWeakReference
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_AHTTPTRANSACTION_IID)

    // called by the connection when it takes ownership of the transaction.
    virtual void SetConnection(nsAHttpConnection *) = 0;

    // used to obtain the connection associated with this transaction
    virtual nsAHttpConnection *Connection() = 0;

    // called by the connection to get security callbacks to set on the
    // socket transport.
    virtual void GetSecurityCallbacks(nsIInterfaceRequestor **) = 0;

    // called to report socket status (see nsITransportEventSink)
    virtual void OnTransportStatus(nsITransport* transport,
                                   nsresult status, uint64_t progress) = 0;

    // called to check the transaction status.
    virtual bool     IsDone() = 0;
    virtual nsresult Status() = 0;
    virtual uint32_t Caps() = 0;

    // called to notify that a requested DNS cache entry was refreshed.
    virtual void     SetDNSWasRefreshed() = 0;

    // called to find out how much request data is available for writing.
    virtual uint64_t Available() = 0;

    // called to read request data from the transaction.
    virtual nsresult ReadSegments(nsAHttpSegmentReader *reader,
                                  uint32_t count, uint32_t *countRead) = 0;

    // called to write response data to the transaction.
    virtual nsresult WriteSegments(nsAHttpSegmentWriter *writer,
                                   uint32_t count, uint32_t *countWritten) = 0;

    // called to close the transaction
    virtual void Close(nsresult reason) = 0;

    // called to indicate a failure with proxy CONNECT
    virtual void SetProxyConnectFailed() = 0;

    // called to retrieve the request headers of the transaction
    virtual nsHttpRequestHead *RequestHead() = 0;

    // determine the number of real http/1.x transactions on this
    // abstract object. Pipelines may have multiple, SPDY has 0,
    // normal http transactions have 1.
    virtual uint32_t Http1xTransactionCount() = 0;

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
    virtual uint32_t PipelineDepth() = 0;

    // Used to inform the connection that it is being used in a pipelined
    // context. That may influence the handling of some errors.
    // The value is the pipeline position (> 1).
    virtual nsresult SetPipelinePosition(int32_t) = 0;
    virtual int32_t  PipelinePosition() = 0;

    // Occasionally the abstract interface has to give way to base implementations
    // to respect differences between spdy, pipelines, etc..
    // These Query* (and IsNUllTransaction()) functions provide a way to do
    // that without using xpcom or rtti. Any calling code that can't deal with
    // a null response from one of them probably shouldn't be using nsAHttpTransaction

    // If we used rtti this would be the result of doing
    // dynamic_cast<nsHttpPipeline *>(this).. i.e. it can be nullptr for
    // non pipeline implementations of nsAHttpTransaction
    virtual nsHttpPipeline *QueryPipeline() { return nullptr; }

    // equivalent to !!dynamic_cast<NullHttpTransaction *>(this)
    // A null transaction is expected to return BASE_STREAM_CLOSED on all of
    // its IO functions all the time.
    virtual bool IsNullTransaction() { return false; }

    // If we used rtti this would be the result of doing
    // dynamic_cast<nsHttpTransaction *>(this).. i.e. it can be nullptr for
    // non nsHttpTransaction implementations of nsAHttpTransaction
    virtual nsHttpTransaction *QueryHttpTransaction() { return nullptr; }

    // If we used rtti this would be the result of doing
    // dynamic_cast<SpdyConnectTransaction *>(this).. i.e. it can be nullptr for
    // other types
    virtual SpdyConnectTransaction *QuerySpdyConnectTransaction() { return nullptr; }

    // return the load group connection information associated with the transaction
    virtual nsILoadGroupConnectionInfo *LoadGroupConnectionInfo() { return nullptr; }

    // return the connection information associated with the transaction
    virtual nsHttpConnectionInfo *ConnectionInfo() = 0;

    // The base definition of these is done in nsHttpTransaction.cpp
    virtual bool ResponseTimeoutEnabled() const;
    virtual PRIntervalTime ResponseTimeout();

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

    // conceptually the security info is part of the connection, but sometimes
    // in the case of TLS tunneled within TLS the transaction might present
    // a more specific security info that cannot be represented as a layer in
    // the connection due to multiplexing. This interface represents such an
    // overload. If it returns NS_FAILURE the connection should be considered
    // authoritative.
    virtual nsresult GetTransactionSecurityInfo(nsISupports **)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAHttpTransaction, NS_AHTTPTRANSACTION_IID)

#define NS_DECL_NSAHTTPTRANSACTION \
    void SetConnection(nsAHttpConnection *); \
    nsAHttpConnection *Connection(); \
    void GetSecurityCallbacks(nsIInterfaceRequestor **);       \
    void OnTransportStatus(nsITransport* transport, \
                           nsresult status, uint64_t progress); \
    bool     IsDone(); \
    nsresult Status(); \
    uint32_t Caps();   \
    void     SetDNSWasRefreshed(); \
    uint64_t Available(); \
    virtual nsresult ReadSegments(nsAHttpSegmentReader *, uint32_t, uint32_t *); \
    virtual nsresult WriteSegments(nsAHttpSegmentWriter *, uint32_t, uint32_t *); \
    void     Close(nsresult reason);                                    \
    nsHttpConnectionInfo *ConnectionInfo();                             \
    void     SetProxyConnectFailed();                                   \
    virtual nsHttpRequestHead *RequestHead();                                   \
    uint32_t Http1xTransactionCount();                                  \
    nsresult TakeSubTransactions(nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions); \
    nsresult AddTransaction(nsAHttpTransaction *);                      \
    uint32_t PipelineDepth();                                           \
    nsresult SetPipelinePosition(int32_t);                              \
    int32_t  PipelinePosition();

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

class nsAHttpSegmentReader
{
public:
    // any returned failure code stops segment iteration
    virtual nsresult OnReadSegment(const char *segment,
                                   uint32_t count,
                                   uint32_t *countRead) = 0;

    // Ask the segment reader to commit to accepting size bytes of
    // data from subsequent OnReadSegment() calls or throw hard
    // (i.e. not wouldblock) exceptions. Implementations
    // can return NS_ERROR_FAILURE if they never make commitments of that size
    // (the default), NS_OK if they make the commitment, or
    // NS_BASE_STREAM_WOULD_BLOCK if they cannot make the
    // commitment now but might in the future and forceCommitment is not true .
    // (forceCommitment requires a hard failure or OK at this moment.)
    //
    // SpdySession uses this to make sure frames are atomic.
    virtual nsresult CommitToSegmentSize(uint32_t size, bool forceCommitment)
    {
        return NS_ERROR_FAILURE;
    }
};

#define NS_DECL_NSAHTTPSEGMENTREADER \
    nsresult OnReadSegment(const char *, uint32_t, uint32_t *);

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

class nsAHttpSegmentWriter
{
public:
    // any returned failure code stops segment iteration
    virtual nsresult OnWriteSegment(char *segment,
                                    uint32_t count,
                                    uint32_t *countWritten) = 0;
};

#define NS_DECL_NSAHTTPSEGMENTWRITER \
    nsresult OnWriteSegment(char *, uint32_t, uint32_t *);

}} // namespace mozilla::net

#endif // nsAHttpTransaction_h__
