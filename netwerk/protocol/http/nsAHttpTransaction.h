/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAHttpTransaction_h__
#define nsAHttpTransaction_h__

#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

class nsIInterfaceRequestor;
class nsITransport;
class nsIRequestContext;

namespace mozilla { namespace net {

class nsAHttpConnection;
class nsAHttpSegmentReader;
class nsAHttpSegmentWriter;
class nsHttpTransaction;
class nsHttpRequestHead;
class nsHttpConnectionInfo;
class NullHttpTransaction;
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

    // called by the connection after a successfull activation of this transaction
    // in other words, tells the transaction it transitioned to the "active" state.
    virtual void OnActivated(bool h2) {}

    // used to obtain the connection associated with this transaction
    virtual nsAHttpConnection *Connection() = 0;

    // called by the connection to get security callbacks to set on the
    // socket transport.
    virtual void GetSecurityCallbacks(nsIInterfaceRequestor **) = 0;

    // called to report socket status (see nsITransportEventSink)
    virtual void OnTransportStatus(nsITransport* transport,
                                   nsresult status, int64_t progress) = 0;

    // called to check the transaction status.
    virtual bool     IsDone() = 0;
    virtual nsresult Status() = 0;
    virtual uint32_t Caps() = 0;

    // called to notify that a requested DNS cache entry was refreshed.
    virtual void     SetDNSWasRefreshed() = 0;

    // called to read request data from the transaction.
    virtual MOZ_MUST_USE nsresult ReadSegments(nsAHttpSegmentReader *reader,
                                               uint32_t count,
                                               uint32_t *countRead) = 0;

    // called to write response data to the transaction.
    virtual MOZ_MUST_USE nsresult WriteSegments(nsAHttpSegmentWriter *writer,
                                                uint32_t count,
                                                uint32_t *countWritten) = 0;

    // These versions of the functions allow the overloader to specify whether or
    // not it is safe to call *Segments() in a loop while they return OK.
    // The callee should turn again to false if it is not, otherwise leave untouched
    virtual MOZ_MUST_USE nsresult
    ReadSegmentsAgain(nsAHttpSegmentReader *reader, uint32_t count,
                      uint32_t *countRead, bool *again)
    {
        return ReadSegments(reader, count, countRead);
    }
    virtual MOZ_MUST_USE nsresult
    WriteSegmentsAgain(nsAHttpSegmentWriter *writer, uint32_t count,
                       uint32_t *countWritten, bool *again)
    {
        return WriteSegments(writer, count, countWritten);
    }

    // called to close the transaction
    virtual void Close(nsresult reason) = 0;

    // called to indicate a failure with proxy CONNECT
    virtual void SetProxyConnectFailed() = 0;

    // called to retrieve the request headers of the transaction
    virtual nsHttpRequestHead *RequestHead() = 0;

    // determine the number of real http/1.x transactions on this
    // abstract object. Pipelines had multiple, SPDY has 0,
    // normal http transactions have 1.
    virtual uint32_t Http1xTransactionCount() = 0;

    // called to remove the unused sub transactions from an object that can
    // handle multiple transactions simultaneously (i.e. h2).
    //
    // Returns NS_ERROR_NOT_IMPLEMENTED if the object does not implement
    // sub-transactions.
    //
    // Returns NS_ERROR_ALREADY_OPENED if the subtransactions have been
    // at least partially written and cannot be moved.
    //
    virtual MOZ_MUST_USE nsresult TakeSubTransactions(
        nsTArray<RefPtr<nsAHttpTransaction> > &outTransactions) = 0;

    // Occasionally the abstract interface has to give way to base implementations
    // to respect differences between spdy, h2, etc..
    // These Query* (and IsNullTransaction()) functions provide a way to do
    // that without using xpcom or rtti. Any calling code that can't deal with
    // a null response from one of them probably shouldn't be using nsAHttpTransaction

    // equivalent to !!dynamic_cast<NullHttpTransaction *>(this)
    // A null transaction is expected to return BASE_STREAM_CLOSED on all of
    // its IO functions all the time.
    virtual bool IsNullTransaction() { return false; }
    virtual NullHttpTransaction *QueryNullTransaction() { return nullptr; }

    // If we used rtti this would be the result of doing
    // dynamic_cast<nsHttpTransaction *>(this).. i.e. it can be nullptr for
    // non nsHttpTransaction implementations of nsAHttpTransaction
    virtual nsHttpTransaction *QueryHttpTransaction() { return nullptr; }

    // If we used rtti this would be the result of doing
    // dynamic_cast<SpdyConnectTransaction *>(this).. i.e. it can be nullptr for
    // other types
    virtual SpdyConnectTransaction *QuerySpdyConnectTransaction() { return nullptr; }

