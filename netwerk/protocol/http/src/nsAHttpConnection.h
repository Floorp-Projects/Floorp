#ifndef nsAHttpConnection_h__
#define nsAHttpConnection_h__

#include "nsISupports.h"

class nsAHttpTransaction;
class nsHttpResponseHead;
class nsHttpConnectionInfo;

//----------------------------------------------------------------------------
// Abstract base class for a HTTP connection
//----------------------------------------------------------------------------

class nsAHttpConnection : public nsISupports
{
public:
    // called by a transaction when the response headers have all been read.
    // the connection can force the transaction to reset it's response headers,
    // and prepare for a new set of response headers, by setting |*reset=TRUE|.
    virtual nsresult OnHeadersAvailable(nsAHttpTransaction *,
                                        nsHttpResponseHead *,
                                        PRBool *reset) = 0;

    // called by a transaction when it completes normally.
    virtual nsresult OnTransactionComplete(nsAHttpTransaction *,
                                           nsresult status) = 0;

    // called by a transaction to suspend/resume the connection.
    virtual nsresult OnSuspend() = 0;
    virtual nsresult OnResume() = 0;
    
    // get a reference to the connection's connection-info object.
    virtual void GetConnectionInfo(nsHttpConnectionInfo **) = 0;

    // called by a transaction to remove itself from the connection (eg. when
    // it reads premature EOF and must restart itself).
    virtual void DropTransaction(nsAHttpTransaction *) = 0;

    // called by a transaction to determine whether or not the connection is
    // persistent... important in determining the end of a response.
    virtual PRBool IsPersistent() = 0;
    
    // called by a transaction when the transaction reads more from the socket
    // than it should have (eg. containing part of the next pipelined response).
    virtual nsresult PushBack(char *data, PRUint32 length) = 0;
};

#endif // nsAHttpConnection_h__
