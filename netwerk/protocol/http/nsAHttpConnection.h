/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAHttpConnection_h__
#define nsAHttpConnection_h__

#include "nsISupports.h"
#include "nsAHttpTransaction.h"

class nsISocketTransport;
class nsIAsyncInputStream;
class nsIAsyncOutputStream;

namespace mozilla { namespace net {

class nsHttpConnectionInfo;
class nsHttpConnection;

//-----------------------------------------------------------------------------
// Abstract base class for a HTTP connection
//-----------------------------------------------------------------------------

// 5a66aed7-eede-468b-ac2b-e5fb431fcc5c
#define NS_AHTTPCONNECTION_IID \
{ 0x5a66aed7, 0xeede, 0x468b, {0xac, 0x2b, 0xe5, 0xfb, 0x43, 0x1f, 0xcc, 0x5c }}

class nsAHttpConnection : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_AHTTPCONNECTION_IID)

    //-------------------------------------------------------------------------
    // NOTE: these methods may only be called on the socket thread.
    //-------------------------------------------------------------------------

    //
    // called by a transaction when the response headers have all been read.
    // the connection can force the transaction to reset it's response headers,
    // and prepare for a new set of response headers, by setting |*reset=TRUE|.
    //
    // @return failure code to close the transaction.
    //
    virtual nsresult OnHeadersAvailable(nsAHttpTransaction *,
                                        nsHttpRequestHead *,
                                        nsHttpResponseHead *,
                                        bool *reset) = 0;

    //
    // called by a transaction to resume either sending or receiving data
    // after a transaction returned NS_BASE_STREAM_WOULD_BLOCK from its
    // ReadSegments/WriteSegments methods.
    //
    virtual nsresult ResumeSend() = 0;
    virtual nsresult ResumeRecv() = 0;

    // called by a transaction to force a "send/recv from network" iteration
    // even if not scheduled by socket associated with connection
    virtual nsresult ForceSend() = 0;
    virtual nsresult ForceRecv() = 0;

    // After a connection has had ResumeSend() called by a transaction,
    // and it is ready to write to the network it may need to know the
    // transaction that has data to write. This is only an issue for
    // multiplexed protocols like SPDY - plain HTTP or pipelined HTTP
    // implicitly have this information in a 1:1 relationship with the
    // transaction(s) they manage.
    virtual void TransactionHasDataToWrite(nsAHttpTransaction *)
    {
        // by default do nothing - only multiplexed protocols need to overload
        return;
    }

    // This is the companion to *HasDataToWrite() for the case
    // when a gecko caller has called ResumeRecv() after being paused
    virtual void TransactionHasDataToRecv(nsAHttpTransaction *)
    {
        // by default do nothing - only multiplexed protocols need to overload
        return;
    }

    // called by the connection manager to close a transaction being processed
    // by this connection.
    //
    // @param transaction
    //        the transaction being closed.
    // @param reason
    //        the reason for closing the transaction.  NS_BASE_STREAM_CLOSED
    //        is equivalent to NS_OK.
    //
    virtual void CloseTransaction(nsAHttpTransaction *transaction,
                                  nsresult reason) = 0;

    // get a reference to the connection's connection info object.
    virtual void GetConnectionInfo(nsHttpConnectionInfo **) = 0;

    // get the transport level information for this connection. This may fail
    // if it is in use.
    virtual nsresult TakeTransport(nsISocketTransport **,
                                   nsIAsyncInputStream **,
                                   nsIAsyncOutputStream **) = 0;

    // called by a transaction to get the security info from the socket.
    virtual void GetSecurityInfo(nsISupports **) = 0;

    // called by a transaction to determine whether or not the connection is
    // persistent... important in determining the end of a response.
    virtual bool IsPersistent() = 0;

    // called to determine or set if a connection has been reused.
    virtual bool IsReused() = 0;
    virtual void DontReuse() = 0;

    // called by a transaction when the transaction reads more from the socket
    // than it should have (eg. containing part of the next pipelined response).
    virtual nsresult PushBack(const char *data, uint32_t length) = 0;

    // Used to determine if the connection wants read events even though
    // it has not written out a transaction. Used when a connection has issued
    // a preamble such as a proxy ssl CONNECT sequence.
    virtual bool IsProxyConnectInProgress() = 0;

    // Used by a transaction to manage the state of previous response bodies on
    // the same connection and work around buggy servers.
    virtual bool LastTransactionExpectedNoContent() = 0;
    virtual void   SetLastTransactionExpectedNoContent(bool) = 0;

    // Transfer the base http connection object along with a
    // reference to it to the caller.
    virtual already_AddRefed<nsHttpConnection> TakeHttpConnection() = 0;

    // Get the nsISocketTransport used by the connection without changing
    //  references or ownership.
    virtual nsISocketTransport *Transport() = 0;

