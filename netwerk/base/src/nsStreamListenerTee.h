#ifndef nsStreamListenerTee_h__
#define nsStreamListenerTee_h__

#include "nsIStreamListener.h"
#include "nsIInputStreamTee.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

class nsStreamListenerTee : public nsIStreamListenerTee
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMLISTENERTEE

    nsStreamListenerTee() { NS_INIT_ISUPPORTS(); }
    virtual ~nsStreamListenerTee() { }

private:
    nsCOMPtr<nsIInputStreamTee> mInputTee;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsIOutputStream>   mSink;
};

#endif
