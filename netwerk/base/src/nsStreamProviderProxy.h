#ifndef nsStreamProviderProxy_h__
#define nsStreamProviderProxy_h__

#include "nsStreamObserverProxy.h"
#include "nsIStreamProvider.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

class nsStreamProviderProxy : public nsStreamProxyBase
                            , public nsIStreamProviderProxy
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMPROVIDER
    NS_DECL_NSISTREAMPROVIDERPROXY

    nsStreamProviderProxy();
    virtual ~nsStreamProviderProxy();

    nsIStreamProvider *GetProvider()
    {
        return NS_STATIC_CAST(nsIStreamProvider *, GetReceiver());
    }

    void SetProviderStatus(nsresult status)
    {
        mProviderStatus = status;
    }

protected:
    nsCOMPtr<nsIInputStream>  mPipeIn;
    nsCOMPtr<nsIOutputStream> mPipeOut;
    nsresult                  mProviderStatus;
};

#endif /* !nsStreamProviderProxy_h__ */
