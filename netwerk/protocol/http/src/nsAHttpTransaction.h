#ifndef nsAHttpTransaction_h__
#define nsAHttpTransaction_h__

#include "nsISupports.h"

class nsAHttpConnection;
class nsIInputStream;
class nsIOutputStream;
class nsIInterfaceRequestor;

//----------------------------------------------------------------------------
// Abstract base class for a HTTP transaction
//----------------------------------------------------------------------------

class nsAHttpTransaction : public nsISupports
{
public:
    // called by the connection when it takes ownership of the transaction.
    virtual void SetConnection(nsAHttpConnection *) = 0;

    // called by the connection to pass socket security-info to the transaction.
    virtual void SetSecurityInfo(nsISupports *) = 0;

    // called by the connection to get notification callbacks to set on the 
    // socket transport.
    virtual void GetNotificationCallbacks(nsIInterfaceRequestor **) = 0;

    // called by the connection to indicate that the socket can be written to.
    // the transaction returns NS_BASE_STREAM_CLOSED when it is finished
    // writing its request(s).
    virtual nsresult OnDataWritable(nsIOutputStream *) = 0;

    // called by the connection to indicate that the socket can be read from.
    // the transaction can return NS_BASE_STREAM_WOULD_BLOCK to suspend the
    // socket read request.
    virtual nsresult OnDataReadable(nsIInputStream *) = 0;

    // called by the connection when the transaction should stop, either due
    // to normal completion, cancelation, or some socket transport error.
    virtual nsresult OnStopTransaction(nsresult status) = 0;

    // called by the connection to report socket status.
    virtual void OnStatus(nsresult status, const PRUnichar *statusText) = 0;

    // called by the connection to check the transaction status.
    virtual PRBool   IsDone() = 0;
    virtual nsresult Status() = 0;
};

#endif // nsAHttpTransaction_h__