    // Cancel and reschedule transactions deeper than the current response.
    // Returns the number of canceled transactions.
    virtual uint32_t CancelPipeline(nsresult originalReason) = 0;

    // Read and write class of transaction that is carried on this connection
    virtual nsAHttpTransaction::Classifier Classification() = 0;
    virtual void Classify(nsAHttpTransaction::Classifier newclass) = 0;

    // The number of transaction bytes written out on this HTTP Connection, does
    // not count CONNECT tunnel setup
    virtual int64_t BytesWritten() = 0;

    // Update the callbacks used to provide security info. May be called on
    // any thread.
    virtual void SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks) = 0;

    // nsHttp.h version
    virtual uint32_t Version() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAHttpConnection, NS_AHTTPCONNECTION_IID)

#define NS_DECL_NSAHTTPCONNECTION(fwdObject)                    \
    nsresult OnHeadersAvailable(nsAHttpTransaction *, nsHttpRequestHead *, nsHttpResponseHead *, bool *reset) override; \
    void CloseTransaction(nsAHttpTransaction *, nsresult) override; \
    nsresult TakeTransport(nsISocketTransport **,    \
                           nsIAsyncInputStream **,   \
                           nsIAsyncOutputStream **) override; \
    bool IsPersistent() override;                         \
    bool IsReused() override;                             \
    void DontReuse() override;                            \
    nsresult PushBack(const char *, uint32_t) override;   \
    already_AddRefed<nsHttpConnection> TakeHttpConnection() override; \
    uint32_t CancelPipeline(nsresult originalReason) override; \
    nsAHttpTransaction::Classifier Classification() override; \
    /*                                                    \
       Thes methods below have automatic definitions that just forward the \
       function to a lower level connection object        \
    */                                                    \
    void GetConnectionInfo(nsHttpConnectionInfo **result) \
      override                                            \
    {                                                     \
      if (!(fwdObject)) {                                 \
          *result = nullptr;                              \
          return;                                         \
      }                                                   \
        return (fwdObject)->GetConnectionInfo(result);    \
    }                                                     \
    void GetSecurityInfo(nsISupports **result) override   \
    {                                                     \
      if (!(fwdObject)) {                                 \
          *result = nullptr;                              \
          return;                                         \
      }                                                   \
      return (fwdObject)->GetSecurityInfo(result);        \
    }                                                     \
    nsresult ResumeSend() override     \
    {                                      \
        if (!(fwdObject))                  \
            return NS_ERROR_FAILURE;       \
        return (fwdObject)->ResumeSend();  \
    }                                      \
    nsresult ResumeRecv() override     \
    {                                      \
        if (!(fwdObject))                  \
            return NS_ERROR_FAILURE;       \
        return (fwdObject)->ResumeRecv();  \
    }                                      \
    nsresult ForceSend() override      \
    {                                      \
        if (!(fwdObject))                  \
            return NS_ERROR_FAILURE;       \
        return (fwdObject)->ForceSend();   \
    }                                      \
    nsresult ForceRecv() override      \
    {                                      \
        if (!(fwdObject))                  \
            return NS_ERROR_FAILURE;       \
        return (fwdObject)->ForceRecv();   \
    }                                      \
    nsISocketTransport *Transport()        \
      override                         \
    {                                      \
        if (!(fwdObject))                  \
            return nullptr;                 \
        return (fwdObject)->Transport();   \
    }                                      \
    uint32_t Version() override        \
    {                                      \
        return (fwdObject) ?               \
            (fwdObject)->Version() :       \
            NS_HTTP_VERSION_UNKNOWN;       \
    }                                      \
    bool IsProxyConnectInProgress() override                \
    {                                                       \
        return (!fwdObject) ? false :                       \
               (fwdObject)->IsProxyConnectInProgress();     \
    }                                                       \
    bool LastTransactionExpectedNoContent() override        \
    {                                                       \
        return (!fwdObject) ? false :                       \
        (fwdObject)->LastTransactionExpectedNoContent();    \
    }                                                       \
    void SetLastTransactionExpectedNoContent(bool val)      \
      override                                              \
    {                                                       \
      if (fwdObject)                                        \
        (fwdObject)->SetLastTransactionExpectedNoContent(val); \
    }                                                       \
    void Classify(nsAHttpTransaction::Classifier newclass)  \
      override                                              \
    {                                                       \
    if (fwdObject)                                          \
        (fwdObject)->Classify(newclass);                    \
    }                                                       \
    int64_t BytesWritten() override                         \
    {     return fwdObject ? (fwdObject)->BytesWritten() : 0; } \
    void SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks) \
      override                                              \
    {                                                       \
        if (fwdObject)                                      \
            (fwdObject)->SetSecurityCallbacks(aCallbacks);  \
    }

} // namespace net
} // namespace mozilla

#endif // nsAHttpConnection_h__