    // return the request context associated with the transaction
    virtual nsIRequestContext *RequestContext() { return nullptr; }

    // return the connection information associated with the transaction
    virtual nsHttpConnectionInfo *ConnectionInfo() = 0;

    // The base definition of these is done in nsHttpTransaction.cpp
    virtual bool ResponseTimeoutEnabled() const;
    virtual PRIntervalTime ResponseTimeout();

    // conceptually the security info is part of the connection, but sometimes
    // in the case of TLS tunneled within TLS the transaction might present
    // a more specific security info that cannot be represented as a layer in
    // the connection due to multiplexing. This interface represents such an
    // overload. If it returns NS_FAILURE the connection should be considered
    // authoritative.
    virtual MOZ_MUST_USE nsresult GetTransactionSecurityInfo(nsISupports **)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    virtual void DisableSpdy() { }
    virtual void ReuseConnectionOnRestartOK(bool) { }

    // Returns true if early-data or fast open is possible.
    virtual MOZ_MUST_USE bool CanDo0RTT() {
        return false;
    }
    // Returns true if early-data is possible and transaction will remember
    // that it is in 0RTT mode (to know should it rewide transaction or not
    // in the case of an error).
    virtual MOZ_MUST_USE bool Do0RTT() {
        return false;
    }
    // This function will be called when a tls handshake has been finished and
    // we know whether early-data that was sent has been accepted or not, e.g.
    // do we need to restart a transaction. This will be called only if Do0RTT
    // returns true.
    // If aRestart parameter is true we need to restart the transaction,
    // otherwise the erly-data has been accepted and we can continue the
    // transaction.
    // If aAlpnChanged is true (and we were assuming http/2), we'll need to take
    // the transactions out of the session, rewind them all, and start them back
    // over as http/1 transactions
    // The function will return success or failure of the transaction restart.
    virtual MOZ_MUST_USE nsresult Finish0RTT(bool aRestart, bool aAlpnChanged) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    virtual MOZ_MUST_USE nsresult RestartOnFastOpenError() {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    virtual uint64_t TopLevelOuterContentWindowId() {
        MOZ_ASSERT(false);
        return 0;
    }

    virtual void SetFastOpenStatus(uint8_t aStatus) {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAHttpTransaction, NS_AHTTPTRANSACTION_IID)

#define NS_DECL_NSAHTTPTRANSACTION \
    void SetConnection(nsAHttpConnection *) override; \
    nsAHttpConnection *Connection() override; \
    void GetSecurityCallbacks(nsIInterfaceRequestor **) override;       \
    void OnTransportStatus(nsITransport* transport, \
                           nsresult status, int64_t progress) override; \
    bool     IsDone() override; \
    nsresult Status() override; \
    uint32_t Caps() override;   \
    void     SetDNSWasRefreshed() override; \
    virtual MOZ_MUST_USE nsresult ReadSegments(nsAHttpSegmentReader *, uint32_t, uint32_t *) override; \
    virtual MOZ_MUST_USE nsresult WriteSegments(nsAHttpSegmentWriter *, uint32_t, uint32_t *) override; \
    virtual void Close(nsresult reason) override;                                \
    nsHttpConnectionInfo *ConnectionInfo() override;                             \
    void     SetProxyConnectFailed() override;                                   \
    virtual nsHttpRequestHead *RequestHead() override;                                   \
    uint32_t Http1xTransactionCount() override;                                  \
    MOZ_MUST_USE nsresult TakeSubTransactions(nsTArray<RefPtr<nsAHttpTransaction> > &outTransactions) override;

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

class nsAHttpSegmentReader
{
public:
    // any returned failure code stops segment iteration
    virtual MOZ_MUST_USE nsresult OnReadSegment(const char *segment,
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
    virtual MOZ_MUST_USE nsresult CommitToSegmentSize(uint32_t size,
                                                      bool forceCommitment)
    {
        return NS_ERROR_FAILURE;
    }
};

#define NS_DECL_NSAHTTPSEGMENTREADER \
    MOZ_MUST_USE nsresult OnReadSegment(const char *, uint32_t, uint32_t *) override;

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

class nsAHttpSegmentWriter
{
public:
    // any returned failure code stops segment iteration
    virtual MOZ_MUST_USE nsresult OnWriteSegment(char *segment,
                                                 uint32_t count,
                                                 uint32_t *countWritten) = 0;
};

#define NS_DECL_NSAHTTPSEGMENTWRITER \
    MOZ_MUST_USE nsresult OnWriteSegment(char *, uint32_t, uint32_t *) override;

} // namespace net
} // namespace mozilla

#endif // nsAHttpTransaction_h__
